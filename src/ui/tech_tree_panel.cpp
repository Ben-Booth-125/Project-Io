#include "tech_tree_panel.hpp"

#include "foldout_column.hpp"
#include "format.hpp"
#include "hover_card.hpp"
#include "presentation.hpp"

#include "../world/digitisation_tree_data.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ui {

namespace {

namespace dg = io::dg;

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

// ---------------------------------------------------------------------------
// Layout — the approved render (Ben's design session, 2026-10-10)
// ---------------------------------------------------------------------------
// Three tree sectors of 120 degrees — Production centred at the top (-90),
// Network at 30, State at 150, angles growing clockwise on screen — each with
// three branch wedges 38 degrees apart, and no centre node. Along a branch's
// angle `a`, in BASE radius units (scaled to the canvas):
//
//   Travel I 48 -> Travel II 74 -> cluster A minors 102 (a +-6) -> major A 134
//   -> two arms at a-10 / a+10: Travel III / V 164 -> Travel IV / VI 194
//   -> arm minors 226 (arm +-5) -> majors B (a-10) / C (a+10) 264 -> mastery 318.
//
// Pair majors sit +-6 degrees off their slot; a quest diamond beside its major;
// a joint on the boundary between the two arms it joins, at 214.
//
// The layout is a pure function of the store's ids, so it is computed once.

constexpr float kTreeCentre[3] = {-90.0f, 30.0f, 150.0f};
constexpr float kBranchStep    = 38.0f;
constexpr float kRimRadius     = 318.0f; ///< The mastery ring, the outermost node.
constexpr float kLabelRadius   = 346.0f; ///< Branch labels.
constexpr float kFitRadius     = 372.0f; ///< What the default zoom fits inside the canvas.
constexpr float kHubRadius     = 40.0f;  ///< The empty hub the tree names sit in (radius 0 maps here).
constexpr float kNameRadius    = 42.0f;  ///< A tree name's centre, raw base units, on its tree's axis.

/// The radii between the four cost rings (RESEARCH.md: rings run inside out
/// and stand for cost), drawn as faint guides.
constexpr float kRingEdge[3] = {118.0f, 179.0f, 245.0f};

struct node_place
{
    float ang = 0.0f; ///< Degrees, screen convention (0 = right, +90 = down).
    float r   = 0.0f; ///< Base radius units.
};

int branch_index(const char* key)
{
    for (int b = 0; b < dg::branch_count; ++b)
        if (std::strcmp(dg::branches[static_cast<std::size_t>(b)].key, key) == 0)
            return b;
    return -1;
}

float branch_angle(int b)
{
    return kTreeCentre[b / 3] + static_cast<float>(b % 3 - 1) * kBranchStep;
}

/// The slot suffix of a branch node's id: "DG-PR-EX-A1" -> "A1".
const char* slot_of(const char* id)
{
    const char* last = std::strrchr(id, '-');
    return last != nullptr ? last + 1 : id;
}

/// Whether the store holds the sibling slot @p slot in the same branch as @p id.
bool has_slot(const char* id, const char* slot)
{
    std::string s(id, static_cast<std::size_t>(slot_of(id) - id));
    s += slot;
    return dg::find_node(s) >= 0;
}

/// Mean of angles in degrees, on the circle.
float circular_mean(float a, float b)
{
    const float x = std::cos(a * kDeg) + std::cos(b * kDeg);
    const float y = std::sin(a * kDeg) + std::sin(b * kDeg);
    return std::atan2(y, x) / kDeg;
}

/// Wrap an angle into (-180, 180] relative to @p ref, so a side test reads.
float rel_angle(float ang, float ref)
{
    float d = ang - ref;
    while (d > 180.0f)  d -= 360.0f;
    while (d <= -180.0f) d += 360.0f;
    return d;
}

std::array<node_place, dg::node_count> compute_layout()
{
    std::array<node_place, dg::node_count> L{};
    std::array<bool, dg::node_count> placed{};

    // Branch nodes and masteries first; quests and joints read their positions.
    for (int i = 0; i < dg::node_count; ++i)
    {
        const dg::node& n = dg::nodes[static_cast<std::size_t>(i)];
        if (n.is_joint || n.is_quest)
            continue;
        const int b = branch_index(n.branch);
        if (b < 0)
            continue;
        const float a = branch_angle(b);
        const char* slot = slot_of(n.id);
        node_place p;
        const char c0 = slot[0];
        const char c1 = slot[0] != '\0' ? slot[1] : '\0';
        if (c0 == 'T')
        {
            switch (c1)
            {
                case '1': p = {a, 48.0f}; break;
                case '2': p = {a, 74.0f}; break;
                case '3': p = {a - 10.0f, 164.0f}; break;
                case '4': p = {a - 10.0f, 194.0f}; break;
                case '5': p = {a + 10.0f, 164.0f}; break;
                default:  p = {a + 10.0f, 194.0f}; break; // T6
            }
        }
        else if (c0 == 'M')
            p = {a, kRimRadius};
        else if (c0 == 'A' || c0 == 'B' || c0 == 'C')
        {
            const float arm = c0 == 'A' ? a : (c0 == 'B' ? a - 10.0f : a + 10.0f);
            const bool  minor = (c1 == '1' || c1 == '2');
            if (minor)
            {
                const char other[3] = {c0, c1 == '1' ? '2' : '1', '\0'};
                const float off = c0 == 'A' ? 6.0f : 5.0f;
                const float d = has_slot(n.id, other) ? (c1 == '1' ? -off : off) : 0.0f;
                p = {arm + d, c0 == 'A' ? 102.0f : 226.0f};
            }
            else
            {
                // The cluster's major, or one side of a pick-one pair ('a' / 'b').
                const float d = c1 == 'a' ? -6.0f : (c1 == 'b' ? 6.0f : 0.0f);
                p = {arm + d, c0 == 'A' ? 134.0f : 264.0f};
            }
        }
        L[static_cast<std::size_t>(i)] = p;
        placed[static_cast<std::size_t>(i)] = true;
    }

    // Quests: beside their major, on the side away from the branch's axis (an
    // A-cluster quest to the + side), clear of a pair's outer member.
    for (int i = 0; i < dg::node_count; ++i)
    {
        const dg::node& n = dg::nodes[static_cast<std::size_t>(i)];
        if (!n.is_quest || n.group_count == 0)
            continue;
        const int b = branch_index(n.branch);
        const float a = b >= 0 ? branch_angle(b) : 0.0f;
        float lo = 1e9f, hi = -1e9f, r = 0.0f;
        int members = 0;
        for (const std::int16_t m : n.groups[0])
        {
            if (m < 0 || !placed[static_cast<std::size_t>(m)])
                continue;
            const node_place& mp = L[static_cast<std::size_t>(m)];
            const float rel = rel_angle(mp.ang, a);
            lo = std::min(lo, rel);
            hi = std::max(hi, rel);
            r += mp.r;
            ++members;
        }
        if (members == 0)
            continue;
        r /= static_cast<float>(members);
        const float centre = 0.5f * (lo + hi);
        const float side = centre < -1.0f ? -1.0f : 1.0f;
        const float edge = side > 0.0f ? hi : lo;
        const float step = 26.0f / r / kDeg; // 26 base units of arc
        L[static_cast<std::size_t>(i)] = {a + edge + side * step, r};
        placed[static_cast<std::size_t>(i)] = true;
    }

    // Joints: on the boundary between the two arms they join.
    for (int i = 0; i < dg::node_count; ++i)
    {
        const dg::node& n = dg::nodes[static_cast<std::size_t>(i)];
        if (!n.is_joint || n.group_count < 2)
            continue;
        const int m0 = n.groups[0][0];
        const int m1 = n.groups[1][0];
        if (m0 < 0 || m1 < 0)
            continue;
        L[static_cast<std::size_t>(i)] = {
            circular_mean(L[static_cast<std::size_t>(m0)].ang, L[static_cast<std::size_t>(m1)].ang),
            214.0f};
        placed[static_cast<std::size_t>(i)] = true;
    }
    return L;
}

const std::array<node_place, dg::node_count>& layout()
{
    static const std::array<node_place, dg::node_count> L = compute_layout();
    return L;
}

// ---------------------------------------------------------------------------
// Node reads
// ---------------------------------------------------------------------------

const dg::node& node_at(int i) { return dg::nodes[static_cast<std::size_t>(i)]; }

bool is_major(const dg::node& n) { return n.t == dg::tier::major; }
bool is_pair(const dg::node& n) { return n.excludes >= 0; }

const char* tier_label(const dg::node& n)
{
    if (n.is_mastery) return "Mastery";
    if (n.is_joint)   return "Joint";
    switch (n.t)
    {
        case dg::tier::travel: return "Travel";
        case dg::tier::minor:  return "Minor";
        case dg::tier::major:  return "Major";
        case dg::tier::quest:  return "Quest";
    }
    return "Node";
}

/// The drawn radius of a node at canvas scale @p k (k = 1 at the default fit).
float node_radius(const dg::node& n, float k)
{
    float r = 3.5f;
    if (n.is_mastery || n.is_joint)    r = 6.0f;
    else if (n.t == dg::tier::minor)   r = 5.0f;
    else if (n.t == dg::tier::major)   r = 9.0f;
    else if (n.t == dg::tier::quest)   r = 7.0f;
    return r * std::clamp(k, 0.8f, 1.8f);
}

const char* branch_name(const char* key)
{
    const int b = branch_index(key);
    return b >= 0 ? dg::branches[static_cast<std::size_t>(b)].name : key;
}

/// "Requires: A + B", groups joined by +, members by "or".
std::string requires_line(const dg::node& n)
{
    if (n.group_count == 0)
        return "Requires: nothing (a branch's first node)";
    std::string s = "Requires: ";
    for (int g = 0; g < n.group_count; ++g)
    {
        if (g > 0)
            s += " + ";
        bool first = true;
        for (const std::int16_t m : n.groups[static_cast<std::size_t>(g)])
        {
            if (m < 0)
                continue;
            if (!first)
                s += " or ";
            s += node_at(m).name;
            first = false;
        }
    }
    return s;
}

enum class node_state : std::uint8_t { held, queued, open, locked };

struct nation_view
{
    entity_id                nation = null_entity;
    const nation_tech_state* st     = nullptr;
    std::array<int, dg::node_count> queue_pos{}; ///< 1-based queue position, 0 = not queued.

