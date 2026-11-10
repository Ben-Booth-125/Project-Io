#include "construction_sites.hpp"

#include "construction_panel.hpp" // grid_explainer / construction_column_card - the shared per-column hover
#include "format.hpp"             // fmt::abbreviate / signed_delta
#include "icons.hpp"              // icons::no_logistics, icons::building
#include "market_ledger.hpp"      // market_city_name - a market, by city
#include "nav_pane.hpp"           // clear_selection_to_resting - the scope line's clear
#include "presentation.hpp"       // palette, extraction_building_name, value_colour
#include "text_fit.hpp"           // fitted - the market column

#include "world/building_profit.hpp"  // estimate_prospective_profit - THE profit figure (no second estimator)
#include "world/input_reach.hpp"      // judged_batches - the plant's use, as the quote judges it
#include "world/construction.hpp"     // construction_capex / construction_site_multiplier
#include "world/logistic_points.hpp"  // lp_route_to - the routed cover
#include "world/logistics.hpp"        // body_tile_grid
#include "world/market_clearing.hpp"  // market_for_tile
#include "world/nation_tech.hpp"      // recipe_unlocked - the door's recipe filter
#include "world/placement_rules.hpp"  // can_place_in_world, k_extractable, is_depositless_site

#include <tuple>
#include <optional>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
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

/// The quotes asked for since the stamp last moved: an economy tick, a building placed
/// or demolished, a road. A mothball or recipe switch while paused is read at the next
/// quarter, as prices are; the door and the finder stay stale together, so they agree.
struct quote_cache
{
    lp_stamp                          stamp;
    std::optional<site_quote_context> cx;
    std::map<std::tuple<entity_id, int, int, int, float>, site_quote> at;
    std::map<std::tuple<entity_id, int, float>, shelf_runway> runway; ///< (market, recipe, cover)
};

const market_plot_history*& quote_history()
{
    static const market_plot_history* h = nullptr;
    return h;
}

const economy_report*& quote_report()
{
    static const economy_report* r = nullptr;
    return r;
}

