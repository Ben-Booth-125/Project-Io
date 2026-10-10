// nation_tech_harness — BL-1273 (every nation holds a tree state, a research
// pool and a queue). docs/economy/RESEARCH.md is the authority; requirement
// group "nation-tech-state" rows R0-R3 (R3's save half is save_roundtrip P5;
// a round-trip row is repeated here on a fixture).
//
//   R1  accrual: the academic-research line's share (debited from the
//       treasury), completed Universities in the nation's territory and
//       completed research institutes of corporations homed in the nation,
//       into the pool, once per call; under construction or decommissioned
//       counts nothing; the same inputs give the same bits twice.
//   R2  the queue: enqueue queues unheld requirements ahead (AND over groups,
//       the cheapest member of a choice); a pick-one pair is refused and the
//       refusal mutates nothing; RP flow to the head, the remainder flows on;
//       a quest with an unmet deed is passed over and stays queued; an empty
//       queue lets the pool wait.
//   R3  a fixture's nation tech state survives write -> read exactly.
//   R0  a campaign run over the curated seeds (docs/generation/seed_library.json):
//       every nation opens holding nothing on the industrial band; through the
//       settle and a live window every nation accrues; with a scripted queue
//       (every nation queues its first ring-2 major, DG-PR-EX-A, after the
//       seat) nodes complete; the whole run is byte-identical twice. Prints the
//       PACING measurement the RP defaults were set against: quarters from the
//       seat until DG-PR-EX-A is held (18 cost units with its path).
//
// Usage: nation_tech_harness [--seeds N | --seed-list a,b,...] [--quarters Q] [--once]
//   --seeds 0   fixture rows only (R1-R3) — the ctest invocation.
//   default     all 16 library seeds, 16 live quarters, each seed built twice.
//
// Build: bash tools/verify/build_lua_harness.sh nation_tech_harness  (it builds
// the shipped world through harness_params.hpp, which loads Lua).

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/nation_budget.hpp"
#include "world/nation_tech.hpp"
#include "world/spawn_seat.hpp"
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

void add_building(world& w, entity_id corp, entity_id tile, building_type type,
                  int ticks_remaining, bool decommissioned)
{
    const entity_id b = w.create_entity();
    building_component bc{};
    bc.tile            = tile;
    bc.type            = type;
    bc.ticks_remaining = ticks_remaining;
    bc.decommissioned  = decommissioned;
    w.buildings[b] = bc;
    w.corporations.at(corp).assets.push_back(b);
}

fixture make_fixture()
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};
    f.nation = f.w.create_entity();
    f.other  = f.w.create_entity();
    nation_component n{};
    n.treasury = 10000.0f;
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

// ---------------------------------------------------------------------------
// R1 — accrual
// ---------------------------------------------------------------------------

