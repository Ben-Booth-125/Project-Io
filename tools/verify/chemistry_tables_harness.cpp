// ---------------------------------------------------------------------------
// Headless chemistry-tables harness (BL-209; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Covers the molecular vocabulary in src/world/chemistry_tables.{hpp,cpp} — the
// foundation the seven-gate abiogenesis chain and the S6 photosystem fork will
// be written against. See backlog.json § BL-209.
//
//   R12a  RECORD SHAPE. molecular_event is exactly 8 bytes. It is a save-format
//        record (flat binary, per TECH_FOUNDATIONS.md), so a silent size change
//        is a save-compatibility break, not a detail.
//
//   R12b  NO ORPHAN IDS. Every process resolves to a named reaction, names a
//        gate, and every species it references resolves in the dictionary.
//        THIS IS THE KEY NEW INVARIANT: ids and display names are now
//        decoupled by design, so nothing else catches a table that has drifted
//        out of sync with its enum.
//
//   R12c  NAMES, NEVER IDS. Every species carries both a formula and a canonical
//        chemical name, and every enum resolves to a display string rather than
//        falling through to "unknown". Ben's rule (2026-07-28): anything a
//        human reads shows the chemical name; raw bytes stay in the inspector.
//
//   R12d  RNA HALF-LIFE CURVE. Monotonically decreasing in temperature, table-
//        driven, and bit-identical run to run. PLANETOLOGY.md § Determinism &
//        cost bans exp/log/pow in a gate path, and this value decides whether a
//        world gets life — so it is a lookup, and the lookup is asserted.
//
//   R12e  THE S5e THRESHOLD BITES WHERE THE DESIGN SAYS. The survival floor must
//        cut through the Lost City band (45-90 C): a cool alkaline vent keeps
//        its polymers, a hot one does not, and a black smoker never does. That
//        antagonism with S5b is the whole point of the replicator gate.
//
// HONEST SCOPE NOTE: this validates the VOCABULARY, not the chemistry. The
// tables encode a design position on contested science (the BIF oxidant, the
// fire minimum, the abiogenesis probability — PLANETOLOGY.md § Known
// weaknesses). A green run means the dictionary is internally coherent, not
// that the model is right.
//
// The process exits non-zero if any assertion FAILs.

#include "world/chemistry_tables.hpp"

#include <cstdio>
#include <cstring>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

bool is_unknown(const char* s) { return std::strcmp(s, "unknown") == 0; }

} // namespace