quote_cache& quotes()
{
    static quote_cache c;
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
    std::uint32_t scope   = 0;
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

/// THE ROW'S IDENTITY (Ben, 2026-10-26: "remove the tile x,y column ... use what
/// the glossary offers, e.g. the province or nearest place name"). A province has
/// no name of its own (GLOSSARY.md § Province) but it is its centre's ground
/// (PROVINCES.md), so a row is named for the population centre its province
/// holds - the lowest centre id where a province holds two, so the name is one
/// answer - and, for ground no centre's province covers, the nearest named centre
/// on the body ("near X"). Built once per evaluation, in id order.
class place_index
{
public:
    place_index(const world& w, entity_id body, int grid_w) : m_w(w), m_gw(grid_w)
    {
        std::vector<std::pair<entity_id, entity_id>> ct(w.population_centre_tile.begin(),
                                                        w.population_centre_tile.end());
        std::sort(ct.begin(), ct.end());
        for (const auto& [cid, ctile] : ct)
        {
            const auto nm = w.population_centre_name.find(cid);
            const auto tt = w.tiles.find(ctile);
            if (nm == w.population_centre_name.end() || nm->second.empty() || tt == w.tiles.end()
                || tt->second.body != body)
                continue;
            if (const std::uint32_t pid = w.provinces.province_of(ctile); pid != 0)
                m_province.try_emplace(pid, nm->second);
            m_centres.push_back({tt->second.grid_x, tt->second.grid_y, &nm->second});
        }
    }

    std::string name(entity_id tid, int x, int y, bool& near) const
    {
        near = false;
        if (const std::uint32_t pid = m_w.provinces.province_of(tid); pid != 0)
            if (const auto it = m_province.find(pid); it != m_province.end())
                return it->second;
        // The nearest named centre, the grid wrapping east-west; ties to the lower id.
        long best = -1;
        const std::string* found = nullptr;
        for (const named_centre& c : m_centres)
        {
            int dx = std::abs(c.x - x);
            if (m_gw > 0)
                dx = std::min(dx, m_gw - dx);
            const int  dy = c.y - y;
            const long d2 = static_cast<long>(dx) * dx + static_cast<long>(dy) * dy;
            if (best < 0 || d2 < best)
            {
                best  = d2;
                found = c.name;
            }
        }
        near = true;
        return found != nullptr ? *found : std::string();
    }

private:
    struct named_centre { int x = 0, y = 0; const std::string* name = nullptr; };
    const world&                         m_w;
    int                                  m_gw = 0;
    std::map<std::uint32_t, std::string> m_province;
    std::vector<named_centre>            m_centres;
};

/// THE BUILD DOOR'S OWN GATES, in its order: the body's air (a propellant recipe this
/// tile cannot run is a row the door never lists), then the world-level placement
/// rules with the player's actor named (own ground, tech, roads, stacks, the province
/// ceiling), then the treasury against the capex. The table and the grid both ask
/// here, so a cell's count is the table's row count by construction.
enum class site_verdict { air, refused, unaffordable, ok };
struct site_gate_result
{
    site_verdict verdict = site_verdict::refused;
    placement_rules::placement_reason why = placement_rules::placement_reason::ok;
    float capex = 0.0f;
};

site_gate_result gate_site(const world& w, const recipe_registry& reg, const site_option& o, const recipe* rc,
                           const placement_gate& gate, entity_id tid, float reach, entity_id actor, float balance)
{
    site_gate_result out;
    if (rc != nullptr && !recipe_runs_at_tile(w, *rc, tid))
    {
        out.verdict = site_verdict::air;
        return out;
    }
    const placement_rules::placement_result pr =
        placement_rules::can_place_in_world(w, tid, o.type, o.target, reach, actor, gate);
    if (!pr)
    {
        out.why = pr.reason;
        return out;
    }
    out.capex   = construction_capex(w, reg, tid, o.type, o.target, o.recipe);
    out.verdict = (balance < out.capex) ? site_verdict::unaffordable : site_verdict::ok;
    return out;
}

bool option_earns(const site_option& o); // the grid's rule, below

void evaluate(world& w, const recipe_registry& reg, ui_state& ui)
{
    site_finder_state& sf = ui.site_finder;
    const auto t0 = std::chrono::steady_clock::now();

    sf.rows.clear();
    sf.cls.clear();
    sf.cell_net.clear();
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
    sf.cell_net.assign(sf.cls.size(), std::numeric_limits<float>::quiet_NaN());

    const entity_id actor = player_actor(w);
    float balance = 0.0f;
    if (const float* p = purse_of(w, actor); p != nullptr)
        balance = *p;

    const site_option& c = sf.choice;
    const placement_gate gate = reg.placement_gate_for(c.type, c.recipe);
    const recipe* rc = (c.type == building_type::processing_facility) ? reg.get_recipe(c.recipe) : nullptr;
    const float flat_cost = reg.live_build_credits(c.type);

    std::map<entity_id, std::string> market_names; // one name lookup per market

    const place_index places(w, ui.active_body, sf.gw);
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
        // THE SCOPE (Ben, 2026-11-08): a selected province narrows the search to its ground.
        if (sf.scope_province != 0 && w.provinces.province_of(tid) != sf.scope_province)
            continue;
        ++sf.own_tiles;

        const site_gate_result sg = gate_site(w, reg, c, rc, gate, tid, ui.max_logistics_reach, actor, balance);
        if (sg.verdict == site_verdict::air || sg.verdict == site_verdict::refused)
            continue;
        const float capex = sg.capex;
        if (sg.verdict == site_verdict::unaffordable)
        {
            ++sf.unaffordable;
            continue;
        }

        const tile_component& tc = w.tiles.at(tid);
        site_row r;
        r.tile = tid;
        r.x    = tc.grid_x;
        r.y    = tc.grid_y;
        r.place = places.name(tid, r.x, r.y, r.place_near);
        r.capex     = capex;
        r.site_mult = construction_site_multiplier(w, reg, tid, c.type, c.target);
        r.flat      = flat_cost * r.site_mult;
        r.cover     = site_lp_cover(w, tid, &r.lp_blocked);
        r.no_logistics = !(r.cover > 0.0f);

        // THE PROFIT IS THE DOOR'S FIGURE, never a second estimator: the same
        // quote, the tile's cover applied.
        const site_quote sq = quote_for_tile(w, reg, tid, c.type, c.target, c.recipe, r.cover);
        const building_profit& bp = sq.profit;
        r.priced = bp.has_data;
        r.profit = bp.net();
        r.starved  = sq.starved;
        r.short_of = sq.short_of;
        // ONE WARNING TO A ROW: a starved row already says it would stand idle.
        if (bp.has_data && !sq.starved)
            if (const shelf_runway sr = shelf_runway_for(w, reg, tid, c.type, c.recipe, r.cover); sr.warn)
            {
                r.runway_warn = true;
                r.runway_text = shelf_runway_text(sr);
            }
        r.profit_full_cover = (r.cover >= 1.0f)
            ? r.profit
            : quote_for_tile(w, reg, tid, c.type, c.target, c.recipe, 1.0f).profit.net();

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
            // The price the quote settled at: the forecast the revenue was valued at.
            const float settled = p * sq.price_share;
            if (bp.has_data && settled > 0.0f)
            {
                r.has_yield = true;
                r.yield     = bp.revenue / settled;
            }
        }

        sf.cls[idx] = r.no_logistics ? 2 : 1;
        // The map draws the table's Net (Ben, 2026-11-09), under the grid's own rule: a
        // building that makes nothing to sell has no figure.
        if (r.priced && option_earns(c))
            sf.cell_net[idx] = r.profit;
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

// --- THE BUILDING GRID (Ben, 2026-11-08) -------------------------------------------
// Every option scored over every tile in scope is far more work than the one chosen
// building the table scores, so a pass is SPREAD OVER FRAMES (k_grid_frame_budget_ms,
// resumable between tiles) and never one long frame. A pass for a new scope or body
// shows its cells pending as they fill; a pass for the same scope (the treasury or the
// frame stamp moved) keeps showing the last figures until the fresh set swaps in whole,
// so a running game's grid does not flicker. Under --verify a pass completes in the
// frame it starts, because a wall-clock budget would make a capture depend on the clock.

constexpr double k_grid_frame_budget_ms = 3.0;
constexpr int    k_grid_clock_every     = 8; ///< Tiles between looks at the clock.

struct grid_key
{
    entity_id     body = null_entity;
    lp_stamp      stamp;
    float         balance = -1.0f;
    float         reach   = -2.0f;
    std::uint32_t scope   = 0;
    bool operator==(const grid_key&) const = default;
};

struct grid_pass
{
    grid_key                key;
    bool                    have_key = false;
    bool                    running  = false;
    bool                    provisional = false; ///< Publish each cell as it finishes.
    std::vector<site_cell>  work;
    std::vector<std::map<int, int>> refusals;    ///< Per option: reason -> tiles, for the commonest.
    std::vector<int>        air_refused;
    std::vector<entity_id>  tiles;               ///< Own ground in scope, in grid order.
    std::size_t             option = 0, tile = 0;
    double                  spent_ms = 0.0;
    int                     frames   = 0;
    std::optional<place_index> places;           ///< Built once per pass, not per frame.
};

grid_pass& pass_state()
{
    static grid_pass p;
    return p;
}

bool same_option(const site_option& a, const site_option& b)
{
    return a.type == b.type && a.target == b.target && a.recipe == b.recipe;
}

bool same_options(const std::vector<site_cell>& cells, const std::vector<site_option>& opts)
{
    if (cells.size() != opts.size())
        return false;
    for (std::size_t i = 0; i < opts.size(); ++i)
        if (!same_option(cells[i].option, opts[i]))
            return false;
    return true;
}

/// Only what makes something has an estimated net to compare: a Port's figure would
/// be its upkeep alone, and a fake loss is worse than no figure.
bool option_earns(const site_option& o)
{
    return o.type == building_type::extraction_site || o.type == building_type::processing_facility;
}

/// The drawn order: priced by best Net, descending, then sites with no figure, then
/// options with no site at all; every tie by option order.
void order_cells(site_finder_state& sf)
{
    sf.cell_order.resize(sf.cells.size());
    for (std::size_t i = 0; i < sf.cells.size(); ++i)
        sf.cell_order[i] = static_cast<int>(i);
    const auto tier = [](const site_cell& c) { return c.sites == 0 ? 2 : (c.has_figure ? 0 : 1); };
    std::stable_sort(sf.cell_order.begin(), sf.cell_order.end(), [&](int ia, int ib) {
        const site_cell& a = sf.cells[static_cast<std::size_t>(ia)];
        const site_cell& b = sf.cells[static_cast<std::size_t>(ib)];
        if (tier(a) != tier(b))
            return tier(a) < tier(b);
        return tier(a) == 0 && a.best_net > b.best_net;
    });
}

void finish_cell(grid_pass& p, std::size_t i, bool scope_empty)
{
    site_cell& c = p.work[i];
    c.pending = false;
    if (c.sites > 0)
        return;
    if (c.unaffordable > 0)
    {
        char buf[160];
        std::snprintf(buf, sizeof buf, "%d %s would take it, but the cheapest costs %s cr, more than the treasury holds.",
                      c.unaffordable, c.unaffordable == 1 ? "tile" : "tiles",
                      fmt::abbreviate(static_cast<double>(c.cheapest_unaffordable)).c_str());
        c.refusal = buf;
        return;
    }
    if (scope_empty)
    {
        c.refusal = "None of this province is your ground.";
        return;
    }
    // The commonest refusal; a tie goes to the lower reason, so the sentence is one answer.
    int best_n = p.air_refused[i], best_reason = -1;
    for (const auto& [reason, n] : p.refusals[i])
        if (n > best_n)
        {
            best_n      = n;
            best_reason = reason;
        }
    if (best_n == 0)
        c.refusal = "You hold no ground on this body to build it on.";
    else if (best_reason < 0)
        c.refusal = "The air here cannot run this recipe.";
    else
        c.refusal = placement_rules::placement_reason_text(static_cast<placement_rules::placement_reason>(best_reason));
}

/// Advance the grid's pass by one frame's budget, starting a new pass when its key moves.
void step_site_grid(world& w, const recipe_registry& reg, ui_state& ui, const std::vector<site_option>& opts)
{
    site_finder_state& sf = ui.site_finder;
    grid_pass& p = pass_state();

    const entity_id actor = player_actor(w);
    float balance = 0.0f;
    if (const float* b = purse_of(w, actor); b != nullptr)
        balance = *b;
    const grid_key key{ ui.active_body, make_lp_stamp(w), balance, ui.max_logistics_reach, sf.scope_province };

    const bool options_moved = !same_options(p.running ? p.work : sf.cells, opts);
    if (!p.have_key || !(key == p.key) || options_moved)
    {
        const bool new_view = !p.have_key || p.key.body != key.body || p.key.scope != key.scope
                           || !same_options(sf.cells, opts);
        p.key      = key;
        p.have_key = true;
        p.running  = true;
        p.option   = 0;
        p.tile     = 0;
        p.spent_ms = 0.0;
        p.frames   = 0;
        p.work.assign(opts.size(), site_cell{});
        for (std::size_t i = 0; i < opts.size(); ++i)
            p.work[i].option = opts[i];
        p.refusals.assign(opts.size(), {});
        p.air_refused.assign(opts.size(), 0);

        p.tiles.clear();
        if (const auto bit = w.bodies.find(ui.active_body);
            bit != w.bodies.end() && bit->second.grid_width > 0 && bit->second.grid_height > 0)
        {
            for (const entity_id tid : body_tile_grid(w, ui.active_body))
            {
                if (tid == null_entity)
                    continue;
                const auto nit = w.tile_to_nation.find(tid);
                if (nit == w.tile_to_nation.end() || nit->second != actor)
                    continue;
                if (sf.scope_province != 0 && w.provinces.province_of(tid) != sf.scope_province)
                    continue;
                p.tiles.push_back(tid);
            }
        }
        sf.scope_foreign = sf.scope_province != 0 && p.tiles.empty();

        {
            const auto gw_it = w.bodies.find(ui.active_body);
            p.places.emplace(w, ui.active_body, gw_it != w.bodies.end() ? gw_it->second.grid_width : 0);
        }
        p.provisional = new_view;
        if (new_view)
        {
            sf.cells = p.work; // every cell pending; order is the options' until settled
            sf.cell_order.resize(sf.cells.size());
            for (std::size_t i = 0; i < sf.cells.size(); ++i)
                sf.cell_order[i] = static_cast<int>(i);
        }
    }
    if (!p.running)
        return;

    const auto t0 = std::chrono::steady_clock::now();
    const auto elapsed = [&]() {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    };
    const bool to_completion = ui.fixed_frame_clock;
    ++p.frames;

    int since_clock = 0;
    bool out_of_budget = false;
    while (p.option < p.work.size() && !out_of_budget)
    {
        site_cell& cell = p.work[p.option];
        const site_option& o = cell.option;
        const recipe* rc = (o.type == building_type::processing_facility) ? reg.get_recipe(o.recipe) : nullptr;
        const placement_gate gate = reg.placement_gate_for(o.type, o.recipe);
        const bool earns = option_earns(o);

        while (p.tile < p.tiles.size())
        {
            const entity_id tid = p.tiles[p.tile++];
            const site_gate_result sg = gate_site(w, reg, o, rc, gate, tid, ui.max_logistics_reach, actor, balance);
            if (sg.verdict == site_verdict::air)
                ++p.air_refused[p.option];
            else if (sg.verdict == site_verdict::refused)
                ++p.refusals[p.option][static_cast<int>(sg.why)];
            else if (sg.verdict == site_verdict::unaffordable)
            {
                if (cell.unaffordable == 0 || sg.capex < cell.cheapest_unaffordable)
                    cell.cheapest_unaffordable = sg.capex;
                ++cell.unaffordable;
            }
            else
            {
                ++cell.sites;
                // THE ROW'S OWN FIGURE: the same cover and the same quote the table asks for.
                const float cover = site_lp_cover(w, tid);
                if (!(cover > 0.0f))
                    ++cell.no_lp;
                bool priced = false;
                long net    = 0;
                if (earns)
                {
                    const site_quote sq = quote_for_tile(w, reg, tid, o.type, o.target, o.recipe, cover);
                    priced = sq.profit.has_data;
                    net    = priced ? std::lround(sq.profit.net()) : 0;
                    if (sq.profit.has_data && (!cell.has_figure || sq.profit.net() > cell.best_net))
                    {
                        const tile_component& tc = w.tiles.at(tid);
                        cell.has_figure   = true;
                        cell.best_net     = sq.profit.net();
                        cell.best_cover   = cover;
                        cell.best_starved = sq.starved;
                        cell.best_place   = p.places->name(tid, tc.grid_x, tc.grid_y, cell.best_near);
                    }
                }
                // The Construct press's site: highest Net, then cover, then lowest id.
                const bool better =
                    cell.build_tile == null_entity
                    || (priced != cell.build_priced ? priced
                        : net != cell.build_net     ? net > cell.build_net
                        : cover != cell.build_cover ? cover > cell.build_cover
                                                    : tid < cell.build_tile);
                if (better)
                {
                    cell.build_tile   = tid;
                    cell.build_priced = priced;
                    cell.build_net    = net;
                    cell.build_cover  = cover;
                }
            }
            if (!to_completion && ++since_clock >= k_grid_clock_every)
            {
                since_clock = 0;
                if (elapsed() >= k_grid_frame_budget_ms)
                {
                    out_of_budget = true;
                    break;
                }
            }
        }
        if (p.tile >= p.tiles.size())
        {
            finish_cell(p, p.option, sf.scope_province != 0 && p.tiles.empty());
            if (p.provisional)
                sf.cells[p.option] = p.work[p.option];
            ++p.option;
            p.tile = 0;
        }
    }
    p.spent_ms += elapsed();

    if (p.option >= p.work.size())
    {
        p.running = false;
        sf.cells  = std::move(p.work);
        p.work.clear();
        order_cells(sf);
        sf.grid_ms     = p.spent_ms;
        sf.grid_frames = p.frames;
    }
}

/// Where the finder is looking (Ben, 2026-11-08): the selected tile's province, unless
/// that tile was selected by a press on the table, which must not narrow its own list.
/// Read off the selection itself, not the canvas's mirror, per SELECTION.md.
void resolve_scope(const world& w, ui_state& ui)
{
    site_finder_state& sf = ui.site_finder;
    if (sf.row_pressed_tile != null_entity && ui.selected_entity == sf.row_pressed_tile)
        return;
    sf.row_pressed_tile = null_entity;
    std::uint32_t pid = 0;
    if (const auto tit = w.tiles.find(ui.selected_entity);
        tit != w.tiles.end() && tit->second.body == ui.active_body)
        pid = w.provinces.province_of(ui.selected_entity);
    if (pid == sf.scope_province)
        return;
    sf.scope_province = pid;
    sf.scope_name.clear();
    sf.scope_near = false;
    if (pid != 0)
    {
        const tile_component& tc = w.tiles.at(ui.selected_entity);
        const auto bit = w.bodies.find(ui.active_body);
        const place_index places(w, ui.active_body, bit != w.bodies.end() ? bit->second.grid_width : 0);
        sf.scope_name = places.name(ui.selected_entity, tc.grid_x, tc.grid_y, sf.scope_near);
    }
}

/// The grid: one cell per option, each its glyph, name, best Net and site count.
void draw_site_grid(const world& w, const recipe_registry& reg, ui_state& ui)
{
    site_finder_state& sf = ui.site_finder;
    if (sf.cells.empty())
        return;
    const ImGuiStyle& st = ImGui::GetStyle();
    const float lh    = ImGui::GetTextLineHeight();
    const float pad   = 4.0f;
    const float gap   = 3.0f;
    const float glyph = std::ceil(lh * 1.5f);
    const float avail = std::max(120.0f, ImGui::GetContentRegionAvail().x - st.ScrollbarSize);

    // THE COLUMN COUNT IS MEASURED: the most columns of at least the minimum width at
    // which every name wraps whole inside its cell (a word wider than the room would
    // elide). The cell is then as tall as the tallest wrapped name needs, so no name
    // is cut at any supported width.
    constexpr int   k_max_cols   = 4;
    constexpr float k_min_cell_w = 124.0f;
    const auto cell_width = [&](int n) {
        return (avail - gap * static_cast<float>(n - 1)) / static_cast<float>(n);
    };
    const auto room_of = [&](float cw) { return cw - pad * 2.0f - glyph - st.ItemInnerSpacing.x; };
    int cols = 1;
    for (int n = k_max_cols; n >= 2; --n)
    {
        if (cell_width(n) < k_min_cell_w)
            continue;
        const float room = room_of(cell_width(n));
        bool fits = true;
        for (const site_cell& c : sf.cells)
            if (ImGui::CalcTextSize(c.option.name.c_str(), nullptr, false, room).x > room + 0.5f)
            {
                fits = false;
                break;
            }
        if (fits)
        {
            cols = n;
            break;
        }
    }
    const float cell_w    = cell_width(cols);
    const float name_room = room_of(cell_w);
    float name_h = glyph;
    for (const site_cell& c : sf.cells)
        name_h = std::max(name_h, ImGui::CalcTextSize(c.option.name.c_str(), nullptr, false, name_room).y);
    const float cell_h = std::ceil(name_h + lh) + pad * 2.0f + 2.0f;

    // Height: what is left once the table has the room it needs. The tab is the finder's
    // alone (Ben, 2026-11-09). A part row shows that it scrolls.
    const int   rows = (static_cast<int>(sf.cells.size()) + cols - 1) / cols;
    const float row_step = cell_h + gap;
    constexpr float k_table_reserve = 260.0f;  // the site ledger's caption and table
    const float reserve = 30.0f + (sf.armed ? k_table_reserve : 0.0f);
    const float room_rows = (ImGui::GetContentRegionAvail().y - reserve) / row_step;
    const float want_rows = std::max(room_rows, 1.5f);
    const float child_h = std::min(static_cast<float>(rows), want_rows) * row_step + gap;

    if (!ImGui::BeginChild("##site_grid", {0.0f, child_h}, 0, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        ImGui::EndChild();
        return;
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImU32 text_ink = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 dim_ink  = ImGui::GetColorU32(ImGuiCol_TextDisabled);

    const int n_cells = static_cast<int>(sf.cell_order.size());
    int chosen_at = -1;
    for (int at = 0; at < n_cells; ++at)
    {
        const int idx = sf.cell_order[static_cast<std::size_t>(at)];
        const site_cell& c = sf.cells[static_cast<std::size_t>(idx)];
        const site_option& o = c.option;
        const int col = at % cols, row = at / cols;
        const ImVec2 mn{origin.x + static_cast<float>(col) * (cell_w + gap),
                        origin.y + static_cast<float>(row) * row_step};
        const ImVec2 mx{mn.x + cell_w, mn.y + cell_h};
        const bool dim       = !c.pending && c.sites == 0;
        const bool is_choice = sf.armed && same_option(sf.choice, o);
        if (is_choice)
            chosen_at = at;

        ImGui::SetCursorScreenPos(mn);
        ImGui::PushID(idx);
        ImGui::SetNextItemAllowOverlap(); // the Construct press sits over the cell's
        bool pressed = ImGui::InvisibleButton("##cell", {cell_w, cell_h});
        const bool hovered = ImGui::IsItemHovered();
        ImGui::PopID();

        dl->AddRectFilled(mn, mx, is_choice ? IM_COL32(60, 90, 70, 200)
                                            : ImGui::GetColorU32(ImGuiCol_Text, hovered ? 0.16f : 0.07f), 3.0f);
        if (is_choice)
            dl->AddRect(mn, mx, palette::selection, 3.0f);

        const float gr = glyph * 0.5f;
        const recipe* orc = (o.type == building_type::processing_facility) ? reg.get_recipe(o.recipe) : nullptr;
        icons::building(dl, {mn.x + pad + gr, mn.y + pad + gr}, gr, o.type,
                        orc != nullptr ? primary_output_resource(*orc) : o.target,
                        dim ? IM_COL32(150, 235, 160, 90) : IM_COL32(150, 235, 160, 255));
        const ImVec2 name_at{mn.x + pad + glyph + st.ItemInnerSpacing.x, mn.y + pad};
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), name_at, dim ? dim_ink : text_ink, o.name.c_str(),
                    nullptr, name_room); // fit-exempt: the column count is chosen so it wraps whole

        // THE LOWER LINE, HALVED (Ben, 2026-11-10): the credits and the site count on the
        // left half, a Construct press on the right that builds at the cell's best site.
        // The line reads a step smaller than the name so the pair fits its half; the
        // count stands at the half's right edge and the credits are clipped short of it,
        // never run into it (the "truncate" of the ruling).
        const float   fig_y  = mn.y + cell_h - pad - lh;
        const float   half_x = mn.x + cell_w * 0.5f;
        const float   small  = ImGui::GetFontSize() * 0.85f;
        const float   small_dy = (lh - small) * 0.5f;
        ImFont* const font   = ImGui::GetFont();
        char count_text[32] = "";
        float count_x = half_x - st.ItemInnerSpacing.x;
        if (!c.pending)
        {
            std::snprintf(count_text, sizeof count_text, "%d", c.sites);
            count_x -= font->CalcTextSizeA(small, FLT_MAX, 0.0f, count_text).x;
            dl->AddText(font, small, {count_x, fig_y + small_dy}, dim_ink, count_text); // fit-exempt: measured above
        }
        const ImVec4 net_clip{mn.x + pad, fig_y - 2.0f, count_x - 3.0f, mx.y};
        const ImVec2 net_at{mn.x + pad, fig_y + small_dy};
        std::string net_text;
        if (c.pending)
            dl->AddText(font, small, net_at, dim_ink, "...");
        else if (c.has_figure)
        {
            net_text = "Cr " + fmt::abbreviate(static_cast<double>(c.best_net));
            dl->AddText(font, small, net_at, value_colour(static_cast<double>(c.best_net)), net_text.c_str(),
                        nullptr, 0.0f, &net_clip); // fit-exempt: clipped short of the count
        }

        // The press: a real button over the cell's right half, so it takes the press
        // and the hover before the cell under it.
        const bool can_build = !c.pending && c.sites > 0 && c.build_tile != null_entity;
        const ImVec2 b_mn{half_x + st.ItemInnerSpacing.x * 0.5f, fig_y - 2.0f};
        const ImVec2 b_mx{mx.x - pad, mx.y - 2.0f};
        ImGui::SetCursorScreenPos(b_mn);
        ImGui::PushID(idx);
        ImGui::BeginDisabled(!can_build);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {1.0f, 0.0f});
        ImGui::SetWindowFontScale(0.85f); // the label reads at the line's size
        const bool build_pressed = ImGui::Button("Construct##cell_build", {b_mx.x - b_mn.x, b_mx.y - b_mn.y});
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopStyleVar();
        ImGui::EndDisabled();
        const bool build_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
        ImGui::PopID();
        if (build_hovered)
            pressed = false; // the press was the button's
        if (build_hovered)
        {
            std::string why;
            if (c.pending)
                why = "Still being priced.";
            else if (can_build)
            {
                const tile_component& bt = w.tiles.at(c.build_tile);
                char buf[200];
                std::snprintf(buf, sizeof buf,
                              "Construct %s at its best site, tile [%d, %d]: the highest Net, then the most "
                              "Logistic Point cover%s.",
                              o.name.c_str(), bt.grid_x, bt.grid_y,
                              sf.scope_province != 0 ? ", in this province" : "");
                why = buf;
            }
            else if (c.unaffordable > 0)
                why = "Can't afford it: every site costs more than the treasury holds.";
            else
                why = c.refusal.empty() ? std::string("No tile of your ground takes it.") : c.refusal;
            ImGui::SetTooltip("%s", why.c_str());
        }
        if (build_pressed && can_build)
        {
            // The map press's path: app::render runs the same construct_nation the Build
            // door does, with its own treasury test.
            ui.construction.pending_tile   = c.build_tile;
            ui.construction.pending_type   = o.type;
            ui.construction.pending_target = o.target;
            ui.construction.pending_recipe = o.recipe;
            ui.construction.last_message.clear();
        }

        // EACH ELEMENT EXPLAINS ITSELF (construction.md): the figure, the count and the
        // press each carry their own card; the first rect the pointer is in wins.
        std::vector<grid_explainer> cards;
        const auto card = [&](const char* key, ImVec2 a, ImVec2 b, std::string text, ImU32 ink = 0u) {
            grid_explainer e;
            e.key = key; e.mn = a; e.mx = b; e.text = std::move(text); e.ink = ink;
            cards.push_back(std::move(e));
        };
        if (c.pending)
            card("cell", mn, mx, "Still being priced.");
        else if (dim)
            card("cell", mn, mx, c.refusal + (is_choice ? " Press to clear it."
                                                        : " Press to see it on the map anyway."));
        else
        {
            const float mid = half_x - st.ItemInnerSpacing.x - std::max(ImGui::CalcTextSize(count_text).x, 16.0f);
            char count_card[220];
            int  n = std::snprintf(count_card, sizeof count_card, "%d %s of your ground %s take it.", c.sites,
                                   c.sites == 1 ? "tile" : "tiles",
                                   sf.scope_province != 0 ? "in this province" : "on this body");
            if (c.no_lp > 0)
                n += std::snprintf(count_card + n, sizeof count_card - n,
                                   " %d with no Logistic Points operating.", c.no_lp);
            if (c.unaffordable > 0)
                std::snprintf(count_card + n, sizeof count_card - n,
                              " %d more cost more than the treasury.", c.unaffordable);
            card("count", {mid, fig_y - 1.0f}, {half_x, mx.y}, count_card);
            if (c.has_figure)
            {
                char buf[240];
                const std::string where = c.best_place.empty() ? std::string("unnamed ground")
                                        : c.best_near          ? "near " + c.best_place
                                                               : c.best_place;
                if (c.best_starved)
                    std::snprintf(buf, sizeof buf,
                                  "The best site, %s, would likely stand idle: its Est. net %+d cr / qtr is upkeep alone.",
                                  where.c_str(), static_cast<int>(std::lround(c.best_net)));
                else
                    std::snprintf(buf, sizeof buf,
                                  "Est. net %+d cr / qtr at the best site, %s, at %d%% Logistic Point cover.",
                                  static_cast<int>(std::lround(c.best_net)), where.c_str(),
                                  static_cast<int>(std::lround(c.best_cover * 100.0f)));
                card("net", {mn.x, fig_y - 1.0f}, {mid, mx.y}, buf, value_colour(static_cast<double>(c.best_net)));
            }
            else
                card("net", {mn.x, fig_y - 1.0f}, {mid, mx.y},
                     option_earns(o) ? "No market prices its sites, so there is no estimate."
                                     : "It makes nothing to sell, so it has no estimated net.");
            card("name", mn, mx, is_choice ? "Press to clear it."
                                           : "Press to list its sites and mark them on the map, then press a "
                                             "marked tile to build there.");
        }
        if (!build_hovered) // the Construct press carries its own card
            construction_column_card(o.name.c_str(), cards);

        sf.picker_rows.push_back({o.name, o.category, gr,
                                  ImGui::CalcTextSize(o.name.c_str(), nullptr, false, name_room).x, name_room,
                                  (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f});
        if (at == 0)
        {
            sf.picker_x = (mn.x + mx.x) * 0.5f;
            sf.picker_y = (mn.y + mx.y) * 0.5f;
        }
        if (pressed)
        {
            if (is_choice)
                sf.armed = false; // the toggle: the chosen cell, pressed again, clears
            else
            {
                sf.armed  = true;
                sf.choice = o;
            }
            ui.construction.last_message.clear(); // a verdict for the previous choice
        }
    }
    // Reserve the grid's own extent so the child's scroll range is right.
    ImGui::SetCursorScreenPos({origin.x, origin.y + static_cast<float>(rows) * row_step});
    ImGui::Dummy({1.0f, 0.0f});

    // A choice made elsewhere (the Military ledger's door) may sit off the visible part.
    static std::string framed;
    const std::string want = (sf.armed && chosen_at >= 0) ? sf.choice.name : std::string();
    if (want != framed)
    {
        framed = want;
        if (chosen_at >= 0)
        {
            const float top = static_cast<float>(chosen_at / cols) * row_step;
            const float now = ImGui::GetScrollY();
            if (top < now || top + cell_h > now + ImGui::GetWindowHeight())
                ImGui::SetScrollY(top);
        }
    }
    ImGui::EndChild();
}

} // namespace

