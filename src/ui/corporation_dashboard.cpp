#include "corporation_dashboard.hpp"

#include "charts.hpp"
#include "detail_level.hpp"   // the fold idiom (BL-214)
#include "foldout_column.hpp" // shell fold-out column host
#include "diplomacy_ledger.hpp" // refresh_nation_gdp: the one public GDP read
#include "format.hpp"
#include "player_side.hpp"
#include "presentation.hpp"
#include "world/survey_system.hpp" // survey_tile_visible: a rival's force past the survey is unseen
#include "world/standing.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace ui {

namespace {

/// The card's title. One card since BL-691 — see the header for why the other
/// three went, and where their questions are answered instead.
constexpr const char* k_balance_title = "Balance";

/// The Balance card's body: the two bars, the quarter's net, and the force line. ONE builder with TWO hosts — the resting ledger column and the
/// full-canvas takeover — which is the shared-body pattern BL-265 established
/// and the reason the two hosts cannot drift apart.
///
void balance_body(const corp_rollups& r)
{
    if (!r.budget_measured)
    {
        ImGui::TextWrapped("No budget breakdown has been recorded yet - "
                           "advance time by one economy quarter.");
        return;
    }

    const balance_columns c = build_balance_columns(r.budget, r.refunds, r.tax_income);

    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  cw = ImGui::GetContentRegionAvail().x;
    // Two bars at text height and a half: Standing takes the column's room beneath.
    const float chart_h = ImGui::GetTextLineHeight() * 3.0f + 8.0f;
    charts::draw_stacked_bars(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + chart_h},
                              c.earnings, c.earning_count, c.expenses, c.expense_count,
                              "Earnings", "Expenses");
    // The drawer leaves the cursor wherever its last hover target sat, so the
    // row is reserved explicitly rather than inferred.
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({cw, chart_h});

    ImGui::Text("Net this quarter: %+.1f", static_cast<double>(r.net()));

    // BL-1215: the refund is named on its own line rather than folded into the
    // net above. It is cash credited back (the seat's cancelled construction,
    // BL-1206), not earnings: corp_command and spawn_seat both subtract it from
    // trailing earnings, so the operating net stays the figure a player tunes.
    // Kept short so it fits the ledger column; the why is on hover.
    if (r.refunds > 0.0f)
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::positive),
                           "Refunds: %+.1f (not earnings)", static_cast<double>(r.refunds));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Construction cancelled when this seat was taken,\n"
                              "credited back at what it had paid. Not counted as earnings.");
    }

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

/// The unit entities @p nation fields that the player can see. Its own, all of them; a
/// rival's, those on surveyed ground, which is exactly what the map's counters show of it
/// (DISCOVERY.md; a counter's count is public, a unit's internals are not, BL-068).
int visible_force(const world& w, const ui_state& s, entity_id nation, bool mine)
{
    int n = 0;
    for (const auto& [uid, u] : w.units)
    {
        if (u.owner != nation || u.count <= 0)
            continue;
        if (!mine && !(s.spectating && s.god_view))
        {
            const auto tit = w.tiles.find(u.position);
            if (tit == w.tiles.end())
                continue;
            const auto bit = w.bodies.find(tit->second.body);
            if (bit == w.bodies.end()
                || !survey_tile_visible(bit->second.survey, bit->second.grid_width, bit->second.grid_height,
                                        tit->second.grid_x, tit->second.grid_y))
                continue;
        }
        ++n;
    }
    return n;
}

