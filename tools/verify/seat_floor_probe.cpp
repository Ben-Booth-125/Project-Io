// ---------------------------------------------------------------------------
// seat_floor_probe — why does the seat's viability floor read 0? (BL-1298)
// ---------------------------------------------------------------------------
// player_seed_sweep --guard's S6 asserts the seat shortlist is non-empty: some
// specialist's STATIC landscape score (spawn_seat.cpp seat_landscape_score, the
// mean over its sited holdings of its catchment market's actual x balance in the
// landscape-search WINNER's score) must be > 0. This probe builds the app's
// start world exactly as player_seed_sweep's build_and_seat does, settles it,
// and prints, per specialist, every holding: building type, tile, the market
// its tile clears in, and that market's actual / balance / extractors /
// processors in the winner's score -- plus the winner's markets with any
// viability. A diagnostic, not a gate: exits 0 always.
//
//   bash tools/verify/build_lua_harness.sh seat_floor_probe
//   build_gen/verify/seat_floor_probe.exe --seed 8 [--ipu N] [--endow N] [--levy N]

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/market_clearing.hpp"
#include "world/spawn_seat.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace {

const char* btype(building_type t)
{
    switch (t)
    {
    case building_type::extraction_site:     return "extraction";
    case building_type::processing_facility: return "processing";
    case building_type::military_base:       return "military";
    default:                                 return "other";
    }
}

} // namespace

