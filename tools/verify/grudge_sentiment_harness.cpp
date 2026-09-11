// ---------------------------------------------------------------------------
// grudge_sentiment_harness — BL-898. Does the Era -1 grudge record actually
// seed nation->nation sentiment, and is what it seeded still legible?
//
// WHAT THIS EXISTS TO CATCH, stated as the defect rather than the feature:
// `pass_one_output::grudges` crossed the pass 1 -> pass 2 handoff for a whole
// sprint with NO CONSUMER. `grudge_between` had exactly one caller in the tree
// (pass_one_handoff, a harness) and `hard_coded_world.cpp` never read the table
// at all. A field that is retained but unread is indistinguishable from one
// that was never retained, except that it looks finished — so the check that
// matters is not "the conversion arithmetic is right", it is "SOMETHING MOVED,
// and it moved because of the record".
//
// WHAT IT DELIBERATELY DOES NOT CHECK. That a grudge changes who a polity
// campaigns against. BL-827's ruling, written at the field in `history_sim.hpp`
// § Grudges, forbids the sim's scorer from reading a grudge at all: the moment
// it became a scorer input it would be an agent term rather than an in-world
// force. G7 asserts the absence directly.
//
// THREE LAYERS, cheapest first:
//   G1-G6  SYNTHETIC — a hand-built grudge table and a hand-built polity ->
//          nation map. Instant, and it is where the arithmetic, the direction,
//          the drops and the determinism are pinned.
//   G7     THE SIM'S OWN RECORD — the era GENERATION ran, re-run from
//          `era_minus_one_fixture`, so the grudges being seeded are the ones a
//          real world actually produced. (Not a synthetic strip: the strip
//          fixture `pass_one_handoff` uses stopped producing a war before this
//          item and leaves an EMPTY grudge table, which is why seven of that
//          harness's rows are red on this tip.)
//   G8     THE WHOLE SEAM — `make_hard_coded_world`, which is the only thing
//          that proves the wiring in `hard_coded_world.cpp` runs. Expensive
//          (~20 s), and the reason this harness is not in the routine gate.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/grudge_sentiment.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <cstdio>
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

/// Entity ids standing in for nations. Any non-zero id will do — the seeding
/// never dereferences them, which is exactly what lets it be checked without a
/// `world`.
constexpr entity_id N0 = 101, N1 = 102, N2 = 103;

/// One hand-built grudge with one named cause, so every row under test carries
/// the who-wronged-whom-WHERE the legibility rule is about.
grudge make_grudge(uint16_t from, uint16_t to, int32_t score,
                   grudge_kind kind, int32_t year, uint16_t region)
{
    grudge g;
    g.from        = from;
    g.to          = to;
    g.score       = score;
    g.peak        = score + 100;
    g.event_count = 3;
    g.events_kept = 1;
    g.events[0]   = grudge_event{ year, region, kind, score };
    return g;
}

float trust_of(const sentiment_table& t, entity_id a, entity_id b)
{
    return sentiment_toward(t, a, b).trust;
}
float access_of(const sentiment_table& t, entity_id a, entity_id b)
{
    return sentiment_toward(t, a, b).access;
}

/// A minimal strip of `n` regions in a row, all one people. Used ONLY to give
/// G6's legibility check real region NAMES to print — nothing here simulates.
settlement_state strip_world(int n, int culture, int col0, int farm_q, int ore_q)
{
    settlement_state ss;
    for (int i = 0; i < n; ++i)
    {
        region p;
        p.col              = col0 + i * 3;
        p.row              = 0;
        p.anchor           = p.col;
        p.culture          = culture_shares::pure(culture);
        p.founding_culture = culture;
        p.farm_q           = farm_q;
        p.ore_q            = ore_q;
        p.port_q           = 100;
        p.settle_score_q   = 900 - i;
        p.population       = 60000;
        p.name             = "Strip" + std::to_string(culture) + "-" + std::to_string(i);
        ss.regions.push_back(p);
    }
    return ss;
}

settlement_state rival_strip(int per_side)
{
    settlement_state ss = strip_world(per_side, 0, 0, 900, 500);
    for (int c = 1; c < 3; ++c)
    {
        settlement_state b = strip_world(per_side, c, c * per_side * 3,
                                         900 + c * 20, 500 + c * 200);
        ss.regions.insert(ss.regions.end(), b.regions.begin(), b.regions.end());
    }
    return ss;
}

} // namespace

