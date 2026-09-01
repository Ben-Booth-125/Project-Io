#include "ground_layer.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kSqrt3 = 1.7320508075688772;
}

void ground_layer::reset(entity_id body, const world& w)
{
    for (chunk& c : m_chunks)
        if (c.tex)
            SDL_DestroyTexture(c.tex);
    m_chunks.clear();
    if (m_far)
    {
        SDL_DestroyTexture(m_far);
        m_far = nullptr;
    }
    m_far_ready = false;
    m_body      = body;
    m_cw = m_ch = 0;
    if (body == null_entity || !w.bodies.count(body))
        return;
    const body_component& b = w.bodies.at(body);
    if (b.grid_width <= 0 || b.grid_height <= 0)
        return; // a gridless body (the star) has no ground to bake
    m_geom     = ui::ground::make_geometry(b.grid_width, b.grid_height, k_full_px_per_r);
    m_far_geom = ui::ground::make_geometry(b.grid_width, b.grid_height, k_far_px_per_r);
    m_src      = ui::ground::prepare_source(w, body);
    m_src_stamp = 0;
    m_cw = (m_geom.W + k_chunk_px - 1) / k_chunk_px;
    m_ch = (m_geom.H + k_chunk_px - 1) / k_chunk_px;
    m_chunks.assign(static_cast<std::size_t>(m_cw) * m_ch, chunk{});
}

void ground_layer::bake_far(SDL_Renderer* r)
{
    // Synchronous by design: the far page is small (≈6 px per hex circumradius,
    // under 2M pixels on the largest body) and it is what makes the painterly
    // read arrive WITH the body rather than assembling in front of the player.
    m_scratch.assign(static_cast<std::size_t>(m_far_geom.W) * m_far_geom.H, 0u);
    ui::ground::bake_region(m_src, m_far_geom, params,
                            0, 0, m_far_geom.W, m_far_geom.H, m_scratch.data());
    if (!m_far)
    {
        m_far = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                  m_far_geom.W, m_far_geom.H);
        if (!m_far)
            return;
        SDL_SetTextureScaleMode(m_far, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(m_far, SDL_BLENDMODE_BLEND);
    }
    SDL_UpdateTexture(m_far, nullptr, m_scratch.data(), m_far_geom.W * 4);
    m_far_ready = true;
}

bool ground_layer::bake_chunk(SDL_Renderer* r, int ci, int cj)
{
    const int px0 = ci * k_chunk_px;
    const int py0 = cj * k_chunk_px;
    const int pw  = std::min(k_chunk_px, m_geom.W - px0);
    const int ph  = std::min(k_chunk_px, m_geom.H - py0);
    if (pw <= 0 || ph <= 0)
        return false;
    chunk& c = m_chunks[static_cast<std::size_t>(cj) * m_cw + ci];
    const std::uint64_t h = ui::ground::region_hash(m_src, m_geom, px0, py0, pw, ph);
    if (c.ready && c.hash == h)
        return false; // current — no work
    m_scratch.assign(static_cast<std::size_t>(pw) * ph, 0u);
    ui::ground::bake_region(m_src, m_geom, params, px0, py0, pw, ph, m_scratch.data());
    if (!c.tex)
    {
        c.tex = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, pw, ph);
        if (!c.tex)
            return false;
        SDL_SetTextureScaleMode(c.tex, SDL_SCALEMODE_LINEAR);
        SDL_SetTextureBlendMode(c.tex, SDL_BLENDMODE_BLEND);
    }
    SDL_UpdateTexture(c.tex, nullptr, m_scratch.data(), pw * 4);
    c.hash  = h;
    c.ready = true;
    return true;
}