int main(int argc, char** argv)
{
    uint32_t seed = 0;
    long long ipu = -1;
    int endow = -1, levy = -1;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        else if (!std::strcmp(argv[i], "--ipu") && i + 1 < argc) ipu = std::atoll(argv[++i]);
        else if (!std::strcmp(argv[i], "--endow") && i + 1 < argc) endow = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--levy") && i + 1 < argc) levy = std::atoi(argv[++i]);
        else { std::printf("usage: seat_floor_probe --seed N [--ipu N] [--endow N] [--levy N]\n"); return 2; }
    }

    lua_state lua;
    world_params p = arc_params(world_arc::shipped);
    p.seed = seed;
    if (ipu >= 1)   p.industry_points_per_1000_treasury_units = ipu;
    if (endow >= 0) p.treasury_endowment_income_q = endow;
    if (levy >= 0)  p.treasury_levy_per_mille_q = levy;

    app_start_world out;
    charter_spend_report rep;
    harness_charter_input charter;
    charter.report = &rep;
    build_app_start_world(lua, p, out, charter);
    run_app_validation_settle(out.w, out.reg);
    const landscape_score& sc = out.land.search.winner_score;
    world& w = out.w;

    std::map<entity_id, const market_score*> by_market;
    for (const market_score& m : sc.markets) by_market[m.market] = &m;

    std::printf("seat_floor_probe seed %u (ipu %lld, endow %d, levy %d; -1 = shipped)\n", seed, ipu, endow, levy);
    std::printf("winner composite %.6f, %zu scored markets; markets with actual x balance > 0:\n",
                sc.composite, sc.markets.size());
    for (const market_score& m : sc.markets)
        if (m.actual * m.balance > 0.0)
            std::printf("  market %llu  actual %.3f balance %.3f  extractors %d processors %d rated %d\n",
                        (unsigned long long)m.market, m.actual, m.balance, m.extractors, m.processors, m.rated);

    std::vector<entity_id> specs;
    for (const auto& [id, c] : w.nations) if (!c.is_major == false) specs.push_back(id);
    std::sort(specs.begin(), specs.end());
    std::printf("specialists after the settle: %zu (the search's roster: %zu)\n", specs.size(), out.land.specialists.size());
    for (const entity_id id : specs)
    {
        const nation_component& c = w.nations.at(id);
        int held = 0;
        const double s = seat_landscape_score(w, sc, id, &held);
        const bool in_roster = std::find(out.land.specialists.begin(), out.land.specialists.end(), id) != out.land.specialists.end();
        std::printf(" corp %llu \"%s\" home_nation %llu  in search roster %s  assets %zu  seat score %.4f (%d holdings scored)\n",
                    (unsigned long long)id, c.name.c_str(), (unsigned long long)id,
                    in_roster ? "yes" : "NO", c.state_assets.size(), s, held);
        // The home nation's budgeted centres, richest first (the walk's own
        // order: points descending, ties to the lower id), with each centre's
        // market and that market's viability -- the champion is chartered at
        // the first that affords the specialist price.
        {
            struct row { entity_id centre; int32_t pts; };
            std::vector<row> rows;
            for (const auto& [cid, pts] : out.land.stockpile.budget.points())
            {
                const auto t = w.population_centre_tile.find(cid);
                if (t == w.population_centre_tile.end()) continue;
                const auto own = w.tile_to_nation.find(t->second);
                if (own == w.tile_to_nation.end() || own->second != id) continue;
                rows.push_back({ cid, pts });
            }
            std::sort(rows.begin(), rows.end(), [](const row& a, const row& b) {
                return a.pts != b.pts ? a.pts > b.pts : a.centre < b.centre; });
            std::printf("    nation's centres by points (%zu budgeted):\n", rows.size());
            for (std::size_t k = 0; k < rows.size() && k < 6; ++k)
            {
                const entity_id cid = rows[k].centre;
                const entity_id tile = w.population_centre_tile.at(cid);
                const entity_id mk = market_for_tile(w, tile);
                const auto mit = by_market.find(mk);
                const auto pc = w.population_centres.find(cid);
                std::printf("      centre %llu pts %d (specialist price %lld) pop %dk scale %d tile %llu market %llu viab %.4f (ext %d proc %d)\n",
                            (unsigned long long)cid, rows[k].pts,
                            (long long)out.land.stockpile_spend.specialist_price_of(cid),
                            pc != w.population_centres.end() ? pc->second.population : -1,
                            pc != w.population_centres.end() ? pc->second.scale : -1,
                            (unsigned long long)tile, (unsigned long long)mk,
                            mit != by_market.end() ? mit->second->actual * mit->second->balance : -1.0,
                            mit != by_market.end() ? mit->second->extractors : -1,
                            mit != by_market.end() ? mit->second->processors : -1);
            }
        }
        for (const entity_id bid : c.state_assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end()) { std::printf("    building %llu MISSING\n", (unsigned long long)bid); continue; }
            const building_component& b = bit->second;
            const entity_id mk = w.tiles.count(b.tile) ? market_for_tile(w, b.tile) : null_entity;
            const auto mit = by_market.find(mk);
            if (mit == by_market.end())
                std::printf("    %-10s tile %llu  market %llu  NOT IN THE WINNER'S SCORE\n", btype(b.type),
                            (unsigned long long)b.tile, (unsigned long long)mk);
            else
                std::printf("    %-10s tile %llu  market %llu  actual %.3f balance %.3f (ext %d proc %d rated %d balanced %d)\n",
                            btype(b.type), (unsigned long long)b.tile, (unsigned long long)mk,
                            mit->second->actual, mit->second->balance, mit->second->extractors,
                            mit->second->processors, mit->second->rated, mit->second->balanced);
        }
    }
    // The winner's spend: unspent points by centre and reason, for every
    // budgeted centre of a specialist's home nation; and each body's caps.
    std::printf("charter spend: %zu specialists, %zu firms, budgeted %lld spent %lld unspent %lld\n",
                rep.specialists.size(), rep.firms.size(), (long long)rep.points_budgeted,
                (long long)rep.points_spent, (long long)rep.points_unspent);
    std::printf("  champions: %d passed over (no processor-capable market), %d fallen back down their nation\n",
                rep.champions_passed_over, rep.champions_fallen_back);
    {
        // Nations holding an affording budgeted centre (the walk's champion set)
        // but no specialist after the settle: a champion lost on the way.
        std::map<entity_id, int> affording;
        for (const auto& [cid, pts] : out.land.stockpile.budget.points())
        {
            const auto t = w.population_centre_tile.find(cid);
            if (t == w.population_centre_tile.end()) continue;
            const auto own = w.tile_to_nation.find(t->second);
            if (own == w.tile_to_nation.end()) continue;
            if (static_cast<int64_t>(pts) >= out.land.stockpile_spend.specialist_price_of(cid))
                ++affording[own->second];
        }
        std::set<entity_id> with_spec;
        for (const entity_id id : specs) with_spec.insert(id /* BL-1407: its own nation */);
        int lost = 0;
        for (const auto& [n, c] : affording)
            if (!with_spec.count(n))
            {
                ++lost;
                std::printf("  nation %llu: %d affording centre(s) but no specialist\n", (unsigned long long)n, c);
            }
        std::printf("  nations with an affording centre %zu, with a specialist %zu, lost %d\n",
                    affording.size(), with_spec.size(), lost);
    }
    for (const charter_body_record& br : rep.bodies)
        std::printf("  body %llu: firms %d, density ceiling %d, per-good cap %d, goods in G %d, firm charters %lld\n",
                    (unsigned long long)br.body, br.firms, br.density_ceiling, br.per_good_cap, br.goods_in_g,
                    (long long)br.firm_charters);
    std::map<int, long long> by_reason;
    for (const charter_unspent& u : rep.unspent) by_reason[static_cast<int>(u.reason)] += u.points;
    std::printf("  unspent by reason (0 no_nation 1 window 2 province_cap 3 no_gap 4 body_cap 5 remainder 7 density 8 late 9 share_unplaced):");
    for (const auto& [r, v] : by_reason) std::printf(" [%d: %lld]", r, v);
    std::printf("\n");
    for (const entity_id id : specs)
    {
        const entity_id nat = id /* BL-1407: its own nation */;
        for (const charter_unspent& u : rep.unspent)
        {
            const auto t = w.population_centre_tile.find(u.centre);
            if (t == w.population_centre_tile.end()) continue;
            const auto own = w.tile_to_nation.find(t->second);
            if (own == w.tile_to_nation.end() || own->second != nat) continue;
            if (out.land.stockpile_spend.specialist_price_of(u.centre) >= out.land.stockpile_spend.specialist_price_points())
                continue; // only the cheap-reach centres
            std::printf("  nation %llu cheap-reach centre %llu: unspent %d points, reason %d\n",
                        (unsigned long long)nat, (unsigned long long)u.centre, u.points, static_cast<int>(u.reason));
        }
    }

    // Every building in each specialist's markets, by owner: who else stands
    // in the catchment the floor reads.
    std::map<entity_id, int> seen;
    for (const entity_id id : specs)
        for (const entity_id bid : w.nations.at(id).state_assets)
            if (const auto bit = w.buildings.find(bid); bit != w.buildings.end() && w.tiles.count(bit->second.tile))
                seen[market_for_tile(w, bit->second.tile)] = 1;
    for (const auto& [mk, unused] : seen)
    {
        (void)unused;
        std::map<std::pair<entity_id, int>, int> by_owner; // (owner, type) -> count
        int tiles_in = 0;
        for (const auto& [tid, tc] : w.tiles)
            if (market_for_tile(w, tid) == mk) ++tiles_in;
        for (const auto& [cid, cc] : w.nations)
            for (const entity_id bid : cc.state_assets)
                if (const auto bit = w.buildings.find(bid); bit != w.buildings.end()
                    && w.tiles.count(bit->second.tile) && market_for_tile(w, bit->second.tile) == mk)
                    ++by_owner[{ cid, static_cast<int>(bit->second.type) }];
        std::printf("market %llu: %d tiles; buildings by (owner, type):", (unsigned long long)mk, tiles_in);
        for (const auto& [k, n] : by_owner)
        {
            const auto cit = w.nations.find(k.first);
            std::printf(" [%llu%s %s x%d]", (unsigned long long)k.first,
                        cit == w.nations.end() ? "?" : (cit->second.is_major == false ? " bg" : " SPEC"),
                        btype(static_cast<building_type>(k.second)), n);
        }
        std::printf("\n");
    }
    return 0;
}
