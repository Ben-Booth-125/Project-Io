// ---------------------------------------------------------------------------
// pass_one_handoff — BL-826 (culture shares), BL-827 (grudges), BL-828 (the
// pass 1 -> pass 2 handoff).
//
// WHAT THIS BINDS, and why it is one harness rather than three. The three items
// are one block: shares are the take-back AND the anti-hegemony lever, grudges
// are the second take-back, and the handoff struct is the contract that says
// both actually cross. Splitting them would mean three harnesses each building
// the same synthetic worlds and each able to pass while the seam between them
// was broken.
//
// SYNTHETIC WORLDS ONLY, deliberately. Every world below is a hand-built strip
// of regions, so the harness runs in seconds and can sit in the routine gate —
// the full-world sims live in `history_sim_harness` and `history_sweep`, which
// are labelled `sweep` for exactly the cost this one refuses to pay. What is
// asserted here is arithmetic and mechanism, both of which a strip can carry.
//
// THE PATTERN, inherited from `history_sim_harness`: run the SAME world twice
// with one variable changed and assert the DIRECTION of the difference. An
// absolute count is a calibration target, and calibration targets get tuned
// toward rather than measured.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/history_sim.hpp"
#include "world/settlement.hpp"

#include <algorithm>
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

/// A minimal strip of `n` regions in a row, all one people.
settlement_state strip_world(int n, int culture, int col0, int farm_q, int ore_q)
{
    settlement_state ss;
    for (int i = 0; i < n; ++i)
    {
        region p;
        p.col = col0 + i * 3;
        p.row = 0;
        p.anchor = p.col;
        p.culture = culture_shares::pure(culture);
        p.founding_culture = culture;
        p.farm_q = farm_q;
        p.ore_q  = ore_q;
        p.port_q = 100;
        p.settle_score_q = 900 - i;
        p.population = 60000; // A war needs men. history_sweep seeds the same fixture.
        p.name = "Strip" + std::to_string(culture) + "-" + std::to_string(i);
        ss.regions.push_back(p);
    }
    return ss;
}

/// THREE rival strips laid end to end. Two would be the smallest world in
/// which a campaign has anywhere to go — but two polities make the grudge table
/// dense by construction (there are only two ordered pairs), so C5j's sparsity
/// claim would be untestable. Three is the smallest world in which "most pairs
/// never meet" can be false, and therefore the smallest one that can check it.
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

constexpr int syn_gw = 60;
constexpr int syn_gh = 30;

/// The parameters every check below starts from: a short, cheap arc with a
/// fine step, so a strip world resolves a war inside a second.
history_sim_params base_params()
{
    history_sim_params p;
    // LONG ENOUGH FOR GROUND TO BE HELD, not merely taken. At an 800-year arc
    // the frontier region flipped every round and the longest-held conquest in
    // the world was five years old, so "long-held is digested" had nothing to
    // measure. Assimilation is a centuries-scale force and the fixture has to
    // give it centuries.
    p.start_year = -3000;
    p.stop_year  = 0;
    p.tick_bands[0] = sim_tick_band{0, 5};
    p.tick_band_count = 1;
    // THE FIXTURE CLOSES THE FRONTIER. A strip world with open ground beside it
    // settles rather than fights — measured, 105 foundings and zero battles —
    // and a harness about conquest needs conquest to happen. This is a property
    // of the FIXTURE, not a re-weighting of the sim: the pressure gate is put
    // out of reach so the only expansion left is at somebody else's expense,
    // which is the world these checks are about.
    p.settle_pressure_q = 1001;
    return p;
}

/// Does the whole region table still satisfy the 1000 invariant?
bool shares_sum_everywhere(const settlement_state& ss, std::string* why)
{
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
        if (ss.regions[i].culture.total_q() != 1000)
        {
            if (why)
                *why = "region " + std::to_string(i) + " sums to "
                     + std::to_string(ss.regions[i].culture.total_q());
            return false;
        }
    return true;
}

