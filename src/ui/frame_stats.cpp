#include "frame_stats.hpp"

#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <functional>

namespace ui {

float frame_sample::other_ms() const
{
    const float measured = build_ms + submit_ms + present_ms;
    return std::max(0.0f, total_ms - measured);
}

namespace {

/// Milliseconds between two steady-clock stamps.
float elapsed_ms(std::chrono::steady_clock::time_point from,
                 std::chrono::steady_clock::time_point to)
{
    const std::chrono::duration<float, std::milli> span = to - from;
    return span.count();
}

/// Share of the frame a phase has to own before the HUD names it as the cause.
/// Below this, no single phase explains the spike and saying one did would be a
/// guess dressed as a measurement.
constexpr float dominance_share = 0.5f;

/// Why the worst retained frame was slow, in the terms the ROADMAP asks for —
/// but only ever from a measure actually taken. Allocation churn is NOT one of
/// them: nothing in the build hooks the allocator, so there is no honest number
/// to attribute it from (the HUD says so in its own footnote rather than
/// inventing one here).
///
/// @param s The sample to attribute.
/// @return  A short cause phrase; "no single dominant phase" when none owns it.
const char* spike_cause(const frame_sample& s)
{
    if (s.total_ms <= 0.0f)
        return "no frames sampled";

    const float floor_ms = s.total_ms * dominance_share;
    if (s.present_ms >= floor_ms)
        return "GPU present / VSync wait";
    if (s.submit_ms >= floor_ms)
        return "draw-call submission";
    if (s.build_ms >= floor_ms)
        return "UI build (panels + canvases)";
    if (s.other_ms() >= floor_ms)
        return "event pump + simulation step";
    return "no single dominant phase";
}

/// One "<label> <value> ms" row, tinted against a budget when one applies.
///
/// @param label     Row label.
/// @param value_ms  The measured figure.
/// @param target_ms Budget the figure is scored against; <= 0 = no budget, drawn
///                  in the neutral text colour with no target suffix.
void budget_row(const char* label, float value_ms, float target_ms)
{
    ImGui::TextUnformatted(label);
    // Column derived from the widest label at the live font size, not a pixel
    // constant — the HUD has to stay aligned under the F10 UI-scale steps
    // (DEVELOPMENT_PRACTICES § Display environment).
    ImGui::SameLine(ImGui::CalcTextSize("1% low   ").x);

    const bool  scored = target_ms > 0.0f;
    const ImU32 colour = !scored               ? palette::neutral
                       : (value_ms <= target_ms) ? palette::positive
                                                 : palette::negative;
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(colour), "%6.2f ms", value_ms);

    if (scored)
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        ImGui::Text("(target < %.2f)", target_ms);
        ImGui::PopStyleColor();
    }
}

} // namespace

void frame_stats::begin_frame()
{
    const clock::time_point now = clock::now();

    // A frame's period is only knowable once the next one starts, so the sample
    // pushed here is the PREVIOUS frame: its staged sub-measures, stamped with the
    // begin-to-begin span. Measuring the period (rather than render()'s own
    // duration) is what makes the total include the event pump and the sim step.
    if (m_pending_valid)
    {
        m_pending.total_ms = elapsed_ms(m_frame_begin, now);
        m_samples[m_next]  = m_pending;
        m_next             = (m_next + 1) % capacity;
        m_count            = std::min(m_count + 1, capacity);
    }

    m_pending       = frame_sample{};
    m_pending_valid = true;
    m_frame_begin   = now;
    m_phase_begin   = now;
}

void frame_stats::mark_build_end()
{
    const clock::time_point now = clock::now();
    m_pending.build_ms = elapsed_ms(m_phase_begin, now);
    m_phase_begin      = now;
}