    bool held(int i) const { return st != nullptr && st->held.test(static_cast<std::size_t>(i)); }

    node_state state(int i) const
    {
        if (held(i))
            return node_state::held;
        if (queue_pos[static_cast<std::size_t>(i)] > 0)
            return node_state::queued;
        const dg::node& n = node_at(i);
        for (int g = 0; g < n.group_count; ++g)
        {
            bool any = false;
            for (const std::int16_t m : n.groups[static_cast<std::size_t>(g)])
                if (m >= 0 && held(m))
                    any = true;
            if (!any)
                return node_state::locked;
        }
        return node_state::open;
    }
};

nation_view view_of(const world& w)
{
    nation_view v;
    v.nation = research_player_nation(w);
    if (v.nation == null_entity)
        return v;
    if (const auto it = w.nation_tech.find(v.nation); it != w.nation_tech.end())
    {
        v.st = &it->second;
        for (std::size_t q = 0; q < it->second.queue.size(); ++q)
            if (it->second.queue[q] < dg::node_count)
                v.queue_pos[it->second.queue[q]] = static_cast<int>(q) + 1;
    }
    return v;
}

const char* state_label(node_state s)
{
    switch (s)
    {
        case node_state::held:   return "Held";
        case node_state::queued: return "Queued";
        case node_state::open:   return "Open: its requirements are held";
        case node_state::locked: return "Locked: a requirement is not held";
    }
    return "";
}

ImU32 with_alpha(ImU32 c, float a)
{
    const unsigned base = (c >> IM_COL32_A_SHIFT) & 0xFFu;
    const unsigned na = static_cast<unsigned>(std::clamp(a, 0.0f, 1.0f) * static_cast<float>(base));
    return (c & ~IM_COL32_A_MASK) | (na << IM_COL32_A_SHIFT);
}

/// The reason a Plan press on @p node is refused, or "" when it would queue.
/// @p plan receives what it would queue.
std::string refusal_reason(const world& w, entity_id nation, int node,
                           std::vector<std::uint16_t>* plan)
{
    const dg::node& n = node_at(node);
    switch (preview_enqueue(w, nation, node, plan))
    {
        case enqueue_result::queued:         return {};
        case enqueue_result::already_held:   return std::string(n.name) + " is already held.";
        case enqueue_result::already_queued: return std::string(n.name) + " is already queued.";
        case enqueue_result::invalid:        return "That node is not on this age's tree.";
        case enqueue_result::refused_pair:
        {
            if (n.excludes >= 0)
            {
                const bool held = nation_holds(w, nation, n.excludes);
                return std::string(n.name) + " is closed: its pair, " + node_at(n.excludes).name
                       + (held ? ", is held." : ", is queued.");
            }
            return std::string(n.name)
                   + " is closed: every route to it crosses a pick-one pair whose other side is"
                     " held or queued.";
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Glyphs — the ring's own vocabulary, drawn at the shared (dl, centre, r, colour)
// shape so the legend and the ring draw one way.
// ---------------------------------------------------------------------------

void dashed_circle(ImDrawList* dl, ImVec2 c, float r, ImU32 col, float thick, int dashes)
{
    const int segs = dashes * 2;
    for (int i = 0; i < segs; i += 2)
    {
        const float a0 = 2.0f * kPi * static_cast<float>(i) / static_cast<float>(segs);
        const float a1 = 2.0f * kPi * static_cast<float>(i + 1) / static_cast<float>(segs);
        dl->PathArcTo(c, r, a0, a1, 4);
        dl->PathStroke(col, 0, thick);
    }
}

void dashed_line(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float thick, float dash)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f)
        return;
    const int n = std::max(1, static_cast<int>(len / dash));
    for (int i = 0; i < n; i += 2)
    {
        const float t0 = static_cast<float>(i) / static_cast<float>(n);
        const float t1 = std::min(1.0f, static_cast<float>(i + 1) / static_cast<float>(n));
        dl->AddLine({a.x + dx * t0, a.y + dy * t0}, {a.x + dx * t1, a.y + dy * t1}, col, thick);
    }
}

void diamond(ImDrawList* dl, ImVec2 c, float r, ImU32 col, bool filled, float thick)
{
    const ImVec2 p[4] = {{c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y}};
    if (filled)
        dl->AddConvexPolyFilled(p, 4, col);
    else
        dl->AddPolyline(p, 4, col, ImDrawFlags_Closed, thick);
}

/// One node, in its tier's glyph, coloured by its state.
void draw_node(ImDrawList* dl, ImVec2 c, float r, const dg::node& n, node_state st)
{
    const bool  held   = st == node_state::held;
    const float dim    = st == node_state::locked ? 0.45f : 1.0f;
    const ImU32 held_c = palette::research_held;

    if (n.is_placeholder)
    {
        // Placeholders: dashed and muted, whatever their tier — the place of a
        // system not yet built (RESEARCH.md § Placeholders).
        dashed_circle(dl, c, r, held ? held_c : palette::research_muted, 1.4f,
                      n.t == dg::tier::major ? 10 : 5);
        return;
    }
    if (n.t == dg::tier::quest)
    {
        const ImU32 q = with_alpha(palette::research_warning, dim);
        diamond(dl, c, r, held ? held_c : with_alpha(q, 0.35f), true, 0.0f);
        diamond(dl, c, r, held ? held_c : q, false, 1.8f);
        return;
    }
    if (n.is_mastery || n.is_joint)
    {
        const ImU32 a = with_alpha(palette::research_accent, dim);
        if (held)
            dl->AddCircleFilled(c, r, held_c);
        dl->AddCircle(c, r, held ? held_c : a, 0, n.is_mastery ? 2.4f : 1.8f);
        return;
    }
    switch (n.t)
    {
        case dg::tier::travel:
            dl->AddCircleFilled(c, r, held ? held_c : with_alpha(palette::research_node, dim));
            break;
        case dg::tier::minor:
            if (held)
                dl->AddCircleFilled(c, r, held_c);
            dl->AddCircle(c, r, held ? held_c : with_alpha(palette::research_node, dim), 0, 1.6f);
            break;
        case dg::tier::major:
        default:
            dl->AddCircleFilled(c, r, held ? held_c : with_alpha(palette::research_accent, dim * 0.9f));
            if (is_pair(n))
                dl->AddCircle(c, r + 1.5f, with_alpha(palette::research_danger, dim), 0, 2.4f);
            break;
    }
}

/// Text anchored outward from the ring's centre along @p ang (degrees): to the
/// right of the point on the right half, left on the left, centred top/bottom.
void outward_text(ImDrawList* dl, ImFont* font, float size, ImVec2 at, float ang, ImU32 col,
                  const char* text, float centre_band = 0.35f)
{
    const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    const float cx = std::cos(ang * kDeg), cy = std::sin(ang * kDeg);
    ImVec2 p = at;
    if (cx > centre_band)       p.x += 0.0f;
    else if (cx < -centre_band) p.x -= ts.x;
    else                        p.x -= ts.x * 0.5f;
    if (cy > 0.35f)       p.y += 0.0f;
    else if (cy < -0.35f) p.y -= ts.y;
    else                  p.y -= ts.y * 0.5f;
    dl->AddText(font, size, p, col, text); // fit-exempt: free-standing ring label, no container to fit
}

/// The hover card's body for node @p i.
void hover_body(const world& w, const research_params& p, const nation_view& v, int i)
{
    const dg::node& n = node_at(i);
    ImGui::TextUnformatted(n.name);
    char line[160];
    std::snprintf(line, sizeof line, "%s%s | ring %d | %d RP", tier_label(n),
                  is_pair(n) ? " (pick one)" : "", static_cast<int>(n.ring),
                  static_cast<int>(static_cast<float>(n.cost_units) * p.rp_per_cost_unit));
    ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    ImGui::TextWrapped("%s", line); // the card is 200 px: a pair major's line runs past it
    ImGui::PopStyleColor();
    ImGui::Separator();
    if (n.is_placeholder)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::research_muted);
        ImGui::TextWrapped("Holds the place of %s: does nothing until that system exists.",
                           n.placeholder);
        ImGui::PopStyleColor();
    }
    ImGui::TextWrapped("%s", n.advantage);
    ImGui::TextWrapped("%s", requires_line(n).c_str());
    if (is_pair(n))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::research_danger);
        ImGui::TextWrapped("Pick one: taking it closes %s for good.", node_at(n.excludes).name);
        ImGui::PopStyleColor();
    }
    if (n.is_quest)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::research_warning);
        ImGui::TextWrapped("Deed: %s", n.deed);
        ImGui::PopStyleColor();
        const char* keys = dg::keys_next_branch[static_cast<std::size_t>(i)];
        if (keys != nullptr && keys[0] != '\0')
            ImGui::TextWrapped("Keys next age: %s", keys);
    }
    if (v.nation == null_entity)
        return;
    ImGui::Separator();
    const node_state st = v.state(i);
    if (st == node_state::queued)
    {
        ImGui::Text("Queued, position %d", v.queue_pos[static_cast<std::size_t>(i)]);
        const std::size_t off = queued_dependents(w, v.nation, i).size();
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        if (off > 1)
            ImGui::TextWrapped("Shift-click to unqueue it and the %d queued entries that need it.",
                               static_cast<int>(off) - 1);
        else
            ImGui::TextWrapped("Shift-click to unqueue it.");
        ImGui::PopStyleColor();
    }
    else
        ImGui::TextWrapped("%s", state_label(st));
    if (st != node_state::held && st != node_state::queued)
    {
        std::vector<std::uint16_t> plan;
        const std::string why = refusal_reason(w, v.nation, i, &plan);
        if (!why.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::research_danger);
            ImGui::TextWrapped("%s", why.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            std::int64_t milli = 0;
            const std::int64_t price = research_price_ppm(w, p); // BL-1324: today's price
            for (const std::uint16_t q : plan)
                milli += node_cost_milli(q, p, price);
            ImGui::PushStyleColor(ImGuiCol_Text, palette::positive);
            ImGui::TextWrapped("Shift-click a node to queue it: %d node%s, %lld RP at today's prices.",
                               static_cast<int>(plan.size()), plan.size() == 1 ? "" : "s",
                               static_cast<long long>(milli / k_milli_per_rp));
            ImGui::PopStyleColor();
        }
    }
}

