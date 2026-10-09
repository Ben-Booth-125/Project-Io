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
//   P10 (retired with the per-rung tiers, BL-1246; see P22) The tier chooser: smallest tier with ppr >= the DRAWN radius, far
//       page at or below 1:1, no radius magnified up to the 192 px top tier,
//       no chunked tier minified past 2:1, and the canvas's border-inset
//       radius shown to be the wrong feed (F34).
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
//   P17-P20 Terrain variant families (BL-1243): the colouring (no shared
//       neighbour, mask-blind), purity/wrap/seam of the variant pass on a
//       plain, a forest and a mountain run, the plain's tile-to-tile spread,
//       and the hash folding family and variant. `--aim` prints the capture
//       aims for scripts/verify/terrain_variants.lua and exits; `--variants`
//       runs P17-P20 alone; `--timing` the variant cost reading alone;
//       `--seam` triages the P11 seam window pass by pass.
//   P21 The worker pool (Ben, 2026-10-09): a set of chunk windows across
//       tiers (flat, 2x supersampled, oblique) baked CONCURRENTLY on the
//       pool's thread count, sharing one source read-only, is byte-identical
//       to the same set baked serially. `--pool` runs P21 alone.
//   P22 The one master (BL-1246, RENDERING.md § Level of detail): the master
//       geometry is aligned (W, H multiples of 16, every level whole pixels
//       and wrap-exact), a master chunk equals its quadrant of a wider window,
//       the chain built chunk by chunk equals the whole image's chain, and the
//       level chooser reads the coarsest level at or above the drawn radius.
//       `--master` runs P22 plus the whole-body master bake reading (1x, 2x).
//   P23 The lock fast path (BL-1246): a window wholly inside survey-masked
//       ground fills with the lock colour directly, byte-identical to the
//       full per-pixel bake (masked chunks and a mask-edge chunk).
//   P24 The partial re-bake (BL-1246): windows the rule draws around
//       installations, re-baked and blitted over a master chunk, are
//       window-invariant (the whole bake is unchanged) and turn the chunk
//       baked before an installation change into the chunk baked after, byte
//       for byte — real installations, a structure straddling a chunk corner,
//       one on the wrap seam; a terrain change or a dense change re-bakes
//       whole. P25 the windows' own mip pieces equal the whole chain.
//       `--patch` runs P24/P25 alone.
//   P26 Tiles hold their own ground (BL-1251, RENDERING.md § Tiles hold their
//       own ground): a tile's centre region bakes byte-identical whatever its
//       neighbours' ground (colour, family, variant, border sets) is — and the
//       pre-BL-1251 wide blend fails the same comparison.
//   P27 Every border set on the home body, and the shore shelf, is pure,
//       wrap-exact, seamless across a chunk edge at the master geometry, and
//       draws. `--border` runs P26/P27 alone and writes bl1251_*.png previews
//       (legacy look vs now) of each edge and of the wide plain, forest,
//       scrub and bare ground.
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
#include <atomic>
#include <thread>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef near // windef.h's 16-bit relics collide with locals
#undef far
#endif

using namespace ui::ground;

namespace {

int g_failures = 0;

/// This thread's CPU cycles in millions (QueryThreadCycleTime: high resolution,
/// and blind to time the thread spent descheduled), or -1 where the platform
/// does not say. A relative reading: compare configurations, not machines.
double thread_cpu_ms()
{
#ifdef _WIN32
    ULONG64 cyc = 0;
    if (QueryThreadCycleTime(GetCurrentThread(), &cyc))
        return static_cast<double>(cyc) / 1e6;
#endif
    return -1.0;
}

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

/// The tile at the heart of the widest run of one cover (BL-1243): the land tile
/// whose radius-3 neighbourhood (37 tiles, columns wrapping) holds the most
/// land tiles of @p cover on plain ground with no dramatic landform or river
/// near — a "wide plain" or "deep forest" to aim a variant window at.
/// Temperate rows only. Returns -1 when the body has none.
int homogeneous_aim(const bake_source& src, terrain_cover cover, int* score_out = nullptr)
{
    const auto land = static_cast<std::uint8_t>(bake_source::tile_class::land);
    const auto cv = static_cast<std::uint8_t>(cover);
    int best = -1, best_score = 0;
    for (int r = src.gh / 5 + 3; r < src.gh * 4 / 5 - 3; ++r)
        for (int c = 0; c < src.gw; ++c)
        {
            const std::size_t i = static_cast<std::size_t>(r) * src.gw + c;
            if (src.cls[i] != land || src.cover[i] != cv || src.near_feature[i])
                continue;
            int score = 0;
            for (int dr = -3; dr <= 3; ++dr)
                for (int dc = -3; dc <= 3; ++dc)
                {
                    if (std::abs(dr) + std::abs(dc) > 4)
                        continue; // a rough hex disc
                    const int rr = r + dr;
                    const int cc = ((c + dc) % src.gw + src.gw) % src.gw;
                    const std::size_t j = static_cast<std::size_t>(rr) * src.gw + cc;
                    if (src.cls[j] == land && src.cover[j] == cv && !src.near_feature[j])
                        ++score;
                }
            if (score > best_score)
            {
                best_score = score;
                best = static_cast<int>(i);
            }
        }
    if (score_out)
        *score_out = best_score;
    return best;
}

/// P21 - the worker pool's correctness row: chunk windows over several
/// geometries, aimed at land, a forest, a landform/river and an installation,
/// baked serially and then concurrently (threads pulling from a shared index,
/// as the ground_layer pool does), compared byte for byte. Two concurrent
/// rounds, in opposite job orders, so the interleaving differs.
void pool_row(const bake_source& src, const body_component& hb, int land_i, const bake_params& p)
{
    std::vector<int> aims = { land_i };
    if (const int f = homogeneous_aim(src, terrain_cover::forest); f >= 0)
        aims.push_back(f);
    for (std::size_t i = 0; i < src.near_feature.size(); ++i)
        if (src.near_feature[i]) { aims.push_back(static_cast<int>(i)); break; }
    for (std::size_t i = 0; i < src.inst.of_tile.size(); ++i)
        if (src.inst.of_tile[i] >= 0) { aims.push_back(static_cast<int>(i)); break; }

    struct win { geometry g; bake_params prm; int x0, y0, w, h; };
    std::vector<win> jobs;
    struct tier { double ppr; double sy; int ss; };
    for (const tier t : { tier{ 24.0, 1.0, 2 }, tier{ 48.0, 1.0, 2 }, tier{ 48.0, k_tilt_sy, 2 },
                          tier{ 96.0, 1.0, 1 } })
    {
        const geometry g = make_geometry(hb.grid_width, hb.grid_height, t.ppr, t.sy);
        bake_params prm = p;
        prm.supersample = t.ss;
        for (const int a : aims)
        {
            const int ar = a / src.gw, ac = a % src.gw;
            const double ax = 1.7320508075688772 * (ac + ((ar & 1) ? 0.5 : 0.0));
            const int side = 160;
            // Two abutting windows per aim: the pool bakes neighbours at once.
            for (int k = 0; k < 2; ++k)
            {
                const int h = std::min(side, g.H);
                win wv{ g, prm, static_cast<int>(ax * g.s) - side + k * side,
                        std::clamp(static_cast<int>((1.5 * ar - g.y_min) * g.s) - side / 2,
                                   0, std::max(0, g.H - h)),
                        side, h };
                jobs.push_back(wv);
            }
        }
    }

    const auto bake = [&](const win& j, std::vector<std::uint32_t>& out) {
        out.assign(static_cast<std::size_t>(j.w) * j.h, 0u);
        bake_region(src, j.g, j.prm, j.x0, j.y0, j.w, j.h, out.data());
    };
    std::vector<std::vector<std::uint32_t>> serial(jobs.size());
    for (std::size_t i = 0; i < jobs.size(); ++i)
        bake(jobs[i], serial[i]);

    const int hc = static_cast<int>(std::thread::hardware_concurrency());
    const int n_threads = std::max(2, hc - 2); // the pool's count, at least two
    bool identical = true;
    for (int round = 0; round < 2; ++round)
    {
        std::vector<std::vector<std::uint32_t>> par(jobs.size());
        std::atomic<std::size_t> next{ 0 };
        std::vector<std::thread> pool;
        for (int t = 0; t < n_threads; ++t)
            pool.emplace_back([&] {
                for (std::size_t k; (k = next.fetch_add(1)) < jobs.size();)
                {
                    const std::size_t i = round == 0 ? k : jobs.size() - 1 - k;
                    bake(jobs[i], par[i]);
                }
            });
        for (std::thread& t : pool)
            t.join();
        for (std::size_t i = 0; i < jobs.size(); ++i)
            identical = identical && par[i] == serial[i];
    }
    std::printf("P21: %zu windows over %zu aims, %d threads\n", jobs.size(), aims.size(), n_threads);
    check(identical, "P21", "concurrent bakes on the pool's thread count equal the serial bakes byte for byte");
}

/// The whole-body master bake on the pool's thread count (RENDERING.md
/// § Level of detail): every 512 px chunk of the master at @p ss, then its
/// mip pieces. Returns the bake wall ms; prints the reading.
double master_bake(const bake_source& src, const bake_params& p, int ss, const char* tag)
{
    const geometry g = make_master_geometry(src.gw, src.gh);
    bake_params prm = p;
    prm.supersample = ss;
    const int cw = (g.W + k_chunk_px - 1) / k_chunk_px;
    const int ch = (g.H + k_chunk_px - 1) / k_chunk_px;
    const int n  = cw * ch;
    const int hc = static_cast<int>(std::thread::hardware_concurrency());
    const int n_threads = std::max(1, hc - 2);
    std::atomic<int> next{ 0 };
    std::atomic<long long> chain_us{ 0 };
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int t = 0; t < n_threads; ++t)
        pool.emplace_back([&] {
            std::vector<std::uint32_t> buf, half, cur;
            for (int k; (k = next.fetch_add(1)) < n;)
            {
                const int ci = k % cw, cj = k / cw;
                const int x0 = ci * k_chunk_px, y0 = cj * k_chunk_px;
                const int w = std::min(k_chunk_px, g.W - x0), h = std::min(k_chunk_px, g.H - y0);
                buf.assign(static_cast<std::size_t>(w) * h, 0u);
                bake_region(src, g, prm, x0, y0, w, h, buf.data());
                const auto c0 = std::chrono::steady_clock::now();
                int lw = w, lh = h;
                cur = buf;
                for (int l = 1; l < k_level_count; ++l)
                {
                    half.assign(static_cast<std::size_t>(lw / 2) * (lh / 2), 0u);
                    downsample_half(cur.data(), lw, lh, half.data());
                    cur.swap(half);
                    lw /= 2; lh /= 2;
                }
                chain_us += std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - c0).count();
            }
        });
    for (std::thread& t : pool)
        t.join();
    const double ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    double bytes = 0.0;
    for (int l = 0; l < k_level_count; ++l)
    {
        const geometry gl = level_geometry(g, l);
        bytes += static_cast<double>(gl.W) * gl.H * 4.0;
    }
    std::printf("MASTER  %-10s %dx%d grid  master %dx%d px (%d chunks)  %dx  %d threads  bake %.1f s"
                "  (chain %.2f s of thread time)  RAM master+chain %.2f GB\n",
                tag, src.gw, src.gh, g.W, g.H, n, ss, n_threads, ms / 1000.0,
                chain_us.load() / 1e6, bytes / (1024.0 * 1024.0 * 1024.0));
    std::fflush(stdout);
    return ms;
}