const char* construction_fold_of(const recipe_registry& reg, building_type t, std::uint16_t recipe_id)
{
    switch (t)
    {
    case building_type::extraction_site:     return "Extraction";
    case building_type::processing_facility:
        // Power Generation is infrastructure (Ben, 2026-11-05; SELECTION.md § The tile
        // construction ledger): a grid is what other plants stand on.
        if (const recipe* rc = reg.get_recipe(recipe_id); rc != nullptr && rc->group == "Power Generation")
            return "Infrastructure";
        return "Processing";
    case building_type::military_base:
    case building_type::airbase:
    case building_type::bunker:              // BL-1426
    case building_type::research_institute:  return "Military";
    default:                                 return "Infrastructure";
    }
}

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
                           construction_fold_of(reg, building_type::extraction_site, no_recipe)});
    // Processing: one per RECIPE in the era-masked browse order the door walks, with
    // the door's unlock filter.
    if (reg.building_available(building_type::processing_facility))
        for (int ri = 0; ri < reg.play_recipe_count(building_type::processing_facility); ++ri)
        {
            const recipe& browsed = reg.play_recipe_at(building_type::processing_facility, ri);
            const std::uint16_t id = reg.recipe_id(browsed.name);
            if (!recipe_offered(w, reg, actor, id))
                continue;
            out.push_back({building_type::processing_facility, resource_type::iron_ore, id,
                           browsed.display_name,
                           construction_fold_of(reg, building_type::processing_facility, id)});
        }
    const std::pair<building_type, const char*> fixed[] = {
        {building_type::port, "Port"},
        {building_type::planetary_marketplace, "Planetary Marketplace"},
        {building_type::launchpad, "Launchpad"},
        {building_type::inland_logistics_hub, "Inland Logistics Hub"},
        {building_type::military_base, "Military Base"},
        {building_type::airbase, "Airbase"},
        {building_type::bunker, "Bunker"}, // BL-1426
        {building_type::research_institute, "Research Institute"},
        {building_type::schooling, "Schooling"},   // the stratum gate decides where (Ben, 2026-11-07)
        {building_type::university, "University"},
    };
    for (const auto& [t, name] : fixed)
        if (reg.building_available(t))
            out.push_back({t, resource_type::iron_ore, no_recipe, name, construction_fold_of(reg, t, no_recipe)});
    // The door's fold order: category, then name; stable so equal names keep the
    // authored order.
    std::stable_sort(out.begin(), out.end(), [](const site_option& a, const site_option& b) {
        if (a.category != b.category)
            return a.category < b.category;
        return a.name < b.name;
    });
    return out;
}