/// A shift-click on node @p i: a queued node toggles off with the entries that
/// need it (NR-1033); any other is previewed, then queued through the seam.
void press_node(const world& w, ui_state& s, entity_id nation, int i)
{
    const std::vector<std::uint16_t> off = queued_dependents(w, nation, i);
    if (!off.empty())
    {
        corp_command cmd;
        cmd.corp  = w.player_entity;
        cmd.verb  = corp_verb::unqueue_research;
        cmd.order = static_cast<std::uint32_t>(i);
        s.pending_research_commands.push_back(cmd);
        const int after = static_cast<int>(off.size()) - 1;
        s.research_message = std::string("Unqueued ") + node_at(i).name
                             + (after > 0 ? " and " + std::to_string(after)
                                                + (after == 1 ? " entry" : " entries")
                                                + " that needed it."
                                          : ".");
        return;
    }
    std::vector<std::uint16_t> plan;
    const std::string why = refusal_reason(w, nation, i, &plan);
    if (!why.empty())
    {
        s.research_message = why;
        return;
    }
    corp_command cmd;
    cmd.corp  = w.player_entity;
    cmd.verb  = corp_verb::queue_research;
    cmd.order = static_cast<std::uint32_t>(i);
    s.pending_research_commands.push_back(cmd);
    const int ahead = static_cast<int>(plan.size()) - 1;
    s.research_message = std::string("Queued ") + node_at(i).name
                         + (ahead > 0 ? " with " + std::to_string(ahead)
                                            + (ahead == 1 ? " requirement" : " requirements")
                                            + " ahead of it."
                                      : ".");
}

