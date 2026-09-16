#pragma once

// ---------------------------------------------------------------------------
// colonisation — how a people comes to live where it lives, before anybody
// fights over it (BL-846 / BL-847 / BL-848 / BL-850).
// ---------------------------------------------------------------------------
//
// `docs/generation/COLONISATION.md` is the authority for every rule below; this
// header is the work, and it does not restate the design. What it does record
// is the two or three places where a design sentence had more than one legal
// implementation, and which one was taken.
//
// THE ONE THING TO KNOW BEFORE READING ANY OF IT: there is NO ACTOR HERE.
// No verb, no score, no utility comparison, no polity making a call. Surplus
// population, the ground in front of it, and the package it carries are the
// whole of the model. That is why the span needs no AI-behaviour grant, and it
// is why nothing in this file may ever grow one — `.claude/rules/io-standing-
// rules.md` requires a widening to be RAISED, never assumed, and a diffusion is
// a force rather than an agent. If a later design wants a people to CHOOSE
// where to spread, that is Ben's grant to give and not this file's to take.
//
// AND THE SECOND: COLONISATION CONSUMES NO INFRASTRUCTURE. No roads, no works,
// no logistics, no supply. The only cost of colonisation is that people
// physically move, and that cost is paid in YEARS. Nothing here is spent, so
// nothing here is budgeted, accounted or balanced — which is exactly what makes
// the span cheap. A stream either has reached ground or has not reached it yet.
//
// ---------------------------------------------------------------------------
// ONE FLOOD, NOT A WALK PER SOURCE — the implementation call that decides the
// cost, and the one place this file departs from the obvious reading
// ---------------------------------------------------------------------------
//
// COLONISATION.md § The stream says "surplus flows to the cheapest reachable
// ground its package can farm", which reads naturally as a search per settled
// region. That is what the Era -1 sim already does for military reach, and
// BL-844 records what it costs: a per-region Dijkstra that is 41% of a
// 4000-year run and QUADRATIC in the region count (measured 2026-09-09,
// exponent 1.95-2.27 across seeds 0 and 1, history_span_cost on build_gen).
// Doing it once per settled region per step would put the span's whole budget
// into the same quadratic.
//
// So the walk here is a SINGLE MULTI-SOURCE frontier over the tile raster,
// advanced incrementally across the span and never recomputed. Every settled
// region seeds it at its own anchor with an arrival year of its founding; the
// frontier expands outward in year-cost order; and each tile is claimed once,
// by whichever stream reaches it first. That is linear in tiles for the whole
// span (~9,500 land tiles on a 261x121 homeworld) rather than quadratic in
// regions, and it is not an approximation of the design — it is the design
// stated as a field instead of as a set of searches. "The cheapest reachable
// ground" and "whoever's stream arrived first" are the same sentence read from
// the tile's end rather than from the region's.
//
// IT ALSO HANDS US CULTURE-BY-ROUTE FOR FREE (BL-848). The flood has to record
// which source claimed each tile in order to run at all, and that record IS the
// answer to "which culture does this ground inherit" — a path fact, not a
// distance fact. There is no second pass, and no proximity rule left anywhere:
// a cradle stops owning the ground beyond its own mountains because the stream
// that came the long way round by the coast got there in fewer years. The god
// map stops being a Voronoi of cradles and becomes a record of routes, which is
// what CREEDS.md always claimed it was and could not deliver, because nobody
// walked.
//
// DETERMINISM. The frontier is a binary heap under a TOTAL order — arrival year
// first, then tile index, then source region — so two streams arriving in the
// same year cannot have their tie broken by heap layout. Costs are integer
// centi-years throughout; there is not a float in the decision path, for the
// same reason history_sim.hpp gives.

#include "components.hpp"

#include <array>
#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Farm classes — the axis a package's affinity is expressed over
// ---------------------------------------------------------------------------

/// WHAT KIND OF GROUND THIS IS, FOR THE PURPOSE OF FARMING IT.
///
/// A COARSENING OF THE THREE TERRAIN AXES, and deliberately coarse. TILES.md's
/// two-axis terrain crossed with landform is 8 x 10 x 7 combinations, and a
/// package carrying an affinity per combination would be both large and
/// meaningless — nothing in the design distinguishes farming volcanic-scrub-
/// canyon from farming volcanic-scrub-rift. What the design DOES distinguish is
/// the handful of ground types a domestication package can be coined on and
/// stall at the edge of: COLONISATION.md's own examples are the floodplain, the
/// valley, the steppe and the forest.
///
/// TWELVE, and the number is a budget rather than a taxonomy: a package's
/// affinity is then twelve bytes, which is small enough to sit on a cradle and
/// to be read inside the flood's inner loop without a table lookup becoming the
/// span's cost. COLONISATION.md § Open questions leaves the representation to
/// the implementation, and this is the answer to it.
///
/// APPEND-ONLY. `count` is the wire width of every affinity array; inserting a
/// value would re-index every package ever coined.
enum class farm_class : uint8_t
{
    floodplain = 0,  ///< Silt under standing water — marsh cover on soft ground. The first granaries.
    grassland  = 1,  ///< Open fertile cover on soil. The ground a broad package spreads across.
    woodland   = 2,  ///< Forest cover. Cleared before it is farmed, which is why it is dear twice.
    valley     = 3,  ///< Low ground between higher terrain; sheltered and fertile.
    highland   = 4,  ///< Elevated plateau. Thin soil, short season.
    montane    = 5,  ///< Mountain, canyon and rift. Farmed only by a package coined on it.
    steppe     = 6,  ///< Dry open ground — scrub or bare soil with no standing water.
    arid       = 7,  ///< Dunes, salt crust, barren ground. Farmed by almost nothing.
    coastal    = 8,  ///< Land on the shoreline ring. Fish and shell as much as grain.
    volcanic   = 9,  ///< Ash over volcanic rock. Extremely fertile, and dangerous.
    boreal     = 10, ///< Cold ground: snow cover or icy substrate.
    stone      = 11, ///< Rocky and metallic substrate under thin or no cover.

