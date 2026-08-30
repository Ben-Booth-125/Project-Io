#include "convoys_ledger.hpp"

#include "foldout_column.hpp"
#include "market_ledger.hpp" // market_city_name — the endpoints' city identity
#include "presentation.hpp"
#include "text_fit.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>  // std::ceil — ticks to arrival
#include <cstdio> // std::snprintf — the progress-bar overlay
#include <vector>

namespace ui {

namespace {

const char* convoy_mode_name(convoy_mode m)
{
    switch (m)
    {
        case convoy_mode::land:  return "Land";
        case convoy_mode::sea:   return "Sea";
        case convoy_mode::air:   return "Air"; // never dispatched in the prototype
        case convoy_mode::space: return "Space";
    }
    return "-";
}

/// Econ ticks until this convoy lands, rounded up — the figure the whole ledger
/// exists for. A held convoy has no arrival time at all: it is not slowed, it is
/// stopped, so reporting a number would be a lie (hence the caller's "held").
int ticks_to_arrival(const convoy_component& c)
{
    if (c.arrived || c.progress >= 1.0f)
        return 0;
    if (!(c.speed > 0.0f))
        return -1; // unknown: a zero speed never arrives
    const float remaining = 1.0f - c.progress;
    return static_cast<int>(std::ceil(remaining / c.speed));
}

/// Format a cargo quantity so that a convoy carrying SOMETHING never reads as
/// carrying nothing.
///
/// THE `x0` DEFECT, MEASURED (BL-689; tools/verify/convoy_cargo_census.cpp).
/// `convoys.md` recorded a row reading `Agricultural Produce x0` — a convoy in
/// flight, haul cost paid, progress bar running, hold apparently empty — and
/// asked whether dispatch could legitimately commit an empty convoy. It cannot,
/// and the census proves it over the real generated world: 1669 dispatches, ZERO
/// with `cargo_qty == 0`. Both dispatch paths forbid it by construction — the
/// auto path takes `min(surplus, shortfall)` with both strictly positive, and the
/// directed verb rejects the command outright on `!(quantity > 0)`.
///
/// What was actually wrong was here. The row printed a float with `"x%.0f"`, and
/// 4.6% of real convoys carry less than 0.5 of a unit (the census measured a
/// minimum of 0.0097 against a median of 5.62). `%.0f` rounds every one of them
/// to "0", so a 0.3-unit convoy and an empty one drew the same six pixels. The
/// display was not "reporting what it was given" — it was rounding it away.
///
/// The tiers are chosen against that measured distribution rather than guessed:
/// whole units where the magnitudes are large enough for a decimal to be noise,
/// more precision as the quantity shrinks, and an explicit "<0.01" floor so even
/// a vanishingly small cargo states that it is small rather than absent.
void cargo_qty_text(char* out, std::size_t n, float qty)
{
    if (!(qty > 0.0f))          std::snprintf(out, n, "x0");      // unreachable; honest if reached
    else if (qty >= 10.0f)      std::snprintf(out, n, "x%.0f", static_cast<double>(qty));
    else if (qty >= 1.0f)       std::snprintf(out, n, "x%.1f", static_cast<double>(qty));
    else if (qty >= 0.01f)      std::snprintf(out, n, "x%.2f", static_cast<double>(qty));
    else                        std::snprintf(out, n, "x<0.01");
}

} // namespace

void draw_convoys_ledger(const world& w, ui_state& s, bool& open)
{
    if (!open)
        return;

    // The window is its own scroller — no nested BeginChild — so
    // `verify.scroll_panel("convoys", …)` reaches it through foldout_begin's own
    // hook. That is deliberate after NR-719: a child scroller here would need
    // `foldout_scroll_child` and would be one more surface where a misaimed
    // request looked green.
    if (!ui::foldout_begin("Convoys"))
    {
        ui::foldout_end();
        return;
    }

    const entity_id corp = w.player_entity;

    // Sorted by soonest arrival, then by id — "what lands next" is the reading
    // order the question implies, and the id tiebreak keeps it stable frame to
    // frame (world::convoys is dispatch-ordered, which is neither).
    std::vector<const convoy_component*> rows;
    for (const convoy_component& c : w.convoys)
        if (c.corp == corp && !c.arrived)
            rows.push_back(&c);
    std::sort(rows.begin(), rows.end(),
              [](const convoy_component* a, const convoy_component* b) {
                  const int ta = ticks_to_arrival(*a);
                  const int tb = ticks_to_arrival(*b);
                  if (ta != tb) return ta < tb;
                  return a->id < b->id;
              });

    if (rows.empty())
    {
        ImGui::TextDisabled("Nothing in flight.");
        ui::foldout_end();
        return;
    }

    ImGui::TextDisabled("%d in flight", static_cast<int>(rows.size()));
    ImGui::Separator();
    ImGui::Spacing();

    const float avail = ImGui::GetContentRegionAvail().x;

    for (const convoy_component* cp : rows)
    {
        const convoy_component& c = *cp;
        // PushID on the CONVOY ID, not the loop index: the id is what the hold
        // command names, and it is stable across the erase an arrival causes.
        ImGui::PushID(static_cast<int>(c.id));

        const resource_presentation& rp = presentation_of(c.cargo_resource);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(rp.colour), "%s", rp.name);
        ImGui::SameLine();
        char qty[24];
        cargo_qty_text(qty, sizeof qty, c.cargo_qty);
        ImGui::TextUnformatted(qty);

        // THE ROUTE GETS ITS OWN LINE, AND WRAPS (BL-689; `convoys.md`'s second
        // inherited defect). It used to be `SameLine`d after the cargo and drawn
        // with a bare TextDisabled, so `Huhaidar -> Kua Sua (Land)` began at
        // whatever x the cargo text happened to end at and ran off the column
        // edge — NR-709's family, four surfaces and counting. The fold-out column
        // is LAYOUT.md container 1, whose policy is WRAP, so wrap_text is the
        // container's own rule applied rather than an elision invented here; a
        // destination name is exactly the string a player must be able to read
        // in full.
        char route[192];
        std::snprintf(route, sizeof route, "%s -> %s  (%s)",
                      market_city_name(w, c.source_market).c_str(),
                      market_city_name(w, c.dest_market).c_str(),
                      convoy_mode_name(c.mode));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ui::wrap_text(ui::text_box::foldout_column, "convoys.route", route, avail);
        ImGui::PopStyleColor();

        const int eta = ticks_to_arrival(c);
        char overlay[48];
        if (c.held)
            std::snprintf(overlay, sizeof overlay, "held");
        else if (eta < 0)
            std::snprintf(overlay, sizeof overlay, "%.0f%%  (stalled)",
                          static_cast<double>(c.progress) * 100.0);
        else
            std::snprintf(overlay, sizeof overlay, "%.0f%%  %d qtr%s",
                          static_cast<double>(c.progress) * 100.0, eta, eta == 1 ? "" : "s");
        ImGui::ProgressBar(c.progress, ImVec2{-1.0f, 0.0f}, overlay);

        ImGui::TextDisabled("Haul cost paid: %.2f", static_cast<double>(c.cost_paid));
        ImGui::SameLine();
        // The toggle rule: the press reads as the state it would move to, and
        // issuing it again undoes it — one verb, `hold_convoy`, both ways.
        if (ImGui::SmallButton(c.held ? "Release" : "Hold"))
        {
            corp_command cmd;
            cmd.corp  = corp;
            cmd.verb  = corp_verb::hold_convoy;
            cmd.order = c.id;
            s.pending_order_commands.push_back(cmd);
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    ui::foldout_end();
}

} // namespace ui
