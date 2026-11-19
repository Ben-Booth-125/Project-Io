// ---------------------------------------------------------------------------
// Headless rival join-war harness (BL-1706, wars draw in allies — the RIVAL SCORER half;
// no SDL / Lua / ImGui). docs/politics/RELATIONS.md § 7 "Joining a war" and
// "Capitulation"; docs/ai/AI_OPPONENT.md § 11, "Rival nations join wars" (Ben,
// 2026-11-19) and "Rival nations wage war whole" (capitulation's calibration).
// ---------------------------------------------------------------------------
// Each row drives `score_nation_war` - the rival's own pass - and reads what it DID
// through the trace it writes and the tables the verbs write; nothing here calls a war
// verb on the rival's behalf. Every "does nothing" row is a WHOLE-WORLD snapshot compare
// (save bytes plus `state_hash`).
//
//   JA1  join instead of a second declaration (+ / - at war with a member across / - at peace: declares)
//   JA2  a friend's defence (+ / - a friend of the declarer)
//   JA3  a leader answers a request (+ accept / - refuse on distrust)
//   JA4  a separate peace: a losing joiner offers (+ / - ahead), a leader answers one
//        (+ weary accepts / - fresh refuses), a joiner answers the leader's (+ / -)
//   JA5  capitulation, leader and joiner (+ losing badly and refused / - not refused)
//   JA6  the player's nation is never acted for (joiner, leader, asker)
//   JA7  twin-run replay
//
// The fixture is war_harness's: a 60 x 20 plains cylinder, its provinces dealt to five
// nations in west-to-east bands (A, B, C, D, E; a ring, A borders B and E), plus F with
// no ground.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/fronts.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/sentiment_emitters.hpp"
#include "world/stance.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

struct snap
{
    std::string bytes;
    uint64_t    hash = 0;
};
snap take(const world& w) { return snap{ to_bytes(w), w.state_hash(0) }; }
bool same(const snap& a, const world& w) { return a.bytes == to_bytes(w) && a.hash == w.state_hash(0); }

constexpr int GW = 60, GH = 20;

sentiment_params war_params()
{
    sentiment_params p;
    auto set = [&p](sentiment_factor_kind k, float a, float t, float al) {
        p.factors[static_cast<std::size_t>(k)].access = a;
        p.factors[static_cast<std::size_t>(k)].trust  = t;
        p.factors[static_cast<std::size_t>(k)].alarm  = al;
    };
    set(sentiment_factor_kind::hostility_declared, -5.0f, -5.0f,  5.0f);
    set(sentiment_factor_kind::war_declared,        0.0f, -2.0f,  3.0f);
    set(sentiment_factor_kind::peace_concluded,     0.0f,  3.0f, -3.0f);
    set(sentiment_factor_kind::province_ceded,     -3.0f, -3.0f,  3.0f);
    return p;
}

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id A = null_entity, B = null_entity, C = null_entity, D = null_entity,
              E = null_entity, F = null_entity;
    std::map<entity_id, std::vector<uint32_t>> provs; ///< each nation's provinces, ascending
};

void build(fixture& f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Flat"; bc.type = body_type::planet; bc.grid_width = GW; bc.grid_height = GH;
    w.bodies[f.body] = bc;
    for (int y = 0; y < GH; ++y)
        for (int x = 0; x < GW; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass;
            tc.cover_density = 150; tc.landform = terrain_landform::plains; tc.habitability = 0.5f;
            w.tiles[t] = tc;
        }
    build_province_partition(w, 0x1459u);
    const auto add_nation = [&](const char* name) {
        const entity_id n = w.create_entity();
        nation_component nc{};
        nc.name = name; nc.treasury = 1.0e6f;
        w.nations[n] = nc;
        return n;
    };
    f.A = add_nation("Aster"); f.B = add_nation("Brael"); f.C = add_nation("Corvan");
    f.D = add_nation("Dusk");  f.E = add_nation("Eld");   f.F = add_nation("Far");
    const entity_id band[5] = { f.A, f.B, f.C, f.D, f.E };
    for (const province& pr : w.provinces.provinces)
    {
        int minx = GW;
        for (const entity_id t : pr.tiles) minx = std::min(minx, w.tiles.at(t).grid_x);
        const entity_id n = band[std::min(4, minx * 5 / GW)];
        for (const entity_id t : pr.tiles)
        {
            w.set_tile_nation(t, n);
            w.nations.at(n).tiles.push_back(t);
        }
        f.provs[n].push_back(pr.id);
    }
    for (const entity_id n : band)
    {
        std::sort(f.provs[n].begin(), f.provs[n].end());
        auto& nc = w.nations.at(n);
        std::sort(nc.tiles.begin(), nc.tiles.end());
        nc.capital_tile = w.provinces.find(f.provs[n].front())->tiles.front();
    }
    seed_province_holders(w);
}

