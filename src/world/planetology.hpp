#pragma once

#include "components.hpp"
#include "tile_generation.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Planetology — the body-level history pass (BL-167)
//
// A deterministic sibling pass that runs BEFORE the six-pass tile pipeline and
// answers "what is this world, and why?". It walks ten gated stages from the
// system's formation to the industrial endowment a civilisation inherits, and
// its outputs are (a) the body_profile the tile pipeline consumes, which used
// to be hand-authored, and (b) a per-resource endowment multiplier plus a
// readable history.
//
// Every stage is a threshold test on already-computed scalars, not a
// simulation. The interesting output is WHICH GATE A BODY DIED AT.
//
// Design authority: docs/generation/PLANETOLOGY.md.
// Architecture: a sibling pass, per BL-051's settled pipeline-shape convention.
// ---------------------------------------------------------------------------

/// How far up the ladder a body's biology got. Ordered — comparisons are
/// meaningful, and the gates below are all `>=` tests on this.
///
/// The split between a body's CURRENT stage and its PEAK stage is the whole
/// graveyard mechanic: fossil resources (coal, petroleum, banded iron) key off
/// the peak and survive extinction; living resources (timber, agricultural
/// produce, peat) key off the current stage and vanish with the biosphere.
enum class life_stage : uint8_t
{
    sterile = 0,    ///< Nothing ever started.
    prebiotic,      ///< Chemistry present, no self-propagating system.
    microbial,      ///< Abiogenesis fired. Anoxic.
    oxygenated,     ///< Oxygen accumulating — the GOE. Marine only.
    complex,        ///< Past the second oxygenation. Eukaryotes, shelly life.
    land,           ///< Ozone column closed; land colonised. Coal is possible here.
    civilised,      ///< Fire, metallurgy, industry.
};

/// The ten stages of the chain. A body's `died_at` names the gate it failed;
/// `chain_stage::count` means it survived every gate.
///
/// ORDERED BY CHAIN POSITION, and that ordering is load-bearing: gates and the
/// harness both compare stages with `>=` (planetology_harness R6 asserts that a
/// knob leaves every stage BEFORE its decision point untouched). So a new stage
/// is inserted at its causal position, NOT appended — the opposite of the
/// append-only rule `body_archetype` carries below. That is safe precisely
/// because no serialiser exists yet; if one lands, this enum needs a stable
/// wire mapping before anything else is inserted.
///
/// ONE ENUM FOR THE WHOLE HISTORY (BL-220 residual call 2). The stages of the
/// historical ladder (docs/lore/HISTORY.md, landing as BL-221/BL-222) join this
/// enum rather than running a parallel one, because a single ordered biography
/// is the entire presentation model — a second enum would mean the ledger could
/// not sort its own lines. Their causal position is AFTER `legacy` (S8 leaves
/// the endowment) and BEFORE `spend` (S9 draws it down, which is what
/// industrialisation DOES), with a seam reserved ahead of settlement for a
/// pre-settlement narrative stage.
enum class chain_stage : uint8_t
{
    system = 0, ///< S0 — one roll for the whole campaign.
    accretion,  ///< S1 — what the body is made of.
    air,        ///< S2 — the retention shoreline.
    engine,     ///< S3 — the interior heat clock and tectonic regime.
    water,      ///< S4 — the thermostat and its two irreversible failures.
    spark,      ///< S5 — abiogenesis.
    breath,     ///< S6 — oxygenation, in two steps.
    green,      ///< S7 — land, and the fire threshold.
    legacy,     ///< S8 — the endowment life leaves behind.
    spend,      ///< S9 — industrial drawdown (homeworld only).
    count,
};

