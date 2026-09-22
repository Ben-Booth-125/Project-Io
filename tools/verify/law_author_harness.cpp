// ---------------------------------------------------------------------------
// Headless law-author harness (BL-480; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// BL-480: a law has an author, and a tax is a transfer. Exercises the reshaped
// law record (src/world/law.{hpp,cpp}) and the transfer seam in
// budget_system.cpp against the requirement group "law-author-and-treasury":
//
//   R1  CONSERVATION: with the levy enacted by nation N, the sum debited from
//       corps in N's jurisdiction equals the sum credited to N's treasury,
//       bit-exactly, over any tick span — the BL-392 class of silent money
//       destruction is gone from this flow. An out-of-jurisdiction corp pays
//       ZERO (extraction in another nation, or on unclaimed tiles).
//   R2  REPEAL returns the arithmetic bit-identical to the never-enacted
//       baseline (the v0.1.3 inertness rule, now with an author): after repeal
//       and a reset to opening balances, every subsequent tick's balances,
//       budget lines and treasuries match the baseline world's exactly.
//   NB  NO-ENACTMENT BIT-IDENTITY: a world whose seeded law is not enacted
//       runs bit-identical to a world with no laws at all. The pre-BL-480
//       build proved the same equivalence (law_harness L1), and the no-law
//       arithmetic path is untouched by this change (`any_law_enacted` gates
//       the whole pass), so this is the transitive form of "no-enactment world
//       bit-identical to the pre-change build" that a single binary can state
//       honestly.
//   AC  AUTHOR CHOICE is deterministic generation output: player's home
//       nation first; else the largest territory, ties to the lowest entity
//       id; no nations, no law. An ill-formed authorless record charges and
//       transfers nothing.
//
// R3's live half (the balance ledger renders its levy line read-only; no
// enact control remains) is a UI check and is owed to the main session's live
// pass — a harness cannot click.
//
// The process exits non-zero if any assertion FAILs.

#include "world/budget_system.hpp"
#include "world/economy_system.hpp"
#include "world/law.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool same(float a, float b) { return a == b; } // bit-identical, deliberately not epsilon

/// Two nations, three payers-in-question:
///   corp_a1, corp_a2 — extraction inside nation A (the enacting nation);
///   corp_b           — extraction inside nation B (out of A's jurisdiction);
///   corp_u           — extraction on unclaimed tiles (no jurisdiction at all).
/// All outputs are small integers so every float sum below is exact.
struct fixture
{
    world     w;
    entity_id body     = null_entity;
    entity_id nation_a = null_entity;
    entity_id nation_b = null_entity;
    entity_id corp_a1  = null_entity;
    entity_id corp_a2  = null_entity;
    entity_id corp_b   = null_entity;
    entity_id corp_u   = null_entity;
    std::vector<building_report> production;
};

entity_id add_corp(world& w, const char* name, entity_id home, bool player)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name        = name;
    cc.balance     = 10000.0f;
    cc.is_player   = player;
    cc.home_nation = home;
    w.corporations[c] = cc;
    if (player)
        w.player_entity = c;
    return c;
}

void add_extraction(fixture& f, entity_id corp, entity_id nation,
                    resource_type res, float qty)
{
    const entity_id tile = f.w.create_entity();
    tile_component  tc{};
    tc.body = f.body;
    f.w.tiles[tile] = tc;
    if (nation != null_entity)
    {
        f.w.tile_to_nation[tile] = nation;
        f.w.nations[nation].tiles.push_back(tile);
    }

    const entity_id b = f.w.create_entity();
    building_component bc{};
    bc.type               = building_type::extraction_site;
    bc.tile               = tile;
    bc.workforce_target   = 0.0f; // no opex noise: the levy is the only variable
    bc.workforce_assigned = 0.0f;
    f.w.buildings[b] = bc;
    f.w.corporations[corp].assets.push_back(b);

    building_report br;
    br.building        = b;
    br.corp            = corp;
    br.body            = f.body;
    br.type            = building_type::extraction_site;
    br.target_resource = res;
    br.active          = true;
    br.output_quantity = qty;
    f.production.push_back(br);
}

fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};

    f.nation_a = f.w.create_entity();
    nation_component na{};
    na.name = "Authoria";
    f.w.nations[f.nation_a] = na;

    f.nation_b = f.w.create_entity();
    nation_component nb{};
    nb.name = "Elsewhere";
    f.w.nations[f.nation_b] = nb;

    f.corp_a1 = add_corp(f.w, "In-Scope One", f.nation_a, /*player=*/true);
    f.corp_a2 = add_corp(f.w, "In-Scope Two", f.nation_a, /*player=*/false);
    f.corp_b  = add_corp(f.w, "Foreign Corp", f.nation_b, /*player=*/false);
    f.corp_u  = add_corp(f.w, "Frontier Corp", null_entity, /*player=*/false);

    // In A's jurisdiction: 30 + 20 iron ore (corp_a1), 40 copper ore (corp_a2).
    add_extraction(f, f.corp_a1, f.nation_a, resource_type::iron_ore, 30.0f);
    add_extraction(f, f.corp_a1, f.nation_a, resource_type::iron_ore, 20.0f);
    add_extraction(f, f.corp_a2, f.nation_a, resource_type::copper_ore, 40.0f);
    // In B's jurisdiction: 25 iron ore (corp_b) — never charged by A's law.
    add_extraction(f, f.corp_b, f.nation_b, resource_type::iron_ore, 25.0f);
    // Unclaimed: 35 iron ore (corp_u) — outside every jurisdiction.
    add_extraction(f, f.corp_u, null_entity, resource_type::iron_ore, 35.0f);
    return f;
}

void enact(world& w, entity_id author, float rate)
{
    law l;
    l.id      = "LAW-EXTRACTION-LEVY";
    l.name    = "Extraction Levy";
    l.effect  = law_effect_kind::extraction_levy;
    l.rate    = rate;
    l.scope_resource  = law::all_resources;
    l.enacting_nation = author;
    l.enacted = true;
    w.laws.push_back(l);
}

/// One budget tick; returns the per-corp breakdown.
std::map<entity_id, corp_budget> tick(fixture& f, const recipe_registry& reg)
{
    const std::unordered_map<entity_id, corp_cash_flow>    flows;
    const std::map<std::pair<entity_id, entity_id>, float> contention;
    std::map<entity_id, corp_budget>                       breakdown;
    apply_budget(f.w, reg, flows, contention, &breakdown, &f.production);
    return breakdown;
}

} // namespace

