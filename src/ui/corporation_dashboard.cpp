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

/// One row: a labelled horizontal magnitude bar. Clicking it drills, when the host
/// can hold a drill. draw_value_bar rather than draw_bars — the column is narrow and
/// the list is long, which is exactly the case that idiom exists for (charts.hpp).
///
/// @param drillable false in the ledger column's in-place expansion (BL-265). The
///        drill state is a single `corp_rollup_drill` index with no card scope, and
///        several cards can now be expanded in place at once — so an in-place drill
///        would be ambiguous about which card it belonged to. The takeover scopes it
///        unambiguously (`expanded.key` IS the card), and it is where a drill has the
///        room anyway. In place, this row is a graph, not a door.
bool rollup_row(int index, const rollup_item& it, float ceiling, ImU32 colour, const char* fmt,
                bool drillable)
{
    const float h = ImGui::GetFrameHeight();
    // Keyed on the ROW INDEX, not on it.subject: a Trade row's subject is the lane's
    // body_a, and two lanes out of the same body would then share an ImGui id — the
    // second row would silently stop responding to clicks. Nothing in a build or a
    // golden capture would show that.
    ImGui::PushID(index);
    bool clicked = false;
    bool hot     = false;
    if (drillable)
    {
        clicked = ImGui::InvisibleButton("##row", {ImGui::GetContentRegionAvail().x, h});
        hot     = ImGui::IsItemHovered();
    }
    else
    {
        ImGui::Dummy({ImGui::GetContentRegionAvail().x, h});
    }
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

/// The folded resting line of one card: title, verdict, then the `⌄ ›` gutter.
/// The chevron used to LEAD this row; BL-265 moved both controls to the right
/// gutter, so a card row's controls sit in the same column as every other row's.
bool card_row(ui_state& s, int card, const char* title, const char* verdict, ImU32 col)
{
    ImGui::PushID(card);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
    ImGui::SameLine();
    gutter_text(col, verdict);
    const bool changed = disclosure_controls(s, detail_surface::corp_rollup, card);
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

/// The four cards' titles. One table, because BL-265 made the takeover show all
/// four rather than only the one that was clicked.
const char* const k_card_titles[4] = {"Production", "Trade", "Workforce", "Finance"};

/// The rows behind a card. Finance drills into its own flows rather than a
/// per-item list, so it is the one card with none — build_UI_example_3 reached the
/// same shape.
const std::vector<rollup_item>& card_items(const corp_rollups& r, int card)
{
    static const std::vector<rollup_item> k_no_items;
    return (card == 0) ? r.production : (card == 1) ? r.trade
         : (card == 2) ? r.workforce  : k_no_items;
}

/// One card's roll-up body — the Finance chart, or the other three cards' rows.
///
/// Extracted by BL-265 because the same content now has TWO hosts: the ledger
/// column's in-place expansion and the full-canvas takeover, which draws all four
/// of these top to bottom. One builder, so the two hosts cannot show different
/// things. Returns the row index clicked this frame, or -1.
int rollup_body(const corp_rollups& r, int card, bool drillable)
{
    if (card == 3)
    {
        if (r.budget_measured)
        {
            // Seven flows now. BL-343 added Levies — what enacted law took out
            // of this quarter — and BL-454 added Force: what the standing army
            // costs to KEEP. Each sits as its OWN bar rather than folded into
            // maintenance or wages, on the same argument: the whole point of the
            // enforcement seam is that the player sees the tax as a number
            // instead of as an unexplained worse price (law.hpp § the
            // enforcement seam), and a force cost buried inside Wages is a term
            // nobody can tune against.
            charts::bar fb[7] = {
                {r.budget.income,      palette::positive, "Income",      false},
                {r.budget.expenditure, palette::negative, "Inputs",      false},
                {r.budget.maintenance, IM_COL32(190, 150, 100, 255), "Maintenance", false},
                {r.budget.wages,       IM_COL32(150, 160, 210, 255), "Wages",       false},
                {r.budget.interest,    IM_COL32(200, 110, 160, 255), "Interest",    false},
                {r.budget.levies,      IM_COL32(210, 175, 90, 255),  "Levies",      false},
                {r.budget.upkeep,      IM_COL32(200, 130, 110, 255), "Force",       false},
            };
            float peak = 0.0f;
            for (const charts::bar& b : fb) peak = std::max(peak, b.value);
            const float ceiling = charts::tight_ceil(std::max(peak, 1.0f));

            // Shorter in the ledger column than on the canvas: 220 px of chart in a
            // fold-out only a few hundred px tall would leave nothing for the three
            // card rows below it.
            const float  bar_h = drillable ? 220.0f : 140.0f;
            const ImVec2 p     = ImGui::GetCursorScreenPos();
            const float  cw    = ImGui::GetContentRegionAvail().x;
            ImGui::Dummy({cw, bar_h});
            charts::draw_bars(ImGui::GetWindowDrawList(), p, {p.x + cw, p.y + bar_h},
                              fb, 7, ceiling, "%.1f");
            ImGui::Spacing();
            ImGui::Text("Net this quarter: %+.1f", static_cast<double>(r.budget.net()));

            // BL-454's force line. The UNMET part is called out explicitly: an
            // army quietly weakening because its goods never arrived is exactly
            // the thing the player must not have to infer from a falling
            // strength number somewhere else. Silent when the corp fields no
            // units, which is the common case today.
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
        else
        {
            ImGui::TextWrapped("No budget breakdown has been recorded yet - "
                               "advance time by one economy quarter.");
        }
        return -1;
    }

    // BL-591: the growth track sits at the TOP of Production, above the
    // per-building list — a corporation-grain fact (Ben's ruling), shown once
    // for the whole card rather than repeated per building. Shared between
    // both hosts (the in-place expansion and the full-canvas takeover) since
    // this function draws for both — the whole reason rollup_body was
    // extracted by BL-265.
    if (card == 0)
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Growth track");
        if (r.reached_good.empty())
            ImGui::TextDisabled("Reached depth 0 — nothing produced yet.");
        else
            ImGui::Text("Reached depth %d, via %s", r.reached_depth, r.reached_good.c_str());

        if (r.next_rung.empty())
            ImGui::TextDisabled("At the era's depth ceiling — nothing further to unlock.");
        else
        {
            ImGui::Text("Next: %s", r.next_rung.c_str());
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                               "Needs: %s", r.missing_inputs.c_str());
        }
        ImGui::Spacing();
    }

    const std::vector<rollup_item>& items = card_items(r, card);
    if (items.empty())
    {
        ImGui::TextDisabled("Nothing to show here yet.");
        return -1;
    }

    float ceiling = 0.0f;
    for (const rollup_item& it : items)
        ceiling = std::max(ceiling, it.value);
    ceiling = charts::tight_ceil(std::max(ceiling, 1.0f));

    const ImU32 col = (card == 0) ? IM_COL32(150, 235, 160, 255)
                    : (card == 1) ? IM_COL32(120, 190, 255, 255)
                                  : IM_COL32(210, 190, 120, 255);
    const char* fmt = (card == 1) ? "%.0f" : (card == 2) ? "%.2f" : "%.1f";

    int clicked = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
        if (rollup_row(i, items[static_cast<std::size_t>(i)], ceiling, col, fmt, drillable))
            clicked = i;
    return clicked;
}

} // namespace

