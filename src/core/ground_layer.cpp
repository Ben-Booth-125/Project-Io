#include "ground_layer.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr double kSqrt3 = 1.7320508075688772;

/// An env flag read once (main thread): set and not "0".
bool env_flag(const char* name)
{
    const char* v = SDL_getenv(name);
    return v && *v && std::strcmp(v, "0") != 0;
}
bool fill_log_on()
{
    static const bool on = env_flag("IO_GROUND_FILL_LOG") || env_flag("IO_GROUND_BENCH");
    return on;
}
bool bench_async_on()
{
    static const bool on = env_flag("IO_GROUND_BENCH");
    return on;
}
}

constexpr double      ground_layer::k_tier_ppr[];
constexpr std::size_t ground_layer::k_tier_cap[];
constexpr int         ground_layer::k_tier_supersample[];

ground_layer::~ground_layer()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// Worker pool
// ---------------------------------------------------------------------------

int ground_layer::pool_size()
{
    static const int n = [] {
        if (const char* v = SDL_getenv("IO_GROUND_WORKERS"))
        {
            const int k = std::atoi(v);
            if (k > 0)
                return std::min(k, 64);
        }
        // Leave the main/sim thread and one spare.
        const int hc = static_cast<int>(std::thread::hardware_concurrency());
        return std::max(1, hc - 2);
    }();
    return n;
}

void ground_layer::worker_main()
{
    // Below-normal priority: a full-viewport fill saturates every worker, and
    // the simulation and render threads must never be starved by it.
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    for (;;)
    {
        job j;
        {
            std::unique_lock lk(m_mx);
            m_cv.wait(lk, [&] { return m_quit || !m_jobs.empty(); });
            if (m_quit)
                return;
            // The lowest (prio, seq): the far page, then the neighbourhood
            // page, then chunks nearest the viewport centre, FIFO among equals.
            // The waiting set is capped near the pool size, so a scan is cheap.
            auto best = m_jobs.begin();
            for (auto it = m_jobs.begin() + 1; it != m_jobs.end(); ++it)
                if (it->prio < best->prio || (it->prio == best->prio && it->seq < best->seq))
                    best = it;
            j = std::move(*best);
            if (best != m_jobs.end() - 1)
                *best = std::move(m_jobs.back());
            m_jobs.pop_back();
        }
        result d;
        d.tier = j.tier; d.ci = j.ci; d.cj = j.cj; d.pw = j.pw; d.ph = j.ph;
        d.hash = j.hash; d.gen = j.gen;
        d.neigh_tile = j.neigh_tile;
        std::copy(std::begin(j.neigh_rect), std::end(j.neigh_rect), std::begin(d.neigh_rect));
        d.px.assign(static_cast<std::size_t>(j.pw) * j.ph, 0u);
        const auto t0 = std::chrono::steady_clock::now();
        ui::ground::bake_region(*j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph, d.px.data());
        const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        {
            std::lock_guard lk(m_mx);
            note_bake(j.tier, ms);
            m_results.push_back(std::move(d));
            --m_inflight; // counted from enqueue until the result LANDS — a popped job is still in flight
        }
    }
}

void ground_layer::enqueue(job j)
{
    {
        std::lock_guard lk(m_mx);
        j.seq = m_seq++;
        m_jobs.push_back(std::move(j));
        ++m_inflight;
    }
    if (m_workers.empty())
    {
        const int n = pool_size();
        m_workers.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            m_workers.emplace_back([this] { worker_main(); });
    }
    m_cv.notify_one();
}