void set_quote_report(const economy_report* report)
{
    quote_report() = report;
}

void set_quote_history(const market_plot_history* history)
{
    quote_history() = history;
}

void invalidate_site_evaluation()
{
    quotes() = quote_cache{};
    last_key() = eval_key{};
}

shelf_runway shelf_runway_for(const world& w, const recipe_registry& reg, entity_id tile,
                              building_type type, std::uint16_t recipe_id, float lp_cover)
{
    shelf_runway out;
    const recipe* rc = (type == building_type::processing_facility) ? reg.get_recipe(recipe_id) : nullptr;
    const float cover = std::clamp(lp_cover, 0.0f, 1.0f);
    const entity_id mid = market_for_tile(w, tile);
    const auto mit = (mid != null_entity) ? w.markets.find(mid) : w.markets.end();
    if (rc == nullptr || !(cover > 0.0f) || mit == w.markets.end())
        return out;

    // Cached with the quote, on its stamp, so the door and the finder stay stale together.
    quote_cache& qc = quotes();
    const lp_stamp now = make_lp_stamp(w);
    if (!(now == qc.stamp))
    {
        qc.stamp = now;
        qc.at.clear();
        qc.runway.clear();
        qc.cx.reset();
    }
    const auto key = std::make_tuple(mid, static_cast<int>(recipe_id), cover);
    if (const auto it = qc.runway.find(key); it != qc.runway.end())
        return it->second;

    // The staffing the quote judges a new plant at, so the use is the plant it priced.
    building_component bc{};
    bc.tile               = tile;
    bc.type               = type;
    bc.recipe             = recipe_id;
    bc.workforce_assigned = construction_initial_workforce(type);
    bc.workforce_target   = default_workforce_target(type);
    const float batches   = judged_batches(reg, bc) * cover;

    const market_component& m = mit->second;
    const std::array<resource_plot_series, resource_count>* trail = nullptr;
    if (const market_plot_history* h = quote_history(); h != nullptr)
        if (const auto hit = h->find(mid); hit != h->end())
            trail = &hit->second;

    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float use = rc->inputs[r] * batches;
        if (!(use > 0.0f) || grid_good_crosses_markets(r))
            continue;
        bool  known = false;
        float drain = 0.0f;
        if (trail != nullptr)
        {
            const std::vector<float>& s = (*trail)[r].shelf;
            if (s.size() >= 2)
            {
                const std::size_t span = std::min<std::size_t>(s.size() - 1, k_shelf_trail_quarters);
                known = true;
                drain = std::max(0.0f, (s[s.size() - 1 - span] - s.back()) / static_cast<float>(span));
            }
        }
        const float shelf    = std::max(0.0f, m.inventory[r]);
        const float quarters = shelf / (use + drain);
        if (!(quarters < k_shelf_runway_warn_quarters))
            continue;
        if (!out.warn || quarters < out.quarters)
        {
            out.others     += out.warn ? 1 : 0;
            out.warn        = true;
            out.good        = static_cast<resource_type>(r);
            out.quarters    = quarters;
            out.shelf       = shelf;
            out.plant_use   = use;
            out.drain_known = known;
            out.drain       = drain;
        }
        else
            ++out.others;
    }
    qc.runway.emplace(key, out);
    return out;
}

