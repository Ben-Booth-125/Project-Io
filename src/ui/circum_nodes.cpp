#include "circum_nodes.hpp"

#include "hover_card.hpp"
#include "presentation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ui {

namespace {

// Structural colours (CIRCUMPLANETARY.md § Orbital nodes): the node rings a
// touch brighter than the moon-orbit rings' (38, 42, 52). Not identity colours —
// a held pip's holder colour comes from presentation.hpp when occupancy lands.
constexpr ImU32 k_ring_low    = IM_COL32( 56,  64,  82, 255);
constexpr ImU32 k_ring_high   = IM_COL32( 50,  57,  74, 255);
constexpr ImU32 k_diamond     = IM_COL32(150, 162, 188, 255);
constexpr ImU32 k_diamond_rim = IM_COL32( 20,  22,  32, 255);
constexpr ImU32 k_pip_free    = IM_COL32(165, 175, 200, 255);
constexpr ImU32 k_pip_held    = IM_COL32(200, 205, 215, 255); ///< A holder with no nation colour.
constexpr ImU32 k_pip_rim     = IM_COL32( 14,  16,  24, 255); ///< Dark rim under every pip.

// Constant pixel sizes — none of these scale with zoom.
constexpr float k_diamond_r   = 4.0f;  ///< Centre to tip.
constexpr float k_pip_r       = 2.4f;
constexpr float k_pip_step    = 6.5f;  ///< Centre-to-centre along a row.
constexpr float k_pip_gap     = 4.0f;  ///< From the node to the first pip's edge.
constexpr float k_ring_hit    = 3.0f;  ///< Either side of a ring's line.
constexpr float k_diamond_hit = 6.0f;

const char* node_kind_suffix(orbital_node_kind k)
{
    switch (k)
    {
        case orbital_node_kind::surface:    return "surface";
        case orbital_node_kind::low_orbit:  return "low orbit";
        case orbital_node_kind::high_orbit: return "high orbit";
        case orbital_node_kind::l1:         return "L1";
        case orbital_node_kind::l2:         return "L2";
        case orbital_node_kind::l4:         return "L4";
        case orbital_node_kind::l5:         return "L5";
    }
    return "";
}

float pip_row_end(const circum_node_mark& m)
{
    const float n = static_cast<float>(std::max(0, m.capacity - 1));
    return m.pip_start.x + m.pip_dir * n * k_pip_step;
}

/// The owners of the assets holding @p n's slots — the world's
/// `orbital_slots_held` rule (a satellite or station core, not lost), walked
/// in ascending asset id.
std::vector<entity_id> slot_holders(const world& w, orbital_node n)
{
    std::vector<entity_id> out;
    for (const auto& [id, a] : w.space_assets)
        if (space_class_holds_slot(a.cls) && a.state != space_asset_state::lost && a.node == n)
            out.push_back(a.owner);
    return out;
}

} // namespace

std::string orbital_node_display_name(const world& w, orbital_node n)
{
    const auto it = w.bodies.find(n.body);
    const std::string body = it != w.bodies.end() ? it->second.name : std::string("?");
    return body + " " + node_kind_suffix(n.kind);
}

