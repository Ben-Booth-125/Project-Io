#pragma once

#include "ui/ground_bake.hpp"
#include "ui/ui_state.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Ground layer (BL-732; BL-1246, one master) — the SDL half of the baked ground.
//
// ONE MASTER PER BODY (docs/ui/RENDERING.md § Level of detail, Ben 2026-10-09:
// "zooming doesn't add more detail"). Each body's ground is baked ONCE, whole
// body, at 96 px per hex at the one camera angle (22.5 deg), in 512 px chunks
// held in SYSTEM RAM. As each master chunk lands, the worker that baked it
// box-downsamples it into its pieces of the 48 / 24 / 12 / 6 px levels — the
// mip chain — so every level is a chunked RAM image too. Each frame the
// Planetary canvas draws ONE level, the coarsest at or above its drawn hex
// radius (minified <= 2:1), and this layer uploads that level's visible
// chunks to GPU textures within a per-frame upload budget, keeping an LRU of
// textures; spare budget prefetches the adjacent levels, so a rung change
// finds its textures already resident.
//
// A body with no master yet draws its FAR PAGE — one direct whole-body bake
// at 6 px per hex, the first job on a body — and, before that, the canvas's
// vector fallback. Nothing else stands in.
//
// THE POOL (Ben, 2026-10-09): N = max(1, hardware threads - 2) workers at
// below-normal OS priority pull from one shared queue, popped lowest
// priority value first: far pages, then hash sweeps, then the Selection
// band's neighbourhood page, then the active body's master chunks under the
// view (nearest the centre first), then the rest of that body, then the
// pre-bake target, then background bodies. The pure bake executes against an
// immutable source snapshot shared read-only by every worker; the render
// thread only drains results into RAM, uploads textures and publishes the
// view. A result lands only in the slot whose outstanding job it is (a job
// sequence number), so arrival order cannot matter.
//
// INVALIDATION IS CONTENT-HASHED, per master chunk. A source snapshot is
// re-taken on a cadence; when its whole-body digest moves, a sweep job hashes
// every master chunk against it, and a chunk whose hash moved re-bakes in the
// master and re-derives only its own mip pieces.
//
// RAM: a budget across bodies (k_ram_budget) drops the least-recently-visited
// body's master and chain, keeping its far page; background bakes start only
// where they fit without dropping anything.
//
// Under --verify everything a frame draws is complete before tick returns: the
// master chunks under the view are baked on the pool and WAITED for, the far
// page and neighbourhood page bake on the main thread, and every visible
// texture is uploaded with no budget — a capture can never race the pool.
// Pre-bake and background bakes are off under --verify.
// ---------------------------------------------------------------------------

struct world;

class ground_layer
{
public:
    ~ground_layer();

    /// Every frame, on every screen (the pre-bake runs behind the wizard's
    /// last round and the seat canvas): drains finished bakes into RAM,
    /// re-takes source snapshots on their cadence, and keeps the pool fed.
    /// @p w null = the world being pre-baked is not the app's yet (the
    /// wizard's cached world): drain and feed only. @p verify = the --verify
    /// run (no pre-bake, no background bodies).
    void pump(const world* w, bool verify);

    /// The Planetary canvas's per-frame driver: body switch, the request's
    /// level and window, uploads within the budget, publish. @p
    /// bake_everything = the --verify path (see file header).
    void tick(SDL_Renderer* r, const world& w, ui_state& ui, bool bake_everything);

    /// Stop the workers and destroy every texture. Call before the renderer dies.
    void shutdown();

    /// A different world is in play (a new campaign, a load): drop every body.
    void forget_world();

    /// STARTUP.md § Handoff: start @p body's master on the pool now (the
    /// homeworld, the moment the world is finished). Not under --verify.
    void prebake(const world& w, entity_id body);
    /// Master chunks landed and current / total for @p body (0 / 0 unknown).
    void master_progress(entity_id body, int& ready, int& total) const;
    /// Every master chunk of @p body has landed and is current.
    bool master_complete(entity_id body) const;
    /// The world just changed under the active body (a build placed): re-take
    /// its source snapshot next pump rather than at the cadence.
    void touch();

    /// The C-F dials; shipped values are ground_bake.hpp's defaults.
    ui::ground::bake_params params;

