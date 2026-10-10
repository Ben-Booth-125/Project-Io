// nation_tech_harness — BL-1273 (every nation holds a tree state, a research
// pool and a queue). docs/economy/RESEARCH.md is the authority; requirement
// group "nation-tech-state" rows R0-R3 (R3's canonical-world half is
// save_roundtrip P5).
//
// NOTHING IN src/ CALLS enqueue_node YET. That is correct: the nation research
// AI (BL-1275) is the scorer that queues, and the panel (BL-1276) is the
// player's verb. Until they land, no nation in a real campaign ever queues, so
// the line never spends and no node completes in play. R0 therefore SCRIPTS a
// queue for every nation after the seat; its pacing rows measure what the
// research arithmetic would do, not what a campaign does today.
//
//   R1  accrual: the academic-research line pays min(its share, what the queue
//       still owes) out of the treasury; an empty queue spends nothing.
//       Universities (where they stand), research institutes (by owner's home
//       nation) and the per-nation base add RP at no treasury cost, empty queue
//       or not; under construction or decommissioned counts nothing. The
//       conversion's three terms (base, linear, sqrt) are dials. A world with
//       nothing to research keeps an empty map and an unmoved state_hash.
//       Deterministic twice.
//   R2  the queue: requirements queued ahead (AND over groups, the cheapest
//       member of a choice); a pick-one pair refused, mutating nothing; RP flow
//       to the head and the remainder flows on; a deed-unmet quest is passed
//       over and stays queued.
//   R3  the save seam on a fixture: round trip exact; the loader refuses a
//       duplicate queue entry, a queued node already held, both sides of a
//       pair, and a negative pool.
//   R0  a campaign run over the curated seeds (docs/generation/seed_library.json):
//       STRUCTURAL rows (the band and "nothing held at open" hold by
//       construction — kept as guards, labelled as such); every nation's pool
//       and queue empty at the seat (the settle's handover); then every nation
//       queues the DG-PR-EX-A path and the PACING rows hold the arithmetic to
//       the pacing targets — they may FAIL until the conversion is set, and are
//       reported, never weakened. The run is byte-identical twice. Each seed
//       also prints the launchpads standing and inter-body convoys in flight,
//       seat through live tick 20 (what Rocketry's launch gate touches).
//
// BL-1274, requirement group "tech-effects-wired":
//   T1  (R1) the effect audit: every node carries >= 1 machine effect; every
//       row of the generated table is read or declared unread by
//       nation_tech.cpp (classify_effect) against the live registry; the store's
//       kind agrees; the code's declared-unread systems are exactly the systems
//       the store waits on. (Store == header is gen_tech_tree_table.js --check.)
//   T2  (R2) the lever rows: every read modifier row moves nation_scalar by
//       exactly its value for the holding nation and not another; extraction
//       through extraction_nominal (Farms are the Farm); processing through
//       credit_processing_outputs; research through advance_research;
//       Rocketry's space_dispatch; logistics_cost through price_convoy_leg on
//       one generated world (haul only, never port handling).
//
// Usage: nation_tech_harness [--seeds N | --seed-list a,b,...] [--quarters Q] [--once]
//                            [--base X] [--lin Y] [--sqrt Z] [--unit U]
//   --seeds 0   fixture rows (R1-R3, T1, T2) plus T2-live's one generated world — the ctest invocation.
//   default     all 16 library seeds, 32 live quarters, each seed built twice,
//               on the registry's research dials (scripts/economy.lua) unless
//               --base/--lin/--sqrt/--unit override them for a measurement.
//
// Build: bash tools/verify/build_lua_harness.sh nation_tech_harness

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/nation_budget.hpp"
#include "world/nation_tech.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/spawn_seat.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_failures;
}

int node(const char* id)
{
    const int n = io::dg::find_node(id);
    if (n < 0)
    {
        std::printf("  FAIL  unknown node id %s (store changed?)\n", id);
        ++g_failures;
    }
    return n;
}

std::int64_t milli(double rp) { return static_cast<std::int64_t>(std::llround(rp * 1000.0)); }

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id nation = null_entity;
    entity_id other  = null_entity; // a second nation
    entity_id corp   = null_entity; // homed in `nation`
    entity_id corp2  = null_entity; // homed in `other`
};

entity_id add_tile(world& w, entity_id body, entity_id owner_nation)
{
    const entity_id t = w.create_entity();
    tile_component tc{};
    tc.body = body;
    w.tiles[t] = tc;
    if (owner_nation != null_entity)
    {
        w.tile_to_nation[t] = owner_nation;
        w.nations.at(owner_nation).tiles.push_back(t);
    }
    return t;
}

entity_id add_building(world& w, entity_id corp, entity_id tile, building_type type,
                       int ticks_remaining, bool decommissioned, float workforce = -1.0f)
{
    const entity_id b = w.create_entity();
    building_component bc{};
    bc.tile               = tile;
    bc.type               = type;
    bc.ticks_remaining    = ticks_remaining;
    bc.decommissioned     = decommissioned;
    // Staffed as the build door staffs it (construction_initial_workforce), unless a
    // row deliberately overrides it.
    bc.workforce_assigned = workforce < 0.0f ? construction_initial_workforce(type) : workforce;
    w.buildings[b] = bc;
    w.corporations.at(corp).assets.push_back(b);
    return b;
}

fixture make_fixture(float treasury = 10000.0f)
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};
    f.nation = f.w.create_entity();
    f.other  = f.w.create_entity();
    nation_component n{};
    n.treasury = treasury;
    f.w.nations[f.nation] = n;
    nation_component o{};
    o.treasury = 0.0f;
    f.w.nations[f.other] = o;

    nation_budget nb;
    nb.weights[static_cast<std::size_t>(budget_priority::academic_research)] = 0.1f;
    nb.weights[static_cast<std::size_t>(budget_priority::schooling)]         = 0.9f;
    nb.reserve_fraction = 0.0f;
    f.w.nation_budgets[f.nation] = nb;

    f.corp = f.w.create_entity();
    corporation_component cc{};
    cc.home_nation = f.nation;
    f.w.corporations[f.corp] = cc;
    f.corp2 = f.w.create_entity();
    corporation_component c2{};
    c2.home_nation = f.other;
    f.w.corporations[f.corp2] = c2;
    return f;
}

const research_accrual* row_for(const std::vector<research_accrual>& rows, entity_id n)
{
    for (const research_accrual& r : rows)
        if (r.nation == n) return &r;
    return nullptr;
}

// ---------------------------------------------------------------------------
// R1 — accrual
// ---------------------------------------------------------------------------

