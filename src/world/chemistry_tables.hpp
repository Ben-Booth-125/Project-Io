#pragma once

#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// Chemistry tables — the molecular vocabulary (BL-209)
//
// The species and reaction dictionary the molecular event trace is written
// against. This header is the FOUNDATION: it fixes the vocabulary and the
// on-disk record shape so the seven S5 gates and the S6 photosystem fork can be
// filled in against a stable contract.
//
// THE CENTRAL CONTRACT, and the reason this file exists separately:
//
//   An event stores IDS. Stoichiometry and names live in the tables below,
//   compiled in and never serialised.
//
// That is where the compression actually comes from — not from hex. Hex text is
// ~2x the size of binary and buys nothing as storage; it is purely a DISPLAY
// encoding for the dev inspector. Because both the inspector and any future
// player-facing surface decode through these same tables, names can never drift
// from ids, and raw bytes can never leak into a player-visible string.
//
// DELIBERATELY STANDALONE. This header depends on <cstdint>/<string> only, not
// on components.hpp or planetology.hpp, and nothing in world/* references it
// yet. The trace's STORAGE belongs to BL-208's append-only world log; this file
// is only the vocabulary, so it can land ahead of that item without wiring.
//
// Design authority: docs/development/backlog.json § BL-209 (designed
// 2026-07-28). Propagates into docs/generation/PLANETOLOGY.md when the work
// lands, per the standing authority-time-slice rule.
// ---------------------------------------------------------------------------

