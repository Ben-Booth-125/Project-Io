#include "tech_tree_panel.hpp"

#include "foldout_column.hpp"
#include "format.hpp"
#include "hover_card.hpp"
#include "presentation.hpp"

#include "../world/digitisation_tree_data.hpp"
#include "../world/space_tech_keys.hpp" // BL-1613: the gate's Rocketry key

#include <imgui.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <imgui_internal.h> // ImRect: the rim labels' clearing
#include <vector>

namespace ui {

namespace {

namespace dg = io::dg;

constexpr float kPi = 3.14159265358979f;
constexpr float kDeg = kPi / 180.0f;

// ---------------------------------------------------------------------------
// Layout — the approved render (Ben's design session, 2026-10-10)
// ---------------------------------------------------------------------------
// Three tree sectors — Production centred at the top (-90), angles growing
// clockwise on screen — of three, three and four branch wedges 34 degrees apart,
// with no centre node (the constants below). Along a branch's
// angle `a`, in BASE radius units (scaled to the canvas):
//
//   Travel I 48 -> Travel II 74 -> cluster A minors 102 (a +-6) -> major A 134
//   -> two arms at a-9 / a+9: Travel III / V 164 -> ... -> the rim majors 264
//   -> mastery 318.
//
// An arm is laid by DEPTH (BL-1436: kit clusters lengthen the arms, DIGITISATION_TREE.md
// § Kit): Travel III / V is depth 0, each node one past its deepest requirement in the
// arm, a cluster's minors (arm +-5) sharing a depth; the arm's steps divide 164 -> 264
// evenly, so an arm of travel, travel, minors, major reads as before (164, 197, 231, 264)
// and a longer arm packs its clusters closer. A node needing BOTH arms (Wind Power) sits
// on the branch's axis at 174, between ring edges 118 and 179: past the arms' first nodes
// (164) and short of the ring 3 guide.
//
// Pair majors sit +-6 degrees off their slot; a quest diamond beside its major;
// a joint on the boundary between the two arms it joins, at 214.
//
// The layout is a pure function of the store's ids, so it is computed once.

// Ten branches on the ring: Production 3, Network 3, State 4 (DIGITISATION_TREE.md § Naval).
// Inside a tree they sit kBranchStep apart; the three tree boundaries share what is left.
constexpr int   kTreeFirstBranch[4] = {0, 3, 6, 10};
static_assert(kTreeFirstBranch[3] == dg::branch_count, "the ring's sectors name every branch");
constexpr float kBranchStep    = 34.0f;
constexpr float kTreeGap       = (360.0f - 7.0f * kBranchStep) / 3.0f;
constexpr float kFirstBranchAngle = -90.0f - kBranchStep; ///< Production stays centred at the top.
constexpr float kRimRadius     = 318.0f; ///< The mastery ring, the outermost node.
constexpr float kLabelRadius   = 372.0f; ///< Branch labels.
constexpr float kFitRadius     = 396.0f; ///< What the default zoom fits inside the canvas.
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
    int t = 0;
    while (t < 2 && b >= kTreeFirstBranch[t + 1])
        ++t;
    return kFirstBranchAngle + static_cast<float>(b - t) * kBranchStep + static_cast<float>(t) * kTreeGap;
}

/// A tree's axis: halfway between its first and last branch.
float tree_centre(int t)
{
    return (branch_angle(kTreeFirstBranch[t]) + branch_angle(kTreeFirstBranch[t + 1] - 1)) * 0.5f;
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
        if (c0 == 'T' && (c1 == '1' || c1 == '2'))
            p = {a, c1 == '1' ? 48.0f : 74.0f};
        else if (c0 == 'M')
            p = {a, kRimRadius};
        else if (c0 == 'A')
        {
            const bool minor = (c1 == '1' || c1 == '2');
            if (minor)
            {
                const char other[3] = {c0, c1 == '1' ? '2' : '1', '\0'};
                const float d = has_slot(n.id, other) ? (c1 == '1' ? -6.0f : 6.0f) : 0.0f;
                p = {a + d, 102.0f};
            }
            else
            {
                // The cluster's major, or one side of a pick-one pair ('a' / 'b').
                const float d = c1 == 'a' ? -6.0f : (c1 == 'b' ? 6.0f : 0.0f);
                p = {a + d, 134.0f};
            }
        }
        else
            continue; // an arm node: laid by depth below
        L[static_cast<std::size_t>(i)] = p;
        placed[static_cast<std::size_t>(i)] = true;
    }

