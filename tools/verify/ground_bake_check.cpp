// Headless ground-bake harness (BL-732, docs/ui/RENDERING.md). No SDL / Lua /
// ImGui — compiles ui/ground_bake.cpp + ui/terrain_palette.cpp beside the world
// objects, which is the whole reason those two files are pure.
//
// Requirement group `ground-bake-renderer` row R2:
//
//   P1  Determinism: the same window bakes byte-identical twice.
//   P2  Wrap: a window at px0 = W (one full period east) bakes byte-identical
//       to the window at px0 = 0 — the cylinder seam carries no discontinuity,
//       grain noise included.
//   P3  Class separation: a land window and a water window differ, and the
//       water one reads blue (b > r on average).
//   P4  Relief responds: relief_gain 0 vs default changes the land bake.
//   P5  The grade is separable and desaturates: grade off vs on changes the
//       bake, and mean channel spread (a cheap saturation proxy) drops.
//   P6  Mask: a source built WITHOUT reveal_all on an unsurveyed body bakes
//       the lock colour, and leaks no terrain hue.
//   P7  region_hash moves when a tile field the bake reads moves.
//   P8/P9 close-tier and oblique bakes stay pure and wrap-exact.
//
// Requirement group `ground-never-magnified` (BL-1244):
//
//   P10 The tier chooser: smallest tier with ppr >= draw_r, far page at or
//       below 1:1, no radius magnified up to the 192 px top tier, no chunked
//       tier minified past 2:1.
//   P11 The 2x supersampled bake is pure, wrap-exact, seamless across a chunk
//       edge, and not a no-op.
//   P12 (a reading) bake ms per 512 px chunk per tier, 1x vs 2x.
//   P13 Installations (BL-1241, structures baked): the structure pass is
//       deterministic and wrap-exact at the flat, oblique and far geometries,
//       actually draws (on vs off differs), and leaves a masked window exact.
//   P14 The chunk hash folds installations: a build, a stack, a recipe-family
//       change, a scale step and a raze each move it; a tile outside the
//       window's reach does not.
// Also (not checks): bake ms per tier with the pass off vs on, and a gallery
// of every procedural form at every tier (structures_gallery_*.png).
//   P15 Landform relief (BL-1242): a bridged mountain run's window is moved
//       by the pass, deterministic, wrap-exact flat and oblique; ground with
//       no feature near it is untouched.
//   P16 Rivers (BL-1242): a river mouth's window likewise.
//
// Also prints bake time per tier (a measurement, not a check) and writes
// feature_<form>_<tier>.png previews for the eye.
//
// Exits 0 on PASS, non-zero naming the failed phase.

#include "ui/ground_bake.hpp"
#include "ui/terrain_palette.hpp"
#include "core/png_writer.hpp"
#include "world/hard_coded_world.hpp"
#include "world/survey_system.hpp"
#include "world/hex_neighbors.hpp"
#include "world/world.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace ui::ground;

namespace {

int g_failures = 0;

void check(bool ok, const char* phase, const char* what)
{
    std::printf("%s  %s: %s\n", ok ? "PASS" : "FAIL", phase, what);
    if (!ok)
        ++g_failures;
}

struct stats { double r = 0, g = 0, b = 0, spread = 0; int n = 0; };

stats measure(const std::vector<std::uint32_t>& px)
{
    stats s;
    for (std::uint32_t c : px)
    {
        if (ui::palette::col_a(c) == 0)
            continue;
        const int r = ui::palette::col_r(c), g = ui::palette::col_g(c),
                  b = ui::palette::col_b(c);
        s.r += r; s.g += g; s.b += b;
        const int mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
        s.spread += mx - mn;
        ++s.n;
    }
    if (s.n)
    {
        s.r /= s.n; s.g /= s.n; s.b /= s.n; s.spread /= s.n;
    }
    return s;
}

} // namespace