/// P22 - the one master and its chain (RENDERING.md § Level of detail):
/// the master geometry is aligned so every level is whole pixels and keeps
/// the wrap; the chain built chunk by chunk equals the chain of the whole
/// image; and the level chooser reads the coarsest level at or above the
/// drawn radius (never past 2:1 minified; the master magnified past 96).
void master_row(const bake_source& src, const body_component& hb, int land_i, const bake_params& p)
{
    const geometry m = make_master_geometry(hb.grid_width, hb.grid_height);
    bool aligned = m.W % k_master_align == 0 && m.H % k_master_align == 0
                && std::fabs(m.tilt_sy - k_tilt_sy) < 1e-9 && m.lift > 0.3 && m.lift < 0.45;
    for (int l = 0; l < k_level_count; ++l)
    {
        const geometry gl = level_geometry(m, l);
        aligned = aligned && gl.W * (1 << l) == m.W && gl.H * (1 << l) == m.H
               && std::fabs(gl.s * (1 << l) - m.s) < 1e-9
               && std::fabs(gl.W / gl.s - m.W / m.s) < 1e-6; // same wrap period
    }
    std::printf("P22: master %dx%d at %.3f px/hex, tilt sy %.4f, lift %.3f\n", m.W, m.H, m.s,
                m.tilt_sy, m.lift);
    check(aligned, "P22", "master W/H are multiples of 16; each level halves exactly and keeps the wrap period");

    // Chunkwise vs whole: a 2x2-chunk master window (1024 px) aimed at land,
    // on the chunk lattice.
    const int lr = land_i / src.gw, lc = land_i % src.gw;
    const double lx = 1.7320508075688772 * (lc + ((lr & 1) ? 0.5 : 0.0));
    const int side = 2 * k_chunk_px;
    const int x0 = (static_cast<int>(lx * m.s) / k_chunk_px) * k_chunk_px;
    const int y0 = std::clamp((static_cast<int>((1.5 * lr - m.y_min) * m.s) / k_chunk_px) * k_chunk_px,
                              0, std::max(0, (m.H - side) / k_chunk_px * k_chunk_px));
    bake_params p1 = p;
    p1.supersample = 1;
    std::vector<std::uint32_t> whole(static_cast<std::size_t>(side) * side);
    bake_region(src, m, p1, x0, y0, side, side, whole.data());
    std::vector<std::uint32_t> wl = whole, tmp;
    int ws = side;
    for (int l = 1; l < k_level_count; ++l)
    {
        tmp.assign(static_cast<std::size_t>(ws / 2) * (ws / 2), 0u);
        downsample_half(wl.data(), ws, ws, tmp.data());
        wl.swap(tmp);
        ws /= 2;
    }
    std::vector<std::uint32_t> placed(static_cast<std::size_t>(ws) * ws, 0u);
    bool chunk_eq_bake = true;
    for (int q = 0; q < 4; ++q)
    {
        const int qx = (q & 1) * k_chunk_px, qy = (q >> 1) * k_chunk_px;
        std::vector<std::uint32_t> c(static_cast<std::size_t>(k_chunk_px) * k_chunk_px);
        bake_region(src, m, p1, x0 + qx, y0 + qy, k_chunk_px, k_chunk_px, c.data());
        for (int y = 0; y < k_chunk_px && chunk_eq_bake; ++y)
            chunk_eq_bake = std::memcmp(c.data() + static_cast<std::size_t>(y) * k_chunk_px,
                                        whole.data() + static_cast<std::size_t>(qy + y) * side + qx,
                                        k_chunk_px * 4u) == 0;
        int cs = k_chunk_px;
        for (int l = 1; l < k_level_count; ++l)
        {
            tmp.assign(static_cast<std::size_t>(cs / 2) * (cs / 2), 0u);
            downsample_half(c.data(), cs, cs, tmp.data());
            c.swap(tmp);
            cs /= 2;
        }
        for (int y = 0; y < cs; ++y)
            std::memcpy(placed.data() + static_cast<std::size_t>(qy / 16 + y) * ws + qx / 16,
                        c.data() + static_cast<std::size_t>(y) * cs, static_cast<std::size_t>(cs) * 4u);
    }
    check(chunk_eq_bake, "P22", "a master chunk bakes byte-identical to its quadrant of a 2x2-chunk window (no chunk seam)");
    check(placed == wl, "P22", "the mip chain built chunk by chunk equals the whole image's chain to the 6 px level");
    {
        std::vector<std::uint32_t> flat(16, 0xFF204060u), half(4);
        downsample_half(flat.data(), 4, 4, half.data());
        check(half == std::vector<std::uint32_t>(4, 0xFF204060u), "P22",
              "a flat run (lock fill, open sea) downsamples byte-exact");
    }

    // The chooser. Rung radii at 1720x1080 (drawn: 6.93 / 13.87 / 27.73 /
    // 55.47 / 110.66) and 3840x2160 (14.0 ... 223.41).
    const struct { double r; int want; } rows[] = {
        { 3.0, 4 }, { 5.93, 4 }, { 6.0, 4 }, { 6.01, 3 }, { 6.93, 3 }, { 12.0, 3 }, { 12.5, 2 },
        { 13.87, 2 }, { 14.0, 2 }, { 24.5, 1 }, { 27.73, 1 }, { 48.5, 0 }, { 55.47, 0 },
        { 96.0, 0 }, { 110.66, 0 }, { 223.41, 0 },
    };
    bool all = true;
    for (const auto& row : rows)
        if (const int got = choose_level(row.r); got != row.want)
        {
            std::printf("      draw_r %.2f -> level %d, want %d\n", row.r, got, row.want);
            all = false;
        }
    check(all, "P22", "chooser reads the coarsest level at or above the drawn radius");
    bool never_mag = true, within_2 = true;
    for (double r = k_level_ppr[k_level_count - 1] + 0.0625; r <= k_master_ppr; r += 0.0625)
    {
        const double ppr = k_level_ppr[choose_level(r)];
        if (ppr / r < 1.0) never_mag = false;
        if (ppr / r > 2.0) within_2 = false;
    }
    check(never_mag && within_2, "P22", "every radius from 6 to 96 px reads a level minified by 1:1 to 2:1");
}

/// P23 - the lock fast path (BL-1246): a window wholly inside survey-masked
/// ground is filled with the lock colour directly; it must be byte-identical
/// to the full per-pixel bake. Checked on master chunks of an unsurveyed body
/// and on the first masked master chunks of the home body under its mask
/// (where the path engages), plus one chunk on a mask edge (where it must
/// not change anything either).
void lock_fast_row(world& w, entity_id home, const bake_params& p)
{
    bake_params fast = p, slow = p;
    fast.supersample = slow.supersample = 1; // the master's
    fast.fast_lock = true;
    slow.fast_lock = false;
    constexpr std::uint32_t lock = ui::palette::col32(12, 14, 20, 255);
    int compared = 0, engaged = 0;
    bool equal = true;
    double ms_fast = 0.0, ms_slow = 0.0;
    std::vector<std::uint32_t> a, b;
    const auto compare = [&](const bake_source& s, const geometry& g, int ci, int cj) {
        const int x0 = ci * k_chunk_px, y0 = cj * k_chunk_px;
        const int cw = std::min(k_chunk_px, g.W - x0), chh = std::min(k_chunk_px, g.H - y0);
        a.assign(static_cast<std::size_t>(cw) * chh, 0u);
        b.assign(a.size(), 0u);
        auto t0 = std::chrono::steady_clock::now();
        bake_region(s, g, fast, x0, y0, cw, chh, a.data());
        ms_fast += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        t0 = std::chrono::steady_clock::now();
        bake_region(s, g, slow, x0, y0, cw, chh, b.data());
        ms_slow += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ++compared;
        equal = equal && a == b;
        return std::all_of(a.begin(), a.end(), [&](std::uint32_t c) { return c == lock; });
    };
    for (const auto& [id, bd] : w.bodies)
        if (bd.grid_width > 0 && bd.survey.phase == survey_phase::hidden)
        {
            const bake_source hs = prepare_source(w, id);
            const geometry g = make_master_geometry(bd.grid_width, bd.grid_height);
            const int cw = (g.W + k_chunk_px - 1) / k_chunk_px, ch = (g.H + k_chunk_px - 1) / k_chunk_px;
            engaged += compare(hs, g, cw / 2, ch / 2) ? 1 : 0;
            break;
        }
    {
        const body_component& hb = w.bodies.at(home);
        const bake_source ms = prepare_source(w, home, /*reveal_all=*/false);
        const geometry g = make_master_geometry(hb.grid_width, hb.grid_height);
        const int cw = (g.W + k_chunk_px - 1) / k_chunk_px, ch = (g.H + k_chunk_px - 1) / k_chunk_px;
        int found = 0, edge = 0;
        std::vector<std::uint32_t> probe;
        for (int cj = 1; cj + 1 < ch && (found < 3 || edge < 1); ++cj)
            for (int ci = 0; ci < cw && (found < 3 || edge < 1); ci += 3)
            {
                // Cheap pre-probe: the fast output alone, to find candidates.
                const int x0 = ci * k_chunk_px, y0 = cj * k_chunk_px;
                probe.assign(static_cast<std::size_t>(std::min(k_chunk_px, g.W - x0))
                             * std::min(k_chunk_px, g.H - y0), 0u);
                bake_region(ms, g, fast, x0, y0, std::min(k_chunk_px, g.W - x0),
                            std::min(k_chunk_px, g.H - y0), probe.data());
                const std::size_t n_lock = static_cast<std::size_t>(
                    std::count(probe.begin(), probe.end(), lock));
                if (n_lock == probe.size() && found < 3)
                {
                    engaged += compare(ms, g, ci, cj) ? 1 : 0;
                    ++found;
                }
                else if (n_lock > 0 && n_lock < probe.size() && edge < 1)
                {
                    compare(ms, g, ci, cj); // a mask edge: the path must not engage
                    ++edge;
                }
            }
    }
    // A mask EDGE: a body this seed has partly surveyed — every master chunk
    // in the rows that cross its mask boundary, so the path is exercised
    // right up to the reach margin where it must stop engaging.
    int edge_chunks = 0;
    // No body is part-surveyed in this harness world, so make one: the home
    // body mid-scan, half its survey regions revealed (restored after).
    body_component& hbm = w.bodies.at(home);
    const survey_state saved = hbm.survey;
    hbm.survey.phase = survey_phase::scanning;
    hbm.survey.regions_done = survey_region_count(hbm.grid_width, hbm.grid_height) / 2;
    for (const auto& [id, bd] : w.bodies)
    {
        if (bd.grid_width <= 0 || edge_chunks > 0)
            continue;
        const bake_source s = prepare_source(w, id);
        const std::size_t n_masked = static_cast<std::size_t>(std::count(
            s.cls.begin(), s.cls.end(), static_cast<std::uint8_t>(bake_source::tile_class::masked)));
        if (n_masked == 0 || n_masked == s.cls.size())
            continue;
        const geometry g = make_master_geometry(bd.grid_width, bd.grid_height);
        const int cw = (g.W + k_chunk_px - 1) / k_chunk_px, ch = (g.H + k_chunk_px - 1) / k_chunk_px;
        // Is any tile within `reach` canonical units of the chunk's window
        // NOT masked? (The fast path's own test at reach 4.5.)
        const auto unmasked_within = [&](int ci, int cj, double reach) {
            const double x0 = ci * k_chunk_px / g.s, x1 = (ci + 1) * k_chunk_px / g.s;
            const double y0 = cj * k_chunk_px / g.s + g.y_min, y1 = (cj + 1) * k_chunk_px / g.s + g.y_min;
            const int r_lo = std::max(0, static_cast<int>(std::floor((y0 - reach) / 1.5)));
            const int r_hi = std::min(s.gh - 1, static_cast<int>(std::ceil((y1 + reach) / 1.5)));
            for (int r = r_lo; r <= r_hi; ++r)
                for (int c = static_cast<int>(std::floor((x0 - reach) / 1.7320508075688772)) - 1;
                     c <= static_cast<int>(std::ceil((x1 + reach) / 1.7320508075688772)) + 1; ++c)
                    if (s.cls[static_cast<std::size_t>(r) * s.gw + ((c % s.gw) + s.gw) % s.gw]
                        != static_cast<std::uint8_t>(bake_source::tile_class::masked))
                        return true;
            return false;
        };
        // The chunks the path engages on CLOSEST to the mask edge (visible
        // ground just past the 4.5 reach, within 4.5 + one chunk), and the
        // mixed chunks beside them: where a reach too short would show.
        int near = 0, mixed = 0;
        for (int cj = 1; cj + 1 < ch; ++cj)
            for (int ci = 0; ci < cw; ++ci)
            {
                if (near < 6 && !unmasked_within(ci, cj, 4.5)
                    && unmasked_within(ci, cj, 4.5 + k_chunk_px / g.s))
                {
                    engaged += compare(s, g, ci, cj) ? 1 : 0;
                    ++near;
                    ++edge_chunks;
                    if (mixed < 3 && cj > 0)
                    {
                        compare(s, g, ci, cj - 1);
                        compare(s, g, ci, cj + 1);
                        ++mixed;
                    }
                }
            }
        std::printf("P23: partly surveyed body %u (%zu of %zu tiles masked)\n",
                    static_cast<unsigned>(id), n_masked, s.cls.size());
    }
    hbm.survey = saved;
    std::printf("P23: %d chunks compared, %d wholly lock; fast %.1f ms vs full %.1f ms\n",
                compared, engaged, ms_fast, ms_slow);
    check(compared >= 2 && engaged >= 1, "P23", "found masked master chunks to compare");
    if (edge_chunks == 0)
        std::printf("SKIP  P23 edge: no partly surveyed body on this seed\n");
    check(equal, "P23", "the lock fast path is byte-identical to the full bake (masked and mask-edge chunks)");
}