std::vector<circum_node_mark> layout_circum_nodes(const world& w, entity_id anchor,
                                                  const std::vector<entity_id>& moons,
                                                  const std::vector<float>& body_radius_px,
                                                  const circum_frame& f)
{
    std::vector<circum_node_mark> out;
    if (!w.bodies.count(anchor))
        return out;

    const orbital_position anchor_au = orbital_node_position(w, {anchor, orbital_node_kind::low_orbit});
    auto to_screen_au = [&](orbital_position p) -> ImVec2 {
        const float dx =  static_cast<float>(p.x_au - anchor_au.x_au) * f.scale;
        const float dy = -static_cast<float>(p.y_au - anchor_au.y_au) * f.scale;
        return { f.view_origin.x + dx * f.zoom, f.view_origin.y + dy * f.zoom };
    };

    // Drawn moon-orbit radii in px, for the spacing the rings must sit inside.
    std::vector<float> orbit_px;
    orbit_px.reserve(moons.size());
    for (entity_id id : moons)
        orbit_px.push_back(w.bodies.at(id).orbital_radius_au * f.scale * f.zoom);
    float inner_px = std::numeric_limits<float>::max();
    for (float r : orbit_px)
        if (r > 0.0f) inner_px = std::min(inner_px, r);
    if (inner_px == std::numeric_limits<float>::max())
        inner_px = 0.5f * f.scale * f.zoom; // a moonless anchor: the canvas's 0.5 AU floor

    auto add_rings = [&](entity_id body, ImVec2 pos, float low_r, float high_r) {
        const orbital_node_kind kinds[2] = { orbital_node_kind::low_orbit, orbital_node_kind::high_orbit };
        const float radii[2] = { low_r, high_r };
        for (int i = 0; i < 2; ++i)
        {
            circum_node_mark m;
            m.node     = { body, kinds[i] };
            m.pos      = pos;
            m.ring_r   = radii[i];
            m.capacity = orbital_slot_capacity(w, m.node);
            m.held     = std::min(m.capacity, orbital_slots_held(w, m.node));
            m.holders  = slot_holders(w, m.node);
            // Pips ride the ring's top: the low orbit's row runs right, the high
            // orbit's left, so the two rows never share a run of the canvas.
            m.pip_dir   = i == 0 ? 1.0f : -1.0f;
            m.pip_start = { pos.x + m.pip_dir * (k_pip_gap + k_pip_r), pos.y - radii[i] };
            out.push_back(m);
        }
    };

    // The anchor's rings, inside the innermost moon orbit.
    {
        const float br   = body_radius_px.empty() ? 6.0f : body_radius_px[0];
        const float low  = std::max(br + 5.0f, 0.16f * inner_px);
        const float high = std::max(low + 6.0f, 0.30f * inner_px);
        add_rings(anchor, f.view_origin, low, high);
    }

    for (std::size_t i = 0; i < moons.size(); ++i)
    {
        const entity_id moon = moons[i];
        const ImVec2 pos = to_screen_au(orbital_node_position(w, {moon, orbital_node_kind::low_orbit}));

        // Spacing to the nearest neighbouring orbit (the anchor counts as one).
        float gap = orbit_px[i];
        for (std::size_t j = 0; j < moons.size(); ++j)
            if (j != i)
            {
                const float d = std::fabs(orbit_px[i] - orbit_px[j]);
                if (d > 0.0f) gap = std::min(gap, d);
            }
        // Kept inside the L1/L2 offset (15% of the drawn orbit), so a pair's
        // L1 and L2 diamonds sit outside the moon's own rings when there is room.
        const float cap  = 0.11f * orbit_px[i];
        const float br   = i + 1 < body_radius_px.size() ? body_radius_px[i + 1] : 2.0f;
        const float low  = std::max(br + 3.0f, std::min(0.16f * gap, 0.6f * cap));
        const float high = std::max(low + 4.0f, std::min(0.30f * gap, cap));
        add_rings(moon, pos, low, high);

        const orbital_node_kind lagrange[4] = { orbital_node_kind::l1, orbital_node_kind::l2,
                                                orbital_node_kind::l4, orbital_node_kind::l5 };
        for (orbital_node_kind k : lagrange)
        {
            circum_node_mark m;
            m.node      = { moon, k };
            m.pos       = to_screen_au(orbital_node_position(w, m.node));
            if (m.pos.y > f.safe_bottom)
            {
                m.pos.y   = f.safe_bottom;
                m.clamped = true;
            }
            m.capacity  = orbital_slot_capacity(w, m.node);
            m.held      = std::min(m.capacity, orbital_slots_held(w, m.node));
            m.holders   = slot_holders(w, m.node);
            m.pip_dir   = 1.0f;
            m.pip_start = { m.pos.x + k_diamond_r + k_pip_gap + k_pip_r, m.pos.y };
            out.push_back(m);
        }
    }
    return out;
}

void draw_circum_node_rings(ImDrawList* dl, const std::vector<circum_node_mark>& marks)
{
    for (const circum_node_mark& m : marks)
        if (m.ring_r > 0.0f)
            dl->AddCircle(m.pos, m.ring_r,
                          m.node.kind == orbital_node_kind::low_orbit ? k_ring_low : k_ring_high,
                          0, 1.0f);
}