int main()
{
    generation_report report;
    world w = make_hard_coded_world(no_prehistory(), &report);

    // The home body: fully surveyed, land + ocean, the canvas's opening view.
    entity_id home = w.home_body;
    if (home == null_entity || !w.bodies.count(home))
    {
        // Fall back to the largest surveyed grid.
        int best = 0;
        for (const auto& [id, b] : w.bodies)
            if (b.grid_width * b.grid_height > best
                && b.survey.phase == survey_phase::surveyed)
            {
                best = b.grid_width * b.grid_height;
                home = id;
            }
    }
    if (home == null_entity)
    {
        std::printf("FAIL  setup: no surveyed body with a grid\n");
        return 1;
    }
    const body_component& hb = w.bodies.at(home);
    std::printf("Body: %s (%dx%d)\n", hb.name.c_str(), hb.grid_width, hb.grid_height);

    const geometry    g   = make_geometry(hb.grid_width, hb.grid_height, 24.0);
    const bake_params p;
    // reveal_all: P1-P5 test bake properties on open ground; the mask has its
    // own phase (P6), on a body that is actually unsurveyed.
    const bake_source src = prepare_source(w, home, /*reveal_all=*/true);

    // Find one land and one water tile to aim windows at.
    int land_i = -1, water_i = -1;
    for (int i = 0; i < static_cast<int>(src.cls.size()); ++i)
    {
        const auto c = static_cast<bake_source::tile_class>(src.cls[i]);
        if (c == bake_source::tile_class::land  && land_i  < 0) land_i  = i;
        if (c == bake_source::tile_class::water && water_i < 0) water_i = i;
        if (land_i >= 0 && water_i >= 0)
            break;
    }
    check(land_i >= 0 && water_i >= 0, "setup", "found land and water tiles");
    if (g_failures)
        return 1;

    auto window_at = [&](int tile_idx, const bake_params& bp,
                         std::vector<std::uint32_t>& out, int side = 96)
    {
        const int r = tile_idx / src.gw, c = tile_idx % src.gw;
        const double cx = 1.7320508075688772 * (c + ((r & 1) ? 0.5 : 0.0));
        const double cy = 1.5 * r;
        const int px0 = static_cast<int>(cx * g.s) - side / 2;
        const int py0 = std::clamp(static_cast<int>((cy - g.y_min) * g.s) - side / 2,
                                   0, std::max(0, g.H - side));
        out.assign(static_cast<std::size_t>(side) * side, 0u);
        bake_region(src, g, bp, px0, py0, side, side, out.data());
        return std::pair<int, int>{ px0, py0 };
    };

    // P1 — determinism.
    std::vector<std::uint32_t> a, b;
    window_at(land_i, p, a);
    window_at(land_i, p, b);
    check(a == b, "P1", "same window bakes byte-identical twice");

    // P2 — wrap: px0 shifted one full period east is the same ground.
    {
        std::vector<std::uint32_t> east(a.size(), 0u);
        const int r = land_i / src.gw, c = land_i % src.gw;
        const double cx = 1.7320508075688772 * (c + ((r & 1) ? 0.5 : 0.0));
        const double cy = 1.5 * r;
        const int side = 96;
        const int px0 = static_cast<int>(cx * g.s) - side / 2;
        const int py0 = std::clamp(static_cast<int>((cy - g.y_min) * g.s) - side / 2,
                                   0, std::max(0, g.H - side));
        bake_region(src, g, p, px0 + g.W, py0, side, side, east.data());
        check(a == east, "P2", "one wrap period east bakes byte-identical (seamless cylinder)");
    }

    // P3 — class separation.
    std::vector<std::uint32_t> wpx;
    window_at(water_i, p, wpx);
    const stats ls = measure(a), ws = measure(wpx);
    check(ws.b > ws.r, "P3", "water reads blue (mean b > mean r)");
    check(std::fabs(ls.r - ws.r) + std::fabs(ls.g - ws.g) + std::fabs(ls.b - ws.b) > 20.0,
          "P3", "land and water windows are distinct");

    // P4 — relief responds.
    {
        bake_params flat = p;
        flat.relief_gain = 0.0f;
        flat.altitude_gain = 0.0f;
        flat.landform_accent = 0.0f;
        std::vector<std::uint32_t> f;
        window_at(land_i, flat, f);
        check(a != f, "P4", "relief_gain moves the land bake");
    }

    // P5 — the grade is separable and desaturates.
    {
        bake_params raw = p;
        raw.grade_enabled = false;
        std::vector<std::uint32_t> u;
        window_at(land_i, raw, u);
        const stats us = measure(u);
        check(u != a, "P5", "grade off changes the bake");
        check(ls.spread < us.spread, "P5", "grade lowers mean channel spread (desaturates)");
    }

    // P6 — mask: an unsurveyed body bakes the lock colour only.
    {
        entity_id hidden = null_entity;
        for (const auto& [id, bd] : w.bodies)
            if (bd.grid_width > 0 && bd.survey.phase == survey_phase::hidden)
            {
                hidden = id;
                break;
            }
        if (hidden == null_entity)
            std::printf("SKIP  P6: no unsurveyed body on this seed\n");
        else
        {
            const body_component& bd = w.bodies.at(hidden);
            const geometry    hg  = make_geometry(bd.grid_width, bd.grid_height, 24.0);
            const bake_source hsv = prepare_source(w, hidden); // mask ON
            std::vector<std::uint32_t> m(96u * 96u, 0u);
            bake_region(hsv, hg, p, hg.W / 2, hg.H / 2, 96, 96, m.data());
            bool all_lock = true;
            for (std::uint32_t c : m)
                if (ui::palette::col_a(c) != 0 && c != ui::palette::col32(12, 14, 20, 255))
                {
                    all_lock = false;
                    break;
                }
            check(all_lock, "P6", "unsurveyed ground bakes as the flat lock colour only");
        }
    }

    // P7 — the content hash sees a field the bake reads.
    {
        bake_source mut = src;
        const std::uint64_t h0 = region_hash(mut, g, 0, 0, 256, 256);
        // Move a tile the window covers: tile (2, 2) is inside pixel (0,0)-(256,256)
        // at s = 24 for any grid this harness runs on.
        const std::size_t i = 2u * src.gw + 2u;
        mut.colour[i] ^= 0x00202020u;
        const std::uint64_t h1 = region_hash(mut, g, 0, 0, 256, 256);
        check(h0 != h1, "P7", "region_hash moves when a covered tile's colour moves");
    }

    // P8 — the close tiers stay pure with every close-only pass active
    // (feature stamps, ridged detail, res_t dials, the fine octave): at
    // 48 px/r the same window bakes byte-identical twice and one wrap period
    // east bakes byte-identical. Aimed at a forest tile so the tree stamps
    // actually run in-window rather than passing vacuously.
    {
        const geometry g48 = make_geometry(hb.grid_width, hb.grid_height, 48.0);
        int forest_i = land_i;
        for (int i = 0; i < static_cast<int>(src.cover.size()); ++i)
            if (src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::land)
                && static_cast<terrain_cover>(src.cover[i]) == terrain_cover::forest)
            {
                forest_i = i;
                break;
            }
        check(static_cast<terrain_cover>(src.cover[forest_i]) == terrain_cover::forest,
              "P8", "found a forest tile for the stamp window");
        const int fr = forest_i / src.gw, fc = forest_i % src.gw;
        const double fx = 1.7320508075688772 * (fc + ((fr & 1) ? 0.5 : 0.0));
        const double fy = 1.5 * fr;
        const int side = 128;
        const int px0 = static_cast<int>(fx * g48.s) - side / 2;
        const int py0 = std::clamp(static_cast<int>((fy - g48.y_min) * g48.s) - side / 2,
                                   0, std::max(0, g48.H - side));
        std::vector<std::uint32_t> a48(static_cast<std::size_t>(side) * side);
        std::vector<std::uint32_t> b48(a48.size()), e48(a48.size());
        bake_region(src, g48, p, px0, py0, side, side, a48.data());
        bake_region(src, g48, p, px0, py0, side, side, b48.data());
        bake_region(src, g48, p, px0 + g48.W, py0, side, side, e48.data());
        check(a48 == b48, "P8", "48 px tier bakes byte-identical twice (stamps + ridged active)");
        check(a48 == e48, "P8", "48 px tier wraps byte-identical one period east");

        // P9 — the OBLIQUE bake (BL-737): at 45 degrees the same window bakes
        // byte-identical twice and one wrap period east; and the projection
        // actually projects (the tilted window differs from the flat one).
        const geometry g45 = make_geometry(hb.grid_width, hb.grid_height, 48.0, 0.70710678);
        const int tpy0 = std::clamp(
            static_cast<int>((fy - g45.y_min) * g45.s) - side / 2, 0, std::max(0, g45.H - side));
        std::vector<std::uint32_t> t1(a48.size()), t2(a48.size()), t3(a48.size());
        bake_region(src, g45, p, px0, tpy0, side, side, t1.data());
        bake_region(src, g45, p, px0, tpy0, side, side, t2.data());
        bake_region(src, g45, p, px0 + g45.W, tpy0, side, side, t3.data());
        check(t1 == t2, "P9", "45-degree oblique bake is byte-identical twice");
        check(t1 == t3, "P9", "45-degree oblique bake wraps byte-identical one period east");
        check(g45.lift > 0.8 && g45.lift < 1.0, "P9", "45-degree lift derives to ~0.9 canonical");
        check(t1 != a48, "P9", "the oblique projection differs from the flat bake");
    }

    // P10 — the never-magnify chooser (BL-1244, RENDERING.md § Level of
    // detail): the smallest tier at or above the drawn radius; the far page
    // only at or below 1:1; minification never past 2:1 on the ladder; the
    // 192 px tier exists. Rows include the measured rung radii at 1720x1080
    // (5.93 / 12.87 / 26.73 / 54.47 / 109.66) and 3840x2160 (13.0 ... 222.41).
    {
        const double* L = k_tier_ladder;
        const int     N = k_tier_count;
        check(N >= 5 && L[N - 1] == 192.0, "P10", "a 192 px chunked tier tops the ladder");
        const struct { double r; int want; } rows[] = {
            { 5.93, -1 }, { 6.0, -1 }, { 6.01, 0 }, { 12.0, 0 }, { 12.87, 1 }, { 13.0, 1 },
            { 26.73, 2 }, { 27.0, 2 }, { 54.47, 3 }, { 54.99, 3 }, { 96.0, 3 },
            { 109.66, 4 }, { 110.98, 4 }, { 192.0, 4 }, { 222.41, 4 },
        };
        bool all = true;
        for (const auto& row : rows)
        {
            const int got = choose_tier(row.r, k_far_ppr, L, N);
            if (got != row.want)
            {
                std::printf("      draw_r %.2f -> tier %d, want %d\n", row.r, got, row.want);
                all = false;
            }
        }
        check(all, "P10", "chooser picks the smallest tier with ppr >= draw_r (far page at <= 6)");
        // Sweep: every radius the ladder covers draws at texel/px in [1, 2].
        bool never_mag = true, within_2 = true;
        for (double r = 0.5; r <= L[N - 1]; r += 0.0625)
        {
            const int t = choose_tier(r, k_far_ppr, L, N);
            const double ppr = t < 0 ? k_far_ppr : L[t];
            if (ppr / r < 1.0) never_mag = false;
            if (r > k_far_ppr && ppr / r > 2.0) within_2 = false;
        }
        check(never_mag, "P10", "no radius up to the top tier is magnified (texel/px >= 1)");
        check(within_2, "P10", "no chunked tier is minified past 2:1");
    }

    // P11 — supersampling (BL-1244): the 2x bake is pure (byte-identical
    // twice), wrap-exact (one period east), seamless (two half windows equal
    // the whole — the downsample and the nominal unsharp cannot seam a chunk
    // edge), and actually different from the single-sample bake.
    {
        const geometry g48 = make_geometry(hb.grid_width, hb.grid_height, 48.0);
        const int lr = land_i / src.gw, lc = land_i % src.gw;
        const double lx = 1.7320508075688772 * (lc + ((lr & 1) ? 0.5 : 0.0));
        const int side = 96;
        const int px0 = static_cast<int>(lx * g48.s) - side / 2;
        const int py0 = std::clamp(static_cast<int>((1.5 * lr - g48.y_min) * g48.s) - side / 2,
                                   0, std::max(0, g48.H - side));
        bake_params p2 = p;   p2.supersample = 2;
        bake_params p1 = p;   p1.supersample = 1;
        const std::size_t n = static_cast<std::size_t>(side) * side;
        std::vector<std::uint32_t> a(n), b(n), e(n), s1(n), whole(n * 2), l(n), r(n);
        bake_region(src, g48, p2, px0, py0, side, side, a.data());
        bake_region(src, g48, p2, px0, py0, side, side, b.data());
        bake_region(src, g48, p2, px0 + g48.W, py0, side, side, e.data());
        bake_region(src, g48, p1, px0, py0, side, side, s1.data());
        check(a == b, "P11", "2x supersampled bake is byte-identical twice");
        check(a == e, "P11", "2x supersampled bake wraps byte-identical one period east");
        check(a != s1, "P11", "supersampling changes the bake (it is not a no-op)");
        // Seam: a 2*side-wide window vs its two halves baked separately.
        bake_region(src, g48, p2, px0, py0, side * 2, side, whole.data());
        bake_region(src, g48, p2, px0, py0, side, side, l.data());
        bake_region(src, g48, p2, px0 + side, py0, side, side, r.data());
        bool seam_ok = true;
        for (int y = 0; y < side && seam_ok; ++y)
            for (int x = 0; x < side; ++x)
                if (whole[static_cast<std::size_t>(y) * side * 2 + x] != l[static_cast<std::size_t>(y) * side + x]
                    || whole[static_cast<std::size_t>(y) * side * 2 + side + x] != r[static_cast<std::size_t>(y) * side + x])
                {
                    seam_ok = false;
                    break;
                }
        check(seam_ok, "P11", "two adjacent 2x windows equal one window spanning both (no chunk seam)");
    }

    // P12 — bake cost per tier (a reading, not a check): one 512 px chunk per
    // tier at 1x and 2x, wall-clock ms, so R3's before/after has a number
    // independent of the app's viewport. The window is centred on land.
    {
        const int lr = land_i / src.gw, lc = land_i % src.gw;
        const double lx = 1.7320508075688772 * (lc + ((lr & 1) ? 0.5 : 0.0));
        const double ladder[] = { k_far_ppr, 12.0, 24.0, 48.0, 96.0, 192.0 };
        std::vector<std::uint32_t> buf(512u * 512u);
        for (const double ppr : ladder)
        {
            const geometry g = make_geometry(hb.grid_width, hb.grid_height, ppr);
            const int cw = std::min(512, g.W), ch = std::min(512, g.H);
            const int px0 = static_cast<int>(lx * g.s) - cw / 2;
            const int py0 = std::clamp(static_cast<int>((1.5 * lr - g.y_min) * g.s) - ch / 2,
                                       0, std::max(0, g.H - ch));
            double ms[2] = {};
            for (int k = 0; k < 2; ++k)
            {
                bake_params pk = p;
                pk.supersample = k == 0 ? 1 : 2;
                const auto t0 = std::chrono::steady_clock::now();
                bake_region(src, g, pk, px0, py0, cw, ch, buf.data());
                ms[k] = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0).count();
            }
            std::printf("TIMING  tier %3.0f px/r  chunk %dx%d  1x %8.1f ms  2x %8.1f ms  (x%.2f)\n",
                        ppr, cw, ch, ms[0], ms[1], ms[0] > 0.0 ? ms[1] / ms[0] : 0.0);
        }
    }

    // ------------------------------------------------------------------
    // BL-1241 (structures baked): the installation pass.
    // ------------------------------------------------------------------
    {
        std::printf("installations on the home body: %zu tiles carry one\n", src.inst.list.size());

        // A GALLERY source: the home ground with every procedural form staged
        // on a land block, so each form is exercised (and previewed) whatever
        // the seed happened to build. Real snapshots carry no world state the
        // pass could not also see here — the pass reads only `inst`.
        const int BW = 16, BH = 10;
        int best = -1, bc0 = 0, br0 = 0;
        // Temperate rows, vegetated land preferred: the forms are judged on the
        // ground most of the game is played on, not on polar ice.
        for (int r0 = src.gh / 4; r0 + BH < src.gh * 3 / 4; ++r0)
            for (int c0 = 0; c0 < src.gw; c0 += 2)
            {
                int land = 0;
                for (int r = r0; r < r0 + BH; ++r)
                    for (int c = c0; c < c0 + BW; ++c)
                    {
                        const std::size_t ti = static_cast<std::size_t>(r) * src.gw + (c % src.gw);
                        if (src.cls[ti] != static_cast<std::uint8_t>(bake_source::tile_class::land))
                            continue;
                        const std::uint32_t col = src.colour[ti];
                        land += 2 + (ui::palette::col_g(col) > ui::palette::col_b(col) + 10 ? 1 : 0);
                    }
                if (land > best) { best = land; bc0 = c0; br0 = r0; }
            }
        std::printf("gallery block at col %d row %d (%d/%d land)\n", bc0, br0, best, BW * BH);

        struct staged { stamp_key k[3]; int n; stamp_key settle; };
        std::vector<staged> roster;
        const auto bld = [](building_type t, int fam) {
            stamp_key k; k.subject = stamp_subject::building;
            k.type = static_cast<std::uint8_t>(t); k.family = static_cast<std::uint8_t>(fam);
            return k;
        };
        for (int f = 0; f < static_cast<int>(extraction_family::count); ++f)
            roster.push_back({ { bld(building_type::extraction_site, f) }, 1, {} });
        for (int f = 0; f < static_cast<int>(processing_family::count); ++f)
            roster.push_back({ { bld(building_type::processing_facility, f) }, 1, {} });
        for (building_type t : { building_type::port, building_type::launchpad,
                                 building_type::inland_logistics_hub, building_type::military_base,
                                 building_type::research_institute, building_type::schooling,
                                 building_type::university })
            roster.push_back({ { bld(t, 0) }, 1, {} });
        {
            stamp_key sc = bld(building_type::processing_facility,
                               static_cast<int>(processing_family::metal_foundry));
            sc.subject = stamp_subject::scaffold;
            roster.push_back({ { sc }, 1, {} });
        }
        for (int sc = 1; sc <= 5; ++sc)
        {
            stamp_key k; k.subject = stamp_subject::settlement; k.scale = static_cast<std::uint8_t>(sc);
            roster.push_back({ {}, 0, k });
        }
        {
            stamp_key k; k.subject = stamp_subject::ruin; k.scale = 1;
            roster.push_back({ {}, 0, k });
        }
        // The stacked tile: three stacks, dominant mine in front.
        roster.push_back({ { bld(building_type::extraction_site, static_cast<int>(extraction_family::mine)),
                             bld(building_type::processing_facility, static_cast<int>(processing_family::metal_foundry)),
                             bld(building_type::inland_logistics_hub, 0) }, 3, {} });
        roster.push_back({ { bld(building_type::extraction_site, static_cast<int>(extraction_family::farm)),
                             bld(building_type::processing_facility, static_cast<int>(processing_family::food_processing)) }, 2, {} });
        {
            stamp_key k; k.subject = stamp_subject::settlement; k.scale = 3;
            roster.push_back({ { bld(building_type::schooling, 0) }, 1, k });
        }

        bake_source gal = src;
        gal.inst.of_tile.assign(gal.cls.size(), -1);
        gal.inst.list.clear();
        std::size_t next = 0;
        std::vector<std::size_t> gal_tiles;
        for (int r = br0; r < br0 + BH && next < roster.size(); r += 2)
            for (int c = bc0; c < bc0 + BW && next < roster.size(); c += 2)
            {
                const std::size_t i = static_cast<std::size_t>(r) * gal.gw + (c % gal.gw);
                if (gal.cls[i] != static_cast<std::uint8_t>(bake_source::tile_class::land))
                    continue;
                tile_installation ti;
                const staged& st = roster[next++];
                for (int j = 0; j < st.n; ++j) ti.stacks[j] = st.k[j];
                ti.n_stacks = static_cast<std::uint8_t>(st.n);
                ti.settlement = st.settle;
                gal.inst.of_tile[i] = static_cast<std::int32_t>(gal.inst.list.size());
                gal.inst.list.push_back(ti);
                gal_tiles.push_back(i);
            }
        std::printf("gallery staged %zu of %zu forms\n", next, roster.size());

        // P13 — determinism and wrap at flat, oblique and far geometry, aimed
        // at the stacked tile (the busiest one).
        // Aim at the three-stack tile (staged third from the end of the roster).
        const std::size_t aim_i = gal_tiles[gal_tiles.size() >= 3 ? gal_tiles.size() - 3 : 0];
        const int ar = static_cast<int>(aim_i / gal.gw), ac = static_cast<int>(aim_i % gal.gw);
        const double axc = 1.7320508075688772 * (ac + ((ar & 1) ? 0.5 : 0.0));
        const double ayc = 1.5 * ar;
        struct gcase { const char* name; double ppr; double sy; };
        for (const gcase& gc : { gcase{ "flat 48", 48.0, 1.0 }, gcase{ "oblique 45 @48", 48.0, 0.70710678 },
                                 gcase{ "far page 6", 6.0, 1.0 }, gcase{ "flat 24", 24.0, 1.0 } })
        {
            const geometry gg = make_geometry(gal.gw, gal.gh, gc.ppr, gc.sy);
            const int side = gc.ppr < 10 ? 64 : 160;
            const int px0 = static_cast<int>(axc * gg.s) - side / 2;
            const int py0 = std::clamp(static_cast<int>((ayc - gg.y_min) * gg.s) - side / 2,
                                       0, std::max(0, gg.H - side));
            std::vector<std::uint32_t> a1(static_cast<std::size_t>(side) * side), a2(a1.size()),
                a3(a1.size()), off(a1.size());
            bake_region(gal, gg, p, px0, py0, side, side, a1.data());
            bake_region(gal, gg, p, px0, py0, side, side, a2.data());
            bake_region(gal, gg, p, px0 + gg.W, py0, side, side, a3.data());
            bake_params po = p; po.installations = false;
            bake_region(gal, gg, po, px0, py0, side, side, off.data());
            char what[128];
            std::snprintf(what, sizeof what, "%s: structures bake byte-identical twice", gc.name);
            check(a1 == a2, "P13", what);
            std::snprintf(what, sizeof what, "%s: structures wrap byte-identical one period east", gc.name);
            check(a1 == a3, "P13", what);
            std::snprintf(what, sizeof what, "%s: the pass draws (on differs from off)", gc.name);
            check(a1 != off, "P13", what);
        }

        // P13 — mask: a body whose survey hides its ground records no structure.
        for (const auto& [id, bd] : w.bodies)
            if (bd.grid_width > 0 && bd.survey.phase == survey_phase::hidden)
            {
                const bake_source hsv = prepare_source(w, id);
                check(hsv.inst.list.empty(), "P13",
                      "an unsurveyed body snapshots no installation (nothing leaks the mask)");
                break;
            }

        // P14 — the hash folds installations, and only within reach.
        {
            const geometry gg = make_geometry(gal.gw, gal.gh, 48.0);
            const int side = 512;
            const int px0 = static_cast<int>(axc * gg.s) - side / 2;
            const int py0 = std::clamp(static_cast<int>((ayc - gg.y_min) * gg.s) - side / 2,
                                       0, std::max(0, gg.H - side));
            const std::uint64_t h0 = region_hash(gal, gg, px0, py0, side, side);
            const auto moved = [&](auto&& mutate) {
                bake_source m = gal;
                mutate(m);
                return region_hash(m, gg, px0, py0, side, side) != h0;
            };
            tile_installation& base = gal.inst.list[static_cast<std::size_t>(gal.inst.of_tile[aim_i])];
            (void)base;
            check(moved([&](bake_source& m) {
                      auto& t = m.inst.list[static_cast<std::size_t>(m.inst.of_tile[aim_i])];
                      if (t.n_stacks < 3) { t.stacks[t.n_stacks] = bld(building_type::port, 0); ++t.n_stacks; }
                      else t.n_stacks = 2; }),
                  "P14", "adding / removing a stack moves the chunk hash");
            check(moved([&](bake_source& m) {
                      auto& t = m.inst.list[static_cast<std::size_t>(m.inst.of_tile[aim_i])];
                      t.stacks[0].family ^= 1; }),
                  "P14", "a family (recipe group / target) change moves the chunk hash");
            check(moved([&](bake_source& m) {
                      auto& t = m.inst.list[static_cast<std::size_t>(m.inst.of_tile[aim_i])];
                      t.stacks[0].subject = stamp_subject::scaffold; }),
                  "P14", "construction start / completion moves the chunk hash");
            check(moved([&](bake_source& m) {
                      auto& t = m.inst.list[static_cast<std::size_t>(m.inst.of_tile[aim_i])];
                      t.settlement.subject = stamp_subject::settlement; t.settlement.scale = 4; }),
                  "P14", "a settlement scale step moves the chunk hash");
            check(moved([&](bake_source& m) {
                      auto& t = m.inst.list[static_cast<std::size_t>(m.inst.of_tile[aim_i])];
                      t.settlement.subject = stamp_subject::ruin; }),
                  "P14", "a raze moves the chunk hash");
            // A build half a body away: outside the window's reach.
            const int far_r = std::clamp(ar + gal.gh / 2, 0, gal.gh - 1) == ar
                                  ? std::max(0, ar - gal.gh / 2) : std::clamp(ar + gal.gh / 2, 0, gal.gh - 1);
            const std::size_t far_i = static_cast<std::size_t>(far_r) * gal.gw
                                    + static_cast<std::size_t>((ac + gal.gw / 2) % gal.gw);
            check(!moved([&](bake_source& m) {
                      if (m.inst.of_tile[far_i] < 0)
                      {
                          m.inst.of_tile[far_i] = static_cast<std::int32_t>(m.inst.list.size());
                          m.inst.list.emplace_back();
                      }
                      auto& t = m.inst.list[static_cast<std::size_t>(m.inst.of_tile[far_i])];
                      t.stacks[0] = bld(building_type::launchpad, 0); t.n_stacks = 1; }),
                  "P14", "a build outside the window's reach leaves the chunk hash alone");
        }

        // Bake time per tier, the pass off vs on, over real content: up to six
        // 512 px chunks spread over the body (the far page whole). Not a check
        // — the before/after record the requirement asks for.
        {
            struct tier { const char* name; double ppr; double sy; };
            for (const tier& t : { tier{ "far 6 (whole page)", 6.0, 1.0 }, tier{ "12", 12.0, 1.0 },
                                   tier{ "24", 24.0, 1.0 }, tier{ "48", 48.0, 1.0 },
                                   tier{ "96", 96.0, 1.0 }, tier{ "48 @22.5", 48.0, 0.92387953 },
                                   tier{ "96 @45", 96.0, 0.70710678 } })
            {
                const geometry gg = make_geometry(src.gw, src.gh, t.ppr, t.sy);
                double ms[2] = { 0, 0 };
                int chunks = 0;
                for (int pass = 0; pass < 2; ++pass)
                {
                    bake_params bp = p;
                    bp.installations = pass == 1;
                    chunks = 0;
                    const auto t0 = std::chrono::steady_clock::now();
                    if (t.ppr < 10)
                    {
                        std::vector<std::uint32_t> buf(static_cast<std::size_t>(gg.W) * gg.H);
                        bake_region(src, gg, bp, 0, 0, gg.W, gg.H, buf.data());
                        chunks = 1;
                    }
                    else
                    {
                        std::vector<std::uint32_t> buf(512u * 512u);
                        const int nx = std::max(1, gg.W / 512), ny = std::max(1, gg.H / 512);
                        for (int k = 0; k < 6; ++k)
                        {
                            const int ci = (k * 7 + 1) % nx, cj = std::min(ny - 1, (k * 3 + ny / 3) % ny);
                            bake_region(src, gg, bp, ci * 512, cj * 512, 512, 512, buf.data());
                            ++chunks;
                        }
                    }
                    ms[pass] = std::chrono::duration<double, std::milli>(
                                   std::chrono::steady_clock::now() - t0).count() / chunks;
                }
                std::printf("bake ms/chunk  tier %-18s  off %8.1f   on %8.1f   (+%.0f%%)\n",
                            t.name, ms[0], ms[1], ms[0] > 0 ? 100.0 * (ms[1] - ms[0]) / ms[0] : 0.0);
            }
        }

        // The dense case: one 512 px chunk over the staged gallery, every form
        // in reach — the pass's worst case rather than the body's average.
        for (const double ppr : { 24.0, 48.0, 96.0 })
        {
            const geometry gg = make_geometry(gal.gw, gal.gh, ppr);
            const int px0 = static_cast<int>(1.7320508075688772 * (bc0 + 1) * gg.s);
            const int py0 = std::max(0, static_cast<int>((1.5 * br0 - gg.y_min) * gg.s));
            std::vector<std::uint32_t> buf(512u * 512u);
            double ms[2];
            for (int pass = 0; pass < 2; ++pass)
            {
                bake_params bp = p;
                bp.installations = pass == 1;
                const auto t0 = std::chrono::steady_clock::now();
                bake_region(gal, gg, bp, px0, py0, 512, 512, buf.data());
                ms[pass] = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - t0).count();
            }
            std::printf("bake ms/chunk  gallery (dense) %3.0f px  off %8.1f   on %8.1f\n", ppr, ms[0], ms[1]);
        }

        // Gallery previews at every tier: the whole staged block, flat tiers
        // 6 (upscaled x4 for the eye), 12, 24, 48; the 96 and the oblique
        // rungs on the block's first half, squashed by the camera factor.
        {
            const double gx0 = 1.7320508075688772 * (bc0 - 0.6);
            const double gy0 = 1.5 * br0 - 1.4;
            const double gx1 = 1.7320508075688772 * (bc0 + BW + 0.2);
            const double gy1 = 1.5 * (br0 + BH) + 0.4;
            struct shot { const char* name; double ppr; double sy; double frac; int up; double qx, qy; };
            for (const shot& sh : { shot{ "far6", 6.0, 1.0, 1.0, 4, 0, 0 }, shot{ "t12", 12.0, 1.0, 1.0, 2, 0, 0 },
                                    shot{ "t24", 24.0, 1.0, 1.0, 1, 0, 0 }, shot{ "t48", 48.0, 1.0, 1.0, 1, 0, 0 },
                                    shot{ "t96", 96.0, 1.0, 0.5, 1, 0, 0 },
                                    shot{ "t48_tilt22", 48.0, 0.92387953, 1.0, 1, 0, 0 },
                                    shot{ "t96_tilt45", 96.0, 0.70710678, 0.5, 1, 0, 0 },
                                    shot{ "t96_tilt45_q2", 96.0, 0.70710678, 0.5, 1, 0.5, 0 },
                                    shot{ "t96_tilt45_q3", 96.0, 0.70710678, 0.5, 1, 0, 0.5 },
                                    shot{ "t96_tilt45_q4", 96.0, 0.70710678, 0.5, 1, 0.5, 0.5 } })
            {
                const geometry gg = make_geometry(gal.gw, gal.gh, sh.ppr, sh.sy);
                const double xs = gx0 + (gx1 - gx0) * sh.qx, xe = xs + (gx1 - gx0) * sh.frac;
                const double ys = gy0 + (gy1 - gy0) * sh.qy - (sh.sy < 1.0 ? 1.2 : 0.0);
                const double ye = gy0 + (gy1 - gy0) * (sh.qy + sh.frac);
                const int px0 = static_cast<int>(xs * gg.s);
                const int py0 = std::max(0, static_cast<int>((ys - gg.y_min) * gg.s));
                const int PW = static_cast<int>((xe - xs) * gg.s);
                const int PH = std::min(gg.H - py0, static_cast<int>((ye - ys) * gg.s));
                std::vector<std::uint32_t> buf(static_cast<std::size_t>(PW) * PH);
                bake_region(gal, gg, p, px0, py0, PW, PH, buf.data());
                // Camera squash (nearest row) for the oblique rungs.
                const int OH = static_cast<int>(PH * sh.sy);
                std::vector<std::uint32_t> sq(static_cast<std::size_t>(PW) * OH);
                for (int y = 0; y < OH; ++y)
                {
                    const int syr = std::min(PH - 1, static_cast<int>(y / sh.sy));
                    std::memcpy(sq.data() + static_cast<std::size_t>(y) * PW,
                                buf.data() + static_cast<std::size_t>(syr) * PW,
                                static_cast<std::size_t>(PW) * 4u);
                }
                const int U = sh.up;
                std::vector<std::uint32_t> up(static_cast<std::size_t>(PW) * U * OH * U);
                for (int y = 0; y < OH * U; ++y)
                    for (int x = 0; x < PW * U; ++x)
                        up[static_cast<std::size_t>(y) * PW * U + x] =
                            sq[static_cast<std::size_t>(y / U) * PW + x / U];
                char path[128];
                std::snprintf(path, sizeof path, "structures_gallery_%s.png", sh.name);
                write_png_rgba(path, PW * U, OH * U,
                               reinterpret_cast<const unsigned char*>(up.data()), PW * U * 4);
                std::printf("preview: %s (%dx%d)\n", path, PW * U, OH * U);
            }
        }
    }


    // ------------------------------------------------------------------
    // Look previews (not a check): bake one interesting window under several
    // parameter variants and write PNGs, so a C-F tuning pass costs ONE world
    // generation instead of one per dial change. Inspected by eye against
    // docs/ui/design/renders/map/it3 panel C-F.
    // ------------------------------------------------------------------
    {
        // Aim at a coastline with relief: a land tile with water within 6 tiles
        // and a nonzero landform bias within 4.
        int aim = land_i;
        for (int i = 0; i < static_cast<int>(src.cls.size()); ++i)
        {
            if (src.cls[i] != static_cast<std::uint8_t>(bake_source::tile_class::land))
                continue;
            const int r = i / src.gw, c = i % src.gw;
            bool near_water = false, near_relief = false;
            for (int dr = -6; dr <= 6 && !(near_water && near_relief); ++dr)
                for (int dc = -6; dc <= 6; ++dc)
                {
                    const int rr = r + dr;
                    if (rr < 0 || rr >= src.gh)
                        continue;
                    const int cc = ((c + dc) % src.gw + src.gw) % src.gw;
                    const std::size_t j = static_cast<std::size_t>(rr) * src.gw + cc;
                    if (src.cls[j] == static_cast<std::uint8_t>(bake_source::tile_class::water))
                        near_water = true;
                    if (std::fabs(src.relief_bias[j]) > 0.05f && std::abs(dr) <= 4 && std::abs(dc) <= 4)
                        near_relief = true;
                }
            if (near_water && near_relief)
            {
                aim = i;
                break;
            }
        }
        const int vr = aim / src.gw, vc = aim % src.gw;
        const double vx = 1.7320508075688772 * (vc + ((vr & 1) ? 0.5 : 0.0));
        const double vy = 1.5 * vr;
        const int PW = 1024, PH = 640;
        const int px0 = static_cast<int>(vx * g.s) - PW / 2;
        const int py0 = std::clamp(static_cast<int>((vy - g.y_min) * g.s) - PH / 2,
                                   0, std::max(0, g.H - PH));

        struct variant { const char* name; bake_params bp; };
        std::vector<variant> vars;
        {
            bake_params v = p;                                  vars.push_back({ "v1_defaults", v }); }
        {
            bake_params v = p; v.warp_amp = 0.0f;               vars.push_back({ "v2_nowarp", v }); }
        {
            bake_params v = p; v.detail_amp = 0.50f;
            v.landform_accent = 3.2f;                           vars.push_back({ "v3_craggy", v }); }
        {
            bake_params v = p; v.relief_gain = 14.0f;
            v.altitude_gain = 0.42f;                            vars.push_back({ "v4_deep_relief", v }); }
        {
            bake_params v = p; v.grade_enabled = false;         vars.push_back({ "v5_ungraded", v }); }
        {
            bake_params v = p; v.warp_amp = 0.60f;
            v.detail_amp = 0.45f; v.relief_gain = 12.0f;        vars.push_back({ "v6_pushed", v }); }

        std::vector<std::uint32_t> buf(static_cast<std::size_t>(PW) * PH);
        for (const variant& v : vars)
        {
            bake_region(src, g, v.bp, px0, py0, PW, PH, buf.data());
            char path[128];
            std::snprintf(path, sizeof path, "ground_preview_%s.png", v.name);
            write_png_rgba(path, PW, PH,
                           reinterpret_cast<const unsigned char*>(buf.data()), PW * 4);
            std::printf("preview: %s\n", path);
        }

        // Close-tier preview: a forest window at 48 px/r — trees, ridges and
        // rock exposure at the resolution the closest zoom rungs actually see.
        {
            const geometry g48 = make_geometry(hb.grid_width, hb.grid_height, 48.0);
            int forest_i = land_i;
            for (int i = 0; i < static_cast<int>(src.cover.size()); ++i)
                if (src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::land)
                    && static_cast<terrain_cover>(src.cover[i]) == terrain_cover::forest)
                {
                    forest_i = i;
                    break;
                }
            const int fr = forest_i / src.gw, fc = forest_i % src.gw;
            const double fx = 1.7320508075688772 * (fc + ((fr & 1) ? 0.5 : 0.0));
            const double fy = 1.5 * fr;
            const int fpx0 = static_cast<int>(fx * g48.s) - PW / 2;
            const int fpy0 = std::clamp(static_cast<int>((fy - g48.y_min) * g48.s) - PH / 2,
                                        0, std::max(0, g48.H - PH));
            bake_region(src, g48, p, fpx0, fpy0, PW, PH, buf.data());
            write_png_rgba("ground_preview_v7_forest48.png", PW, PH,
                           reinterpret_cast<const unsigned char*>(buf.data()), PW * 4);
            std::printf("preview: ground_preview_v7_forest48.png\n");

            // The same window through the 45-degree oblique bake — standing
            // trees, height-displaced hills — squashed by the camera factor so
            // the PNG previews what the player will actually see.
            const geometry g45p = make_geometry(hb.grid_width, hb.grid_height, 48.0, 0.70710678);
            const int opy0 = std::clamp(
                static_cast<int>((fy - g45p.y_min) * g45p.s) - PH / 2,
                0, std::max(0, g45p.H - PH));
            bake_region(src, g45p, p, fpx0, opy0, PW, PH, buf.data());
            // Nearest-row squash to cos(45): sample source row py/0.7071.
            std::vector<std::uint32_t> sq(static_cast<std::size_t>(PW) * PH, 0u);
            for (int y = 0; y < PH; ++y)
            {
                const int syr = std::min(PH - 1,
                                         static_cast<int>(std::lround(y / 0.70710678)));
                if (syr < PH)
                    std::memcpy(sq.data() + static_cast<std::size_t>(y) * PW,
                                buf.data() + static_cast<std::size_t>(syr) * PW,
                                static_cast<std::size_t>(PW) * 4u);
            }
            write_png_rgba("ground_preview_v8_tilt45.png", PW, PH,
                           reinterpret_cast<const unsigned char*>(sq.data()), PW * 4);
            std::printf("preview: ground_preview_v8_tilt45.png\n");
        }
    }

    // ------------------------------------------------------------------
    // P15 / P16 — the landform relief and river passes (BL-1242): each moves
    // the bake where its feature stands, bakes byte-identical twice and one
    // wrap period east (flat 48 px and the 45-degree oblique tier), and leaves
    // ground with no feature near it untouched. Feature previews follow, at
    // the far page, 24 px and 96 px, for the eye.
    // ------------------------------------------------------------------
    {
        const auto land = static_cast<std::uint8_t>(bake_source::tile_class::land);
        const auto water = static_cast<std::uint8_t>(bake_source::tile_class::water);
        auto popcount6 = [](std::uint8_t v) { int c = 0; for (int b = 0; b < 6; ++b) c += (v >> b) & 1; return c; };
        // Aims: the most-linked tile of each linear landform, a crater, and
        // the highest-flow river mouth (a land tile flowing into water).
        int aim_lf[7] = { -1, -1, -1, -1, -1, -1, -1 };
        int best_links[7] = { -1, -1, -1, -1, -1, -1, -1 };
        int mouth = -1;
        float mouth_flow = 0.0f;
        // A river MOUTH is at the sea, not a lake: read substrates off the world.
        std::vector<std::uint8_t> is_sea(src.cls.size(), 0);
        for (const auto& [tid, t] : w.tiles)
            if (t.body == home && t.grid_x >= 0 && t.grid_x < src.gw && t.grid_y >= 0
                && t.grid_y < src.gh)
                is_sea[static_cast<std::size_t>(t.grid_y) * src.gw + t.grid_x] =
                    (t.substrate == terrain_substrate::ocean || t.substrate == terrain_substrate::coast)
                        ? 1 : 0;
        for (int i = 0; i < static_cast<int>(src.cls.size()); ++i)
        {
            if (src.cls[i] != land)
                continue;
            const int lf = src.landform[i];
            const int lk = popcount6(src.lf_links[i]);
            if (lf >= 0 && lf < 7 && lk > best_links[lf])
            {
                best_links[lf] = lk;
                aim_lf[lf] = i;
            }
            for (int s = 0; s < 6; ++s)
            {
                if (!(src.river_out[i] & (1u << s)))
                    continue;
                const hex_neighbors::coord nb = hex_neighbors::neighbour(i % src.gw, i / src.gw, s);
                if (nb.gy < 0 || nb.gy >= src.gh)
                    continue;
                const int j = nb.gy * src.gw + ((nb.gx % src.gw) + src.gw) % src.gw;
                if (src.cls[j] == water && is_sea[j] && src.river_flow[i] > mouth_flow)
                {
                    mouth_flow = src.river_flow[i];
                    mouth = i;
                }
            }
        }
        const int mtn = aim_lf[static_cast<int>(terrain_landform::mountain)];
        check(mtn >= 0 && best_links[static_cast<int>(terrain_landform::mountain)] >= 1,
              "P15", "found a bridged mountain run");
        check(mouth >= 0, "P16", "found a river mouth");
        std::printf("      mountain run at [%d,%d] (%d links); rift [%d]; canyon [%d]; crater [%d]; "
                    "mouth at [%d,%d] flow %.0f\n",
                    mtn % src.gw, mtn / src.gw,
                    best_links[static_cast<int>(terrain_landform::mountain)],
                    aim_lf[static_cast<int>(terrain_landform::rift)],
                    aim_lf[static_cast<int>(terrain_landform::canyon)],
                    aim_lf[static_cast<int>(terrain_landform::crater)],
                    mouth % src.gw, mouth / src.gw, mouth_flow);
        // Walk the mouth's main stem upstream (largest inflow each step) to its
        // source, so a capture script can frame the whole river.
        if (mouth >= 0)
        {
            int cur = mouth, steps = 0;
            for (;;)
            {
                int next = -1;
                float bf = 0.0f;
                for (int s = 0; s < 6; ++s)
                {
                    if (!(src.river_in[cur] & (1u << s)))
                        continue;
                    const hex_neighbors::coord nb = hex_neighbors::neighbour(cur % src.gw, cur / src.gw, s);
                    if (nb.gy < 0 || nb.gy >= src.gh)
                        continue;
                    const int j = nb.gy * src.gw + ((nb.gx % src.gw) + src.gw) % src.gw;
                    if (src.river_flow[j] > bf) { bf = src.river_flow[j]; next = j; }
                }
                if (next < 0 || ++steps > 4096)
                    break;
                cur = next;
            }
            std::printf("      main stem: source [%d,%d] -> mouth [%d,%d], %d tiles\n",
                        cur % src.gw, cur / src.gw, mouth % src.gw, mouth / src.gw, steps + 1);
        }
        for (int i = 0; i < static_cast<int>(src.cls.size()); ++i)
            if (src.cls[i] == land && src.landform[i] == static_cast<std::uint8_t>(terrain_landform::mountain)
                && src.lf_links[i] == 0 && i / src.gw > 20 && i / src.gw < src.gh - 20)
            {
                std::printf("      lone mountain at [%d,%d]\n", i % src.gw, i / src.gw);
                break;
            }
        for (int lf : { 2, 3, 5, 6 })
            if (aim_lf[lf] >= 0)
                std::printf("      landform %d best at [%d,%d] (%d links)\n", lf,
                            aim_lf[lf] % src.gw, aim_lf[lf] / src.gw, best_links[lf]);

        const auto window = [&](int tile_idx, const geometry& gg, const bake_params& bp,
                                int side, int dpx, std::vector<std::uint32_t>& out)
        {
            const int r = tile_idx / src.gw, c = tile_idx % src.gw;
            const double cx = 1.7320508075688772 * (c + ((r & 1) ? 0.5 : 0.0));
            const double cy = 1.5 * r - src.height[tile_idx] * gg.lift;
            const int x0 = static_cast<int>(cx * gg.s) - side / 2 + dpx;
            const int y0 = std::clamp(static_cast<int>((cy - gg.y_min) * gg.s) - side / 2,
                                      0, std::max(0, gg.H - side));
            out.assign(static_cast<std::size_t>(side) * side, 0u);
            bake_region(src, gg, bp, x0, y0, side, side, out.data());
        };
        const geometry g48f = make_geometry(hb.grid_width, hb.grid_height, 48.0);
        const geometry g45f = make_geometry(hb.grid_width, hb.grid_height, 96.0, 0.70710678);
        bake_params no_lf = p;  no_lf.landform_strength = 0.0f;
        bake_params no_rv = p;  no_rv.river_strength = 0.0f;
        std::vector<std::uint32_t> a1, a2, a3, a4;
        if (mtn >= 0)
        {
            window(mtn, g48f, p, 160, 0, a1);
            window(mtn, g48f, p, 160, 0, a2);
            window(mtn, g48f, p, 160, g48f.W, a3);
            window(mtn, g48f, no_lf, 160, 0, a4);
            check(a1 == a2, "P15", "a mountain run bakes byte-identical twice");
            check(a1 == a3, "P15", "a mountain run wraps byte-identical one period east");
            check(a1 != a4, "P15", "the landform pass moves a mountain run");
            window(mtn, g45f, p, 160, 0, a1);
            window(mtn, g45f, p, 160, g45f.W, a3);
            check(a1 == a3, "P15", "the oblique mountain run wraps byte-identical");
        }
        if (mouth >= 0)
        {
            window(mouth, g48f, p, 160, 0, a1);
            window(mouth, g48f, p, 160, 0, a2);
            window(mouth, g48f, p, 160, g48f.W, a3);
            window(mouth, g48f, no_rv, 160, 0, a4);
            check(a1 == a2, "P16", "a river mouth bakes byte-identical twice");
            check(a1 == a3, "P16", "a river mouth wraps byte-identical one period east");
            check(a1 != a4, "P16", "the river pass moves a river mouth");
            window(mouth, g45f, p, 160, 0, a1);
            window(mouth, g45f, p, 160, g45f.W, a3);
            check(a1 == a3, "P16", "the oblique river mouth wraps byte-identical");
        }
        // Featureless ground stays byte-identical with both passes off.
        {
            int quiet = -1;
            for (int i = 0; i < static_cast<int>(src.cls.size()) && quiet < 0; ++i)
            {
                if (src.cls[i] != land || src.near_feature[i])
                    continue;
                bool ok = true;
                const int r = i / src.gw, c = i % src.gw;
                for (int dr = -3; dr <= 3 && ok; ++dr)
                    for (int dc = -3; dc <= 3 && ok; ++dc)
                    {
                        const int rr = r + dr;
                        if (rr < 0 || rr >= src.gh) { ok = false; break; }
                        const int cc = ((c + dc) % src.gw + src.gw) % src.gw;
                        if (src.near_feature[static_cast<std::size_t>(rr) * src.gw + cc])
                            ok = false;
                    }
                if (ok)
                    quiet = i;
            }
            if (quiet >= 0)
            {
                bake_params off = p; off.landform_strength = 0.0f; off.river_strength = 0.0f;
                window(quiet, g48f, p, 96, 0, a1);
                window(quiet, g48f, off, 96, 0, a2);
                check(a1 == a2, "P15", "ground with no feature near it is untouched by both passes");
            }
        }

        // Previews.
        struct shot { const char* name; int tile; };
        const shot shots[] = {
            { "mountain", mtn },
            { "rift",   aim_lf[static_cast<int>(terrain_landform::rift)] },
            { "canyon", aim_lf[static_cast<int>(terrain_landform::canyon)] },
            { "crater", aim_lf[static_cast<int>(terrain_landform::crater)] },
            { "mouth",  mouth },
        };
        const double tiers[] = { 6.0, 24.0, 96.0 };
        std::vector<std::uint32_t> pb;
        for (const shot& sh : shots)
        {
            if (sh.tile < 0)
                continue;
            for (double tp : tiers)
            {
                const geometry gp = make_geometry(hb.grid_width, hb.grid_height, tp);
                const int side = tp < 10.0 ? 160 : 512;
                window(sh.tile, gp, p, side, 0, pb);
                char path[128];
                std::snprintf(path, sizeof path, "feature_%s_%d.png", sh.name, static_cast<int>(tp));
                write_png_rgba(path, side, side, reinterpret_cast<const unsigned char*>(pb.data()),
                               side * 4);
                // The same window with both passes off: the A/B the eye needs.
                bake_params off = p; off.landform_strength = 0.0f; off.river_strength = 0.0f;
                window(sh.tile, gp, off, side, 0, pb);
                std::snprintf(path, sizeof path, "feature_%s_%d_off.png", sh.name, static_cast<int>(tp));
                write_png_rgba(path, side, side, reinterpret_cast<const unsigned char*>(pb.data()),
                               side * 4);
            }
        }
    }

    // ------------------------------------------------------------------
    // Bake time per tier (BL-1242: recorded before and after the landform
    // and river passes). Not a check — a measurement. One 512 px chunk per
    // tier aimed at the body's strongest-relief tile, the two oblique tiers,
    // and the far page whole. Debug builds time far slower than Release;
    // compare like with like.
    // ------------------------------------------------------------------
    {
        int aim = land_i;
        float best = -1.0f;
        for (int i = 0; i < static_cast<int>(src.relief_bias.size()); ++i)
            if (src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::land)
                && src.relief_bias[i] > best)
            {
                best = src.relief_bias[i];
                aim  = i;
            }
        const int ar = aim / src.gw, ac = aim % src.gw;
        const double ax = 1.7320508075688772 * (ac + ((ar & 1) ? 0.5 : 0.0));
        const double ay = 1.5 * ar;
        struct tier { double px; double sy; const char* name; };
        const tier tiers[] = { { 12.0, 1.0, "12" }, { 24.0, 1.0, "24" }, { 48.0, 1.0, "48" },
                               { 96.0, 0.92387953, "96@22.5" }, { 192.0, 0.70710678, "192@45" } };
        std::vector<std::uint32_t> tb(512u * 512u);
        // Each row times the default bake AND the same window with the BL-1242
        // passes off, in one process, so load on the machine cancels out of the
        // comparison. Best of three each: Debug timing is noisy.
        bake_params feat_off = p;
        feat_off.landform_strength = 0.0f;
        feat_off.river_strength    = 0.0f;
        const auto best_of3 = [&](const geometry& gg, const bake_params& bp, int x0, int y0,
                                  int w_, int h_, std::uint32_t* buf) -> double
        {
            double best = 1e30;
            for (int rep = 0; rep < 3; ++rep)
            {
                const auto t0 = std::chrono::steady_clock::now();
                bake_region(src, gg, bp, x0, y0, w_, h_, buf);
                const auto t1 = std::chrono::steady_clock::now();
                best = std::min(best, std::chrono::duration<double, std::milli>(t1 - t0).count());
            }
            return best;
        };
        for (const tier& t : tiers)
        {
            const geometry gt = make_geometry(hb.grid_width, hb.grid_height, t.px, t.sy);
            const int side = 512;
            const int tpx0 = static_cast<int>(ax * gt.s) - side / 2;
            const int tpy0 = std::clamp(static_cast<int>((ay - gt.y_min) * gt.s) - side / 2,
                                        0, std::max(0, gt.H - side));
            const double on  = best_of3(gt, p, tpx0, tpy0, side, side, tb.data());
            const double off = best_of3(gt, feat_off, tpx0, tpy0, side, side, tb.data());
            std::printf("TIME  tier %-8s 512x512 chunk: %8.1f ms (features off %8.1f ms)\n",
                        t.name, on, off);
        }
        {
            const geometry g6 = make_geometry(hb.grid_width, hb.grid_height, 6.0);
            std::vector<std::uint32_t> fb(static_cast<std::size_t>(g6.W) * g6.H);
            const double on  = best_of3(g6, p, 0, 0, g6.W, g6.H, fb.data());
            const double off = best_of3(g6, feat_off, 0, 0, g6.W, g6.H, fb.data());
            std::printf("TIME  far page %dx%d: %8.1f ms (features off %8.1f ms)\n",
                        g6.W, g6.H, on, off);
        }
    }

    std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "ALL PASS",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
