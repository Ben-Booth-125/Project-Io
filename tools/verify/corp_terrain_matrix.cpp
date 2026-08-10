// Headless corporation-terrain viability harness (BL-075). No SDL / Lua / ImGui.
//
// Builds the single canonical generated world (make_hard_coded_world) and, for
// every generated corporation, runs a two-tier viability matrix:
//
//   Tier-1 (hard FAIL — exits non-zero):
//     A1 Asset legality   — every starting asset sits on terrain its placement
//                           rules accept (the same placement_rules seam generation
//                           gated on). An illegally-placed authored asset is a real
//                           generation bug.
//     A2 Buildable focus  — the corp's industrial focus has >= 1 valid candidate
//                           tile on its assigned body for at least one building
//                           type in that focus.
//
//   Tier-2 (WARN — printed loudly, never fails the build):
//     A3 Reachable market — at least one output resource of the focus has a market
//                           centre on the body (or would be reachable via convoy).
//     A4 Profitable chain — the chain's output base_price exceeds input + wage +
//                           maintenance cost, so the focus is not structurally
//                           loss-making. Deliberately COARSE: base_price (the
//                           authored rarity floor), not a cleared market price, and
//                           nominal (workforce = 1) staffing.
//
// Semantics: the process exits non-zero ONLY on a tier-1 FAIL. Tier-2 issues print
// as WARN and do not affect the exit code. One PASS/FAIL/WARN line is printed per
// (seed, corp) assertion, naming the failing assertion.
//
// Seed scope (MVP): make_hard_coded_world() takes no seed parameter — world-gen is
// not seedable today — so this checks the single canonical world. The harness is
// structured with the seed count as a constant + loop (k_seed_count) so it widens
// to a true seed sweep WITHOUT restructuring. FOLLOW-ON: making world-gen seedable
// (threading a seed through make_hard_coded_world) is out of scope for BL-075.
//
// Kept outside src/ so the CMake glob does not pull it into the game build.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

const char* focus_name(industrial_focus f)
{
    switch (f)
    {
        case industrial_focus::extraction: return "extraction";
        case industrial_focus::processing: return "processing";
        case industrial_focus::trade:      return "trade";
    }
    return "?";
}

const char* building_name(building_type t)
{
    switch (t)
    {
        case building_type::none:                return "none";
        case building_type::extraction_site:     return "extraction_site";
        case building_type::processing_facility: return "processing_facility";
        case building_type::port:                return "port";
        case building_type::launchpad:           return "launchpad";
        case building_type::inland_logistics_hub: return "inland_logistics_hub";
        case building_type::military_base:       return "military_base";
    }
    return "?";
}

const char* resource_name(resource_type r)
{
    switch (r)
    {
        case resource_type::iron_ore:              return "iron_ore";
        case resource_type::coal:                  return "coal";
        case resource_type::petroleum:             return "petroleum";
        case resource_type::silica:                return "silica";
        case resource_type::copper_ore:            return "copper_ore";
        case resource_type::rare_earth_ore:        return "rare_earth_ore";
        case resource_type::agricultural_produce:  return "agricultural_produce";
        case resource_type::water:                 return "water";
        case resource_type::iron_nickel_ore:       return "iron_nickel_ore";
        case resource_type::platinum_group_metals: return "platinum_group_metals";
        case resource_type::regolith:              return "regolith";
        case resource_type::stone:                 return "stone";
        case resource_type::timber:                return "timber";
        case resource_type::sand:                  return "sand";
        case resource_type::clay:                  return "clay";
        case resource_type::peat:                  return "peat";
        case resource_type::tobacco:               return "tobacco";
        case resource_type::spices:                return "spices";
        case resource_type::coffee:                return "coffee";
        case resource_type::furs:                  return "furs";
        case resource_type::steel:                 return "steel";
        case resource_type::refined_fuel:          return "refined_fuel";
        case resource_type::food_rations:          return "food_rations";
        case resource_type::grain:                 return "grain";
        case resource_type::fodder:                return "fodder";
        case resource_type::salt:                  return "salt";
        case resource_type::transport_capacity:    return "transport_capacity";
        case resource_type::charcoal:              return "charcoal";
        case resource_type::iron_blooms:           return "iron_blooms";
        case resource_type::bullion:               return "bullion";
        case resource_type::trade_goods_misc:      return "trade_goods_misc";
        case resource_type::propellant:            return "propellant";
        case resource_type::count:                 return "count";
    }
    return "?";
}

