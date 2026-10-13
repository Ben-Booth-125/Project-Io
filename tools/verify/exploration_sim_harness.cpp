// ---------------------------------------------------------------------------
// exploration_sim_harness — BL-931 (the Exploration span runs, 1200 -> 1660,
// on the shared engine). The Exploration and Industry trees' rows (T1-T4b,
// T7, T8) went with the trees (BL-1272; docs/generation/trees/TREES.md: the
// history runs no technology tree).
//
// Binds:
//   R1   resuming from a prior span's close reproduces the SAME state a
//        fresh (non-resumed) run of that same span would have reached --
//        i.e. `resume_polities == nullptr` is untouched by this item
//   R2   the Exploration span, resumed off a real Empires close, runs its
//        full 460 years without crashing and without touching the Empires
//        span's own recorded outcome (BL-462's acceptance test, replayed)
//   R3   the resumed run is deterministic: same seed, same fixture, twice
//        -> byte-identical ownership record and counters
//   T6   BL-953: a want leans the campaign prize and ranks subjection
//   R3b  REGRESSION PIN of the lean-off (w_want_q 0) resumed span's counters;
//        not an equivalence proof -- T6.5/T6.5b/T6.9 carry "w_want_q 0 changes
//        nothing"
//   R3c  BL-953: with the lean on the span stays deterministic (reported)
//   T9   BL-1096: the purchase fork's price and verb, pure -- a coastal
//        affordable native is bought, an interior or unaffordable one is
//        taken; both constants at zero is OFF; out of domain is REJECTED
//   R3d  BL-1096 CONTROL: the fork switched off through its constants
//        reproduces its pinned counters exactly (the pre-BL-1096 R3b pins
//        until sprint 48's re-bless, 2026-10-03; since then the fixture holds
//        no subject, so off == R3b's on) (and the BL-1097 sea-leg record,
//        noted beside the land record, moves nothing there)
//   R3e  BL-1096/BL-1097 on the real span: bought <= formed, one event per
//        binding by verb, kinds are verbs; the leg table sorted, non-empty
//        where subjects were held, one sea_lane_opened per lane (reported)
//   R4   objects with a term expire on schedule and not before
//   R5   the round-level upkeep hook is callable and moves nothing (it is a
//        documented no-op until BL-932)
//   R6   BL-956: the Exploration handoff (`exploration_output`) passes its
//        validator on a real world, fails it when corrupted, and is what
//        world setup seeds sentiment and stamps roads from
//   R7   BL-954: trade flows -- opened only by trade_access, bounded by want,
//        holding and line, one want shared across sellers, one holding
//        shared across buyers, income at both ends, no flat market income,
//        the MARGINAL trade a clause opens in treaty value
//   R8   BL-955: spend is ALLOCATED -- lean ranks, one scored choice per
//        polity per round among port/navy/army/hold, the creed and Alarm
//        deciding which, decays untouched, the argmax tie order
//   T10  BL-1041 DEFAULT A through the sim's own wiring: a region the span
//        founds inherits its parent's surveyed fuel x0.7, its forest -1
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// ---------------------------------------------------------------------------
// BL-955 fixture: one capital seat per polity, each polity its own culture.
// `state.polities` IS the vector the upkeep mutates (the contract
// `exploration_spend_context::state` states), so the Alarm read and the
// purchases see one table. Build the context at the use site, after the
// fixture has stopped moving.
// ---------------------------------------------------------------------------
struct spend_fixture
{
    std::vector<region> regions;
    history_sim_state   state;
    creed_state         creeds;
};

/// A culture with the two lean inputs set: sea legs, and the war god's temper.
culture lean_culture(int sea_legs_q, int zeal, int dominion)
{
    culture c;
    c.sea_legs_q = sea_legs_q;
    c.pantheon.resize(2);
    c.pantheon[1].zeal     = zeal;
    c.pantheon[1].dominion = dominion;
    return c;
}

int add_spend_polity(spend_fixture& fx, const culture& c, int64_t treasury, int port_window_q,
                     int port_stock_q)
{
    const int i = static_cast<int>(fx.state.polities.size());
    region r;
    r.nation = i; r.treasury = treasury; r.port_q = port_window_q; r.port_stock_q = port_stock_q;
    // BL-972: a paid army step is a LEVY off the seat's pool, so every
    // fixture seat carries one -- 200,000 people (manpower ceiling 10,000 at
    // the 5% frac) with the whole ceiling banked.
    r.population = 200000; r.manpower_stock = 10000;
    fx.regions.push_back(r);
    polity q;
    q.id = i; q.alive = true; q.capital = i; q.culture = i;
    fx.state.polities.push_back(q);
    fx.creeds.cultures.push_back(c);
    return i;
}

exploration_spend_context spend_context(spend_fixture& fx)
{
    exploration_spend_context ctx;
    ctx.state  = &fx.state;
    ctx.creeds = &fx.creeds;
    return ctx;
}

} // namespace

