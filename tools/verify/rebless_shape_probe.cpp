// ---------------------------------------------------------------------------
// rebless_shape_probe — the world's SHAPE on one row per seed, built to compile
// against BOTH sides of a re-bless (DELIVERY.md § The digest re-bless is one act
// per WAVE, rule 3: "states what changed in world SHAPE rather than in hash").
//
// Written for sprint 48's one re-bless (2026-10-03). The sprint's own census
// tools (centre_census, market_census, culture_preference_census) are newer than
// the sprint's starting world and do not build against it, so a combined
// before/after needed one instrument that reads only what both trees carry.
// Fields only one side has are read through a `requires` probe and print -1
// where the tree does not carry them ("not recorded", never zero).
//
// THE WORLD READ is the app's: build_app_start_world (generation with the app's
// config and works, the landscape search's winner applied), the validation settle
// (run_app_validation_settle), then the seat (seat_player_corporation) -- the
// order player_seed_sweep --digest runs. The only addition is an
// era_minus_one_fixture handed to make_hard_coded_world, which records the spans'
// states and changes no output (culture_preference_census reads the same).
//
// REPORTS; DOES NOT GATE. Every number here is a reading for a human to judge.
//
// Build: bash tools/verify/build_lua_harness.sh rebless_shape_probe
// Run from the repo root: build_gen/verify/rebless_shape_probe.exe [--seeds a,b,c]
//   (default: the 16 curated seeds of docs/generation/seed_library.json)
// Output: one `ROW seed=<n> key=value ...` line per seed, for a script to pool,
//   then one `MIX seed=<n> ext:<good>=n proc:<recipe>=n ...` line: live buildings
//   on the home body by extraction target (resource index) and processor recipe.
//   Sprint 49's re-bless (2026-10-07) added the MIX line and the treasury fields
//   (player / rival / background balances, nation treasuries); both read only
//   fields that exist on 1a44b6df and after.
// ---------------------------------------------------------------------------

#include "harness_params.hpp"

#include "scripting/lua_state.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/province.hpp"
#include "world/settlement.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{

std::string row;

void kv(const char* k, long long v)
{
    row += ' ';
    row += k;
    row += '=';
    row += std::to_string(v);
}

template <typename S> long long sea_trade_legs(const S& s)
{
    if constexpr (requires { s.sea_legs_noted_trade; }) return s.sea_legs_noted_trade;
    else return -1;
}
template <typename S> long long fleets_opened(const S& s)
{
    if constexpr (requires { s.naval_fleets_opened; }) return s.naval_fleets_opened;
    else return -1;
}
template <typename S> long long across_water(const S& s)
{
    if constexpr (requires { s.treaties_formed_across_water; }) return s.treaties_formed_across_water;
    else return -1;
}
template <typename S> long long far_across_water(const S& s)
{
    if constexpr (requires { s.far_treaties_formed_across_water; }) return s.far_treaties_formed_across_water;
    else return -1;
}
template <typename S> long long met_by_sea(const S& s)
{
    if constexpr (requires { s.contacts_met_by_sea; }) return s.contacts_met_by_sea;
    else return -1;
}
template <typename T> long long lane_tiles(const world& w, entity_id body)
{
    long long n = 0;
    if constexpr (requires(const T& t) { t.lane_level; })
    {
        for (const auto& [id, t] : w.tiles)
            if (t.body == body && t.lane_level > 0) ++n;
        return n;
    }
    else return -1;
}

void span(const char* tag, const history_sim_state& s)
{
    const std::string p(tag);
    kv((p + "_battles").c_str(), s.battles);
    kv((p + "_conquests").c_str(), s.conquests);
    kv((p + "_foundings").c_str(), s.foundings);
    kv((p + "_subjections").c_str(), s.subjections_formed);
    kv((p + "_bought").c_str(), s.provinces_bought);
    kv((p + "_naval_battles").c_str(), s.naval_battles);
    kv((p + "_sea_leg_battles").c_str(), s.sea_leg_battles);
    kv((p + "_legs_campaign").c_str(), s.sea_legs_noted_campaign);
    kv((p + "_legs_purchase").c_str(), s.sea_legs_noted_purchase);
    kv((p + "_legs_tribute").c_str(), s.sea_legs_noted_tribute);
    kv((p + "_legs_trade").c_str(), sea_trade_legs(s));
    kv((p + "_lanes_opened").c_str(), s.sea_lanes_opened);
    kv((p + "_fleets_opened").c_str(), fleets_opened(s));
    kv((p + "_treaties").c_str(), s.treaties_formed);
    kv((p + "_treaties_across_water").c_str(), across_water(s));
    kv((p + "_far_across_water").c_str(), far_across_water(s));
    kv((p + "_met_by_sea").c_str(), met_by_sea(s));
}

