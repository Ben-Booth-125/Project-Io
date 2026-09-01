#include "company_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "format.hpp"         // fmt::credits
#include "presentation.hpp"   // palette

#include "world/standing.hpp" // corp_files_return — the one disclosure gate (FINANCE.md)

#include <imgui.h>

#include <algorithm>
#include <vector>

namespace ui {

namespace {

/// What a company's holdings amount to, counted off buildings alone.
struct holdings_tally
{
    int on_body = 0; ///< Holdings on the navigation anchor body.
    int bodies  = 0; ///< Distinct bodies carrying at least one holding.
};

/// Both figures come from buildings, which are already visible on canvas — the
/// same reasoning by which the corporations table prints its Reach axis for
/// every firm with no disclosure gate (corporation_panel.cpp, BL-633). Nothing
/// about HOW the firm operates is read here: no recipe, no rate, no stockpile.
holdings_tally tally_holdings(const world& w, const corporation_component& c, entity_id body)
{
    holdings_tally t;
    // Reach is a handful of bodies at prototype scale, so a linear scan over a
    // vector beats a set and keeps the walk order the asset list's own.
    std::vector<entity_id> seen;

    for (const entity_id bid : c.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit == w.tiles.end())
            continue;

        const entity_id on = tit->second.body;
        if (on == null_entity)
            continue;
        if (on == body)
            ++t.on_body;
        if (std::find(seen.begin(), seen.end(), on) == seen.end())
        {
            seen.push_back(on);
            ++t.bodies;
        }
    }
    return t;
}

/// The line that keeps this surface honest. A stand-in that looks finished is
/// worse than one that admits what it is: without this, the thin fact list reads
/// as a design decision rather than as the absence of one.
void draw_placeholder_notice()
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned), "Placeholder surface");
    ImGui::TextWrapped(
        "The Company ledger has no design yet - no top question, no sub-views, no lens on "
        "open, no close semantics beyond the standing toggle rule. It exists so a click "
        "under the Company lens lands somewhere, and is due to be designed in the ledger "
        "batch.");
}

} // namespace

void draw_company_ledger(const world& w, ui_state& s, bool& open)
{
    if (!open)
        return;

    if (!ui::foldout_begin("Company"))
    {
        ui::foldout_end();
        return;
    }

    const auto it = w.corporations.find(s.selected_company);
    if (s.selected_company == null_entity || it == w.corporations.end())
    {
        // An honest empty state rather than a vanishing window: this ledger is
        // opened by a click, and a click whose target has since gone should read
        // as "nothing here", not as a surface that broke.
        ImGui::TextDisabled("No company selected.");
        ImGui::TextDisabled("Click a holding under the Company lens to open one.");
        draw_placeholder_notice();
        ui::foldout_end();
        return;
    }

    const corporation_component& c = it->second;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", c.name.c_str());

    if (c.is_background)
    {
        ImGui::TextDisabled("Background company - not a corporation.");
    }
    else
    {
        // GLOSSARY.md, Ben 2026-08-28: the two words are not interchangeable.
        // Rendering a corporation on the company surface would quietly undo that
        // split, so a mismatched target is named rather than absorbed.
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                           "Not a background company.");
        ImGui::TextWrapped("A corporation - the player or a rival - belongs in the "
                           "Corporations table, not here.");
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Deliberately NOT draw_corporation_summary (entity_summary.hpp), which the
    // ledger family's shared-content-builder rule would otherwise point at: that
    // builder prints `starting_capital` for any firm with no disclosure gate, and
    // a placeholder is the wrong place to widen what a rival firm publishes. The
    // three lines below are the public subset only.
    const auto nit = w.nations.find(c.home_nation);
    if (nit != w.nations.end())
        ImGui::Text("Registered in %s", nit->second.name.c_str());
    // No "home nation unknown" fallback — the generator never produces an
    // unregistered firm, so a line saying so would be inventing a state.

    const holdings_tally t = tally_holdings(w, c, s.active_body);
    if (const auto bit = w.bodies.find(s.active_body); bit != w.bodies.end())
        ImGui::Text("Holdings on %s: %d", bit->second.name.c_str(), t.on_body);
    ImGui::Text("Present on %d %s", t.bodies, t.bodies == 1 ? "body" : "bodies");

    // The one gated figure, on exactly the gate the corporations table uses: a
    // firm publishes a readable return iff it files (FINANCE.md § Disclosure).
    // The dash means *this company does not file*, never *you have not earned
    // this*, so the reason sits on the hover.
    ImGui::Text("Capital:");
    ImGui::SameLine();
    if (corp_files_return(c.ownership_class))
    {
        const ImU32 col = (c.balance < 0.0f) ? palette::negative : palette::positive;
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s",
                           ui::fmt::credits(c.balance).c_str());
    }
    else
    {
        ImGui::TextDisabled("-");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Does not file - this company publishes no return.");
    }

    draw_placeholder_notice();

    ui::foldout_end();
}

} // namespace ui