/// The column's two projections — RP a quarter and each queue entry's arrival
/// — recomputed only when what they read changes: the day, the nation, its
/// treasury, pool, held set or queue (cold review of BL-1276: they ran every
/// frame). View state only; nothing here reaches the sim.
struct research_projection
{
    bool                            valid    = false;
    int                             day      = -1;
    entity_id                       nation   = null_entity;
    float                           treasury = 0.0f;
    std::int64_t                    pool     = 0;
    std::bitset<dg::node_count>     held;
    std::vector<std::uint16_t>      queue;
    std::int64_t                    rate_milli = 0;
    std::vector<int>                arrival;
};

const research_projection& projection(const world& w, entity_id nation, const research_params& p)
{
    static research_projection c;
    const auto sit = w.nation_tech.find(nation);
    const nation_tech_state* st = sit != w.nation_tech.end() ? &sit->second : nullptr;
    const float treasury = w.nations.at(nation).treasury;
    const std::int64_t pool = st != nullptr ? st->pool_milli : 0;
    static const std::vector<std::uint16_t> none;
    const std::vector<std::uint16_t>& queue = st != nullptr ? st->queue : none;
    const std::bitset<dg::node_count> held = st != nullptr ? st->held : std::bitset<dg::node_count>{};
    if (c.valid && c.day == w.current_day_tick && c.nation == nation && c.treasury == treasury
        && c.pool == pool && c.held == held && c.queue == queue)
        return c;
    c.valid      = true;
    c.day        = w.current_day_tick;
    c.nation     = nation;
    c.treasury   = treasury;
    c.pool       = pool;
    c.held       = held;
    c.queue      = queue;
    c.rate_milli = estimate_rp_per_quarter_milli(w, nation, p);
    c.arrival    = project_queue_arrivals(w, nation, p);
    return c;
}

