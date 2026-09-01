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
// for the bottom rung, and chunked tiers at 12/24/48/96 px matching the four
// zoom rungs above it. The canvas's ground_request carries the drawn hex
// radius; the smallest tier that covers it becomes the ACTIVE tier and its
// chunks are baked around the viewport, LRU-capped.
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
        ui::ground::geometry  geom;
        int                   cw = 0, ch = 0;
        std::unordered_map<std::uint32_t, chunk> chunks; ///< key = cj * cw + ci
    };

    /// A self-contained bake job: source snapshot + geometry + params travel
    /// with it, so the worker never reads a ground_layer member.
    struct job
    {
        int  tier = -1;             ///< -1 = the far page.
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

    static constexpr int k_tiers = 4;
    static constexpr double k_tier_ppr[k_tiers] = { 12.0, 24.0, 48.0, 96.0 };
    static constexpr std::size_t k_tier_cap[k_tiers] = { 60, 90, 48, 48 };
    tier_state    m_tiers[k_tiers];

    ui::ground::geometry m_far_geom;
    SDL_Texture*  m_far = nullptr;
    std::uint64_t m_far_hash = 0;
    std::uint32_t m_far_gen_checked = ~0u; ///< Far hash runs once per source generation.
    bool          m_far_ready  = false;
    bool          m_far_queued = false;

    // Worker plumbing. The worker starts lazily on the first enqueue.
    std::thread             m_worker;
    std::mutex              m_mx;
    std::condition_variable m_cv;
    std::deque<job>         m_jobs;
    std::vector<result>     m_results;
    int                     m_inflight = 0; ///< Jobs enqueued whose results have not landed (guarded by m_mx).
    bool                    m_quit = false;

    std::vector<std::uint32_t> m_scratch; ///< Synchronous-path bake buffer.

    static constexpr int    k_chunk_px      = 512;
    static constexpr double k_far_px_per_r  = 6.0;
    static constexpr int    k_max_queued    = 6; ///< Outstanding jobs cap — keeps the queue near the viewport.
};