    // The arms, by depth (see the header): Travel III (arm -1) and Travel V (arm +1) are
    // depth 0; every other arm node is one past its deepest requirement, and takes its
    // first requirement's arm. Requirements may sit later in the store (the kit nodes are
    // appended), so relax until nothing moves.
    std::array<int, dg::node_count> depth{}, side{};
    depth.fill(-1);
    const auto arm_node = [&](int i) {
        const dg::node& n = dg::nodes[static_cast<std::size_t>(i)];
        return !n.is_joint && !n.is_quest && !placed[static_cast<std::size_t>(i)]
            && branch_index(n.branch) >= 0;
    };
    for (int i = 0; i < dg::node_count; ++i)
    {
        if (!arm_node(i))
            continue;
        const char* slot = slot_of(dg::nodes[static_cast<std::size_t>(i)].id);
        if (slot[0] == 'T' && (slot[1] == '3' || slot[1] == '5'))
        {
            depth[static_cast<std::size_t>(i)] = 0;
            side[static_cast<std::size_t>(i)]  = slot[1] == '3' ? -1 : 1;
        }
    }
    for (bool moved = true; moved;)
    {
        moved = false;
        for (int i = 0; i < dg::node_count; ++i)
        {
            const dg::node& n = dg::nodes[static_cast<std::size_t>(i)];
            if (!arm_node(i) || depth[static_cast<std::size_t>(i)] >= 0 || n.group_count == 0)
                continue;
            int d = -1, s = 0;
            bool ready = true;
            for (int g = 0; g < n.group_count && ready; ++g)
            {
                const int m = n.groups[static_cast<std::size_t>(g)][0];
                if (m < 0 || !arm_node(m))
                    continue; // a requirement outside the arm (none today)
                if (depth[static_cast<std::size_t>(m)] < 0)
                    ready = false;
                else if (depth[static_cast<std::size_t>(m)] > d)
                {
                    d = depth[static_cast<std::size_t>(m)];
                    if (s == 0) s = side[static_cast<std::size_t>(m)];
                }
            }
            if (!ready || d < 0)
                continue;
            depth[static_cast<std::size_t>(i)] = d + 1;
            side[static_cast<std::size_t>(i)]  = s;
            moved = true;
        }
    }
    // Each (branch, arm)'s deepest step, so its steps divide 164 -> 264.
    std::array<int, dg::branch_count * 2> deepest{};
    for (int i = 0; i < dg::node_count; ++i)
        if (depth[static_cast<std::size_t>(i)] >= 0)
        {
            const int k = branch_index(dg::nodes[static_cast<std::size_t>(i)].branch) * 2
                        + (side[static_cast<std::size_t>(i)] > 0 ? 1 : 0);
            deepest[static_cast<std::size_t>(k)] = std::max(deepest[static_cast<std::size_t>(k)],
                                                            depth[static_cast<std::size_t>(i)]);
        }
    for (int i = 0; i < dg::node_count; ++i)
    {
        const int dep = depth[static_cast<std::size_t>(i)];
        if (dep < 0)
            continue;
        const dg::node& n = dg::nodes[static_cast<std::size_t>(i)];
        const int b = branch_index(n.branch);
        const int s = side[static_cast<std::size_t>(i)];
        const int top = std::max(1, deepest[static_cast<std::size_t>(b * 2 + (s > 0 ? 1 : 0))]);
        const float arm = branch_angle(b) + (s > 0 ? 9.0f : -9.0f);
        const char* slot = slot_of(n.id);
        float d = 0.0f;
        if (slot[0] != 'T' && (slot[1] == '1' || slot[1] == '2'))
        {
            const char other[3] = {slot[0], slot[1] == '1' ? '2' : '1', '\0'};
            d = has_slot(n.id, other) ? (slot[1] == '1' ? -5.0f : 5.0f) : 0.0f;
        }
        else if (slot[1] == 'a' || slot[1] == 'b')
            d = slot[1] == 'a' ? -6.0f : 6.0f;
        bool needs_minus = false, needs_plus = false;
        for (int g = 0; g < n.group_count; ++g)
        {
            const int m = n.groups[static_cast<std::size_t>(g)][0];
            if (m < 0 || depth[static_cast<std::size_t>(m)] < 0)
                continue;
            (side[static_cast<std::size_t>(m)] > 0 ? needs_plus : needs_minus) = true;
        }
        if (needs_minus && needs_plus)
            L[static_cast<std::size_t>(i)] = {branch_angle(b), 174.0f};
        else
            L[static_cast<std::size_t>(i)] = {arm + d, 164.0f + 100.0f * static_cast<float>(dep) / static_cast<float>(top)};
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

/// What a node's glyph reads off it - the same for either age's tree (BL-1613:
/// the two generated node structs share the grammar, RESEARCH.md § The shape of
/// an age's tree), so both trees draw through one glyph set.
struct node_look
{
    dg::tier t           = dg::tier::travel;
    bool     placeholder = false;
    bool     mastery     = false;
    bool     joint       = false;
    bool     pair        = false;
};

node_look look_of(const dg::node& n)
{
    return {n.t, n.is_placeholder, n.is_mastery, n.is_joint, n.excludes >= 0};
}

node_look look_of(const io::es::node& n)
{
    return {static_cast<dg::tier>(n.t), n.is_placeholder, n.is_mastery, n.is_joint, n.excludes >= 0};
}

/// The drawn radius of a node at canvas scale @p k (k = 1 at the default fit).
float look_radius(const node_look& n, float k)
{
    float r = 3.5f;
    if (n.mastery || n.joint)          r = 6.0f;
    else if (n.t == dg::tier::minor)   r = 5.0f;
    else if (n.t == dg::tier::major)   r = 9.0f;
    else if (n.t == dg::tier::quest)   r = 7.0f;
    return r * std::clamp(k, 0.8f, 1.8f);
}

float node_radius(const dg::node& n, float k) { return look_radius(look_of(n), k); }

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
        case enqueue_result::refused_locked: return std::string(n.name) + " is locked."; // BL-1612: Early Space only
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
void draw_look(ImDrawList* dl, ImVec2 c, float r, const node_look& n, node_state st)
{
    const bool  held   = st == node_state::held;
    const float dim    = st == node_state::locked ? 0.45f : 1.0f;
    const ImU32 held_c = palette::research_held;

    if (n.placeholder)
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
    if (n.mastery || n.joint)
    {
        const ImU32 a = with_alpha(palette::research_accent, dim);
        if (held)
            dl->AddCircleFilled(c, r, held_c);
        dl->AddCircle(c, r, held ? held_c : a, 0, n.mastery ? 2.4f : 1.8f);
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
            if (n.pair)
                dl->AddCircle(c, r + 1.5f, with_alpha(palette::research_danger, dim), 0, 2.4f);
            break;
    }
}

void draw_node(ImDrawList* dl, ImVec2 c, float r, const dg::node& n, node_state st)
{
    draw_look(dl, c, r, look_of(n), st);
}

/// Text anchored outward from the ring's centre along @p ang (degrees): to the
/// right of the point on the right half, left on the left, centred top/bottom.
/// A label already on the ring. Each one placed steps a line further from the centre until it
/// clears those before it, so ten branches' rim names never print over each other.
void outward_text(ImDrawList* dl, ImFont* font, float size, ImVec2 at, float ang, ImU32 col,
                  const char* text, float centre_band, std::vector<ImRect>& placed)
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
    const float step = cy < 0.0f ? -ts.y : ts.y;
    const auto clashes = [&](const ImVec2 q) {
        const ImRect box(q, {q.x + ts.x, q.y + ts.y});
        for (const ImRect& o : placed)
            if (o.Overlaps(box))
                return true;
        return false;
    };
    for (int tries = 0; tries < 8 && clashes(p); ++tries)
        p.y += step;
    placed.emplace_back(p, ImVec2{p.x + ts.x, p.y + ts.y});
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

/// A plain click on node @p i: it becomes the research target (Ben, 2026-11-11). The
/// queue is emptied, the node is queued with its unheld requirements ahead of it, and
/// the old entries are queued again behind it in their order - one an exclusion now
/// refuses, or one already queued as a requirement, is refused by the seam and dropped.
/// The pool stays. Shift-click (press_node) only adds to the queue.
void target_node(const world& w, ui_state& s, entity_id nation, int i)
{
    std::vector<std::uint16_t> plan;
    const auto sit = w.nation_tech.find(nation);
    const std::vector<std::uint16_t> old = sit != w.nation_tech.end() ? sit->second.queue
                                                                      : std::vector<std::uint16_t>{};
    if (!old.empty() && old.front() == static_cast<std::uint16_t>(i))
    {
        s.research_message = std::string("Already researching ") + node_at(i).name + ".";
        return;
    }
    // Refusals read against the queue as it will be: empty, so only held, pick-one
    // and unknown-node reasons remain.
    const bool queued_now = std::find(old.begin(), old.end(), static_cast<std::uint16_t>(i)) != old.end();
    if (!queued_now)
    {
        const std::string why = refusal_reason(w, nation, i, &plan);
        if (!why.empty())
        {
            s.research_message = why;
            return;
        }
    }
    corp_command cmd;
    cmd.corp = w.player_entity;
    if (!old.empty())
    {
        cmd.verb = corp_verb::clear_research;
        s.pending_research_commands.push_back(cmd);
    }
    cmd.verb  = corp_verb::queue_research;
    cmd.order = static_cast<std::uint32_t>(i);
    s.pending_research_commands.push_back(cmd);
    for (const std::uint16_t e : old)
    {
        if (e == static_cast<std::uint16_t>(i))
            continue;
        cmd.order = e;
        s.pending_research_commands.push_back(cmd);
    }
    s.research_message = std::string("Researching ") + node_at(i).name + " next.";
}

/// The column's two projections — RP a quarter and each queue entry's arrival
/// — recomputed only when what they read changes: the day, the nation, its
/// treasury, pool, held set or queue (cold review of BL-1276: they ran every
/// frame). View state only; nothing here reaches the sim.
// BL-1613: the Early Space reads the column shares with the canvas (defined
// with the Early Space tree below).
std::string gate_missing(const space_gate_status& g);
const char* es_tier_label(const io::es::node& n);

struct research_projection
{
    bool                            valid    = false;
    int                             day      = -1;
    entity_id                       nation   = null_entity;
    float                           treasury = 0.0f;
    std::int64_t                    pool     = 0;
    std::bitset<dg::node_count>     held;
    std::vector<std::uint16_t>      queue;
    std::bitset<io::es::node_count> es_held;  ///< BL-1613: the Early Space half
    std::vector<std::uint16_t>      es_queue;
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
    const std::vector<std::uint16_t>& es_queue = st != nullptr ? st->es_queue : none;
    const std::bitset<io::es::node_count> es_held = st != nullptr ? st->es_held : std::bitset<io::es::node_count>{};
    if (c.valid && c.day == w.current_day_tick && c.nation == nation && c.treasury == treasury
        && c.pool == pool && c.held == held && c.queue == queue && c.es_held == es_held
        && c.es_queue == es_queue)
        return c;
    c.valid      = true;
    c.day        = w.current_day_tick;
    c.nation     = nation;
    c.treasury   = treasury;
    c.pool       = pool;
    c.held       = held;
    c.queue      = queue;
    c.es_held    = es_held;
    c.es_queue   = es_queue;
    c.rate_milli = estimate_rp_per_quarter_milli(w, nation, p);
    c.arrival    = project_queue_arrivals(w, nation, p);
    return c;
}

/// "Mar 1963" for the month @p months_ahead after the current one (BL-1642: a
/// projection step is an econ tick, a month).
std::string quarter_label(const world& w, int months_ahead)
{
    const fmt::calendar_date d = fmt::date_from_day(
        static_cast<std::uint64_t>(std::max(0, w.current_day_tick)));
    const int idx = d.year * 12 + (d.month - 1) + months_ahead;
    char b[24];
    std::snprintf(b, sizeof b, "%s %d", fmt::month_abbrev(idx % 12 + 1), idx / 12);
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
        ImGui::TextWrapped("%lld RP a month | %lld RP in hand | %d of %d held",
                           static_cast<long long>(rate / k_milli_per_rp),
                           static_cast<long long>((st != nullptr ? st->pool_milli : 0) / k_milli_per_rp),
                           st != nullptr ? static_cast<int>(st->held.count()) : 0, dg::node_count);
        ImGui::PopStyleColor();
        // BL-1613: the next age's tree, and the gate that opens it.
        {
            const space_gate_status gate = nation_space_gate_status(w, nation, p);
            ImGui::PushStyleColor(ImGuiCol_Text, gate.met() ? palette::text_secondary : palette::research_danger);
            if (gate.met())
                ImGui::TextWrapped("Early Space: %d of %d held. The gate into space is met.",
                                   st != nullptr ? static_cast<int>(st->es_held.count()) : 0, io::es::node_count);
            else
                ImGui::TextWrapped("Early Space is locked: the gate into space needs %s.",
                                   gate_missing(gate).c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Separator();

        ImGui::TextWrapped("Click a node to research it next; shift-click to add it to the "
                           "queue, or to take a queued node off. Hover reads it.");
        ImGui::Spacing();

        // --- the queue
        // BL-1613: ONE logical queue - the Digitisation entries, then the Early
        // Space tail behind them; `arrival` is indexed the same way.
        const std::vector<int>& arrival = proj.arrival;
        const std::size_t dg_n  = st != nullptr ? st->queue.size() : 0;
        const std::size_t total = st != nullptr ? dg_n + st->es_queue.size() : 0;
        const bool empty = total == 0;
        ImGui::Text("Queue (%d)", static_cast<int>(total));
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
                                                       * (static_cast<float>(total) + 1.5f))}))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#");
            ImGui::TableSetupColumn("Node", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Tier");
            ImGui::TableSetupColumn("RP (today)"); // BL-1324: costs at today's price index
            ImGui::TableSetupColumn("Arrives");
            ImGui::TableHeadersRow();
            bool after_deed = false;
            const std::int64_t price = research_price_ppm(w, p);
            for (std::size_t q = 0; q < total; ++q)
            {
                const bool es_row = q >= dg_n;
                const int  i      = es_row ? st->es_queue[q - dg_n] : st->queue[q];
                const char* name  = es_row ? io::es::nodes[static_cast<std::size_t>(i)].name : node_at(i).name;
                const char* adv   = es_row ? io::es::nodes[static_cast<std::size_t>(i)].advantage : node_at(i).advantage;
                const bool quest  = es_row ? io::es::nodes[static_cast<std::size_t>(i)].is_quest : node_at(i).is_quest;
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", static_cast<int>(q) + 1);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(name);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s%s\n%s", name, es_row ? " (Early Space)" : "", adv);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(es_row ? es_tier_label(io::es::nodes[static_cast<std::size_t>(i)])
                                              : tier_label(node_at(i)));
                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(
                                        (es_row ? early_space_node_cost_milli(i, p, price) : node_cost_milli(i, p, price))
                                        / k_milli_per_rp));
                ImGui::TableNextColumn();
                const int a = q < arrival.size() ? arrival[q] : -1;
                if (a > 0)
                    ImGui::TextUnformatted(quarter_label(w, a).c_str());
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, palette::research_warning);
                    ImGui::TextUnformatted(quest ? "on its deed" : (after_deed ? "after a deed" : "not in 120 y"));
                    ImGui::PopStyleColor();
                    if (quest)
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
                // BL-1613: the last entry of the one queue is the Early Space tail's, if it has one.
                s.research_message = std::string("Removed ")
                                     + (st->es_queue.empty() ? node_at(st->queue.back()).name
                                                             : io::es::nodes[st->es_queue.back()].name)
                                     + ".";
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

