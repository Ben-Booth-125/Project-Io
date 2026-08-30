#include "tech_tree_panel.hpp"

#include "foldout_column.hpp"
#include "presentation.hpp" // BL-344: resource_name / building_type_name for condition_text

#include <imgui.h>

#include <algorithm> // std::clamp — MSVC pulls it in transitively, g++ does not (NR-450)
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

// ---------------------------------------------------------------------------
// Palette — GLOBAL_STYLE_SHEET.md, tech tree sub-track (2026-08-25). The
// background and the EARNED/LOCKED accent pair are SETTLED (colour2 render);
// the depth treatment — flat hard-edged shadows, stepped ring bevel, etched
// connector grooves — is the wide8 candidate softened per wide11, still being
// tuned. Amber and cyan are the only two live hues on this surface; everything
// structural stays in the grey ramp (wide4's many-hued pass was rejected).
// ---------------------------------------------------------------------------

constexpr ImU32 kColBackground    = IM_COL32(15, 15, 20, 255);    ///< #0F0F14 — app.cpp's clear colour.
constexpr ImU32 kColEarnedFill    = IM_COL32(232, 168, 46, 255);  ///< Amber — EARNED.
constexpr ImU32 kColEarnedEdge    = IM_COL32(120, 84, 18, 255);
constexpr ImU32 kColLockedFill    = IM_COL32(44, 186, 224, 255);  ///< Saturated cyan — LOCKED (a full second live colour).
constexpr ImU32 kColLockedEdge    = IM_COL32(16, 88, 110, 255);
constexpr ImU32 kColUnearnFill    = IM_COL32(20, 21, 27, 255);    ///< No gate authored — recessive near-bg plate…
constexpr ImU32 kColUnearnEdge    = IM_COL32(110, 116, 128, 170); ///< …under a dim grey outline, never a third accent.
constexpr ImU32 kColNodeShadow    = IM_COL32(0, 0, 0, 110);       ///< Flat offset shadow, no blur; alpha softened per wide11.
constexpr ImU32 kColRingHighlight = IM_COL32(66, 74, 90, 110);    ///< Terrace step: lit inner lip…
constexpr ImU32 kColRingShadow    = IM_COL32(0, 0, 0, 160);       ///< …dark outer edge-line, no gradient.
constexpr ImU32 kColEdgeIncision  = IM_COL32(6, 7, 10, 220);      ///< Connector groove: the cut…
constexpr ImU32 kColEdgeLip       = IM_COL32(96, 106, 124, 110);  ///< …and its lit lower lip.
constexpr float kShadowOffset     = 2.5f;                          ///< Screen px, deliberately not zoom-scaled.

/// Node silhouette by category (wide7 — varied shapes fixed hexagon fatigue).
/// Capstones alone carry the large hexagon; branch nodes point at each other's
/// exclusion partner (A up, B down); regimes read as record-markers.
enum class node_shape { circle, hexagon, diamond, tri_up, tri_down };

bool is_branch_suffix(const std::string& id); // defined with the layout helpers below

node_shape shape_for(const tech_node& t)
{
    if (t.kind == "capstone") return node_shape::hexagon;
    if (t.kind == "regime")   return node_shape::diamond;
    if (is_branch_suffix(t.id))
        return t.id.back() == 'A' ? node_shape::tri_up : node_shape::tri_down;
    return node_shape::circle;
}