/// The one-word verdict. Derived FROM the timeline by a switch on which gate
/// the body died at, never rolled ahead of it — so the name always names a
/// cause the player can read in the history above it.
enum class body_archetype : uint8_t
{
    cradle = 0,     ///< Survived everything, then spent it. Earth.
    mat_world,      ///< Died at the GOE. Colossal iron, real oil, no land life, no fire.
    boring_billion, ///< Passed the GOE, never got the second. O2 present and useless.
    graveyard,      ///< Climbed the ladder, then a terminal extinction. Geology outlives biology.
    waterworld,     ///< No land to colonise. Passes every naive habitability test; hosts no industry.
    oven,           ///< Runaway greenhouse. Venus.
    snowball_lock,  ///< Ice-albedo runaway with no tectonic way back.
    relict_wet,     ///< Was wet, then the air bled away over Gyr. Mars.
    ocean_vault,    ///< Airless, but life under the ice on tidal heat. Europa.
    cryo_organic,   ///< Thick cold atmosphere, hydrocarbons, no liquid water. Titan.
    bake_out,       ///< Tidal runaway — resurfaced, volatile-stripped. Io.
    dead_rock,      ///< Never held air. Luna, Mercury.
    core_fragment,  ///< Differentiated then stripped. The object IS the ore body.

    // --- Added by BL-209 (the molecular event trace) ------------------------
    // APPENDED, never inserted: these ids are a save-format value and a
    // determinism input, so existing archetypes must keep their numbers.
    // None of the three is produced yet — the S5/S6 gates that reach them are
    // the implementation work. See backlog.json § BL-209.
    silent_eden,      ///< Died at S5c or S5e on an otherwise perfect world.
    rna_lock,         ///< Died at S5g. Real life, permanently simple.
    ferrotroph_world, ///< Died at S6b. Unbounded banded iron, and no oxygen ever.
};

/// How far the abiogenesis chain actually got, gate by gate.
///
/// A SEPARATE AXIS FROM `life_stage`, deliberately. `life_stage` is the
/// ECONOMIC gate — every resource rule in the model is a `>=` test on it, and
/// inserting new rungs would renumber it and silently move those tests. This
/// enum answers a different question: not "what can this world grow?" but "how
/// far did its chemistry climb before it stopped?".
///
/// Everything below `cellular` maps to `life_stage::prebiotic`, so the resource
/// model is untouched while the history gains seven distinguishable endings.
enum class abiogenesis_depth : uint8_t
{
    none = 0,      ///< No organic feedstock ever formed. S5a.
    feedstock,     ///< HCN and formaldehyde present, no free energy. S5b.
    reductant,     ///< Energy and gradient, no reactive phosphorus. S5c.
    nucleotides,   ///< Monomers made, never concentrated enough to link. S5d.
    polymers,      ///< Chains formed, nothing copied itself — or heat erased it. S5e.
    replicators,   ///< Copying, but no compartment and so no individuality. S5f.
    cellular,      ///< Cells, held under the RNA-world genome ceiling. S5g.
    coded,         ///< DNA, proofreading, and a genome that can grow.
};

// ---------------------------------------------------------------------------
// Checkpoints — where a branch decision lives, and how a lean narrows it
// (BL-217, GENERATION_CHECKPOINT_BRANCH_MODEL). This is the class-agnostic
// mechanism; PLANETOLOGY.md's S5-S8 mass-extinction points are the first
// checkpoint class to use it, and a second (BL-218's historical rupture, at
// the settlement stage) is meant to reuse this file's types unchanged.
//
// See PLANETOLOGY.md's "Checkpoints" section for the full design rationale —
// this header only carries the settled shape.
// ---------------------------------------------------------------------------