std::string shelf_runway_text(const shelf_runway& s)
{
    if (!s.warn)
        return std::string();
    const std::string good = resource_name(s.good);
    char lead[120];
    if (s.quarters < 1.0f)
        std::snprintf(lead, sizeof lead, "Under a quarter of %s left on this market's shelf", good.c_str());
    else if (s.quarters < 2.0f)
        std::snprintf(lead, sizeof lead, "Only about 1 quarter of %s left on this market's shelf", good.c_str());
    else
        std::snprintf(lead, sizeof lead, "Only about %.0f quarters of %s left on this market's shelf",
                      std::floor(s.quarters), good.c_str());
    char tail[160];
    if (s.drain > 0.0f)
        std::snprintf(tail, sizeof tail, ": it would use %.1f a quarter, and the shelf is already falling %.1f.",
                      static_cast<double>(s.plant_use), static_cast<double>(s.drain));
    else if (s.drain_known)
        std::snprintf(tail, sizeof tail, " at the %.1f a quarter it would use.", static_cast<double>(s.plant_use));
    else // An unread trail is not a flat shelf, so the sentence does not read as one.
        std::snprintf(tail, sizeof tail,
                      " at the %.1f a quarter it would use. That is its own use alone: this shelf has no "
                      "trail read yet, so a fall would not show.", static_cast<double>(s.plant_use));
    std::string out = std::string(lead) + tail;
    if (s.others > 0)
        out += s.others == 1 ? " One more input is close too." : " More inputs are close too.";
    return out;
}

