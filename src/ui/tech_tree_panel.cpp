#include "tech_tree_panel.hpp"

#include "foldout_column.hpp"

#include <imgui.h>

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui {

namespace {

/// "invention" / "tier 2" / "capstone" — the kind column, with the tier folded in.
std::string kind_label(const tech_node& t)
{
    if (t.kind == "tier" && t.tier > 0)
        return "tier " + std::to_string(t.tier);
    return t.kind;
}

constexpr std::size_t kLabelBudget = 18; ///< Chars before an on-canvas label truncates.

/// The on-canvas node label (BL-310 round 3) — never the bare id. Authored
/// `short_name` wins; otherwise `name`, truncated past kLabelBudget chars
/// rather than left to overlap its neighbours. ASCII "..." not a unicode
/// ellipsis — BL-234's own lesson (missing codepoints render "?"), the same
/// reason the return control draws its glyph rather than typing one. The
/// full name and unlocks text are always in the hover tooltip regardless.
std::string display_label(const tech_node& t)
{
    if (!t.short_name.empty())
        return t.short_name;
    if (t.name.size() <= kLabelBudget)
        return t.name;
    return t.name.substr(0, kLabelBudget - 3) + "...";
}

// ---------------------------------------------------------------------------
// The radial constellation (BL-310) — ANCIENT_TECH_LADDER.md § Geometry,
// transcribed for the two authored eras (views 1/2). Rings are graph depth
// within a quest (time, centre-out); sectors are one wedge per gate quest
// (domain, angular). No tech fog: nothing is ever "owned" in this mock (no
// player progression state exists — BL-087 resolution 6), so the whole
// authored web renders, same honesty stance the old list view took.
// ---------------------------------------------------------------------------

constexpr float kRingSpacing  = 78.0f;  ///< Canvas-space px between rings.
constexpr float kBaseRadius   = 56.0f;  ///< Canvas-space px from centre to ring 1.
constexpr float kNodeRadius   = 8.0f;   ///< Canvas-space px, before zoom.
constexpr float kKeystoneMul  = 1.5f;   ///< Keystone/capstone nodes draw larger.
constexpr float kMinZoom      = 0.35f;
constexpr float kMaxZoom      = 3.0f;

/// Ring = the authored `tier` field when set (>0) — the Era 1 sectors set
/// tier = R1/R2/R3 directly, and the Antiquity transcription sets tier =
/// band index (T1=1..T6=6), both authoritative. Falls back to 1 + longest
/// same-quest-prereq chain length (memoized DFS) when tier is unset, which is
/// only the Era 0 data (predates this geometry and never populated tier
/// consistently) — a tech with no same-quest prereqs sits at ring 1, matching
/// "can you get there at all?" (ANCIENT_TECH_LADDER.md's own ring-1 reading,
/// R1 "Reach").
int compute_ring(const tech_node& t, const std::string& quest_id,
                  const std::unordered_map<std::string, const tech_node*>& by_id,
                  std::unordered_map<std::string, int>& memo,
                  std::vector<std::string>& in_progress)
{
    if (t.tier > 0)
        return t.tier;

    auto memo_it = memo.find(t.id);
    if (memo_it != memo.end())
        return memo_it->second;

    // Cycle guard: a same-quest prereq cycle (shouldn't happen in authored
    // data, but this is display code — never crash on malformed input) bottoms
    // out at ring 1 for the cycle's re-entry rather than recursing forever.
    for (const std::string& p : in_progress)
        if (p == t.id)
            return 1;

    int max_prereq_ring = 0;
    in_progress.push_back(t.id);
    for (const std::string& prereq_id : t.prereqs)
    {
        auto it = by_id.find(prereq_id);
        if (it == by_id.end() || it->second->quest != quest_id)
            continue; // cross-quest/cross-era prereq — doesn't deepen THIS quest's ring
        max_prereq_ring = std::max(max_prereq_ring, compute_ring(*it->second, quest_id, by_id, memo, in_progress));
    }
    in_progress.pop_back();

    const int ring = max_prereq_ring + 1;
    memo[t.id] = ring;
    return ring;
}

/// True if `id` ends in "A" or "B" immediately after a non-alnum boundary is
/// NOT required here — the authored convention (tech_tree.lua, BL-310) is a
/// literal "A"/"B" suffix on keystone-branch tech ids (E1-LA-03A / E1-LA-03B).
bool is_branch_suffix(const std::string& id)
{
    return !id.empty() && (id.back() == 'A' || id.back() == 'B');
}

struct node_layout
{
    const tech_node* node = nullptr;
    ImVec2 canvas_pos{}; ///< Pre-pan/zoom canvas-space position (centre = origin).
};

// ---------------------------------------------------------------------------
// Era-tab icons (BL-310 round 3, Ben 2026-08-06: "make the icons bigger,
// with small icons of the map (no labels)"). Deliberately real data, not a
// hand-drawn stand-in glyph: each icon is that era's actual node positions,
// tiny — a literal small icon of the map. Position math is duplicated from
// draw_constellation below rather than shared, since the icon needs none of
// its edge/label/hover machinery and this keeps that already-golden-blessed
// function untouched.
// ---------------------------------------------------------------------------

/// Normalised (roughly [-1, 1]) node positions for one era's gate quests, for
/// icon-scale rendering only. Empty for an era with no authored quests (Era 2
/// today) — the caller draws a placeholder ring instead.
std::vector<ImVec2> compute_icon_positions(const tech_tree_registry& tree, int era)
{
    std::vector<const tech_quest*> quests;
    for (const tech_quest& q : tree.quests())
        if (q.type == "gate" && q.era == era)
            quests.push_back(&q);
    if (quests.empty())
        return {};

    std::unordered_map<std::string, const tech_node*> by_id;
    for (const tech_node& t : tree.techs())
        by_id[t.id] = &t;

    std::unordered_map<std::string, int> ring_memo;
    std::vector<std::string> in_progress;
    int global_max_ring = 1;
    for (const tech_quest* q : quests)
        for (const tech_node& t : tree.techs())
            if (t.quest == q->id)
                global_max_ring = std::max(global_max_ring,
                    compute_ring(t, q->id, by_id, ring_memo, in_progress));

    std::vector<ImVec2> out;
    const float wedge_width = 6.28318530718f / static_cast<float>(quests.size());
    for (std::size_t qi = 0; qi < quests.size(); ++qi)
    {
        const tech_quest* q = quests[qi];
        const float wedge_start = -1.57079632679f + static_cast<float>(qi) * wedge_width;
        const float margin = wedge_width * 0.12f;

        std::unordered_map<int, std::vector<const tech_node*>> by_ring;
        for (const tech_node& t : tree.techs())
            if (t.quest == q->id)
                by_ring[compute_ring(t, q->id, by_id, ring_memo, in_progress)].push_back(&t);

        for (auto& [ring, nodes] : by_ring)
        {
            const float radius = static_cast<float>(ring) / static_cast<float>(global_max_ring);
            for (std::size_t i = 0; i < nodes.size(); ++i)
            {
                const float t_frac = nodes.size() == 1
                    ? 0.5f
                    : static_cast<float>(i) / static_cast<float>(nodes.size() - 1);
                const float angle = wedge_start + margin + (wedge_width - 2.0f * margin) * t_frac;
                out.push_back({ radius * std::cos(angle), radius * std::sin(angle) });
            }
        }
    }
    return out;
}

/// Draws one era's tiny constellation thumbnail — dots only, no edges or
/// text (illegible at icon scale and unnecessary for a selector).
void draw_era_icon(ImDrawList* dl, ImVec2 centre, float radius,
                    const tech_tree_registry& tree, int era, ImU32 dot_colour)
{
    const std::vector<ImVec2> positions = compute_icon_positions(tree, era);
    if (positions.empty())
    {
        // No authored quests (Era 2) — a dashed ring, so the placeholder
        // still reads as "a map, not drawn yet" rather than a blank square.
        constexpr int kSegments = 16;
        for (int i = 0; i < kSegments; i += 2)
        {
            const float a0 = 6.28318530718f * static_cast<float>(i) / kSegments;
            const float a1 = 6.28318530718f * static_cast<float>(i + 1) / kSegments;
            dl->AddLine({centre.x + radius * 0.7f * std::cos(a0), centre.y + radius * 0.7f * std::sin(a0)},
                        {centre.x + radius * 0.7f * std::cos(a1), centre.y + radius * 0.7f * std::sin(a1)},
                        IM_COL32(110, 116, 128, 160), 1.5f);
        }
        return;
    }
    for (const ImVec2& p : positions)
        dl->AddCircleFilled({centre.x + p.x * radius * 0.85f, centre.y + p.y * radius * 0.85f},
                             std::max(1.2f, radius * 0.05f), dot_colour);
}

/// Icon-only era-tab button — replaces the text ui::nav_button row (Ben,
/// 2026-08-06). Same toggle-rule semantics as ui::nav_button (re-click while
/// active closes the panel via `close`); the tooltip carries the name that
/// used to be printed on the button.
void era_icon_button(const char* tooltip, int id, int& view, bool* close,
                      const tech_tree_registry& tree, int era, ImU32 dot_colour, ImU32 bg_tint)
{
    constexpr float kSize = 72.0f;
    const bool active = (view == id);
    ImGui::PushID(id);
    ImGui::InvisibleButton("##era_icon", {kSize, kSize});
    const bool hot = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked())
    {
        if (active && close) *close = false; // BL-126 toggle rule
        else                 view = id;
    }
    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 border = active ? IM_COL32(120, 170, 230, 255)
                        : hot   ? IM_COL32(150, 158, 172, 220)
                                : IM_COL32(70, 78, 92, 180);
    dl->AddRectFilled(mn, mx, bg_tint, 6.0f);
    dl->AddRect(mn, mx, border, 6.0f, 0, active ? 2.0f : 1.0f);
    draw_era_icon(dl, {(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f}, kSize * 0.36f, tree, era, dot_colour);
    if (hot)
        ImGui::SetTooltip("%s", tooltip);
    ImGui::PopID();
}

/// Draws one era's gate quests as a radial web. Returns true if it drew
/// something (false ⇒ caller shows the "no quests authored" placeholder).
bool draw_constellation(const tech_tree_registry& tree, int era,
                         float& pan_x, float& pan_y, float& zoom, bool is_history)
{
    std::vector<const tech_quest*> quests;
    for (const tech_quest& q : tree.quests())
        if (q.type == "gate" && q.era == era)
            quests.push_back(&q);
    if (quests.empty())
        return false;

    std::unordered_map<std::string, const tech_node*> by_id;
    for (const tech_node& t : tree.techs())
        by_id[t.id] = &t;

    // Layout: one wedge per quest, techs placed by (ring, index-within-ring).
    std::vector<node_layout> layout;
    std::unordered_map<std::string, ImVec2> pos_by_id; // canvas-space, for edge lookup
    std::unordered_map<std::string, int> ring_memo;
    std::vector<std::string> in_progress;

    const float wedge_width = 6.28318530718f / static_cast<float>(quests.size());
    struct wedge_info { const tech_quest* quest; float centre_angle; float max_radius; };
    std::vector<wedge_info> wedges;

    for (std::size_t qi = 0; qi < quests.size(); ++qi)
    {
        const tech_quest* q = quests[qi];
        const float wedge_start  = -1.57079632679f + static_cast<float>(qi) * wedge_width;
        const float wedge_centre = wedge_start + wedge_width * 0.5f;
        const float margin       = wedge_width * 0.12f;

        // Group this quest's techs by ring.
        std::unordered_map<int, std::vector<const tech_node*>> by_ring;
        int max_ring = 1;
        for (const tech_node& t : tree.techs())
        {
            if (t.quest != q->id)
                continue;
            const int ring = compute_ring(t, q->id, by_id, ring_memo, in_progress);
            by_ring[ring].push_back(&t);
            max_ring = std::max(max_ring, ring);
        }

        for (auto& [ring, nodes] : by_ring)
        {
            const float radius = kBaseRadius + static_cast<float>(ring - 1) * kRingSpacing;
            for (std::size_t i = 0; i < nodes.size(); ++i)
            {
                const float t_frac = nodes.size() == 1
                    ? 0.5f
                    : static_cast<float>(i) / static_cast<float>(nodes.size() - 1);
                const float angle = wedge_start + margin + (wedge_width - 2.0f * margin) * t_frac;
                const ImVec2 cpos{ radius * std::cos(angle), radius * std::sin(angle) };
                layout.push_back({ nodes[i], cpos });
                pos_by_id[nodes[i]->id] = cpos;
            }
        }

        wedges.push_back({ q, wedge_centre, kBaseRadius + static_cast<float>(max_ring - 1) * kRingSpacing });
    }

    // Canvas.
    ImGui::BeginChild("##tt_canvas", ImVec2{0, 0}, false,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 region = ImGui::GetContentRegionAvail();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 centre{ origin.x + region.x * 0.5f + pan_x, origin.y + region.y * 0.5f + pan_y };

    // Middle-drag pans, matching the zoom-ladder canvases' own idiom
    // (body_surface_canvas.cpp) — Ben tried left-click-pans (2026-08-06) and
    // preferred consistency with the rest of the app instead.
    if (ImGui::IsWindowHovered())
    {
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            pan_x += io.MouseDelta.x;
            pan_y += io.MouseDelta.y;
        }
        if (io.MouseWheel != 0.0f)
            zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), kMinZoom, kMaxZoom);
    }

    auto to_screen = [&](ImVec2 c) { return ImVec2{ centre.x + c.x * zoom, centre.y + c.y * zoom }; };

    // Ring guides — subtle, so they read as depth without competing with nodes.
    float outer_radius = kBaseRadius;
    for (const wedge_info& w : wedges)
        outer_radius = std::max(outer_radius, w.max_radius);
    for (float r = kBaseRadius; r <= outer_radius + 1.0f; r += kRingSpacing)
        dl->AddCircle(centre, r * zoom, IM_COL32(60, 70, 90, 90), 64, 1.0f);

    // Wedge boundary rays + quest labels.
    for (std::size_t i = 0; i < wedges.size(); ++i)
    {
        const float ray_angle = -1.57079632679f + static_cast<float>(i) * wedge_width;
        const ImVec2 ray_end = to_screen({ (outer_radius + kRingSpacing * 0.5f) * std::cos(ray_angle),
                                            (outer_radius + kRingSpacing * 0.5f) * std::sin(ray_angle) });
        dl->AddLine(centre, ray_end, IM_COL32(50, 58, 74, 70), 1.0f);

        const ImVec2 label_pos = to_screen({ (wedges[i].max_radius + kRingSpacing * 0.55f) * std::cos(wedges[i].centre_angle),
                                              (wedges[i].max_radius + kRingSpacing * 0.55f) * std::sin(wedges[i].centre_angle) });
        const std::string label = wedges[i].quest->name;
        const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
        dl->AddText({ label_pos.x - text_size.x * 0.5f, label_pos.y - text_size.y * 0.5f },
                    IM_COL32(190, 200, 215, 220), label.c_str());
    }

    // Edges — prereqs whose source has a position in THIS view. A prereq
    // outside the view (e.g. an Era-1 node's Era-0 gate) is silently
    // skipped, not drawn as a dangling stub (BL-310 R2).
    for (const node_layout& nl : layout)
    {
        for (const std::string& prereq_id : nl.node->prereqs)
        {
            auto it = pos_by_id.find(prereq_id);
            if (it == pos_by_id.end())
                continue;
            dl->AddLine(to_screen(it->second), to_screen(nl.canvas_pos), IM_COL32(90, 100, 120, 140), 1.5f);
        }
    }

    // Keystone exclusion marks — a keystone's direct branch children (id
    // suffix A/B) get a warm connecting line + a "><" exclusion glyph at
    // their midpoint, drawn OVER the plain prereq edges above.
    std::unordered_map<std::string, std::vector<const tech_node*>> branches_by_keystone;
    for (const node_layout& nl : layout)
    {
        if (!is_branch_suffix(nl.node->id))
            continue;
        for (const std::string& p : nl.node->prereqs)
        {
            auto kit = by_id.find(p);
            if (kit != by_id.end() && kit->second->kind == "capstone")
                branches_by_keystone[p].push_back(nl.node);
        }
    }
    for (auto& [keystone_id, branch_nodes] : branches_by_keystone)
    {
        if (branch_nodes.size() != 2)
            continue; // not a clean binary fork — draw nothing extra, plain edges still show
        const ImVec2 a = to_screen(pos_by_id[branch_nodes[0]->id]);
        const ImVec2 b = to_screen(pos_by_id[branch_nodes[1]->id]);
        dl->AddLine(a, b, IM_COL32(200, 120, 60, 160), 1.5f);
        const ImVec2 mid{ (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
        const ImVec2 text_size = ImGui::CalcTextSize("excludes");
        dl->AddText({ mid.x - text_size.x * 0.5f, mid.y - text_size.y * 0.5f },
                    IM_COL32(230, 160, 90, 230), "excludes");
    }

    // Nodes.
    const ImVec2 mouse = ImGui::GetMousePos();
    for (const node_layout& nl : layout)
    {
        const ImVec2 spos = to_screen(nl.canvas_pos);
        const bool is_capstone = (nl.node->kind == "capstone");
        const bool is_regime   = (nl.node->kind == "regime");
        const bool branch_a = is_branch_suffix(nl.node->id) && nl.node->id.back() == 'A';
        const bool branch_b = is_branch_suffix(nl.node->id) && nl.node->id.back() == 'B';

        const float r = kNodeRadius * zoom * (is_capstone ? kKeystoneMul : 1.0f);
        ImU32 fill;
        if (is_history)
        {
            // History palette (Era -1) — muted/sepia, deliberately less saturated
            // than Era 1's bright draft palette: this content is settled record,
            // not a proposal awaiting a decision. Keystones read as a warm bronze
            // rather than gold; regime (roster) nodes read distinctly from techs.
            if (is_capstone)    fill = IM_COL32(180, 140, 90, 220);
            else if (is_regime) fill = IM_COL32(140, 120, 100, 210);
            else                fill = IM_COL32(120, 130, 140, 200);
        }
        else
        {
            fill = IM_COL32(90, 130, 180, 230);
            if (branch_a) fill = IM_COL32(90, 170, 110, 230);
            else if (branch_b) fill = IM_COL32(150, 110, 190, 230);
            else if (is_capstone) fill = IM_COL32(215, 175, 70, 230);
        }

        const bool hovered = ImGui::IsWindowHovered() &&
            (mouse.x - spos.x) * (mouse.x - spos.x) + (mouse.y - spos.y) * (mouse.y - spos.y) <= r * r * 2.25f;

        dl->AddCircleFilled(spos, r, fill);
        dl->AddCircle(spos, r, hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 24, 32, 200), 24,
                      hovered ? 2.0f : 1.0f);

        if (zoom > 0.6f)
        {
            // Name, not id (BL-310 round 3, Ben: "so players do not have to
            // work out a dictionary in their mind for each tech"). Authored
            // short_name wins where it exists; otherwise a truncated `name`
            // is still a name — never the bare id, which is what the player
            // would have had to decode.
            const std::string label = display_label(*nl.node);
            const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
            dl->AddText({ spos.x - text_size.x * 0.5f, spos.y + r + 2.0f },
                        IM_COL32(170, 178, 190, 210), label.c_str());
        }

        if (hovered)
        {
            std::string tip = nl.node->name + "\n[" + kind_label(*nl.node) + ", " + nl.node->cost
                             + ", " + nl.node->condition + "]";
            if (!nl.node->unlocks.empty())
                tip += "\n" + nl.node->unlocks;
            ImGui::SetTooltip("%s", tip.c_str());
        }
    }

    ImGui::EndChild();
    return true;
}

} // namespace