    count      = 12
};

inline constexpr int farm_class_count = static_cast<int>(farm_class::count);

/// Classify one tile. PURE and total — every land tile gets exactly one class,
/// and water gets none (the caller must not ask; see `colonisation_field`).
///
/// ORDER MATTERS AND IS DELIBERATE. The tests run most-specific first, so a
/// marsh on volcanic rock reads as `volcanic` (the thing that makes it unusual)
/// rather than as `floodplain` (the thing it shares with half the map). A tile
/// that answers to nothing falls through to `steppe`, which is the honest
/// default for open ground the other eleven tests did not recognise — not a
/// sentinel, and not an error.
farm_class classify_farm_class(terrain_substrate s, terrain_cover c,
                               terrain_landform lf, bool shoreline);

// ---------------------------------------------------------------------------
// The domestication package (BL-847)
// ---------------------------------------------------------------------------

/// WHAT A CRADLE RAISED FROM ITS OWN BIOSPHERE, and the mechanism that makes
/// distance from a cradle cost something.
///
/// Without it every region is founded on identical terms whatever its distance
/// from the first granary — so there is no frontier, nothing stalls, and a
/// migration route carries nothing but headcount. COLONISATION.md § The
/// domestication package is the authority; the two fields below are its two.
///
/// COINED EXACTLY AS ONE PANTHEON IS, from the ground the cradle's own window
/// holds. Package and pantheon are the same act of self-description read on two
/// axes, which is WHY the map of gods and the map of farming end up being the
/// same map rather than two maps that happen to agree.
struct domestication_package
{
    /// Per-mille yield on each farm class, indexed by `farm_class`. Zero means
    /// this package cannot farm that ground AT ALL — and ground no package
    /// suits is simply not settled. Emptiness is a real outcome here, not a
    /// failure to fill.
    std::array<uint16_t, farm_class_count> affinity{};

    /// How many classes it tolerates at all — the count of non-zero entries
    /// above. Stored rather than derived because it is read once per candidate
    /// tile inside the flood, and because it is the quantity the sweep argues.
    ///
    /// BREADTH IS THE SPAN'S ASYMMETRY GENERATOR, AND THAT IS ITS JOB. A broad
    /// package colonises a continent; a narrow one fills its valley and stops.
    /// Two cradles of identical richness therefore produce wildly different
    /// worlds, and neither is clamped toward the other — which is precisely
    /// what GENERATION_STRATEGY.md § Asymmetry is the deliverable asks
    /// generation to produce and currently has no instrument for.
    uint8_t breadth = 0;

    /// Yield on @p f, 0 where this package cannot farm it.
    int yield_on(farm_class f) const
    {
        const int i = static_cast<int>(f);
        return (i >= 0 && i < farm_class_count) ? affinity[static_cast<std::size_t>(i)] : 0;
    }

    bool can_farm(farm_class f) const { return yield_on(f) > 0; }
};

/// The floor a class's raw suitability must clear to enter a package at all.
/// Below it the cradle never domesticated anything for that ground, and the
/// package carries a zero rather than a small number — the difference between
/// "farms it badly" and "does not farm it" is the whole of what makes a
/// frontier stall.
inline constexpr int package_affinity_floor = 120;

/// Coin one cradle's package from the ground inside its window.
///
/// AFFINITY FROM THE CRADLE'S OWN GROUND: a package coined on a floodplain
/// farms floodplains, and carries nothing about a steppe it never saw. The
/// window is a square of radius @p window_radius around (@p col, @p row) — the
/// same basin the ladder scored the cradle on.
///
/// BREADTH FROM HOW VARIED THAT WINDOW WAS: a cradle in uniform country coins a
/// NARROW package; a cradle spanning a gradient coins a BROAD one. It falls out
/// of the same count rather than being drawn beside it, which is what keeps the
/// two fields one act of self-description instead of two rolls.
///
/// SEEDED, NOT ROLLED. There is no RNG in this function: the package is a
/// deterministic consequence of upstream scalars, in the sense
/// GENERATION_STRATEGY.md § Asymmetry is the deliverable means it.
domestication_package coin_package(const std::vector<terrain_substrate>& substrate,
                                   const std::vector<terrain_cover>&     cover,
                                   const std::vector<terrain_landform>&  landform,
                                   int gw, int gh, int col, int row,
                                   int window_radius);

/// The floored union of two packages — what a daughter founded on ground
/// marginal for A, beside a people carrying B, carries away.
///
/// FLOORED SO A DAUGHTER IS NEVER BETTER THAN THE BETTER PARENT: each class
/// takes the MAXIMUM of the two affinities, never their sum. Nobody trades and
/// nobody decides; adjacency is the whole mechanism.
///
/// KEPT DELIBERATELY, NOT BY DEFAULT (Ben, 2026-09-09). Cutting it was the live
/// alternative — the world's settlement pattern would then be fully determined
/// at Stage 0 by the cradle windows, which is simpler and defensible. It
/// survives because a frontier that can never unstick makes the long run
/// static, and the 4000-year ladder is the target.
domestication_package cross_packages(const domestication_package& a,
                                     const domestication_package& b);

