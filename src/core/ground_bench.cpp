// ---------------------------------------------------------------------------
// BL-1247 — the ground benchmark (`ProjectIo --bench-ground`).
//
// TECH_FOUNDATIONS.md § Target hardware sets five budgets the minimum PC must
// hold; this reports them, on the machine it runs on, from the LIVE app (not
// --verify, whose ground bakes what each frame draws synchronously and so
// times the wrong thing):
//
//   [1] the home body's pre-bake time ADDED to generation — the "Painting the
//       ground" wait at the seat — budget 15 s, for a player who reads the
//       wizard's rounds (the walk dwells --dwell ms on every lapse round and
//       before Begin; --dwell 0 is the click-straight-through case, which the
//       doc tracks with no budget);
//   [2] whether any zoom or pan step on the home body shows a stand-in — a
//       frame whose drawn level is not wholly final under the view (a master
//       chunk not landed or current, or a visible chunk not yet uploaded) —
//       budget none;
//   [3] the time until a first visit to another body is sharp under the view
//       — budget 2 s (background bakes are held back, so it IS a first visit);
//   [4] frame time at every rung, during a sustained pan — budget 16.7 ms, the
//       frame's own work (frame_sample::work_ms, wall clock less the vsync
//       wait), judged on the 1%-low (the mean of the slowest 1%);
//   [5] the time from a building placed to its ground re-baked and on screen
//       — budget 1 s.
//
// It also prints the ground's TOTAL paint wall time (pre-bake start -> the home
// master complete and current), the bake's per-chunk cost, and the process's
// memory as the OS counts it (working set and private bytes, now and peak) at
// each phase. The sim is paused in play so the readings are the ground's.
// Prints "BENCH_GROUND" lines, then exits. Not a skill (CLAUDE.md § Tool
// creation is skill creation: wrapping it needs Ben's permission).
// ---------------------------------------------------------------------------

#include "app.hpp"
#include "process_memory.hpp"

#include "ui/body_surface_canvas.hpp" // planetary_zoom_stepped (the x2 ladder)
#include "ui/frame_stats.hpp"
#include "ui/view_nav.hpp"
#include "world/construction.hpp"
#include "world/logistics.hpp"
#include "world/placement_rules.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <unordered_set>

namespace {

using bclock = std::chrono::steady_clock;

double secs_since(bclock::time_point t0)
{
    return std::chrono::duration<double>(bclock::now() - t0).count();
}

const char* verdict(bool pass) { return pass ? "PASS" : "FAIL"; }

constexpr double k_gib = 1073741824.0;

// Frames per step. A zoom step is watched for k_zoom_frames after the rung
// changes; a pan runs k_pan_frames at k_pan_px a frame (the scripted bench's
// -24 px, ground_fill_bench.lua).
constexpr int    k_zoom_frames = 60;
constexpr int    k_pan_frames  = 300;
constexpr float  k_pan_px      = -24.0f;
constexpr double k_settle_s    = 120.0; ///< A view that is not final this long is reported, not waited on.
constexpr double k_rebake_s    = 60.0;
constexpr double k_visit_s     = 120.0;
constexpr double k_master_s    = 300.0;

} // namespace

void app::bench_mem(const char* label)
{
    const io_mem::process_memory m = io_mem::query();
    const ground_layer::stats s = m_ground.stats_snapshot();
    m_bench.mem.push_back({ label, m.working_set, m.peak_working_set, m.private_bytes, m.peak_private,
                            static_cast<double>(s.ram_total) });
    std::printf("BENCH_GROUND memory @ %-22s working set %.2f GB (peak %.2f)  private %.2f GB (peak %.2f)"
                "  ground RAM %.2f GB\n",
                label, m.working_set / k_gib, m.peak_working_set / k_gib, m.private_bytes / k_gib,
                m.peak_private / k_gib, s.ram_total / k_gib);
    // BL-1311: the fixed ground budget and how the chunks are held.
    std::printf("BENCH_GROUND ground @ %-22s budget %.2f GB (others' share now %.2f GB); chunks held RGB %d "
                "(%.2f GB) / RGBA %d (%.2f GB)\n",
                label, s.ground_budget / k_gib, s.others_budget / k_gib, s.rgb_chunks, s.rgb_bytes / k_gib,
                s.rgba_chunks, s.rgba_bytes / k_gib);
    std::fflush(stdout);
}