void rows_r1()
{
    std::printf("\nR1 accrual: the research line pays what the queue owes; buildings and base are free\n");
    research_params p{};
    p.base_rp_per_nation = 0.0f;
    p.rp_per_research_credit = 12.0f;
    p.rp_per_research_credit_sqrt = 0.0f;
    p.rp_per_university = 100.0f;
    p.rp_per_institute = 50.0f;
    p.rp_per_cost_unit = 100.0f;

    fixture f = make_fixture();
    const entity_id home_tile  = add_tile(f.w, f.body, f.nation);
    const entity_id other_tile = add_tile(f.w, f.body, f.other);
    // A completed University on the nation's ground, owned by the OTHER
    // nation's corp: a University counts where it STANDS.
    add_building(f.w, f.corp2, home_tile, building_type::university, 0, false);
    add_building(f.w, f.corp, home_tile, building_type::university, 3, false);  // under construction
    add_building(f.w, f.corp, home_tile, building_type::university, 0, true);   // decommissioned
    // An institute counts for its OWNER's home nation, wherever it stands.
    add_building(f.w, f.corp, other_tile, building_type::research_institute, 0, false);
    add_building(f.w, f.corp2, other_tile, building_type::research_institute, 0, false);
    add_building(f.w, f.corp2, other_tile, building_type::research_institute, 5, false);
    // BL-1274 GUARD: STAFFED only. No door makes an unstaffed University or institute
    // today (both staff at construction); one with no workforce counts nothing.
    add_building(f.w, f.corp, home_tile, building_type::university, 0, false, 0.0f);
    add_building(f.w, f.corp, other_tile, building_type::research_institute, 0, false, 0.0f);

    // Quarter 1, empty queues: no treasury moves; the buildings still accrue.
    world twin = f.w;
    std::vector<research_accrual> rows;
    advance_research(f.w, p, &rows);
    const research_accrual* a = row_for(rows, f.nation);
    const research_accrual* b = row_for(rows, f.other);
    check(a != nullptr && b != nullptr && rows.size() == 2, "R1a one row per nation");
    if (a == nullptr || b == nullptr) return;
    check(a->line_share > 999.0f && a->line_spend == 0.0f
              && f.w.nations.at(f.nation).treasury == 10000.0f,
          "R1b an EMPTY queue owes nothing: the line spends nothing, the treasury keeps its share");
    check(a->universities == 1 && a->institutes == 1 && b->universities == 0 && b->institutes == 1,
          "R1c STAFFED Universities count where they stand, institutes by owner's nation; building, torn-down or unstaffed count nothing");
    check(f.w.nation_tech.at(f.nation).pool_milli == milli(150.0)
              && f.w.nation_tech.at(f.other).pool_milli == milli(50.0),
          "R1d ...and they accrue RP with an empty queue, at no treasury cost");

    // Quarter 2: queue the DG-PR-EX-A path (18 units = 1800 RP). Owed 1800 -
    // pool 150 - free 150 = 1500 RP = 125 credits at 12 RP/credit, well under
    // the 1000-credit share: the line pays the bill, not the share.
    enqueue_node(f.w, f.nation, node("DG-PR-EX-A"));
    rows.clear();
    advance_research(f.w, p, &rows);
    a = row_for(rows, f.nation);
    std::printf("     quarter 2: share %.1f  owed %lld milli  spend %.3f  RP %lld milli  completed %d\n",
                a->line_share, static_cast<long long>(a->owed_milli), a->line_spend,
                static_cast<long long>(a->rp_milli), a->completed);
    check(std::fabs(a->line_spend - 125.0f) < 1e-3f
              && std::fabs(f.w.nations.at(f.nation).treasury - 9875.0f) < 1e-2f,
          "R1e the line pays min(share, what the queue owes): 125 of a 1000 share");
    check(a->completed == 5 && f.w.nation_tech.at(f.nation).pool_milli == 0
              && f.w.nation_tech.at(f.nation).held.test(static_cast<std::size_t>(node("DG-PR-EX-A"))),
          "R1f ...exactly enough: the five-node path completes with nothing left over");

    // A long queue on a poor nation: the share caps the spend.
    {
        fixture g = make_fixture(100.0f);
        enqueue_node(g.w, g.nation, node("DG-PR-EX-M"));
        std::vector<research_accrual> r2;
        advance_research(g.w, p, &r2);
        const research_accrual* ga = row_for(r2, g.nation);
        check(ga != nullptr && std::fabs(ga->line_spend - 10.0f) < 1e-4f
                  && ga->rp_milli == milli(120.0) && std::fabs(g.w.nations.at(g.nation).treasury - 90.0f) < 1e-4f,
              "R1g ...and a share smaller than the bill is spent whole (10 credits -> 120 RP)");
    }

    // The conversion's shape is a dial: base + linear + sqrt.
    {
        research_params q = p;
        q.base_rp_per_nation = 5.0f;
        q.rp_per_research_credit = 2.0f;
        q.rp_per_research_credit_sqrt = 30.0f;
        check(line_rp_milli(100.0f, q) == milli(2.0 * 100.0 + 30.0 * 10.0),
              "R1h the line converts credits by linear + sqrt terms");
        check(std::fabs(line_credits_for(milli(500.0), q) - 100.0) < 1e-6,
              "R1i ...and the inverse returns the credits that buy a given RP");
        fixture g = make_fixture(0.0f);
        std::vector<research_accrual> r3;
        advance_research(g.w, q, &r3);
        check(row_for(r3, g.nation)->rp_milli == milli(5.0) && g.w.nation_tech.at(g.nation).pool_milli == milli(5.0),
              "R1j the base accrues to every nation, empty queue and empty treasury alike");
    }

    // Nothing to research: the map stays empty and state_hash does not move.
    {
        fixture g = make_fixture();
        const std::uint64_t before = g.w.state_hash(0);
        advance_research(g.w, p);
        check(g.w.nation_tech.empty() && g.w.state_hash(0) == before
                  && g.w.nations.at(g.nation).treasury == 10000.0f,
              "R1k with no source of RP and no queue, no entry is created and the hash is unmoved");
    }

    // Deterministic: the twin, stepped identically, carries identical bits.
    advance_research(twin, p);
    enqueue_node(twin, f.nation, node("DG-PR-EX-A"));
    advance_research(twin, p);
    check(twin.state_hash(0) == f.w.state_hash(0)
              && twin.nation_tech.at(f.nation).pool_milli == f.w.nation_tech.at(f.nation).pool_milli,
          "R1l the same inputs accrue the same bits");
}

// ---------------------------------------------------------------------------
// R2 — the queue
// ---------------------------------------------------------------------------

std::string queue_ids(const std::vector<std::uint16_t>& q)
{
    std::string s;
    for (const std::uint16_t n : q)
    {
        if (!s.empty()) s += " ";
        s += io::dg::nodes[n].id;
    }
    return s;
}