site_quote quote_for_tile(world& w, const recipe_registry& reg, entity_id tile, building_type type,
                          resource_type target, std::uint16_t recipe, float lp_cover)
{
    const economy_report* report = quote_report();
    quote_cache& qc = quotes();
    const lp_stamp now = make_lp_stamp(w);
    if (!(now == qc.stamp) || !qc.cx.has_value())
    {
        if (!(now == qc.stamp))
            qc.runway.clear();
        qc.stamp = now;
        qc.at.clear();
        qc.cx.emplace(make_site_quote_context(w, reg, report));
    }
    const auto key = std::make_tuple(tile, static_cast<int>(type), static_cast<int>(target),
                                     static_cast<int>(recipe), lp_cover);
    if (const auto it = qc.at.find(key); it != qc.at.end())
        return it->second;
    site_quote q = quote_site(w, reg, *qc.cx, tile, type, target, recipe, lp_cover);
    qc.at.emplace(key, q);
    return q;
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
                        sf.choice.recipe, balance, ui.max_logistics_reach, sf.scope_province };
    if (!(key == last_key()) || sf.body != ui.active_body)
    {
        last_key() = key;
        evaluate(w, reg, ui);
    }
    sf.live = (ui.primary_level == canvas_level::planetary) && sf.body == ui.active_body;
}