int main()
{
    std::printf("=== exploration_sim_harness (BL-930 / BL-931) ===\n\n");

    // -----------------------------------------------------------------
    // T5: BL-939 -- the scarcity signal itself (refresh + the contact gate)
    // -----------------------------------------------------------------
    {
        std::vector<region> regions(2);
        regions[0].nation = 0; regions[0].has_market = true;
        regions[0].dominant = region_class::none; // wants everything, dominates nothing
        regions[1].nation = 1; regions[1].dominant = region_class::farm; // no market

        std::vector<polity> qs(2);
        qs[0].id = 0; qs[0].alive = true;
        qs[1].id = 1; qs[1].alive = true;

        refresh_market_scarcity(regions, qs);
        check(regions[0].scarcity_q[scarcity_good_index(region_class::farm)] > 0,
              "T5.1  a market on ground its own polity holds nowhere carries a live scarcity signal");
        check(regions[1].scarcity_q[0] == 0 && regions[1].scarcity_q[1] == 0
           && regions[1].scarcity_q[2] == 0 && regions[1].scarcity_q[3] == 0,
              "T5.2  a region with no market carries no scarcity signal at all");

        history_sim_state s;
        check(market_scarcity_q(regions, s, /*viewer=*/0, /*market_region=*/0, region_class::farm)
                  == regions[0].scarcity_q[scarcity_good_index(region_class::farm)],
              "T5.3  a polity reads its OWN market's signal unconditionally");
        check(market_scarcity_q(regions, s, /*viewer=*/1, /*market_region=*/0, region_class::farm) == 0,
              "T5.4  a polity that has never met the market's holder reads 0 (the omniscience guard)");

        s.contacts.push_back(contact{1, 0}); // viewer 1 reading holder 0's market
        std::sort(s.contacts.begin(), s.contacts.end(),
                  [](const contact& a, const contact& b) {
                      return a.from != b.from ? a.from < b.from : a.to < b.to;
                  });
        check(market_scarcity_q(regions, s, /*viewer=*/1, /*market_region=*/0, region_class::farm)
                  == regions[0].scarcity_q[scarcity_good_index(region_class::farm)],
              "T5.5  once contact is recorded, a foreign viewer reads the same signal the holder does");
    }

    // -----------------------------------------------------------------
    // T6: BL-953 -- a want points a campaign outward, and ranks subjection
    // -----------------------------------------------------------------
    {
        // Polity 0's seat (region 0) is an ORE market that holds no farm and
        // no energy anywhere; region 1 is farm ground, region 2 ore ground.
        std::vector<region> regions(3);
        regions[0].nation = 0; regions[0].has_market = true;
        regions[0].dominant = region_class::ore;
        regions[0].culture.id[0] = 0; // the seat's people: culture 0
        regions[1].nation = 1; regions[1].dominant = region_class::farm;
        regions[2].nation = 2; regions[2].dominant = region_class::ore;

        std::vector<polity> qs(3);
        for (int i = 0; i < 3; ++i) { qs[i].id = i; qs[i].alive = true; qs[i].capital = i; qs[i].culture = 0; }
        refresh_market_scarcity(regions, qs);

        const std::vector<culture_good_preference> no_prefs;
        const int want_farm = polity_good_want_q(regions, qs, no_prefs, 0, region_class::farm);
        const int want_ore  = polity_good_want_q(regions, qs, no_prefs, 0, region_class::ore);
        check(want_farm > 0 && want_ore == 0,
              "T6.1  a polity wants a good its seat lacks, and not the good its seat holds");

        std::vector<culture_good_preference> prefs(1);
        prefs[0].culture = 0; prefs[0].good = region_class::farm; prefs[0].weight_q = 1000;
        const int want_farm_pref = polity_good_want_q(regions, qs, prefs, 0, region_class::farm);
        check(want_farm_pref > want_farm
           && want_farm_pref == (regions[0].scarcity_q[scarcity_good_index(region_class::farm)] * 1000) / 1000
           && want_farm == (regions[0].scarcity_q[scarcity_good_index(region_class::farm)] * 500) / 1000,
              "T6.2  the people's preference sharpens the want: scarcity x (500 + weight/2) / 1000");

        check(polity_good_want_q(regions, qs, prefs, 1, region_class::ore) == 0,
              "T6.3  a seat with no market carries no want at all");

        // (a) The SAME prize, leaned by the want for what the target holds.
        const int w = 1000;
        const int prize = 400;
        const int prize_wanted   = want_leaned_campaign_value(prize, w, want_farm_pref); // target: farm ground
        const int prize_unwanted = want_leaned_campaign_value(prize, w, want_ore);       // target: ore ground
        check(prize_wanted > prize && prize_unwanted == prize,
              "T6.4  a wanted good raises a campaign prize over the same target without the want");
        check(want_leaned_campaign_value(prize, 0, want_farm_pref) == prize,
              "T6.5  at w_want_q = 0 the lean is the identity");
        {
            // T6.5b -- the identity across a spread of prizes and wants, so
            // "w_want_q 0 changes nothing" does not rest on one pair of numbers.
            bool identity = true;
            for (const int v : {-50, 0, 1, 137, 400, 999, 5000, 250000})
                for (const int want : {0, 1, 350, 700, 1000})
                    identity = identity && want_leaned_campaign_value(v, 0, want) == v;
            check(identity,
                  "T6.5b at w_want_q = 0 the lean is the identity for every prize and want tried");
        }
        check(want_leaned_campaign_value(0, w, 1000) == 0 && want_leaned_campaign_value(-50, w, 1000) == -50,
              "T6.6  a non-positive prize is never rescued by a want");

        // (b) Subjection picks the higher-want native, ties to the lower id.
        const std::vector<std::pair<int, int>> natives = {
            {1, polity_good_want_q(regions, qs, prefs, 0, regions[1].dominant)},  // farm: wanted
            {2, polity_good_want_q(regions, qs, prefs, 0, regions[2].dominant)},  // ore: held
        };
        check(choose_subjection_native(natives) == 1,
              "T6.7  subjection binds the native whose seat holds the good the arriving power wants");
        const std::vector<std::pair<int, int>> reversed = { {2, 900}, {5, 900}, {1, 100} };
        check(choose_subjection_native(reversed) == 2,
              "T6.8  equal wants tie-break on the LOWER native id, whatever the input order");
        const std::vector<std::pair<int, int>> all_zero = { {7, 0}, {3, 0}, {9, 0} };
        check(choose_subjection_native(all_zero) == 3 && choose_subjection_native({}) == -1,
              "T6.9  with every want 0 the pick is the lowest eligible id (the old id-order walk)");
    }

    // -----------------------------------------------------------------
    // T9: BL-1096 -- BOUGHT OR TAKEN, decided on the ground's terms
    // (EXPLORATION.md sec Two ways to claim ground across water). The
    // verb is a pure function of the native seat, the buyer's purse and the
    // two constants, so the three cases the requirement names go straight
    // to it: a coastal affordable native is bought, an interior one is
    // taken, a coastal unaffordable one is taken.
    // -----------------------------------------------------------------
    {
        history_sim_params p;
        p.subjection_purchase_rate_q = 1000; // the seller's whole chest
        p.subjection_purchase_floor  = 400;

        region coastal_poor;  coastal_poor.port_q  = 500; coastal_poor.treasury  = 100;
        region coastal_rich;  coastal_rich.port_q  = 500; coastal_rich.treasury  = 5000;
        region interior_poor; interior_poor.port_q = 0;   interior_poor.treasury = 100;

        check(purchase_price_q(coastal_poor, p) == 400 && purchase_price_q(coastal_rich, p) == 5000,
              "T9.1  the price is the greater of the floor and the seat's treasury at the rate");
        {
            history_sim_params half = p;
            half.subjection_purchase_rate_q = 500;
            region neg; neg.treasury = -50;
            check(purchase_price_q(coastal_rich, half) == 2500 && purchase_price_q(neg, half) == 400,
                  "T9.2  the rate is per mille of the seller's stock; a negative stock prices as empty");
        }
        check(subjection_verb(coastal_poor, 1000, p) == 0,
              "T9.3  a coastal native whose price the buyer covers is BOUGHT (verb 0)");
        check(subjection_verb(interior_poor, 1000000, p) == 1,
              "T9.4  an interior native is TAKEN however rich the buyer (no coast to land on)");
        check(subjection_verb(coastal_rich, 4999, p) == 1 && subjection_verb(coastal_rich, 5000, p) == 0,
              "T9.5  a coastal native the buyer cannot afford is TAKEN; the boundary is >= price");
        {
            // ZERO-DISABLED: both at zero prices nothing and takes every coast.
            history_sim_params off = p;
            off.subjection_purchase_rate_q = 0;
            off.subjection_purchase_floor  = 0;
            check(subjection_purchase_params_valid(off)
               && subjection_verb(coastal_poor, 1LL << 40, off) == 1,
                  "T9.6  with both constants at zero the fork is OFF: valid, and every binding taken");
            // One of the two alone is a legal shape: a flat price, or a pure rate.
            history_sim_params flat = off; flat.subjection_purchase_floor = 400;
            history_sim_params rate = off; rate.subjection_purchase_rate_q = 1000;
            check(subjection_verb(coastal_poor, 400, flat) == 0
               && subjection_verb(coastal_rich, 4999, rate) == 1
               && subjection_verb(coastal_rich, 5000, rate) == 0,
                  "T9.7  a floor alone is a flat price; a rate alone is proportional to the seller's stock");
        }
        {
            // REJECT, NEVER CLAMP: out of domain refuses the fork whole.
            history_sim_params bad_rate = p;  bad_rate.subjection_purchase_rate_q = 10001;
            history_sim_params neg_rate = p;  neg_rate.subjection_purchase_rate_q = -1;
            history_sim_params neg_floor = p; neg_floor.subjection_purchase_floor = -1;
            history_sim_params big_floor = p; big_floor.subjection_purchase_floor = (1LL << 48) + 1;
            history_sim_params edge = p;      edge.subjection_purchase_rate_q = 10000;
                                              edge.subjection_purchase_floor  = 1LL << 48;
            check(!subjection_purchase_params_valid(bad_rate) && !subjection_purchase_params_valid(neg_rate)
               && !subjection_purchase_params_valid(neg_floor) && !subjection_purchase_params_valid(big_floor)
               && subjection_purchase_params_valid(edge),
                  "T9.8  the rate is valid on 0-10000 and the floor on 0-2^48, inclusive, and nowhere else");
            check(subjection_verb(coastal_poor, 1LL << 40, bad_rate) == 1
               && subjection_verb(coastal_poor, 1LL << 40, neg_floor) == 1,
                  "T9.9  a rejected pair buys nothing: every binding is taken, never a clamped price");
        }
    }

    // -----------------------------------------------------------------
    // R1/R2/R3: the resumed span, over a REAL Empires close
    // -----------------------------------------------------------------
    era_minus_one_fixture fixture;
    world_params wp;
    wp.seed = 0xABCDEF01u;
    generation_report rep;
    make_hard_coded_world(wp, &rep, {}, nullptr, nullptr, &fixture);

    check(fixture.ran, "R0  the fixture captured a real Empires-round invocation");
    if (fixture.ran)
    {
        // R1: reproduce generation's own Empires run EXACTLY (BL-462's own
        // acceptance test), from the captured fixture -- this is the
        // `resume_polities == nullptr` path, UNCHANGED by this item.
        settlement_state ss_a = fixture.settlement;
        const history_sim_state hs_a = run_history_sim(
            ss_a, &fixture.creeds, fixture.terrain.view(), fixture.gw, fixture.gh,
            fixture.params, fixture.seed, nullptr, fixture.works, nullptr);

        check(hs_a.battles == fixture.battles && hs_a.conquests == fixture.conquests
           && hs_a.foundings == fixture.foundings,
              "R1  a fresh (non-resumed) re-run still reproduces generation's own Empires "
              "outcome bit-for-bit (BL-931 touched nothing on the null-resume path)");

        // R2/R3: fold the handoff, then run the Exploration span off it,
        // TWICE from independent copies, at the same seed.
        const pass_one_output p1 = make_pass_one_output(ss_a, hs_a, &fixture.creeds);

        check(!p1.polities.empty(), "R2.0  the Empires close leaves at least one living polity");

        // `tweak`, when given, edits the params after every default below is
        // set -- the BL-1096 control (R3d) switches the purchase fork off
        // through it, so the control and the pinned run differ in exactly
        // the two constants and nothing else.
        const auto run_exploration = [&](uint32_t seed, int w_want_q = 0,
                                         std::vector<region>* regions_out = nullptr,
                                         void (*tweak)(history_sim_params&) = nullptr) {
            settlement_state ss = ss_a; // Independent copy each call.
            history_sim_params ep;
            ep.start_year = fixture.params.stop_year; // 1200, wherever Empires closed.
            ep.stop_year  = fixture.params.stop_year + 460; // EXPLORATION.md's own span.
            // BL-1036: the two 1200 anchors, explicit, as exploration_sim_params sets them.
            ep.consolidation_year    = ep.start_year;
            ep.near_home_cutoff_year = ep.start_year;
            ep.tick_bands[0]   = {ep.stop_year, 4};
            ep.tick_band_count = 1;
            ep.exploration_upkeep_enabled          = true;
            ep.city_states_by_population_threshold = true;
            ep.settle_requires_razed_ground         = true;
            // BL-1132 (review fix): the settle spacing as generation sets it,
            // copied from exploration_sim_params rather than left at the
            // struct default of 1, so these checks run generation's site rule.
            ep.settle_min_spacing_tiles = exploration_sim_params(world_params{}).settle_min_spacing_tiles;
            ep.w_want_q                             = w_want_q; // BL-953; 0 = the pre-change span
            // BL-1044 (cold review, finding 2): the corridor tier as generation
            // sets it — on since BL-1044 — named here rather than taken from the
            // struct default. R3b's counters were pinned with it off; on this
            // seed they hold exactly with it on (exploration_sim_harness ALL
            // PASS, 2026-09-22): no Empires corridor here reopens off its rung.
            ep.resume_seeds_corridor_tier = exploration_sim_params(world_params{}).resume_seeds_corridor_tier;
            ep.resume_polities  = &p1.polities;
            ep.resume_grudges   = &p1.grudges;
            ep.resume_contacts  = &p1.contacts;
            ep.resume_corridors = &p1.surviving_corridors;
            ep.resume_civilisations    = &p1.civilisations;    // BL-1049: as generation's 1200 resume
            ep.resume_universal_creeds = &p1.universal_creeds;
            if (tweak != nullptr) tweak(ep);
            history_sim_state st = run_history_sim(ss, &fixture.creeds, fixture.terrain.view(),
                                                   fixture.gw, fixture.gh, ep, seed, nullptr,
                                                   fixture.works, nullptr);
            if (regions_out != nullptr) *regions_out = ss.regions; // the span's closing ground
            return st;
        };

        std::vector<region> ex1_regions;
        const history_sim_state ex1 = run_exploration(0x515C0E17u, 0, &ex1_regions);

        // R8.18 (BL-955 F3): the paid standing army's raw invariant on a REAL
        // generated world -- at the close, region by region, and every round.
        {
            int64_t close_bad = 0, paid_regions = 0;
            for (const region& r : ex1_regions)
            {
                if (!standing_army_invariant_holds(r)) ++close_bad;
                if (r.standing_army > 0) ++paid_regions;
            }
            std::printf("      R8.18: regions=%zu carrying paid heads=%lld close violations=%lld "
                        "per-round violations=%lld\n", ex1_regions.size(),
                        static_cast<long long>(paid_regions), static_cast<long long>(close_bad),
                        static_cast<long long>(ex1.standing_army_invariant_violations));
            check(!ex1_regions.empty() && paid_regions > 0 && close_bad == 0
               && ex1.standing_army_invariant_violations == 0,
                  "R8.18 on a real generated world, every region at the span's close has standing_army <= "
                  "army_stock and a paid count only on its payer's ground; the per-round check counts 0 "
                  "(BL-955 F3)");
        }
        const history_sim_state ex2 = run_exploration(0x515C0E17u);

        check(ex1.years == 460, "R2.1  the resumed span runs its full 460 years (1200 -> 1660)");
        check(static_cast<int>(ex1.polities.size()) == static_cast<int>(p1.polities.size())
           || ex1.polities.size() > p1.polities.size(), // BL-920 can raise new city states mid-span
              "R2.2  the resumed run opens on the handed-off polity set, never fewer");

        bool same_owner_changes = ex1.owner_changes.size() == ex2.owner_changes.size();
        if (same_owner_changes)
            for (std::size_t i = 0; i < ex1.owner_changes.size(); ++i)
                if (ex1.owner_changes[i].year   != ex2.owner_changes[i].year
                 || ex1.owner_changes[i].region != ex2.owner_changes[i].region
                 || ex1.owner_changes[i].owner  != ex2.owner_changes[i].owner)
                { same_owner_changes = false; break; }

        check(same_owner_changes
           && ex1.battles == ex2.battles && ex1.conquests == ex2.conquests
           && ex1.foundings == ex2.foundings,
              "R3  the resumed Exploration span is deterministic: same seed, same fixture, "
              "twice -> byte-identical ownership record and counters");

        std::printf("      exploration span: years=%lld battles=%lld conquests=%lld "
                    "foundings=%lld polities(1200)=%d polities(1660)=%d\n",
                    static_cast<long long>(ex1.years), static_cast<long long>(ex1.battles),
                    static_cast<long long>(ex1.conquests), static_cast<long long>(ex1.foundings),
                    static_cast<int>(p1.polities.size()), static_cast<int>(ex1.polities.size()));

        // R3b: A REGRESSION PIN OF THE LEAN-OFF SPAN, NOT AN EQUIVALENCE
        // PROOF. These are the resumed span's counters at `w_want_q` 0 on this
        // exact fixture and seed. They were first read with the harness built
        // BEFORE BL-953 touched the scorer or subjection (2026-09-14, main @
        // 9b342fc6), when they did prove BL-953 at 0 moved nothing; since then
        // other changes have legitimately moved the span, so today they only
        // catch an UNEXPLAINED move. What carries "w_want_q 0 changes nothing"
        // now is the unit pair T6.5/T6.5b (the lean is the identity at 0) and
        // T6.9 (all-zero wants pick the lowest id). A change that moves these
        // re-pins them with its cause stated.
        //
        // RE-PINNED 2026-09-14 at sprint-41 integration: BL-954 (trade flows
        // replace the flat market income, trade value enters treaty value, one
        // want shared across sellers) moves the lean-off span -- was battles 426,
        // conquests 423, foundings 451, tribute 134254916, treaties 287, owner
        // changes 2453. The pre-BL-953 equivalence itself was proven on BL-953`s
        // own branch (59768711) before trade existed.
        //
        // RE-PINNED 2026-09-14 at the sprint-41 wave-1 review fixes: treaty
        // value now reads the MARGINAL trade a clause opens (F3) and a seller's
        // holding is shared across its buyers (F5) -- was tribute 134235484,
        // treaties 295 (battles 483, conquests 425, foundings 497, subjections
        // 5, freed 1, broken 0, owner changes 2501 unmoved).
        //
        // RE-PINNED 2026-09-14 by BL-955 (spend is ALLOCATED): ports, navies
        // and standing armies are one scored choice per polity per round
        // instead of all three bought whenever affordable, so far fewer
        // fleets and standing armies stand and the span's wars move -- was
        // battles 483, conquests 425, foundings 497, tribute 134235719,
        // treaties 294, owner changes 2501 (subjections 5, freed 1, broken 0
        // unmoved).
        //
        // RE-PINNED 2026-09-14 by the BL-955 review fix: a PAID standing army
        // (`region::standing_army`) now persists through the yearly muster's
        // disband, decays only when unfunded, and saturates at
        // army_saturation_per_region x held regions -- was battles 398,
        // conquests 343, foundings 432, tribute 134335519, treaties 265, owner
        // changes 2387 (subjections 5, freed 1, broken 0 unmoved).
        //
        // RE-PINNED 2026-09-14 by the BL-955 cold-review fixes: paid heads are
        // lost against the pool BEFORE a loss (F1), the scorer's defender levy
        // reads ordinary men only (F2), saturation reads the realm's paid
        // heads (F4), stale paid counts are voided on every ownership change
        // (F5a) -- was battles 414, conquests 357, foundings 420, tribute
        // 134357387, treaties 285, owner changes 2337 (subjections 5, freed 1,
        // broken 0 unmoved).
        //
        // RE-PINNED 2026-09-14 by BL-949 (b): a resumed span now seeds its live
        // corridor use counts from `resume_corridors`, so a line the Empires round
        // paved opens the Exploration span as a Track/Road for reach (and as walked
        // ground for trade links) instead of tier 0 -- was battles 407,
        // conquests 351, foundings 418, subjections 5, freed 1, tribute
        // 134357387, treaties 291, broken 0, owner changes 2327.
        //
        // RE-PINNED 2026-09-14 by BL-950: the deterrence split widened --
        // deterrence_alarm_weight_q 400 -> 575 (a long-known neighbour with visible
        // capability binds more readily) and treaty_far_penalty_q 350 -> 700 (a pair
        // met during the span no longer binds except on trade) -- so fewer
        // neighbour wars and more frontier ones --
        // was battles 554, conquests 496, foundings 445, subjections 5,
        // freed 1, tribute 134354043, treaties 283, broken 2, owner changes
        // 2280.
        //
        // RE-PINNED 2026-09-14 by Ben`s authorisation (NR-867):
        // deterrence_alarm_weight_q 575 -> 525 -- was battles 258, conquests 257,
        // foundings 579, subjections 5, freed 1, tribute 134353672, treaties 307,
        // broken 4, owner changes 2154.
        std::printf("      pinned-read: subjections=%lld freed=%lld tribute=%lld treaties=%lld "
                    "broken=%lld owner_changes=%zu\n",
                    static_cast<long long>(ex1.subjections_formed),
                    static_cast<long long>(ex1.subjections_freed),
                    static_cast<long long>(ex1.tribute_remitted),
                    static_cast<long long>(ex1.treaties_formed),
                    static_cast<long long>(ex1.treaties_broken), ex1.owner_changes.size());
        // RE-PINNED 2026-09-16 (Ben, NR-877): the sprint-42 wave-1 re-bless, one
        // authorisation over five named causes -- deposits scaled by the interior's
        // budget at the fossil epoch, a river step priced like a shore step, force
        // upkeep paid per head from the treasury, every wired tree effect reaching
        // the sim, and a nation opening with its folded polities' 1660 chest.
        // 308/306/472/2196 -> 322/207/739/2410. The span is calmer and more
        // commercial: fewer conquests, far more foundings, and subjection and
        // tribute rise sharply as realms that can pay hold subjects instead of
        // taking ground. Authorised against that shape, never against the hashes.
        // RE-PINNED 2026-09-16 (Ben, backlog wave A re-bless, authorised to measure and
        // re-bless the merged world): 322/207/739/2410 -> 30/27/816/2061. ONE CAUSE moves
        // this fixture -- BL-1021, Empires-round trade paid per unlike KIND of ground a
        // realm reaches rather than per walked unlike corridor, so the 1200 close this
        // span resumes from hands forward a different map and stock. BL-842 (small
        // grudges decay) and BL-1009 (the widened digest) leave these counters exactly
        // as they were; BL-841 was reverted after cold review. THIS FIXTURE IS THE
        // w_want_q = 0 VARIANT AND IS NOT THE PHASE'S READING: its span nearly stops
        // fighting (battles 322 -> 30), while exploration_sweep over the 16 parity seeds
        // is the verdict on whether displacement survives (see the wave A DEVLOG entry).
        // RE-PINNED 2026-09-25 (Ben, sprint 47's one re-bless, authorised in principle
        // 2026-09-24): tribute 228427344 -> 228427744. ONE CAUSE, +400 tribute and
        // nothing else -- BL-1096 (the purchase verb): a realm may now BUY a
        // province it reaches instead of taking it, at the ruled purchase rate, and
        // the payment lands in the seller's tribute counter. Battles 30, conquests 27,
        // foundings 816, subjections 3, freed 0, treaties 354, broken 1 and the
        // 2061 owner changes are unmoved: on this w_want_q = 0 fixture the verb
        // fires once. Read on the merged sprint-47 tree (894e736e) from a rebuilt
        // harness; the sea-leg record (BL-1097), the band (BL-1101) and the tariff
        // posture (BL-1102) touch nothing this fixture counts.
        // RE-PINNED 2026-10-03 (sprint 48's one re-bless, PREPARED for Ben's
        // authorisation; docs/development/drafts/sprint-48-rebless.md):
        // 30/27/816 battles/conquests/foundings, subjections 3, tribute
        // 228427744, treaties 354, broken 1, owner changes 2061 ->
        // 239/150/134, subjections 0, freed 0, tribute 0, treaties 496, broken
        // 4, owner changes 864. Read on main a899bb2a. This fixture's 1200
        // close is rebuilt by sprint 48's settlement chain (BL-1130/1132/1133/
        // 1141: polities(1200) 457 on far fewer, deeper regions), so the span
        // resumes on a different map: it fights more and founds far less
        // (settle spacing leaves no room; BL-1132 recorded foundings -65% on
        // the 16 seeds), and on this w_want_q = 0 fixture no realm holds a
        // subject at all, so the purchase verb and tribute read 0. The sea
        // chain (BL-1120/1140/1142/1147/1152/1153) is inside the same span.
        // Not attributed per cause on this fixture: the items measured their
        // own moves on the 16 seeds, and this pin only catches an
        // unexplained move from here.
        // RE-PINNED 2026-10-07 (sprint 49's one re-bless, PREPARED for Ben's
        // authorisation; docs/development/drafts/sprint-49-rebless.md):
        // 239/150/134, subjections 0, freed 0, tribute 0, treaties 496, broken
        // 4, owner changes 864 -> 86/83/112, subjections 2, freed 0, tribute
        // 2481129, treaties 359, broken 1, owner changes 778. Read on main
        // c77f2d5a. ONE CAUSE, measured: the lake size cap (BL-1200, enclosed
        // water of 150+ tiles is sea). The harness built on c39d2989 (the
        // merge before it) passes on the old pins; built on 8277706d (its
        // merge) it reads exactly the new values, which hold unchanged to the
        // tip. The fixture's 1200 close is re-drawn by the re-classified water,
        // so the span resumes on a different map: it fights a third as much
        // and a realm holds subjects again, so purchases and tribute return.
        // RE-PINNED 2026-10-10 (BL-1272, the history runs no technology tree): 86/83/112, 2, 0, 2481129, 359, 1, 778 -> 24/21/114, 5, 0, 37154489, 265, 0, 656 (1200 handoff polities 437 -> 255: the tree's defence/cohesion modifiers left the Empires span; sea legs open by date).
        // RE-PINNED 2026-10-11 (BL-1272, Ben: a fleet afloat has a hull): subjections 5 -> 1, tribute 37154489 -> 1. On this fixture the P90 navy floor is 0, so all five subjects were taken by powers with no ships; the > 0 hull clause alone moves it.
        // RE-PINNED 2026-10-12 (BL-1290, civil-war famine and flight in Empires; Ben's standing rule to
        // re-pin at Empires-moving merges): the span resumes from a different 1200 close (polities 255 -> 375).
        // 24/21/114, 1, 0, 1, 265, 0, 656 -> 246/147/131, 1, 0, 0, 479, 0, 860.
        // RE-PINNED 2026-10-13 (BL-1287, the rise and fall reshapes the Empires close; standing rule):
        // 246/147/131, 1, 0, 0, 479, 0, 860 -> 14/11/142, 1, 0, 3331882, 240, 0, 730.
        // RE-PINNED 2026-10-14 (BL-1288 late culture + BL-1289/1294 Industrialisation merges; standing rule):
        // -> 16/16/110, 1, 0, 2577953, 274, 0, 627.
        check(ex1.battles == 16 && ex1.conquests == 16 && ex1.foundings == 110
           && ex1.subjections_formed == 1 && ex1.subjections_freed == 0
           && ex1.tribute_remitted == 2577953 && ex1.treaties_formed == 274
           && ex1.treaties_broken == 0 && ex1.owner_changes.size() == 627,
              "R3b  REGRESSION PIN: the w_want_q = 0 Exploration span matches its pinned counters "
              "exactly (battles, conquests, foundings, subjections, tribute, treaties, owner record)");

        // R3c (BL-953): the same span with the lean ON -- deterministic, and
        // REPORTED rather than gated on direction (a seed owes no outcome).
        {
            const int want_w = exploration_sim_params(world_params{}).w_want_q;
            const history_sim_state wa = run_exploration(0x515C0E17u, want_w);
            const history_sim_state wb = run_exploration(0x515C0E17u, want_w);
            check(want_w != 0,
                  "R3c.0  the Exploration span's own params carry a non-zero want lean");
            check(wa.battles == wb.battles && wa.conquests == wb.conquests
               && wa.foundings == wb.foundings && wa.subjections_formed == wb.subjections_formed
               && wa.owner_changes.size() == wb.owner_changes.size(),
                  "R3c.1  with the want lean on, the span is still deterministic (same seed twice)");
            check(wa.standing_army_invariant_violations == 0,
                  "R8.18b with generation's own want lean on, the per-round paid-army invariant check "
                  "counts 0 (BL-955 F3)");
            std::printf("      want lean w_want_q=%d: battles=%lld conquests=%lld foundings=%lld "
                        "subjections=%lld owner_changes=%zu\n", want_w,
                        static_cast<long long>(wa.battles), static_cast<long long>(wa.conquests),
                        static_cast<long long>(wa.foundings),
                        static_cast<long long>(wa.subjections_formed), wa.owner_changes.size());
        }
        // BL-940 -- REPORTED, not gated: whether any polity in THIS seed ever
        // held EX-WY-1a and had a treasury to spend is a fact about the seed,
        // same discipline `unsustained_attrition_events` and the naval
        // counters already hold to elsewhere in this codebase.
        std::printf("      exploration road ladder: post_roads_built=%lld  "
                    "treasury_spent_on_roads=%lld\n",
                    static_cast<long long>(ex1.post_roads_built),
                    static_cast<long long>(ex1.treasury_spent_on_roads));

        // R3d (BL-1096): THE CONTROL. The same span with the purchase fork
        // switched OFF through the two constants (both zero) must be what the
        // block was before the fork existed: the pre-BL-1096 R3b pins, exactly
        // -- battles 30, conquests 27, foundings 816, subjections 3, freed 0,
        // tribute 228427344, treaties 354, broken 1, owner changes 2061 (the
        // 2026-09-16 wave A re-bless). That is "byte-identical to a run without
        // the change" at this harness's grain: every counter and the whole
        // ownership record, with the fork's own counters at zero. `subject_kind`
        // is the one field that reads differently by design -- it is now the
        // VERB, so with the fork off every subject reads TAKEN (1) where the
        // proxy used to read a coast (0); nothing decides on it. The sea-leg
        // record (BL-1097) is noted beside the land record and reads nothing
        // back, so it is inside this same control: the land record and the
        // ownership record are unmoved by its presence.
        {
            const history_sim_state off = run_exploration(0x515C0E17u, 0, nullptr,
                [](history_sim_params& p) {
                    p.subjection_purchase_rate_q = 0;
                    p.subjection_purchase_floor  = 0;
                });
            bool subjects_all_taken = true;
            for (const polity& q : off.polities)
                if (q.overlord >= 0 && q.subject_kind != 1) subjects_all_taken = false;
            check(!off.subjection_purchase_params_rejected
               && off.provinces_bought == 0 && off.treasury_spent_on_purchases == 0
               && subjects_all_taken,
                  "R3d.0 with both purchase constants at zero the fork is off: nothing bought, nothing "
                  "spent, not rejected, every subject TAKEN");
            // RE-PINNED 2026-10-03 (sprint 48's one re-bless, PREPARED for Ben's
            // authorisation; see R3b's note for the cause): 30/27/816, 3, 0,
            // 228427344, 354, 1, 2061 -> 239/150/134, 0, 0, 0, 496, 4, 864, read
            // on main a899bb2a. The pre-BL-1096 world these pins first named
            // is no longer reproducible; on the re-blessed fixture no realm
            // holds a subject, so the fork-off span equals R3b's fork-on span
            // counter for counter -- the control now pins that equality.
            // RE-PINNED 2026-10-07 (sprint 49's one re-bless, PREPARED for
            // Ben's authorisation; see R3b's note for the cause, BL-1200):
            // 239/150/134, 0, 0, 0, 496, 4, 864 -> 86/83/112, 2, 0, 747063,
            // 359, 1, 778, read on main c77f2d5a. Subjects exist again on this
            // fixture, so the fork-off span and R3b's fork-on span no longer
            // agree counter for counter.
            // RE-PINNED 2026-10-10 (BL-1272, the history runs no technology
            // tree; R3b's cause): 86/83/112, 2, 0, 747063, 359, 1, 778 ->
            // 24/21/114, 5, 0, 37154178, 265, 0, 656. The fork-off span and
            // R3b's fork-on span share every war, treaty and ownership counter
            // and differ only in tribute (37154178 here against R3b's
            // 37154489; the purchase fork is the only switch between them, its
            // payment mechanism not traced further here). The BL-1272 fix round
            // (sea legs need naval capacity; the coastal naval ramp) left both
            // pins unmoved on this fixture. RE-PINNED 2026-10-11 (Ben: a fleet
            // afloat has a hull, R3b's cause): subjections 5 -> 1, tribute
            // 37154178 -> 1.
            // RE-PINNED 2026-10-12 (BL-1290, R3b's cause): -> 246/147/131, 1, 0, 0, 479, 0, 860.
            // RE-PINNED 2026-10-13 (BL-1287, R3b's cause): -> 14/11/142, 1, 0, 918015, 240, 0, 730.
            // RE-PINNED 2026-10-14 (BL-1288, R3b's cause): -> 16/16/110, 1, 0, 668772, 274, 0, 627.
            check(off.battles == 16 && off.conquests == 16 && off.foundings == 110
               && off.subjections_formed == 1 && off.subjections_freed == 0
               && off.tribute_remitted == 668772 && off.treaties_formed == 274
               && off.treaties_broken == 0 && off.owner_changes.size() == 627,
                  "R3d.1 CONTROL: the fork-off span matches its pinned counters exactly (battles, "
                  "conquests, foundings, subjections, tribute, treaties, owner record)");
            std::printf("      control (purchase off): battles=%lld conquests=%lld foundings=%lld "
                        "subjections=%lld freed=%lld tribute=%lld treaties=%lld broken=%lld "
                        "owner_changes=%zu "
                        "sea_legs=%zu (campaign %lld, tribute-round %lld, lanes %lld)\n",
                        static_cast<long long>(off.battles), static_cast<long long>(off.conquests),
                        static_cast<long long>(off.foundings),
                        static_cast<long long>(off.subjections_formed),
                        static_cast<long long>(off.subjections_freed),
                        static_cast<long long>(off.tribute_remitted),
                        static_cast<long long>(off.treaties_formed),
                        static_cast<long long>(off.treaties_broken), off.owner_changes.size(),
                        off.sea_legs.size(), static_cast<long long>(off.sea_legs_noted_campaign),
                        static_cast<long long>(off.sea_legs_noted_tribute),
                        static_cast<long long>(off.sea_lanes_opened));
        }

        // R3e (BL-1096 / BL-1097): the fork ON, on the real span -- the
        // bookkeeping holds together whatever this seed happened to buy.
        // REPORTED, not gated on direction: whether this fixture buys at all
        // is a fact about the seed; the 16-seed reading is exploration_sweep's.
        {
            int64_t bought_events = 0, bound_events = 0, lane_events = 0;
            for (const lapse_event& e : ex1.events)
            {
                if (e.kind == static_cast<uint8_t>(lapse_event_kind::province_bought)) ++bought_events;
                if (e.kind == static_cast<uint8_t>(lapse_event_kind::subject_bound))   ++bound_events;
                if (e.kind == static_cast<uint8_t>(lapse_event_kind::sea_lane_opened)) ++lane_events;
            }
            bool kinds_are_verbs = true;
            for (const polity& q : ex1.polities)
                if ((q.overlord >= 0) != (q.subject_kind >= 0)
                 || q.subject_kind < -1 || q.subject_kind > 1) kinds_are_verbs = false;
            check(!ex1.subjection_purchase_params_rejected
               && ex1.provinces_bought <= ex1.subjections_formed
               && bought_events == ex1.provinces_bought
               && bound_events  == ex1.subjections_formed - ex1.provinces_bought
               && kinds_are_verbs,
                  "R3e.0 fork ON: bought <= formed; one province_bought event per purchase and one "
                  "subject_bound per take; every subject's kind is a verb set iff an overlord is");
            check((ex1.provinces_bought == 0) == (ex1.treasury_spent_on_purchases == 0)
               && ex1.treasury_spent_on_purchases
                    >= ex1.provinces_bought * exploration_sim_params(world_params{}).subjection_purchase_floor,
                  "R3e.1 the treasury moved for purchases is zero iff nothing was bought and never "
                  "below bought x floor");
            // BL-1097: the leg table is non-empty on a span that held subjects
            // (the tribute-round leg is written once per round per link), sorted
            // and canonical; land corridors are covered by R3d's control.
            bool legs_sorted = true;
            int64_t legs_over_tier = 0;
            for (std::size_t i = 0; i < ex1.sea_legs.size(); ++i)
            {
                const sea_leg& l = ex1.sea_legs[i];
                if (l.a >= l.b || l.uses <= 0) legs_sorted = false;
                if (i > 0 && !(std::make_pair(ex1.sea_legs[i - 1].a, ex1.sea_legs[i - 1].b)
                             < std::make_pair(l.a, l.b))) legs_sorted = false;
                if (l.uses >= exploration_sim_params(world_params{}).sea_lane_tier1_uses) ++legs_over_tier;
            }
            check(legs_sorted
               && (ex1.subjections_formed == 0 || !ex1.sea_legs.empty())
               && ex1.sea_lanes_opened == lane_events
               && ex1.sea_legs_noted_tribute + ex1.sea_legs_noted_campaign + ex1.sea_legs_noted_purchase > 0,
                  "R3e.2 the sea-leg table is sorted and canonical, non-empty where subjects were held, "
                  "and one sea_lane_opened event was noted per lane opened");
            std::printf("      purchase fork ON: bought=%lld of %lld formed, spent=%lld | sea legs=%zu "
                        "over tier=%lld (noted: campaign %lld purchase %lld tribute-round %lld; lanes %lld)\n",
                        static_cast<long long>(ex1.provinces_bought),
                        static_cast<long long>(ex1.subjections_formed),
                        static_cast<long long>(ex1.treasury_spent_on_purchases), ex1.sea_legs.size(),
                        static_cast<long long>(legs_over_tier),
                        static_cast<long long>(ex1.sea_legs_noted_campaign),
                        static_cast<long long>(ex1.sea_legs_noted_purchase),
                        static_cast<long long>(ex1.sea_legs_noted_tribute),
                        static_cast<long long>(ex1.sea_lanes_opened));
        }

    }

    // -----------------------------------------------------------------
    // R6: BL-956 -- the Exploration handoff is a validated value, and
    // world setup reads 1660 grudges and corridors from it
    // -----------------------------------------------------------------
    check(fixture.exploration_ran,
          "R6.0  default params run the Exploration span during generation");
    if (fixture.exploration_ran)
    {
        const exploration_output& eo = fixture.exploration_handoff;
        std::string why;
        const bool valid = exploration_output_valid(eo, &why);
        if (!valid) std::printf("      exploration_output_valid: %s\n", why.c_str());
        check(valid, "R6.1  the real generated world's exploration_output passes its validator");
        check(eo.start_year == fixture.exploration_params.start_year
           && eo.stop_year == fixture.exploration_params.stop_year,
              "R6.2  the handoff's span is the span generation actually ran");

        // BL-969: the culture table crosses IN the value, sized to its own
        // count, and the validator bites on a short copy -- the fixture
        // holds no post-span creed_state to prove equality against here;
        // the shipped path does that itself (world_determinism R3.6).
        check(!eo.cultures.empty()
           && eo.cultures.size() == static_cast<std::size_t>(eo.culture_count),
              "R6.2' the 1660 culture table crosses in the handoff, sized to culture_count (BL-969)");
        if (!eo.cultures.empty())
        {
            exploration_output short_table = eo;
            short_table.cultures.pop_back();
            std::string w;
            check(!exploration_output_valid(short_table, &w)
               && w.find("culture_count") != std::string::npos,
                  "R6.2'' a culture table shorter than culture_count fails the validator, for that reason");
        }

        // R6.3-R6.5: the validator is not a rubber stamp -- a deliberately
        // corrupted copy of the same value fails it.
        if (!eo.polities.empty())
        {
            exploration_output bad = eo;
            bad.polities[0].overlord = static_cast<int32_t>(bad.polities.size()) + 7;
            bad.polities[0].subject_kind = 1;
            check(!exploration_output_valid(bad, nullptr),
                  "R6.3  an out-of-range overlord fails the validator");

            exploration_output self_lord = eo;
            self_lord.polities[0].overlord = 0;
            self_lord.polities[0].subject_kind = 1;
            check(!exploration_output_valid(self_lord, nullptr),
                  "R6.4  a polity that is its own overlord fails the validator");
        }
        if (eo.polities.size() >= 2 && !eo.holdings.empty() && !eo.holdings[0].regions.empty())
        {
            // Re-own a held region to ANOTHER polity that is in range, so the
            // only thing wrong is the holding/ownership disagreement -- and
            // prove the validator failed for exactly that reason.
            exploration_output bad = eo;
            const int r = bad.holdings[0].regions[0];
            bad.regions[static_cast<std::size_t>(r)].nation = bad.holdings[0].polity == 0 ? 1 : 0;
            std::string bad_why;
            const bool rejected = !exploration_output_valid(bad, &bad_why);
            if (rejected) std::printf("      R6.5 rejected with: %s\n", bad_why.c_str());
            check(rejected && bad_why.find("holdings disagree with its nation") != std::string::npos,
                  "R6.5  a holding that disagrees with region ownership fails the validator, for that reason");
        }

        // R6.8-R6.13: the value checks and the trade-flow checks reject too.
        if (!eo.regions.empty() && !eo.polities.empty())
        {
            const auto rejects_with = [](const exploration_output& v, const char* needle) {
                std::string w;
                return !exploration_output_valid(v, &w) && w.find(needle) != std::string::npos;
            };
            exploration_output b1 = eo; b1.regions[0].nation = -2;
            check(rejects_with(b1, "out of range"), "R6.8  a region owner below -1 fails the validator");
            exploration_output b2 = eo; b2.regions[0].treasury = -1;
            check(rejects_with(b2, "negative treasury"), "R6.9  a negative treasury fails the validator");
            exploration_output b3 = eo; b3.regions[0].port_stock_q = 1001;
            check(rejects_with(b3, "port_stock_q"), "R6.10 a port_stock_q above 1000 fails the validator");
            exploration_output b4 = eo; b4.regions[0].army_stock = -1;
            check(rejects_with(b4, "army_stock"), "R6.11 a negative army_stock fails the validator");
            exploration_output b5 = eo; b5.polities[0].navy_stock = -1;
            check(rejects_with(b5, "navy_stock"), "R6.12 a negative navy_stock fails the validator");

            // A flow on a pair of living polities that holds no trade_access.
            int la = -1, lb = -1;
            for (const polity& q : eo.polities)
            {
                if (!q.alive) continue;
                if (la < 0) { la = q.id; continue; }
                bool bound = false;
                for (const dated_object& d : eo.dated_objects)
                    if (d.kind == static_cast<int32_t>(treaty_clause::trade_access)
                     && ((d.a == la && d.b == q.id) || (d.a == q.id && d.b == la)))
                        bound = true;
                if (!bound) { lb = q.id; break; }
            }
            if (la >= 0 && lb >= 0)
            {
                exploration_output b6 = eo;
                b6.trade_flows = { trade_flow{static_cast<uint16_t>(la), static_cast<uint16_t>(lb), 0, 10} };
                check(rejects_with(b6, "no standing trade_access"),
                      "R6.13 a flow whose pair holds no standing trade_access clause fails the validator");
            }
        }
        std::printf("      handoff trade_flows=%zu\n", eo.trade_flows.size());

        // R6.6/R6.7: world setup consumed the LAST close's own tables -- not
        // the 1200 pass-one set, which on this seed differs (so an equality
        // with the close cannot be a 1200 read). RE-POINTED by BL-1044: the
        // Industrialisation span runs by default, so the last close is its 1960
        // value (`industrialisation_handoff`, which setup reads since BL-1053) and
        // the 1660 handoff only where the span did not run -- the binding the
        // fixture's own capture documents (era_minus_one.hpp, setup_grudges).
        const exploration_output& last_close = fixture.industrialisation_ran
            ? static_cast<const exploration_output&>(fixture.industrialisation_handoff) : eo;
        const char* last_close_name = fixture.industrialisation_ran ? "1960 Industrialisation" : "1660 Exploration";
        const auto grudges_same = [](const std::vector<grudge>& x, const std::vector<grudge>& y) {
            if (x.size() != y.size()) return false;
            for (std::size_t i = 0; i < x.size(); ++i)
                if (x[i].from != y[i].from || x[i].to != y[i].to || x[i].score != y[i].score) return false;
            return true;
        };
        const auto corridors_same = [](const std::vector<history_corridor>& x,
                                       const std::vector<history_corridor>& y) {
            if (x.size() != y.size()) return false;
            for (std::size_t i = 0; i < x.size(); ++i)
                if (x[i].a != y[i].a || x[i].b != y[i].b || x[i].uses != y[i].uses) return false;
            return true;
        };
        std::printf("      setup reads the %s close (industrialisation_ran=%d)\n", last_close_name,
                    fixture.industrialisation_ran ? 1 : 0);
        check(grudges_same(fixture.setup_grudges, last_close.grudges)
           && !grudges_same(fixture.setup_grudges, fixture.pre_exploration_grudges),
              "R6.6  sentiment was seeded from the last close's grudge table (1960 with the span "
              "on), not the 1200 one");
        check(corridors_same(fixture.setup_corridors, last_close.surviving_corridors)
           && !corridors_same(fixture.setup_corridors, fixture.pre_exploration_corridors),
              "R6.7  roads were stamped from the last close's surviving network (1960 with the "
              "span on), not the 1200 one");
        // And with the span on, not the 1660 one either -- where the two closes
        // differ on this seed (REPORTED when they coincide: equality with both
        // then proves nothing either way).
        if (fixture.industrialisation_ran)
        {
            if (!grudges_same(eo.grudges, last_close.grudges))
                check(!grudges_same(fixture.setup_grudges, eo.grudges),
                      "R6.6b with the span on, sentiment was not seeded from the 1660 grudge table");
            else
                std::printf("      R6.6b not asked: the 1660 and 1960 grudge tables coincide on this seed\n");
            if (!corridors_same(eo.surviving_corridors, last_close.surviving_corridors))
                check(!corridors_same(fixture.setup_corridors, eo.surviving_corridors),
                      "R6.7b with the span on, roads were not stamped from the 1660 network");
            else
                std::printf("      R6.7b not asked: the 1660 and 1960 networks coincide on this seed\n");
        }

        // REPORTED, not gated: how far 1660 moved from 1200 on this seed.
        const auto pair_in = [](const std::vector<grudge>& v, int f, int t) {
            for (const grudge& g : v) if (g.from == f && g.to == t) return true;
            return false;
        };
        int g_only_1200 = 0, g_only_1660 = 0;
        for (const grudge& g : fixture.pre_exploration_grudges)
            if (!pair_in(eo.grudges, g.from, g.to)) ++g_only_1200;
        for (const grudge& g : eo.grudges)
            if (!pair_in(fixture.pre_exploration_grudges, g.from, g.to)) ++g_only_1660;
        const auto corr_in = [](const std::vector<history_corridor>& v, int a, int b) {
            for (const history_corridor& c : v) if (c.a == a && c.b == b) return true;
            return false;
        };
        int c_only_1200 = 0, c_only_1660 = 0;
        for (const history_corridor& c : fixture.pre_exploration_corridors)
            if (!corr_in(eo.surviving_corridors, c.a, c.b)) ++c_only_1200;
        for (const history_corridor& c : eo.surviving_corridors)
            if (!corr_in(fixture.pre_exploration_corridors, c.a, c.b)) ++c_only_1660;
        std::printf("      handoff 1200 -> 1660: grudges %zu -> %zu (%d dropped, %d new); "
                    "corridors %zu -> %zu (%d dropped, %d new)\n",
                    fixture.pre_exploration_grudges.size(), eo.grudges.size(),
                    g_only_1200, g_only_1660,
                    fixture.pre_exploration_corridors.size(), eo.surviving_corridors.size(),
                    c_only_1200, c_only_1660);
        std::printf("      world setup: grudge_sentiment_rows=%lld dropped=%lld "
                    "stamped_corridors=%lld\n",
                    static_cast<long long>(rep.grudge_sentiment_rows),
                    static_cast<long long>(rep.grudge_sentiment_dropped),
                    static_cast<long long>(rep.prehistory_corridors));
        int subjects = 0;
        for (const polity& q : eo.polities) if (q.alive && q.overlord >= 0) ++subjects;
        std::printf("      handoff 1660: regions=%zu polities=%zu holdings=%zu subjects=%d "
                    "standing_dated_objects=%zu contacts=%zu wants=%zu culture_prefs=%zu\n",
                    eo.regions.size(), eo.polities.size(), eo.holdings.size(), subjects,
                    eo.dated_objects.size(), eo.contacts.size(), eo.wants.size(),
                    eo.culture_preference.size());
    }

    // -----------------------------------------------------------------
    // R4: objects with a term
    // -----------------------------------------------------------------
    {
        std::vector<dated_object> objs;
        objs.push_back(dated_object{1210, 1, 0, 1});
        objs.push_back(dated_object{1200, 2, 2, 3});
        objs.push_back(dated_object{1660, 3, 4, 5});

        expire_dated_objects(objs, 1199);
        check(objs.size() == 3, "R4.1  nothing expires before its own term");

        expire_dated_objects(objs, 1200);
        check(objs.size() == 2, "R4.2  a term ending AT the current year expires (<=, not <)");

        expire_dated_objects(objs, 1210);
        check(objs.size() == 1, "R4.3  a later term expires once its own year arrives");

        expire_dated_objects(objs, 1660);
        check(objs.empty(), "R4.4  every remaining term is gone once the span's own close arrives");
    }

    // -----------------------------------------------------------------
    // R5: the upkeep hook is callable and EARNS (BL-932 gave it a body)
    // -----------------------------------------------------------------
    {
        std::vector<region> regions(2);
        regions[0].nation = 0; regions[0].farm_q = 1000; regions[0].ore_q = 1000;
        regions[0].energy_q = 1000; regions[0].port_q = 1000;
        regions[0].has_market = true;
        regions[1].nation = 1; // a poor, unconnected polity's capital: no endowment set

        // BL-954: ids set explicitly. With the default id (-1) no region read
        // as held, so the endowment term earned nothing and R5.1 passed only
        // on the flat market income BL-954 removed -- it now tests endowment.
        std::vector<polity> qs(2);
        qs[0].id = 0; qs[0].capital = 0; qs[0].capacity[0] = 3;
        qs[1].id = 1; qs[1].capital = 1; qs[1].cohesion_q = 700;
        history_sim_params ep2;
        ep2.start_year = 1200; // consolidation year: not this call's `year`
        ep2.consolidation_year    = 1200; // BL-1036: the anchors are explicit now,
        ep2.near_home_cutoff_year = 1200; // no longer read off `start_year`.

        run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep2, /*year=*/1234, /*step_years=*/4);

        check(regions[0].treasury > regions[1].treasury,
              "R5.1  a rich, market-carrying capital earns more treasury than a bare one");
        check(regions[1].treasury == 0,
              "R5.2  a capital with no endowment, no market and no corridor touch earns nothing");
        check(qs[0].capacity[0] == 3 && qs[1].cohesion_q == 700,
              "R5.3  upkeep does not disturb fields it has no business touching");

        // R5.4: the ONE-TIME consolidation, at year == consolidation_year only.
        // BL-935's PAY/INVEST now spends from this same treasury the very
        // same call, so this test isolates EARN's own consolidation act by
        // switching every BL-935 cost off -- R5.5/R5.6 below cover spend.
        history_sim_params ep_consolidate_only = ep2;
        ep_consolidate_only.port_build_cost_q = 0;
        ep_consolidate_only.navy_build_cost_q = 0;
        ep_consolidate_only.standing_army_build_cost_q = 0;
        regions[0].material_stock = 500;
        // BL-998 (Ben, 2026-09-15, NR-871): EVERY held seat folds, not the
        // capital alone. Give polity 0 a second, non-capital seat carrying
        // its own hoard, and polity 1 a seat of its own that must NOT fold
        // into polity 0's purse (nation is the filter, not is_seat alone).
        regions[0].is_seat = true;
        regions.push_back(region{});
        regions[2].nation = 0; regions[2].is_seat = true; regions[2].material_stock = 300;
        regions.push_back(region{});
        regions[3].nation = 1; regions[3].is_seat = true; regions[3].material_stock = 70;
        // A hinterland region (not a seat) with a stray stock is left alone:
        // stores only ever accumulate on seats, and the fold reads seats only.
        regions.push_back(region{});
        regions[4].nation = 0; regions[4].is_seat = false; regions[4].material_stock = 11;
        const int64_t before_treasury   = regions[0].treasury;
        const int64_t before_treasury_1 = regions[1].treasury;
        run_exploration_upkeep(regions, qs, {}, ep_consolidate_only, /*year=*/1200, /*step_years=*/4);
        check(regions[0].treasury >= before_treasury + 500 && regions[0].material_stock == 0,
              "R5.4  consolidation folds material_stock into treasury once, at the span's own "
              "start year, and empties the seat's material_stock");
        check(regions[0].treasury >= before_treasury + 800 && regions[2].material_stock == 0,
              "R5.4b a two-seat polity opens with BOTH seats' stock in its treasury and the "
              "non-capital seat at 0 (BL-998)");
        check(regions[1].treasury == before_treasury_1 + 70 && regions[3].material_stock == 0
           && regions[4].material_stock == 11,
              "R5.4c another polity's seat folds into ITS capital, not a neighbour's, and a "
              "non-seat region's stray stock is not swept (BL-998)");
    }

    // -----------------------------------------------------------------
    // R5b: BL-935 -- ports, navies and standing armies, PAY then INVEST
    // -----------------------------------------------------------------
    {
        // BL-955: a purchase is now ONE scored choice per polity per round, so
        // the three stocks can no longer all fire off one funded call. Each
        // is driven by a situation that asks for it: an EXPANSIONIST seat with
        // a port window (port, then navy once the port stands) beside a
        // CONSOLIDATOR seat (standing army). Both purses are full.
        spend_fixture fx;
        add_spend_polity(fx, lean_culture(/*sea*/1000, /*zeal*/10, /*dominion*/0), 100000,
                         /*port window*/1000, /*port stock*/0);
        add_spend_polity(fx, lean_culture(/*sea*/0, /*zeal*/0, /*dominion*/10), 100000,
                         /*port window*/1000, /*port stock*/0);
        const exploration_spend_context ctx = spend_context(fx);
        std::vector<region>& regions = fx.regions;
        std::vector<polity>& qs      = fx.state.polities;

        history_sim_params ep;
        ep.start_year = 9999; // never this call's `year` -- no consolidation noise
        ep.consolidation_year    = 9999; // BL-1036: the anchors are explicit now;
        ep.near_home_cutoff_year = 9999; // both keep the value start_year gave them.

        exploration_upkeep_spend spend;
        run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep, /*year=*/1234,
                               /*step_years=*/1, &spend, nullptr, nullptr, &ctx);
        check(regions[0].port_stock_q > 0 && spend.ports == ep.port_build_cost_q,
              "R5.5  a funded port raises `port_stock_q` and spends the treasury doing it");
        const bool army_funded = regions[1].army_stock > 0
                              && spend.standing_armies == ep.standing_army_build_cost_q;

        regions[0].port_stock_q = 500; // the port stands: clears `navy_min_port_stock_q`
        exploration_upkeep_spend spend_navy;
        run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep, /*year=*/1235,
                               /*step_years=*/1, &spend_navy, nullptr, nullptr, &ctx);
        check(qs[0].navy_stock > 0 && spend_navy.navies == ep.navy_build_cost_q,
              "R5.6  a funded, port-staged navy grows and spends the treasury doing it");
        check(army_funded,
              "R5.7  a funded standing army adds to `army_stock` and spends the treasury doing it");

        // R5.8: underfunded, a built port silts and the navy decays -- a
        // fleet is a running cost, never a one-time purchase. BL-972: that
        // running cost is a BILL; a fleet whose bill is met holds, and the
        // hulls the purse cannot keep are what decay.
        {
            const int64_t navy_paid_before = qs[0].navy_stock;
            exploration_upkeep_spend spend_paid;
            run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep, /*year=*/1235,
                                   /*step_years=*/1, &spend_paid, nullptr, nullptr, &ctx);
            check(qs[0].navy_stock >= navy_paid_before && spend_paid.navy_unpaid == 0
               && spend_paid.navy_upkeep
                    == (navy_paid_before * ep.navy_upkeep_per_1000_units_year_q) / 1000,
                  "R5.9a a fleet whose bill the purse meets does not decay, and the bill charged is "
                  "navy_stock x navy_upkeep_per_1000_units_year_q / 1000 per year (BL-972)");
        }
        regions[0].treasury = 0;
        const int port_before = regions[0].port_stock_q;
        const int64_t navy_before = qs[0].navy_stock;
        exploration_upkeep_spend spend_unpaid;
        run_exploration_upkeep(regions, qs, /*corridors=*/{}, ep, /*year=*/1235,
                               /*step_years=*/1, &spend_unpaid);
        check(regions[0].port_stock_q < port_before,
              "R5.8  an underfunded port silts toward nothing");
        check(qs[0].navy_stock < navy_before && spend_unpaid.navy_unpaid == 1,
              "R5.9  a navy whose bill the purse cannot meet decays (treasury 0: the whole fleet is the "
              "unpaid share, BL-955's decay exactly) (BL-972)");
    }

    // -----------------------------------------------------------------
    // R8: BL-955 -- spend is ALLOCATED, not bought whenever affordable
    // -----------------------------------------------------------------
    {
        history_sim_params ep;
        ep.start_year = 1200; // contacts before this are near home; no consolidation at 1234
        ep.consolidation_year    = 1200; // BL-1036: the anchors are explicit now,
        ep.near_home_cutoff_year = 1200; // no longer read off `start_year`.

        const culture expansionist = lean_culture(/*sea*/1000, /*zeal*/10, /*dominion*/0);  // expn 1000, cons 0
        const culture middling     = lean_culture(/*sea*/500,  /*zeal*/5,  /*dominion*/5);  // expn 500,  cons 250
        const culture consolidator = lean_culture(/*sea*/0,    /*zeal*/0,  /*dominion*/10); // expn 0,    cons 1000

        // R8.0 -- the ranks: per-mille over living, cultured polities; ties share.
        {
            spend_fixture fx;
            add_spend_polity(fx, expansionist, 0, 0, 0);
            add_spend_polity(fx, middling, 0, 0, 0);
            add_spend_polity(fx, consolidator, 0, 0, 0);
            add_spend_polity(fx, consolidator, 0, 0, 0); // ties polity 2 on both leans
            add_spend_polity(fx, expansionist, 0, 0, 0);
            fx.state.polities[4].alive = false;          // dead: out of the set, reads 0
            std::vector<int> er, cr;
            exploration_lean_ranks(fx.state.polities, &fx.creeds, er, cr);
            std::printf("      ranks: expn=[%d %d %d %d %d] cons=[%d %d %d %d %d]\n",
                        er[0], er[1], er[2], er[3], er[4], cr[0], cr[1], cr[2], cr[3], cr[4]);
            check(er[0] == 1000 && er[1] == 666 && er[2] == 0 && er[3] == 0 && er[4] == 0
               && cr[0] == 0 && cr[1] == 333 && cr[2] == 666 && cr[3] == 666 && cr[4] == 0,
                  "R8.0  lean ranks: strictly-lower count x 1000 / (n-1) over living cultured polities; "
                  "ties share a rank; the dead read 0 (BL-955)");
            std::vector<int> er0, cr0;
            exploration_lean_ranks(fx.state.polities, nullptr, er0, cr0);
            check(er0.size() == 5 && cr0.size() == 5
               && std::all_of(er0.begin(), er0.end(), [](int v) { return v == 0; })
               && std::all_of(cr0.begin(), cr0.end(), [](int v) { return v == 0; }),
                  "R8.0b with no creeds every polity ranks 0 on both leans (BL-955)");
        }

        // R8.1 -- the same full purse and the same coastal seat, two creeds.
        {
            spend_fixture fx;
            add_spend_polity(fx, expansionist, 100000, 1000, 0);
            add_spend_polity(fx, consolidator, 100000, 1000, 0);
            const exploration_spend_context ctx = spend_context(fx);
            exploration_upkeep_spend spend;
            run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234, 4, &spend,
                                   nullptr, nullptr, &ctx);
            const bool expn_outward = fx.regions[0].port_stock_q > 0 || fx.state.polities[0].navy_stock > 0;
            const bool cons_outward = fx.regions[1].port_stock_q > 0 || fx.state.polities[1].navy_stock > 0;
            std::printf("      R8.1: expansionist port=%d navy=%lld army=%lld | consolidator port=%d "
                        "navy=%lld army=%lld\n",
                        fx.regions[0].port_stock_q, static_cast<long long>(fx.state.polities[0].navy_stock),
                        static_cast<long long>(fx.regions[0].army_stock), fx.regions[1].port_stock_q,
                        static_cast<long long>(fx.state.polities[1].navy_stock),
                        static_cast<long long>(fx.regions[1].army_stock));
            check(expn_outward && !cons_outward,
                  "R8.1  a coastal expansion-ranked polity with a full purse buys a port/navy step; a "
                  "consolidator-ranked polity with the identical purse and seat does not (BL-955)");
        }

        // R8.2 -- a fully funded polity buys at most one stock per round, over many rounds.
        {
            spend_fixture fx;
            add_spend_polity(fx, expansionist, 10000000, 1000, 0);
            add_spend_polity(fx, middling, 10000000, 1000, 0);
            add_spend_polity(fx, consolidator, 10000000, 1000, 0);
            const exploration_spend_context ctx = spend_context(fx);
            bool at_most_one = true, counters_agree = true;
            int64_t steps_total = 0, navy_steps = 0;
            for (int round = 0; round < 24; ++round)
            {
                std::vector<int>     port_before, navy_before, army_before;
                for (std::size_t i = 0; i < fx.regions.size(); ++i)
                {
                    port_before.push_back(fx.regions[i].port_stock_q);
                    navy_before.push_back(static_cast<int>(fx.state.polities[i].navy_stock));
                    army_before.push_back(static_cast<int>(fx.regions[i].army_stock));
                }
                exploration_upkeep_spend spend;
                run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234 + round * 4, 4,
                                       &spend, nullptr, nullptr, &ctx);
                int rises = 0;
                for (std::size_t i = 0; i < fx.regions.size(); ++i)
                {
                    const int r = (fx.regions[i].port_stock_q > port_before[i] ? 1 : 0)
                                + (fx.state.polities[i].navy_stock > navy_before[i] ? 1 : 0)
                                + (fx.regions[i].army_stock > army_before[i] ? 1 : 0);
                    if (r > 1) at_most_one = false;
                    rises += r;
                }
                const int64_t steps = spend.port_steps + spend.navy_steps + spend.army_steps;
                if (steps != rises) counters_agree = false;
                steps_total += steps;
                navy_steps  += spend.navy_steps;
            }
            std::printf("      R8.2: 24 rounds x 3 funded polities -> %lld steps bought (%lld navy)\n",
                        static_cast<long long>(steps_total), static_cast<long long>(navy_steps));
            check(at_most_one && counters_agree && steps_total > 0 && steps_total <= 24 * 3,
                  "R8.2  a fully funded polity builds at most one stock per round (24 rounds, 3 polities; "
                  "the step counters match the stocks that rose) (BL-955)");
        }

        // R8.3 -- Alarm from a long-known neighbour tips a middling consolidator to the army.
        {
            const auto run_middle = [&](int contact_year) {
                spend_fixture fx;
                add_spend_polity(fx, expansionist, 0, 0, 0);
                add_spend_polity(fx, middling, 100000, /*no port window*/0, 0);
                add_spend_polity(fx, consolidator, 0, 0, 0);
                fx.regions[2].army_stock = 100000; // a visible, standing threat (>= the reference)
                contact c12; c12.from = 1; c12.to = 2; c12.first.year = contact_year;
                contact c21; c21.from = 2; c21.to = 1; c21.first.year = contact_year;
                fx.state.contacts = { c12, c21 }; // sorted (from, to)
                const exploration_spend_context ctx = spend_context(fx);
                const int64_t army_before = fx.regions[1].army_stock;
                exploration_upkeep_spend spend;
                run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234, 4, &spend,
                                       nullptr, nullptr, &ctx);
                return fx.regions[1].army_stock > army_before;
            };
            const bool near_home_buys = run_middle(/*contact_year=*/1000); // met before the span
            const bool frontier_buys  = run_middle(/*contact_year=*/1210); // met during it
            check(near_home_buys && !frontier_buys,
                  "R8.3  a middling consolidator holds without Alarm and buys the army step once a "
                  "long-known neighbour's standing force alarms it; a frontier contact does not (BL-955)");
        }

        // R8.8 -- BL-972: THE BILL BINDS. A polity standing the old cap's
        // worth of paid heads (BL-955's 300 per held region, one region) on a
        // seat earning the most the endowment term can pay is poorer every
        // round, whatever else it chooses: the bill (300 x 1667 x 4 / 1000 =
        // 2000 a round) is an order above the income ((3 x 1000 + 0) / 4 = 750
        // mean endowment x 40 / 1000 x 4 years = 120). This is the proof the
        // cap could be deleted on: the cost does what the cap did, in the world.
        {
            spend_fixture fx;
            add_spend_polity(fx, consolidator, 100000, /*no port window*/0, 0);
            add_spend_polity(fx, expansionist, 0, 0, 0);
            fx.regions[0].farm_q = fx.regions[0].ore_q = fx.regions[0].energy_q = 1000;
            const int64_t cap_heads = 300; // BL-955's old army_saturation_per_region x 1 held region
            fx.regions[0].army_stock = cap_heads; fx.regions[0].standing_army = cap_heads;
            fx.regions[0].standing_army_owner = 0;
            const exploration_spend_context ctx = spend_context(fx);
            bool poorer_every_round = true;
            int64_t prev = fx.regions[0].treasury, first_bill = 0, steps = 0;
            for (int round = 0; round < 8; ++round)
            {
                exploration_upkeep_spend spend;
                run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234 + round * 4, 4, &spend,
                                       nullptr, nullptr, &ctx);
                if (round == 0) first_bill = spend.army_upkeep;
                steps += spend.army_steps;
                if (fx.regions[0].treasury >= prev) poorer_every_round = false;
                prev = fx.regions[0].treasury;
            }
            const int64_t expected_bill =
                (cap_heads * ep.standing_army_upkeep_per_1000_heads_year_q * 4) / 1000;
            const int64_t income = ((3000 / 4) * ep.treasury_endowment_income_q / 1000) * 4;
            std::printf("      R8.8: first-round bill=%lld (expected %lld) round income=%lld army steps=%lld "
                        "treasury 100000 -> %lld over 8 rounds\n",
                        static_cast<long long>(first_bill), static_cast<long long>(expected_bill),
                        static_cast<long long>(income), static_cast<long long>(steps),
                        static_cast<long long>(fx.regions[0].treasury));
            check(poorer_every_round && first_bill == expected_bill && expected_bill > income,
                  "R8.8  a polity standing the old cap's paid heads is poorer every round: the per-head bill "
                  "exceeds the seat's whole endowment income, so the cost binds where the cap used to (BL-972)");
        }

        // R8.9-R8.12 -- the PAID standing army persists through the muster,
        // decays only when unfunded, dies with the pool, and saturates.
        {
            spend_fixture fx;
            add_spend_polity(fx, consolidator, 100000, 0, 0);
            add_spend_polity(fx, expansionist, 0, 0, 0); // a second realm, so the leans rank
            fx.regions[0].population = 200000; // a real garrison target
            const int64_t target = garrison_target(fx.regions[0], ep.garrison_fraction_q);
            fx.regions[0].army_stock = target;  // the muster's own garrison, exactly at target
            const exploration_spend_context ctx = spend_context(fx);
            const int64_t manpower_before = fx.regions[0].manpower_stock; // BL-972: the levy's pool
            exploration_upkeep_spend spend;
            run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234, 4, &spend,
                                   nullptr, nullptr, &ctx);
            const int64_t paid = standing_army_heads(fx.regions[0]);
            const int64_t stock_bought = fx.regions[0].army_stock;
            check(spend.levy_raised == paid && paid > 0
               && fx.regions[0].manpower_stock == manpower_before - paid,
                  "R8.9b the bought heads are a LEVY: the seat's manpower_stock fell by exactly the step "
                  "bought, and the step counter agrees (BL-972)");

            // A year of the muster, disbanding HALF of any excess.
            region mustered = fx.regions[0];
            muster_garrison(mustered, ep.garrison_fraction_q, ep.garrison_muster_q, /*disband*/500);
            region unpaid = fx.regions[0];
            unpaid.standing_army = 0; // the same men, unpaid
            muster_garrison(unpaid, ep.garrison_fraction_q, ep.garrison_muster_q, /*disband*/500);
            std::printf("      R8.9: target=%lld bought stock=%lld paid=%lld | after muster: paid-stock=%lld "
                        "unpaid-stock=%lld\n", static_cast<long long>(target),
                        static_cast<long long>(stock_bought), static_cast<long long>(paid),
                        static_cast<long long>(mustered.army_stock), static_cast<long long>(unpaid.army_stock));
            check(spend.army_steps == 1 && paid == ep.standing_army_build_step_q
               && mustered.army_stock == stock_bought && standing_army_heads(mustered) == paid
               && unpaid.army_stock < stock_bought,
                  "R8.9  a bought standing army survives a year of muster intact; the same heads unpaid "
                  "are disbanded as excess (BL-955)");

            // Unpaid next round (empty purse): the whole paid army is the
            // unpaid share, so BL-955's decay applies in full -- and BL-972
            // sends the men HOME: they leave army_stock and return to the
            // seat's manpower pool. The levy returns on disband.
            fx.regions[0].treasury = 0;
            const int64_t manpower_before_decay = fx.regions[0].manpower_stock;
            exploration_upkeep_spend spend2;
            run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1238, 4, &spend2,
                                   nullptr, nullptr, &ctx);
            const int64_t gone     = (paid * ep.standing_army_decay_per_mille_year_q * 4) / 1000;
            const int64_t expected = paid - gone;
            std::printf("      R8.10: paid=%lld unpaid -> %lld go home (army %lld -> %lld, manpower %lld -> %lld)\n",
                        static_cast<long long>(paid), static_cast<long long>(gone),
                        static_cast<long long>(stock_bought), static_cast<long long>(fx.regions[0].army_stock),
                        static_cast<long long>(manpower_before_decay),
                        static_cast<long long>(fx.regions[0].manpower_stock));
            check(spend2.army_steps == 0 && spend2.army_unpaid == 1
               && standing_army_heads(fx.regions[0]) == expected
               && fx.regions[0].army_stock == stock_bought - gone
               && fx.regions[0].manpower_stock == manpower_before_decay + gone
               && spend2.levy_returned == gone,
                  "R8.10 unpaid, the paid standing army decays at standing_army_decay_per_mille_year_q and "
                  "the men go home: army_stock falls by the same heads and the seat's manpower_stock rises "
                  "by them -- the levy returns on disband (BL-972; the rate is BL-955's)");

            // Lost in battle alike: a pool losing half loses half its paid heads;
            // ground taken by another polity carries none of the loser's.
            region battle = fx.regions[0];
            battle.army_stock = 1000; battle.standing_army = 400; battle.standing_army_owner = battle.nation;
            battle.army_stock = 500;
            scale_standing_army(battle, 1000);
            region taken = battle;
            taken.nation = 7; // conquered: `tgt.nation` moves, the paid status does not
            check(battle.standing_army == 200 && standing_army_heads(taken) == 0,
                  "R8.11 a battle loss falls on paid heads in proportion (1000->500 pool, 400->200 paid), "
                  "and ground that changes hands carries none of the loser's paid army (BL-955)");

            // BL-972: the bill binds on a real-sized realm too. 120 held
            // regions at the old cap (36,000 paid heads), every region's
            // endowment maxed and 200 corridors touched -- the most the
            // non-tribute terms can earn in a round -- and the round still
            // ends poorer, with the whole realm billed as one.
            {
                spend_fixture rf;
                add_spend_polity(rf, consolidator, 10000000, /*no port window*/0, 0);
                add_spend_polity(rf, expansionist, 0, 0, 0);
                rf.regions[0].farm_q = rf.regions[0].ore_q = rf.regions[0].energy_q = 1000;
                rf.regions[0].army_stock = 300; rf.regions[0].standing_army = 300;
                rf.regions[0].standing_army_owner = 0;
                for (int i = 1; i < 120; ++i)
                {
                    region held;
                    held.nation = 0; held.population = 200000;
                    held.farm_q = held.ore_q = held.energy_q = held.port_q = 1000;
                    held.army_stock = 300; held.standing_army = 300; held.standing_army_owner = 0;
                    rf.regions.push_back(held);
                }
                std::vector<history_corridor> corridors(200);
                for (int i = 0; i < 200; ++i)
                {
                    corridors[static_cast<std::size_t>(i)].a = 0;
                    corridors[static_cast<std::size_t>(i)].b = static_cast<uint16_t>(2 + (i % 118));
                }
                const exploration_spend_context ctx = spend_context(rf);
                const int64_t before = rf.regions[0].treasury;
                exploration_upkeep_spend spend;
                run_exploration_upkeep(rf.regions, rf.state.polities, corridors, ep, 1234, 4, &spend,
                                       nullptr, nullptr, &ctx);
                const int64_t income_max = (1000 * ep.treasury_endowment_income_q / 1000) * 4
                                         + 200 * ep.treasury_corridor_income_q * 4;
                const int64_t expected_bill =
                    (36000 * ep.standing_army_upkeep_per_1000_heads_year_q * 4) / 1000;
                std::printf("      R8.12: 120 regions x 300 paid -> bill=%lld (expected %lld) vs max income=%lld; "
                            "treasury %lld -> %lld\n", static_cast<long long>(spend.army_upkeep),
                            static_cast<long long>(expected_bill), static_cast<long long>(income_max),
                            static_cast<long long>(before), static_cast<long long>(rf.regions[0].treasury));
                check(spend.army_upkeep == expected_bill && expected_bill > income_max
                   && rf.regions[0].treasury < before && spend.army_unpaid == 0,
                      "R8.12 the bill binds on a real-sized realm: 120 regions x 300 paid heads are billed as "
                      "one realm, and the bill exceeds the most the endowment and corridor terms can earn in a "
                      "round (BL-972)");
            }
        }

        // R8.13-R8.17 -- the cold review's cases (F1, F2, F4, F5b).
        {
            // F1: paid men outnumber the survivors. 1000 heads, all paid, 60% lost.
            region r;
            r.nation = 0; r.standing_army_owner = 0;
            r.army_stock = 1000; r.standing_army = 1000;
            r.army_stock = 400;
            scale_standing_army(r, 1000);
            check(r.standing_army == 400 && standing_army_invariant_holds(r),
                  "R8.13 a 60% loss on an all-paid 1000-head pool leaves 400 paid (not 160): the paid "
                  "count is read against the pool before the loss (BL-955 F1)");

            // F1: a march cannot carry more paid heads than men.
            region g;
            g.nation = 0; g.standing_army_owner = 0;
            g.army_stock = 1000; g.standing_army = 1000;
            const int64_t drawn_part = draw_army_with_standing(g, 600);
            region h = g; // what is left: 400 all paid
            const int64_t drawn_all = draw_army_with_standing(h, 10000);
            region mixed;
            mixed.nation = 0; mixed.standing_army_owner = 0;
            mixed.army_stock = 1000; mixed.standing_army = 300;
            const int64_t drawn_mixed = draw_army_with_standing(mixed, 500);
            check(drawn_part == 600 && g.army_stock == 400 && g.standing_army == 400
               && drawn_all == 400 && h.army_stock == 0 && h.standing_army == 0
               && drawn_mixed == 150 && mixed.standing_army == 150
               && standing_army_invariant_holds(g) && standing_army_invariant_holds(mixed),
                  "R8.14 a gather draws paid heads in proportion and never more paid heads than men "
                  "committed (600 of an all-paid 1000 -> 600; 500 of 1000 with 300 paid -> 150) (BL-955 F1)");

            // F2: the scorer's defender estimate mirrors the battle-path muster.
            region d;
            d.nation = 0; d.standing_army_owner = 0;
            d.population = 200000;
            const int64_t d_target = garrison_target(d, ep.garrison_fraction_q);
            d.army_stock = d_target / 2; d.standing_army = d_target / 2; // every man paid
            d.manpower_stock = d_target * 4;
            const int64_t estimate = defender_levy_estimate(d, ep);
            region mustered = d;
            muster_garrison(mustered, ep.garrison_fraction_q, ep.defence_levy_q, ep.garrison_disband_q);
            std::printf("      R8.15: target=%lld stock=%lld (all paid) estimate=%lld battle-path muster=%lld\n",
                        static_cast<long long>(d_target), static_cast<long long>(d.army_stock),
                        static_cast<long long>(estimate), static_cast<long long>(mustered.army_stock));
            check(estimate == mustered.army_stock && estimate > d.army_stock,
                  "R8.15 for a region whose men are all paid, the scorer's defender estimate equals what "
                  "the battle-path muster fields: the levy reads ordinary men only (BL-955 F2)");

            // F4 under BL-972: the BILL reads the REALM's paid heads, not the
            // seat's -- heads a campaign marched onto other held ground are
            // billed to the capital that pays them, and when the bill is
            // short they go home from the ground they stand on.
            struct realm_bill { int64_t upkeep, unpaid, pool, stock; };
            const auto realm_run = [&](int64_t paid_off_seat, int64_t treasury) {
                spend_fixture fx;
                add_spend_polity(fx, consolidator, treasury, 0, 0);
                add_spend_polity(fx, expansionist, 0, 0, 0);
                region held;
                held.nation = 0; held.population = 200000; held.manpower_stock = 0;
                held.army_stock = paid_off_seat; held.standing_army = paid_off_seat;
                held.standing_army_owner = 0;
                fx.regions.push_back(held); // a second held region, not the seat
                const exploration_spend_context ctx = spend_context(fx);
                exploration_upkeep_spend spend;
                run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234, 4, &spend,
                                       nullptr, nullptr, &ctx);
                return realm_bill{spend.army_upkeep, spend.army_unpaid,
                                  fx.regions[2].manpower_stock, fx.regions[2].army_stock};
            };
            const realm_bill billed = realm_run(1000, 100000);
            const realm_bill broke  = realm_run(1000, 0);
            const int64_t off_seat_bill = (1000 * ep.standing_army_upkeep_per_1000_heads_year_q * 4) / 1000;
            const int64_t home = (1000 * ep.standing_army_decay_per_mille_year_q * 4) / 1000;
            check(billed.upkeep == off_seat_bill && billed.unpaid == 0 && billed.pool == 0
               && broke.unpaid == 1 && broke.pool == home && broke.stock == 1000 - home,
                  "R8.16 paid heads standing off the seat are billed to the seat that pays them; unpaid, they "
                  "go home into the pool of the ground they stand on (BL-972; BL-955 F4's realm read)");

            // F5b under BL-972: one hull with a full purse has its bill met
            // and does not decay at all; the same hull with a purse under one
            // unit's rate is unpaid, decays to zero, cannot be rebuilt (the
            // step costs more than the bill it could not meet) and IS a
            // lapse. The lapse is still recorded after the step, as F5b set.
            {
                struct one_hull { int64_t navy, steps, unpaid, lapses; };
                // The poor seat carries NO port window: a window is endowment
                // too, and even a quarter of one earns 40 a round -- enough to
                // meet one hull's bill of 6. (Its port stock still clears
                // navy_min_port_stock_q for the read; nothing is bought.)
                const auto run_one_hull = [&](int64_t treasury, int port_window_q) {
                    spend_fixture fx;
                    add_spend_polity(fx, expansionist, treasury, port_window_q, /*port full*/1000);
                    add_spend_polity(fx, consolidator, 0, 0, 0);
                    fx.state.polities[0].navy_stock = 1;
                    const exploration_spend_context ctx = spend_context(fx);
                    exploration_upkeep_spend spend;
                    run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234, 4, &spend,
                                           nullptr, nullptr, &ctx);
                    return one_hull{fx.state.polities[0].navy_stock, spend.navy_steps, spend.navy_unpaid,
                                    static_cast<int64_t>(spend.navies_lapsed.size())};
                };
                const one_hull rich = run_one_hull(100000, /*window*/1000);
                const one_hull poor = run_one_hull(1, /*no window*/0); // under one hull's four-year rate (6)
                check(rich.steps == 1 && rich.navy == 1 + ep.navy_build_step_q && rich.unpaid == 0
                   && rich.lapses == 0
                   && poor.steps == 0 && poor.navy == 0 && poor.unpaid == 1 && poor.lapses == 1,
                      "R8.17 one hull with a full purse is paid, holds and grows; the same hull with a purse "
                      "under its rate is unpaid, lapses to zero and is recorded once (BL-972; BL-955 F5b's "
                      "record-after-step order kept)");
            }
        }

        // R8.4 -- the decays are untouched: BL-935's R5.8/R5.9 above run unchanged
        // on a round that buys nothing. Here: a navy bought and then held lapses.
        {
            spend_fixture fx;
            add_spend_polity(fx, consolidator, 0, 0, 0);
            fx.state.polities[0].navy_stock = 3;
            const exploration_spend_context ctx = spend_context(fx);
            exploration_upkeep_spend spend;
            run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234, 4, &spend,
                                   nullptr, nullptr, &ctx);
            run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1238, 4, &spend,
                                   nullptr, nullptr, &ctx);
            run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1242, 4, &spend,
                                   nullptr, nullptr, &ctx);
            check(fx.state.polities[0].navy_stock == 0 && spend.navies_lapsed.size() == 1
               && spend.navies_lapsed[0] == 0,
                  "R8.4  an unfunded navy still decays every round, and its lapse to zero is recorded "
                  "once (BL-955)");
        }

        // R8.19 -- BL-972: with the cap gone, a cap-free allocation still does
        // not buy an army every round. Two in-world forces stop it, and each
        // is shown alone: (a) the PURSE -- a modest treasury with no income
        // buys steps until the bill plus the next step outrun it, then the
        // unpaid share goes home; (b) the LEVY -- a bottomless purse on a seat
        // whose pool is finite buys only while the pool lends a whole step,
        // then holds.
        {
            struct rounds_out { int64_t steps, unpaid_rounds, returned, paid, treasury, manpower; };
            const auto run_rounds = [&](int64_t treasury, int64_t manpower, int rounds) {
                spend_fixture fx;
                add_spend_polity(fx, consolidator, treasury, 0, 0);
                add_spend_polity(fx, expansionist, 0, 0, 0);
                fx.regions[0].manpower_stock = manpower;
                fx.regions[0].population     = 2000000; // ceiling 100,000: the pool, not its cap, binds
                const exploration_spend_context ctx = spend_context(fx);
                int64_t steps = 0, unpaid_rounds = 0, returned = 0;
                for (int round = 0; round < rounds; ++round)
                {
                    exploration_upkeep_spend spend;
                    run_exploration_upkeep(fx.regions, fx.state.polities, {}, ep, 1234 + round * 4, 4,
                                           &spend, nullptr, nullptr, &ctx);
                    steps += spend.army_steps; unpaid_rounds += spend.army_unpaid;
                    returned += spend.levy_returned;
                }
                return rounds_out{steps, unpaid_rounds, returned, standing_army_heads(fx.regions[0]),
                                  fx.regions[0].treasury, fx.regions[0].manpower_stock};
            };
            const rounds_out purse = run_rounds(/*treasury*/6000, /*manpower*/100000, 30);
            const rounds_out levy  = run_rounds(/*treasury*/10000000, /*manpower*/10000, 60);
            std::printf("      R8.19: purse-bound (6000, no income, 30 rounds): steps=%lld unpaid rounds=%lld "
                        "sent home=%lld paid at close=%lld treasury=%lld\n",
                        static_cast<long long>(purse.steps), static_cast<long long>(purse.unpaid_rounds),
                        static_cast<long long>(purse.returned), static_cast<long long>(purse.paid),
                        static_cast<long long>(purse.treasury));
            std::printf("      R8.19: levy-bound (10M, pool 10000, 60 rounds): steps=%lld unpaid rounds=%lld "
                        "paid at close=%lld pool left=%lld\n",
                        static_cast<long long>(levy.steps), static_cast<long long>(levy.unpaid_rounds),
                        static_cast<long long>(levy.paid), static_cast<long long>(levy.manpower));
            check(purse.steps > 0 && purse.steps < 30 && purse.unpaid_rounds > 0 && purse.returned > 0,
                  "R8.19a purse-bound: a modest treasury with no income buys some army steps, not one every "
                  "round; once the bill outruns the purse the unpaid share goes home (BL-972)");
            check(levy.steps > 0 && levy.steps < 60 && levy.steps <= 32 && levy.unpaid_rounds == 0
               && levy.manpower < ep.standing_army_build_step_q * 2,
                  "R8.19b levy-bound: a bottomless purse buys only while the seat's pool lends a whole step "
                  "(10,000 banked at 500 per mille: at most 32 steps of 300 in 60 rounds), then holds (BL-972)");
        }

        // R8.5 -- the argmax tie order: hold, then army, then port, then navy.
        {
            exploration_spend_scores s;
            s.army_eligible = s.port_eligible = s.navy_eligible = true;
            s.hold_q = s.army_q = s.port_q = s.navy_q = 500;
            const bool all_tie_hold = choose_exploration_spend(s) == exploration_spend_option::hold;
            s.hold_q = 250;
            const bool tie_army = choose_exploration_spend(s) == exploration_spend_option::army_step;
            s.army_q = 400;
            const bool tie_port = choose_exploration_spend(s) == exploration_spend_option::port_step;
            s.port_q = 400;
            const bool navy_top = choose_exploration_spend(s) == exploration_spend_option::navy_step;
            s.navy_eligible = false; s.navy_q = 900;
            const bool ineligible_loses = choose_exploration_spend(s) == exploration_spend_option::army_step;
            check(all_tie_hold && tie_army && tie_port && navy_top && ineligible_loses,
                  "R8.5  argmax is a total order: an exact tie goes to hold, then army, then port, then "
                  "navy; an ineligible option never wins (BL-955)");

            // And scoring: an empty purse makes nothing eligible, so hold.
            exploration_spend_facts f;
            f.expansion_rank_q = 1000; f.port_window_q = 1000; f.treasury = 0;
            check(choose_exploration_spend(score_exploration_spend(ep, f)) == exploration_spend_option::hold,
                  "R8.6  a purse that covers no cost holds, whatever the leans ask for (BL-955)");
            // BL-972: no fleet saturates in the scorer -- a fleet a hundred
            // times a realm's size scores exactly what an empty slip does;
            // its bill is what bounds it. A full port is still not eligible.
            f.treasury = 100000; f.port_stock_q = 1000;
            f.navy_stock = 40000;
            const exploration_spend_scores huge = score_exploration_spend(ep, f);
            f.navy_stock = 0;
            const exploration_spend_scores fresh = score_exploration_spend(ep, f);
            check(huge.navy_q == fresh.navy_q && fresh.navy_q > 0 && !fresh.port_eligible,
                  "R8.7  the navy score carries no saturation term (a 40,000-unit fleet scores as an empty "
                  "slip); a full port is not eligible for another step (BL-972)");
            // BL-972: the army step needs a whole step of levy room.
            exploration_spend_facts g;
            g.consolidator_rank_q = 1000; g.treasury = 100000;
            g.levy_room = ep.standing_army_build_step_q - 1;
            const bool short_pool = !score_exploration_spend(ep, g).army_eligible;
            g.levy_room = ep.standing_army_build_step_q;
            const bool whole_step = score_exploration_spend(ep, g).army_eligible;
            g.standing_army = 1000000; // a million paid heads: no cap in the score either
            const bool no_army_cap = score_exploration_spend(ep, g).army_q == 600;
            check(short_pool && whole_step && no_army_cap,
                  "R8.7b the army step is eligible only when the seat's pool lends a whole step "
                  "(levy_room >= standing_army_build_step_q), and its score carries no saturation term "
                  "(BL-972)");
        }
    }

    // -----------------------------------------------------------------
    // R7: BL-954 -- trade is a want met by throughput
    // -----------------------------------------------------------------
    {
        const int farm = scarcity_good_index(region_class::farm);

        // Polity 0 (the BUYER): one market capital holding nothing, so it
        // wants farm at the "held nowhere" level. Polity 1 (the SELLER): a
        // market capital dominant in nothing plus one farm region --
        // holding_q for farm is 500, and for every other good 0. A supply corridor joins buyer region 0 to seller
        // region 2; its line is the weaker side's reach, 600.
        std::vector<region> base_regions(3);
        base_regions[0].nation = 0; base_regions[0].has_market = true;
        base_regions[0].dominant = region_class::none; base_regions[0].network_supply_q = 1000;
        base_regions[1].nation = 1; base_regions[1].has_market = true;
        base_regions[1].dominant = region_class::none;
        base_regions[2].nation = 1; base_regions[2].dominant = region_class::farm;
        base_regions[2].network_supply_q = 600;

        std::vector<polity> base_qs(2);
        base_qs[0].id = 0; base_qs[0].alive = true; base_qs[0].capital = 0;
        base_qs[1].id = 1; base_qs[1].alive = true; base_qs[1].capital = 1;

        const std::vector<history_corridor> corridors = { history_corridor{0, 2, 5} };

        history_sim_params ep;
        ep.start_year = 9999; // no consolidation
        ep.consolidation_year    = 9999; // BL-1036: the anchors are explicit now;
        ep.near_home_cutoff_year = 9999; // both keep the value start_year gave them.
        ep.port_build_cost_q = 0; ep.navy_build_cost_q = 0; ep.standing_army_build_cost_q = 0;

        const std::vector<dated_object> no_trade = {
            dated_object{2000, static_cast<int32_t>(treaty_clause::non_aggression), 0, 1} };
        const std::vector<dated_object> with_trade = {
            dated_object{2000, static_cast<int32_t>(treaty_clause::non_aggression), 0, 1},
            dated_object{2000, static_cast<int32_t>(treaty_clause::trade_access), 0, 1} };

        // R7.1 -- no flow without the clause, even with want, holder and line.
        {
            std::vector<region> regions = base_regions;
            std::vector<polity> qs = base_qs;
            std::vector<trade_flow> flows = { trade_flow{9, 9, 0, 1} }; // must be overwritten
            run_exploration_upkeep(regions, qs, corridors, ep, 1234, 4, nullptr, &no_trade, &flows);
            check(flows.empty(),
                  "R7.1  no flow forms without a trade_access clause (non-aggression alone opens none)");
        }

        // R7.2-R7.6 -- the clause opens exactly the one flow the ground supports.
        std::vector<region> regions = base_regions;
        std::vector<polity> qs = base_qs;
        std::vector<trade_flow> flows;
        run_exploration_upkeep(regions, qs, corridors, ep, 1234, 4, nullptr, &with_trade, &flows);
        const bool one_flow = flows.size() == 1 && flows[0].seller == 1 && flows[0].buyer == 0
                           && flows[0].good == farm;
        check(one_flow, "R7.2  a trade_access clause opens the seller->buyer farm flow, and only it");
        const int raw_want = regions[0].scarcity_raw_q[farm];
        check(one_flow && raw_want == 700 && flows[0].volume_q == 500,
              "R7.3  volume is bounded by the seller's holding (min of want 700, holding 500, line 600)");
        check(one_flow && regions[0].scarcity_q[farm] == raw_want - flows[0].volume_q
           && regions[0].scarcity_raw_q[farm] == 700,
              "R7.4  a met want relieves the buyer's signal (raw kept, unmet = raw - inbound)");

        {
            std::vector<region> r_none = base_regions;
            std::vector<polity> q_none = base_qs;
            run_exploration_upkeep(r_none, q_none, corridors, ep, 1234, 4, nullptr, &no_trade, nullptr);
            const int64_t expected = (500LL * ep.treasury_trade_income_q * 4) / 1000;
            check(expected > 0
               && regions[0].treasury - r_none[0].treasury == expected
               && regions[1].treasury - r_none[1].treasury == expected,
                  "R7.5  the flow credits BOTH capitals, volume x treasury_trade_income_q x step / 1000");
        }

        // Want, holding and line each bound the volume on their own.
        {
            std::vector<region> rw = regions;
            rw[0].scarcity_raw_q[farm] = 120;
            const trade_context ctx = build_trade_context(rw, qs, corridors);
            check(trade_flow_volume_q(ctx, rw, qs, 1, 0, farm) == 120,
                  "R7.6  volume is bounded by the buyer's raw want");
        }
        {
            std::vector<region> rl = regions;
            rl[2].network_supply_q = 80;
            const trade_context ctx = build_trade_context(rl, qs, corridors);
            check(trade_flow_volume_q(ctx, rl, qs, 1, 0, farm) == 80,
                  "R7.7  volume is bounded by the land line (the weaker side's reach to the border)");
        }
        {
            // No corridor: the line is the sea, and only the SELLER's navy carries it.
            std::vector<region> rs = regions;
            std::vector<polity> qn = qs;
            rs[0].port_stock_q = 800; rs[1].port_stock_q = 300;
            const trade_context ctx = build_trade_context(rs, qn, /*corridors=*/{});
            check(trade_flow_volume_q(ctx, rs, qn, 1, 0, farm) == 0,
                  "R7.8  no corridor and no seller navy -> no line, no volume");
            qn[0].navy_stock = 500; // the BUYER's navy does not carry the seller's goods
            check(trade_flow_volume_q(ctx, rs, qn, 1, 0, farm) == 0,
                  "R7.9  a buyer's navy alone opens no sea line");
            qn[1].navy_stock = 500;
            check(trade_flow_volume_q(ctx, rs, qn, 1, 0, farm) == 300,
                  "R7.10 the seller's navy opens the sea line, bounded by the smaller built port");
        }

        // No flat market income remains: a market capital with no endowment,
        // no corridor and no trade earns nothing at all.
        {
            std::vector<region> rm(1);
            rm[0].nation = 0; rm[0].has_market = true;
            std::vector<polity> qm(1);
            qm[0].id = 0; qm[0].alive = true; qm[0].capital = 0;
            run_exploration_upkeep(rm, qm, {}, ep, 1234, 4);
            check(rm[0].treasury == 0,
                  "R7.11 a market capital with no endowment, corridor or trade earns nothing (no flat market income)");
        }

        // A pair with trade to open is worth more to bind than the same pair without.
        {
            const trade_context ctx = build_trade_context(regions, qs, corridors);
            const int trade_value = pair_trade_value_q(ctx, regions, qs, 0, 1, flows);
            check(trade_value == 500,
                  "R7.12 pair_trade_value_q reads the flow the clause WOULD open, before any binding");
            for (const bool near : {true, false})
            {
                const int without = treaty_value_q(ep, 0, 0, 0, /*aggression=*/400, /*alarm=*/0, near, 0);
                const int with_tr = treaty_value_q(ep, 0, 0, 0, /*aggression=*/400, /*alarm=*/0, near,
                                                    trade_value);
                check(with_tr > without,
                      near ? "R7.13 trade to open raises treaty value near home"
                           : "R7.14 trade to open raises treaty value far from home too");
            }
        }

        // One want, shared: a buyer bound to TWO holders imports its want once,
        // not once per seller. Polity 2 mirrors polity 1 (market seat + one farm
        // region at reach 600), so each seller alone would carry 500 of the 700.
        {
            std::vector<region> r2 = base_regions;
            r2.resize(5);
            r2[3].nation = 2; r2[3].has_market = true; r2[3].dominant = region_class::none;
            r2[4].nation = 2; r2[4].dominant = region_class::farm; r2[4].network_supply_q = 600;
            std::vector<polity> q2 = base_qs;
            q2.resize(3);
            q2[2].id = 2; q2[2].alive = true; q2[2].capital = 3;
            const std::vector<history_corridor> c2 = { history_corridor{0, 2, 5}, history_corridor{0, 4, 5} };
            const std::vector<dated_object> two_partners = {
                dated_object{2000, static_cast<int32_t>(treaty_clause::trade_access), 0, 1},
                dated_object{2000, static_cast<int32_t>(treaty_clause::trade_access), 0, 2} };
            std::vector<trade_flow> f2;
            run_exploration_upkeep(r2, q2, c2, ep, 1234, 4, nullptr, &two_partners, &f2);
            int64_t inbound = 0;
            for (const trade_flow& f : f2) if (f.buyer == 0 && f.good == farm) inbound += f.volume_q;
            check(f2.size() == 2 && inbound == r2[0].scarcity_raw_q[farm] && r2[0].scarcity_q[farm] == 0
               && f2[0].volume_q == 500 && f2[1].volume_q == 200,
                  "R7.15 one want is shared across sellers: 700 wanted from two 500-lines arrives as 500 + 200");

            // R7.16/R7.17 -- treaty value is the MARGINAL trade a clause opens.
            // Seller 1 already brings buyer 0 500 of its 700, so binding
            // seller 2 opens only the 200 left; seller 1's own clause is worth
            // its 500 because seller 2's 200 leaves that much room.
            const trade_context ctx2 = build_trade_context(r2, q2, c2);
            const int v02 = pair_trade_value_q(ctx2, r2, q2, 0, 2, f2);
            const int v01 = pair_trade_value_q(ctx2, r2, q2, 0, 1, f2);
            std::printf("      marginal treaty trade: pair(0,2)=%d pair(0,1)=%d\n", v02, v01);
            check(v02 == 200,
                  "R7.16 pair_trade_value_q reads only the want other sellers leave (200, not the unshared 500)");
            check(v01 == 500,
                  "R7.17 a bound pair's own flows do not count against its re-scored value");
        }

        // R7.18 -- one holding, shared: a seller with ONE farm region (holding
        // 500) bound to TWO buyers each wanting 700 exports its holding once.
        // Buyer 0 is region 0; buyer 2 is region 3; the seller is polity 1
        // (market seat region 1, farm region 2), reached by both over land.
        {
            std::vector<region> r3 = base_regions;
            r3.resize(4);
            r3[3].nation = 2; r3[3].has_market = true; r3[3].dominant = region_class::none;
            r3[3].network_supply_q = 1000;
            std::vector<polity> q3 = base_qs;
            q3.resize(3);
            q3[2].id = 2; q3[2].alive = true; q3[2].capital = 3;
            const std::vector<history_corridor> c3 = { history_corridor{0, 2, 5}, history_corridor{2, 3, 5} };
            const std::vector<dated_object> two_buyers = {
                dated_object{2000, static_cast<int32_t>(treaty_clause::trade_access), 0, 1},
                dated_object{2000, static_cast<int32_t>(treaty_clause::trade_access), 1, 2} };
            std::vector<trade_flow> f3;
            run_exploration_upkeep(r3, q3, c3, ep, 1234, 4, nullptr, &two_buyers, &f3);
            int64_t outbound = 0;
            for (const trade_flow& f : f3) if (f.seller == 1 && f.good == farm) outbound += f.volume_q;
            std::printf("      shared holding: wants %d/%d, flows=%zu, outbound=%lld\n",
                        r3[0].scarcity_raw_q[farm], r3[3].scarcity_raw_q[farm], f3.size(),
                        static_cast<long long>(outbound));
            check(r3[0].scarcity_raw_q[farm] == 700 && r3[3].scarcity_raw_q[farm] == 700
               && outbound == 500 && f3.size() == 1 && f3[0].buyer == 0 && f3[0].volume_q == 500,
                  "R7.18 one holding is shared across buyers: a 500 holding bound to two 700-wants exports 500 once, "
                  "ties to the lower buyer");
        }
    }

    // -----------------------------------------------------------------
    // T10 (was T8.15.1/2, BL-1041 fix round): DEFAULT A THROUGH THE SIM'S OWN
    // WIRING, on the real generated world. The span runs from 1200 with every
    // region SURVEYED at fuel 100: a region the run FOUNDS inherits its
    // parent's surveyed fuel x0.7 (history_sim.cpp, the Settle founding):
    // with A on, every founded region reads a value on the chain 100 -> 70 ->
    // 49 -> ... and its forest stays -1; with A off it reads -1.
    // -----------------------------------------------------------------
    check(fixture.exploration_ran, "T10.0  the shipped world ran its Exploration span (the fixture below needs it)");
    if (fixture.exploration_ran)
    {
        struct survey_run { settlement_state ss; history_sim_state hs; };
        const auto run_surveyed = [&](bool a_on) {
            history_sim_params ep = fixture.exploration_params;
            ep.resume_polities  = &fixture.pre_exploration_polities;
            ep.resume_grudges   = &fixture.pre_exploration_grudges;
            ep.resume_contacts  = &fixture.pre_exploration_contacts;
            ep.resume_corridors = &fixture.pre_exploration_corridors;
            ep.resume_civilisations    = &fixture.pre_exploration_civilisations;    // BL-1049
            ep.resume_universal_creeds = &fixture.pre_exploration_universal_creeds;
            ep.industry_survey_inherits_at_founding = a_on;
            survey_run o;
            o.ss = fixture.pre_exploration_settlement;
            for (region& r : o.ss.regions)           r.survey_fuel_q = 100;
            for (region& r : o.ss.pending_foundings) r.survey_fuel_q = 100;
            creed_state cs = fixture.pre_exploration_creeds;
            o.hs = run_history_sim(o.ss, &cs, fixture.terrain.view(), fixture.gw, fixture.gh, ep,
                                   fixture.exploration_seed, nullptr, fixture.works, nullptr);
            return o;
        };
        const std::size_t base_n = fixture.pre_exploration_settlement.regions.size();
        // The inheritance chain from 100 at the x0.7 keep, plus 100 itself
        // (a scheduled founding keeps its own, surveyed, reading).
        std::vector<int> chain = { 100 };
        for (int v = 100; v > 0;) { v = (v * 700) / 1000; chain.push_back(v); }
        const auto on_chain = [&](int v) { return std::find(chain.begin(), chain.end(), v) != chain.end(); };

        const survey_run ab = run_surveyed(true);
        const survey_run na = run_surveyed(false); // A off

        int founded = 0, founded_on_chain = 0, founded_forest_unsurveyed = 0;
        for (std::size_t i = base_n; i < ab.ss.regions.size(); ++i)
        {
            ++founded;
            if (on_chain(ab.ss.regions[i].survey_fuel_q)) ++founded_on_chain;
            if (ab.ss.regions[i].survey_forest_q == -1) ++founded_forest_unsurveyed;
        }
        int na_founded = 0, na_unsurveyed = 0;
        for (std::size_t i = base_n; i < na.ss.regions.size(); ++i)
        {
            ++na_founded;
            if (na.ss.regions[i].survey_fuel_q == -1) ++na_unsurveyed;
        }
        std::printf("      T10 surveyed at fuel 100, 1200 -> 1660: A on founded %d, on the x0.7 chain %d, forest"
                    " -1 %d; A off founded %d, unsurveyed %d\n", founded, founded_on_chain,
                    founded_forest_unsurveyed, na_founded, na_unsurveyed);
        check(founded > 0 && founded_on_chain == founded && founded_forest_unsurveyed == founded,
              "T10.1  DEFAULT A: every region the run founds inherits its parent's surveyed fuel x0.7 (the"
              " 100 -> 70 -> 49 chain), its forest left -1");
        check(na_founded > 0 && na_unsurveyed == na_founded,
              "T10.2  DEFAULT A off: the same foundings read -1 (T10.1 is the switch, not a default)");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