void rows_r2()
{
    std::printf("\nR2 the queue\n");
    research_params p{};
    p.rp_per_cost_unit = 100.0f;

    {
        fixture f = make_fixture();
        const enqueue_result r = enqueue_node(f.w, f.nation, node("DG-PR-EX-A"));
        const auto& q = f.w.nation_tech[f.nation].queue;
        std::printf("     queue after enqueue(DG-PR-EX-A): %s\n", queue_ids(q).c_str());
        check(r == enqueue_result::queued && queue_ids(q) ==
                  "DG-PR-EX-T1 DG-PR-EX-T2 DG-PR-EX-A1 DG-PR-EX-A2 DG-PR-EX-A",
              "R2a requirements queue ahead, every AND group, in order");
        check(enqueue_node(f.w, f.nation, node("DG-PR-EX-A")) == enqueue_result::already_queued
                  && q.size() == 5,
              "R2b re-queuing a queued node changes nothing");
        check(enqueue_node(f.w, f.nation, node("DG-PR-EX-T3")) == enqueue_result::queued
                  && q.size() == 6 && io::dg::nodes[q.back()].id == std::string("DG-PR-EX-T3"),
              "R2c a requirement already queued is not queued twice");
        check(enqueue_node(f.w, null_entity, node("DG-PR-EX-A")) == enqueue_result::invalid
                  && enqueue_node(f.w, f.nation, io::dg::node_count) == enqueue_result::invalid,
              "R2d an unknown nation or node is refused");
    }

    {
        fixture f = make_fixture();
        const int ba = node("DG-ST-OR-Ba");
        const int bb = node("DG-ST-OR-Bb");
        const int m  = node("DG-ST-OR-M"); // requires [[Ba, Bb]]
        check(enqueue_node(f.w, f.nation, m) == enqueue_result::queued,
              "R2e a node with a choice group queues");
        const auto q = f.w.nation_tech[f.nation].queue;
        const bool has_ba = std::find(q.begin(), q.end(), ba) != q.end();
        const bool has_bb = std::find(q.begin(), q.end(), bb) != q.end();
        check(has_ba && !has_bb, "R2f ...through ONE member of the choice (equal cost: the earlier)");
        const world before = f.w;
        check(enqueue_node(f.w, f.nation, bb) == enqueue_result::refused_pair,
              "R2g a queue holding one side of a pair refuses the other");
        check(f.w.nation_tech[f.nation].queue == before.nation_tech.at(f.nation).queue
                  && f.w.state_hash(0) == before.state_hash(0),
              "R2h ...and the refusal mutates nothing");
        fixture g = make_fixture();
        g.w.nation_tech[g.nation].held.set(static_cast<std::size_t>(bb));
        check(enqueue_node(g.w, g.nation, ba) == enqueue_result::refused_pair
                  && g.w.nation_tech[g.nation].queue.empty(),
              "R2i a HELD pair side refuses the other, queue untouched");
        check(enqueue_node(g.w, g.nation, m) == enqueue_result::queued,
              "R2j a choice group satisfied by the held side queues without it");
    }

    {
        fixture f = make_fixture();
        enqueue_node(f.w, f.nation, node("DG-PR-EX-A1")); // T1 T2 A1
        nation_tech_state& st = f.w.nation_tech[f.nation];
        st.pool_milli = milli(100.0 + 100.0 + 50.0); // T1 and T2, and 50 over
        const int done = flow_research_queue(f.w, f.nation, p);
        check(done == 2 && st.held.test(static_cast<std::size_t>(node("DG-PR-EX-T1")))
                  && st.held.test(static_cast<std::size_t>(node("DG-PR-EX-T2"))),
              "R2k RP flow to the head; each node completes when its cost is met");
        check(st.pool_milli == milli(50.0) && st.queue.size() == 1,
              "R2l the remainder stays with the next entry (A1 at 200 waits with 50)");
        st.pool_milli += milli(150.0);
        check(flow_research_queue(f.w, f.nation, p) == 1 && st.pool_milli == 0 && st.queue.empty(),
              "R2m ...and completes it once the remainder plus new RP cover its cost");
        // Small increments are not lost: 10,000 thousandths make ten RP exactly.
        nation_tech_state acc;
        for (int i = 0; i < 10000; ++i) acc.pool_milli += 1;
        check(acc.pool_milli == milli(10.0), "R2n a milli-RP pool loses no small increment");
    }

    {
        fixture f = make_fixture();
        const int q = node("DG-PR-EX-Q"); // requires A
        enqueue_node(f.w, f.nation, q);
        enqueue_node(f.w, f.nation, node("DG-PR-EX-T3"));
        nation_tech_state& st = f.w.nation_tech[f.nation];
        std::int64_t path = 0;
        for (const char* id : {"DG-PR-EX-T1", "DG-PR-EX-T2", "DG-PR-EX-A1", "DG-PR-EX-A2",
                               "DG-PR-EX-A", "DG-PR-EX-T3"})
            path += node_cost_milli(node(id), p);
        st.pool_milli = path + node_cost_milli(q, p);
        check(!deed_met(f.w, f.nation, q) && io::dg::nodes[static_cast<std::size_t>(q)].deed_waits_on[0] != '\0',
              "R2o a deed declared unread reads unmet");
        const int done = flow_research_queue(f.w, f.nation, p);
        check(done == 6 && st.held.test(static_cast<std::size_t>(node("DG-PR-EX-T3")))
                  && !st.held.test(static_cast<std::size_t>(q)),
              "R2p the deed-unmet quest is passed over: RP flow past it to T3");
        check(st.queue.size() == 1 && st.queue[0] == q && st.pool_milli == node_cost_milli(q, p),
              "R2q ...and it stays queued, its cost waiting in the pool");
    }
}

// ---------------------------------------------------------------------------
// R3 — the save seam on a fixture
// ---------------------------------------------------------------------------

bool round_trip(const world& src, world& dst)
{
    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    write_world_snapshot(src, ss);
    return read_world_snapshot(dst, ss);
}

void rows_r3()
{
    std::printf("\nR3 the save seam (fixture; save_roundtrip P5 runs it on a generated world)\n");
    fixture f = make_fixture();
    enqueue_node(f.w, f.nation, node("DG-NE-SO-A"));
    f.w.nation_tech[f.nation].held.set(static_cast<std::size_t>(node("DG-ST-FO-T1")));
    f.w.nation_tech[f.nation].pool_milli = 12375;
    f.w.nation_tech[f.other].pool_milli  = 1; // one thousandth of an RP survives

    world back;
    const bool ok = round_trip(f.w, back);
    check(ok, "R3a a world carrying nation tech state reads back");
    bool same = ok && back.nation_tech.size() == f.w.nation_tech.size();
    if (same)
        for (const auto& [nid, st] : f.w.nation_tech)
        {
            const auto it = back.nation_tech.find(nid);
            same = same && it != back.nation_tech.end() && it->second.held == st.held
                && it->second.pool_milli == st.pool_milli && it->second.queue == st.queue;
        }
    check(same, "R3b held set, pool (milli-RP, the in-progress remainder) and queue, exactly");
    check(world_save_version == 47u, "R3c at world_save_version 47");

    auto refused = [&](void (*corrupt)(world&, const fixture&), const char* what) {
        world bad = f.w;
        corrupt(bad, f);
        world dest = f.w;
        const std::uint64_t before = dest.state_hash(0);
        check(!round_trip(bad, dest) && dest.state_hash(0) == before, what);
    };
    refused([](world& w, const fixture& fx) {
                auto& q = w.nation_tech[fx.nation].queue;
                q.push_back(q.front());
            }, "R3d a duplicate queue entry refuses the stream, destination untouched");
    refused([](world& w, const fixture& fx) {
                auto& st = w.nation_tech[fx.nation];
                st.held.set(st.queue.front());
            }, "R3e a queued node already held refuses the stream");
    refused([](world& w, const fixture& fx) {
                auto& st = w.nation_tech[fx.nation];
                st.held.set(static_cast<std::size_t>(io::dg::find_node("DG-ST-OR-Ba")));
                st.queue.push_back(static_cast<std::uint16_t>(io::dg::find_node("DG-ST-OR-Bb")));
            }, "R3f both sides of a pick-one pair (one held, one queued) refuse the stream");
    refused([](world& w, const fixture& fx) { w.nation_tech[fx.other].pool_milli = -1; },
            "R3g a negative pool refuses the stream");
}

// ---------------------------------------------------------------------------
// T1/T2 — tech-effects-wired (BL-1274): the effect audit and the lever rows
// ---------------------------------------------------------------------------

const char* reading_name(effect_reading r)
{
    switch (r)
    {
        case effect_reading::read:     return "read";
        case effect_reading::unread:   return "unread";
        case effect_reading::invalid:  return "INVALID";
    }
    return "?";
}

const char* kind_name(io::dg::effect_kind k)
{
    switch (k)
    {
        case io::dg::effect_kind::modifier:         return "modifier";
        case io::dg::effect_kind::unlock_recipe:    return "unlock_recipe";
        case io::dg::effect_kind::unlock_structure: return "unlock_structure";
        case io::dg::effect_kind::mechanic:         return "mechanic";
        case io::dg::effect_kind::unread:           return "unread";
    }
    return "?";
}