void rows_r1()
{
    std::printf("\nR1 accrual: the research line, Universities, research institutes\n");
    const research_params p{};

    fixture f = make_fixture();
    const entity_id home_tile  = add_tile(f.w, f.body, f.nation);
    const entity_id other_tile = add_tile(f.w, f.body, f.other);
    // A completed University on the nation's ground, owned by the OTHER
    // nation's corp: a University counts where it STANDS.
    add_building(f.w, f.corp2, home_tile, building_type::university, 0, false);
    // One under construction and one decommissioned: neither counts.
    add_building(f.w, f.corp, home_tile, building_type::university, 3, false);
    add_building(f.w, f.corp, home_tile, building_type::university, 0, true);
    // A completed institute of the nation's corp, standing on the OTHER
    // nation's ground: an institute counts for its OWNER's home nation.
    add_building(f.w, f.corp, other_tile, building_type::research_institute, 0, false);
    // The other nation's corp's institute counts for the other nation.
    add_building(f.w, f.corp2, other_tile, building_type::research_institute, 0, false);
    add_building(f.w, f.corp2, other_tile, building_type::research_institute, 5, false);

    world twin = f.w;
    std::vector<research_accrual> rows;
    advance_research(f.w, p, &rows);

    const research_accrual* a = nullptr;
    const research_accrual* b = nullptr;
    for (const research_accrual& r : rows)
    {
        if (r.nation == f.nation) a = &r;
        if (r.nation == f.other)  b = &r;
    }
    check(a != nullptr && b != nullptr && rows.size() == 2, "R1a one row per nation");
    if (a == nullptr || b == nullptr) return;

    // share = treasury x (1 - reserve) x w_academic / sum(w) = 10000 x 0.1.
    check(std::fabs(a->line_spend - 1000.0f) < 1e-3f, "R1b the line spends its share of the treasury");
    check(std::fabs(f.w.nations.at(f.nation).treasury - 9000.0f) < 1e-3f,
          "R1c ...and the share leaves the treasury (a direct expenditure)");
    check(a->universities == 1 && a->institutes == 1,
          "R1d one completed University where it stands, one institute by owner's nation");
    check(b->universities == 0 && b->institutes == 1 && b->line_spend == 0.0f,
          "R1e the other nation: its corp's completed institute only, no budget, no spend");
    const float want_a = 1000.0f * p.rp_per_research_credit + p.rp_per_university + p.rp_per_institute;
    check(std::fabs(a->rp - want_a) < 1e-3f && std::fabs(f.w.nation_tech.at(f.nation).pool - want_a) < 1e-3f,
          "R1f RP = spend x rate + Universities + institutes, into the pool");
    check(std::fabs(f.w.nation_tech.at(f.other).pool - p.rp_per_institute) < 1e-4f,
          "R1g the other nation's pool holds its institute's RP");
    check(a->multiplier == 1.0f, "R1h the research multiplier is identity until node effects land");
    std::printf("     nation A: spend %.1f  univ %d  inst %d  RP %.2f | nation B: RP %.2f\n",
                a->line_spend, a->universities, a->institutes, a->rp, b->rp);

    // A nation's pool with nothing queued waits: a second quarter adds again.
    const float after_one = f.w.nation_tech.at(f.nation).pool;
    advance_research(f.w, p);
    check(f.w.nation_tech.at(f.nation).pool > after_one && f.w.nation_tech.at(f.nation).queue.empty()
              && f.w.nation_tech.at(f.nation).held.none(),
          "R1i with an empty queue the pool accrues and waits; nothing is held");

    // Deterministic: the twin, stepped identically, carries identical bits.
    advance_research(twin, p);
    advance_research(twin, p);
    check(twin.state_hash(0) == f.w.state_hash(0)
              && twin.nation_tech.at(f.nation).pool == f.w.nation_tech.at(f.nation).pool,
          "R1j the same inputs accrue the same bits");
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
    const research_params p{};
    const float unit = p.rp_per_cost_unit;

    // Prerequisite expansion, AND groups: the major needs both minors.
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
        // A node whose only requirement is already queued queues alone.
        check(enqueue_node(f.w, f.nation, node("DG-PR-EX-T3")) == enqueue_result::queued
                  && q.size() == 6 && io::dg::nodes[q.back()].id == std::string("DG-PR-EX-T3"),
              "R2c a requirement already queued is not queued twice");
        check(enqueue_node(f.w, null_entity, node("DG-PR-EX-A")) == enqueue_result::invalid
                  && enqueue_node(f.w, f.nation, io::dg::node_count) == enqueue_result::invalid,
              "R2d an unknown nation or node is refused");
    }

    // A choice group: the cheapest member; the pick-one pair guard.
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
        // Held, not queued, closes the pair too.
        fixture g = make_fixture();
        g.w.nation_tech[g.nation].held.set(static_cast<std::size_t>(bb));
        check(enqueue_node(g.w, g.nation, ba) == enqueue_result::refused_pair
                  && g.w.nation_tech[g.nation].queue.empty(),
              "R2i a HELD pair side refuses the other, queue untouched");
        // ...while the choice group routes around it.
        check(enqueue_node(g.w, g.nation, m) == enqueue_result::queued,
              "R2j a choice group satisfied by the held side queues without it");
    }

    // RP flow: the head, completion, overflow, waiting.
    {
        fixture f = make_fixture();
        enqueue_node(f.w, f.nation, node("DG-PR-EX-A1")); // T1 T2 A1
        nation_tech_state& st = f.w.nation_tech[f.nation];
        st.pool = 1.0f * unit + 1.0f * unit + 50.0f; // T1 and T2, and 50 over
        const int done = flow_research_queue(f.w, f.nation, p);
        check(done == 2 && st.held.test(static_cast<std::size_t>(node("DG-PR-EX-T1")))
                  && st.held.test(static_cast<std::size_t>(node("DG-PR-EX-T2"))),
              "R2k RP flow to the head; each node completes when its cost is met");
        check(std::fabs(st.pool - 50.0f) < 1e-3f && st.queue.size() == 1,
              "R2l the remainder stays with the next entry (A1 at 200 waits with 50)");
        st.pool += 150.0f;
        check(flow_research_queue(f.w, f.nation, p) == 1 && st.pool == 0.0f && st.queue.empty(),
              "R2m ...and completes it once the remainder plus new RP cover its cost");
    }

    // A quest with an unmet deed is passed over; it stays queued.
    {
        fixture f = make_fixture();
        const int q = node("DG-PR-EX-Q"); // requires A
        enqueue_node(f.w, f.nation, q);        // T1 T2 A1 A2 A Q
        enqueue_node(f.w, f.nation, node("DG-PR-EX-T3")); // ... T3 (requires A)
        nation_tech_state& st = f.w.nation_tech[f.nation];
        float path = 0.0f;
        for (const char* id : {"DG-PR-EX-T1", "DG-PR-EX-T2", "DG-PR-EX-A1", "DG-PR-EX-A2",
                               "DG-PR-EX-A", "DG-PR-EX-T3"})
            path += node_cost_rp(node(id), p);
        st.pool = path + node_cost_rp(q, p);
        check(!deed_met(f.w, f.nation, q), "R2n every deed reads unmet until the effect pass wires them");
        const int done = flow_research_queue(f.w, f.nation, p);
        check(done == 6 && st.held.test(static_cast<std::size_t>(node("DG-PR-EX-T3")))
                  && !st.held.test(static_cast<std::size_t>(q)),
              "R2o the deed-unmet quest is passed over: RP flow past it to T3");
        check(st.queue.size() == 1 && st.queue[0] == q
                  && std::fabs(st.pool - node_cost_rp(q, p)) < 1e-3f,
              "R2p ...and it stays queued, its cost waiting in the pool");
    }
}

