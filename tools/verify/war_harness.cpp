// ---------------------------------------------------------------------------
// Headless war harness (BL-1459 war record and declaration, BL-1428 war goals and
// peace; no SDL / Lua / ImGui) — docs/politics/RELATIONS.md § 7.
// ---------------------------------------------------------------------------
// Requirement groups `war-record-and-declaration` (R1-R5) and `war-goals-and-peace`
// (R1-R4), and `rivals-declare-war` R1/R3 (BL-1461: the RW rows, a rival's war pass
// through the verbs, never for the player, replayable). Every refusal is checked by a WHOLE-WORLD snapshot compare (the save
// bytes, which carry sentiment, stance and every war table, plus `state_hash`),
// never by the return code alone — the untrusted-boundary rule.
//
// The fixture is a 60 x 20 plains cylinder, partitioned, its provinces dealt to five
// nations in west-to-east bands by their western-most tile column (A, B, C, D, E) —
// so A borders B and, across the wrap, E; C borders B and D — plus F, a nation with
// no ground, which borders nobody. Seeded holders follow ownership.
//
// The process exits non-zero if any assertion FAILs.

#include "world/battle_system.hpp"
#include "world/budget_system.hpp" // fix round: apply_budget files the conqueror's return
#include "world/components.hpp"
#include "world/corp_ai.hpp"      // BL-1461: score_nation_war, war_ai_trace_sink
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/embargo.hpp"      // BL-1562: set_embargo refuses an ended nation
#include "world/fronts.hpp"       // BL-1461: derive_fronts (where the RW rows stand a unit)
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
#include <cstring>
#include <functional>
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

/// The whole-world fingerprint a refusal must leave untouched.
struct snap
{
    std::string bytes;
    uint64_t    hash = 0;
};
snap take(const world& w) { return snap{ to_bytes(w), w.state_hash(0) }; }
bool same(const snap& a, const world& w) { return a.bytes == to_bytes(w) && a.hash == w.state_hash(0); }

constexpr int GW = 60, GH = 20;

/// Authored weights for the four war factors (the harness's own, so the rows read
/// the arithmetic, not scripts/economy.lua's calibration).
sentiment_params war_params()
{
    sentiment_params p;
    // scripts/economy.lua's rows (BL-1458's calibration), all three dimensions. Every
    // row below reads the weight back from this table, never a literal.
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
    std::vector<entity_id> cell;
    std::map<entity_id, std::vector<uint32_t>> provs; ///< each nation's provinces, ascending
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * GW + x)]; }
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
            f.cell.push_back(t);
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

std::size_t holder_index(const world& w, uint32_t pid)
{
    for (std::size_t i = 0; i < w.provinces.provinces.size(); ++i)
        if (w.provinces.provinces[i].id == pid) return i;
    return 0;
}

corp_command war_cmd(corp_verb v, entity_id actor, entity_id other, std::vector<uint32_t> provs,
                     uint32_t order = 0)
{
    corp_command c;
    c.corp = actor; c.as_nation = actor; c.verb = v; c.counterparty = other; c.order = order;
    c.air_provinces = no_air_provinces();
    for (std::size_t i = 0; i < provs.size() && i < c.air_provinces.size(); ++i)
        c.air_provinces[i] = provs[i];
    return c;
}

/// The bytes of every section the writer appends AFTER the v75 peace refusals (v77
/// provenance, v78 occupation, v79 defend orders, v80 research books, v82 growth
/// carry, v83 ended nations). The offset rows below count back from the stream's end and must step over
/// these; they had gone stale as each section landed (BL-1562 brought them current).
std::size_t tail_after_refusals(const world& w)
{
    return world_save_contributors_section_bytes(w) + world_save_occupation_section_bytes(w)
         + world_save_defend_section_bytes(w) + world_save_research_books_section_bytes(w)
         + world_save_growth_carry_section_bytes(w) + world_save_ended_section_bytes(w);
}

/// BL-1562: whether @p nation may order a PUSH on its front toward @p neighbour through
/// the command seam (`set_front_order`, the front card's press), tried on a COPY so the
/// fixture is untouched.
bool front_push_allowed_unanswered(const world& w, entity_id nation, entity_id neighbour)
{
    world c = w;
    const std::vector<front> fr = current_fronts(c, nation);
    for (const front& f : fr)
        if (f.neighbour == neighbour)
        {
            corp_command cmd;
            cmd.corp = nation; cmd.as_nation = nation; cmd.verb = corp_verb::set_front_order;
            cmd.tile = f.key; cmd.order = 1;
            recipe_registry reg;
            return apply_corp_command(c, reg, cmd) == corp_command_result::applied;
        }
    return false;
}

float trust_of(const world& w, entity_id o, entity_id s) { return sentiment_toward(w.sentiment, o, s).trust; }
float access_of(const world& w, entity_id o, entity_id s) { return sentiment_toward(w.sentiment, o, s).access; }
float alarm_of(const world& w, entity_id o, entity_id s) { return sentiment_toward(w.sentiment, o, s).alarm; }
const sentiment_factor& weight(const sentiment_params& p, sentiment_factor_kind k)
{
    return p.factors[static_cast<std::size_t>(k)];
}

} // namespace

// The ACTIONS.json entries name these numbers (corp_verb is append-only).
static_assert(static_cast<int>(corp_verb::declare_war) == 47 && static_cast<int>(corp_verb::answer_war) == 48
                  && static_cast<int>(corp_verb::offer_peace) == 49 && static_cast<int>(corp_verb::accept_peace) == 50
                  && static_cast<int>(corp_verb::refuse_peace) == 51 && static_cast<int>(corp_verb::impose_peace) == 52,
              "the war verbs' values as ACTIONS.json names them");

