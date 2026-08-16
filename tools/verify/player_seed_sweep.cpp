// player_seed_sweep — which seeds give the PLAYER a corp worth playing?
//
// Written 2026-08-16 after a live look at an autostarted campaign found the
// player corp holding no processing facility at all: the Method page (BL-430/431's
// whole surface) had nothing to show, because the seed had handed the player a
// pure-extraction corp. That is not a rendering bug and no existing harness would
// ever have caught it — `seed_sweep_probe` asks only whether generation THREW,
// and `pregame_balance_harness` asks a rich question of exactly one seed.
//
// WHAT "WORTH PLAYING" MEANS HERE, stated so the verdict is arguable rather than
// magic. Two conditions, both about the opening position the player is handed:
//
//   * PROCESSING — the corp owns at least one processing_facility. Without one
//     the chain-depth ladder (BL-428), alternate methods (BL-430) and the whole
//     Method page are unreachable from the start position; the player can build
//     one, but the opening reads as a strictly poorer game.
//   * SOLVENT — the corp's balance after the warm start is positive AND it never
//     dipped below zero on the way. Ending up positive after a trip through
//     insolvency is a different (worse) opening than never dipping, so both are
//     reported separately rather than collapsed into one flag.
//
// This harness REPORTS. It deliberately does not filter, hard-code a whitelist,
// or reject seeds at generation — which of those to do is a design call
// (backlog: starting-corp selection), and a sweep that silently enforced one
// would be making that call by implication.
//
// Determinism: every seed is generated from world_params alone and the warm start
// is the same fixed sequence pregame_balance_harness uses, so a rerun reproduces
// the table exactly.
//
// Run: .\build\player_seed_sweep.exe [seed_count] [warm_ticks]

#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

/// The same tick composition pregame_balance_harness runs, so a balance printed
/// here is directly comparable with the one printed there.
void tick(world& w, const recipe_registry& reg, int t)
{
    dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));
    advance_convoys(w);
    const economy_report report = run_economy_step(w, reg);
    const auto flows = clear_markets(w, reg, report);
    apply_budget(w, reg, flows, report.workforce_contention, nullptr);
    credit_arrived_convoys(w, t);
}

void seed_default_recipes(world& w, const recipe_registry& reg)
{
    const entity_id default_recipe = reg.recipe_id("steel");
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = static_cast<uint16_t>(default_recipe);
}

struct seed_row
{
    uint32_t seed        = 0;
    int      processing  = 0;
    int      extraction  = 0;
    int      other       = 0;
    float    opening     = 0.0f;
    float    final_bal   = 0.0f;
    bool     went_negative = false;
    bool     threw       = false;
};

bool playable(const seed_row& r)
{
    return !r.threw && r.processing >= 1 && r.final_bal > 0.0f && !r.went_negative;
}

// --- BL-435: the roster mode ------------------------------------------------
//
// `player_seed_sweep --roster <seed>` prints EVERY corporation's opening for one
// seed, not just the one the generator handed the player. Two jobs, and it is one
// mode because they want exactly the same numbers:
//
//   * It is the measurement behind BL-435. The sweep's headline finding — 13/24
//     seeds give the player a pure-extraction corp — says nothing about whether
//     a BETTER corp existed in the same world to be chosen instead. This answers
//     that directly, and a selection screen is only worth building if it does.
//   * It is the data the selection screen renders. Name, focus, buildings by
//     type, balance, per corp. Producing it here first means the screen is built
//     against a shape already known to be reachable, rather than the other way
//     round.
//
// It deliberately does NOT run the warm start: these are the GENERATED openings,
// which is what a player choosing before the first tick would be choosing between.
struct corp_row
{
    entity_id   id         = null_entity;
    std::string name;
    int         processing = 0;
    int         extraction = 0;
    int         other      = 0;
    float       balance    = 0.0f;
    bool        is_player  = false;
    bool        specialist = false;
};

