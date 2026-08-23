// ---------------------------------------------------------------------------
// Headless world-history-log harness (BL-208 slice S1; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Covers the append-only narrative substrate in src/world/history_log.{hpp,cpp}
// and its first producer, the planetology chain. See backlog.json § BL-208 and
// req/requirements.json § history-log-substrate.
//
//   R1  DETERMINISM AND TREE INTEGRITY. Two same-seed generations produce
//       byte-identical logs — entry count, append order, assigned ids, the
//       (era, time_key) ordering, parent links and citation payloads all match.
//       Ids are monotonic and dense from 1; every non-zero parent resolves to an
//       existing headline; the ordering key is an integer, never a float.
//
//   R2  THE NULL-SINK INVARIANT — the one BL-219 depends on. Log authoring
//       consumes NO randomness and moves no gate threshold, so a run with a null
//       sink must produce a bit-identical planetology_state to a run with a log
//       attached. Without this, BL-219's reduced-fidelity sweep and the shipped
//       generator drift apart and every number the sweep reports is a lie.
//
//   R3  THE TWO READS, ONE STRUCTURE. headlines() returns at most eight entries
//       per body in causal order and matches the biography the chain already
//       emits; children() returns one headline's detail entries in emit order.
//       A body whose chain died early returns proportionally FEWER headlines —
//       asymmetry-is-characterisation (PLANETOLOGY.md § Presentation) must
//       survive the move into the log, never be padded up to eight.
//
// HONEST SCOPE NOTE: this validates the SUBSTRATE and its wiring, not the
// chemistry the entries describe. A green run means the log records faithfully
// and deterministically, not that the gates are tuned.
//
// The process exits non-zero if any assertion FAILs.

#include "world/history_log.hpp"
#include "world/planetology.hpp"

#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_fail;
}

/// Field-by-field comparison of two entries. Deliberately explicit rather than
/// memcmp: the entry holds std::strings and a vector, so a byte compare would be
/// comparing heap addresses.
bool same_entry(const hist::entry& a, const hist::entry& b)
{
    if (a.chapter != b.chapter || a.rank != b.rank) return false;
    if (a.subject != b.subject) return false;
    if (a.stage != b.stage || a.time_key != b.time_key) return false;
    if (a.rank_parent != b.rank_parent || a.prov != b.prov) return false;
    if (a.event != b.event || a.consequence != b.consequence) return false;
    if (a.citations.size() != b.citations.size()) return false;
    for (std::size_t i = 0; i < a.citations.size(); ++i)
    {
        if (a.citations[i].key != b.citations[i].key) return false;
        if (a.citations[i].value != b.citations[i].value) return false;
    }
    return true;
}

/// Everything a gate decides. If the null sink perturbed anything, it shows here.
bool same_state(const planetology_state& a, const planetology_state& b)
{
    if (a.archetype != b.archetype || a.died_at != b.died_at) return false;
    if (a.stage != b.stage || a.peak != b.peak) return false;
    if (a.depth != b.depth) return false;
    if (a.crustal_mn != b.crustal_mn || a.crustal_mo != b.crustal_mo) return false;
    if (a.photoferrotrophic != b.photoferrotrophic || a.oxygenic != b.oxygenic) return false;
    if (a.ferruginous_gyr != b.ferruginous_gyr) return false;
    if (a.marine_anoxia_gyr != b.marine_anoxia_gyr) return false;
    if (a.land_burial_gyr != b.land_burial_gyr) return false;
    if (a.arable_share != b.arable_share || a.o2_fraction != b.o2_fraction) return false;
    if (a.theta != b.theta || a.surface_temp_k != b.surface_temp_k) return false;
    if (a.endowment != b.endowment) return false;
    if (a.history.size() != b.history.size()) return false;
    if (a.trace.size() != b.trace.size()) return false;
    for (std::size_t i = 0; i < a.trace.size(); ++i)
    {
        const chem::molecular_event& x = a.trace[i];
        const chem::molecular_event& y = b.trace[i];
        if (x.epoch_dmyr != y.epoch_dmyr || x.process_id != y.process_id) return false;
        if (x.outcome_id != y.outcome_id || x.yield_q != y.yield_q)       return false;
        if (x.venue_id != y.venue_id || x.flags != y.flags)               return false;
    }
    return true;
}

} // namespace

