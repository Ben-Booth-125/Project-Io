#pragma once

// ---------------------------------------------------------------------------
// Settlement & industrialisation — HISTORY.md Stages 3-4, made mechanical
// (BL-218 nations rewrite + BL-219's region read; docs/lore/HISTORY.md,
// docs/generation/NATION_GENERATION.md).
//
// The ladder (BL-221) counted how many independent peoples the land supported.
// The creeds (BL-235) gave each of them a tongue and a pantheon. This pass is
// the rung between those and the political map: it settles PROVINCES, carries
// each one's founding CULTURE forward, industrialises the ones the ground can
// pay for, and then hands the political pass its seeds.
//
// THREE THINGS THIS PASS IS FOR:
//
//  1. BELIEF MAPPED ONTO GROUND. A region is not a blob with a name; it is
//     a place a named people settled, and it carries their gods. The culture
//     it inherits is the nearest cradle's, so the pantheon distribution is a
//     map of who walked where. A region in an ancient ore window whose
//     culture raised a FORGE god industrialises earlier than one that did not
//     — the creed and the deposit are the same historical fact seen twice.
//
//  2. CHARACTER AS AN OUTPUT, NOT A DRAW. NATION_GENERATION's Pass 4 drew
//     ideology/expansionism/economic_focus from an independent RNG. Here each
//     axis derives from one quantity this pass produced (BL-218, settled
//     2026-08-02): expansionism <- the border-contest integral, economic_focus
//     <- the dominant resource class of the regions settled DURING
//     industrialisation, ideology <- industrialisation timing against
//     neighbours.
//
//  3. THE RECORD IS NOT SAFE. Wars redraw borders, spread the victor's gods
//     over the loser's regions, and ERASE part of the loser's history —
//     replaced by a dated lacuna, not silently dropped (Ben, 2026-08-02:
//     "don't be afraid to have parts of the record erased when two nations go
//     to war"). A history that cannot be damaged is a history nobody fought
//     over.
//
// THE PASS DRIVES, IT DOES NOT NARRATE — the BL-221/BL-235 rule, inherited.
// Regions are placed BEFORE the political map and become the nation seeds;
// the BFS/growth machinery BL-053 tuned is kept and only its INPUTS change
// (BL-218 § 1: "Seeding changes. Expansion does not.").
//
// DETERMINISM. Integer scoring with explicit index tie-breaks, fresh stage
// tags, every container walk in raster or sorted-id order, no transcendentals.
// The rupture checkpoints reuse planetology.hpp's class-agnostic
// `resolve_checkpoint` rather than inventing a second branch mechanism —
// BL-217 named this pass as its intended second client.
// ---------------------------------------------------------------------------

#include "creeds.hpp"
#include "history_ladder.hpp"
#include "planetology.hpp"
#include "world.hpp"

#include <cstdint>
#include <string>
#include <vector>

/// What a region's ground is mainly good for — the ANCIENT endowment, read
/// once from the deposits under it and never re-read. This is the quantity
/// BL-218 asks economic_focus to derive from and BL-219 asks corporate focus
/// to derive from, so it is deliberately coarse and shared.
enum class region_class : uint8_t
{
    none = 0,  ///< Nothing worth naming; a subsistence region.
    farm,      ///< Agricultural produce dominates.
    ore,       ///< Iron/copper/rare-earth — the forge god's country.
    energy,    ///< Coal and petroleum: the Stage 4 fuel that breaks the ceiling.
    port,      ///< Coastal, deposit-poor: it lives on what passes through.
};

/// One settled region — the unit of settlement history, and the unit BL-219
/// reads a corporation's focus from.
struct region
{
    int anchor = -1;        ///< Raster index (row * gw + col) of the core tile.
    int col = 0;
    int row = 0;

    /// Index into `creed_state::cultures` — WHOSE GODS this region keeps.
    /// Starts as the nearest cradle's and can be overwritten by conquest.
    int culture = -1;
    /// The culture that FOUNDED it. Never overwritten, so a conquered region
    /// still records who built it — the erasure is of the record, not of this.
    int founding_culture = -1;
    /// True once a conqueror's pantheon replaced the founders' here.
    bool creed_conquered = false;

    std::string name;       ///< "<people>'s <quarter>", in the founders' tongue.

    int settle_score_q = 0; ///< 0-1000 — how strongly the ground invited settlement.