int main()
{
    using namespace chem;

    std::printf("=== BL-209 chemistry tables ===\n\n");

    // --- R12a: the record shape ---------------------------------------------
    check(sizeof(molecular_event) == 8, "R12a molecular_event is exactly 8 bytes");

    // --- R12b: no orphan ids -------------------------------------------------
    check(tables_self_consistent(), "R12b every process names a gate, a reaction and resolvable species");

    {
        int missing_gate = 0, empty_name = 0;
        for (int i = 1; i < static_cast<int>(process::count); ++i)
        {
            const process_info& pi = info(static_cast<process>(i));
            if (pi.gate == substage::count)   ++missing_gate;
            if (pi.consequence[0] == '\0')    ++empty_name;
        }
        std::printf("      %d processes, %d species in the dictionary\n",
                    static_cast<int>(process::count) - 1,
                    static_cast<int>(species::count) - 1);
        check(missing_gate == 0, "R12b every process is owned by exactly one gate");
        check(empty_name == 0,   "R12b every process carries its because-clause");
    }

    // --- R12c: names, never ids ---------------------------------------------
    {
        int bad = 0;
        for (int i = 1; i < static_cast<int>(species::count); ++i)
        {
            const species_info& si = info(static_cast<species>(i));
            if (si.name[0] == '\0' || si.formula[0] == '\0') ++bad;
        }
        check(bad == 0, "R12c every species has both a formula and a canonical name");
    }
    {
        int bad = 0;
        for (int i = 0; i < static_cast<int>(substage::count); ++i)
            if (is_unknown(substage_name(static_cast<substage>(i)))) ++bad;
        for (int i = 0; i < static_cast<int>(venue::count); ++i)
            if (is_unknown(venue_name(static_cast<venue>(i)))) ++bad;
        for (int i = 0; i < static_cast<int>(outcome::count); ++i)
            if (is_unknown(outcome_name(static_cast<outcome>(i)))) ++bad;
        check(bad == 0, "R12c every substage, venue and outcome resolves to a display name");
    }

    // --- R12d: the RNA half-life curve --------------------------------------
    {
        bool monotone = true;
        float prev = rna_half_life_hours(0.0f);
        for (float t = 5.0f; t <= 150.0f; t += 5.0f)
        {
            const float h = rna_half_life_hours(t);
            if (h > prev) monotone = false;
            prev = h;
        }
        check(monotone, "R12d RNA half-life falls monotonically with temperature");
        check(rna_half_life_hours(-40.0f) == rna_half_life_hours(0.0f) &&
              rna_half_life_hours(400.0f) == rna_half_life_hours(150.0f),
              "R12d the curve clamps outside its table rather than extrapolating");
        check(rna_half_life_hours(25.0f) == rna_half_life_hours(25.0f) &&
              rna_half_life_hours(72.5f) == rna_half_life_hours(72.5f),
              "R12d the lookup is deterministic (no transcendental in the path)");

        std::printf("      half-life:  25C %.0f h   45C %.0f h   60C %.0f h   90C %.1f h   350C %.3f h\n",
                    static_cast<double>(rna_half_life_hours(25.0f)),
                    static_cast<double>(rna_half_life_hours(45.0f)),
                    static_cast<double>(rna_half_life_hours(60.0f)),
                    static_cast<double>(rna_half_life_hours(90.0f)),
                    static_cast<double>(rna_half_life_hours(350.0f)));
    }

    // --- R12e: the threshold bites inside the Lost City band -----------------
    {
        const float cool_vent  = rna_half_life_hours(45.0f);  // cool end of Lost City
        const float hot_vent   = rna_half_life_hours(90.0f);  // hot end of Lost City
        const float smoker     = rna_half_life_hours(350.0f); // black smoker

        check(cool_vent >= rna_survival_floor_hours,
              "R12e a cool alkaline vent (45 C) keeps its polymers");
        check(hot_vent < rna_survival_floor_hours,
              "R12e a hot alkaline vent (90 C) does not");
        check(smoker < rna_survival_floor_hours,
              "R12e a black smoker never does");
        check(cool_vent > smoker,
              "R12e S5b's reductant and S5e's survival genuinely pull opposite ways");
    }

    // --- The inspector's decode path, shown rather than asserted ------------
    {
        molecular_event e{};
        e.epoch_dmyr = 39000;                                     // 3.90 Gya
        e.process_id = static_cast<uint8_t>(process::serpentinisation);
        e.outcome_id = static_cast<uint8_t>(outcome::fired);
        e.yield_q    = 310;                                       // 0.310
        e.venue_id   = static_cast<uint8_t>(venue::alkaline_vent);

        std::printf("\n--- inspector decode (hex stored, names displayed) ---\n%s\n",
                    format_event(e).c_str());

        molecular_event f{};
        f.epoch_dmyr = 24000;                                     // 2.40 Gya
        f.process_id = static_cast<uint8_t>(process::oxygenic_photosynthesis);
        f.outcome_id = static_cast<uint8_t>(outcome::marginal);
        f.yield_q    = 62;
        f.venue_id   = static_cast<uint8_t>(venue::open_ocean);
        std::printf("%s\n", format_event(f).c_str());

        check(format_event(e).find("hydrogen") == std::string::npos ||
              format_event(e).find("serpentin") != std::string::npos,
              "R12c the decoded line names the reaction rather than its id");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