namespace {

/// The Digitisation ring (BL-1276). Returns the hovered node (an `io::dg`
/// index), or -1.
int draw_dg_ring(const world& w, ui_state& s, ImDrawList* dl, const foldout_rect& canvas,
                 bool canvas_hovered)
{
    const ImGuiIO& io = ImGui::GetIO();
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
        const float ang = branch_angle(kTreeFirstBranch[t + 1] - 1) + kTreeGap * 0.5f;
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

    // --- labels: branch names on the rim, then rim majors around them, tree names inside
    std::vector<ImRect> placed_labels;
    for (int b = 0; b < dg::branch_count; ++b)
    {
        const float a = branch_angle(b);
        outward_text(dl, font, ImGui::GetFontSize(), at(a, kLabelRadius), a, palette::research_accent,
                     dg::branches[static_cast<std::size_t>(b)].name, 0.35f, placed_labels);
    }
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
                     0.04f, placed_labels);
    }
    // Tree names inside their own sectors (Ben, 2026-10-10): each on its
    // tree's axis in the hub, inside the ring of its root travel nodes,
    // so the three never run together at the centre.
    for (int t = 0; t < 3; ++t)
    {
        const char* tn = dg::branches[static_cast<std::size_t>(kTreeFirstBranch[t])].tree_name;
        const ImVec2 ts = font->CalcTextSizeA(small, FLT_MAX, 0.0f, tn);
        const ImVec2 c  = raw(tree_centre(t), kNameRadius);
        dl->AddText(font, small, {c.x - ts.x * 0.5f, c.y - ts.y * 0.5f}, palette::research_accent,
                    tn); // fit-exempt: free-standing ring label
    }
    // Ring numerals on the Markets | Freight boundary, which no joint sits on.
    {
        const float ang = (branch_angle(4) + branch_angle(5)) * 0.5f;
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
                               ? "Hover a node to read it. Click a node to research it next, with its "
                                 "requirements ahead of it; shift-click to add it to the queue, or to "
                                 "take a queued node off."
                               : "Hover a node to read it. No corporation is seated, so there is no "
                                 "nation to plan for.";
        dl->AddText({canvas.x + 12.0f, canvas.y + 10.0f}, palette::text_secondary, line); // fit-exempt: canvas caption
        if (!s.research_message.empty() && can_plan)
            dl->AddText({canvas.x + 12.0f, canvas.y + 10.0f + ImGui::GetTextLineHeightWithSpacing()},
                        palette::research_warning, s.research_message.c_str()); // fit-exempt: canvas caption
        const float lh = ImGui::GetFontSize() * 0.9f + 6.0f;
        legend(dl, {canvas.x + 12.0f, canvas.y + canvas.h - 10.0f * lh - 12.0f}, k);
    }

    // --- the press, through the seam. SHIFT + left click adds to the queue (or
    // toggles a queued node off) (Ben, 2026-10-10); a plain left click makes the node
    // the research target, the rest of the queue behind it (Ben, 2026-11-11).
    if (can_plan && hovered >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (shift)
            press_node(w, s, v.nation, hovered);
        else
            target_node(w, s, v.nation, hovered);
    }

    // --- publish node positions for the verify API
    s.tech_tree_node_screen.resize(static_cast<std::size_t>(dg::node_count) * 2);
    for (int i = 0; i < dg::node_count; ++i)
    {
        s.tech_tree_node_screen[static_cast<std::size_t>(i) * 2]     = scr[static_cast<std::size_t>(i)].x;
        s.tech_tree_node_screen[static_cast<std::size_t>(i) * 2 + 1] = scr[static_cast<std::size_t>(i)].y;
    }

    return hovered;
}

