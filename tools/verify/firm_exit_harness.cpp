// firm_exit_harness — BL-743. The insolvency wind-up, pinned.
//
// What run_firm_exits promises (corp_command.hpp § BL-743):
//   F1  the trigger: a corp whose last N FILED returns all closed below the
//       floor is erased; its buildings demolish, its pools dump to the local
//       market's REAL inventory (conservation: inventory gains what pools
//       lose), its units disband.
//   F2  the player is exempt ABSOLUTELY — same returns, is_player, survives.
//   F3  an (N-1)-quarter streak survives; one solvent quarter inside the
//       window resets the sentence.
//   F4  THE RE-WALK: after an exit, no store the dissolution table names
//       still holds the id — the buyout harness's discipline, applied to the
//       no-heir path. A checklist that walked the implementation's own list
//       would pass whenever the implementation is self-consistently wrong, so
//       this walks the WORLD.
//   F5  inert params (the C++ defaults) wind up nothing on the same fixture —
//       byte-identical world, the standing inertness discipline.
//   F6  determinism: two identical fixtures exit identically.
//
// Fixture-only (hand-built world, params passed directly) — the trigger and
// the wind-up are pure world arithmetic; the LIVE values are economy.lua's
// and the campaign_lapse cells measure their consequence.
//
// Exits 0 on PASS, non-zero on any failure.

#include "world/construction.hpp"
#include "world/corp_command.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* row, const char* what)
{
    std::printf("  [%s] %-3s %s\n", ok ? "PASS" : "FAIL", row, what);
    if (ok) ++g_pass; else ++g_fail;
}

constexpr std::size_t k_stone = static_cast<std::size_t>(resource_type::stone);

struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id market = null_entity;
    entity_id doomed = null_entity;   ///< the insolvent corp
    entity_id healthy = null_entity;  ///< a solvent bystander
    entity_id tile_a = null_entity;
    entity_id doomed_building = null_entity;
    entity_id doomed_unit     = null_entity;
};

/// One body, one market, one doomed corp (a building, a pool of 40 stone, a
/// unit, a sell order, a hostile pair with the bystander, a sentiment row),
/// one healthy bystander. The doomed corp files `quarters` returns at
/// `filed_balance`.
fixture make_fixture(int quarters, float filed_balance)
{
    fixture f;
    f.body = f.w.create_entity();
    f.w.bodies[f.body] = body_component{};

    f.market = f.w.create_entity();
    {
        market_component mc{};
        mc.body = f.body;
        mc.base_price[k_stone] = 1.0f;
        f.w.markets[f.market] = mc;
    }

    f.tile_a = f.w.create_entity();
    {
        tile_component tc{};
        tc.body = f.body;
        f.w.tiles[f.tile_a] = tc;
    }

    auto add_corp = [&](const char* name, bool player) {
        const entity_id c = f.w.create_entity();
        corporation_component cc{};
        cc.name      = name;
        cc.is_player = player;
        f.w.corporations[c] = cc;
        return c;
    };
    f.doomed  = add_corp("Sunk Ventures", false);
    f.healthy = add_corp("Solvent & Sons", false);

    // The doomed corp's estate.
    f.doomed_building = f.w.create_entity();
    {
        building_component bc{};
        bc.tile = f.tile_a;
        bc.type = building_type::extraction_site;
        f.w.buildings[f.doomed_building] = bc;
        f.w.corporations.at(f.doomed).assets.push_back(f.doomed_building);
    }
    f.w.corp_body_pools[std::make_pair(f.doomed, f.body)].quantities[k_stone] = 40.0f;
    {
        f.doomed_unit = f.w.create_entity();
        unit_component uc{};
        uc.owner = f.doomed;
        f.w.units[f.doomed_unit] = uc;
    }
    {
        sell_order o{};
        o.id       = 7;
        o.corp     = f.doomed;
        o.body     = f.body;
        o.resource = resource_type::stone;
        o.quantity = 5.0f;
        f.w.sell_orders.push_back(o);
    }
    f.w.corp_hostile_pairs.insert(std::minmax(f.doomed, f.healthy));
    f.w.sentiment.pairs[{f.doomed, f.healthy}] = sentiment_value{};

    // The filing history that seals its fate.
    f.w.corporations.at(f.doomed).balance = filed_balance;
    for (int i = 0; i < quarters; ++i)
    {
        quarterly_return r{};
        r.balance = filed_balance;
        f.w.corporations.at(f.doomed).returns.push_back(r);
    }
    // The bystander files healthy quarters.
    for (int i = 0; i < quarters; ++i)
    {
        quarterly_return r{};
        r.balance = 500.0f;
        f.w.corporations.at(f.healthy).returns.push_back(r);
    }
    return f;
}

