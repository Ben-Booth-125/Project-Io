#include "corporation_dashboard.hpp"

#include "charts.hpp"
#include "detail_level.hpp"   // the fold idiom (BL-214)
#include "foldout_column.hpp" // shell fold-out column host
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace ui {

namespace {

/// The card's title. One card since BL-691 — see the header for why the other
/// three went, and where their questions are answered instead.
constexpr const char* k_balance_title = "Balance";

/// The Balance card's body: the two-column chart, the quarter's net, and the
/// force line. ONE builder with TWO hosts — the resting ledger column and the
/// full-canvas takeover — which is the shared-body pattern BL-265 established
/// and the reason the two hosts cannot drift apart.
///
/// @param chart_h Height budgeted by the host from its OWN remaining space. The
///        column is ~380 px at 1280 and 384 px at 1920 — the difference between
///        the two resolutions is all vertical — so a literal here would fit one
///        screen and waste or overflow the other.
void balance_body(const corp_rollups& r, float chart_h)
{
    if (!r.budget_measured)
    {
        ImGui::TextWrapped("No budget breakdown has been recorded yet - "
                           "advance time by one economy quarter.");
        return;
    }

    const balance_columns c = build_balance_columns(r.budget);

    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  cw = ImGui::GetContentRegionAvail().x;
    // bar_cap 0 auto-fits the columns to the host. The building card's 44 px cap
    // comes from the mockup its chart was drawn from; here the chart IS the
    // surface, so two hairlines in a 380 px column would be the wrong reading of
    // the same shape.
    charts::draw_stacked_columns(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + chart_h},
                                 c.earnings, c.earning_count,
                                 c.expenses, c.expense_count,
                                 /*bar_cap=*/0.0f, "Earnings", "Expenses",
                                 /*tight_axis=*/true);
    // The drawer leaves the cursor wherever its last hover target sat, so the
    // row is reserved explicitly rather than inferred.
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({cw, chart_h});

    ImGui::Text("Net this quarter: %+.1f", static_cast<double>(r.budget.net()));

    // BL-454's force line. The UNMET part is called out explicitly: an army
    // quietly weakening because its goods never arrived is exactly the thing the
    // player must not have to infer from a falling strength number somewhere
    // else. Silent when the corp fields no units, which is the common case.
    if (r.budget.force_units > 0)
    {
        if (r.budget.force_unsupplied > 0)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                               "Force: %d units, %d UNSUPPLIED and weakening",
                               r.budget.force_units, r.budget.force_unsupplied);
        else
            ImGui::Text("Force: %d units, all supplied", r.budget.force_units);
    }
}

/// Height to give the chart inside the host's remaining space: everything left,
/// less the lines that follow it. The void under this card is the whole reason
/// the surface was cut to one, so the chart takes the space rather than leaving
/// it — but it never takes the space the net and force lines need.
float chart_budget(const corp_rollups& r)
{
    const float line = ImGui::GetTextLineHeightWithSpacing();
    const int   tail = 1 + ((r.budget_measured && r.budget.force_units > 0) ? 1 : 0);
    return std::max(140.0f, ImGui::GetContentRegionAvail().y - line * (tail + 1));
}

} // namespace

std::size_t corp_card_count()
{
    return 1;
}