/// "Q3 1963" for the quarter @p quarters_ahead after the current one.
std::string quarter_label(const world& w, int quarters_ahead)
{
    const fmt::calendar_date d = fmt::date_from_day(
        static_cast<std::uint64_t>(std::max(0, w.current_day_tick)));
    const int idx = d.year * 4 + (d.quarter - 1) + quarters_ahead;
    char b[24];
    std::snprintf(b, sizeof b, "Q%d %d", idx % 4 + 1, idx / 4);
    return b;
}

void legend(ImDrawList* dl, ImVec2 origin, float k)
{
    struct row { const char* label; int kind; };
    static constexpr row rows[] = {
        {"Travel", 0}, {"Minor", 1}, {"Major", 2}, {"Pick-one pair", 3}, {"Quest", 4},
        {"Joint / mastery", 5}, {"Placeholder", 6}, {"AND link (needs every group)", 7},
        {"Held", 8}, {"Queued (position)", 9},
    };
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize() * 0.9f;
    const float lh = fs + 6.0f;
    float y = origin.y;
    // A stand-in node per kind, so the legend draws through draw_node itself.
    const auto find_kind = [](int kind) -> int {
        for (int i = 0; i < dg::node_count; ++i)
        {
            const dg::node& n = dg::nodes[static_cast<std::size_t>(i)];
            const bool real = !n.is_placeholder;
            switch (kind)
            {
                case 0: if (real && n.t == dg::tier::travel && !n.is_joint && !n.is_mastery) return i; break;
                case 1: if (real && n.t == dg::tier::minor) return i; break;
                case 2: if (real && n.t == dg::tier::major && n.excludes < 0) return i; break;
                case 3: if (real && n.excludes >= 0) return i; break;
                case 4: if (n.is_quest) return i; break;
                case 5: if (n.is_joint) return i; break;
                case 6: if (n.is_placeholder && n.t == dg::tier::minor) return i; break;
                default: break;
            }
        }
        return 0;
    };
    for (const row& r : rows)
    {
        const ImVec2 c{origin.x + 10.0f, y + lh * 0.5f};
        if (r.kind <= 6)
        {
            const int i = find_kind(r.kind);
            draw_node(dl, c, node_radius(node_at(i), std::min(k, 1.0f)), node_at(i), node_state::open);
        }
        else if (r.kind == 7)
            dl->AddLine({c.x - 9.0f, c.y}, {c.x + 9.0f, c.y}, palette::research_accent, 2.0f);
        else if (r.kind == 8)
            dl->AddCircleFilled(c, 5.0f, palette::research_held);
        else
        {
            dl->AddCircle(c, 6.0f, palette::research_queued, 0, 1.6f);
            dl->AddText(font, fs * 0.8f, {c.x + 6.0f, c.y - fs * 0.9f}, palette::research_queued, "1"); // fit-exempt: legend glyph
        }
        dl->AddText(font, fs, {origin.x + 26.0f, y + (lh - fs) * 0.5f}, palette::text_secondary, r.label); // fit-exempt: legend label
        y += lh;
    }
}

} // namespace

entity_id research_player_nation(const world& w)
{
    // BL-1407: the player IS its nation.
    return player_nation(w);
}

// ---------------------------------------------------------------------------
// The column: nation, queue (no mode strip: shift-click queues, Ben 2026-10-10)
// ---------------------------------------------------------------------------