/// One resolved branch decision. Held in an APPEND-ONLY, ORDERED, TAGGED list
/// on `planetology_state` (`checkpoints`, below) — deliberately shaped to
/// match BL-208's eventual world-history-log so migrating into it is a MOVE,
/// not a rewrite, once that lands. BL-208 is not built yet and this struct
/// must not depend on it.
///
/// No random-access map keyed by stage, and no mutable last-branch-wins
/// field: either would make BL-208 a conversion project instead of a move.
///
/// `seed_used` is mandatory, not diagnostic — a branch that cannot be
/// replayed in isolation from its own seed is not debuggable, and any future
/// sweep tool or bug report needs to re-run exactly this decision. Within
/// `run_planetology`, the per-body `seed` argument plus `stage_id` is enough
/// to reconstruct the sub-stream a checkpoint drew from (each checkpoint
/// class owns a fixed stage-tag table), so `seed_used` is recorded as that
/// per-body seed rather than an internal RNG state.
struct checkpoint_record
{
    chain_stage stage_id = chain_stage::system; ///< Which gate this checkpoint sits at.
    std::string branch_taken;                   ///< Human-readable outcome label.
    uint32_t    seed_used = 0;                   ///< The seed this decision is replayable from.
    bool        viability_result = true;         ///< Did the resulting state clear its floor?
};

/// A deterministic splitmix64 sub-stream, public (unlike planetology.cpp's
/// internal `rng`) so a checkpoint resolver OUTSIDE planetology.cpp — a
/// future checkpoint class such as BL-218's historical rupture — draws from
/// the SAME RNG mechanism rather than inventing a second one
/// (PLANETOLOGY.md § Determinism & cost).
struct checkpoint_rng
{
    uint64_t s;
    explicit checkpoint_rng(uint32_t seed, uint32_t stage_tag);

    /// Uniform in [0,1). Mantissa-mapped, matching planetology.cpp's `rng`.
    float unit();

    /// Uniform integer in [0, count). `count` must be > 0.
    int index(int count);
};

/// One branch a checkpoint could take, and whether a lean's eligibility
/// filter allows it to be proposed this attempt.
///
/// A LEAN IS AN ELIGIBILITY FILTER, NEVER A WEIGHT (BL-217, settled
/// 2026-08-02): it only removes candidates from this list. The choice among
/// the survivors stays the existing uniform reject-and-reroll — a lean must
/// never make a branch more likely, only impossible.
struct checkpoint_branch
{
    const char* label = "";     ///< `branch_taken` text if this one is chosen.
    bool        eligible = true;
};

/// Resolve one checkpoint, class-agnostically.
///
/// @param stage_id     Which chain_stage this checkpoint sits at (recorded,
///                     not interpreted by this function).
/// @param seed         The body's per-body seed (recorded as `seed_used`).
/// @param stage_tag    This checkpoint's fixed RNG sub-stream tag.
/// @param max_attempts Reroll cap — a pathological all-ineligible lean must
///                     not spin forever.
/// @param propose      `(checkpoint_rng&, uint32_t attempt) ->
///                     std::vector<checkpoint_branch>`. Builds this attempt's
///                     candidate list; a lean's eligibility filter sets
///                     `eligible = false` on the entries it excludes.
/// @param apply        `(checkpoint_rng&, const checkpoint_branch&) -> void`.
///                     Applies the chosen branch to caller-owned state
///                     (mutate via the caller's own closure capture).
/// @param floor_ok     `() -> bool`. The checkpoint class's own viability
///                     predicate, evaluated AFTER `apply` — reroll on false,
///                     per the homeworld rule (constrain inputs, never clamp
///                     outputs).
/// @param out          Receives exactly one checkpoint_record per attempt, so
///                     the whole reroll history is visible, not just the
///                     final winner.
/// @return true once a branch clears `floor_ok` within `max_attempts`.
///
/// If EVERY candidate at an attempt is ineligible, that attempt is itself a
/// viability failure — the loop rerolls rather than falling back to an
/// ineligible branch (the corollary BL-217 calls out explicitly).
///
/// GENERIC ACROSS CHECKPOINT CLASSES: nothing here names "biology" or the
/// four S5-S8 stages — a future checkpoint class reuses this unchanged by
/// supplying its own propose/apply/floor_ok.
template <typename Propose, typename Apply, typename FloorOk>
bool resolve_checkpoint(chain_stage stage_id, uint32_t seed, uint32_t stage_tag,
                        uint32_t max_attempts,
                        Propose propose, Apply apply, FloorOk floor_ok,
                        std::vector<checkpoint_record>& out)
{
    for (uint32_t attempt = 0; attempt < max_attempts; ++attempt)
    {
        checkpoint_rng r(seed, stage_tag ^ (attempt * 0x2545F491u));

        std::vector<checkpoint_branch> candidates = propose(r, attempt);

        int eligible_count = 0;
        for (const auto& c : candidates) if (c.eligible) ++eligible_count;

        if (eligible_count == 0)
        {
            // The lean's eligibility filter excluded every candidate. That IS
            // a viability failure — never silently fall back to an
            // ineligible branch.
            out.push_back(checkpoint_record{ stage_id, "(no eligible branch — rerolled)", seed, false });
            continue;
        }

        int pick = r.index(eligible_count);
        const checkpoint_branch* chosen = nullptr;
        for (const auto& c : candidates)
        {
            if (!c.eligible) continue;
            if (pick == 0) { chosen = &c; break; }
            --pick;
        }

        apply(r, *chosen);
        const bool ok = floor_ok();
        out.push_back(checkpoint_record{ stage_id, chosen->label, seed, ok });
        if (ok) return true;
    }
    return false;
}