/// Byte-level equality of everything the three items add, plus the counters the
/// sim already carried. R-DET's comparison.
bool same_run(const history_sim_state& a, const history_sim_state& b,
              const settlement_state& sa, const settlement_state& sb)
{
    if (a.battles != b.battles || a.conquests != b.conquests
     || a.foundings != b.foundings)
        return false;
    if (a.owner_changes.size() != b.owner_changes.size()) return false;
    for (std::size_t i = 0; i < a.owner_changes.size(); ++i)
        if (a.owner_changes[i].year   != b.owner_changes[i].year
         || a.owner_changes[i].region != b.owner_changes[i].region
         || a.owner_changes[i].owner  != b.owner_changes[i].owner)
            return false;

    if (a.grudges.size() != b.grudges.size()) return false;
    for (std::size_t i = 0; i < a.grudges.size(); ++i)
    {
        const grudge& x = a.grudges[i];
        const grudge& y = b.grudges[i];
        if (x.from != y.from || x.to != y.to || x.score != y.score
         || x.peak != y.peak || x.event_count != y.event_count
         || x.events_kept != y.events_kept)
            return false;
        for (int k = 0; k < x.events_kept; ++k)
            if (x.events[k].year != y.events[k].year
             || x.events[k].region != y.events[k].region
             || x.events[k].kind != y.events[k].kind
             || x.events[k].magnitude != y.events[k].magnitude)
                return false;
    }

    if (sa.regions.size() != sb.regions.size()) return false;
    for (std::size_t i = 0; i < sa.regions.size(); ++i)
        if (sa.regions[i].nation != sb.regions[i].nation
         || sa.regions[i].culture != sb.regions[i].culture) // culture_shares::operator==
            return false;
    return true;
}

} // namespace