void ground_layer::purge_waiting_except(int keep)
{
    std::vector<job> dropped;
    {
        std::lock_guard lk(m_mx);
        auto split = std::partition(m_jobs.begin(), m_jobs.end(), [this, keep](const job& j) {
            return j.tier < 0 || (j.tier == keep && j.gen == m_gen);
        });
        dropped.assign(std::make_move_iterator(split), std::make_move_iterator(m_jobs.end()));
        m_jobs.erase(split, m_jobs.end());
        m_inflight -= static_cast<int>(dropped.size());
    }
    // These never reach a worker: their slots become re-enqueueable now.
    for (const job& j : dropped)
    {
        tier_state& t = m_tiers[j.tier];
        const auto it = t.chunks.find(static_cast<std::uint32_t>(j.cj) * t.cw + j.ci);
        if (it != t.chunks.end() && it->second.job_gen == j.gen)
            it->second.queued = false;
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ground_layer::reset(entity_id body, const world& w)
{
    // Abandon queued work and orphan in-flight results via the generation bump;
    // the worker itself keeps running. Jobs cleared HERE never reach the worker,
    // so their in-flight counts are settled now — only the one it may already
    // hold keeps its count until its result lands.
    {
        std::lock_guard lk(m_mx);
        m_inflight -= static_cast<int>(m_jobs.size());
        m_jobs.clear();
        m_results.clear();
    }
    ++m_gen;
    for (tier_state& t : m_tiers)
    {
        for (auto& [k, c] : t.chunks)
            if (c.tex)
                SDL_DestroyTexture(c.tex);
        t.chunks.clear();
        t.cw = t.ch = 0;
    }
    if (m_far)
    {
        SDL_DestroyTexture(m_far);
        m_far = nullptr;
    }
    m_far_ready = m_far_queued = false;
    if (m_neigh)
    {
        SDL_DestroyTexture(m_neigh);
        m_neigh = nullptr;
    }
    m_neigh_ready = m_neigh_queued = false;
    m_neigh_tile = null_entity;
    m_neigh_want = 0;
    m_active_tier = -1;
    m_standin_tier = -1;
    m_body = body;
    m_src.reset();
    if (body == null_entity || !w.bodies.count(body))
        return;
    const body_component& b = w.bodies.at(body);
    if (b.grid_width <= 0 || b.grid_height <= 0)
        return; // a gridless body (the star) has no ground to bake
    m_far_geom = ui::ground::make_geometry(b.grid_width, b.grid_height, k_far_px_per_r);
    m_neigh_geom = ui::ground::make_geometry(b.grid_width, b.grid_height, k_neigh_px_per_r);
    for (int t = 0; t < k_tiers; ++t)
    {
        m_tiers[t].geom_ppr = k_tier_ppr[t];
        m_tiers[t].baked_sy = 1.0;
        m_tiers[t].geom = ui::ground::make_geometry(b.grid_width, b.grid_height, k_tier_ppr[t]);
        m_tiers[t].cw = (m_tiers[t].geom.W + k_chunk_px - 1) / k_chunk_px;
        m_tiers[t].ch = (m_tiers[t].geom.H + k_chunk_px - 1) / k_chunk_px;
    }
    refresh_source(w);
}

void ground_layer::refresh_source(const world& w)
{
    m_src = std::make_shared<const ui::ground::bake_source>(
        ui::ground::prepare_source(w, m_body, /*reveal_all=*/false, registry));
    m_src_age = 0;
    ++m_gen; // in-flight results against the old source are stale
}

void ground_layer::shutdown()
{
    if (!m_workers.empty())
    {
        {
            std::lock_guard lk(m_mx);
            m_quit = true;
            m_jobs.clear();
        }
        m_cv.notify_all();
        for (std::thread& t : m_workers)
            t.join();
        m_workers.clear();
        m_quit = false;
    }
    m_inflight = 0;
    m_results.clear();
    for (tier_state& t : m_tiers)
    {
        for (auto& [k, c] : t.chunks)
            if (c.tex)
                SDL_DestroyTexture(c.tex);
        t.chunks.clear();
    }
    if (m_far)
    {
        SDL_DestroyTexture(m_far);
        m_far = nullptr;
    }
    m_far_ready = m_far_queued = false;
    if (m_neigh)
    {
        SDL_DestroyTexture(m_neigh);
        m_neigh = nullptr;
    }
    m_neigh_ready = m_neigh_queued = false;
    m_body = null_entity;
    m_src.reset();
}

// ---------------------------------------------------------------------------
// Jobs, uploads, eviction
// ---------------------------------------------------------------------------

ground_layer::job ground_layer::make_chunk_job(int tier, int ci, int cj) const
{
    const tier_state& t = m_tiers[tier];
    job j;
    j.tier = tier; j.ci = ci; j.cj = cj;
    j.px0 = ci * k_chunk_px;
    j.py0 = cj * k_chunk_px;
    j.pw  = std::min(k_chunk_px, t.geom.W - j.px0);
    j.ph  = std::min(k_chunk_px, t.geom.H - j.py0);
    j.gen = m_gen;
    j.src = m_src;
    j.geom = t.geom;
    j.prm  = params;
    j.prm.supersample = std::min(params.supersample, k_tier_supersample[tier]);
    j.hash = ui::ground::region_hash(*m_src, t.geom, j.px0, j.py0, j.pw, j.ph);
    return j;
}

void ground_layer::upload(SDL_Renderer* r, const result& d, bool sync)
{
    // THE PENDING FLAG CLEARS FIRST, UNCONDITIONALLY. Only one job per slot can
    // be outstanding (the queued guards), so the arriving result IS that job —
    // whatever happens next (stale generation, a failed texture allocation),
    // the slot must become re-enqueueable or it is bricked until a body switch.
    // The review fleet confirmed both wedge paths (2026-09-01): a generation
    // bump mid-bake orphaning the result, and SDL_CreateTexture returning null.
    if (d.tier == -2)
    {
        // The neighbourhood page (BL-1241). Same flag discipline as the far
        // page: the queued flag clears first, whatever happens next — but only
        // for the job the flag stands for (the pool: an orphan from a body
        // switch can land after its slot's next job was enqueued).
        if (!sync && (!m_neigh_queued || d.gen != m_neigh_job_gen))
            return;
        m_neigh_queued = false;
        if (d.gen != m_gen)
        {
            m_neigh_want = 0; // re-requested next tick against the live source
            return;
        }
        if (m_neigh && (m_neigh_w != d.pw || m_neigh_h != d.ph))
        {
            SDL_DestroyTexture(m_neigh);
            m_neigh = nullptr;
        }
        if (!m_neigh)
        {
            m_neigh = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                        d.pw, d.ph);
            if (!m_neigh)
            {
                m_neigh_ready = false; // the old page's texture is gone with its size
                m_neigh_want  = 0;     // transient failure: re-requested next tick
                return;
            }
            m_neigh_w = d.pw;
            m_neigh_h = d.ph;
            SDL_SetTextureScaleMode(m_neigh, SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(m_neigh, SDL_BLENDMODE_BLEND);
        }
        SDL_UpdateTexture(m_neigh, nullptr, d.px.data(), d.pw * 4);
        ++m_bake_counters.neigh_bakes;
        m_neigh_hash = d.hash;
        // The result's OWN subject, carried by the job — never the request's
        // current one, which may have moved on while this baked (F34).
        m_neigh_tile = d.neigh_tile;
        std::copy(std::begin(d.neigh_rect), std::end(d.neigh_rect), std::begin(m_neigh_rect));
        m_neigh_ready = true;
        return;
    }
    if (d.tier < 0)
    {
        if (!sync && (!m_far_queued || d.gen != m_far_job_gen))
            return; // an orphan of a superseded far job (see the chunk case)
        m_far_queued = false;
        if (d.gen != m_gen)
        {
            // Let the far hash re-run this generation rather than waiting out
            // the next source refresh — the page heals on the next tick.
            m_far_gen_checked = ~0u;
            return;
        }
        if (!m_far)
        {
            m_far = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                      d.pw, d.ph);
            if (!m_far)
            {
                m_far_gen_checked = ~0u; // retry next tick
                return;
            }
            SDL_SetTextureScaleMode(m_far, SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(m_far, SDL_BLENDMODE_BLEND);
        }
        SDL_UpdateTexture(m_far, nullptr, d.px.data(), d.pw * 4);
        ++m_bake_counters.far_bakes;
        m_far_hash  = d.hash;
        m_far_ready = true;
        return;
    }
    tier_state& t = m_tiers[d.tier];
    const auto cit = t.chunks.find(static_cast<std::uint32_t>(d.cj) * t.cw + d.ci);
    // RESULTS ARE KEYED BY (generation, tier, key) (the pool): only the job the
    // slot's queued flag stands for may clear it. A result from an earlier
    // incarnation of the slot (reset, tilt change, eviction) is dropped
    // untouched, so arrival order cannot matter.
    if (cit == t.chunks.end()
        || (!sync && (!cit->second.queued || cit->second.job_gen != d.gen)))
        return; // (the synchronous --verify path bakes the slot it just made)
    chunk& c = cit->second;
    c.queued = false; // before any early return — see above
    if (d.gen != m_gen)
        return; // baked against a dead body/source; the wanted loop re-enqueues
    if (!c.tex)
    {
        c.tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                  d.pw, d.ph);
        if (!c.tex)
            return; // transient failure: not ready, not queued → retried
        SDL_SetTextureScaleMode(c.tex, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(c.tex, SDL_BLENDMODE_BLEND);
    }
    SDL_UpdateTexture(c.tex, nullptr, d.px.data(), d.pw * 4);
    ++m_bake_counters.chunk_bakes;
    if (c.ready)
        ++m_bake_counters.chunk_rebakes;
    ++c.bakes;
    c.hash  = d.hash;
    c.ready = true;
    c.stale = false;
    c.last_want = m_frame;
}

void ground_layer::drain_results(SDL_Renderer* r)
{
    std::vector<result> done;
    {
        std::lock_guard lk(m_mx);
        done.swap(m_results);
    }
    for (const result& d : done)
        upload(r, d, /*sync=*/false);
}

void ground_layer::bake_now(SDL_Renderer* r, const job& j)
{
    m_scratch.assign(static_cast<std::size_t>(j.pw) * j.ph, 0u);
    const auto t0 = std::chrono::steady_clock::now();
    ui::ground::bake_region(*j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph,
                            m_scratch.data());
    {
        const double ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - t0).count();
        std::lock_guard lk(m_mx);
        note_bake(j.tier, ms);
    }
    result d;
    d.tier = j.tier; d.ci = j.ci; d.cj = j.cj; d.pw = j.pw; d.ph = j.ph;
    d.hash = j.hash; d.gen = j.gen;
    d.neigh_tile = j.neigh_tile;
    std::copy(std::begin(j.neigh_rect), std::end(j.neigh_rect), std::begin(d.neigh_rect));
    d.px = m_scratch;
    upload(r, d, /*sync=*/true);
}

void ground_layer::note_bake(int tier, double ms)
{
    const int slot = tier + 1;
    if (slot < 0 || slot >= k_stat_slots)
        return;
    m_stats.bake_ms[slot] += ms;
    ++m_stats.bakes[slot];
}

void ground_layer::reset_stats()
{
    std::lock_guard lk(m_mx);
    m_stats = stats{};
}

ground_layer::stats ground_layer::stats_snapshot() const
{
    stats s;
    {
        std::lock_guard lk(m_mx);
        s = m_stats;
    }
    s.tier_ppr[0] = k_far_px_per_r;
    if (m_far_ready && m_far)
        s.resident_bytes[0] = static_cast<long long>(m_far_geom.W) * m_far_geom.H * 4;
    for (int t = 0; t < k_tiers && t + 1 < k_stat_slots; ++t)
    {
        s.tier_ppr[t + 1] = k_tier_ppr[t];
        const tier_state& ts = m_tiers[t];
        long long bytes = 0;
        for (const auto& [key, c] : ts.chunks)
        {
            if (!c.ready || !c.tex || ts.cw <= 0)
                continue;
            const int ci = static_cast<int>(key % static_cast<std::uint32_t>(ts.cw));
            const int cj = static_cast<int>(key / static_cast<std::uint32_t>(ts.cw));
            bytes += static_cast<long long>(std::min(k_chunk_px, ts.geom.W - ci * k_chunk_px))
                   * std::min(k_chunk_px, ts.geom.H - cj * k_chunk_px) * 4;
        }
        s.resident_bytes[t + 1] = bytes;
    }
    s.active_slot = m_active_tier + 1;
    return s;
}

void ground_layer::evict(tier_state& t, std::size_t cap)
{
    while (t.chunks.size() > cap)
    {
        auto victim = t.chunks.end();
        for (auto it = t.chunks.begin(); it != t.chunks.end(); ++it)
        {
            if (it->second.queued)
                continue; // a result is coming for it; skip this round
            if (victim == t.chunks.end()
                || it->second.last_want < victim->second.last_want)
                victim = it;
        }
        if (victim == t.chunks.end())
            break;
        if (victim->second.tex)
            SDL_DestroyTexture(victim->second.tex);
        t.chunks.erase(victim);
    }
}

// ---------------------------------------------------------------------------
// The per-frame driver
// ---------------------------------------------------------------------------

void ground_layer::tick(SDL_Renderer* r, const world& w, ui_state& ui, bool bake_everything)
{
    // IO_GROUND_BENCH: the pool path under --verify, so a script can time a
    // real fill. Off by default; a capture taken in this mode can race.
    if (bake_everything && bench_async_on())
        bake_everything = false;
    ++m_frame;
    const entity_id body = ui.active_body;
    if (body != m_body)
    {
        reset(body, w);
        if (fill_log_on() && m_src)
        {
            m_far_pending = true;
            m_far_t0 = clock::now();
        }
    }
    if (m_body == null_entity || !m_src)
    {
        ui.ground = ground_view{};
        return;
    }

    drain_results(r);
    if (m_far_pending && m_far_ready)
    {
        m_far_pending = false;
        std::printf("GROUND_FILL far page %dx%d  %.1f ms  workers %d\n", m_far_geom.W,
                    m_far_geom.H, std::chrono::duration<double, std::milli>(
                        clock::now() - m_far_t0).count(), pool_size());
        std::fflush(stdout);
    }

    // Source refresh on a slow cadence (urban transform, survey reveal move it;
    // nothing else does). Gated on NO WORK IN FLIGHT — not merely an empty
    // queue: the worker pops a job before baking it, so m_jobs.empty() is true
    // mid-bake, and a generation bump then would orphan the result (the review
    // fleet's confirmed critical, 2026-09-01). Even though upload now heals the
    // flags on an orphaned result, refreshing under a live bake wastes the
    // bake; --verify refreshes every call so a capture sees this frame's world.
    bool idle;
    {
        std::lock_guard lk(m_mx);
        idle = m_jobs.empty() && m_inflight == 0;
    }
    if (bake_everything || (++m_src_age >= 30 && idle))
        refresh_source(w);

    // --- The far page ---
    // Hashed once per SOURCE GENERATION, not per frame: the far hash walks the
    // whole grid, and nothing can move it while the source snapshot stands.
    if (m_src && m_far_gen_checked != m_gen)
    {
        m_far_gen_checked = m_gen;
        const std::uint64_t fh = ui::ground::region_hash(
            *m_src, m_far_geom, 0, 0, m_far_geom.W, m_far_geom.H);
        const bool far_stale = !m_far_ready || fh != m_far_hash;
        if (far_stale && (bake_everything || !m_far_queued))
        {
            job j;
            j.tier = -1; j.px0 = 0; j.py0 = 0;
            j.pw = m_far_geom.W; j.ph = m_far_geom.H;
            j.gen = m_gen; j.src = m_src; j.geom = m_far_geom; j.prm = params;
            j.hash = fh;
            if (bake_everything)
                bake_now(r, j);
            else
            {
                m_far_queued = true;
                m_far_job_gen = j.gen;
                j.prio = -2.0; // the far page before everything on a body switch
                enqueue(std::move(j));
            }
        }
    }

    // --- The Selection band's neighbourhood page (BL-1241) ---
    // A flat close-tier bake of the selected tile's neighbourhood, so the
    // zoomed view shows the same ground — structures included — as the canvas.
    {
        const ground_neigh_request& nq = ui.ground_neigh_req;
        if (nq.valid && nq.body == m_body && m_src && m_neigh_geom.W > 0 && !m_neigh_queued)
        {
            constexpr double kSqrt3_ = 1.7320508075688772;
            const ui::ground::geometry& g = m_neigh_geom;
            const double cx = kSqrt3_ * (nq.col + ((nq.row & 1) ? 0.5 : 0.0));
            const double cy = 1.5 * nq.row;
            const double hx = (nq.radius + 1.2) * kSqrt3_;
            const double hy = (nq.radius + 1.2) * 1.5 + 0.6;
            const int px0 = static_cast<int>(std::floor((cx - hx) * g.s));
            const int py0 = std::clamp(static_cast<int>(std::floor((cy - hy - g.y_min) * g.s)),
                                       0, std::max(0, g.H - 1));
            const int pw  = std::max(1, static_cast<int>(std::ceil(2.0 * hx * g.s)));
            const int ph  = std::max(1, std::min(g.H - py0,
                                                 static_cast<int>(std::ceil(2.0 * hy * g.s))));
            std::uint64_t h = ui::ground::region_hash(*m_src, g, px0, py0, pw, ph);
            // The window AND the subject: py0 clamps to 0 near the top edge, so
            // two rows in one column can share a window (and so a hash), and
            // the page would stick on the old tile (F34). The requested tile,
            // its grid position and the radius are folded in explicitly.
            const auto fold = [&h](std::uint64_t v) { h ^= v; h *= 1099511628211ull; };
            fold(static_cast<std::uint32_t>(px0));
            fold(static_cast<std::uint32_t>(py0));
            fold(static_cast<std::uint32_t>(pw));
            fold(static_cast<std::uint32_t>(ph));
            fold(static_cast<std::uint64_t>(nq.tile));
            fold(static_cast<std::uint32_t>(nq.col));
            fold(static_cast<std::uint32_t>(nq.row));
            fold(static_cast<std::uint32_t>(nq.radius));
            const bool stale = !m_neigh_ready || h != m_neigh_hash || m_neigh_tile != nq.tile;
            if (stale && (h != m_neigh_want || !m_neigh_ready || bake_everything))
            {
                job j;
                j.tier = -2; j.px0 = px0; j.py0 = py0; j.pw = pw; j.ph = ph;
                j.gen = m_gen; j.src = m_src; j.geom = g; j.prm = params;
                j.hash = h;
                j.neigh_tile = nq.tile;
                j.neigh_rect[0] = static_cast<float>(px0 / g.s);
                j.neigh_rect[1] = static_cast<float>(py0 / g.s + g.y_min);
                j.neigh_rect[2] = static_cast<float>((px0 + pw) / g.s);
                j.neigh_rect[3] = static_cast<float>((py0 + ph) / g.s + g.y_min);
                m_neigh_want = h;
                if (bake_everything)
                    bake_now(r, j);
                else
                {
                    m_neigh_queued = true;
                    m_neigh_job_gen = j.gen;
                    j.prio = -1.0;
                    enqueue(std::move(j));
                }
            }
        }
    }

    // --- The active tier's wanted set ---
    const ground_request& req = ui.ground_req;
    if (req.valid && req.body == m_body && req.draw_r > 0.0f)
    {
        // THE GROUND IS NEVER MAGNIFIED (BL-1244, Ben 2026-10-08): the
        // smallest tier at or above the DRAWN hex radius (the canvas passes
        // hex_size * zoom, not its 1 px border-inset), drawn minified — never
        // past 2:1 on the x2 ladder — and the far page only where it too is
        // at or below 1:1. hex_size is fit-derived, so where a rung lands
        // against its tier still moves with the window; past the 192 px top
        // tier the ground magnifies — the ladder's one bound.
        const int prev_active = m_active_tier;
        m_active_tier = ui::ground::choose_tier(req.draw_r, k_far_px_per_r,
                                                k_tier_ppr, k_tiers);
        // A tier that has just become active is re-hashed IMMEDIATELY, every
        // ready chunk in view (F34): its chunks may have baked many days ago,
        // as another rung's stand-in or before the rung was last left.
        const bool became_active = m_active_tier != prev_active;
        if (became_active && !bake_everything)
            purge_waiting_except(m_active_tier); // the left rung's backlog yields
        if (became_active && fill_log_on())
        {
            m_fill_pending = m_active_tier >= 0;
            m_fill_t0 = clock::now();
            m_fill_bakes0 = m_bake_counters.chunk_bakes;
        }

        int active_ready = 0, active_total = 0;
        if (m_active_tier >= 0)
        {
            tier_state& t = m_tiers[m_active_tier];
            // BL-737: the tilted rungs want OBLIQUE tiers. A tilt change
            // rebuilds this tier's geometry and drops its chunks; the tilt is
            // rung-locked, so this fires on rung transitions only. The
            // generation bump orphans any in-flight bake against the old
            // projection (upload clears its flag and drops the result).
            const double want_sy =
                std::clamp(static_cast<double>(req.sy > 0.0f ? req.sy : 1.0f), 0.3, 1.0);
            if (std::fabs(want_sy - t.baked_sy) > 1e-3)
            {
                for (auto& [k, c] : t.chunks)
                    if (c.tex)
                        SDL_DestroyTexture(c.tex);
                t.chunks.clear();
                t.geom = ui::ground::make_geometry(t.geom.gw, t.geom.gh, t.geom_ppr, want_sy);
                t.cw = (t.geom.W + k_chunk_px - 1) / k_chunk_px;
                t.ch = (t.geom.H + k_chunk_px - 1) / k_chunk_px;
                t.baked_sy = want_sy;
                ++m_gen;
                if (!bake_everything)
                    purge_waiting_except(m_active_tier); // its waiting jobs are now stale
            }
            const chunk_window win = window_of(t, req);

            int queued_now = 0;
            int fills = 0;
            {
                std::lock_guard lk(m_mx);
                queued_now = static_cast<int>(m_jobs.size());
            }
            const int max_waiting = pool_size() + k_queue_slack;
            // Visit order. --verify keeps raster order (its mid-fill frame
            // under verify_fill_limit depends on it); the pool path visits —
            // and so enqueues — nearest the viewport centre first.
            struct cand { int ci, cj; double d2; };
            std::vector<cand> order;
            order.reserve(static_cast<std::size_t>(std::max(0, win.ci_hi - win.ci_lo + 1))
                          * static_cast<std::size_t>(std::max(0, win.cj_hi - win.cj_lo + 1)));
            const double ccx = 0.5 * (req.x0 + req.x1) * t.geom.s / k_chunk_px - 0.5;
            const double ccy = (0.5 * (req.y0 + req.y1) - t.geom.y_min) * t.geom.s / k_chunk_px - 0.5;
            for (int cj = win.cj_lo; cj <= win.cj_hi; ++cj)
                for (int ci = win.ci_lo; ci <= win.ci_hi; ++ci)
                    order.push_back({ ci, cj, (ci - ccx) * (ci - ccx) + (cj - ccy) * (cj - ccy) });
            if (!bake_everything)
                std::stable_sort(order.begin(), order.end(),
                                 [](const cand& a, const cand& b) { return a.d2 < b.d2; });
            std::size_t wanted = 0;
            for (const cand& cd : order)
                {
                    const int ci = cd.ci, cj = cd.cj;
                    const int cw = ((ci % t.cw) + t.cw) % t.cw;
                    const std::uint32_t key = static_cast<std::uint32_t>(cj) * t.cw + cw;
                    chunk& c = t.chunks[key];
                    c.last_want = m_frame;
                    ++wanted;
                    if (c.queued)
                        continue;
                    // Hash only what could need work: an unbaked chunk, a
                    // stale one, or a staleness sweep of a ready one —
                    // amortised live, but EVERY tick under --verify (a capture
                    // must never show stale ground because the sweep phase
                    // hadn't come round) and on the tick the tier becomes
                    // active.
                    const bool sweep = c.ready
                        && (c.stale || bake_everything || became_active
                            || (m_frame + key) % 60 == 0);
                    if (c.ready && !sweep)
                        continue;
                    // A cold chunk with the queue full: nothing to do this tick
                    // but its region hash — skip it (the pool path only).
                    if (!c.ready && !bake_everything && queued_now >= max_waiting)
                        continue;
                    job j = make_chunk_job(m_active_tier, cw, cj);
                    if (c.ready && j.hash == c.hash)
                    {
                        c.stale = false; // its inputs moved back: the texture is true again
                        continue;
                    }
                    if (c.ready && became_active)
                        c.stale = true; // a texture from a past visit whose inputs moved: hide it
                    if (bake_everything)
                    {
                        // verify_fill_limit: a capture of the MID-FILL frame
                        // bakes at most N chunks this tick, deterministically.
                        if (verify_fill_limit < 0 || fills < verify_fill_limit)
                        {
                            bake_now(r, j);
                            ++fills;
                        }
                    }
                    else if (queued_now < max_waiting)
                    {
                        c.queued = true;
                        c.job_gen = j.gen;
                        j.prio = cd.d2;
                        ++queued_now;
                        enqueue(std::move(j));
                    }
                }
            // The cap must never trim the set the viewport is actually using,
            // or eviction and re-bake thrash every frame (review fleet,
            // 2026-09-01): the wanted set, doubled, floors it.
            evict(t, std::max(k_tier_cap[m_active_tier], wanted * 2));
            coverage(t, win, active_ready, active_total);
            if (m_fill_pending && active_ready >= active_total)
            {
                m_fill_pending = false;
                std::printf("GROUND_FILL tier %d (%.0f px/r)  chunks %d  baked %llu  %.1f ms  workers %d\n",
                            m_active_tier, t.geom_ppr, active_total,
                            static_cast<unsigned long long>(m_bake_counters.chunk_bakes - m_fill_bakes0),
                            std::chrono::duration<double, std::milli>(clock::now() - m_fill_t0).count(),
                            pool_size());
                std::fflush(stdout);
            }
        }

        // THE STAND-IN (BL-1244; F34): while the active tier's view is not
        // fully ready, ONE other tier carries the frame under its chunks —
        // the nearest FINER tier resident in view (a zoom-out keeps the crisp
        // rung it just left, minified), else the nearest coarser one (a
        // zoom-in shows the rung below for the frames it fills), else the
        // far page alone. Once the active view is whole there is none.
        const int prev_standin = m_standin_tier;
        m_standin_tier = -1;
        if (m_active_tier >= 0 && active_ready < active_total)
        {
            const auto resident = [&](int ti) {
                int rd = 0, tot = 0;
                coverage(m_tiers[ti], window_of(m_tiers[ti], req), rd, tot);
                return rd > 0;
            };
            for (int ti = m_active_tier + 1; ti < k_tiers && m_standin_tier < 0; ++ti)
                if (resident(ti))
                    m_standin_tier = ti;
            for (int ti = m_active_tier - 1; ti >= 0 && m_standin_tier < 0; --ti)
                if (resident(ti))
                    m_standin_tier = ti;
        }
        // A stand-in is RE-HASHED, never baked (F34): its chunks in view ride
        // the active tier's sweep phase — every tick under --verify, and the
        // whole view on the tick it becomes the stand-in — and a chunk whose
        // inputs moved is marked stale and stops drawing, rather than showing
        // ground that is no longer there.
        if (m_standin_tier >= 0)
        {
            tier_state& st = m_tiers[m_standin_tier];
            const bool fresh = m_standin_tier != prev_standin;
            const chunk_window win = window_of(st, req);
            for (int cj = win.cj_lo; cj <= win.cj_hi; ++cj)
                for (int ci = win.ci_lo; ci <= win.ci_hi; ++ci)
                {
                    const int cw = ((ci % st.cw) + st.cw) % st.cw;
                    const std::uint32_t key = static_cast<std::uint32_t>(cj) * st.cw + cw;
                    const auto it = st.chunks.find(key);
                    if (it == st.chunks.end() || !it->second.ready || it->second.queued)
                        continue;
                    chunk& c = it->second;
                    c.last_want = m_frame;
                    if (!(c.stale || fresh || bake_everything || (m_frame + key) % 60 == 0))
                        continue;
                    c.stale = chunk_hash(st, cw, cj) != c.hash;
                }
        }

        // The top tier is resident only while its rung is active or it stands
        // in (RENDERING.md § Level of detail): its memory is the viewport's
        // chunks, never a body's worth left behind by a zoom-out — but it is
        // kept while the rung below fills, so the zoom-out shows it rather
        // than a coarser tier. Eviction never touches the stand-in.
        if (m_active_tier != k_tiers - 1 && m_standin_tier != k_tiers - 1
            && !m_tiers[k_tiers - 1].chunks.empty())
        {
            tier_state& top = m_tiers[k_tiers - 1];
            for (auto it = top.chunks.begin(); it != top.chunks.end();)
            {
                if (it->second.queued)
                {
                    ++it; // a result is in flight; upload heals it, next sweep drops it
                    continue;
                }
                if (it->second.tex)
                    SDL_DestroyTexture(it->second.tex);
                it = top.chunks.erase(it);
            }
        }
    }

    publish(ui);
}

ground_layer::chunk_window ground_layer::window_of(const tier_state& t,
                                                   const ground_request& req) const
{
    chunk_window win;
    if (t.cw <= 0 || t.ch <= 0)
        return win;
    const double s = t.geom.s;
    win.ci_lo = static_cast<int>(std::floor(req.x0 * s / k_chunk_px)) - 1;
    win.ci_hi = static_cast<int>(std::ceil (req.x1 * s / k_chunk_px));
    win.cj_lo = std::max(0, static_cast<int>(
        std::floor((req.y0 - t.geom.y_min) * s / k_chunk_px)) - 1);
    win.cj_hi = std::min(t.ch - 1, static_cast<int>(
        std::ceil((req.y1 - t.geom.y_min) * s / k_chunk_px)));
    return win;
}

void ground_layer::coverage(const tier_state& t, const chunk_window& win,
                            int& ready, int& total) const
{
    ready = total = 0;
    if (t.cw <= 0)
        return;
    for (int cj = win.cj_lo; cj <= win.cj_hi; ++cj)
        for (int ci = win.ci_lo; ci <= win.ci_hi; ++ci)
        {
            ++total;
            const int cw = ((ci % t.cw) + t.cw) % t.cw;
            const auto it = t.chunks.find(static_cast<std::uint32_t>(cj) * t.cw + cw);
            if (it != t.chunks.end() && it->second.ready && !it->second.stale && it->second.tex)
                ++ready;
        }
}

std::uint64_t ground_layer::chunk_hash(const tier_state& t, int ci, int cj) const
{
    const int px0 = ci * k_chunk_px;
    const int py0 = cj * k_chunk_px;
    return ui::ground::region_hash(*m_src, t.geom, px0, py0,
                                   std::min(k_chunk_px, t.geom.W - px0),
                                   std::min(k_chunk_px, t.geom.H - py0));
}

std::vector<ground_layer::chunk_probe> ground_layer::probe_chunks() const
{
    std::vector<chunk_probe> out;
    if (!m_src)
        return out;
    if (m_far_ready && m_far && m_far_geom.W > 0)
    {
        chunk_probe p;
        p.tier  = -1;
        p.key   = 0;
        p.bakes = m_bake_counters.far_bakes;
        p.installation_hash = ui::ground::installation_hash(*m_src, m_far_geom, 0, 0,
                                                            m_far_geom.W, m_far_geom.H);
        p.region_hash = ui::ground::region_hash(*m_src, m_far_geom, 0, 0,
                                                m_far_geom.W, m_far_geom.H);
        out.push_back(p);
    }
    if (m_active_tier < 0)
        return out;
    const tier_state& t = m_tiers[m_active_tier];
    if (t.cw <= 0)
        return out;
    for (const auto& [key, c] : t.chunks)
    {
        if (!c.ready || !c.tex)
            continue;
        const int ci  = static_cast<int>(key % static_cast<std::uint32_t>(t.cw));
        const int cj  = static_cast<int>(key / static_cast<std::uint32_t>(t.cw));
        const int px0 = ci * k_chunk_px;
        const int py0 = cj * k_chunk_px;
        const int pw  = std::min(k_chunk_px, t.geom.W - px0);
        const int ph  = std::min(k_chunk_px, t.geom.H - py0);
        chunk_probe p;
        p.tier  = m_active_tier;
        p.key   = key;
        p.bakes = c.bakes;
        p.installation_hash = ui::ground::installation_hash(*m_src, t.geom, px0, py0, pw, ph);
        p.region_hash       = ui::ground::region_hash(*m_src, t.geom, px0, py0, pw, ph);
        out.push_back(p);
    }
    std::sort(out.begin(), out.end(), [](const chunk_probe& a, const chunk_probe& b) {
        return a.tier != b.tier ? a.tier < b.tier : a.key < b.key;
    });
    return out;
}

std::uint64_t ground_layer::installation_digest() const
{
    if (!m_src || m_far_geom.W <= 0)
        return 0;
    return ui::ground::installation_hash(*m_src, m_far_geom, 0, 0, m_far_geom.W, m_far_geom.H);
}

void ground_layer::publish(ui_state& ui) const
{
    ground_view& v = ui.ground;
    v.body      = m_body;
    v.far_ready = m_far_ready && m_far;
    v.far.tex   = m_far;
    v.far.x0    = 0.0f;
    v.far.x1    = static_cast<float>(m_far_geom.W / m_far_geom.s);
    v.far.y0    = static_cast<float>(m_far_geom.y_min);
    v.far.y1    = static_cast<float>(m_far_geom.H / m_far_geom.s + m_far_geom.y_min);
    v.neigh      = ground_chunk_view{};
    v.neigh_tile = null_entity;
    if (m_neigh_ready && m_neigh)
    {
        v.neigh.tex = m_neigh;
        v.neigh.x0  = m_neigh_rect[0];
        v.neigh.y0  = m_neigh_rect[1];
        v.neigh.x1  = m_neigh_rect[2];
        v.neigh.y1  = m_neigh_rect[3];
        v.neigh_tile = m_neigh_tile;
    }
    v.chunks.clear();
    v.standin.clear();
    v.tier_ppr = 0.0;
    const auto emit = [](const tier_state& t, std::vector<ground_chunk_view>& out)
    {
        for (const auto& [key, c] : t.chunks)
        {
            if (!c.ready || !c.tex || c.stale || t.cw <= 0)
                continue; // a stale texture is ground that is no longer there
            const int ci  = static_cast<int>(key % static_cast<std::uint32_t>(t.cw));
            const int cj  = static_cast<int>(key / static_cast<std::uint32_t>(t.cw));
            const int px0 = ci * k_chunk_px;
            const int py0 = cj * k_chunk_px;
            const int pw  = std::min(k_chunk_px, t.geom.W - px0);
            const int ph  = std::min(k_chunk_px, t.geom.H - py0);
            ground_chunk_view cv;
            cv.x0  = static_cast<float>(px0 / t.geom.s);
            cv.x1  = static_cast<float>((px0 + pw) / t.geom.s);
            cv.y0  = static_cast<float>(py0 / t.geom.s + t.geom.y_min);
            cv.y1  = static_cast<float>((py0 + ph) / t.geom.s + t.geom.y_min);
            cv.tex = c.tex;
            out.push_back(cv);
        }
    };
    if (m_active_tier >= 0)
    {
        v.tier_ppr = m_tiers[m_active_tier].geom_ppr;
        // THE STAND-IN (BL-1244; F34 — chosen in tick): drawn over the far
        // page, under the active chunks, so the far page shows only where
        // nothing closer is. (A stand-in baked at another rung's tilt is off
        // by that tilt's lift for the frames it stands in; the far page,
        // always flat, already was.)
        if (m_standin_tier >= 0 && m_standin_tier != m_active_tier)
            emit(m_tiers[m_standin_tier], v.standin);
        emit(m_tiers[m_active_tier], v.chunks);
    }
}
