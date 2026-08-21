// Headless river-generation harness (BL-170). No SDL / Lua / ImGui.
//
// Exercises generate_rivers directly against a small synthetic body (a triangular
// ridge sloping down to an ocean band on a hand-built heightmap) rather than the
// full six-pass pipeline, so the assertions are cheap and the setup is legible.
// Asserts:
//   1. Determinism — same seed produces an identical river-edge/downstream bitmask
//      set across two generations; a different seed produces a different set.
//   2. Mutual consistency — every recorded edge borders exactly two tiles, and if
//      tile A records a river on the side facing B, B records it on the side
//      facing A.
//   3. Monotonic descent — every recorded (from, downstream-side) edge strictly
//      decreases height from `from` to the neighbour it crosses into; no cycles.
//   4. river_edge_discount is strictly cheaper than "no river" on a river-adjacent
//      edge, and downstream is cheaper than upstream.
//
// Exits 0 on PASS, non-zero on any failure (naming it).

#include "world/components.hpp"
#include "world/river_generation.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

// Builds a synthetic 12x10 body: height descends from a ridge at the top rows
// toward an ocean band at the bottom rows, with some cross-grid noise-like
// variation so the trace is not a single straight line. Deterministic — pure
// function of (gw, gh, salt).
struct synth_body
{
    world w;
    std::vector<entity_id> tiles;
    std::vector<float> height;
    int gw = 12;
    int gh = 10;
};

synth_body build_synth_body(uint32_t salt)
{
    synth_body sb;
    sb.tiles.resize(static_cast<std::size_t>(sb.gw * sb.gh));
    sb.height.resize(static_cast<std::size_t>(sb.gw * sb.gh));

    const entity_id body_id = sb.w.create_entity();
    sb.w.bodies[body_id] = body_component{
        .name = "Synth", .type = body_type::planet, .parent = null_entity,
        .grid_width = sb.gw, .grid_height = sb.gh,
    };

    for (int row = 0; row < sb.gh; ++row)
    {
        for (int col = 0; col < sb.gw; ++col)
        {
            const int idx = col + row * sb.gw;
            // Ridge at row 0 (height ~1.0), descending toward ocean at the last two
            // rows (height ~0.0). A small per-column jitter (from `salt`) keeps the
            // slope from being perfectly uniform without breaking monotonic descent
            // row-to-row, which is what guarantees a clean trace to the ocean band.
            const float row_frac = 1.0f - static_cast<float>(row) / static_cast<float>(sb.gh - 1);
            const uint32_t h = (static_cast<uint32_t>(col) * 2654435761u + salt) & 0xFFu;
            const float jitter = (static_cast<float>(h) / 255.0f) * 0.02f;
            sb.height[static_cast<std::size_t>(idx)] = row_frac * 0.9f + jitter;

            const bool is_ocean = row >= sb.gh - 2;
            terrain_landform lf = terrain_landform::plains;
            if (!is_ocean && row <= 1)
                lf = terrain_landform::mountain; // source candidates: top two rows.

            const entity_id tid = sb.w.create_entity();
            sb.w.tiles[tid] = tile_component{
                .body = body_id, .grid_x = col, .grid_y = row,
                .substrate = is_ocean ? terrain_substrate::ocean : terrain_substrate::rocky,
                .landform = lf,
            };
            sb.tiles[static_cast<std::size_t>(idx)] = tid;
        }
    }
    return sb;
}

// Snapshot of (river_edges, river_downstream) per raster index, for comparison.
std::vector<std::pair<uint8_t, uint8_t>> snapshot(const synth_body& sb)
{
    std::vector<std::pair<uint8_t, uint8_t>> out;
    out.reserve(sb.tiles.size());
    for (const entity_id tid : sb.tiles)
    {
        const auto& tc = sb.w.tiles.at(tid);
        out.emplace_back(tc.river_edges, tc.river_downstream);
    }
    return out;
}

} // namespace