/// Stands @p men split over every @p stride-th tile of @p owner's span toward @p toward,
/// each with a logistics hub under it (war_harness's supplied front), the capital on the
/// middle post so the hubs chain.
std::size_t stand_front(world& w, entity_id owner, entity_id toward, int men, int stride)
{
    std::vector<entity_id> span;
    for (const front& fr : derive_fronts(w, owner))
        if (fr.neighbour == toward) span.insert(span.end(), fr.span.begin(), fr.span.end());
    std::vector<entity_id> posts;
    for (std::size_t i = 0; i < span.size(); i += static_cast<std::size_t>(stride)) posts.push_back(span[i]);
    if (!posts.empty()) w.nations.at(owner).capital_tile = posts[posts.size() / 2];
    for (const entity_id t : posts)
    {
        const entity_id u = w.create_entity();
        unit_component uc{}; uc.position = t; uc.owner = owner;
        uc.count = men / static_cast<int>(posts.size()); uc.establishment = uc.count;
        w.units[u] = uc;
        const entity_id b = w.create_entity();
        building_component bc{}; bc.type = building_type::inland_logistics_hub; bc.tile = t;
        w.buildings[b] = bc;
    }
    return span.size();
}

void grudge(world& w, const sentiment_params& sp, entity_id observer, entity_id subject, float magnitude)
{
    apply_sentiment_events(w.sentiment, sp,
                           { sentiment_event{ observer, subject, sentiment_factor_kind::hostility_declared,
                                              magnitude } });
}

void befriend(world& w, entity_id a, entity_id b) { w.corp_friend_pairs.insert(canonical_pair(a, b)); }

std::vector<war_ai_trace_row> trace;

bool did(war_ai_choice c, entity_id actor, entity_id target)
{
    for (const war_ai_trace_row& t : trace)
        if (t.choice == c && t.nation == actor && t.target == target && t.applied) return true;
    return false;
}
bool did_any(war_ai_choice c, entity_id actor)
{
    for (const war_ai_trace_row& t : trace)
        if (t.choice == c && t.nation == actor && t.applied) return true;
    return false;
}

/// C declares on B (goal: B's first province) and B answers (goal: C's first) at @p tick.
war_key_t open_cb(fixture& f, const sentiment_params& sp, int tick)
{
    f.w.current_econ_tick = tick;
    (void)declare_war(f.w, sp, f.C, f.B, { f.provs[f.B][0] });
    (void)answer_war(f.w, f.B, f.C, { f.provs[f.C][0] });
    return war_key_t{ f.C, f.B };
}

/// open_cb, then A asks to join C's side (goal: B's last province) and C accepts, at @p tick.
war_key_t open_cb_with_a(fixture& f, const sentiment_params& sp, int tick)
{
    const war_key_t key = open_cb(f, sp, tick);
    (void)request_join_war(f.w, f.A, key, 0, { f.provs[f.B].back() });
    (void)accept_join(f.w, sp, f.C, f.A);
    return key;
}

war_member* joiner_of(world& w, const war_key_t& key, entity_id n)
{
    for (war_member& m : w.wars.at(key).joiners)
        if (m.nation == n) return &m;
    return nullptr;
}

} // namespace

