#pragma once

#include "ui/ground_bake.hpp"
#include "ui/ui_state.hpp"

#include <SDL3/SDL.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Ground layer (BL-732 / wave 2) — the SDL half of the baked-chunk ground.
//
// Owns the textures the Planetary canvas draws its ground from, in TIERS that
// pair with the stepped x2 zoom ladder (Ben, 2026-09-01 — stepped zoom so
// every level is crisp): one whole-body FAR page at ~6 px per hex circumradius
// for the bottom rung, and chunked tiers at 12/24/48/96/192 px for the rungs
// above it. The canvas's ground_request carries the drawn hex radius; the
// smallest tier at or above it becomes the ACTIVE tier — drawn minified, never
// magnified (BL-1244) — and its chunks are baked around the viewport,
// LRU-capped, with the next tier down standing in while they fill. Every bake
// is supersampled 2x and downsampled before upload (ui/ground_bake).
//
// ALL BAKING RUNS ON A WORKER THREAD (wave 2's perf half): the pure bake
// (ui/ground_bake) executes against an immutable source snapshot; the render
// thread only hashes, enqueues, uploads finished buffers into SDL textures and
// publishes the view. A generation counter discards results that outlive their
// source or body. Under --verify everything bakes synchronously on the main
// thread instead, so a capture can never race the worker.
// ---------------------------------------------------------------------------

struct world;

class ground_layer
{
public:
    ~ground_layer();

    /// Per-frame driver. See file header. @p bake_everything = the --verify
    /// path: synchronous main-thread bakes of the far page plus every chunk
    /// (all tiers) the last request touches.
    void tick(SDL_Renderer* r, const world& w, ui_state& ui, bool bake_everything);

    /// Stop the worker and destroy every texture. Call before the renderer dies.
    void shutdown();

    /// The C-F dials; shipped values are ground_bake.hpp's defaults.
    ui::ground::bake_params params;

    /// Bake-cost and residency instrumentation (BL-1244): cumulative bake
    /// milliseconds and bake count per slot since the last reset_stats(),
    /// slot 0 = the far page, slot 1 + t = chunked tier t; plus the resident
    /// texture bytes per slot NOW. Read by verify.ground_stats().
    static constexpr int k_stat_slots = 8;
    struct stats
    {
        double    bake_ms[k_stat_slots]        = {};
        int       bakes[k_stat_slots]          = {};
        long long resident_bytes[k_stat_slots] = {};
        double    tier_ppr[k_stat_slots]       = {};
        int       active_slot = -1;
    };
    stats stats_snapshot() const;
    void  reset_stats();

    /// Verify-only: under bake_everything, bake at most this many chunks per
    /// tick (-1 = no limit) — so a script can capture the deterministic
    /// MID-FILL frame a rung change shows while its tier fills (BL-1244).
    int verify_fill_limit = -1;
    /// BL-1241: bake counters, so a verify run can show what re-bakes and when
    /// (a re-bake must follow a construction event, never a tick). Monotonic
    /// across body switches; read-only to callers.
    struct bake_stats
    {
        std::uint64_t far_bakes = 0;
        std::uint64_t chunk_bakes = 0;
        std::uint64_t chunk_rebakes = 0; ///< A chunk that was already ready, baked again.
        std::uint64_t neigh_bakes = 0;
    };
    const bake_stats& bake_counters() const { return m_bake_counters; }

    /// BL-1241: a digest of every installation in the current source snapshot
    /// (0 with no source) — moves exactly when what the structure pass draws does.
    std::uint64_t installation_digest() const;

    /// BL-1241: the recipe registry a processing facility's structure stamp
    /// reads its family (recipe group) from. Set by the app; null = general form.
    const recipe_registry* registry = nullptr;

private:
    struct chunk
    {
        SDL_Texture*  tex  = nullptr;
        std::uint64_t hash = 0;
        bool          ready  = false;
        bool          queued = false;
        std::uint64_t last_want = 0; ///< Frame stamp for LRU eviction.
    };

    struct tier_state
    {
        double                geom_ppr = 0.0;
        double                baked_sy = 1.0; ///< Tilt the tier is baked at (BL-737).
        ui::ground::geometry  geom;
        int                   cw = 0, ch = 0;
        std::unordered_map<std::uint32_t, chunk> chunks; ///< key = cj * cw + ci
    };

