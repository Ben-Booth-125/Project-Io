// Headless harness: the corp standing profile (reach / capital / market share).
//
// Re-specified by BL-633 (retire standing bands). It LOSES its band-boundary assertions — the
// five bands (negligible / minor / notable / major / dominant) are retired, Ben 2026-08-26:
// "We don't need company information to be invisible" — and GAINS the disclosure gate in their
// place. What it covers now:
//   - R1: the bands are gone. Enforced by construction: the enum, its three banding functions
//     and their boundary constants no longer exist, so any file still naming one would fail to
//     compile, and the shape tripwire below fails if a banded field creeps back in.
//   - BL-1407: the class-based gate below RETIRED with ownership_class; a nation reads only its own
//     books, so capital_disclosed is TRUE for the observer's own row alone.
//   - (retired) The disclosure gate (FINANCE.md § Disclosure): capital_disclosed is TRUE iff the firm's
//     ownership_class is publicly_held. It is a fact about the FIRM, never about the reader —
//     tested by giving the player's own corp each class in turn and getting the firm's answer.
//   - The public axes (DISCOVERY.md § Competitor visibility): reach and market share are exact
//     for every corporation, player and rival alike, at every ownership class.
//   - R3: the operational fog is untouched. corp_standing carries the three accounting axes and
//     nothing else — no production rate, no stockpile quantity, no recipe, no workforce dial.
//     An open book tells you what a firm earned, never how it operates.
//   - market_share: a zero-total cash-flow map produces 0.0 for every corp, never NaN/inf.
//   - Determinism: compute_corp_standings run twice on the same world/cash-flow is identical.
//
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

// R3 tripwire. corp_standing is an ACCOUNTING record: an id, a player flag, the three figures a
// filed return could carry, and the one disclosure bit that gates the filed one. Any field added
// to it — a production rate, a stockpile quantity, an assigned recipe, a workforce dial — grows
// the struct and trips this. It is a tripwire, not a proof: it says "the shape changed, come and
// justify it", which is exactly the review moment the operational fog needs.
struct expected_standing_shape
{
    entity_id corp;
    bool      is_player;
    int       reach_bodies;
    float     capital_balance;
    float     market_share;
    bool      capital_disclosed;
};

} // namespace

