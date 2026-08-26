// Headless harness: the corp standing profile (reach / capital / market share).
//
// Re-specified by BL-633 (retire standing bands). It LOSES its band-boundary assertions — the
// five bands (negligible / minor / notable / major / dominant) are retired, Ben 2026-08-26:
// "We don't need company information to be invisible" — and GAINS the disclosure gate in their
// place. What it covers now:
//   - R1: the bands are gone. Enforced by construction: the enum, its three banding functions
//     and their boundary constants no longer exist, so any file still naming one would fail to
//     compile, and the shape tripwire below fails if a banded field creeps back in.
//   - The disclosure gate (FINANCE.md § Disclosure): capital_disclosed is TRUE iff the firm's
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

    // =====================================================================
    // The disclosure gate in isolation — binary, no graded middle.
    // =====================================================================
    {
        check(corp_files_return(ownership_class::publicly_held),
              "corp_files_return(public) == true (a public firm files)");
        check(!corp_files_return(ownership_class::privately_held),
              "corp_files_return(private) == false (its books are its own)");
        check(!corp_files_return(ownership_class::closed),
              "corp_files_return(closed) == false (no filing at all)");
    }

    // =====================================================================
    // compute_corp_standings — 4-corp world, one per disclosure case.
    //   player   : public   (files)
    //   rival_a  : private  (does not file, negative balance)
    //   rival_b  : closed   (does not file, large balance)
    //   rival_c  : public   (files — a RIVAL whose capital is readable exactly,
    //                        which the retired banding could never show)
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

        auto make_corp = [&](bool is_player, const char* name, float cash, ownership_class oc,
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
            cc.name            = name;
            cc.is_player       = is_player;
            cc.balance         = cash;
            cc.assets          = assets;
            cc.ownership_class = oc;
            w.corporations[corp] = cc;
            return corp;
        };

        const entity_id player_corp = make_corp(true,  "Player Co", 25000.0f,
                                                ownership_class::publicly_held, {tile_a, tile_b});
        const entity_id rival_a     = make_corp(false, "Rival A",   -50.0f,
                                                ownership_class::privately_held, {tile_a});
        const entity_id rival_b     = make_corp(false, "Rival B",   60000.0f,
                                                ownership_class::closed, {});
        const entity_id rival_c     = make_corp(false, "Rival C",   1000.0f,
                                                ownership_class::publicly_held, {tile_b});
        w.player_entity = player_corp;

        std::unordered_map<entity_id, corp_cash_flow> flows;
        flows[player_corp] = corp_cash_flow{ /*income*/ 30.0f, /*expenditure*/ 5.0f };
        flows[rival_a]      = corp_cash_flow{ /*income*/ 10.0f, /*expenditure*/ 1.0f };
        // rival_b and rival_c have no cash-flow entry this tick (no trade) — must not crash.

        const std::vector<corp_standing> a  = compute_corp_standings(w, flows);
        const std::vector<corp_standing> b2 = compute_corp_standings(w, flows);

        check(a.size() == 4, "compute_corp_standings: one entry per corp");

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
        check(p && ra && rb && rc, "compute_corp_standings: all four corps found in output");

        // -----------------------------------------------------------------
        // The public axes — exact for EVERY corp, player and rival alike, at
        // every ownership class. This is what retiring the bands bought.
        // -----------------------------------------------------------------
        if (p)
        {
            check(p->reach_bodies == 2, "player (public): reach_bodies == 2 (two distinct bodies)");
            // total income this tick = 30 (player) + 10 (rival_a) + 0 + 0 = 40.
            check(std::fabs(p->market_share - 0.75f) < 1e-6f, "player (public): market_share == 0.75");
            check(p->capital_disclosed, "player (public): capital_disclosed == true");
            check(std::fabs(p->capital_balance - 25000.0f) < 1e-6f, "player (public): capital_balance == 25000");
        }
        if (ra)
        {
            check(ra->reach_bodies == 1, "rival_a (private): reach_bodies == 1 — PUBLIC axis, printed anyway");
            check(std::fabs(ra->market_share - 0.25f) < 1e-6f,
                  "rival_a (private): market_share == 0.25 — PUBLIC axis, printed anyway");
            check(!ra->capital_disclosed, "rival_a (private): capital_disclosed == false (a dash on screen)");
        }
        if (rb)
        {
            check(rb->reach_bodies == 0, "rival_b (closed): reach_bodies == 0 (no assets)");
            check(rb->market_share == 0.0f, "rival_b (closed): market_share == 0.0 (no cash-flow entry this tick)");
            check(!std::isnan(rb->market_share), "rival_b (closed): market_share is not NaN");
            check(!rb->capital_disclosed, "rival_b (closed): capital_disclosed == false (a dash on screen)");
        }
        if (rc)
        {
            check(rc->reach_bodies == 1, "rival_c (public): reach_bodies == 1");
            check(rc->capital_disclosed, "rival_c (public): capital_disclosed == true — a RIVAL's exact capital");
            check(std::fabs(rc->capital_balance - 1000.0f) < 1e-6f, "rival_c (public): capital_balance == 1000");
        }

        // -----------------------------------------------------------------
        // The gate is a fact about the FIRM, not about the reader. Flip the
        // player's own corp to `closed` and its own row stops filing; flip a
        // rival to `public` and its row files. Nothing about who is reading
        // enters the computation.
        // -----------------------------------------------------------------
        // RE-SPECIFIED (NR-662). The property is still "the gate follows the FIRM,
        // not the reader" -- but it is demonstrated on RIVALS, because disclosure
        // governs what one firm may learn about ANOTHER and the observer's own
        // corporation is outside its scope entirely. A corporation always reads its
        // own books whatever its class; a firm that could not would be unrunnable,
        // and a closed firm reading its own books while publishing none is exactly
        // what `closed` means. FINANCE.md § Disclosure is the authority.
        {
            w.corporations.at(player_corp).ownership_class = ownership_class::closed;
            w.corporations.at(rival_c).ownership_class     = ownership_class::closed;
            w.corporations.at(rival_b).ownership_class     = ownership_class::publicly_held;
            const std::vector<corp_standing> flipped = compute_corp_standings(w, flows);

            bool player_still_reads_own = false;
            bool rival_c_now_closed = false;
            bool rival_b_now_public = false;
            bool axes_unmoved = true;
            for (const corp_standing& cs : flipped)
            {
                if (cs.corp == player_corp)
                {
                    player_still_reads_own = cs.capital_disclosed;
                    axes_unmoved = axes_unmoved && cs.reach_bodies == 2 &&
                                   std::fabs(cs.market_share - 0.75f) < 1e-6f;
                }
                if (cs.corp == rival_c)
                    rival_c_now_closed = !cs.capital_disclosed;
                if (cs.corp == rival_b)
                    rival_b_now_public = cs.capital_disclosed;
            }
            check(rival_c_now_closed,
                  "gate follows the FIRM: a rival set closed => capital_disclosed == false");
            check(rival_b_now_public,
                  "gate follows the FIRM: a rival set public => capital_disclosed == true");
            check(player_still_reads_own,
                  "the observer reads its OWN books whatever its class (NR-662): player set closed => still disclosed");
            check(axes_unmoved,
                  "the public axes are unaffected by ownership class (reach/share unchanged)");

            // Restore for nothing in particular — the world dies with the scope.
            w.corporations.at(player_corp).ownership_class = ownership_class::publicly_held;
            w.corporations.at(rival_b).ownership_class     = ownership_class::closed;
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

        // corporation_component defaults ownership_class to `closed`, so a corp nobody generated
        // a class for does NOT file — the safe default, and the one FINANCE.md wants.
        // RE-SPECIFIED (NR-662): scoped to corps that are NOT the observer's own. This
        // fixture sets w.player_entity = c1, and c1 reads its own books by the exemption
        // above, so asserting over EVERY row would be asserting the opposite property.
        bool defaults_closed = true;
        bool own_books_readable = false;
        for (const corp_standing& cs : out)
        {
            if (cs.is_player) { own_books_readable = cs.capital_disclosed; continue; }
            if (cs.capital_disclosed) defaults_closed = false;
        }
        check(defaults_closed,
              "a RIVAL with the default ownership_class (closed) does not file — safe default");
        check(own_books_readable,
              "and the observer's own default-closed corp still reads its own books (NR-662)");
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
