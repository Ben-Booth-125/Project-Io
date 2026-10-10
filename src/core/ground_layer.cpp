#include "ground_layer.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#include <process.h> // _getpid
// The executable's path, for the cache stamp. Declared here rather than via
// <windows.h>, whose `near` / `far` macros would rewrite job_kind::far.
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void* module, wchar_t* name,
                                                                            unsigned long size);
#else
#include <unistd.h>  // getpid
#endif

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
/// IO_GROUND_NO_PATCH=1 (a measurement switch, BL-1246): every re-bake is a
/// whole chunk, as before the partial re-bake — the "before" of its reading.
bool patching_on()
{
    static const bool on = !env_flag("IO_GROUND_NO_PATCH");
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

// ---------------------------------------------------------------------------
// The disk cache's file format (BL-1259). One file per master chunk:
//
//   header (56 bytes): "IOGC", u32 format, u64 stamp, u32 body, u32 chunk
//   index, i32 width, i32 height, u64 content hash (the region_hash the chunk
//   was baked against), u32 levels, u32 payload bytes, u64 payload check;
//   payload: the chunk's piece of every level (master first), each encoded
//   as below.
//
// A piece is mode 1 when every fully transparent pixel is exactly 0 — its
// alpha plane run-length coded (alpha byte, varint run) and the RGB bytes of
// the non-transparent pixels raw — else mode 0, raw RGBA. Lossless either way;
// the alpha plane of baked ground is almost entirely runs of 255 (and the
// transparent margin above and below the body), so mode 1 is ~3/4 the size.
// The check is over the payload bytes: a truncated or bit-rotted file fails
// it, and a failed load re-bakes.
// ---------------------------------------------------------------------------
constexpr std::uint32_t k_cache_format = 1;
constexpr std::size_t   k_header_bytes = 56;

std::uint64_t fold64(std::uint64_t h, std::uint64_t v)
{
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    h *= 0xBF58476D1CE4E5B9ull;
    return h ^ (h >> 31);
}

/// A fast 64-bit check over bytes (8 at a time).
std::uint64_t bytes_check(const std::uint8_t* p, std::size_t n, std::uint64_t h = 0x243F6A8885A308D3ull)
{
    std::size_t i = 0;
    for (; i + 8 <= n; i += 8)
    {
        std::uint64_t w;
        std::memcpy(&w, p + i, 8);
        h ^= w;
        h *= 0x9E3779B97F4A7C15ull;
        h ^= h >> 29;
    }
    std::uint64_t tail = 0;
    for (std::size_t k = 0; i < n; ++i, ++k)
        tail |= static_cast<std::uint64_t>(p[i]) << (8 * k);
    return fold64(h, tail ^ (static_cast<std::uint64_t>(n) << 3));
}

template <class T> void put(std::vector<std::uint8_t>& out, T v)
{
    const std::size_t at = out.size();
    out.resize(at + sizeof(T));
    std::memcpy(out.data() + at, &v, sizeof(T));
}
template <class T> T get_at(const std::uint8_t* p, std::size_t at)
{
    T v;
    std::memcpy(&v, p + at, sizeof(T));
    return v;
}
void put_varint(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    while (v >= 0x80)
    {
        out.push_back(static_cast<std::uint8_t>(v | 0x80));
        v >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(v));
}
bool get_varint(const std::uint8_t*& p, const std::uint8_t* end, std::uint32_t& v)
{
    v = 0;
    for (int shift = 0; shift < 35; shift += 7)
    {
        if (p >= end)
            return false;
        const std::uint8_t b = *p++;
        v |= static_cast<std::uint32_t>(b & 0x7F) << shift;
        if (!(b & 0x80))
            return true;
    }
    return false;
}

/// Encode @p n pixels (RGBA32, alpha the high byte of the word).
void encode_piece(const std::uint32_t* px, std::size_t n, std::vector<std::uint8_t>& out)
{
    bool clean = true; // every alpha-0 pixel is exactly 0
    for (std::size_t i = 0; i < n && clean; ++i)
        clean = (px[i] >> 24) != 0 || px[i] == 0;
    if (!clean)
    {
        out.push_back(0);
        const std::size_t at = out.size();
        out.resize(at + n * 4);
        std::memcpy(out.data() + at, px, n * 4);
        return;
    }
    out.push_back(1);
    std::size_t i = 0;
    while (i < n)
    {
        const std::uint8_t a = static_cast<std::uint8_t>(px[i] >> 24);
        std::size_t j = i + 1;
        while (j < n && static_cast<std::uint8_t>(px[j] >> 24) == a)
            ++j;
        out.push_back(a);
        put_varint(out, static_cast<std::uint32_t>(j - i));
        i = j;
    }
    for (std::size_t k = 0; k < n; ++k)
        if (px[k] >> 24)
        {
            out.push_back(static_cast<std::uint8_t>(px[k]));
            out.push_back(static_cast<std::uint8_t>(px[k] >> 8));
            out.push_back(static_cast<std::uint8_t>(px[k] >> 16));
        }
}

bool decode_piece(const std::uint8_t*& p, const std::uint8_t* end, std::uint32_t* dst, std::size_t n)
{
    if (p >= end)
        return false;
    const std::uint8_t mode = *p++;
    if (mode == 0)
    {
        if (static_cast<std::size_t>(end - p) < n * 4)
            return false;
        std::memcpy(dst, p, n * 4);
        p += n * 4;
        return true;
    }
    if (mode != 1)
        return false;
    std::size_t i = 0;
    while (i < n)
    {
        if (p >= end)
            return false;
        const std::uint32_t a = *p++;
        std::uint32_t run = 0;
        if (!get_varint(p, end, run) || run == 0 || run > n - i)
            return false;
        for (std::uint32_t k = 0; k < run; ++k)
            dst[i + k] = a << 24;
        i += run;
    }
    for (std::size_t k = 0; k < n; ++k)
        if (dst[k] >> 24)
        {
            if (end - p < 3)
                return false;
            dst[k] |= static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
                    | (static_cast<std::uint32_t>(p[2]) << 16);
            p += 3;
        }
    return true;
}

std::filesystem::path utf8_path(const char* s)
{
    return std::filesystem::path(reinterpret_cast<const char8_t*>(s));
}
std::string path_utf8(const std::filesystem::path& p)
{
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::filesystem::path chunk_file(const std::filesystem::path& dir, int idx)
{
    char name[32];
    std::snprintf(name, sizeof name, "c%05d.gch", idx);
    return dir / name;
}

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
    case job_kind::load:
    {
        // BL-1259: a spilled chunk back from the disk cache. A file that is
        // missing, foreign or corrupt lands as a failure and re-bakes.
        const auto t0 = std::chrono::steady_clock::now();
        d.loaded = load_chunk_file(j, d);
        if (!d.loaded)
        {
            d.px.clear();
            for (auto& m : d.mip)
                m.clear();
        }
        d.ms = ms_since(t0);
        return;
    }
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
        const auto t_job = t0;
        d.src = j.src;
        d.rebake = j.old_src != nullptr;
        // THE PARTIAL RE-BAKE (BL-1246): a stored chunk whose installations
        // alone moved re-bakes only the windows around them — the window rule
        // is ui::ground::installation_patch_rects — and derives only their mip
        // pieces; the render thread blits both over what it holds.
        if (j.old_src)
        {
            std::vector<gb::pixel_rect> rects;
            if (gb::installation_patch_rects(*j.old_src, *j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph, rects))
            {
                d.patched = true;
                d.patches.resize(rects.size());
                for (std::size_t k = 0; k < rects.size(); ++k)
                {
                    const gb::pixel_rect& r = rects[k];
                    result::patch& pc = d.patches[k];
                    pc.r = { r.x0 - j.px0, r.y0 - j.py0, r.w, r.h };
                    pc.px.assign(static_cast<std::size_t>(r.w) * r.h, 0u);
                    gb::bake_region(*j.src, j.geom, j.prm, r.x0, r.y0, r.w, r.h, pc.px.data());
                    gb::derive_mip_pieces(pc.px.data(), r.w, r.h, pc.mip);
                }
                d.hash = gb::region_hash(*j.src, j.geom, j.px0, j.py0, j.pw, j.ph);
                d.ms = ms_since(t_job);
                if (level_ms)
                    level_ms[0] = d.ms;
                return;
            }
        }
        d.px.assign(static_cast<std::size_t>(j.pw) * j.ph, 0u);
        gb::bake_region(*j.src, j.geom, j.prm, j.px0, j.py0, j.pw, j.ph, d.px.data());
        d.hash = gb::region_hash(*j.src, j.geom, j.px0, j.py0, j.pw, j.ph);
        if (level_ms)
            level_ms[0] = ms_since(t0);
        // A master chunk normally bakes in well under a second; one far past
        // that is a worker held off the CPU (the pool runs below normal
        // priority) or a pathological window — either way it is the wait's
        // tail, so the fill log names it.
        if (fill_log_on() && ms_since(t0) > 1500.0)
        {
            std::printf("GROUND_FILL slow chunk %d (px %d,%d): %.0f ms\n", j.idx, j.px0, j.py0,
                        ms_since(t0));
            std::fflush(stdout);
        }
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
        d.ms = ms_since(t_job);
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
    drain_disk();
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
    b.baked_src.assign(n, nullptr);
    b.job.assign(n, 0);
    b.ready.assign(n, 0);
    b.dirty.assign(n, 0);
    b.disk_has.assign(n, 0);
    b.disk_hash.assign(n, 0);
    b.disk_same.assign(n, 0);
    b.disk_pending.assign(n, 0);
    b.disk_size.assign(n, 0);
    b.disk_raw.assign(n, 0);
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
    set_source(b, std::make_shared<const gb::bake_source>(
                      gb::prepare_source(w, b.id, b.reveal_all, registry)));
}

void ground_layer::set_source(body_state& b, std::shared_ptr<const gb::bake_source> src)
{
    b.src = std::move(src);
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
    if (dg != b.src_digest && fill_log_on() && b.n_ready > 0)
    {
        std::printf("GROUND_FILL source moved: body %u frame %llu (%d of %d ready)\n",
                    static_cast<unsigned>(b.id), static_cast<unsigned long long>(m_frame),
                    b.n_ready, b.n_chunks);
        std::fflush(stdout);
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
            return !(j.body == b.id && (j.kind == job_kind::master || j.kind == job_kind::load));
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
    std::fill(b.baked_src.begin(), b.baked_src.end(), nullptr);
    std::fill(b.ready.begin(), b.ready.end(), 0);
    std::fill(b.dirty.begin(), b.dirty.end(), 0);
    b.n_ready = 0;
    b.ram = static_cast<long long>(b.far_px.size()) * 4;
    b.background = false;
}

long long ground_layer::budget_total() const
{
    long long total = 0;
    for (const auto& [id, bp] : m_bodies)
        if (!pinned(id))
            total += bp->ram;
    return total;
}

void ground_layer::make_room(long long need, entity_id keep_a, entity_id keep_b)
{
    // BL-1259: the home body is pinned — outside the budget, never a victim.
    long long total = budget_total();
    // `need` is what @p keep_a will hold once whole; count what it holds now
    // as part of it.
    if (pinned(keep_a))
        need = 0;
    else if (const body_state* k = find(keep_a))
        total -= k->ram;
    while (total + need > ram_budget)
    {
        body_state* victim = nullptr;
        for (auto& [id, bp] : m_bodies)
        {
            body_state& c = *bp;
            if (c.id == keep_a || c.id == keep_b || pinned(c.id))
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
        if (m_cache_live)
            cache_init();
        if (cache_live())
            spill_master(*victim, /*sync=*/false, nullptr); // prints its own line
        else
        {
            drop_master(*victim);
            std::printf("[ground] RAM budget: dropped the master of body %u (%.2f GB freed)\n",
                        static_cast<unsigned>(victim->id), (before - victim->ram) / 1073741824.0);
            std::fflush(stdout);
        }
        total -= before - victim->ram;
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
    // BL-1259: the cache is this world's: its files go (on the writer, after
    // any write still queued), and a writer result still in flight lands in
    // nothing.
    ++m_world_gen;
    for (const std::filesystem::path& dir : m_session_dirs)
    {
        disk_task t;
        t.k = disk_task::kind::remove_dir;
        t.dir = dir;
        std::lock_guard lk(m_wmx);
        m_wtasks.push_back(std::move(t));
    }
    if (!m_session_dirs.empty())
    {
        m_session_dirs.clear();
        if (!m_writer.joinable())
            m_writer = std::thread([this] { writer_main(); });
        m_wcv.notify_one();
    }
    m_active = null_entity;
    m_prebake = null_entity;
    m_home = null_entity;
    m_boundary_log = false;
    m_boundary_track = false;
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

void ground_layer::prebake(const world& w, entity_id body, bool assume_surveyed)
{
    body_state* b = ensure_body(w, body);
    if (!b)
        return;
    if (b->reveal_all != assume_surveyed)
    {
        b->reveal_all = assume_surveyed;
        refresh_source(*b, w);
    }
    m_prebake = body;
    m_home = body; // the pre-bake target is the homeworld: pinned (BL-1259)
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

bool ground_layer::master_current(entity_id body) const
{
    const body_state* b = find(body);
    return b && b->n_chunks > 0 && b->n_ready >= b->n_chunks && b->sweep_job == 0
        && b->swept_digest == b->src_digest;
}

bool ground_layer::resnapshot(const world& w, bool assume_surveyed)
{
    body_state* b = find(m_prebake);
    if (!b)
        return false;
    const auto bit = w.bodies.find(b->id);
    if (bit == w.bodies.end() || bit->second.grid_width != b->gw
        || bit->second.grid_height != b->gh)
        return false; // the homeworld is not this one any more
    b->reveal_all = assume_surveyed;
    land_boundary(*b, std::make_shared<const gb::bake_source>(
                          gb::prepare_source(w, b->id, b->reveal_all, registry)),
                  "round-boundary");
    return true;
}

bool ground_layer::resnapshot(entity_id body, std::shared_ptr<const gb::bake_source> src,
                              bool assume_surveyed)
{
    body_state* b = find(m_prebake);
    if (!b || !src || b->id != body || src->gw != b->gw || src->gh != b->gh)
        return false;
    b->reveal_all = assume_surveyed;
    land_boundary(*b, std::move(src), "in-round (roads laid)");
    return true;
}

void ground_layer::land_boundary(body_state& bs, std::shared_ptr<const gb::bake_source> src,
                                 const char* what)
{
    body_state* const b = &bs;
    const std::uint64_t before = b->src_digest;
    const std::shared_ptr<const gb::bake_source> old_src = b->src;
    set_source(*b, std::move(src));
    if (fill_log_on() && old_src && b->src && b->src_digest != before)
    {
        // What the boundary moved, per source field, in tiles.
        const gb::bake_source& o = *old_src;
        const gb::bake_source& n = *b->src;
        const auto diff = [](const auto& x, const auto& y) {
            if (x.size() != y.size()) return -1;
            int c = 0;
            for (std::size_t i = 0; i < x.size(); ++i) c += !(x[i] == y[i]);
            return c;
        };
        std::printf("GROUND_FILL boundary diff (tiles): cls %d colour %d height %d cover %d "
                    "density %d landform %d river_in %d family %d vparam %d installations %zu -> %zu "
                    "road %d lane %d (route pieces %zu -> %zu)\n",
                    diff(o.cls, n.cls), diff(o.colour, n.colour), diff(o.height, n.height),
                    diff(o.cover, n.cover), diff(o.density, n.density), diff(o.landform, n.landform),
                    diff(o.river_in, n.river_in), diff(o.family, n.family), diff(o.vparam, n.vparam),
                    o.inst.list.size(), n.inst.list.size(), diff(o.road, n.road), diff(o.lane, n.lane),
                    o.route_pieces.size(), n.route_pieces.size());
        std::fflush(stdout);
    }
    if (b->src_digest != before)
        m_boundary_log = true; // name the sweep this boundary causes
    std::printf("[ground] %s snapshot of body %u: %s (%d of %d master chunks landed)\n",
                what, static_cast<unsigned>(b->id),
                b->src_digest == before ? "unchanged, nothing re-bakes" : "moved, sweeping",
                b->n_ready, b->n_chunks);
    std::fflush(stdout);
    feed(/*verify=*/false);
}

void ground_layer::touch()
{
    if (body_state* b = find(m_active))
        b->src_age = k_src_cadence;
}

bool ground_layer::pool_path_under_verify()
{
    return bench_async_on();
}

bool ground_layer::complete_master(const world& w, entity_id body, double timeout_ms)
{
    body_state* bp = ensure_body(w, body);
    if (!bp)
        return false;
    body_state& b = *bp;
    b.last_visit = m_frame;
    make_room(master_bytes(b), body, m_active);
    refresh_source(b, w); // the snapshot "current" is measured against
    const auto t0 = clock::now();
    const auto chunk_busy = [&] {
        for (int i = 0; i < b.n_chunks; ++i)
            if (b.job[i] != 0 || b.disk_pending[i])
                return true;
        return false;
    };
    for (;;)
    {
        drain();
        // This snapshot's sweep, on the main thread (a pool sweep in flight is
        // superseded: its sequence no longer matches and it lands in nothing).
        bool any = false;
        for (int i = 0; i < b.n_chunks && !any; ++i)
            any = b.ready[i] || b.job[i];
        if (any && b.swept_digest != b.src_digest)
        {
            job j = make_sweep_job(b);
            b.sweep_job = j.seq;
            bake_sync(std::move(j));
        }
        if (master_current(body))
            return true;
        // Every chunk not landed, or landed against an older snapshot, and
        // not already in the pool (a job in flight lands first; if it was
        // taken against an older source it lands dirty and the next round
        // re-queues it).
        for (int i = 0; i < b.n_chunks; ++i)
            if (b.job[i] == 0 && !b.disk_pending[i] && (!b.ready[i] || b.dirty[i]))
            {
                job j = make_chunk_job(b, i, k_prio_view + i, cache_live());
                b.job[i] = j.seq;
                enqueue(std::move(j));
            }
        for (;;)
        {
            drain();
            if (!chunk_busy() && b.sweep_job == 0)
                break; // (a pool sweep in flight against this snapshot lands first)
            if (ms_since(t0) > timeout_ms)
                return false;
            std::unique_lock lk(m_mx);
            m_done_cv.wait_for(lk, std::chrono::milliseconds(20), [&] { return !m_results.empty(); });
        }
        if (ms_since(t0) > timeout_ms)
            return master_current(body);
    }
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
    // BL-1259: the writer stops (writes still queued are abandoned — the
    // cache is this session's), then this session's cache directories go.
    std::deque<disk_task> abandoned;
    if (m_writer.joinable())
    {
        {
            std::lock_guard lk(m_wmx);
            m_wquit = true;
            abandoned.swap(m_wtasks);
        }
        m_wcv.notify_all();
        m_writer.join();
        m_writer = std::thread();
        m_wquit = false;
        m_wresults.clear();
    }
    for (const disk_task& t : abandoned) // a previous world's directories, not yet removed
        if (t.k == disk_task::kind::remove_dir)
        {
            std::error_code ec;
            std::filesystem::remove_all(t.dir, ec);
        }
    for (const std::filesystem::path& dir : m_session_dirs)
    {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
    m_session_dirs.clear();
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
    // A stored chunk re-baking: the worker may re-bake only the windows its
    // installations moved (it falls back to the whole chunk itself).
    if (b.ready[idx] && !s.px[idx].empty() && b.baked_src[idx] && patching_on())
        j.old_src = b.baked_src[idx];
    return j;
}

ground_layer::job ground_layer::make_chunk_job(body_state& b, int idx, double prio, bool allow_disk)
{
    if (allow_disk && !m_disk_root.empty() && !b.ready[idx] && b.disk_has[idx] && !b.disk_pending[idx])
    {
        // The current hash table says the file is stale: bake, do not load.
        const bool hashes_known = b.hashes_epoch != ~0u && b.swept_digest == b.src_digest
                               && b.want_hash.size() == static_cast<std::size_t>(b.n_chunks);
        if (hashes_known && b.disk_hash[idx] != b.want_hash[idx])
            ++m_bake_counters.load_skipped;
        else
        {
            // A load. When the hashes are not yet known (the sweep is still
            // queued), the landing — or the sweep after it — marks it dirty
            // if it moved, and it re-bakes then.
            job j = make_master_job(b, idx, prio);
            j.kind    = job_kind::load;
            j.old_src = nullptr;
            j.file    = chunk_file(b.disk_dir, idx);
            j.stamp   = m_disk_stamp ^ bytes_check(reinterpret_cast<const std::uint8_t*>(&params),
                                                   sizeof params);
            return j;
        }
    }
    return make_master_job(b, idx, prio);
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
        int n_ready = 0, n_dirty = 0, n_unbaked = 0;
        for (int i = 0; i < b.n_chunks; ++i)
        {
            b.dirty[i] = b.ready[i] && b.baked_hash[i] != b.want_hash[i];
            n_ready += b.ready[i] && !b.dirty[i];
            n_dirty += b.dirty[i];
            n_unbaked += !b.ready[i];
        }
        b.n_ready = n_ready;
        // A round boundary's own sweep (resnapshot): say what it costs. Only
        // the sweep against the CURRENT snapshot -- an older one in flight
        // when the boundary came lands first and is superseded.
        if (m_boundary_log && b.id == m_prebake && d.hash == b.src_digest)
        {
            m_boundary_log = false;
            std::printf("[ground] boundary sweep: %d of %d master chunks moved and re-bake "
                        "(%d not yet baked at all)\n", n_dirty, b.n_chunks, n_unbaked);
            std::fflush(stdout);
            if (n_dirty > 0)
            {
                m_boundary_track = true;
                m_boundary_base  = m_bake_counters;
                m_boundary_t0    = clock::now();
            }
        }
        return;
    }
    case job_kind::master:
    case job_kind::load:
        break;
    case job_kind::neigh:
        return;
    }

    // A master chunk: store it, place its mip pieces.
    const int idx = d.idx;
    if (idx < 0 || idx >= b.n_chunks || d.seq != b.job[idx])
        return; // not the job this slot is waiting on (dropped, superseded)
    b.job[idx] = 0;
    const bool from_disk = d.kind == job_kind::load;
    if (from_disk)
    {
        m_bake_counters.load_ms += d.ms;
        m_bake_counters.load_read_ms += d.read_ms;
    }
    if (from_disk && !d.loaded)
    {
        // BL-1259: a missing, foreign or corrupt file — forget it; the next
        // feed bakes the chunk, silently.
        if (!b.disk_pending[idx])
        {
            b.disk_has[idx] = 0;
            b.disk_size[idx] = 0;
            b.disk_raw[idx] = 0;
        }
        b.disk_same[idx] = 0;
        ++m_bake_counters.load_failures;
        return;
    }
    const bool was_counted = b.ready[idx] && !b.dirty[idx];
    const bool was_ready   = b.ready[idx] != 0;
    if (d.patched)
    {
        // A partial re-bake: blit each window over the stored chunk, and its
        // mip pieces over the chunk's share of every coarser level.
        level_store& s0 = b.lv[0];
        std::vector<std::uint32_t>& base = s0.px[idx];
        if (base.empty())
            return; // nothing stored to patch (a drop clears the slot first, so unreachable)
        const int ci = idx % s0.cw, cj = idx / s0.cw;
        const int cw0 = std::min(gb::k_chunk_px, s0.geom.W - ci * gb::k_chunk_px);
        for (const result::patch& pc : d.patches)
        {
            for (int y = 0; y < pc.r.h; ++y)
                std::memcpy(base.data() + static_cast<std::size_t>(pc.r.y0 + y) * cw0 + pc.r.x0,
                            pc.px.data() + static_cast<std::size_t>(y) * pc.r.w,
                            static_cast<std::size_t>(pc.r.w) * 4u);
            for (int l = 1; l < L; ++l)
            {
                level_store& s = b.lv[l];
                const int li = ci >> l, lj = cj >> l;
                const int lidx = lj * s.cw + li;
                const int lw = std::min(gb::k_chunk_px, s.geom.W - li * gb::k_chunk_px);
                std::vector<std::uint32_t>& dst = s.px[lidx];
                if (dst.empty())
                    continue; // never placed (unreachable once the chunk has landed)
                const int f  = 1 << l;
                const int ox = (ci & (f - 1)) * (gb::k_chunk_px >> l) + (pc.r.x0 >> l);
                const int oy = (cj & (f - 1)) * (gb::k_chunk_px >> l) + (pc.r.y0 >> l);
                const int pw = pc.r.w >> l, ph = pc.r.h >> l;
                for (int y = 0; y < ph; ++y)
                    std::memcpy(dst.data() + static_cast<std::size_t>(oy + y) * lw + ox,
                                pc.mip[l].data() + static_cast<std::size_t>(y) * pw,
                                static_cast<std::size_t>(pw) * 4u);
                ++s.ver[lidx];
            }
            ++m_bake_counters.patch_windows;
            m_bake_counters.patch_px += static_cast<std::uint64_t>(pc.r.w) * pc.r.h;
        }
        if (!d.patches.empty())
            ++s0.ver[idx];
        ++m_bake_counters.chunk_patches;
    }
    else
    {
        // The whole chunk: store it, place its mip pieces.
        if (was_ready)
            m_bake_counters.rebake_px += static_cast<std::uint64_t>(d.pw) * d.ph;
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
    }
    b.ready[idx] = 1;
    // A load knows its hash, not the source it was baked against: a later
    // re-bake of it is a whole chunk.
    b.baked_src[idx] = from_disk ? nullptr : d.src;
    if (was_ready)
        m_bake_counters.rebake_ms += d.ms;
    b.baked_hash[idx] = d.hash;
    // Hashed against an older snapshot than the current table? Then the
    // landing itself says whether it is already stale.
    b.dirty[idx] = b.swept_digest == b.src_digest && b.hashes_epoch != ~0u
                && d.hash != b.want_hash[idx];
    const bool now_counted = !b.dirty[idx];
    b.n_ready += (now_counted ? 1 : 0) - (was_counted ? 1 : 0);
    if (from_disk)
    {
        b.disk_same[idx] = 1; // its file holds exactly these pixels
        ++m_bake_counters.chunk_loads;
        return;
    }
    b.disk_same[idx] = 0;
    ++b.bakes[idx];
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
            any = b.ready[i] || b.job[i] || b.disk_has[i]; // a spilled chunk needs the table too
        if (any && b.swept_digest != b.src_digest && b.sweep_job == 0)
        {
            job j = make_sweep_job(b);
            b.sweep_job = j.seq;
            enqueue(std::move(j));
            ++queued;
        }
    };
    const bool disk = cache_live();
    body_state* act = find(m_active);
    body_state* pre = m_prebake != m_active ? find(m_prebake) : nullptr;
    if (act)
        housekeep(*act);
    if (pre)
        housekeep(*pre);

    const auto needs = [](const body_state& b, int i) {
        // A chunk whose spill is still being written waits for it (then loads).
        return b.job[i] == 0 && !b.disk_pending[i] && (!b.ready[i] || b.dirty[i]);
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
            job j = make_chunk_job(b, c[n].i, base + c[n].d2, disk);
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
                job j = make_chunk_job(*act, e.i, k_prio_view + e.d2, disk);
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
    const long long total = budget_total(); // the home body is pinned outside it
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
            if (!pinned(c.id) && total + est > ram_budget)
                continue;
            b = ensure_body(w, c.id);
            if (!b)
                continue;
            b->background = true;
        }
        else if (!pinned(c.id) && total - b->ram + master_bytes(*b) > ram_budget)
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
    if (verify)
    {
        // Name the path once: a run that meant the pool path but lost the env
        // var (a POSIX `VAR=1 cmd` prefix in PowerShell or cmd sets nothing)
        // is on the synchronous one, where the master never fills by itself.
        static bool named = false;
        if (!named)
        {
            named = true;
            std::printf("[ground] --verify ground path: %s\n",
                        bench_async_on() ? "POOL (IO_GROUND_BENCH: the master fills in the background)"
                                         : "SYNCHRONOUS (only what each frame draws is baked; "
                                           "verify.ground_complete_master() bakes the rest)");
            std::fflush(stdout);
        }
    }
    if (verify && bench_async_on())
        verify = false;
    m_cache_live = !verify; // the --verify synchronous path: a budget drop is a plain drop
    ++m_frame;
    drain();
    // A round boundary's re-bake, done: what it cost (BL-1246, the partial
    // re-bake reading — pixels are load-independent, times are not).
    if (m_boundary_track && master_current(m_prebake))
    {
        m_boundary_track = false;
        const bake_stats& n = m_bake_counters;
        const bake_stats& o = m_boundary_base;
        const auto whole = (n.chunk_rebakes - o.chunk_rebakes) - (n.chunk_patches - o.chunk_patches);
        const double chunk_px = static_cast<double>(gb::k_chunk_px) * gb::k_chunk_px;
        std::printf("[ground] boundary re-bake done in %.2f s wall: %llu whole chunks (%.1f Mpx) + "
                    "%llu patched chunks, %llu windows (%.1f Mpx) = %.1f chunk-equivalents; "
                    "worker time %.2f s\n",
                    ms_since(m_boundary_t0) / 1000.0, static_cast<unsigned long long>(whole),
                    (n.rebake_px - o.rebake_px) / 1e6,
                    static_cast<unsigned long long>(n.chunk_patches - o.chunk_patches),
                    static_cast<unsigned long long>(n.patch_windows - o.patch_windows),
                    (n.patch_px - o.patch_px) / 1e6,
                    ((n.rebake_px - o.rebake_px) + (n.patch_px - o.patch_px)) / chunk_px,
                    (n.rebake_ms - o.rebake_ms) / 1000.0);
        std::fflush(stdout);
    }
    if (!wp)
    {
        // The pre-bake runs from a world the app does not yet hold (the
        // wizard's rounds, each moving the world forward on its own worker):
        // keep the pool fed, read nothing. NO CADENCE SNAPSHOT HERE -- a
        // round worker may be mutating the world it would read; the app
        // re-takes the source only at a round boundary (resnapshot), on the
        // main thread, from a world no worker holds.
        if (!verify)
            feed(/*verify=*/false);
        return;
    }
    const world& w = *wp;
    m_world = wp;
    if (w.home_body != null_entity)
        m_home = w.home_body; // pinned in RAM (BL-1259)

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

    static const bool stall_log = env_flag("IO_GROUND_STALL_LOG");
    if (stall_log && m_frame % 60 == 0)
    {
        for (const auto& [id, bp] : m_bodies)
        {
            const body_state& b = *bp;
            int rd = 0, dt = 0, jb = 0, idle = 0, jb_ready = 0;
            for (int i = 0; i < b.n_chunks; ++i)
            {
                rd += b.ready[i] != 0;
                dt += b.dirty[i] != 0;
                jb += b.job[i] != 0;
                jb_ready += b.job[i] != 0 && b.ready[i] && !b.dirty[i];
                idle += b.job[i] == 0 && (!b.ready[i] || b.dirty[i]);
            }
            int q = 0, infl = 0, res = 0, qb = 0;
            {
                std::lock_guard lk(m_mx);
                q = static_cast<int>(m_jobs.size());
                infl = m_inflight;
                res = static_cast<int>(m_results.size());
                for (const job& j : m_jobs)
                    qb += j.body == b.id && j.kind == job_kind::master;
            }
            static const clock::time_point t_start = clock::now();
            std::printf("GROUND_STALL t%.1fs f%llu body %u%s%s n_ready %d/%d ready %d dirty %d jobs %d "
                        "(ready-clean with job %d) idle-needing %d | queue %d (this body %d) inflight %d "
                        "results %d | epoch %u hashes_epoch %u digest %016llx swept %016llx sweep_job %llu "
                        "far_pending %d far_ready %d ram %.2f GB\n",
                        ms_since(t_start) / 1000.0,
                        static_cast<unsigned long long>(m_frame), static_cast<unsigned>(b.id),
                        b.id == m_active ? " (active)" : "", b.id == m_prebake ? " (prebake)" : "",
                        b.n_ready, b.n_chunks, rd, dt, jb, jb_ready, idle, q, qb, infl, res,
                        b.src_epoch, b.hashes_epoch, static_cast<unsigned long long>(b.src_digest),
                        static_cast<unsigned long long>(b.swept_digest),
                        static_cast<unsigned long long>(b.sweep_job), b.far_pending, b.far_ready ? 1 : 0,
                        b.ram / 1073741824.0);
        }
        std::fflush(stdout);
    }

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
    m_cache_live = !bake_everything;
    m_world = &w;
    if (w.home_body != null_entity)
        m_home = w.home_body;

    const entity_id body = ui.active_body;
    if (body != m_active)
    {
        flush_gpu();
        m_active = body;
        m_fresh_view = true; // the first view on a body uploads whole (see the budget below)
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
    m_view_final = false;
    if (have_view)
    {
        // The first view on a body (a body switch, play opening) uploads its
        // visible chunks whole, in one frame: the frame is a transition
        // anyway, and a budgeted fill would show the far page for a few
        // frames over ground that is already baked in RAM.
        const bool whole_view = bake_everything || m_fresh_view;
        m_fresh_view = false;
        int budget = whole_view ? (1 << 30) : k_upload_budget;
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
            // The ring (a pan reveals it), with what the view leaves.
            visit(level, ring, true, false);
            // The adjacent levels (a rung change reads one) — PREFETCH, on
            // its own small budget and only once the drawn view is whole, so
            // it never competes with the view or hitches a pan. The coarser
            // level's whole view (a quarter of the chunks); the finer one's
            // central half (what a step in shows; the whole would be four
            // times the chunks and thrash while panning).
            if (m_pending_uploads == 0)
            {
                budget = std::min(budget, k_prefetch_budget);
                if (level + 1 < L)
                    visit(level + 1, window_of(b, level + 1, req, 0), false, false);
                if (level - 1 >= 0)
                {
                    ground_request half = req;
                    const float qx = 0.25f * (req.x1 - req.x0), qy = 0.25f * (req.y1 - req.y0);
                    half.x0 += qx; half.x1 -= qx; half.y0 += qy; half.y1 -= qy;
                    visit(level - 1, window_of(b, level - 1, half, 0), false, false);
                }
            }
        }
        evict_gpu();

        // The view is final: every master chunk under it landed and current,
        // and the drawn level's visible chunks on the GPU at their version
        // (ground_stats().view_final — BL-1259's return-to-a-body reading).
        m_view_final = m_pending_uploads == 0;
        if (m_view_final)
            for (const int i : under_view(vis))
                if (!b.ready[i] || b.dirty[i])
                {
                    m_view_final = false;
                    break;
                }
        if (m_view_final)
        {
            const level_store& s = b.lv[level];
            for (int lj = vis.cj_lo; lj <= vis.cj_hi && m_view_final; ++lj)
                for (int li = vis.ci_lo; li <= vis.ci_hi && m_view_final; ++li)
                {
                    const int idx = lj * s.cw + ((li % s.cw) + s.cw) % s.cw;
                    const auto it = m_gpu.find((static_cast<std::uint32_t>(level) << 24)
                                               | static_cast<std::uint32_t>(idx));
                    m_view_final = it != m_gpu.end() && it->second.ver == s.ver[idx];
                }
        }

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
        s.master_current = master_current(m_active);
        if (m_far_tex && b->far_ready)
            s.gpu_bytes[0] = static_cast<long long>(b->far_geom.W) * b->far_geom.H * 4;
    }
    for (const auto& [key, g] : m_gpu)
        if (g.tex)
            s.gpu_bytes[1 + (key >> 24)] += static_cast<long long>(g.w) * g.h * 4;
    s.active_slot = m_publish_level >= 0 ? 1 + m_publish_level : 0;
    s.pending_uploads = m_pending_uploads;
    s.view_final = m_view_final;
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

// ---------------------------------------------------------------------------
// The disk cache (BL-1259; RENDERING.md § Chunks, cache and invalidation)
//
// LOCATION: %LOCALAPPDATA%\ProjectIo\ground_cache on Windows ($XDG_CACHE_HOME
// or ~/.cache /ProjectIo/ground_cache elsewhere; IO_GROUND_CACHE_DIR
// overrides). Local app data, not the save directory: it is the per-machine,
// non-roaming place for a regenerable cache — a save folder is the player's
// (and may sync to a cloud drive), and gigabytes of ground there would follow
// it. One directory per body: s<stamp>-w<world>-b<body>-<W>x<H>-p<pid>, where
// the stamp folds the file format, the executable's size and write time (a
// rebuilt bake can never read an old one) and the master constants; the world
// key is the body's terrain hash at its first spill (the ground layer does not
// hold the seed; this keys the same thing more tightly — seed and wizard
// dials); pid keeps two running copies apart. The bake dials are folded into
// every file's stamp, so a changed dial reads as a miss. SESSION-SCOPED: this
// run's directories are deleted at exit and on a new world, and a directory
// nobody has written for an hour is purged at the first spill (a crashed
// run's).
// ---------------------------------------------------------------------------

void ground_layer::cache_init()
{
    if (m_disk_init)
        return;
    m_disk_init = true;
    namespace fs = std::filesystem;
    fs::path root;
    if (const char* v = SDL_getenv("IO_GROUND_CACHE_DIR"); v && *v)
        root = utf8_path(v);
    else if (const char* la = SDL_getenv("LOCALAPPDATA"); la && *la)
        root = utf8_path(la) / "ProjectIo" / "ground_cache";
    else if (const char* xc = SDL_getenv("XDG_CACHE_HOME"); xc && *xc)
        root = utf8_path(xc) / "ProjectIo" / "ground_cache";
    else if (const char* home = SDL_getenv("HOME"); home && *home)
        root = utf8_path(home) / ".cache" / "ProjectIo" / "ground_cache";
    std::error_code ec;
    if (!root.empty())
        fs::create_directories(root, ec);
    if (root.empty() || ec || !fs::is_directory(root, ec))
    {
        std::printf("[ground] disk cache: no usable location -- a budget drop re-bakes on return\n");
        std::fflush(stdout);
        return;
    }
    m_disk_root = root;
    if (const char* v = SDL_getenv("IO_GROUND_CACHE_CAP_GB"))
        if (const double gbs = std::atof(v); gbs > 0.0)
            m_disk_cap = static_cast<long long>(gbs * 1073741824.0);

    // The stamp: format x executable identity x master constants.
    std::uint64_t st = fold64(0x47524F554E44ull, k_cache_format);
    fs::path exe_path;
#ifdef _WIN32
    wchar_t exe[1024] = {};
    const unsigned long n = GetModuleFileNameW(nullptr, exe, 1024);
    if (n > 0 && n < 1024)
        exe_path = exe;
#else
    exe_path = fs::read_symlink("/proc/self/exe", ec);
    ec.clear();
#endif
    if (!exe_path.empty())
    {
        const auto sz = fs::file_size(exe_path, ec);
        if (!ec)
            st = fold64(st, static_cast<std::uint64_t>(sz));
        ec.clear();
        const auto wt = fs::last_write_time(exe_path, ec);
        if (!ec)
            st = fold64(st, static_cast<std::uint64_t>(wt.time_since_epoch().count()));
        ec.clear();
    }
    st = fold64(st, static_cast<std::uint64_t>(gb::k_chunk_px));
    st = fold64(st, static_cast<std::uint64_t>(L));
    st = fold64(st, static_cast<std::uint64_t>(k_master_ss));
    m_disk_stamp = st;

    // A crashed run's directories: nobody has written them for an hour.
    bool queued = false;
    const auto now = fs::file_time_type::clock::now();
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec))
    {
        std::error_code e2;
        if (!it->is_directory(e2))
            continue;
        const auto wt = fs::last_write_time(it->path(), e2);
        if (!e2 && now - wt > std::chrono::hours(1))
        {
            disk_task t;
            t.k = disk_task::kind::remove_dir;
            t.dir = it->path();
            std::lock_guard lk(m_wmx);
            m_wtasks.push_back(std::move(t));
            queued = true;
        }
    }
    if (queued)
    {
        if (!m_writer.joinable())
            m_writer = std::thread([this] { writer_main(); });
        m_wcv.notify_one();
    }
    std::printf("[ground] disk cache: %s (cap %.0f GB, session-scoped)\n",
                path_utf8(m_disk_root).c_str(), m_disk_cap / 1073741824.0);
    std::fflush(stdout);
}

void ground_layer::ensure_disk_keys(body_state& b)
{
    if (!b.disk_dir.empty() || m_disk_root.empty())
        return;
    std::uint64_t wfp = 0;
    if (b.src)
        wfp = gb::terrain_hash(*b.src, b.far_geom, 0, 0, b.far_geom.W, b.far_geom.H);
#ifdef _WIN32
    const unsigned pid = static_cast<unsigned>(_getpid());
#else
    const unsigned pid = static_cast<unsigned>(getpid());
#endif
    char name[128];
    std::snprintf(name, sizeof name, "s%016llx-w%016llx-b%u-%dx%d-p%u",
                  static_cast<unsigned long long>(m_disk_stamp), static_cast<unsigned long long>(wfp),
                  static_cast<unsigned>(b.id), b.lv[0].geom.W, b.lv[0].geom.H, pid);
    b.disk_dir = m_disk_root / name;
    m_session_dirs.push_back(b.disk_dir);
}

int ground_layer::spill_master(body_state& b, bool sync, int* skipped, const char* why)
{
    cache_init();
    if (m_disk_root.empty())
    {
        drop_master(b);
        return -1;
    }
    ensure_disk_keys(b);
    // Waiting master / load jobs of this body go; in-flight ones are orphaned
    // (their slots are cleared below, so they land in nothing).
    {
        std::lock_guard lk(m_mx);
        const auto split = std::partition(m_jobs.begin(), m_jobs.end(), [&](const job& j) {
            return !(j.body == b.id && (j.kind == job_kind::master || j.kind == job_kind::load));
        });
        m_inflight -= static_cast<int>(m_jobs.end() - split);
        m_jobs.erase(split, m_jobs.end());
    }
    disk_task t;
    t.k     = disk_task::kind::spill;
    t.body  = b.id;
    t.gen   = m_world_gen;
    t.dir   = b.disk_dir;
    t.purge = !b.disk_purged;
    t.stamp = m_disk_stamp ^ bytes_check(reinterpret_cast<const std::uint8_t*>(&params), sizeof params);
    b.disk_purged = true;
    int skip = 0;
    const level_store& s0 = b.lv[0];
    for (int i = 0; i < b.n_chunks; ++i)
    {
        // A chunk whose hash moved holds stale pixels: not worth a file (a
        // file it already has still holds ITS hash's content, and stays).
        if (!b.ready[i] || b.dirty[i] || s0.px[i].empty())
            continue;
        if (b.disk_has[i] && b.disk_same[i] && b.disk_hash[i] == b.baked_hash[i])
        {
            ++skip; // its file already holds these pixels
            continue;
        }
        const int ci = i % s0.cw, cj = i / s0.cw;
        spill_chunk c;
        c.idx  = i;
        c.pw   = std::min(gb::k_chunk_px, s0.geom.W - ci * gb::k_chunk_px);
        c.ph   = std::min(gb::k_chunk_px, s0.geom.H - cj * gb::k_chunk_px);
        c.hash = b.baked_hash[i];
        t.chunks.push_back(c);
        b.disk_has[i]     = 1;
        b.disk_hash[i]    = b.baked_hash[i];
        b.disk_same[i]    = 1;
        b.disk_pending[i] = 1;
    }
    // Every level leaves RAM now: into the task when there is something to
    // write (the writer frees it as it goes), else straight away.
    for (int l = 0; l < L; ++l)
    {
        level_store& s = b.lv[l];
        t.cw[l] = s.cw;
        t.lw[l] = s.geom.W;
        t.lh[l] = s.geom.H;
        if (!t.chunks.empty())
            t.lv[l] = std::move(s.px);
        else
            std::vector<std::vector<std::uint32_t>>().swap(s.px);
        s.px.clear();
        s.px.resize(static_cast<std::size_t>(s.cw) * s.ch);
        for (std::uint32_t& v : s.ver)
            ++v;
    }
    std::fill(b.job.begin(), b.job.end(), 0);
    std::fill(b.baked_src.begin(), b.baked_src.end(), nullptr);
    std::fill(b.ready.begin(), b.ready.end(), 0);
    std::fill(b.dirty.begin(), b.dirty.end(), 0);
    const long long freed = b.ram - static_cast<long long>(b.far_px.size()) * 4;
    b.n_ready = 0;
    b.ram = static_cast<long long>(b.far_px.size()) * 4;
    b.background = false;
    ++m_bake_counters.spills;
    m_bake_counters.write_skipped += static_cast<std::uint64_t>(skip);
    if (skipped)
        *skipped = skip;
    // The cap's eviction order: other bodies with files, least recently visited first.
    std::vector<const body_state*> others;
    for (const auto& [id, bp] : m_bodies)
        if (id != b.id && !bp->disk_dir.empty())
            others.push_back(bp.get());
    std::sort(others.begin(), others.end(), [](const body_state* x, const body_state* y) {
        return x->last_visit < y->last_visit || (x->last_visit == y->last_visit && x->id < y->id);
    });
    for (const body_state* o : others)
        t.evict_order.push_back({ o->id, o->disk_dir });
    const int n = static_cast<int>(t.chunks.size());
    std::printf("[ground] %s: spilled the master of body %u to the disk cache (%d chunks to write, "
                "%d already on disk; %.2f GB freed)\n",
                why, static_cast<unsigned>(b.id), n, skip, freed / 1073741824.0);
    std::fflush(stdout);
    if (sync)
    {
        std::vector<disk_result> out;
        run_disk_task(t, out);
        for (const disk_result& r : out)
            land_disk(r);
    }
    else
    {
        {
            std::lock_guard lk(m_wmx);
            m_wtasks.push_back(std::move(t));
        }
        if (!m_writer.joinable())
            m_writer = std::thread([this] { writer_main(); });
        m_wcv.notify_one();
    }
    return n;
}

void ground_layer::writer_main()
{
    // Below normal, as the pool: the writer must never take the render or
    // simulation thread's core.
    SDL_SetCurrentThreadPriority(SDL_THREAD_PRIORITY_LOW);
    for (;;)
    {
        disk_task t;
        {
            std::unique_lock lk(m_wmx);
            m_wcv.wait(lk, [&] { return m_wquit || !m_wtasks.empty(); });
            if (m_wquit)
                return;
            t = std::move(m_wtasks.front());
            m_wtasks.pop_front();
            m_wbusy = true;
        }
        std::vector<disk_result> out;
        run_disk_task(t, out);
        t = disk_task{}; // the level buffers go before the writer says it is idle
        {
            std::lock_guard lk(m_wmx);
            for (disk_result& r : out)
                m_wresults.push_back(r);
            m_wbusy = false;
        }
        m_widle_cv.notify_all();
    }
}

void ground_layer::run_disk_task(disk_task& t, std::vector<disk_result>& out)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string key = path_utf8(t.dir);
    if (t.k == disk_task::kind::remove_dir)
    {
        fs::remove_all(t.dir, ec);
        std::lock_guard lk(m_wmx);
        m_wdir_bytes.erase(key);
        return;
    }
    const auto fail_rest = [&](std::size_t from) {
        for (std::size_t k = from; k < t.chunks.size(); ++k)
            out.push_back({ t.body, t.gen, t.chunks[k].idx, false, 0u, 0u });
    };
    if (t.purge)
    {
        fs::remove_all(t.dir, ec);
        std::lock_guard lk(m_wmx);
        m_wdir_bytes.erase(key);
    }
    ec.clear();
    fs::create_directories(t.dir, ec);
    if (ec)
    {
        fail_rest(0);
        return;
    }
    // The cap: the bytes this spill may add (an upper bound: raw RGBA and its
    // mip pieces) over what the session holds; past it, evict whole bodies,
    // least recently visited first. And never fill the disk: keep 2 GB free.
    long long add = 0;
    for (const spill_chunk& c : t.chunks)
        add += static_cast<long long>(c.pw) * c.ph * 4 * 4 / 3 + 4096;
    {
        long long held = 0;
        {
            std::lock_guard lk(m_wmx);
            for (const auto& [k, v] : m_wdir_bytes)
                held += v;
        }
        for (const auto& [id, dir] : t.evict_order)
        {
            if (held + add <= m_disk_cap)
                break;
            const std::string dk = path_utf8(dir);
            long long had = 0;
            {
                std::lock_guard lk(m_wmx);
                const auto it = m_wdir_bytes.find(dk);
                if (it == m_wdir_bytes.end())
                    continue;
                had = it->second;
                m_wdir_bytes.erase(it);
            }
            fs::remove_all(dir, ec);
            ec.clear();
            held -= had;
            out.push_back({ id, t.gen, -1, false, 0u, 0u }); // its files are gone
            std::printf("[ground] disk cache: cap %.0f GB -- evicted body %u (%.2f GB)\n",
                        m_disk_cap / 1073741824.0, static_cast<unsigned>(id), had / 1073741824.0);
            std::fflush(stdout);
        }
        const fs::space_info sp = fs::space(t.dir, ec);
        if (!ec && static_cast<long long>(sp.available) < add + (2LL << 30))
        {
            std::printf("[ground] disk cache: under 2 GB free -- body %u not written (re-bakes on return)\n",
                        static_cast<unsigned>(t.body));
            std::fflush(stdout);
            fail_rest(0);
            return;
        }
        ec.clear();
    }
    // Free as we go: chunks in block order (the 2^(L-1) square a coarsest
    // level chunk covers), each level chunk released once the last chunk that
    // reads it is written — the spill's RAM drains with its writes rather than
    // all at the end (a whole home master is ~6.7 GB).
    {
        const int cw0 = t.cw[0];
        const int sh = L - 1;
        std::stable_sort(t.chunks.begin(), t.chunks.end(), [cw0, sh](const spill_chunk& a, const spill_chunk& c) {
            const int ai = a.idx % cw0, aj = a.idx / cw0, ci2 = c.idx % cw0, cj2 = c.idx / cw0;
            if ((aj >> sh) != (cj2 >> sh)) return (aj >> sh) < (cj2 >> sh);
            if ((ai >> sh) != (ci2 >> sh)) return (ai >> sh) < (ci2 >> sh);
            return a.idx < c.idx;
        });
    }
    std::vector<std::vector<int>> refs(L);
    for (int l = 1; l < L; ++l)
    {
        refs[l].assign(t.lv[l].size(), 0);
        for (const spill_chunk& c : t.chunks)
        {
            const int ci = c.idx % t.cw[0], cj = c.idx / t.cw[0];
            const std::size_t lidx = static_cast<std::size_t>(cj >> l) * t.cw[l] + (ci >> l);
            if (lidx < refs[l].size())
                ++refs[l][lidx];
        }
        for (std::size_t i = 0; i < refs[l].size(); ++i)
            if (refs[l][i] == 0)
                std::vector<std::uint32_t>().swap(t.lv[l][i]); // read by no chunk being written
    }
    {
        // The master chunks not being written go now too.
        std::vector<std::uint8_t> keep(t.lv[0].size(), 0);
        for (const spill_chunk& c : t.chunks)
            if (static_cast<std::size_t>(c.idx) < keep.size())
                keep[static_cast<std::size_t>(c.idx)] = 1;
        for (std::size_t i = 0; i < t.lv[0].size(); ++i)
            if (!keep[i])
                std::vector<std::uint32_t>().swap(t.lv[0][i]);
    }
    std::vector<std::uint8_t> buf;
    std::vector<std::uint32_t> piece;
    for (std::size_t k = 0; k < t.chunks.size(); ++k)
    {
        {
            std::lock_guard lk(m_wmx);
            if (m_wquit)
                return; // shutting down: the session's files go anyway
        }
        const spill_chunk& c = t.chunks[k];
        buf.clear();
        buf.resize(k_header_bytes);
        std::uint32_t raw = 0;
        bool ok = true;
        const int cw0 = t.cw[0];
        const int ci = c.idx % cw0, cj = c.idx / cw0;
        for (int l = 0; l < L && ok; ++l)
        {
            const int pw = c.pw >> l, ph = c.ph >> l;
            const std::size_t n = static_cast<std::size_t>(pw) * ph;
            if (l == 0)
            {
                const std::vector<std::uint32_t>& src = t.lv[0][static_cast<std::size_t>(c.idx)];
                if (src.size() != n)
                {
                    ok = false;
                    break;
                }
                encode_piece(src.data(), n, buf);
            }
            else
            {
                // This chunk's share of level l (the placement land() made).
                const int li = ci >> l, lj = cj >> l;
                const std::size_t lidx = static_cast<std::size_t>(lj) * t.cw[l] + li;
                const int lw = std::min(gb::k_chunk_px, t.lw[l] - li * gb::k_chunk_px);
                const int f  = 1 << l;
                const int ox = (ci & (f - 1)) * (gb::k_chunk_px >> l);
                const int oy = (cj & (f - 1)) * (gb::k_chunk_px >> l);
                if (lidx >= t.lv[l].size() || t.lv[l][lidx].empty())
                {
                    ok = false;
                    break;
                }
                const std::vector<std::uint32_t>& src = t.lv[l][lidx];
                piece.resize(n);
                for (int y = 0; y < ph; ++y)
                    std::memcpy(piece.data() + static_cast<std::size_t>(y) * pw,
                                src.data() + static_cast<std::size_t>(oy + y) * lw + ox,
                                static_cast<std::size_t>(pw) * 4u);
                encode_piece(piece.data(), n, buf);
            }
            raw += static_cast<std::uint32_t>(n * 4);
        }
        // The master piece is no longer needed, nor any level chunk this was
        // the last reader of: free them as we go.
        std::vector<std::uint32_t>().swap(t.lv[0][static_cast<std::size_t>(c.idx)]);
        for (int l = 1; l < L; ++l)
        {
            const std::size_t lidx = static_cast<std::size_t>(cj >> l) * t.cw[l] + (ci >> l);
            if (lidx < refs[l].size() && --refs[l][lidx] == 0)
                std::vector<std::uint32_t>().swap(t.lv[l][lidx]);
        }
        if (!ok)
        {
            out.push_back({ t.body, t.gen, c.idx, false, 0u, 0u });
            continue;
        }
        const std::uint32_t payload = static_cast<std::uint32_t>(buf.size() - k_header_bytes);
        std::uint8_t* h = buf.data();
        const std::uint32_t fmt = k_cache_format;
        const std::uint32_t lv = static_cast<std::uint32_t>(L), body = static_cast<std::uint32_t>(t.body);
        const std::uint32_t idx = static_cast<std::uint32_t>(c.idx);
        const std::uint64_t check = bytes_check(buf.data() + k_header_bytes, payload);
        std::memcpy(h, "IOGC", 4);
        std::memcpy(h + 4, &fmt, 4);
        std::memcpy(h + 8, &t.stamp, 8);
        std::memcpy(h + 16, &body, 4);
        std::memcpy(h + 20, &idx, 4);
        std::memcpy(h + 24, &c.pw, 4);
        std::memcpy(h + 28, &c.ph, 4);
        std::memcpy(h + 32, &c.hash, 8);
        std::memcpy(h + 40, &lv, 4);
        std::memcpy(h + 44, &payload, 4);
        std::memcpy(h + 48, &check, 8);
        // Atomic: a temp file, closed, then renamed over the chunk's name — a
        // crash leaves a .tmp (never loaded) or the whole file, never half.
        const fs::path fin = chunk_file(t.dir, c.idx);
        fs::path tmp = fin;
        tmp.replace_extension(".tmp");
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
            f.close();
            ok = !f.fail();
        }
        if (ok)
        {
            fs::rename(tmp, fin, ec);
            if (ec)
            {
                ec.clear();
                fs::remove(fin, ec);
                ec.clear();
                fs::rename(tmp, fin, ec);
            }
            ok = !ec;
            ec.clear();
        }
        if (!ok)
        {
            fs::remove(tmp, ec);
            ec.clear();
            out.push_back({ t.body, t.gen, c.idx, false, 0u, 0u });
            continue;
        }
        {
            std::lock_guard lk(m_wmx);
            m_wdir_bytes[key] += static_cast<long long>(buf.size());
        }
        out.push_back({ t.body, t.gen, c.idx, true, static_cast<std::uint32_t>(buf.size()), raw });
    }
}

bool ground_layer::load_chunk_file(const job& j, result& d)
{
    const auto t0 = std::chrono::steady_clock::now();
    std::ifstream f(j.file, std::ios::binary | std::ios::ate);
    if (!f)
        return false;
    const std::streamoff size = f.tellg();
    if (size < static_cast<std::streamoff>(k_header_bytes) || size > (64LL << 20))
        return false;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(buf.data()), size);
    if (!f)
        return false;
    f.close();
    d.read_ms = ms_since(t0);
    const std::uint8_t* h = buf.data();
    if (std::memcmp(h, "IOGC", 4) != 0 || get_at<std::uint32_t>(h, 4) != k_cache_format
        || get_at<std::uint64_t>(h, 8) != j.stamp
        || get_at<std::uint32_t>(h, 16) != static_cast<std::uint32_t>(j.body)
        || get_at<std::uint32_t>(h, 20) != static_cast<std::uint32_t>(j.idx)
        || get_at<std::int32_t>(h, 24) != j.pw || get_at<std::int32_t>(h, 28) != j.ph
        || get_at<std::uint32_t>(h, 40) != static_cast<std::uint32_t>(L)
        || get_at<std::uint32_t>(h, 44) != buf.size() - k_header_bytes)
        return false;
    const std::uint8_t* p = buf.data() + k_header_bytes;
    const std::uint8_t* end = buf.data() + buf.size();
    if (bytes_check(p, static_cast<std::size_t>(end - p)) != get_at<std::uint64_t>(h, 48))
        return false;
    for (int l = 0; l < L; ++l)
    {
        std::vector<std::uint32_t>& dst = l == 0 ? d.px : d.mip[l];
        const std::size_t n = static_cast<std::size_t>(j.pw >> l) * static_cast<std::size_t>(j.ph >> l);
        dst.assign(n, 0u);
        if (!decode_piece(p, end, dst.data(), n))
            return false;
    }
    if (p != end)
        return false;
    d.hash = get_at<std::uint64_t>(h, 32);
    return true;
}

