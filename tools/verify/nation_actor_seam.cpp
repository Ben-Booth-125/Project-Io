// ---------------------------------------------------------------------------
// nation_actor_seam — BL-1399 (nation builds own ground), requirement group `own-ground`
// ---------------------------------------------------------------------------
// docs/politics/NATIONS.md § What a nation owns / § Own ground; docs/GLOSSARY.md
// § Own ground (Ben, 2026-10-23). On the SHIPPED start (harness_params.hpp's app
// mirror), per seed:
//
//   N1  A NATION BUILDS FOR ITSELF. A MINOR nation (no champion, so no corporation
//       could ever have stood in for it) builds on its own ground through the legal
//       verb — cmd.corp == cmd.as_nation == the nation — and the building joins its
//       state_assets. (R3)
//   N2  OWN GROUND, EVERY KIND. For each building kind the world can place
//       (extraction site, processing facility, logistics hub, port), a tile that
//       kind is placeable on for ITS owner is refused to another nation
//       (`not_own_ground` / `foreign_ground`) — and unowned land is refused to every
//       nation. Each refusal leaves state_hash unchanged. (R1)
//   N3  ROADS: ONE CROSSING IN. From a roaded tile of a nation's own ground, a
//       crossing into a neighbour's tile (another nation's, or nobody's) is not
//       refused for being foreign; from a tile the nation does not hold, it is
//       refused `foreign_ground`. (R2)
//
// BL-1402 port lane M (NATIONS.md § What a nation owns; the corporations are about to be
// folded into their nations, so every verb below must already serve a nation acting as
// itself). Each refusal leaves the fingerprint (state_hash + treasuries + road edges +
// the state ledger's construction line) unchanged:
//   M1  HIRE. A minor hires at its own completed military base, paid from its treasury;
//       a hire past its treasury room is refused (a nation never runs into debt), and
//       another nation cannot muster at its base.
//   M2  MARCH. The hired unit marches as its nation; another nation cannot march it.
//   M3  ROAD. A minor lays a road from its own roaded tile, paid from its treasury and
//       booked on its state ledger; with an empty treasury it is refused.
//   M4  DEMOLISH. A minor demolishes its own building; another nation is refused.
//   M5  BUDGET PAYEE. A claim paid to a nation lands in its treasury; a nation's claim on
//       its OWN budget is no transfer at all.
//   M6  PROCUREMENT is refused to a nation (CONTRACTS.md: named-supplier procurement is
//       retired); the trade reserve's answer is reported (lane T owns it).
//   M7  RESEARCH. The seat's nation queues research as itself; an AI nation is refused.
//
// Build: cmd //c tools\verify\build_lua_harness.bat nation_actor_seam
// Run:   build_gen/verify/nation_actor_seam.exe [seed ...]   (default: 0)
// Exits non-zero on any FAIL. Every row is REQUIRED: a row that cannot find its fixture on
// a seed FAILS (cold review, 2026-10-23: a finding that cannot fail lets a seed pass with
// no checks at all).

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/construction.hpp"
#include "world/corp_command.hpp"
#include "world/logistics.hpp"
#include "world/nation_budget.hpp"
#include "world/nation_tech.hpp"
#include "world/placement_rules.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/state_industry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0, g_finding = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

void finding(const char* row, const std::string& what)
{
    std::printf("  [FINDING] %s %s\n", row, what.c_str());
    ++g_finding;
}