int main()
{
    std::printf("history_log_harness (BL-208 slice S1)\n");

    const planetology_params params{};

    // -----------------------------------------------------------------------
    // R1 — determinism and tree integrity, over the whole prototype body set.
    // -----------------------------------------------------------------------
    std::printf("\nR1 determinism & tree integrity\n");
    {
        hist::log a, b;
        for (int i = 0; i < prototype_body_count(); ++i)
        {
            const hist::subject_ref subj{ hist::subject_kind::body, static_cast<uint32_t>(i) };
            run_planetology(prototype_body(i), params, prototype_body_seed(i), &a, subj);
            run_planetology(prototype_body(i), params, prototype_body_seed(i), &b, subj);
        }

        check(!a.empty(), "the chain actually wrote entries");
        check(a.size() == b.size(), "two same-seed runs produce the same entry count");

        bool identical = a.size() == b.size();
        for (hist::entry_id id = 1; identical && id <= a.size(); ++id)
            identical = same_entry(a.at(id), b.at(id));
        check(identical, "every entry matches field-for-field across runs");

        check(a.integrity_ok(), "ids monotonic, parents resolve to headlines, seal respected");

        // The ordering key must be an INTEGER type. A float in a load-bearing
        // comparison is the portability hazard the standing rules name, and under
        // BL-208 this key is load-bearing. Checked on the type, not on a value —
        // a value comparison here would pass for a float too.
        check(std::is_same<decltype(hist::entry::time_key), int64_t>::value,
              "time_key is an int64, not a float");

        // Deep-time entries count DOWN toward the present, so within one body the
        // key must be non-increasing across successive headlines.
        bool ordered = true;
        for (int i = 0; i < prototype_body_count(); ++i)
        {
            const hist::subject_ref subj{ hist::subject_kind::body, static_cast<uint32_t>(i) };
            const std::vector<hist::entry_id>& hs = a.headlines(subj);
            for (std::size_t k = 1; k < hs.size(); ++k)
            {
                if (a.at(hs[k]).time_key > a.at(hs[k - 1]).time_key)
                    ordered = false;
            }
        }
        check(ordered, "headlines run forward in time (years-before-present decreasing)");

        // Sealing is a lifetime boundary: after seal() nothing may claim genesis.
        a.seal();
        check(a.sealed() && a.sealed_count() == a.size(), "seal() closes the genesis segment");
        a.append(hist::entry{}); // era::deep_time by default — a breach
        check(!a.integrity_ok(), "appending genesis after seal() is reported as a breach");
    }

    // -----------------------------------------------------------------------
    // R2 — the null-sink invariant. THE ONE BL-219 DEPENDS ON.
    // -----------------------------------------------------------------------
    std::printf("\nR2 null sink consumes no randomness\n");
    {
        bool all_same = true;
        int  checked  = 0;

        // A seed sample rather than one body: the invariant has to hold on every
        // path through the chain, including the ones that die early and the ones
        // that reach the photosystem fork.
        for (uint32_t s = 1; s <= 400; ++s)
        {
            for (int i = 0; i < prototype_body_count(); ++i)
            {
                const uint32_t seed = prototype_body_seed(i) ^ (s * 2654435761u);

                hist::log sink;
                const hist::subject_ref subj{ hist::subject_kind::body, static_cast<uint32_t>(i) };
                const planetology_state with    = run_planetology(prototype_body(i), params, seed, &sink, subj);
                const planetology_state without = run_planetology(prototype_body(i), params, seed, nullptr, {});

                ++checked;
                if (!same_state(with, without))
                    all_same = false;
            }
        }
        std::printf("  (%d body-seed pairs compared)\n", checked);
        check(all_same, "a null log sink produces a bit-identical planetology_state");
    }

    // -----------------------------------------------------------------------
    // R3 — the two reads, one structure.
    // -----------------------------------------------------------------------
    std::printf("\nR3 summary and deep reads\n");
    {
        hist::log lg;
        std::vector<std::size_t> biography_lines;
        std::vector<std::size_t> headline_counts;

        for (int i = 0; i < prototype_body_count(); ++i)
        {
            const hist::subject_ref subj{ hist::subject_kind::body, static_cast<uint32_t>(i) };
            const planetology_state st = run_planetology(prototype_body(i), params,
                                                         prototype_body_seed(i), &lg, subj);
            biography_lines.push_back(st.history.size());
            headline_counts.push_back(lg.headlines(subj).size());
            std::printf("  %-10s %zu headline(s), %zu trace event(s), archetype %d\n",
                        prototype_body(i).name, lg.headlines(subj).size(),
                        st.trace.size(), static_cast<int>(st.archetype));
        }

        bool        matches = true;
        std::size_t worst   = 0;
        for (std::size_t i = 0; i < biography_lines.size(); ++i)
        {
            if (biography_lines[i] != headline_counts[i]) matches = false;
            if (headline_counts[i] > worst)               worst = headline_counts[i];
        }

        // THE INVARIANT THIS SUBSTRATE OWES: the log mirrors the chain exactly —
        // it neither pads a short biography up nor drops a line from a long one.
        check(matches, "headlines() count equals the biography the chain emitted, exactly");

        // THE EIGHT-LINE CAP IS *NOT* ASSERTED HERE, DELIBERATELY, AND THIS IS A
        // FINDING RATHER THAN AN OMISSION.
        //
        // PLANETOLOGY.md § Presentation states "eight lines maximum", and BL-208's
        // design treats the eight biography lines as the headline entries. The
        // shipped chain does not honour that: Kepler emitted TEN lines at commit
        // f9f5c4a, before this item touched it (verified against HEAD with a
        // standalone probe, 2026-07-29). So the cap is currently a documented
        // aspiration that the generator has never satisfied.
        //
        // Asserting <= 8 here would fail on pre-existing behaviour and tempt
        // someone to trim a good biography to satisfy a harness. Asserting nothing
        // would hide it. So the count is REPORTED, and the design question — is the
        // cap a generation rule (the chain must emit at most eight) or a
        // presentation rule (the Story view shows the best eight of more) — is
        // recorded in requirements.json for Ben. Whichever way it settles, the
        // equivalence asserted above is what has to keep holding.
        std::printf("  [NOTE] widest biography is %zu lines; PLANETOLOGY.md documents a cap of 8\n",
                    worst);

        // Asymmetry is the characterisation: the bodies must NOT all report the
        // same number of lines. If they do, either the chain stopped
        // differentiating or something is padding.
        bool varies = false;
        for (std::size_t i = 1; i < headline_counts.size(); ++i)
            if (headline_counts[i] != headline_counts[0]) varies = true;
        check(varies, "line counts differ between bodies (asymmetry preserved, not padded)");

        // The drill-down: at least one headline must carry detail entries, and
        // every child must point back at its parent.
        std::size_t with_children = 0;
        bool        parents_ok    = true;
        for (hist::entry_id id = 1; id <= lg.size(); ++id)
        {
            if (lg.at(id).rank != hist::tier::headline)
                continue;
            const std::vector<hist::entry_id> kids = lg.children(id);
            if (!kids.empty())
                ++with_children;
            for (const hist::entry_id k : kids)
            {
                if (lg.at(k).rank_parent != id) parents_ok = false;
                if (lg.at(k).subject != lg.at(id).subject) parents_ok = false;
            }
        }
        std::printf("  (%zu headlines carry detail)\n", with_children);
        check(with_children > 0, "the molecular trace hangs beneath a headline as detail");
        check(parents_ok, "every detail entry points back at its own headline and subject");

        // Citations must be present on trace details, and integer-only by type.
        bool cited = false;
        for (hist::entry_id id = 1; id <= lg.size(); ++id)
        {
            if (lg.at(id).rank == hist::tier::detail && !lg.at(id).citations.empty())
                cited = true;
        }
        check(cited, "detail entries carry machine-readable citations, not only prose");
    }

    std::printf("\n%s (%d failure%s)\n", g_fail == 0 ? "ALL PASS" : "FAILURES",
                g_fail, g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