/// STANDING (BL-1509; ledgers/corporation.md § Slot 1): the nation's rank among nations on
/// GDP, market share, reach and total force, each a bar against the leader's figure. All
/// four are public; a rival's treasury is not, and is not here (FINANCE.md § Disclosure).
void standing_body(const world& w, const recipe_registry& reg, const economy_report& report,
                   const std::vector<corp_standing>& standings, ui_state& s, entity_id me)
{
    refresh_nation_gdp(w, reg, report, s);
    auto& rows = s.diplomacy.standing;
    rows.clear();

    std::vector<entity_id> nations;
    for (const auto& [id, n] : w.nations)
        nations.push_back(id);
    std::sort(nations.begin(), nations.end());

    const auto standing_of = [&](entity_id n) -> const corp_standing* {
        for (const corp_standing& c : standings)
            if (c.corp == n) return &c;
        return nullptr;
    };
    const auto gdp = [&](entity_id n) {
        const auto it = s.diplomacy.gdp_year.find(n);
        return it == s.diplomacy.gdp_year.end() ? 0.0 : it->second;
    };
    const auto share = [&](entity_id n) {
        const corp_standing* c = standing_of(n);
        return c != nullptr ? static_cast<double>(c->market_share) * 100.0 : 0.0;
    };
    const auto reach = [&](entity_id n) {
        const corp_standing* c = standing_of(n);
        return c != nullptr ? static_cast<double>(c->reach_bodies) : 0.0;
    };
    const auto force = [&](entity_id n) {
        return static_cast<double>(visible_force(w, s, n, n == me));
    };

    const auto row = [&](const char* label, const char* fmt_v, const char* tip, auto&& value_of) {
        double mine = 0.0, leader = 0.0;
        entity_id leader_id = null_entity;
        int rank = 1, of = 0;
        std::vector<double> vals;
        for (const entity_id n : nations)
        {
            const double v = value_of(n);
            ++of;
            if (n == me) mine = v;
            if (leader_id == null_entity || v > leader) { leader = v; leader_id = n; }
            vals.push_back(v);
        }
        for (const double v : vals)
            if (v > mine) ++rank;
        rows.push_back({label, mine, leader, rank, of});

        ImGui::TextUnformatted(label);
        char pos[32];
        std::snprintf(pos, sizeof pos, "#%d of %d", rank, of);
        const float pw = ImGui::CalcTextSize(pos).x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - pw));
        ImGui::TextDisabled("%s", pos);

        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  cw = ImGui::GetContentRegionAvail().x;
        const float  h  = ImGui::GetTextLineHeight();
        charts::draw_value_bar(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + h},
                               static_cast<float>(mine), static_cast<float>(leader > 0.0 ? leader : 1.0),
                               rank == 1 ? palette::positive : palette::selection, fmt_v);
        ImGui::Dummy({cw, h});
        if (ImGui::IsItemHovered())
        {
            const char* leader_name = owner_display_name(w, leader_id);
            ImGui::SetTooltip("%s\nThe bar is yours against the leader, %s: %.1f.", tip,
                              leader_name != nullptr ? leader_name : "-", leader);
        }
    };

    ImGui::Spacing();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Standing");
    ImGui::Separator();
    row("GDP", "%.0f", "What your buildings' goods sell for in a year.", gdp);
    row("Market share", "%.1f%%", "Your share of what the markets cleared last quarter.", share);
    row("Reach", "%.0f", "Bodies with at least one of the nation's buildings.", reach);
    row("Total force", "%.0f", "Units in the field. A rival's are the ones on ground you have surveyed.", force);
}

} // namespace

std::size_t corp_card_count()
{
    return 1;
}

balance_columns build_balance_columns(const corp_budget& b, float refunds, float tax_income)
{
    balance_columns c;

    // Earnings. Subsidies are the one inflow that is not market income
    // (BL-537), so they are their own segment when a nation paid any, and
    // absent otherwise — which leaves the ordinary case a plain single bar.
    c.earnings[c.earning_count++] = {b.income, palette::positive, "Income", nullptr};
    if (b.subsidies > 0.0f)
        c.earnings[c.earning_count++] =
            {b.subsidies, IM_COL32(120, 190, 255, 255), "Subsidies", nullptr};
    // BL-1215: a refund the quarter's return booked (BL-1206) is cash in, so it
    // stacks on the earnings side — named, never merged into Income.
    if (refunds > 0.0f)
        c.earnings[c.earning_count++] =
            {refunds, IM_COL32(150, 215, 170, 255), "Refunds",
             "Cancelled construction credited back - not earnings"};
    // BL-1406: a NATION's tax is an inflow — the tier's share of its buildings'
    // wages, back to the treasury (NATIONS.md § 2; corporation.md § Slot 1).
    if (tax_income > 0.0f)
        c.earnings[c.earning_count++] =
            {tax_income, IM_COL32(185, 150, 120, 255), "Taxes",
             "Your tax tier's share of your buildings' wages, back to the treasury"};

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
    const charts::stack_segment all[7] = {
        {b.expenditure, IM_COL32(205, 120, 95, 255),  "Inputs",      nullptr},
        {b.maintenance, IM_COL32(200, 170, 95, 255),  "Maintenance", nullptr},
        {b.wages,       IM_COL32(160, 115, 200, 255), "Wages",       nullptr},
        {b.interest,    IM_COL32(200, 110, 160, 255), "Interest",    nullptr},
        {b.levies,      IM_COL32(210, 175, 90, 255),  "Levies",      nullptr},
        {b.upkeep,      IM_COL32(200, 130, 110, 255), "Force",       nullptr},
        // BL-1335: the home nation's tax on the quarter's operating profit.
        {b.taxes,       IM_COL32(185, 150, 120, 255), "Taxes",
         "Your home nation's tax tier: its share of this quarter's operating profit"},
    };
    for (const charts::stack_segment& s : all)
        if (s.value > 0.0f)
            c.expenses[c.expense_count++] = s;

    return c;
}