namespace chem {

/// Which gate emitted an event. S5 is the seven-gate abiogenesis chain; S6a-c
/// are the cofactor forks that now sit IN FRONT of the existing flux-balance
/// oxygenation test rather than replacing it.
enum class substage : uint8_t
{
    s5a_feedstock = 0,  ///< Anything to build with? HCN and formaldehyde.
    s5b_reductant,      ///< Free energy of the right shape? Serpentinisation.
    s5c_phosphorylation,///< Could it make a nucleotide? The hard one.
    s5d_concentration,  ///< Dense enough to polymerise? The 0.1 M threshold.
    s5e_replicator,     ///< Did a polymer copy itself AND survive the heat?
    s5f_compartment,    ///< Did it become an individual?
    s5g_code,           ///< Did DNA lift the genome ceiling?
    s6a_anoxygenic,     ///< Photoferrotrophy — banded iron with no free oxygen.
    s6b_oxygenic,       ///< The Z-scheme. Gates on manganese.
    s6c_nitrogen,       ///< Nitrogenase. Gates on molybdenum, and so on soil.
    count,
};

/// Where a reaction ran. The venue is load-bearing at S5d and S5e: it sets both
/// the concentration mechanism and the temperature the polymers must survive.
enum class venue : uint8_t
{
    none = 0,
    alkaline_vent,     ///< Lost City analogue. 45-90 C, pH 9-11, up to 200 mM H2.
    black_smoker,      ///< ~350 C. Reductant in abundance, and lethal to RNA.
    subaerial_pool,    ///< Wet-dry cycling. Needs land, and drives condensation.
    hydrothermal_pore, ///< Thermophoresis, ~5000x. Works WITHOUT land.
    carbonate_lake,    ///< Ca drops out as carbonate, freeing phosphate.
    subsurface_ocean,  ///< Under the ice. The one branch that survives an airless verdict.
    open_ocean,        ///< Always too dilute. "The ocean was a soup" is chemically false.
    atmosphere,
    count,
};

/// How a gate resolved. `marginal` is not decoration — PLANETOLOGY.md's
/// anti-rigging device is to surface the MARGIN, so a world that barely passed
/// reads as lucky rather than as scripted.
enum class outcome : uint8_t
{
    failed = 0,  ///< The gate did not pass. The chain ends here.
    marginal,    ///< Passed, but only just. Print the margin.
    fired,       ///< Passed comfortably.
    bypassed,    ///< Not evaluated — an earlier gate already terminated the chain.
    count,
};

/// What kind of thing a species is. Drives grouping in the inspector.
enum class species_class : uint8_t
{
    volatile_gas = 0, ///< Atmospheric feedstock.
    mineral,          ///< Rock-forming and ore phases.
    ion,              ///< Dissolved species and cofactor metals.
    monomer,          ///< Building blocks.
    polymer,          ///< Chains.
    membrane,
    product,          ///< Outputs that matter downstream.
    count,
};

/// The species dictionary. Deliberately compact — every entry has to be named
/// by at least one reaction below, or it does not belong here.
enum class species : uint8_t
{
    none = 0,
    // --- Volatiles ---
    n2, co2, ch4, h2, h2o, nh3, co, h2s,
    // --- Prebiotic C/N carriers ---
    hcn, hcho, formamide,
    // --- Minerals ---
    olivine, serpentine, magnetite, hematite, brucite, mackinawite, pyrite,
    apatite, schreibersite,
    // --- Ions and cofactors ---
    fe2, fe3, mg2, mn, mo, phosphite, orthophosphate, citrate,
    // --- Monomers ---
    ribose, adenine, guanine, cytosine, uracil, glycine, nucleotide,
    // --- Polymers ---
    oligonucleotide, rna, ribozyme, peptide, dna,
    // --- Membrane ---
    fatty_acid, vesicle,
    // --- Products ---
    o2, acetate, organic_carbon,
    count,
};

/// The reaction dictionary. `process_id` in a molecular_event indexes this.
enum class process : uint8_t
{
    none = 0,
    // --- S5a Feedstock ---
    hcn_synthesis,          ///< Spark/UV on a reduced gas mix. Adenine is literally (HCN)5.
    formaldehyde_synthesis, ///< The sugar precursor.
    impact_reduction,       ///< An iron-rich impactor reduces H2O to H2 for ~10^5-10^6 yr.
    // --- S5b Reductant ---
    serpentinisation,       ///< Exothermic. The free reductant, and the proton gradient.
    wood_ljungdahl,         ///< CO2 + H2 -> acetate. The oldest carbon-fixation path.
    fes_barrier_growth,     ///< Fe(Ni)S walls whose unit cells match ferredoxin's active site.
    // --- S5c Phosphorylation ---
    schreibersite_corrosion,///< Meteoritic Fe3P -> reactive reduced phosphorus.
    carbonate_lake_release, ///< Ca precipitates as carbonate; P concentrates ~10^5-10^6x.
    nucleoside_phosphorylation,
    // --- S5d Concentration ---
    hcn_oligomerisation,    ///< Above ~0.1 M only. Below ~0.01 M hydrolysis wins.
    formose_reaction,       ///< Formaldehyde -> ribose.
    wet_dry_condensation,   ///< Dehydration drives it; in bulk water it is uphill.
    thermophoretic_accumulation, ///< ~5000x in a pore network, with no land at all.
    // --- S5e Replicator ---
    template_extension,     ///< Nonenzymatic copying — the RNA world's onset.
    phosphodiester_hydrolysis, ///< THE COMPETING REACTION. Steeply temperature-dependent.
    ribozyme_folding,
    // --- S5f Compartment ---
    vesicle_assembly,
    cation_flocculation,    ///< Mg2+ destroys the membrane that RNA catalysis needs.
    // --- S5g Code ---
    ribosomal_translation,  ///< The ribosome is itself a ribozyme.
    ribonucleotide_reduction, ///< RNA -> DNA. Lifts Eigen's error threshold.
    proofreading_replication,
    // --- S6 ---
    photoferrotrophy,       ///< Fe(II) as donor. Precipitates banded iron with NO free oxygen.
    oxygenic_photosynthesis,///< Splitting water needs +1.23 V — hence two photosystems, and Mn.
    nitrogen_fixation,      ///< N2's 941 kJ/mol triple bond. Mo-dependent.
    bif_precipitation,      ///< Fe(III) is insoluble; it rains out basin-wide.
    organic_carbon_burial,  ///< Exactly 1.00 mol O2 stranded per mol C buried.
    count,
};

/// One event in the trace. EXACTLY 8 BYTES, fixed width, flat binary — Io's
/// settled serialisation format (TECH_FOUNDATIONS.md). No strings, ever.
///
/// Budget: ~60 events on a living world, ~10 on a dead one, so under 500 bytes
/// per body — inside the envelope PLANETOLOGY.md already reserves.
struct molecular_event
{
    uint16_t epoch_dmyr = 0;  ///< Deci-Myr before present (Myr x 10). Covers 6.5 Gyr.
    uint8_t  process_id = 0;  ///< chem::process.
    uint8_t  outcome_id = 0;  ///< chem::outcome.
    uint16_t yield_q    = 0;  ///< Fixed-point quantity, 1/1000. Meaning is per-process.
    uint8_t  venue_id   = 0;  ///< chem::venue.
    uint8_t  flags      = 0;  ///< Reserved. Keeps the record 8-byte aligned.
};

static_assert(sizeof(molecular_event) == 8, "molecular_event must stay 8 bytes — it is a save-format record");

/// A species' display identity. `formula` is the compact form ("HCN"); `name`
/// is the canonical chemical name ("hydrogen cyanide"). Ben's rule: any surface
/// a human reads shows the NAME, never the id and never the raw bytes.
struct species_info
{
    const char*   formula = "";
    const char*   name    = "";
    species_class cls     = species_class::product;
};

/// A reaction's display identity and stoichiometry. Up to three reactants and
/// three products; unused slots are species::none.
struct process_info
{
    const char* name        = "";
    const char* consequence = ""; ///< The because-clause. Never a bare scalar.
    substage    gate        = substage::count;
    species     reactants[3]{};
    species     products[3]{};
};

const species_info& info(species s);
const process_info& info(process p);

const char* substage_name(substage s);
const char* venue_name(venue v);
const char* outcome_name(outcome o);

/// RNA phosphodiester half-life, in hours, at a given temperature.
///
/// THE DETERMINISM-CRITICAL FUNCTION IN THIS FILE. Hydrolysis is Arrhenius, and
/// PLANETOLOGY.md § Determinism & cost bans exp/log/pow anywhere in a gate path
/// — they are not bit-identical across compilers, libm versions or fast-math
/// settings. So this is a 16-entry TABLE over 0-150 C with linear interpolation
/// between bins, on the precedent of the radiogenic decay already binned at
/// 0.05 Gyr. Never reintroduce a transcendental here.
///
/// The curve runs ~10x per 20 C: roughly 4 years at 25 C, hours near 100 C.
/// This is what creates S5e's antagonism with S5b — vents supply the reductant
/// and the proton gradient, and vents are hot.
float rna_half_life_hours(float temp_c);

/// The survival threshold a replicator has to clear at S5e, in hours. Sits near
/// 55 C, which cuts straight through the Lost City band (45-90 C): the cool end
/// of an alkaline vent works, the hot end does not, and a black smoker never does.
constexpr float rna_survival_floor_hours = 1000.0f;

/// Decode one event into the dev inspector's two-line form: raw hex on the
/// first line, the reaction resolved to chemical NAMES on the second.
///
///   3f 0c 02 4e 20 02 00 00
///     3.90 Gya  serpentinisation  fired  yield 0.31  venue alkaline vent
///     olivine + H2O -> serpentine + magnetite + H2
std::string format_event(const molecular_event& e);

/// The raw record as lowercase hex, space-separated. Display only — the bytes
/// on disk are binary.
std::string hex_bytes(const molecular_event& e);

/// Every process id in the table resolves, and every species it names resolves.
/// The orphan-id check is BL-209's key new invariant (harness group R12): ids and
/// display names are now decoupled, so nothing else catches a table that has
/// drifted out of sync with the enums.
bool tables_self_consistent();

} // namespace chem