int run_roster(uint32_t seed, const recipe_registry& reg)
{
    world_params p = no_prehistory();
    p.seed = seed;
    world w = make_hard_coded_world(p);
    seed_default_recipes(w, reg);

    // Snapshot the corp set BEFORE background firms are added. The distinction is
    // load-bearing for BL-435 and invisible afterwards: the generator's player
    // pick draws only from these SPECIALIST corps (corporation_generation.cpp's
    // uniform draw over corp_ids), while generate_background_firms (BL-365) adds
    // the mundane industry that saturates the market. "The world contains corps
    // with processors" is true of the background firms almost by construction, so
    // counting them together would answer a question nobody asked.
    std::vector<entity_id> specialists;
    for (const auto& [id, cc] : w.corporations)
        specialists.push_back(id);
    std::sort(specialists.begin(), specialists.end());

    generate_background_firms(w, reg, seed ^ 0x8A21F00Du);

    std::vector<corp_row> rows;
    for (const auto& [id, cc] : w.corporations)
    {
        corp_row r;
        r.id        = id;
        r.name      = cc.name;
        r.balance   = cc.balance;
        r.is_player = (cc.is_player || id == w.player_entity);
        r.specialist = std::binary_search(specialists.begin(), specialists.end(), id);
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            switch (bit->second.type)
            {
                case building_type::processing_facility: ++r.processing; break;
                case building_type::extraction_site:     ++r.extraction; break;
                default:                                 ++r.other;      break;
            }
        }
        rows.push_back(std::move(r));
    }
    // Sort by entity id: w.corporations is an unordered map, and an unordered
    // walk would make this table differ run to run on the same seed (the BL-406
    // lesson). Ids are assigned in generation order, so this is also the order
    // a selection screen would list them in.
    std::sort(rows.begin(), rows.end(),
              [](const corp_row& a, const corp_row& b) { return a.id < b.id; });

    std::printf("player_seed_sweep --roster %u — every corporation's generated opening\n\n", seed);
    std::printf("  #  kind  proc  extr  other  player  name\n");
    std::printf("---  ----  ----  ----  -----  ------  ----\n");
    int spec_total = 0, spec_proc = 0, bg_total = 0, bg_proc = 0;
    int with_proc = 0, idx = 0;
    for (const corp_row& r : rows)
    {
        if (r.specialist) { ++spec_total; if (r.processing >= 1) ++spec_proc; }
        else              { ++bg_total;   if (r.processing >= 1) ++bg_proc;   }
        if (r.processing >= 1)
            ++with_proc;
        std::printf("%3d  %4s  %4d  %4d  %5d  %6s  %s\n",
                    idx++, r.specialist ? "SPEC" : "bg",
                    r.processing, r.extraction, r.other,
                    r.is_player ? "<<<" : "", r.name.c_str());
    }

    std::printf("\n=== seed %u ===\n", seed);
    std::printf("  SPECIALIST corps (the pool the player pick draws from): %2d, %2d with a processor\n",
                spec_total, spec_proc);
    std::printf("  background firms (BL-365, not currently selectable):    %2d, %2d with a processor\n",
                bg_total, bg_proc);
    for (const corp_row& r : rows)
        if (r.is_player)
            std::printf("  generator handed the player: %s [%s] — %d proc, %d extr, %d other\n",
                        r.name.c_str(), r.specialist ? "specialist" : "background",
                        r.processing, r.extraction, r.other);
    std::printf("  BL-435's premise needs spec_proc >= 1 while the player's own proc == 0.\n");
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    const bool roster_mode = (argc > 1 && std::string(argv[1]) == "--roster");
    const int n_seeds   = (!roster_mode && argc > 1) ? std::atoi(argv[1]) : 24;
    const int warm_ticks = (!roster_mode && argc > 2) ? std::atoi(argv[2]) : 12;
    if (!roster_mode && (n_seeds <= 0 || warm_ticks <= 0))
    {
        std::printf("usage: %s [seed_count] [warm_ticks]  (both positive)\n"
                    "       %s --roster <seed>   (every corp's opening, one seed)\n",
                    argv[0], argv[0]);
        return 2;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    // Vacuity guard (the standing lesson from interbody_pull_harness): a registry
    // that loaded nothing would make every seed look identically poor.
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    // BL-435 roster mode. Placed after the vacuity guard on purpose: a roster
    // printed from an empty registry would show every corp as equally poor.
    if (roster_mode)
        return run_roster(static_cast<uint32_t>(argc > 2 ? std::atoi(argv[2]) : 0), reg);

    std::printf("player_seed_sweep — %d seeds, %d warm ticks (%.2f in-game years)\n\n",
                n_seeds, warm_ticks, warm_ticks / 4.0);
    std::printf("seed  proc  extr  other   opening      final  dipped  verdict\n");
    std::printf("----  ----  ----  -----  --------  ---------  ------  -------\n");

    std::vector<seed_row> rows;
    rows.reserve(static_cast<std::size_t>(n_seeds));

    for (int i = 0; i < n_seeds; ++i)
    {
        seed_row r;
        r.seed = static_cast<uint32_t>(i);
        try
        {
            world_params p = no_prehistory();
            p.seed = r.seed;
            world w = make_hard_coded_world(p);
            seed_default_recipes(w, reg);
            generate_background_firms(w, reg, r.seed ^ 0x8A21F00Du);

            const entity_id corp = w.player_entity;
            const auto      cit  = w.corporations.find(corp);
            if (cit == w.corporations.end())
            {
                r.threw = true;
            }
            else
            {
                for (const entity_id bid : cit->second.assets)
                {
                    const auto bit = w.buildings.find(bid);
                    if (bit == w.buildings.end())
                        continue;
                    switch (bit->second.type)
                    {
                        case building_type::processing_facility: ++r.processing; break;
                        case building_type::extraction_site:     ++r.extraction; break;
                        default:                                 ++r.other;      break;
                    }
                }
                r.opening = cit->second.balance;
                for (int t = 1; t <= warm_ticks; ++t)
                {
                    tick(w, reg, t);
                    if (w.corporations[corp].balance < 0.0f)
                        r.went_negative = true;
                }
                r.final_bal = w.corporations[corp].balance;
            }
        }
        catch (const std::exception& e)
        {
            r.threw = true;
            std::printf("%4u  THREW: %s\n", r.seed, e.what());
        }
        catch (...)
        {
            r.threw = true;
            std::printf("%4u  THREW: unknown\n", r.seed);
        }

        if (!r.threw)
            std::printf("%4u  %4d  %4d  %5d  %8.1f  %9.1f  %6s  %s\n",
                        r.seed, r.processing, r.extraction, r.other,
                        static_cast<double>(r.opening), static_cast<double>(r.final_bal),
                        r.went_negative ? "YES" : "no",
                        playable(r) ? "PLAYABLE" : "-");
        std::fflush(stdout);
        rows.push_back(r);
    }

    // --- Summary: why each rejected seed was rejected ------------------------
    int ok = 0, no_proc = 0, insolvent = 0, dipped = 0, threw = 0;
    std::vector<uint32_t> playable_seeds;
    for (const seed_row& r : rows)
    {
        if (r.threw)                { ++threw;      continue; }
        if (playable(r))            { ++ok; playable_seeds.push_back(r.seed); continue; }
        if (r.processing < 1)         ++no_proc;
        if (r.final_bal <= 0.0f)      ++insolvent;
        if (r.went_negative)          ++dipped;
    }

    std::printf("\n=== %d/%d playable ===\n", ok, n_seeds);
    std::printf("  rejected: %d no processing facility, %d ended insolvent, "
                "%d dipped negative, %d threw\n", no_proc, insolvent, dipped, threw);
    std::printf("  (a seed can be rejected for more than one reason, so these overlap)\n");

    std::printf("\nplayable seeds: ");
    if (playable_seeds.empty())
        std::printf("(none)\n");
    else
    {
        for (std::size_t i = 0; i < playable_seeds.size(); ++i)
            std::printf("%s%u", i ? ", " : "", playable_seeds[i]);
        std::printf("\n");
    }

    // Reports rather than gates: exit 0 unless generation actually threw, which is
    // seed_sweep_probe's failure condition and a real defect either way.
    return threw ? 1 : 0;
}