// ---------------------------------------------------------------------------
// BL-1613: the Early Space tree (`io::es`) - the next age's tree, keyed branch by
// branch by the Digitisation quests and opened by the gate into space
// (RESEARCH.md § Quests key the next age; ERAS.md § Era 1 -> Era 2 gate).
// ---------------------------------------------------------------------------
// Three branches, one per tree, so it is laid as three COLUMNS rather than a
// ring: each branch flows top to bottom by depth (one row per step past its
// root), a cluster's minors either side of the step above them, the two arms
// after the first major 1.8 units either side, a node needing two groups
// between them, and a quest one step beside its requirement. Base units: a row
// is 1, a column kEsColumn. A pure function of the store, computed once.

namespace es = io::es;

constexpr float kEsColumn   = 8.0f;  ///< Column spacing, base units.
constexpr float kEsArm      = 1.8f;  ///< An arm's offset from its parent.
constexpr float kEsMinor    = 0.7f;  ///< A cluster minor's offset from its parent.
constexpr float kEsQuest    = 1.3f;  ///< A quest's offset beside its requirement.
constexpr float kEsHeadRows = 2.2f;  ///< Rows above the roots the branch headers take.

struct es_place
{
    float x = 0.0f; ///< Across, base units, 0 = the middle column's axis.
    float y = 0.0f; ///< Down, rows.
};

int es_branch_index(const char* key)
{
    for (int b = 0; b < es::branch_count; ++b)
        if (std::strcmp(es::branches[static_cast<std::size_t>(b)].key, key) == 0)
            return b;
    return -1;
}