    /// Ancient endowment windows, 0-1000, surveyed once over the region's
    /// neighbourhood. These are the deposits that were already in the ground
    /// before anybody arrived; industrialisation spends them.
    int farm_q = 0;
    int ore_q = 0;
    int energy_q = 0;
    int port_q = 0;
    region_class dominant = region_class::none;

    int64_t founded_year = 0;     ///< Calendar year settled (negative = before epoch year 0).
    int64_t industrial_year = 0;  ///< Calendar year the furnaces lit; 0 when never.
    bool    industrialised = false;

    int nation = -1;   ///< Index into the nation-id list, once the political pass has run.
    int contest_q = 0; ///< 0-1000 — how hard this region's frontier was pressed.

    // --- Demography (BL-273) ----------------------------------------------
    // The region is the unit of population as well as of settlement — see
    // demography.md's section header below for the model. Left at zero here;
    // seeding an initial headcount is the graduation pass's job (BL-271), not
    // this item's (design: "you don't need to wire that graduation path").

    /// Integer headcount. Zero until a caller seeds it — a demography step
    /// from zero stays zero (nothing to grow from), which is the logistic
    /// model's own edge case, not a special case coded around it.
    int64_t population = 0;
    /// Calendar year `advance_region_demography` last advanced this
    /// region to — bookkeeping so repeated calls compose (each step only
    /// covers the years since the last one), never re-read for anything else.
    int64_t last_demography_year = 0;
    /// Recruitable manpower currently banked — the army budget BL-273 closes
    /// the loop with. A bounded fraction of `population` (`manpower_ceiling`),
    /// refilled gradually by `replenish_manpower`, spent by `raise_manpower`.
    int64_t manpower_stock = 0;

    // --- Era -1 works (BL-321) --------------------------------------------
    // What this region has BUILT, and what those works are worth. The works
    // TABLE lives in works_roster.hpp/works.lua; only the per-region record
    // lives here, and deliberately as plain integers rather than a
    // `work_effect` member: works_roster.hpp already forward-declares
    // `region`, so including it back here would close a cycle to store five
    // ints. The accumulators below are the same five fields by another name.
    //
    // A BITMASK, NOT A VECTOR. The design offered either; the mask wins because
    // this struct is copied on every settle (the Settle verb push_backs a whole
    // region) and lives in a vector that grows past 400 entries inside a pass
    // already costing ~23 s of a ~25 s world. A mask keeps `region` free of
    // heap ownership and makes "have I already built this?" one AND.
    //
    // The consequence is a hard 32-row ceiling on the works table, which
    // `works_registry::load_from_lua` checks so a 33rd row fails at startup
    // rather than silently becoming unbuildable.

    /// Bit `i` set = row `i` of `works_registry::rows()` stands here. Works are
    /// PHYSICAL: conquest transfers them with the region rather than razing
    /// them, which is what makes a developed region worth taking.
    uint32_t works_built = 0;

    /// Accumulated per-mille effects of `works_built`, maintained incrementally
    /// by `apply_work_to_region` so every read is O(1). Re-derivable from the
    /// mask at any time via `works_registry::total_effect_mask`.
    int work_capacity_mod   = 0; ///< Raises `region_carrying_capacity`.
    int work_manpower_mod   = 0; ///< Raises `manpower_ceiling`.
    int work_reach_mod      = 0; ///< Discounts terrain-weighted supply cost.
    int work_defence_mod    = 0; ///< Readiness this region adds when DEFENDING.
    int work_industrial_mod = 0; ///< Pull-forward on the industrial clock.
};

// ---------------------------------------------------------------------------
// Stage 1 — the enforceable promise, as a diffusion frame (BL-638)
// ---------------------------------------------------------------------------

/// WHERE the enforceable promise was written, and how far it travelled from
/// there. HISTORY.md § Stage 1: contract law extends beyond kinship until an
/// entity can outlive its members and own, sue and be sued. The ladder already
/// picks the seat (`history_ladder_state::charter_cradle` — the best-placed
/// TRADING cradle, because Stage 1 is about promises between strangers) and
/// the creeds already plant the sealed-oath god on it. This frame is the third
/// read of that same fact: which ground the charter reached.
///
/// ERA-AGNOSTIC BY CONSTRUCTION, which is the whole reason it exists. The
/// ladder picks a charter cradle in EVERY era, so a class derived from this
/// frame says something on an antiquity world as well as an industrial one.
/// Stage 4's furnace dates cannot: an antiquity world skips Stage 4 entirely,
/// so `median_industrial_year` is 0 there and anything reading it collapses.
///
/// Pure: `derive_charter_reach` takes no rng and the two predicates take no
/// state beyond the frame and the region.
struct charter_reach
{
    /// True once a charter cradle exists at all. A world with no agrarian
    /// cradle wrote no charter, and NOTHING reached the promise there — an
    /// honest all-closed world, and the one the public floor's unmeetable
    /// waiver exists for.
    bool written = false;