// The building types a focus is characterised by. Mirrors focus_asset_pattern in
// corporation_generation.cpp (anonymous namespace, so reproduced here): the anchor
// type first, then the supporting mix. A2 needs only "is there a candidate tile for
// at least one of these", so the set (not the fill order) is what matters.
std::vector<building_type> focus_building_types(industrial_focus f)
{
    switch (f)
    {
        case industrial_focus::extraction:
            return { building_type::extraction_site, building_type::processing_facility };
        case industrial_focus::processing:
            return { building_type::processing_facility, building_type::extraction_site };
        case industrial_focus::trade:
            return { building_type::port, building_type::extraction_site,
                     building_type::processing_facility };
    }
    return { building_type::extraction_site };
}

// Coarse reference economy — a hand-built registry mirroring scripts/economy.lua
// and scripts/recipes.lua (the authored Lua that the real build loads). The
// recipe_registry header documents this "constructed by hand in a headless test
// harness" path; only its inline setters/getters are used, so recipe_registry.cpp
// (the Lua loader) is NOT linked. base_price is NOT taken from here — it is read
// from the actual generated market on the body (the real data).
recipe_registry build_reference_registry()
{
    recipe_registry reg;

    building_economics ext{};  ext.base_rate = 20.0f; ext.maintenance = 5.0f;  ext.base_wage = 8.0f;
    building_economics proc{}; proc.base_rate = 8.0f; proc.maintenance = 10.0f; proc.base_wage = 12.0f;
    building_economics port{}; port.base_rate = 0.0f; port.maintenance = 8.0f;  port.base_wage = 6.0f;
    reg.set_economics(building_type::extraction_site, ext);
    reg.set_economics(building_type::processing_facility, proc);
    reg.set_economics(building_type::port, port);

    recipe steel{};   steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;             steel.outputs[ri(resource_type::steel)] = 1.0f;
    recipe fuel{};    fuel.name = "refined_fuel";
    fuel.inputs[ri(resource_type::petroleum)] = 2.0f;            fuel.outputs[ri(resource_type::refined_fuel)] = 1.0f;
    recipe food{};    food.name = "food_rations";
    food.inputs[ri(resource_type::agricultural_produce)] = 2.0f; food.outputs[ri(resource_type::food_rations)] = 1.0f;
    reg.add_recipe(steel);
    reg.add_recipe(fuel);
    reg.add_recipe(food);

    return reg;
}

// The body a corporation operates on: derived from its assets' tiles, else from its
// home nation's first tile. In the prototype every nation (and hence every corp)
// sits on Kepler, but deriving it keeps the harness correct once bodies diverge.
entity_id corp_body(const world& w, const corporation_component& corp)
{
    for (entity_id bid : corp.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end())
            return tit->second.body;
    }
    const auto nit = w.nations.find(corp.home_nation);
    if (nit != w.nations.end())
        for (entity_id tid : nit->second.tiles)
        {
            const auto tit = w.tiles.find(tid);
            if (tit != w.tiles.end())
                return tit->second.body;
        }
    return null_entity;
}

// The base_price array for a resource on a body, taken from any market anchored to
// that body (Kepler's markets share one authored template). Returns nullptr if the
// body has no market.
const std::array<float, resource_count>* body_base_prices(const world& w, entity_id body)
{
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body)
            return &mc.base_price;
    return nullptr;
}