std::string fmt(const char* f, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

entity_id holder_of(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

std::vector<entity_id> sorted_nations(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<entity_id> sorted_tiles(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& kv : w.tiles) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

// One placeable building of a kind: the type, its target and recipe, and the gate.
struct kind
{
    const char*   name;
    building_type type;
    resource_type target;
    std::uint16_t recipe;
};

placement_rules::placement_result place_check(world& w, const recipe_registry& reg,
                                              entity_id tile, const kind& k, entity_id actor)
{
    if (const auto tit = w.tiles.find(tile); tit != w.tiles.end())
        body_reach_field(w, tit->second.body);
    return placement_rules::can_place_in_world(w, tile, k.type, k.target,
                                               reg.construction().max_logistics_reach, actor,
                                               reg.placement_gate_for(k.type, k.recipe));
}

bool refused_for_ground(const placement_rules::placement_result& r)
{
    return r.reason == placement_rules::placement_reason::not_own_ground
        || r.reason == placement_rules::placement_reason::foreign_ground;
}

// ---- BL-1402 port lane M: the verbs a nation acting as itself may issue --------
// A FINGERPRINT of everything these rows could move: state_hash (buildings, units,
// trades, the state_assets lists) plus what it does not fold - every treasury, every
// road edge and the state ledger's construction line. A refusal must leave all of it.
std::uint64_t fingerprint(const world& w)
{
    std::uint64_t h = w.state_hash(0);
    auto mix = [&h](std::uint64_t v) { h ^= v; h *= 1099511628211ull; };
    for (const entity_id n : sorted_nations(w))
    {
        float t = w.nations.at(n).treasury;
        std::uint32_t bits = 0;
        std::memcpy(&bits, &t, sizeof bits);
        mix(n); mix(bits);
        if (const auto lit = w.state_ledger.find(n); lit != w.state_ledger.end())
        {
            float c = lit->second.construction;
            std::memcpy(&bits, &c, sizeof bits);
            mix(bits);
        }
    }
    for (const entity_id t : sorted_tiles(w))
        mix(w.tiles.at(t).road_edges);
    return h;
}

corp_command self_cmd(entity_id nation, corp_verb verb)
{
    corp_command cmd;
    cmd.corp = nation;
    cmd.as_nation = nation;
    cmd.verb = verb;
    return cmd;
}

/// The money a nation may commit is its treasury less what the tick committed; the
/// fixtures clear the ledger so the treasury IS the room, and the arithmetic is exact.
void set_room(world& w, entity_id nation, float treasury)
{
    w.state_ledger.erase(nation);
    w.nations.at(nation).treasury = treasury;
}

void run_port_rows(const world& w0, const recipe_registry& reg, const std::vector<entity_id>& nations)
{
    // ---- M1/M2: a minor hires at its own base, then marches the unit ----------
    std::printf("--- M1 a nation hires as itself / M2 marches ---\n");
    {
        bool tried = false;
        const kind base{"military base", building_type::military_base, resource_type::iron_ore, no_recipe};
        for (const entity_id m : nations)
        {
            if (is_major_nation(w0, m) || tried) continue;
            for (const entity_id t : w0.nations.at(m).tiles)
            {
                world w = w0;
                if (!place_check(w, reg, t, base, m)) continue;
                set_room(w, m, 1.0e9f);
                entity_id b = null_entity;
                if (construct_nation(w, reg, m, t, base.type, base.target, no_recipe, b)
                    != construction_result::placed)
                    continue;
                w.buildings.at(b).ticks_remaining = 0; // completion is not the subject
                const auto avail = available_rows(w, m, campaign_roster_band_for(reg.era()));
                if (avail.empty()) continue;
                tried = true;
                const roster_row* row = avail.front();
                const std::uint8_t type =
                    static_cast<std::uint8_t>(row - unit_roster_table().data());
                const float cost = reg.military().hire_base_cost
                    + reg.military().hire_cost_per_power * static_cast<float>(row->power_mod);
                corp_command cmd = self_cmd(m, corp_verb::hire_unit);
                cmd.tile = t;
                cmd.unit_type = type;

                // Refusal 1: the treasury cannot cover the hire - a nation never runs into debt.
                set_room(w, m, cost * 0.5f);
                std::uint64_t before = fingerprint(w);
                corp_command_result r = apply_corp_command(w, reg, cmd);
                check(r == corp_command_result::rejected_funds && fingerprint(w) == before, "M1",
                      fmt("minor %llu cannot hire past its treasury (result %d, unchanged %d)",
                          static_cast<unsigned long long>(m), static_cast<int>(r),
                          fingerprint(w) == before ? 1 : 0));
                // Refusal 2: another nation hiring at this nation's base.
                set_room(w, m, 1.0e6f);
                entity_id other = null_entity;
                for (const entity_id n : nations) if (n != m) { other = n; break; }
                {
                    world o = w;
                    set_room(o, other, 1.0e6f);
                    corp_command oc = self_cmd(other, corp_verb::hire_unit);
                    oc.tile = t;
                    oc.unit_type = type;
                    before = fingerprint(o);
                    r = apply_corp_command(o, reg, oc);
                    check(r != corp_command_result::applied && fingerprint(o) == before, "M1",
                          fmt("nation %llu cannot muster at another nation's base (result %d)",
                              static_cast<unsigned long long>(other), static_cast<int>(r)));
                }
                // The hire.
                entity_id unit = null_entity;
                r = apply_corp_command(w, reg, cmd, &unit);
                const bool hired = r == corp_command_result::applied && w.units.count(unit) != 0
                    && w.units.at(unit).owner == m && w.units.at(unit).muster_base == b
                    && w.nations.at(m).treasury == 1.0e6f - cost;
                check(hired, "M1", fmt("minor %llu hires a unit (row %u) at its own base as itself, "
                                       "paid %.2f from its treasury (result %d)",
                                       static_cast<unsigned long long>(m), static_cast<unsigned>(type),
                                       static_cast<double>(cost), static_cast<int>(r)));
                if (!hired) break;

                // M2: march it to a land province on the same body.
                const entity_id body = w.tiles.at(t).body;
                const std::uint32_t here = w.provinces.province_of(t);
                {
                    world o = w;
                    corp_command oc = self_cmd(other, corp_verb::march_unit);
                    oc.subject = unit;
                    for (const province& p : w.provinces.provinces)
                        if (p.body == body && p.id != here) { oc.province = p.id; break; }
                    before = fingerprint(o);
                    r = apply_corp_command(o, reg, oc);
                    check(r == corp_command_result::rejected_not_owner && fingerprint(o) == before, "M2",
                          fmt("another nation cannot march this nation's unit (result %d)",
                              static_cast<int>(r)));
                }
                bool marched = false;
                int asked = 0;
                for (const province& p : w.provinces.provinces)
                {
                    if (p.body != body || p.id == here || p.tiles.empty()) continue;
                    if (province_kind_of(w, p) != province_kind::land) continue;
                    if (++asked > 40) break;
                    corp_command mc = self_cmd(m, corp_verb::march_unit);
                    mc.subject = unit;
                    mc.province = p.id;
                    world o = w;
                    if (apply_corp_command(o, reg, mc) == corp_command_result::applied
                        && o.units.at(unit).order.dest_province == p.id)
                    {
                        marched = true;
                        break;
                    }
                }
                check(marched, "M2", fmt("minor %llu marches its unit as itself (%d provinces asked)",
                                         static_cast<unsigned long long>(m), asked));
                break;
            }
        }
        if (!tried) check(false, "M1", "no minor can stand a military base with a hireable row");
    }

    // ---- M3: a minor lays a road from its own ground -------------------------
    std::printf("--- M3 a nation lays a road as itself ---\n");
    {
        bool tried = false;
        for (const entity_id m : nations)
        {
            if (is_major_nation(w0, m) || tried) continue;
            for (const entity_id t : w0.nations.at(m).tiles)
            {
                if (max_road_tier(w0.tiles.at(t)) == 0) continue;
                int side = -1;
                std::uint8_t tier = 0;
                for (int s = 0; s < 6 && side < 0; ++s)
                    for (std::uint8_t k = 1; k <= 3 && side < 0; ++k)
                        if (placement_rules::can_place_road(w0, t, s, k, m)) { side = s; tier = k; }
                if (side < 0) continue;
                tried = true;
                world w = w0;
                corp_command cmd = self_cmd(m, corp_verb::place_road);
                cmd.tile = t;
                cmd.road_tier = tier;
                cmd.road_side = static_cast<std::uint8_t>(side);
                set_room(w, m, 0.0f);
                std::uint64_t before = fingerprint(w);
                corp_command_result r = apply_corp_command(w, reg, cmd);
                check(r == corp_command_result::rejected_funds && fingerprint(w) == before, "M3",
                      fmt("minor %llu cannot lay a road with an empty treasury (result %d)",
                          static_cast<unsigned long long>(m), static_cast<int>(r)));
                set_room(w, m, 1.0e6f);
                r = apply_corp_command(w, reg, cmd);
                const float paid = 1.0e6f - w.nations.at(m).treasury;
                const float booked = w.state_ledger.count(m) ? w.state_ledger.at(m).construction : 0.0f;
                check(r == corp_command_result::applied
                          && road_tier_on_side(w.tiles.at(t), side) == tier && paid > 0.0f
                          && std::fabs(paid - booked) <= 1.0e-3f * std::max(1.0f, paid),
                      "M3", fmt("minor %llu lays a tier-%u road from its tile %llu side %d as itself, "
                                "paid %.2f from its treasury, booked %.2f (result %d)",
                                static_cast<unsigned long long>(m), static_cast<unsigned>(tier),
                                static_cast<unsigned long long>(t), side, static_cast<double>(paid),
                                static_cast<double>(booked), static_cast<int>(r)));
                break;
            }
        }
        if (!tried) check(false, "M3", "no minor holds a roaded tile with a legal crossing");
    }

    // ---- M4: a minor demolishes its own building -----------------------------
    std::printf("--- M4 a nation demolishes its own building ---\n");
    {
        bool tried = false;
        const kind hub{"logistics hub", building_type::inland_logistics_hub, resource_type::iron_ore, no_recipe};
        for (const entity_id m : nations)
        {
            if (is_major_nation(w0, m) || tried) continue;
            for (const entity_id t : w0.nations.at(m).tiles)
            {
                world w = w0;
                if (!place_check(w, reg, t, hub, m)) continue;
                set_room(w, m, 1.0e9f);
                entity_id b = null_entity;
                if (construct_nation(w, reg, m, t, hub.type, hub.target, no_recipe, b)
                    != construction_result::placed)
                    continue;
                tried = true;
                entity_id other = null_entity;
                for (const entity_id n : nations) if (n != m) { other = n; break; }
                corp_command oc = self_cmd(other, corp_verb::demolish);
                oc.subject = b;
                std::uint64_t before = fingerprint(w);
                corp_command_result r = apply_corp_command(w, reg, oc);
                check(r == corp_command_result::rejected_not_owner && fingerprint(w) == before, "M4",
                      fmt("nation %llu cannot demolish another nation's building (result %d)",
                          static_cast<unsigned long long>(other), static_cast<int>(r)));
                corp_command cmd = self_cmd(m, corp_verb::demolish);
                cmd.subject = b;
                r = apply_corp_command(w, reg, cmd);
                const auto& sa = w.nations.at(m).state_assets;
                check(r == corp_command_result::applied && w.buildings.count(b) == 0
                          && std::find(sa.begin(), sa.end(), b) == sa.end(),
                      "M4", fmt("minor %llu demolishes its own hub %llu as itself (result %d)",
                                static_cast<unsigned long long>(m), static_cast<unsigned long long>(b),
                                static_cast<int>(r)));
                break;
            }
        }
        if (!tried) check(false, "M4", "no minor can stand a hub to demolish");
    }

    // ---- M5: a budget claim paid to a nation lands in its treasury -----------
    std::printf("--- M5 a budget claim paid to a nation ---\n");
    if (nations.size() < 2)
        check(false, "M5", "fewer than two nations on this seed");
    else
    {
        const entity_id a = nations[0], b = nations[1];
        std::map<entity_id, nation_budget> budgets;
        budgets[a].weights[static_cast<std::size_t>(budget_priority::public_works)] = 1.0f;
        budget_claim to_b;
        to_b.nation = a;
        to_b.corp   = b;
        to_b.line   = budget_priority::public_works;
        to_b.amount = 100.0f;
        budget_claim to_self = to_b;
        to_self.corp = a;
        {
            world w = w0;
            set_room(w, a, 1000.0f);
            set_room(w, b, 50.0f);
            national_budget_tick rep;
            run_national_budget(w, budgets, {to_b}, &rep);
            check(w.nations.at(a).treasury == 900.0f && w.nations.at(b).treasury == 150.0f
                      && rep.transfers.size() == 1 && rep.transfers[0].corp == b,
                  "M5", fmt("nation %llu's claim on nation %llu's budget lands in its treasury "
                            "(payer %.2f, payee %.2f, %zu transfer(s))",
                            static_cast<unsigned long long>(b), static_cast<unsigned long long>(a),
                            static_cast<double>(w.nations.at(a).treasury),
                            static_cast<double>(w.nations.at(b).treasury), rep.transfers.size()));
        }
        {
            world w = w0;
            set_room(w, a, 1000.0f);
            const std::uint64_t before = fingerprint(w);
            national_budget_tick rep;
            run_national_budget(w, budgets, {to_self}, &rep);
            check(fingerprint(w) == before && rep.transfers.empty() && rep.total_transferred == 0.0f,
                  "M5", fmt("a nation's claim on its OWN budget is no transfer and moves nothing "
                            "(%zu transfer(s))", rep.transfers.size()));
        }
    }

    // ---- M6: procurement is refused to a nation; trade is lane T's ----------
    std::printf("--- M6 refusals: procurement, trade ---\n");
    {
        const entity_id m = nations.front();
        // BL-1407: no supplier firm exists; the counterparty is another nation.
        entity_id corp = null_entity;
        for (const entity_id n : nations)
            if (n != m) { corp = n; break; }
        world w = w0;
        for (const corp_verb v : {corp_verb::request_quote, corp_verb::accept_quote,
                                  corp_verb::cancel_contract})
        {
            corp_command cmd = self_cmd(m, v);
            cmd.counterparty = corp;
            cmd.target = resource_type::iron_ore;
            cmd.quantity = 10.0f;
            cmd.order = 1;
            const std::uint64_t before = fingerprint(w);
            const corp_command_result r = apply_corp_command(w, reg, cmd);
            check(r == corp_command_result::rejected_invalid && fingerprint(w) == before, "M6",
                  fmt("procurement verb %d is refused to a nation, nothing moved (result %d)",
                      static_cast<int>(v), static_cast<int>(r)));
        }
        corp_command st = self_cmd(m, corp_verb::set_trade_reserve);
        st.quantity = 1.0f;
        const std::uint64_t before = fingerprint(w);
        const corp_command_result r = apply_corp_command(w, reg, st);
        finding("M6", fmt("set_trade_reserve as a nation answers %d, unchanged %d "
                          "(lane T owns the nation's trade book)",
                          static_cast<int>(r), fingerprint(w) == before ? 1 : 0));
    }

    // ---- M7: research as the seat's nation acting as itself -----------------
    std::printf("--- M7 research verbs as the nation ---\n");
    {
        const entity_id pn = player_nation(w0);
        if (pn == null_entity)
            check(false, "M7", "no player nation on this seed");
        else
        {
            world w = w0;
            int queued = -1;
            for (int n = 0; n < io::dg::node_count && queued < 0; ++n)
            {
                corp_command cmd = self_cmd(pn, corp_verb::queue_research);
                cmd.order = static_cast<std::uint32_t>(n);
                if (apply_corp_command(w, reg, cmd) == corp_command_result::applied) queued = n;
            }
            check(queued >= 0, "M7", fmt("the player's nation %llu queues research as itself (node %d)",
                                         static_cast<unsigned long long>(pn), queued));
            entity_id rival = null_entity;
            for (const entity_id n : nations) if (n != pn) { rival = n; break; }
            corp_command rc = self_cmd(rival, corp_verb::clear_research);
            const std::uint64_t before = fingerprint(w);
            auto qlen_of = [&w](entity_id n) {
                const auto it = w.nation_tech.find(n);
                return it != w.nation_tech.end() ? it->second.queue.size() : std::size_t{0};
            };
            const std::size_t qlen = qlen_of(rival);
            const corp_command_result r = apply_corp_command(w, reg, rc);
            check(r == corp_command_result::rejected_state && fingerprint(w) == before
                      && qlen_of(rival) == qlen,
                  "M7", fmt("an AI nation acting as itself is refused the seat's research verbs (result %d)",
                            static_cast<int>(r)));
        }
    }
}

void run_seed(lua_state& lua, std::uint32_t seed)
{
    std::printf("\n=== seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    world& w = out.w;
    const recipe_registry& reg = out.reg;
    const std::vector<entity_id> nations = sorted_nations(w);
    const std::vector<entity_id> tiles = sorted_tiles(w);

    // The kinds: every extractable resource is tried for the extraction site.
    std::vector<kind> kinds;
    for (std::size_t r = 0; r < resource_count; ++r)
        kinds.push_back({"extraction site", building_type::extraction_site,
                         static_cast<resource_type>(r), no_recipe});
    kinds.push_back({"processing facility", building_type::processing_facility,
                     resource_type::iron_ore, reg.default_recipe_id_at(w, tiles.front())});
    kinds.push_back({"logistics hub", building_type::inland_logistics_hub,
                     resource_type::iron_ore, no_recipe});
    kinds.push_back({"port", building_type::port, resource_type::iron_ore, no_recipe});
    kinds.push_back({"launchpad", building_type::launchpad, resource_type::iron_ore, no_recipe});
    kinds.push_back({"military base", building_type::military_base, resource_type::iron_ore, no_recipe});
    kinds.push_back({"research institute", building_type::research_institute, resource_type::iron_ore, no_recipe});
    kinds.push_back({"schooling", building_type::schooling, resource_type::iron_ore, no_recipe});
    kinds.push_back({"university", building_type::university, resource_type::iron_ore, no_recipe});
    kinds.push_back({"planetary marketplace", building_type::planetary_marketplace, resource_type::iron_ore, no_recipe});

    // ---- N1: a minor builds for itself --------------------------------------
    std::printf("--- N1 a nation builds for itself ---\n");
    {
        // A processor recipe the OLD state door refused (not a strategic sector), so
        // the row proves the sector limit is gone, not only that a door opened.
        std::uint16_t civil = no_recipe;
        for (int i = 0; i < reg.recipe_count(building_type::processing_facility) && civil == no_recipe; ++i)
        {
            const std::uint16_t rid =
                reg.recipe_id(reg.recipe_at(building_type::processing_facility, i).name);
            if (!state_runs(reg, building_type::processing_facility, rid)) civil = rid;
        }
        std::vector<kind> want;
        for (const kind& k : kinds)
            if (k.type == building_type::extraction_site) { want.push_back(k); break; }
        want.push_back({"non-strategic processor", building_type::processing_facility,
                        resource_type::iron_ore, civil});
        want.push_back({"logistics hub", building_type::inland_logistics_hub,
                        resource_type::iron_ore, no_recipe});
        for (kind k : want)
        {
            bool tried = false;
            for (const entity_id m : nations)
            {
                if (is_major_nation(w, m)) continue;
                for (const entity_id t : w.nations.at(m).tiles)
                {
                    // Any extractable resource stands in for the extraction kind.
                    kind use = k;
                    bool ok = false;
                    if (k.type == building_type::extraction_site)
                    {
                        for (std::size_t r = 0; r < resource_count && !ok; ++r)
                        {
                            use.target = static_cast<resource_type>(r);
                            ok = static_cast<bool>(place_check(w, reg, t, use, m));
                        }
                    }
                    else
                        ok = static_cast<bool>(place_check(w, reg, t, use, m));
                    if (!ok) continue;
                    tried = true;
                    world copy = w;
                    copy.nations.at(m).treasury = 1.0e9f; // the verb's money gate is not the subject
                    corp_command cmd;
                    cmd.corp = m;
                    cmd.as_nation = m;
                    cmd.verb = corp_verb::build;
                    cmd.tile = t;
                    cmd.type = use.type;
                    cmd.target = use.target;
                    cmd.recipe = use.type == building_type::processing_facility ? use.recipe : no_recipe;
                    entity_id b = null_entity;
                    const corp_command_result res = apply_corp_command(copy, reg, cmd, &b);
                    const auto& sa = copy.nations.at(m).state_assets;
                    const bool built = res == corp_command_result::applied && b != null_entity
                        && std::find(sa.begin(), sa.end(), b) != sa.end()
                        && (use.type != building_type::processing_facility
                            || copy.buildings.at(b).recipe == use.recipe);
                    check(built, "N1", fmt("minor %llu builds a %s on its own tile %llu as itself "
                                           "(result %d)", static_cast<unsigned long long>(m), k.name,
                                           static_cast<unsigned long long>(t), static_cast<int>(res)));
                    break;
                }
                if (tried) break;
            }
            if (!tried) check(false, "N1", fmt("no minor has a tile a %s is placeable on", k.name));
        }
    }

    // ---- N2: own ground, every kind -----------------------------------------
    std::printf("--- N2 own ground, every kind ---\n");
    {
        int foreign_kinds = 0, foreign_ok = 0, unowned_ok = 0, unowned_tried = 0;
        std::vector<std::string> covered;
        for (const kind& k : kinds)
        {
            bool done = false;
            for (const entity_id t : tiles)
            {
                const entity_id owner = holder_of(w, t);
                if (owner == null_entity || !place_check(w, reg, t, k, owner)) continue;
                // Another nation that is NOT tech-locked for the kind, so the ground is
                // the only reason left to refuse (the tech gate runs first).
                entity_id other = null_entity;
                placement_rules::placement_result r = placement_rules::placement_reason::ok;
                for (const entity_id n : nations)
                {
                    if (n == owner) continue;
                    r = place_check(w, reg, t, k, n);
                    if (r.reason != placement_rules::placement_reason::tech_locked) { other = n; break; }
                }
                if (other == null_entity) continue;
                const std::uint64_t before = w.state_hash(0);
                entity_id b = null_entity;
                const construction_result cr =
                    construct_nation(w, reg, other, t, k.type, k.target,
                                     k.type == building_type::processing_facility ? k.recipe : no_recipe, b);
                const bool unchanged = w.state_hash(0) == before && b == null_entity;
                ++foreign_kinds;
                const bool ok = refused_for_ground(r) && cr == construction_result::foreign_ground && unchanged;
                if (ok) ++foreign_ok;
                if (std::find(covered.begin(), covered.end(), k.name) == covered.end())
                {
                    covered.push_back(k.name);
                    check(ok, "N2", fmt("a %s placeable for its owner on tile %llu is refused to nation "
                                        "%llu (reason %d, door %d, world unchanged %d)", k.name,
                                        static_cast<unsigned long long>(t),
                                        static_cast<unsigned long long>(other),
                                        static_cast<int>(r.reason), static_cast<int>(cr), unchanged ? 1 : 0));
                }
                done = true;
                break;
            }
            (void)done;
        }
        std::printf("  foreign refusals: %d of %d kinds tried\n", foreign_ok, foreign_kinds);
        for (const char* need : {"extraction site", "processing facility", "logistics hub", "port"})
            if (std::find(covered.begin(), covered.end(), need) == covered.end())
                check(false, "N2", fmt("no %s is placeable for its owner anywhere on this seed", need));
        for (const kind& k : kinds)
            if (k.type != building_type::extraction_site
                && std::find(covered.begin(), covered.end(), k.name) == covered.end())
                finding("N2", fmt("no %s is placeable for its owner on this seed (not required)", k.name));

        // Unowned land: refused to every nation for the ground. The own-ground gate
        // runs straight after the tile-level and tech gates and before every reach or
        // road test, so on a tile whose TERRAIN admits the kind, a nation's answer is
        // `not_own_ground` (or `tech_locked`, which is not this row's subject) - an
        // unowned tile is out of every network's reach, so asking the full check with
        // no builder would never find a fixture.
        for (const entity_id t : tiles)
        {
            if (holder_of(w, t) != null_entity) continue;
            const auto tit = w.tiles.find(t);
            if (placement_rules::is_water_tile(tit->second.substrate)) continue;
            for (const kind& k : kinds)
            {
                if (!placement_rules::can_place(tit->second, k.type, k.target)) continue;
                ++unowned_tried;
                bool all = true;
                int asked = 0;
                for (const entity_id n : nations)
                {
                    const placement_rules::placement_result r = place_check(w, reg, t, k, n);
                    if (r.reason == placement_rules::placement_reason::tech_locked) continue;
                    ++asked;
                    all = all && r.reason == placement_rules::placement_reason::not_own_ground;
                }
                if (asked == 0) { --unowned_tried; continue; }
                {
                    const std::uint64_t before = w.state_hash(0);
                    entity_id b = null_entity;
                    const construction_result cr = construct_nation(
                        w, reg, nations.front(), t, k.type, k.target,
                        k.type == building_type::processing_facility ? k.recipe : no_recipe, b);
                    all = all && cr != construction_result::placed && b == null_entity
                        && w.state_hash(0) == before;
                }
                if (all) ++unowned_ok;
                break;
            }
            if (unowned_tried >= 50) break;
        }
        if (unowned_tried == 0)
            check(false, "N2", "no unowned land tile admits any kind on this seed");
        else
            check(unowned_ok == unowned_tried, "N2",
                  fmt("unowned land is refused to every nation (%d of %d tiles)", unowned_ok, unowned_tried));
    }

    // ---- N3: roads, one crossing in -----------------------------------------
    std::printf("--- N3 roads: one crossing in ---\n");
    {
        bool into_tested = false, from_tested = false;
        for (const entity_id t : tiles)
        {
            const entity_id owner = holder_of(w, t);
            if (owner == null_entity) continue;
            const tile_component& tc = w.tiles.at(t);
            if (max_road_tier(tc) == 0) continue;
            for (int side = 0; side < 6; ++side)
            {
                const entity_id nb = road_edge_neighbour(w, t, side);
                if (nb == null_entity || holder_of(w, nb) == owner) continue;
                const placement_rules::placement_result in =
                    placement_rules::can_place_road(w, t, side, 3, owner);
                // Only a crossing some other rule does not already refuse proves the
                // border half: a water end or an existing road says nothing about it.
                if (!into_tested && (in.reason == placement_rules::placement_reason::ok
                                     || in.reason == placement_rules::placement_reason::foreign_ground))
                {
                    into_tested = true;
                    check(in.reason == placement_rules::placement_reason::ok, "N3",
                          fmt("from its own roaded tile %llu, nation %llu may lay a crossing INTO "
                              "tile %llu, which it does not hold (reason %d)",
                              static_cast<unsigned long long>(t), static_cast<unsigned long long>(owner),
                              static_cast<unsigned long long>(nb), static_cast<int>(in.reason)));
                }
                if (!into_tested) continue;
                // From the neighbour's side: the same owner may not build FROM ground it
                // does not hold - asked on any roaded tile it does not hold.
                if (!from_tested)
                {
                    for (const entity_id f : tiles)
                    {
                        if (holder_of(w, f) == owner || max_road_tier(w.tiles.at(f)) == 0) continue;
                        for (int fs = 0; fs < 6 && !from_tested; ++fs)
                        {
                            const placement_rules::placement_result fr =
                                placement_rules::can_place_road(w, f, fs, 3, owner);
                            if (fr.reason == placement_rules::placement_reason::ocean
                                || fr.reason == placement_rules::placement_reason::already_road)
                                continue;
                            from_tested = true;
                            check(fr.reason == placement_rules::placement_reason::foreign_ground, "N3",
                                  fmt("nation %llu may not lay a road from tile %llu, which it does not "
                                      "hold (reason %d)", static_cast<unsigned long long>(owner),
                                      static_cast<unsigned long long>(f), static_cast<int>(fr.reason)));
                        }
                        if (from_tested) break;
                    }
                }
                break;
            }
            if (into_tested && from_tested) break;
        }
        if (!into_tested) check(false, "N3", "no roaded tile borders ground its nation does not hold");
        if (!from_tested) check(false, "N3", "no roaded tile outside a nation's ground to test from");
    }

    run_port_rows(w, reg, nations);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    if (seeds.empty())
        seeds.push_back(0);

    lua_state lua;
    for (const std::uint32_t s : seeds)
        run_seed(lua, s);

    std::printf("\nnation_actor_seam: %d pass, %d fail, %d finding(s) -- %s\n", g_pass, g_fail,
                g_finding, g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