    /// Index into `creed_state::cultures` of the people who keep the sealed
    /// oath — the charter cradle's own culture. -1 when no creed pass ran.
    int culture = -1;

    int col = 0;      ///< The seat's core tile (the charter cradle's).
    int row = 0;
    int grid_w = 0;   ///< Column wrap for the contact metric.

    /// Contact distance within which the charter is LIVED rather than merely
    /// known — the seat's own trading hinterland.
    int near_dist = 0;
    /// ... and within which it has at least been COPIED. Beyond this, nothing.
    int far_dist = 0;
    /// How much distance a fully-port region discounts: the charter is a
    /// merchant's instrument and arrives by ship, which is why the ladder
    /// weights coastal access so heavily when it picks the seat at all.
    int port_reach = 0;
};

/// Derive the frame from the ladder and the creeds.
///
/// The two radii are set by the ground, not by a target: the same barrier
/// terrain that prices conquest prices contact, so the charter's copies travel
/// far across open country and stall against broken country. At zero
/// resistance the copied band is half the world; at total resistance, a tenth
/// of it. The lived band is half the copied one.
charter_reach derive_charter_reach(const history_ladder_state& hl,
                                   const creed_state& cs, int gw, int gh);

/// Does this region LIVE the enforceable promise — its own creed witnesses the
/// sealed oath, or it sits inside the seat's trading hinterland?
bool charter_lived(const charter_reach& ch, const region& p);

/// Has the promise at least REACHED this region — lived, or copied from
/// neighbours in contact with the seat? HISTORY.md § Stage 2's lesson is that
/// an institution re-emerges next door, so the reach deliberately ignores
/// political borders: it is a map of contact, not of sovereignty.
bool charter_copied(const charter_reach& ch, const region& p);

/// What the settlement pass computed for one body.
struct settlement_state
{
    std::vector<region> regions;      ///< In placement order (best ground first).
    std::vector<history_event> history;   ///< Dated settlement / furnace / war lines.
    std::vector<checkpoint_record> checkpoints; ///< Rupture branch decisions (BL-217 shape).

    /// How many history lines the wars destroyed. Every one is replaced by a
    /// dated lacuna line, so this is a count of holes the player can SEE.
    int lacunae = 0;

    /// The world-median industrialisation year over industrialised regions,
    /// or 0 when none industrialised. BL-219's "early vs late" pivot reads it.
    int64_t median_industrial_year = 0;

    /// Stage 1's diffusion frame (BL-638). Carried on the settlement record
    /// rather than passed separately because every consumer already holds one:
    /// the corporation pass takes `const settlement_state*`, and the generation
    /// report copies this struct whole.
    charter_reach charter;
};

/// Settle the body: place regions, inherit each one's cradle culture, survey
/// its ancient endowment, and industrialise the ones the ground can pay for.
///
/// Pure deterministic function of its inputs. Must run AFTER `run_creeds` /
/// `record_tribal_conflict` (it reads the surviving cultures) and BEFORE
/// `generate_nations` (it supplies the seeds).
///
/// @param pl        Body planetology state (arable share, endemics).
/// @param hl        The ladder — cradle positions, fragmentation, charter cradle.
/// @param cs        The creeds — one culture per cradle, with its pantheon.
/// @param w         World holding the tiles. Read-only.
/// @param tile_ids  Raster-order tile IDs (index = row * gw + col).
/// @param gw, gh    Grid dimensions (columns wrap; rows do not).
/// @param target_regions  How many regions to aim for; the political pass's
///                  own seed budget, so the two agree by construction. Clamped
///                  to what the land can actually hold at the separation rule.
/// @param seed      Per-body seed, already folded with the campaign seed.
/// @param stop_year Calendar year the pass generates AT (BL-271). 1960 runs the
///                  full arc. Below 1700: regions founded after it are dropped
///                  (not yet founded), Stage 4 never runs (no furnace has lit by
///                  antiquity), and demography is seeded at founding then grown
///                  to `stop_year` (the graduation path the region struct
///                  names as BL-271's job).
settlement_state run_settlement(const planetology_state& pl,
                                const history_ladder_state& hl,
                                const creed_state& cs,
                                const world& w,
                                const std::vector<entity_id>& tile_ids,
                                int gw, int gh,
                                int target_regions,
                                uint32_t seed,
                                int64_t stop_year = 1960);

