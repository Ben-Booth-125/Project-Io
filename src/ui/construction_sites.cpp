#include "construction_sites.hpp"

#include "construction_panel.hpp" // grid_explainer / construction_column_card - the shared per-column hover
#include "format.hpp"             // fmt::abbreviate / signed_delta
#include "icons.hpp"              // icons::no_logistics, icons::building
#include "market_ledger.hpp"      // market_city_name - a market, by city
#include "presentation.hpp"       // palette, extraction_building_name, value_colour
#include "text_fit.hpp"           // fitted - the market column

#include "world/building_profit.hpp"  // estimate_prospective_profit - THE profit figure (no second estimator)
#include "world/construction.hpp"     // construction_capex / construction_site_multiplier
#include "world/logistic_points.hpp"  // lp_route_to - the routed cover
#include "world/logistics.hpp"        // body_tile_grid
#include "world/market_clearing.hpp"  // market_for_tile
#include "world/nation_tech.hpp"      // recipe_unlocked - the door's recipe filter
#include "world/placement_rules.hpp"  // can_place_in_world, k_extractable, is_depositless_site

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>

namespace ui {

namespace {

/// The rebuild key the Logistic Point memo and the site evaluation hold against —
/// the body_frame_stamp pattern (body_surface_canvas.cpp): what a cover can move on
/// is an ECONOMY tick (a source idles or completes, prices and ground move; the
/// quarter counter, not the day tick - keying on the day rebuilt a whole-body pass every
/// game day while time ran), a building placed or
/// demolished, or a road laid (the logistics generation).
struct lp_stamp
{
    int           econ_tick = -1;
    std::size_t   buildings = 0;
    std::size_t   ground    = 0;
    std::uint64_t logi_gen  = ~0ull;
    bool operator==(const lp_stamp&) const = default;
};

lp_stamp make_lp_stamp(const world& w)
{
    return { w.current_econ_tick, w.buildings.size(), w.tile_to_nation.size(),
             w.logistics_cache_generation };
}

struct cover_cache
{
    lp_stamp      stamp;
    lp_reach_memo memo; // every source's own field, built once per stamp
    std::unordered_map<entity_id, std::pair<float, bool>> at;
};

cover_cache& covers()
{
    static cover_cache c;
    return c;
}

/// What the evaluation was last built for. Any field moving rebuilds it.
struct eval_key
{
    entity_id     body    = null_entity;
    lp_stamp      stamp;
    building_type type    = building_type::none;
    resource_type target  = resource_type::iron_ore;
    std::uint16_t recipe  = no_recipe;
    float         balance = -1.0f;
    float         reach   = -2.0f;
    bool operator==(const eval_key&) const = default;
};

eval_key& last_key()
{
    static eval_key k;
    return k;
}

/// Rows whose displayed figure (whole credits) another row shares — the tie the
/// player would see. Deterministic: counted off a sorted copy.
int count_ties(std::vector<std::pair<long long, long long>> keys)
{
    std::sort(keys.begin(), keys.end());
    int ties = 0;
    for (std::size_t i = 0; i < keys.size();)
    {
        std::size_t j = i;
        while (j < keys.size() && keys[j] == keys[i])
            ++j;
        if (j - i > 1)
            ties += static_cast<int>(j - i);
        i = j;
    }
    return ties;
}

float market_price(const world& w, entity_id market, resource_type r)
{
    const auto mit = w.markets.find(market);
    if (mit == w.markets.end())
        return 0.0f;
    const std::size_t ri = static_cast<std::size_t>(r);
    return mit->second.price[ri] > 0.0f ? mit->second.price[ri] : mit->second.base_price[ri];
}

void evaluate(world& w, const recipe_registry& reg, ui_state& ui)
{
    site_finder_state& sf = ui.site_finder;
    const auto t0 = std::chrono::steady_clock::now();

    sf.rows.clear();
    sf.cls.clear();
    sf.own_tiles = sf.unaffordable = sf.no_lp_rows = 0;
    sf.body = ui.active_body;
    sf.gw = sf.gh = 0;

    const auto bit = w.bodies.find(ui.active_body);
    if (bit == w.bodies.end() || bit->second.grid_width <= 0 || bit->second.grid_height <= 0)
        return;
    sf.gw = bit->second.grid_width;
    sf.gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, ui.active_body);
    sf.cls.assign(static_cast<std::size_t>(sf.gw) * static_cast<std::size_t>(sf.gh), 0);