std::string site_press_refusal(const world& w, const recipe_registry& reg, const ui_state& ui, entity_id tile)
{
    const site_finder_state& sf = ui.site_finder;
    const site_option&       o  = sf.choice;
    if (sf.scope_province != 0 && w.provinces.province_of(tile) != sf.scope_province)
        return "That is outside the selected province. Clear the selection to search all your ground.";
    const entity_id actor = player_actor(w);
    float balance = 0.0f;
    if (const float* p = purse_of(w, actor); p != nullptr)
        balance = *p;
    const recipe* rc = (o.type == building_type::processing_facility) ? reg.get_recipe(o.recipe) : nullptr;
    const site_gate_result sg = gate_site(w, reg, o, rc, reg.placement_gate_for(o.type, o.recipe), tile,
                                          ui.max_logistics_reach, actor, balance);
    char buf[160];
    switch (sg.verdict)
    {
    case site_verdict::air:
        return "The air here cannot run this recipe.";
    case site_verdict::refused:
        return placement_rules::placement_reason_text(sg.why);
    case site_verdict::unaffordable:
        std::snprintf(buf, sizeof buf, "Can't afford it: the press charges %d cr and the balance is short.",
                      static_cast<int>(std::lround(sg.capex)));
        return buf;
    default:
        return std::string();
    }
}