/// One era row in the fold-out menu (BL-310 round 4) — the icon thumbnail
/// AND the name are both independently clickable, either selects the era
/// (Ben, 2026-08-06: "a player's selection of each Era's tech tree should be
/// a button click for the name, or the map click ... both of these should be
/// in menu space"). Same toggle-rule semantics as ui::nav_button throughout.
void era_menu_row(const char* label, int id, int& view, bool* close,
                   const tech_tree_registry& tree, int era, ImU32 dot_colour, ImU32 bg_tint)
{
    ImGui::PushID(id);
    era_icon_button(label, id, view, close, tree, era, dot_colour, bg_tint);
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::Dummy({0, 6.0f}); // rough vertical centring against the 72px icon
    ui::nav_button(label, id, view, close);
    ImGui::EndGroup();
    ImGui::PopID();
}

void draw_tech_tree_menu(const tech_tree_registry& tree, ui_state& s)
{
    if (!s.show_tech_tree)
        return;
    if (!ui::foldout_begin("Tech Tree"))
    {
        ui::foldout_end();
        return;
    }

    // Short line, not TextDisabled's %s form — the shell column is ~380px
    // and the old "N quests, M techs" tail clipped rather than wrapped there.
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("BL-087 design mock - read-only.");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    era_menu_row("Era -1 Antiquity",    0, s.tech_tree_view, &s.show_tech_tree, tree, -1,
                 IM_COL32(200, 170, 120, 255), IM_COL32(40, 36, 30, 200));
    ImGui::Spacing();
    era_menu_row("Era 0 - Terrestrial", 1, s.tech_tree_view, &s.show_tech_tree, tree, 0,
                 IM_COL32(120, 160, 210, 255), IM_COL32(24, 30, 42, 200));
    ImGui::Spacing();
    era_menu_row("Era 1 - Early Space", 2, s.tech_tree_view, &s.show_tech_tree, tree, 1,
                 IM_COL32(130, 200, 235, 255), IM_COL32(22, 34, 46, 200));
    ImGui::Spacing();
    era_menu_row("Era 2 (unauthored)",  3, s.tech_tree_view, &s.show_tech_tree, tree, 2,
                 IM_COL32(150, 150, 150, 255), IM_COL32(30, 30, 34, 200));

    ui::foldout_end();
}