// ---------------------------------------------------------------------------
// Predation (BL-850) — what caps the pressure
// ---------------------------------------------------------------------------

/// THE GROUND'S OWN DANGER, per-mille, before anybody lives on it.
///
/// Two terms, both read from passes that already run (COLONISATION.md
/// § Predation): the BODY term, from PLANETOLOGY.md's simulated biosphere —
/// how far life got and how productive it is, so a world that never reached
/// land animals carries none of this at all — and the REGION term, from
/// terrain cover, where dense forest and wetland are dangerous and open
/// lowland and cold country much less so.
///
/// IT IS NOT `hazard`, AND MUST NOT BE FOLDED INTO IT. `tile_component::
/// hazard_level` is an EXTRACTION danger consumed by the economy's site draw,
/// and Ben ruled against `hazard` as a body-scale state on 2026-08-31 — its
/// design job is off-world. Predation is a distinct quantity with a distinct
/// consumer, and sharing the name would silently couple two unrelated models.
///
/// @param body_biosphere_q  0-1000, how far life got on this body.
int predation_base_q(int body_biosphere_q, terrain_cover c, terrain_landform lf);

/// Predation actually in force on ground carrying @p population heads.
///
/// SETTLEMENT CLEARS PREDATORS, AND THE DECAY IS LOGARITHMIC IN POPULATION
/// (Ben, 2026-09-09). Each DOUBLING of population buys the same fixed
/// reduction, so the returns diminish forever: the first settlers pay the most
/// and their descendants pay steadily less, without any population ever quite
/// clearing the ground.
///
/// That form is load-bearing in both directions. A permanent wall would have
/// made the long run static in exactly the way § Packages broaden by crossing
/// exists to avoid; a decay that reached ZERO would have made every frontier
/// temporary, which is the same defect wearing the opposite sign. A log curve
/// has no population at which it is done, so wild country stays marginally
/// wild — predation is a TRANSIENT FRONTIER COST, never fully bought off.
///
/// IT READS *CURRENT* POPULATION, SO PREDATION COMES BACK. A region the sim
/// sacks loses the heads that were holding the wild down, and its predation
/// rises again toward what the ground carries on its own. CONQUEST RE-WILDS
/// GROUND — for free, out of a rule written for something else.
///
/// @param per_doubling_q  What one doubling of population buys, per-mille of
///                        the base. COLONISATION.md leaves this magnitude to
///                        `history_sweep`; the default here is a plausible
///                        placeholder so the loop runs, NOT a tuned value.
int predation_now_q(int base_q, int64_t population, int per_doubling_q);

/// The default `per_doubling_q`. A PLACEHOLDER awaiting the sweep, in the same
/// sense history_sim_params' `w_*` weights are placeholders — set to a
/// plausible magnitude so the loop runs and a harness can bind assertions to
/// its behaviour.
///
/// SIZED TO THE RANGE THAT ACTUALLY OCCURS, and that is the whole of why it is
/// 40 rather than something rounder. A region is founded with ~2,000 heads and
/// grows toward a capacity in the high hundreds of thousands, so the live range
/// is about 11 to 20 doublings. At 40 per doubling that range spans 440 to 800
/// per-mille bought — the curve is still moving everywhere the game actually
/// sits, and it saturates against the floor only past a billion heads.
///
/// The first cut used 90, which exhausted the floor at TEN doublings (1,024
/// heads) and so read as a CONSTANT for every region the game ever has. The
/// mechanism was not mistuned, it was inert; `colonisation_harness` C7 caught
/// it as three identical readings at 1k / 2k / 4k. Worth keeping in view: a
/// decay coefficient is only meaningful against a stated population range, and
/// this one had never been checked against the range it would meet.
inline constexpr int predation_per_doubling_default_q = 40;

/// How far predation holds sustainable population BELOW
/// `region_carrying_capacity(farm_q)`, as a per-mille multiplier on it.
///
/// THIS IS THE CAP. A penned people's surplus is consumed by death rather than
/// accumulating into a pressure the model has nowhere to send. The brake is an
/// in-world force with a cause a player can point at, never a term inside
/// anybody's head.
///
/// IT PRICES THE SAME GROUND TWICE, and that is why it earns its place: dense
/// forest is dear for a stream to CROSS and dangerous to LIVE IN once crossed,
/// so forest colonisation stalls on both terms at once, with no constant tuned
/// to make it happen.
int predation_capacity_mult_q(int predation_q);

// ---------------------------------------------------------------------------
// The year-cost walk
// ---------------------------------------------------------------------------

/// Centi-years to cross one ordinary lowland tile.
///
/// THE SPAN'S ONE SCALE CONSTANT, and it is what "the cost is paid in years"
/// means numerically. At 1200 (twelve years a tile) a stream covers roughly 330
/// tiles of easy ground across a 4000-year span, which fills a 261-wide
/// homeworld continent in one to two millennia and leaves the barriers taking
/// centuries longer. A range that costs a stream three centuries to cross is a
/// range that shaped a civilisation, and it did so without a single resource
/// changing hands.
///
/// A PLACEHOLDER LIKE THE OTHERS: `history_sweep` argues it against the shape
/// of the histories it produces, not a harness.
inline constexpr int32_t colonisation_base_centiyears = 1200;