    /// Bake-cost and residency instrumentation, read by verify.ground_stats().
    /// Slot 0 = the far page; slot 1 + l = level l (0 = the master). bake_ms /
    /// bakes: cumulative since reset_stats() — master chunk bakes in slot 1,
    /// mip pieces derived in slots 2..5. ram_bytes: the active body's RAM per
    /// level; gpu_bytes: its uploaded textures per level.
    static constexpr int k_stat_slots = 8;
    struct stats
    {
        double    bake_ms[k_stat_slots]   = {};
        int       bakes[k_stat_slots]     = {};
        long long ram_bytes[k_stat_slots] = {};
        long long gpu_bytes[k_stat_slots] = {};
        double    ppr[k_stat_slots]       = {};
        int       active_slot = -1;
        long long ram_total   = 0;   ///< Every body's master + chain + far page, bytes.
        int       bodies_resident = 0;
        int       master_ready = 0, master_total = 0; ///< Active body.
        std::uint64_t uploads = 0;   ///< Texture uploads since reset_stats().
        int       pending_uploads = 0; ///< Visible chunks of the drawn level not yet on the GPU (last tick).
    };
    stats stats_snapshot() const;
    void  reset_stats();

    /// Verify-only: under bake_everything, bake at most this many master
    /// chunks per tick (-1 = no limit) — a first-visit frame part-way through.
    int verify_fill_limit = -1;

    /// BL-1241: bake counters (a re-bake must follow a construction event,
    /// never a tick). Monotonic across body switches.
    struct bake_stats
    {
        std::uint64_t far_bakes = 0;
        std::uint64_t chunk_bakes = 0;   ///< Master chunks landed.
        std::uint64_t chunk_rebakes = 0; ///< A master chunk that was already ready, baked again.
        std::uint64_t neigh_bakes = 0;
    };
    const bake_stats& bake_counters() const { return m_bake_counters; }

    /// BL-1241: a digest of every installation in the active body's source.
    std::uint64_t installation_digest() const;

    /// BL-1241: the recipe registry a processing facility's stamp reads its
    /// family from. Set by the app; null = general form.
    const recipe_registry* registry = nullptr;

    /// Verify-only (ground_rebake.lua): one row per READY master chunk of the
    /// active body under the last request's view, plus the far page (tier -1,
    /// key 0): its key, its bake count, and the installation and region
    /// hashes over its own window against the current source.
    struct chunk_probe
    {
        int           tier = -1;
        std::uint32_t key  = 0;
        std::uint64_t bakes = 0;
        std::uint64_t installation_hash = 0;
        std::uint64_t region_hash = 0;
    };
    std::vector<chunk_probe> probe_chunks() const;

    /// Per-frame GPU upload budget, chunks (each <= 1 MB): 12 MB a frame keeps
    /// a 60 fps frame (measured; RENDERING.md § Level of detail).
    static constexpr int         k_upload_budget = 12;
    /// GPU texture LRU cap (textures of the active body; each <= 1 MB).
    static constexpr std::size_t k_gpu_cap = 320;
    /// RAM budget across bodies (TECH_FOUNDATIONS.md § Target hardware: 16 GB
    /// minimum): the home body's master + chain is ~3.8 GB.
    static constexpr long long   k_ram_budget = 6LL * 1024 * 1024 * 1024;
    /// Master supersampling: 1x (measured 2026-10-09: the 2x whole-home bake
    /// does not fit the 15 s pre-bake budget — RENDERING.md § Level of detail).
    static constexpr int         k_master_ss = 1;

private:
    static constexpr int L = ui::ground::k_level_count;

    struct level_store
    {
        ui::ground::geometry geom;
        int cw = 0, ch = 0;
        std::vector<std::vector<std::uint32_t>> px; ///< Per chunk; empty = nothing written yet.
        std::vector<std::uint32_t>              ver; ///< Bumps on every write.
    };