int main()
{
    std::printf("standing_harness (corp standing profile — BL-633 disclosure gate)\n");

    // =====================================================================
    // R3 — the shape of the record. No operational quantity may appear here.
    // =====================================================================
    {
        check(sizeof(corp_standing) == sizeof(expected_standing_shape),
              "R3: corp_standing carries the accounting axes and nothing more "
              "(no production/stockpile/recipe/workforce field)");
    }

    // BL-1407: the disclosure gate in isolation (`corp_files_return` over the
    // ownership class) RETIRED with the class. Every owner is a nation, and a
    // nation reads only its OWN books (FINANCE.md § Disclosure; NR-662).

    // =====================================================================
    // compute_corp_standings — 4-nation world (BL-1407: the owners are nations).
    //   player   : reads its own books
    //   rival_a  : negative treasury, books its own
    //   rival_b  : large treasury, no buildings
    //   rival_c  : one building on Beta
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
            (void)is_player;
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
            nation_component cc{};
            cc.name            = name;
            cc.treasury         = cash;
            cc.state_assets          = assets;
            w.nations[corp] = cc;
            return corp;
        };

        const entity_id player_corp = make_corp(true,  "Player Co", 25000.0f, {tile_a, tile_b});
        const entity_id rival_a     = make_corp(false, "Rival A",   -50.0f,   {tile_a});
        const entity_id rival_b     = make_corp(false, "Rival B",   60000.0f, {});
        const entity_id rival_c     = make_corp(false, "Rival C",   1000.0f,  {tile_b});
        w.player_entity = player_corp;

        std::unordered_map<entity_id, corp_cash_flow> flows;
        flows[player_corp] = corp_cash_flow{ /*income*/ 30.0f, /*expenditure*/ 5.0f };
        flows[rival_a]      = corp_cash_flow{ /*income*/ 10.0f, /*expenditure*/ 1.0f };
        // rival_b and rival_c have no cash-flow entry this tick (no trade) — must not crash.

        const std::vector<corp_standing> a  = compute_corp_standings(w, flows);
        const std::vector<corp_standing> b2 = compute_corp_standings(w, flows);

        // BL-1407: the ranked owners are the nations HOLDING a building
        // (economic_owner_ids); rival_b holds none, so it has no row.
        check(a.size() == 3, "compute_corp_standings: one entry per nation holding a building (rival_b holds none)");

        int player_count = 0;
        for (const corp_standing& cs : a)
            if (cs.is_player) ++player_count;
        check(player_count == 1, "compute_corp_standings: exactly one is_player entry");

        // Determinism: identical across two calls (compare field-by-field, sorted by entity_id
        // already since compute_corp_standings walks a sorted id list).
        bool identical = (a.size() == b2.size());
        for (std::size_t i = 0; identical && i < a.size(); ++i)
        {
            const corp_standing& x = a[i];
            const corp_standing& y = b2[i];
            identical = identical && x.corp == y.corp && x.is_player == y.is_player &&
                        x.reach_bodies == y.reach_bodies &&
                        x.capital_balance == y.capital_balance &&
                        x.market_share == y.market_share &&
                        x.capital_disclosed == y.capital_disclosed;
        }
        check(identical, "compute_corp_standings: two calls on the same world/cash-flow are identical");

        const corp_standing* p  = nullptr;
        const corp_standing* ra = nullptr;
        const corp_standing* rb = nullptr;
        const corp_standing* rc = nullptr;
        for (const corp_standing& cs : a)
        {
            if (cs.corp == player_corp) p  = &cs;
            if (cs.corp == rival_a)     ra = &cs;
            if (cs.corp == rival_b)     rb = &cs;
            if (cs.corp == rival_c)     rc = &cs;
        }
        check(p && ra && !rb && rc, "compute_corp_standings: the three holders found; the holderless rival_b absent");

        // -----------------------------------------------------------------
        // The public axes — exact for EVERY corp, player and rival alike, at
        // whatever the books' disclosure. This is what retiring the bands bought.
        // -----------------------------------------------------------------
        if (p)
        {
            check(p->reach_bodies == 2, "player: reach_bodies == 2 (two distinct bodies)");
            // total income this tick = 30 (player) + 10 (rival_a) + 0 + 0 = 40.
            check(std::fabs(p->market_share - 0.75f) < 1e-6f, "player: market_share == 0.75");
            check(p->capital_disclosed, "player: capital_disclosed == true (its own books)");
            check(std::fabs(p->capital_balance - 25000.0f) < 1e-6f, "player: capital_balance == 25000");
        }
        if (ra)
        {
            check(ra->reach_bodies == 1, "rival_a: reach_bodies == 1 — PUBLIC axis, printed anyway");
            check(std::fabs(ra->market_share - 0.25f) < 1e-6f,
                  "rival_a: market_share == 0.25 — PUBLIC axis, printed anyway");
            check(!ra->capital_disclosed, "rival_a: capital_disclosed == false (another nation's books; a dash on screen)");
        }
        if (rb)
        {
            check(rb->reach_bodies == 0, "rival_b: reach_bodies == 0 (no assets)");
            check(rb->market_share == 0.0f, "rival_b: market_share == 0.0 (no cash-flow entry this tick)");
            check(!std::isnan(rb->market_share), "rival_b: market_share is not NaN");
            check(!rb->capital_disclosed, "rival_b: capital_disclosed == false (a dash on screen)");
        }
        if (rc)
        {
            check(rc->reach_bodies == 1, "rival_c: reach_bodies == 1");
            check(!rc->capital_disclosed, "rival_c: capital_disclosed == false (no class makes a rival's books public)");
        }

        // BL-1407: "the gate follows the FIRM's class" RETIRED with the class.
    }

    // =====================================================================
    // Zero-total cash flow — every corp's market_share must be 0, never NaN.
    // =====================================================================
    {
        world w;
        const entity_id c1 = w.create_entity();
        const entity_id c2 = w.create_entity();
        nation_component cc1{}; cc1.name = "Zero A"; cc1.treasury = 100.0f;
        nation_component cc2{}; cc2.name = "Zero B"; cc2.treasury = 200.0f;
        // BL-1407: a standing row needs a holding (economic_owner_ids).
        for (nation_component* nc : { &cc1, &cc2 })
        {
            const entity_id bld = w.create_entity();
            w.buildings[bld] = building_component{};
            nc->state_assets.push_back(bld);
        }
        w.nations[c1] = cc1;
        w.nations[c2] = cc2;
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

        // BL-1407: a RIVAL nation's books are never disclosed; the observer's own are
        // (NR-662). This fixture sets w.player_entity = c1.
        bool defaults_closed = true;
        bool own_books_readable = false;
        for (const corp_standing& cs : out)
        {
            if (cs.is_player) { own_books_readable = cs.capital_disclosed; continue; }
            if (cs.capital_disclosed) defaults_closed = false;
        }
        check(defaults_closed,
              "a RIVAL nation's capital is not disclosed");
        check(own_books_readable,
              "and the observer reads its own books (NR-662)");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