/// THE CORRIDOR PRICE — percent of the base a stream pays on the routes people
/// actually followed: the coastal shelf and a river course (BL-857, BL-967).
///
/// ONE CONSTANT FOR BOTH, BY DESIGN. COLONISATION.md § Surplus flows names
/// "river courses and coastal shelf" as one tier of cheap ground, not two, and
/// the coast's discount is the one BL-857 argued STEEP enough that a coastal
/// route beats an inland one over any comparable distance (Ben, 2026-09-09).
/// A river takes the same price rather than a dial of its own: downstream-
/// cheaper-than-upstream, or a river cheaper than a shore, would be a second
/// claim the design does not yet make, and a second number `history_sweep`
/// would have to argue separately.
inline constexpr int32_t colonisation_corridor_pct = 30;

/// Impassable. Open ocean, and any tile no stream may enter.
inline constexpr int32_t colonisation_impassable = INT32_MAX;

/// The calendar year every cradle's stream begins moving.
///
/// EVERY CRADLE STARTS AT THE SAME MOMENT, which is the honest reading of Stage
/// 0: the cradles are where agriculture began, not a staggered set of later
/// arrivals. What separates them afterwards is the ground and the package they
/// carry, never a head start — and that matters, because a stagger would be a
/// second asymmetry generator competing with the one BL-847 exists to be.
///
/// -2400 (2400 BCE), matching `CIVILISATION.md` § The span is 400 BCE to
/// 1200 CE: pass 1 runs 2400 BCE -> 1200 CE, 3,600 years, divided at 400 BCE
/// into the migration (this constant -> 400 BCE, 2,000 years) and the empire
/// round (400 BCE -> 1200 CE, 1,600 years). BL-871 moved this from -4000
/// (which matched the OLD, undifferentiated `history_sim_params::start_year`
/// before the two rounds split) — the migration and the empire sim are no
/// longer one continuous span, so this constant now names the migration's
/// own start rather than borrowing the sim's.
inline constexpr int64_t colonisation_start_year = -2400;

// ---------------------------------------------------------------------------
// Water: the coast is the road, and a crude hop crosses a strait (BL-857)
// ---------------------------------------------------------------------------
//
// **Ben, 2026-09-09: not enough emphasis on crude coastal and overseas
// migration routes.** Two constants carry that, and both are the same claim the
// span already makes about mountains, applied to water: the routes people
// actually followed should be the cheap ones, and the map should show it.

/// How many water tiles a stream may cross in one hop, land to land.
///
/// CRUDE, AND THE WORD IS THE SPECIFICATION. This is a raft across a strait or
/// a scramble to an island already visible from the shore — not seafaring. It
/// takes no capability, no work, no harbour and no staging hub, because this
/// span has no infrastructure in it at all (§ No actor, and no infrastructure)
/// and must never grow any. It is emphatically NOT the staged harbour-works
/// model of `MILITARY_HISTORY.md` § Sea legs, which belongs to a later era with
/// institutions in it.
///
/// THREE, so a strait or an island chain is crossable and an ocean is not. The
/// bound is what keeps "people got everywhere" from becoming "people sailed",
/// and a harness asserts it: a stream must not cross open water.
inline constexpr int colonisation_max_hop_tiles = 3;

/// Centi-years a hop costs, per water tile crossed, on top of the landing.
///
/// DEAR RATHER THAN IMPOSSIBLE. A crossing is the expensive way to reach ground
/// — it should lose to any reasonable land route and win only where there is no
/// land route at all, which is exactly the case it exists for: the awkward
/// corners of a world that nothing walks to.
inline constexpr int32_t colonisation_hop_centiyears = 2600;

// ---------------------------------------------------------------------------
// Migration spawns cultures (BL-856)
// ---------------------------------------------------------------------------

/// Centi-years a stream may walk before the people who arrive are no longer the
/// people who set out.
///
/// **A culture is not only something a cradle coins; it is something a
/// migration PRODUCES (Ben, 2026-09-09).** The first build carried one culture
/// per cradle for the life of the run, so a stream that crossed a continent
/// arrived as its own ancestors and a whole homeworld ended with five peoples.
/// That is a map of where agriculture started, not a map of a migration.
///
/// TIME, NOT DISTANCE, and the choice matters. Distance walked would make a
/// people crossing easy ground diverge as fast as one grinding over a range,
/// which is backwards: what separates a daughter from its parent is the
/// GENERATIONS in between, and this span already denominates everything in
/// years. So a stream that spends six centuries getting somewhere arrives as a
/// different people whether it walked far or walked hard.
///
/// A PLACEHOLDER awaiting `history_sweep`, like every magnitude here.
///
/// 200 YEARS, DOWN FROM 600 (BL-918). Diversity is the deliverable and the
/// splits are tuned TOWARD it (COLONISATION.md § Diversity is the
/// deliverable; Ben, 2026-09-11: "if you need to over-tune splits, I
/// encourage that"). A third of the interval triples how often a lineage may
/// diverge by DISTANCE over the same walk; it stays a per-lineage rate limit,
/// so it cannot reproduce the 1,739-culture runaway, which came from counting
/// tiles rather than streams (see `run_colonisation`). Seven generations is
/// about the shortest span over which two halves of a people stop
/// understanding each other, which is the thing this number is standing for.
inline constexpr int64_t colonisation_split_centiyears = 20000; // 200 years