/// The region anchors, as raster indices, in placement order — the seed list
/// `nation_params::seed_tiles` takes. This is the whole of "seeding changes,
/// expansion does not": the Voronoi/BFS growth machinery is untouched, it just
/// starts from places people actually settled.
std::vector<int> settlement_seed_tiles(const settlement_state& ss);

/// Attribute every region to the nation that ended up holding it, compute the
/// border-contest integral, and DERIVE the three political axes from the
/// settlement record (BL-218 § 2). Overwrites whatever Pass 4 drew.
///
/// Appends the Stage 4 industrialisation lines, which can only be attributed to
/// a nation once one exists — the same two-entry-point shape the ladder uses.
///
/// @param nation_ids  Nation entity ids in generation order (generate_nations'
///                    return value); regions store indices into this list.
void derive_national_character(settlement_state& ss,
                               const creed_state& cs,
                               world& w,
                               const std::vector<entity_id>& nation_ids,
                               const std::vector<entity_id>& tile_ids,
                               int gw, int gh);

/// The historical-rupture checkpoint class (BL-218 § 2a) — collapse, war and
/// revolution as TRANSFORMS on nation state, never as narration alone.
///
///  * **Collapse** — the nation's peripheral regions are lost to their
///    nearest neighbour and their industrial clock resets; abundance falls.
///  * **War** — the contested border redraws toward the stronger; the loser's
///    expansionism rises (grievance); the victor's gods are planted on the
///    regions taken; and PART OF THE LOSER'S RECORD IS DESTROYED, replaced
///    by a dated lacuna.
///  * **Revolution** — territory untouched; the ideology axis flips; abundance
///    takes a one-off hit.
///
/// Branch eligibility is a FILTER, never a weight (BL-217's rule): a nation
/// with no land neighbour cannot go to war, a single-region nation cannot
/// collapse. Every attempt appends a `checkpoint_record`, failures included.
///
/// Mutates @p ss (records, redaction, region culture) and @p w (tile
/// ownership, nation character, resource abundance).
void resolve_historical_ruptures(settlement_state& ss,
                                 const creed_state& cs,
                                 world& w,
                                 const std::vector<entity_id>& nation_ids,
                                 const std::vector<entity_id>& tile_ids,
                                 int gw, int gh,
                                 uint32_t seed);

/// Index of the region whose anchor is nearest (col,row), or -1 when there
/// are none. Column-wrapped; ties break on the lowest region index.
int nearest_region(const settlement_state& ss, int col, int row, int gw);

/// BL-219's derivation: a corporation anchored in @p p operates at the tier its
/// region's history earned. The ancient endowment sets the base class; the
/// **movement up the value chain** — early industrialisers refined, late ones
/// stayed raw — shifts it one tier when the region industrialised before the
/// world median.
///
/// @param p        The corporation's home region.
/// @param median   `settlement_state::median_industrial_year` (0 = nobody did).
industrial_focus focus_from_region(const region& p, int64_t median);

// ---------------------------------------------------------------------------
// Demography (BL-273) — population growth, war/plague drawdown, manpower.
//
// Feeds the eventual Era -1 history sim (BL-271, not built by this item) and,
// on graduation to the 1960 era, the population-centre scale distribution
// (POPULATION.md) — neither consumer is wired up here; this is the
// region-level model only, correct and self-contained per the item's own
// scope note.
//
// INTEGER FIXED-POINT THROUGHOUT. Every rate below is a "_q" quantity in
// thousandths (1000 = 1.0), following PLANETOLOGY.md's "no floats in a gate
// path" convention — population feeds manpower, which is itself a budget
// other deterministic systems will draw against, so it is a gate path too.
// ---------------------------------------------------------------------------

/// The population a region's farm endowment can support, in raw headcount
/// — "the ground feeds who it can feed" (BL-273 design). A linear function of
/// `farm_q` (0-1000, world-relative per settlement.cpp's `score_against`) off
/// a subsistence floor, so even a farm_q=0 region holds a small population.
///
/// NOT industrialisation-aware in this first cut (BL-273 design: carrying
/// capacity "does NOT need to move with industrialisation" yet) — a future
/// item can add an industrial-era term without changing this signature.
int64_t region_carrying_capacity(int farm_q);