void draw_tech_tree_panel(const tech_tree_registry& tree, bool& open, int& view,
                           float& pan_x, float& pan_y, float& zoom)
{
    if (!open)
        return;

    // Full-canvas takeover (BL-310, per BL-265's settled geometry): bounded to
    // ui::canvas_rect() rather than a floating popup, so the shell chrome,
    // header and Selection band survive. ImGuiCond_Always because canvas_rect()
    // is a function of DisplaySize, re-derived every frame like the pre-BL-265
    // fold overlay it replaces (detail_level.cpp).
    //
    // PURE CONSTELLATION (BL-310 round 4, Ben: "ignore the tech navigation for
    // canvas - this is just tech constellation"). No title, no instructions,
    // no era tabs, no return control — era selection lives entirely in the
    // shell-column menu (draw_tech_tree_menu) now, which already carries its
    // own toggle-rule close (re-click the active era). This window is the
    // rendered graph and nothing else.
    const foldout_rect canvas = ui::canvas_rect();
    ImGui::SetNextWindowPos({canvas.x, canvas.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({canvas.w, canvas.h}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::Begin("##tech_tree_canvas", nullptr, flags);

    if (view == 0)
    {
        // Real render (BL-310, NR-054, Ben 2026-08-06: "render ancient tech,
        // even just as a history"). Read-only — nothing here is ever chosen,
        // only derived (BL-274's endowment/diffusion mechanism); the history
        // palette (draw_constellation's is_history branch) marks that.
        if (!draw_constellation(tree, -1, pan_x, pan_y, zoom, /*is_history=*/true))
            ImGui::TextDisabled("Placeholder — no quests authored for this era yet.");
    }
    else if (view == 1 || view == 2)
    {
        const int era = view - 1;
        if (!draw_constellation(tree, era, pan_x, pan_y, zoom, /*is_history=*/false))
            ImGui::TextDisabled("Placeholder — no quests authored for this era yet.");
    }
    else
    {
        // Era 2 — placeholder only (Ben, 2026-08-06: standing lines don't need
        // rendering; keep this slot for Era 2 instead). Standing-line DATA
        // (L-LOG/L-AUTO/L-MIL) stays in tech_tree.lua — Era 1's E1-EX-KEYSTONE
        // still prereqs L-AU-01 — it simply has no view of its own any more.
        ImGui::TextDisabled("Placeholder — no in-engine tree yet.");
        ImGui::TextWrapped(
            "Era 2 has no design pass yet (ROADMAP.md's post-v0.1.0 arc). "
            "Standing lines (Logistics, Automation, Military) still deepen every "
            "era in the data — L-AU-01 still gates Era 1's Autonomy Doctrine — "
            "they just no longer have a rendered view of their own.");
    }

    ImGui::End();
}

} // namespace ui