void ground_layer::drain_disk()
{
    std::vector<disk_result> done;
    {
        std::lock_guard lk(m_wmx);
        done.swap(m_wresults);
    }
    for (const disk_result& r : done)
        land_disk(r);
}

void ground_layer::land_disk(const disk_result& r)
{
    if (r.gen != m_world_gen)
        return; // a previous world's
    body_state* bp = find(r.body);
    if (!bp)
        return;
    body_state& b = *bp;
    if (r.idx < 0)
    {
        forget_disk(b); // evicted by the cap
        return;
    }
    if (r.idx >= b.n_chunks)
        return;
    const std::size_t i = static_cast<std::size_t>(r.idx);
    b.disk_pending[i] = 0;
    if (r.ok)
    {
        b.disk_size[i] = r.bytes;
        b.disk_raw[i]  = r.raw;
        ++m_bake_counters.chunk_writes;
    }
    else
    {
        b.disk_has[i]  = 0;
        b.disk_same[i] = 0;
        b.disk_size[i] = 0;
        b.disk_raw[i]  = 0;
        ++m_bake_counters.write_failures;
    }
}

void ground_layer::forget_disk(body_state& b)
{
    for (int i = 0; i < b.n_chunks; ++i)
        if (!b.disk_pending[i])
        {
            b.disk_has[i]  = 0;
            b.disk_same[i] = 0;
            b.disk_size[i] = 0;
            b.disk_raw[i]  = 0;
        }
}