void draw_tech_tree_menu(const world& w, const research_params& p, ui_state& s)
{
    if (!s.show_tech_tree)
        return;

    const entity_id nation = research_player_nation(w);

    if (ui::foldout_begin("Research"))
    {
        if (nation == null_entity)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
            ImGui::TextWrapped("No corporation is seated, so there is no nation to plan for. "
                               "Hover a node on the ring to read it.");
            ImGui::PopStyleColor();
            ui::foldout_end();
            return;
        }

        const nation_component& nc = w.nations.at(nation);
        const auto sit = w.nation_tech.find(nation);
        const nation_tech_state* st = sit != w.nation_tech.end() ? &sit->second : nullptr;
        const research_projection& proj = projection(w, nation, p);
        const std::int64_t rate = proj.rate_milli;

        ImGui::Text("Nation: %s", nc.name.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        ImGui::TextWrapped("%lld RP a quarter | %lld RP in hand | %d of %d held",
                           static_cast<long long>(rate / k_milli_per_rp),
                           static_cast<long long>((st != nullptr ? st->pool_milli : 0) / k_milli_per_rp),
                           st != nullptr ? static_cast<int>(st->held.count()) : 0, dg::node_count);
        ImGui::PopStyleColor();
        ImGui::Separator();

        ImGui::TextWrapped("Shift-click a node to queue it; shift-click a queued node to "
                           "take it off. Hover reads it.");
        ImGui::Spacing();

        // --- the queue
        const std::vector<int>& arrival = proj.arrival;
        const bool empty = st == nullptr || st->queue.empty();
        ImGui::Text("Queue (%d)", empty ? 0 : static_cast<int>(st->queue.size()));
        if (empty)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
            ImGui::TextWrapped("Nothing queued. The pool accrues and waits.");
            ImGui::PopStyleColor();
        }
        else if (ImGui::BeginTable("##research_queue", 5,
                                   ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit
                                       | ImGuiTableFlags_ScrollY,
                                   {0.0f, std::min(ImGui::GetContentRegionAvail().y - 90.0f,
                                                   ImGui::GetTextLineHeightWithSpacing()
                                                       * (static_cast<float>(st->queue.size()) + 1.5f))}))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#");
            ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Tier");
            ImGui::TableSetupColumn("RP (today)"); // BL-1324: costs at today's price index
            ImGui::TableSetupColumn("Arrives");
            ImGui::TableHeadersRow();
            bool after_deed = false;
            for (std::size_t q = 0; q < st->queue.size(); ++q)
            {
                const int i = st->queue[q];
                const dg::node& n = node_at(i);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", static_cast<int>(q) + 1);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(n.name);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\n%s", n.name, n.advantage);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(tier_label(n));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(
                                        node_cost_milli(i, p, research_price_ppm(w, p)) / k_milli_per_rp));
                ImGui::TableNextColumn();
                const int a = q < arrival.size() ? arrival[q] : -1;
                if (a > 0)
                    ImGui::TextUnformatted(quarter_label(w, a).c_str());
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, palette::research_warning);
                    ImGui::TextUnformatted(n.is_quest ? "on its deed" : (after_deed ? "after a deed" : "not in 120 y"));
                    ImGui::PopStyleColor();
                    if (n.is_quest)
                        after_deed = true;
                }
            }
            ImGui::EndTable();
        }

        // --- the queue controls, always present while there is a nation
        {
            if (empty)
                ImGui::BeginDisabled();
            if (ImGui::Button("Remove last"))
            {
                corp_command cmd;
                cmd.corp  = w.player_entity;
                cmd.verb  = corp_verb::unqueue_research;
                cmd.order = unqueue_last;
                s.pending_research_commands.push_back(cmd);
                s.research_message = std::string("Removed ") + node_at(st->queue.back()).name + ".";
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                corp_command cmd;
                cmd.corp = w.player_entity;
                cmd.verb = corp_verb::clear_research;
                s.pending_research_commands.push_back(cmd);
                s.research_message = "Queue cleared. The pool stays.";
            }
            if (empty)
                ImGui::EndDisabled();
        }
        if (!s.research_message.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
            ImGui::TextWrapped("%s", s.research_message.c_str());
            ImGui::PopStyleColor();
        }
    }
    ui::foldout_end();
}

// ---------------------------------------------------------------------------
// The canvas: the ring
// ---------------------------------------------------------------------------