balance_columns build_balance_columns(const corp_budget& b)
{
    balance_columns c;

    // Earnings. Subsidies are the one inflow that is not market income
    // (BL-537), so they are their own segment when a nation paid any, and
    // absent otherwise — which leaves the ordinary case a plain single bar.
    c.earnings[c.earning_count++] = {b.income, palette::positive, "Income", nullptr};
    if (b.subsidies > 0.0f)
        c.earnings[c.earning_count++] =
            {b.subsidies, IM_COL32(120, 190, 255, 255), "Subsidies", nullptr};

    // Expenses — every outflow `corp_budget::net()` subtracts, each its OWN
    // segment. Levies and force upkeep are not folded into maintenance or wages
    // for the same reason they were given their own bars when they landed: the
    // point of the enforcement seam is that the player sees the tax as a number
    // rather than as an unexplained worse price (BL-343), and a force cost
    // buried inside Wages is a term nobody can tune against (BL-454).
    //
    // A zero flow is DROPPED here rather than charted flat, which is what makes
    // `expense_count` mean "segments drawn". Interest is the live case: charged
    // only while the balance is negative, so a solvent corp's stack is one
    // segment shorter than an indebted corp's.
    const charts::stack_segment all[6] = {
        {b.expenditure, IM_COL32(205, 120, 95, 255),  "Inputs",      nullptr},
        {b.maintenance, IM_COL32(200, 170, 95, 255),  "Maintenance", nullptr},
        {b.wages,       IM_COL32(160, 115, 200, 255), "Wages",       nullptr},
        {b.interest,    IM_COL32(200, 110, 160, 255), "Interest",    nullptr},
        {b.levies,      IM_COL32(210, 175, 90, 255),  "Levies",      nullptr},
        {b.upkeep,      IM_COL32(200, 130, 110, 255), "Force",       nullptr},
    };
    for (const charts::stack_segment& s : all)
        if (s.value > 0.0f)
            c.expenses[c.expense_count++] = s;

    return c;
}

corp_rollups derive_corp_rollups(const world& w, const recipe_registry& reg,
                                 const economy_report& report, entity_id corp)
{
    (void)reg; // the per-building walks left with the Production card

    corp_rollups r;

    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return r;
    r.balance = cit->second.balance;

    const auto bit = report.budgets.find(corp);
    if (bit != report.budgets.end())
    {
        r.budget          = bit->second;
        r.budget_measured = true;
    }

    return r;
}

void draw_corporation_dashboard(const world& w, const recipe_registry& reg,
                                const economy_report& report,
                                ui_state& s, bool& open)
{
    if (!open)
        return;

    const entity_id corp = w.player_entity;
    const auto      cit  = w.corporations.find(corp);
    const char*     name = (cit == w.corporations.end()) ? "Corporation" : cit->second.name.c_str();

    const corp_rollups r = derive_corp_rollups(w, reg, report, corp);

    // The card's verdict: the quarter's net and the balance it moved.
    char verdict[128];
    if (r.budget_measured)
        std::snprintf(verdict, sizeof verdict, "%+.1f/qtr, balance %.0f",
                      static_cast<double>(r.budget.net()), static_cast<double>(r.balance));
    else
        std::snprintf(verdict, sizeof verdict, "balance %.0f (no quarter measured yet)",
                      static_cast<double>(r.balance));
    const ImU32 verdict_col =
        (r.budget_measured && r.budget.net() < 0.0f) ? palette::negative : palette::positive;

    // ── The resting column: the card, and its chart under it ──
    if (ui::foldout_begin("Corporation"))
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", name);
        ImGui::Separator();

        ImGui::PushID(0);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s",
                           k_balance_title);
        ImGui::SameLine();
        gutter_text(verdict_col, verdict);
        // in_place = false: the card already shows its content at rest, so `⌄`
        // has nothing to disclose and only `›` is drawn — in the same rightmost
        // column as every other row in the app.
        disclosure_controls(s, detail_surface::corp_rollup, 0, /*in_place=*/false);
        balance_body(r, chart_budget(r));
        ImGui::PopID();
    }
    ui::foldout_end();

    // ── The full-canvas takeover: the same card, given the canvas ──
    // `expanded.key` no longer chooses WHICH card — there is one — so any key
    // opens it. That keeps every existing press and script working across the
    // cut rather than making three of four of them a dead no-op.
    if (s.expanded.surface != detail_surface::corp_rollup)
        return;

    if (fold_overlay_begin(s, detail_surface::corp_rollup, s.expanded.key, name))
    {
        // No verdict line here: the takeover coexists with the ledger column by
        // design (the row that opened it stays visible), so repeating the
        // verdict would print the same figure twice on one screen — and the net
        // line under the chart is already the third.
        ImGui::SeparatorText(k_balance_title);
        balance_body(r, chart_budget(r));
        fold_overlay_end(s);
    }
}

} // namespace ui
