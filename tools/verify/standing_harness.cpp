// Headless harness (BL-262 first slice): the corp standing profile (reach / capital / market
// share bands). Covers:
//   - Boundary determinism: each banding function lands the documented boundary value in the
//     HIGHER band, per standing.hpp's ">=" rule.
//   - market_share: a zero-total cash-flow map produces 0.0 for every corp, never NaN/inf.
//   - compute_corp_standings: run twice on the same world/cash-flow is byte-identical
//     (determinism); exactly one entry has is_player == true.
// Hand-builds a minimal world (no Lua / SDL / ImGui); kept outside src/ so the CMake glob
// ignores it. Follows the corp_ai_harness.cpp pattern.

#include "world/components.hpp"
#include "world/market_clearing.hpp"
#include "world/standing.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <unordered_map>
#include <vector>

namespace {

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

} // namespace

int main()
{
    std::printf("standing_harness (BL-262 corp standing profile)\n");

    // =====================================================================
    // band_reach boundaries
    // =====================================================================
    {
        check(band_reach(0) == standing_band::negligible, "band_reach(0) == negligible");
        check(band_reach(1) == standing_band::minor,      "band_reach(1) == minor");
        check(band_reach(2) == standing_band::notable,    "band_reach(2) == notable");
        check(band_reach(3) == standing_band::major,      "band_reach(3) == major");
        check(band_reach(4) == standing_band::dominant,   "band_reach(4) == dominant");
        check(band_reach(9) == standing_band::dominant,   "band_reach(9) == dominant (4+)");
    }

    // =====================================================================
    // band_capital boundaries — a value exactly ON a boundary lands HIGHER.
    // =====================================================================
    {
        check(band_capital(-1.0f)      == standing_band::negligible, "band_capital(-1) == negligible");
        check(band_capital(0.0f)       == standing_band::minor,      "band_capital(0) == minor (boundary->higher)");
        check(band_capital(4999.99f)   == standing_band::minor,      "band_capital(4999.99) == minor");
        check(band_capital(5000.0f)    == standing_band::notable,    "band_capital(5000) == notable (boundary->higher)");
        check(band_capital(19999.99f)  == standing_band::notable,    "band_capital(19999.99) == notable");
        check(band_capital(20000.0f)   == standing_band::major,      "band_capital(20000) == major (boundary->higher)");
        check(band_capital(49999.99f)  == standing_band::major,      "band_capital(49999.99) == major");
        check(band_capital(50000.0f)   == standing_band::dominant,   "band_capital(50000) == dominant (boundary->higher)");
    }

    // =====================================================================
    // band_market_share boundaries — same ">=" rule.
    // =====================================================================
    {
        check(band_market_share(0.0f)   == standing_band::negligible, "band_market_share(0.0) == negligible");
        check(band_market_share(0.0499f)== standing_band::negligible, "band_market_share(0.0499) == negligible");
        check(band_market_share(0.05f)  == standing_band::minor,      "band_market_share(0.05) == minor (boundary->higher)");
        check(band_market_share(0.1499f)== standing_band::minor,      "band_market_share(0.1499) == minor");
        check(band_market_share(0.15f)  == standing_band::notable,    "band_market_share(0.15) == notable (boundary->higher)");
        check(band_market_share(0.2999f)== standing_band::notable,    "band_market_share(0.2999) == notable");
        check(band_market_share(0.30f)  == standing_band::major,      "band_market_share(0.30) == major (boundary->higher)");
        check(band_market_share(0.4999f)== standing_band::major,      "band_market_share(0.4999) == major");
        check(band_market_share(0.50f)  == standing_band::dominant,   "band_market_share(0.50) == dominant (boundary->higher)");
        check(band_market_share(1.0f)   == standing_band::dominant,   "band_market_share(1.0) == dominant");
    }

    // =====================================================================
    // compute_corp_standings — small 3-corp world (player + two rivals).
    // =====================================================================
    {
        world w;

        const entity_id body_a = w.create_entity();
        {
            body_component b{};
            b.name = "Alpha";
            b.type = body_type::planet;
            w.bodies[body_a] = b;
        }
        const entity_id body_b = w.create_entity();
        {
            body_component b{};
            b.name = "Beta";
            b.type = body_type::planet;
            w.bodies[body_b] = b;
        }

        const entity_id tile_a = w.create_entity();
        {
            tile_component tc{};
            tc.body = body_a;
            tc.substrate = terrain_substrate::rocky;
            tc.landform    = terrain_landform::plains;
            w.tiles[tile_a] = tc;
        }
        const entity_id tile_b = w.create_entity();
        {
            tile_component tc{};
            tc.body = body_b;
            tc.substrate = terrain_substrate::rocky;
            tc.landform    = terrain_landform::plains;
            w.tiles[tile_b] = tc;
        }

        auto make_corp = [&](bool is_player, const char* name, float cash,
                             std::vector<entity_id> tiles) -> entity_id {
            std::vector<entity_id> assets;
            for (entity_id t : tiles)
            {
                const entity_id bld = w.create_entity();
                building_component b{};
                b.tile = t;
                b.type = building_type::extraction_site;
                w.buildings[bld] = b;
                assets.push_back(bld);
            }
            const entity_id corp = w.create_entity();
            corporation_component cc;
            cc.name      = name;
            cc.is_player = is_player;
            cc.balance   = cash;
            cc.assets    = assets;
            w.corporations[corp] = cc;
            return corp;
        };

        const entity_id player_corp = make_corp(true,  "Player Co", 25000.0f, {tile_a, tile_b});
        const entity_id rival_a     = make_corp(false, "Rival A",   -50.0f,   {tile_a});
        const entity_id rival_b     = make_corp(false, "Rival B",   60000.0f, {});
        w.player_entity = player_corp;

        std::unordered_map<entity_id, corp_cash_flow> flows;
        flows[player_corp] = corp_cash_flow{ /*income*/ 30.0f, /*expenditure*/ 5.0f };
        flows[rival_a]      = corp_cash_flow{ /*income*/ 10.0f, /*expenditure*/ 1.0f };
        // rival_b has no cash-flow entry this tick (no trade) — must not crash.

        const std::vector<corp_standing> a = compute_corp_standings(w, flows);
        const std::vector<corp_standing> b2 = compute_corp_standings(w, flows);

        check(a.size() == 3, "compute_corp_standings: one entry per corp");

        int player_count = 0;
        for (const corp_standing& cs : a)
            if (cs.is_player) ++player_count;
        check(player_count == 1, "compute_corp_standings: exactly one is_player entry");

        // Determinism: byte-identical across two calls (compare field-by-field, sorted by
        // entity_id already since compute_corp_standings walks a sorted id list).
        bool identical = (a.size() == b2.size());
        for (std::size_t i = 0; identical && i < a.size(); ++i)
        {
            const corp_standing& x = a[i];
            const corp_standing& y = b2[i];
            identical = identical && x.corp == y.corp && x.is_player == y.is_player &&
                        x.reach_bodies == y.reach_bodies &&
                        x.capital_balance == y.capital_balance &&
                        x.market_share == y.market_share &&
                        x.reach_band == y.reach_band &&
                        x.capital_band == y.capital_band &&
                        x.share_band == y.share_band;
        }
        check(identical, "compute_corp_standings: two calls on the same world/cash-flow are byte-identical");

        // Find each corp's row and check the honest exact figures.
        const corp_standing* p = nullptr;
        const corp_standing* ra = nullptr;
        const corp_standing* rb = nullptr;
        for (const corp_standing& cs : a)
        {
            if (cs.corp == player_corp) p = &cs;
            if (cs.corp == rival_a)     ra = &cs;
            if (cs.corp == rival_b)     rb = &cs;
        }
        check(p && ra && rb, "compute_corp_standings: all three corps found in output");

        if (p)
        {
            check(p->reach_bodies == 2, "player: reach_bodies == 2 (two distinct bodies)");
            check(p->reach_band == standing_band::notable, "player: reach_band == notable (2 bodies)");
            check(std::fabs(p->capital_balance - 25000.0f) < 1e-6f, "player: capital_balance == 25000");
            check(p->capital_band == standing_band::major, "player: capital_band == major");
            // total income this tick = 30 (player) + 10 (rival_a) + 0 (rival_b) = 40.
            check(std::fabs(p->market_share - (30.0f / 40.0f)) < 1e-6f, "player: market_share == 0.75");
            check(p->share_band == standing_band::dominant, "player: share_band == dominant");
        }
        if (ra)
        {
            check(ra->reach_bodies == 1, "rival_a: reach_bodies == 1");
            check(ra->capital_band == standing_band::negligible, "rival_a: capital_band == negligible (negative balance)");
            check(std::fabs(ra->market_share - 0.25f) < 1e-6f, "rival_a: market_share == 0.25");
        }
        if (rb)
        {
            check(rb->reach_bodies == 0, "rival_b: reach_bodies == 0 (no assets)");
            check(rb->reach_band == standing_band::negligible, "rival_b: reach_band == negligible");
            check(rb->capital_band == standing_band::dominant, "rival_b: capital_band == dominant (60000)");
            check(rb->market_share == 0.0f, "rival_b: market_share == 0.0 (no cash-flow entry this tick)");
            check(!std::isnan(rb->market_share), "rival_b: market_share is not NaN");
        }
    }

    // =====================================================================
    // Zero-total cash flow — every corp's market_share must be 0, never NaN.
    // =====================================================================
    {
        world w;
        const entity_id c1 = w.create_entity();
        const entity_id c2 = w.create_entity();
        corporation_component cc1; cc1.name = "Zero A"; cc1.balance = 100.0f;
        corporation_component cc2; cc2.name = "Zero B"; cc2.balance = 200.0f;
        w.corporations[c1] = cc1;
        w.corporations[c2] = cc2;
        w.player_entity = c1;

        std::unordered_map<entity_id, corp_cash_flow> zero_flows;
        zero_flows[c1] = corp_cash_flow{ 0.0f, 0.0f };
        zero_flows[c2] = corp_cash_flow{ 0.0f, 0.0f };

        const std::vector<corp_standing> out = compute_corp_standings(w, zero_flows);
        bool all_zero_no_nan = true;
        for (const corp_standing& cs : out)
        {
            if (cs.market_share != 0.0f || std::isnan(cs.market_share))
                all_zero_no_nan = false;
        }
        check(all_zero_no_nan,
              "compute_corp_standings: zero-total cash flow => market_share == 0.0 for every corp, no NaN");

        // Also cover the empty-map case explicitly (no cash_flow entries at all).
        const std::unordered_map<entity_id, corp_cash_flow> empty_flows;
        const std::vector<corp_standing> out2 = compute_corp_standings(w, empty_flows);
        bool empty_ok = true;
        for (const corp_standing& cs : out2)
            if (cs.market_share != 0.0f || std::isnan(cs.market_share))
                empty_ok = false;
        check(empty_ok, "compute_corp_standings: empty cash-flow map => market_share == 0.0 for every corp");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