/// The calendar year the campaign opens in — 1 January 1960 (ERAS.md § Era 0),
/// and the zero point every `history_event` counts backwards from.
///
/// `ui::fmt::campaign_epoch_year` (src/ui/format.hpp) is the same year for the
/// tick calendar; the two must agree, and tile_inspector.cpp static_asserts it
/// where both are in scope. It is duplicated rather than shared because the
/// world layer cannot include a UI header.
constexpr int64_t campaign_epoch_year = 1960;

/// Convert deep time in billions of years to the stored representation.
///
/// Deep-time gates read naturally in Gya ("the GOE at 2.4"), so the emitters
/// still say it that way and this narrows it once, here. The rounding is a
/// plain offset-and-truncate rather than `std::llround` to keep libm out of a
/// value that participates in sorting (PLANETOLOGY.md § Determinism & cost).
constexpr int64_t years_from_gya(float gya)
{
    const double y = static_cast<double>(gya) * 1.0e9;
    return static_cast<int64_t>(y >= 0.0 ? y + 0.5 : y - 0.5);
}

/// Convert a calendar year (negative for BCE, astronomical numbering) to the
/// stored representation. The historical ladder's entry point.
constexpr int64_t years_from_calendar_year(int64_t year)
{
    return campaign_epoch_year - year;
}

/// One dated line of a body's history. `event` is the left column (what
/// happened); `consequence` is the right column (what it left behind) and is
/// empty for lines that carry no endowment payload.
///
/// The right column is the entire feature — "640 Myr of ferruginous ocean ->
/// iron x2.8" is a sentence a player argues with; a bare scalar is one they
/// scroll past. See PLANETOLOGY.md § Presentation.
struct history_event
{
    /// Years before the campaign epoch. Positive is the past; 0 is the epoch
    /// itself.
    ///
    /// ONE FIELD SPANS BOTH REGIMES rather than two the consumers must branch
    /// on — deep time and recorded history sort into a single ordered
    /// biography, which is the whole presentation model.
    ///
    /// It is an INTEGER for three reasons, of which only the first two are
    /// what BL-220 originally claimed:
    ///
    ///  1. DISPLAY. 1450 CE is 5.1e-7 Gya. The ledger's `%.2f Gya` renders
    ///     every historical date as "0.00", and no single float format reads
    ///     sensibly at both 4.5 Gya and 1687 CE. R14 pins this.
    ///  2. ARITHMETIC. Any date derived NEAR the epoch from a deep-time
    ///     baseline loses catastrophically: `4.5f - 273yr` is exactly `4.5f`.
    ///     The historical ladder computes dates that way constantly.
    ///  3. EXACTNESS. An integer year is exact by construction, sorts exactly,
    ///     and tie-breaks deterministically — and a timestamp that
    ///     participates in sorting is effectively a gate path
    ///     (PLANETOLOGY.md § Determinism & cost).
    ///
    /// Correcting the record: BL-220's filed design also claimed float would
    /// make "two events centuries apart compare EQUAL". That is overstated —
    /// float32 holds ~7 significant digits regardless of exponent, so 1687 and
    /// 1688 stored directly as Gya do compare unequal and round-trip intact.
    /// The conclusion stands on (1) and (2); the stated mechanism did not.
    ///
    /// 4.5 Gyr needs 33 bits; int64_t leaves the range unbounded for any stage
    /// that lands later.
    ///
    /// Build it with `years_from_gya` or `years_from_calendar_year`; render it
    /// with `format_history_date`, never by hand.
    int64_t     years_before_epoch = 0;
    chain_stage stage = chain_stage::system;   ///< Which gate emitted this line.
    std::string event;                         ///< Left column — the cause.
    std::string consequence;                   ///< Right column — the endowment effect (may be empty).
};