/// Centi-years a stream must have carried ITS OWN country before crossing
/// into new country coins a daughter — the BIOME trigger's rate limit
/// (BL-918). Fifty years on the stream, twenty-five on the lineage (below).
///
/// THE BIOME TRIGGER NOW FIRES ON EVERY FARM-CLASS TRANSITION, not once per
/// class per lineage as BL-864's first cut bounded it. That bound was chosen
/// against the runaway rather than against the design: it meant a people that
/// came down from the highlands onto a floodplain, and then a second group of
/// the same people that came down two centuries later by another pass, were
/// one daughter rather than two — and the second is precisely the kin-but-
/// distinct people the round is supposed to produce.
///
/// WHAT BOUNDS IT INSTEAD is time in the country, on two clocks:
///   - a STREAM must have been a people of its class for this long before a
///     transition can split it (`front_entry::since_split_cy`), so a daughter
///     coined at a class boundary cannot coin a grand-daughter on the very
///     next tile when the boundary is jagged — the chain that would rebuild
///     the runaway one tile at a time;
///   - a LINEAGE coins at most one biome daughter per
///     `colonisation_biome_lineage_centiyears` (`last_biome_split_cy`, keyed
///     by parent), so a broad lobe crossing a boundary along its whole width
///     in the same decade produces ONE daughter rather than one per tile —
///     the grain fix, applied here as it already was to the distance trigger.
///
/// MEASURED, NOT GUESSED. A first cut set both clocks to a century and
/// `colonisation_harness` C15 read 171/220/137 cultures at a tree depth of 2
/// against the 806/794/603 at depth 16/13/11 the once-per-class bound had
/// produced — the loosening had tightened, because the old trigger carried
/// no time gate at all and its chains were doing the work. Fifty years on
/// the stream is the shortest gate that still stops the jagged-boundary
/// chain (a stream re-coined every tile), and twenty-five on the lineage is
/// enough that one lobe crossing at once is one daughter, not thirty. Both
/// are shorter than the distance interval, deliberately: a new country is a
/// stronger reason to differ than a long walk over the same ground, so where
/// both apply the country is the explanation. The recorded 1,739 runaway is
/// the ceiling to stay well under, not the target; C15 prints the count.
inline constexpr int64_t colonisation_biome_split_centiyears   = 5000; // 50 years, per stream
inline constexpr int64_t colonisation_biome_lineage_centiyears = 2500; // 25 years, per lineage

// ---------------------------------------------------------------------------
// Isolation (BL-918): a range breaks into insular groups
// ---------------------------------------------------------------------------

/// How far apart two settled regions may sit and still be one people
/// (BL-918). The same nine the Era -1 sim uses for its region graph
/// (`history_sim_params::neighbour_radius`), restated here rather than
/// included because this header has no business depending on the sim's: a
/// number the two must agree on is stated in both places with a pointer, as
/// `culture::origin_farm_class` does for `farm_class`.
inline constexpr int colonisation_isolation_radius = 9;

/// Years a group must be cut off from its people's origin before it IS a
/// people of its own — the divergence span (BL-918).
///
/// THE ISOLATION TRIGGER is the one split that happens AFTER settlement. The
/// walk's three triggers all fire on a stream in motion, so a culture that
/// had finished spreading across a range could never come apart however
/// badly the range divided it. This is that split: at each record step of
/// the migration, a culture's settled regions are partitioned into components
/// over cheap-traversal adjacency (two anchors within
/// `colonisation_isolation_radius` with no mountain, canyon, rift or water
/// on the line between them), and a component that has been cut off from the
/// component holding the culture's first settlement for longer than this
/// span coins a daughter on itself.
///
/// A CONSEQUENCE OF TERRAIN AND TIME, never a roll: which regions are cut off
/// is a fact about the ground, and how long is a fact about the calendar.
/// 200 years, matching the distance interval, and erring toward more splits
/// as the design asks — a range that has been divided for seven generations
/// has divided its people. Only the CULTURE round runs it (Ben, 2026-09-11,
/// ruling on NR-839): the Empires phase makes peoples by mixing, never by
/// isolation.
inline constexpr int64_t colonisation_isolation_span_years = 200;

/// Years between the migration's record steps — how often the partition is
/// re-read (BL-918). Fifty: fine enough that a group cut off for the span
/// is coined within a generation of it, coarse enough that a 2,000-year
/// migration is forty partitions rather than two thousand.
inline constexpr int64_t colonisation_isolation_step_years = 50;

/// WHICH OF THE FOUR WAYS a people became two (BL-918) — the axis the split
/// census is printed over. Append-only; the census array is indexed by it.
enum class split_trigger : uint8_t
{
    distance  = 0, ///< Walked long enough (BL-856; `colonisation_split_centiyears`).
    biome     = 1, ///< Settled country of another farm class (BL-864, loosened by BL-918).
    size      = 2, ///< Held more ground than one people holds (BL-864).
    isolation = 3, ///< Cut off from its origin after settlement (BL-918).

    count     = 4
};

inline constexpr int split_trigger_count = static_cast<int>(split_trigger::count);

const char* split_trigger_name(split_trigger t);

/// Tiles one culture may hold before it divides on its next founding (BL-864).
///
/// **A people spread over a continent is not one people.** Ben, 2026-09-09:
/// the Culture round is the INPUT to the conquest round, and a handful of vast
/// cultures gives Empires nothing to contest — no frontiers that mean anything,
/// few neighbours who are recognisably foreign.
///
/// A SIZE TRIGGER NEEDS NO NEW STATE: the walk already claims tiles one at a
/// time in arrival order, so counting them per culture is free. The counter
/// resets on the split, so this is bounded by (land tiles / this) rather than
/// by a threshold picked to look right — which is the discipline BL-856's first
/// cut lacked when it coined 1,739 peoples on one world.
///
/// `history_sweep`'s magnitude like every other here.
inline constexpr int colonisation_culture_max_tiles = 340;