int main()
{
    std::printf("=== rival join-war scorer (BL-1706) ===\n");
    recipe_registry reg;
    reg.set_sentiment(war_params());
    const sentiment_params& sp = reg.sentiment();
    war_ai_trace_sink() = &trace;

    {
        fixture f; build(f);
        bool shape = true;
        for (const entity_id n : { f.A, f.B, f.C, f.D, f.E })
            shape = shape && f.provs[n].size() >= 4;
        check(shape, "setup: every banded nation holds at least four provinces");
        if (!shape) { std::printf("RESULT: %d failures / %d checks\n", failures + 1, checks); return 1; }
    }

    // =====================================================================
    std::printf("\nJA1 - join instead of a second declaration\n");
    {
        // + B is at war with C; A holds a grudge on B and a supplied 2:1 front toward it.
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb(f, sp, 20);
        grudge(w, sp, f.A, f.B, 2.0f);
        (void)stand_front(w, f.A, f.B, 2000, 2);
        trace.clear();
        score_nation_war(w, reg, f.A, 20);
        const auto rit = w.join_requests.find({ key, f.A });
        bool goals_ok = rit != w.join_requests.end() && !rit->second.goals.empty();
        if (rit != w.join_requests.end())
            for (const uint32_t g : rit->second.goals) goals_ok = goals_ok && province_owner(w, g) == f.B;
        check(rit != w.join_requests.end() && rit->second.side == 0 && goals_ok && did(war_ai_choice::join, f.A, f.B)
                  && !did_any(war_ai_choice::declare, f.A) && w.wars.size() == 1 && !at_war(w, f.A, f.B),
              "JA1 a rival with a motive on a nation already at war asks to join the side opposite it, declaring nothing");
        // The price: accepted, the join charges hostility_declared from B toward A.
        trace.clear();
        score_nation_war(w, reg, f.C, 20);
        std::size_t charged = 0;
        for (const sentiment_event& e : w.sentiment_pending)
            charged += (e.observer == f.B && e.subject == f.A && e.kind == sentiment_factor_kind::hostility_declared) ? 1 : 0;
        check(at_war(w, f.A, f.B) && war_side(w.wars.at(key), f.A) == 0 && charged == 1,
              "JA1b C accepts; A stands on C's side, at war with B, and pays the join price (B's hostility_declared)");
    }
    {
        // - the same, but A already fights E, who joined B's side: at war with a member across.
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb(f, sp, 20);
        (void)request_join_war(w, f.E, key, 1, { f.provs[f.C][1] });
        (void)accept_join(w, sp, f.B, f.E);
        (void)declare_war(w, sp, f.A, f.E, { f.provs[f.E][0] });
        grudge(w, sp, f.A, f.B, 2.0f);
        (void)stand_front(w, f.A, f.B, 2000, 2);
        check(war_side(w.wars.at(key), f.E) == 1 && at_war(w, f.A, f.E), "JA1c (setup) E joined B's side; A fights E");
        trace.clear();
        const snap s = take(w);
        score_nation_war(w, reg, f.A, 20);
        check(same(s, w) && !did_any(war_ai_choice::join, f.A) && !did_any(war_ai_choice::declare, f.A),
              "JA1c at war with a member across, it neither joins nor declares a second war on B");
    }
    {
        // - B at peace: the same rival declares (a new war only on a nation at peace).
        fixture f; build(f);
        world& w = f.w;
        w.current_econ_tick = 20;
        grudge(w, sp, f.A, f.B, 2.0f);
        (void)stand_front(w, f.A, f.B, 2000, 2);
        trace.clear();
        score_nation_war(w, reg, f.A, 20);
        check(did(war_ai_choice::declare, f.A, f.B) && w.join_requests.empty() && !did_any(war_ai_choice::join, f.A),
              "JA1d with B at peace the same rival declares, and files no join");
    }
    {
        // - no motive: nothing, though B is at war and A's front is supplied.
        fixture f; build(f);
        world& w = f.w;
        (void)open_cb(f, sp, 20);
        (void)stand_front(w, f.A, f.B, 2000, 2);
        trace.clear();
        const snap s = take(w);
        score_nation_war(w, reg, f.A, 20);
        check(same(s, w), "JA1e with no motive toward B, no join (never on the war alone)");
    }

    // =====================================================================
    std::printf("\nJA2 - a friend's defence\n");
    {
        // + D declares on C; B is C's friend.
        fixture f; build(f);
        world& w = f.w;
        w.current_econ_tick = 20;
        (void)declare_war(w, sp, f.D, f.C, { f.provs[f.C][0] });
        (void)answer_war(w, f.C, f.D, { f.provs[f.D][0] });
        befriend(w, f.B, f.C);
        trace.clear();
        score_nation_war(w, reg, f.B, 20);
        const auto rit = w.join_requests.find({ war_key_t{ f.D, f.C }, f.B });
        for (const war_ai_trace_row& t : trace)
            std::printf("  [info] JA2 trace choice %d target %llu u %.3f price %.3f trade %.3f goals %d applied %d\n",
                        static_cast<int>(t.choice), static_cast<unsigned long long>(t.target),
                        static_cast<double>(t.utility), static_cast<double>(t.price), static_cast<double>(t.trade),
                        t.goals, t.applied ? 1 : 0);
        check(rit != w.join_requests.end() && rit->second.side == 1 && rit->second.goals.size() == 1
                  && province_owner(w, rit->second.goals[0]) == f.D && did(war_ai_choice::join, f.B, f.D),
              "JA2 the defender's friend asks to join the defender's side, naming one of the declarer's provinces");
    }
    {
        // - B is the DECLARER's friend: the doc admits a friend of the defender only.
        fixture f; build(f);
        world& w = f.w;
        w.current_econ_tick = 20;
        (void)declare_war(w, sp, f.D, f.C, { f.provs[f.C][0] });
        (void)answer_war(w, f.C, f.D, { f.provs[f.D][0] });
        befriend(w, f.B, f.D);
        trace.clear();
        const snap s = take(w);
        score_nation_war(w, reg, f.B, 20);
        check(same(s, w), "JA2b a friend of the declarer files nothing");
    }
    {
        // - the friend is already weary (exhaustion residual 100): the utility falls under the bar.
        fixture f; build(f);
        world& w = f.w;
        w.current_econ_tick = 20;
        (void)declare_war(w, sp, f.D, f.C, { f.provs[f.C][0] });
        (void)answer_war(w, f.C, f.D, { f.provs[f.D][0] });
        befriend(w, f.B, f.C);
        w.war_exhaustion_residual[f.B] = 100.0f;
        trace.clear();
        const snap s = take(w);
        score_nation_war(w, reg, f.B, 20);
        check(same(s, w), "JA2c an exhausted friend weighs the join and files nothing (never on friendship alone)");
    }

    // =====================================================================
    std::printf("\nJA3 - a leader answers a request on its side\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb(f, sp, 20);
        (void)request_join_war(w, f.A, key, 0, { f.provs[f.B].back() });
        check(w.join_requests.count({ key, f.A }) == 1, "JA3 (setup) A's request waits");
        world refuse = w;
        trace.clear();
        score_nation_war(w, reg, f.C, 20);
        check(war_side(w.wars.at(key), f.A) == 0 && w.join_requests.empty() && did(war_ai_choice::accept_join, f.C, f.A),
              "JA3 a leader with no distrust of the asker accepts it through accept_join");
        // - C distrusts A (Trust -15, past the wary edge at -10).
        grudge(refuse, sp, f.C, f.A, 3.0f);
        trace.clear();
        score_nation_war(refuse, reg, f.C, 20);
        check(war_side(refuse.wars.at(key), f.A) < 0 && refuse.join_requests.empty()
                  && refuse.join_refusals.count({ key, f.A }) == 1 && did(war_ai_choice::refuse_join, f.C, f.A),
              "JA3b a leader that distrusts the asker refuses through refuse_join; the cooldown starts");
        // And A, cooling, does not ask again.
        grudge(refuse, sp, f.A, f.B, 2.0f);
        (void)stand_front(refuse, f.A, f.B, 2000, 2);
        trace.clear();
        const snap s = take(refuse);
        score_nation_war(refuse, reg, f.A, 21);
        check(same(s, refuse), "JA3c the refused asker does not ask again while its refusal cools");
    }

    // =====================================================================
    std::printf("\nJA4 - a separate peace\n");
    {
        // + A joined C's side at tick 10; 24 months on, its side is losing (score -25 for it).
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb_with_a(f, sp, 10);
        check(war_side(w.wars.at(key), f.A) == 0, "JA4 (setup) A joined C's side");
        joiner_of(w, key, f.A)->men_lost = 75;
        w.wars.at(key).defender_men_lost = 25;
        w.current_econ_tick = 34;
        world ahead = w;
        world leader_weary = w, leader_fresh = w, offered = w, offered_ahead = w;
        trace.clear();
        score_nation_war(w, reg, f.A, 34);
        const auto sit = w.separate_peace_offers.find({ f.A, f.B });
        check(sit != w.separate_peace_offers.end() && sit->second.offerer == f.A
                  && sit->second.terms.kind == peace_kind::white && did(war_ai_choice::offer, f.A, f.B),
              "JA4 a losing joiner offers the other side's leader a white separate peace");
        // - ahead: no offer.
        joiner_of(ahead, key, f.A)->men_lost = 25;
        ahead.wars.at(key).defender_men_lost = 75;
        trace.clear();
        const snap s = take(ahead);
        score_nation_war(ahead, reg, f.A, 34);
        check(same(s, ahead), "JA4b a joiner whose side is ahead offers no separate peace");

        // The leader answers it: + weary accepts; - fresh and winning refuses.
        for (world* x : { &leader_weary, &leader_fresh })
            (void)offer_peace(*x, f.A, f.B, peace_terms{});
        leader_weary.wars.at(key).defender_exhaustion = 80.0f;
        trace.clear();
        score_nation_war(leader_weary, reg, f.B, 34);
        check(!at_war(leader_weary, f.A, f.B) && at_war(leader_weary, f.C, f.B)
                  && did(war_ai_choice::accept, f.B, f.A),
              "JA4c a weary leader accepts the joiner's separate peace: the joiner alone leaves");
        trace.clear();
        score_nation_war(leader_fresh, reg, f.B, 34);
        check(at_war(leader_fresh, f.A, f.B) && leader_fresh.separate_peace_offers.empty()
                  && leader_fresh.peace_refusals.count({ f.A, f.B }) == 1 && did(war_ai_choice::refuse, f.B, f.A),
              "JA4d a fresh leader, winning, refuses it; the joiner's cooldown starts");

        // The joiner answers the leader's: + losing accepts; - ahead refuses.
        (void)offer_peace(offered, f.B, f.A, peace_terms{});
        trace.clear();
        score_nation_war(offered, reg, f.A, 34);
        check(!at_war(offered, f.A, f.B) && at_war(offered, f.C, f.B) && did(war_ai_choice::accept, f.A, f.B),
              "JA4e a losing joiner accepts the leader's white separate peace");
        joiner_of(offered_ahead, key, f.A)->men_lost = 25;
        offered_ahead.wars.at(key).defender_men_lost = 75;
        (void)offer_peace(offered_ahead, f.B, f.A, peace_terms{});
        trace.clear();
        score_nation_war(offered_ahead, reg, f.A, 34);
        check(at_war(offered_ahead, f.A, f.B) && offered_ahead.separate_peace_offers.empty()
                  && did(war_ai_choice::refuse, f.A, f.B),
              "JA4f a joiner whose side is ahead refuses it");
    }

    // =====================================================================
    std::printf("\nJA5 - capitulation\n");
    {
        // Leader: C declared on B at 10; C's side lost every battle (score -50), and its
        // offer to B was refused at 30.
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb(f, sp, 10);
        w.wars.at(key).declarer_men_lost = 100;
        w.current_econ_tick = 34;
        check(war_score(w, w.wars.at(key)) == -50, "JA5 (setup) the score stands at -50 against C");
        world unrefused = w;
        w.peace_refusals[{ f.C, f.B }] = 30;
        trace.clear();
        score_nation_war(w, reg, f.C, 34);
        check(!at_war(w, f.C, f.B) && !w.war_history.empty() && w.war_history.back().end == war_end::capitulation
                  && did(war_ai_choice::capitulate, f.C, f.B),
              "JA5 a leader losing badly whose last offer was refused capitulates; the war ends");
        trace.clear();
        score_nation_war(unrefused, reg, f.C, 34);
        check(at_war(unrefused, f.C, f.B) && !did_any(war_ai_choice::capitulate, f.C)
                  && did(war_ai_choice::offer, f.C, f.B),
              "JA5b the same score with no refused offer: it offers peace and does not capitulate");
    }
    {
        // Joiner: A on C's side; A's losses alone put the side at -50; its separate offer refused.
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb_with_a(f, sp, 10);
        joiner_of(w, key, f.A)->men_lost = 100;
        w.current_econ_tick = 34;
        world shallow = w;
        w.peace_refusals[{ f.A, f.B }] = 30;
        trace.clear();
        score_nation_war(w, reg, f.A, 34);
        check(!at_war(w, f.A, f.B) && at_war(w, f.C, f.B) && w.wars.count(key) == 1
                  && did(war_ai_choice::capitulate, f.A, f.B),
              "JA5c a joiner losing badly and refused capitulates: it alone leaves, the war goes on");
        // - losing, but not badly (-25), refused: neither capitulates nor offers while cooling.
        joiner_of(shallow, key, f.A)->men_lost = 75;
        shallow.wars.at(key).defender_men_lost = 25;
        shallow.peace_refusals[{ f.A, f.B }] = 30;
        trace.clear();
        const snap s = take(shallow);
        score_nation_war(shallow, reg, f.A, 34);
        check(same(s, shallow), "JA5d a joiner losing short of -50, refused: no capitulation, no repeated offer");
    }

    // =====================================================================
    std::printf("\nJA6 - the player's nation is never acted for\n");
    {
        // As a joiner losing badly and refused.
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb_with_a(f, sp, 10);
        joiner_of(w, key, f.A)->men_lost = 100;
        w.current_econ_tick = 34;
        w.peace_refusals[{ f.A, f.B }] = 30;
        w.player_entity = f.A;
        trace.clear();
        const snap s = take(w);
        score_nation_war(w, reg, f.A, 34);
        check(same(s, w) && trace.empty(), "JA6 the player's joiner: no capitulation, no offer");
    }
    {
        // As a leader with a request waiting.
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_cb(f, sp, 20);
        (void)request_join_war(w, f.A, key, 0, { f.provs[f.B].back() });
        w.player_entity = f.C;
        trace.clear();
        const snap s = take(w);
        score_nation_war(w, reg, f.C, 20);
        check(same(s, w) && trace.empty(), "JA6b the player's nation, a leader: the request waits for the player");
    }
    {
        // As a would-be asker with a motive and a supplied front.
        fixture f; build(f);
        world& w = f.w;
        (void)open_cb(f, sp, 20);
        grudge(w, sp, f.A, f.B, 2.0f);
        (void)stand_front(w, f.A, f.B, 2000, 2);
        w.player_entity = f.A;
        trace.clear();
        const snap s = take(w);
        score_nation_war(w, reg, f.A, 20);
        check(same(s, w) && trace.empty(), "JA6c the player's nation asks to join nothing");
    }

    // =====================================================================
    std::printf("\nJA7 - replay\n");
    {
        std::vector<std::string> bytes;
        std::vector<std::size_t> rows;
        std::vector<std::size_t> joins;
        for (int run = 0; run < 2; ++run)
        {
            fixture f; build(f);
            world& w = f.w;
            (void)open_cb(f, sp, 20);
            grudge(w, sp, f.A, f.B, 2.0f);
            befriend(w, f.D, f.B);
            (void)stand_front(w, f.A, f.B, 2000, 2);
            trace.clear();
            for (int t = 20; t < 60; ++t)
            {
                w.current_econ_tick = t;
                if (t == 40)
                    for (war_member& m : w.wars.begin()->second.joiners) m.men_lost += 400;
                for (const entity_id n : { f.A, f.B, f.C, f.D, f.E, f.F })
                    score_nation_war(w, reg, n, t);
            }
            bytes.push_back(to_bytes(w));
            rows.push_back(trace.size());
            std::size_t j = 0;
            for (const war_ai_trace_row& r : trace)
                j += (r.applied && (r.choice == war_ai_choice::join || r.choice == war_ai_choice::accept_join)) ? 1 : 0;
            joins.push_back(j);
        }
        std::printf("  [info] JA7 trace rows %zu, applied joins+accepts %zu\n", rows[0], joins[0]);
        check(bytes[0] == bytes[1] && rows[0] == rows[1] && joins[0] >= 2,
              "JA7 two built worlds, forty ticks of every nation's pass with joins in play, byte-identical");
    }

    war_ai_trace_sink() = nullptr;
    std::printf("\nRESULT: %d failures / %d checks\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
