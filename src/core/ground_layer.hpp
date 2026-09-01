#pragma once

#include "ui/ground_bake.hpp"
#include "ui/ui_state.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Ground layer (BL-732) — the SDL half of the baked-chunk ground renderer.
//
// Owns the SDL_Textures the Planetary canvas draws its ground from: one
// low-res whole-body FAR page (baked synchronously on body switch, so the
// painterly read arrives with the body) and a grid of full-res 512 px chunks
// baked a budgeted few per frame against the canvas's last ground_request
// (fallback-by-coverage hides the latency). Content-hash invalidation: a
// chunk whose region_hash moves (urban transform, survey reveal) re-bakes.
//
// The pure bake lives in ui/ground_bake.{hpp,cpp}; this file is plumbing —
// textures, budgets, hashes — and stays out of the headless tier.
// ---------------------------------------------------------------------------

struct world;

class ground_layer
{
public:
    /// Per-frame driver. Ensures the cache describes @p ui.active_body, bakes
    /// the far page + budgeted chunks, and fills @p ui.ground for the canvas.
    /// @p bake_everything bakes every chunk synchronously — the --verify path,
    /// where a capture must not race the budget.
    void tick(SDL_Renderer* r, const world& w, ui_state& ui, bool bake_everything);

    /// Destroy every texture. Call before the renderer goes down.
    void shutdown();

    /// The C-F dials, exposed so a tuning pass (and only a tuning pass) can
    /// move them; the shipped values live in ground_bake.hpp's defaults.
    ui::ground::bake_params params;

private:
    struct chunk
    {
        SDL_Texture*  tex  = nullptr;
        std::uint64_t hash = 0;
        bool          ready = false;
    };

    void reset(entity_id body, const world& w);
    bool bake_chunk(SDL_Renderer* r, int ci, int cj);
    void bake_far(SDL_Renderer* r);
    void publish(ui_state& ui) const;

    entity_id                m_body = null_entity;
    ui::ground::geometry     m_geom;      ///< Full-res geometry.
    ui::ground::geometry     m_far_geom;  ///< Far-page geometry.
    ui::ground::bake_source  m_src;       ///< Rebuilt when the source hash moves.
    std::uint64_t            m_src_stamp = 0; ///< Cheap whole-source change probe.
    std::vector<chunk>       m_chunks;
    int                      m_cw = 0, m_ch = 0;
    SDL_Texture*             m_far = nullptr;
    bool                     m_far_ready = false;
    std::vector<std::uint32_t> m_scratch; ///< Bake buffer, reused.

    static constexpr int    k_chunk_px       = 512;
    static constexpr double k_full_px_per_r  = 24.0; ///< Full-res baked px per hex circumradius.
    static constexpr double k_far_px_per_r   = 6.0;  ///< Far-page px per hex circumradius.
    static constexpr int    k_bakes_per_tick = 2;    ///< Chunk budget per frame (off the hot path).
};
