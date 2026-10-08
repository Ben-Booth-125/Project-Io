#include "building_state.hpp"

#include "presentation.hpp"
#include "world/economy_system.hpp"  // economy_report / building_report
#include "world/placement_rules.hpp" // is_depositless_site — an idle Well/Wharf is labour-short (BL-1198/1199)
#include "world/recipe_registry.hpp" // price_band — the fair-price ceiling multiple
#include "world/world.hpp"           // pool_key_for_tile, find_pool

#include <cstdio>

namespace ui {

namespace {

// The economy tick's own row for this building, via the per-tick index (BL-360),
// falling back to a scan for a hand-built report that never filled it — the
// same lookup estimate_building_profit makes.
const building_report* report_row(const economy_report* report, entity_id id)
{
    if (report == nullptr)
        return nullptr;
    if (const auto rit = report->building_row.find(id);
        rit != report->building_row.end() && rit->second < report->buildings.size())
        return &report->buildings[rit->second];
    if (report->building_row.empty())
        for (const building_report& r : report->buildings)
            if (r.building == id)
                return &r;
    return nullptr;
}

// Why the limiting input is short, from the two facts the production tick tested
// (economy_system.cpp run_processing): the market shelf, and the fair-price
// ceiling over it. A good on the shelf that the ceiling refused is "over the
// ceiling"; an empty shelf is "none on the shelf". "No producer in reach" is NOT
// derived here: it needs the input_reach producer index (a logistics walk), which
// is a pass over every building and too dear to run per frame on a hover.
input_short_cause input_cause_of(const world& w, const recipe_registry* reg,
                                 const building_component& b, resource_type r)
{
    const entity_id key = pool_key_for_tile(w, b.tile);
    const auto      mit = w.markets.find(key);
    if (mit == w.markets.end())
        return input_short_cause::unknown; // market-less body: no shelf to read
    const market_component& mc = mit->second;
    const std::size_t       ri = static_cast<std::size_t>(r);
    const float res_mult = (reg != nullptr) ? reg->price_band().reservation_mult : 0.0f;
    if (mc.inventory[ri] > 1e-4f)
    {
        if (reg != nullptr && !shelf_admits(mc, ri, res_mult, /*off_buys=*/true))
            return input_short_cause::over_ceiling;
        return input_short_cause::unknown; // on the shelf and admitted: just not enough
    }
    return input_short_cause::none_on_shelf;
}

// The pre-tick fallback: no row for this building yet, so read its staffing —
// the heuristic `hover_building_detail` always used.
building_running_state classify_unreported(const world& w, const building_component& b)
{
    building_running_state s;
    if (b.workforce_assigned < 0.1f)
    {
        s.kind   = building_run_kind::idle;
        s.reason = building_run_reason::labour_short;
        if (b.type == building_type::extraction_site)
        {
            const auto tile_it = w.tiles.find(b.tile);
            if (tile_it != w.tiles.end())
            {
                const float dep = tile_it->second
                    .resource_deposit[static_cast<std::size_t>(b.target_resource)];
                // BL-1198/BL-1199: a Well or Fishing Wharf draws no deposit, so
                // idle means labour.
                if (dep <= 0.0f &&
                    !placement_rules::is_depositless_site(w, b.tile, b.target_resource))
                    s.reason = building_run_reason::no_deposit;
            }
        }
    }
    else if (b.workforce_assigned < 0.5f)
    {
        s.kind   = building_run_kind::understaffed;
        s.reason = building_run_reason::labour_short;
    }
    return s;
}

} // namespace

building_running_state classify_building_running(const world& w, const recipe_registry* reg,
                                                 const economy_report* report, entity_id id,
                                                 const building_component& b)
{
    building_running_state s;

    // Construction outranks every other state: a site with ticks_remaining > 0
    // has no output to explain yet, whatever its staffing (BL-323 S4).
    if (b.ticks_remaining > 0)
    {
        s.kind      = building_run_kind::under_construction;
        s.eta_ticks = b.ticks_remaining;
        return s;
    }
    if (b.decommissioned)
    {
        s.kind = building_run_kind::mothballed;
        return s;
    }
    // Ports, hubs, launchpads, bases and the passive buildings produce nothing
    // and staff at zero by design (components.hpp, building_type): reading their
    // zero workforce as "labour short" would report a shortage that cannot
    // exist. A finished one that is not mothballed is simply running.
    if (b.type != building_type::extraction_site &&
        b.type != building_type::processing_facility)
        return s;
    if (b.type == building_type::processing_facility &&
        (b.recipe == no_recipe || (reg != nullptr && reg->get_recipe(b.recipe) == nullptr)))
    {
        s.kind   = building_run_kind::idle;
        s.reason = building_run_reason::no_method;
        return s;
    }

    const building_report* row = report_row(report, id);
    if (row == nullptr)
        return classify_unreported(w, b);

    s.reported = true;
    s.output   = row->output_quantity;

    // Labour first: a building nobody staffs is short of labour whatever else is
    // short, and naming an input would send the player to the wrong market.
    const bool unstaffed = row->effective_workforce < 0.1f || b.workforce_target <= 0;

    if (row->idle || !row->active)
    {
        s.kind = building_run_kind::idle;
        if (row->exhausted)
            s.reason = building_run_reason::deposit_spent;
        else if (unstaffed)
            s.reason = building_run_reason::labour_short;
        else if (b.type == building_type::processing_facility && row->has_limiting)
        {
            s.reason      = building_run_reason::input_short;
            s.input       = row->limiting_input;
            s.input_cause = input_cause_of(w, reg, b, row->limiting_input);
        }
        else if (b.type == building_type::extraction_site)
        {
            const auto  tile_it = w.tiles.find(b.tile);
            const float dep     = (tile_it != w.tiles.end())
                ? tile_it->second.resource_deposit[static_cast<std::size_t>(b.target_resource)]
                : 0.0f;
            s.reason = (dep <= 0.0f &&
                        !placement_rules::is_depositless_site(w, b.tile, b.target_resource))
                     ? building_run_reason::no_deposit
                     : building_run_reason::labour_short;
        }
        else
            s.reason = building_run_reason::labour_short;
        return s;
    }

    // Producing — but at full rate?
    if (row->effective_workforce < 0.5f)
    {
        s.kind   = building_run_kind::understaffed;
        s.reason = building_run_reason::labour_short;
    }
    else if (b.type == building_type::processing_facility && row->has_limiting &&
             row->run < 0.999f)
    {
        // Running on a part batch: the scarcest input set the run fraction.
        // PARTIAL, never "Understaffed" — the labour is there, and naming it
        // would send the player to the wrong lever. A run of nothing is Idle.
        s.kind        = (row->run <= 1e-4f) ? building_run_kind::idle
                                            : building_run_kind::partial;
        s.reason      = building_run_reason::input_short;
        s.input       = row->limiting_input;
        s.input_cause = input_cause_of(w, reg, b, row->limiting_input);
    }
    return s;
}

const char* running_state_word(building_run_kind k)
{
    switch (k)
    {
    case building_run_kind::running:            return "Running";
    case building_run_kind::understaffed:       return "Understaffed";
    case building_run_kind::partial:            return "Partial";
    case building_run_kind::idle:               return "Idle";
    case building_run_kind::mothballed:         return "Mothballed";
    case building_run_kind::under_construction: return "Under construction";
    }
    return "Running";
}

std::string running_state_reason(const building_running_state& s)
{
    if (s.kind == building_run_kind::under_construction)
    {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%d tick%s", s.eta_ticks, s.eta_ticks == 1 ? "" : "s");
        return buf;
    }
    switch (s.reason)
    {
    case building_run_reason::none:          return {};
    case building_run_reason::labour_short:  return "labour short";
    case building_run_reason::no_deposit:    return "no deposit on this tile";
    case building_run_reason::deposit_spent: return "deposit spent";
    case building_run_reason::no_method:     return "no method set";
    case building_run_reason::input_short:
    {
        std::string out = "input short: ";
        out += resource_name(s.input);
        if (s.input_cause == input_short_cause::none_on_shelf)
            out += ", none on the shelf";
        else if (s.input_cause == input_short_cause::over_ceiling)
            out += ", over the fair-price ceiling";
        return out;
    }
    }
    return {};
}

std::string running_state_text(const building_running_state& s)
{
    std::string out = running_state_word(s.kind);
    const std::string why = running_state_reason(s);
    if (!why.empty())
    {
        out += " \xe2\x80\x94 ";
        out += why;
    }
    return out;
}

ImU32 running_state_colour(building_run_kind k)
{
    switch (k)
    {
    case building_run_kind::running:            return palette::positive;
    case building_run_kind::idle:               return palette::negative;
    case building_run_kind::mothballed:         return palette::neutral;
    case building_run_kind::understaffed:
    case building_run_kind::partial:
    case building_run_kind::under_construction: return IM_COL32(225, 180, 90, 255);
    }
    return palette::neutral;
}

} // namespace ui