    /// A self-contained bake job: source snapshot + geometry + params travel
    /// with it, so the worker never reads a ground_layer member.
    struct job
    {
        int  tier = -1;             ///< -1 = the far page; -2 = the neighbourhood page (BL-1241).
        int  ci = 0, cj = 0;
        int  px0 = 0, py0 = 0, pw = 0, ph = 0;
        std::uint64_t hash = 0;
        std::uint32_t gen  = 0;
        std::shared_ptr<const ui::ground::bake_source> src;
        ui::ground::geometry   geom;
        ui::ground::bake_params prm;
    };

    struct result
    {
        int  tier = -1;
        int  ci = 0, cj = 0, pw = 0, ph = 0;
        std::uint64_t hash = 0;
        std::uint32_t gen  = 0;
        std::vector<std::uint32_t> px;
    };

    void reset(entity_id body, const world& w);
    void refresh_source(const world& w);
    void worker_main();
    void enqueue(job j);
    void drain_results(SDL_Renderer* r);
    void upload(SDL_Renderer* r, const result& d);
    void bake_now(SDL_Renderer* r, const job& j); ///< Synchronous (--verify) path.
    job  make_chunk_job(int tier, int ci, int cj) const;
    void evict(tier_state& t, std::size_t cap);
    void publish(ui_state& ui) const;

    entity_id     m_body = null_entity;
    std::uint32_t m_gen  = 0;       ///< Bumped on body switch and source refresh.
    std::uint64_t m_frame = 0;      ///< LRU clock.
    int           m_src_age = 0;    ///< Frames since the source snapshot was taken.
    int           m_active_tier = -1;
    std::shared_ptr<const ui::ground::bake_source> m_src;

    static constexpr int k_tiers = ui::ground::k_tier_count;
    static constexpr double k_tier_ppr[k_tiers] = {
        ui::ground::k_tier_ladder[0], ui::ground::k_tier_ladder[1], ui::ground::k_tier_ladder[2],
        ui::ground::k_tier_ladder[3], ui::ground::k_tier_ladder[4] };
    static constexpr std::size_t k_tier_cap[k_tiers] = { 60, 90, 48, 48, 48 };
    /// Per-tier supersample ceiling (BL-1244), min'd with params.supersample.
    /// The 192 px tier bakes single-sample: at 2x its fill time at the
    /// reference window measured ~30 s on the one worker (99 chunks x ~305 ms),
    /// past what the brief allowed — the 2x-at-192 call is Ben's. Raise this
    /// entry to 2 to take it.
    static constexpr int k_tier_supersample[k_tiers] = { 2, 2, 2, 2, 1 };
    tier_state    m_tiers[k_tiers];

    ui::ground::geometry m_far_geom;
    SDL_Texture*  m_far = nullptr;
    std::uint64_t m_far_hash = 0;
    std::uint32_t m_far_gen_checked = ~0u; ///< Far hash runs once per source generation.
    bool          m_far_ready  = false;
    bool          m_far_queued = false;

    // Worker plumbing. The worker starts lazily on the first enqueue.
    std::thread             m_worker;
    mutable std::mutex      m_mx;
    std::condition_variable m_cv;
    std::deque<job>         m_jobs;
    std::vector<result>     m_results;
    int                     m_inflight = 0; ///< Jobs enqueued whose results have not landed (guarded by m_mx).
    bool                    m_quit = false;

    std::vector<std::uint32_t> m_scratch; ///< Synchronous-path bake buffer.
    bake_stats m_bake_counters;

    // BL-1241: the Selection band's neighbourhood page — one small flat bake
    // around the selected tile at a fixed close tier, re-baked when its window
    // hash moves (a selection change, a build in view).
    ui::ground::geometry m_neigh_geom;
    SDL_Texture*  m_neigh = nullptr;
    int           m_neigh_w = 0, m_neigh_h = 0;
    std::uint64_t m_neigh_hash = 0;
    std::uint64_t m_neigh_want = 0;     ///< Hash of the page last enqueued.
    entity_id     m_neigh_tile = null_entity;
    entity_id     m_neigh_want_tile = null_entity;
    float         m_neigh_rect[4] = { 0, 0, 0, 0 }; ///< Canonical x0, y0, x1, y1 of the ready page.
    float         m_neigh_want_rect[4] = { 0, 0, 0, 0 };
    bool          m_neigh_ready  = false;
    bool          m_neigh_queued = false;
    static constexpr double k_neigh_px_per_r = 48.0;

    void  note_bake(int tier, double ms); ///< Accumulates m_stats; caller holds m_mx.
    stats m_stats;

    static constexpr int    k_chunk_px      = 512;
    static constexpr double k_far_px_per_r  = ui::ground::k_far_ppr;
    static constexpr int    k_max_queued    = 6; ///< Outstanding jobs cap — keeps the queue near the viewport.
};