/// One culture the migration coined: `culture` descends from `parent`.
///
/// The walk allocates ids and records the parentage; it does not coin names,
/// tongues or pantheons — that is `creeds.hpp`'s vocabulary and this header has
/// none of it. `run_settlement` materialises the record from this list.
struct culture_spawn
{
    int32_t culture = -1; ///< The new culture's id.
    int32_t parent  = -1; ///< The culture it descends from.
    int32_t tile    = -1; ///< Where it diverged, for naming and for the record.
    /// The farm class the daughter is a people OF (BL-864/BL-865). Equal to the
    /// parent's for a drift or size split; the NEW country's where the split
    /// happened because the stream settled ground of a class it was not coined
    /// on — which is the case this field exists to carry forward.
    farm_class origin_class = farm_class::steppe;
    /// The calendar year the daughter was coined — when its stream landed on the
    /// tile that split it off (BL-873). Same clock as `colonisation_field::arrival_year`.
    int64_t coined_year = 0;
    /// True when this daughter was coined at the far end of the crude overseas
    /// hop (BL-857) rather than at an ordinary land split (BL-901). The one
    /// fact of the three `sea_legs_q` terms that is a DEED rather than a
    /// circumstance — set only at the hop's spawn site in `colonisation.cpp`.
    bool crossed_water = false;
    /// Which trigger coined it (BL-918) — the axis of the split census.
    split_trigger trigger = split_trigger::distance;
};

/// Radius of the window a cradle coins its package from.
///
/// The basin the ladder already scored the cradle on, in the sense
/// `agrarian_cradle::fertile_tiles` counts it. Large enough that a cradle on a
/// terrain boundary sees BOTH sides — which is the whole of where a broad
/// package comes from — and small enough that it is a basin rather than a
/// continent.
inline constexpr int colonisation_cradle_window = 6;

/// Centi-years for a stream to cross the tile at @p idx.
///
/// COST IS GROUND ALONE — river courses and coastal shelf cheap (the routes
/// people actually followed), open lowland ordinary, barrier terrain dear,
/// open ocean impassable. Nothing about who is crossing, nothing about what
/// they carry, and nothing that could be spent.
///
/// RIVERS ARE CORRIDORS, PRICED AS THE COAST IS (BL-967). A river in this
/// codebase is an EDGE on `tile_component::river_edges`, not a tile property,
/// so the walk reads it through a one-byte raster — non-zero where any river
/// edge touches the tile — that every caller builds from the mask
/// (`run_settlement` directly; the sim through `sim_terrain_view::river`). A
/// tile on a river takes `colonisation_corridor_pct`, the SAME discount as the
/// shoreline, so a history's corridors are its rivers as much as its shores.
/// Direction is not priced: a stream walks a river's bank, and which way the
/// water flows is not a claim COLONISATION.md makes about that walk.
int32_t tile_year_cost(terrain_substrate s, terrain_cover c, terrain_landform lf,
                       bool shoreline, bool river);

// ---------------------------------------------------------------------------
// The flood
// ---------------------------------------------------------------------------

/// One source the diffusion spreads from: a settled region with surplus.
struct colonisation_source
{
    int32_t tile        = -1; ///< Raster index of the region's anchor.
    int32_t region      = -1; ///< Index into the caller's region table.
    int32_t culture     = -1; ///< The culture its stream carries.
    int64_t ready_year  = 0;  ///< Calendar year this source begins sending.
    domestication_package package{};
};

/// WHAT THE WALK LEAVES BEHIND — one entry per land tile, and the substrate
/// every downstream consumer reads.
///
/// A FIELD, NOT A LIST OF EVENTS. The flood has to record which stream claimed
/// each tile in order to run at all (that is how it knows not to re-enter it),
/// so the record costs nothing beyond the run itself, and it answers all three
/// questions the span is asked at once: WHEN was this ground reached
/// (`arrival_year`), BY WHOM (`source_region` / `culture`), and COULD anyone
/// have farmed it (`reached` false with a finite cost means the ground was
/// crossable but no arriving package could farm it — emptiness as a real
/// outcome).
struct colonisation_field
{
    int32_t gw = 0;
    int32_t gh = 0;

    /// Calendar year the first stream reached each tile; `never_reached` where
    /// none did. Parallel to the raster.
    std::vector<int64_t> arrival_year;

    /// Which source region's stream arrived first, or -1. THE CULTURE-BY-ROUTE
    /// ANSWER (BL-848): a path fact, not a distance fact.
    std::vector<int32_t> source_region;

    /// The culture that arrived, or -1. Denormalised off `source_region` so a
    /// consumer need not hold the region table.
    std::vector<int32_t> culture;

    /// The farm class each land tile was classified as. Computed once and kept
    /// because both the flood and the founding gate read it.
    std::vector<farm_class> ground;

    /// True where a stream arrived AND its package could farm the ground. Only
    /// these tiles are candidates for a founding — ground no package suits is
    /// not settled, and stays empty for as long as that holds.
    std::vector<uint8_t> farmable;