/// P24 / P25 - the partial re-bake (BL-1246, RENDERING.md § Chunks, cache and
/// invalidation: a building's change re-bakes a window around it, not its
/// whole chunk). On master chunks, exactly as ground_layer bakes them:
///   P24 window invariance — a chunk baked whole, then the windows the rule
///       draws around its installations re-baked and blitted over it, is
///       unchanged byte for byte; and the patch itself — a chunk baked
///       against the source BEFORE an installation change, patched with the
///       windows the rule draws from the before/after diff, equals the whole
///       chunk baked AFTER, byte for byte: real installations, a staged
///       structure straddling a chunk corner (every chunk it reaches), one on
///       the wrap seam (both sides), a build, a removal, a family change, a
///       raze. A terrain change and a dense change fall back to a whole bake.
///   P25 the mip pieces derived from the windows alone, placed over the old
///       chunk's chain, equal the new chunk's chain at every level.
void patch_row(const bake_source& src, const bake_params& p)
{
    const geometry m = make_master_geometry(src.gw, src.gh);
    bake_params p1 = p;
    p1.supersample = 1; // the master's (ground_layer::k_master_ss)
    const int cw = (m.W + k_chunk_px - 1) / k_chunk_px;
    const int ch = (m.H + k_chunk_px - 1) / k_chunk_px;
    constexpr double kS3 = 1.7320508075688772;

    bake_source bare = src;
    bare.inst.of_tile.assign(bare.cls.size(), -1);
    bare.inst.list.clear();
    const auto staged = [](const bake_source& base, std::size_t i, const tile_installation& ti) {
        bake_source s = base;
        if (s.inst.of_tile[i] < 0)
        {
            s.inst.of_tile[i] = static_cast<std::int32_t>(s.inst.list.size());
            s.inst.list.push_back(ti);
        }
        else
            s.inst.list[static_cast<std::size_t>(s.inst.of_tile[i])] = ti;
        return s;
    };
    const auto cleared = [](const bake_source& base, std::size_t i) {
        bake_source s = base;
        s.inst.of_tile[i] = -1; // the list entry is left behind, unreferenced
        return s;
    };
    const auto chunk_of = [&](std::size_t i, int& ci, int& cj) {
        const int r = static_cast<int>(i / src.gw), c = static_cast<int>(i % src.gw);
        const double x = kS3 * (c + ((r & 1) ? 0.5 : 0.0)) * m.s;
        const double y = (1.5 * r - m.y_min) * m.s;
        ci = std::clamp(static_cast<int>(x) / k_chunk_px, 0, cw - 1);
        cj = std::clamp(static_cast<int>(y) / k_chunk_px, 0, ch - 1);
    };

    int cases = 0, windows = 0;
    long long patch_px = 0, chunk_px = 0;
    bool p24_ok = true, p25_ok = true, inv_ok = true;
    std::vector<std::uint32_t> base, target, out;
    std::vector<std::uint32_t> base_mip[k_level_count], target_mip[k_level_count], pm[k_level_count];
    // One case: chunk (ci, cj) baked against @p base_src, the windows of the
    // (rect_old -> rect_new) diff re-baked against rect_new and blitted,
    // compared with the chunk baked whole against rect_new.
    const auto run = [&](const char* name, const bake_source& base_src, const bake_source& rect_old,
                         const bake_source& rect_new, int ci, int cj, bool invariance) {
        const int x0 = ci * k_chunk_px, y0 = cj * k_chunk_px;
        const int w = std::min(k_chunk_px, m.W - x0), h = std::min(k_chunk_px, m.H - y0);
        std::vector<pixel_rect> rects;
        const bool ok = installation_patch_rects(rect_old, rect_new, m, p1, x0, y0, w, h, rects, 1.0);
        base.assign(static_cast<std::size_t>(w) * h, 0u);
        target.assign(base.size(), 0u);
        bake_region(base_src, m, p1, x0, y0, w, h, base.data());
        bake_region(rect_new, m, p1, x0, y0, w, h, target.data());
        derive_mip_pieces(base.data(), w, h, base_mip);
        derive_mip_pieces(target.data(), w, h, target_mip);
        long long area = 0;
        for (const pixel_rect& r : rects)
        {
            out.assign(static_cast<std::size_t>(r.w) * r.h, 0u);
            bake_region(rect_new, m, p1, r.x0, r.y0, r.w, r.h, out.data());
            const int rx = r.x0 - x0, ry = r.y0 - y0;
            for (int y = 0; y < r.h; ++y)
                std::memcpy(base.data() + static_cast<std::size_t>(ry + y) * w + rx,
                            out.data() + static_cast<std::size_t>(y) * r.w, static_cast<std::size_t>(r.w) * 4u);
            derive_mip_pieces(out.data(), r.w, r.h, pm);
            int lw = w;
            for (int l = 1; l < k_level_count; ++l)
            {
                lw /= 2;
                for (int y = 0; y < (r.h >> l); ++y)
                    std::memcpy(base_mip[l].data() + static_cast<std::size_t>((ry >> l) + y) * lw + (rx >> l),
                                pm[l].data() + static_cast<std::size_t>(y) * (r.w >> l),
                                static_cast<std::size_t>(r.w >> l) * 4u);
            }
            area += static_cast<long long>(r.w) * r.h;
        }
        bool same = ok && base == target;
        int first = -1;
        if (!same && ok)
            for (std::size_t q = 0; q < base.size() && first < 0; ++q)
                if (base[q] != target[q])
                    first = static_cast<int>(q);
        bool chain = ok;
        for (int l = 1; l < k_level_count && chain; ++l)
            chain = base_mip[l] == target_mip[l];
        std::printf("P24: %-34s chunk (%d,%d)  %zu window%s  %.1f%% of the chunk%s",
                    name, ci, cj, rects.size(), rects.size() == 1 ? "" : "s",
                    100.0 * area / (static_cast<double>(w) * h), ok ? "" : "  [rule refused]");
        if (first >= 0)
            std::printf("  FIRST MISMATCH at chunk px (%d,%d)", first % w, first / w);
        std::printf("\n");
        ++cases;
        windows += static_cast<int>(rects.size());
        patch_px += area;
        chunk_px += static_cast<long long>(w) * h;
        (invariance ? inv_ok : p24_ok) = (invariance ? inv_ok : p24_ok) && same;
        p25_ok = p25_ok && chain;
        return rects.size();
    };

    // --- Real installations: the chunk holding the most of them ---
    std::vector<int> per_chunk(static_cast<std::size_t>(cw) * ch, 0);
    std::vector<std::size_t> inst_tiles;
    for (std::size_t i = 0; i < src.inst.of_tile.size(); ++i)
        if (src.inst.of_tile[i] >= 0)
        {
            int ci = 0, cj = 0;
            chunk_of(i, ci, cj);
            ++per_chunk[static_cast<std::size_t>(cj) * cw + ci];
            inst_tiles.push_back(i);
        }
    const auto busiest = std::max_element(per_chunk.begin(), per_chunk.end());
    if (!inst_tiles.empty() && *busiest > 0)
    {
        const int k = static_cast<int>(busiest - per_chunk.begin());
        const int ci = k % cw, cj = k / cw;
        std::printf("P24: the real installations' busiest master chunk (%d,%d) holds %d\n", ci, cj, *busiest);
        run("invariance (real, every structure)", src, bare, src, ci, cj, true);
        run("real: every structure built", bare, bare, src, ci, cj, false);
        // One structure in that chunk: built, demolished, re-familied, razed.
        std::size_t one = inst_tiles.front();
        for (const std::size_t i : inst_tiles)
        {
            int a = 0, b = 0;
            chunk_of(i, a, b);
            if (a == ci && b == cj) { one = i; break; }
        }
        const bake_source without = cleared(src, one);
        run("real: one build", without, without, src, ci, cj, false);
        run("real: one demolition", src, src, without, ci, cj, false);
        tile_installation ti = src.inst.list[static_cast<std::size_t>(src.inst.of_tile[one])];
        if (ti.n_stacks > 0)
            ti.stacks[0].family ^= 1;
        else
            ti.settlement.subject = ti.settlement.subject == stamp_subject::ruin
                                        ? stamp_subject::settlement : stamp_subject::ruin;
        const bake_source changed = staged(src, one, ti);
        run("real: one family change / raze", src, src, changed, ci, cj, false);
    }
    else
        std::printf("SKIP  P24 real: this seed's home body stands no installation\n");

    // The biggest staged tile: three stacks and a metropolis.
    tile_installation big;
    {
        const auto bld = [](building_type t, int fam) {
            stamp_key k; k.subject = stamp_subject::building;
            k.type = static_cast<std::uint8_t>(t); k.family = static_cast<std::uint8_t>(fam);
            return k;
        };
        big.stacks[0] = bld(building_type::processing_facility, static_cast<int>(processing_family::refinery));
        big.stacks[1] = bld(building_type::extraction_site, static_cast<int>(extraction_family::mine));
        big.stacks[2] = bld(building_type::port, 0);
        big.n_stacks = 3;
        big.settlement.subject = stamp_subject::settlement;
        big.settlement.scale = 5;
    }
    const auto visible = [&](std::size_t i) {
        return src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::land)
            || src.cls[i] == static_cast<std::uint8_t>(bake_source::tile_class::water);
    };
    // A staged tile built: the rule run on its own master chunk and all eight
    // around it (columns wrapping) — a chunk the rule leaves unpatched must
    // come out unchanged too. Returns the chunks given a window, and whether
    // both sides of the wrap seam were.
    const auto run_reached = [&](const char* name, std::size_t i, bool& both_seam_sides) {
        const bake_source before = cleared(src, i);
        const bake_source after  = staged(src, i, big);
        int ci0 = 0, cj0 = 0;
        chunk_of(i, ci0, cj0);
        int n = 0;
        bool east = false, west = false;
        for (int dj = -1; dj <= 1; ++dj)
            for (int di = -1; di <= 1; ++di)
            {
                const int cj = cj0 + dj;
                const int ci = ((ci0 + di) % cw + cw) % cw;
                if (cj < 0 || cj >= ch)
                    continue;
                if (run(name, before, before, after, ci, cj, false) > 0)
                {
                    ++n;
                    east = east || ci == 0;
                    west = west || ci == cw - 1;
                }
            }
        both_seam_sides = east && west;
        return n;
    };

    // --- A structure straddling a chunk CORNER: the tile whose centre is
    // nearest a corner of the master chunk lattice, in temperate rows ---
    {
        std::size_t best = 0;
        double best_d = 1e30;
        for (int r = src.gh / 4; r < src.gh * 3 / 4; ++r)
            for (int c = 1; c + 1 < src.gw; ++c)
            {
                const std::size_t i = static_cast<std::size_t>(r) * src.gw + c;
                if (!visible(i))
                    continue;
                const double x = kS3 * (c + ((r & 1) ? 0.5 : 0.0)) * m.s;
                const double y = (1.5 * r - m.y_min) * m.s - 40.0; // structures stand up: aim a little high
                const double dx = std::fabs(x - std::round(x / k_chunk_px) * k_chunk_px);
                const double dy = std::fabs(y - std::round(y / k_chunk_px) * k_chunk_px);
                if (dx + dy < best_d) { best_d = dx + dy; best = i; }
            }
        bool seam_unused = false;
        const int n = run_reached("staged: straddling a chunk corner", best, seam_unused);
        std::printf("P24: the corner tile [%zu,%zu] reaches %d master chunks\n",
                    best % src.gw, best / src.gw, n);
        check(n >= 3, "P24", "the corner case reaches at least three chunks (it straddles)");

        // EVERY FORM, one at a time, built on that tile: a window wide enough
        // for any structure, baked before and after, patched by the rule —
        // so no form draws a pixel its window misses.
        const auto bld = [](building_type t, int fam, stamp_subject sj = stamp_subject::building) {
            stamp_key k; k.subject = sj;
            k.type = static_cast<std::uint8_t>(t); k.family = static_cast<std::uint8_t>(fam);
            return k;
        };
        std::vector<tile_installation> forms;
        const auto one = [&](stamp_key a) { tile_installation t; t.stacks[0] = a; t.n_stacks = 1; forms.push_back(t); };
        for (int f = 0; f < static_cast<int>(extraction_family::count); ++f)
            one(bld(building_type::extraction_site, f));
        for (int f = 0; f < static_cast<int>(processing_family::count); ++f)
            one(bld(building_type::processing_facility, f));
        for (building_type t : { building_type::port, building_type::launchpad,
                                 building_type::inland_logistics_hub, building_type::military_base,
                                 building_type::research_institute, building_type::schooling,
                                 building_type::university })
            one(bld(t, 0));
        one(bld(building_type::processing_facility, static_cast<int>(processing_family::metal_foundry),
                stamp_subject::scaffold));
        for (int sc = 1; sc <= 5; ++sc)
        {
            tile_installation t; t.settlement.subject = stamp_subject::settlement;
            t.settlement.scale = static_cast<std::uint8_t>(sc);
            forms.push_back(t);
        }
        {
            tile_installation t; t.settlement.subject = stamp_subject::ruin; t.settlement.scale = 1;
            forms.push_back(t);
        }
        forms.push_back(big);
        const int br = static_cast<int>(best / src.gw), bc = static_cast<int>(best % src.gw);
        const double bx = kS3 * (bc + ((br & 1) ? 0.5 : 0.0)), by = 1.5 * br;
        const int fx0 = (static_cast<int>((bx - 3.5) * m.s) / k_patch_align) * k_patch_align;
        const int fy0 = (static_cast<int>((by - 4.0 - m.y_min) * m.s) / k_patch_align) * k_patch_align;
        const int fw = static_cast<int>(7.5 * m.s) / k_patch_align * k_patch_align;
        const int fh = static_cast<int>(7.0 * m.s) / k_patch_align * k_patch_align;
        const bake_source before = cleared(src, best);
        std::vector<std::uint32_t> fb(static_cast<std::size_t>(fw) * fh), fa(fb.size()), fo;
        bake_region(before, m, p1, fx0, fy0, fw, fh, fb.data());
        int forms_ok = 0;
        double max_frac = 0.0;
        for (const tile_installation& ti : forms)
        {
            const bake_source after = staged(before, best, ti);
            bake_region(after, m, p1, fx0, fy0, fw, fh, fa.data());
            std::vector<pixel_rect> rr;
            std::vector<std::uint32_t> patched = fb;
            bool good = installation_patch_rects(before, after, m, p1, fx0, fy0, fw, fh, rr, 1.0);
            long long area = 0;
            for (const pixel_rect& r : rr)
            {
                fo.assign(static_cast<std::size_t>(r.w) * r.h, 0u);
                bake_region(after, m, p1, r.x0, r.y0, r.w, r.h, fo.data());
                for (int y = 0; y < r.h; ++y)
                    std::memcpy(patched.data() + static_cast<std::size_t>(r.y0 - fy0 + y) * fw + (r.x0 - fx0),
                                fo.data() + static_cast<std::size_t>(y) * r.w, static_cast<std::size_t>(r.w) * 4u);
                area += static_cast<long long>(r.w) * r.h;
            }
            good = good && patched == fa && fa != fb;
            // And the form's window-invariance at another window origin on the
            // 16 px lattice: the overlap of a shifted window bakes the same.
            {
                const int sx = 48, sy = 32;
                std::vector<std::uint32_t> sh(fa.size());
                bake_region(after, m, p1, fx0 + sx, fy0 + sy, fw, fh, sh.data());
                for (int y = 0; y + sy < fh && good; ++y)
                    good = std::memcmp(sh.data() + static_cast<std::size_t>(y) * fw,
                                       fa.data() + static_cast<std::size_t>(y + sy) * fw + sx,
                                       static_cast<std::size_t>(fw - sx) * 4u) == 0;
            }
            max_frac = std::max(max_frac, area / (static_cast<double>(k_chunk_px) * k_chunk_px));
            if (!good)
            {
                int dx0 = fw, dy0 = fh, dx1 = -1, dy1 = -1, miss = 0;
                for (int y = 0; y < fh; ++y)
                    for (int x = 0; x < fw; ++x)
                    {
                        const std::size_t q = static_cast<std::size_t>(y) * fw + x;
                        if (fa[q] == fb[q])
                            continue;
                        dx0 = std::min(dx0, x); dx1 = std::max(dx1, x);
                        dy0 = std::min(dy0, y); dy1 = std::max(dy1, y);
                        miss += patched[q] != fa[q];
                    }
                std::printf("      form %s type %d family %d scale %d: the window misses %d pixels it "
                            "draws (changed px x %d..%d y %d..%d of the %dx%d window",
                            stamp_name(ti.n_stacks ? ti.stacks[0] : ti.settlement),
                            ti.stacks[0].type, ti.stacks[0].family, ti.settlement.scale, miss,
                            dx0, dx1, dy0, dy1, fw, fh);
                for (const pixel_rect& r : rr)
                    std::printf("; rect x %d..%d y %d..%d", r.x0 - fx0, r.x0 - fx0 + r.w - 1,
                                r.y0 - fy0, r.y0 - fy0 + r.h - 1);
                std::printf(")\n");
                // Triage: which pass is window-dependent? The rect baked alone
                // vs the same pixels of the wide window, each pass off in turn.
                if (!rr.empty())
                {
                    const pixel_rect r = rr.front();
                    struct off { const char* name; void (*set)(bake_params&); };
                    const off offs[] = {
                        { "all on",           [](bake_params&) {} },
                        { "no-installations", [](bake_params& q) { q.installations = false; } },
                        { "no-unsharp",       [](bake_params& q) { q.unsharp_amount = 0.0f; } },
                        { "no-ink",           [](bake_params& q) { q.edge_ink = q.shore_ink = 0.0f; } },
                        { "no-grade",         [](bake_params& q) { q.grade_enabled = false; } },
                        { "no-trees",         [](bake_params& q) { q.tree_density = 0.0f; } },
                        { "no-landforms",     [](bake_params& q) { q.landform_strength = 0.0f; } },
                        { "no-rivers",        [](bake_params& q) { q.river_strength = 0.0f; } },
                        { "no-variants",      [](bake_params& q) { q.variant_strength = 0.0f; } },
                    };
                    std::vector<std::uint32_t> wide(fa.size()), part;
                    for (const off& o : offs)
                    {
                        bake_params q = p1;
                        o.set(q);
                        bake_region(after, m, q, fx0, fy0, fw, fh, wide.data());
                        part.assign(static_cast<std::size_t>(r.w) * r.h, 0u);
                        bake_region(after, m, q, r.x0, r.y0, r.w, r.h, part.data());
                        int bad = 0, fx = -1, fy = -1;
                        for (int y = 0; y < r.h; ++y)
                            for (int x = 0; x < r.w; ++x)
                                if (part[static_cast<std::size_t>(y) * r.w + x]
                                    != wide[static_cast<std::size_t>(r.y0 - fy0 + y) * fw + (r.x0 - fx0 + x)])
                                {
                                    if (!bad) { fx = x; fy = y; }
                                    ++bad;
                                }
                        std::printf("        triage %-16s %d px differ (first at rect px %d,%d)\n", o.name, bad, fx, fy);
                    }
                }
            }
            forms_ok += good ? 1 : 0;
        }
        std::printf("P24: %d of %zu forms patched exactly; the largest window is %.0f%% of a chunk\n",
                    forms_ok, forms.size(), 100.0 * max_frac);
        check(forms_ok == static_cast<int>(forms.size()), "P24",
              "every procedural form's window holds every pixel it draws, and the form bakes alike at another window origin");
    }
    // --- A structure in a FOREST: the trees its cleared ground removes ---
    {
        const int f = homogeneous_aim(src, terrain_cover::forest);
        if (f >= 0)
        {
            bool unused = false;
            run_reached("staged: in a forest (trees cleared)", static_cast<std::size_t>(f), unused);
        }
        else
            std::printf("SKIP  P24 forest: no forest on this seed's home body\n");
    }
    // --- A structure on the WRAP SEAM: column 0, mid-body ---
    {
        std::size_t seam = static_cast<std::size_t>(src.gh / 2) * src.gw;
        for (int r = src.gh / 2; r < src.gh * 3 / 4; r += 2) // even rows: centre at x = 0 exactly
        {
            const std::size_t i = static_cast<std::size_t>(r) * src.gw;
            if (visible(i)) { seam = i; break; }
        }
        bool both = false;
        run_reached("staged: on the wrap seam", seam, both);
        check(both, "P24", "the wrap-seam structure is patched on both sides of the seam");
    }

    // --- The fall-backs ---
    {
        const int ci = cw / 2, cj = ch / 2;
        const int x0 = ci * k_chunk_px, y0 = cj * k_chunk_px;
        const int w = std::min(k_chunk_px, m.W - x0), h = std::min(k_chunk_px, m.H - y0);
        // A terrain change inside the chunk's reach: never a patch.
        std::size_t t = 0;
        for (std::size_t i = 0; i < src.cls.size(); ++i)
        {
            int a = 0, b = 0;
            chunk_of(i, a, b);
            if (a == ci && b == cj && visible(i)) { t = i; break; }
        }
        bake_source terr = staged(src, t, big);
        terr.colour[t] ^= 0x00101010u;
        std::vector<pixel_rect> rr;
        check(!installation_patch_rects(cleared(src, t), terr, m, p1, x0, y0, w, h, rr),
              "P24", "a terrain change in reach re-bakes the chunk whole");
        // Every tile of the chunk building at once: past 40%, whole.
        bake_source dense = bare;
        for (std::size_t i = 0; i < src.cls.size(); ++i)
        {
            int a = 0, b = 0;
            chunk_of(i, a, b);
            if (a == ci && b == cj && visible(i))
            {
                dense.inst.of_tile[i] = static_cast<std::int32_t>(dense.inst.list.size());
                dense.inst.list.push_back(big);
            }
        }
        check(!installation_patch_rects(bare, dense, m, p1, x0, y0, w, h, rr),
              "P24", "windows past 40% of the chunk re-bake it whole");
        check(installation_patch_rects(src, src, m, p1, x0, y0, w, h, rr) && rr.empty(),
              "P24", "no change, no window");
    }

    std::printf("P24: %d cases, %d windows, %.1f%% of their chunks' pixels re-baked\n", cases, windows,
                chunk_px ? 100.0 * patch_px / chunk_px : 0.0);
    check(inv_ok, "P24", "windows around installations re-baked and blitted leave a whole-chunk bake byte-identical");
    check(p24_ok, "P24", "a chunk patched by the window rule equals the chunk baked whole after the change");
    check(p25_ok, "P25", "the windows' own mip pieces, placed, equal the whole chunk's chain at 48/24/12/6");
}

} // namespace