void prefs(const char* tag, const exploration_output& o)
{
    long long entries = 0, cultures = 0;
    int last = -1;
    for (const culture_good_preference& c : o.culture_preference)
    {
        ++entries;
        if (c.culture != last) { ++cultures; last = c.culture; }
    }
    const std::string p(tag);
    kv((p + "_pref_entries").c_str(), entries);
    kv((p + "_pref_cultures").c_str(), cultures);
    kv((p + "_cultures").c_str(), o.culture_count);
    kv((p + "_trade_flows").c_str(), static_cast<long long>(o.trade_flows.size()));
}

long long pct(std::vector<long long> v, int q)
{
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    std::size_t i = (v.size() * static_cast<std::size_t>(q) + 99) / 100;
    if (i == 0) i = 1;
    return v[std::min(i, v.size()) - 1];
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t at = 0;
            while (at <= s.size())
            {
                const std::size_t c = s.find(',', at);
                const std::string tok = s.substr(at, c == std::string::npos ? std::string::npos : c - at);
                if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (c == std::string::npos) break;
                at = c + 1;
            }
        }

    lua_state lua;
    for (uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        auto out = std::make_unique<app_start_world>();
        era_minus_one_fixture fx;
        // build_app_base_world, with the fixture handed in.
        out->params = p;
        lua.load("scripts/world_gen.lua");
        out->cfg = world_gen_config{};
        out->cfg.load_from_lua(lua);
        if (out->works.size() == 0)
        {
            lua.load("scripts/works.lua");
            out->works.load_from_lua(lua);
        }
        out->report = generation_report{};
        out->w = make_hard_coded_world(p, &out->report, out->cfg, nullptr, &out->works, &fx);
        seed_genesis_history(out->w, out->report);
        init_survey_states(out->w);
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        out->reg.load_from_lua(lua);
        band_registry_from_world(out->reg, out->w);
        assign_default_recipes(out->w, out->reg);
        apply_app_start_landscape(*out);
        run_app_validation_settle(out->w, out->reg);
        const spawn_seat_result seat =
            seat_player_corporation(out->w, seed, out->land.search.winner_score);

        const world& w = out->w;
        const entity_id home = w.home_body;
        row = "ROW seed=" + std::to_string(seed);

        // Centres and the scale mix.
        long long centres = 0, by_scale[6] = {0, 0, 0, 0, 0, 0}, centre_pop_k = 0, anchors = 0;
        for (const auto& [id, c] : w.population_centres)
        {
            const auto t = w.population_centre_tile.find(id);
            if (t == w.population_centre_tile.end()) continue;
            const auto tt = w.tiles.find(t->second);
            if (tt == w.tiles.end() || tt->second.body != home) continue;
            ++centres;
            ++by_scale[std::clamp(c.scale, 0, 5)];
            centre_pop_k += c.population;
            if (c.province_anchor) ++anchors;
        }
        kv("centres", centres);
        kv("villages", by_scale[1]);
        kv("towns", by_scale[2]);
        kv("cities", by_scale[3]);
        kv("metropolises", by_scale[4]);
        kv("megacities", by_scale[5]);
        kv("anchor_centres", anchors);
        kv("centre_pop_k", centre_pop_k);

        // Urban share off the generation's settlement record.
        long long pop = 0, urban = 0, regions = 0, living = 0, region_centres = 0;
        if (w.gen_settlement)
            for (const region& r : w.gen_settlement->regions)
            {
                ++regions;
                if (r.population > 0) ++living;
                pop += r.population;
                urban += r.urban_population;
                region_centres += r.centres;
            }
        kv("regions", regions);
        kv("living_regions", living);
        kv("population", pop);
        kv("urban_population", urban);
        kv("urban_permille", pop > 0 ? urban * 1000 / pop : -1);
        kv("region_centres", region_centres);

        // Land provinces on the home body.
        std::vector<long long> sizes;
        for (const province& pr : w.provinces.provinces)
            if (pr.body == home && province_kind_of(w, pr) == province_kind::land)
                sizes.push_back(static_cast<long long>(pr.tiles.size()));
        kv("land_provinces", static_cast<long long>(sizes.size()));
        kv("province_p50", pct(sizes, 50));
        kv("province_p90", pct(sizes, 90));
        kv("province_max", sizes.empty() ? 0 : *std::max_element(sizes.begin(), sizes.end()));

        // Markets.
        long long markets = 0;
        for (const auto& [id, m] : w.markets)
            if (m.body == home) ++markets;
        kv("markets", markets);

        // Roads by tier, and the stamped sea lane tiles.
        long long road[4] = {0, 0, 0, 0}, land = 0;
        for (const auto& [id, t] : w.tiles)
        {
            if (t.body != home) continue;
            if (t.road_level > 0) ++road[std::min<int>(t.road_level, 3)];
        }
        (void)land;
        kv("road_tiles", road[1] + road[2] + road[3]);
        kv("road_track", road[1]);
        kv("road_road", road[2]);
        kv("road_highway", road[3]);
        kv("lane_tiles", lane_tiles<tile_component>(w, home));

        // Corporations, the charter, the arms.
        long long corps = 0, rivals = 0;
        for (const auto& [id, c] : w.corporations)
        {
            ++corps;
            if (!c.is_player) ++rivals;
        }
        kv("corporations", corps);
        kv("firms", static_cast<long long>(out->land.firms.size()));
        kv("specialists", static_cast<long long>(out->land.specialists.size()));
        std::set<entity_id> armed;
        long long rival_groups = 0, rival_units = 0, player_units = 0, other_units = 0;
        for (const auto& [id, u] : w.units)
        {
            const auto c = w.corporations.find(u.owner);
            if (c == w.corporations.end()) { other_units += u.count; continue; }
            if (c->second.is_player) { player_units += u.count; continue; }
            armed.insert(u.owner);
            ++rival_groups;
            rival_units += u.count;
        }
        kv("rivals", rivals);
        kv("rivals_armed", static_cast<long long>(armed.size()));
        kv("rival_unit_groups", rival_groups);
        kv("rival_units", rival_units);
        kv("player_units", player_units);
        kv("non_corp_units", other_units);
        kv("seat_menu", seat.specialist_count);
        kv("seat_shortlist", seat.shortlist_size);
        kv("seat_floor_unmet", seat.floor_unmet ? 1 : 0);

        // Treasuries (sprint 49 addition; every field read here exists on both
        // sides of sprint 49's re-bless). Corporation balances split player /
        // background / other rival; nation treasuries on the nation component.
        {
            long long player_bal = 0, bg_sum = 0, rival_sum = 0, rivals_neg = 0;
            std::vector<long long> rbal;
            for (const auto& [id, c] : w.corporations)
            {
                const long long b = static_cast<long long>(c.balance);
                if (c.is_player) { player_bal += b; continue; }
                rival_sum += b;
                rbal.push_back(b);
                if (b < 0) ++rivals_neg;
                if (c.is_background) bg_sum += b;
            }
            kv("player_balance", player_bal);
            kv("rival_balance_sum", rival_sum);
            kv("rival_balance_p50", pct(rbal, 50));
            kv("background_balance_sum", bg_sum);
            kv("rivals_negative", rivals_neg);
            long long nat_sum = 0, nations = 0;
            std::vector<long long> nt;
            for (const auto& [id, n] : w.nations)
            {
                ++nations;
                nat_sum += static_cast<long long>(n.treasury);
                nt.push_back(static_cast<long long>(n.treasury));
            }
            kv("nations", nations);
            kv("nation_treasury_sum", nat_sum);
            kv("nation_treasury_p50", pct(nt, 50));
        }

        // Buildings on the home body by type, and the mix by recipe / target
        // (sprint 49 addition). Printed as a second MIX line per seed.
        std::string mix = "MIX seed=" + std::to_string(seed);
        {
            std::map<std::string, long long> by;
            long long bt[16] = {}, under_construction = 0, decommissioned = 0;
            for (const auto& [id, b] : w.buildings)
            {
                const auto tt = w.tiles.find(b.tile);
                if (tt == w.tiles.end() || tt->second.body != home) continue;
                if (b.decommissioned) { ++decommissioned; continue; }
                ++bt[std::min<int>(static_cast<int>(b.type), 15)];
                if (b.ticks_remaining > 0) ++under_construction;
                std::string key;
                if (b.type == building_type::extraction_site)
                {
                    const recipe* r = b.recipe != no_recipe ? out->reg.get_recipe(b.recipe) : nullptr;
                    key = r ? "ext:" + r->name : "ext:r" + std::to_string(static_cast<int>(b.target_resource));
                }
                else if (b.type == building_type::processing_facility)
                {
                    const recipe* r = b.recipe != no_recipe ? out->reg.get_recipe(b.recipe) : nullptr;
                    key = r ? "proc:" + r->name : "proc:none";
                }
                else continue;
                for (char& ch : key) if (ch == ' ' || ch == '=') ch = '_';
                ++by[key];
            }
            kv("bld_extraction", bt[1]);
            kv("bld_processing", bt[2]);
            kv("bld_port", bt[3]);
            kv("bld_logistics_hub", bt[5]);
            kv("bld_military", bt[6]);
            kv("bld_other", bt[4] + bt[7] + bt[8] + bt[9]);
            kv("bld_under_construction", under_construction);
            kv("bld_decommissioned", decommissioned);
            for (const auto& [k, n] : by) mix += " " + k + "=" + std::to_string(n);
        }

        // The spans.
        kv("emp_battles", fx.battles);
        kv("emp_conquests", fx.conquests);
        kv("emp_foundings", fx.foundings);
        if (fx.exploration_ran)
        {
            span("exp", fx.exploration_state);
            prefs("exp", fx.exploration_handoff);
        }
        if (fx.industrialisation_ran)
        {
            span("ind", fx.industrialisation_state);
            prefs("ind", fx.industrialisation_handoff);
        }
        std::printf("%s\n%s\n", row.c_str(), mix.c_str());
        std::fflush(stdout);
    }
    return 0;
}