std::array<es_place, es::node_count> compute_es_layout()
{
    std::array<es_place, es::node_count> L{};
    std::array<int, es::node_count> depth{};
    depth.fill(-1);
    for (bool moved = true; moved;)
    {
        moved = false;
        for (int i = 0; i < es::node_count; ++i)
        {
            if (depth[static_cast<std::size_t>(i)] >= 0)
                continue;
            const es::node& n = es::nodes[static_cast<std::size_t>(i)];
            int  d     = -1;
            bool ready = true;
            for (int g = 0; g < n.group_count && ready; ++g)
            {
                const int m = n.groups[static_cast<std::size_t>(g)][0];
                if (m < 0)
                    continue;
                if (depth[static_cast<std::size_t>(m)] < 0)
                    ready = false;
                else
                    d = std::max(d, depth[static_cast<std::size_t>(m)]);
            }
            if (!ready)
                continue;
            depth[static_cast<std::size_t>(i)] = d + 1; // a root: -1 + 1 = 0
            moved = true;
        }
    }
    int max_depth = 0;
    for (const int d : depth)
        max_depth = std::max(max_depth, d);

    // Shallow first, so every requirement is placed before what needs it.
    for (int d = 0; d <= max_depth; ++d)
        for (int i = 0; i < es::node_count; ++i)
        {
            if (depth[static_cast<std::size_t>(i)] != d)
                continue;
            const es::node& n = es::nodes[static_cast<std::size_t>(i)];
            const float axis = static_cast<float>(std::max(0, es_branch_index(n.branch)) - 1) * kEsColumn;
            es_place pl{axis, static_cast<float>(d)};
            const int r0 = n.group_count > 0 ? n.groups[0][0] : -1;
            if (n.group_count == 0 || r0 < 0)
                pl.x = axis;
            else if (n.is_quest)
            {
                // Beside its requirement, on the side AWAY from that node's label
                // (a left-arm major's label runs left, every other's right).
                const float rx = L[static_cast<std::size_t>(r0)].x;
                pl = {rx + (rx < axis - 0.1f ? kEsQuest : -kEsQuest), L[static_cast<std::size_t>(r0)].y};
            }
            else if (n.group_count >= 2)
            {
                float x = 0.0f;
                int   k = 0;
                for (int g = 0; g < n.group_count; ++g)
                    if (const int m = n.groups[static_cast<std::size_t>(g)][0]; m >= 0)
                    {
                        x += L[static_cast<std::size_t>(m)].x;
                        ++k;
                    }
                pl.x = k > 0 ? x / static_cast<float>(k) : axis;
            }
            else
            {
                // Siblings: the non-quest single-group nodes on the same parent,
                // in store order, spread either side of it.
                int idx = 0, count = 0;
                for (int j = 0; j < es::node_count; ++j)
                {
                    const es::node& o = es::nodes[static_cast<std::size_t>(j)];
                    if (o.is_quest || o.group_count != 1 || o.groups[0][0] != r0)
                        continue;
                    if (j == i)
                        idx = count;
                    ++count;
                }
                const float spread = n.t == es::tier::minor ? kEsMinor : kEsArm;
                const float off = count > 1
                                      ? (static_cast<float>(idx) - 0.5f * static_cast<float>(count - 1)) * 2.0f * spread
                                            / static_cast<float>(count - 1)
                                      : 0.0f;
                pl.x = L[static_cast<std::size_t>(r0)].x + off;
            }
            L[static_cast<std::size_t>(i)] = pl;
        }
    return L;
}

const std::array<es_place, es::node_count>& es_layout()
{
    static const std::array<es_place, es::node_count> L = compute_es_layout();
    return L;
}

float es_rows()
{
    float r = 0.0f;
    for (const es_place& p : es_layout())
        r = std::max(r, p.y);
    return r + 1.0f;
}

const es::node& es_at(int i) { return es::nodes[static_cast<std::size_t>(i)]; }

const char* es_tier_label(const es::node& n)
{
    if (n.is_mastery) return "Mastery";
    if (n.is_joint)   return "Joint";
    switch (n.t)
    {
        case es::tier::travel: return "Travel";
        case es::tier::minor:  return "Minor";
        case es::tier::major:  return "Major";
        case es::tier::quest:  return "Quest";
    }
    return "Node";
}

std::string es_requires_line(const es::node& n)
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
            s += es_at(m).name;
            first = false;
        }
    }
    return s;
}

/// The name of the Digitisation node carrying `space_dispatch` (Rocketry).
const char* rocketry_name()
{
    for (int i = 0; i < dg::node_count; ++i)
        if (node_has_mechanic(i, space_keys::dispatch))
            return node_at(i).name;
    return "Rocketry";
}

/// What the gate into space still needs, or "" when it is met.
std::string gate_missing(const space_gate_status& g)
{
    std::string s;
    const auto add = [&](const std::string& part) {
        if (!s.empty())
            s += "; ";
        s += part;
    };
    if (!g.rocketry)
        add(std::string(rocketry_name()) + " held");
    if (!g.launchpad)
        add("a staffed Launchpad on your home body");
    if (!g.propellant)
        add("propellant on a home-body market's shelf");
    return s;
}

/// What keys Early Space branch @p b, as the player reads it: the Digitisation
/// node whose holding opens it. Under the stand-in (nation_tech.cpp
/// `branch_keyed_in`) a quest whose deed is unread keys its branch through its
/// own requirement, the cluster's major, so that major is the one to name.
const char* es_branch_key_name(int b)
{
    const int q = early_space_branch_key_node(b);
    if (q < 0)
        return "?";
    const dg::node& qn = node_at(q);
    if (qn.deed_waits_on[0] != '\0' && qn.group_count > 0 && qn.groups[0][0] >= 0)
        return node_at(qn.groups[0][0]).name;
    return qn.name;
}

struct es_view
{
    entity_id                nation = null_entity;
    const nation_tech_state* st     = nullptr;
    space_gate_status        gate;
    std::array<bool, es::branch_count> keyed{};
    std::array<int, es::node_count>    queue_pos{}; ///< 1-based position on the ONE queue, 0 = not queued.

    bool held(int i) const { return st != nullptr && st->es_held.test(static_cast<std::size_t>(i)); }