    /// THE YEAR THE MIGRATION FINISHED — when the last stream that was ever
    /// going to land, landed (BL-858). `start_year` where nothing arrived.
    ///
    /// THIS IS THE ROUND'S TERMINATING CONDITION, and it is a READING rather
    /// than a loop. Ben's rule is "all land has some culture"; a naive reading
    /// of that never terminates, because ground no package can farm never gets
    /// a culture by construction — and that is not a rare world, it is EVERY
    /// world: 44-57% of land is unfarmable today, dominated by polar ice
    /// (BL-859's census). So the condition cannot be "wait until every tile is
    /// coloured".
    ///
    /// What it actually means is: **wait until nothing more is going to
    /// happen.** The flood already answers that exactly — it terminates when
    /// its frontier is exhausted, having reached everything reachable — so the
    /// last arrival IS the moment the migration is over, and no safety stop,
    /// no iteration cap and no watchdog is needed. The walk is bounded by the
    /// tile count and cannot run away.
    ///
    /// That is why this is one field rather than a mechanism: the terminating
    /// condition was already implicit in a walk that finishes, and the only
    /// thing missing was reading it back.
    int64_t last_arrival_year = 0;

    /// THE CULTURES THIS MIGRATION COINED (BL-856), in allocation order — which
    /// is arrival order, so the list is a record of the routes that produced
    /// them. Empty when no stream walked far enough to diverge.
    std::vector<culture_spawn> spawns;

    /// HOW MANY OF `spawns` EACH TRIGGER COINED (BL-918), indexed by
    /// `split_trigger`. The walk fills the first three; `run_isolation_splits`
    /// has its own field. Redundant with a count over `spawns`, kept so a
    /// reader that only holds the census need not hold the list.
    std::array<int32_t, split_trigger_count> split_census{};

    bool empty() const { return arrival_year.empty(); }
};

/// A tile no stream ever reached.
inline constexpr int64_t colonisation_never_reached = INT64_MAX;

/// Inputs to one run of the walk. Rasters are in row * gw + col order, exactly
/// as `sim_terrain_view` holds them.
struct colonisation_input
{
    const std::vector<terrain_substrate>* substrate = nullptr; ///< Required.
    const std::vector<terrain_cover>*     cover     = nullptr; ///< Optional; `none` when absent.
    const std::vector<terrain_landform>*  landform  = nullptr; ///< Optional; `plains` when absent.

    /// OPTIONAL RIVER RASTER — non-zero where a river edge touches the tile,
    /// built from `tile_component::river_edges` by the caller (BL-967). Null
    /// prices every tile as riverless, which a synthetic map may want; the
    /// real callers always pass one.
    const std::vector<uint8_t>*           river     = nullptr;

    int gw = 0;
    int gh = 0;

    /// A FAR BACKSTOP, not the migration's end (BL-858). The walk terminates on
    /// its own when its frontier is exhausted, and `last_arrival_year` reports
    /// when that was; this only stops an arrival year running to an absurd
    /// value on a pathological map.
    int64_t boundary_year = 0;

    /// The first id the walk may allocate to a culture it coins — one past the
    /// last cradle culture. Below it are the cultures that already exist.
    int32_t first_spawn_culture = 0;
};

/// Run the diffusion.
///
/// COMPLEXITY: O(T log T) in LAND TILES, once, for the whole span — not per
/// source and not per step. See this file's header for why that is the design
/// rather than an optimisation of it.
///
/// DETERMINISM: the frontier is ordered by (arrival_year, tile, source_region),
/// a TOTAL order, so two streams arriving in the same year break their tie on
/// stable indices rather than on heap layout.
colonisation_field run_colonisation(const colonisation_input& in,
                                    const std::vector<colonisation_source>& sources);

/// Bytes the field occupies — the quantity a requirement bounds, stated the way
/// `owner_ring_bytes` states the time-lapse's.
int64_t colonisation_field_bytes(const colonisation_field& f);

// ---------------------------------------------------------------------------
// Isolation (BL-918): the split that happens AFTER settlement
// ---------------------------------------------------------------------------

/// One settled region as the isolation pass sees it — an anchor, the people
/// on it, and when they got there. The caller's `region` is far wider than
/// this and lives in a header this one must not include; the pass reads
/// three fields and writes one.
struct isolation_region
{
    int32_t tile         = -1; ///< Raster index of the anchor.
    int32_t culture      = -1; ///< The people holding it; REWRITTEN on a split.
    int64_t founded_year = 0;  ///< Calendar year settled; not present before it.
};

/// One region changing people at a record step (BL-918) — the migration
/// time-lapse's hook: a lobe changing hue as it diverges is one of these per
/// region in the component, at the year the daughter was coined.
struct region_reculture
{
    int32_t region  = -1; ///< Index into the caller's region list.
    int32_t culture = -1; ///< The daughter it now belongs to.
    int32_t parent  = -1; ///< The people it belonged to until then.
    int64_t year    = 0;  ///< The record step it happened at.
};

/// What the isolation pass produced.
struct isolation_result
{
    /// The daughters coined, in allocation order (ascending year, then the
    /// parent's id, then the component's seat), each with
    /// `trigger == split_trigger::isolation`.
    std::vector<culture_spawn> spawns;
    /// Every region that changed people, in the same order.
    std::vector<region_reculture> recultured;
    /// How many record steps the pass read.
    int32_t steps = 0;
};

/// Is this ground a BARRIER to cheap traversal for the purpose of the
/// isolation partition (BL-918)? Water, or a mountain, canyon or rift
/// landform. Pure. Stated as a function so the harness and the pass agree.
bool isolation_barrier(terrain_substrate s, terrain_landform lf);