/// Render a timestamp for display, picking its unit from its magnitude.
///
/// Deep time is NOT linearised (PLANETOLOGY.md § Presentation) — "4.50 Gya" and
/// "1687" are the same field seen at the two ends of its range:
///
///   | >= 1 Gyr        | `4.50 Gya`         |
///   | >= 1 Myr        | `252 Mya`          |
///   | >= 10 kyr       | `11,650 years ago` |
///   | within 10 kyr   | `1687`, `-3200`    |
///   | 0               | `now`              |
///
/// The 10 kyr boundary is the conventional start of the Neolithic: older than
/// that a date is a CLIMATE event and reads as an interval; younger than that
/// it is a HUMAN one and reads as a calendar year. That is what puts the
/// historical ladder's own lines (BL-221's granary cities, its charters at
/// 1602) on the calendar side without a second field to switch on.
///
/// The boundary is a judgement call, not a physical constant, and it is the
/// one number here worth revisiting: push it too late and a Younger-Dryas
/// climate line starts claiming a calendar year it has no business having;
/// too early and a granary-cities line reads as a bare interval.
std::string format_history_date(int64_t years_before_epoch);

/// The player-facing generation knobs, chosen on the New World setup screen and
/// carried in `world_params`. Defaults reproduce a Sol-like system with an
/// Earth-like homeworld.
///
/// These are deliberately few. Each one has to change something a player can
/// read in the resulting history, or it does not belong here
/// (PLANETOLOGY.md § Presentation — "for every generated value, name the
/// decision it changes").
struct planetology_params
{
    /// Stellar mass in solar masses. Sets luminosity (L ~ M^3.5), main-sequence
    /// lifetime, the ice line, and therefore every body's instellation. A heavier
    /// star pushes the habitable corridor outward and shortens the time budget.
    float star_mass = 1.00f;

    /// System age in Gyr. The radiogenic heat budget decays against it (so old
    /// systems are tectonically dead), and it is the time budget the whole
    /// biological ladder has to climb.
    float system_age_gyr = 4.55f;

    /// System metallicity as a multiple of solar. One nebula, one metallicity —
    /// this is the system-wide ore floor, and it correlates every body.
    float metallicity = 1.00f;

    /// Homeworld target ocean fraction. Buys shelf area (marine oil, coastline)
    /// and costs land (coal, arable, exposed ore). Arable peaks around 0.5-0.6.
    float home_ocean = 0.60f;

    /// The iron/coal dial, and the model's best piece of chemistry.
    /// 0 = oxygenated EARLY: a short ferruginous window (iron-lean), but ozone
    ///     arrives early, land plants get a long run, and coal is rich.
    /// 1 = oxygenated LATE: an unbounded banded-iron window (iron-rich), but
    ///     late land colonisation and a short coal age.
    /// One number sets both, in opposite directions, with every gate still passed.
    float oxygenation = 0.50f;