int main()
{
    // --- 1. Determinism: same seed twice ---
    synth_body a = build_synth_body(1);
    synth_body b = build_synth_body(1);
    generate_rivers(a.w, a.tiles, a.gw, a.gh, a.height, /*seed=*/0xABCDu);
    generate_rivers(b.w, b.tiles, b.gw, b.gh, b.height, /*seed=*/0xABCDu);
    check(snapshot(a) == snapshot(b), "same seed -> identical river bitmasks");

    // --- 1b. Different seed differs ---
    synth_body c = build_synth_body(1);
    generate_rivers(c.w, c.tiles, c.gw, c.gh, c.height, /*seed=*/0x1234u);
    check(snapshot(a) != snapshot(c), "different seed -> different river bitmasks");

    // Sanity: rivers were actually generated (a degenerate all-zero result would
    // trivially pass the checks below without testing anything).
    bool any_river = false;
    for (const auto& [edges, down] : snapshot(a))
    {
        (void)down;
        if (edges != 0) { any_river = true; break; }
    }
    check(any_river, "at least one river edge was generated");

    // --- 2. Mutual consistency + 3. monotonic descent, over world `a` ---
    bool mutual_ok = true;
    bool descent_ok = true;
    for (int idx = 0; idx < a.gw * a.gh; ++idx)
    {
        const auto& tc = a.w.tiles.at(a.tiles[static_cast<std::size_t>(idx)]);
        if (tc.river_edges == 0) continue;
        const int col = idx % a.gw;
        const int row = idx / a.gw;
        const bool odd_row = (row & 1) != 0;

        for (int side = 0; side < 6; ++side)
        {
            const auto bit = static_cast<uint8_t>(1u << side);
            if ((tc.river_edges & bit) == 0) continue;

            // Recover the neighbour via the same hex-side convention river_generation.cpp
            // uses (E,NE,NW,W,SW,SE); hex_side_for_offset is the shared, tested mapping,
            // so search over all (dc,dr) offsets for the one matching this side.
            int nrow = -1, ncol = -1, dcf = 0, drf = 0;
            bool found_offset = false;
            for (int dr = -1; dr <= 1 && !found_offset; ++dr)
                for (int dc = -1; dc <= 1 && !found_offset; ++dc)
                {
                    if (dc == 0 && dr == 0) continue;
                    if (hex_side_for_offset(dc, dr, odd_row) == side)
                    {
                        dcf = dc; drf = dr; found_offset = true;
                    }
                }
            check(found_offset, "hex side resolves to a neighbour offset");
            if (!found_offset) continue;

            nrow = row + drf;
            ncol = ((col + dcf) % a.gw + a.gw) % a.gw;
            if (nrow < 0 || nrow >= a.gh) { mutual_ok = false; continue; }
            const int nidx = ncol + nrow * a.gw;
            const auto& ntc = a.w.tiles.at(a.tiles[static_cast<std::size_t>(nidx)]);

            const int opp_side = (side + 3) % 6;
            const auto opp_bit = static_cast<uint8_t>(1u << opp_side);
            if ((ntc.river_edges & opp_bit) == 0)
                mutual_ok = false;

            const bool downstream_here = (tc.river_downstream & bit) != 0;
            if (downstream_here)
            {
                // Downstream from `tc`: height must strictly decrease into the neighbour.
                if (!(a.height[static_cast<std::size_t>(nidx)] < a.height[static_cast<std::size_t>(idx)]))
                    descent_ok = false;
            }
        }
    }
    check(mutual_ok, "every recorded river edge is mutually consistent between its two tiles");
    check(descent_ok, "every downstream edge strictly decreases height (no cycles)");

    // --- 4. river_edge_discount ---
    {
        tile_component plain_tc{};
        plain_tc.river_edges = 0;
        plain_tc.river_downstream = 0;
        check(river_edge_discount(plain_tc, 0) == 1.0f, "no river on side -> discount 1.0 (no discount)");

        tile_component down_tc{};
        down_tc.river_edges = 0b0000'0001; // side 0
        down_tc.river_downstream = 0b0000'0001; // side 0 is downstream
        const float down_mult = river_edge_discount(down_tc, 0);
        check(down_mult < 1.0f, "downstream river edge is strictly cheaper than no river");

        tile_component up_tc{};
        up_tc.river_edges = 0b0000'0001; // side 0
        up_tc.river_downstream = 0; // side 0 is upstream (inflow)
        const float up_mult = river_edge_discount(up_tc, 0);
        check(up_mult < 1.0f, "upstream river edge is strictly cheaper than no river");
        check(down_mult < up_mult, "downstream is strictly cheaper than upstream");
    }

    if (g_failures == 0)
        std::printf("\nRIVER GENERATION OK - deterministic, consistent, monotonic, and discounted.\n");
    else
        std::printf("\nRIVER GENERATION FAIL - %d divergence(s).\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