/// T1 — R1: every node carries >= 1 effect; the store's rows (via the generated
/// header; gen_tech_tree_table.js --check holds store == header) agree with the
/// code's declaration (classify_effect, declared_unread_systems).
void rows_t1(const recipe_registry& reg)
{
    std::printf("\nT1 the effect audit (R1): store == header == the code's declaration\n");
    bool every_node = true, contiguous = true;
    std::uint16_t expect = 0;
    for (const io::dg::node& n : io::dg::nodes)
    {
        every_node = every_node && n.effect_end > n.effect_begin;
        contiguous = contiguous && n.effect_begin == expect;
        expect = n.effect_end;
    }
    contiguous = contiguous && expect == io::dg::effect_count;
    check(every_node, "T1a every node carries at least one machine effect");
    check(contiguous, "T1b the node slices tile the effect table exactly");

    std::map<std::string, int> tally;
    std::map<std::string, int> waits;
    int invalid = 0, kind_mismatch = 0;
    for (const io::dg::effect& e : io::dg::effects)
    {
        const effect_reading r = classify_effect(e, &reg);
        ++tally[std::string(kind_name(e.kind)) + " -> " + reading_name(r)];
        if (r == effect_reading::invalid)
        {
            ++invalid;
            std::printf("     INVALID  %s  %s %s %s %s\n", io::dg::nodes[static_cast<std::size_t>(e.node)].id,
                        kind_name(e.kind), e.subject, e.scope, e.target[0] ? e.target : e.waits_on);
        }
        // The store's KIND must say what the code does: an unread row is
        // unread and every other row is read — two states, no third.
        const bool store_unread = e.kind == io::dg::effect_kind::unread;
        if (r != effect_reading::invalid && store_unread != (r == effect_reading::unread))
            ++kind_mismatch;
        if (store_unread) ++waits[e.waits_on];
    }
    for (const io::dg::node& n : io::dg::nodes)
        if (n.deed_waits_on[0] != '\0') ++waits[n.deed_waits_on];
    for (const auto& [k, c] : tally) std::printf("     %-34s %3d rows\n", k.c_str(), c);
    check(invalid == 0, "T1c every effect row is read or declared unread (none INVALID)");
    check(kind_mismatch == 0, "T1d the store's kind agrees with the code: unread rows unread, the rest read");

    // The declared-unread list and the store's waits_on set are the same set.
    const auto& declared = declared_unread_systems();
    int undeclared = 0, unused = 0;
    for (const auto& [sys, c] : waits)
        if (std::find(declared.begin(), declared.end(), std::string_view(sys)) == declared.end())
        {
            ++undeclared;
            std::printf("     waits_on not declared in code: %s\n", sys.c_str());
        }
    for (const std::string_view d : declared)
        if (waits.find(std::string(d)) == waits.end())
        {
            ++unused;
            std::printf("     declared in code, waited on by nothing: %.*s\n", static_cast<int>(d.size()), d.data());
        }
    check(undeclared == 0 && unused == 0,
          "T1e the code's declared-unread systems are exactly the systems the store waits on");

    // Placeholders and quests.
    bool placeholders = true, quests = true;
    int  quests_n = 0, deeds_open = 0;
    for (std::size_t i = 0; i < io::dg::nodes.size(); ++i)
    {
        const io::dg::node& n = io::dg::nodes[i];
        if (n.is_placeholder)
            placeholders = placeholders && n.effect_end - n.effect_begin == 1
                        && io::dg::effects[n.effect_begin].kind == io::dg::effect_kind::unread
                        && std::string_view(io::dg::effects[n.effect_begin].waits_on) == n.placeholder;
        if (n.is_quest)
        {
            ++quests_n;
            quests = quests && n.deed_waits_on[0] != '\0'; // no deed reader exists: every deed is declared
            if (n.deed_waits_on[0] == '\0') ++deeds_open;
        }
    }
    check(placeholders, "T1f every placeholder carries one unread effect waiting on its system");
    check(quests && quests_n == 9, "T1g all nine quest deeds are declared unread (no deed reader exists)");
    std::printf("     quests whose deed can complete today: %d of %d\n", deeds_open, quests_n);
}

/// A target that a modifier row's scope matches, for the lever rows.
effect_target target_for(const io::dg::effect& e)
{
    effect_target t;
    const std::string_view s = e.scope;
    auto after = [&](std::string_view p) { return s.substr(p.size()); };
    if (s.rfind("resource:", 0) == 0)
    {
        bool ok = false;
        t.resource = static_cast<int>(resource_names::resource_from_name(std::string(after("resource:")), ok));
    }
    else if (s.rfind("group:", 0) == 0) t.group = after("group:");
    else if (s.rfind("recipe:", 0) == 0) t.recipe = after("recipe:");
    else if (s.rfind("site:", 0) == 0) t.site = after("site:");
    else if (s == "building:university") { t.building = building_type::university; t.scoped_only = true; }
    return t;
}