    /// Homeworld industrial drawdown [0,1] — the fraction of the ACCESSIBLE
    /// surface endowment the prior industrial era has already consumed. The ore
    /// is still there; the cheap ore is not. This is the generated, in-fiction
    /// reason a corporation goes to space.
    float drawdown = 0.62f;

    /// Homeworld mass in Earth masses. Sets escape velocity (and so atmospheric
    /// retention), the interior heat budget, the mobile-lid floor, and the cost
    /// of reaching orbit. Decided at the Accretion stage.
    float home_mass = 1.00f;

    /// Radiogenic endowment (U/Th/K) as a multiple of Earth's, INDEPENDENT of
    /// metallicity — U and Th come from rare r-process events, so mantle Th/U
    /// plausibly varies an order of magnitude between systems at the same
    /// [Fe/H]. Drives how long the interior stays hot. Decided at Engine.
    float radiogenic = 1.00f;

    /// How readily life starts once the chemistry preconditions are met [0,1].
    /// 1.0 fires whenever the gate passes; 0.0 never. This is openly a
    /// game-design dial wearing scientific clothing — the real probability is
    /// unconstrained by orders of magnitude (n = 1, with anthropic selection
    /// guaranteeing we observe the world where it happened). Decided at Spark.
    float abiogenesis_ease = 1.00f;

    /// Coal-basin climate [0,1]: 0 = seasonal and well-drained (mires never
    /// stack), 1 = everwet equatorial swamp over subsiding basins. ~90% of
    /// Earth's coal comes from one such window, and the cause is climate x
    /// tectonics rather than anything about the plants. Decided at Green.
    float coal_climate = 0.50f;
};


/// The authored physical facts about a body — the inputs the chain consumes.
/// These stay hand-written per body (mass and orbit are what a prototype body
/// set IS); everything downstream of them is derived.
struct body_inputs
{
    const char* name                = "";
    float       mass_earths         = 1.0f;
    float       orbit_au            = 1.0f;   ///< Semi-major axis from the star.
    float       parent_mass_earths  = 0.0f;   ///< Non-zero for moons — drives tidal heating.
    float       parent_orbit_au     = 0.0f;   ///< The moon's distance from its primary, in AU.
    float       eccentricity        = 0.0f;
    bool        is_homeworld        = false;  ///< Runs the constrained-input corridor.
    bool        core_fragment       = false;  ///< Differentiated then stripped (Pallas).
    float       ice_shell_hint      = 0.0f;   ///< >0 marks a candidate subsurface-ocean body.
};

// ---------------------------------------------------------------------------
// The prototype body set
//
// The authored physical facts for the four bodies the prototype ships. Shared by
// make_hard_coded_world and the New World wizard's live preview so the charts a
// player makes decisions against cannot drift from the world actually built.
// ---------------------------------------------------------------------------

int              prototype_body_count();
const body_inputs& prototype_body(int index);
uint32_t         prototype_body_seed(int index); ///< Pre-XOR per-body seed literal.

/// One endemic trade good a body's biosphere evolved — the C -> D stage.
///
/// Endemism is the mechanism. A biosphere does not spread a species evenly over a
/// world; it evolves it in one climate band, in one region, and nowhere else.
/// That is what made nutmeg, cinchona and tobacco worth crossing an ocean for:
/// not utility, but the fact that you could not grow them at home.
///
/// So an endemic good carries its ORIGIN, and its price is a function of distance
/// from that origin (docs/economy/RESOURCES.md § Mercantile value track).
struct endemic_good
{
    resource_type good = resource_type::tobacco;

    /// The climate band it evolved in, as |latitude|: 0 at the equator, 1 at the
    /// pole. Expressed this way rather than as the tile pipeline's internal band
    /// enum so the two stay decoupled.
    float lat_lo = 0.0f;
    float lat_hi = 1.0f;

    /// The region it spread across, as a fraction of the circumference. This is
    /// the part that makes it endemic rather than merely climatic: two worlds can
    /// both have temperate grassland and only one of them grows tobacco.
    float sector_centre = 0.0f; ///< 0-1 around the globe.
    float sector_width  = 0.2f; ///< 0-1 fraction of the circumference.