ground_layer::cache_info ground_layer::cache_snapshot() const
{
    cache_info c;
    c.root = path_utf8(m_disk_root);
    c.cap  = m_disk_cap;
    c.live = cache_live();
    std::vector<const body_state*> order;
    for (const auto& [id, bp] : m_bodies)
        order.push_back(bp.get());
    std::sort(order.begin(), order.end(), [](const body_state* x, const body_state* y) { return x->id < y->id; });
    for (const body_state* b : order)
    {
        cache_body cb;
        cb.body = b->id;
        for (int i = 0; i < b->n_chunks; ++i)
        {
            if (b->disk_has[i] && b->disk_size[i] > 0)
            {
                cb.bytes += b->disk_size[i];
                cb.raw_bytes += b->disk_raw[i];
                ++cb.files;
            }
            cb.pending += b->disk_pending[i] != 0;
        }
        cb.resident = b->ram > static_cast<long long>(b->far_px.size()) * 4;
        cb.pinned   = pinned(b->id);
        c.bytes   += cb.bytes;
        c.pending += cb.pending;
        c.bodies.push_back(cb);
    }
    return c;
}

int ground_layer::spill_body(entity_id body, bool sync, int* skipped)
{
    body_state* b = find(body);
    if (!b)
        return -1;
    cache_init();
    if (m_disk_root.empty())
        return -1;
    return spill_master(*b, sync, skipped, "verify");
}