/// T2 — R2: each read effect moves its lever, for the holding nation's corps
/// and not another nation's.
void rows_t2(const recipe_registry& reg)
{
    std::printf("\nT2 the lever rows (R2): holding the node moves its lever for its nation only\n");

    // (a) Every READ modifier row, through nation_scalar, the hook every read
    // site calls: nation A holds the node, nation B does not.
    {
        int rows = 0, moved = 0, leaked = 0, wrong = 0;
        for (std::size_t k = 0; k < io::dg::effects.size(); ++k)
        {
            const io::dg::effect& e = io::dg::effects[k];
            if (e.kind != io::dg::effect_kind::modifier || classify_effect(e, &reg) != effect_reading::read)
                continue;
            modifier_subject subj = modifier_subject::extraction_rate;
            {
                const std::string_view s = e.subject;
                subj = s == "processing_yield" ? modifier_subject::processing_yield
                     : s == "logistics_cost" ? modifier_subject::logistics_cost
                     : s == "national_research" ? modifier_subject::national_research
                     : modifier_subject::extraction_rate;
            }
            fixture f = make_fixture();
            f.w.nation_tech[f.nation].held.set(static_cast<std::size_t>(e.node));
            const effect_target t = target_for(e);
            const float base = 100.0f;
            // Expected: the node's own rows of this subject that match, in order.
            float expect = base;
            const io::dg::node& n = io::dg::nodes[static_cast<std::size_t>(e.node)];
            for (std::uint16_t j = n.effect_begin; j < n.effect_end; ++j)
            {
                const io::dg::effect& o = io::dg::effects[j];
                if (o.kind != io::dg::effect_kind::modifier || std::string_view(o.subject) != e.subject) continue;
                const bool match = std::string_view(o.scope) == e.scope
                                || (std::string_view(o.scope) == "all" && !t.scoped_only);
                if (!match) continue;
                scalar_modifier m;
                m.op        = o.op == io::dg::effect_op::add ? modifier_op::add : modifier_op::multiply;
                m.magnitude = o.value;
                expect = apply_scalar_modifier(expect, m);
            }
            const float a = nation_scalar(f.w, f.nation, subj, t, base);
            const float b = nation_scalar(f.w, f.other, subj, t, base);
            ++rows;
            if (a != base) ++moved;
            if (b != base) ++leaked;
            if (a != expect)
            {
                ++wrong;
                std::printf("     %s %s %s: %.6f expected %.6f\n", n.id, e.subject, e.scope, a, expect);
            }
        }
        std::printf("     %d read modifier rows: %d moved for the holder, %d leaked to the other nation, %d off\n",
                    rows, moved, leaked, wrong);
        check(rows > 0 && moved == rows && wrong == 0, "T2a every read modifier row moves its scalar by exactly its value for the holder");
        check(leaked == 0, "T2b ...and not for a nation that does not hold it");
    }

    // (b) Extraction through its real read site, extraction_nominal.
    {
        fixture f = make_fixture();
        const entity_id t1 = add_tile(f.w, f.body, f.nation);
        f.w.tiles[t1].resource_deposit[static_cast<std::size_t>(resource_type::petroleum)] = 50.0f;
        building_component site{};
        site.tile               = t1;
        site.type               = building_type::extraction_site;
        site.target_resource    = resource_type::petroleum;
        site.workforce_assigned = 0.5f;
        const float before_a = extraction_nominal(f.w, reg, site, 1.0f, f.corp);
        const float before_b = extraction_nominal(f.w, reg, site, 1.0f, f.corp2);
        f.w.nation_tech[f.nation].held.set(static_cast<std::size_t>(node("DG-PR-EX-A2"))); // Oil Platforms +5%
        const float after_a = extraction_nominal(f.w, reg, site, 1.0f, f.corp);
        const float after_b = extraction_nominal(f.w, reg, site, 1.0f, f.corp2);
        std::printf("     petroleum site: holder %.4f -> %.4f, other nation %.4f -> %.4f\n",
                    before_a, after_a, before_b, after_b);
        check(before_a > 0.0f && after_a == before_a * 1.05f && after_b == before_b,
              "T2c Drill-Bit Metallurgy: an Oil Platform of the holder's corp draws x1.05; the other nation's draws the same");
        site.target_resource = resource_type::coal;
        f.w.tiles[t1].resource_deposit[static_cast<std::size_t>(resource_type::coal)] = 50.0f;
        check(extraction_nominal(f.w, reg, site, 1.0f, f.corp) == extraction_nominal(f.w, reg, site, 1.0f, f.corp2),
              "T2d ...and a coal Mine, outside its scope, is untouched");
    }

    // (c) Research through advance_research: the national share and the University term.
    {
        research_params p{};
        p.base_rp_per_nation          = 100.0f;
        p.rp_per_research_credit      = 0.0f;
        p.rp_per_research_credit_sqrt = 0.0f;
        p.rp_per_university           = 100.0f;
        fixture f = make_fixture(0.0f);
        f.w.nation_tech[f.nation].held.set(static_cast<std::size_t>(node("DG-NE-SO-A")));  // national research +15%
        f.w.nation_tech[f.other].held.set(static_cast<std::size_t>(node("DG-NE-SO-T1"))); // +1%, to show the sum is per nation
        std::vector<research_accrual> rows;
        advance_research(f.w, p, &rows);
        const research_accrual* a = row_for(rows, f.nation);
        const research_accrual* b = row_for(rows, f.other);
        check(a && b && a->multiplier == 1.15f && a->rp_milli == milli(115.0)
                  && b->multiplier == 1.01f && b->rp_milli == milli(101.0),
              "T2e national research: Mainframe Computing +15% scales the holder's RP x1.15; the other nation's own +1% only");

        fixture g = make_fixture(0.0f);
        const entity_id home = add_tile(g.w, g.body, g.nation);
        const entity_id away = add_tile(g.w, g.body, g.other);
        add_building(g.w, g.corp, home, building_type::university, 0, false);
        add_building(g.w, g.corp2, away, building_type::university, 0, false);
        g.w.nation_tech[g.nation].held.set(static_cast<std::size_t>(node("DG-NE-SO-A2"))); // universities +20%
        g.w.nation_tech[g.nation].held.set(static_cast<std::size_t>(node("DG-NE-SO-T1"))); // +1%, must not double into the University term
        rows.clear();
        advance_research(g.w, p, &rows);
        a = row_for(rows, g.nation);
        b = row_for(rows, g.other);
        std::printf("     University term: holder %lld milli, other %lld milli\n",
                    static_cast<long long>(a ? a->rp_milli : -1), static_cast<long long>(b ? b->rp_milli : -1));
        check(a && b && a->rp_milli == milli((100.0 + 120.0) * 1.01) && b->rp_milli == milli(200.0),
              "T2f Universities +20%: the holder's University term x1.2 inside its x1.01 total; the other nation's x1");
    }

    // (d) Farms are the Farm (BL-1274 review #5): Hybrid Seed moves a Farm's
    // draw, not a Fishing Wharf's, though both draw produce.
    {
        fixture f = make_fixture();
        const entity_id t1 = add_tile(f.w, f.body, f.nation);
        f.w.tiles[t1].resource_deposit[static_cast<std::size_t>(resource_type::agricultural_produce)] = 50.0f;
        building_component farm{};
        farm.tile               = t1;
        farm.type               = building_type::extraction_site;
        farm.target_resource    = resource_type::agricultural_produce;
        farm.workforce_assigned = 0.5f;
        const float before = extraction_nominal(f.w, reg, farm, 1.0f, f.corp);
        f.w.nation_tech[f.nation].held.set(static_cast<std::size_t>(node("DG-PR-EX-C1"))); // Farms +5%
        const float after = extraction_nominal(f.w, reg, farm, 1.0f, f.corp);
        check(before > 0.0f && after == before * 1.05f
                  && extraction_nominal(f.w, reg, farm, 1.0f, f.corp2) == before,
              "T2g Hybrid Seed: a Farm (produce from a deposit) of the holder's corp draws x1.05; the other nation's the same");
        effect_target wharf;
        wharf.resource = static_cast<int>(resource_type::agricultural_produce);
        wharf.site     = extraction_site_name(resource_type::agricultural_produce, /*depositless=*/true);
        check(wharf.site == "fishing_wharf"
                  && nation_scalar(f.w, f.nation, modifier_subject::extraction_rate, wharf, 100.0f) == 100.0f,
              "T2h ...and a Fishing Wharf drawing the same good is not a Farm: untouched");
    }

    // (e) Processing through its real read site, credit_processing_outputs: a
    // propellant run of the holder's corp credits x1.2 (Propellant Synthesis),
    // the other nation's corp the base, on both routes.
    {
        fixture f = make_fixture();
        bool routes = true, moved = true, other = true;
        for (const char* name : { "propellant_atmospheric", "propellant_electrolysis" })
        {
            const recipe* rc = reg.get_recipe(reg.recipe_id(name));
            if (rc == nullptr) { routes = false; continue; }
            world w0 = f.w;
            stockpile_component base{}, held{}, oth{};
            const float q0 = credit_processing_outputs(w0, reg, f.corp, *rc, base, 2.0f);
            world w1 = f.w;
            w1.nation_tech[f.nation].held.set(static_cast<std::size_t>(node("DG-PR-EN-C")));
            const float q1 = credit_processing_outputs(w1, reg, f.corp, *rc, held, 2.0f);
            const float q2 = credit_processing_outputs(w1, reg, f.corp2, *rc, oth, 2.0f);
            const std::size_t pr = static_cast<std::size_t>(resource_type::propellant);
            std::printf("     %s: base %.4f  holder %.4f  other nation %.4f (propellant credited)\n",
                        name, base.quantities[pr], held.quantities[pr], oth.quantities[pr]);
            moved = moved && base.quantities[pr] > 0.0f && held.quantities[pr] == base.quantities[pr] * 1.2f && q1 > q0;
            other = other && oth.quantities[pr] == base.quantities[pr] && q2 == q0;
        }
        check(routes, "T2i both propellant routes exist in the registry, and stay open to every corp");
        check(moved, "T2j Propellant Synthesis: a run of the holder's corp credits x1.2 propellant, on both routes");
        check(other, "T2k ...and the other nation's corp is credited the base");
        bool open = true;
        for (const char* name : { "propellant_atmospheric", "propellant_electrolysis" })
            open = open && recipe_unlocked(f.w, reg, f.corp, reg.recipe_id(name))
                        && recipe_unlocked(f.w, reg, f.corp2, reg.recipe_id(name));
        bool all_types = true;
        for (std::uint8_t ty = 0; ty < building_type_count; ++ty)
            all_types = all_types && structure_unlocked(f.w, f.corp2, static_cast<building_type>(ty));
        check(open && all_types, "T2l no recipe or structure unlock is read: propellant and every building type open to every nation");
    }

    // (f) Rocketry gates launch: the space_dispatch mechanic.
    {
        fixture f = make_fixture();
        const bool closed = !nation_mechanic(f.w, f.nation, "space_dispatch")
                         && !nation_mechanic(f.w, f.other, "space_dispatch");
        f.w.nation_tech[f.nation].held.set(static_cast<std::size_t>(node("DG-NE-SO-B")));
        const bool holder = nation_mechanic(f.w, f.nation, "space_dispatch");
        const bool other  = !nation_mechanic(f.w, f.other, "space_dispatch");
        check(closed && holder && other,
              "T2m Rocketry: space_dispatch is closed to every nation at the start, open to its holder only");
        check(nation_mechanic(f.w, f.nation, "no_such_mechanic"), "T2n a mechanic no node names is open");
    }
}