int main()
{
    std::printf("=== pass_one_handoff: BL-826 shares / BL-827 grudges / BL-828 handoff ===\n\n");

    // -----------------------------------------------------------------------
    // C1 — THE ARITHMETIC. Integer, exact, and conserved.
    // -----------------------------------------------------------------------
    //
    // This group is first because everything after it is meaningless if the
    // invariant does not hold: a share table that does not sum to 1000 is not a
    // distribution, and every reading taken off it would be a fraction of an
    // unknown whole.
    {
        check(culture_shares{}.total_q() == 1000,
              "C1a  an unsettled region's shares still sum to 1000 (all tail)");
        check(culture_shares{}.plurality() == -1 && culture_shares{}.empty(),
              "C1b  an unsettled region names nobody");
        check(culture_shares::pure(3).total_q() == 1000
              && culture_shares::pure(3).share_of(3) == 1000
              && culture_shares::pure(3).majority(3),
              "C1c  a pure region is wholly one people");

        // The invariant under a long, varied sequence of shifts — the case that
        // actually breaks an integer redistribution, because the floor losses
        // accumulate. Driven by a deterministic mix, never an RNG.
        bool conserved = true;
        bool bounded   = true;
        culture_shares c = culture_shares::pure(0);
        for (int step = 0; step < 4000; ++step)
        {
            const int who = (step * 7 + step / 13) % 6;      // 6 cultures, 3 slots
            const int amt = 1 + (step * 31 + step / 7) % 40; // 1..40 per-mille
            c.shift_toward(who, amt);
            if (c.total_q() != 1000) { conserved = false; break; }
            for (int k = 0; k < culture_share_slots; ++k)
                if (c.weight_q[k] < 0 || c.weight_q[k] > 1000
                    || (c.id[k] < 0) != (c.weight_q[k] == 0))
                    bounded = false;
            if (c.other_q < 0) bounded = false;
        }
        check(conserved, "C1d  4000 mixed shifts conserve the 1000 total exactly");
        check(bounded,   "C1e  no slot ever goes negative, past 1000, or names nobody with weight");

        // Sorted descending, always — the plurality has to be element 0 or
        // every `plurality()` call site is silently wrong.
        bool sorted = true;
        for (int k = 1; k < culture_share_slots; ++k)
            if (c.id[k] >= 0 && c.weight_q[k - 1] < c.weight_q[k]) sorted = false;
        check(sorted, "C1f  the slots stay sorted descending, so plurality() is element 0");

        // PROPORTIONAL, SO IT NEVER COMPLETES. A conquered people is never
        // arithmetically erased, which is the design claim on `shift_toward`.
        culture_shares d = culture_shares::pure(1);
        for (int i = 0; i < 20000; ++i) d.shift_toward(0, 5);
        check(d.share_of(0) < 1000 && d.total_q() == 1000,
              "C1g  assimilation is asymptotic — 20000 shifts never reach 1000");

        // And it is MONOTONE: holding longer is always at least as digested.
        culture_shares e1 = culture_shares::pure(1), e2 = culture_shares::pure(1);
        for (int i = 0; i < 50;  ++i) e1.shift_toward(0, 4);
        for (int i = 0; i < 200; ++i) e2.shift_toward(0, 4);
        check(e2.share_of(0) > e1.share_of(0),
              "C1h  more years held is strictly more assimilated");
    }

    // -----------------------------------------------------------------------
    // C2 — ASSIMILATION AT THE SIM'S OWN RATE, over the spans that matter.
    // -----------------------------------------------------------------------
    //
    // The separation the design claims: a conquest held four centuries is well
    // on its way, one held a single century has barely started. Asserted as a
    // RANGE on each and as an ORDER between them, never as a pinned digit —
    // a pinned digit here would be a calibration target for
    // `assimilation_per_year_q` rather than a check on the mechanism.
    {
        const history_sim_params p = base_params();
        culture_shares recent = culture_shares::pure(1);
        culture_shares old    = culture_shares::pure(1);
        for (int y = 0; y < 100; ++y) recent.shift_toward(0, p.assimilation_per_year_q);
        for (int y = 0; y < 400; ++y) old.shift_toward(0, p.assimilation_per_year_q);

        std::printf("      assimilation at %d/1000/yr: 100 years -> %d/1000, 400 years -> %d/1000\n",
                    p.assimilation_per_year_q, recent.share_of(0), old.share_of(0));

        check(recent.share_of(0) > 0 && recent.share_of(0) < 350,
              "C2a  a CENTURY-old conquest has started and is nowhere near digested");
        check(old.share_of(0) > 400 && old.share_of(0) < 900,
              "C2b  a FOUR-CENTURY-old conquest is well digested and still not complete");
        check(old.share_of(0) > recent.share_of(0) * 2,
              "C2c  long-held ground is far more assimilated than recently-taken ground");

        // THE RATE IS SCALED BY THE STEP, which is the stepped-clock rule for
        // any quantity that is a rate — and THE TWO ARE NOT EQUAL, deliberately.
        //
        // Two things separate them and both are real. A proportional rate
        // COMPOUNDS, so even in exact arithmetic 400 steps of 0.2% and 20 steps
        // of 4% differ. And the take is an INTEGER floor of a per-mille
        // quantity, so at a one-year step and a 2/1000 rate a foreign remainder
        // of 998 yields floor(1.996) = 1 rather than 2 — the fine step is
        // systematically CONSERVATIVE. That bias vanishes as the step widens
        // (the whole ladder above a step of ~5 is unaffected) and it errs
        // toward "ground stays foreign longer", which is the direction this
        // design wants to err in.
        //
        // So the assertion is the ORDER and the BAND, not equality: a coarse
        // band digests at least as much as a fine one over the same span, and
        // neither lands in a different half of the range from the other. An
        // equality here would be a pinned digit dressed as an invariant.
        culture_shares fine = culture_shares::pure(1), coarse = culture_shares::pure(1);
        for (int r = 0; r < 400; ++r) fine.shift_toward(0, p.assimilation_per_year_q * 1);
        for (int r = 0; r < 20;  ++r) coarse.shift_toward(0, p.assimilation_per_year_q * 20);
        std::printf("      400 years at step 1 -> %d/1000, at step 20 -> %d/1000\n",
                    fine.share_of(0), coarse.share_of(0));
        check(coarse.share_of(0) >= fine.share_of(0),
              "C2e  a coarse band never digests LESS than a fine one (the floor is conservative)");
        check(fine.share_of(0) > 250 && coarse.share_of(0) < 800,
              "C2d  rate x step puts both band widths in the same broad band of digestion");
    }

    // -----------------------------------------------------------------------
    // C3 — THE INVARIANT SURVIVES A REAL RUN.
    // -----------------------------------------------------------------------
    settlement_state war_ss = rival_strip(6);
    history_sim_params war_p = base_params();
    history_sim_state war = run_history_sim(war_ss, nullptr, sim_terrain_view{},
                                            syn_gw, syn_gh, war_p, 0x51DEu);
    {
        std::string why;
        check(shares_sum_everywhere(war_ss, &why),
              "C3a  every region's shares sum to 1000 after a full run");
        if (!why.empty()) std::printf("      %s\n", why.c_str());

        std::printf("      run: %lld battles / %lld conquests / %lld foundings / %d regions\n",
                    static_cast<long long>(war.battles),
                    static_cast<long long>(war.conquests),
                    static_cast<long long>(war.foundings),
                    static_cast<int>(war_ss.regions.size()));
        check(war.conquests > 0,
              "C3b  the rival strip actually fights and takes ground (later checks need it)");
    }

    // -----------------------------------------------------------------------
    // C4 — A LONG-HELD CONQUEST HAS ASSIMILATED, A RECENT ONE HAS NOT, IN THE
    //      SIM ITSELF rather than in the arithmetic above.
    // -----------------------------------------------------------------------
    //
    // Read off the time-lapse rather than off a second bookkeeping copy: the
    // last `owner_change` on a region is the year its current holder took it,
    // and its holder's share is what assimilation has done since. One source,
    // so the check cannot pass against a record the sim does not actually keep.
    {
        std::vector<std::pair<int64_t, int>> held; // (years held, holder share)
        std::vector<int64_t> taken(war_ss.regions.size(), INT64_MIN);
        std::vector<int>     from_someone(war_ss.regions.size(), 0);
        for (const owner_change& c : war.owner_changes)
        {
            if (c.region >= war_ss.regions.size()) continue;
            if (taken[c.region] != INT64_MIN) from_someone[c.region] = 1; // Not the seed row.
            taken[c.region] = c.year;
        }
        for (std::size_t i = 0; i < war_ss.regions.size(); ++i)
        {
            if (!from_someone[i]) continue; // Never changed hands: nothing to digest.
            const int owner_id = war_ss.regions[i].nation;
            if (owner_id < 0 || owner_id >= static_cast<int>(war.polities.size())) continue;
            const int c = war.polities[static_cast<std::size_t>(owner_id)].culture;
            held.push_back({war_p.stop_year - taken[i],
                            war_ss.regions[i].culture.share_of(c)});
        }
        std::sort(held.begin(), held.end());

        check(!held.empty(), "C4a  at least one region changed hands, so there is ground to digest");
        if (!held.empty())
        {
            const auto& shortest = held.front();
            const auto& longest  = held.back();
            std::printf("      %d conquered regions | shortest held %lld yr -> %d/1000 |"
                        " longest held %lld yr -> %d/1000\n",
                        static_cast<int>(held.size()),
                        static_cast<long long>(shortest.first), shortest.second,
                        static_cast<long long>(longest.first),  longest.second);

            check(longest.second > shortest.second,
                  "C4b  the longest-held conquest is MORE digested than the shortest-held one");
            check(shortest.second < 1000,
                  "C4c  a conquest is never digested instantly — the border moved, the people did not");
            check(longest.second > 0,
                  "C4d  and holding ground does move it, so assimilation is not inert");
        }
    }

    // -----------------------------------------------------------------------
    // C5 — GRUDGES: raised by named events, carrying their cause.
    // -----------------------------------------------------------------------
    {
        check(!war.grudges.empty(),
              "C5a  a war raises grudges at all");

        bool sorted = true, directed = true, caused = true, placed = true, dated = true;
        std::pair<int, int> last{-1, -1};
        for (const grudge& g : war.grudges)
        {
            const std::pair<int, int> key{g.from, g.to};
            if (!(last < key)) sorted = false;
            last = key;
            if (g.from == g.to) directed = false;
            if (g.events_kept <= 0 || g.event_count <= 0) caused = false;
            for (int k = 0; k < g.events_kept; ++k)
            {
                if (g.events[k].region != owner_none
                    && g.events[k].region >= war_ss.regions.size()) placed = false;
                if (g.events[k].year < war_p.start_year || g.events[k].year > war_p.stop_year)
                    dated = false;
            }
        }
        check(sorted,   "C5b  the table is sorted by (from, to) — a sparse vector, never a keyed map");
        check(directed, "C5c  no polity holds a grudge against itself");
        check(caused,   "C5d  every grudge carries at least one named cause, not just a magnitude");
        check(placed,   "C5e  every kept event names a real place");
        check(dated,    "C5f  every kept event carries a date inside the run");

        // ASYMMETRY IS THE FLAVOUR. If every pair were mirrored the direction
        // would be decoration; at least one pair must be one-sided or unequal.
        bool asymmetric = false;
        for (const grudge& g : war.grudges)
            if (grudge_between(war, g.to, g.from) != g.score) asymmetric = true;
        check(asymmetric, "C5g  resentment is not symmetric — at least one pair is one-sided");

        // DECAY IS LIVE, and the direction is what is asserted. Same world,
        // same seed, decay off — the standing scores must come out higher.
        history_sim_params nodecay = war_p;
        nodecay.grudge_decay_per_year_q = 0;
        settlement_state nd_ss = rival_strip(6);
        const history_sim_state nd = run_history_sim(nd_ss, nullptr, sim_terrain_view{},
                                                     syn_gw, syn_gh, nodecay, 0x51DEu);
        int64_t decayed_sum = 0, undecayed_sum = 0;
        for (const grudge& g : war.grudges) decayed_sum   += g.score;
        for (const grudge& g : nd.grudges)  undecayed_sum += g.score;
        std::printf("      grudge totals: decay on %lld / decay off %lld"
                    " (%d pairs on, %d off)\n",
                    static_cast<long long>(decayed_sum),
                    static_cast<long long>(undecayed_sum),
                    static_cast<int>(war.grudges.size()),
                    static_cast<int>(nd.grudges.size()));
        check(undecayed_sum > decayed_sum,
              "C5h  decay is LIVE — the same war leaves smaller standing scores with it on");

        // A DECAYED SCORE IS BELOW ITS PEAK somewhere, or `peak` is a second
        // copy of `score` and the record cannot show a feud that cooled.
        bool cooled = false;
        for (const grudge& g : war.grudges) if (g.peak > g.score) cooled = true;
        check(cooled, "C5i  at least one feud cooled — peak outlives the standing score");

        // SPARSE. Most pairs never meet, so the table must be far short of the
        // dense n*(n-1) it would be if every polity resented every other.
        const std::size_t n = war.polities.size();
        const std::size_t dense = n > 1 ? n * (n - 1) : 1;
        check(war.grudges.size() < dense,
              "C5j  the table is sparse — not every pair is aggrieved");

        // PRINTABLE, TOP-N, WITH THE EVENTS BEHIND EACH. The design's whole
        // point: a number a player cannot ask about is a modifier, not a story.
        const std::vector<grudge> top = top_grudges(war, 3);
        std::printf("      --- top grudges, with their causes ---\n");
        for (const grudge& g : top)
        {
            std::printf("      polity %u -> %u : %d (peak %d, %d events)\n",
                        g.from, g.to, g.score, g.peak, g.event_count);
            for (int k = 0; k < g.events_kept; ++k)
                std::printf("          %s\n", grudge_event_line(g.events[k], war_ss).c_str());
        }
        check(!top.empty() && top.front().events_kept > 0,
              "C5k  the top grudges print with the events behind each");
        bool ordered = true;
        for (std::size_t i = 1; i < top.size(); ++i)
            if (top[i - 1].score < top[i].score) ordered = false;
        check(ordered, "C5l  the top-N listing is ordered by a total order with an explicit tie-break");
    }

    // -----------------------------------------------------------------------
    // C6 — DETERMINISM. Byte-identical from a seed, twice.
    // -----------------------------------------------------------------------
    {
        settlement_state a_ss = rival_strip(6);
        settlement_state b_ss = rival_strip(6);
        const history_sim_state a = run_history_sim(a_ss, nullptr, sim_terrain_view{},
                                                    syn_gw, syn_gh, war_p, 0x51DEu);
        const history_sim_state b = run_history_sim(b_ss, nullptr, sim_terrain_view{},
                                                    syn_gw, syn_gh, war_p, 0x51DEu);
        check(same_run(a, b, a_ss, b_ss),
              "C6a  the same seed produces a bit-identical run, shares and grudges included");

        // And a DIFFERENT seed does not, or the equality above would be vacuous.
        settlement_state c_ss = rival_strip(6);
        const history_sim_state c = run_history_sim(c_ss, nullptr, sim_terrain_view{},
                                                    syn_gw, syn_gh, war_p, 0x9F03u);
        check(!same_run(a, c, a_ss, c_ss),
              "C6b  a different seed produces a different history (C6a is not vacuous)");
    }

    // -----------------------------------------------------------------------
    // C7 — THE HANDOFF (BL-828). The doc's list and the struct's fields.
    // -----------------------------------------------------------------------
    {
        const pass_one_output o = make_pass_one_output(war_ss, war, /*culture_count=*/3);
        std::string why;
        const bool ok = pass_one_output_valid(o, &why);
        if (!ok) std::printf("      invalid: %s\n", why.c_str());
        check(ok, "C7a  the handoff validates — shares, holdings and grudges all well-formed");

        check(o.regions.size() == war_ss.regions.size()
              && o.polities.size() == war.polities.size(),
              "C7b  the region table and the polities cross whole — nothing is reset");

        // THE HOLDINGS ARE THE POLITICAL MAP, and they must account for every
        // owned region exactly once. A realm arriving as one nation rather than
        // as a Voronoi cell per region is the whole point of carrying them.
        std::size_t counted = 0;
        for (const polity_holdings& h : o.holdings) counted += h.regions.size();
        std::size_t owned = 0;
        for (const region& p : o.regions) if (p.nation >= 0) ++owned;
        check(counted == owned,
              "C7c  the holdings account for every owned region exactly once");
        check(!o.holdings.empty() && o.holdings.size() <= o.polities.size(),
              "C7d  one holdings entry per LIVING polity, never more");

        check(o.grudges.size() == war.grudges.size(),
              "C7e  the grudges cross with the rest, as an input to sentiment");
        check(o.start_year == war_p.start_year && o.stop_year == war_p.stop_year,
              "C7f  the span crosses with the record it describes");

        // THE VALIDATOR ACTUALLY BITES. A check that cannot fail is not a check
        // — break the invariant and watch it caught, which is the only evidence
        // that C7a means anything.
        pass_one_output broken = o;
        if (!broken.regions.empty())
        {
            broken.regions[0].culture.other_q += 1; // 1001 — no longer a distribution.
            check(!pass_one_output_valid(broken, &why),
                  "C7g  the validator catches a share table that does not sum to 1000");
        }
        pass_one_output broken2 = o;
        if (!broken2.holdings.empty() && !broken2.holdings[0].regions.empty())
        {
            broken2.holdings[0].regions.push_back(
                static_cast<int>(broken2.regions.size()) + 5); // Out of range.
            check(!pass_one_output_valid(broken2, &why),
                  "C7h  the validator catches a holding that names a region off the map");
        }
    }

    std::printf("\n%s (%d failures)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