namespace {

/// The nation branch of `derive_corp_rollups`, shared with
/// `nation_operating_net` so the header's NET and the card's net are one formula.
bool fill_nation_rollups(const world& w, entity_id nation, corp_rollups& r)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return false;
    r.nation  = true;
    r.balance = nit->second.treasury;
    if (const auto lit = w.state_ledger.find(nation); lit != w.state_ledger.end())
    {
        const world::state_tick_ledger& led = lit->second;
        r.budget.income      = led.income;
        r.budget.expenditure = led.expenditure;
        r.budget.maintenance = led.maintenance;
        r.budget.wages       = led.wages;
        r.tax_income         = led.taxes;
        r.budget_measured    = true;
    }
    // The force line (BL-1509): the units themselves, since the nation's row holds no
    // force flow of its own. A unit below full supply is the "weakening" the line names.
    for (const auto& [uid, u] : w.units)
        if (u.owner == nation && u.count > 0)
        {
            ++r.budget.force_units;
            if (u.supply_factor_permille < 1000)
                ++r.budget.force_unsupplied;
        }
    return true;
}

} // namespace

bool nation_operating_net(const world& w, entity_id nation, float& net)
{
    corp_rollups r;
    if (!fill_nation_rollups(w, nation, r) || !r.budget_measured)
        return false;
    net = r.net();
    return true;
}

corp_rollups derive_corp_rollups(const world& w, const recipe_registry& reg,
                                 const economy_report& report, entity_id corp)
{
    (void)reg; // the per-building walks left with the Production card

    corp_rollups r;

    // BL-1406 (corporation.md § Slot 1): the NATION's overview, once the world is
    // folded. Its money is the treasury, and its quarter is its own row of the
    // state ledger (`world::state_ledger`): sales, the inputs and upkeep goods it
    // bought, maintenance, wages, and the tax that returns to the treasury.
    if (fill_nation_rollups(w, corp, r))
        return r;

    // BL-1407: no corporation exists; an actor that is not a nation has no books.
    (void)report;
    return r;
}

void draw_corporation_dashboard(const world& w, const recipe_registry& reg,
                                const economy_report& report,
                                const std::vector<corp_standing>& standings,
                                ui_state& s, bool& open)
{
    if (!open)
        return;

    // BL-1406: the player's NATION (its actor once folded) — the Nation overview.
    const entity_id corp = player_actor(w);
    const char*     name = "Nation";
    if (const auto nit = w.nations.find(corp); nit != w.nations.end())
        name = nit->second.name.c_str();

    const corp_rollups r = derive_corp_rollups(w, reg, report, corp);

    // The card's verdict: the quarter's net and the treasury it moved.
    const char* purse = r.nation ? "treasury" : "balance";
    char verdict[128];
    if (r.budget_measured)
        std::snprintf(verdict, sizeof verdict, "%+.1f/qtr, %s %.0f",
                      static_cast<double>(r.net()), purse, static_cast<double>(r.balance));
    else
        std::snprintf(verdict, sizeof verdict, "%s %.0f (no quarter measured yet)",
                      purse, static_cast<double>(r.balance));
    const ImU32 verdict_col =
        (r.budget_measured && r.net() < 0.0f) ? palette::negative : palette::positive;

    // ── The resting column: the card, and its chart under it ──
    if (ui::foldout_begin("Nation overview"))
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
        balance_body(r);
        standing_body(w, reg, report, standings, s, corp);
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
        balance_body(r);
        standing_body(w, reg, report, standings, s, corp);
        fold_overlay_end(s);
    }
}

} // namespace ui