void draw_tech_tree_panel(const world& w, const research_params& p, ui_state& s)
{
    if (!s.show_tech_tree)
    {
        s.tech_tree_node_screen.clear();
        s.tech_tree_hover_node  = -1;
        s.tech_tree_hover_stuck = false;
        return;
    }

    const foldout_rect canvas = ui::canvas_rect();
    ImGui::SetNextWindowPos({canvas.x, canvas.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({canvas.w, canvas.h}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##tech_tree_canvas", nullptr, flags);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiIO& io = ImGui::GetIO();
    const bool canvas_hovered = ImGui::IsWindowHovered();

    // --- camera: wheel zooms about the cursor, right/middle drag pans
    // The ring fits the canvas BELOW the caption band (the mode line and the
    // press message, two lines at the top-left): centred in what is left, so
    // the top rim's branch label never runs into the caption.
    const float caption_h = 10.0f + 2.0f * ImGui::GetTextLineHeightWithSpacing();
    const float ring_h    = std::max(1.0f, canvas.h - caption_h);
    const ImVec2 mid{canvas.x + canvas.w * 0.5f, canvas.y + caption_h + ring_h * 0.5f};
    const float fit = std::max(0.2f, (std::min(canvas.w, ring_h) * 0.5f - 8.0f) / kFitRadius);
    if (canvas_hovered && io.MouseWheel != 0.0f)
    {
        const float old_zoom = s.tech_tree_zoom;
        s.tech_tree_zoom = std::clamp(s.tech_tree_zoom * (io.MouseWheel > 0.0f ? 1.15f : 1.0f / 1.15f),
                                      0.6f, 4.0f);
        // Keep the point under the cursor fixed.
        const ImVec2 c{mid.x + s.tech_tree_pan_x, mid.y + s.tech_tree_pan_y};
        const float f = s.tech_tree_zoom / old_zoom;
        s.tech_tree_pan_x += (io.MousePos.x - c.x) * (1.0f - f);
        s.tech_tree_pan_y += (io.MousePos.y - c.y) * (1.0f - f);
    }
    if (canvas_hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Right)
                           || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)))
    {
        s.tech_tree_pan_x += io.MouseDelta.x;
        s.tech_tree_pan_y += io.MouseDelta.y;
    }
    const float k = fit * s.tech_tree_zoom;               // base unit -> px
    const ImVec2 centre{mid.x + s.tech_tree_pan_x, mid.y + s.tech_tree_pan_y};
    // A raw polar point (base units), and the RING's point: the layout's radii
    // pushed out by a hub so each tree's name fits inside its own sector,
    // between its root travel nodes, without the three meeting at the centre
    // (Ben, 2026-10-10). The rim stays where it was; the inner rings compress.
    const auto raw = [&](float ang, float r) {
        return ImVec2{centre.x + std::cos(ang * kDeg) * r * k, centre.y + std::sin(ang * kDeg) * r * k};
    };
    const auto at = [&](float ang, float r) {
        return raw(ang, kHubRadius + r * (kRimRadius - kHubRadius) / kRimRadius);
    };

    const nation_view v = view_of(w);
    const bool can_plan = v.nation != null_entity;
    const bool shift = io.KeyShift;
    const auto& L = layout();

    dl->PushClipRect({canvas.x, canvas.y}, {canvas.x + canvas.w, canvas.y + canvas.h}, true);

    // --- ring guides: the three edges between the four cost rings
    for (const float e : kRingEdge)
    {
        const ImVec2 edge = at(0.0f, e);
        dashed_circle(dl, centre, edge.x - centre.x, IM_COL32(120, 126, 138, 60), 1.0f, 72);
    }
    // Sector dividers between the three trees.
    for (int t = 0; t < 3; ++t)
    {
        const float ang = kTreeCentre[t] + 60.0f;
        dashed_line(dl, at(ang, 20.0f), at(ang, kLabelRadius), IM_COL32(120, 126, 138, 70), 1.0f, 6.0f);
    }

    std::array<ImVec2, dg::node_count> scr{};
    for (int i = 0; i < dg::node_count; ++i)
        scr[static_cast<std::size_t>(i)] = at(L[static_cast<std::size_t>(i)].ang, L[static_cast<std::size_t>(i)].r);

    // --- links: requirement -> node; AND links (2+ groups) in the accent
    for (int i = 0; i < dg::node_count; ++i)
    {
        const dg::node& n = node_at(i);
        const bool and_link = n.group_count >= 2;
        const node_state st = v.state(i);
        const float dim = st == node_state::locked ? 0.55f : 1.0f;
        for (int g = 0; g < n.group_count; ++g)
            for (const std::int16_t m : n.groups[static_cast<std::size_t>(g)])
            {
                if (m < 0)
                    continue;
                ImU32 col = and_link ? with_alpha(palette::research_accent, 0.85f * dim)
                                     : with_alpha(palette::research_link, dim);
                if (v.held(m) && v.held(i))
                    col = with_alpha(palette::research_held, 0.85f);
                if (n.is_placeholder)
                    dashed_line(dl, scr[static_cast<std::size_t>(m)], scr[static_cast<std::size_t>(i)],
                                with_alpha(palette::research_muted, dim), 1.0f, 4.0f);
                else
                    dl->AddLine(scr[static_cast<std::size_t>(m)], scr[static_cast<std::size_t>(i)], col,
                                and_link ? 1.8f : 1.1f);
            }
        // A pick-one pair: a dashed danger tie between its two sides.
        if (n.excludes > i)
            dashed_line(dl, scr[static_cast<std::size_t>(i)], scr[static_cast<std::size_t>(n.excludes)],
                        palette::research_danger, 1.4f, 3.0f);
    }

    // --- hover resolution (nearest node within its glyph + a margin)
    int hovered = -1;
    if (canvas_hovered)
    {
        float best = 1e9f;
        for (int i = 0; i < dg::node_count; ++i)
        {
            const float dx = io.MousePos.x - scr[static_cast<std::size_t>(i)].x;
            const float dy = io.MousePos.y - scr[static_cast<std::size_t>(i)].y;
            const float d2 = dx * dx + dy * dy;
            const float reach = node_radius(node_at(i), k) + 5.0f;
            if (d2 <= reach * reach && d2 < best)
            {
                best = d2;
                hovered = i;
            }
        }
    }

    // Holding Shift over a node previews what a shift-click would queue (or,
    // on a queued node, what it would take off).
    std::vector<std::uint16_t> preview;
    if (can_plan && shift && hovered >= 0)
    {
        preview = queued_dependents(w, v.nation, hovered);
        if (preview.empty())
            refusal_reason(w, v.nation, hovered, &preview);
    }

    // --- nodes
    ImFont* font = ImGui::GetFont();
    const float small = ImGui::GetFontSize() * 0.8f;
    for (int i = 0; i < dg::node_count; ++i)
    {
        const dg::node& n = node_at(i);
        const ImVec2 c = scr[static_cast<std::size_t>(i)];
        const float r = node_radius(n, k);
        const node_state st = v.state(i);
        draw_node(dl, c, r, n, st);
        if (std::find(preview.begin(), preview.end(), static_cast<std::uint16_t>(i)) != preview.end())
            dl->AddCircle(c, r + 5.0f, with_alpha(palette::hover, 0.8f), 0, 1.4f);
        if (st == node_state::queued)
        {
            dl->AddCircle(c, r + 3.0f, palette::research_queued, 0, 1.6f);
            char b[8];
            std::snprintf(b, sizeof b, "%d", v.queue_pos[static_cast<std::size_t>(i)]);
            dl->AddText(font, small, {c.x + r + 2.0f, c.y - r - small}, palette::research_queued, b); // fit-exempt: ring badge
        }
        if (i == hovered)
            dl->AddCircle(c, r + 4.0f, palette::hover, 0, 2.0f);
    }

    // --- labels: rim majors, branch names on the rim, tree names inside
    for (int i = 0; i < dg::node_count; ++i)
    {
        const dg::node& n = node_at(i);
        if (!is_major(n) || n.ring != 4)
            continue;
        const node_place& pl = L[static_cast<std::size_t>(i)];
        const float r = node_radius(n, k);
        const ImVec2 lp = at(pl.ang, pl.r + (r + 4.0f) / k);
        // A narrow centre band: a branch's two rim majors straddle its axis, so
        // at the top and bottom of the ring their labels run apart, not over
        // each other.
        outward_text(dl, font, small, lp, pl.ang,
                     n.is_placeholder ? palette::research_muted : palette::text_secondary, n.name,
                     0.04f);
    }
    for (int b = 0; b < dg::branch_count; ++b)
    {
        const float a = branch_angle(b);
        outward_text(dl, font, ImGui::GetFontSize(), at(a, kLabelRadius), a, palette::research_accent,
                     dg::branches[static_cast<std::size_t>(b)].name);
    }
    // Tree names inside their own sectors (Ben, 2026-10-10): each on its
    // tree's axis in the hub, inside the ring of its three root travel nodes,
    // so the three never run together at the centre.
    for (int t = 0; t < 3; ++t)
    {
        const char* tn = dg::branches[static_cast<std::size_t>(t * 3)].tree_name;
        const ImVec2 ts = font->CalcTextSizeA(small, FLT_MAX, 0.0f, tn);
        const ImVec2 c  = raw(kTreeCentre[t], kNameRadius);
        dl->AddText(font, small, {c.x - ts.x * 0.5f, c.y - ts.y * 0.5f}, palette::research_accent,
                    tn); // fit-exempt: free-standing ring label
    }
    // Ring numerals on the one boundary no joint sits on.
    {
        const float ang = 49.0f;
        const float mids[4] = {96.0f, 150.0f, 212.0f, 292.0f};
        for (int r = 0; r < 4; ++r)
        {
            char b[8];
            std::snprintf(b, sizeof b, "R%d", r + 1);
            const ImVec2 c = at(ang, mids[r]);
            const ImVec2 ts = font->CalcTextSizeA(small, FLT_MAX, 0.0f, b);
            dl->AddText(font, small, {c.x - ts.x * 0.5f, c.y - ts.y * 0.5f},
                        IM_COL32(150, 156, 168, 160), b); // fit-exempt: ring numeral
        }
    }

    dl->PopClipRect();

    // --- the mode line and the legend, in the canvas corners
    {
        const char* line = can_plan
                               ? "Hover a node to read it. Shift-click a node to queue it, with its "
                                 "requirements ahead of it; shift-click a queued node to take it off."
                               : "Hover a node to read it. No corporation is seated, so there is no "
                                 "nation to plan for.";
        dl->AddText({canvas.x + 12.0f, canvas.y + 10.0f}, palette::text_secondary, line); // fit-exempt: canvas caption
        if (!s.research_message.empty() && can_plan)
            dl->AddText({canvas.x + 12.0f, canvas.y + 10.0f + ImGui::GetTextLineHeightWithSpacing()},
                        palette::research_warning, s.research_message.c_str()); // fit-exempt: canvas caption
        const float lh = ImGui::GetFontSize() * 0.9f + 6.0f;
        legend(dl, {canvas.x + 12.0f, canvas.y + canvas.h - 10.0f * lh - 12.0f}, k);
    }

    // --- the press: SHIFT + left click queues (or toggles a queued node off)
    // through the seam (Ben, 2026-10-10). A plain click only inspects.
    if (can_plan && shift && hovered >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        press_node(w, s, v.nation, hovered);

    // --- publish node positions for the verify API
    s.tech_tree_node_screen.resize(static_cast<std::size_t>(dg::node_count) * 2);
    for (int i = 0; i < dg::node_count; ++i)
    {
        s.tech_tree_node_screen[static_cast<std::size_t>(i) * 2]     = scr[static_cast<std::size_t>(i)].x;
        s.tech_tree_node_screen[static_cast<std::size_t>(i) * 2 + 1] = scr[static_cast<std::size_t>(i)].y;
    }

    ImGui::End();

    // --- the shared hover card: glance, then stick (TOOLTIP.md § The model)
    const float dt = s.fixed_frame_clock ? kVerifyFrameSeconds
                                         : std::min(io.DeltaTime, kHoverMaxFrameSeconds);
    if (s.tech_tree_hover_stuck)
    {
        const bool inside = io.MousePos.x >= s.tech_tree_card_min_x - kHoverCardExitPadPx
                            && io.MousePos.x <= s.tech_tree_card_max_x + kHoverCardExitPadPx
                            && io.MousePos.y >= s.tech_tree_card_min_y - kHoverCardExitPadPx
                            && io.MousePos.y <= s.tech_tree_card_max_y + kHoverCardExitPadPx;
        if (!inside && hovered != s.tech_tree_hover_node)
        {
            s.tech_tree_hover_stuck = false;
            s.tech_tree_hover_node  = -1;
            s.tech_tree_hover_secs  = 0.0f;
        }
    }
    if (!s.tech_tree_hover_stuck)
    {
        if (hovered != s.tech_tree_hover_node)
        {
            s.tech_tree_hover_node = hovered;
            s.tech_tree_hover_secs = 0.0f;
        }
        else if (hovered >= 0)
            s.tech_tree_hover_secs += dt;
        if (hovered >= 0 && s.tech_tree_hover_secs >= kHoverStickDelaySec - kHoverThresholdEpsilon)
        {
            s.tech_tree_hover_stuck   = true;
            s.tech_tree_card_anchor_x = io.MousePos.x;
            s.tech_tree_card_anchor_y = io.MousePos.y;
        }
    }
    if (s.tech_tree_hover_node >= 0
        && s.tech_tree_hover_secs >= kHoverAppearDelaySec - kHoverThresholdEpsilon)
    {
        const ImVec2 anchor = s.tech_tree_hover_stuck
                                  ? ImVec2{s.tech_tree_card_anchor_x, s.tech_tree_card_anchor_y}
                                  : io.MousePos;
        const int node = s.tech_tree_hover_node;
        ImVec2 mn, mx;
        draw_hover_card(anchor, [&] { hover_body(w, p, v, node); }, &mn, &mx);
        s.tech_tree_card_min_x = mn.x;
        s.tech_tree_card_min_y = mn.y;
        s.tech_tree_card_max_x = mx.x;
        s.tech_tree_card_max_y = mx.y;
    }
}

} // namespace ui