    node_state state(int i) const
    {
        if (held(i))
            return node_state::held;
        if (queue_pos[static_cast<std::size_t>(i)] > 0)
            return node_state::queued;
        const int b = es_branch_index(es_at(i).branch);
        if (!gate.met() || b < 0 || !keyed[static_cast<std::size_t>(b)])
            return node_state::locked;
        const es::node& n = es_at(i);
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

es_view es_view_of(const world& w, const research_params& p)
{
    es_view v;
    v.nation = research_player_nation(w);
    if (v.nation == null_entity)
        return v;
    v.gate = nation_space_gate_status(w, v.nation, p);
    for (int b = 0; b < es::branch_count; ++b)
        v.keyed[static_cast<std::size_t>(b)] = early_space_branch_keyed(w, v.nation, b);
    if (const auto it = w.nation_tech.find(v.nation); it != w.nation_tech.end())
    {
        v.st = &it->second;
        const int ahead = static_cast<int>(it->second.queue.size()); // the Digitisation entries stand first
        for (std::size_t q = 0; q < it->second.es_queue.size(); ++q)
            if (it->second.es_queue[q] < es::node_count)
                v.queue_pos[it->second.es_queue[q]] = ahead + static_cast<int>(q) + 1;
    }
    return v;
}

/// The reason a press on Early Space node @p i is refused, or "" when it would
/// queue; @p plan receives what it would queue.
std::string es_refusal_reason(const world& w, const research_params& p, const es_view& v, int i,
                              std::vector<std::uint16_t>* plan)
{
    const es::node& n = es_at(i);
    switch (preview_enqueue_early_space(w, v.nation, i, plan, p))
    {
        case enqueue_result::queued:         return {};
        case enqueue_result::already_held:   return std::string(n.name) + " is already held.";
        case enqueue_result::already_queued: return std::string(n.name) + " is already queued.";
        case enqueue_result::refused_pair:
        case enqueue_result::invalid:        return "That node is not on this age's tree.";
        case enqueue_result::refused_locked:
        {
            if (!v.gate.met())
                return std::string(n.name) + " is locked: the gate into space needs " + gate_missing(v.gate) + ".";
            const int b = es_branch_index(n.branch);
            return std::string(n.name) + " is locked: " + es::branches[static_cast<std::size_t>(std::max(0, b))].name
                   + " opens with " + es_branch_key_name(std::max(0, b)) + ".";
        }
    }
    return {};
}

void es_hover_body(const world& w, const research_params& p, const es_view& v, int i)
{
    const es::node& n = es_at(i);
    ImGui::TextUnformatted(n.name);
    char line[160];
    std::snprintf(line, sizeof line, "Early Space | %s | ring %d | %d RP", es_tier_label(n),
                  static_cast<int>(n.ring),
                  static_cast<int>(static_cast<float>(n.cost_units) * p.rp_per_cost_unit));
    ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    ImGui::TextWrapped("%s", line);
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::TextWrapped("%s", n.advantage);
    ImGui::TextWrapped("%s", es_requires_line(n).c_str());
    if (n.is_quest)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::research_warning);
        ImGui::TextWrapped("Deed: %s", n.deed);
        ImGui::PopStyleColor();
        const char* keys = es::keys_next_branch[static_cast<std::size_t>(i)];
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
        const std::size_t off = queued_early_space_dependents(w, v.nation, i).size();
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        if (off > 2)
            ImGui::TextWrapped("Shift-click to unqueue it and the %d queued entries that need it.",
                               static_cast<int>(off) - 1);
        else if (off == 2)
            ImGui::TextWrapped("Shift-click to unqueue it and the queued entry that needs it.");
        else
            ImGui::TextWrapped("Shift-click to unqueue it.");
        ImGui::PopStyleColor();
        return;
    }
    if (st == node_state::held)
    {
        ImGui::TextUnformatted("Held");
        return;
    }
    std::vector<std::uint16_t> plan;
    const std::string why = es_refusal_reason(w, p, v, i, &plan);
    if (!why.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::research_danger);
        ImGui::TextWrapped("%s", why.c_str());
        ImGui::PopStyleColor();
        return;
    }
    std::int64_t milli = 0;
    const std::int64_t price = research_price_ppm(w, p);
    for (const std::uint16_t q : plan)
        milli += early_space_node_cost_milli(q, p, price);
    ImGui::PushStyleColor(ImGuiCol_Text, palette::positive);
    ImGui::TextWrapped("Shift-click to queue it, behind every Digitisation entry: %d node%s, %lld RP at today's prices.",
                       static_cast<int>(plan.size()), plan.size() == 1 ? "" : "s",
                       static_cast<long long>(milli / k_milli_per_rp));
    ImGui::PopStyleColor();
}

/// A shift-click on Early Space node @p i: a queued node toggles off with the
/// tail entries that need it; any other is previewed, then queued through the seam.
void es_press_node(const world& w, const research_params& p, ui_state& s, const es_view& v, int i)
{
    const std::vector<std::uint16_t> off = queued_early_space_dependents(w, v.nation, i);
    if (!off.empty())
    {
        corp_command cmd;
        cmd.corp  = w.player_entity;
        cmd.verb  = corp_verb::unqueue_space_research;
        cmd.order = static_cast<std::uint32_t>(i);
        s.pending_research_commands.push_back(cmd);
        const int after = static_cast<int>(off.size()) - 1;
        s.research_message = std::string("Unqueued ") + es_at(i).name
                             + (after > 0 ? " and " + std::to_string(after)
                                                + (after == 1 ? " entry" : " entries") + " that needed it."
                                          : ".");
        return;
    }
    std::vector<std::uint16_t> plan;
    const std::string why = es_refusal_reason(w, p, v, i, &plan);
    if (!why.empty())
    {
        s.research_message = why;
        return;
    }
    corp_command cmd;
    cmd.corp  = w.player_entity;
    cmd.verb  = corp_verb::queue_space_research;
    cmd.order = static_cast<std::uint32_t>(i);
    s.pending_research_commands.push_back(cmd);
    const int ahead = static_cast<int>(plan.size()) - 1;
    s.research_message = std::string("Queued ") + es_at(i).name
                         + (ahead > 0 ? " with " + std::to_string(ahead)
                                            + (ahead == 1 ? " requirement" : " requirements") + " ahead of it."
                                      : ".");
}

/// The wheel zoom and right/middle drag pan both trees share.
void canvas_camera(ui_state& s, ImVec2 mid, bool canvas_hovered)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (canvas_hovered && io.MouseWheel != 0.0f)
    {
        const float old_zoom = s.tech_tree_zoom;
        s.tech_tree_zoom = std::clamp(s.tech_tree_zoom * (io.MouseWheel > 0.0f ? 1.15f : 1.0f / 1.15f),
                                      0.6f, 4.0f);
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
}