    struct body_state
    {
        entity_id id = null_entity;
        int gw = 0, gh = 0;
        level_store lv[L];
        // Per master chunk (index = cj * cw + ci of level 0).
        std::vector<std::uint64_t> baked_hash; ///< Hash the landed bake was taken against.
        std::vector<std::uint64_t> want_hash;  ///< The current source's hash (valid when hashes_epoch == src_epoch).
        std::vector<std::uint64_t> bakes;      ///< Landings per chunk (verify probe).
        std::vector<std::uint64_t> job;        ///< Sequence of the outstanding job (0 = none).
        std::vector<std::uint8_t>  ready;      ///< Landed at least once since the master was (re)allocated.
        std::vector<std::uint8_t>  dirty;      ///< Ready, but its hash moved: re-bake.
        int n_ready = 0;  ///< ready && !dirty
        int n_chunks = 0;
        bool master_alloc = false;             ///< Level buffers are live (false after a RAM drop).
        std::shared_ptr<const ui::ground::bake_source> src;
        std::uint32_t src_epoch = 0;
        std::uint32_t hashes_epoch = ~0u;
        std::uint64_t src_digest = 0;          ///< Whole-body region hash of src (the far page's).
        std::uint64_t swept_digest = 0;        ///< Digest the last sweep ran against.
        std::uint64_t sweep_job = 0;
        int           src_age = 0;
        // The far page: one whole-body image, baked as 512 px pieces on the
        // pool (single-sample: it is the fallback a first visit shows for the
        // second or so before the master covers the view).
        ui::ground::geometry far_geom;
        int           far_cw = 0, far_ch = 0;
        std::vector<std::uint32_t> far_px;
        std::vector<std::uint64_t> far_jobs;   ///< Outstanding job per piece (0 = none).
        int           far_pending = 0;         ///< Pieces of the bake under way not yet landed.
        std::uint64_t far_baking = 0;          ///< Digest the bake under way was taken against.
        std::uint64_t far_hash = 0;            ///< Digest the shown page was baked against.
        bool          far_ready = false;
        std::uint32_t far_ver = 0;
        // Visits (RAM budget order) and background order.
        std::uint64_t last_visit = 0;
        int           visits = 0;
        bool          background = false;      ///< Baked speculatively (not visited, not pre-baked).
        long long     ram = 0;
    };

    enum class job_kind : std::uint8_t { far, sweep, master, neigh };
    struct job
    {
        job_kind      kind = job_kind::master;
        entity_id     body = null_entity;
        int           idx  = 0;                ///< Master chunk index (master jobs).
        std::uint64_t seq  = 0;
        double        prio = 0.0;
        std::shared_ptr<const ui::ground::bake_source> src;
        ui::ground::geometry    geom;
        ui::ground::bake_params prm;
        int px0 = 0, py0 = 0, pw = 0, ph = 0;
        std::uint32_t epoch = 0;               ///< Source epoch (sweeps).
        std::uint64_t hash  = 0;               ///< Neighbourhood page's folded hash.
        entity_id     neigh_tile = null_entity;
        float         neigh_rect[4] = { 0, 0, 0, 0 };
    };
    struct result
    {
        job_kind      kind = job_kind::master;
        entity_id     body = null_entity;
        int           idx = 0, pw = 0, ph = 0;
        std::uint64_t seq = 0;
        std::uint64_t hash = 0;
        std::uint32_t epoch = 0;
        std::vector<std::uint32_t> px;
        std::vector<std::uint32_t> mip[L];     ///< Levels 1..L-1 pieces (master jobs).
        std::vector<std::uint64_t> hashes;     ///< Sweep result.
        entity_id     neigh_tile = null_entity;
        float         neigh_rect[4] = { 0, 0, 0, 0 };
    };

    struct gpu_chunk
    {
        SDL_Texture*  tex = nullptr;
        int           w = 0, h = 0;
        std::uint32_t ver = 0;
        std::uint64_t last_used = 0;
    };

    // Bodies.
    body_state* find(entity_id id);
    const body_state* find(entity_id id) const;
    body_state* ensure_body(const world& w, entity_id id);
    void alloc_master(body_state& b);
    void drop_master(body_state& b);
    void refresh_source(body_state& b, const world& w);
    void make_room(long long need, entity_id keep_a, entity_id keep_b);
    static long long master_bytes(const body_state& b);

    // Pool.
    void worker_main();
    void enqueue(job j);
    static int pool_size();
    static void run_job(const job& j, result& d, double* level_ms);
    std::uint64_t next_seq() { return ++m_seq_counter; }
    job make_master_job(const body_state& b, int idx, double prio);
    /// Start a far-page bake of @p b against its current source: one job per
    /// 512 px piece, enqueued on the pool.
    void start_far(body_state& b);
    /// --verify: drain results until @p done() holds (the pool bakes; the
    /// main thread waits — a capture never races it).
    template <class Pred> void wait_until(Pred done);
    job make_sweep_job(const body_state& b);
    int  waiting() const;
    void feed(bool verify);
    void drain();
    void land(result& d, bool sync);
    void bake_sync(job j);                     ///< Run on the main thread and land.

