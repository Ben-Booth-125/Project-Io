#pragma once

#include <imgui.h>

#include <array>
#include <chrono>
#include <cstddef>

namespace ui {

/// One frame's timing breakdown plus the draw volume that produced it.
///
/// The three measured phases are wall-clock spans taken inside `app::render`:
/// **build** (the ImGui panel/canvas construction), **submit** (`ImGui::Render` +
/// the SDLRenderer3 draw-data pass) and **present** (`SDL_RenderPresent`). Their
/// sum is less than `total_ms`, which is measured begin-to-begin across the whole
/// frame; the remainder (`other_ms`) is the event pump and the simulation step.
struct frame_sample
{
    float total_ms   = 0.0f; ///< Full frame period, render()-begin to render()-begin.
    float build_ms   = 0.0f; ///< Time constructing the ImGui frame (panels + canvases).
    float submit_ms  = 0.0f; ///< Time in ImGui::Render + RenderDrawData (draw-call submission).
    float present_ms = 0.0f; ///< Time in SDL_RenderPresent. With VSync on this ABSORBS the wait for the refresh — a large value here is idle, not cost.
    int   vertices   = 0;    ///< ImDrawData::TotalVtxCount for the frame.
    int   draw_cmds  = 0;    ///< Σ ImDrawList::CmdBuffer.Size — one command is typically one GPU draw call.

    /// Everything outside the three measured phases: the SDL event pump and the
    /// simulation/economy step that run between one render() and the next.
    ///
    /// @return Unattributed milliseconds, clamped at zero.
    float other_ms() const;
};

/// Rolling frame-time instrument behind the frame-budget HUD (BL-249, the v0.1.0
/// quality audit). A fixed ring of recent `frame_sample`s plus the derived
/// last / average / max / 1%-low figures the audit is scored against.
///
/// **Sampling model.** A frame's *period* is only known once the next frame
/// starts, so `begin_frame` closes out the previous frame: it stamps the period
/// onto the sub-measures staged by that frame's `mark_*` calls and pushes the
/// completed sample. The four calls therefore bracket one render pass:
///
/// ```
/// begin_frame();                 // top of render()
///   ... build the ImGui frame ...
/// mark_build_end();              // immediately before ImGui::Render()
///   ... ImGui::Render() + RenderDrawData ...
/// mark_submit_end(draw_data);    // after RenderDrawData
///   ... an F12 / verify capture may land here ...
/// mark_present_begin();          // immediately before SDL_RenderPresent
/// mark_present_end();            // after SDL_RenderPresent
/// ```
///
/// Present is deliberately bracketed by its own pair rather than continuing from
/// the submit mark: a screenshot readback sits between the two, and charging its
/// cost to `present_ms` would have the HUD name the GPU for a disk write.
///
/// Pure instrumentation — it reads the clock and ImGui's own draw-data counters
/// and touches no simulation state, so it cannot affect determinism.
class frame_stats
{
public:
    /// Frames retained. Five seconds at 60 Hz — long enough for a pan across the
    /// full tile grid to show its tail, short enough that a fixed spike ages out
    /// instead of pinning the max forever.
    static constexpr std::size_t capacity = 300;

    /// v0.1.0 audit targets (ROADMAP.md § v0.1.0 — Audit instruments): average
    /// frame under 8 ms, worst frame inside one 60 Hz refresh.
    static constexpr float target_avg_ms = 8.0f;
    static constexpr float target_max_ms = 16.7f;

    /// Close out the previous frame (pushing its completed sample) and start
    /// timing this one. Call once at the top of the frame, before any drawing.
    void begin_frame();

    /// Mark the end of the UI-build phase. Call immediately before `ImGui::Render`.
    void mark_build_end();

    /// Mark the end of the draw-submission phase and capture this frame's draw
    /// volume. Call after the renderer backend has consumed the draw data.
    ///
    /// @param draw_data The frame's draw data (`ImGui::GetDrawData()`), read for
    ///                  vertex and command counts only.
    void mark_submit_end(const ImDrawData& draw_data);

    /// Start the present phase. Call immediately before `SDL_RenderPresent`, after
    /// any capture work, so only the present itself is timed.
    void mark_present_begin();

    /// Mark the end of the present phase. Call after `SDL_RenderPresent`.
    void mark_present_end();

    /// @return Samples retained, saturating at `capacity`.
    std::size_t count() const { return m_count; }

    /// @return The most recently completed sample; a zeroed sample before the
    ///         first frame closes.
    const frame_sample& last() const;

    /// @return The retained sample with the largest `total_ms` — the spike the
    ///         HUD attributes; a zeroed sample when nothing is retained.
    const frame_sample& worst() const;

    /// @return Mean `total_ms` over the retained window, or 0 when empty.
    float average_ms() const;

    /// @return Largest retained `total_ms`, or 0 when empty.
    float max_ms() const;

    /// Mean of the slowest 1% of retained frames (at least one), the conventional
    /// "1% low" expressed as a duration rather than a frame rate. It reports the
    /// sustained tail, which a lone `max` cannot distinguish from a one-off hitch.
    ///
    /// @return The 1%-low frame time in ms, or 0 when empty.
    float low_1pct_ms() const;

private:
    using clock = std::chrono::steady_clock;

    /// Ring of completed samples. Value-initialised, so the zeroed element at
    /// index 0 doubles as the empty-state result for last() / worst().
    std::array<frame_sample, capacity> m_samples{};
    std::size_t m_next  = 0; ///< Write cursor into m_samples.
    std::size_t m_count = 0; ///< Samples retained, saturating at capacity.

    frame_sample m_pending{};              ///< Sub-measures of the frame in flight; pushed at the next begin_frame, when its period is known.
    bool         m_pending_valid = false;  ///< False until the first begin_frame has staged a frame.

    clock::time_point m_frame_begin{}; ///< begin_frame timestamp of the frame in flight.
    clock::time_point m_phase_begin{}; ///< Start of the phase the next mark_* call closes.
};

/// Draw the frame-budget HUD (BL-249) — last / average / max / 1%-low frame times
/// against the v0.1.0 targets, and, for the worst retained frame, the phase
/// breakdown plus the draw volume that explains it.
///
/// An **audit instrument, not shipped chrome**: it is off by default and only ever
/// reports measures actually taken. Allocation churn is deliberately absent — see
/// the note the HUD itself carries.
///
/// @param stats     The rolling instrument to report.
/// @param first_pos Position for the window the first time it appears; it is
///                  movable afterwards.
/// @param open      Visibility flag; the window's close button writes false.
void draw_frame_budget_hud(const frame_stats& stats, ImVec2 first_pos, bool& open);

} // namespace ui