// Focus output resources (what the chain sells).
//   extraction — the raw resources its extraction sites target.
//   processing — the three refined recipe outputs.
//   trade      — the tradeable set it might move (extractables + refined).
std::vector<resource_type> focus_outputs(const world& w, const corporation_component& corp)
{
    std::vector<resource_type> out;
    switch (corp.focus)
    {
        case industrial_focus::extraction:
        {
            for (entity_id bid : corp.assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit != w.buildings.end() && bit->second.type == building_type::extraction_site)
                {
                    const resource_type r = bit->second.target_resource;
                    if (std::find(out.begin(), out.end(), r) == out.end())
                        out.push_back(r);
                }
            }
            if (out.empty())
                for (resource_type r : placement_rules::k_extractable)
                    out.push_back(r);
            break;
        }
        case industrial_focus::processing:
            out = { resource_type::steel, resource_type::refined_fuel, resource_type::food_rations };
            break;
        case industrial_focus::trade:
            out = { resource_type::iron_ore, resource_type::petroleum, resource_type::water,
                    resource_type::agricultural_produce, resource_type::steel,
                    resource_type::refined_fuel, resource_type::food_rations };
            break;
    }
    return out;
}

// Report state for the whole run.
int g_tier1_fail = 0; // number of tier-1 FAIL assertions across all (seed, corp)
int g_tier2_warn = 0; // number of tier-2 WARN assertions

// --- A1: asset legality (tier-1) ------------------------------------------------
// Uses placement_rules::can_place — the exact tile-level rule generation gated
// placement on ("Every placement is gated by placement_rules::can_place"). A port
// on a non-coastal tile is additionally reported as a NOTE via can_place_in_world
// (coastal is a player-construction-time rule generation does not enforce), but is
// NOT a tier-1 failure, to avoid flagging a looser-generation rule as a bug.
bool assert_asset_legality(const world& w, entity_id cid, const corporation_component& corp)
{
    int bad = 0;
    for (entity_id bid : corp.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
        {
            ++bad;
            std::printf("      A1 offending asset %u: no building_component\n",
                        static_cast<unsigned>(bid));
            continue;
        }
        const building_component& bc = bit->second;
        const auto tit = w.tiles.find(bc.tile);
        if (tit == w.tiles.end())
        {
            ++bad;
            std::printf("      A1 offending asset %u (%s): tile %u missing\n",
                        static_cast<unsigned>(bid), building_name(bc.type),
                        static_cast<unsigned>(bc.tile));
            continue;
        }
        const tile_component& tc = tit->second;
        if (!placement_rules::can_place(tc, bc.type, bc.target_resource))
        {
            ++bad;
            // Derive a human reason (this branch predates placement_result/message()).
            const char* why = "rejected";
            if (placement_rules::is_ocean_tile(tc.composition))
                why = "ocean tile";
            else if (bc.type == building_type::extraction_site
                     && tc.resource_deposit[ri(bc.target_resource)] <= 0.0f)
                why = "no extractable deposit of target";
            std::printf("      A1 offending asset %u (%s target=%s): %s\n",
                        static_cast<unsigned>(bid), building_name(bc.type),
                        resource_name(bc.target_resource), why);
        }
        else if (bc.type == building_type::port
                 && !placement_rules::can_place_in_world(w, bc.tile, bc.type, bc.target_resource))
        {
            std::printf("      A1 NOTE: port asset %u is non-coastal (passes can_place, "
                        "would fail player-time can_place_in_world) — not a tier-1 fail\n",
                        static_cast<unsigned>(bid));
        }
    }
    const bool pass = (bad == 0);
    std::printf("    %s  A1 asset legality (%zu assets, %d illegal)  [tier-1]\n",
                pass ? "PASS" : "FAIL", corp.assets.size(), bad);
    if (!pass) { ++g_tier1_fail; std::printf("      corp=%u FAILS A1 (asset legality)\n",
                                             static_cast<unsigned>(cid)); }
    return pass;
}