void draw_circum_node_marks(ImDrawList* dl, const std::vector<circum_node_mark>& marks)
{
    for (const circum_node_mark& m : marks)
    {
        if (m.ring_r <= 0.0f)
        {
            const ImVec2 c = m.pos;
            const float  r = k_diamond_r;
            dl->AddQuadFilled({c.x, c.y - r - 1.0f}, {c.x + r + 1.0f, c.y},
                              {c.x, c.y + r + 1.0f}, {c.x - r - 1.0f, c.y}, k_diamond_rim);
            dl->AddQuadFilled({c.x, c.y - r}, {c.x + r, c.y}, {c.x, c.y + r}, {c.x - r, c.y},
                              k_diamond);
        }
        for (int s = 0; s < m.capacity; ++s)
        {
            const ImVec2 p = { m.pip_start.x + m.pip_dir * static_cast<float>(s) * k_pip_step,
                               m.pip_start.y };
            dl->AddCircleFilled(p, k_pip_r + 1.0f, k_pip_rim, 10);
            if (s < m.held)
            {
                const ImU32 c = static_cast<std::size_t>(s) < m.holders.size()
                                    ? palette::nation_colour(m.holders[static_cast<std::size_t>(s)])
                                    : k_pip_held;
                dl->AddCircleFilled(p, k_pip_r, c, 10);
            }
            else
                dl->AddCircle(p, k_pip_r - 0.4f, k_pip_free, 10, 1.3f);
        }
    }
}

int hit_circum_node(const std::vector<circum_node_mark>& marks, ImVec2 mouse)
{
    int   best      = -1;
    int   best_tier = 2; // 0 diamond / pip row, 1 ring
    float best_d    = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < marks.size(); ++i)
    {
        const circum_node_mark& m = marks[i];
        int   tier = 2;
        float d    = std::numeric_limits<float>::max();

        // The pip row, as a band one pip tall.
        if (m.capacity > 0)
        {
            const float x0 = std::min(m.pip_start.x, pip_row_end(m)) - k_pip_r - 1.0f;
            const float x1 = std::max(m.pip_start.x, pip_row_end(m)) + k_pip_r + 1.0f;
            const float y0 = m.pip_start.y - k_pip_r - 2.0f;
            const float y1 = m.pip_start.y + k_pip_r + 2.0f;
            if (mouse.x >= x0 && mouse.x <= x1 && mouse.y >= y0 && mouse.y <= y1)
            {
                tier = 0;
                d    = std::fabs(mouse.y - m.pip_start.y);
            }
        }
        const float dx = mouse.x - m.pos.x;
        const float dy = mouse.y - m.pos.y;
        const float dc = std::sqrt(dx * dx + dy * dy);
        if (m.ring_r <= 0.0f)
        {
            if (dc <= k_diamond_hit && (tier > 0 || dc < d)) { tier = 0; d = dc; }
        }
        else if (tier > 1)
        {
            const float off = std::fabs(dc - m.ring_r);
            if (off <= k_ring_hit) { tier = 1; d = off; }
        }
        if (tier < best_tier || (tier == best_tier && tier < 2 && d < best_d))
        {
            best      = static_cast<int>(i);
            best_tier = tier;
            best_d    = d;
        }
    }
    return best;
}

void draw_circum_node_card(const world& w, const circum_node_mark& m, ImVec2 mouse)
{
    const std::string name = orbital_node_display_name(w, m.node);
    draw_hover_card(mouse, [&]() {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", name.c_str());
        ImGui::Text("Free slots: %d of %d", m.capacity - m.held, m.capacity);

        const orbital_node home = { w.home_body, orbital_node_kind::surface };
        const auto hb = w.bodies.find(w.home_body);
        if (hb == w.bodies.end())
        {
            ImGui::TextDisabled("No home surface to reach it from");
            return;
        }
        const orbital_path_result path = orbital_path(w, orbital_graph_params{}, home, m.node);
        if (!path.ok)
        {
            ImGui::TextDisabled("No route from %s", hb->second.name.c_str());
            return;
        }
        // U+2206 (Mathematical Operators, in the font's ranges) stands for the Δ.
        ImGui::TextWrapped("From %s surface: %.1f \xE2\x88\x86v, %.1f days",
                           hb->second.name.c_str(), path.dv, path.days);
    });
}

} // namespace ui
