// ---------------------------------------------------------------------------
// Headless joined-war harness (BL-1706, wars draw in allies; no SDL / Lua / ImGui) —
// docs/politics/RELATIONS.md § 7, "Joining a war" and "Capitulation" (Ben, 2026-11-19).
// ---------------------------------------------------------------------------
// The world layer of a war with sides: asking, accepting and refusing to join; "at
// war" across sides; the score per side; the leaders' cession handing each goal to the
// member that named it; a separate peace; capitulation, a joiner's and a leader's; a
// leader's end passing its side on; the v97 save; twin determinism. Every refusal is
// checked by a WHOLE-WORLD snapshot compare (save bytes plus `state_hash`).
//
// The fixture is war_harness's: a 60 x 20 plains cylinder, its provinces dealt to five
// nations in west-to-east bands (A, B, C, D, E), plus F with no ground.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/sentiment_emitters.hpp"
#include "world/stance.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cmath>
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

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
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
    build_province_partition(w, 0x1706u);
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

void set_holder(world& w, uint32_t pid, entity_id n)
{
    for (std::size_t i = 0; i < w.provinces.provinces.size(); ++i)
        if (w.provinces.provinces[i].id == pid) w.province_holder[i] = n;
}

corp_command cmd_of(corp_verb v, entity_id actor, entity_id other, std::vector<uint32_t> provs = {},
                    entity_id subject = null_entity, uint32_t order = 0)
{
    corp_command c;
    c.corp = actor; c.as_nation = actor; c.verb = v; c.counterparty = other; c.subject = subject;
    c.order = order;
    c.air_provinces = no_air_provinces();
    for (std::size_t i = 0; i < provs.size() && i < c.air_provinces.size(); ++i)
        c.air_provinces[i] = provs[i];
    return c;
}

std::size_t pending_of(const world& w, entity_id observer, entity_id subject, sentiment_factor_kind k)
{
    std::size_t n = 0;
    for (const sentiment_event& e : w.sentiment_pending)
        if (e.observer == observer && e.subject == subject && e.kind == k) ++n;
    return n;
}

/// The opening every scenario shares: A declares on C (goal C's province 0), and C
/// answers (goal A's province 0).
war_key_t open_war(fixture& f, const sentiment_params& sp)
{
    (void)declare_war(f.w, sp, f.A, f.C, { f.provs[f.C][0] });
    (void)answer_war(f.w, f.C, f.A, { f.provs[f.A][0] });
    return war_key_t{ f.A, f.C };
}

} // namespace