    const entity_id actor = player_actor(w);
    float balance = 0.0f;
    if (const float* p = purse_of(w, actor); p != nullptr)
        balance = *p;

    const site_option& c = sf.choice;
    const placement_gate gate = reg.placement_gate_for(c.type, c.recipe);
    const recipe* rc = (c.type == building_type::processing_facility) ? reg.get_recipe(c.recipe) : nullptr;
    const float flat_cost = reg.economics(c.type).build_cost;

    std::map<entity_id, std::string> market_names; // one name lookup per market

    // THE ROW'S IDENTITY (Ben, 2026-10-26: "remove the tile x,y column ... use what
    // the glossary offers, e.g. the province or nearest place name"). A province has
    // no name of its own (GLOSSARY.md § Province) but it is its centre's ground
    // (PROVINCES.md), so a row is named for the population centre its province
    // holds - the lowest centre id where a province holds two, so the name is one
    // answer - and, for ground no centre's province covers, the nearest named centre
    // on the body ("near X"). Built once per evaluation, in id order.
    std::map<std::uint32_t, std::string> province_place;
    struct named_centre { int x = 0, y = 0; const std::string* name = nullptr; };
    std::vector<named_centre> centres;
    {
        std::vector<std::pair<entity_id, entity_id>> ct(w.population_centre_tile.begin(),
                                                        w.population_centre_tile.end());
        std::sort(ct.begin(), ct.end());
        for (const auto& [cid, ctile] : ct)
        {
            const auto nm  = w.population_centre_name.find(cid);
            const auto tt  = w.tiles.find(ctile);
            if (nm == w.population_centre_name.end() || nm->second.empty() || tt == w.tiles.end()
                || tt->second.body != ui.active_body)
                continue;
            if (const std::uint32_t pid = w.provinces.province_of(ctile); pid != 0)
                province_place.try_emplace(pid, nm->second);
            centres.push_back({tt->second.grid_x, tt->second.grid_y, &nm->second});
        }
    }
    const int gw_wrap = sf.gw;
    const auto place_of = [&](entity_id tid, int x, int y, bool& near) -> std::string {
        near = false;
        if (const std::uint32_t pid = w.provinces.province_of(tid); pid != 0)
            if (const auto it = province_place.find(pid); it != province_place.end())
                return it->second;
        // The nearest named centre, the grid wrapping east-west; ties to the lower id.
        long best = -1;
        const std::string* name = nullptr;
        for (const named_centre& c : centres)
        {
            int dx = std::abs(c.x - x);
            if (gw_wrap > 0)
                dx = std::min(dx, gw_wrap - dx);
            const int  dy = c.y - y;
            const long d2 = static_cast<long>(dx) * dx + static_cast<long>(dy) * dy;
            if (best < 0 || d2 < best)
            {
                best = d2;
                name = c.name;
            }
        }
        near = true;
        return name != nullptr ? *name : std::string();
    };
    for (std::size_t idx = 0; idx < grid.size() && idx < sf.cls.size(); ++idx)
    {
        const entity_id tid = grid[idx];
        if (tid == null_entity)
            continue;
        // Nations build only on their own ground (NATIONS.md § Own ground): the rest of
        // the body is not even asked, which is also what keeps this pass cheap.
        const auto nit = w.tile_to_nation.find(tid);
        if (nit == w.tile_to_nation.end() || nit->second != actor)
            continue;
        ++sf.own_tiles;

        // THE BUILD DOOR'S OWN GATES, in its order: the body's air (a propellant
        // recipe this tile cannot run is a row the door never lists), then the
        // world-level placement rules with the player's actor named (own ground, tech,
        // roads, stacks, the province ceiling), then the treasury against the capex.
        if (rc != nullptr && !recipe_runs_at_tile(w, *rc, tid))
            continue;
        const placement_rules::placement_result pr = placement_rules::can_place_in_world(
            w, tid, c.type, c.target, ui.max_logistics_reach, actor, gate);
        if (!pr)
            continue;
        const float capex = construction_capex(w, reg, tid, c.type, c.target, c.recipe);
        if (balance < capex)
        {
            ++sf.unaffordable;
            continue;
        }

        const tile_component& tc = w.tiles.at(tid);
        site_row r;
        r.tile = tid;
        r.x    = tc.grid_x;
        r.y    = tc.grid_y;
        r.place = place_of(tid, r.x, r.y, r.place_near);
        r.capex     = capex;
        r.site_mult = construction_site_multiplier(w, reg, tid, c.type, c.target);
        r.flat      = flat_cost * r.site_mult;
        r.cover     = site_lp_cover(w, tid, &r.lp_blocked);
        r.no_logistics = !(r.cover > 0.0f);

        // THE PROFIT IS THE DOOR'S FIGURE, never a second estimator: the same
        // function, the same arguments, the tile's cover applied.
        const building_profit bp = estimate_prospective_profit(
            w, reg, tid, c.type, c.target, c.recipe, nullptr, nullptr, r.cover);
        r.priced = bp.has_data;
        r.profit = bp.net();
        r.profit_full_cover = (r.cover >= 1.0f)
            ? r.profit
            : estimate_prospective_profit(w, reg, tid, c.type, c.target, c.recipe,
                                          nullptr, nullptr, 1.0f).net();

        r.market = market_for_tile(w, tid);
        if (r.market != null_entity)
        {
            auto [it, fresh] = market_names.try_emplace(r.market);
            if (fresh)
                it->second = market_city_name(w, r.market);
            r.market_name = it->second;
        }

        if (c.type == building_type::extraction_site)
        {
            const std::size_t ri = static_cast<std::size_t>(c.target);
            r.deposit     = tc.resource_deposit[ri];
            r.remaining   = tc.resource_remaining[ri];
            r.depositless = placement_rules::is_depositless_site(w, tid, c.target);
            // Yield is the estimate's own revenue at the price it was valued at, so
            // it can never be a different reading of the tile from the profit beside it.
            const float p = market_price(w, r.market, c.target);
            if (bp.has_data && p > 0.0f)
            {
                r.has_yield = true;
                r.yield     = bp.revenue / p;
            }
        }

        sf.cls[idx] = r.no_logistics ? 2 : 1;
        if (r.no_logistics)
            ++sf.no_lp_rows;
        sf.rows.push_back(std::move(r));
    }