/// F4's re-walk: every store the dissolution table names, checked for the id.
bool world_holds(const world& w, entity_id id)
{
    if (w.corporations.count(id)) return true;
    for (const auto& kv : w.buildings)
        (void)kv; // buildings carry no corp field; ownership is the asset list
    for (const auto& kv : w.corp_body_pools)
        if (kv.first.first == id) return true;
    for (const auto& kv : w.units)
        if (kv.second.owner == id) return true;
    for (const auto& c : w.convoys)
        if (c.corp == id) return true;
    for (const auto& r : w.trade_routes)
        if (r.corp == id) return true;
    for (const auto& o : w.sell_orders)
        if (o.corp == id) return true;
    for (const auto& o : w.buy_orders)
        if (o.corp == id) return true;
    for (const auto& q : w.procurement_quotes)
        if (q.buyer == id || q.supplier == id) return true;
    for (const auto& c : w.procurement_contracts)
        if (c.buyer == id || c.supplier == id) return true;
    for (const auto& b : w.battles)
        if (b.attacker == id || b.defender == id) return true;
    for (const auto& pr : w.corp_hostile_pairs)
        if (pr.first == id || pr.second == id) return true;
    for (const auto& pr : w.corp_friend_pairs)
        if (pr.first == id || pr.second == id) return true;
    for (const auto& pr : w.corp_friend_offers)
        if (pr.first == id || pr.second == id) return true;
    for (const auto& kv : w.sentiment.pairs)
        if (kv.first.first == id || kv.first.second == id) return true;
    if (w.corp_embargo_conditions.count(id)) return true;
    if (w.earned_techs.count(id)) return true;
    if (w.corp_modifiers.count(id)) return true;
    for (const auto& kv : w.workforce_supply_overrides)
        if (kv.first.first == id) return true;
    return false;
}

} // namespace

int main()
{
    std::printf("firm_exit_harness — BL-743, the insolvency wind-up\n\n");

    firm_exit_params p;
    p.balance_floor        = -100.0f;
    p.consecutive_quarters = 4;

    // F1: the trigger fires and the estate liquidates.
    {
        fixture f = make_fixture(4, -500.0f);
        std::vector<firm_exit_record> out;
        run_firm_exits(f.w, p, &out);
        check(out.size() == 1 && out[0].corp == f.doomed &&
              out[0].holdings == 1 && out[0].units == 1 &&
              f.w.corporations.count(f.doomed) == 0 &&
              f.w.corporations.count(f.healthy) == 1 &&
              f.w.buildings.count(f.doomed_building) == 0 &&
              f.w.units.count(f.doomed_unit) == 0,
              "F1", "four filed quarters below the floor: the firm is erased, "
                    "its building demolished, its unit disbanded");
        check(f.w.markets.at(f.market).inventory[k_stone] == 40.0f &&
              f.w.corp_body_pools.count(std::make_pair(f.doomed, f.body)) == 0,
              "F1b", "...and its pool lands WHOLE in the local market's real "
                     "inventory - the conservation law, not a vanishing");
    }

    // F2: the player is exempt absolutely.
    {
        fixture f = make_fixture(4, -500.0f);
        f.w.corporations.at(f.doomed).is_player = true;
        f.w.player_entity = f.doomed;
        std::vector<firm_exit_record> out;
        run_firm_exits(f.w, p, &out);
        check(out.empty() && f.w.corporations.count(f.doomed) == 1,
              "F2", "the player's corp files the same failing quarters and "
                    "SURVIVES - the never-erase-the-seat ruling");
    }

    // F3: a shorter streak, or one solvent quarter inside the window, survives.
    {
        fixture f = make_fixture(3, -500.0f); // N-1 quarters
        std::vector<firm_exit_record> out;
        run_firm_exits(f.w, p, &out);
        const bool short_streak_ok = out.empty() && f.w.corporations.count(f.doomed) == 1;

        fixture g = make_fixture(4, -500.0f);
        g.w.corporations.at(g.doomed).returns[2].balance = 50.0f; // one good quarter
        std::vector<firm_exit_record> out2;
        run_firm_exits(g.w, p, &out2);
        check(short_streak_ok && out2.empty() && g.w.corporations.count(g.doomed) == 1,
              "F3", "a streak one quarter short survives, and one solvent "
                    "quarter inside the window resets the sentence");
    }

    // F4: the re-walk — no store still holds the erased id.
    {
        fixture f = make_fixture(4, -500.0f);
        std::vector<firm_exit_record> out;
        run_firm_exits(f.w, p, &out);
        check(out.size() == 1 && !world_holds(f.w, f.doomed),
              "F4", "the re-walk: after the wind-up, no store the dissolution "
                    "table names still holds the erased id");
    }

    // F5: inert params wind up nothing — the C++ defaults are the off switch.
    {
        fixture f = make_fixture(4, -500.0f);
        std::vector<firm_exit_record> out;
        run_firm_exits(f.w, firm_exit_params{}, &out);
        check(out.empty() && f.w.corporations.count(f.doomed) == 1 &&
              f.w.corp_body_pools.count(std::make_pair(f.doomed, f.body)) == 1 &&
              f.w.markets.at(f.market).inventory[k_stone] == 0.0f,
              "F5", "inert params (the unloaded-registry defaults) touch "
                    "nothing - the standing inertness discipline");
    }

    // F6: determinism — two identical fixtures exit identically.
    {
        fixture a = make_fixture(4, -500.0f);
        fixture b = make_fixture(4, -500.0f);
        std::vector<firm_exit_record> oa, ob;
        run_firm_exits(a.w, p, &oa);
        run_firm_exits(b.w, p, &ob);
        check(oa.size() == ob.size() && oa.size() == 1 &&
              oa[0].corp == ob[0].corp && oa[0].holdings == ob[0].holdings &&
              oa[0].units == ob[0].units && oa[0].balance == ob[0].balance &&
              a.w.markets.at(a.market).inventory[k_stone]
                  == b.w.markets.at(b.market).inventory[k_stone],
              "F6", "two runs of one fixture exit identically, records included");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