// ---------------------------------------------------------------------------
// R3 — the fixture round trip (save_roundtrip P5 is the canonical-world row)
// ---------------------------------------------------------------------------

void rows_r3()
{
    std::printf("\nR3 the save seam (fixture; save_roundtrip P5 runs it on a generated world)\n");
    fixture f = make_fixture();
    enqueue_node(f.w, f.nation, node("DG-NE-SO-A"));
    f.w.nation_tech[f.nation].held.set(static_cast<std::size_t>(node("DG-ST-FO-T1")));
    f.w.nation_tech[f.nation].pool = 12.375f;
    f.w.nation_tech[f.other].pool  = 0.1f;

    std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
    write_world_snapshot(f.w, ss);
    world back;
    const bool ok = read_world_snapshot(back, ss);
    check(ok, "R3a a world carrying nation tech state reads back");
    bool same = ok && back.nation_tech.size() == f.w.nation_tech.size();
    if (same)
        for (const auto& [nid, st] : f.w.nation_tech)
        {
            const auto it = back.nation_tech.find(nid);
            same = same && it != back.nation_tech.end() && it->second.held == st.held
                && it->second.pool == st.pool && it->second.queue == st.queue;
        }
    check(same, "R3b held set, pool (the in-progress remainder) and queue, exactly");
    check(world_save_version == 47u, "R3c at world_save_version 47");
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

struct nation_row
{
    entity_id nation          = null_entity;
    int       quarters_to_maj = -1;   ///< Quarters from the seat until DG-PR-EX-A is held; -1 never.
    float     rp_settle       = 0.0f; ///< Pool at the seat (accrued through the settle).
    float     rp_live         = 0.0f; ///< RP accrued over the live window.
    int       held_end        = 0;
    float     treasury_seat   = 0.0f; ///< Treasury at the seat (diagnostic).
    float     academic_share  = 0.0f; ///< The line's fraction of the treasury per quarter at the seat.
    int       research_bldgs  = 0;    ///< Completed Universities + institutes counting for it at the seat.
    std::vector<float> cum_rp;        ///< RP accrued since generation, after each live quarter.
};

struct seed_run
{
    std::string             fail;
    bool                    band_industrial = false;
    bool                    none_held_open  = false;
    bool                    none_held_seat  = false;
    int                     nations         = 0;
    std::vector<nation_row> rows;
    std::uint64_t           digest          = 0;
    double                  secs            = 0.0;
};

seed_run run_seed(std::uint32_t seed, int quarters)
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
    const recipe_registry& reg = start->reg;

    out.band_industrial = campaign_band_from_world(w) == era_band::industrial
                       && reg.era() == era_band::industrial;
    out.nations = static_cast<int>(w.nations.size());
    out.none_held_open = true;
    for (const auto& [nid, st] : w.nation_tech)
        if (st.held.any() || !st.queue.empty()) out.none_held_open = false;

    run_app_validation_settle(w, reg);
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    if (seat.seated == null_entity) { out.fail = "no corporation seated"; return out; }

    out.none_held_seat = true;
    for (const auto& [nid, st] : w.nation_tech)
        if (st.held.any()) out.none_held_seat = false;

    // The scripted queue: every nation, ascending, queues its first ring-2 major.
    std::vector<entity_id> nids;
    for (const auto& kv : w.nations) nids.push_back(kv.first);
    std::sort(nids.begin(), nids.end());
    const int major = io::dg::find_node("DG-PR-EX-A");
    std::map<entity_id, int> bldgs; // advance_research's own counting rule
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end() || bit->second.ticks_remaining > 0 || bit->second.decommissioned)
                continue;
            if (bit->second.type == building_type::university)
            {
                if (const auto tn = w.tile_to_nation.find(bit->second.tile); tn != w.tile_to_nation.end())
                    ++bldgs[tn->second];
            }
            else if (bit->second.type == building_type::research_institute && cc.home_nation != null_entity)
                ++bldgs[cc.home_nation];
        }
    std::map<entity_id, nation_row> rows;
    for (const entity_id nid : nids)
    {
        nation_row r;
        r.nation    = nid;
        r.treasury_seat = w.nations.at(nid).treasury;
        r.research_bldgs = bldgs.count(nid) ? bldgs.at(nid) : 0;
        if (const auto bit = w.nation_budgets.find(nid); bit != w.nation_budgets.end())
        {
            float tot = 0.0f;
            for (const float x : bit->second.weights) if (x > 0.0f) tot += x;
            const float wa = bit->second.weights[static_cast<std::size_t>(budget_priority::academic_research)];
            r.academic_share = tot > 0.0f ? wa / tot * (1.0f - std::clamp(bit->second.reserve_fraction, 0.0f, 1.0f)) : 0.0f;
        }
        r.rp_settle = w.nation_tech.count(nid) ? w.nation_tech.at(nid).pool : 0.0f;
        rows[nid]   = r;
        enqueue_node(w, nid, major);
    }

    const research_params p{};
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
        (void)run_settle_tick(w, reg, first + (qtr - 1), day, /*spectating=*/false);

        for (const entity_id nid : nids)
        {
            const auto it = w.nation_tech.find(nid);
            if (it == w.nation_tech.end()) continue;
            // Everything ever spent is the held set's cost (nothing is held
            // before the seat), so pool + spent - pool at the seat = live RP.
            float spent = 0.0f;
            for (int n = 0; n < io::dg::node_count; ++n)
                if (it->second.held.test(static_cast<std::size_t>(n)))
                    spent += node_cost_rp(n, p);
            nation_row& r = rows[nid];
            if (r.quarters_to_maj < 0 && it->second.held.test(static_cast<std::size_t>(major)))
                r.quarters_to_maj = qtr;
            r.held_end = static_cast<int>(it->second.held.count());
            // pool + everything ever spent - pool at the seat = RP accrued live
            r.rp_live = it->second.pool + spent - r.rp_settle;
            r.cum_rp.push_back(it->second.pool + spent); // settle pool included
        }
    }

    for (const auto& kv : rows) out.rows.push_back(kv.second);
    out.digest = w.state_hash(w.current_day_tick);
    // Fold the tech state explicitly too, so the digest cannot pass on a hash
    // that skipped it.
    for (const auto& [nid, st] : w.nation_tech)
    {
        std::uint32_t pool_bits = 0;
        std::memcpy(&pool_bits, &st.pool, sizeof pool_bits);
        out.digest = out.digest * 1099511628211ull ^ (static_cast<std::uint64_t>(nid) << 32 | pool_bits);
        out.digest = out.digest * 1099511628211ull ^ static_cast<std::uint64_t>(st.held.count());
        for (const std::uint16_t n : st.queue)
            out.digest = out.digest * 1099511628211ull ^ n;
    }
    out.secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return out;
}