    float richness = 1.0f;
};

/// Everything the chain computed for one body.
struct planetology_state
{
    body_archetype archetype = body_archetype::dead_rock;
    chain_stage    died_at   = chain_stage::count; ///< `count` == survived every gate.
    life_stage     stage     = life_stage::sterile; ///< CURRENT — gates living resources.
    life_stage     peak      = life_stage::sterile; ///< PEAK — gates fossil resources.

    // --- Derived physical state (the numbers the report quotes) ---
    float v_esc_kms           = 0.0f;
    float instellation        = 0.0f; ///< S, in Earth units.
    float shore               = 0.0f; ///< (v_esc/11.186)^4 / S, normalised so Mars = 1.0.
    float theta               = 0.0f; ///< Thermal budget; Earth-now = 1.0.
    float surface_pressure_bar= 0.0f;
    float surface_temp_k      = 0.0f;
    float o2_fraction         = 0.0f; ///< Mole fraction, 0-0.35.
    float ferruginous_gyr     = 0.0f; ///< Banded-iron window duration — the iron term.
    float marine_anoxia_gyr   = 0.0f; ///< The petroleum term.
    float land_burial_gyr     = 0.0f; ///< The coal term.
    float arable_share        = 0.0f;
    float drawdown            = 0.0f; ///< Homeworld only.
    float cold_traps          = 0.0f; ///< Polar water ice on airless bodies.
    bool  mobile_lid          = false;///< Plate tectonics — gates porphyry copper.
    bool  core_exposed        = false;

    /// The DERIVED solar parameters. This is the narrow waist: Planetology's
    /// minimum viable output is that these six stop being hand-authored literals
    /// in make_hard_coded_world().
    body_profile profile{};

    /// Per-resource deposit multiplier, applied inside the tile pass. 1.0 is
    /// neutral; 0.0 means the body cannot have that resource at all.
    std::array<float, resource_count> endowment{};

    /// What this biosphere evolved that grows nowhere else. Empty on any world
    /// that never reached a land biosphere — the C -> D stage is downstream of
    /// B -> C, so a world with no forests has no cash crops either.
    std::vector<endemic_good> endemics;

    /// The readable history. Short for a failed world — the brevity IS the
    /// characterisation, and it is never padded.
    std::vector<history_event> history;

    /// Every checkpoint branch decision this body's chain resolved, in the
    /// order it resolved them (BL-217). APPEND-ONLY — see checkpoint_record.
    /// Today's only checkpoint class is the S5-S8 biological mass-extinction
    /// retrofit; it does not change the biology itself, only makes the branch
    /// legible. A failed reroll attempt (see resolve_checkpoint) also appends
    /// a record, so this can hold more entries than there are gates.
    std::vector<checkpoint_record> checkpoints;
};

/// Run the full ten-stage chain for one body.
///
/// A pure deterministic function of (@p seed, @p in, @p p) — no global state, no
/// entropy, no standard-library distributions, and no transcendentals in any
/// gate path (PLANETOLOGY.md § Determinism & cost names both hazards).
///
/// @param in   The body's authored physical facts.
/// @param p    The campaign's planetology knobs.
/// @param seed Per-body seed, already folded with the campaign seed.
/// @return     The body's derived profile, endowment, archetype and history.
planetology_state run_planetology(const body_inputs& in,
                                  const planetology_params& p,
                                  uint32_t seed);

// ---------------------------------------------------------------------------
// Preferences — the New World wizard's actual input
//
// The wizard does not set parameters, it expresses PREFERENCES. A lean narrows
// the range a value is sampled from; the value itself is rolled from the seed
// and is never editable. "A dimmer star" is a preference; `star_mass = 1.0342`
// is a setting, and the moment a player can type it the screen is a form again.
//
// Ben's framing (2026-07-22): "if you have preferences you can find them, but
// really you don't get full customization. Just a step more detail than a
// seeded world."
// ---------------------------------------------------------------------------

