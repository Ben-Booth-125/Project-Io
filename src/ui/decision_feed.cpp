#include "decision_feed.hpp"

#include "foldout_column.hpp"

#include <imgui.h>

namespace ui {

// SEAM STUB (BL-407). The shell, the rail slot, the flags and the call site are
// wired; the feed's body is the item's own work. Kept compiling so the seam can
// land ahead of the content.
void draw_decision_feed(const world&, ui_state&, bool* open)
{
    if (!open || !*open)
        return;

    if (!foldout_begin("AI decisions"))
    {
        foldout_end();
        return;
    }

    ImGui::TextDisabled("Not yet built.");

    foldout_end();
}

} // namespace ui