void ground_layer::wait_cache_idle()
{
    {
        std::unique_lock lk(m_wmx);
        m_widle_cv.wait(lk, [&] { return m_wtasks.empty() && !m_wbusy; });
    }
    drain_disk();
}

ground_layer::restore_result ground_layer::restore_body(const world& w, entity_id body, double timeout_ms)
{
    restore_result rr;
    const auto t0 = clock::now();
    wait_cache_idle();
    drain();
    body_state* bp = find(body);
    if (!bp || m_disk_root.empty())
        return rr;
    body_state& b = *bp;
    refresh_source(b, w);
    // This snapshot's table first, so a moved chunk is known before its load.
    if (b.swept_digest != b.src_digest)
    {
        job j = make_sweep_job(b);
        b.sweep_job = j.seq;
        bake_sync(std::move(j));
    }
    const std::uint64_t loads0 = m_bake_counters.chunk_loads;
    const std::uint64_t fails0 = m_bake_counters.load_failures;
    std::vector<int> set;
    for (int i = 0; i < b.n_chunks; ++i)
        if (!b.ready[i] && b.disk_has[i] && b.job[i] == 0)
            set.push_back(i);
    const auto run = [&](bool allow_disk) {
        for (const int i : set)
            if (!b.ready[i] && b.job[i] == 0)
            {
                job j = make_chunk_job(b, i, k_prio_view + i, allow_disk);
                if (j.kind == job_kind::master)
                    ++rr.rebaked;
                b.job[i] = j.seq;
                enqueue(std::move(j));
            }
        for (;;)
        {
            drain();
            bool busy = false;
            for (const int i : set)
                busy = busy || b.job[i] != 0;
            if (!busy)
                return true;
            if (ms_since(t0) > timeout_ms)
                return false;
            std::unique_lock lk(m_mx);
            m_done_cv.wait_for(lk, std::chrono::milliseconds(20), [&] { return !m_results.empty(); });
        }
    };
    bool ok = run(true);
    // A load that failed left its chunk unbaked: bake it now.
    ok = ok && run(false);
    rr.loaded = static_cast<int>(m_bake_counters.chunk_loads - loads0);
    rr.failed = static_cast<int>(m_bake_counters.load_failures - fails0);
    rr.ok = ok;
    for (const int i : set)
        rr.ok = rr.ok && b.ready[i] && !b.dirty[i];
    rr.ms = ms_since(t0);
    return rr;
}