// --- A2: buildable focus (tier-1) -----------------------------------------------
// At least one building type in the focus has >= 1 tile on the body that
// can_place_in_world accepts. Uses the full world-level check (coastal for ports,
// launchpad cap) since this models where the corp could actually build next.
bool assert_buildable_focus(const world& w, entity_id cid, const corporation_component& corp,
                            entity_id body)
{
    bool any_type_buildable = false;
    for (building_type bt : focus_building_types(corp.focus))
    {
        int candidates = 0;
        for (const auto& [tid, tc] : w.tiles)
        {
            if (tc.body != body)
                continue;
            bool any = false;
            const resource_type tgt = placement_rules::richest_extractable(tc, any);
            if (placement_rules::can_place_in_world(w, tid, bt, tgt))
                ++candidates;
        }
        std::printf("      A2 %-20s candidate tiles: %d\n", building_name(bt), candidates);
        if (candidates > 0)
            any_type_buildable = true;
    }
    std::printf("    %s  A2 buildable focus (focus=%s)  [tier-1]\n",
                any_type_buildable ? "PASS" : "FAIL", focus_name(corp.focus));
    if (!any_type_buildable) { ++g_tier1_fail;
        std::printf("      corp=%u FAILS A2 (buildable focus)\n", static_cast<unsigned>(cid)); }
    return any_type_buildable;
}

// --- A3: reachable market (tier-2 WARN) -----------------------------------------
void assert_reachable_market(const world& w, entity_id cid, const corporation_component& corp,
                             entity_id body)
{
    const std::array<float, resource_count>* base = body_base_prices(w, body);
    std::vector<resource_type> reachable;
    if (base)
        for (resource_type r : focus_outputs(w, corp))
            if ((*base)[ri(r)] > 0.0f)
                reachable.push_back(r);

    if (!reachable.empty())
    {
        std::printf("    PASS  A3 reachable market: %s tradeable on body market  [tier-2]\n",
                    resource_name(reachable.front()));
    }
    else
    {
        ++g_tier2_warn;
        std::printf("    WARN  A3 reachable market: no focus output has a market centre on the "
                    "body (convoy reachability not modelled)  [tier-2]\n");
        std::printf("      corp=%u WARN A3 (reachable market)\n", static_cast<unsigned>(cid));
    }
}

// --- A4: profitable chain (tier-2 WARN, coarse) ---------------------------------
// Per-tick, nominal (workforce = 1) profitability at authored base prices:
//   extraction: rev = base_rate * base_price(out); cost = base_wage + maintenance.
//   processing: rev = base_rate * sum(base_price[out]*qty);
//               cost = base_rate * sum(base_price[in]*qty) + base_wage + maintenance.
//   trade:      no production chain (moves others' goods) — reported N/A, not WARN.
// The focus passes if >= 1 candidate chain is non-loss-making; WARN only if every
// candidate chain is structurally loss-making at base prices.
void assert_profitable_chain(const world& w, const recipe_registry& reg,
                             entity_id cid, const corporation_component& corp, entity_id body)
{
    const std::array<float, resource_count>* base = body_base_prices(w, body);
    if (!base)
    {
        ++g_tier2_warn;
        std::printf("    WARN  A4 profitable chain: no market on body — cannot price chain  [tier-2]\n");
        std::printf("      corp=%u WARN A4 (profitable chain)\n", static_cast<unsigned>(cid));
        return;
    }

    if (corp.focus == industrial_focus::trade)
    {
        std::printf("    PASS  A4 profitable chain: N/A for trade focus (moves goods, no "
                    "production chain)  [tier-2]\n");
        return;
    }

    bool any_profitable = false;

    if (corp.focus == industrial_focus::extraction)
    {
        const building_economics& e = reg.economics(building_type::extraction_site);
        for (resource_type r : focus_outputs(w, corp))
        {
            const float rev  = e.base_rate * (*base)[ri(r)];
            const float cost = e.base_wage + e.maintenance;
            const float profit = rev - cost;
            std::printf("      A4 extract %-20s rev=%.2f cost=%.2f profit=%+.2f%s\n",
                        resource_name(r), rev, cost, profit, profit > 0.0f ? "" : "  (LOSS)");
            if (profit > 0.0f) any_profitable = true;
        }
    }
    else // processing
    {
        const building_economics& e = reg.economics(building_type::processing_facility);
        for (std::size_t id = 0; id < reg.recipe_count(); ++id)
        {
            const recipe* rc = reg.get_recipe(static_cast<uint16_t>(id));
            if (!rc) continue;
            float out_val = 0.0f, in_val = 0.0f;
            resource_type out_res = resource_type::count;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (rc->outputs[r] > 0.0f) { out_val += (*base)[r] * rc->outputs[r];
                                             out_res = static_cast<resource_type>(r); }
                if (rc->inputs[r]  > 0.0f) { in_val  += (*base)[r] * rc->inputs[r]; }
            }
            const float rev  = e.base_rate * out_val;
            const float cost = e.base_rate * in_val + e.base_wage + e.maintenance;
            const float profit = rev - cost;
            std::printf("      A4 %-12s -> %-14s rev=%.2f cost=%.2f profit=%+.2f%s\n",
                        rc->name.c_str(), resource_name(out_res), rev, cost, profit,
                        profit > 0.0f ? "" : "  (LOSS)");
            if (profit > 0.0f) any_profitable = true;
        }
    }

    if (any_profitable)
    {
        std::printf("    PASS  A4 profitable chain: at least one chain clears cost at base_price  [tier-2]\n");
    }
    else
    {
        ++g_tier2_warn;
        std::printf("    WARN  A4 profitable chain: every candidate chain is structurally "
                    "loss-making at base_price  [tier-2]\n");
        std::printf("      corp=%u WARN A4 (profitable chain)\n", static_cast<unsigned>(cid));
    }
}

} // namespace