/// The same ceiling raised by a region's WORKS (BL-321). `capacity_mod_q` is
/// per-mille — a Granary at +180 feeds 18% more people off the same ground.
///
/// This is the promised industrialisation-aware term the one-argument overload
/// says a future item can add "without changing this signature", and it is
/// added exactly that way: the old signature still exists and still means what
/// it meant, so every caller that has no works to account for is untouched.
/// Clamped at the bottom so a (currently impossible) negative modifier cannot
/// drive the ceiling under the subsistence floor.
int64_t region_carrying_capacity(int farm_q, int capacity_mod_q);

/// Advance one region's population by `years` simulated years: logistic
/// growth toward `region_carrying_capacity(p.farm_q)`, war drawdown scaled
/// by `war_pressure_q`, then a manpower-stock replenishment pass. Integer
/// fixed-point throughout; no RNG — growth/drawdown are not a checkpoint
/// draw (only `resolve_plague_event` is, per the design's explicit call-out).
///
/// Pure function of `p`'s own fields and the two arguments: two calls with
/// identical inputs produce identical outputs, so a caller replaying the
/// same (years, war_pressure_q) sequence over the same starting region
/// reproduces the same trajectory — the determinism the sim harness checks.
///
/// @param p               Region to advance, mutated in place.
/// @param years            Simulated years this step covers; <= 0 is a no-op.
/// @param war_pressure_q   0-1000, this step's battle intensity on `p` (0 =
///                         no war). The caller derives it from combat/
///                         checkpoint records — this function only spends it.
void advance_region_demography(region& p, int years, int war_pressure_q);

/// The manpower ceiling a region's CURRENT population can support — a
/// bounded fraction (`manpower_ceiling`'s own constant), not additive, so a
/// region cannot bank more than its living population could ever field.
int64_t manpower_ceiling(int64_t population);

/// The same ceiling raised by a region's WORKS (BL-321) — an Arsenal at +320
/// lets the same population field 32% more. Per-mille, clamped non-negative.
int64_t manpower_ceiling(int64_t population, int manpower_mod_q);

/// Move `manpower_stock` a fraction of the way toward its ceiling. Called
/// once per `advance_region_demography` step (after growth/drawdown update
/// `population`) — recovery is gradual, not an instant snap-to-ceiling, so a
/// region that just spent its levy stays weak for a few years, not one.
void replenish_manpower(region& p);

/// Raise up to `want` manpower from `p.manpower_stock`. Returns the amount
/// actually raised: `0 <= raised <= min(want, manpower_stock)`, so raising
/// past what is available is bounded rather than going negative or
/// unbounded — the self-limiting close BL-273 asks for (ancient hegemonies
/// stall on manpower exhaustion rather than being capped by fiat).
int64_t raise_manpower(region& p, int64_t want);

/// Resolve one plague-event checkpoint over a body's settled regions,
/// reusing `resolve_checkpoint`'s class-agnostic mechanism from
/// planetology.hpp exactly as BL-217 named it to be reused a second time —
/// eligibility is a FILTER, never a weight. Each candidate branch is a
/// possible epicentre region; a region is ELIGIBLE only if it still has
/// population to strike (`population > 0`).
///
/// SIMPLIFICATION, noted per the design's own fallback clause: severity uses
/// a GRID-PROXIMITY connectivity proxy (nearby regions by `col`/`row`, via
/// the same Chebyshev distance settlement.cpp already computes) rather than
/// reading the full logistics/trade graph — that graph is built at a later
/// generation stage than settlement and is not reachable here without
/// pulling in the logistics module, which the design's fallback explicitly
/// allows trading for cheapness. A true trade-linked read is a future
/// refinement, not this item's job.
///
/// Mutates `regions` in place (population loss at the epicentre and its
/// grid neighbours) and appends exactly one `checkpoint_record` per attempt
/// to `checkpoints` (failed/ineligible attempts included, per BL-217's rule).
///
/// @param regions    The body's regions; indices double as branch ids.
/// @param checkpoints  Receives one record per attempt (see BL-217 shape).
/// @param seed         Per-body seed (recorded as the checkpoint's seed_used).
/// @param stage_tag    This call's RNG sub-stream tag — vary per plague-check
///                      opportunity so repeat calls draw independently.
/// @param gw            Grid width, for the column-wrapped distance metric.
/// @return true once a branch was applied and cleared its floor; false if
///         every region was ineligible (nobody left to strike) within the
///         attempt budget.
bool resolve_plague_event(std::vector<region>& regions,
                          std::vector<checkpoint_record>& checkpoints,
                          uint32_t seed, uint32_t stage_tag, int gw);