void ground_layer::tick(SDL_Renderer* r, const world& w, ui_state& ui, bool bake_everything)
{
    const entity_id body = ui.active_body;
    if (body != m_body)
        reset(body, w);
    if (m_body == null_entity || m_cw == 0)
    {
        ui.ground = ground_view{};
        return;
    }

    // Source refresh: the fields the bake reads move rarely (urban transform,
    // survey reveal), so the source arrays are rebuilt on a slow cadence rather
    // than per frame; the per-chunk hash below turns a real change into a
    // re-bake on the next pass. --verify refreshes every call: a capture must
    // see this frame's world, not last second's.
    if (bake_everything || ++m_src_stamp >= 30)
    {
        m_src = ui::ground::prepare_source(w, m_body);
        m_src_stamp = 0;
    }

    if (!m_far_ready)
        bake_far(r);

    if (bake_everything)
    {
        for (int cj = 0; cj < m_ch; ++cj)
            for (int ci = 0; ci < m_cw; ++ci)
                bake_chunk(r, ci, cj);
    }
    else if (ui.ground_req.valid && ui.ground_req.body == m_body)
    {
        // The visible window, grown by one chunk of slack so a pan meets baked
        // ground. x wraps (the cylinder); y clamps.
        const double s  = m_geom.s;
        const int ci_lo = static_cast<int>(std::floor(ui.ground_req.x0 * s / k_chunk_px)) - 1;
        const int ci_hi = static_cast<int>(std::ceil(ui.ground_req.x1 * s / k_chunk_px));
        const int cj_lo = std::max(0, static_cast<int>(
            std::floor((ui.ground_req.y0 - m_geom.y_min) * s / k_chunk_px)) - 1);
        const int cj_hi = std::min(m_ch - 1, static_cast<int>(
            std::ceil((ui.ground_req.y1 - m_geom.y_min) * s / k_chunk_px)));
        int budget = k_bakes_per_tick;
        for (int cj = cj_lo; cj <= cj_hi && budget > 0; ++cj)
            for (int ci = ci_lo; ci <= ci_hi && budget > 0; ++ci)
            {
                const int cw = ((ci % m_cw) + m_cw) % m_cw;
                if (bake_chunk(r, cw, cj))
                    --budget;
            }
    }

    publish(ui);
}

void ground_layer::publish(ui_state& ui) const
{
    ground_view& v = ui.ground;
    v.body     = m_body;
    v.s        = m_geom.s;
    v.y_min    = m_geom.y_min;
    v.chunk_px = k_chunk_px;
    v.cw       = m_cw;
    v.ch       = m_ch;
    v.ready.assign(static_cast<std::size_t>(m_cw) * m_ch, 0);
    v.chunks.clear();
    for (int cj = 0; cj < m_ch; ++cj)
        for (int ci = 0; ci < m_cw; ++ci)
        {
            const chunk& c = m_chunks[static_cast<std::size_t>(cj) * m_cw + ci];
            if (!c.ready || !c.tex)
                continue;
            v.ready[static_cast<std::size_t>(cj) * m_cw + ci] = 1;
            const int px0 = ci * k_chunk_px;
            const int py0 = cj * k_chunk_px;
            const int pw  = std::min(k_chunk_px, m_geom.W - px0);
            const int ph  = std::min(k_chunk_px, m_geom.H - py0);
            ground_chunk_view cv;
            cv.x0  = static_cast<float>(px0 / m_geom.s);
            cv.x1  = static_cast<float>((px0 + pw) / m_geom.s);
            cv.y0  = static_cast<float>(py0 / m_geom.s + m_geom.y_min);
            cv.y1  = static_cast<float>((py0 + ph) / m_geom.s + m_geom.y_min);
            cv.tex = c.tex;
            v.chunks.push_back(cv);
        }
    v.far_ready = m_far_ready && m_far;
    v.far.tex   = m_far;
    v.far.x0    = 0.0f;
    v.far.x1    = static_cast<float>(m_far_geom.W / m_far_geom.s);
    v.far.y0    = static_cast<float>(m_far_geom.y_min);
    v.far.y1    = static_cast<float>(m_far_geom.H / m_far_geom.s + m_far_geom.y_min);
}

void ground_layer::shutdown()
{
    for (chunk& c : m_chunks)
        if (c.tex)
            SDL_DestroyTexture(c.tex);
    m_chunks.clear();
    if (m_far)
    {
        SDL_DestroyTexture(m_far);
        m_far = nullptr;
    }
    m_far_ready = false;
    m_body      = null_entity;
    m_cw = m_ch = 0;
}