int main()
{
    // MVP: one canonical world. World-gen is not seedable today (see file header) —
    // the loop + constant let this widen to a true seed sweep without restructuring.
    constexpr int k_seed_count = 1;

    std::printf("BL-075 corp-terrain viability matrix — %d seed(s), single canonical world (MVP)\n",
                k_seed_count);
    std::printf("Semantics: tier-1 FAIL exits non-zero; tier-2 WARN is loud but non-fatal.\n");

    const recipe_registry reg = build_reference_registry();

    int corps_total = 0;

    for (int seed_idx = 0; seed_idx < k_seed_count; ++seed_idx)
    {
        world w = make_hard_coded_world();

        // Deterministic corp order (unordered_map iteration is unspecified).
        std::vector<entity_id> corp_ids;
        corp_ids.reserve(w.corporations.size());
        for (const auto& [cid, cc] : w.corporations)
            corp_ids.push_back(cid);
        std::sort(corp_ids.begin(), corp_ids.end());

        std::printf("\n=== seed %d — %zu corporations ===\n", seed_idx, corp_ids.size());

        for (entity_id cid : corp_ids)
        {
            ++corps_total;
            const corporation_component& corp = w.corporations.at(cid);
            const entity_id body = corp_body(w, corp);
            const char* body_name = "?";
            const auto bit = w.bodies.find(body);
            if (bit != w.bodies.end()) body_name = bit->second.name.c_str();

            std::printf("\n  corp=%u \"%s\" focus=%s body=%s assets=%zu%s\n",
                        static_cast<unsigned>(cid), corp.name.c_str(), focus_name(corp.focus),
                        body_name, corp.assets.size(), corp.is_player ? " [PLAYER]" : "");

            assert_asset_legality(w, cid, corp);
            assert_buildable_focus(w, cid, corp, body);
            assert_reachable_market(w, cid, corp, body);
            assert_profitable_chain(w, reg, cid, corp, body);
        }
    }

    std::printf("\n----------------------------------------------------------------\n");
    std::printf("Summary: %d corp(s) over %d seed(s) — tier-1 FAILs=%d, tier-2 WARNs=%d\n",
                corps_total, k_seed_count, g_tier1_fail, g_tier2_warn);
    std::printf("Result: %s (exit %d)\n",
                g_tier1_fail ? "FAIL (tier-1 assertion failed)"
                             : (g_tier2_warn ? "PASS with WARNINGS" : "PASS"),
                g_tier1_fail ? 1 : 0);

    return g_tier1_fail ? 1 : 0;
}
