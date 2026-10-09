#include "ground_layer.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

namespace gb = ui::ground;

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
double ms_since(std::chrono::steady_clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

/// Frames between source snapshots of the active body and the pre-bake target
/// (an urban transform, a survey reveal or a build moves the source; a player
/// build also calls touch(), so it never waits the cadence out).
constexpr int k_src_cadence = 30;

/// Queue priority bands (lower pops first).
constexpr double k_prio_far        = -3.0;
constexpr double k_prio_sweep      = -2.0;
constexpr double k_prio_neigh      = -1.0;
constexpr double k_prio_view       = 0.0;     ///< + squared chunk distance from the view centre
constexpr double k_prio_body       = 1.0e6;   ///< The rest of the active body
constexpr double k_prio_prebake    = 2.0e6;
constexpr double k_prio_background = 3.0e6;

} // namespace

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

void ground_layer::run_job(const job& j, result& d, double* level_ms)
{
    d.kind = j.kind; d.body = j.body; d.idx = j.idx; d.seq = j.seq; d.epoch = j.epoch;
    d.pw = j.pw; d.ph = j.ph;
    switch (j.kind)
    {
    case job_kind::sweep:
    {
        // Every master chunk's content hash against this snapshot.
        const int cw = (j.geom.W + gb::k_chunk_px - 1) / gb::k_chunk_px;
        const int ch = (j.geom.H + gb::k_chunk_px - 1) / gb::k_chunk_px;
        d.hashes.resize(static_cast<std::size_t>(cw) * ch);
        for (int cj = 0; cj < ch; ++cj)
            for (int ci = 0; ci < cw; ++ci)
            {
                const int x0 = ci * gb::k_chunk_px, y0 = cj * gb::k_chunk_px;
                d.hashes[static_cast<std::size_t>(cj) * cw + ci] = gb::region_hash(
                    *j.src, j.geom, x0, y0, std::min(gb::k_chunk_px, j.geom.W - x0),
                    std::min(gb::k_chunk_px, j.geom.H - y0));
            }
        d.hash = j.hash; // the digest the table belongs to
        return;
    }
    case job_kind::far:
    case job_kind::neigh:
        d.px.assign(static_cast<std::size_t>(j.pw) * j.ph, 0u);
        gb::bake_region(*j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph, d.px.data());
        d.hash = j.hash;
        d.neigh_tile = j.neigh_tile;
        std::copy(std::begin(j.neigh_rect), std::end(j.neigh_rect), std::begin(d.neigh_rect));
        return;
    case job_kind::master:
    {
        auto t0 = std::chrono::steady_clock::now();
        d.px.assign(static_cast<std::size_t>(j.pw) * j.ph, 0u);
        gb::bake_region(*j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph, d.px.data());
        d.hash = gb::region_hash(*j.src, j.geom, j.px0, j.py0, j.pw, j.ph);
        if (level_ms)
            level_ms[0] = ms_since(t0);
        // The mip pieces: this chunk's share of every coarser level, each a
        // box-downsample of the one above (sides are multiples of 16).
        int lw = j.pw, lh = j.ph;
        const std::vector<std::uint32_t>* prev = &d.px;
        for (int l = 1; l < L; ++l)
        {
            t0 = std::chrono::steady_clock::now();
            d.mip[l].assign(static_cast<std::size_t>(lw / 2) * (lh / 2), 0u);
            gb::downsample_half(prev->data(), lw, lh, d.mip[l].data());
            lw /= 2; lh /= 2;
            prev = &d.mip[l];
            if (level_ms)
                level_ms[l] = ms_since(t0);
        }
        return;
    }
    }
}

void ground_layer::worker_main()
{
    // Below-normal priority: a whole-body bake saturates every worker, and
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
            // The lowest (prio, seq). The waiting set is capped near twice
            // the pool size, so a scan is cheap.
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
        double lms[L] = {};
        const auto t0 = std::chrono::steady_clock::now();
        run_job(j, d, lms);
        const double ms = ms_since(t0);
        {
            std::lock_guard lk(m_mx);
            if (j.kind == job_kind::far)
            {
                m_stats.bake_ms[0] += ms;
                ++m_stats.bakes[0];
            }
            else if (j.kind == job_kind::master)
                for (int l = 0; l < L; ++l)
                {
                    m_stats.bake_ms[1 + l] += lms[l];
                    ++m_stats.bakes[1 + l];
                }
            m_results.push_back(std::move(d));
            --m_inflight; // counted from enqueue until the result LANDS
        }
        m_done_cv.notify_all();
    }
}