std::uint64_t ground_layer::master_digest(entity_id body, int* chunks) const
{
    std::uint64_t h = 0xCBF29CE484222325ull;
    int n = 0;
    if (const body_state* bp = find(body))
    {
        const body_state& b = *bp;
        const level_store& s0 = b.lv[0];
        std::vector<std::uint32_t> piece;
        for (int i = 0; i < b.n_chunks; ++i)
        {
            if (!b.ready[i] || s0.px[i].empty())
                continue;
            ++n;
            h = fold64(h, static_cast<std::uint64_t>(i));
            h = bytes_check(reinterpret_cast<const std::uint8_t*>(s0.px[i].data()), s0.px[i].size() * 4, h);
            const int ci = i % s0.cw, cj = i / s0.cw;
            const int pw0 = std::min(gb::k_chunk_px, s0.geom.W - ci * gb::k_chunk_px);
            const int ph0 = std::min(gb::k_chunk_px, s0.geom.H - cj * gb::k_chunk_px);
            for (int l = 1; l < L; ++l)
            {
                const level_store& s = b.lv[l];
                const int li = ci >> l, lj = cj >> l;
                const std::size_t lidx = static_cast<std::size_t>(lj) * s.cw + li;
                const int lw = std::min(gb::k_chunk_px, s.geom.W - li * gb::k_chunk_px);
                const int f  = 1 << l;
                const int ox = (ci & (f - 1)) * (gb::k_chunk_px >> l);
                const int oy = (cj & (f - 1)) * (gb::k_chunk_px >> l);
                const int pw = pw0 >> l, ph = ph0 >> l;
                if (s.px[lidx].empty())
                {
                    h = fold64(h, 0xDEADull);
                    continue;
                }
                piece.resize(static_cast<std::size_t>(pw) * ph);
                for (int y = 0; y < ph; ++y)
                    std::memcpy(piece.data() + static_cast<std::size_t>(y) * pw,
                                s.px[lidx].data() + static_cast<std::size_t>(oy + y) * lw + ox,
                                static_cast<std::size_t>(pw) * 4u);
                h = bytes_check(reinterpret_cast<const std::uint8_t*>(piece.data()), piece.size() * 4, h);
            }
        }
    }
    if (chunks)
        *chunks = n;
    return h;
}

void ground_layer::drop_body(entity_id body)
{
    body_state* b = find(body);
    if (!b)
        return;
    wait_cache_idle();
    drop_master(*b);
    forget_disk(*b);
}

int ground_layer::corrupt_cached_chunk(entity_id body)
{
    wait_cache_idle();
    body_state* b = find(body);
    if (!b)
        return -1;
    for (int i = 0; i < b->n_chunks; ++i)
    {
        if (!b->disk_has[i] || b->disk_size[i] <= k_header_bytes + 64)
            continue;
        const std::filesystem::path p = chunk_file(b->disk_dir, i);
        std::fstream f(p, std::ios::binary | std::ios::in | std::ios::out);
        if (!f)
            return -1;
        const std::streamoff at = static_cast<std::streamoff>(b->disk_size[i] / 2);
        char bytes[16];
        f.seekg(at);
        f.read(bytes, sizeof bytes);
        for (char& c : bytes)
            c = static_cast<char>(c ^ 0x5A);
        f.seekp(at);
        f.write(bytes, sizeof bytes);
        return f ? i : -1;
    }
    return -1;
}