void frame_stats::mark_submit_end(const ImDrawData& draw_data)
{
    // Does NOT restart the phase clock: whatever runs between here and
    // mark_present_begin (an F12 / verify screenshot readback) belongs to neither
    // phase and falls into other_ms rather than being blamed on the GPU.
    m_pending.submit_ms = elapsed_ms(m_phase_begin, clock::now());

    m_pending.vertices  = draw_data.TotalVtxCount;
    m_pending.draw_cmds = 0;
    for (int i = 0; i < draw_data.CmdLists.Size; ++i)
        m_pending.draw_cmds += draw_data.CmdLists[i]->CmdBuffer.Size;
}

void frame_stats::mark_present_begin()
{
    m_phase_begin = clock::now();
}

void frame_stats::mark_present_end()
{
    m_pending.present_ms = elapsed_ms(m_phase_begin, clock::now());
}

const frame_sample& frame_stats::last() const
{
    if (m_count == 0)
        return m_samples[0]; // value-initialised: the zeroed empty-state sample
    return m_samples[(m_next + capacity - 1) % capacity];
}

const frame_sample& frame_stats::worst() const
{
    if (m_count == 0)
        return m_samples[0];

    std::size_t worst_index = 0;
    for (std::size_t i = 1; i < m_count; ++i)
        if (m_samples[i].total_ms > m_samples[worst_index].total_ms)
            worst_index = i;
    return m_samples[worst_index];
}

float frame_stats::average_ms() const
{
    if (m_count == 0)
        return 0.0f;

    float sum = 0.0f;
    for (std::size_t i = 0; i < m_count; ++i)
        sum += m_samples[i].total_ms;
    return sum / static_cast<float>(m_count);
}

float frame_stats::max_ms() const
{
    return worst().total_ms;
}

float frame_stats::low_1pct_ms() const
{
    if (m_count == 0)
        return 0.0f;

    std::array<float, capacity> totals{};
    for (std::size_t i = 0; i < m_count; ++i)
        totals[i] = m_samples[i].total_ms;

    // Slowest 1% of the window, at least one frame — so a short window still
    // reports a tail rather than silently reporting nothing.
    constexpr std::size_t percent = 100;
    const std::size_t     tail    = std::max<std::size_t>(1, m_count / percent);
    std::partial_sort(totals.begin(), totals.begin() + static_cast<std::ptrdiff_t>(tail),
                      totals.begin() + static_cast<std::ptrdiff_t>(m_count),
                      std::greater<float>());

    float sum = 0.0f;
    for (std::size_t i = 0; i < tail; ++i)
        sum += totals[i];
    return sum / static_cast<float>(tail);
}

void draw_frame_budget_hud(const frame_stats& stats, ImVec2 first_pos, bool& open)
{
    if (!open)
        return;

    ImGui::SetNextWindowPos(first_pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("Frame Budget###frame_budget_hud", &open,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoNav))
    {
        // --- the four scored figures ---
        budget_row("last",   stats.last().total_ms, 0.0f);
        budget_row("avg",    stats.average_ms(),    frame_stats::target_avg_ms);
        budget_row("max",    stats.max_ms(),        frame_stats::target_max_ms);
        budget_row("1% low", stats.low_1pct_ms(),   frame_stats::target_max_ms);

        // --- why the worst frame was the worst ---
        ImGui::SeparatorText("Worst frame");
        const frame_sample& w = stats.worst();
        // ASCII only in the drawn strings — the UI font's glyph range is still an
        // open item (BL-234), so a typographic dash here would risk a tofu box.
        ImGui::Text("%.2f ms - %s", w.total_ms, spike_cause(w));
        ImGui::Text("build %.2f  submit %.2f  present %.2f  other %.2f ms",
                    w.build_ms, w.submit_ms, w.present_ms, w.other_ms());
        ImGui::Text("%d verts / %d draw cmds", w.vertices, w.draw_cmds);

        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        // Honesty note, not a placeholder: the ROADMAP names allocation churn as a
        // candidate cause, but nothing here hooks the allocator, so there is no
        // measure to report. Say that rather than infer a number from frame time.
        ImGui::TextUnformatted("allocation churn: not instrumented");
        ImGui::Text("window %zu / %zu frames", stats.count(), frame_stats::capacity);
        ImGui::PopStyleColor();
    }
    ImGui::End();
}

} // namespace ui