int main()
{
    std::printf("=== grudge_sentiment_harness (BL-898) ===\n\n");

    const grudge_sentiment_params gp; // The shipped defaults.
    std::printf("params: score_full %d  min_score %d  full_access %.1f  full_trust %.1f\n\n",
                gp.score_full, gp.min_score,
                static_cast<double>(gp.full_access), static_cast<double>(gp.full_trust));

    // -----------------------------------------------------------------------
    // G1 — IT MOVES AT ALL, AND IT IS DIRECTED.
    // -----------------------------------------------------------------------
    {
        const std::vector<entity_id> map = { N0, N1, N2 };
        const std::vector<grudge> gs = {
            make_grudge(0, 1, 10000, grudge_kind::seat_sacked, -1840, 3),
        };

        sentiment_table t;
        std::vector<grudge_sentiment_seed> seeds;
        const grudge_sentiment_report rep = seed_grudge_sentiment(t, gs, map, gp, &seeds);

        check(rep.rows_seeded == 1 && t.pairs.size() == 1,
              "G1a  a full-cap grudge seeds exactly one sentiment row");

        const float acc = access_of(t, N0, N1);
        const float tru = trust_of(t, N0, N1);
        std::printf("      polity 0 -> 1, score 10000: access %.3f  trust %.3f\n",
                    static_cast<double>(acc), static_cast<double>(tru));

        check(acc < 0.0f && tru < 0.0f,
              "G1b  a grudge is NEGATIVE sentiment on both dimensions");

        check(acc == gp.full_access && tru == gp.full_trust,
              "G1c  a grudge at score_full lands exactly the authored full weight");

        // DIRECTED: the aggrieved polity is the OBSERVER, and the reverse row
        // does not exist. "A resents B" is not "B resents A".
        check(sentiment_toward(t, N1, N0).neutral(),
              "G1d  the reverse pair is untouched — sentiment's direction matches the grudge's");

        check(seeds.size() == 1 && seeds[0].observer == N0 && seeds[0].subject == N1,
              "G1e  observer = successor of grudge::from, subject = successor of grudge::to");
    }

    // -----------------------------------------------------------------------
    // G2 — THE SCALE. Monotone in the score, and clamped at the cap.
    // -----------------------------------------------------------------------
    {
        const std::vector<entity_id> map = { N0, N1, N2 };
        float last = 1.0f;
        bool  monotone = true;
        std::printf("      score -> trust:");
        for (int score : { 500, 1000, 2500, 5000, 10000, 30000 })
        {
            sentiment_table t;
            const std::vector<grudge> gs = {
                make_grudge(0, 1, score, grudge_kind::ground_taken, -900, 2),
            };
            seed_grudge_sentiment(t, gs, map, gp, nullptr);
            const float v = trust_of(t, N0, N1);
            std::printf("  %d:%.2f", score, static_cast<double>(v));
            if (score <= 10000 && v >= last) monotone = false;
            if (score <= 10000) last = v;
        }
        std::printf("\n");
        check(monotone, "G2a  a bigger grudge means WORSE sentiment, strictly");

        sentiment_table over;
        seed_grudge_sentiment(over, { make_grudge(0, 1, 99999, grudge_kind::realm_ended, -10, 1) },
                              map, gp, nullptr);
        check(trust_of(over, N0, N1) == gp.full_trust,
              "G2b  a score past score_full clamps rather than overshooting");
    }

    // -----------------------------------------------------------------------
    // G3 — THE DONE-WHEN. Two worlds differing ONLY in their grudge record open
    // with DIFFERENT nation->nation sentiment.
    // -----------------------------------------------------------------------
    {
        const std::vector<entity_id> map = { N0, N1, N2 };

        const std::vector<grudge> quiet = {
            make_grudge(0, 1, 1200, grudge_kind::border_raided, -2200, 4),
        };
        const std::vector<grudge> bitter = {
            make_grudge(0, 1, 8800, grudge_kind::seat_sacked, -2200, 4),
            make_grudge(2, 1, 6400, grudge_kind::ground_taken, -1100, 7),
        };

        sentiment_table a, b;
        seed_grudge_sentiment(a, quiet,  map, gp, nullptr);
        seed_grudge_sentiment(b, bitter, map, gp, nullptr);

        std::printf("      quiet record:  rows %zu, 0->1 trust %.3f\n",
                    a.pairs.size(), static_cast<double>(trust_of(a, N0, N1)));
        std::printf("      bitter record: rows %zu, 0->1 trust %.3f, 2->1 trust %.3f\n",
                    b.pairs.size(), static_cast<double>(trust_of(b, N0, N1)),
                    static_cast<double>(trust_of(b, N2, N1)));

        check(a.pairs != b.pairs,
              "G3a  two records differing only in their grudges seed DIFFERENT sentiment");
        check(trust_of(b, N0, N1) < trust_of(a, N0, N1),
              "G3b  and the bitterer history is the one that opens worse");
        check(a.pairs.size() == 1 && b.pairs.size() == 2,
              "G3c  a record with a second quarrel opens with a second soured pair");
    }

    // -----------------------------------------------------------------------
    // G4 — WHAT IS REFUSED, counted rather than swallowed.
    // -----------------------------------------------------------------------
    {
        // Polity 1 has NO successor nation (absorbed by the size floor); polity
        // 2 folded into the SAME nation as polity 0.
        const std::vector<entity_id> map = { N0, null_entity, N0 };
        const std::vector<grudge> gs = {
            make_grudge(0, 1, 9000, grudge_kind::ground_taken, -500, 1), // no successor
            make_grudge(0, 2, 9000, grudge_kind::seat_sacked,  -400, 2), // self pair
            make_grudge(2, 0,  200, grudge_kind::border_raided, -300, 3), // below min_score
            make_grudge(0, 9, 9000, grudge_kind::realm_ended,   -200, 4), // out of range
        };

        sentiment_table t;
        const grudge_sentiment_report rep = seed_grudge_sentiment(t, gs, map, gp, nullptr);
        std::printf("      seeded %d, below_floor %d, no_successor %d, self_pair %d, oob %d\n",
                    rep.rows_seeded, rep.below_floor, rep.no_successor,
                    rep.self_pair, rep.out_of_range);

        check(t.pairs.empty() && rep.rows_seeded == 0,
              "G4a  nothing is seeded when every row is refused");
        check(rep.no_successor == 1,
              "G4b  a polity with no successor nation is DROPPED, not redirected");
        check(rep.self_pair == 1,
              "G4c  two polities folded into one nation seed nothing — a nation has no self-row");
        check(rep.below_floor == 1,
              "G4d  a grudge under min_score stays sparse");
        check(rep.out_of_range == 1,
              "G4e  a polity id past the successor table is refused, never read past the end");
    }

    // -----------------------------------------------------------------------
    // G5 — DETERMINISM, and NEVER OVERWRITING LIVE SENTIMENT.
    // -----------------------------------------------------------------------
    {
        const std::vector<entity_id> map = { N0, N1, N2 };
        const std::vector<grudge> gs = {
            make_grudge(0, 1, 7000, grudge_kind::seat_sacked, -1500, 5),
            make_grudge(1, 2, 3000, grudge_kind::ground_taken, -800, 6),
            make_grudge(2, 0, 4500, grudge_kind::border_raided, -400, 7),
        };

        sentiment_table a, b;
        seed_grudge_sentiment(a, gs, map, gp, nullptr);
        seed_grudge_sentiment(b, gs, map, gp, nullptr);
        check(a.pairs == b.pairs, "G5a  two seedings of one record are bit-identical");

        // A row already moved by conduct is ADDED TO, never replaced.
        sentiment_table live;
        sentiment_params sp;
        sp.factors[static_cast<std::size_t>(sentiment_factor_kind::trade_conducted)] =
            sentiment_factor{ 5.0f, 5.0f };
        apply_sentiment_events(live, sp, { sentiment_event{ N0, N1,
                                          sentiment_factor_kind::trade_conducted, 1.0f } });
        const float before = trust_of(live, N0, N1);
        seed_grudge_sentiment(live, gs, map, gp, nullptr);
        const float after = trust_of(live, N0, N1);
        std::printf("      live row %.3f -> %.3f after seeding\n",
                    static_cast<double>(before), static_cast<double>(after));
        check(after < before && after != gp.full_trust,
              "G5b  seeding ACCUMULATES onto an existing row rather than overwriting it");

        sentiment_table empty_t;
        check(seed_grudge_sentiment(empty_t, {}, map, gp, nullptr).rows_seeded == 0
                  && empty_t.pairs.empty(),
              "G5c  an empty grudge record seeds nothing at all (the inert case)");
    }

    // -----------------------------------------------------------------------
    // G6 — LEGIBILITY. Every seeded row still names who wronged whom, WHERE and
    // WHEN. This is the constraint that makes a grudge admissible at all: a
    // force with a visible cause, never a bare aggression weight.
    // -----------------------------------------------------------------------
    {
        settlement_state ss = rival_strip(4);
        const std::vector<entity_id> map = { N0, N1, N2 };
        const std::vector<grudge> gs = {
            make_grudge(0, 1, 8000, grudge_kind::seat_sacked, -1742, 5),
            make_grudge(2, 0, 3300, grudge_kind::ground_taken, -910, 2),
        };

        sentiment_table t;
        std::vector<grudge_sentiment_seed> seeds;
        seed_grudge_sentiment(t, gs, map, gp, &seeds);

        bool all_caused = !seeds.empty();
        for (const grudge_sentiment_seed& s : seeds)
        {
            if (!s.has_cause) { all_caused = false; continue; }
            if (s.cause.year == 0 || s.cause.region == owner_none) all_caused = false;
            std::printf("      %s\n", grudge_sentiment_line(s, ss).c_str());
        }
        check(all_caused,
              "G6a  every seeded row carries a named cause with a PLACE and a DATE");

        const std::string line = grudge_sentiment_line(seeds[0], ss);
        check(line.find("Strip") != std::string::npos
                  && line.find("-1742") != std::string::npos
                  && line.find("the seat sacked") != std::string::npos,
              "G6b  the printed line names the region, the year and the wrong");
    }

    // -----------------------------------------------------------------------
    // G7 — THE SIM'S OWN RECORD, and the ruling it must not break.
    //
    // RUN FROM THE GENERATION FIXTURE, NOT FROM A SYNTHETIC STRIP, and the
    // choice is forced rather than preferred. The strip world `pass_one_handoff`
    // uses stopped producing a war at some point before this item: measured on
    // this tip it resolves 2 battles, 2 conquests and ZERO grudge pairs, and
    // seven of that harness's own rows are red because of it. Seeding from a
    // record that does not exist would have measured nothing, and tuning the
    // fixture until a war appeared would be measuring the tuning. So this runs
    // the era generation ACTUALLY RAN, handed back through
    // `era_minus_one_fixture` — which is the whole reason that fixture exists
    // (BL-462: no second construction to drift).
    // -----------------------------------------------------------------------
    {
        world_params wp;
        wp.seed = 20260911u;
        world_gen_config stop_early;
        stop_early.stop_after_ancient_era = true; // The era is all G7 needs.

        era_minus_one_fixture fx;
        generation_report     fr{};
        (void)make_hard_coded_world(wp, &fr, stop_early, nullptr, nullptr, &fx);

        check(fx.ran, "G7a  generation ran the Era -1 sim and handed back its fixture");

        settlement_state re_ss = fx.settlement;
        const history_sim_state hs =
            run_history_sim(re_ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                            fx.params, fx.seed, nullptr, fx.works);

        std::printf("      re-run: %lld battles / %lld conquests, %zu grudge pairs,"
                    " %zu polities\n",
                    static_cast<long long>(hs.battles),
                    static_cast<long long>(hs.conquests),
                    hs.grudges.size(), hs.polities.size());

        check(!hs.grudges.empty(),
              "G7b  the era generation runs produces a grudge record to seed from");

        // Identity map: every polity is its own nation, so nothing is dropped
        // for want of a successor and what is measured is the CONVERSION.
        std::vector<entity_id> map(hs.polities.size(), null_entity);
        for (std::size_t i = 0; i < map.size(); ++i)
            map[i] = static_cast<entity_id>(1000 + i);

        sentiment_table t;
        std::vector<grudge_sentiment_seed> seeds;
        grudge_sentiment_params sp = gp;
        sp.score_full = static_cast<int32_t>(fx.params.grudge_cap);
        const grudge_sentiment_report rep = seed_grudge_sentiment(t, hs.grudges, map, sp, &seeds);

        double worst = 0.0, sum = 0.0;
        for (const auto& kv : t.pairs)
        {
            sum += kv.second.trust;
            if (kv.second.trust < worst) worst = kv.second.trust;
        }
        std::printf("      seeded %d of %zu grudges -> %zu sentiment rows"
                    " (below floor %d); worst trust %.3f, mean %.3f\n",
                    rep.rows_seeded, hs.grudges.size(), t.pairs.size(),
                    rep.below_floor, worst,
                    t.pairs.empty() ? 0.0 : sum / static_cast<double>(t.pairs.size()));

        check(rep.rows_seeded > 0 && !t.pairs.empty(),
              "G7c  a REAL Era -1 record seeds real nation->nation sentiment");
        check(worst < 0.0, "G7d  the sentiment it seeds is adverse, measurably");

        if (!seeds.empty())
            std::printf("      worst quarrel: %s\n",
                        grudge_sentiment_line(seeds.front(), re_ss).c_str());

        // THE RULING (BL-827). The SAME era with every grudge magnitude zeroed
        // must run IDENTICALLY, because nothing in the sim reads a grudge. The
        // moment a grudge became a scorer input this row would fail.
        settlement_state   off_ss = fx.settlement;
        history_sim_params off_p  = fx.params;
        off_p.grudge_ground_taken  = 0;
        off_p.grudge_seat_sacked   = 0;
        off_p.grudge_border_raided = 0;
        off_p.grudge_realm_ended   = 0;
        const history_sim_state off =
            run_history_sim(off_ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                            off_p, fx.seed, nullptr, fx.works);

        check(off.grudges.empty(),
              "G7e  zeroing every grudge magnitude empties the record (the lever works)");
        check(off.battles == hs.battles && off.conquests == hs.conquests
                  && off.foundings == hs.foundings,
              "G7f  ...and the history is UNCHANGED — the sim's scorer reads no grudge (BL-827)");
    }

    // -----------------------------------------------------------------------
    // G8 — THE WHOLE SEAM. The only check that proves the wiring in
    // `hard_coded_world.cpp` runs at all. Expensive on purpose.
    // -----------------------------------------------------------------------
    {
        world_params wp;
        wp.seed = 20260911u;

        generation_report rep_a{};
        const world wa = make_hard_coded_world(wp, &rep_a);

        std::printf("      generation: %lld prehistory battles, %d grudge rows seeded,"
                    " %d dropped, %zu sentiment rows total\n",
                    static_cast<long long>(rep_a.prehistory_battles),
                    rep_a.grudge_sentiment_rows, rep_a.grudge_sentiment_dropped,
                    wa.sentiment.pairs.size());

        check(rep_a.grudge_sentiment_rows > 0,
              "G8a  a generated world seeds nation->nation sentiment from its grudge record");
        check(!wa.sentiment.pairs.empty(),
              "G8b  ...and the rows are actually in the world's sentiment table");

        bool any_adverse = false;
        double worst = 0.0;
        for (const auto& kv : wa.sentiment.pairs)
        {
            if (kv.second.trust < 0.0f || kv.second.access < 0.0f) any_adverse = true;
            if (kv.second.access < worst) worst = kv.second.access;
        }
        std::printf("      worst seeded access %.3f\n", worst);
        check(any_adverse, "G8c  the seeded sentiment is adverse, as a grudge must be");

        // Determinism across two generations of one seed — the property
        // `world_determinism` asserts for the world, asserted for this table.
        generation_report rep_b{};
        const world wb = make_hard_coded_world(wp, &rep_b);
        check(wa.sentiment.pairs == wb.sentiment.pairs,
              "G8d  two generations of one seed seed IDENTICAL sentiment");

        // And a different seed does not, or G8d is vacuous.
        world_params wp2 = wp;
        wp2.seed = 20260912u;
        generation_report rep_c{};
        const world wc = make_hard_coded_world(wp2, &rep_c);
        std::printf("      seed 20260912: %d rows seeded, %zu sentiment rows\n",
                    rep_c.grudge_sentiment_rows, wc.sentiment.pairs.size());
        check(wa.sentiment.pairs != wc.sentiment.pairs,
              "G8e  a different history opens with different sentiment (G8d is not vacuous)");
    }

    std::printf("\n%s — %d failure(s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