void add_node_shape(ImDrawList* dl, node_shape s, ImVec2 c, float r, ImU32 col,
                     bool filled, float thickness = 1.0f)
{
    switch (s)
    {
    case node_shape::circle:
        if (filled) dl->AddCircleFilled(c, r, col);
        else        dl->AddCircle(c, r, col, 24, thickness);
        break;
    case node_shape::hexagon:
        if (filled) dl->AddNgonFilled(c, r, col, 6);
        else        dl->AddNgon(c, r, col, 6, thickness);
        break;
    case node_shape::diamond:
        if (filled) dl->AddNgonFilled(c, r, col, 4);
        else        dl->AddNgon(c, r, col, 4, thickness);
        break;
    case node_shape::tri_up:
    {
        const ImVec2 p1{c.x, c.y - r}, p2{c.x - 0.866f * r, c.y + 0.5f * r}, p3{c.x + 0.866f * r, c.y + 0.5f * r};
        if (filled) dl->AddTriangleFilled(p1, p2, p3, col);
        else        dl->AddTriangle(p1, p2, p3, col, thickness);
        break;
    }
    case node_shape::tri_down:
    {
        const ImVec2 p1{c.x, c.y + r}, p2{c.x - 0.866f * r, c.y - 0.5f * r}, p3{c.x + 0.866f * r, c.y - 0.5f * r};
        if (filled) dl->AddTriangleFilled(p1, p2, p3, col);
        else        dl->AddTriangle(p1, p2, p3, col, thickness);
        break;
    }
    }
}

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
// Cached constellation geometry (BL-362). The ring layout — string-keyed maps
// plus the memoised compute_ring DFS — is a pure function of (tree, era): pan
// and zoom apply per frame in to_screen, and the layout is canvas-space, so
// window size never enters it either. Recomputing it every frame the takeover
// is open was pure waste; each era's geometry builds once and is looked up
// thereafter.
//
// The cache holds LAYOUT ONLY. Everything that depends on the live world —
// BL-344's earned / locked / no-gate-authored states and its per-condition
// itemising — reads `w` and `corp` in the draw path, per frame, and is
// deliberately not cached: those answers change as the campaign runs. The
// tech_node pointers are safe to retain because the registry is startup-loaded
// and read-only (tech_tree.hpp), and the stamp below drops everything if it is
// ever swapped or reloaded.
// ---------------------------------------------------------------------------

struct wedge_info { const tech_quest* quest; float centre_angle; float max_radius; };

struct constellation_geometry
{
    std::vector<node_layout> layout;
    std::unordered_map<std::string, ImVec2> pos_by_id; ///< Canvas-space, for edge lookup.
    std::vector<wedge_info> wedges;
    float wedge_width = 0.0f;
    /// Keystone exclusion marks: canvas-space endpoints of each clean binary
    /// fork (id suffix A/B off a capstone prereq), derived here so the draw
    /// loop needs no per-frame by_id map at all.
    std::vector<std::pair<ImVec2, ImVec2>> branch_pairs;
};

struct tech_geometry_cache
{
    const tech_tree_registry* tree = nullptr;
    std::uint32_t generation = 0;
    std::unordered_map<int, constellation_geometry> by_era;
    std::unordered_map<int, std::vector<ImVec2>>    icons_by_era;
};

tech_geometry_cache& geometry_cache_for(const tech_tree_registry& tree)
{
    static tech_geometry_cache cache;
    // Stamp on the registry's reload generation, NOT its address or entry counts:
    // verify.new_world reloads the same tech_tree.lua into the same app member, so
    // both are unchanged across a reload that destroys every node this cache's
    // `const tech_node*` values point into.
    if (cache.tree != &tree || cache.generation != tree.generation())
    {
        cache = {};
        cache.tree       = &tree;
        cache.generation = tree.generation();
    }
    return cache;
}

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
std::vector<ImVec2> compute_icon_positions_uncached(const tech_tree_registry& tree, int era)
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
    // Stamp-checked (BL-362): four of these draw every frame the fold-out menu
    // is open, and each rebuild walked every tech per quest.
    tech_geometry_cache& cache = geometry_cache_for(tree);
    auto it = cache.icons_by_era.find(era);
    if (it == cache.icons_by_era.end())
        it = cache.icons_by_era.emplace(era, compute_icon_positions_uncached(tree, era)).first;
    const std::vector<ImVec2>& positions = it->second;
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