/// Are two anchors CHEAPLY ADJACENT — within `colonisation_isolation_radius`
/// (Chebyshev over the wrapped grid, as the sim's region graph measures) with
/// no barrier tile on the straight line between them? Pure; integer only.
/// @p barrier is one byte per tile in raster order.
bool isolation_adjacent(const std::vector<uint8_t>& barrier, int gw, int gh,
                        int32_t tile_a, int32_t tile_b);

/// Run the isolation split over a settled map (BL-918).
///
/// At each record step from @p start_year to @p end_year (every
/// `colonisation_isolation_step_years`, and the end year itself), every
/// culture's regions founded by that year are partitioned into components
/// over `isolation_adjacent`. The ORIGIN component is the one holding the
/// culture's earliest-founded region (ties on the lower index). A region in
/// any other component is cut off; a component whose oldest cut-off member
/// has been cut off for `colonisation_isolation_span_years` or more coins a
/// daughter (id from @p next_culture, which is advanced) on its seat — its
/// earliest-founded region — and every region in it is rewritten to the
/// daughter in @p regions.
///
/// DETERMINISM: regions are walked in index order, components are keyed by
/// their lowest region index, and every tie is broken on an index. No float,
/// no roll, no container whose iteration order is a hash's.
///
/// Reads @p f only for `ground` (the daughter's origin class) and @p in for
/// the barrier rasters. Complexity: one adjacency build over the regions
/// (bucketed by radius, so linear in regions times a local neighbourhood),
/// then per step a union-find over the adjacency edges.
isolation_result run_isolation_splits(const colonisation_input&      in,
                                      const colonisation_field&      f,
                                      std::vector<isolation_region>& regions,
                                      int32_t&                       next_culture,
                                      int64_t                        start_year,
                                      int64_t                        end_year);

// ---------------------------------------------------------------------------
// Why a cradle stopped (BL-851)
// ---------------------------------------------------------------------------

/// WHAT HAPPENED TO ONE SOURCE'S STREAM.
///
/// FOUR OUTCOMES LOOK IDENTICAL FROM OUTSIDE — "the cradle stopped" — and only
/// one of them is death. COLONISATION.md § What a cradle stopping actually
/// means lists them, and the reason they need separating is not tidiness: a
/// sweep that cannot tell them apart cannot argue a single constant in that
/// document. `N cradles stopped` carries no information when SESSILE-FOREVER is
/// a normal and frequent outcome by design — the breadth term in BL-847 makes
/// it one deliberately — because the reading cannot distinguish the design
/// working from a cradle that was never viable.
enum class cradle_outcome : uint8_t
{
    /// It spread: it holds farmable ground beyond its own anchor.
    spread = 0,

    /// NARROW AFFINITY, no adjacent ground of a matching class. The stream
    /// walked — it crossed real ground — and none of what it crossed could be
    /// farmed by what it carried. Sessile, NOT dead: people persist, and the
    /// gods never leave home.
    sterility = 1,

    /// AFFINITY IS FINE; every exit is barrier terrain whose year-cost the span
    /// never pays. Sessile too, and distinguishable from sterility only by WHY
    /// the stream stopped — which is exactly why both are listed. Read here as
    /// "it barely moved at all", against sterility's "it moved and found
    /// nothing".
    encirclement = 2,

    /// A NEIGHBOUR'S STREAM REACHED THE SAME GROUND FIRST and `culture_shares`
    /// split. Not death: the gods survive as a minority share and the record is
    /// intact. Read as "ground this package COULD have farmed was claimed by
    /// somebody else".
    dilution = 3,

    /// SUSTAINABLE POPULATION SITS BELOW the density Stage 0 needs for surplus.
    /// The only one of the four that IS death — and it should never be reached,
    /// because `agrarian_score` reads predation, so lethal ground is never
    /// chosen as a cradle in the first place. A cradle that forms and then
    /// fails is the worse design of the two: it spends a simulation discovering
    /// what a score could have said for free. Seeing this outcome at all is a
    /// finding about the SELECTION, not about the span.
    predation_floor = 4,
};

const char* cradle_outcome_name(cradle_outcome o);

/// Per-source population facts the outcome test needs and the field does not
/// carry. Supplied by the caller because the field is about GROUND and this is
/// about the people standing on it.
struct cradle_vitals
{
    /// Sustainable heads on this source's own ground, AFTER predation has taken
    /// its share (`region_carrying_capacity` x `predation_capacity_mult_q`).
    int64_t sustainable = 0;

    /// The headcount below which a people raises no surplus to send at all —
    /// Stage 0's density requirement. Below it the cradle is on the predation
    /// floor whatever the ground around it looks like.
    int64_t surplus_threshold = 0;
};

/// Classify every source's outcome against a finished field.
///
/// ORDER OF TESTS IS THE CLASSIFICATION, and it runs most-fatal first so a
/// cradle that is BOTH penned and dying is reported as dying. Returns one entry
/// per source, in source order.
///
/// PURE, and a read rather than a step: it changes nothing and the field it
/// reads is already final. `vitals` may be empty, in which case the predation
/// floor is never reported — the honest answer when the caller did not supply
/// the population facts, rather than a silent zero that would read as "nobody
/// died".
std::vector<cradle_outcome> classify_cradle_outcomes(
    const colonisation_field&               f,
    const std::vector<colonisation_source>& sources,
    const std::vector<cradle_vitals>&       vitals);