void app::bench_ground_frame()
{
    if (!m_bench.on || m_bench.done)
        return;
    bench_state& B = m_bench;
    const auto now = bclock::now();

    // --- Before play: the pre-bake and the seat wait ----------------------
    if (!B.in_play)
    {
        const entity_id target = m_ground.prebake_target();
        if (target != null_entity)
        {
            const auto t = m_ground.prebake_started();
            if (t != B.prebake_t0)
            {
                B.prebake_t0 = t;
                ++B.prebake_starts;
                B.was_current = false;
                bench_mem("pre-bake start");
            }
            const bool cur = m_ground.master_current(target);
            if (cur && !B.was_current)
                B.current_since = now;
            B.was_current = cur;
            static bclock::time_point last_line{};
            if (secs_since(last_line) > 10.0)
            {
                last_line = now;
                int ready = 0, total = 0;
                m_ground.master_progress(target, ready, total);
                std::printf("BENCH_GROUND painting: %d / %d master chunks after %.1f s\n", ready, total,
                            secs_since(B.prebake_t0));
                std::fflush(stdout);
            }
        }
        if (m_screen == app_screen::painting_ground && B.paint_wait_t0 == bclock::time_point{})
            B.paint_wait_t0 = now;
        if (m_screen != app_screen::in_game)
            return;
        // Play has opened.
        B.in_play = true;
        B.seat_wait_s = B.paint_wait_t0 == bclock::time_point{} ? 0.0
                      : std::chrono::duration<double>(now - B.paint_wait_t0).count();
        if (B.prebake_t0 != bclock::time_point{} && B.was_current)
            B.paint_total_s = std::chrono::duration<double>(B.current_since - B.prebake_t0).count();
        const ground_layer::stats s = m_ground.stats_snapshot();
        std::printf("BENCH_GROUND play opened: seat wait %.1f s; ground paint wall time %.1f s "
                    "(pre-bake start -> home master current; %d pre-bake start(s))\n",
                    B.seat_wait_s, B.paint_total_s, B.prebake_starts);
        std::printf("BENCH_GROUND bake CPU: %.1f s over %d master jobs (%.1f ms each), %.1f s of it partial "
                    "re-bakes (thread CPU time, which other processes on the machine do not inflate)\n",
                    s.cpu_ms / 1000.0, s.bakes[1], s.bakes[1] ? s.cpu_ms / s.bakes[1] : 0.0,
                    s.patch_cpu_ms / 1000.0);
        std::printf("BENCH_GROUND per chunk: %d master chunk bakes, %.1f ms worker time each "
                    "(mip levels %.1f ms each); far page %d pieces, %.1f ms each; "
                    "one whole master %.2f GB\n",
                    s.bakes[1], s.bakes[1] ? s.bake_ms[1] / s.bakes[1] : 0.0,
                    s.bakes[1] ? (s.bake_ms[2] + s.bake_ms[3] + s.bake_ms[4] + s.bake_ms[5]) / s.bakes[1] : 0.0,
                    s.bakes[0], s.bakes[0] ? s.bake_ms[0] / s.bakes[0] : 0.0,
                    m_ground.master_size(m_world.home_body) / k_gib);
        {
            // BL-1311: where the master jobs' worker time went, per pass.
            static const char* const names[ground_layer::stats::k_pass_slots] = {
                "base field", "edge ink", "trees", "landforms+rivers", "installations+routes",
                "cast shadows", "grade", "unsharp", "re-bake window rule", "content hash", "mip chain"
            };
            double total = 0.0;
            for (const double v : s.pass_ms)
                total += v;
            std::printf("BENCH_GROUND worker time by pass (master jobs, %.1f s in all):", total / 1000.0);
            for (int k = 0; k < ground_layer::stats::k_pass_slots; ++k)
                std::printf("%s %s %.1f s (%.0f%%)", k ? "," : "", names[k], s.pass_ms[k] / 1000.0,
                            total > 0.0 ? 100.0 * s.pass_ms[k] / total : 0.0);
            std::printf("\n");
            std::printf("BENCH_GROUND partial re-bakes: %d chunks (%d with nothing to bake), %.1f s worker "
                        "(%.1f ms each):", s.patch_jobs, s.patch_empty, s.patch_ms / 1000.0,
                        s.patch_jobs ? s.patch_ms / s.patch_jobs : 0.0);
            for (int k = 0; k < ground_layer::stats::k_pass_slots - 1; ++k)
                std::printf("%s %s %.1f s", k ? "," : "", names[k], s.patch_pass_ms[k] / 1000.0);
            std::printf("\n");
        }
        std::fflush(stdout);
        bench_mem("play open");
        {
            // The main thread re-takes the drawn body's source snapshot every
            // 30 frames (RENDERING.md § Chunks, cache and invalidation): its
            // cost lands on one frame in thirty, so it is [4]'s first suspect.
            const auto ts = bclock::now();
            const ui::ground::bake_source snap =
                ui::ground::prepare_source(m_world, m_world.home_body, false, &m_registry);
            std::printf("BENCH_GROUND source snapshot of the home body (main thread, every 30 frames): "
                        "%.1f ms (%d tiles)\n", secs_since(ts) * 1000.0, snap.gw * snap.gh);
        }

        // Set the stage: the sim held still (the readings are the ground's), no
        // speculative bakes (a first visit must be one), the home surface.
        m_sim_loop.set_speed(0);
        m_ground.background_enabled = false;
        B.home  = m_world.home_body;
        B.zoom0 = ui::planetary_zoom_stepped(0.0f, -1); // the ladder's bottom rung
        ui::focus_on_surface(m_world, m_ui, B.home);
        m_ui.planetary_zoom = B.zoom0;
        B.phase = 1;
        B.frames = 0;
        B.t0 = now;
        return;
    }

    const bool final_ = m_ground.view_final();
    const auto watch = [&](bool zoom_step) {
        ++B.watched_frames;
        if (final_)
        {
            B.standin_run = 0;
            return;
        }
        ++B.standin_frames;
        ++(zoom_step ? B.standin_zoom : B.standin_pan);
        B.standin_worst_run = std::max(B.standin_worst_run, ++B.standin_run);
    };
    const auto set_rung = [&](int k) {
        m_ui.planetary_zoom = std::min(B.zoom0 * static_cast<float>(1 << k),
                                       ui::planetary_zoom_stepped(1.0e6f, +1));
    };
    ++B.frames;

    switch (B.phase)
    {
    case 1: // Settle on the whole home body (rung 0).
        if (final_ || secs_since(B.t0) > k_settle_s)
        {
            if (!final_)
                std::printf("BENCH_GROUND home view not final after %.0f s at rung 0\n", k_settle_s);
            B.phase = 2;
            B.step = 0;
            B.frames = 0;
            frame_stats_instance().reset();
        }
        break;

    case 2: // Up the ladder: each rung a zoom step, then a sustained pan.
    {
        const int k = B.step;
        if (B.frames == 1 && k > 0)
            set_rung(k);
        if (B.frames <= k_zoom_frames)
        {
            watch(/*zoom_step=*/true);
            break;
        }
        // The pan. The previous frame's work, read now it is closed.
        if (B.frames > k_zoom_frames + 1)
            B.rung_ms[k].push_back(frame_stats_instance().last().work_ms());
        watch(/*zoom_step=*/false);
        B.rung_level_ppr[k] = m_ui.ground.level_ppr;
        if (B.frames <= k_zoom_frames + k_pan_frames)
        {
            m_ui.planetary_pan_x += k_pan_px;
            break;
        }
        std::sort(B.rung_ms[k].begin(), B.rung_ms[k].end());
        std::printf("BENCH_GROUND rung %d done (zoom %.2f, drawn hex radius %.1f px, level %.0f px/hex): "
                    "stand-in frames so far %d\n", k, m_ui.planetary_zoom, m_ui.ground_req.draw_r,
                    B.rung_level_ppr[k], B.standin_frames);
        std::fflush(stdout);
        B.frames = 0;
        if (++B.step >= 5)
        {
            B.phase = 3;
            B.step = 3;
        }
        break;
    }

    case 3: // Back down the ladder, a zoom step at a time.
        if (B.frames == 1)
            set_rung(B.step);
        watch(/*zoom_step=*/true);
        if (B.frames >= k_zoom_frames)
        {
            B.frames = 0;
            if (--B.step < 0)
            {
                std::printf("BENCH_GROUND zoom/pan done: %d stand-in frame(s) of %d watched\n",
                            B.standin_frames, B.watched_frames);
                std::fflush(stdout);
                bench_mem("after zoom/pan");
                // Frame the whole body again for the re-bake.
                ui::focus_on_surface(m_world, m_ui, B.home);
                m_ui.planetary_zoom = B.zoom0;
                B.phase = 4;
                B.t0 = now;
            }
        }
        break;

    case 4: // Place a building; time its ground re-baked and on screen.
        if (!final_ && secs_since(B.t0) < k_settle_s)
            break;
        {
            // The same placement the verify harness's build_first_valid makes
            // (construct_building, the interactive path's seam): the first
            // valid unoccupied tile of the home body for an iron extraction
            // site, funds topped up.
            const auto cit = m_world.corporations.find(m_world.player_entity);
            if (cit != m_world.corporations.end())
                cit->second.balance = std::max(cit->second.balance, 1.0e6f);
            std::unordered_set<entity_id> occupied;
            for (const auto& [bid, bc] : m_world.buildings)
                occupied.insert(bc.tile);
            body_reach_field(m_world, B.home);
            entity_id placed = null_entity;
            for (const auto& [tid, tc] : m_world.tiles)
            {
                if (tc.body != B.home || occupied.count(tid))
                    continue;
                if (!placement_rules::can_place_in_world(m_world, tid, building_type::extraction_site,
                                                         resource_type::iron_ore,
                                                         m_registry.construction().max_logistics_reach))
                    continue;
                entity_id built = null_entity;
                if (construct_building(m_world, m_registry, m_world.player_entity, tid,
                                       building_type::extraction_site, resource_type::iron_ore, built)
                    == construction_result::placed)
                {
                    placed = built;
                    std::printf("BENCH_GROUND placed an extraction site at tile (%d,%d)\n", tc.grid_x,
                                tc.grid_y);
                    break;
                }
            }
            if (placed == null_entity)
            {
                B.rebake_note = "no tile accepted the placement";
                B.phase = 6;
                B.frames = 0;
                break;
            }
            m_ground.touch(); // as the live construction path does
            B.rebakes0 = m_ground.bake_counters().chunk_rebakes;
            B.t0 = bclock::now();
            B.phase = 5;
        }
        break;

    case 5:
        if (m_ground.bake_counters().chunk_rebakes > B.rebakes0 && final_)
        {
            B.rebake_s = secs_since(B.t0);
            std::printf("BENCH_GROUND building placed -> ground re-baked and on screen: %.3f s\n",
                        B.rebake_s);
            std::fflush(stdout);
            B.phase = 6;
            B.frames = 0;
        }
        else if (secs_since(B.t0) > k_rebake_s)
        {
            B.rebake_note = m_ground.bake_counters().chunk_rebakes > B.rebakes0
                          ? "re-baked, but the view never went final"
                          : "no chunk re-baked (the placement moved no chunk hash)";
            B.phase = 6;
            B.frames = 0;
        }
        break;

    case 6: // A first visit to another body.
        if (B.frames == 1)
        {
            // The nearest body to the home body's orbit with a surface (the
            // order the background bakes would take it in).
            const auto orbit_of = [this](entity_id id) {
                const auto it = m_world.bodies.find(id);
                if (it == m_world.bodies.end())
                    return 0.0;
                const auto& bc = it->second;
                if (bc.parent != null_entity && m_world.bodies.count(bc.parent))
                    return static_cast<double>(m_world.bodies.at(bc.parent).orbital_radius_au);
                return static_cast<double>(bc.orbital_radius_au);
            };
            const double home_au = orbit_of(B.home);
            double best = 1e300;
            for (const auto& [id, bc] : m_world.bodies)
            {
                if (id == B.home || bc.grid_width <= 0 || bc.grid_height <= 0)
                    continue;
                const double d = std::fabs(orbit_of(id) - home_au);
                if (d < best || (d == best && id < B.other))
                {
                    best = d;
                    B.other = id;
                    B.other_name = bc.name;
                }
            }
            if (B.other == null_entity)
            {
                B.phase = 8;
                break;
            }
            ui::focus_on_surface(m_world, m_ui, B.other);
            B.t0 = bclock::now();
            break;
        }
        if (m_ui.active_body == B.other && final_)
        {
            B.visit_s = secs_since(B.t0);
            std::printf("BENCH_GROUND first visit to %s: sharp under the view after %.3f s\n",
                        B.other_name.c_str(), B.visit_s);
            std::fflush(stdout);
            B.phase = 7;
        }
        else if (secs_since(B.t0) > k_visit_s)
            B.phase = 7;
        break;

    case 7: // ...and its whole master, for the record (and the memory).
        if (m_ground.master_complete(B.other) || secs_since(B.t0) > k_master_s)
        {
            if (m_ground.master_complete(B.other))
                B.visit_master_s = secs_since(B.t0);
            std::printf("BENCH_GROUND %s's whole master: %s\n", B.other_name.c_str(),
                        B.visit_master_s >= 0 ? "complete" : "not complete within the cap");
            bench_mem("after first visit");
            B.phase = 8;
            B.frames = 0;
            B.t0 = bclock::now();
            if (B.background_s > 0.0)
            {
                ui::focus_on_surface(m_world, m_ui, B.home);
                m_ground.background_enabled = true;
            }
        }
        break;

    case 8: // Optionally, the background bakes (the steady-state memory).
        if (B.background_s > 0.0 && secs_since(B.t0) < B.background_s)
            break;
        if (B.background_s > 0.0)
            bench_mem("after background bakes");
        bench_report();
        B.done = true;
        m_quit_requested = true;
        break;

    default:
        break;
    }
}