/// Builds one era's radial layout: one wedge per quest, techs placed by
/// (ring, index-within-ring). Pure function of (tree, era) — the cached half
/// of draw_constellation (BL-362). Carries no world-derived state.
constellation_geometry build_constellation(const tech_tree_registry& tree, int era)
{
    constellation_geometry geo;

    std::vector<const tech_quest*> quests;
    for (const tech_quest& q : tree.quests())
        if (q.type == "gate" && q.era == era)
            quests.push_back(&q);
    if (quests.empty())
        return geo;

    std::unordered_map<std::string, const tech_node*> by_id;
    for (const tech_node& t : tree.techs())
        by_id[t.id] = &t;

    std::unordered_map<std::string, int> ring_memo;
    std::vector<std::string> in_progress;

    geo.wedge_width = 6.28318530718f / static_cast<float>(quests.size());

    for (std::size_t qi = 0; qi < quests.size(); ++qi)
    {
        const tech_quest* q = quests[qi];
        const float wedge_start  = -1.57079632679f + static_cast<float>(qi) * geo.wedge_width;
        const float wedge_centre = wedge_start + geo.wedge_width * 0.5f;
        const float margin       = geo.wedge_width * 0.12f;

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
                const float angle = wedge_start + margin + (geo.wedge_width - 2.0f * margin) * t_frac;
                const ImVec2 cpos{ radius * std::cos(angle), radius * std::sin(angle) };
                geo.layout.push_back({ nodes[i], cpos });
                geo.pos_by_id[nodes[i]->id] = cpos;
            }
        }

        geo.wedges.push_back({ q, wedge_centre, kBaseRadius + static_cast<float>(max_ring - 1) * kRingSpacing });
    }

    // Keystone exclusion pairs — a keystone's direct branch children (id
    // suffix A/B). Only a clean binary fork gets a pair; anything else draws
    // its plain prereq edges alone.
    std::unordered_map<std::string, std::vector<const tech_node*>> branches_by_keystone;
    for (const node_layout& nl : geo.layout)
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
            continue;
        geo.branch_pairs.emplace_back(geo.pos_by_id[branch_nodes[0]->id],
                                      geo.pos_by_id[branch_nodes[1]->id]);
    }

    return geo;
}