int main()
{
    std::printf("=== law author harness (BL-480) ===\n\n");

    recipe_registry reg;
    constexpr float k_rate = 2.0f;
    // Expected per tick, rate 2: corp_a1 pays 2 x 50 = 100; corp_a2 pays 2 x 40
    // = 80; A's treasury gains 180. corp_b and corp_u pay 0.
    constexpr float k_a1_levy   = 100.0f;
    constexpr float k_a2_levy   = 80.0f;
    constexpr float k_per_tick  = 180.0f;
    constexpr int   k_span      = 8;

    // --- R1: conservation + scope over a tick span --------------------------
    std::printf("-- R1  the levy is a transfer, bounded by jurisdiction --\n");
    {
        fixture f = make_fixture();
        enact(f.w, f.nation_a, k_rate);

        float debited_total = 0.0f;
        bool  per_tick_ok = true, scope_ok = true, foreign_ok = true;
        for (int t = 0; t < k_span; ++t)
        {
            const float treasury_before = f.w.nations[f.nation_a].treasury;
            const auto  bud = tick(f, reg);

            const float debited = bud.at(f.corp_a1).levies + bud.at(f.corp_a2).levies;
            debited_total += debited;

            per_tick_ok = per_tick_ok &&
                same(bud.at(f.corp_a1).levies, k_a1_levy) &&
                same(bud.at(f.corp_a2).levies, k_a2_levy) &&
                same(f.w.nations[f.nation_a].treasury - treasury_before, k_per_tick);
            scope_ok   = scope_ok &&
                bud.at(f.corp_b).levies == 0.0f &&
                bud.at(f.corp_u).levies == 0.0f;
            foreign_ok = foreign_ok && f.w.nations[f.nation_b].treasury == 0.0f;
        }
        check(per_tick_ok,
              "R1a every tick: each in-scope corp pays rate x its in-territory "
              "output, and A's treasury gains exactly the tick's debits");
        check(same(f.w.nations[f.nation_a].treasury, debited_total),
              "R1b over the span: sum(debited) == treasury credited, bit-exact "
              "- money conserved, nothing destroyed");
        check(same(f.w.nations[f.nation_a].treasury,
                   k_per_tick * static_cast<float>(k_span)),
              "R1c ...and the total is the arithmetic total (180 x 8 = 1440)");
        check(scope_ok,
              "R1d out-of-jurisdiction corps pay ZERO (foreign territory and "
              "unclaimed tiles alike)");
        check(foreign_ok,
              "R1e the non-enacting nation's treasury never moves");
    }

    // --- R2: repeal returns to the never-enacted baseline -------------------
    std::printf("\n-- R2  repeal restores the baseline bit-identically --\n");
    {
        fixture baseline = make_fixture(); // never enacted: no laws at all
        fixture repealed = make_fixture();
        enact(repealed.w, repealed.nation_a, k_rate);

        for (int t = 0; t < 3; ++t)
            tick(repealed, reg); // charged for three ticks

        repealed.w.laws.front().enacted = false; // the author repeals
        // Reset the flows the enacted period moved, back to opening values —
        // what remains under test is whether any RESIDUE of enactment perturbs
        // the arithmetic after repeal (a one-way ratchet would).
        for (auto& [cid, cc] : repealed.w.corporations)
            cc.balance = 10000.0f;
        repealed.w.nations[repealed.nation_a].treasury = 0.0f;

        bool identical = true;
        for (int t = 0; t < k_span; ++t)
        {
            const auto bb = tick(baseline, reg);
            const auto rb = tick(repealed, reg);
            for (const auto& [cid, cb] : bb)
            {
                const corp_budget& rc = rb.at(cid);
                identical = identical &&
                    same(cb.net(), rc.net()) &&
                    cb.levies == 0.0f && rc.levies == 0.0f;
            }
        }
        check(identical,
              "R2a after repeal, every tick's budgets match the never-enacted "
              "baseline bit-identically (levies zero on both sides)");

        bool balances_match = true;
        for (const auto& [cid, cc] : baseline.w.corporations)
            balances_match = balances_match &&
                same(cc.balance, repealed.w.corporations.at(cid).balance);
        check(balances_match,
              "R2b ...and the corp balances are bit-identical after the span");
        check(repealed.w.nations[repealed.nation_a].treasury == 0.0f &&
              repealed.w.nations[repealed.nation_b].treasury == 0.0f,
              "R2c ...and no treasury moves after repeal");
    }

    // --- NB: a no-enactment world is bit-identical to a no-law world --------
    // The pre-BL-480 build proved un-enacted == no-laws (law_harness L1); the
    // no-law path's gate (`any_law_enacted`) is unchanged, so equality here is
    // the single-binary form of "bit-identical to the pre-change build".
    std::printf("\n-- NB  no enactment, no trace --\n");
    {
        fixture lawless  = make_fixture();
        fixture repealed = make_fixture();
        enact(repealed.w, repealed.nation_a, k_rate);
        repealed.w.laws.front().enacted = false;

        bool identical = true;
        for (int t = 0; t < k_span; ++t)
        {
            tick(lawless, reg);
            tick(repealed, reg);
            for (const auto& [cid, cc] : lawless.w.corporations)
                identical = identical &&
                    same(cc.balance, repealed.w.corporations.at(cid).balance);
        }
        check(identical,
              "NB1 a world whose law is never enacted runs bit-identical to a "
              "world with no laws at all, every tick");
        check(repealed.w.nations[repealed.nation_a].treasury == 0.0f,
              "NB2 ...and its treasury stays exactly zero");
    }

    // --- AC: author choice is deterministic; authorless is inert ------------
    std::printf("\n-- AC  the author invariant --\n");
    {
        fixture f = make_fixture();
        seed_prototype_laws(f.w, k_rate);
        // BL-741: EVERY nation authors its own levy over its own jurisdiction
        // (one law per nation, sorted author order, all enacted) — one seeded
        // author left 42 of 43 treasuries empty and every state demand channel
        // unfunded. The single-author contract this row used to pin is gone.
        const entity_id lo = std::min(f.nation_a, f.nation_b);
        const entity_id hi = std::max(f.nation_a, f.nation_b);
        check(f.w.laws.size() == 2 &&
              f.w.laws[0].enacting_nation == lo && f.w.laws[0].enacted &&
              f.w.laws[1].enacting_nation == hi && f.w.laws[1].enacted &&
              f.w.laws[0].id != f.w.laws[1].id,
              "AC1 (BL-741) every nation authors its own enacted levy, sorted "
              "author order, distinct law ids");

        // No player home nation: the largest territory wins, ties to lowest id.
        world w2;
        const entity_id small = w2.create_entity();
        const entity_id big   = w2.create_entity();
        w2.nations[small] = nation_component{};
        w2.nations[big]   = nation_component{};
        w2.nations[small].tiles.resize(3);
        w2.nations[big].tiles.resize(5);
        check(choose_levy_author(w2) == big,
              "AC2 fallback author is the largest territory");
        w2.nations[small].tiles.resize(5);
        check(choose_levy_author(w2) == std::min(small, big),
              "AC3 ...ties break to the lowest entity id");

        // An ill-formed authorless record charges and transfers nothing.
        fixture g = make_fixture();
        enact(g.w, null_entity, k_rate); // cannot exist; must be inert
        const auto bud = tick(g, reg);
        check(bud.at(g.corp_a1).levies == 0.0f &&
              g.w.nations[g.nation_a].treasury == 0.0f,
              "AC4 an authorless record is defensively inert - no charge, no "
              "transfer");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
