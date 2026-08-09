#include "corporation_dashboard.hpp"

#include "charts.hpp"
#include "detail_level.hpp"   // the fold idiom (BL-214)
#include "foldout_column.hpp" // shell fold-out column host
#include "format.hpp"
#include "presentation.hpp"
#include "selection_panel.hpp" // draw_building_profit — the Production drill's core
#include "view_nav.hpp"        // focus_on_entity — the [>] host axis

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace ui {

namespace {

/// A building's one-line identity: what it is, what it works on, and where.
/// Built once per frame per building, so it is a string rather than a formatter.
std::string building_label(const world& w, const building_component& b)
{
    std::string s = building_type_name(b.type);
    if (b.type == building_type::extraction_site)
    {
        s += " - ";
        s += presentation_of(b.target_resource).name;
    }
    const auto tit = w.tiles.find(b.tile);
    if (tit != w.tiles.end())
    {
        char buf[32];
        std::snprintf(buf, sizeof buf, " [%d, %d]", tit->second.grid_x, tit->second.grid_y);
        s += buf;
    }
    return s;
}

const char* body_name(const world& w, entity_id id)
{
    const auto it = w.bodies.find(id);
    return (it == w.bodies.end()) ? "?" : it->second.name.c_str();
}

/// One drillable row: a labelled horizontal magnitude bar. Clicking it drills.
/// draw_value_bar rather than draw_bars — the column is narrow and the list is
/// long, which is exactly the case that idiom exists for (charts.hpp).
bool rollup_row(int index, const rollup_item& it, float ceiling, ImU32 colour, const char* fmt)
{
    const float h = ImGui::GetFrameHeight();
    // Keyed on the ROW INDEX, not on it.subject: a Trade row's subject is the lane's
    // body_a, and two lanes out of the same body would then share an ImGui id — the
    // second row would silently stop responding to clicks. Nothing in a build or a
    // golden capture would show that.
    ImGui::PushID(index);
    const bool clicked = ImGui::InvisibleButton("##row", {ImGui::GetContentRegionAvail().x, h});
    const bool hot     = ImGui::IsItemHovered();
    ImGui::PopID();

    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    if (hot)
        dl->AddRectFilled(mn, mx, IM_COL32(255, 255, 255, 18), 3.0f);

    // Label left, bar right: the name is the identity and must not be squeezed by
    // the bar, so the bar takes a fixed right-hand share (the BL-081 pattern).
    const float bar_w = std::min(220.0f, (mx.x - mn.x) * 0.45f);
    dl->AddText({mn.x + 4.0f, mn.y + (h - ImGui::GetTextLineHeight()) * 0.5f}, // fit-exempt: row label in a measured value-bar row
                it.flagged ? palette::negative : palette::text_secondary, it.label.c_str());
    charts::draw_value_bar(dl, {mx.x - bar_w, mn.y + 3.0f}, {mx.x, mx.y - 3.0f},
                           it.value, ceiling, colour, fmt);

    if (hot)
        ImGui::SetTooltip("Click to drill in");
    return clicked;
}

/// The folded resting line of one card: chevron, title, verdict.
bool card_row(ui_state& s, int card, const char* title, const char* verdict, ImU32 col)
{
    ImGui::PushID(card);
    const bool changed = fold_chevron(s, detail_surface::corp_rollup, card);
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s", verdict);
    ImGui::PopID();
    return changed;
}

/// The breadcrumb every drill shares: `Corporation > Card > Subject`, the card
/// segment popping back to the roll-up. One level deep everywhere — a building has
/// no further subject that is not an invented layer.
void drill_breadcrumb(ui_state& s, const char* corp, const char* card, const char* subject)
{
    ImGui::TextDisabled("%s", corp);
    ImGui::SameLine();
    ImGui::TextDisabled(">");
    ImGui::SameLine();
    if (ImGui::SmallButton(card))
        s.corp_rollup_drill = -1;
    ImGui::SameLine();
    ImGui::TextDisabled(">");
    ImGui::SameLine();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", subject);
    ImGui::Separator();
}

} // namespace

corp_rollups derive_corp_rollups(const world& w, const economy_report& report, entity_id corp)
{
    corp_rollups r;

    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return r;
    r.balance = cit->second.balance;

    // --- Finance: the five flows, straight from the budget step. -------------
    const auto bit = report.budgets.find(corp);
    if (bit != report.budgets.end())
    {
        r.budget          = bit->second;
        r.budget_measured = true;
        r.sold            = r.budget.income;
        r.bought          = r.budget.expenditure;
    }

    // --- Production + Workforce: one pass over this corp's building reports. --
    for (const building_report& br : report.buildings)
    {
        if (br.corp != corp)
            continue;
        const auto bldit = w.buildings.find(br.building);
        if (bldit == w.buildings.end())
            continue;

        if (br.exhausted) ++r.exhausted;
        if (br.active)    ++r.producing;
        if (br.idle)      ++r.idle;
        r.output_per_tick += br.output_quantity;

        std::string label = building_label(w, bldit->second);
        r.production.push_back({br.building, null_entity, label,
                                br.output_quantity, br.idle});
        r.workforce.push_back({br.building, null_entity, label,
                               br.effective_workforce,
                               br.effective_workforce < bldit->second.workforce_assigned * 0.999f});
    }
    std::sort(r.production.begin(), r.production.end(),
              [](const rollup_item& a, const rollup_item& b) { return a.value > b.value; });
    // Worst-staffed first: this card's question is "is labour holding me back?",
    // and the answer is at the bottom of the list, not the top.
    std::sort(r.workforce.begin(), r.workforce.end(),
              [](const rollup_item& a, const rollup_item& b) { return a.value < b.value; });

    for (const auto& [key, scalar] : report.workforce_contention)
        if (key.first == corp)
            r.tightest_contention = std::min(r.tightest_contention, scalar);

    // --- Trade: the corp's own lanes, by cumulative traffic. ------------------
    for (const trade_route& tr : w.trade_routes)
    {
        if (tr.corp != corp)
            continue;
        ++r.routes;
        r.convoys += tr.convoy_count;
        std::string label = std::string(body_name(w, tr.body_a)) + " - " + body_name(w, tr.body_b);
        r.trade.push_back({tr.body_a, tr.body_b, label,
                           static_cast<float>(tr.convoy_count), false});
    }
    std::sort(r.trade.begin(), r.trade.end(),
              [](const rollup_item& a, const rollup_item& b) { return a.value > b.value; });

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

    const corp_rollups r = derive_corp_rollups(w, report, corp);

    // ── The folded column: four verdict lines and nothing else ──
    if (ui::foldout_begin("Corporation"))
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", name);
        ImGui::Separator();

        char buf[128];

        std::snprintf(buf, sizeof buf, "%d making, %d idle, %.1f/qtr",
                      r.producing, r.idle, static_cast<double>(r.output_per_tick));
        card_row(s, 0, "Production", buf,
                 r.idle > 0 ? palette::negative : palette::positive);

        std::snprintf(buf, sizeof buf, "%d lanes, %d convoys run", r.routes, r.convoys);
        card_row(s, 1, "Trade", buf,
                 r.routes > 0 ? palette::positive : palette::neutral);

        std::snprintf(buf, sizeof buf, "%.0f%% of labour demand met",
                      static_cast<double>(r.tightest_contention * 100.0f));
        card_row(s, 2, "Workforce", buf,
                 r.tightest_contention < 0.999f ? palette::negative : palette::positive);

        if (r.budget_measured)
            std::snprintf(buf, sizeof buf, "%+.1f/qtr, balance %.0f",
                          static_cast<double>(r.budget.net()), static_cast<double>(r.balance));
        else
            std::snprintf(buf, sizeof buf, "balance %.0f (no quarter measured yet)",
                          static_cast<double>(r.balance));
        card_row(s, 3, "Finance", buf,
                 (r.budget_measured && r.budget.net() < 0.0f) ? palette::negative
                                                              : palette::positive);
    }
    ui::foldout_end();

    // ── The expanded card, full screen ──
    // One overlay for all four: the card index is the fold key, so which card is
    // open is already state and does not need a second flag.
    static const char* k_titles[4] = {"Production", "Trade", "Workforce", "Finance"};
    for (int card = 0; card < 4; ++card)
    {
        if (!fold_overlay_begin(s, detail_surface::corp_rollup, card, k_titles[card]))
            continue;

        // Finance drills into its own flows rather than a per-item list, so it is
        // the one card with no rows — build_UI_example_3 reached the same shape.
        static const std::vector<rollup_item> k_no_items;
        const std::vector<rollup_item>& items =
            (card == 0) ? r.production : (card == 1) ? r.trade
                        : (card == 2) ? r.workforce : k_no_items;

        const int drill = s.corp_rollup_drill;
        const bool have_drill = drill >= 0 && drill < static_cast<int>(items.size());

        if (have_drill)
        {
            const rollup_item& it = items[static_cast<std::size_t>(drill)];
            drill_breadcrumb(s, name, k_titles[card], it.label.c_str());

            // Four drills, four shapes — the property build_UI_example_3 was built
            // to demonstrate and the one worth preserving.
            if (card == 0)
            {
                // Production: this building's operating economics. The shared
                // profitability builder rather than a second copy of the same sums.
                draw_building_profit(w, reg, report, it.subject);
            }
            else if (card == 1)
            {
                // Trade: the lane's traffic and how recently it ran.
                const trade_route* lane = nullptr;
                for (const trade_route& tr : w.trade_routes)
                    if (tr.corp == corp && tr.body_a == it.subject && tr.body_b == it.second)
                    { lane = &tr; break; }
                if (lane)
                {
                    ImGui::Text("%d convoys completed on this lane.", lane->convoy_count);
                    ImGui::TextDisabled("Last completion: day %d.", lane->last_tick);
                    ImGui::Spacing();
                    ImGui::TextWrapped(
                        "A lane exists because a convoy finished on it. It is the same "
                        "record the commercial-sphere fog reads, so what you can see of "
                        "a body and what you have shipped there are one fact.");
                }
            }
            else if (card == 2)
            {
                // Workforce: assigned against effective, and what the gap costs.
                const auto bldit = w.buildings.find(it.subject);
                if (bldit != w.buildings.end())
                {
                    const building_component& b = bldit->second;
                    ImGui::Text("Assigned: %.0f%%", static_cast<double>(b.workforce_assigned * 100.0f));
                    ImGui::Text("Effective: %.0f%%", static_cast<double>(it.value * 100.0f));
                    ImGui::Spacing();
                    if (it.flagged)
                        ImGui::TextWrapped(
                            "Labour on this body is contended: every building the "
                            "corporation runs there is throttled by the same scalar, so "
                            "this shortfall is shared, not local to this building.");
                    else
                        ImGui::TextWrapped("This building gets the labour it asks for.");
                }
            }
        }
        else
        {
            // The roll-up itself: the chart, then the drillable rows.
            if (card == 3)
            {
                if (r.budget_measured)
                {
                    charts::bar fb[5] = {
                        {r.budget.income,      palette::positive, "Income",      false},
                        {r.budget.expenditure, palette::negative, "Inputs",      false},
                        {r.budget.maintenance, IM_COL32(190, 150, 100, 255), "Maintenance", false},
                        {r.budget.wages,       IM_COL32(150, 160, 210, 255), "Wages",       false},
                        {r.budget.interest,    IM_COL32(200, 110, 160, 255), "Interest",    false},
                    };
                    float peak = 0.0f;
                    for (const charts::bar& b : fb) peak = std::max(peak, b.value);
                    const float ceiling = charts::tight_ceil(std::max(peak, 1.0f));

                    const ImVec2 p  = ImGui::GetCursorScreenPos();
                    const float  cw = ImGui::GetContentRegionAvail().x;
                    ImGui::Dummy({cw, 220.0f});
                    charts::draw_bars(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + 220.0f},
                                      fb, 5, ceiling, "%.1f");
                    ImGui::Spacing();
                    ImGui::Text("Net this quarter: %+.1f", static_cast<double>(r.budget.net()));
                }
                else
                {
                    ImGui::TextWrapped("No budget breakdown has been recorded yet - "
                                       "advance time by one economy quarter.");
                }
            }
            else
            {
                float ceiling = 0.0f;
                for (const rollup_item& it : items)
                    ceiling = std::max(ceiling, it.value);
                ceiling = charts::tight_ceil(std::max(ceiling, 1.0f));

                const ImU32 col = (card == 0) ? IM_COL32(150, 235, 160, 255)
                                : (card == 1) ? IM_COL32(120, 190, 255, 255)
                                              : IM_COL32(210, 190, 120, 255);
                const char* fmt = (card == 1) ? "%.0f" : (card == 2) ? "%.2f" : "%.1f";

                if (items.empty())
                {
                    ImGui::TextDisabled("Nothing to show here yet.");
                }
                else
                {
                    for (int i = 0; i < static_cast<int>(items.size()); ++i)
                        if (rollup_row(i, items[static_cast<std::size_t>(i)], ceiling, col, fmt))
                            s.corp_rollup_drill = i;
                }

            }
        }

        fold_overlay_end(s);
    }
}

} // namespace ui