    // The tie measurement, on the whole credits the ledger shows.
    std::vector<std::pair<long long, long long>> k_full, k_profit, k_both;
    k_full.reserve(sf.rows.size());
    k_profit.reserve(sf.rows.size());
    k_both.reserve(sf.rows.size());
    for (const site_row& r : sf.rows)
    {
        const long long pf = std::llround(r.profit_full_cover);
        const long long pp = std::llround(r.profit);
        k_full.push_back({pf, 0});
        k_profit.push_back({pp, 0});
        k_both.push_back({pp, std::llround(r.capex)});
    }
    sf.ties_full_cover  = count_ties(std::move(k_full));
    sf.ties_profit      = count_ties(std::move(k_profit));
    sf.ties_profit_cost = count_ties(std::move(k_both));
    std::vector<std::pair<long long, long long>> k_cov;
    for (const site_row& r : sf.rows)
        if (r.cover > 0.0f)
            k_cov.push_back({std::llround(r.profit), 0});
    sf.covered_rows = static_cast<int>(k_cov.size());
    sf.ties_covered = count_ties(std::move(k_cov));

    sort_site_rows(sf.rows, sf.sort_col, sf.sort_desc);

    ++sf.evaluations;
    sf.eval_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

const char* category_of(building_type t)
{
    switch (t)
    {
    case building_type::extraction_site:     return "Extraction";
    case building_type::processing_facility: return "Processing";
    case building_type::military_base:
    case building_type::airbase:
    case building_type::research_institute:  return "Military";
    default:                                 return "Infrastructure";
    }
}

} // namespace

std::vector<site_option> site_options(const world& w, const recipe_registry& reg)
{
    std::vector<site_option> out;
    const entity_id actor = player_actor(w);
    // Extraction: one per extractable good, named as the door names it ("Farm" for
    // produce, which also covers the coastal Fishing Wharf; "Well" ground passes the
    // water option's placement through `can_place_in_world`).
    if (reg.building_available(building_type::extraction_site))
        for (const resource_type er : placement_rules::k_extractable)
            out.push_back({building_type::extraction_site, er, no_recipe,
                           er == resource_type::agricultural_produce ? std::string("Farm")
                                                                     : std::string(extraction_building_name(er)),
                           "Extraction"});
    // Processing: one per RECIPE in the era-masked browse order the door walks, with
    // the door's unlock filter.
    if (reg.building_available(building_type::processing_facility))
        for (int ri = 0; ri < reg.recipe_count(building_type::processing_facility); ++ri)
        {
            const recipe& browsed = reg.recipe_at(building_type::processing_facility, ri);
            const std::uint16_t id = reg.recipe_id(browsed.name);
            if (!recipe_unlocked(w, reg, actor, id))
                continue;
            out.push_back({building_type::processing_facility, resource_type::iron_ore, id,
                           browsed.display_name, "Processing"});
        }
    const std::pair<building_type, const char*> fixed[] = {
        {building_type::port, "Port"},
        {building_type::planetary_marketplace, "Planetary Marketplace"},
        {building_type::launchpad, "Launchpad"},
        {building_type::inland_logistics_hub, "Inland Logistics Hub"},
        {building_type::military_base, "Military Base"},
        {building_type::airbase, "Airbase"},
        {building_type::research_institute, "Research Institute"},
    };
    for (const auto& [t, name] : fixed)
        if (reg.building_available(t))
            out.push_back({t, resource_type::iron_ore, no_recipe, name, category_of(t)});
    // The door's fold order: category, then name; stable so equal names keep the
    // authored order.
    std::stable_sort(out.begin(), out.end(), [](const site_option& a, const site_option& b) {
        if (a.category != b.category)
            return a.category < b.category;
        return a.name < b.name;
    });
    return out;
}

float site_lp_cover(world& w, entity_id tile, bool* blocked)
{
    if (blocked != nullptr)
        *blocked = false;
    cover_cache& cc = covers();
    const lp_stamp now = make_lp_stamp(w);
    if (!(now == cc.stamp))
    {
        cc.stamp = now;
        cc.memo.clear();
        cc.at.clear();
    }
    if (const auto it = cc.at.find(tile); it != cc.at.end())
    {
        if (blocked != nullptr)
            *blocked = it->second.second;
        return it->second.first;
    }
    float cover = 0.0f;
    bool  blk   = false;
    const auto tit = w.tiles.find(tile);
    const auto nit = w.tile_to_nation.find(tile);
    if (tit != w.tiles.end() && nit != w.tile_to_nation.end() && nit->second != null_entity)
    {
        const lp_route r = lp_route_to(w, cc.memo, nit->second, tit->second.body, tile);
        cover = r.cover;
        blk   = r.blocked;
    }
    cc.at.emplace(tile, std::make_pair(cover, blk));
    if (blocked != nullptr)
        *blocked = blk;
    return cover;
}

void sort_site_rows(std::vector<site_row>& rows, int col, bool desc)
{
    std::sort(rows.begin(), rows.end(), [col, desc](const site_row& a, const site_row& b) {
        // An unpriced row (no market resolves) has nothing honest to rank on: it goes
        // last in either direction rather than posing as a zero.
        if (col == 1 && a.priced != b.priced)
            return a.priced;
        int c = 0; // <0: a first ascending
        switch (col)
        {
        case 0: c = a.place.compare(b.place); c = (c < 0) ? -1 : (c > 0 ? 1 : 0); break;
        case 1: c = (a.profit != b.profit) ? (a.profit < b.profit ? -1 : 1) : 0; break;
        case 2: c = (a.capex != b.capex) ? (a.capex < b.capex ? -1 : 1) : 0; break;
        case 3: c = a.market_name.compare(b.market_name); c = (c < 0) ? -1 : (c > 0 ? 1 : 0); break;
        case 4: c = (a.cover != b.cover) ? (a.cover < b.cover ? -1 : 1) : 0; break;
        case 5:
            if (a.has_yield != b.has_yield)
                return a.has_yield;
            c = (a.yield != b.yield) ? (a.yield < b.yield ? -1 : 1) : 0;
            break;
        default: break;
        }
        if (c != 0)
            return desc ? c > 0 : c < 0;
        return a.tile < b.tile; // THE tie-break, in every direction
    });
}

void update_site_finder(world& w, const recipe_registry& reg, ui_state& ui)
{
    site_finder_state& sf = ui.site_finder;
    if (!sf.armed)
    {
        sf.live = false;
        return;
    }
    const entity_id actor = player_actor(w);
    float balance = 0.0f;
    if (const float* p = purse_of(w, actor); p != nullptr)
        balance = *p;
    const eval_key key{ ui.active_body, make_lp_stamp(w), sf.choice.type, sf.choice.target,
                        sf.choice.recipe, balance, ui.max_logistics_reach };
    if (!(key == last_key()) || sf.body != ui.active_body)
    {
        last_key() = key;
        evaluate(w, reg, ui);
    }
    sf.live = (ui.primary_level == canvas_level::planetary) && sf.body == ui.active_body;
}

void draw_site_finder(world& w, const recipe_registry& reg, ui_state& ui)
{
    site_finder_state& sf = ui.site_finder;
    sf.drawn.clear();
    sf.picker_x = sf.picker_y = -1.0f;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Find a site");

    // THE PICKER. A selector, not a state: the chosen row is the target the finder
    // prices, and choosing it again (or "None") clears it, so the visible choice is
    // a toggle as the standing rule asks.
    const std::vector<site_option> opts = site_options(w, reg);
    {
        const ImVec2 pp = ImGui::GetCursorScreenPos();
        sf.picker_x = pp.x + 40.0f;
        sf.picker_y = pp.y + ImGui::GetFrameHeight() * 0.5f;
    }
    ImGui::SetNextItemWidth(-1.0f);
    const char* preview = sf.armed ? sf.choice.name.c_str() : "Choose a building...";
    if (ImGui::BeginCombo("##site_building", preview, ImGuiComboFlags_HeightLarge))
    {
        if (ImGui::Selectable("None", !sf.armed))
            sf.armed = false;
        std::string fold;
        for (std::size_t i = 0; i < opts.size(); ++i)
        {
            const site_option& o = opts[i];
            if (o.category != fold)
            {
                fold = o.category;
                ImGui::SeparatorText(fold.c_str());
            }
            const bool is_choice = sf.armed && sf.choice.type == o.type
                && sf.choice.target == o.target && sf.choice.recipe == o.recipe;
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(o.name.c_str(), is_choice))
            {
                if (is_choice)
                    sf.armed = false; // the toggle: the active choice, pressed again, clears
                else
                {
                    sf.armed  = true;
                    sf.choice = o;
                }
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    update_site_finder(w, reg, ui);
    if (!sf.armed)
    {
        ImGui::TextDisabled("Choose a building to mark the tiles that take it.");
        return;
    }

    // What the figures are, stated once (the door's caption, with the cover named).
    ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    if (sf.rows.empty())
        ImGui::TextWrapped("No tile of your ground on this body takes it.");
    else
        ImGui::TextWrapped("%d sites on your ground, by est. net / qtr at today's local prices "
                           "(50%% staffing, no labour shortage, each tile's Logistic Point cover).",
                           static_cast<int>(sf.rows.size()));
    if (sf.no_lp_rows > 0)
    {
        ImGui::PopStyleColor();
        const ImVec2 gp = ImGui::GetCursorScreenPos();
        const float  lh = ImGui::GetTextLineHeight();
        icons::no_logistics(ImGui::GetWindowDrawList(), {gp.x + lh * 0.5f, gp.y + lh * 0.5f},
                            lh * 0.45f, palette::site_no_logistics);
        ImGui::Dummy({lh, lh});
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::site_no_logistics),
                           "%d with no Logistic Points operating", sf.no_lp_rows);
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    }
    if (sf.unaffordable > 0)
        ImGui::TextWrapped("%d more would take it but cost more than the treasury.", sf.unaffordable);
    ImGui::PopStyleColor();
    if (sf.rows.empty())
        return;

    // THE SITE LEDGER. One row per tile, every column sortable, profit descending by
    // default, every tie broken by tile id (sort_site_rows).
    const float line_h  = ImGui::GetTextLineHeightWithSpacing();
    const int   shown   = std::min<int>(static_cast<int>(sf.rows.size()), 9);
    const float table_h = line_h * static_cast<float>(shown + 1) + 6.0f;
    const ImGuiTableFlags tf = ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg
                             | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV
                             | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
    // Column widths, MEASURED, never guessed (Ben, 2026-10-26: "every column header
    // must be legible: no 'Yi...' elision at any supported width"). A sortable
    // header reserves the sort arrow beside its label (imgui_tables.cpp's
    // `w_arrow`: 0.65 of the font size plus the frame padding), and the 2026-10-25
    // constants were sized to the figures alone, so "Yield" lost its tail to the
    // arrow. Each fixed column is now the wider of its header with the arrow and
    // its widest figure; the two stretch columns (Place, Market) share the rest,
    // and every header publishes what it needed against what it got.
    const float arrow_w = std::floor(ImGui::GetFontSize() * 0.65f + ImGui::GetStyle().FramePadding.x);
    // The figures' widest is measured off the rows themselves rather than a
    // worst-case sample, because at the fold-out's narrowest every pixel a fixed
    // column does not need is a pixel the two named columns' headers do.
    float fig_net = 0.0f, fig_cost = 0.0f, fig_yield = 0.0f;
    for (const site_row& r : sf.rows)
    {
        if (r.priced)
            fig_net = std::max(fig_net, ImGui::CalcTextSize(fmt::abbreviate(static_cast<double>(r.profit)).c_str()).x);
        fig_cost = std::max(fig_cost, ImGui::CalcTextSize(fmt::abbreviate(static_cast<double>(r.capex)).c_str()).x);
        if (r.has_yield)
            fig_yield = std::max(fig_yield, ImGui::CalcTextSize(fmt::abbreviate(static_cast<double>(r.yield)).c_str()).x);
    }
    // THE ARROW IS RESERVED ONLY WHERE IT DRAWS. ImGui's own header reserves the
    // arrow's width on every sortable column, sorted or not, and elides the label
    // against it - which at ~290 px of table cost each of six headers ~12 px for
    // an arrow five of them never show. So the headers are drawn here: ImGui's
    // header item keeps the press, the hover and the arrow on the SORTED column,
    // and the label is drawn whole beside it. A header needs its text, plus the
    // arrow only on the column the rows are sorted by.
    const auto  header_need = [&](const char* label, int uid) {
        return ImGui::CalcTextSize(label).x + (uid == sf.sort_col ? arrow_w : 0.0f);
    };
    const auto  fixed_w = [&](const char* label, int uid, float widest) {
        return std::ceil(std::max(header_need(label, uid), widest)) + 1.0f;
    };
    const float lh0     = ImGui::GetTextLineHeight();
    const float w_net   = fixed_w("Net", 1, fig_net);
    const float w_cost  = fixed_w("Cost", 2, fig_cost);
    // A zero cover reads as the struck truck ALONE: the flag is the zero, and a
    // "0%" beside it cost the width the Market header needed.
    const float w_lp    = std::max(fixed_w("LP", 4, ImGui::CalcTextSize("100%").x), std::ceil(lh0) + 1.0f);
    const float w_yield = fixed_w("Yield", 5, fig_yield);
    // The two stretch columns share what is left in proportion to what their
    // headers need, so neither elides while the other has room to spare.
    const float need_place  = header_need("Place", 0);
    const float need_market = header_need("Market", 3);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {2.0f, 1.0f});
    if (!ImGui::BeginTable("##site_ledger", 6, tf, {0.0f, table_h}))
    {
        ImGui::PopStyleVar();
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Place",  ImGuiTableColumnFlags_WidthStretch, need_place, 0);
    ImGui::TableSetupColumn("Net",    ImGuiTableColumnFlags_WidthFixed
                                      | ImGuiTableColumnFlags_DefaultSort
                                      | ImGuiTableColumnFlags_PreferSortDescending, w_net, 1);
    ImGui::TableSetupColumn("Cost",   ImGuiTableColumnFlags_WidthFixed, w_cost, 2);
    ImGui::TableSetupColumn("Market", ImGuiTableColumnFlags_WidthStretch, need_market, 3);
    ImGui::TableSetupColumn("LP",     ImGuiTableColumnFlags_WidthFixed
                                      | ImGuiTableColumnFlags_PreferSortDescending, w_lp, 4);
    ImGui::TableSetupColumn("Yield",  ImGuiTableColumnFlags_WidthFixed
                                      | ImGuiTableColumnFlags_PreferSortDescending, w_yield, 5);
    // The header row, drawn by hand so each header can report its own fit.
    sf.headers.clear();
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for (int ci = 0; ci < 6; ++ci)
    {
        if (!ImGui::TableSetColumnIndex(ci))
            continue;
        const char*  name = ImGui::TableGetColumnName(ci);
        const float  have = ImGui::GetContentRegionAvail().x;
        const ImVec2 at   = ImGui::GetCursorScreenPos();
        char id[24];
        std::snprintf(id, sizeof id, "##site_hdr%d", ci);
        ImGui::TableHeader(id); // the press, the hover and the sorted column's arrow
        ImGui::GetWindowDrawList()->AddText(at, ImGui::GetColorU32(ImGuiCol_Text), name);
        sf.headers.push_back({name, header_need(name, ci), have});
    }

    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty)
    {
        if (specs->SpecsCount > 0)
        {
            sf.sort_col  = static_cast<int>(specs->Specs[0].ColumnUserID);
            sf.sort_desc = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
        }
        sort_site_rows(sf.rows, sf.sort_col, sf.sort_desc);
        specs->SpecsDirty = false;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiListClipper clip;
    clip.Begin(static_cast<int>(sf.rows.size()));
    while (clip.Step())
        for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i)
        {
            const site_row& r = sf.rows[static_cast<std::size_t>(i)];
            ImGui::TableNextRow();
            std::vector<grid_explainer> cells;
            const auto cell = [&](const char* key, float wpx, std::string text, ImU32 ink = 0) {
                const ImVec2 p = ImGui::GetCursorScreenPos();
                grid_explainer e;
                e.key  = key;
                e.mn   = {p.x - 2.0f, p.y - 1.0f};
                e.mx   = {p.x + wpx + 2.0f, p.y + line_h - 1.0f};
                e.text = std::move(text);
                e.ink  = ink;
                cells.push_back(std::move(e));
            };

            // Place — the row's identity and its press: select the tile and frame
            // it. The place action stays the tile's Build door below.
            ImGui::TableSetColumnIndex(0);
            const std::string place = r.place.empty() ? std::string("Unnamed ground")
                                    : r.place_near   ? "near " + r.place
                                                     : r.place;
            const ImVec2 rp = ImGui::GetCursorScreenPos();
            const float  pw = std::max(16.0f, ImGui::GetContentRegionAvail().x);
            cell("place", pw,
                 (r.place.empty() ? std::string("No named place is near this tile. ")
                  : r.place_near  ? "No centre's province holds this tile; " + r.place
                                        + " is the nearest place. "
                                  : "In the province of " + r.place + ". ")
                     + "Press to select the tile and frame it on the map; its Build door "
                       "below places the building.");
            char label[32];
            std::snprintf(label, sizeof label, "##site%u", static_cast<unsigned>(r.tile));
            if (ImGui::Selectable(label, ui.selected_entity == r.tile,
                                  ImGuiSelectableFlags_SpanAllColumns))
            {
                ui.selected_entity          = r.tile;
                ui.selected_province        = w.provinces.province_of(r.tile);
                ui.province_sync_entity     = r.tile;
                ui.planetary_center_col     = r.x;
                ui.planetary_center_row     = r.y;
                ui.planetary_center_pending = true;
            }
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(fitted(place.c_str(), pw).c_str());
            sf.drawn.push_back({r.tile, rp.x + 12.0f, rp.y + ImGui::GetTextLineHeight() * 0.5f});

            // Net / qtr — the door's estimate, cover applied.
            ImGui::TableSetColumnIndex(1);
            {
                char buf[240];
                if (!r.priced)
                    std::snprintf(buf, sizeof buf, "No market prices this tile, so there is no estimate.");
                else if (r.cover < 1.0f)
                    std::snprintf(buf, sizeof buf,
                                  "Est. net %+d cr / qtr: it would run at %d%% while its Logistic "
                                  "Point cover is %d%%. Fully served it would make %+d.",
                                  static_cast<int>(std::lround(r.profit)),
                                  static_cast<int>(std::lround(r.cover * 100.0f)),
                                  static_cast<int>(std::lround(r.cover * 100.0f)),
                                  static_cast<int>(std::lround(r.profit_full_cover)));
                else
                    std::snprintf(buf, sizeof buf,
                                  "Est. net %+d cr / qtr at today's local prices, fully served.",
                                  static_cast<int>(std::lround(r.profit)));
                const ImU32 ink = r.priced ? value_colour(static_cast<double>(r.profit)) : 0u;
                cell("profit", w_net, buf, ink);
                if (r.priced)
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ink), "%s",
                                       fmt::abbreviate(static_cast<double>(r.profit)).c_str());
                else
                    ImGui::TextDisabled("-");
            }

            // Cost — the flat credits times the site multiplier, paid at placement,
            // plus the materials at this market's posted prices, bought in arrears as
            // each tick draws them (Ben, 2026-10-26: costs depend on market prices).
            ImGui::TableSetColumnIndex(2);
            {
                char buf[260];
                std::snprintf(buf, sizeof buf,
                              "Costs %d cr: %d cr of flat build cost, paid when it is placed, and "
                              "%d cr of materials at this market's posted prices, bought as it "
                              "builds. This ground's site multiplier is x%.2f.",
                              static_cast<int>(std::lround(r.capex)),
                              static_cast<int>(std::lround(r.flat)),
                              static_cast<int>(std::lround(std::max(0.0f, r.capex - r.flat))),
                              static_cast<double>(r.site_mult));
                cell("cost", w_cost, buf);
                ImGui::TextDisabled("%s", fmt::abbreviate(static_cast<double>(r.capex)).c_str());
            }

            // Market — which market the tile clears in.
            ImGui::TableSetColumnIndex(3);
            {
                const float mw = std::max(16.0f, ImGui::GetContentRegionAvail().x);
                cell("market", mw, r.market == null_entity
                                       ? std::string("No market supplies this tile.")
                                       : "Sells and buys at " + r.market_name + "'s market.");
                ImGui::TextUnformatted(r.market == null_entity
                                           ? "-"
                                           : fitted(r.market_name.c_str(), mw).c_str());
            }

            // LP — the routed Logistic Point cover, and the flag where it is zero.
            ImGui::TableSetColumnIndex(4);
            {
                const ImVec2 p  = ImGui::GetCursorScreenPos();
                const float  lh = ImGui::GetTextLineHeight();
                std::string text;
                if (r.no_logistics)
                    text = r.lp_blocked
                        ? "Cover 0%: no Logistic Points operate here. A source reaches it but is not tied to "
                          "the capital. It would build, and not run."
                        : "Cover 0%: no Logistic Points operate here, and no source reaches it. It would build, "
                          "and not run until a hub, station or port does.";
                else
                {
                    char buf[200];
                    std::snprintf(buf, sizeof buf,
                                  "Logistic Point cover %d%%: the share of its run the pool can grant here.",
                                  static_cast<int>(std::lround(r.cover * 100.0f)));
                    text = buf;
                }
                cell("lp", w_lp, text, r.no_logistics ? palette::site_no_logistics : 0u);
                if (r.no_logistics)
                {
                    icons::no_logistics(dl, {p.x + lh * 0.5f, p.y + lh * 0.5f}, lh * 0.45f,
                                        palette::site_no_logistics);
                    ImGui::Dummy({lh, lh}); // the flag IS the zero; its card says so
                }
                else
                    ImGui::Text("%d%%", static_cast<int>(std::lround(r.cover * 100.0f)));
            }

            // Yield — units of what it makes per qtr (extraction: the deposit's figure).
            ImGui::TableSetColumnIndex(5);
            {
                char buf[240];
                if (r.has_yield && r.depositless)
                    std::snprintf(buf, sizeof buf,
                                  "About %.1f units / qtr. No deposit: it draws on the river or the "
                                  "sea, which never runs out.", static_cast<double>(r.yield));
                else if (r.has_yield)
                    std::snprintf(buf, sizeof buf,
                                  "About %.1f units / qtr from a deposit of richness %.2f, %.0f units "
                                  "left in the ground.", static_cast<double>(r.yield),
                                  static_cast<double>(r.deposit), static_cast<double>(r.remaining));
                else
                    std::snprintf(buf, sizeof buf, "No deposit figure: its output does not turn on this ground.");
                cell("yield", w_yield, buf);
                if (r.has_yield)
                    ImGui::TextDisabled("%s", fmt::abbreviate(static_cast<double>(r.yield)).c_str());
                else
                    ImGui::TextDisabled("-");
            }

            const std::string title = sf.choice.name + ", " + place;
            construction_column_card(title.c_str(), cells);
        }
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

} // namespace ui
