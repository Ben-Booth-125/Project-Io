#include "ui/generation_wait.hpp"

#include "world/hard_coded_world.hpp" // generation_progress, generation_stage_labels

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace ui {

namespace {

/// One centred line of dim text.
void centred_text(const char* s, float pane_w, ImU32 colour)
{
    const float tw = ImGui::CalcTextSize(s).x;
    if (pane_w > tw)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (pane_w - tw) * 0.5f);
    ImGui::PushStyleColor(ImGuiCol_Text, colour);
    ImGui::TextUnformatted(s);
    ImGui::PopStyleColor();
}

} // namespace

void draw_generation_wait(generation_progress& prog, float pane_w, const char* caption,
                          bool live_clock)
{
    const float bar_w = std::min(420.0f, std::max(1.0f, pane_w));
    const float bar_x = std::max(0.0f, (pane_w - bar_w) * 0.5f);

    // The outer bar: weighted by measured step cost, and held at its high-water
    // mark so a relaxed read that lands between two of the worker's stores can
    // never draw the bar a step backwards.
    const float f = std::max(prog.fraction(), prog.wait_shown);
    prog.wait_shown = f;
    ImGui::Dummy({pane_w, 10.0f});
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + bar_x);
    ImGui::ProgressBar(f, {bar_w, 18.0f}, "");

    // The inner bar: progress WITHIN the step under way -- the span's years,
    // the village walk, the corridors, the validation run's quarters. Drawn
    // only while a step reports it; the sim's own counter, not an animation.
    const int sub_total = prog.sub_total.load(std::memory_order_relaxed);
    if (sub_total > 0)
    {
        const int sub_done = prog.sub_progress.load(std::memory_order_relaxed);
        ImGui::Dummy({pane_w, 4.0f});
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + bar_x);
        ImGui::ProgressBar(std::clamp(static_cast<float>(sub_done) / static_cast<float>(sub_total),
                                      0.0f, 1.0f),
                           {bar_w, 10.0f}, "");
    }
    else
    {
        // Hold the layout still: the caption must not jump up and down by a bar's
        // height every time a step without an inner count begins.
        ImGui::Dummy({pane_w, 4.0f + 10.0f});
    }

    // The caption: the step under way, in generation's own words.
    int li = prog.label.load(std::memory_order_relaxed);
    if (li < 0 || li >= generation_stage_label_count) li = 0;
    ImGui::Dummy({pane_w, 4.0f});
    centred_text(caption != nullptr ? caption : generation_stage_labels[li], pane_w,
                 IM_COL32(150, 158, 172, 255));

    // The elapsed count: proof the run is alive. Whole seconds -- a tenth would
    // flicker, and the question it answers is "is this still going".
    long long secs = 0;
    if (live_clock && prog.wait_began != std::chrono::steady_clock::time_point{})
        secs = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::steady_clock::now() - prog.wait_began).count();
    char buf[32];
    std::snprintf(buf, sizeof buf, "%lld s", secs);
    centred_text(buf, pane_w, IM_COL32(120, 128, 142, 255));
}

} // namespace ui