/// A preference on one axis. `any` samples the whole viable range.
enum class lean : uint8_t { any = 0, low, mid, high };

/// What the player expresses, grouped into the wizard's three rounds. The
/// grouping is the chain's own A -> B -> C shape: what kind of world is this,
/// what happened on it, and what did you walk into.
struct world_preferences
{
    // --- Round A: the System ---
    lean star       = lean::any; ///< dimmer / sun-like / brighter
    lean world_size = lean::any; ///< small / earth-like / large
    lean interior   = lean::any; ///< cold and old / moderate / young and vigorous
    lean metal      = lean::any; ///< metal-poor / normal / metal-rich nebula

    // --- Round B: Life ---
    lean ocean        = lean::any; ///< continental / balanced / oceanic
    lean oxygen_story = lean::any; ///< low = oxygenated early (coal-rich, iron-lean);
                                   ///  high = oxygenated late (iron-rich, coal-lean)
    lean coal_basins  = lean::any; ///< seasonal / mixed / everwet

    // --- Round C: Inheritance ---
    lean drawdown = lean::any; ///< barely touched / worked / stripped

    /// Per-round reroll counter. Bumping one re-draws that round (and everything
    /// downstream of it) without disturbing the rounds already committed above it.
    uint32_t roll[3] = { 0, 0, 0 };
};

/// Whether a generated homeworld clears the strict Earth-like floor.
///
/// Ben's call (2026-07-22): the floor stays STRICT — every homeworld should be
/// recognisably Earth — and a miss is handled by REJECTING AND REROLLING rather
/// than by clamping, so no value is ever silently overridden.
struct viability
{
    bool        ok     = false;
    const char* reason = ""; ///< The clause that rejected it, for the sweep's histogram.
};

viability homeworld_viability(const planetology_state& s);

/// Preferences + campaign seed resolved into concrete parameters.
struct resolved_world
{
    planetology_params params;
    float              home_orbit_au = 1.0f; ///< DERIVED from the star, not authored.
    uint32_t           attempts      = 1;    ///< Viability draws consumed; 1 = first passed.
    bool               gave_up       = false;///< Hit the attempt cap; params are the best seen.
};

/// Resolve preferences into the parameters generation actually runs on.
///
/// Pure in (@p pref, @p seed). Internally rejects and rerolls until the homeworld
/// clears homeworld_viability, folding the attempt index into the draw so the
/// whole search stays a deterministic function of the seed — same seed and
/// preferences, same attempt count, same world.
///
/// The homeworld's ORBIT is derived here rather than authored. Luminosity goes as
/// M^3.5, so pinning the orbit at 1 AU while the star's mass varies swings
/// instellation across roughly 0.17-4.1 against a viable window near 0.34-1.05 —
/// which is why two thirds of unshaped draws used to die at the Water gate as
/// Ovens and Snowballs. A homeworld sits in its star's habitable zone by
/// construction, not by luck.
resolved_world resolve_preferences(const world_preferences& pref, uint32_t seed);

/// Run the chain over the whole prototype body set — the live preview the New
/// World wizard draws its charts from. Cheap enough (well under a millisecond)
/// to call every frame a slider moves.
///
/// @param p             The resolved parameters.
/// @param home_orbit_au The homeworld's derived orbit (resolved_world::home_orbit_au).
///                      Passing <= 0 keeps the authored orbit.
/// @param seed          The campaign master seed.
/// @param out           Receives one state per body, in prototype_body order.
void preview_system(const planetology_params& p, float home_orbit_au, uint32_t seed,
                    std::vector<planetology_state>& out);

/// Human-readable names, for the report surface.
const char* archetype_name(body_archetype a);
const char* archetype_blurb(body_archetype a); ///< One line: what it has and lacks.
const char* life_stage_name(life_stage s);
const char* chain_stage_name(chain_stage s);
const char* chain_stage_title(chain_stage s); ///< The question the stage answers.