/// P26 / P27 — tiles hold their own ground (BL-1251, RENDERING.md § Tiles hold
/// their own ground).
///   P26 The edge band holds: a tile's centre region bakes byte-identical
///       whatever its neighbours' ground is (colour, family, variant — so
///       their border sets too); the pre-BL-1251 wide blend (edge_band 0)
///       fails the same comparison, so the row has teeth.
///   P27 Every border set found on the home body, and the shore shelf, bakes
///       deterministically, wrap-exact one period east, seamless across a
///       chunk edge, at the real master geometry — and actually draws
///       (border_strength 0 differs).
/// @p previews also writes bl1251_<subject>_{legacy,now}.png master windows.
void border_row(const bake_source& src, const body_component& hb, const bake_params& p, bool previews)
{
    const auto land = static_cast<std::uint8_t>(bake_source::tile_class::land);
    const auto water = static_cast<std::uint8_t>(bake_source::tile_class::water);
    const auto nb = [&](int i, int side) -> int
    {
        const hex_neighbors::coord c = hex_neighbors::neighbour(i % src.gw, i / src.gw, side);
        if (c.gy < 0 || c.gy >= src.gh)
            return -1;
        return c.gy * src.gw + ((c.gx % src.gw) + src.gw) % src.gw;
    };
    const auto bare_tile = [&](int i) -> bool
    {
        return i >= 0 && src.cls[i] == land && !src.near_feature[i]
            && (static_cast<std::size_t>(i) >= src.inst.of_tile.size() || src.inst.of_tile[i] < 0);
    };
    const auto temperate = [&](int i) { const int r = i / src.gw; return r > src.gh / 5 && r < src.gh * 4 / 5; };

    // ---- P26: a tile whose whole ring is quiet land.
    {
        int t = -1;
        for (int i = 0; i < static_cast<int>(src.cls.size()) && t < 0; ++i)
        {
            if (!temperate(i) || !bare_tile(i))
                continue;
            bool ok = true;
            for (int s = 0; s < 6 && ok; ++s)
                ok = bare_tile(nb(i, s));
            if (ok)
                t = i;
        }
        check(t >= 0, "P26", "found a land tile whose six neighbours are quiet land");
        if (t >= 0)
        {
            // Every neighbour's ground changed: another family (and so another
            // border set facing t), another colour, another variant.
            bake_source alt = src;
            for (int s = 0; s < 6; ++s)
            {
                const int j = nb(t, s);
                alt.family[j]  = static_cast<std::uint8_t>(alt.family[t] == 0 ? 4 : 0); // grass <-> bare
                alt.colour[j]  = ui::palette::col32(200, 40, 160, 255);
                alt.variant[j] = static_cast<std::uint8_t>((alt.variant[j] + 1) % k_variant_count);
                for (int k = 0; k < k_vparam_count; ++k)
                    alt.vparam[static_cast<std::size_t>(j) * k_vparam_count + k] *= -1.5f;
            }
            rederive_border_sets(alt);
            const geometry gf = make_geometry(hb.grid_width, hb.grid_height, 96.0); // flat: canonical = pixel
            const int tr = t / src.gw, tc = t % src.gw;
            const double cx = 1.7320508075688772 * (tc + ((tr & 1) ? 0.5 : 0.0)), cy = 1.5 * tr;
            const int side = 256;
            const int px0 = static_cast<int>(cx * gf.s) - side / 2;
            const int py0 = static_cast<int>((cy - gf.y_min) * gf.s) - side / 2;
            const auto centre_equal = [&](const bake_params& q, int& compared) -> bool
            {
                std::vector<std::uint32_t> a(static_cast<std::size_t>(side) * side), b(a.size());
                bake_region(src, gf, q, px0, py0, side, side, a.data());
                bake_region(alt, gf, q, px0, py0, side, side, b.data());
                compared = 0;
                bool eq = true;
                for (int y = 0; y < side; ++y)
                    for (int x = 0; x < side; ++x)
                    {
                        const double dx = (px0 + x + 0.5) / gf.s - cx;
                        const double dy = (py0 + y + 0.5) / gf.s + gf.y_min - cy;
                        if (dx * dx + dy * dy > 0.30 * 0.30)
                            continue;
                        ++compared;
                        eq = eq && a[static_cast<std::size_t>(y) * side + x] == b[static_cast<std::size_t>(y) * side + x];
                    }
                return eq;
            };
            int n_now = 0, n_old = 0;
            const bool now = centre_equal(p, n_now);
            bake_params legacy = p;
            legacy.edge_band = 0.0f;
            legacy.border_strength = 0.0f;
            legacy.pattern_strength = 0.0f;
            const bool old = centre_equal(legacy, n_old);
            std::printf("P26: tile [%d,%d], %d centre pixels (r 0.30 of the 0.866 inradius)\n", tc, tr, n_now);
            check(now && n_now > 1000, "P26", "a tile's centre region is its own ground whatever its neighbours are");
            check(!old, "P26", "the pre-BL-1251 wide blend fails the same comparison (the row has teeth)");
        }
    }

    // ---- P27: each border set, and the shore shelf, at the master geometry.
    const geometry gm = make_master_geometry(hb.grid_width, hb.grid_height);
    struct subject { const char* name; int tile; int side; };
    std::vector<subject> subs;
    static const char* const set_names[bs_count] = { "none", "forest fringe", "scrub fringe", "scree lip",
                                                     "field edge", "reed fringe", "drift lip", "snow drift" };
    for (int set = 1; set < bs_count; ++set)
    {
        int found = -1, fside = -1;
        for (int i = 0; i < static_cast<int>(src.cls.size()) && found < 0; ++i)
        {
            if (!temperate(i) && set != bs_snow)
                continue;
            if (!bare_tile(i))
                continue;
            for (int s = 0; s < 6; ++s)
                if (src.bset[static_cast<std::size_t>(i) * 6 + s] == set && bare_tile(nb(i, s)))
                {
                    found = i;
                    fside = s;
                    break;
                }
        }
        if (found >= 0)
            subs.push_back({ set_names[set], found, fside });
        else
            std::printf("P27: no %s edge on this body\n", set_names[set]);
    }
    {
        int found = -1, fside = -1;
        for (int i = 0; i < static_cast<int>(src.cls.size()) && found < 0; ++i)
        {
            if (!temperate(i) || !bare_tile(i))
                continue;
            for (int s = 0; s < 6; ++s)
            {
                const int j = nb(i, s);
                if (j >= 0 && src.cls[j] == water && !src.near_feature[j])
                {
                    found = i;
                    fside = s;
                    break;
                }
            }
        }
        if (found >= 0)
            subs.push_back({ "shore shelf", found, fside });
    }
    const auto has = [&](const char* nm) {
        for (const subject& s : subs) if (std::strcmp(s.name, nm) == 0) return true;
        return false;
    };
    check(has("forest fringe") && has("scree lip") && has("field edge") && has("shore shelf"), "P27",
          "found a forest fringe, a scree lip, a field edge and a shore on the home body");
    static const double kDx[6] = { 1.7320508075688772, 0.8660254037844386, -0.8660254037844386,
                                   -1.7320508075688772, -0.8660254037844386, 0.8660254037844386 };
    static const double kDy[6] = { 0.0, -1.5, -1.5, 0.0, 1.5, 1.5 };
    bake_params off = p;
    off.border_strength = 0.0f;
    bool all_det = true, all_wrap = true, all_seam = true, all_draw = true;
    for (const subject& sj : subs)
    {
        const int tr = sj.tile / src.gw, tc = sj.tile % src.gw;
        const double mx = 1.7320508075688772 * (tc + ((tr & 1) ? 0.5 : 0.0)) + 0.5 * kDx[sj.side];
        const double my = 1.5 * tr + 0.5 * kDy[sj.side];
        const int side = 128;
        const int px0 = static_cast<int>(mx * gm.s) - side;
        const int py0 = std::clamp(static_cast<int>((my - gm.y_min - 0.3) * gm.s) - side / 2, 0, gm.H - side);
        const std::size_t n = static_cast<std::size_t>(side) * side * 2;
        std::vector<std::uint32_t> a(n), b(n), e(n), o(n), l(n / 2), r(n / 2);
        bake_region(src, gm, p, px0, py0, side * 2, side, a.data());
        bake_region(src, gm, p, px0, py0, side * 2, side, b.data());
        bake_region(src, gm, p, px0 + gm.W, py0, side * 2, side, e.data());
        bake_region(src, gm, off, px0, py0, side * 2, side, o.data());
        bake_region(src, gm, p, px0, py0, side, side, l.data());
        bake_region(src, gm, p, px0 + side, py0, side, side, r.data());
        bool seam = true;
        for (int y = 0; y < side && seam; ++y)
            for (int x = 0; x < side; ++x)
                if (a[static_cast<std::size_t>(y) * side * 2 + x] != l[static_cast<std::size_t>(y) * side + x]
                    || a[static_cast<std::size_t>(y) * side * 2 + side + x] != r[static_cast<std::size_t>(y) * side + x])
                {
                    seam = false;
                    break;
                }
        int diff = 0;
        for (std::size_t i = 0; i < n; ++i)
            diff += a[i] != o[i];
        std::printf("P27: %-13s tile [%d,%d] side %d: %s, %s, %s, %d px drawn\n", sj.name, tc, tr, sj.side,
                    a == b ? "pure" : "IMPURE", a == e ? "wrap-exact" : "WRAP SEAM", seam ? "seamless" : "CHUNK SEAM", diff);
        all_det  = all_det && a == b;
        all_wrap = all_wrap && a == e;
        all_seam = all_seam && seam;
        all_draw = all_draw && diff > 200;
        if (previews)
        {
            // A wider look for the eye: 512 x 320 master pixels, legacy vs now.
            const int W = 512, H = 320;
            const int qx0 = static_cast<int>(mx * gm.s) - W / 2;
            const int qy0 = std::clamp(static_cast<int>((my - gm.y_min - 0.3) * gm.s) - H / 2, 0, gm.H - H);
            bake_params legacy = p;
            legacy.edge_band = 0.0f; legacy.border_strength = 0.0f; legacy.pattern_strength = 0.0f;
            legacy.edge_ink = 0.30f;
            std::vector<std::uint32_t> pb(static_cast<std::size_t>(W) * H);
            for (int k = 0; k < 2; ++k)
            {
                bake_region(src, gm, k ? p : legacy, qx0, qy0, W, H, pb.data());
                char path[160];
                std::string nm = sj.name;
                for (char& c : nm) if (c == ' ') c = '_';
                std::snprintf(path, sizeof path, "bl1251_%s_%s.png", nm.c_str(), k ? "now" : "legacy");
                write_png_rgba(path, W, H, reinterpret_cast<const unsigned char*>(pb.data()), W * 4);
            }
        }
    }
    check(!subs.empty() && all_det,  "P27", "every border set and the shore bake byte-identical twice");
    check(!subs.empty() && all_wrap, "P27", "every border set and the shore wrap byte-identical one period east");
    check(!subs.empty() && all_seam, "P27", "every border set and the shore are seamless across a chunk edge");
    check(!subs.empty() && all_draw, "P27", "every border set and the shore actually draw (strength 0 differs)");
    if (previews)
    {
        // The wide plain and a deep forest's interior, for the patterns.
        for (const auto& [nm, cov] : { std::pair{ "plain", terrain_cover::grass },
                                       std::pair{ "forest", terrain_cover::forest },
                                       std::pair{ "scrub", terrain_cover::scrub },
                                       std::pair{ "bare", terrain_cover::none } })
        {
            const int t = homogeneous_aim(src, cov);
            if (t < 0)
                continue;
            const int tr = t / src.gw, tc = t % src.gw;
            const double cx = 1.7320508075688772 * (tc + ((tr & 1) ? 0.5 : 0.0));
            const int W = 640, H = 400;
            const int qx0 = static_cast<int>(cx * gm.s) - W / 2;
            const int qy0 = std::clamp(static_cast<int>((1.5 * tr - gm.y_min - 0.3) * gm.s) - H / 2, 0, gm.H - H);
            bake_params legacy = p;
            legacy.edge_band = 0.0f; legacy.border_strength = 0.0f; legacy.pattern_strength = 0.0f;
            legacy.edge_ink = 0.30f;
            std::vector<std::uint32_t> pb(static_cast<std::size_t>(W) * H);
            for (int k = 0; k < 2; ++k)
            {
                bake_region(src, gm, k ? p : legacy, qx0, qy0, W, H, pb.data());
                char path[160];
                std::snprintf(path, sizeof path, "bl1251_%s_%s.png", nm, k ? "now" : "legacy");
                write_png_rgba(path, W, H, reinterpret_cast<const unsigned char*>(pb.data()), W * 4);
            }
            std::printf("preview %s at [%d,%d]\n", nm, tc, tr);
        }
    }
}