/// T2 (live) — logistics_cost through its real read site, price_convoy_leg, on
/// a generated world (a hand fixture cannot route a leg). The corp's home nation
/// holds Freight Practice I (-1% logistics cost): an overland leg costs x0.99;
/// a ported leg discounts its haul and not its port handling; a node held by
/// another nation moves nothing.
bool rows_t2_live(std::uint32_t seed, bool overland_rows)
{
    std::printf("\nT2 (live, seed %u) logistics_cost through price_convoy_leg\n", seed);
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { std::printf("  FAIL  world build threw: %s\n", e.what()); ++g_failures; return false; }
    world& w = start->w;
    const recipe_registry& reg = start->reg;
    run_app_validation_settle(w, start->reg);
    const logistics_nodes nodes = collect_logistics_nodes(w);
    const std::size_t ri = static_cast<std::size_t>(resource_type::steel);
    const float qty = 10.0f;

    std::vector<entity_id> mids;
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(mids.begin(), mids.end());

    struct found_leg { entity_id corp = null_entity, src = null_entity, dest = null_entity; convoy_leg leg; };
    found_leg overland, ported;
    for (const auto& [key, pool] : w.corp_market_pools)
    {
        if (overland.corp != null_entity && ported.corp != null_entity) break;
        const auto cit = w.corporations.find(key.first);
        if (cit == w.corporations.end() || cit->second.home_nation == null_entity) continue;
        const auto sit = w.markets.find(key.second);
        if (sit == w.markets.end()) continue;
        for (const entity_id d : mids)
        {
            if (d == key.second || w.markets.at(d).body != sit->second.body) continue;
            const convoy_leg l = price_convoy_leg(w, reg, nodes, key.first, key.second, d, ri, qty,
                                                  reg.logistics_cost(convoy_mode::space));
            if (!l.viable || !(l.cost > 0.0f)) continue;
            found_leg& slot = l.port_a != null_entity ? ported : overland;
            if (slot.corp == null_entity) slot = { key.first, key.second, d, l };
        }
    }
    if (overland_rows)
        check(overland.corp != null_entity, "T2o the seed carries a viable overland leg to measure");
    if (overland.corp == null_entity) return false;

    auto reprice = [&](const found_leg& f) {
        return price_convoy_leg(w, reg, nodes, f.corp, f.src, f.dest, ri, qty,
                                reg.logistics_cost(convoy_mode::space));
    };
    const entity_id home = w.corporations.at(overland.corp).home_nation;
    entity_id elsewhere = null_entity;
    for (const auto& kv : w.nations)
        if (kv.first != home && (elsewhere == null_entity || kv.first < elsewhere)) elsewhere = kv.first;
    const int fr = node("DG-NE-FR-T1");

    world saved = w;
    if (overland_rows)
    {
    if (elsewhere != null_entity)
        w.nation_tech[elsewhere].held.set(static_cast<std::size_t>(fr));
    const float other_cost = reprice(overland).cost;
    w = saved;
    w.nation_tech[home].held.set(static_cast<std::size_t>(fr));
    const float held_cost = reprice(overland).cost;
    std::printf("     overland leg: base %.4f  holder %.4f  (node held elsewhere: %.4f)\n",
                overland.leg.cost, held_cost, other_cost);
    check(held_cost == overland.leg.cost * 0.99f, "T2p Freight Practice I: the holder's corp prices an overland leg x0.99");
    check(other_cost == overland.leg.cost, "T2q ...and the node held by another nation moves the leg nothing");
    }

    if (ported.corp == null_entity)
    {
        std::printf("     no ported leg on this seed\n");
        return false;
    }
    w = saved;
    w.nation_tech[w.corporations.at(ported.corp).home_nation].held.set(static_cast<std::size_t>(fr));
    const float p_after = reprice(ported).cost;
    const double handling = static_cast<double>(reg.port_handling()) * 2.0 * qty; // two ports
    const double expect   = (static_cast<double>(ported.leg.cost) - handling) * 0.99 + handling;
    std::printf("     ported leg: base %.4f  holder %.4f  expected haul-only %.4f (handling %.4f)\n",
                ported.leg.cost, p_after, expect, handling);
    check(std::fabs(static_cast<double>(p_after) - expect) <= 1e-4 * std::max(1.0, expect),
          "T2r ...and a ported leg discounts its haul, never its port handling");
    return true;
}

// ---------------------------------------------------------------------------
// R0 — the campaign run
// ---------------------------------------------------------------------------

std::vector<std::uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<std::uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t q = pos;
        while (q < text.size() && (text[q] == ' ' || text[q] == ':' || text[q] == '\t')) ++q;
        if (q < text.size() && text[q] >= '0' && text[q] <= '9')
            out.push_back(static_cast<std::uint32_t>(std::strtoul(text.c_str() + q, nullptr, 10)));
    }
    return out;
}

std::vector<std::uint32_t> parse_list(const std::string& s)
{
    std::vector<std::uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) out.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

struct dial_override
{
    bool  set_base = false, set_lin = false, set_sqrt = false, set_unit = false;
    float base = 0.0f, lin = 0.0f, sqrt_k = 0.0f, unit = 0.0f;
};

struct nation_row
{
    entity_id nation          = null_entity;
    int       quarters_to_maj = -1;   ///< Live quarters until DG-PR-EX-A is held; -1 never.
    bool      source          = false;///< Some RP source at the seat (line share, building, base).
    double    rp_live         = 0.0;  ///< RP accrued over the live window.
    double    spend_live      = 0.0;  ///< Credits the research line spent over the window.
    double    full_rp         = 0.0;  ///< RP it WOULD convert over the window spending its whole share every quarter.
    int       held_end        = 0;
};

struct seed_run
{
    std::string             fail;
    bool                    band_industrial = false;
    bool                    none_held_open  = false;
    bool                    empty_at_seat   = false;
    int                     nations         = 0;
    int                     launchpads      = 0; ///< Most launchpads standing, seat through live tick 20.
    int                     space_convoys   = 0; ///< Most inter-body convoys in flight, seat through live tick 20.
    std::vector<nation_row> rows;
    std::uint64_t           digest          = 0;
    double                  secs            = 0.0;
    research_params         params{};
};