/// Draws one era's gate quests as a radial web. Returns true if it drew
/// something (false ⇒ caller shows the "no quests authored" placeholder).
/// Layout comes from the per-era geometry cache; the node/tooltip pass — and
/// with it every BL-344 world read — still runs per frame.
bool draw_constellation(const tech_tree_registry& tree, const world& w, entity_id corp, int era,
                         float& pan_x, float& pan_y, float& zoom, bool is_history)
{
    tech_geometry_cache& cache = geometry_cache_for(tree);
    auto git = cache.by_era.find(era);
    if (git == cache.by_era.end())
        git = cache.by_era.emplace(era, build_constellation(tree, era)).first;
    const constellation_geometry& geo = git->second;
    if (geo.wedges.empty())
        return false;

    const std::vector<node_layout>& layout = geo.layout;
    const std::unordered_map<std::string, ImVec2>& pos_by_id = geo.pos_by_id;
    const std::vector<wedge_info>& wedges = geo.wedges;
    const float wedge_width = geo.wedge_width;

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

    // Ring guides — wide8's terraced bevel: each ring is a flat step, drawn as
    // a dark outer edge-line plus a lit inner lip, never a gradient.
    float outer_radius = kBaseRadius;
    for (const wedge_info& w : wedges)
        outer_radius = std::max(outer_radius, w.max_radius);
    for (float r = kBaseRadius; r <= outer_radius + 1.0f; r += kRingSpacing)
    {
        dl->AddCircle(centre, r * zoom + 1.5f, kColRingShadow, 96, 1.0f);
        dl->AddCircle(centre, r * zoom, kColRingHighlight, 96, 1.0f);
    }

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
        dl->AddText({ label_pos.x - text_size.x * 0.5f, label_pos.y - text_size.y * 0.5f }, // fit-exempt: on-canvas node label, canvas pans to content
                    IM_COL32(190, 200, 215, 220), label.c_str());
    }

    // Edges — prereqs whose source has a position in THIS view. A prereq
    // outside the view (e.g. an Era-1 node's Era-0 gate) is silently
    // skipped, not drawn as a dangling stub (BL-310 R2).
    // Each connector is an etched groove (wide8): the incision plus a lit lip
    // offset a pixel below. Whether the groove survives real crossing density
    // is an open question on the style sheet — this is the stress-test.
    for (const node_layout& nl : layout)
    {
        for (const std::string& prereq_id : nl.node->prereqs)
        {
            auto it = pos_by_id.find(prereq_id);
            if (it == pos_by_id.end())
                continue;
            const ImVec2 a = to_screen(it->second);
            const ImVec2 b = to_screen(nl.canvas_pos);
            dl->AddLine({a.x, a.y + 1.5f}, {b.x, b.y + 1.5f}, kColEdgeLip, 1.0f);
            dl->AddLine(a, b, kColEdgeIncision, 2.0f);
        }
    }

    // Keystone exclusion marks — a keystone's direct branch children (id
    // suffix A/B) get a warm connecting line + a "><" exclusion glyph at
    // their midpoint, drawn OVER the plain prereq edges above. Pairs come
    // precomputed with the cached geometry.
    for (const auto& [pa, pb] : geo.branch_pairs)
    {
        const ImVec2 a = to_screen(pa);
        const ImVec2 b = to_screen(pb);
        // Warm-neutral grey, not the old orange — amber now means EARNED
        // (colour2), and a third live hue broke wide4. Warmth alone carries
        // the "these two conflict" read.
        dl->AddLine(a, b, IM_COL32(150, 140, 124, 150), 1.5f);
        const ImVec2 mid{ (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
        const ImVec2 text_size = ImGui::CalcTextSize("excludes");
        dl->AddText({ mid.x - text_size.x * 0.5f, mid.y - text_size.y * 0.5f }, // fit-exempt: on-canvas node label, canvas pans to content
                    IM_COL32(184, 172, 150, 220), "excludes");
    }

    // Nodes.
    const ImVec2 mouse = ImGui::GetMousePos();
    for (const node_layout& nl : layout)
    {
        const ImVec2 spos = to_screen(nl.canvas_pos);
        const bool is_capstone = (nl.node->kind == "capstone");
        const bool is_regime   = (nl.node->kind == "regime");

        const float r = kNodeRadius * zoom * (is_capstone ? kKeystoneMul : 1.0f);
        const node_shape shape = shape_for(*nl.node);

        // Colour is keyed to STATE, not kind (colour2, SETTLED): amber EARNED,
        // cyan LOCKED, dim grey outline for a node with no authored gate. Kind
        // now lives in the silhouette (shape_for), so the old per-kind and
        // per-branch hues are gone — branch identity is the triangle's point.
        ImU32 fill, edge;
        bool recessive = false;
        if (is_history)
        {
            // History palette (Era -1) — muted/sepia, deliberately less saturated:
            // this content is settled record, not a proposal awaiting a decision.
            if (is_capstone)    fill = IM_COL32(180, 140, 90, 220);
            else if (is_regime) fill = IM_COL32(140, 120, 100, 210);
            else                fill = IM_COL32(120, 130, 140, 200);
            edge = IM_COL32(20, 24, 32, 200);
        }
        else if (!nl.node->earnable)
        {
            fill = kColUnearnFill;
            edge = kColUnearnEdge;
            recessive = true; // no shadow — deliberately recessive, not a third state colour
        }
        else if (w.has_tech(corp, nl.node->id))
        {
            fill = kColEarnedFill;
            edge = kColEarnedEdge;
        }
        else
        {
            fill = kColLockedFill;
            edge = kColLockedEdge;
        }

        const bool hovered = ImGui::IsWindowHovered() &&
            (mouse.x - spos.x) * (mouse.x - spos.x) + (mouse.y - spos.y) * (mouse.y - spos.y) <= r * r * 2.25f;

        if (!recessive)
            add_node_shape(dl, shape, {spos.x + kShadowOffset, spos.y + kShadowOffset}, r,
                           kColNodeShadow, /*filled=*/true);
        add_node_shape(dl, shape, spos, r, fill, /*filled=*/true);
        add_node_shape(dl, shape, spos, r,
                       hovered ? IM_COL32(255, 255, 255, 255) : edge,
                       /*filled=*/false, hovered ? 2.0f : 1.2f);

        if (zoom > 0.6f)
        {
            // Name, not id (BL-310 round 3, Ben: "so players do not have to
            // work out a dictionary in their mind for each tech"). Authored
            // short_name wins where it exists; otherwise a truncated `name`
            // is still a name — never the bare id, which is what the player
            // would have had to decode.
            const std::string label = display_label(*nl.node);
            const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
            dl->AddText({ spos.x - text_size.x * 0.5f, spos.y + r + 2.0f }, // fit-exempt: on-canvas node label, canvas pans to content
                        IM_COL32(170, 178, 190, 210), label.c_str());
        }

        if (hovered)
        {
            std::string tip = nl.node->name + "\n[" + kind_label(*nl.node) + ", " + nl.node->cost
                             + ", " + nl.node->condition_label + "]";
            if (!nl.node->unlocks.empty())
                tip += "\n" + nl.node->unlocks;

            // BL-344: say honestly whether this node is EARNABLE, and whether it
            // is earned. Before this, every node looked alike and none of them
            // could be earned at all — a picture of a system rather than the
            // system. A node with no authored gate says so rather than reading
            // as unlocked (an empty condition_set would be TRUE by definition).
            if (!nl.node->earnable)
            {
                tip += "\nNo gate authored - not yet earnable.";
            }
            else if (w.has_tech(corp, nl.node->id))
            {
                tip += "\nEARNED.";
            }
            else
            {
                tip += "\nLOCKED. Requires:";
                for (const condition& c : nl.node->condition.all)
                    tip += "\n  - " + condition_text(c, resource_name, building_type_name)
                         + (evaluate_condition(c, w, corp) ? "  (met)" : "");
            }
            // Hover card on the settled dark ground — flat plate, dim grey
            // border, matching the node treatment. (wide8's hard-edged card
            // shadow needs a custom tooltip window; not attempted here.)
            ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(22, 23, 30, 245));
            ImGui::PushStyleColor(ImGuiCol_Border, kColUnearnEdge);
            ImGui::SetTooltip("%s", tip.c_str());
            ImGui::PopStyleColor(2);
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

void draw_tech_tree_panel(const tech_tree_registry& tree, const world& w, entity_id corp,
                          bool& open, int& view,
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
    // The takeover paints the SETTLED background itself rather than inheriting
    // ImGui's default window grey — the style sheet's ground is the app clear
    // colour, and the bevel/groove treatment only reads against it.
    ImGui::PushStyleColor(ImGuiCol_WindowBg, kColBackground);
    ImGui::Begin("##tech_tree_canvas", nullptr, flags);

    if (view == 0)
    {
        // Real render (BL-310, NR-054, Ben 2026-08-06: "render ancient tech,
        // even just as a history"). Read-only — nothing here is ever chosen,
        // only derived (BL-274's endowment/diffusion mechanism); the history
        // palette (draw_constellation's is_history branch) marks that.
        if (!draw_constellation(tree, w, corp, -1, pan_x, pan_y, zoom, /*is_history=*/true))
            ImGui::TextDisabled("Placeholder — no quests authored for this era yet.");
    }
    else if (view == 1 || view == 2)
    {
        const int era = view - 1;
        if (!draw_constellation(tree, w, corp, era, pan_x, pan_y, zoom, /*is_history=*/false))
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
    ImGui::PopStyleColor();
}

} // namespace ui
