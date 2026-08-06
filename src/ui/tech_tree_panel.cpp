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

/// Join a string list with " + " for the prereqs / opens columns.
std::string join(const std::vector<std::string>& parts)
{
    std::string out;
    for (const std::string& p : parts)
    {
        if (!out.empty())
            out += " + ";
        out += p;
    }
    return out;
}

/// One quest's collapsing header + tech table. Still used by the Standing
/// lines tab (view 3), which has no ring structure (BL-310: "deepen every
/// Era, never gate one" — a standing line isn't a constellation region).
void draw_quest(const tech_tree_registry& tree, const tech_quest& q)
{
    const std::string header = q.name + "  [" + q.id + ", " + q.status + "]###" + q.id;
    if (!ImGui::CollapsingHeader(header.c_str()))
        return;

    if (!q.thesis.empty())
        ImGui::TextDisabled("%s", q.thesis.c_str());
    if (!q.opens.empty())
        ImGui::Text("Capstone opens: %s", join(q.opens).c_str());

    std::vector<const tech_node*> nodes;
    for (const tech_node& t : tree.techs())
        if (t.quest == q.id)
            nodes.push_back(&t);

    if (nodes.empty())
    {
        ImGui::TextDisabled("(no techs enumerated)");
        return;
    }

    constexpr ImGuiTableFlags table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    const std::string table_id = "techs##" + q.id;
    if (ImGui::BeginTable(table_id.c_str(), 7, table_flags))
    {
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Tech");
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Cost");
        ImGui::TableSetupColumn("Payoff");
        ImGui::TableSetupColumn("Condition");
        ImGui::TableSetupColumn("Prereqs");
        ImGui::TableHeadersRow();

        for (const tech_node* t : nodes)
        {
            ImGui::TableNextRow();
            const bool is_capstone = (t->kind == "capstone");
            const bool is_economic = (t->condition == "surplus" || t->condition == "market"
                                      || t->condition == "stockpile");
            if (is_capstone)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                                       ImGui::GetColorU32(ImVec4{0.45f, 0.35f, 0.10f, 0.35f}));
            else if (is_economic)
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                                       ImGui::GetColorU32(ImVec4{0.15f, 0.35f, 0.45f, 0.35f}));

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(t->id.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(t->name.c_str());
            if (!t->unlocks.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s", t->unlocks.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(kind_label(*t).c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(t->cost.c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(t->payoff.c_str());
            ImGui::TableSetColumnIndex(5);
            ImGui::TextUnformatted(t->condition.c_str());
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(join(t->prereqs).c_str());
        }
        ImGui::EndTable();
    }
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

/// Ring = 1 + longest same-quest-prereq chain length (memoized DFS). Computed
/// from graph structure rather than the authored `tier` field, which the
/// Era 0 data (predates this geometry) never populated consistently — a tech
/// with no same-quest prereqs sits at ring 1, matching "can you get there at
/// all?" (ANCIENT_TECH_LADDER.md's own ring-1 reading, R1 "Reach").
int compute_ring(const tech_node& t, const std::string& quest_id,
                  const std::unordered_map<std::string, const tech_node*>& by_id,
                  std::unordered_map<std::string, int>& memo,
                  std::vector<std::string>& in_progress)
{
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

/// Draws one era's gate quests as a radial web. Returns true if it drew
/// something (false ⇒ caller shows the "no quests authored" placeholder).
bool draw_constellation(const tech_tree_registry& tree, int era,
                         float& pan_x, float& pan_y, float& zoom)
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
        const bool branch_a = is_branch_suffix(nl.node->id) && nl.node->id.back() == 'A';
        const bool branch_b = is_branch_suffix(nl.node->id) && nl.node->id.back() == 'B';

        const float r = kNodeRadius * zoom * (is_capstone ? kKeystoneMul : 1.0f);
        ImU32 fill = IM_COL32(90, 130, 180, 230);
        if (branch_a) fill = IM_COL32(90, 170, 110, 230);
        else if (branch_b) fill = IM_COL32(150, 110, 190, 230);
        else if (is_capstone) fill = IM_COL32(215, 175, 70, 230);

        const bool hovered = ImGui::IsWindowHovered() &&
            (mouse.x - spos.x) * (mouse.x - spos.x) + (mouse.y - spos.y) * (mouse.y - spos.y) <= r * r * 2.25f;

        dl->AddCircleFilled(spos, r, fill);
        dl->AddCircle(spos, r, hovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(20, 24, 32, 200), 24,
                      hovered ? 2.0f : 1.0f);

        if (zoom > 0.6f)
        {
            const std::string short_id = nl.node->id;
            const ImVec2 text_size = ImGui::CalcTextSize(short_id.c_str());
            dl->AddText({ spos.x - text_size.x * 0.5f, spos.y + r + 2.0f },
                        IM_COL32(170, 178, 190, 210), short_id.c_str());
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

void draw_tech_tree_panel(const tech_tree_registry& tree, bool& open, int& view,
                           float& pan_x, float& pan_y, float& zoom)
{
    if (!open)
        return;

    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(
        ImVec2{disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
    ImGui::SetNextWindowSize(ImVec2{900.0f, 620.0f}, ImGuiCond_Appearing);
    if (ImGui::Begin("Tech Tree (mock)", &open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::TextDisabled(
            "BL-087 design mock — read-only; the tech system is post-prototype. "
            "%d quests, %d techs. Middle-drag to pan, scroll to zoom, hover a node for its unlock.",
            static_cast<int>(tree.quests().size()), static_cast<int>(tree.techs().size()));
        ImGui::Separator();

        ui::nav_button("Era -1 Antiquity", 0, view, &open);
        ImGui::SameLine();
        ui::nav_button("Era 0 — Terrestrial", 1, view, &open);
        ImGui::SameLine();
        ui::nav_button("Era 1 — Early Space", 2, view, &open);
        ImGui::SameLine();
        ui::nav_button("Standing lines", 3, view, &open);
        ImGui::Separator();
        ImGui::Spacing();

        if (view == 0)
        {
            ImGui::TextDisabled("Placeholder — no in-engine tree yet.");
            ImGui::TextWrapped(
                "The ancient ladder (0 CE to campaign epoch): derived by the Era -1 "
                "sim from endowment and contact, never clicked. Its 58 techs, 8 "
                "vertex quests and 3 keystones are data in "
                "docs/research/ancient_tech_ladder.json; whether this surface "
                "renders them is BL-296's open question 4.");
        }
        else if (view == 1 || view == 2)
        {
            const int era = view - 1;
            if (!draw_constellation(tree, era, pan_x, pan_y, zoom))
                ImGui::TextDisabled("Placeholder — no quests authored for this era yet.");
        }
        else
        {
            ImGui::TextDisabled("Deepen every Era; never gate an Era.");
            for (const tech_quest& q : tree.quests())
                if (q.type == "standing")
                    draw_quest(tree, q);
        }
    }
    ImGui::End();
}

} // namespace ui