seed_run run_seed(std::uint32_t seed, int quarters, const dial_override& ov)
{
    seed_run out;
    const auto t0 = std::chrono::steady_clock::now();
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, wp, *start); }
    catch (const std::exception& e) { out.fail = std::string("world build threw: ") + e.what(); return out; }
    world& w = start->w;
    recipe_registry& reg = start->reg;
    {
        research_params rp = reg.research();
        if (ov.set_base) rp.base_rp_per_nation = ov.base;
        if (ov.set_lin)  rp.rp_per_research_credit = ov.lin;
        if (ov.set_sqrt) rp.rp_per_research_credit_sqrt = ov.sqrt_k;
        if (ov.set_unit) rp.rp_per_cost_unit = ov.unit;
        reg.set_research(rp);
        out.params = rp;
    }
    const research_params& p = reg.research();

    out.band_industrial = campaign_band_from_world(w) == era_band::industrial
                       && reg.era() == era_band::industrial;
    out.nations = static_cast<int>(w.nations.size());
    out.none_held_open = true;
    for (const auto& [nid, st] : w.nation_tech)
        if (st.held.any() || !st.queue.empty()) out.none_held_open = false;

    run_app_validation_settle(w, reg); // ends with the handover (clear_research_at_handover)
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity) { out.fail = "no corporation seated"; return out; }

    // Every nation's pool and queue empty at the seat (RESEARCH.md: no nation
    // opens ahead in technology).
    out.empty_at_seat = true;
    for (const auto& [nid, st] : w.nation_tech)
        if (!st.empty()) out.empty_at_seat = false;

    // BL-1274: what Rocketry's launch gate (space_dispatch) touches — launchpads
    // standing and inter-body convoys in flight, at the seat and over the first
    // 20 live ticks (counted in the loop below).
    const auto count_space = [&](int& pads, int& convoys) {
        int p_now = 0, c_now = 0;
        for (const auto& [bid, b] : w.buildings)
            if (b.type == building_type::launchpad && !b.decommissioned) ++p_now;
        for (const convoy_component& c : w.convoys)
            if (c.mode == convoy_mode::space) ++c_now;
        pads    = std::max(pads, p_now);
        convoys = std::max(convoys, c_now);
    };
    count_space(out.launchpads, out.space_convoys);

    std::vector<entity_id> nids;
    for (const auto& kv : w.nations) nids.push_back(kv.first);
    std::sort(nids.begin(), nids.end());
    std::map<entity_id, int> bldgs; // advance_research's own counting rule
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end() || bit->second.ticks_remaining > 0 || bit->second.decommissioned
                || !(bit->second.workforce_assigned > 0.0f))
                continue;
            if (bit->second.type == building_type::university)
            {
                if (const auto tn = w.tile_to_nation.find(bit->second.tile); tn != w.tile_to_nation.end())
                    ++bldgs[tn->second];
            }
            else if (bit->second.type == building_type::research_institute && cc.home_nation != null_entity)
                ++bldgs[cc.home_nation];
        }

    // The scripted queue (BL-1275 will be the real caller): every nation,
    // ascending, queues the DG-PR-EX-A path.
    const int major = io::dg::find_node("DG-PR-EX-A");
    std::map<entity_id, nation_row> rows;
    for (const entity_id nid : nids)
    {
        nation_row r;
        r.nation = nid;
        bool share = false;
        if (const auto bit = w.nation_budgets.find(nid); bit != w.nation_budgets.end())
            share = bit->second.weights[static_cast<std::size_t>(budget_priority::academic_research)] > 0.0f
                 && bit->second.reserve_fraction < 1.0f;
        r.source = (share && w.nations.at(nid).treasury > 0.0f) || bldgs.count(nid) > 0
                || p.base_rp_per_nation > 0.0f;
        rows[nid] = r;
        enqueue_node(w, nid, major);
    }

    const int first = k_app_validation_ticks;
    constexpr int k_econ_tick_days = 90; // sim_loop::econ_tick_days
    for (int qtr = 1; qtr <= quarters; ++qtr)
    {
        // run_app_live_window's body, one quarter at a time so the tech state
        // can be read between quarters (its own loop numbers days from 1).
        const int day = qtr * k_econ_tick_days;
        advance_orbits(w, static_cast<double>(k_econ_tick_days));
        advance_surveys(w, k_econ_tick_days);
        w.current_day_tick = day;
        const settle_tick_result res = run_settle_tick(w, reg, first + (qtr - 1), day, /*spectating=*/false);
        if (qtr <= 20)
            count_space(out.launchpads, out.space_convoys);
        for (const research_accrual& a : res.report.research)
        {
            nation_row& r = rows[a.nation];
            r.rp_live    += static_cast<double>(a.rp_milli) / 1000.0;
            r.spend_live += a.line_spend;
            {   // the whole-tree rate: the base plus the line spending its WHOLE share
                const double sh = std::max(0.0, static_cast<double>(a.line_share));
                r.full_rp += p.base_rp_per_nation + p.rp_per_research_credit * sh
                           + p.rp_per_research_credit_sqrt * std::sqrt(sh);
            }
        }
        for (const entity_id nid : nids)
        {
            const auto it = w.nation_tech.find(nid);
            if (it == w.nation_tech.end()) continue;
            nation_row& r = rows[nid];
            if (r.quarters_to_maj < 0 && it->second.held.test(static_cast<std::size_t>(major)))
                r.quarters_to_maj = qtr;
            r.held_end = static_cast<int>(it->second.held.count());
        }
    }

    for (const auto& kv : rows) out.rows.push_back(kv.second);
    out.digest = w.state_hash(w.current_day_tick);
    for (const auto& [nid, st] : w.nation_tech)
    {
        out.digest = out.digest * 1099511628211ull ^ (static_cast<std::uint64_t>(nid) << 32)
                   ^ static_cast<std::uint64_t>(st.pool_milli);
        out.digest = out.digest * 1099511628211ull ^ static_cast<std::uint64_t>(st.held.count());
        for (const std::uint16_t n : st.queue)
            out.digest = out.digest * 1099511628211ull ^ n;
    }
    out.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return out;
}

double pct(std::vector<double> v, double q)
{
    if (v.empty()) return -1.0;
    std::sort(v.begin(), v.end());
    const std::size_t i = std::min(v.size() - 1, static_cast<std::size_t>(q * static_cast<double>(v.size())));
    return v[i];
}