void ground_layer::enqueue(job j)
{
    {
        std::lock_guard lk(m_mx);
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

int ground_layer::waiting() const
{
    std::lock_guard lk(m_mx);
    return static_cast<int>(m_jobs.size());
}

void ground_layer::drain()
{
    std::vector<result> done;
    {
        std::lock_guard lk(m_mx);
        done.swap(m_results);
    }
    for (result& d : done)
        land(d, /*sync=*/false);
}

void ground_layer::bake_sync(job j)
{
    result d;
    double lms[L] = {};
    const auto t0 = std::chrono::steady_clock::now();
    run_job(j, d, lms);
    const double ms = ms_since(t0);
    {
        std::lock_guard lk(m_mx);
        if (j.kind == job_kind::far)
        {
            m_stats.bake_ms[0] += ms;
            ++m_stats.bakes[0];
        }
    }
    land(d, /*sync=*/true);
}

// ---------------------------------------------------------------------------
// Bodies
// ---------------------------------------------------------------------------

ground_layer::body_state* ground_layer::find(entity_id id)
{
    const auto it = m_bodies.find(id);
    return it == m_bodies.end() ? nullptr : it->second.get();
}

const ground_layer::body_state* ground_layer::find(entity_id id) const
{
    const auto it = m_bodies.find(id);
    return it == m_bodies.end() ? nullptr : it->second.get();
}

long long ground_layer::master_bytes(const body_state& b)
{
    long long n = 0;
    for (int l = 0; l < L; ++l)
        n += static_cast<long long>(b.lv[l].geom.W) * b.lv[l].geom.H * 4;
    return n;
}

ground_layer::body_state* ground_layer::ensure_body(const world& w, entity_id id)
{
    if (body_state* b = find(id))
        return b;
    if (id == null_entity)
        return nullptr;
    const auto bit = w.bodies.find(id);
    if (bit == w.bodies.end() || bit->second.grid_width <= 0 || bit->second.grid_height <= 0)
        return nullptr; // a gridless body (the star) has no ground to bake
    auto bs = std::make_unique<body_state>();
    body_state& b = *bs;
    b.id = id;
    b.gw = bit->second.grid_width;
    b.gh = bit->second.grid_height;
    const gb::geometry m = gb::make_master_geometry(b.gw, b.gh);
    for (int l = 0; l < L; ++l)
    {
        level_store& s = b.lv[l];
        s.geom = gb::level_geometry(m, l);
        s.cw = (s.geom.W + gb::k_chunk_px - 1) / gb::k_chunk_px;
        s.ch = (s.geom.H + gb::k_chunk_px - 1) / gb::k_chunk_px;
        s.px.resize(static_cast<std::size_t>(s.cw) * s.ch);
        s.ver.assign(static_cast<std::size_t>(s.cw) * s.ch, 0u);
    }
    b.n_chunks = b.lv[0].cw * b.lv[0].ch;
    const std::size_t n = static_cast<std::size_t>(b.n_chunks);
    b.baked_hash.assign(n, 0);
    b.want_hash.assign(n, 0);
    b.bakes.assign(n, 0);
    b.job.assign(n, 0);
    b.ready.assign(n, 0);
    b.dirty.assign(n, 0);
    b.far_geom = gb::make_geometry(b.gw, b.gh, gb::k_far_ppr, gb::k_tilt_sy);
    b.far_cw = (b.far_geom.W + gb::k_chunk_px - 1) / gb::k_chunk_px;
    b.far_ch = (b.far_geom.H + gb::k_chunk_px - 1) / gb::k_chunk_px;
    b.far_jobs.assign(static_cast<std::size_t>(b.far_cw) * b.far_ch, 0);
    m_bodies.emplace(id, std::move(bs));
    refresh_source(b, w);
    return &b;
}

void ground_layer::refresh_source(body_state& b, const world& w)
{
    b.src = std::make_shared<const gb::bake_source>(
        gb::prepare_source(w, b.id, /*reveal_all=*/false, registry));
    ++b.src_epoch;
    b.src_age = 0;
    // The whole-body digest (the far page's hash): when it holds still,
    // nothing any chunk reads has moved, and no sweep is needed.
    const std::uint64_t dg = gb::region_hash(*b.src, b.far_geom, 0, 0, b.far_geom.W, b.far_geom.H);
    if (dg != b.src_digest && b.id == m_active && fill_log_on() && b.n_ready > 0)
    {
        m_rebake_pending = true;
        m_rebake_t0 = clock::now();
    }
    b.src_digest = dg;
}

void ground_layer::drop_master(body_state& b)
{
    // Keep the far page; free every level. Outstanding master jobs are
    // orphaned (their slots' sequence numbers are cleared, so they land in
    // nothing) and waiting ones are purged.
    {
        std::lock_guard lk(m_mx);
        const auto split = std::partition(m_jobs.begin(), m_jobs.end(), [&](const job& j) {
            return !(j.body == b.id && j.kind == job_kind::master);
        });
        m_inflight -= static_cast<int>(m_jobs.end() - split);
        m_jobs.erase(split, m_jobs.end());
    }
    for (int l = 0; l < L; ++l)
    {
        for (std::size_t i = 0; i < b.lv[l].px.size(); ++i)
        {
            std::vector<std::uint32_t>().swap(b.lv[l].px[i]);
            ++b.lv[l].ver[i];
        }
    }
    std::fill(b.job.begin(), b.job.end(), 0);
    std::fill(b.ready.begin(), b.ready.end(), 0);
    std::fill(b.dirty.begin(), b.dirty.end(), 0);
    b.n_ready = 0;
    b.ram = static_cast<long long>(b.far_px.size()) * 4;
    b.background = false;
}

void ground_layer::make_room(long long need, entity_id keep_a, entity_id keep_b)
{
    long long total = 0;
    for (const auto& [id, bp] : m_bodies)
        total += bp->ram;
    // `need` is what @p keep_a will hold once whole; count what it holds now
    // as part of it.
    if (const body_state* k = find(keep_a))
        total -= k->ram;
    while (total + need > k_ram_budget)
    {
        body_state* victim = nullptr;
        for (auto& [id, bp] : m_bodies)
        {
            body_state& c = *bp;
            if (c.id == keep_a || c.id == keep_b)
                continue;
            if (c.ram <= static_cast<long long>(c.far_px.size()) * 4)
                continue; // nothing but its far page
            if (!victim || c.last_visit < victim->last_visit
                || (c.last_visit == victim->last_visit && c.id < victim->id))
                victim = &c;
        }
        if (!victim)
            return;
        const long long before = victim->ram;
        drop_master(*victim);
        total -= before - victim->ram;
        std::printf("[ground] RAM budget: dropped the master of body %u (%.2f GB freed)\n",
                    static_cast<unsigned>(victim->id), (before - victim->ram) / 1073741824.0);
        std::fflush(stdout);
    }
}

void ground_layer::forget_world()
{
    {
        std::lock_guard lk(m_mx);
        m_inflight -= static_cast<int>(m_jobs.size());
        m_jobs.clear();
        m_results.clear();
    }
    flush_gpu();
    m_bodies.clear();
    m_active = null_entity;
    m_prebake = null_entity;
    m_in_play = false;
    m_view_valid = false;
    m_publish_level = -1;
    m_publish_keys.clear();
    if (m_neigh)
    {
        SDL_DestroyTexture(m_neigh);
        m_neigh = nullptr;
    }
    m_neigh_ready = false;
    m_neigh_job = 0;
    m_neigh_want = 0;
    m_neigh_tile = null_entity;
    m_neigh_body = null_entity;
    m_neigh_upload = false;
}

void ground_layer::prebake(const world& w, entity_id body)
{
    body_state* b = ensure_body(w, body);
    if (!b)
        return;
    m_prebake = body;
    b->last_visit = m_frame;
    make_room(master_bytes(*b), body, m_active);
    if (fill_log_on())
    {
        m_prebake_pending = true;
        m_prebake_t0 = clock::now();
    }
    feed(/*verify=*/false);
}

void ground_layer::master_progress(entity_id body, int& ready, int& total) const
{
    ready = total = 0;
    if (const body_state* b = find(body))
    {
        ready = b->n_ready;
        total = b->n_chunks;
    }
}

bool ground_layer::master_complete(entity_id body) const
{
    const body_state* b = find(body);
    return b && b->n_chunks > 0 && b->n_ready >= b->n_chunks;
}

void ground_layer::touch()
{
    if (body_state* b = find(m_active))
        b->src_age = k_src_cadence;
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
    forget_world();
    if (m_far_tex)
    {
        SDL_DestroyTexture(m_far_tex);
        m_far_tex = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Jobs
// ---------------------------------------------------------------------------

ground_layer::job ground_layer::make_master_job(const body_state& b, int idx, double prio)
{
    const level_store& s = b.lv[0];
    job j;
    j.kind = job_kind::master;
    j.body = b.id;
    j.idx  = idx;
    j.seq  = next_seq();
    j.prio = prio;
    j.src  = b.src;
    j.geom = s.geom;
    j.prm  = params;
    j.prm.supersample = k_master_ss;
    const int ci = idx % s.cw, cj = idx / s.cw;
    j.px0 = ci * gb::k_chunk_px;
    j.py0 = cj * gb::k_chunk_px;
    j.pw  = std::min(gb::k_chunk_px, s.geom.W - j.px0);
    j.ph  = std::min(gb::k_chunk_px, s.geom.H - j.py0);
    return j;
}

void ground_layer::start_far(body_state& b)
{
    b.far_baking  = b.src_digest;
    b.far_pending = 0;
    for (int k = 0; k < b.far_cw * b.far_ch; ++k)
    {
        job j;
        j.kind = job_kind::far;
        j.body = b.id;
        j.idx  = k;
        j.seq  = next_seq();
        j.prio = k_prio_far;
        j.src  = b.src;
        j.geom = b.far_geom;
        j.prm  = params;
        j.prm.supersample = 1; // the fallback: fast beats anti-aliased here
        j.px0  = (k % b.far_cw) * gb::k_chunk_px;
        j.py0  = (k / b.far_cw) * gb::k_chunk_px;
        j.pw   = std::min(gb::k_chunk_px, b.far_geom.W - j.px0);
        j.ph   = std::min(gb::k_chunk_px, b.far_geom.H - j.py0);
        j.hash = b.src_digest;
        b.far_jobs[static_cast<std::size_t>(k)] = j.seq;
        ++b.far_pending;
        enqueue(std::move(j));
    }
}

template <class Pred>
void ground_layer::wait_until(Pred done)
{
    for (;;)
    {
        drain();
        if (done())
            return;
        std::unique_lock lk(m_mx);
        m_done_cv.wait_for(lk, std::chrono::milliseconds(20), [&] { return !m_results.empty(); });
    }
}

ground_layer::job ground_layer::make_sweep_job(const body_state& b)
{
    job j;
    j.kind  = job_kind::sweep;
    j.body  = b.id;
    j.seq   = next_seq();
    j.prio  = k_prio_sweep;
    j.src   = b.src;
    j.geom  = b.lv[0].geom;
    j.epoch = b.src_epoch;
    j.hash  = b.src_digest;
    return j;
}

void ground_layer::land(result& d, bool sync)
{
    (void)sync;
    if (d.kind == job_kind::neigh)
    {
        if (d.seq != m_neigh_job)
            return; // an orphan of a superseded page
        m_neigh_job = 0;
        m_neigh_px  = std::move(d.px);
        m_neigh_w   = d.pw;
        m_neigh_h   = d.ph;
        m_neigh_hash = d.hash;
        // The result's OWN subject, carried by the job — never the request's
        // current one, which may have moved on while this baked (F34).
        m_neigh_tile = d.neigh_tile;
        std::copy(std::begin(d.neigh_rect), std::end(d.neigh_rect), std::begin(m_neigh_rect));
        m_neigh_upload = true;
        ++m_bake_counters.neigh_bakes;
        return;
    }
    body_state* bp = find(d.body);
    if (!bp)
        return; // the body was forgotten while this baked
    body_state& b = *bp;
    switch (d.kind)
    {
    case job_kind::far:
    {
        if (d.idx < 0 || d.idx >= static_cast<int>(b.far_jobs.size())
            || d.seq != b.far_jobs[static_cast<std::size_t>(d.idx)])
            return;
        b.far_jobs[static_cast<std::size_t>(d.idx)] = 0;
        if (b.far_px.empty())
        {
            b.far_px.assign(static_cast<std::size_t>(b.far_geom.W) * b.far_geom.H, 0u);
            b.ram += static_cast<long long>(b.far_px.size()) * 4;
        }
        const int x0 = (d.idx % b.far_cw) * gb::k_chunk_px, y0 = (d.idx / b.far_cw) * gb::k_chunk_px;
        for (int y = 0; y < d.ph; ++y)
            std::memcpy(b.far_px.data() + static_cast<std::size_t>(y0 + y) * b.far_geom.W + x0,
                        d.px.data() + static_cast<std::size_t>(y) * d.pw,
                        static_cast<std::size_t>(d.pw) * 4u);
        if (--b.far_pending == 0)
        {
            // Whole: shown from the next upload, never part-written (the GPU
            // copy is replaced only here).
            b.far_hash  = b.far_baking;
            b.far_ready = true;
            ++b.far_ver;
            ++m_bake_counters.far_bakes;
        }
        return;
    }
    case job_kind::sweep:
    {
        if (d.seq != b.sweep_job)
            return;
        b.sweep_job = 0;
        b.want_hash = std::move(d.hashes);
        b.hashes_epoch = d.epoch;
        b.swept_digest = d.hash;
        int n_ready = 0;
        for (int i = 0; i < b.n_chunks; ++i)
        {
            b.dirty[i] = b.ready[i] && b.baked_hash[i] != b.want_hash[i];
            n_ready += b.ready[i] && !b.dirty[i];
        }
        b.n_ready = n_ready;
        return;
    }
    case job_kind::master:
        break;
    case job_kind::neigh:
        return;
    }

    // A master chunk: store it, place its mip pieces.
    const int idx = d.idx;
    if (idx < 0 || idx >= b.n_chunks || d.seq != b.job[idx])
        return; // not the job this slot is waiting on (dropped, superseded)
    b.job[idx] = 0;
    const bool was_counted = b.ready[idx] && !b.dirty[idx];
    const bool was_ready   = b.ready[idx] != 0;
    {
        level_store& s0 = b.lv[0];
        std::vector<std::uint32_t>& dst = s0.px[idx];
        b.ram -= static_cast<long long>(dst.size()) * 4;
        dst = std::move(d.px);
        b.ram += static_cast<long long>(dst.size()) * 4;
        ++s0.ver[idx];
    }
    const int ci = idx % b.lv[0].cw, cj = idx / b.lv[0].cw;
    for (int l = 1; l < L; ++l)
    {
        level_store& s = b.lv[l];
        const int li = ci >> l, lj = cj >> l;
        const int lidx = lj * s.cw + li;
        const int lw = std::min(gb::k_chunk_px, s.geom.W - li * gb::k_chunk_px);
        const int lh = std::min(gb::k_chunk_px, s.geom.H - lj * gb::k_chunk_px);
        std::vector<std::uint32_t>& dst = s.px[lidx];
        if (dst.empty())
        {
            dst.assign(static_cast<std::size_t>(lw) * lh, 0u); // transparent until its pieces land
            b.ram += static_cast<long long>(dst.size()) * 4;
        }
        const int f  = 1 << l;
        const int ox = (ci & (f - 1)) * (gb::k_chunk_px >> l);
        const int oy = (cj & (f - 1)) * (gb::k_chunk_px >> l);
        const int pw = d.pw >> l, ph = d.ph >> l;
        const std::vector<std::uint32_t>& piece = d.mip[l];
        for (int y = 0; y < ph; ++y)
            std::memcpy(dst.data() + static_cast<std::size_t>(oy + y) * lw + ox,
                        piece.data() + static_cast<std::size_t>(y) * pw,
                        static_cast<std::size_t>(pw) * 4u);
        ++s.ver[lidx];
    }
    b.ready[idx] = 1;
    b.baked_hash[idx] = d.hash;
    ++b.bakes[idx];
    // Hashed against an older snapshot than the current table? Then the
    // landing itself says whether it is already stale.
    b.dirty[idx] = b.swept_digest == b.src_digest && b.hashes_epoch != ~0u
                && d.hash != b.want_hash[idx];
    const bool now_counted = !b.dirty[idx];
    b.n_ready += (now_counted ? 1 : 0) - (was_counted ? 1 : 0);
    ++m_bake_counters.chunk_bakes;
    if (was_ready)
        ++m_bake_counters.chunk_rebakes;
}

ground_layer::window ground_layer::window_of(const body_state& b, int level,
                                             const ground_request& req, int margin) const
{
    // The LEVEL-chunk window over the request (li unwrapped; lj clamped).
    window win;
    const level_store& s = b.lv[level];
    if (s.cw <= 0 || s.ch <= 0)
        return win;
    const double sc = s.geom.s / gb::k_chunk_px;
    win.ci_lo = static_cast<int>(std::floor(req.x0 * sc)) - margin;
    win.ci_hi = static_cast<int>(std::floor(req.x1 * sc)) + margin;
    win.cj_lo = std::max(0, static_cast<int>(std::floor((req.y0 - s.geom.y_min) * sc)) - margin);
    win.cj_hi = std::min(s.ch - 1, static_cast<int>(std::floor((req.y1 - s.geom.y_min) * sc)) + margin);
    return win;
}

void ground_layer::feed(bool verify)
{
    if (verify)
        return; // --verify bakes what it draws in tick, and waits for it
    const int cap = 2 * pool_size();
    int queued = waiting();
    if (queued >= cap)
        return;

    // Far pages and sweeps first, for the bodies being worked on.
    const auto housekeep = [&](body_state& b) {
        if ((!b.far_ready || b.far_hash != b.src_digest) && b.far_pending == 0)
        {
            start_far(b);
            queued += b.far_pending;
        }
        bool any = false;
        for (int i = 0; i < b.n_chunks && !any; ++i)
            any = b.ready[i] || b.job[i];
        if (any && b.swept_digest != b.src_digest && b.sweep_job == 0)
        {
            job j = make_sweep_job(b);
            b.sweep_job = j.seq;
            enqueue(std::move(j));
            ++queued;
        }
    };
    body_state* act = find(m_active);
    body_state* pre = m_prebake != m_active ? find(m_prebake) : nullptr;
    if (act)
        housekeep(*act);
    if (pre)
        housekeep(*pre);

    const auto needs = [](const body_state& b, int i) {
        return b.job[i] == 0 && (!b.ready[i] || b.dirty[i]);
    };
    // Every unbaked or dirty chunk of @p b, nearest (cx, cy) first.
    const auto fill_body = [&](body_state& b, double base, double cx, double cy) {
        if (queued >= cap || b.n_ready >= b.n_chunks)
            return;
        struct cand { int i; double d2; };
        std::vector<cand> c;
        const int cw = b.lv[0].cw;
        cx = std::fmod(cx, static_cast<double>(cw));
        if (cx < 0.0)
            cx += cw; // the view centre is unwrapped; the cylinder distance needs it in [0, cw)
        for (int i = 0; i < b.n_chunks; ++i)
            if (needs(b, i))
            {
                double dx = std::fabs((i % cw) - cx);
                dx = std::min(dx, cw - dx); // the cylinder
                const double dy = (i / cw) - cy;
                c.push_back({ i, dx * dx + dy * dy });
            }
        const std::size_t k = std::min(c.size(), static_cast<std::size_t>(cap - queued));
        std::partial_sort(c.begin(), c.begin() + static_cast<std::ptrdiff_t>(k), c.end(),
                          [](const cand& a, const cand& bb) { return a.d2 < bb.d2 || (a.d2 == bb.d2 && a.i < bb.i); });
        for (std::size_t n = 0; n < k; ++n)
        {
            job j = make_master_job(b, c[n].i, base + c[n].d2);
            b.job[c[n].i] = j.seq;
            enqueue(std::move(j));
            ++queued;
        }
    };

    // The active body: the chunks under the view first, then the rest.
    if (act && act->src)
    {
        if (m_view_valid)
        {
            const level_store& s0 = act->lv[0];
            const int f = 1 << m_view_level;
            struct cand { int i; double d2; };
            std::vector<cand> c;
            for (int lj = m_view_win.cj_lo; lj <= m_view_win.cj_hi; ++lj)
                for (int li = m_view_win.ci_lo; li <= m_view_win.ci_hi; ++li)
                    for (int cj = lj * f; cj < (lj + 1) * f && cj < s0.ch; ++cj)
                        for (int cu = li * f; cu < (li + 1) * f; ++cu)
                        {
                            const int ci = ((cu % s0.cw) + s0.cw) % s0.cw;
                            const int i  = cj * s0.cw + ci;
                            if (!needs(*act, i))
                                continue;
                            const double dx = cu - m_view_cx, dy = cj - m_view_cy;
                            c.push_back({ i, dx * dx + dy * dy });
                        }
            std::sort(c.begin(), c.end(), [](const cand& a, const cand& b) {
                return a.d2 < b.d2 || (a.d2 == b.d2 && a.i < b.i);
            });
            for (const cand& e : c)
            {
                if (queued >= cap)
                    break;
                if (!needs(*act, e.i))
                    continue; // a wrap copy already queued it
                job j = make_master_job(*act, e.i, k_prio_view + e.d2);
                act->job[e.i] = j.seq;
                enqueue(std::move(j));
                ++queued;
            }
        }
        fill_body(*act, k_prio_body, m_view_valid ? m_view_cx : 0.0, m_view_valid ? m_view_cy : 0.0);
    }
    if (pre && pre->src)
        fill_body(*pre, k_prio_prebake, pre->lv[0].cw * 0.5, pre->lv[0].ch * 0.5);

    // Background bodies, once play is open: most-visited, then nearest the
    // home body's orbit, first; only where the whole master fits the RAM
    // budget without dropping anything.
    // IO_GROUND_NO_BACKGROUND=1 (a measurement switch): no speculative
    // bakes, so a bench can time a true first visit.
    static const bool no_background = env_flag("IO_GROUND_NO_BACKGROUND");
    if (!m_in_play || queued >= cap || !m_world || no_background)
        return;
    if ((act && act->n_ready < act->n_chunks) || (pre && pre->n_ready < pre->n_chunks))
        return;
    const world& w = *m_world;
    const auto orbit_of = [&w](entity_id id) {
        const auto it = w.bodies.find(id);
        if (it == w.bodies.end())
            return 0.0;
        const body_component& bc = it->second;
        if (bc.parent != null_entity && w.bodies.count(bc.parent))
            return static_cast<double>(w.bodies.at(bc.parent).orbital_radius_au);
        return static_cast<double>(bc.orbital_radius_au);
    };
    const double home_au = orbit_of(w.home_body);
    struct bcand { entity_id id; int visits; double dist; };
    std::vector<bcand> order;
    for (const auto& [id, bc] : w.bodies)
    {
        if (bc.grid_width <= 0 || bc.grid_height <= 0)
            continue;
        const body_state* b = find(id);
        if (b && b->n_ready >= b->n_chunks)
            continue;
        order.push_back({ id, b ? b->visits : 0, std::fabs(orbit_of(id) - home_au) });
    }
    std::sort(order.begin(), order.end(), [](const bcand& a, const bcand& b) {
        if (a.visits != b.visits) return a.visits > b.visits;
        if (a.dist != b.dist) return a.dist < b.dist;
        return a.id < b.id;
    });
    long long total = 0;
    for (const auto& [id, bp] : m_bodies)
        total += bp->ram;
    for (const bcand& c : order)
    {
        if (queued >= cap)
            break;
        body_state* b = find(c.id);
        if (!b)
        {
            // Estimate before allocating anything: geometry only.
            const auto bit = w.bodies.find(c.id);
            const gb::geometry m = gb::make_master_geometry(bit->second.grid_width,
                                                            bit->second.grid_height);
            long long est = 0;
            for (int l = 0; l < L; ++l)
            {
                const gb::geometry gl = gb::level_geometry(m, l);
                est += static_cast<long long>(gl.W) * gl.H * 4;
            }
            if (total + est > k_ram_budget)
                continue;
            b = ensure_body(w, c.id);
            if (!b)
                continue;
            b->background = true;
        }
        else if (total - b->ram + master_bytes(*b) > k_ram_budget)
            continue;
        housekeep(*b);
        fill_body(*b, k_prio_background, b->lv[0].cw * 0.5, b->lv[0].ch * 0.5);
        break; // one background body at a time
    }
}

// ---------------------------------------------------------------------------
// The per-frame drivers
// ---------------------------------------------------------------------------

void ground_layer::pump(const world* wp, bool verify)
{
    if (verify && bench_async_on())
        verify = false;
    ++m_frame;
    drain();
    if (!wp)
    {
        // The pre-bake runs from a world the app does not yet hold (the
        // wizard's cached round-6 world): keep the pool fed, read nothing.
        if (!verify)
            feed(/*verify=*/false);
        return;
    }
    const world& w = *wp;
    m_world = wp;

    // A body that left the world (or changed its grid) is forgotten.
    for (auto it = m_bodies.begin(); it != m_bodies.end();)
    {
        const auto bit = w.bodies.find(it->first);
        if (bit == w.bodies.end() || bit->second.grid_width != it->second->gw
            || bit->second.grid_height != it->second->gh)
        {
            if (it->first == m_active)
            {
                flush_gpu();
                m_active = null_entity;
            }
            if (it->first == m_prebake)
                m_prebake = null_entity;
            it = m_bodies.erase(it);
        }
        else
            ++it;
    }
    if (verify)
        return;

    // Source snapshots on their cadence, for the bodies being drawn or
    // pre-baked; a background body keeps the snapshot its bake began with
    // and is re-taken when it is visited.
    if (body_state* b = find(m_active); b && ++b->src_age >= k_src_cadence)
        refresh_source(*b, w);
    if (m_prebake != m_active)
        if (body_state* b = find(m_prebake); b && ++b->src_age >= k_src_cadence)
            refresh_source(*b, w);

    feed(/*verify=*/false);

    if (m_master_pending && master_complete(m_active))
    {
        m_master_pending = false;
        const body_state* b = find(m_active);
        long long total = 0;
        for (const auto& [id, bp] : m_bodies)
            total += bp->ram;
        std::printf("GROUND_FILL master body %u  %dx%d  %d chunks  %.1f ms from the visit  workers %d"
                    "  RAM body %.2f GB  all bodies %.2f GB\n",
                    static_cast<unsigned>(b->id), b->lv[0].geom.W, b->lv[0].geom.H, b->n_chunks,
                    ms_since(m_visit_t0), pool_size(), b->ram / 1073741824.0, total / 1073741824.0);
        std::fflush(stdout);
    }
    if (m_prebake_pending && master_complete(m_prebake))
    {
        m_prebake_pending = false;
        const body_state* b = find(m_prebake);
        std::printf("GROUND_FILL pre-bake master %dx%d  %d chunks  %.1f ms  workers %d  RAM %.2f GB\n",
                    b->lv[0].geom.W, b->lv[0].geom.H, b->n_chunks, ms_since(m_prebake_t0),
                    pool_size(), b->ram / 1073741824.0);
        std::fflush(stdout);
    }
}

void ground_layer::flush_gpu()
{
    for (auto& [k, g] : m_gpu)
        if (g.tex)
            SDL_DestroyTexture(g.tex);
    m_gpu.clear();
    m_far_tex_ver = ~0u;
    m_publish_keys.clear();
    m_publish_level = -1;
}

bool ground_layer::upload(SDL_Renderer* r, body_state& b, int level, int idx)
{
    const level_store& s = b.lv[level];
    const std::vector<std::uint32_t>& px = s.px[idx];
    if (px.empty())
        return false;
    const int li = idx % s.cw, lj = idx / s.cw;
    const int w = std::min(gb::k_chunk_px, s.geom.W - li * gb::k_chunk_px);
    const int h = std::min(gb::k_chunk_px, s.geom.H - lj * gb::k_chunk_px);
    const std::uint32_t key = (static_cast<std::uint32_t>(level) << 24) | static_cast<std::uint32_t>(idx);
    gpu_chunk& g = m_gpu[key];
    if (g.tex && (g.w != w || g.h != h))
    {
        SDL_DestroyTexture(g.tex);
        g.tex = nullptr;
    }
    if (!g.tex)
    {
        g.tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        if (!g.tex)
        {
            m_gpu.erase(key); // transient failure: retried next frame
            return false;
        }
        g.w = w;
        g.h = h;
        SDL_SetTextureScaleMode(g.tex, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(g.tex, SDL_BLENDMODE_BLEND);
    }
    SDL_UpdateTexture(g.tex, nullptr, px.data(), w * 4);
    g.ver = s.ver[idx];
    g.last_used = m_frame;
    {
        std::lock_guard lk(m_mx);
        ++m_stats.uploads;
    }
    return true;
}

void ground_layer::evict_gpu()
{
    if (m_gpu.size() <= k_gpu_cap)
        return;
    std::vector<std::pair<std::uint64_t, std::uint32_t>> age;
    age.reserve(m_gpu.size());
    for (const auto& [k, g] : m_gpu)
        if (g.last_used < m_frame)
            age.push_back({ g.last_used, k });
    std::sort(age.begin(), age.end());
    for (std::size_t i = 0; i < age.size() && m_gpu.size() > k_gpu_cap; ++i)
    {
        const auto it = m_gpu.find(age[i].second);
        if (it->second.tex)
            SDL_DestroyTexture(it->second.tex);
        m_gpu.erase(it);
    }
}

void ground_layer::tick(SDL_Renderer* r, const world& w, ui_state& ui, bool bake_everything)
{
    // IO_GROUND_BENCH: the pool path under --verify, so a script can time a
    // real fill. Off by default; a capture taken in this mode can race.
    if (bake_everything && bench_async_on())
        bake_everything = false;
    if (!bake_everything)
        m_in_play = true;
    m_world = &w;

    const entity_id body = ui.active_body;
    if (body != m_active)
    {
        flush_gpu();
        m_active = body;
        if (body_state* nb = ensure_body(w, body))
        {
            ++nb->visits;
            nb->background = false;
            if (nb->src_age > 0)
                refresh_source(*nb, w); // a visit re-takes the snapshot
            if (fill_log_on())
            {
                m_visit_pending = true;
                m_master_pending = nb->n_ready < nb->n_chunks;
                m_far_pending = !nb->far_ready;
                m_visit_t0 = clock::now();
            }
        }
    }
    body_state* bp = find(m_active);
    if (!bp)
    {
        ui.ground = ground_view{};
        m_view_valid = false;
        return;
    }
    body_state& b = *bp;
    b.last_visit = m_frame;
    make_room(master_bytes(b), m_active, m_prebake);

    if (bake_everything)
        refresh_source(b, w); // a capture sees this frame's world

    // --- The request: the drawn level and its window ---
    const ground_request& req = ui.ground_req;
    const bool have_view = req.valid && req.body == m_active && req.draw_r > 0.0f;
    int level = -1;
    window vis{}, ring{};
    if (have_view)
    {
        level = gb::choose_level(req.draw_r);
        vis   = window_of(b, level, req, 0);
        ring  = window_of(b, level, req, 1);
        if (level != m_publish_level && fill_log_on())
        {
            m_fill_pending = true;
            m_fill_t0 = clock::now();
            m_fill_level = level;
        }
        m_view_valid = true;
        m_view_level = level;
        m_view_win   = vis;
        const double sc = b.lv[0].geom.s / gb::k_chunk_px; // master chunks per canonical unit
        m_view_cx = 0.5 * (req.x0 + req.x1) * sc - 0.5;
        m_view_cy = (0.5 * (req.y0 + req.y1) - b.lv[0].geom.y_min) * sc - 0.5;
    }
    else
        m_view_valid = false;

    // The master chunks under the visible window (unwrapped ci, deduped).
    const auto under_view = [&](const window& v) {
        std::vector<int> out;
        const level_store& s0 = b.lv[0];
        const int f = 1 << level;
        std::vector<std::uint8_t> seen(static_cast<std::size_t>(b.n_chunks), 0);
        for (int lj = v.cj_lo; lj <= v.cj_hi; ++lj)
            for (int cj = lj * f; cj < (lj + 1) * f && cj < s0.ch; ++cj)
                for (int li = v.ci_lo; li <= v.ci_hi; ++li)
                    for (int cu = li * f; cu < (li + 1) * f; ++cu)
                    {
                        const int i = cj * s0.cw + ((cu % s0.cw) + s0.cw) % s0.cw;
                        if (!seen[i])
                        {
                            seen[i] = 1;
                            out.push_back(i);
                        }
                    }
        return out;
    };

    if (bake_everything)
    {
        // --- THE --verify PATH: everything this frame draws, complete ---
        if (!b.far_ready || b.far_hash != b.src_digest)
        {
            if (b.far_pending == 0)
                start_far(b);
            wait_until([&] { return b.far_pending == 0; });
        }
        bool any = false;
        for (int i = 0; i < b.n_chunks && !any; ++i)
            any = b.ready[i] != 0;
        if (any && b.swept_digest != b.src_digest)
        {
            job j = make_sweep_job(b);
            b.sweep_job = j.seq;
            bake_sync(std::move(j));
        }
        if (have_view)
        {
            // Bake on the pool and WAIT: the bake is pure and results land by
            // slot, so this is the serial result, only sooner.
            std::vector<int> need;
            for (const int i : under_view(vis)) // raster order
                if (b.job[i] == 0 && (!b.ready[i] || b.dirty[i]))
                    need.push_back(i);
            std::sort(need.begin(), need.end());
            if (verify_fill_limit >= 0 && static_cast<int>(need.size()) > verify_fill_limit)
                need.resize(static_cast<std::size_t>(verify_fill_limit));
            for (const int i : need)
            {
                job j = make_master_job(b, i, static_cast<double>(i));
                b.job[i] = j.seq;
                enqueue(std::move(j));
            }
            wait_until([&] {
                for (const int i : need)
                    if (b.job[i] != 0)
                        return false;
                return true;
            });
        }
    }

    // --- The Selection band's neighbourhood page (BL-1241) ---
    // A flat close-tier bake of the selected tile's neighbourhood: the band
    // is a plan view of the selection, not the canvas camera.
    {
        if (m_neigh_body != m_active)
        {
            m_neigh_body = m_active;
            m_neigh_geom = gb::make_geometry(b.gw, b.gh, k_neigh_px_per_r);
            m_neigh_ready = false;
            m_neigh_job = 0;
            m_neigh_want = 0;
            m_neigh_tile = null_entity;
            m_neigh_upload = false;
        }
        const ground_neigh_request& nq = ui.ground_neigh_req;
        if (nq.valid && nq.body == m_active && b.src && m_neigh_geom.W > 0 && m_neigh_job == 0)
        {
            constexpr double kSqrt3_ = 1.7320508075688772;
            const gb::geometry& g = m_neigh_geom;
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
            std::uint64_t h = gb::region_hash(*b.src, g, px0, py0, pw, ph);
            // The window AND the subject (F34): py0 clamps near the top edge,
            // so two rows can share a window and its hash.
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
                j.kind = job_kind::neigh;
                j.body = m_active;
                j.seq  = next_seq();
                j.prio = k_prio_neigh;
                j.px0 = px0; j.py0 = py0; j.pw = pw; j.ph = ph;
                j.src = b.src; j.geom = g; j.prm = params;
                j.hash = h;
                j.neigh_tile = nq.tile;
                j.neigh_rect[0] = static_cast<float>(px0 / g.s);
                j.neigh_rect[1] = static_cast<float>(py0 / g.s + g.y_min);
                j.neigh_rect[2] = static_cast<float>((px0 + pw) / g.s);
                j.neigh_rect[3] = static_cast<float>((py0 + ph) / g.s + g.y_min);
                m_neigh_want = h;
                m_neigh_job = j.seq;
                if (bake_everything)
                    bake_sync(std::move(j));
                else
                    enqueue(std::move(j));
            }
        }
        if (m_neigh_upload)
        {
            m_neigh_upload = false;
            if (m_neigh)
            {
                float fw = 0, fh = 0;
                SDL_GetTextureSize(m_neigh, &fw, &fh);
                if (static_cast<int>(fw) != m_neigh_w || static_cast<int>(fh) != m_neigh_h)
                {
                    SDL_DestroyTexture(m_neigh);
                    m_neigh = nullptr;
                }
            }
            if (!m_neigh)
            {
                m_neigh = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                            m_neigh_w, m_neigh_h);
                if (m_neigh)
                {
                    SDL_SetTextureScaleMode(m_neigh, SDL_SCALEMODE_LINEAR);
                    SDL_SetTextureBlendMode(m_neigh, SDL_BLENDMODE_BLEND);
                }
            }
            if (m_neigh)
            {
                SDL_UpdateTexture(m_neigh, nullptr, m_neigh_px.data(), m_neigh_w * 4);
                m_neigh_ready = true;
            }
            else
            {
                m_neigh_ready = false;
                m_neigh_want  = 0; // transient failure: re-requested next tick
            }
        }
    }

    // --- The far page texture ---
    if (b.far_ready && m_far_tex_ver != b.far_ver)
    {
        if (m_far_tex)
        {
            float fw = 0, fh = 0;
            SDL_GetTextureSize(m_far_tex, &fw, &fh);
            if (static_cast<int>(fw) != b.far_geom.W || static_cast<int>(fh) != b.far_geom.H)
            {
                SDL_DestroyTexture(m_far_tex);
                m_far_tex = nullptr;
            }
        }
        if (!m_far_tex)
        {
            m_far_tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                          b.far_geom.W, b.far_geom.H);
            if (m_far_tex)
            {
                SDL_SetTextureScaleMode(m_far_tex, SDL_SCALEMODE_LINEAR);
                SDL_SetTextureBlendMode(m_far_tex, SDL_BLENDMODE_BLEND);
            }
        }
        if (m_far_tex)
        {
            SDL_UpdateTexture(m_far_tex, nullptr, b.far_px.data(), b.far_geom.W * 4);
            m_far_tex_ver = b.far_ver;
        }
    }
    if (m_far_pending && b.far_ready)
    {
        m_far_pending = false;
        std::printf("GROUND_FILL far page %dx%d  %.1f ms  workers %d\n", b.far_geom.W,
                    b.far_geom.H, ms_since(m_visit_t0), pool_size());
        std::fflush(stdout);
    }

    // --- Uploads: the drawn level's visible chunks, then the ring, then the
    // adjacent levels (prefetch, so a rung change finds them resident) ---
    m_publish_keys.clear();
    m_publish_level = level;
    m_pending_uploads = 0;
    if (have_view)
    {
        int budget = bake_everything ? (1 << 30) : k_upload_budget;
        const auto visit = [&](int lv, const window& v, bool publish, bool count) {
            const level_store& s = b.lv[lv];
            struct cand { int idx; double d2; };
            std::vector<cand> c;
            const double cx = 0.5 * (v.ci_lo + v.ci_hi), cy = 0.5 * (v.cj_lo + v.cj_hi);
            for (int lj = v.cj_lo; lj <= v.cj_hi; ++lj)
                for (int li = v.ci_lo; li <= v.ci_hi; ++li)
                {
                    const int idx = lj * s.cw + ((li % s.cw) + s.cw) % s.cw;
                    c.push_back({ idx, (li - cx) * (li - cx) + (lj - cy) * (lj - cy) });
                }
            std::stable_sort(c.begin(), c.end(), [](const cand& a, const cand& bb) { return a.d2 < bb.d2; });
            for (const cand& e : c)
            {
                const std::uint32_t key = (static_cast<std::uint32_t>(lv) << 24)
                                        | static_cast<std::uint32_t>(e.idx);
                if (std::find(m_publish_keys.begin(), m_publish_keys.end(), key) != m_publish_keys.end())
                    continue; // a wrap copy of a chunk already handled
                const auto it = m_gpu.find(key);
                const bool on_gpu = it != m_gpu.end() && it->second.tex;
                const bool fresh  = on_gpu && it->second.ver == s.ver[e.idx];
                if (!fresh && !s.px[e.idx].empty())
                {
                    if (budget > 0 && upload(r, b, lv, e.idx))
                        --budget;
                    else if (count)
                        ++m_pending_uploads;
                }
                const auto jt = m_gpu.find(key);
                if (jt != m_gpu.end() && jt->second.tex)
                {
                    jt->second.last_used = m_frame;
                    if (publish)
                        m_publish_keys.push_back(key);
                }
            }
        };
        visit(level, vis, true, true);
        if (!bake_everything)
        {
            // The ring (a pan reveals it) and the adjacent levels (a rung
            // change reads one): uploaded with what the budget leaves.
            visit(level, ring, true, false);
            for (const int adj : { level - 1, level + 1 })
                if (adj >= 0 && adj < L)
                    visit(adj, window_of(b, adj, req, 0), false, false);
        }
        evict_gpu();

        // Fill timing: the drawn level's visible chunks all final.
        if ((m_fill_pending || m_visit_pending || m_rebake_pending) && m_pending_uploads == 0)
        {
            bool final_ = true;
            for (const int i : under_view(vis))
                final_ = final_ && b.ready[i] && !b.dirty[i];
            const level_store& s = b.lv[level];
            for (int lj = vis.cj_lo; lj <= vis.cj_hi && final_; ++lj)
                for (int li = vis.ci_lo; li <= vis.ci_hi && final_; ++li)
                {
                    const int idx = lj * s.cw + ((li % s.cw) + s.cw) % s.cw;
                    const auto it = m_gpu.find((static_cast<std::uint32_t>(level) << 24)
                                               | static_cast<std::uint32_t>(idx));
                    final_ = it != m_gpu.end() && it->second.ver == s.ver[idx];
                }
            if (final_)
            {
                if (m_fill_pending)
                    std::printf("GROUND_FILL level %d (%.0f px/r)  draw_r %.1f  %.1f ms  workers %d\n",
                                level, gb::k_level_ppr[level], req.draw_r, ms_since(m_fill_t0),
                                pool_size());
                if (m_visit_pending)
                    std::printf("GROUND_FILL visit body %u  view final in %.1f ms  (master %d/%d)\n",
                                static_cast<unsigned>(b.id), ms_since(m_visit_t0), b.n_ready,
                                b.n_chunks);
                if (m_rebake_pending)
                    std::printf("GROUND_FILL re-bake  view final %.1f ms after the source moved\n",
                                ms_since(m_rebake_t0));
                std::fflush(stdout);
                m_fill_pending = m_visit_pending = m_rebake_pending = false;
            }
        }
    }

    feed(bake_everything);
    publish(ui, &b);
}

void ground_layer::publish(ui_state& ui, const body_state* bp) const
{
    ground_view& v = ui.ground;
    v = ground_view{};
    if (!bp)
        return;
    const body_state& b = *bp;
    v.body      = b.id;
    v.far_ready = b.far_ready && m_far_tex && m_far_tex_ver == b.far_ver;
    v.far.tex   = v.far_ready ? m_far_tex : nullptr;
    v.far.x0    = 0.0f;
    v.far.x1    = static_cast<float>(b.far_geom.W / b.far_geom.s);
    v.far.y0    = static_cast<float>(b.far_geom.y_min);
    v.far.y1    = static_cast<float>(b.far_geom.H / b.far_geom.s + b.far_geom.y_min);
    if (m_neigh_ready && m_neigh && m_neigh_body == b.id)
    {
        v.neigh.tex = m_neigh;
        v.neigh.x0  = m_neigh_rect[0];
        v.neigh.y0  = m_neigh_rect[1];
        v.neigh.x1  = m_neigh_rect[2];
        v.neigh.y1  = m_neigh_rect[3];
        v.neigh_tile = m_neigh_tile;
    }
    if (m_publish_level < 0)
        return;
    const level_store& s = b.lv[m_publish_level];
    v.level_ppr = gb::k_level_ppr[m_publish_level];
    v.chunks.reserve(m_publish_keys.size());
    for (const std::uint32_t key : m_publish_keys)
    {
        const auto it = m_gpu.find(key);
        if (it == m_gpu.end() || !it->second.tex)
            continue;
        const int idx = static_cast<int>(key & 0xFFFFFFu);
        const int li = idx % s.cw, lj = idx / s.cw;
        const int px0 = li * gb::k_chunk_px, py0 = lj * gb::k_chunk_px;
        ground_chunk_view cv;
        cv.x0  = static_cast<float>(px0 / s.geom.s);
        cv.x1  = static_cast<float>((px0 + it->second.w) / s.geom.s);
        cv.y0  = static_cast<float>(py0 / s.geom.s + s.geom.y_min);
        cv.y1  = static_cast<float>((py0 + it->second.h) / s.geom.s + s.geom.y_min);
        cv.tex = it->second.tex;
        v.chunks.push_back(cv);
    }
}

// ---------------------------------------------------------------------------
// Instrumentation
// ---------------------------------------------------------------------------

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
    s.ppr[0] = gb::k_far_ppr;
    for (int l = 0; l < L && 1 + l < k_stat_slots; ++l)
        s.ppr[1 + l] = gb::k_level_ppr[l];
    for (const auto& [id, bp] : m_bodies)
    {
        s.ram_total += bp->ram;
        if (bp->ram > static_cast<long long>(bp->far_px.size()) * 4)
            ++s.bodies_resident;
    }
    if (const body_state* b = find(m_active))
    {
        s.ram_bytes[0] = static_cast<long long>(b->far_px.size()) * 4;
        for (int l = 0; l < L; ++l)
            for (const auto& px : b->lv[l].px)
                s.ram_bytes[1 + l] += static_cast<long long>(px.size()) * 4;
        s.master_ready = b->n_ready;
        s.master_total = b->n_chunks;
        if (m_far_tex && b->far_ready)
            s.gpu_bytes[0] = static_cast<long long>(b->far_geom.W) * b->far_geom.H * 4;
    }
    for (const auto& [key, g] : m_gpu)
        if (g.tex)
            s.gpu_bytes[1 + (key >> 24)] += static_cast<long long>(g.w) * g.h * 4;
    s.active_slot = m_publish_level >= 0 ? 1 + m_publish_level : 0;
    s.pending_uploads = m_pending_uploads;
    return s;
}

std::vector<ground_layer::chunk_probe> ground_layer::probe_chunks() const
{
    std::vector<chunk_probe> out;
    const body_state* bp = find(m_active);
    if (!bp || !bp->src)
        return out;
    const body_state& b = *bp;
    if (b.far_ready)
    {
        chunk_probe p;
        p.tier  = -1;
        p.key   = 0;
        p.bakes = m_bake_counters.far_bakes;
        p.installation_hash = gb::installation_hash(*b.src, b.far_geom, 0, 0, b.far_geom.W, b.far_geom.H);
        p.region_hash = b.src_digest;
        out.push_back(p);
    }
    if (!m_view_valid)
        return out;
    // The ready master chunks under the last view.
    const level_store& s0 = b.lv[0];
    const int f = 1 << m_view_level;
    std::vector<std::uint8_t> seen(static_cast<std::size_t>(b.n_chunks), 0);
    for (int lj = m_view_win.cj_lo; lj <= m_view_win.cj_hi; ++lj)
        for (int cj = lj * f; cj < (lj + 1) * f && cj < s0.ch; ++cj)
            for (int li = m_view_win.ci_lo; li <= m_view_win.ci_hi; ++li)
                for (int cu = li * f; cu < (li + 1) * f; ++cu)
                {
                    const int i = cj * s0.cw + ((cu % s0.cw) + s0.cw) % s0.cw;
                    if (seen[i] || !b.ready[i])
                        continue;
                    seen[i] = 1;
                    const int px0 = (i % s0.cw) * gb::k_chunk_px, py0 = cj * gb::k_chunk_px;
                    const int pw = std::min(gb::k_chunk_px, s0.geom.W - px0);
                    const int ph = std::min(gb::k_chunk_px, s0.geom.H - py0);
                    chunk_probe p;
                    p.tier  = 0;
                    p.key   = static_cast<std::uint32_t>(i);
                    p.bakes = b.bakes[i];
                    p.installation_hash = gb::installation_hash(*b.src, s0.geom, px0, py0, pw, ph);
                    p.region_hash       = gb::region_hash(*b.src, s0.geom, px0, py0, pw, ph);
                    out.push_back(p);
                }
    std::sort(out.begin(), out.end(), [](const chunk_probe& a, const chunk_probe& c) {
        return a.tier != c.tier ? a.tier < c.tier : a.key < c.key;
    });
    return out;
}

std::uint64_t ground_layer::installation_digest() const
{
    const body_state* b = find(m_active);
    if (!b || !b->src || b->far_geom.W <= 0)
        return 0;
    return gb::installation_hash(*b->src, b->far_geom, 0, 0, b->far_geom.W, b->far_geom.H);
}