int main(int argc, char** argv)
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

    // --aim: print the variant-capture aims (BL-1243) and stop — the tile
    // coordinates scripts/verify/terrain_variants.lua frames, without the
    // ten-minute Debug run of every phase.
    if (argc > 1 && std::strcmp(argv[1], "--aim") == 0)
    {
        for (const auto& [name, cov] : { std::pair{ "plain (grass)", terrain_cover::grass },
                                         std::pair{ "bare (none)",   terrain_cover::none },
                                         std::pair{ "forest",        terrain_cover::forest },
                                         std::pair{ "scrub",         terrain_cover::scrub } })
        {
            int score = 0;
            const int t = homogeneous_aim(src, cov, &score);
            if (t >= 0)
                std::printf("AIM  %-14s [%d,%d]  %d/%d of its neighbourhood alike\n", name,
                            t % src.gw, t / src.gw, score, 37);
            else
                std::printf("AIM  %-14s none\n", name);
        }
        return 0;
    }

    // --pool: the worker-pool row (P21) alone.
    if (argc > 1 && std::strcmp(argv[1], "--pool") == 0)
    {
        pool_row(src, hb, land_i, p);
        std::printf("%s (%d failures)\n", g_failures ? "FAIL" : "PASS", g_failures);
        return g_failures ? 1 : 0;
    }

    // --prof: where a master chunk's time goes under the BL-1251 passes — one
    // 512 px master chunk (the real geometry, 1x) on the wide plain and on the
    // forest fringe, best of 3, with each pass off in turn. A reading.
    if (argc > 1 && std::strcmp(argv[1], "--prof") == 0)
    {
        const geometry gm = make_master_geometry(hb.grid_width, hb.grid_height);
        bake_params base = p;
        base.supersample = 1;
        struct cfg { const char* name; bake_params q; };
        std::vector<cfg> cfgs;
        cfgs.push_back({ "now", base });
        { bake_params q = base; q.pattern_strength = 0.0f; cfgs.push_back({ "no patterns", q }); }
        { bake_params q = base; q.border_strength = 0.0f;  cfgs.push_back({ "no borders", q }); }
        { bake_params q = base; q.border_strength = 0.0f; q.pattern_strength = 0.0f; cfgs.push_back({ "band only", q }); }
        { bake_params q = base; q.edge_band = 0.0f; q.border_strength = 0.0f; q.pattern_strength = 0.0f;
          q.edge_ink = 0.30f; q.shore_ink = 0.42f; cfgs.push_back({ "pre-BL-1251", q }); }
        std::vector<std::uint32_t> tb(512u * 512u);
        for (const auto& [nm, cov] : { std::pair{ "plain", terrain_cover::grass },
                                       std::pair{ "forest", terrain_cover::forest } })
        {
            const int aim = std::max(0, homogeneous_aim(src, cov));
            const int ar = aim / src.gw, ac = aim % src.gw;
            const double ax = 1.7320508075688772 * (ac + ((ar & 1) ? 0.5 : 0.0));
            const int x0 = static_cast<int>(ax * gm.s) / 512 * 512;
            const int y0 = std::clamp(static_cast<int>((1.5 * ar - gm.y_min) * gm.s) / 512 * 512, 0, gm.H - 512);
            // Interleaved (every configuration once per round, nine rounds)
            // and the minimum kept, of wall time and of this thread's cycles:
            // a load spike then costs one sample of one configuration, not
            // a whole configuration's reading.
            std::vector<double> wall(cfgs.size(), 1e30), cyc(cfgs.size(), 1e30);
            for (int rep = 0; rep < 9; ++rep)
                for (std::size_t k = 0; k < cfgs.size(); ++k)
                {
                    const double c0 = thread_cpu_ms();
                    const auto t0 = std::chrono::steady_clock::now();
                    bake_region(src, gm, cfgs[k].q, x0, y0, 512, 512, tb.data());
                    const double c1 = thread_cpu_ms();
                    wall[k] = std::min(wall[k], std::chrono::duration<double, std::milli>(
                                                    std::chrono::steady_clock::now() - t0).count());
                    if (c1 >= 0.0)
                        cyc[k] = std::min(cyc[k], c1 - c0);
                }
            for (std::size_t k = 0; k < cfgs.size(); ++k)
                std::printf("PROF  %-7s master chunk  %-12s %8.1f ms wall  %8.1f Mcycles  (%+.0f%% vs pre-BL-1251)\n",
                            nm, cfgs[k].name, wall[k], cyc[k] < 1e29 ? cyc[k] : -1.0,
                            100.0 * (wall[k] / wall.back() - 1.0));
        }
        return 0;
    }

    // --border: the BL-1251 rows (P26, P27) alone, with previews.
    if (argc > 1 && std::strcmp(argv[1], "--border") == 0)
    {
        border_row(src, hb, p, /*previews=*/true);
        std::printf("%s (%d failures)\n", g_failures ? "FAIL" : "PASS", g_failures);
        return g_failures ? 1 : 0;
    }

    // --patch: the partial re-bake rows (P24, P25) alone.
    if (argc > 1 && std::strcmp(argv[1], "--patch") == 0)
    {
        patch_row(src, p);
        std::printf("%s (%d failures)\n", g_failures ? "FAIL" : "PASS", g_failures);
        return g_failures ? 1 : 0;
    }

    // --master: the whole-body master bake reading (BL-1246; RENDERING.md
    // § Level of detail; TECH_FOUNDATIONS.md § Target hardware — the pre-bake
    // must add at most 15 s): the home body's master on the pool's thread
    // count, 1x and 2x, under the survey mask as play opens and fully
    // revealed (the worst case), plus P22. A reading; only P22 can fail.
    if (argc > 1 && std::strcmp(argv[1], "--master") == 0)
    {
        master_row(src, hb, land_i, p);
        lock_fast_row(w, home, p);
        const bake_source masked = prepare_source(w, home, /*reveal_all=*/false);
        master_bake(masked, p, 1, "masked");
        master_bake(src, p, 1, "revealed");
        // An unsurveyed body: every tile the lock fill.
        for (const auto& [id, bd] : w.bodies)
            if (bd.grid_width > 0 && bd.survey.phase == survey_phase::hidden)
            {
                master_bake(prepare_source(w, id), p, 1, "unsurveyed");
                break;
            }
        master_bake(masked, p, 2, "masked");
        master_bake(src, p, 2, "revealed");
        // Where the revealed master's time goes: the whole 1x bake with one
        // pass switched off at a time (a reading for whoever optimises it).
        {
            struct off { const char* name; void (*set)(bake_params&); };
            const off offs[] = {
                { "no-installations", [](bake_params& q) { q.installations = false; } },
                { "no-landforms",     [](bake_params& q) { q.landform_strength = 0.0f; } },
                { "no-rivers",        [](bake_params& q) { q.river_strength = 0.0f; } },
                { "no-variants",      [](bake_params& q) { q.variant_strength = 0.0f; } },
                { "no-trees",         [](bake_params& q) { q.tree_density = 0.0f; } },
                { "no-edges/unsharp", [](bake_params& q) { q.edge_ink = q.shore_ink = 0.0f; q.unsharp_amount = 0.0f; } },
                { "no-patterns",      [](bake_params& q) { q.pattern_strength = 0.0f; } },
                { "no-borders",       [](bake_params& q) { q.border_strength = 0.0f; } },
                { "pre-BL-1251",      [](bake_params& q) { q.edge_band = 0.0f; q.border_strength = 0.0f;
                                                           q.pattern_strength = 0.0f; q.edge_ink = 0.30f;
                                                           q.shore_ink = 0.42f; } },
            };
            for (const off& o : offs)
            {
                bake_params q = p;
                o.set(q);
                master_bake(src, q, 1, o.name);
            }
        }
        std::printf("%s (%d failures)\n", g_failures ? "FAIL" : "PASS", g_failures);
        return g_failures ? 1 : 0;
    }

    // --variants: run only the BL-1243 rows (P17-P20) and their previews,
    // without the timing readings — the fast loop for tuning the tables.
    const bool variants_only = argc > 1 && std::strcmp(argv[1], "--variants") == 0;

    // --timing: the BL-1243 cost reading alone — one 512 px chunk per tier on
    // the wide plain, variants off vs on, interleaved, best of 7 each.
    if (argc > 1 && std::strcmp(argv[1], "--timing") == 0)
    {
        const int aim = std::max(0, homogeneous_aim(src, terrain_cover::grass));
        const int ar = aim / src.gw, ac = aim % src.gw;
        const double ax = 1.7320508075688772 * (ac + ((ar & 1) ? 0.5 : 0.0));
        bake_params v_off = p;
        v_off.variant_strength = 0.0f;
        std::vector<std::uint32_t> tb(512u * 512u);
        for (double tp : { 6.0, 12.0, 24.0, 48.0, 96.0 })
        {
            const geometry gt = make_geometry(hb.grid_width, hb.grid_height, tp);
            const int cw = std::min(512, gt.W), ch = std::min(512, gt.H);
            const int x0 = static_cast<int>(ax * gt.s) - cw / 2;
            const int y0 = std::clamp(static_cast<int>((1.5 * ar - gt.y_min) * gt.s) - ch / 2,
                                      0, std::max(0, gt.H - ch));
            double ms[2] = { 1e30, 1e30 };
            for (int rep = 0; rep < 7; ++rep)
                for (int k = 0; k < 2; ++k)
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    bake_region(src, gt, k ? p : v_off, x0, y0, cw, ch, tb.data());
                    ms[k] = std::min(ms[k], std::chrono::duration<double, std::milli>(
                                                std::chrono::steady_clock::now() - t0).count());
                }
            std::printf("TIME  variants  tier %3.0f px/r  chunk %dx%d (plain)  off %8.1f ms  on %8.1f ms  (%+.0f%%)\n",
                        tp, cw, ch, ms[0], ms[1], 100.0 * (ms[1] / ms[0] - 1.0));
        }
        return 0;
    }

    // --seam: the P11 chunk-seam window alone, with each pass switched off in
    // turn, naming the first mismatching pixel — the triage for a seam.
    if (argc > 1 && std::strcmp(argv[1], "--seam") == 0)
    {
        const geometry g48 = make_geometry(hb.grid_width, hb.grid_height, 48.0);
        const int lr = land_i / src.gw, lc = land_i % src.gw;
        const double lx = 1.7320508075688772 * (lc + ((lr & 1) ? 0.5 : 0.0));
        const int side = 96;
        const int px0 = static_cast<int>(lx * g48.s) - side / 2;
        const int py0 = std::clamp(static_cast<int>((1.5 * lr - g48.y_min) * g48.s) - side / 2,
                                   0, std::max(0, g48.H - side));
        std::printf("seam window px0 %d py0 %d (tile [%d,%d])\n", px0, py0, lc, lr);
        struct variant_case { const char* name; bake_params bp; };
        std::vector<variant_case> cases;
        cases.push_back({ "default", p });
        { bake_params q = p; q.variant_strength = 0.0f;  cases.push_back({ "variants off", q }); }
        { bake_params q = p; q.tree_density = 0.0f;      cases.push_back({ "trees off", q }); }
        { bake_params q = p; q.installations = false;    cases.push_back({ "installations off", q }); }
        { bake_params q = p; q.landform_strength = 0.0f; q.river_strength = 0.0f; cases.push_back({ "features off", q }); }
        // Ground nudges with the variants OFF: a seam that appears here is
        // not the variants' — it is a pass whose rounding is window-relative
        // and merely revealed by whatever ground lies under it.
        for (float k : { 1.02f, 1.04f, 0.98f })
        {
            bake_params q = p; q.variant_strength = 0.0f; q.noise_strength *= k;
            static char names[3][32];
            static int ni = 0;
            std::snprintf(names[ni % 3], sizeof names[0], "variants off, grain x%.2f", k);
            cases.push_back({ names[ni++ % 3], q });
        }
        for (float k : { 1.02f, 1.04f, 0.98f })
        {
            bake_params q = p; q.variant_strength = 0.0f; q.noise_strength *= k; q.installations = false;
            static char names2[3][40];
            static int ni2 = 0;
            std::snprintf(names2[ni2 % 3], sizeof names2[0], "  same, installations off x%.2f", k);
            cases.push_back({ names2[ni2++ % 3], q });
        }
        for (const variant_case& vc : cases)
        {
            const std::size_t n = static_cast<std::size_t>(side) * side;
            std::vector<std::uint32_t> whole(n * 2), l(n), r(n);
            bake_region(src, g48, vc.bp, px0, py0, side * 2, side, whole.data());
            bake_region(src, g48, vc.bp, px0, py0, side, side, l.data());
            bake_region(src, g48, vc.bp, px0 + side, py0, side, side, r.data());
            int bad = 0, fx = -1, fy = -1;
            for (int y = 0; y < side; ++y)
                for (int x = 0; x < side * 2; ++x)
                {
                    const std::uint32_t wv = whole[static_cast<std::size_t>(y) * side * 2 + x];
                    const std::uint32_t hv = x < side ? l[static_cast<std::size_t>(y) * side + x]
                                                      : r[static_cast<std::size_t>(y) * side + x - side];
                    if (wv != hv) { if (!bad) { fx = x; fy = y; } ++bad; }
                }
            std::printf("  %-18s %d mismatching pixels (first at %d,%d)\n", vc.name, bad, fx, fy);
        }
        return 0;
    }

    // ------------------------------------------------------------------
    // BL-1243 (terrain variant families), RENDERING.md § Mountains, rivers
    // and terrain variety — "More tile sets".
    //   P17 The variant colouring: no two neighbours share an index (bar the
    //       wrap corner), and it is mask-blind (a pure function of the grid).
    //   P18 A wide plain and a forest at 48/96: the variant pass is pure,
    //       wrap-exact flat and oblique, chunk-seamless at 2x, and moves the
    //       bake; tile-to-tile variation across the plain rises.
    //   P19 A mountain run: the form's variants move it, purely, wrap-exact.
    //   P20 region_hash folds the family and the variant index.
    // Also (not checks): bake ms per tier with variants off vs on, and
    // variants_<subject>_<tier>[_off].png previews.
    // ------------------------------------------------------------------
    const auto variant_rows = [&](bool timing)
    {
        const auto land = static_cast<std::uint8_t>(bake_source::tile_class::land);
        // P17 — the colouring.
        {
            int conflicts = 0, corner = 0;
            for (int i = 0; i < static_cast<int>(src.variant.size()); ++i)
                for (int side = 0; side < 6; ++side)
                {
                    const hex_neighbors::coord nb = hex_neighbors::neighbour(i % src.gw, i / src.gw, side);
                    if (nb.gy < 0 || nb.gy >= src.gh)
                        continue;
                    const int j = nb.gy * src.gw + ((nb.gx % src.gw) + src.gw) % src.gw;
                    if (src.variant[i] == src.variant[j])
                    {
                        // The one place the raster colouring may fail: the
                        // last column against column 0 across the wrap.
                        const int ci = i % src.gw, cj = j % src.gw;
                        const bool wrap_pair = (ci == src.gw - 1 && cj == 0) || (ci == 0 && cj == src.gw - 1);
                        (wrap_pair ? corner : conflicts) += 1;
                    }
                }
            int hist[k_variant_count] = {};
            for (std::uint8_t v : src.variant)
                ++hist[v];
            std::printf("      variants: %d / %d / %d / %d tiles; %d neighbour pairs share one at the wrap seam\n",
                        hist[0], hist[1], hist[2], hist[3], corner / 2);
            check(conflicts == 0, "P17", "no two neighbours share a variant away from the wrap seam");
            const bake_source masked = prepare_source(w, home, /*reveal_all=*/false);
            check(masked.variant == src.variant, "P17",
                  "the variant colouring is mask-blind (a pure function of the grid)");
        }

        const int plain_i  = homogeneous_aim(src, terrain_cover::grass);
        const int forest_i = homogeneous_aim(src, terrain_cover::forest);
        int mtn_i = -1, mtn_links = -1;
        for (int i = 0; i < static_cast<int>(src.cls.size()); ++i)
            if (src.cls[i] == land && src.landform[i] == static_cast<std::uint8_t>(terrain_landform::mountain))
            {
                int links = 0;
                for (int s = 0; s < 6; ++s)
                    links += (src.lf_links[i] >> s) & 1;
                if (links > mtn_links) { mtn_links = links; mtn_i = i; }
            }
        check(plain_i >= 0 && forest_i >= 0 && mtn_i >= 0, "P18", "found a plain, a forest and a mountain run");

        const auto vwin = [&](int tile_idx, const geometry& gg, const bake_params& bp, int side,
                              int dpx, std::vector<std::uint32_t>& out, int wide = 1)
        {
            const int r = tile_idx / src.gw, c = tile_idx % src.gw;
            const double cx = 1.7320508075688772 * (c + ((r & 1) ? 0.5 : 0.0));
            const double cy = 1.5 * r - src.height[tile_idx] * gg.lift;
            const int x0 = static_cast<int>(cx * gg.s) - side / 2 + dpx;
            const int y0 = std::clamp(static_cast<int>((cy - gg.y_min) * gg.s) - side / 2,
                                      0, std::max(0, gg.H - side));
            out.assign(static_cast<std::size_t>(side) * side * wide, 0u);
            bake_region(src, gg, bp, x0, y0, side * wide, side, out.data());
        };
        bake_params v_off = p;
        v_off.variant_strength = 0.0f;
        const geometry g48v = make_geometry(hb.grid_width, hb.grid_height, 48.0);
        const geometry g96t = make_geometry(hb.grid_width, hb.grid_height, 96.0, k_tilt_sy);
        std::vector<std::uint32_t> a1, a2, a3, a4;
        struct subj { const char* name; int tile; const char* phase; };
        for (const subj& sj : { subj{ "plain", plain_i, "P18" }, subj{ "forest", forest_i, "P18" },
                                subj{ "mountain", mtn_i, "P19" } })
        {
            if (sj.tile < 0)
                continue;
            char what[160];
            vwin(sj.tile, g48v, p, 160, 0, a1);
            vwin(sj.tile, g48v, p, 160, 0, a2);
            vwin(sj.tile, g48v, p, 160, g48v.W, a3);
            vwin(sj.tile, g48v, v_off, 160, 0, a4);
            std::snprintf(what, sizeof what, "%s @48: variants bake byte-identical twice", sj.name);
            check(a1 == a2, sj.phase, what);
            std::snprintf(what, sizeof what, "%s @48: variants wrap byte-identical one period east", sj.name);
            check(a1 == a3, sj.phase, what);
            std::snprintf(what, sizeof what, "%s @48: the variant pass moves the bake", sj.name);
            check(a1 != a4, sj.phase, what);
            vwin(sj.tile, g96t, p, 160, 0, a1);
            vwin(sj.tile, g96t, p, 160, g96t.W, a3);
            std::snprintf(what, sizeof what, "%s @96 oblique 45: variants wrap byte-identical", sj.name);
            check(a1 == a3, sj.phase, what);
            // Chunk seam at 2x: one window two chunks wide equals its halves.
            {
                std::vector<std::uint32_t> whole, l, r;
                vwin(sj.tile, g48v, p, 128, 0, whole, 2);
                vwin(sj.tile, g48v, p, 128, 0, l);
                vwin(sj.tile, g48v, p, 128, 128, r);
                bool seam_ok = true;
                for (int y = 0; y < 128 && seam_ok; ++y)
                    for (int x = 0; x < 128; ++x)
                        if (whole[static_cast<std::size_t>(y) * 256 + x] != l[static_cast<std::size_t>(y) * 128 + x]
                            || whole[static_cast<std::size_t>(y) * 256 + 128 + x] != r[static_cast<std::size_t>(y) * 128 + x])
                        {
                            std::printf("      first seam mismatch at (%d,%d): whole %08x/%08x, halves %08x/%08x\n",
                                        x, y, whole[static_cast<std::size_t>(y) * 256 + x],
                                        whole[static_cast<std::size_t>(y) * 256 + 128 + x],
                                        l[static_cast<std::size_t>(y) * 128 + x], r[static_cast<std::size_t>(y) * 128 + x]);
                            seam_ok = false;
                            break;
                        }
                std::snprintf(what, sizeof what, "%s @48: two adjacent windows equal one spanning both (no chunk seam)", sj.name);
                check(seam_ok, sj.phase, what);
            }
        }

        // P18 — the anti-wallpaper reading. A wide plain reads as wallpaper
        // when every tile has the same CHARACTER — the same hue and the same
        // texture. Per interior plain tile (all six neighbours the same
        // cover: an edge tile mixes the next cover in through the warp), a
        // disc of radius 0.45 about the centre gives a character vector —
        // mean warm (r - b), mean green (g - (r + b) / 2), local contrast
        // (luminance SD) — off and on. Two things are read:
        //   * the spread of that character across the plain, off -> on (a
        //     reading: cover density already spreads hue tile to tile);
        //   * the CHECK: the variants are distinct characters, not noise.
        //     Grouping each tile's shift (on - off) by its variant index,
        //     the spread BETWEEN the group means must exceed the spread
        //     WITHIN a group — the variant index explains most of the shift.
        //     The within-group spread is not all noise: each variant's broad
        //     patches deliberately vary inside it, and the cross-fade carries
        //     some of each neighbour into a tile's disc. (First drafted at
        //     2x; measured 1.69x on the home body's plain — the bar was
        //     re-stated, not the tables tuned to it: the tables are set by eye.)
        // Read at the 24 px tier, where a 768 px window holds a plain's
        // worth of interior tiles.
        if (plain_i >= 0)
        {
            const geometry g24v = make_geometry(hb.grid_width, hb.grid_height, 24.0);
            const int side = 768;
            const int r0 = plain_i / src.gw, c0 = plain_i % src.gw;
            const double cx0 = 1.7320508075688772 * (c0 + ((r0 & 1) ? 0.5 : 0.0));
            const int x0 = static_cast<int>(cx0 * g24v.s) - side / 2;
            const int y0 = std::clamp(static_cast<int>((1.5 * r0 - g24v.y_min) * g24v.s) - side / 2,
                                      0, std::max(0, g24v.H - side));
            std::vector<std::uint32_t> on(static_cast<std::size_t>(side) * side), off(on.size());
            bake_region(src, g24v, p,     x0, y0, side, side, on.data());
            bake_region(src, g24v, v_off, x0, y0, side, side, off.data());
            struct tile_stat { double c[3]; int v; };
            std::vector<tile_stat> st_on, st_off;
            for (int r = 0; r < src.gh; ++r)
                for (int c = 0; c < src.gw; ++c)
                {
                    const std::size_t ti = static_cast<std::size_t>(r) * src.gw + c;
                    if (src.cls[ti] != land || src.cover[ti] != src.cover[plain_i])
                        continue;
                    bool interior = true;
                    for (int side_ = 0; side_ < 6 && interior; ++side_)
                    {
                        const hex_neighbors::coord nb = hex_neighbors::neighbour(c, r, side_);
                        if (nb.gy < 0 || nb.gy >= src.gh) { interior = false; break; }
                        const std::size_t nj = static_cast<std::size_t>(nb.gy) * src.gw
                                             + ((nb.gx % src.gw) + src.gw) % src.gw;
                        interior = src.cls[nj] == land && src.cover[nj] == src.cover[plain_i];
                    }
                    if (!interior)
                        continue;
                    const double tx = 1.7320508075688772 * (c + ((r & 1) ? 0.5 : 0.0)) * g24v.s - x0;
                    const double ty = (1.5 * r - g24v.y_min) * g24v.s - y0;
                    const double rad = 0.45 * g24v.s;
                    if (tx - rad < 0 || ty - rad < 0 || tx + rad >= side || ty + rad >= side)
                        continue;
                    for (int k = 0; k < 2; ++k)
                    {
                        const std::vector<std::uint32_t>& img = k ? on : off;
                        double sw = 0, sg = 0, sl = 0, sll = 0;
                        int np = 0;
                        for (int y = static_cast<int>(ty - rad); y <= static_cast<int>(ty + rad); ++y)
                            for (int x = static_cast<int>(tx - rad); x <= static_cast<int>(tx + rad); ++x)
                            {
                                if ((x - tx) * (x - tx) + (y - ty) * (y - ty) > rad * rad)
                                    continue;
                                const std::uint32_t cc = img[static_cast<std::size_t>(y) * side + x];
                                const double R = ui::palette::col_r(cc), G = ui::palette::col_g(cc),
                                             B = ui::palette::col_b(cc);
                                const double L = 0.2126 * R + 0.7152 * G + 0.0722 * B;
                                sw += R - B;
                                sg += G - 0.5 * (R + B);
                                sl += L; sll += L * L;
                                ++np;
                            }
                        const double ml = sl / np;
                        (k ? st_on : st_off).push_back(
                            { { sw / np, sg / np, std::sqrt(std::max(0.0, sll / np - ml * ml)) },
                              src.variant[ti] });
                    }
                }
            const auto spread = [](const std::vector<tile_stat>& v, int which) {
                double s = 0, ss = 0;
                for (const tile_stat& t : v) { s += t.c[which]; ss += t.c[which] * t.c[which]; }
                const double m = s / v.size();
                return std::sqrt(std::max(0.0, ss / v.size() - m * m));
            };
            if (st_on.size() >= 8)
            {
                const double chroma_off = std::hypot(spread(st_off, 0), spread(st_off, 1));
                const double chroma_on  = std::hypot(spread(st_on, 0),  spread(st_on, 1));
                std::printf("      plain @24: %zu interior tiles; per-tile chroma spread %.2f -> %.2f, "
                            "local-contrast spread %.2f -> %.2f (variants off -> on)\n",
                            st_on.size(), chroma_off, chroma_on, spread(st_off, 2), spread(st_on, 2));
                // Between- vs within-variant spread of the shift, over the
                // three character axes together.
                double gm[k_variant_count][3] = {};
                int gn[k_variant_count] = {};
                for (std::size_t t = 0; t < st_on.size(); ++t)
                {
                    ++gn[st_on[t].v];
                    for (int a = 0; a < 3; ++a)
                        gm[st_on[t].v][a] += st_on[t].c[a] - st_off[t].c[a];
                }
                double all[3] = {};
                int groups = 0;
                for (int v = 0; v < k_variant_count; ++v)
                    if (gn[v])
                    {
                        ++groups;
                        for (int a = 0; a < 3; ++a) { gm[v][a] /= gn[v]; all[a] += gm[v][a]; }
                    }
                for (int a = 0; a < 3; ++a) all[a] /= std::max(1, groups);
                double between = 0, within = 0;
                for (int v = 0; v < k_variant_count; ++v)
                    if (gn[v])
                        for (int a = 0; a < 3; ++a) between += (gm[v][a] - all[a]) * (gm[v][a] - all[a]);
                between = std::sqrt(between / std::max(1, groups));
                for (std::size_t t = 0; t < st_on.size(); ++t)
                    for (int a = 0; a < 3; ++a)
                    {
                        const double d = st_on[t].c[a] - st_off[t].c[a] - gm[st_on[t].v][a];
                        within += d * d;
                    }
                within = std::sqrt(within / st_on.size());
                std::printf("      plain @24: variant shift spread between variants %.2f, within a variant %.2f "
                            "(%d variants present)\n", between, within, groups);
                check(groups >= 3 && between > within, "P18",
                      "the plain's variants are distinct characters (between-variant spread > within)");
            }
            else
                std::printf("SKIP  P18: fewer than 8 interior plain tiles in the window\n");
        }

        // P20 — the hash folds family and variant.
        {
            const int r0 = plain_i >= 0 ? plain_i / src.gw : 2, c0 = plain_i >= 0 ? plain_i % src.gw : 2;
            const int x0 = static_cast<int>(1.7320508075688772 * c0 * g48v.s) - 256;
            const int y0 = std::max(0, static_cast<int>((1.5 * r0 - g48v.y_min) * g48v.s) - 256);
            const std::size_t ti = static_cast<std::size_t>(r0) * src.gw + c0;
            const std::uint64_t h0 = region_hash(src, g48v, x0, y0, 512, 512);
            bake_source m1 = src; m1.family[ti] ^= 1u;
            bake_source m2 = src; m2.variant[ti] = static_cast<std::uint8_t>((m2.variant[ti] + 1) % k_variant_count);
            check(region_hash(m1, g48v, x0, y0, 512, 512) != h0, "P20", "region_hash moves with a tile's variant family");
            check(region_hash(m2, g48v, x0, y0, 512, 512) != h0, "P20", "region_hash moves with a tile's variant index");
        }

        // Previews and timing (readings).
        {
            struct shot { const char* name; int tile; };
            std::vector<std::uint32_t> pb;
            for (const shot& sh : { shot{ "plain", plain_i }, shot{ "forest", forest_i }, shot{ "mountain", mtn_i } })
            {
                if (sh.tile < 0)
                    continue;
                for (double tp : { 24.0, 48.0, 96.0 })
                {
                    const geometry gp = make_geometry(hb.grid_width, hb.grid_height, tp);
                    vwin(sh.tile, gp, p, 512, 0, pb);
                    char path[128];
                    std::snprintf(path, sizeof path, "variants_%s_%d.png", sh.name, static_cast<int>(tp));
                    write_png_rgba(path, 512, 512, reinterpret_cast<const unsigned char*>(pb.data()), 512 * 4);
                    vwin(sh.tile, gp, v_off, 512, 0, pb);
                    std::snprintf(path, sizeof path, "variants_%s_%d_off.png", sh.name, static_cast<int>(tp));
                    write_png_rgba(path, 512, 512, reinterpret_cast<const unsigned char*>(pb.data()), 512 * 4);
                }
            }
            std::vector<std::uint32_t> tb(512u * 512u);
            const int aim = plain_i >= 0 ? plain_i : land_i;
            const int ar = aim / src.gw, ac = aim % src.gw;
            const double ax = 1.7320508075688772 * (ac + ((ar & 1) ? 0.5 : 0.0));
            for (double tp : { 6.0, 12.0, 24.0, 48.0, 96.0 })
            {
                if (!timing)
                    break;
                const geometry gt = make_geometry(hb.grid_width, hb.grid_height, tp);
                const int cw = std::min(512, gt.W), ch = std::min(512, gt.H);
                const int x0 = static_cast<int>(ax * gt.s) - cw / 2;
                const int y0 = std::clamp(static_cast<int>((1.5 * ar - gt.y_min) * gt.s) - ch / 2,
                                          0, std::max(0, gt.H - ch));
                double ms[2] = { 1e30, 1e30 };
                for (int rep = 0; rep < 3; ++rep) // best of 3, off/on interleaved: load cancels
                    for (int k = 0; k < 2; ++k)
                    {
                        const auto t0 = std::chrono::steady_clock::now();
                        bake_region(src, gt, k ? p : v_off, x0, y0, cw, ch, tb.data());
                        ms[k] = std::min(ms[k], std::chrono::duration<double, std::milli>(
                                                    std::chrono::steady_clock::now() - t0).count());
                    }
                std::printf("TIME  variants  tier %3.0f px/r  chunk %dx%d (plain)  off %8.1f ms  on %8.1f ms  (%+.0f%%)\n",
                            tp, cw, ch, ms[0], ms[1], 100.0 * (ms[1] / ms[0] - 1.0));
            }
        }
    };

    if (variants_only)
    {
        variant_rows(/*timing=*/false);
        std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "ALL PASS",
                    g_failures, g_failures == 1 ? "" : "s");
        return g_failures ? 1 : 0;
    }


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

        // P9 — the OBLIQUE bake (BL-737; BL-1246: the one angle, 22.5
        // degrees — the 45-degree tier is retired): the same window bakes
        // byte-identical twice and one wrap period east; and the projection
        // actually projects (the tilted window differs from the flat one).
        const geometry g45 = make_geometry(hb.grid_width, hb.grid_height, 48.0, k_tilt_sy);
        const int tpy0 = std::clamp(
            static_cast<int>((fy - g45.y_min) * g45.s) - side / 2, 0, std::max(0, g45.H - side));
        std::vector<std::uint32_t> t1(a48.size()), t2(a48.size()), t3(a48.size());
        bake_region(src, g45, p, px0, tpy0, side, side, t1.data());
        bake_region(src, g45, p, px0, tpy0, side, side, t2.data());
        bake_region(src, g45, p, px0 + g45.W, tpy0, side, side, t3.data());
        check(t1 == t2, "P9", "22.5-degree oblique bake is byte-identical twice");
        check(t1 == t3, "P9", "22.5-degree oblique bake wraps byte-identical one period east");
        check(g45.lift > 0.35 && g45.lift < 0.40, "P9", "22.5-degree lift derives to 0.9 tan(22.5) ~ 0.373 canonical");
        check(t1 != a48, "P9", "the oblique projection differs from the flat bake");
    }

    // P10 retired with the per-rung tier ladder (BL-1246, one master): the
    // never-magnify chooser it pinned is gone. The one-master chooser, the
    // master's alignment and the chunkwise mip chain are P22.
    master_row(src, hb, land_i, p);
    lock_fast_row(w, home, p);

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

    // P21 - the worker pool: concurrent bakes equal serial ones.
    pool_row(src, hb, land_i, p);

    // P12 — bake cost per tier (a reading, not a check): one 512 px chunk per
    // tier at 1x and 2x, wall-clock ms, so R3's before/after has a number
    // independent of the app's viewport. The window is centred on land.
    {
        const int lr = land_i / src.gw, lc = land_i % src.gw;
        const double lx = 1.7320508075688772 * (lc + ((lr & 1) ? 0.5 : 0.0));
        const double ladder[] = { k_far_ppr, 12.0, 24.0, 48.0, 96.0 };
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
        const geometry g45f = make_geometry(hb.grid_width, hb.grid_height, 96.0, k_tilt_sy);
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

    variant_rows(/*timing=*/true);

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
                               { 96.0, k_tilt_sy, "96@22.5" } };
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

    // P24 / P25 - the partial re-bake (BL-1246).
    patch_row(src, p);

    // P26 / P27 - tiles hold their own ground (BL-1251).
    border_row(src, hb, p, /*previews=*/false);

    std::printf("\n%s (%d failure%s)\n", g_failures ? "FAILED" : "ALL PASS",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