int main()
{
    std::printf("=== war (BL-1459, BL-1428) ===\n");
    recipe_registry reg;
    reg.set_sentiment(war_params());
    const sentiment_params& sp = reg.sentiment();

    {
        fixture f; build(f);
        bool shape = true;
        for (const entity_id n : { f.A, f.B, f.C, f.D, f.E })
            shape = shape && f.provs[n].size() >= 4;
        check(shape, "setup: every banded nation holds at least four provinces");
        if (!shape) { std::printf("RESULT: %d failures / %d checks\n", failures + 1, checks); return 1; }
    }

    // =====================================================================
    std::printf("\nW-R1 - declare_war writes hostility and the record; refusals mutate nothing\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> goals = { f.provs[f.B][2], f.provs[f.B][0] }; // any order
        // A friendship and a pending offer between the pair, to dissolve.
        w.corp_friend_pairs.insert(canonical_pair(f.A, f.B));
        w.corp_friend_offers.insert({ f.B, f.A });
        w.current_econ_tick = 7; w.construction_quarter_day = 33;
        const war_result r = declare_war(w, sp, f.A, f.B, goals);
        const war_record* rec = war_between(w, f.A, f.B);
        check(r == war_result::applied && is_hostile(w, f.A, f.B) && is_hostile(w, f.B, f.A),
              "R1a declare_war applies: BOTH hostility rows are written, declarer's and defender's (Ben, 2026-11-09)");
        check(front_push_allowed_unanswered(w, f.B, f.A),
              "R1a2 the defender may push at once, unanswered: the front's gate (is_hostile defender->declarer) is open");
        check(rec != nullptr && rec->declarer == f.A && rec->defender == f.B && !rec->answered
                  && rec->declarer_goals == std::vector<uint32_t>({ f.provs[f.B][0], f.provs[f.B][2] })
                  && rec->defender_goals.empty() && rec->opened_tick == 7 && rec->opened_day == 33,
              "R1b the record opens: declarer, defender, goals ascending, unanswered, opened tick/day");
        check(!are_friends(w, f.A, f.B) && !has_pending_friendship_offer(w, f.B, f.A),
              "R1c friendship between the pair dissolves, and the pending offer is withdrawn");

        const auto refused = [&](war_result got, war_result want, const snap& s, const char* what) {
            check(got == want && same(s, w), what);
        };
        snap s = take(w);
        refused(declare_war(w, sp, f.A, f.B, { f.provs[f.B][1] }), war_result::rejected_state, s,
                "R1d an existing war refuses a second declaration, mutating nothing");
        refused(declare_war(w, sp, f.B, f.A, { f.provs[f.A][1] }), war_result::rejected_state, s,
                "R1e ...in either direction (the defender cannot open a second war on the declarer)");
        refused(declare_war(w, sp, f.C, f.D, {}), war_result::rejected_invalid, s,
                "R1f zero goals is refused, mutating nothing");
        refused(declare_war(w, sp, f.C, f.D, { f.provs[f.D][0], f.provs[f.D][1], f.provs[f.D][2], f.provs[f.D][3] }),
                war_result::rejected_invalid, s, "R1g four goals is refused, mutating nothing");
        refused(declare_war(w, sp, f.C, f.D, { f.provs[f.C][0] }), war_result::rejected_invalid, s,
                "R1h a goal that is the declarer's own province is refused, mutating nothing");
        refused(declare_war(w, sp, f.C, f.D, { f.provs[f.E][0] }), war_result::rejected_invalid, s,
                "R1i a goal that is a third nation's province is refused, mutating nothing");
        refused(declare_war(w, sp, f.C, f.D, { f.provs[f.D][0], f.provs[f.D][0] }), war_result::rejected_invalid, s,
                "R1j a repeated goal is refused, mutating nothing");
        refused(declare_war(w, sp, f.C, f.C, { f.provs[f.C][0] }), war_result::rejected_invalid, s,
                "R1k declarer == defender is refused, mutating nothing");
        refused(declare_war(w, sp, f.C, 0x7FFFFFF0u, { f.provs[f.D][0] }), war_result::rejected_invalid, s,
                "R1l an unknown nation is refused, mutating nothing");
        refused(declare_war(w, sp, f.C, f.D, { 0x7FFFFFF0u }), war_result::rejected_invalid, s,
                "R1m an unknown province id is refused, mutating nothing");
        // A truce.
        w.truces[canonical_pair(f.C, f.D)] = w.current_econ_tick + 3;
        s = take(w);
        refused(declare_war(w, sp, f.D, f.C, { f.provs[f.C][0] }), war_result::rejected_state, s,
                "R1n a standing truce refuses the declaration (either party), mutating nothing");
        // return_to_neutral between war parties.
        refused(war_result(static_cast<uint8_t>(return_to_neutral(w, f.A, f.B))), war_result::rejected_state, s,
                "R1o return_to_neutral by the declarer is refused while the war stands, mutating nothing");
        refused(war_result(static_cast<uint8_t>(return_to_neutral(w, f.B, f.A))), war_result::rejected_state, s,
                "R1p ...and by the defender");
        corp_command rtn; rtn.corp = f.A; rtn.as_nation = f.A; rtn.verb = corp_verb::return_to_neutral;
        rtn.counterparty = f.B;
        check(apply_corp_command(w, reg, rtn) == corp_command_result::rejected_state && same(s, w),
              "R1q ...and through the command seam");
        corp_command dh; dh.corp = f.B; dh.as_nation = f.B; dh.verb = corp_verb::declare_hostile;
        dh.counterparty = f.A;
        check(apply_corp_command(w, reg, dh) == corp_command_result::rejected_state && same(s, w),
              "R1r a bare declare_hostile between war parties is refused (the defender answers instead)");
        // An expired truce no longer refuses.
        w.current_econ_tick += 3;
        check(!truce_between(w, f.C, f.D) && declare_war(w, sp, f.D, f.C, { f.provs[f.C][0] }) == war_result::applied,
              "R1s once the truce expires, the declaration applies");
        // Any of the defender's provinces, the capital's included.
        const uint32_t cap = w.provinces.province_of(w.nations.at(f.E).capital_tile);
        check(declare_war(w, sp, f.D, f.E, { cap }) == war_result::applied,
              "R1t the defender's capital province is a legal goal (Ben, 2026-10-30)");
    }

    // =====================================================================
    std::printf("\nW-R2 - the price, the justifications, preview == charge\n");
    {
        fixture f; build(f);
        world& w = f.w;
        // No justification: A declares on C (two goals). A borders B and E, C borders B
        // and D, so B, D, E watch from a border; F (landless) does not.
        const std::vector<uint32_t> goals = { f.provs[f.C][0], f.provs[f.C][1] };
        const war_price pr = declaration_price(w, sp, f.A, f.C, goals);
        bool none = pr.valid && !pr.justified;
        for (bool j : pr.justifications) none = none && !j;
        check(none, "R2a with no justification, none holds");
        std::map<entity_id, war_price_row> by;
        for (const war_price_row& r : pr.rows)
            by[r.observer * 2 + (r.kind == sentiment_factor_kind::hostility_declared ? 1 : 0)] = r;
        const war_price_row h = by[f.C * 2 + 1];
        check(h.kind == sentiment_factor_kind::hostility_declared && h.magnitude == 1.0f
                  && h.access == weight(sp, sentiment_factor_kind::hostility_declared).access
                  && h.trust == weight(sp, sentiment_factor_kind::hostility_declared).trust
                  && h.alarm == weight(sp, sentiment_factor_kind::hostility_declared).alarm,
              "R2b hostility_declared lands on the defender's row toward the declarer, once");
        const float gf = 1.0f + k_war_per_extra_goal; // two goals
        check(by[f.F * 2].magnitude == gf && by[f.F * 2].trust == weight(sp, sentiment_factor_kind::war_declared).trust * gf
                  && by[f.F * 2].alarm == weight(sp, sentiment_factor_kind::war_declared).alarm * gf && !by[f.F * 2].borders,
              "R2c war_declared on a non-bordering nation: base x the goal factor (two goals: x1.5)");
        check(by[f.B * 2].borders && by[f.D * 2].borders && by[f.E * 2].borders
                  && by[f.B * 2].magnitude == gf * k_war_border_multiplier
                  && by[f.D * 2].magnitude == gf * k_war_border_multiplier,
              "R2d a nation bordering either party watches twice as hard (B, D, E: x2)");
        check(by.count(f.A * 2) == 0 && by.count(f.C * 2) == 0 && pr.rows.size() == 1 + 4,
              "R2e every OTHER nation is priced, the two parties are not");
        const war_price one = declaration_price(w, sp, f.A, f.C, { f.provs[f.C][0] });
        const war_price three = declaration_price(w, sp, f.A, f.C, { f.provs[f.C][0], f.provs[f.C][1], f.provs[f.C][2] });
        float m1 = 0, m3 = 0;
        for (const auto& r : one.rows)   if (r.observer == f.F) m1 = r.magnitude;
        for (const auto& r : three.rows) if (r.observer == f.F) m3 = r.magnitude;
        check(m1 == 1.0f && m3 == 2.0f, "R2f each goal past the first raises the price (1 goal x1, 3 goals x2)");

        // Preview == charge.
        std::map<entity_id, sentiment_value> before;
        for (const auto& [nid, nc] : w.nations) before[nid] = sentiment_toward(w.sentiment, nid, f.A);
        const std::size_t queued_before = w.sentiment_pending.size();
        check(declare_war(w, sp, f.A, f.C, goals) == war_result::applied, "R2g the declaration applies");
        bool untouched = true;
        for (const auto& [nid, nc] : w.nations)
            untouched = untouched && sentiment_toward(w.sentiment, nid, f.A) == before[nid];
        check(untouched && w.sentiment_pending.size() == queued_before + pr.rows.size(),
              "R2g' the charge is GATHERED (queue_conduct): no row moves until the tick's fold");
        (void)fold_pending_sentiment(w, reg);
        bool eq = true;
        float sa = 0, st = 0, sal = 0;
        for (const war_price_row& r : pr.rows)
        {
            const sentiment_value now = sentiment_toward(w.sentiment, r.observer, f.A);
            eq = eq && now.access - before[r.observer].access == r.access
                    && now.trust - before[r.observer].trust == r.trust
                    && now.alarm - before[r.observer].alarm == r.alarm;
            sa += now.access - before[r.observer].access;
            st += now.trust - before[r.observer].trust;
            sal += now.alarm - before[r.observer].alarm;
        }
        check(eq && sa == pr.sum_access && st == pr.sum_trust && sal == pr.sum_alarm && pr.sum_alarm > 0.0f,
              "R2h after the fold, declare_war charged exactly the preview, per nation and summed, Alarm included");
        check(trust_of(w, f.A, f.C) == 0.0f && trust_of(w, f.C, f.C) == 0.0f,
              "R2i the declarer's own rows are not charged");

        // Each justification in turn, each detected by its row, each halving.
        const auto jtest = [&](std::size_t row, const char* label, auto&& arrange) {
            fixture g; build(g);
            arrange(g);
            const std::vector<uint32_t> gg = { g.provs[g.C][1] };
            const war_price p = declaration_price(g.w, sp, g.A, g.C, gg);
            bool only = p.valid && p.justified && p.justifications[row];
            for (std::size_t i = 0; i < p.justifications.size(); ++i)
                if (i != row) only = only && !p.justifications[i];
            float mf = 0;
            for (const auto& r : p.rows) if (r.observer == g.F && r.kind == sentiment_factor_kind::war_declared) mf = r.magnitude;
            check(only && mf == k_war_justified_multiplier,
                  std::string("R2j ") + label + " is detected by its row, alone, and halves the third-party price");
        };
        jtest(0, "grievance (declarer's Trust toward the defender in the hostile-leaning band, <= -15)", [&](fixture& g) {
            g.w.sentiment.pairs[{ g.A, g.C }] = sentiment_value{ 0.0f, -15.0f };
        });
        jtest(1, "embargo (the defender embargoes the declarer, BL-1418)", [&](fixture& g) {
            g.w.nation_embargoes.insert({ g.C, g.A });
        });
        jtest(2, "lost ground (a goal the declarer ceded to the defender at an earlier peace)", [&](fixture& g) {
            g.w.province_cessions.push_back(province_cession{ g.A, g.C, g.provs[g.C][1], 0 });
        });
        jtest(3, "a friend attacked (the defender declared war on the declarer's friend)", [&](fixture& g) {
            g.w.corp_friend_pairs.insert(canonical_pair(g.A, g.D));
            (void)declare_war(g.w, sp, g.C, g.D, { g.provs[g.D][0] });
        });
        {
            // Grievance holds at Trust <= -15 (Ben, 2026-11-03, NR-1104, NR-1129): -49, once
            // short of the adverse band, holds; -14.9, just short of -15, does not.
            fixture g; build(g);
            g.w.sentiment.pairs[{ g.A, g.C }] = sentiment_value{ 0.0f, -49.0f };
            const auto j49 = war_justifications(g.w, sp, g.A, g.C, { g.provs[g.C][1] });
            g.w.sentiment.pairs[{ g.A, g.C }] = sentiment_value{ 0.0f, -14.9f };
            const auto j = war_justifications(g.w, sp, g.A, g.C, { g.provs[g.C][1] });
            check(j49[0] && !j[0], "R2k grievance holds at Trust -49 (hostile-leaning) and not at -14.9 (NR-1129: the edge is -15)");
            // The lost ground must be THIS goal ceded by THIS declarer to THIS defender.
            g.w.province_cessions.push_back(province_cession{ g.B, g.C, g.provs[g.C][1], 0 });
            g.w.province_cessions.push_back(province_cession{ g.A, g.C, g.provs[g.C][2], 0 });
            const auto j2 = war_justifications(g.w, sp, g.A, g.C, { g.provs[g.C][1] });
            check(!j2[2], "R2l a province someone else ceded, or another goal, is not lost ground");
        }
        {
            // A refused declaration previews as invalid.
            fixture g; build(g);
            const war_price p = declaration_price(g.w, sp, g.A, g.C, { g.provs[g.A][0] });
            check(!p.valid && p.rows.empty(), "R2m a declaration that would be refused previews invalid, with no rows");
        }
    }

    // =====================================================================
    std::printf("\nW-R3 - the defender's answer is free\n");
    {
        fixture f; build(f);
        world& w = f.w;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        snap s = take(w);
        check(answer_war(w, f.B, f.A, { f.provs[f.B][1] }) == war_result::rejected_invalid && same(s, w),
              "R3a an answer naming the defender's own province is refused, mutating nothing");
        check(answer_war(w, f.B, f.A, { f.provs[f.A][0], f.provs[f.A][1], f.provs[f.A][2], f.provs[f.A][3] })
                      == war_result::rejected_invalid && same(s, w),
              "R3b an answer naming four goals is refused, mutating nothing");
        check(answer_war(w, f.C, f.A, { f.provs[f.A][0] }) == war_result::rejected_state && same(s, w),
              "R3c a nation answering a war that is not its own is refused, mutating nothing");
        check(answer_war(w, f.A, f.B, { f.provs[f.B][1] }) == war_result::rejected_state && same(s, w),
              "R3d the declarer cannot 'answer' its own war, mutating nothing");
        const sentiment_table before = w.sentiment;
        const std::size_t queued = w.sentiment_pending.size();
        check(answer_war(w, f.B, f.A, { f.provs[f.A][3], f.provs[f.A][1] }) == war_result::applied,
              "R3e the defender's answer applies");
        const war_record* rec = war_between(w, f.A, f.B);
        check(rec && rec->answered && is_hostile(w, f.B, f.A)
                  && rec->defender_goals == std::vector<uint32_t>({ f.provs[f.A][1], f.provs[f.A][3] }),
              "R3f the defender's hostility row stands and the answer names its goals (ascending)");
        check(before.pairs == w.sentiment.pairs && w.sentiment_pending.size() == queued,
              "R3g it costs nothing: no sentiment row moves and nothing is queued");
        s = take(w);
        check(answer_war(w, f.B, f.A, { f.provs[f.A][2] }) == war_result::rejected_state && same(s, w),
              "R3h a second answer is refused, mutating nothing");
    }

    // =====================================================================
    std::printf("\nW-R4 - the snapshot and the seam\n");
    {
        fixture f; build(f);
        world& w = f.w;
        // The seam: out-of-range lists reject the whole command.
        const snap s = take(w);
        corp_command gap = war_cmd(corp_verb::declare_war, f.A, f.B, { f.provs[f.B][0] });
        gap.air_provinces[2] = f.provs[f.B][1]; // a gap at slot 1
        check(apply_corp_command(w, reg, gap) == corp_command_result::rejected_invalid && same(s, w),
              "R4a a goal list with a gap is refused whole, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::declare_war, f.A, f.B,
                  { f.provs[f.B][0], f.provs[f.B][1], f.provs[f.B][2], f.provs[f.B][3] }))
                      == corp_command_result::rejected_invalid && same(s, w),
              "R4b a fourth goal is refused whole, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::declare_war, f.A, f.B, { 0u }))
                      == corp_command_result::rejected_invalid && same(s, w),
              "R4c a province id 0 is refused whole, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::declare_war, f.A, f.B, {}))
                      == corp_command_result::rejected_invalid && same(s, w),
              "R4d an empty goal list is refused whole, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::declare_war, f.A, 0x7FFFFFF0u, { f.provs[f.B][0] }))
                      == corp_command_result::rejected_invalid && same(s, w),
              "R4e an out-of-range counterparty is refused whole, mutating nothing");
        corp_command other = war_cmd(corp_verb::declare_war, f.A, f.B, { f.provs[f.B][0] });
        other.as_nation = f.C;
        check(apply_corp_command(w, reg, other) == corp_command_result::rejected_not_owner && same(s, w),
              "R4f acting for another nation is refused, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::declare_war, f.A, f.B, { f.provs[f.B][0] }))
                      == corp_command_result::applied && at_war(w, f.A, f.B),
              "R4g a well-formed declare_war applies through the seam");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::answer_war, f.B, f.A, { f.provs[f.A][0] }))
                      == corp_command_result::applied && war_between(w, f.A, f.B)->answered,
              "R4h a well-formed answer_war applies through the seam");

        // The snapshot.
        w.truces[canonical_pair(f.C, f.D)] = 40;
        w.province_cessions.push_back(province_cession{ f.D, f.E, f.provs[f.D][0], 3 });
        w.war_exhaustion_residual[f.E] = 12.5f;
        w.wars.at({ f.A, f.B }).declarer_men_lost = 120;
        w.wars.at({ f.A, f.B }).defender_exhaustion = 33.25f;
        const std::string b = to_bytes(w);
        world back;
        const bool ok = from_bytes(b, back);
        check(ok && to_bytes(back) == b && back.state_hash(0) == w.state_hash(0)
                  && back.wars.size() == 1 && back.truces.size() == 1
                  && back.province_cessions.size() == 1 && back.war_exhaustion_residual.size() == 1,
              "R4i the wars, truces, cessions and residual round-trip the snapshot byte for byte");
        uint32_t ver = 0; std::memcpy(&ver, b.data() + 4, 4);
        check(ver == world_save_version && world_save_version >= 71, "R4j the stream is version 71 or later (BL-1418's embargo section, then BL-1460's generation capitals, follow the wars)");
        check(b.size() >= world_save_war_section_bytes(w), "R4k the war section's measured length fits the stream");
        // An older stream loads with no wars.
        std::ostringstream o67(std::ios::binary);
        write_world_snapshot_pre_war(w, o67);
        const std::string b67 = o67.str();
        world old;
        check(from_bytes(b67, old) && old.wars.empty() && old.truces.empty() && old.peace_offers.empty()
                  && old.war_history.empty() && old.province_cessions.empty()
                  && b67.size() == b.size() - world_save_war_section_bytes(w)
                                              - world_save_embargo_section_bytes(w) - world_save_gen_capital_section_bytes(w) - world_save_recruits_section_bytes(w) - world_save_sea_recruits_section_bytes(w) - world_save_refusals_section_bytes(w) /* BL-1480 (v75) */ - tail_after_refusals(w) /* v77..v83 */ // BL-1418 (v70)
                                              - world_save_kit_goods_bytes(w), // BL-1422: arrays 49 wide before v74
              "R4l a v68 stream (today's less the war and embargo sections) loads with no wars");
        // Untrusted: an answered byte of 2 is refused whole.
        std::string bad = b;
        const std::size_t war0 = b.size() - world_save_embargo_section_bytes(w) - world_save_gen_capital_section_bytes(w) - world_save_recruits_section_bytes(w) - world_save_sea_recruits_section_bytes(w) - world_save_refusals_section_bytes(w) /* BL-1480 (v75) */ - tail_after_refusals(w) /* v77..v83 */ // BL-1418 (v70), last
                               - world_save_war_section_bytes(w) + 4; // past the war count
        bad[war0 + 8] = 2;
        world untouched;
        check(!from_bytes(bad, untouched) && untouched.nations.empty(),
              "R4m a corrupt war record (answered byte 2) is refused whole, the destination untouched");
        // A war naming an unknown nation is refused.
        std::string bad2 = b;
        const entity_id ghost = 0x7FFFFFF0u;
        std::memcpy(&bad2[war0], &ghost, sizeof ghost);
        world untouched2;
        check(!from_bytes(bad2, untouched2) && untouched2.nations.empty(),
              "R4n a war naming no nation in the stream is refused whole");
    }

    // =====================================================================
    std::printf("\nW-R5 - the right-click declare-and-attack declares through declare_war\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const uint32_t target = f.provs[f.B][2];
        const entity_id tile = w.provinces.find(target)->tiles.back();
        check(attack_goal_province(w, f.B, tile) == target,
              "R5a the attacked tile's province is the goal when the defender owns it");
        const entity_id own = w.provinces.find(f.provs[f.A][1])->tiles.front();
        check(attack_goal_province(w, f.B, own) == w.provinces.province_of(w.nations.at(f.B).capital_tile),
              "R5b a stack on ground not the defender's names the defender's capital province instead");
        // The app's path: declare_war through the seam with that province as the first goal.
        corp_command c = war_cmd(corp_verb::declare_war, f.A, f.B, {});
        c.air_provinces[0] = attack_goal_province(w, f.B, tile);
        check(apply_corp_command(w, reg, c) == corp_command_result::applied
                  && war_between(w, f.A, f.B)->declarer_goals.front() == target && is_hostile(w, f.A, f.B),
              "R5c the declare opens a war whose first goal is the attacked tile's province");
    }

    // =====================================================================
    std::printf("\nP-R1 - the war score\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> dg = { f.provs[f.B][0], f.provs[f.B][1] };
        const std::vector<uint32_t> fg = { f.provs[f.A][0] };
        (void)declare_war(w, sp, f.A, f.B, dg);
        (void)answer_war(w, f.B, f.A, fg);
        const auto score = [&] { return war_score(w, *war_between(w, f.A, f.B)); };
        const auto tiles = [&](uint32_t p) { return static_cast<double>(w.provinces.find(p)->tiles.size()); };
        check(score() == 0, "P1a a fresh war scores 0 (no goal held, no battle)");
        // NR-1103 (Ben, 2026-10-31): each side's occupation is normalised by ITS OWN goals.
        w.province_holder[holder_index(w, dg[0])] = f.A;
        const double dtot = tiles(dg[0]) + tiles(dg[1]);
        const int want1 = static_cast<int>(std::llround(50.0 * tiles(dg[0]) / dtot));
        check(score() == want1, "P1b the declarer holding one goal scores 50 x its tiles over ITS goals' tiles ("
                                    + std::to_string(want1) + ")");
        w.province_holder[holder_index(w, fg[0])] = f.B;
        const int want2 = static_cast<int>(std::llround(50.0 * (tiles(dg[0]) / dtot - 1.0)));
        check(score() == want2, "P1c the defender holding all of its goals subtracts its full share, 50 ("
                                    + std::to_string(want2) + ")");
        war_record& rec = w.wars.at({ f.A, f.B });
        rec.declarer_men_lost = 100; rec.defender_men_lost = 300;
        const int want3 = static_cast<int>(std::llround(50.0 * (tiles(dg[0]) / dtot - 1.0) + 50.0 * 200.0 / 400.0));
        check(score() == want3, "P1d battles: the declarer's share of strength destroyed less the defender's, half the score ("
                                    + std::to_string(want3) + ")");
        w.province_holder[holder_index(w, dg[1])] = f.A;
        w.province_holder[holder_index(w, fg[0])] = f.A; // the defender holds none of its goals
        rec.declarer_men_lost = 0;
        check(score() == 100,
              "P1e answered war: the declarer's full occupation reaches the whole occupation half (+50), +100 with only defender losses");
        w.province_holder[holder_index(w, dg[0])] = f.B; w.province_holder[holder_index(w, dg[1])] = f.B;
        w.province_holder[holder_index(w, fg[0])] = f.B;
        rec.declarer_men_lost = 500; rec.defender_men_lost = 0;
        check(score() == -100,
              "P1f the mirror: the defender's full occupation reaches the whole half (-50), -100 with only declarer losses");
        {
            // An unanswered war: the declarer's goals are every goal named, so +100 is reachable.
            fixture g; build(g);
            const std::vector<uint32_t> gg = { g.provs[g.B][0], g.provs[g.B][1] };
            (void)declare_war(g.w, sp, g.A, g.B, gg);
            for (const uint32_t p : gg) g.w.province_holder[holder_index(g.w, p)] = g.A;
            g.w.wars.at({ g.A, g.B }).defender_men_lost = 10;
            check(war_score(g.w, *war_between(g.w, g.A, g.B)) == 100,
                  "P1h unanswered: every goal held and only defender losses scores +100");
        }
        bool in_range = true;
        for (int dl : { 0, 1, 7, 1000 }) for (int fl : { 0, 3, 999 })
        {
            rec.declarer_men_lost = dl; rec.defender_men_lost = fl;
            const int s = score();
            in_range = in_range && s >= -100 && s <= 100;
        }
        check(in_range, "P1g the score stays in [-100, 100]");
    }

    // =====================================================================
    std::printf("\nP-R2 - exhaustion and morale\n");
    {
        fixture f; build(f);
        world& w = f.w;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        const auto unit = [&](entity_id owner, entity_id tile, int men) {
            const entity_id u = w.create_entity();
            unit_component uc{}; uc.position = tile; uc.owner = owner; uc.count = men; uc.establishment = men;
            w.units[u] = uc;
            return u;
        };
        const entity_id home_tile  = w.provinces.find(f.provs[f.A][1])->tiles.front();
        const entity_id enemy_tile = w.provinces.find(f.provs[f.B][2])->tiles.front();
        const entity_id ua = unit(f.A, home_tile, 900);
        const entity_id ub = unit(f.B, w.provinces.find(f.provs[f.B][1])->tiles.front(), 1000);
        // Strength lost: a dispatch of a battle between the pair.
        battle_tick bt;
        battle_dispatch d; d.attacker = f.A; d.defender = f.B; d.attacker_men_lost = 100; d.defender_men_lost = 0;
        bt.dispatches.push_back(d);
        note_war_battles(w, bt);
        war_record* rec = &w.wars.at({ f.A, f.B });
        const float want = k_war_exhaustion_loss_full * 100.0f / (900.0f + 100.0f);
        check(rec->declarer_men_lost == 100 && std::fabs(rec->declarer_exhaustion - want) < 1e-4f
                  && rec->defender_exhaustion == 0.0f,
              "P2a strength lost raises its side's exhaustion (60 x 100 lost / 1000 standing-plus-lost = 6) and only its side's");
        // A battle between nations not at war with each other moves nothing.
        battle_tick other; battle_dispatch od = d; od.attacker = f.C; od.defender = f.D; other.dispatches.push_back(od);
        const snap s0 = take(w);
        note_war_battles(w, other);
        check(same(s0, w), "P2b a battle between nations not at war moves no war");
        // Field days: one of A's two units stands on B's ground for a quarter.
        const float e0 = rec->declarer_exhaustion;
        const entity_id ua2 = unit(f.A, enemy_tile, 100);
        accumulate_war_field_days(w, 90); // the quarter's 90 days, as advance_quarter_to walks them
        run_war_quarter(w);
        rec = &w.wars.at({ f.A, f.B });
        const float field = k_war_exhaustion_per_field_day * 90.0f * 0.5f; // half its units abroad
        check(std::fabs(rec->declarer_exhaustion - (e0 + field)) < 1e-3f && rec->defender_exhaustion == 0.0f,
              "P2c each day in the field raises exhaustion (0.1 a day x 90 days x half the force abroad = 4.5)");
        // Provinces the enemy holds.
        w.province_holder[holder_index(w, f.provs[f.B][3])] = f.A;
        const float e1 = rec->defender_exhaustion;
        w.units.erase(ua2);
        run_war_quarter(w);
        rec = &w.wars.at({ f.A, f.B });
        check(std::fabs(rec->defender_exhaustion - (e1 + k_war_exhaustion_per_held_province)) < 1e-4f,
              "P2d each of its provinces the enemy holds raises the owner's exhaustion (2 a quarter)");
        // Morale: the ceiling follows exhaustion.
        rec->declarer_exhaustion = 40.0f;
        w.units.at(ua).morale_permille = 1000; w.units.at(ua).readiness_permille = 1000;
        apply_war_morale_ceiling(w);
        check(war_morale_ceiling(w, f.A) == 800 && w.units.at(ua).morale_permille == 800
                  && w.units.at(ua).readiness_permille == 800
                  && w.units.at(ub).morale_permille == war_morale_ceiling(w, f.B)
                  && war_morale_ceiling(w, f.B) == 1000 - static_cast<int>(rec->defender_exhaustion * 5.0f + 0.5f),
              "P2e exhaustion moves morale: 40 exhaustion caps the nation's units at 800, readiness under it; the other side by its own");
        // Out-of-contact recovery does not lift morale back above the ceiling.
        w.units.at(ua).morale_permille = 500; w.units.at(ua).readiness_permille = 500;
        for (int day = 0; day < 90; ++day) recover_out_of_contact(w);
        check(w.units.at(ua).morale_permille == 800 && w.units.at(ua).readiness_permille == 800 && !recovery_pending(w),
              "P2f daily recovery climbs no higher than the ceiling (" + std::to_string(w.units.at(ua).morale_permille) + ")");
        // Exhaustion stays in [0, 100].
        rec->declarer_exhaustion = 99.0f;
        battle_tick big; battle_dispatch bd = d; bd.attacker_men_lost = 100000; big.dispatches.push_back(bd);
        note_war_battles(w, big);
        rec = &w.wars.at({ f.A, f.B });
        check(rec->declarer_exhaustion == 100.0f, "P2g exhaustion saturates at 100");
        // Decay at peace: the residual carries out and falls while at peace.
        rec->declarer_exhaustion = 40.0f;
        check(offer_peace(w, f.A, f.B, peace_terms{}) == war_result::applied
                  && accept_peace(w, sp, f.B, f.A) == war_result::applied,
              "P2h (a white peace closes the war)");
        const float r0 = w.war_exhaustion_residual.count(f.A) ? w.war_exhaustion_residual.at(f.A) : -1.0f;
        run_war_quarter(w);
        const float r1 = w.war_exhaustion_residual.count(f.A) ? w.war_exhaustion_residual.at(f.A) : -1.0f;
        check(r0 == 40.0f && std::fabs(r1 - 40.0f * (1.0f - k_war_exhaustion_decay)) < 1e-4f,
              "P2i at peace the nation's exhaustion carries out of the war and decays (40 -> 30 a quarter)");
        for (int q = 0; q < 40; ++q) run_war_quarter(w);
        check(w.war_exhaustion_residual.count(f.A) == 0 && war_morale_ceiling(w, f.A) == 1000,
              "P2j and is gone in time, the morale ceiling back at 1000");
        // A nation still at war does not shed its residual.
        w.war_exhaustion_residual[f.C] = 20.0f;
        w.current_econ_tick += 100; // past every truce
        (void)declare_war(w, sp, f.C, f.D, { f.provs[f.D][0] });
        run_war_quarter(w);
        check(w.war_exhaustion_residual.at(f.C) == 20.0f, "P2k a nation in an open war keeps its residual");
    }

    // =====================================================================
    std::printf("\nP-R3 - offers, cession, impose, truce, the peace factors\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> dg = { f.provs[f.B][0], f.provs[f.B][1], f.provs[f.B][2] };
        (void)declare_war(w, sp, f.A, f.B, dg);
        (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
        const auto key = std::make_pair(f.A, f.B);
        peace_terms white;
        peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = { dg[0] };

        snap s = take(w);
        check(offer_peace(w, f.A, f.B, cede) == war_result::rejected_state && same(s, w),
              "P3a a cession of a goal the receiver does not hold is refused, mutating nothing");
        w.province_holder[holder_index(w, dg[0])] = f.A;
        peace_terms nongoal = cede; nongoal.provinces = { f.provs[f.B][3] };
        w.province_holder[holder_index(w, f.provs[f.B][3])] = f.A;
        s = take(w);
        check(offer_peace(w, f.A, f.B, nongoal) == war_result::rejected_state && same(s, w),
              "P3b a cession of a held province that is not a goal is refused, mutating nothing");
        peace_terms badshape = white; badshape.provinces = { dg[0] };
        check(offer_peace(w, f.A, f.B, badshape) == war_result::rejected_invalid && same(s, w),
              "P3c white peace naming a province is refused, mutating nothing");
        check(offer_peace(w, f.A, f.C, white) == war_result::rejected_state && same(s, w),
              "P3d an offer with no war between the pair is refused, mutating nothing");
        check(offer_peace(w, f.A, f.B, cede) == war_result::applied && w.peace_offers.count(key) == 1
                  && at_war(w, f.A, f.B),
              "P3e a legal offer files in its own table; a pending offer is not a peace");
        s = take(w);
        check(offer_peace(w, f.B, f.A, white) == war_result::rejected_state && same(s, w),
              "P3f one pending offer per war: a second (from either side) is refused, mutating nothing");
        check(accept_peace(w, sp, f.A, f.B) == war_result::rejected_state && same(s, w),
              "P3g the offerer cannot accept its own offer, mutating nothing");
        check(refuse_peace(w, f.B, f.A) == war_result::applied && w.peace_offers.empty() && at_war(w, f.A, f.B),
              "P3h refusing erases the offer; the war goes on");
        // Cession accepted. The refused offerer waits out the four-tick cooldown first
        // (Ben, 2026-11-03, NR-1120; RW11 pins the cooldown itself).
        w.current_econ_tick += k_war_offer_cooldown_ticks;
        check(offer_peace(w, f.A, f.B, cede) == war_result::applied, "P3i (re-offer the cession, the cooldown served)");
        (void)fold_pending_sentiment(w, reg); // the declaration's own conduct, folded first
        const float trust_ab = trust_of(w, f.A, f.B), trust_ba = trust_of(w, f.B, f.A);
        const float acc_ba = access_of(w, f.B, f.A);
        const float al_ab = alarm_of(w, f.A, f.B), al_ba = alarm_of(w, f.B, f.A);
        w.current_econ_tick = 20;
        check(accept_peace(w, sp, f.B, f.A) == war_result::applied, "P3j the receiver accepts the cession");
        check(!at_war(w, f.A, f.B) && !is_hostile(w, f.A, f.B) && !is_hostile(w, f.B, f.A)
                  && w.peace_offers.empty() && w.war_history.size() == 1
                  && w.war_history.back().end == war_end::cession && w.war_history.back().gainer == f.A
                  && w.war_history.back().ceded == std::vector<uint32_t>({ dg[0] }),
              "P3k at peace both hostility rows clear and the record closes into the history");
        check(w.province_cessions.size() == 1 && w.province_cessions[0].from == f.B && w.province_cessions[0].to == f.A
                  && w.province_cessions[0].province == dg[0]
                  && province_holder_for(w, dg[0]) == f.A && province_owner(w, dg[0]) == f.A,
              "P3l the cession records the ceded province, keeps its holder, and moves its ground to the gainer (BL-1460)");
        (void)fold_pending_sentiment(w, reg);
        const sentiment_factor& pc = weight(sp, sentiment_factor_kind::peace_concluded);
        const sentiment_factor& ce = weight(sp, sentiment_factor_kind::province_ceded);
        check(trust_of(w, f.A, f.B) - trust_ab == pc.trust
                  && std::fabs(trust_of(w, f.B, f.A) - trust_ba - (pc.trust + ce.trust)) < 1e-5f
                  && access_of(w, f.B, f.A) - acc_ba == ce.access
                  // Alarm floors at 0 (NR-1115): peace's relief folds first (kind order),
                  // floored, then the cession's Alarm lands on top.
                  && std::fabs(alarm_of(w, f.A, f.B) - std::max(0.0f, al_ab + pc.alarm)) < 1e-5f
                  && std::fabs(alarm_of(w, f.B, f.A) - (std::max(0.0f, al_ba + pc.alarm) + ce.alarm)) < 1e-5f,
              "P3m peace_concluded warms both rows; province_ceded lands on the loser's row toward the winner (x provinces ceded)");
        check(truce_between(w, f.A, f.B) && w.truces.at(canonical_pair(f.A, f.B)) == 20 + k_war_truce_ticks,
              "P3n a truce of 16 ticks stands");
        s = take(w);
        check(declare_war(w, sp, f.A, f.B, { dg[1] }) == war_result::rejected_state
                  && declare_war(w, sp, f.B, f.A, { f.provs[f.A][2] }) == war_result::rejected_state && same(s, w),
              "P3o the truce refuses declare_war either way, mutating nothing");
        w.current_econ_tick = 20 + k_war_truce_ticks - 1;
        check(declare_war(w, sp, f.B, f.A, { f.provs[f.A][2] }) == war_result::rejected_state,
              "P3p ...through its fifteenth tick");
        w.current_econ_tick = 20 + k_war_truce_ticks;
        // The lost ground the loser can now name.
        const auto j = war_justifications(w, sp, f.B, f.A, { dg[0] });
        check(j[2], "P3q the ceded province is LOST GROUND for the loser (a goal it ceded to the declarer's target)");
    }
    {
        // White peace hands the occupied goals back.
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> dg = { f.provs[f.B][0] };
        (void)declare_war(w, sp, f.A, f.B, dg);
        (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
        w.province_holder[holder_index(w, dg[0])] = f.A;
        w.province_holder[holder_index(w, f.provs[f.A][0])] = f.B;
        check(offer_peace(w, f.B, f.A, peace_terms{}) == war_result::applied
                  && accept_peace(w, sp, f.A, f.B) == war_result::applied
                  && province_holder_for(w, dg[0]) == f.B && province_holder_for(w, f.provs[f.A][0]) == f.A
                  && w.province_cessions.empty() && w.war_history.back().end == war_end::white_peace,
              "P3r a white peace changes no hands: every occupied goal's holder returns to its owner");
    }
    {
        // BL-1460 fix round. A unit of @p owner standing on @p tile.
        const auto put = [](world& w, entity_id owner, entity_id tile) {
            const entity_id id = w.create_entity();
            unit_component uc{}; uc.position = tile; uc.owner = owner; uc.count = 500; uc.establishment = 500;
            w.units[id] = uc;
            return id;
        };
        const auto in_prov = [](const world& w, entity_id unit, uint32_t p) {
            return w.provinces.province_of(w.units.at(unit).position) == p;
        };
        {
            // A PARTIAL CESSION RETURNS THE OTHER GOALS: A holds two of B's provinces
            // and B one of A's; the peace cedes one. The other two go back to their
            // owners and the occupiers' units there withdraw.
            fixture f; build(f);
            world& w = f.w;
            const uint32_t b0 = f.provs[f.B][0], b1 = f.provs[f.B][1], a0 = f.provs[f.A][0];
            (void)declare_war(w, sp, f.A, f.B, { b0, b1 });
            (void)answer_war(w, f.B, f.A, { a0 });
            w.province_holder[holder_index(w, b0)] = f.A;
            w.province_holder[holder_index(w, b1)] = f.A;
            w.province_holder[holder_index(w, a0)] = f.B;
            const entity_id ua = put(w, f.A, w.provinces.find(b1)->tiles[w.provinces.find(b1)->tiles.size() / 2]);
            const entity_id ub = put(w, f.B, w.provinces.find(a0)->tiles[w.provinces.find(a0)->tiles.size() / 2]);
            peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = { b0 };
            check(offer_peace(w, f.A, f.B, cede) == war_result::applied && accept_peace(w, sp, f.B, f.A) == war_result::applied
                      && province_owner(w, b0) == f.A && province_holder_for(w, b0) == f.A
                      && province_holder_for(w, b1) == f.B && province_holder_for(w, a0) == f.A
                      && !in_prov(w, ua, b1) && !in_prov(w, ub, a0),
                  "P3r2 a partial cession: the ceded goal passes, every other goal held against its owner by a "
                  "party returns, and the occupiers' units there withdraw (fix round)");
        }
        {
            // A WHITE PEACE LEAVES A THIRD NATION ALONE: C holds b1 from its own war
            // with B. A and B make white peace over b0 and b1; C's hold and its unit stand.
            fixture f; build(f);
            world& w = f.w;
            const uint32_t b0 = f.provs[f.B][0], b1 = f.provs[f.B][1];
            (void)declare_war(w, sp, f.A, f.B, { b0, b1 });
            (void)declare_war(w, sp, f.C, f.B, { b1 });
            w.province_holder[holder_index(w, b0)] = f.A;
            w.province_holder[holder_index(w, b1)] = f.C;
            const entity_id ua = put(w, f.A, w.provinces.find(b0)->tiles[w.provinces.find(b0)->tiles.size() / 2]);
            const entity_id uc = put(w, f.C, w.provinces.find(b1)->tiles[w.provinces.find(b1)->tiles.size() / 2]);
            const entity_id uc_at = w.units.at(uc).position;
            check(offer_peace(w, f.B, f.A, peace_terms{}) == war_result::applied
                      && accept_peace(w, sp, f.A, f.B) == war_result::applied
                      && province_holder_for(w, b0) == f.B && !in_prov(w, ua, b0)
                      && province_holder_for(w, b1) == f.C && w.units.at(uc).position == uc_at
                      && at_war(w, f.C, f.B),
                  "P3r3 a white peace returns only what THIS war's other party holds: a third nation's occupation "
                  "from its own war, and its unit, are untouched (fix round)");
        }
    }
    {
        // Impose at 100, and only at 100.
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> dg = { f.provs[f.B][0], f.provs[f.B][1] };
        (void)declare_war(w, sp, f.A, f.B, dg);
        peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = dg;
        w.province_holder[holder_index(w, dg[0])] = f.A;
        w.wars.at({ f.A, f.B }).defender_men_lost = 50;
        snap s = take(w);
        check(war_score(w, *war_between(w, f.A, f.B)) < 100
                  && impose_peace(w, sp, f.A, f.B, peace_terms{}) == war_result::rejected_state && same(s, w),
              "P3s below 100 the declarer cannot impose, mutating nothing");
        w.province_holder[holder_index(w, dg[1])] = f.A;
        check(impose_peace(w, sp, f.A, f.B, cede) == war_result::applied && !at_war(w, f.A, f.B)
                  && w.war_history.back().end == war_end::imposed_cession && w.war_history.back().final_score == 100
                  && w.province_cessions.size() == 2,
              "P3u at +100 the declarer imposes its cession without acceptance");
    }
    {
        // The DEFENDER imposes at -100 (NR-1103 made it reachable), and only there.
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> dg = { f.provs[f.B][0] };
        const std::vector<uint32_t> fg = { f.provs[f.A][0], f.provs[f.A][1] };
        (void)declare_war(w, sp, f.A, f.B, dg);
        (void)answer_war(w, f.B, f.A, fg);
        for (const uint32_t p : fg) w.province_holder[holder_index(w, p)] = f.B; // full occupation
        war_record& rec = w.wars.at({ f.A, f.B });
        rec.declarer_men_lost = 400; rec.defender_men_lost = 100; // battles favour B, not wholly
        peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.B; cede.provinces = fg;
        snap s = take(w);
        const int below = war_score(w, rec);
        check(below > -100 && below < 0
                  && impose_peace(w, sp, f.B, f.A, cede) == war_result::rejected_state && same(s, w),
              "P3t below 100 in its favour (" + std::to_string(below) + ") the DEFENDER cannot impose, mutating nothing");
        rec.defender_men_lost = 0; // no defender losses: the battles half is wholly B's
        s = take(w);
        check(war_score(w, rec) == -100
                  && impose_peace(w, sp, f.A, f.B, peace_terms{}) == war_result::rejected_state && same(s, w),
              "P3w at -100 the DECLARER cannot impose, mutating nothing");
        check(impose_peace(w, sp, f.B, f.A, cede) == war_result::applied && !at_war(w, f.A, f.B)
                  && w.war_history.back().end == war_end::imposed_cession && w.war_history.back().final_score == -100
                  && w.war_history.back().gainer == f.B && w.province_cessions.size() == 2
                  && w.province_cessions[0].from == f.A && w.province_cessions[0].to == f.B,
              "P3x at -100 the defender imposes its cession of the declarer's provinces without acceptance");
    }
    {
        // A last province MAY be ceded (Ben, 2026-11-09, overturning NR-1102).
        fixture f; build(f);
        world& w = f.w;
        // Give all but one of B's provinces to C on the territorial map, leaving B one.
        const std::vector<uint32_t> bp = f.provs[f.B];
        for (std::size_t i = 1; i < bp.size(); ++i)
            for (const entity_id t : w.provinces.find(bp[i])->tiles) w.set_tile_nation(t, f.C);
        (void)declare_war(w, sp, f.A, f.B, { bp[0] });
        w.province_holder[holder_index(w, bp[0])] = f.A;
        w.wars.at({ f.A, f.B }).defender_men_lost = 10;
        peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = { bp[0] };
        world offered = w;
        check(war_score(w, *war_between(w, f.A, f.B)) == 100
                  && check_peace_terms(w, *war_between(w, f.A, f.B), cede) == war_result::applied
                  && offer_peace(offered, f.A, f.B, cede) == war_result::applied,
              "P3v a cession of the loser's last province is LEGAL: the terms check and the offer applies (2026-11-09)");
        check(impose_peace(w, sp, f.A, f.B, cede) == war_result::applied && province_owner(w, bp[0]) == f.A
                  && nation_ended(w, f.B),
              "P3v' imposed, it moves the province and ends the loser");
    }

    {
        // BL-1460: with the ground moving at a cession, the LOST GROUND justification
        // holds through the real flow (war-record-and-declaration R2) and the
        // last-province floor counts across wars (war-goals-and-peace R3). Every
        // cession below is a real peace; no cession row is forged.
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> bp = f.provs[f.B];
        const std::vector<uint32_t> first(bp.begin(), bp.begin() + 3);
        const std::size_t keep = bp.size() - 3; // provinces B keeps after the first peace
        // War 1: A takes three of B's provinces.
        check(declare_war(w, sp, f.A, f.B, first) == war_result::applied, "P3y (A declares on three of B's provinces)");
        for (const uint32_t p : first) w.province_holder[holder_index(w, p)] = f.A;
        peace_terms cede1; cede1.kind = peace_kind::cession; cede1.gainer = f.A; cede1.provinces = first;
        check(offer_peace(w, f.A, f.B, cede1) == war_result::applied && accept_peace(w, sp, f.B, f.A) == war_result::applied,
              "P3y (B accepts the cession of three provinces)");
        bool moved = true;
        for (const uint32_t p : first) moved = moved && province_owner(w, p) == f.A;
        check(moved, "P3y a real cession moves the ceded provinces to the gainer on the territorial map");
        // Lost ground, through declare_war itself: B names a province it ceded, now A's.
        w.current_econ_tick += k_war_truce_ticks;
        const war_price pr = declaration_price(w, sp, f.B, f.A, { first[0] });
        check(pr.valid && pr.justifications[2] && pr.justified
                  && declare_war(w, sp, f.B, f.A, { first[0] }) == war_result::applied
                  && w.wars.count({ f.B, f.A }) == 1,
              "P3z LOST GROUND holds through the real flow: B declares on A naming a province it ceded, "
              "the preview shows the justification, and the declaration is accepted (it was refused while "
              "the ground stayed B's)");
        // Across wars: whittle B down to one province by successive real cessions to
        // other declarers; then the last one is ceded too, and B ends.
        const entity_id takers[4] = { f.C, f.D, f.E, f.F };
        std::size_t left = keep, taker = 0;
        bool whittled = true;
        while (left > 1 && taker < 64)
        {
            const entity_id t = takers[taker++ % 4];
            if (taker % 4 == 1) w.current_econ_tick += k_war_truce_ticks; // each pair's truce has run
            std::vector<uint32_t> goals;
            for (std::size_t i = 3; i < bp.size() && goals.size() < std::min<std::size_t>(3, left - 1); ++i)
                if (province_owner(w, bp[i]) == f.B) goals.push_back(bp[i]);
            whittled = whittled && declare_war(w, sp, t, f.B, goals) == war_result::applied;
            for (const uint32_t p : goals) w.province_holder[holder_index(w, p)] = t;
            peace_terms c; c.kind = peace_kind::cession; c.gainer = t; c.provinces = goals;
            whittled = whittled && offer_peace(w, t, f.B, c) == war_result::applied
                    && accept_peace(w, sp, f.B, t) == war_result::applied;
            left -= goals.size();
        }
        uint32_t last = 0;
        for (const uint32_t p : bp) if (province_owner(w, p) == f.B) last = p;
        check(whittled && left == 1 && last != 0, "P3v2 (B is down to one province after cessions in "
              + std::to_string(taker + 1) + " wars)");
        w.current_econ_tick += k_war_truce_ticks;
        const entity_id t = f.C;
        const bool declared = declare_war(w, sp, t, f.B, { last }) == war_result::applied;
        w.province_holder[holder_index(w, last)] = t;
        peace_terms c; c.kind = peace_kind::cession; c.gainer = t; c.provinces = { last };
        w.wars.at({ t, f.B }).defender_men_lost = 10; // score 100: impose is reachable
        check(declared && offer_peace(w, t, f.B, c) == war_result::applied
                  && accept_peace(w, sp, f.B, t) == war_result::applied
                  && province_owner(w, last) == t && nation_ended(w, f.B),
              "P3v2 ACROSS wars: after earlier peaces ceded the rest, a cession of B's last province is "
              "offered and accepted, and B ends (2026-11-09)");
    }

    // =====================================================================
    // BL-1562 (WAR_BOTH_HOSTILE_ELIMINATION; RELATIONS.md § 7, NATIONS.md § Elimination,
    // Ben 2026-11-09): a declaration makes both sides hostile, a last province may be
    // ceded, and the loser then ends.
    std::printf("\nW-E - both sides hostile at the declaration; a last province ends the nation\n");
    {
        fixture f; build(f);
        world& w = f.w;
        check(!front_push_allowed_unanswered(w, f.B, f.A),
              "E0 control: before any declaration B may not push toward A (no hostility)");
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        check(!war_between(w, f.A, f.B)->answered && front_push_allowed_unanswered(w, f.B, f.A)
                  && front_push_allowed_unanswered(w, f.A, f.B),
              "E0b declared on and UNANSWERED, B may push toward A through set_front_order at once");
        // A bare declare_hostile stays directed.
        check(declare_hostile(w, f.C, f.D) == stance_result::applied && is_hostile(w, f.C, f.D)
                  && !is_hostile(w, f.D, f.C),
              "E0c a bare declare_hostile, with no war, stays directed");
        // A peace clears both rows of an unanswered war.
        check(impose_peace(w, sp, f.A, f.B, peace_terms{}) == war_result::rejected_state, "E0d (no impose short of 100)");
        (void)offer_peace(w, f.A, f.B, peace_terms{});
        check(accept_peace(w, sp, f.B, f.A) == war_result::applied && !is_hostile(w, f.A, f.B)
                  && !is_hostile(w, f.B, f.A),
              "E0e a peace on an unanswered war clears both hostility rows");
    }
    {
        // The elimination, every effect checked. B keeps ONE province (the rest moved to C
        // on the territorial map); A takes it by an imposed cession.
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> bp = f.provs[f.B];
        for (std::size_t i = 1; i < bp.size(); ++i)
            for (const entity_id t : w.provinces.find(bp[i])->tiles) w.set_tile_nation(t, f.C);
        const uint32_t last = bp[0];
        check(declare_war(w, sp, f.A, f.B, { last }) == war_result::applied
                  && declare_war(w, sp, f.D, f.B, { last }) == war_result::applied
                  && declare_war(w, sp, f.B, f.E, { f.provs[f.E][0] }) == war_result::applied,
              "E1 (B is at war with A, D and E)");
        check(offer_peace(w, f.B, f.D, peace_terms{}) == war_result::applied, "E1 (B has a pending peace offer to D)");
        w.corp_friend_pairs.insert(canonical_pair(f.B, f.C));
        w.corp_friend_offers.insert({ f.B, f.F });
        w.corp_friend_offers.insert({ f.F, f.B });
        w.nation_embargoes.insert({ f.B, f.C });
        w.nation_embargoes.insert({ f.C, f.B });
        w.corp_hostile_pairs.insert({ f.F, f.B }); // a bare hostility toward it
        { law l; l.id = "LAW-B-TARIFF"; l.name = "B tariff"; l.effect = law_effect_kind::import_tariff;
          l.enacted = true; l.enacting_nation = f.B; l.rate = 0.1f; w.laws.push_back(l); }
        { law l; l.id = "LAW-C-TARIFF"; l.name = "C tariff"; l.effect = law_effect_kind::import_tariff;
          l.enacted = true; l.enacting_nation = f.C; l.rate = 0.1f; w.laws.push_back(l); }
        w.sentiment.pairs[{ f.C, f.B }] = sentiment_value{ 0.0f, -10.0f, 5.0f };
        w.sentiment.pairs[{ f.B, f.C }] = sentiment_value{ 0.0f, 10.0f, 0.0f };
        w.sentiment.pairs[{ f.C, f.D }] = sentiment_value{ 0.0f, 4.0f, 0.0f };
        const entity_id last_tile = w.provinces.find(last)->tiles.front();
        entity_id bu = w.create_entity();
        { unit_component uc{}; uc.position = last_tile; uc.owner = f.B; uc.count = 400; uc.establishment = 400; w.units[bu] = uc; }
        entity_id au = w.create_entity();
        { unit_component uc{}; uc.position = f.w.provinces.find(f.provs[f.A][0])->tiles.front(); uc.owner = f.A;
          uc.count = 400; uc.establishment = 400; w.units[au] = uc; }
        { convoy_component c{}; c.corp = f.B; c.id = w.allocate_convoy_id(); w.convoys.push_back(c); }
        { convoy_component c{}; c.corp = f.A; c.id = w.allocate_convoy_id(); w.convoys.push_back(c); }
        { standing_trade t{}; t.owner = f.B; t.id = w.allocate_trade_id(); w.trades.push_back(t); }
        { standing_trade t{}; t.owner = f.A; t.id = w.allocate_trade_id(); w.trades.push_back(t); }
        w.nations.at(f.B).treasury = 5000.0f;
        const float a_before = w.nations.at(f.A).treasury;
        w.province_holder[holder_index(w, last)] = f.A;
        w.wars.at({ f.A, f.B }).defender_men_lost = 10; // score 100
        const std::size_t hist_before = w.history_log.size();
        peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = { last };
        check(impose_peace(w, sp, f.A, f.B, cede) == war_result::applied, "E2 A imposes the cession of B's last province");

        bool no_ground = w.nations.at(f.B).tiles.empty();
        for (const auto& [t, o] : w.tile_to_nation) no_ground = no_ground && o != f.B;
        check(nation_ended(w, f.B) && no_ground && province_owner(w, last) == f.A
                  && w.nations.count(f.B) == 1 && w.nations.at(f.B).name == "Brael",
              "E3 B has ENDED: it owns no tile, its last province is A's, and it keeps its id and name");
        bool no_war = true;
        for (const auto& [k, war] : w.wars) no_war = no_war && k.first != f.B && k.second != f.B;
        int white = 0;
        for (const war_history_entry& h : w.war_history)
            if ((h.declarer == f.D && h.defender == f.B) || (h.declarer == f.B && h.defender == f.E))
                white += h.end == war_end::white_peace ? 1 : 0;
        bool no_truce = true;
        for (const auto& [k, e] : w.truces) no_truce = no_truce && k.first != f.B && k.second != f.B;
        check(no_war && white == 2 && no_truce,
              "E4 its other wars (with D, with E) close as WHITE PEACE, and no truce names it");
        bool rel_clear = true;
        for (const auto& k : w.corp_hostile_pairs) rel_clear = rel_clear && k.first != f.B && k.second != f.B;
        for (const auto& k : w.corp_friend_pairs)  rel_clear = rel_clear && k.first != f.B && k.second != f.B;
        for (const auto& k : w.corp_friend_offers) rel_clear = rel_clear && k.first != f.B && k.second != f.B;
        for (const auto& k : w.nation_embargoes)   rel_clear = rel_clear && k.first != f.B && k.second != f.B;
        for (const auto& [k, o] : w.peace_offers)  rel_clear = rel_clear && k.first != f.B && k.second != f.B;
        check(rel_clear, "E5 its pending peace and friendship offers drop; hostility, friendship and embargo rows to and from it are removed");
        bool laws_ok = w.laws.size() == 1 && w.laws[0].enacting_nation == f.C;
        check(laws_ok, "E5b the tariff it levied is removed; another nation's stays");
        bool sent_clear = w.sentiment.pairs.count({ f.C, f.D }) == 1;
        for (const auto& [k, v] : w.sentiment.pairs) sent_clear = sent_clear && k.first != f.B && k.second != f.B;
        for (const sentiment_event& e : w.sentiment_pending) sent_clear = sent_clear && e.observer != f.B && e.subject != f.B;
        check(sent_clear, "E5c every sentiment row to or from it, and conduct queued about it, is gone; others stay");
        bool force_gone = w.units.count(bu) == 0 && w.units.count(au) == 1;
        for (const auto& [uid, u] : w.units) force_gone = force_gone && u.owner != f.B;
        bool convoys_ok = w.convoys.size() == 1 && w.convoys[0].corp == f.A;
        bool trades_ok  = w.trades.size() == 1 && w.trades[0].owner == f.A;
        check(force_gone && convoys_ok && trades_ok,
              "E6 its units disband where they stand; its convoys and trades are cancelled; A's stand");
        check(w.nations.at(f.B).treasury == 0.0f && w.nations.at(f.A).treasury == a_before + 5000.0f,
              "E7 its positive treasury (5000) passes to the conqueror, exactly");
        {
            // Fix round (cold review): the inheritance is BOOKED. The conqueror's next
            // return lists it as a refund-style receipt and its net is its balance change.
            world wb = w;
            const std::size_t a_rows = wb.nations.at(f.A).returns.size();
            const std::size_t b_rows = wb.nations.at(f.B).returns.size();
            const nation_component& bn = wb.nations.at(f.B);
            const bool b_books_clear = bn.refund_unbooked == 0.0f && bn.research_unbooked == 0.0
                                    && bn.upkeep_unbooked == 0.0;
            apply_budget(wb, reg, {}, {});
            const nation_component& an = wb.nations.at(f.A);
            bool booked = an.returns.size() == a_rows + 1 && wb.nations.at(f.B).returns.size() == b_rows;
            if (booked)
            {
                const quarterly_return& r = an.returns.back();
                booked = r.refunds == 5000.0f
                      && r.net == static_cast<double>(an.treasury) - static_cast<double>(a_before)
                      && r.balance == an.treasury;
                std::printf("    E7b A's return: refunds %.1f net %.3f balance %.1f (was %.1f)\n",
                            r.refunds, r.net, r.balance, a_before);
            }
            check(booked && b_books_clear,
                  "E7b the inheritance is BOOKED: A's next return carries it (refunds 5000) and its net "
                  "equals its balance less its previous balance; B's unbooked amounts are zero and it files no return");
        }
        // A's stub convoy and trade were controls for E6 only: they name no market, which
        // the loader (rightly) refuses, so they go before the save rows below.
        w.convoys.clear();
        w.trades.clear();
        int ended_entries = 0;
        for (std::size_t i = hist_before; i < w.history_log.size(); ++i)
            if (w.history_log[i].corp == f.B && w.history_log[i].event.find("has ended") != std::string::npos)
                ++ended_entries;
        check(ended_entries == 1, "E8 the history log writes ONE entry for its end");
        const snap s = take(w);
        corp_command act = war_cmd(corp_verb::declare_war, f.B, f.C, { f.provs[f.C][0] });
        check(apply_corp_command(w, reg, act) == corp_command_result::rejected_no_corp
                  && declare_war(w, sp, f.C, f.B, { last }) != war_result::applied
                  && declare_hostile(w, f.C, f.B) != stance_result::applied
                  && offer_friendship(w, f.C, f.B) != stance_result::applied
                  && set_embargo(w, f.C, f.B, true) != embargo_result::applied
                  && same(s, w),
              "E9 an ended nation is no actor (rejected_no_corp) and no counterparty: every verb refuses, mutating nothing");
        w.settle_build_gate = false;
        score_nation_war(w, reg, f.B, 40, true);
        check(same(s, w), "E10 the war scorer skips it, even under spectate");
        const war_price pr = declaration_price(w, sp, f.A, f.C, { f.provs[f.C][0] });
        bool watches = false;
        for (const war_price_row& r : pr.rows) watches = watches || r.observer == f.B;
        check(pr.valid && !watches, "E10b an ended nation prices no declaration (no row observes for it)");
        queue_conduct(w, f.C, f.B, sentiment_factor_kind::war_declared, 1.0f);
        check(same(s, w), "E10c conduct about an ended nation is never gathered");

        // Save / load: the flag travels and hashes.
        world back;
        check(from_bytes(to_bytes(w), back) && back.nations.at(f.B).ended && !back.nations.at(f.A).ended
                  && back.state_hash(0) == w.state_hash(0) && to_bytes(back) == to_bytes(w),
              "E11 save/load round-trips the ended flag byte-for-byte, state_hash equal");
        world cleared = w;
        cleared.nations.at(f.B).ended = false;
        check(cleared.state_hash(0) != w.state_hash(0), "E11b state_hash moves with the ended flag");
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_ended(w, o);
            const std::string v80 = o.str();
            world old;
            std::istringstream in(v80, std::ios::binary);
            check(read_world_snapshot(old, in) && !old.nations.at(f.B).ended
                      && to_bytes(w).size() == v80.size() + world_save_ended_section_bytes(w)
                      && cleared.state_hash(0) == old.state_hash(0),
                  "E12 a v82 stream (before the flag) loads with every nation not ended, and is the v83 stream less the section");
        }
        {
            world bad = w;
            bad.nations.at(f.C).ended = true; // a nation with ground marked ended
            world back2;
            check(!from_bytes(to_bytes(bad), back2) && back2.nations.empty(),
                  "E13 a stream marking a nation with ground ended is refused whole on load");
        }
        {
            // Fix round (cold review): an ended nation that keeps anything elimination clears
            // is refused whole, the destination untouched. Each corruption is applied to the
            // honest ended world, one at a time.
            // (A stub trade, convoy, wing, muster, front order or space asset is refused by its
            // own table's validator, so it cannot show this check; the loader holds those too.)
            const entity_id B = f.B, C = f.C;
            const entity_id some_tile = w.provinces.find(f.provs[f.A][0])->tiles.front();
            const entity_id a_building = w.nations.at(f.A).state_assets.empty()
                                       ? null_entity : w.nations.at(f.A).state_assets.front();
            struct corrupt { const char* what; std::function<void(world&)> apply; };
            std::vector<corrupt> rows = {
                { "hostility row",   [&](world& x) { x.corp_hostile_pairs.insert({ C, B }); } },
                { "friendship row",  [&](world& x) { x.corp_friend_pairs.insert(canonical_pair(B, C)); } },
                { "friend offer",    [&](world& x) { x.corp_friend_offers.insert({ C, B }); } },
                { "embargo row",     [&](world& x) { x.nation_embargoes.insert({ C, B }); } },
                { "sentiment row",   [&](world& x) { x.sentiment.pairs[{ C, B }] = sentiment_value{ 0.0f, 1.0f, 0.0f }; } },
                { "law",             [&](world& x) { law l; l.id = "LAW-X"; l.name = "x"; l.effect = law_effect_kind::import_tariff;
                                                     l.enacted = true; l.enacting_nation = B; l.rate = 0.1f; x.laws.push_back(l); } },
                { "positive treasury", [&](world& x) { x.nations.at(B).treasury = 1.0f; } },
                { "live capital",    [&](world& x) { x.nations.at(B).capital_tile = some_tile; } },
            };
            if (a_building != null_entity)
                rows.push_back({ "building", [&](world& x) { x.nations.at(B).state_assets.push_back(a_building);
                                                             auto& v = x.nations.at(f.A).state_assets; v.erase(v.begin()); } });
            for (const corrupt& c : rows)
            {
                world bad = w;
                c.apply(bad);
                world dest = w; // a live destination: a refusal must leave it exactly as it was
                const std::string before = to_bytes(dest);
                const bool refused = !from_bytes(to_bytes(bad), dest);
                check(refused && to_bytes(dest) == before,
                      std::string("E13b a stream whose ended nation keeps a ") + c.what
                          + " is refused whole, the destination untouched");
            }
        }
    }
    {
        // An old save's unanswered war (one row) loads with both rows.
        fixture f; build(f);
        world& w = f.w;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        w.corp_hostile_pairs.erase({ f.B, f.A }); // what a pre-ruling declaration wrote
        std::ostringstream o(std::ios::binary);
        write_world_snapshot_pre_ended(w, o);
        world old;
        std::istringstream in(o.str(), std::ios::binary);
        check(read_world_snapshot(old, in) && is_hostile(old, f.A, f.B) && is_hostile(old, f.B, f.A)
                  && !old.wars.at({ f.A, f.B }).answered,
              "E14 an old save's unanswered war (one hostility row) loads with both rows, still unanswered");
        // Fix round (cold review): the upgrade is the OLD format's only. A current stream
        // missing the defender's row is refused whole.
        world dest;
        check(!from_bytes(to_bytes(w), dest) && dest.nations.empty(),
              "E14b a current-format stream with an unanswered war missing the defender's row is refused");
        // ... and the upgrade writes the row as write_hostility does: a standing friendship
        // offer between the pair drops.
        w.corp_friend_offers.insert({ f.B, f.A });
        std::ostringstream o2(std::ios::binary);
        write_world_snapshot_pre_ended(w, o2);
        world old2;
        std::istringstream in2(o2.str(), std::ios::binary);
        const bool loaded2 = read_world_snapshot(old2, in2);
        check(loaded2 && is_hostile(old2, f.B, f.A) && old2.corp_friend_offers.count({ f.B, f.A }) == 0,
              "E14c the old-format upgrade drops a standing friendship offer between the war's parties");
    }
    {
        // Fix round (cold review): a mixed province, and a third party at peace with the
        // conqueror. B keeps one province (bp[0]) and ONE tile inside bp[1], which is C's by
        // plurality. D, at war with B, has a unit on B's last province; F, at war with A,
        // has one there too.
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> bp = f.provs[f.B];
        for (std::size_t i = 1; i < bp.size(); ++i)
            for (const entity_id t : w.provinces.find(bp[i])->tiles) w.set_tile_nation(t, f.C);
        const entity_id stray = w.provinces.find(bp[1])->tiles.front();
        w.set_tile_nation(stray, f.B);
        const uint32_t last = bp[0];
        const entity_id holder1 = province_holder_for(w, bp[1]);
        check(province_owner(w, bp[1]) == f.C && w.tile_to_nation.at(stray) == f.B,
              "E17 (bp[1] is C's by plurality, with one tile still B's)");
        check(declare_war(w, sp, f.A, f.B, { last }) == war_result::applied
                  && declare_war(w, sp, f.D, f.B, { last }) == war_result::applied
                  && declare_war(w, sp, f.F, f.A, { f.provs[f.A][0] }) == war_result::applied,
              "E17 (A and D at war with B; F at war with A)");
        const std::vector<entity_id>& lt = w.provinces.find(last)->tiles;
        const entity_id du = w.create_entity();
        { unit_component uc{}; uc.position = lt.back(); uc.owner = f.D; uc.count = 300; uc.establishment = 300; w.units[du] = uc; }
        const entity_id fu = w.create_entity();
        { unit_component uc{}; uc.position = lt.back(); uc.owner = f.F; uc.count = 300; uc.establishment = 300; w.units[fu] = uc; }
        w.province_holder[holder_index(w, last)] = f.A;
        w.wars.at({ f.A, f.B }).defender_men_lost = 10;
        const std::size_t cessions_before = w.province_cessions.size();
        peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = { last };
        check(impose_peace(w, sp, f.A, f.B, cede) == war_result::applied && nation_ended(w, f.B),
              "E17 (A imposes the cession of B's last province; B ends)");
        bool no_record = true;
        for (std::size_t i = cessions_before; i < w.province_cessions.size(); ++i)
            no_record = no_record && w.province_cessions[i].province != bp[1];
        check(w.tile_to_nation.at(stray) == f.A && province_owner(w, bp[1]) == f.C
                  && province_holder_for(w, bp[1]) == holder1 && no_record,
              "E17 a tile B owned inside C's province passes ALONE: the tile is A's, the province stays C's, "
              "its holder is unwritten and no cession is recorded");
        const auto in_last = [&](entity_id uid) {
            return w.units.count(uid) != 0 && w.provinces.province_of(w.units.at(uid).position) == last;
        };
        check(w.units.count(du) == 1 && !in_last(du),
              "E18 D's unit (at war with B, at peace with A) withdraws off A's new ground");
        check(in_last(fu),
              "E18b control: F's unit, at war with A, stays where it stands");
    }
    {
        // A debt is written off, not inherited.
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> bp = f.provs[f.B];
        for (std::size_t i = 1; i < bp.size(); ++i)
            for (const entity_id t : w.provinces.find(bp[i])->tiles) w.set_tile_nation(t, f.C);
        (void)declare_war(w, sp, f.A, f.B, { bp[0] });
        w.province_holder[holder_index(w, bp[0])] = f.A;
        w.nations.at(f.B).treasury = -300.0f;
        const float a_before = w.nations.at(f.A).treasury;
        peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = { bp[0] };
        (void)offer_peace(w, f.A, f.B, cede);
        check(accept_peace(w, sp, f.B, f.A) == war_result::applied && nation_ended(w, f.B)
                  && w.nations.at(f.B).treasury == 0.0f && w.nations.at(f.A).treasury == a_before,
              "E15 a debt (-300) is written off at the end: the conqueror's treasury is untouched");
        // A cession that leaves the loser ground does NOT end it.
        fixture g; build(g);
        (void)declare_war(g.w, sp, g.A, g.B, { g.provs[g.B][0] });
        g.w.province_holder[holder_index(g.w, g.provs[g.B][0])] = g.A;
        peace_terms c2; c2.kind = peace_kind::cession; c2.gainer = g.A; c2.provinces = { g.provs[g.B][0] };
        (void)offer_peace(g.w, g.A, g.B, c2);
        check(accept_peace(g.w, sp, g.B, g.A) == war_result::applied && !nation_ended(g.w, g.B)
                  && !g.w.nations.at(g.B).tiles.empty(),
              "E16 control: a cession that leaves the loser a province does not end it");
    }

    // =====================================================================
    std::printf("\nP-R4 - saved, and through the seam\n");
    {
        fixture f; build(f);
        world& w = f.w;
        const std::vector<uint32_t> dg = { f.provs[f.B][0] };
        (void)declare_war(w, sp, f.A, f.B, dg);
        w.province_holder[holder_index(w, dg[0])] = f.A;
        w.wars.at({ f.A, f.B }).declarer_exhaustion = 17.5f;
        w.wars.at({ f.A, f.B }).defender_men_lost = 40;
        const snap s = take(w);
        corp_command bad = war_cmd(corp_verb::offer_peace, f.A, f.B, { dg[0] }, 3);
        check(apply_corp_command(w, reg, bad) == corp_command_result::rejected_invalid && same(s, w),
              "R4a terms byte 3 is refused whole, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::offer_peace, f.A, f.B, { dg[0] }, 0))
                      == corp_command_result::rejected_invalid && same(s, w),
              "R4b white peace naming a province is refused whole, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::offer_peace, f.A, f.B, {}, 1))
                      == corp_command_result::rejected_invalid && same(s, w),
              "R4c a cession naming nothing is refused whole, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::accept_peace, f.B, f.A, {}))
                      == corp_command_result::rejected_state && same(s, w),
              "R4d accepting an offer nobody made is refused, mutating nothing");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::offer_peace, f.A, f.B, { dg[0] }, 1))
                      == corp_command_result::applied && w.peace_offers.size() == 1,
              "R4e a cession offer applies through the seam");
        w.truces[canonical_pair(f.C, f.D)] = 30;
        w.war_exhaustion_residual[f.D] = 9.0f;
        const std::string b = to_bytes(w);
        world back;
        check(from_bytes(b, back) && to_bytes(back) == b && back.state_hash(0) == w.state_hash(0)
                  && back.peace_offers.size() == 1 && back.wars.at({ f.A, f.B }).declarer_exhaustion == 17.5f
                  && back.wars.at({ f.A, f.B }).defender_men_lost == 40
                  && war_score(back, back.wars.at({ f.A, f.B })) == war_score(w, w.wars.at({ f.A, f.B })),
              "R4f offers, truces, the score's inputs and exhaustion round-trip the snapshot");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::refuse_peace, f.B, f.A, {})) == corp_command_result::applied
                  && w.peace_offers.empty(),
              "R4g refuse_peace applies through the seam");
        w.wars.at({ f.A, f.B }).declarer_men_lost = 40; // battles even: the score is 50
        check(apply_corp_command(w, reg, war_cmd(corp_verb::impose_peace, f.A, f.B, {}, 0))
                      == corp_command_result::rejected_state,
              "R4h impose_peace short of 100 is refused through the seam");
        check(apply_corp_command(w, reg, war_cmd(corp_verb::offer_peace, f.B, f.A, {}, 0)) == corp_command_result::applied
                  && apply_corp_command(w, reg, war_cmd(corp_verb::accept_peace, f.A, f.B, {})) == corp_command_result::applied
                  && !at_war(w, f.A, f.B) && w.war_history.size() == 1,
              "R4i a white offer and its acceptance conclude the peace through the seam");
        // A corrupt offer (terms byte 7) is refused on load.
        (void)declare_war(w, sp, f.C, f.E, { f.provs[f.E][0] });
        (void)offer_peace(w, f.C, f.E, peace_terms{});
        const std::string b2 = to_bytes(w);
        std::string bad2 = b2;
        // The offer sits after the war count + one war record + the truce table.
        const war_record& wr = w.wars.begin()->second;
        const std::size_t off = b2.size() - world_save_embargo_section_bytes(w) - world_save_gen_capital_section_bytes(w) - world_save_recruits_section_bytes(w) - world_save_sea_recruits_section_bytes(w) - world_save_refusals_section_bytes(w) /* BL-1480 (v75) */ - tail_after_refusals(w) /* v77..v83 */ - world_save_war_section_bytes(w) + 4
                              + (4 + 4 + 1 + 4 + 4 + 8 + 8 + 4 + 4 + 4 + 4 + 4 + 4 * wr.declarer_goals.size() + 4 + 4 * wr.defender_goals.size())
                              + 4 + 12 * w.truces.size() + 4 + 4 + 4 + 4 + 4; // to the kind byte
        bad2[off] = 7;
        world untouched;
        check(w.wars.size() == 1 && !from_bytes(bad2, untouched) && untouched.nations.empty(),
              "R4j a corrupt peace offer (kind byte 7) is refused whole on load");
    }

    // =====================================================================
    std::printf("\nF - the cold-review fixes\n");
    {
        // F1: a peace ends every live battle between the parties, with no holder write.
        fixture f; build(f);
        world& w = f.w;
        const uint32_t goal = f.provs[f.B][0];
        (void)declare_war(w, sp, f.A, f.B, { goal });
        (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
        const province& gp = *w.provinces.find(goal);
        entity_id t = null_entity, nb = null_entity;
        for (const entity_id c : gp.tiles)
        {
            const tile_component& tc = w.tiles.at(c);
            if (tc.grid_x > 0 && tc.grid_x < GW - 1)
            {
                t = c;
                nb = f.at(tc.grid_x - 1, tc.grid_y);
                if (nb == null_entity || w.provinces.province_of(nb) == 0) nb = f.at(tc.grid_x + 1, tc.grid_y);
                break;
            }
        }
        const auto unit = [&](entity_id owner, entity_id tile, int men) {
            const entity_id u = w.create_entity();
            unit_component uc{}; uc.position = tile; uc.owner = owner; uc.count = men; uc.establishment = men;
            w.units[u] = uc;
            return u;
        };
        const entity_id ua = unit(f.A, nb, 3000);
        (void)unit(f.B, t, 1); // one man: the battle, left running, would end it (F1e)
        w.province_holder[holder_index(w, goal)] = f.A; // A occupies the goal
        const bool opened = open_tile_battle(w, 0, t, f.A, f.B, { ua });
        check(opened && w.battles.size() == 1, "F1a setup: a live battle between the parties in the goal province");
        check(offer_peace(w, f.B, f.A, peace_terms{}) == war_result::applied
                  && accept_peace(w, sp, f.A, f.B) == war_result::applied,
              "F1b a white peace is concluded during the battle");
        check(w.battles.empty() && province_holder_for(w, goal) == f.B,
              "F1c the peace ends the battle (closed as drawn) and the goal's holder is returned to its owner");
        for (int day = 1; day <= 30; ++day)
            (void)run_military_day(w, reg, day);
        check(w.battles.empty() && province_holder_for(w, goal) == f.B,
              "F1d thirty days later no battle has run and the holder stays returned");
        // The counterfactual, so F1d can fail: the same battle left running after the
        // holder is handed back DOES write the holder within the thirty days.
        fixture g; build(g);
        (void)declare_war(g.w, sp, g.A, g.B, { goal });
        const entity_id ga = g.w.create_entity();
        { unit_component uc{}; uc.position = nb; uc.owner = g.A; uc.count = 3000; uc.establishment = 3000; g.w.units[ga] = uc; }
        { const entity_id gb = g.w.create_entity(); unit_component uc{}; uc.position = t; uc.owner = g.B; uc.count = 1; uc.establishment = 1; g.w.units[gb] = uc; }
        (void)open_tile_battle(g.w, 0, t, g.A, g.B, { ga });
        g.w.province_holder[holder_index(g.w, goal)] = g.B;
        for (int day = 1; day <= 30; ++day)
            (void)run_military_day(g.w, reg, day);
        std::printf("    [F1e] battles=%zu holder=%s units=%zu rounds=%d\n", g.w.battles.size(),
                    province_holder_for(g.w, goal) == g.A ? "A" : "B", g.w.units.size(),
                    g.w.battles.empty() ? -1 : g.w.battles.front().state.rounds_fought);
        check(province_holder_for(g.w, goal) == g.A,
              "F1e (counterfactual) a battle left running moves the holder within thirty days");
    }
    {
        // F2: a truce refuses a bare hostility too, either way, mutating nothing.
        fixture f; build(f);
        world& w = f.w;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        (void)offer_peace(w, f.A, f.B, peace_terms{});
        (void)accept_peace(w, sp, f.B, f.A);
        const snap s = take(w);
        corp_command dh; dh.verb = corp_verb::declare_hostile;
        dh.corp = f.A; dh.as_nation = f.A; dh.counterparty = f.B;
        const corp_command_result r1 = apply_corp_command(w, reg, dh);
        dh.corp = f.B; dh.as_nation = f.B; dh.counterparty = f.A;
        const corp_command_result r2 = apply_corp_command(w, reg, dh);
        check(truce_between(w, f.A, f.B) && r1 == corp_command_result::rejected_state
                  && r2 == corp_command_result::rejected_state && same(s, w),
              "F2a under a truce declare_hostile is refused either way, mutating nothing");
        w.current_econ_tick += k_war_truce_ticks;
        dh.corp = f.A; dh.as_nation = f.A; dh.counterparty = f.B;
        check(apply_corp_command(w, reg, dh) == corp_command_result::applied, "F2b once the truce expires it applies");
    }
    {
        // F3: field days accumulate daily.
        fixture f; build(f);
        world& w = f.w;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        const entity_id abroad = w.provinces.find(f.provs[f.B][2])->tiles.front();
        const entity_id home   = w.provinces.find(f.provs[f.A][1])->tiles.front();
        const entity_id u = w.create_entity();
        unit_component uc{}; uc.position = abroad; uc.owner = f.A; uc.count = 500; uc.establishment = 500;
        w.units[u] = uc;
        for (int day = 0; day < 89; ++day) accumulate_war_field_days(w, 1);
        w.units.at(u).position = home; // steps home on day 90
        accumulate_war_field_days(w, 1);
        run_war_quarter(w);
        const float e = w.wars.at({ f.A, f.B }).declarer_exhaustion;
        check(std::fabs(e - 8.9f) < 1e-3f && w.wars.at({ f.A, f.B }).declarer_field_days == 0.0f,
              "F3a 89 days in the field then a step home charges 8.9 (" + std::to_string(e) + "), and the accumulator resets");
        // The days the walk skips (nothing military moving) count at today's share.
        w.units.at(u).position = abroad;
        w.construction_quarter_day = 0;
        const int walked = advance_quarter_to(w, reg, 90, 0);
        check(walked == 90 && std::fabs(w.wars.at({ f.A, f.B }).declarer_field_days - 90.0f) < 1e-3f,
              "F3b a quarter the military pass skips still counts every one of its days (90 field days)");
    }
    {
        // F4: state_hash folds the cessions, the history, opened_day and the offer.
        fixture f; build(f);
        world& w = f.w;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        const uint64_t h0 = w.state_hash(0);
        w.wars.at({ f.A, f.B }).opened_day += 1;
        const uint64_t h1 = w.state_hash(0);
        w.province_cessions.push_back(province_cession{ f.C, f.D, f.provs[f.C][0], 0 });
        const uint64_t h2 = w.state_hash(0);
        w.war_history.push_back(war_history_entry{});
        w.war_history.back().declarer = f.C; w.war_history.back().defender = f.D;
        const uint64_t h3 = w.state_hash(0);
        (void)offer_peace(w, f.A, f.B, peace_terms{});
        const uint64_t h4 = w.state_hash(0);
        w.peace_offers.begin()->second.offered_tick += 1;
        const uint64_t h5 = w.state_hash(0);
        check(h0 != h1 && h1 != h2 && h2 != h3 && h3 != h4 && h4 != h5,
              "F4 state_hash moves with opened_day, a cession, a history entry, an offer and its tick");
    }
    {
        // F5: the loader refuses a war whose hostility rows are missing.
        fixture f; build(f);
        world& w = f.w;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        world a = w;
        a.corp_hostile_pairs.erase({ f.A, f.B });
        world back;
        check(!from_bytes(to_bytes(a), back) && back.nations.empty(),
              "F5a a war with no declarer->defender hostility row is refused whole on load");
        (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
        world b = w;
        b.corp_hostile_pairs.erase({ f.B, f.A });
        world back2;
        check(!from_bytes(to_bytes(b), back2) && back2.nations.empty(),
              "F5b an answered war with no defender->declarer row is refused whole on load");
        world back3;
        check(from_bytes(to_bytes(w), back3), "F5c the intact stream loads");
    }

    // =====================================================================
    // BL-1461 (rivals declare war; requirement group `rivals-declare-war` R3): the
    // rival war pass acts only through the legal verbs, and never for the player.
    std::printf("\nRW - a rival wages war through the verbs (BL-1461)\n");
    {
        // The rows read what the pass DID through the trace it writes and the tables
        // the verbs write; nothing here calls a war verb on the rival's behalf.
        std::vector<war_ai_trace_row> trace;
        war_ai_trace_sink() = &trace;
        const auto did = [&trace](war_ai_choice c, entity_id actor, entity_id target) {
            for (const war_ai_trace_row& t : trace)
                if (t.choice == c && t.nation == actor && t.target == target && t.applied) return true;
            return false;
        };
        const auto front_tile = [](const world& w, entity_id owner, entity_id toward) {
            for (const front& fr : derive_fronts(w, owner))
                if (fr.neighbour == toward && !fr.span.empty()) return fr.span.front();
            return null_entity;
        };
        const auto add_unit = [](world& w, entity_id owner, entity_id tile, int men) {
            const entity_id u = w.create_entity();
            unit_component uc{}; uc.position = tile; uc.owner = owner; uc.count = men; uc.establishment = men;
            w.units[u] = uc;
        };
        const auto grudge = [&sp](world& w, entity_id observer, entity_id subject, float magnitude) {
            apply_sentiment_events(w.sentiment, sp,
                                   { sentiment_event{ observer, subject, sentiment_factor_kind::hostility_declared,
                                                      magnitude } });
        };

        // RW1 - the defender answers, naming goals among the declarer's provinces.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 20;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            trace.clear();
            score_nation_war(w, reg, f.B, 20);
            const war_record* rec = war_between(w, f.A, f.B);
            bool goals_ok = rec != nullptr && !rec->defender_goals.empty() && rec->defender_goals.size() <= 3;
            if (rec != nullptr)
                for (const uint32_t g : rec->defender_goals) goals_ok = goals_ok && province_owner(w, g) == f.A;
            check(rec != nullptr && rec->answered && is_hostile(w, f.B, f.A) && goals_ok
                      && did(war_ai_choice::answer, f.B, f.A),
                  "RW1 a rival defender answers through answer_war: its hostility row, 1..3 goals among the declarer's provinces");
        }
        // RW2 - the player's nation is never acted for: not answered, not offered, not declared.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 20;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            w.player_entity = f.B;
            grudge(w, f.B, f.C, 2.0f);
            add_unit(w, f.B, front_tile(w, f.B, f.C), 2000);
            trace.clear();
            const snap s = take(w);
            score_nation_war(w, reg, f.B, 20);
            check(same(s, w) && trace.empty(),
                  "RW2 the player's nation is never acted for: no answer, no declaration, nothing mutated");
        }
        // RW3 - impose at 100 through impose_peace, its held goals ceded to it.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 20;
            const std::vector<uint32_t> dg = { f.provs[f.B][0], f.provs[f.B][1] };
            (void)declare_war(w, sp, f.A, f.B, dg);
            for (const uint32_t p : dg) w.province_holder[holder_index(w, p)] = f.A;
            w.wars.at({ f.A, f.B }).defender_men_lost = 50;
            check(war_score(w, *war_between(w, f.A, f.B)) == 100, "RW3 (setup) the declarer's score stands at 100");
            trace.clear();
            score_nation_war(w, reg, f.A, 20);
            check(!at_war(w, f.A, f.B) && !w.war_history.empty()
                      && w.war_history.back().end == war_end::imposed_cession && w.war_history.back().gainer == f.A
                      && w.war_history.back().ceded == dg && did(war_ai_choice::impose, f.A, f.B),
                  "RW3 at 100 a rival imposes through impose_peace: its held goals are ceded to it");
        }
        // RW3b - a rival MAY impose on the player's nation at 100 (Ben, 2026-11-03,
        // NR-1119: "only the player answers and ends" means no rival acts FOR it; the
        // player is not immune).
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 10;
            const std::vector<uint32_t> dg = { f.provs[f.B][0], f.provs[f.B][1] };
            (void)declare_war(w, sp, f.A, f.B, dg);
            for (const uint32_t p : dg) w.province_holder[holder_index(w, p)] = f.A;
            w.wars.at({ f.A, f.B }).defender_men_lost = 50;
            w.player_entity = f.B;
            w.current_econ_tick = 20;
            trace.clear();
            score_nation_war(w, reg, f.A, 20);
            check(!at_war(w, f.A, f.B) && !w.war_history.empty()
                      && w.war_history.back().end == war_end::imposed_cession && w.war_history.back().gainer == f.A
                      && w.war_history.back().ceded == dg && did(war_ai_choice::impose, f.A, f.B)
                      && !did(war_ai_choice::offer, f.A, f.B),
                  "RW3b at 100 against the player's nation a rival imposes through impose_peace, its held goals ceded (NR-1119)");
        }
        // RW4 - accept: a weary rival accepts the other side's offer through accept_peace.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 10;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
            w.current_econ_tick = 20;
            w.wars.at({ f.A, f.B }).declarer_exhaustion = 80.0f;
            (void)offer_peace(w, f.B, f.A, peace_terms{});
            trace.clear();
            score_nation_war(w, reg, f.A, 20);
            check(!at_war(w, f.A, f.B) && !w.war_history.empty() && w.war_history.back().end == war_end::white_peace
                      && did(war_ai_choice::accept, f.A, f.B),
                  "RW4 a weary rival (exhaustion 80) accepts the white peace offered it, through accept_peace");
        }
        // RW5 - refuse: a fresh rival refuses through refuse_peace; the war goes on.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 16;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
            w.current_econ_tick = 20;
            (void)offer_peace(w, f.B, f.A, peace_terms{});
            trace.clear();
            score_nation_war(w, reg, f.A, 20);
            check(at_war(w, f.A, f.B) && w.peace_offers.empty() && did(war_ai_choice::refuse, f.A, f.B),
                  "RW5 a fresh rival (no exhaustion, level score, four ticks in) refuses through refuse_peace; the war goes on");
        }
        // RW6 - offer: a weary rival offers peace through offer_peace.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 10;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
            w.current_econ_tick = 18;
            w.wars.at({ f.A, f.B }).defender_exhaustion = 60.0f;
            trace.clear();
            score_nation_war(w, reg, f.B, 18);
            const auto oit = w.peace_offers.find({ f.A, f.B });
            check(at_war(w, f.A, f.B) && oit != w.peace_offers.end() && oit->second.offerer == f.B
                      && oit->second.terms.kind == peace_kind::white && did(war_ai_choice::offer, f.B, f.A),
                  "RW6 a weary rival (exhaustion 60, eight ticks in) offers a white peace through offer_peace");
        }
        // RW7 - a declaration: a motive, an edge on the front, through declare_war only.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 20;
            grudge(w, f.A, f.B, 2.0f);
            add_unit(w, f.A, front_tile(w, f.A, f.B), 2000);
            {
                world no_edge = w;
                for (auto& [uid, u] : no_edge.units) u.count = 0; // no force: no edge on the front
                trace.clear();
                const snap s = take(no_edge);
                score_nation_war(no_edge, reg, f.A, 20);
                check(same(s, no_edge), "RW7a with the motive but no edge on the front, no declaration");
            }
            {
                world no_motive = w;
                no_motive.sentiment = sentiment_table{};
                trace.clear();
                const snap s = take(no_motive);
                score_nation_war(no_motive, reg, f.A, 20);
                check(same(s, no_motive), "RW7b with the edge but no motive, no declaration (never on one threshold)");
            }
            trace.clear();
            score_nation_war(w, reg, f.A, 20);
            const war_record* rec = war_between(w, f.A, f.B);
            bool goals_ok = rec != nullptr && rec->declarer == f.A && !rec->declarer_goals.empty()
                         && rec->declarer_goals.size() <= 3;
            if (rec != nullptr)
                for (const uint32_t g : rec->declarer_goals) goals_ok = goals_ok && province_owner(w, g) == f.B;
            check(goals_ok && is_hostile(w, f.A, f.B) && is_hostile(w, f.B, f.A) // both rows (2026-11-09)
                      && did(war_ai_choice::declare, f.A, f.B) && w.wars.size() == 1,
                  "RW7 with a motive and an edge a rival declares through declare_war: 1..3 goals among the target's provinces");
            trace.clear();
            const snap s = take(w);
            score_nation_war(w, reg, f.A, 21);
            check(same(s, w) && !did(war_ai_choice::declare, f.A, f.B),
                  "RW7c a second evaluation does not declare again (one war per pair; the open war weighs)");
        }
        // RW8 - the settle gate: no war verb while the settle's build gate is up.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 5;
            grudge(w, f.A, f.B, 2.0f);
            add_unit(w, f.A, front_tile(w, f.A, f.B), 2000);
            w.settle_build_gate = true;
            trace.clear();
            const snap s = take(w);
            score_nation_war(w, reg, f.A, 5);
            check(same(s, w) && trace.empty(), "RW8 under the settle gate the pass does nothing (no war begins in the settle)");
        }
        // RW9 - replay: the same pass on two built worlds gives byte-identical worlds and traces.
        {
            std::vector<std::string> bytes;
            std::vector<std::size_t> rows;
            for (int run = 0; run < 2; ++run)
            {
                fixture f; build(f);
                world& w = f.w;
                w.current_econ_tick = 20;
                grudge(w, f.A, f.B, 2.0f);
                grudge(w, f.C, f.D, 1.0f);
                add_unit(w, f.A, front_tile(w, f.A, f.B), 2000);
                add_unit(w, f.C, front_tile(w, f.C, f.D), 1500);
                trace.clear();
                for (int t = 20; t < 40; ++t)
                {
                    w.current_econ_tick = t;
                    for (const entity_id n : { f.A, f.B, f.C, f.D, f.E, f.F })
                        score_nation_war(w, reg, n, t);
                }
                bytes.push_back(to_bytes(w));
                rows.push_back(trace.size());
            }
            check(bytes[0] == bytes[1] && rows[0] == rows[1] && rows[0] > 0,
                  "RW9 the pass replays: two built worlds, twenty ticks of every nation's pass, byte-identical");
        }
        // RW10 - a STALE offer is refused, so the war does not freeze behind it, and a
        // new offer can follow (cold review, 2026-10-31). Twice: from a rival, and from
        // the player's nation.
        for (const bool from_player : { false, true })
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 10;
            const std::vector<uint32_t> dg = { f.provs[f.B][0] };
            (void)declare_war(w, sp, f.A, f.B, dg);
            (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
            w.province_holder[holder_index(w, dg[0])] = f.A;
            peace_terms cede; cede.kind = peace_kind::cession; cede.gainer = f.A; cede.provinces = dg;
            const bool filed = offer_peace(w, f.A, f.B, cede) == war_result::applied;
            w.province_holder[holder_index(w, dg[0])] = f.B; // B re-takes it: the terms go stale
            // B is weary enough that the terms, were they live, would score ACCEPT:
            // without the stale check the pass would try accept_peace, be refused,
            // and leave the offer standing - the freeze this row pins.
            w.wars.at({ f.A, f.B }).defender_exhaustion = 80.0f;
            if (from_player) w.player_entity = f.A;
            w.current_econ_tick = 20;
            {
                world probe = w;
                check(filed && accept_peace(probe, sp, f.B, f.A) == war_result::rejected_state
                          && probe.peace_offers.count({ f.A, f.B }) == 1,
                      std::string("RW10 (setup") + (from_player ? ", player" : "")
                          + ") the stale offer: accept_peace refuses it and the offer stands");
            }
            trace.clear();
            score_nation_war(w, reg, f.B, 20);
            check(at_war(w, f.A, f.B) && w.peace_offers.empty() && did(war_ai_choice::refuse, f.B, f.A)
                      && !did(war_ai_choice::accept, f.B, f.A),
                  std::string("RW10") + (from_player ? "b a stale offer from the PLAYER'S nation" : " a stale offer")
                      + " is refused through refuse_peace; the war goes on and the offer is gone");
            trace.clear();
            score_nation_war(w, reg, f.B, 20);
            const auto oit = w.peace_offers.find({ f.A, f.B });
            check(oit != w.peace_offers.end() && oit->second.offerer == f.B && did(war_ai_choice::offer, f.B, f.A),
                  std::string("RW10") + (from_player ? "b'" : "'") + " a new offer can follow: the weary rival now offers peace");
        }
        // RW11 - a refused offer may not be repeated for four ticks (Ben, 2026-11-03,
        // NR-1120): the VERB refuses it, for any offerer, mutating nothing; the other
        // side may still offer; the fifth tick admits it.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 10;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
            w.current_econ_tick = 20;
            const bool refused = offer_peace(w, f.A, f.B, peace_terms{}) == war_result::applied
                              && refuse_peace(w, f.B, f.A) == war_result::applied
                              && w.peace_refusals.count({ f.A, f.B }) == 1 && w.peace_refusals.at({ f.A, f.B }) == 20;
            bool cooling = true;
            for (int t = 20; t < 24; ++t)
            {
                w.current_econ_tick = t;
                const snap s = take(w);
                cooling = cooling && peace_offer_cooling(w, f.A, f.B)
                       && offer_peace(w, f.A, f.B, peace_terms{}) == war_result::rejected_state && same(s, w);
            }
            check(refused && cooling,
                  "RW11a a refused offer records its tick; for ticks 20..23 the offerer's next offer is refused, mutating nothing");
            {
                world other = w;
                check(offer_peace(other, f.B, f.A, peace_terms{}) == war_result::applied,
                      "RW11b the side that refused may still offer while the other's offer cools");
            }
            w.current_econ_tick = 24;
            check(!peace_offer_cooling(w, f.A, f.B) && offer_peace(w, f.A, f.B, peace_terms{}) == war_result::applied,
                  "RW11c four ticks on (tick 24) the offerer may offer again");
            (void)accept_peace(w, sp, f.B, f.A);
            check(!at_war(w, f.A, f.B) && w.peace_refusals.empty(), "RW11d a refusal's record ends with the war");
        }
        // RW12 - the rival pass does not churn: refused, a weary rival waits out the
        // cooldown and then offers again.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 10;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
            w.wars.at({ f.A, f.B }).defender_exhaustion = 60.0f;
            int offers = 0, refused_verbs = 0;
            std::vector<int> offered_at;
            for (int t = 18; t <= 26; ++t)
            {
                w.current_econ_tick = t;
                trace.clear();
                score_nation_war(w, reg, f.B, t);
                for (const war_ai_trace_row& r : trace)
                    if (r.choice == war_ai_choice::offer)
                    {
                        if (r.applied) { ++offers; offered_at.push_back(t); }
                        else ++refused_verbs;
                    }
                if (w.peace_offers.count({ f.A, f.B }) != 0)
                    (void)refuse_peace(w, f.A, f.B); // A refuses every one
            }
            check(at_war(w, f.A, f.B) && offers == 3 && refused_verbs == 0
                      && offered_at == std::vector<int>({ 18, 22, 26 }),
                  "RW12 a weary rival refused at every offer offers at ticks 18, 22, 26 - once per cooldown, never a refused verb (NR-1120)");
        }
        // RW13 - under spectate the pass scores the player's nation like any rival
        // (BL-409's rule; Ben, 2026-11-03, NR-1120): it answers a war on it.
        {
            fixture f; build(f);
            world& w = f.w;
            w.current_econ_tick = 20;
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            w.player_entity = f.B;
            world seated = w;
            trace.clear();
            score_nation_war(seated, reg, f.B, 20, /*spectating=*/false);
            const bool seated_quiet = trace.empty() && !seated.wars.at({ f.A, f.B }).answered;
            trace.clear();
            score_nation_war(w, reg, f.B, 20, /*spectating=*/true);
            check(seated_quiet && w.wars.at({ f.A, f.B }).answered && did(war_ai_choice::answer, f.B, f.A),
                  "RW13 seated, the player's nation is never acted for; spectating, the pass answers for it like any rival");
        }
        // RW14 - the public record (Ben, 2026-11-03, NR-1120): a declaration and a peace
        // each write the world history log, one `war` entry per party tagged by its nation.
        {
            fixture f; build(f);
            world& w = f.w;
            const std::size_t before = w.history_log.size();
            (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
            const auto war_entries = [&w](std::size_t from) {
                std::vector<const world_history_entry*> out;
                for (std::size_t i = from; i < w.history_log.size(); ++i)
                    if (w.history_log[i].topic == history_topic::war) out.push_back(&w.history_log[i]);
                return out;
            };
            const auto d = war_entries(before);
            const bool decl = d.size() == 2 && d[0]->corp == f.A && d[1]->corp == f.B
                           && d[0]->event == d[1]->event && d[0]->event.find("declared war on") != std::string::npos;
            const std::size_t mid = w.history_log.size();
            (void)offer_peace(w, f.A, f.B, peace_terms{});
            (void)accept_peace(w, sp, f.B, f.A);
            const auto pz = war_entries(mid);
            const bool peace = pz.size() == 2 && pz[0]->corp == f.A && pz[1]->corp == f.B
                            && pz[0]->event.find("white peace") != std::string::npos;
            check(decl && peace,
                  "RW14 a declaration and a peace each write two `war` history entries, one tagged by each party");
            const std::string b = to_bytes(w);
            world back;
            check(from_bytes(b, back) && back.history_log.size() == w.history_log.size()
                      && back.history_log.back().topic == history_topic::war,
                  "RW14b the `war` topic round-trips the history log");
        }
        war_ai_trace_sink() = nullptr;
    }

    // =====================================================================
    std::printf("\nW-RF - the refusal record is saved (BL-1480, world_save_version 75)\n");
    {
        fixture f; build(f);
        world& w = f.w;
        w.current_econ_tick = 10;
        (void)declare_war(w, sp, f.A, f.B, { f.provs[f.B][0] });
        (void)answer_war(w, f.B, f.A, { f.provs[f.A][0] });
        (void)offer_peace(w, f.A, f.B, peace_terms{});
        (void)refuse_peace(w, f.B, f.A);
        const std::string b = to_bytes(w);
        uint32_t ver = 0;
        std::memcpy(&ver, b.data() + 4, 4);
        world back;
        // BL-1436 (Ben, 2026-11-03, ruling 6): v76 widens the resource arrays for airframes;
        // the refusals section rides on unchanged.
        check(ver >= 76 && ver == world_save_version && from_bytes(b, back) && to_bytes(back) == b
                  && back.peace_refusals == w.peace_refusals && back.state_hash(0) == w.state_hash(0),
              "RF1 the stream is today's (v76 or later) and the refusal round-trips byte for byte, its hash equal");
        check(w.state_hash(0) != [&] { world z = w; z.peace_refusals.clear(); return z.state_hash(0); }(),
              "RF2 the refusal is in the state hash");
        std::ostringstream o74(std::ios::binary);
        write_world_snapshot_pre_refusals(w, o74);
        const std::string b74 = o74.str();
        world old;
        check(world_save_refusals_section_bytes(w) == 4 + 12 && b74.size() + 16 + world_save_airframes_bytes(w) /* BL-1436: v74 is 54 wide */ + tail_after_refusals(w) == b.size()
                  && from_bytes(b74, old) && old.peace_refusals.empty() && old.wars.size() == 1,
              "RF3 a v74 stream (today's less the 16-byte refusals section) reads as a world with no refusals");
        const auto refused = [&](std::string bad, const char* what) {
            world untouched;
            check(!from_bytes(bad, untouched) && untouched.nations.empty(), what);
        };
        const std::size_t sec = b.size() - tail_after_refusals(w) - world_save_refusals_section_bytes(w);
        {
            std::string bad = b;
            std::memcpy(&bad[sec + 8], &bad[sec + 4], 4); // (A, A)
            refused(bad, "RF4 a refusal naming one nation twice is refused whole, the destination untouched");
        }
        {
            std::string bad = b;
            const int32_t neg = -1;
            std::memcpy(&bad[sec + 12], &neg, 4);
            refused(bad, "RF5 a negative refusal tick is refused whole");
        }
        {
            std::string bad = b;
            const entity_id c = f.C;
            std::memcpy(&bad[sec + 8], &c, 4); // (A, C): no war between them
            refused(bad, "RF6 a refusal between nations at peace is refused whole");
        }
    }

    // =====================================================================
    std::printf("\nW-D - determinism\n");
    {
        fixture f1; build(f1);
        fixture f2; build(f2);
        for (fixture* f : { &f1, &f2 })
        {
            f->w.corp_friend_pairs.insert(canonical_pair(f->A, f->D));
            (void)declare_war(f->w, sp, f->C, f->D, { f->provs[f->D][0] });
            (void)declare_war(f->w, sp, f->A, f->C, { f->provs[f->C][2], f->provs[f->C][0] });
            (void)answer_war(f->w, f->C, f->A, { f->provs[f->A][1] });
        }
        check(to_bytes(f1.w) == to_bytes(f2.w) && f1.w.state_hash(3) == f2.w.state_hash(3),
              "D1 the same sequence on two built worlds gives byte-identical snapshots and hashes");
    }

    std::printf("\nRESULT: %d failures / %d checks\n", failures, checks);
    return failures == 0 ? 0 : 1;
}