/// The Early Space canvas. Returns the hovered node (an `io::es` index), or -1.
int draw_es_tree(const world& w, const research_params& p, ui_state& s, ImDrawList* dl,
                 const foldout_rect& canvas, bool canvas_hovered, const es_view& v)
{
    const ImGuiIO& io = ImGui::GetIO();
    const float caption_h = 10.0f + 2.0f * ImGui::GetTextLineHeightWithSpacing();
    const float body_h    = std::max(1.0f, canvas.h - caption_h);
    const float rows      = es_rows() + kEsHeadRows + 0.6f;
    const float cols      = 3.0f * kEsColumn;
    const ImVec2 mid{canvas.x + canvas.w * 0.5f, canvas.y + caption_h + body_h * 0.5f};
    canvas_camera(s, mid, canvas_hovered);
    const float fit = std::max(4.0f, std::min((canvas.w - 24.0f) / cols, (body_h - 12.0f) / rows));
    const float k   = fit * s.tech_tree_zoom;              // base unit -> px
    const ImVec2 centre{mid.x + s.tech_tree_pan_x, mid.y + s.tech_tree_pan_y};
    const float top = -0.5f * rows + kEsHeadRows;          // row 0's y, in rows from the centre
    const auto at = [&](const es_place& pl) {
        return ImVec2{centre.x + pl.x * k, centre.y + (top + pl.y) * k};
    };
    const float rk  = s.tech_tree_zoom;                    // glyph scale (k = 1 at the default fit)
    const bool can_plan = v.nation != null_entity;
    const bool shift    = io.KeyShift;
    const auto& L = es_layout();

    dl->PushClipRect({canvas.x, canvas.y}, {canvas.x + canvas.w, canvas.y + canvas.h}, true);

    std::array<ImVec2, es::node_count> scr{};
    for (int i = 0; i < es::node_count; ++i)
        scr[static_cast<std::size_t>(i)] = at(L[static_cast<std::size_t>(i)]);

    // Column dividers between the three branches.
    for (int b = 1; b < es::branch_count; ++b)
    {
        const float x = centre.x + (static_cast<float>(b) - 1.5f) * kEsColumn * k;
        dashed_line(dl, {x, centre.y + (top - kEsHeadRows) * k}, {x, centre.y + (top + es_rows()) * k},
                    IM_COL32(120, 126, 138, 70), 1.0f, 6.0f);
    }

    // Links: requirement -> node; AND links (2+ groups) in the accent.
    for (int i = 0; i < es::node_count; ++i)
    {
        const es::node& n = es_at(i);
        const bool and_link = n.group_count >= 2;
        const float dim = v.state(i) == node_state::locked ? 0.55f : 1.0f;
        for (int g = 0; g < n.group_count; ++g)
            for (const std::int16_t m : n.groups[static_cast<std::size_t>(g)])
            {
                if (m < 0)
                    continue;
                ImU32 col = and_link ? with_alpha(palette::research_accent, 0.85f * dim)
                                     : with_alpha(palette::research_link, dim);
                if (v.held(m) && v.held(i))
                    col = with_alpha(palette::research_held, 0.85f);
                dl->AddLine(scr[static_cast<std::size_t>(m)], scr[static_cast<std::size_t>(i)], col,
                            and_link ? 1.8f : 1.1f);
            }
    }

    int hovered = -1;
    if (canvas_hovered)
    {
        float best = 1e9f;
        for (int i = 0; i < es::node_count; ++i)
        {
            const float dx = io.MousePos.x - scr[static_cast<std::size_t>(i)].x;
            const float dy = io.MousePos.y - scr[static_cast<std::size_t>(i)].y;
            const float d2 = dx * dx + dy * dy;
            const float reach = look_radius(look_of(es_at(i)), rk) + 5.0f;
            if (d2 <= reach * reach && d2 < best)
            {
                best    = d2;
                hovered = i;
            }
        }
    }

    std::vector<std::uint16_t> preview;
    if (can_plan && shift && hovered >= 0)
    {
        preview = queued_early_space_dependents(w, v.nation, hovered);
        if (preview.empty())
            es_refusal_reason(w, p, v, hovered, &preview);
    }

    ImFont* font = ImGui::GetFont();
    const float small = ImGui::GetFontSize() * 0.8f;
    for (int i = 0; i < es::node_count; ++i)
    {
        const es::node& n = es_at(i);
        const ImVec2 c = scr[static_cast<std::size_t>(i)];
        const float r = look_radius(look_of(n), rk);
        const node_state st = v.state(i);
        draw_look(dl, c, r, look_of(n), st);
        if (std::find(preview.begin(), preview.end(), static_cast<std::uint16_t>(i)) != preview.end())
            dl->AddCircle(c, r + 5.0f, with_alpha(palette::hover, 0.8f), 0, 1.4f);
        if (st == node_state::queued)
        {
            dl->AddCircle(c, r + 3.0f, palette::research_queued, 0, 1.6f);
            char b[8];
            std::snprintf(b, sizeof b, "%d", v.queue_pos[static_cast<std::size_t>(i)]);
            dl->AddText(font, small, {c.x + r + 2.0f, c.y - r - small}, palette::research_queued, b); // fit-exempt: tree badge
        }
        if (i == hovered)
            dl->AddCircle(c, r + 4.0f, palette::hover, 0, 2.0f);
        // Majors and masteries carry their names: a left-arm node's label runs
        // left, every other to the right, so two arms' labels never meet.
        if (n.t == es::tier::major || n.is_mastery)
        {
            const int b = es_branch_index(n.branch);
            const float axis = static_cast<float>(std::max(0, b) - 1) * kEsColumn;
            const bool left = L[static_cast<std::size_t>(i)].x < axis - 0.1f;
            const ImVec2 ts = font->CalcTextSizeA(small, FLT_MAX, 0.0f, n.name);
            const ImVec2 tp{left ? c.x - r - 4.0f - ts.x : c.x + r + 4.0f, c.y - ts.y * 0.5f};
            dl->AddText(font, small, tp, st == node_state::locked ? palette::research_muted : palette::text_secondary,
                        n.name); // fit-exempt: free-standing tree label
        }
    }

    // Branch headers: the branch, then what opens it.
    for (int b = 0; b < es::branch_count; ++b)
    {
        const float x = centre.x + static_cast<float>(b - 1) * kEsColumn * k;
        const char* name = es::branches[static_cast<std::size_t>(b)].name;
        const ImVec2 ns = font->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, name);
        const float hy = centre.y + (top - kEsHeadRows) * k;
        dl->AddText(font, ImGui::GetFontSize(), {x - ns.x * 0.5f, hy}, palette::research_accent, name); // fit-exempt: free-standing tree label
        std::string key;
        ImU32 col = palette::research_danger;
        if (!can_plan)
            key = std::string("Opens with ") + es_branch_key_name(b);
        else if (v.keyed[static_cast<std::size_t>(b)])
        {
            key = std::string("Keyed: ") + es_branch_key_name(b) + " held";
            col = palette::positive;
        }
        else
            key = std::string("Needs ") + es_branch_key_name(b);
        const ImVec2 ks = font->CalcTextSizeA(small, FLT_MAX, 0.0f, key.c_str());
        dl->AddText(font, small, {x - ks.x * 0.5f, hy + ns.y + 2.0f}, col, key.c_str()); // fit-exempt: free-standing tree label
    }

    dl->PopClipRect();

    // The caption band: the mode line, then the gate (or the press message).
    s.tech_tree_es_locked = can_plan && !v.gate.met();
    s.tech_tree_es_lock_reason.clear();
    {
        const char* line = can_plan
                               ? "Early Space: hover a node to read it. Shift-click a node to queue it behind every "
                                 "Digitisation entry; shift-click a queued node to take it off."
                               : "Early Space: hover a node to read it. No nation is seated, so there is nothing to plan.";
        dl->AddText({canvas.x + 12.0f, canvas.y + 10.0f}, palette::text_secondary, line); // fit-exempt: canvas caption
        const float y2 = canvas.y + 10.0f + ImGui::GetTextLineHeightWithSpacing();
        if (s.tech_tree_es_locked)
        {
            s.tech_tree_es_lock_reason = "Locked: the gate into space needs " + gate_missing(v.gate) + ".";
            dl->AddText({canvas.x + 12.0f, y2}, palette::research_danger, s.tech_tree_es_lock_reason.c_str()); // fit-exempt: canvas caption
        }
        else if (!s.research_message.empty() && can_plan)
            dl->AddText({canvas.x + 12.0f, y2}, palette::research_warning, s.research_message.c_str()); // fit-exempt: canvas caption
        const float lh = ImGui::GetFontSize() * 0.9f + 6.0f;
        legend(dl, {canvas.x + 12.0f, canvas.y + canvas.h - 10.0f * lh - 12.0f}, rk);
    }

    if (can_plan && shift && hovered >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        es_press_node(w, p, s, v, hovered);

    s.tech_tree_es_node_screen.resize(static_cast<std::size_t>(es::node_count) * 2);
    for (int i = 0; i < es::node_count; ++i)
    {
        s.tech_tree_es_node_screen[static_cast<std::size_t>(i) * 2]     = scr[static_cast<std::size_t>(i)].x;
        s.tech_tree_es_node_screen[static_cast<std::size_t>(i) * 2 + 1] = scr[static_cast<std::size_t>(i)].y;
    }
    return hovered;
}