void draw_site_finder(world& w, const recipe_registry& reg, ui_state& ui)
{
    site_finder_state& sf = ui.site_finder;
    sf.drawn.clear();
    sf.picker_rows.clear();
    sf.picker_x = sf.picker_y = -1.0f;

    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Find a site");

    // THE SCOPE LINE (Ben, 2026-11-08): the search is the selected tile's province, or
    // the whole body; the line names which, and clears it.
    resolve_scope(w, ui);
    if (sf.scope_province != 0)
    {
        const std::string where = sf.scope_name.empty() ? std::string("This province")
                                : sf.scope_near         ? "The province near " + sf.scope_name
                                                        : "Province of " + sf.scope_name;
        const std::string text = sf.scope_foreign ? where + ": none of it is your ground" : where;
        const float clear_w = ImGui::CalcTextSize("Clear").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float start_x = ImGui::GetCursorPosX();
        const float room    = ImGui::GetContentRegionAvail().x;
        ImGui::PushTextWrapPos(start_x + room - clear_w - 6.0f);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
        grid_explainer e;
        e.key  = "scope";
        e.mn   = ImGui::GetItemRectMin();
        e.mx   = ImGui::GetItemRectMax();
        e.text = "The buildings, their figures, the table and the map consider only this province's "
                 "ground. It follows the selected tile.";
        construction_column_card("Province", {std::move(e)});
        ImGui::SameLine(start_x + room - clear_w);
        if (ImGui::SmallButton("Clear##site_scope"))
        {
            clear_selection_to_resting(ui);
            sf.row_pressed_tile = null_entity;
            resolve_scope(w, ui);
        }
        if (ImGui::IsItemHovered())
            construction_row_card("Clear", [] {
                ImGui::TextDisabled("Clears the selected tile, so the search covers all your ground on this body.");
            });
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        ImGui::TextUnformatted("All your ground on this body");
        ImGui::PopStyleColor();
        grid_explainer e;
        e.key  = "scope";
        e.mn   = ImGui::GetItemRectMin();
        e.mx   = ImGui::GetItemRectMax();
        e.text = "Select a tile to narrow the search to its province.";
        construction_column_card("Scope", {std::move(e)});
    }

    // THE BUILDING GRID (Ben, 2026-11-08): every option's best estimated net, before
    // any is chosen. A selector, not a state: the chosen cell is the target the finder
    // prices, and pressing it again clears it, so the visible choice is a toggle.
    const std::vector<site_option> opts = site_options(w, reg);
    step_site_grid(w, reg, ui, opts);
    draw_site_grid(w, reg, ui);

    update_site_finder(w, reg, ui);
    if (!sf.armed)
        return;

    // The last map press's verdict: what the door says, here where the press was aimed.
    if (!ui.construction.last_message.empty())
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::neutral), "%s",
                           ui.construction.last_message.c_str());

    // The counts the table cannot show. The assumptions are the Net header's card.
    ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    if (sf.rows.empty())
        ImGui::TextWrapped("No tile of your ground %s takes it.",
                           sf.scope_province != 0 ? "in this province" : "on this body");
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
    constexpr float k_runway_mark_w = 8.0f; // the amber mark beside a warned row's figure
    for (const site_row& r : sf.rows)
    {
        if (r.priced)
            fig_net = std::max(fig_net, ImGui::CalcTextSize(fmt::abbreviate(static_cast<double>(r.profit)).c_str()).x
                                            + (r.runway_warn ? k_runway_mark_w : 0.0f));
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
        if (ci == 1) // the assumptions every Net in the column shares (Ben, 2026-11-09)
        {
            grid_explainer e;
            e.key  = "net_header";
            e.mn   = ImGui::GetItemRectMin();
            e.mx   = ImGui::GetItemRectMax();
            e.text = "Est. net / qtr at today's local prices, at 50% staffing, no labour shortage and "
                     "each tile's Logistic Point cover.";
            construction_column_card("Net", {std::move(e)});
        }
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
            // it. A press on the map places the building.
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
                     + "Press to select the tile and frame it on the map; press the tile "
                       "on the map to build there.");
            char label[32];
            std::snprintf(label, sizeof label, "##site%u", static_cast<unsigned>(r.tile));
            if (ImGui::Selectable(label, ui.selected_entity == r.tile,
                                  ImGuiSelectableFlags_SpanAllColumns))
            {
                ui.selected_entity          = r.tile;
                ui.selected_province        = w.provinces.province_of(r.tile);
                ui.province_sync_entity     = r.tile;
                sf.row_pressed_tile         = r.tile; // selects it, without narrowing this list
                ui.planetary_center_col     = r.x;
                ui.planetary_center_row     = r.y;
                ui.planetary_center_pending = true;
            }
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(fitted(place.c_str(), pw).c_str());
            sf.drawn.push_back({r.tile, rp.x + 12.0f, rp.y + ImGui::GetTextLineHeight() * 0.5f, std::string()});

            // Net / qtr — the door's estimate, cover applied.
            ImGui::TableSetColumnIndex(1);
            {
                char buf[240];
                if (!r.priced)
                    std::snprintf(buf, sizeof buf, "No market prices this tile, so there is no estimate.");
                else if (r.starved)
                    std::snprintf(buf, sizeof buf,
                                  "It would likely stand idle: this market cannot get it %s at a price it "
                                  "will pay. Est. net %+d cr / qtr is its upkeep alone.",
                                  resource_name(r.short_of),
                                  static_cast<int>(std::lround(r.profit)));
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
                                  "Est. net %+d cr / qtr once its output is selling here, fully served.",
                                  static_cast<int>(std::lround(r.profit)));
                const ImU32 ink = r.priced ? value_colour(static_cast<double>(r.profit)) : 0u;
                const std::string net_text = r.runway_warn ? r.runway_text + " " + buf : std::string(buf);
                // A warning reads amber on its card, however good the net beneath it is.
                cell("profit", w_net, net_text,
                     r.runway_warn ? ImGui::ColorConvertFloat4ToU32(construction_rate_colour(0.5f)) : ink);
                if (r.priced)
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(ink), "%s",
                                       fmt::abbreviate(static_cast<double>(r.profit)).c_str());
                else
                    ImGui::TextDisabled("-");
                if (r.runway_warn)
                {
                    const ImVec2 fm = ImGui::GetItemRectMax();
                    const float  lh = ImGui::GetTextLineHeight();
                    icons::value_mark(dl, {fm.x + k_runway_mark_w * 0.5f, fm.y - lh * 0.5f}, 2.5f,
                                      ImGui::ColorConvertFloat4ToU32(construction_rate_colour(0.5f)));
                }
                sf.drawn.back().net_text = net_text;
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
