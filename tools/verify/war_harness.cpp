// ---------------------------------------------------------------------------
// Headless war harness (BL-1459 war record and declaration, BL-1428 war goals and
// peace; no SDL / Lua / ImGui) — docs/politics/RELATIONS.md § 7.
// ---------------------------------------------------------------------------
// Requirement groups `war-record-and-declaration` (R1-R5) and `war-goals-and-peace`
// (R1-R4). Every refusal is checked by a WHOLE-WORLD snapshot compare (the save
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
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/sentiment.hpp"
#include "world/stance.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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
    auto set = [&p](sentiment_factor_kind k, float a, float t) {
        p.factors[static_cast<std::size_t>(k)].access = a;
        p.factors[static_cast<std::size_t>(k)].trust  = t;
    };
    set(sentiment_factor_kind::hostility_declared, -10.0f, -10.0f);
    set(sentiment_factor_kind::war_declared,         0.0f,  -3.0f);
    set(sentiment_factor_kind::peace_concluded,      0.0f,   4.0f);
    set(sentiment_factor_kind::province_ceded,      -6.0f,  -6.0f);
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

float trust_of(const world& w, entity_id o, entity_id s) { return sentiment_toward(w.sentiment, o, s).trust; }
float access_of(const world& w, entity_id o, entity_id s) { return sentiment_toward(w.sentiment, o, s).access; }

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
        check(r == war_result::applied && is_hostile(w, f.A, f.B) && !is_hostile(w, f.B, f.A),
              "R1a declare_war applies: the declarer's hostility row is written, the defender's is not");
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
                  && h.access == -10.0f && h.trust == -10.0f,
              "R2b hostility_declared lands on the defender's row toward the declarer, once");
        const float gf = 1.0f + k_war_per_extra_goal; // two goals
        check(by[f.F * 2].magnitude == gf && by[f.F * 2].trust == -3.0f * gf && !by[f.F * 2].borders,
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
        check(declare_war(w, sp, f.A, f.C, goals) == war_result::applied, "R2g the declaration applies");
        bool eq = true;
        float sa = 0, st = 0;
        for (const war_price_row& r : pr.rows)
        {
            const sentiment_value now = sentiment_toward(w.sentiment, r.observer, f.A);
            eq = eq && now.access - before[r.observer].access == r.access
                    && now.trust - before[r.observer].trust == r.trust;
            sa += now.access - before[r.observer].access;
            st += now.trust - before[r.observer].trust;
        }
        check(eq && sa == pr.sum_access && st == pr.sum_trust,
              "R2h declare_war charged exactly the preview, per nation and summed");
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
        jtest(0, "grievance (declarer's Trust toward the defender in the adverse band)", [&](fixture& g) {
            g.w.sentiment.pairs[{ g.A, g.C }] = sentiment_value{ 0.0f, -60.0f };
        });
        jtest(1, "embargo (the defender's condition_set refuses the declarer)", [&](fixture& g) {
            condition c; c.subject = condition_subject::surplus;
            c.comparator = condition_comparator::at_least; c.operand = 1.0e12f;
            condition_set cs; cs.all.push_back(c);
            g.w.corp_embargo_conditions[g.C] = cs;
        });
        jtest(2, "lost ground (a goal the declarer ceded to the defender at an earlier peace)", [&](fixture& g) {
            g.w.province_cessions.push_back(province_cession{ g.A, g.C, g.provs[g.C][1], 0 });
        });
        jtest(3, "a friend attacked (the defender declared war on the declarer's friend)", [&](fixture& g) {
            g.w.corp_friend_pairs.insert(canonical_pair(g.A, g.D));
            (void)declare_war(g.w, sp, g.C, g.D, { g.provs[g.D][0] });
        });
        {
            // A grievance just inside the neutral side of the band edge does not hold.
            fixture g; build(g);
            g.w.sentiment.pairs[{ g.A, g.C }] = sentiment_value{ 0.0f, -49.0f };
            const auto j = war_justifications(g.w, sp, g.A, g.C, { g.provs[g.C][1] });
            check(!j[0], "R2k a Trust of -49 (wary, not adverse) is no grievance");
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
        check(answer_war(w, f.B, f.A, { f.provs[f.A][3], f.provs[f.A][1] }) == war_result::applied,
              "R3e the defender's answer applies");
        const war_record* rec = war_between(w, f.A, f.B);
        check(rec && rec->answered && is_hostile(w, f.B, f.A)
                  && rec->defender_goals == std::vector<uint32_t>({ f.provs[f.A][1], f.provs[f.A][3] }),
              "R3f it writes the defender's hostility row and names its goals (ascending)");
        check(before.pairs == w.sentiment.pairs, "R3g it costs nothing: no sentiment row moves");
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
        check(ver == world_save_version && world_save_version == 69, "R4j the stream is version 69");
        check(b.size() >= world_save_war_section_bytes(w), "R4k the war section's measured length fits the stream");
        // An older stream loads with no wars.
        std::ostringstream o67(std::ios::binary);
        write_world_snapshot_pre_war(w, o67);
        const std::string b67 = o67.str();
        world old;
        check(from_bytes(b67, old) && old.wars.empty() && old.truces.empty() && old.peace_offers.empty()
                  && old.war_history.empty() && old.province_cessions.empty()
                  && b67.size() == b.size() - world_save_war_section_bytes(w),
              "R4l a v67 stream (today's less the war section) loads with no wars");
        // Untrusted: an answered byte of 2 is refused whole.
        std::string bad = b;
        const std::size_t war0 = b.size() - world_save_war_section_bytes(w) + 4; // past the war count
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
        w.province_holder[holder_index(w, dg[0])] = f.A;
        const double total = tiles(dg[0]) + tiles(dg[1]) + tiles(fg[0]);
        const int want1 = static_cast<int>(std::llround(50.0 * tiles(dg[0]) / total));
        check(score() == want1, "P1b the declarer holding one goal scores 50 x its tiles over all goals' tiles ("
                                    + std::to_string(want1) + ")");
        w.province_holder[holder_index(w, fg[0])] = f.B;
        const int want2 = static_cast<int>(std::llround(50.0 * (tiles(dg[0]) - tiles(fg[0])) / total));
        check(score() == want2, "P1c the defender holding its goal subtracts its tiles (" + std::to_string(want2) + ")");
        war_record& rec = w.wars.at({ f.A, f.B });
        rec.declarer_men_lost = 100; rec.defender_men_lost = 300;
        const int want3 = static_cast<int>(std::llround(50.0 * (tiles(dg[0]) - tiles(fg[0])) / total + 50.0 * 200.0 / 400.0));
        check(score() == want3, "P1d battles: the declarer's share of strength destroyed less the defender's, half the score ("
                                    + std::to_string(want3) + ")");
        // THE FORMULA AS WRITTEN (RELATIONS.md § 7): the denominator is EVERY goal named,
        // so once the defender has answered, holding all of one's own goals is not the
        // whole occupation half — neither side can reach +-100 in an answered war. Raised
        // in the BL-1428 report as a design question; asserted here as the formula reads.
        w.province_holder[holder_index(w, dg[1])] = f.A;
        w.province_holder[holder_index(w, fg[0])] = f.A; // A also keeps its own province
        rec.declarer_men_lost = 0;
        const int want5 = static_cast<int>(std::llround(50.0 * (tiles(dg[0]) + tiles(dg[1])) / total + 50.0));
        check(score() == want5 && want5 < 100,
              "P1e answered war: every declarer goal held, only defender losses scores 50 x its goals' share + 50 ("
                  + std::to_string(want5) + "), short of 100");
        w.province_holder[holder_index(w, dg[0])] = f.B; w.province_holder[holder_index(w, dg[1])] = f.B;
        w.province_holder[holder_index(w, fg[0])] = f.B;
        rec.declarer_men_lost = 500; rec.defender_men_lost = 0;
        const int want6 = static_cast<int>(std::llround(-50.0 * tiles(fg[0]) / total - 50.0));
        check(score() == want6 && want6 > -100,
              "P1f the mirror: the defender holding its goal, only declarer losses (" + std::to_string(want6) + ")");
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
        // Cession accepted.
        check(offer_peace(w, f.A, f.B, cede) == war_result::applied, "P3i (re-offer the cession)");
        const float trust_ab = trust_of(w, f.A, f.B), trust_ba = trust_of(w, f.B, f.A);
        const float acc_ba = access_of(w, f.B, f.A);
        w.current_econ_tick = 20;
        check(accept_peace(w, sp, f.B, f.A) == war_result::applied, "P3j the receiver accepts the cession");
        check(!at_war(w, f.A, f.B) && !is_hostile(w, f.A, f.B) && !is_hostile(w, f.B, f.A)
                  && w.peace_offers.empty() && w.war_history.size() == 1
                  && w.war_history.back().end == war_end::cession && w.war_history.back().gainer == f.A
                  && w.war_history.back().ceded == std::vector<uint32_t>({ dg[0] }),
              "P3k at peace both hostility rows clear and the record closes into the history");
        check(w.province_cessions.size() == 1 && w.province_cessions[0].from == f.B && w.province_cessions[0].to == f.A
                  && w.province_cessions[0].province == dg[0]
                  && province_holder_for(w, dg[0]) == f.A && province_owner(w, dg[0]) == f.B,
              "P3l the cession stub records the ceded province and keeps its holder; tile_to_nation is BL-1460's (still B's)");
        check(trust_of(w, f.A, f.B) - trust_ab == 4.0f
                  && trust_of(w, f.B, f.A) - trust_ba == 4.0f - 6.0f
                  && access_of(w, f.B, f.A) - acc_ba == -6.0f,
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
        s = take(w);
        check(war_score(w, *war_between(w, f.A, f.B)) == 100
                  && impose_peace(w, sp, f.B, f.A, peace_terms{}) == war_result::rejected_state && same(s, w),
              "P3t at +100 the DEFENDER cannot impose, mutating nothing");
        check(impose_peace(w, sp, f.A, f.B, cede) == war_result::applied && !at_war(w, f.A, f.B)
                  && w.war_history.back().end == war_end::imposed_cession && w.war_history.back().final_score == 100
                  && w.province_cessions.size() == 2,
              "P3u at +100 the declarer imposes its cession without acceptance");
    }
    {
        // Never the loser's last province.
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
        const snap s = take(w);
        check(war_score(w, *war_between(w, f.A, f.B)) == 100
                  && offer_peace(w, f.A, f.B, cede) == war_result::rejected_state
                  && impose_peace(w, sp, f.A, f.B, cede) == war_result::rejected_state && same(s, w),
              "P3v a cession of the loser's last province is refused, offered or imposed, mutating nothing (NR-1102)");
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
        const std::size_t off = b2.size() - world_save_war_section_bytes(w) + 4
                              + (4 + 4 + 1 + 4 + 4 + 8 + 8 + 4 + 4 + 4 + 4 * wr.declarer_goals.size() + 4 + 4 * wr.defender_goals.size())
                              + 4 + 12 * w.truces.size() + 4 + 4 + 4 + 4 + 4; // to the kind byte
        bad2[off] = 7;
        world untouched;
        check(w.wars.size() == 1 && !from_bytes(bad2, untouched) && untouched.nations.empty(),
              "R4j a corrupt peace offer (kind byte 7) is refused whole on load");
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
