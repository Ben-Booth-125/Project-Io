#pragma once

#include "ui/ground_bake.hpp"
#include "ui/ui_state.hpp"

#include <SDL3/SDL.h>

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Ground layer (BL-732; BL-1246, one master) — the SDL half of the baked ground.
//
// ONE MASTER PER BODY (docs/ui/RENDERING.md § Level of detail, Ben 2026-10-09:
// "zooming doesn't add more detail"). Each body's ground is baked ONCE, whole
// body, at 128 px per hex at the one camera angle (22.5 deg), in 512 px chunks
// held in SYSTEM RAM. As each master chunk lands, the worker that baked it
// box-downsamples it into its pieces of the 64 / 32 / 16 / 8 px levels — the
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
// master and re-derives only its own mip pieces. A chunk whose move is
// INSTALLATIONS ONLY (terrain unchanged) re-bakes only a window around each
// changed installation and blits it in (ui::ground::installation_patch_rects,
// the window rule), re-deriving only those windows' mip pieces; past 40% of
// the chunk, or on any terrain change, the chunk re-bakes whole.
//
// RAM (BL-1259, Ben 2026-10-10): the HOME body's master is pinned — never
// dropped, and not counted against the budget. The other bodies share a
// budget (ram_budget, 8 GB): when one must leave RAM, the least-recently-
// visited one SPILLS to the disk cache — its finished chunks (master piece +
// mip pieces + the content hash each was baked against) are written by one
// writer thread, atomically (temp file + rename), and its RAM freed — keeping
// its far page. A revisit streams its chunks back on the pool as LOAD jobs, in
// the same view-first order as bakes; a chunk whose content hash moved
// meanwhile re-bakes instead, and a missing or corrupt file falls back to a
// re-bake silently. The cache is session-scoped (written and read by this run
// only, deleted at exit and on a new world) and bounded (k_disk_cap, LRU by
// body). Background bakes start only where they fit without dropping anything.
//
// Under --verify everything a frame draws is complete before tick returns: the
// master chunks under the view are baked on the pool and WAITED for, the far
// page and neighbourhood page bake on the main thread, and every visible
// texture is uploaded with no budget — a capture can never race the pool.
// Pre-bake and background bakes are off under --verify, and so is the disk
// cache (a budget drop is a plain drop) except where a script drives it
// explicitly and synchronously (verify.ground_spill / ground_restore).
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
    /// homeworld, from the wizard's Life round, or the cold path's world the
    /// moment it is built). Not under --verify. @p assume_surveyed bakes the
    /// body unmasked: the wizard's worlds carry no survey state until the
    /// finish (`init_survey_states`), and the home body is always surveyed in
    /// play, so its master is baked as play will show it.
    void prebake(const world& w, entity_id body, bool assume_surveyed = false);
    /// A ROUND BOUNDARY (STARTUP.md § Handoff): re-take the pre-bake target's
    /// source snapshot from @p w NOW, on the calling (main) thread, which must
    /// own @p w — a round's landed world, or the adopted one. The pre-bake
    /// takes no snapshot of its own while pump() is handed no world, so this
    /// is the only way a source moves before play. Only the master chunks
    /// whose content hash moved re-bake (the sweep). False when there is no
    /// pre-bake target or @p w no longer holds it at the same grid (the
    /// homeworld changed identity): the caller starts again.
    bool resnapshot(const world& w, bool assume_surveyed);
    /// AN IN-ROUND SNAPSHOT (STARTUP.md § Handoff, Ben 2026-10-10): the same
    /// as resnapshot, with a source a round worker built from its OWN world at
    /// a point inside the round (round 6, right after the campaign road
    /// network is laid) and handed across — the main thread never reads a
    /// world a worker is writing. @p src must be prepare_source of @p body with
    /// reveal_all == @p assume_surveyed and the registry play runs on, so its
    /// hashes agree with the boundary that follows. False when the pre-bake
    /// target is not @p body at the same grid.
    bool resnapshot(entity_id body, std::shared_ptr<const ui::ground::bake_source> src,
                    bool assume_surveyed);
    /// The master is complete AND current: every chunk landed against the
    /// latest source snapshot, and that snapshot's sweep has run (a chunk
    /// a boundary moved is not counted whole until it has re-baked).
    bool master_current(entity_id body) const;
    /// Master chunks landed and current / total for @p body (0 / 0 unknown).
    void master_progress(entity_id body, int& ready, int& total) const;
    /// Every master chunk of @p body has landed and is current.
    bool master_complete(entity_id body) const;
    /// The world just changed under the active body (a build placed): re-take
    /// its source snapshot next pump rather than at the cadence.
    void touch();

    /// Verify-only: bake @p body's WHOLE master now, from whatever state it is
    /// in, and return once it is complete AND current against a snapshot of
    /// @p w taken on entry — or false after @p timeout_ms. Blocks the calling
    /// (main) thread; the pool bakes. The only way a --verify run reaches a
    /// whole master on the synchronous path, which by design bakes only what
    /// each frame draws (pump never feeds the pool there): a script polling
    /// master_ready on that path waits forever at the view's coverage (2026-10-09:
    /// 1960 of 2975 at the 1720x1080 home view).
    bool complete_master(const world& w, entity_id body, double timeout_ms);
    /// IO_GROUND_BENCH is set: --verify runs the live pool path (pump feeds the
    /// pool, the master fills in the background), not the synchronous one.
    static bool pool_path_under_verify();

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
        bool      master_current = false;            ///< Active body: whole and swept against the latest snapshot.
        std::uint64_t uploads = 0;   ///< Texture uploads since reset_stats().
        int       pending_uploads = 0; ///< Visible chunks of the drawn level not yet on the GPU (last tick).
        bool      view_final = false;  ///< Last tick: every master chunk under the view landed, current and uploaded.
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
        // BL-1246, the partial re-bake (re-bakes only; first bakes are not counted):
        std::uint64_t chunk_patches = 0; ///< Re-bakes done as windows (of chunk_rebakes).
        std::uint64_t patch_windows = 0; ///< Windows those patches baked.
        std::uint64_t patch_px      = 0; ///< Pixels the windows baked (no apron).
        std::uint64_t rebake_px     = 0; ///< Pixels whole-chunk re-bakes baked.
        double        rebake_ms     = 0; ///< Worker time of every re-bake, whole and patched.
        // BL-1259, the disk cache:
        std::uint64_t chunk_loads    = 0; ///< Master chunks restored from the disk cache (not counted as bakes).
        std::uint64_t load_failures  = 0; ///< Loads that found a missing / corrupt / foreign file (re-baked instead).
        std::uint64_t load_skipped   = 0; ///< Spilled chunks whose hash had moved: re-baked without a load.
        std::uint64_t chunk_writes   = 0; ///< Chunk files written.
        std::uint64_t write_skipped  = 0; ///< Spilled chunks whose file already held their pixels.
        std::uint64_t write_failures = 0;
        std::uint64_t spills         = 0; ///< Masters spilled.
        double        load_ms        = 0; ///< Worker time of every load (read + check + decode).
        double        load_read_ms   = 0; ///< Of which: opening and reading the file.
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

    // --- BL-1259: the disk cache ---------------------------------------
    /// RAM budget across the bodies OTHER than home (home is pinned outside
    /// it). Verify may lower it to force a spill (verify.ground_set_ram_budget).
    long long ram_budget = k_ram_budget;
    struct cache_body
    {
        entity_id     body = null_entity;
        long long     bytes = 0;      ///< Chunk files of this body on disk.
        long long     raw_bytes = 0;  ///< What those files hold uncompressed (the ratio).
        int           files = 0;
        int           pending = 0;    ///< Chunk writes queued, not done.
        bool          resident = false; ///< Master in RAM (more than its far page).
        bool          pinned = false;
    };
    struct cache_info
    {
        std::string   root;           ///< Empty = no usable location (the cache is off).
        long long     cap = 0;        ///< Disk cap, bytes.
        long long     bytes = 0;      ///< Every body of this session.
        int           pending = 0;    ///< Chunk writes queued.
        bool          live = false;   ///< The budget spills (false: --verify synchronous path, drops).
        std::vector<cache_body> bodies;
    };
    cache_info cache_snapshot() const;
    /// Spill @p body's master to the disk cache now (any body, the home one
    /// included — a verify lever; the budget never spills home). @p sync: write
    /// on the calling thread and land the results before returning (the
    /// --verify synchronous path); otherwise queue to the writer thread.
    /// Returns chunk files to write; @p skipped = chunks whose file already
    /// held their pixels. -1 = no such body or no cache location.
    int  spill_body(entity_id body, bool sync, int* skipped = nullptr);
    /// Block until the writer thread is idle and its results have landed.
    void wait_cache_idle();
    /// Verify: bring back every spilled chunk of @p body from the cache on the
    /// pool and wait — a moved hash re-bakes, a bad file re-bakes. The source
    /// is re-taken from @p w and swept first.
    struct restore_result { int loaded = 0, rebaked = 0, failed = 0; double ms = 0; bool ok = false; };
    restore_result restore_body(const world& w, entity_id body, double timeout_ms);
    /// Verify: a digest over every READY master chunk of @p body — the chunk's
    /// index, its master piece and its share of every mip level — and how many.
    std::uint64_t master_digest(entity_id body, int* chunks = nullptr) const;
    /// Verify: drop @p body's master WITHOUT the cache (and forget its files) —
    /// the re-bake baseline.
    void drop_body(entity_id body);
    /// Verify: corrupt the lowest-index cached chunk file of @p body (flip
    /// bytes mid-payload). Returns that chunk index, or -1.
    int  corrupt_cached_chunk(entity_id body);

    /// Per-frame GPU upload budget for the drawn view and its ring, chunks
    /// (each <= 1 MB) — RENDERING.md § Chunks, cache and invalidation.
    static constexpr int         k_upload_budget = 8;
    /// Per-frame budget for prefetching the adjacent levels, spent only when
    /// the drawn view is whole (measured 2026-10-09: an unthrottled prefetch
    /// of the finer level held a whole-grid pan at ~15 fps).
    static constexpr int         k_prefetch_budget = 2;
    /// GPU texture LRU cap (textures of the active body; each <= 1 MB).
    static constexpr std::size_t k_gpu_cap = 320;
    /// RAM budget across the bodies other than home (TECH_FOUNDATIONS.md §
    /// Target hardware: 16 GB minimum; RENDERING.md § Chunks, cache and
    /// invalidation): the home master is pinned outside it.
    static constexpr long long   k_ram_budget = 8LL * 1024 * 1024 * 1024;
    /// Disk cache cap across bodies (BL-1259): LRU by body past it.
    /// IO_GROUND_CACHE_CAP_GB overrides.
    static constexpr long long   k_disk_cap = 32LL * 1024 * 1024 * 1024;
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
        /// The source each stored chunk's pixels were baked against (null = none
        /// stored): a dirty chunk diffs it against the current source to re-bake
        /// only the windows its installations moved.
        std::vector<std::shared_ptr<const ui::ground::bake_source>> baked_src;
        std::vector<std::uint64_t> job;        ///< Sequence of the outstanding job (0 = none).
        std::vector<std::uint8_t>  ready;      ///< Landed at least once since the master was (re)allocated.
        std::vector<std::uint8_t>  dirty;      ///< Ready, but its hash moved: re-bake.
        // BL-1259, the disk cache (per master chunk):
        std::vector<std::uint8_t>  disk_has;     ///< A file for this chunk was written (or is queued) this session.
        std::vector<std::uint64_t> disk_hash;    ///< The content hash that file was baked against.
        std::vector<std::uint8_t>  disk_same;    ///< The RAM pixels are the file's (a spill need not rewrite it).
        std::vector<std::uint8_t>  disk_pending; ///< Its write is queued: neither load nor bake it yet.
        std::vector<std::uint32_t> disk_size;    ///< File bytes (0 = none).
        std::vector<std::uint32_t> disk_raw;     ///< Uncompressed bytes the file holds.
        std::filesystem::path      disk_dir;     ///< This body's cache directory (empty = not yet keyed).
        bool                       disk_purged = false; ///< The directory was cleared for this session.
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
        bool          reveal_all = false;      ///< Snapshot unmasked (the wizard's home pre-bake).
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

    enum class job_kind : std::uint8_t { far, sweep, master, neigh, load };
    struct job
    {
        job_kind      kind = job_kind::master;
        entity_id     body = null_entity;
        int           idx  = 0;                ///< Master chunk index (master jobs).
        std::uint64_t seq  = 0;
        double        prio = 0.0;
        std::shared_ptr<const ui::ground::bake_source> src;
        /// Master jobs: the source the stored chunk was baked against, when the
        /// chunk may be re-baked as windows (null = bake it whole).
        std::shared_ptr<const ui::ground::bake_source> old_src;
        ui::ground::geometry    geom;
        ui::ground::bake_params prm;
        int px0 = 0, py0 = 0, pw = 0, ph = 0;
        std::uint32_t epoch = 0;               ///< Source epoch (sweeps).
        std::uint64_t hash  = 0;               ///< Neighbourhood page's folded hash.
        entity_id     neigh_tile = null_entity;
        float         neigh_rect[4] = { 0, 0, 0, 0 };
        std::filesystem::path file;            ///< Load jobs: the chunk file.
        std::uint64_t stamp = 0;               ///< Load jobs: the stamp its header must carry.
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
        std::shared_ptr<const ui::ground::bake_source> src; ///< Master jobs: what it was baked against.
        bool          rebake = false;          ///< Master jobs: the chunk was already stored.
        /// A partial re-bake: windows (chunk-relative pixels) and their mip
        /// pieces, to blit over the stored chunk. Empty with `patched` = the
        /// chunk's pixels did not change at all.
        struct patch
        {
            ui::ground::pixel_rect     r;
            std::vector<std::uint32_t> px;
            std::vector<std::uint32_t> mip[L];
        };
        bool               patched = false;
        std::vector<patch> patches;
        double             ms = 0.0;           ///< Worker bake time (master jobs).
        bool               loaded = false;     ///< Load jobs: the file decoded (else: re-bake).
        double             read_ms = 0.0;      ///< Load jobs: opening and reading the file.
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
    void set_source(body_state& b, std::shared_ptr<const ui::ground::bake_source> src);
    /// A boundary's new source (either resnapshot): log what moved, set it,
    /// and feed the pool so its sweep runs.
    void land_boundary(body_state& b, std::shared_ptr<const ui::ground::bake_source> src,
                       const char* what);
    void make_room(long long need, entity_id keep_a, entity_id keep_b);
    bool pinned(entity_id id) const { return id != null_entity && (id == m_home || id == m_prebake); }
    long long budget_total() const; ///< RAM of every body outside the pin.
    static long long master_bytes(const body_state& b);

    // Pool.
    void worker_main();
    void enqueue(job j);
    static int pool_size();
    static void run_job(const job& j, result& d, double* level_ms);
    std::uint64_t next_seq() { return ++m_seq_counter; }
    job make_master_job(const body_state& b, int idx, double prio);
    /// The job that brings chunk @p idx back: a LOAD from the disk cache when
    /// its file holds the current content (or the hashes are not yet known),
    /// otherwise a bake. @p allow_disk false = always a bake.
    job make_chunk_job(body_state& b, int idx, double prio, bool allow_disk);
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

    // --- The disk cache (BL-1259) ---
    struct spill_chunk { int idx = 0, pw = 0, ph = 0; std::uint64_t hash = 0; };
    struct disk_task
    {
        enum class kind : std::uint8_t { spill, remove_dir } k = kind::spill;
        entity_id     body = null_entity;
        std::uint64_t gen = 0;
        std::filesystem::path dir;
        bool          purge = false;       ///< Clear the directory first (its first spill this session).
        std::uint64_t stamp = 0;
        int           cw[L] = {};          ///< Level chunk columns.
        int           lw[L] = {}, lh[L] = {}; ///< Level widths / heights, px.
        std::vector<std::vector<std::uint32_t>> lv[L]; ///< The level chunks, moved out of the body.
        std::vector<spill_chunk> chunks;   ///< The chunks to write.
        /// Who may be evicted if the cap is passed: other bodies' directories,
        /// least recently visited first (a main-thread snapshot).
        std::vector<std::pair<entity_id, std::filesystem::path>> evict_order;
    };
    struct disk_result
    {
        entity_id     body = null_entity;
        std::uint64_t gen = 0;
        int           idx = -1;            ///< -1: a whole-body event (its files evicted).
        bool          ok = false;
        std::uint32_t bytes = 0;
        std::uint32_t raw = 0;
    };
    bool cache_live() const { return m_cache_live && !m_disk_root.empty(); }
    void cache_init();                     ///< Root, stamp, the stale-directory purge (once).
    void ensure_disk_keys(body_state& b);
    /// Move @p b's master out of RAM into a write task (and run it, @p sync).
    int  spill_master(body_state& b, bool sync, int* skipped, const char* why = "RAM budget");
    void writer_main();
    void run_disk_task(disk_task& t, std::vector<disk_result>& out); ///< Writer thread (or the caller, sync).
    void land_disk(const disk_result& r);
    void drain_disk();
    void forget_disk(body_state& b);       ///< Its files are no longer trusted.
    static bool load_chunk_file(const job& j, result& d);

    std::filesystem::path m_disk_root;
    bool          m_disk_init = false;
    bool          m_cache_live = true;      ///< False on the --verify synchronous path.
    std::uint64_t m_disk_stamp = 0;         ///< Format version x executable identity x bake params.
    long long     m_disk_cap = k_disk_cap;
    std::uint64_t m_world_gen = 1;          ///< Bumps on forget_world: stale writer results land nowhere.
    std::thread   m_writer;
    std::mutex    m_wmx;
    std::condition_variable m_wcv;          ///< The writer waits on tasks.
    std::condition_variable m_widle_cv;     ///< wait_cache_idle waits on the writer.
    std::deque<disk_task>    m_wtasks;
    std::vector<disk_result> m_wresults;
    bool          m_wbusy = false;
    bool          m_wquit = false;
    /// Writer-owned: bytes per body directory this session (the cap's ledger).
    std::unordered_map<std::string, long long> m_wdir_bytes;
    std::vector<std::filesystem::path> m_session_dirs; ///< Deleted at exit and on a new world.
    entity_id     m_home = null_entity;     ///< The pinned body (the world's home body).
    bool          m_view_final = false;

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
    bool          m_fresh_view = false;   ///< The next view is the first on this body: upload it whole.

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
    // IO_GROUND_STALL_LOG=1 prints every body's chunk states (ready / dirty /
    // in the pool / needing a job), the queue and the source epochs every 60
    // pumps — the reading that names why a master is not advancing.
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
    bool              m_boundary_log = false; ///< resnapshot() moved the source: log its sweep.
    bool              m_rebake_pending = false;
    clock::time_point m_rebake_t0{};
    // The partial re-bake reading: a round boundary's sweep snapshots the
    // counters, and pump() prints what it cost once the master is current.
    bool              m_boundary_track = false;
    bake_stats        m_boundary_base{};
    clock::time_point m_boundary_t0{};
};