double median_of(std::vector<double> v)
{
    if (v.empty()) return -1.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return n % 2 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

void rows_r0(const std::vector<std::uint32_t>& seeds, int quarters, bool twice)
{
    std::printf("\nR0 the campaign run: %zu seeds, %d live quarters, %s\n", seeds.size(), quarters,
                twice ? "each built twice" : "once");
    bool all_band = true, all_open = true, all_seat = true, all_accrue = true, any_complete = false;
    bool all_same = true;
    int  fails = 0;
    std::vector<double> q_all;          // quarters-to-major, every nation (never = quarters+1)
    std::vector<double> rp_per_q_all;   // live RP per quarter, every nation
    std::vector<double> treasury_all, share_all;
    std::vector<nation_row> all_rows;
    int never = 0, total_nations = 0, sourceless = 0, sourceless_accrued = 0, bldg_nations = 0;
    for (const std::uint32_t s : seeds)
    {
        const seed_run a = run_seed(s, quarters);
        if (!a.fail.empty()) { std::printf("  seed %u FAIL %s\n", s, a.fail.c_str()); ++fails; continue; }
        bool same = true;
        if (twice)
        {
            const seed_run b = run_seed(s, quarters);
            same = b.fail.empty() && b.digest == a.digest;
            all_same = all_same && same;
        }
        all_band = all_band && a.band_industrial;
        all_open = all_open && a.none_held_open;
        all_seat = all_seat && a.none_held_seat;
        std::vector<double> qs;
        int accrued = 0, completed = 0;
        for (const nation_row& r : a.rows)
        {
            ++total_nations;
            const bool source = (r.treasury_seat > 0.0f && r.academic_share > 0.0f) || r.research_bldgs > 0;
            if (r.rp_live > 0.0f) ++accrued;
            if (!source) { ++sourceless; if (r.rp_live > 0.0f) ++sourceless_accrued; }
            else if (!(r.rp_live > 0.0f)) all_accrue = false;
            if (r.research_bldgs > 0) ++bldg_nations;
            if (r.held_end > 0) { ++completed; any_complete = true; }
            const double q = r.quarters_to_maj < 0 ? quarters + 1 : r.quarters_to_maj;
            if (r.quarters_to_maj < 0) ++never;
            qs.push_back(q);
            q_all.push_back(q);
            rp_per_q_all.push_back(r.rp_live / quarters);
            treasury_all.push_back(r.treasury_seat);
            share_all.push_back(r.academic_share);
            all_rows.push_back(r);
        }
        std::printf("  seed %-10u nations %2d  accrued %2d  completed>=1 %2d  median q-to-major %5.1f"
                    "  digest %016llx %s (%.0f s)\n",
                    s, a.nations, accrued, completed, median_of(qs),
                    static_cast<unsigned long long>(a.digest), twice ? (same ? "twice-equal" : "DIFFERS") : "",
                    a.secs);
    }
    check(fails == 0, "R0a every seed builds, settles and seats");
    check(all_band, "R0b every seed opens on the industrial band");
    check(all_open, "R0c every nation opens holding no node");
    check(all_seat, "R0d ...and still holds none at the seat (nothing queued through the settle)");
    check(all_accrue, "R0e every nation with a research source at the seat accrues over the live window");
    std::printf("     %d of %d nations had no source at the seat (no treasury or no research line, and no\n"
                "     University or institute); %d of those accrued later anyway. %d nations hold a research building.\n",
                sourceless, total_nations, sourceless_accrued, bldg_nations);
    check(any_complete, "R0f the scripted queue completes nodes");
    if (twice) check(all_same, "R0g the whole run is byte-identical twice, per seed");

    const double med_q  = median_of(q_all);
    const double med_rp = median_of(rp_per_q_all);
    std::printf("\n  PACING (RESEARCH.md § Cost; BL-1273 defaults): over %d nations, median quarters from the\n"
                "  seat to DG-PR-EX-A (18 cost units with its path) = %.1f; %d never within %d quarters\n"
                "  (counted as %d). Median live RP per quarter = %.1f (%.2f units at %.0f RP/unit).\n",
                total_nations, med_q, never, quarters, quarters + 1, med_rp,
                med_rp / research_params{}.rp_per_cost_unit, research_params{}.rp_per_cost_unit);
    std::printf("  inputs at the seat: median treasury %.1f, median research-line share %.4f of it per quarter\n",
                median_of(treasury_all), median_of(share_all));
    std::vector<double> rps = rp_per_q_all;
    std::sort(rps.begin(), rps.end());
    if (!rps.empty())
        std::printf("  live RP/quarter: p10 %.1f  p25 %.1f  p50 %.1f  p75 %.1f  p90 %.1f  max %.1f\n",
                    rps[rps.size() / 10], rps[rps.size() / 4], rps[rps.size() / 2],
                    rps[rps.size() * 3 / 4], rps[rps.size() * 9 / 10], rps.back());
    std::vector<double> sorted = q_all;
    std::sort(sorted.begin(), sorted.end());
    if (!sorted.empty())
        std::printf("  quarters-to-major: p10 %.0f  p25 %.0f  p50 %.0f  p75 %.0f  p90 %.0f\n",
                    sorted[sorted.size() / 10], sorted[sorted.size() / 4], sorted[sorted.size() / 2],
                    sorted[sorted.size() * 3 / 4], sorted[sorted.size() * 9 / 10]);

    // THE RATE SOLVE. Nothing the research line spends depends on the RP rate
    // (the debit is the line's share; RP feed nothing back yet), so a nation's
    // cumulative RP scales linearly with rp_per_research_credit wherever no
    // University or institute contributes. For each nation: the rate at which
    // it would hold DG-PR-EX-A by live quarter k_target. The median of those is
    // the rate the pacing rule asks for; the spread says what it costs.
    constexpr int k_target = 10;
    const research_params p0{};
    const double path_rp = 18.0 * p0.rp_per_cost_unit;
    std::vector<double> need;
    for (const nation_row& r : all_rows)
    {
        if (static_cast<int>(r.cum_rp.size()) < k_target) continue;
        const double c = r.cum_rp[static_cast<std::size_t>(k_target - 1)];
        need.push_back(c > 0.0 ? p0.rp_per_research_credit * path_rp / c
                               : std::numeric_limits<double>::infinity());
    }
    if (!need.empty())
    {
        std::vector<double> ns = need;
        std::sort(ns.begin(), ns.end());
        const double rate = ns[ns.size() / 2];
        std::printf("\n  RATE SOLVE (research line only; %d nations hold a research building): the rate at which\n"
                    "  the median nation holds DG-PR-EX-A by live quarter %d = %.3g RP/credit at %.0f RP/unit.\n"
                    "  Needed rate by nation: p10 %.3g  p25 %.3g  p50 %.3g  p75 %.3g  p90 %.3g\n",
                    bldg_nations, k_target, rate, p0.rp_per_cost_unit, ns[ns.size() / 10], ns[ns.size() / 4],
                    ns[ns.size() / 2], ns[ns.size() * 3 / 4], ns[ns.size() * 9 / 10]);
        // What that rate does across the field: units per quarter over the
        // live window, and quarters to buy the whole tree (1087 units).
        std::vector<double> ups;
        for (const nation_row& r : all_rows)
            if (!r.cum_rp.empty())
                ups.push_back((r.cum_rp.back() - r.rp_settle) * rate / p0.rp_per_research_credit
                              / p0.rp_per_cost_unit / static_cast<double>(r.cum_rp.size()));
        std::sort(ups.begin(), ups.end());
        double total_units = 0.0;
        for (int n = 0; n < io::dg::node_count; ++n) total_units += io::dg::nodes[static_cast<std::size_t>(n)].cost_units;
        if (!ups.empty())
            std::printf("  At that rate, cost units per live quarter: p10 %.2f  p50 %.2f  p90 %.2f  max %.1f;\n"
                        "  the whole tree (%.0f units) would take the p90 nation %.0f quarters, the richest %.1f.\n",
                        ups[ups.size() / 10], ups[ups.size() / 2], ups[ups.size() * 9 / 10], ups.back(),
                        total_units, ups[ups.size() * 9 / 10] > 0 ? total_units / ups[ups.size() * 9 / 10] : -1.0,
                        ups.back() > 0 ? total_units / ups.back() : -1.0);
    }
}

} // namespace

int main(int argc, char** argv)
{
    int  seed_count = 16;
    int  quarters   = 16;
    bool twice      = true;
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seed_count = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seed-list") && i + 1 < argc) seeds = parse_list(argv[++i]);
        else if (!std::strcmp(argv[i], "--quarters") && i + 1 < argc) quarters = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--once")) twice = false;
        else
        {
            std::fprintf(stderr, "usage: nation_tech_harness [--seeds N | --seed-list a,b] [--quarters Q] [--once]\n");
            return 2;
        }
    }

    std::printf("nation_tech_harness (BL-1273): %d nodes in the Digitisation tree\n", io::dg::node_count);
    rows_r1();
    rows_r2();
    rows_r3();

    if (seeds.empty() && seed_count > 0)
    {
        seeds = library_seeds("docs/generation/seed_library.json");
        if (static_cast<int>(seeds.size()) > seed_count) seeds.resize(static_cast<std::size_t>(seed_count));
        if (seeds.empty()) { std::printf("  FAIL  no library seeds found\n"); ++g_failures; }
    }
    if (!seeds.empty())
        rows_r0(seeds, quarters, twice);

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