int main()
{
    std::printf("=== war join (BL-1706) ===\n");
    recipe_registry reg;
    reg.set_sentiment(war_params());
    const sentiment_params& sp = reg.sentiment();

    // =====================================================================
    std::printf("\nJ1 - a request is refused, mutating nothing, on every bad case\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_war(f, sp);
        const uint32_t a1 = f.provs[f.A][1], a2 = f.provs[f.A][2];
        const auto refused = [&](const corp_command& c, corp_command_result want) {
            const snap s = take(w);
            return apply_corp_command(w, reg, c) == want && same(s, w);
        };
        check(refused(cmd_of(corp_verb::request_join_war, f.B, f.C, { f.provs[f.C][1] }, f.A),
                      corp_command_result::rejected_invalid),
              "J1a a goal on the asker's chosen side's own ground is refused (rejected_invalid)");
        check(refused(cmd_of(corp_verb::request_join_war, f.B, f.C, {}, f.A), corp_command_result::rejected_invalid),
              "J1b no goal is refused (rejected_invalid)");
        check(refused(cmd_of(corp_verb::request_join_war, f.A, f.C, { a1 }, f.A), corp_command_result::rejected_state),
              "J1c a member may not ask (rejected_state)");
        check(refused(cmd_of(corp_verb::request_join_war, f.B, f.C, { a1 }, f.E), corp_command_result::rejected_state),
              "J1d no war between the two named leaders (rejected_state)");
        check(refused(cmd_of(corp_verb::request_join_war, f.B, f.C, { f.provs[f.A][0] }, f.A),
                      corp_command_result::rejected_state),
              "J1e a goal its side already named (C's answer) is refused (rejected_state)");
        {
            const snap s = take(w);
            check(request_join_war(w, f.B, key, 2, { a1 }) == war_result::rejected_invalid && same(s, w),
                  "J1f side 2 is refused (rejected_invalid)");
        }
        {
            w.truces[canonical_pair(f.B, f.A)] = w.current_econ_tick + 5;
            check(refused(cmd_of(corp_verb::request_join_war, f.B, f.C, { a1 }, f.A), corp_command_result::rejected_state),
                  "J1g a truce with a member across refuses the request (rejected_state)");
            w.truces.clear();
        }
        {
            world c = w;
            (void)declare_war(c, sp, f.D, f.A, { f.provs[f.A][3] });
            const snap s = take(c);
            check(request_join_war(c, f.D, key, 1, { a1 }) == war_result::rejected_state && same(s, c),
                  "J1h a nation already at war with a member across may not join (rejected_state)");
        }
        check(apply_corp_command(w, reg, cmd_of(corp_verb::request_join_war, f.B, f.C, { a2, a1 }, f.A))
                  == corp_command_result::applied
                  && w.join_requests.count({ key, f.B }) == 1
                  && w.join_requests.at({ key, f.B }).side == 1
                  && w.join_requests.at({ key, f.B }).goals == std::vector<uint32_t>({ std::min(a1, a2), std::max(a1, a2) }),
              "J1i a clean request is filed, side 1, goals ascending");
        check(refused(cmd_of(corp_verb::request_join_war, f.B, f.C, { a1 }, f.A), corp_command_result::rejected_state),
              "J1j one pending request per nation per war (rejected_state)");
        check(refused(cmd_of(corp_verb::accept_join, f.A, f.B), corp_command_result::rejected_state),
              "J1k the OTHER side's leader cannot accept it (rejected_state)");
        // Refuse; the cooldown runs a year.
        check(apply_corp_command(w, reg, cmd_of(corp_verb::refuse_join, f.C, f.B)) == corp_command_result::applied
                  && w.join_requests.empty() && w.join_refusals.at({ key, f.B }) == w.current_econ_tick,
              "J1l refuse_join erases the request and records the refusal tick");
        check(refused(cmd_of(corp_verb::request_join_war, f.B, f.C, { a1 }, f.A), corp_command_result::rejected_state),
              "J1m a refused nation may not ask again while cooling (rejected_state)");
        w.current_econ_tick += k_war_offer_cooldown_ticks;
        check(request_join_war(w, f.B, key, 1, { a1 }) == war_result::applied,
              "J1n after k_war_offer_cooldown_ticks it may ask again");
    }

    // =====================================================================
    std::printf("\nJ2 - accepting writes hostility across and appends the joiner; the price is charged\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_war(f, sp);
        w.sentiment_pending.clear();
        const uint32_t a1 = f.provs[f.A][1];
        w.corp_friend_pairs.insert(canonical_pair(f.B, f.C)); // B is the defender's friend
        (void)request_join_war(w, f.B, key, 1, { a1 });
        const war_price price = join_price(w, sp, key, f.B, 1, { a1 });
        check(price.valid && price.justified && price.justifications[3],
              "J2a joining the defender's side as its friend holds 'a friend attacked'");
        check(apply_corp_command(w, reg, cmd_of(corp_verb::accept_join, f.C, f.B)) == corp_command_result::applied,
              "J2b the side's leader accepts");
        const war_record* rec = war_between(w, f.B, f.A);
        check(rec != nullptr && rec->joiners.size() == 1 && rec->joiners[0].nation == f.B
                  && rec->joiners[0].side == 1 && rec->joiners[0].goals == std::vector<uint32_t>({ a1 })
                  && w.join_requests.empty(),
              "J2c the joiner is appended to the war (side 1, its goals); the request is gone");
        check(is_hostile(w, f.B, f.A) && is_hostile(w, f.A, f.B) && !is_hostile(w, f.B, f.C)
                  && are_friends(w, f.B, f.C),
              "J2d hostility both ways with the member across; none with its own side, the friendship kept");
        check(at_war(w, f.B, f.A) && at_war(w, f.A, f.B) && !at_war(w, f.B, f.C) && at_war(w, f.A, f.C)
                  && war_side(*rec, f.B) == 1 && war_side_members(*rec, 1) == std::vector<entity_id>({ f.C, f.B }),
              "J2e at_war across sides, not within one; the side lists leader then joiner");
        bool rows_ok = pending_of(w, f.A, f.B, sentiment_factor_kind::hostility_declared) == 1
                    && pending_of(w, f.D, f.B, sentiment_factor_kind::war_declared) == 1
                    && pending_of(w, f.C, f.B, sentiment_factor_kind::war_declared) == 1;
        std::size_t charged = 0;
        for (const sentiment_event& e : w.sentiment_pending)
            if (e.subject == f.B) ++charged;
        check(rows_ok && charged == price.rows.size(),
              "J2f the join is charged as join_price says: hostility_declared from A, war_declared from the rest");
        {
            world c = w;
            (void)request_join_war(c, f.E, key, 1, { f.provs[f.A][2] });
            c.truces[canonical_pair(f.E, f.A)] = c.current_econ_tick + 3; // E can no longer join
            const snap s = take(c);
            check(accept_join(c, sp, f.C, f.E) == war_result::rejected_state && same(s, c),
                  "J2h a request that no longer holds is refused at acceptance, mutating nothing");
            run_war_quarter(c);
            check(c.join_requests.empty(), "J2i the month's sweep lapses it");
        }
        check(nation_war_exhaustion(w, f.B) == 0.0f && declare_war(w, sp, f.B, f.A, { f.provs[f.A][3] })
                                                            == war_result::rejected_state,
              "J2g a joiner may not declare a second war on a member across");
    }

    // =====================================================================
    std::printf("\nJ3 - the score is per side; losses attribute to each member\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_war(f, sp);
        const uint32_t a1 = f.provs[f.A][1];
        (void)request_join_war(w, f.B, key, 1, { a1 });
        (void)accept_join(w, sp, f.C, f.B);
        const war_record& rec = w.wars.at(key);
        check(war_score(w, rec) == 0, "J3a nothing held, nothing lost: 0");
        set_holder(w, a1, f.B); // the joiner holds its own goal
        const double t0 = static_cast<double>(w.provinces.find(f.provs[f.A][0])->tiles.size());
        const double t1 = static_cast<double>(w.provinces.find(a1)->tiles.size());
        const int want = static_cast<int>(std::llround(-50.0 * t1 / (t0 + t1)));
        check(war_score(w, rec) == want && want < 0,
              "J3b the defender's side holds the joiner's goal: occupation counts it for the side, tile-weighted (" +
                  std::to_string(war_score(w, rec)) + ")");
        note_war_losses(w, { war_losses{ f.B, f.A, 0, 100 } });
        check(rec.declarer_men_lost == 100 && rec.joiners[0].men_lost == 0 && war_side_men_lost(rec, 0) == 100,
              "J3c a loss between a joiner and a leader lands on the right members");
        note_war_losses(w, { war_losses{ f.B, f.A, 100, 0 } });
        check(rec.joiners[0].men_lost == 100 && war_side_men_lost(rec, 1) == 100 && rec.joiners[0].exhaustion > 0.0f
                  && nation_war_exhaustion(w, f.B) == rec.joiners[0].exhaustion,
              "J3d the joiner's own losses raise its own tally and exhaustion; nation_war_exhaustion reads it");
    }

    // =====================================================================
    std::printf("\nJ4 - the leaders' cession hands each goal to the member that named it\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_war(f, sp);
        const uint32_t a1 = f.provs[f.A][1];
        (void)request_join_war(w, f.B, key, 1, { a1 });
        (void)accept_join(w, sp, f.C, f.B);
        (void)request_join_war(w, f.E, key, 1, { f.provs[f.A][2] }); // pending when the war closes
        set_holder(w, a1, f.B);
        peace_terms t;
        t.kind = peace_kind::cession; t.gainer = f.C; t.provinces = { a1 };
        check(offer_peace(w, f.C, f.A, t) == war_result::applied, "J4a C offers the cession of B's goal, held by B");
        check(accept_peace(w, sp, f.A, f.C) == war_result::applied, "J4b A accepts");
        check(province_owner(w, a1) == f.B && w.wars.empty() && w.join_requests.empty(),
              "J4c the province passes to B, who named it; the war is closed; the pending request lapsed");
        check(!at_war(w, f.A, f.B) && !is_hostile(w, f.A, f.B) && !is_hostile(w, f.B, f.A)
                  && truce_between(w, f.A, f.B) && truce_between(w, f.A, f.C),
              "J4d every pair across is at peace, hostility cleared, truced");
        check(!w.war_history.empty() && w.war_history.back().end == war_end::cession
                  && w.war_history.back().ceded == std::vector<uint32_t>({ a1 }),
              "J4e the history records the cession");
        check(pending_of(w, f.A, f.B, sentiment_factor_kind::province_ceded) == 1,
              "J4f province_ceded lands on the loser's row toward the namer");
    }

    // =====================================================================
    std::printf("\nJ5 - a separate peace takes the joiner alone out\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_war(f, sp);
        (void)request_join_war(w, f.B, key, 1, { f.provs[f.A][1] });
        (void)accept_join(w, sp, f.C, f.B);
        (void)request_join_war(w, f.D, key, 0, { f.provs[f.B][1] });
        check(accept_join(w, sp, f.A, f.D) == war_result::applied && at_war(w, f.D, f.B) && at_war(w, f.D, f.C),
              "J5a D joins the declarer's side: at war with both members across");
        {
            const snap s = take(w);
            check(offer_peace(w, f.B, f.D, peace_terms{}) == war_result::rejected_state && same(s, w),
                  "J5b two joiners have no peace of their own (rejected_state, nothing moved)");
            check(impose_peace(w, sp, f.B, f.A, peace_terms{}) == war_result::rejected_state && same(s, w),
                  "J5c a joiner cannot impose (rejected_state)");
        }
        check(offer_peace(w, f.B, f.A, peace_terms{}) == war_result::applied
                  && w.separate_peace_offers.count({ f.B, f.A }) == 1 && w.peace_offers.empty(),
              "J5d B offers A a separate white peace; it waits apart from the leaders' offer");
        check(accept_peace(w, sp, f.A, f.B) == war_result::applied, "J5e A accepts it");
        const war_record* rec = war_between(w, f.A, f.C);
        check(rec != nullptr && rec->joiners.size() == 1 && rec->joiners[0].nation == f.D
                  && !at_war(w, f.B, f.A) && !at_war(w, f.B, f.D) && at_war(w, f.D, f.C),
              "J5f only B left; A, C and D fight on");
        check(truce_between(w, f.B, f.A) && truce_between(w, f.B, f.D) && !is_hostile(w, f.B, f.D)
                  && rec->joiners[0].goals.empty(),
              "J5g B is truced with every member across; D's goal on B's ground dropped");
    }

    // =====================================================================
    std::printf("\nJ6 - capitulation: a joiner's cedes un-held goals; a leader's releases its joiners\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_war(f, sp);
        const uint32_t c0 = f.provs[f.C][0], b1 = f.provs[f.B][1], b2 = f.provs[f.B][2];
        (void)request_join_war(w, f.B, key, 1, { f.provs[f.A][1] });
        (void)accept_join(w, sp, f.C, f.B);
        (void)request_join_war(w, f.D, key, 0, { b1, b2 });
        (void)accept_join(w, sp, f.A, f.D);
        set_holder(w, b1, f.D); // b2 stays un-held
        {
            const snap s = take(w);
            check(apply_corp_command(w, reg, cmd_of(corp_verb::capitulate, f.B, f.C)) == corp_command_result::rejected_state
                      && same(s, w),
                  "J6a capitulating toward a sidemate is refused (rejected_state)");
        }
        check(apply_corp_command(w, reg, cmd_of(corp_verb::capitulate, f.B, f.D)) == corp_command_result::applied,
              "J6b B capitulates");
        check(province_owner(w, b1) == f.D && province_owner(w, b2) == f.D,
              "J6c both of D's goals on B's ground pass to D, held or not");
        const war_record* rec = war_between(w, f.A, f.C);
        check(rec != nullptr && !at_war(w, f.B, f.A) && truce_between(w, f.B, f.D) && at_war(w, f.D, f.C),
              "J6d only B left the war; truces with every member across");
        check(apply_corp_command(w, reg, cmd_of(corp_verb::capitulate, f.C, f.A)) == corp_command_result::applied,
              "J6e C, the defender's leader, capitulates");
        check(w.wars.empty() && province_owner(w, c0) == f.A && !at_war(w, f.D, f.C) && truce_between(w, f.D, f.C)
                  && truce_between(w, f.A, f.C),
              "J6f the war closes; A's goal on C's ground passes to A; D leaves with a white peace, truced");
        check(!w.war_history.empty() && w.war_history.back().end == war_end::capitulation
                  && w.war_history.back().ceded == std::vector<uint32_t>({ c0 }),
              "J6g the history records a capitulation and what it ceded");
    }

    // =====================================================================
    std::printf("\nJ7 - a leader whose nation ends hands its side to the earliest joiner\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const war_key_t key = open_war(f, sp);
        (void)request_join_war(w, f.B, key, 1, { f.provs[f.A][1] });
        (void)accept_join(w, sp, f.C, f.B);
        (void)request_join_war(w, f.D, key, 1, { f.provs[f.A][2] });
        (void)accept_join(w, sp, f.C, f.D);
        eliminate_nation(w, sp, f.C, f.E);
        const auto it = w.wars.find({ f.A, f.B });
        check(it != w.wars.end() && w.wars.size() == 1 && it->second.defender == f.B && it->second.answered
                  && it->second.joiners.size() == 1 && it->second.joiners[0].nation == f.D,
              "J7a the war is re-keyed (A, B): B leads the side, D still a joiner");
        check(it != w.wars.end() && it->second.declarer_goals.empty() && at_war(w, f.A, f.B) && at_war(w, f.A, f.D),
              "J7b A's goal on the ended nation's ground dropped; A still fights B and D");
    }

    // =====================================================================
    std::printf("\nJ8 - the v97 save round-trips the sides byte-identical; twins hash alike\n");
    {
        const auto run = [&](fixture& f) {
            world& w = f.w;
            const war_key_t key = open_war(f, sp);
            (void)request_join_war(w, f.B, key, 1, { f.provs[f.A][1] });
            (void)accept_join(w, sp, f.C, f.B);
            (void)request_join_war(w, f.D, key, 0, { f.provs[f.B][1] });
            (void)request_join_war(w, f.E, key, 0, { f.provs[f.C][2] });
            (void)refuse_join(w, f.A, f.E);
            (void)offer_peace(w, f.B, f.A, peace_terms{});
            note_war_losses(w, { war_losses{ f.B, f.A, 30, 40 } });
            accumulate_war_field_days(w, 10);
            run_war_quarter(w);
        };
        fixture f1; build(f1); run(f1);
        fixture f2; build(f2); run(f2);
        const std::string b = to_bytes(f1.w);
        world r;
        const bool ok = from_bytes(b, r);
        check(ok && to_bytes(r) == b && r.state_hash(0) == f1.w.state_hash(0) && r.join_requests.size() == 1
                  && r.join_refusals.size() == 1 && r.separate_peace_offers.size() == 1
                  && r.wars.begin()->second.joiners.size() == 1,
              "J8a a world with a joiner, a request, a refusal and a separate offer re-saves byte-identical");
        check(f1.w.state_hash(0) == f2.w.state_hash(0) && b == to_bytes(f2.w), "J8b twins hash and save alike");
        {
            world t = f1.w;
            t.wars.begin()->second.joiners[0].goals = {};
            check(t.state_hash(0) != f1.w.state_hash(0), "J8c the hash covers a joiner's goals");
        }
        // Tamper: the joiner's side byte (after the record's fixed fields and its goals).
        {
            world bad = f1.w;
            bad.corp_hostile_pairs.erase({ f1.B, f1.A }); // a joiner across without its hostility row
            world t;
            t.nations.emplace(999, nation_component{});
            check(!from_bytes(to_bytes(bad), t) && t.nations.size() == 1 && t.nations.count(999) == 1,
                  "J8d a stream whose joiner lacks its hostility row across is refused whole");
        }
        {
            fixture g; build(g);
            const std::string plain = to_bytes(g.w);
            world rr;
            check(from_bytes(plain, rr) && rr.join_requests.empty() && rr.state_hash(0) == g.w.state_hash(0),
                  "J8e a world with no war saves and loads as before (no sides)");
        }
        {
            fixture g; build(g);
            (void)open_war(g, sp);
            std::ostringstream o96(std::ios::binary);
            write_world_snapshot_legacy(g.w, {}, o96, k_world_save_version_pre_war_joiners);
            world rr;
            check(o96.str().size() + 32 == to_bytes(g.w).size() && from_bytes(o96.str(), rr)
                      && rr.wars.size() == 1 && rr.wars.begin()->second.joiners.empty()
                      && to_bytes(rr) == to_bytes(g.w) && rr.state_hash(0) == g.w.state_hash(0),
                  "J8f a v96 stream (32 bytes a war shorter) loads with no joiners and re-saves as today's");
        }
    }

    std::printf("\nRESULT: %d failures / %d checks\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
