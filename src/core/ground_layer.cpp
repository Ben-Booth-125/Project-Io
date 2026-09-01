#include "ground_layer.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kSqrt3 = 1.7320508075688772;
}

constexpr double      ground_layer::k_tier_ppr[];
constexpr std::size_t ground_layer::k_tier_cap[];

ground_layer::~ground_layer()
{
    shutdown();
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

void ground_layer::worker_main()
{
    for (;;)
    {
        job j;
        {
            std::unique_lock lk(m_mx);
            m_cv.wait(lk, [&] { return m_quit || !m_jobs.empty(); });
            if (m_quit)
                return;
            j = std::move(m_jobs.front());
            m_jobs.pop_front();
        }
        result d;
        d.tier = j.tier; d.ci = j.ci; d.cj = j.cj; d.pw = j.pw; d.ph = j.ph;
        d.hash = j.hash; d.gen = j.gen;
        d.px.assign(static_cast<std::size_t>(j.pw) * j.ph, 0u);
        ui::ground::bake_region(*j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph, d.px.data());
        {
            std::lock_guard lk(m_mx);
            m_results.push_back(std::move(d));
            --m_inflight; // counted from enqueue until the result LANDS — a popped job is still in flight
        }
    }
}

void ground_layer::enqueue(job j)
{
    {
        std::lock_guard lk(m_mx);
        m_jobs.push_back(std::move(j));
        ++m_inflight;
    }
    if (!m_worker.joinable())
        m_worker = std::thread([this] { worker_main(); });
    m_cv.notify_one();
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
    m_active_tier = -1;
    m_body = body;
    m_src.reset();
    if (body == null_entity || !w.bodies.count(body))
        return;
    const body_component& b = w.bodies.at(body);
    if (b.grid_width <= 0 || b.grid_height <= 0)
        return; // a gridless body (the star) has no ground to bake
    m_far_geom = ui::ground::make_geometry(b.grid_width, b.grid_height, k_far_px_per_r);
    for (int t = 0; t < k_tiers; ++t)
    {
        m_tiers[t].geom_ppr = k_tier_ppr[t];
        m_tiers[t].geom = ui::ground::make_geometry(b.grid_width, b.grid_height, k_tier_ppr[t]);
        m_tiers[t].cw = (m_tiers[t].geom.W + k_chunk_px - 1) / k_chunk_px;
        m_tiers[t].ch = (m_tiers[t].geom.H + k_chunk_px - 1) / k_chunk_px;
    }
    refresh_source(w);
}

void ground_layer::refresh_source(const world& w)
{
    m_src = std::make_shared<const ui::ground::bake_source>(
        ui::ground::prepare_source(w, m_body));
    m_src_age = 0;
    ++m_gen; // in-flight results against the old source are stale
}

void ground_layer::shutdown()
{
    if (m_worker.joinable())
    {
        {
            std::lock_guard lk(m_mx);
            m_quit = true;
            m_jobs.clear();
        }
        m_cv.notify_one();
        m_worker.join();
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
    j.hash = ui::ground::region_hash(*m_src, t.geom, j.px0, j.py0, j.pw, j.ph);
    return j;
}

void ground_layer::upload(SDL_Renderer* r, const result& d)
{
    // THE PENDING FLAG CLEARS FIRST, UNCONDITIONALLY. Only one job per slot can
    // be outstanding (the queued guards), so the arriving result IS that job —
    // whatever happens next (stale generation, a failed texture allocation),
    // the slot must become re-enqueueable or it is bricked until a body switch.
    // The review fleet confirmed both wedge paths (2026-09-01): a generation
    // bump mid-bake orphaning the result, and SDL_CreateTexture returning null.
    if (d.tier < 0)
    {
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
        m_far_hash  = d.hash;
        m_far_ready = true;
        return;
    }
    tier_state& t = m_tiers[d.tier];
    chunk& c = t.chunks[static_cast<std::uint32_t>(d.cj) * t.cw + d.ci];
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
    c.hash  = d.hash;
    c.ready = true;
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
        upload(r, d);
}

void ground_layer::bake_now(SDL_Renderer* r, const job& j)
{
    m_scratch.assign(static_cast<std::size_t>(j.pw) * j.ph, 0u);
    ui::ground::bake_region(*j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph,
                            m_scratch.data());
    result d;
    d.tier = j.tier; d.ci = j.ci; d.cj = j.cj; d.pw = j.pw; d.ph = j.ph;
    d.hash = j.hash; d.gen = j.gen;
    d.px = m_scratch;
    upload(r, d);
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
    ++m_frame;
    const entity_id body = ui.active_body;
    if (body != m_body)
        reset(body, w);
    if (m_body == null_entity || !m_src)
    {
        ui.ground = ground_view{};
        return;
    }

    drain_results(r);

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
                enqueue(std::move(j));
            }
        }
    }

    // --- The active tier's wanted set ---
    const ground_request& req = ui.ground_req;
    if (req.valid && req.body == m_body && req.draw_r > 0.0f)
    {
        // Smallest tier within the magnification headroom of the drawn hex
        // radius; below the first tier the far page alone carries the frame
        // (the bottom zoom rung). The headroom exists because hex_size is
        // FIT-DERIVED (window height / grid rows), so the ladder's rungs land
        // NEAR the tier sizes, not on them — at the reference 1080p window the
        // rungs run ~4-14% magnified. Too tight a factor here jumps every rung
        // one tier high (2x the memory and bake for a minified image); the
        // review fleet caught exactly that at 0.93. Beyond the top tier the
        // 96 px bake magnifies unboundedly on very large windows — a known
        // bound, recorded in RENDERING.md.
        constexpr float k_tier_headroom = 1.2f;
        int tier = k_tiers - 1;
        for (int t = 0; t < k_tiers; ++t)
            if (k_tier_ppr[t] * k_tier_headroom >= req.draw_r)
            {
                tier = t;
                break;
            }
        m_active_tier = (req.draw_r <= k_far_px_per_r * 1.15f) ? -1 : tier;

        if (m_active_tier >= 0)
        {
            tier_state& t = m_tiers[m_active_tier];
            const double s = t.geom.s;
            const int ci_lo = static_cast<int>(std::floor(req.x0 * s / k_chunk_px)) - 1;
            const int ci_hi = static_cast<int>(std::ceil (req.x1 * s / k_chunk_px));
            const int cj_lo = std::max(0, static_cast<int>(
                std::floor((req.y0 - t.geom.y_min) * s / k_chunk_px)) - 1);
            const int cj_hi = std::min(t.ch - 1, static_cast<int>(
                std::ceil((req.y1 - t.geom.y_min) * s / k_chunk_px)));

            int queued_now = 0;
            {
                std::lock_guard lk(m_mx);
                queued_now = static_cast<int>(m_jobs.size());
            }
            std::size_t wanted = 0;
            for (int cj = cj_lo; cj <= cj_hi; ++cj)
                for (int ci = ci_lo; ci <= ci_hi; ++ci)
                {
                    const int cw = ((ci % t.cw) + t.cw) % t.cw;
                    const std::uint32_t key = static_cast<std::uint32_t>(cj) * t.cw + cw;
                    chunk& c = t.chunks[key];
                    c.last_want = m_frame;
                    ++wanted;
                    if (c.queued)
                        continue;
                    // Hash only what could need work: an unbaked chunk, or a
                    // staleness sweep of a ready one — amortised live, but
                    // EVERY tick under --verify: a capture must never show
                    // stale ground because the sweep phase hadn't come round.
                    const bool sweep = c.ready
                        && (bake_everything || (m_frame + key) % 60 == 0);
                    if (c.ready && !sweep)
                        continue;
                    job j = make_chunk_job(m_active_tier, cw, cj);
                    if (c.ready && j.hash == c.hash)
                        continue;
                    if (bake_everything)
                        bake_now(r, j);
                    else if (queued_now < k_max_queued)
                    {
                        c.queued = true;
                        ++queued_now;
                        enqueue(std::move(j));
                    }
                }
            // The cap must never trim the set the viewport is actually using,
            // or eviction and re-bake thrash every frame (review fleet,
            // 2026-09-01): the wanted set, doubled, floors it.
            evict(t, std::max(k_tier_cap[m_active_tier], wanted * 2));
        }
    }

    publish(ui);
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
    v.chunks.clear();
    v.tier_ppr = 0.0;
    if (m_active_tier >= 0)
    {
        const tier_state& t = m_tiers[m_active_tier];
        v.tier_ppr = t.geom_ppr;
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
            ground_chunk_view cv;
            cv.x0  = static_cast<float>(px0 / t.geom.s);
            cv.x1  = static_cast<float>((px0 + pw) / t.geom.s);
            cv.y0  = static_cast<float>(py0 / t.geom.s + t.geom.y_min);
            cv.y1  = static_cast<float>((py0 + ph) / t.geom.s + t.geom.y_min);
            cv.tex = c.tex;
            v.chunks.push_back(cv);
        }
    }
}