    // GPU.
    void flush_gpu();
    bool upload(SDL_Renderer* r, body_state& b, int level, int idx);
    void evict_gpu();
    void publish(ui_state& ui, const body_state* b) const;

    /// The master-chunk window under a level-l view (ci unwrapped; cj clamped).
    struct window { int ci_lo = 0, ci_hi = -1, cj_lo = 0, cj_hi = -1; };
    window window_of(const body_state& b, int level, const ground_request& req, int margin) const;

    std::unordered_map<entity_id, std::unique_ptr<body_state>> m_bodies;
    entity_id     m_active = null_entity;   ///< Body the canvas draws (GPU textures are its).
    entity_id     m_prebake = null_entity;  ///< The pre-bake target.
    std::uint64_t m_frame = 0;
    bool          m_in_play = false;        ///< A tick has run: background bodies may bake.
    const world*  m_world = nullptr;        ///< The world pump last saw (sources are re-taken from it).

    // The last request (what the pump prioritises).
    bool          m_view_valid = false;
    int           m_view_level = 0;
    window        m_view_win;
    double        m_view_cx = 0.0, m_view_cy = 0.0; ///< View centre, master chunk units.

    // GPU textures of the active body: key = level << 24 | chunk index.
    std::unordered_map<std::uint32_t, gpu_chunk> m_gpu;
    SDL_Texture*  m_far_tex = nullptr;
    std::uint32_t m_far_tex_ver = ~0u;
    std::vector<std::uint32_t> m_publish_keys; ///< Visible keys of the drawn level, on GPU.
    int           m_publish_level = -1;
    int           m_pending_uploads = 0;

    // Pool plumbing. The workers start lazily on the first enqueue.
    std::vector<std::thread> m_workers;
    mutable std::mutex      m_mx;
    std::condition_variable m_cv;      ///< Workers wait on jobs.
    std::condition_variable m_done_cv; ///< The --verify wait on results.
    std::vector<job>        m_jobs;
    std::vector<result>     m_results;
    int                     m_inflight = 0;
    bool                    m_quit = false;
    std::uint64_t           m_seq_counter = 0;

    bake_stats m_bake_counters;
    stats      m_stats;                 ///< Cumulative parts (guarded by m_mx).

    // BL-1241: the Selection band's neighbourhood page — one small FLAT bake
    // around the selected tile (the band is a plan view, not the canvas).
    ui::ground::geometry m_neigh_geom;
    entity_id     m_neigh_body = null_entity;
    SDL_Texture*  m_neigh = nullptr;
    int           m_neigh_w = 0, m_neigh_h = 0;
    std::uint64_t m_neigh_hash = 0;
    std::uint64_t m_neigh_want = 0;
    std::uint64_t m_neigh_job  = 0;
    entity_id     m_neigh_tile = null_entity;
    float         m_neigh_rect[4] = { 0, 0, 0, 0 };
    bool          m_neigh_ready = false;
    std::vector<std::uint32_t> m_neigh_px;     ///< Landed, waiting for upload.
    bool          m_neigh_upload = false;
    static constexpr double k_neigh_px_per_r = 48.0;

    // Fill timing (off by default): IO_GROUND_FILL_LOG=1 prints, per rung
    // change, the wall time until the drawn level's visible chunks are all on
    // the GPU, per body switch the time until the far page and the view are
    // final, per pre-bake the master's wall time, and per re-bake the time
    // from the sweep that found it dirty to its upload. IO_GROUND_BENCH=1
    // also runs the pool path under --verify, so a script can drive it.
    using clock = std::chrono::steady_clock;
    bool              m_fill_pending = false;
    clock::time_point m_fill_t0{};
    int               m_fill_level = -1;
    bool              m_visit_pending = false;
    clock::time_point m_visit_t0{};
    bool              m_far_pending = false;
    bool              m_master_pending = false; ///< The visited body's whole master, timed from the visit.
    bool              m_prebake_pending = false;
    clock::time_point m_prebake_t0{};
    bool              m_rebake_pending = false;
    clock::time_point m_rebake_t0{};
};