double median_of(std::vector<double> v)
{
    if (v.empty()) return -1.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

void rows_r0(const std::vector<std::uint32_t>& seeds, int quarters, bool twice, const dial_override& ov)
{
    std::printf("\nR0 the campaign run: %zu seeds, %d live quarters, %s\n", seeds.size(), quarters,
                twice ? "each built twice" : "once");
    bool all_band = true, all_open = true, all_seat = true, all_same = true, any_complete = false;
    int  fails = 0;
    std::vector<double> q_src;  // quarters-to-major, nations with a source (never = quarters+1)
    std::vector<double> rp_src, spend_src;
    double best_full = 0.0; // the richest nation's whole-share RP per quarter
    int src_n = 0, src_done = 0, total = 0;
    research_params used{};
    for (const std::uint32_t s : seeds)
    {
        const seed_run a = run_seed(s, quarters, ov);
        if (!a.fail.empty()) { std::printf("  seed %u FAIL %s\n", s, a.fail.c_str()); ++fails; continue; }
        used = a.params;
        bool same = true;
        if (twice)
        {
            const seed_run b = run_seed(s, quarters, ov);
            same = b.fail.empty() && b.digest == a.digest;
            all_same = all_same && same;
        }
        all_band = all_band && a.band_industrial;
        all_open = all_open && a.none_held_open;
        all_seat = all_seat && a.empty_at_seat;
        int s_src = 0, s_done = 0;
        std::vector<double> qs;
        for (const nation_row& r : a.rows)
        {
            ++total;
            if (r.held_end > 0) any_complete = true;
            if (!r.source) continue;
            ++s_src;
            const bool done = r.quarters_to_maj > 0;
            if (done) ++s_done;
            const double q = done ? r.quarters_to_maj : quarters + 1;
            qs.push_back(q);
            q_src.push_back(q);
            rp_src.push_back(r.rp_live / quarters);
            spend_src.push_back(r.spend_live / quarters);
            best_full = std::max(best_full, r.full_rp / quarters);
        }
        src_n += s_src;
        src_done += s_done;
        std::printf("  seed %-10u nations %2d  with a source %2d  reached DG-PR-EX-A %2d  median q %5.1f"
                    "  seat..t20: %d launchpads, %d space convoys"
                    "  digest %016llx %s (%.0f s)\n",
                    s, a.nations, s_src, s_done, median_of(qs), a.launchpads, a.space_convoys,
                    static_cast<unsigned long long>(a.digest), twice ? (same ? "twice-equal" : "DIFFERS") : "",
                    a.secs);
    }
    std::printf("  dials: base %.3g  linear %.4g RP/credit  sqrt %.4g RP/sqrt(credit)  unit %.4g RP\n",
                used.base_rp_per_nation, used.rp_per_research_credit, used.rp_per_research_credit_sqrt,
                used.rp_per_cost_unit);

    check(fails == 0, "R0a every seed builds, settles and seats");
    check(all_band, "R0b STRUCTURAL: every seed opens on the industrial band (campaign_band_from_world is a constant)");
    check(all_open, "R0c STRUCTURAL: no nation holds a node at generation (nothing writes nation_tech before play)");
    check(all_seat, "R0d every nation's pool and queue are empty at the seat (the settle's handover)");
    check(any_complete, "R0e the scripted queue completes nodes");
    if (twice) check(all_same, "R0f the whole run is byte-identical twice, per seed");

    // PACING (RESEARCH.md § Cost; the targets the conversion is tuned to). May
    // FAIL until the conversion is set; reported, never weakened.
    const double frac   = src_n > 0 ? static_cast<double>(src_done) / src_n : 0.0;
    const double med_q  = median_of(q_src);
    const double fast_q = pct(q_src, 0.10); // the p90-fastest nation
    std::printf("\n  PACING over %d nations with a source (%d without): %d of %d (%.1f%%) reach DG-PR-EX-A\n"
                "  within %d quarters; median quarters %.1f; the fastest decile (p10) %.0f.\n",
                src_n, total - src_n, src_done, src_n, 100.0 * frac, quarters, med_q, fast_q);
    std::printf("  quarters-to-major (never = %d): p10 %.0f  p25 %.0f  p50 %.0f  p75 %.0f  p90 %.0f\n",
                quarters + 1, pct(q_src, 0.1), pct(q_src, 0.25), pct(q_src, 0.5), pct(q_src, 0.75),
                pct(q_src, 0.9));
    std::printf("  live RP/quarter: p10 %.1f  p50 %.1f  p90 %.1f  max %.1f | line spend/quarter: p50 %.2f  p90 %.2f\n",
                pct(rp_src, 0.1), pct(rp_src, 0.5), pct(rp_src, 0.9), pct(rp_src, 1.0),
                pct(spend_src, 0.5), pct(spend_src, 0.9));
    constexpr double k_min_reach = 0.90;
    // The targets scale with the RP unit: set at 100 RP a unit (16 quarters, median 8-12, p10 >= 4),
    // and Ben's 200 (an age is ~80 years) doubles them. --quarters should be 16 x the scale.
    const double sc = used.rp_per_cost_unit / 100.0;
    double tree_units = 0.0;
    for (int n = 0; n < io::dg::node_count; ++n) tree_units += io::dg::nodes[static_cast<std::size_t>(n)].cost_units;
    const double tree_q = best_full > 0.0 ? tree_units * used.rp_per_cost_unit / best_full : -1.0;
    std::printf("  the richest nation, spending its whole research share every quarter, converts %.1f RP a quarter:\n"
                "  the whole tree (%.0f units, %.0f RP) in %.0f quarters (%.1f years).\n",
                best_full, tree_units, tree_units * used.rp_per_cost_unit, tree_q, tree_q / 4.0);
    std::printf("  thresholds: >= %.0f%% reach it in %d; median in [%.0f, %.0f]; p10 >= %.0f\n",
                100.0 * k_min_reach, static_cast<int>(16 * sc), 8 * sc, 12 * sc, 4 * sc);
    check(quarters >= static_cast<int>(16 * sc) && frac >= k_min_reach,
          "R0g PACING: >= 90% of nations with a source reach their first ring-2 major in 16 x scale quarters");
    check(med_q >= 8.0 * sc && med_q <= 12.0 * sc, "R0h PACING: the median nation reaches it in 8-12 x scale quarters");
    check(fast_q >= 4.0 * sc, "R0i PACING: the fastest decile takes no fewer than ~4 x scale quarters");
}

} // namespace

int main(int argc, char** argv)
{
    int  seed_count = 16;
    int  quarters   = 32; // 16 x the RP-unit scale (200 / 100)
    bool twice      = true;
    dial_override ov;
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        const bool more = i + 1 < argc;
        if (!std::strcmp(argv[i], "--seeds") && more) seed_count = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seed-list") && more) seeds = parse_list(argv[++i]);
        else if (!std::strcmp(argv[i], "--quarters") && more) quarters = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--once")) twice = false;
        else if (!std::strcmp(argv[i], "--base") && more) { ov.set_base = true; ov.base = static_cast<float>(std::atof(argv[++i])); }
        else if (!std::strcmp(argv[i], "--lin") && more)  { ov.set_lin = true; ov.lin = static_cast<float>(std::atof(argv[++i])); }
        else if (!std::strcmp(argv[i], "--sqrt") && more) { ov.set_sqrt = true; ov.sqrt_k = static_cast<float>(std::atof(argv[++i])); }
        else if (!std::strcmp(argv[i], "--unit") && more) { ov.set_unit = true; ov.unit = static_cast<float>(std::atof(argv[++i])); }
        else
        {
            std::fprintf(stderr, "usage: nation_tech_harness [--seeds N | --seed-list a,b] [--quarters Q] [--once]"
                                 " [--base X] [--lin Y] [--sqrt Z] [--unit U]\n");
            return 2;
        }
    }

    std::printf("nation_tech_harness (BL-1273): %d nodes in the Digitisation tree\n", io::dg::node_count);
    rows_r1();
    rows_r2();
    rows_r3();
    {
        // The real data layer, as app::load_economy loads it: the audit checks
        // unlock and scope names against the registry the game runs on.
        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        lua.load("scripts/world_gen.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);
        rows_t1(reg);
        rows_t2(reg);
    }
    {
        // Curated seeds for the rows a hand fixture cannot route: the overland
        // rows on the first (28, the fastest build), then seeds in turn until
        // one carries a ported leg for the handling split.
        bool ported = false;
        bool first_live = true;
        for (const std::uint32_t s : { 28u, 40u, 12u, 9u, 43u })
        {
            ported = rows_t2_live(s, first_live);
            first_live = false;
            if (ported) break;
        }
        check(ported, "T2s a curated seed carries a ported leg (the handling split was measured)");
    }

    if (seeds.empty() && seed_count > 0)
    {
        seeds = library_seeds("docs/generation/seed_library.json");
        if (static_cast<int>(seeds.size()) > seed_count) seeds.resize(static_cast<std::size_t>(seed_count));
        if (seeds.empty()) { std::printf("  FAIL  no library seeds found\n"); ++g_failures; }
    }
    if (!seeds.empty())
        rows_r0(seeds, quarters, twice, ov);

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