corp_rollups derive_corp_rollups(const world& w, const recipe_registry& reg,
                                 const economy_report& report, entity_id corp)
{
    corp_rollups r;

    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return r;
    r.balance = cit->second.balance;

    // --- Growth track (BL-591): reached depth, the good that set it, what's
    // next, and what's missing to get there. -----------------------------
    {
        const corporation_component& cc = cit->second;
        r.reached_depth = corp_reached_depth(cc, reg);

        // The good that set the reached depth: the first (ascending resource_type
        // id — deterministic, BL-406's lesson) produced-ever good at exactly that
        // depth. A fresh corp (reached_depth == 0, nothing produced) leaves this
        // empty rather than naming an arbitrary depth-0 raw it has never touched.
        for (std::size_t ri = 0; ri < resource_count; ++ri)
        {
            if (!cc.produced_ever[ri])
                continue;
            if (reg.depth_of(static_cast<resource_type>(ri)) != r.reached_depth)
                continue;
            r.reached_good = ui::resource_name(static_cast<resource_type>(ri));
            break;
        }

        // Next rung: every era-allowed processing recipe whose required depth is
        // EXACTLY reached_depth + 1. Only processing_facility carries a recipe
        // (extraction sits at depth 0, no required_depth to ask about), so that
        // is the whole search space — recipe_required_depth is a recipe property,
        // not a building_type one, so recipe_at/recipe_count is the right walk.
        const int target_depth = r.reached_depth + 1;
        const int n = reg.recipe_count(building_type::processing_facility);
        std::vector<uint16_t> next_rung_ids; // absolute ids, for the missing-input pass below
        for (int i = 0; i < n; ++i)
        {
            const recipe&  rc  = reg.recipe_at(building_type::processing_facility, i);
            const uint16_t rid = reg.recipe_id(rc.name);
            if (rid == no_recipe || reg.recipe_required_depth(rid) != target_depth)
                continue;
            next_rung_ids.push_back(rid);
            if (!r.next_rung.empty())
                r.next_rung += ", ";
            r.next_rung += rc.display_name;
        }

        // Missing inputs: across every next-rung recipe, every input this corp
        // has never produced — not just the recipe's single deepest input,
        // since the design's own wording is "the input those recipes need that
        // the corp has never produced", plain and unrestricted. A recipe at
        // reached_depth + 1 always has AT LEAST ONE such input by construction
        // (its deepest one, at depth reached_depth + 1, cannot have been
        // produced without the corp already having reached that depth) — this
        // walk also surfaces any shallower input the corp simply hasn't gotten
        // around to yet, which is real instruction, not noise: "build a
        // Charcoal Burner" is exactly as actionable as "smelt your first
        // blooms". Collected once per resource, in resource_type order, so two
        // recipes needing the same good name it once.
        if (!next_rung_ids.empty())
        {
            std::array<bool, resource_count> named{};
            for (const uint16_t rid : next_rung_ids)
            {
                const recipe* rc = reg.get_recipe(rid);
                if (!rc)
                    continue;
                for (std::size_t ri = 0; ri < resource_count; ++ri)
                {
                    if (rc->inputs[ri] <= 0.0f || named[ri] || cc.produced_ever[ri])
                        continue;
                    named[ri] = true;
                    if (!r.missing_inputs.empty())
                        r.missing_inputs += ", ";
                    r.missing_inputs += ui::resource_name(static_cast<resource_type>(ri));
                }
            }
        }
    }

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

    const corp_rollups r = derive_corp_rollups(w, reg, report, corp);

    // ── The folded column: four verdict lines and nothing else ──
    if (ui::foldout_begin("Corporation"))
    {
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", name);
        ImGui::Separator();

        char buf[128];

        // Each card rests as one verdict line and carries the `⌄ ›` gutter. `⌄`
        // grows the card's own chart or rows HERE, under its verdict, and several
        // may be open at once — that is the accordion BL-265 asked for.
        auto card = [&](int index, const char* title, ImU32 col) {
            card_row(s, index, title, buf, col);
            if (!is_open_in_place(s, detail_surface::corp_rollup, index))
                return;
            ImGui::PushID(index);
            ImGui::Indent();
            rollup_body(r, index, /*drillable=*/false);
            ImGui::Unindent();
            ImGui::PopID();
        };

        std::snprintf(buf, sizeof buf, "%d making, %d idle, %.1f/qtr",
                      r.producing, r.idle, static_cast<double>(r.output_per_tick));
        card(0, k_card_titles[0], r.idle > 0 ? palette::negative : palette::positive);

        std::snprintf(buf, sizeof buf, "%d lanes, %d convoys run", r.routes, r.convoys);
        card(1, k_card_titles[1], r.routes > 0 ? palette::positive : palette::neutral);

        std::snprintf(buf, sizeof buf, "%.0f%% of labour demand met",
                      static_cast<double>(r.tightest_contention * 100.0f));
        card(2, k_card_titles[2],
             r.tightest_contention < 0.999f ? palette::negative : palette::positive);

        if (r.budget_measured)
            std::snprintf(buf, sizeof buf, "%+.1f/qtr, balance %.0f",
                          static_cast<double>(r.budget.net()), static_cast<double>(r.balance));
        else
            std::snprintf(buf, sizeof buf, "balance %.0f (no quarter measured yet)",
                          static_cast<double>(r.balance));
        card(3, k_card_titles[3],
             (r.budget_measured && r.budget.net() < 0.0f) ? palette::negative
                                                          : palette::positive);
    }
    ui::foldout_end();

    // ── The full-canvas takeover: ALL FOUR cards, scrolled ──
    // BL-265 change 3: "anything full screen deserves its full space." The takeover
    // used to show only the card that was clicked and leave its three siblings
    // folded behind it, which spent the largest rectangle in the app on a quarter of
    // the surface. The card index survives as `expanded.key` — not to choose what is
    // drawn, but to SCOPE the drill, which is a single index with no card of its own.
    if (s.expanded.surface != detail_surface::corp_rollup)
        return;

    const int card = std::clamp(s.expanded.key, 0, 3);
    // The corp's own name titles it: the accordion is the dashboard, not one card.
    if (fold_overlay_begin(s, detail_surface::corp_rollup, s.expanded.key, name))
    {
        const std::vector<rollup_item>& items = card_items(r, card);

        const int drill = s.corp_rollup_drill;
        const bool have_drill = drill >= 0 && drill < static_cast<int>(items.size());

        if (have_drill)
        {
            const rollup_item& it = items[static_cast<std::size_t>(drill)];
            drill_breadcrumb(s, name, k_card_titles[card], it.label.c_str());

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
            // The whole accordion: all four roll-ups, headed, top to bottom. The
            // window scrolls, so a long Production list does not squeeze Finance off
            // the bottom. Clicking a row drills, and re-scopes `expanded.key` to the
            // card it came from — deferred to after the overlay closes so the target
            // cannot change while its own window is still open.
            int next_card = -1;
            int next_drill = -1;
            for (int c = 0; c < 4; ++c)
            {
                ImGui::PushID(c);
                ImGui::SeparatorText(k_card_titles[c]);
                const int clicked = rollup_body(r, c, /*drillable=*/true);
                if (clicked >= 0) { next_card = c; next_drill = clicked; }
                ImGui::PopID();
            }
            if (next_drill >= 0)
            {
                s.expanded.key       = next_card;
                s.corp_rollup_drill  = next_drill;
            }
        }

        fold_overlay_end(s);
    }
}

} // namespace ui