/// The age switch, top-right of the canvas: one tab per age's tree. Switching is
/// a view change; a press on the tab already shown closes the research panel
/// (the toggle rule: re-clicking the active sub-view tab closes it). The Early
/// Space tab names its lock while the gate into space is unmet.
void draw_age_tabs(const world& w, const research_params& p, ui_state& s, const foldout_rect& canvas)
{
    const entity_id nation = research_player_nation(w);
    const bool locked = nation != null_entity && !nation_meets_space_gate(w, nation, p);
    const char* labels[2] = {"Digitisation", locked ? "Early Space (locked)" : "Early Space"};
    const ImGuiStyle& st = ImGui::GetStyle();
    float width = 0.0f;
    float wd[2];
    for (int a = 0; a < 2; ++a)
    {
        wd[a] = ImGui::CalcTextSize(labels[a]).x + st.FramePadding.x * 2.0f;
        width += wd[a];
    }
    width += st.ItemSpacing.x;
    float x = canvas.x + canvas.w - width - 12.0f;
    const float y = canvas.y + 8.0f;
    s.tech_tree_age_tab_screen.assign(4, 0.0f);
    for (int a = 0; a < 2; ++a)
    {
        ImGui::SetCursorScreenPos({x, y});
        const bool active = s.tech_tree_age == a;
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushID(a);
        if (ImGui::Button(labels[a]))
        {
            if (active)
                s.show_tech_tree = false; // the toggle rule
            else
                s.tech_tree_age = a;
        }
        ImGui::PopID();
        if (active)
            ImGui::PopStyleColor();
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        s.tech_tree_age_tab_screen[static_cast<std::size_t>(a) * 2]     = 0.5f * (mn.x + mx.x);
        s.tech_tree_age_tab_screen[static_cast<std::size_t>(a) * 2 + 1] = 0.5f * (mn.y + mx.y);
        x += wd[a] + st.ItemSpacing.x;
    }
}

} // namespace

void draw_tech_tree_panel(const world& w, const research_params& p, ui_state& s)
{
    if (!s.show_tech_tree)
    {
        s.tech_tree_node_screen.clear();
        s.tech_tree_es_node_screen.clear();
        s.tech_tree_age_tab_screen.clear();
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

    // --- BL-1613: the age switch, then the age's tree
    const int age_before = s.tech_tree_age;
    draw_age_tabs(w, p, s, canvas);
    if (s.tech_tree_age != age_before)
    {
        // A different tree: its own camera and no hover carried across.
        s.tech_tree_pan_x = s.tech_tree_pan_y = 0.0f;
        s.tech_tree_zoom        = 1.0f;
        s.tech_tree_hover_node  = -1;
        s.tech_tree_hover_secs  = 0.0f;
        s.tech_tree_hover_stuck = false;
    }
    const bool early_space = s.tech_tree_age == 1;
    const es_view ev = early_space ? es_view_of(w, p) : es_view{};
    int hovered = -1;
    if (early_space)
    {
        s.tech_tree_node_screen.clear();
        hovered = draw_es_tree(w, p, s, dl, canvas, canvas_hovered, ev);
    }
    else
    {
        s.tech_tree_es_node_screen.clear();
        s.tech_tree_es_locked = false;
        s.tech_tree_es_lock_reason.clear();
        hovered = draw_dg_ring(w, s, dl, canvas, canvas_hovered);
    }
    const nation_view v = view_of(w);

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
        draw_hover_card(anchor, [&] {
            if (early_space)
                es_hover_body(w, p, ev, node);
            else
                hover_body(w, p, v, node);
        }, &mn, &mx);
        s.tech_tree_card_min_x = mn.x;
        s.tech_tree_card_min_y = mn.y;
        s.tech_tree_card_max_x = mx.x;
        s.tech_tree_card_max_y = mx.y;
    }
}

} // namespace ui