void app::bench_report()
{
    bench_state& B = m_bench;
#ifdef NDEBUG
    const char* build = "Release";
#else
    const char* build = "Debug";
#endif
    int ww = 0, wh = 0;
    SDL_GetWindowSize(m_window, &ww, &wh);
    std::printf("\nBENCH_GROUND ===== the ground benchmark (TECH_FOUNDATIONS.md § Target hardware) =====\n");
    std::printf("BENCH_GROUND build %s  window %dx%d  seed %u  dwell %lld ms/round  hardware threads %u  RAM %d MB\n",
                build, ww, wh, B.seed, B.dwell_ms, std::thread::hardware_concurrency(), SDL_GetSystemRAM());
    int pass = 0;

    // [1] The pre-bake added to generation.
    {
        const bool tracked_only = B.dwell_ms == 0;
        const bool ok = B.seat_wait_s <= 15.0;
        pass += ok;
        std::printf("BENCH_GROUND [1] pre-bake added to generation (the seat wait): %.1f s  budget 15 s  %s%s\n",
                    B.seat_wait_s, verdict(ok),
                    tracked_only ? "  (--dwell 0: the click-through case, tracked with no budget)" : "");
        std::printf("BENCH_GROUND     total ground paint wall time (pre-bake start -> home master current): %.1f s\n",
                    B.paint_total_s);
    }
    // [2] Stand-ins on the home body.
    {
        const bool ok = B.standin_frames == 0 && B.watched_frames > 0;
        pass += ok;
        std::printf("BENCH_GROUND [2] stand-in frames on home zoom/pan: %d of %d (zoom steps %d, pans %d; "
                    "longest run %d)  budget none  %s\n",
                    B.standin_frames, B.watched_frames, B.standin_zoom, B.standin_pan,
                    B.standin_worst_run, verdict(ok));
    }
    // [3] The first visit.
    {
        const bool ok = B.visit_s >= 0.0 && B.visit_s <= 2.0;
        pass += ok;
        if (B.visit_s >= 0.0)
            std::printf("BENCH_GROUND [3] first visit to another body (%s) sharp under the view: %.2f s  "
                        "budget 2 s  %s\n", B.other_name.c_str(), B.visit_s, verdict(ok));
        else
            std::printf("BENCH_GROUND [3] first visit to another body (%s): not sharp within %.0f s  "
                        "budget 2 s  FAIL\n", B.other_name.c_str(), k_visit_s);
        if (B.visit_master_s >= 0.0)
            std::printf("BENCH_GROUND     (its whole master: %.1f s from the visit)\n", B.visit_master_s);
    }
    // [4] Frame time per rung.
    {
        bool ok = true;
        for (int k = 0; k < 5; ++k)
        {
            const std::vector<float>& v = B.rung_ms[k];
            if (v.empty())
            {
                ok = false;
                std::printf("BENCH_GROUND [4] rung %d: no frames\n", k);
                continue;
            }
            double sum = 0.0;
            for (const float f : v)
                sum += f;
            const std::size_t n1 = std::max<std::size_t>(1, v.size() / 100);
            double low = 0.0;
            for (std::size_t i = v.size() - n1; i < v.size(); ++i)
                low += v[i];
            low /= static_cast<double>(n1);
            const float p95 = v[std::min(v.size() - 1, v.size() * 95 / 100)];
            const bool r_ok = low <= 16.7;
            ok = ok && r_ok;
            std::printf("BENCH_GROUND [4] rung %d (level %3.0f px/hex): frame work avg %.2f  p95 %.2f  "
                        "1%%-low %.2f  max %.2f ms over %zu panning frames  %s\n",
                        k, B.rung_level_ppr[k], sum / v.size(), p95, low, v.back(), v.size(),
                        verdict(r_ok));
        }
        pass += ok;
        std::printf("BENCH_GROUND [4] frame time at every rung (1%%-low frame work)  budget 16.7 ms  %s\n",
                    verdict(ok));
    }
    // [5] The re-bake.
    {
        const bool ok = B.rebake_s >= 0.0 && B.rebake_s <= 1.0;
        pass += ok;
        if (B.rebake_s >= 0.0)
            std::printf("BENCH_GROUND [5] building placed -> its ground re-baked: %.3f s  budget 1 s  %s\n",
                        B.rebake_s, verdict(ok));
        else
            std::printf("BENCH_GROUND [5] building placed -> its ground re-baked: %s  budget 1 s  FAIL\n",
                        B.rebake_note.c_str());
    }
    // Memory.
    bench_mem("end");
    double peak_ws = 0, peak_priv = 0, ground_max = 0;
    for (const auto& r : B.mem)
    {
        peak_ws   = std::max(peak_ws, r.peak_ws);
        peak_priv = std::max(peak_priv, r.peak_priv);
        ground_max = std::max(ground_max, r.ground);
    }
    std::printf("BENCH_GROUND PEAK process memory: working set %.2f GB, private bytes %.2f GB "
                "(ground RAM at most %.2f GB at a reading)\n",
                peak_ws / k_gib, peak_priv / k_gib, ground_max / k_gib);
    std::printf("BENCH_GROUND RESULT %d of 5 budgets PASS\n", pass);
    std::fflush(stdout);
}
