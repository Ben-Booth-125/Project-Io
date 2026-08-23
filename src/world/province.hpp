#pragma once

#include "entity.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Province partition (BL-515) — provinces GROWN from settlement, stopped by
// terrain
// ---------------------------------------------------------------------------
// ALGORITHM RULING, 2026-08-21 (Ben, having seen the 3x3-block partition
// rendered): "packing each province perfectly looks nice, but it is scarcely
// how borders were defined in history." BL-515 replaces the block partition —
// the third and settled algorithm. What the province IS, and everything
// downstream of it (BL-511 rendering, BL-513's ceiling, march_unit's payload),
// is unchanged; only the shapes are drawn differently.
//
// A deterministic, seeded partition of every body's tiles into small,
// contiguous, purely spatial cells.
//
// BL-516 (Ben, 2026-08-21): "We can also draw provinces over the ocean, using
// 3-12 size coastal tile provinces. Ocean provinces should be much larger, but
// not larger than say 80 tiles." So the partition now covers WATER as well, in
// three DOMAINS that are partitioned separately and never mix:
//
//   * LAND          — passes 1-3 below, the 7-12 band, unchanged in every respect.
//   * COASTAL WATER — the shoreline ring and the lakes, on the SAME 3-12 band as
//                     land. No population centre seeds one; it is hinterland only.
//   * OPEN OCEAN    — the sea out of sight of land, on its own much larger band.
//
// A PROVINCE NEVER SPANS TWO DOMAINS. The land-only invariant is NARROWED, not
// deleted (NR-428): every LAND province is still hex-connected land, and no
// province anywhere mixes land with water or a lake with the sea. The domains
// are exclusive by construction — a tile's substrate names exactly one — so the
// claim is structural rather than checked.
//
// NOTHING CAN BE IN A SEA PROVINCE YET, and that is expected: units are
// land-bound (march_unit refuses a water destination outright), buildings refuse
// water, and a sea province sustains zero of them. They are addressable empty
// space, built without inventing the naval model that will eventually fill them.
//
// The province is deliberately NOT the region: no name, no owner, no culture,
// no economy. It exists because BL-467 (battle state) needs an engagement
// envelope with a stable identity, and folds the province id into the battle's
// seed stream.
//
// THE FOUR RULINGS THE ALGORITHM IMPLEMENTS
//
//   1. Provinces GROW FROM POPULATION CENTRES, and seed strength scales with
//      the centre's scale (1-5): a metropolis draws a larger province than a
//      village does.
//   2. BOUNDARIES ARE RIVERS, ELEVATION DIFFERENCE, AND SOMETIMES ROADS — but
//      a ROAD BINDS. Tiles a road links tend to share a province; a road is
//      never a divider.
//   3. Country no centre reaches becomes HINTERLAND provinces under the same
//      boundary rules, seeded from the LEAST-ACCESSIBLE tile so a hinterland is
//      shaped by its terrain rather than being whatever was left over.
//   4. Size is a growth BUDGET, not a clamp: 7-12 soft, 3-12 hard, and
//      BOUNDARIES WIN TIES. TINY PROVINCES ARE KEPT — "don't reject tiny
//      provinces" — so nothing is merged away to satisfy a floor. 12 is a
//      PREFERENCE (Ben, 2026-08-21, NR-438: "we prefer up to 12 tiles, but up
//      to 20 is permitted in rare cases"); the absolute bound is 20, and it is
//      the only size claim the harness asserts.
//
// THE ID ORDER IS STILL THE CONTRACT. Downstream code walks provinces in
// ascending `province::id` and gets an order that does not depend on container
// internals, tile-map iteration order, or the order bodies were created in.
//
// THE PARTITION IS PART OF WORLD GENERATION AND VERSIONS WITH IT. It is never
// patched in place: a change to the algorithm re-rolls every battle in every
// world. Authority: docs/generation/TILE_GENERATION.md § Province partition,
// docs/GLOSSARY.md (the spatial vocabulary) and BL-515.
// ---------------------------------------------------------------------------

struct world;

// ---------------------------------------------------------------------------
// The size band — a GROWTH BUDGET, not a clamp (Ben's ruling, 2026-08-21)
// ---------------------------------------------------------------------------

/// Soft floor of the target band, and the growth budget a hinterland seed is
/// given. SOFT: a region grows freely until it holds this many tiles, and past
/// it annexes only ground NO HARDER TO REACH than the ground it already holds
/// (the mean of its own step costs — see build_province_partition's pass 1).
/// That self-referential brake is what "boundaries win ties" means
/// mechanically, and it needs no threshold constant of its own. Nothing
/// enforces this floor downward: a province that ran out of land ships small.
inline constexpr std::size_t k_province_min_tiles = 7;

/// PREFERRED ceiling, and the clamp GROWTH obeys. Passes 1 and 2 stop here
/// outright, whatever the cost of the next edge. It is NOT the bound a shipped
/// province is guaranteed to satisfy — see `k_province_hard_cap_tiles`.
///
/// Ben, 2026-08-21, ruling on NR-438: "We prefer up to 12 tiles, but up to 20
/// is permitted in rare cases." So 12 stopped being the one absolute rule in
/// this file the moment singleton absorption (pass 3) could carry a full
/// region past it, and the ruling makes that deliberate rather than a slip.
inline constexpr std::size_t k_province_max_tiles = 12;

/// HARD CAP — the bound that really is absolute, and the only size assertion
/// the harness makes (Ben, 2026-08-21, NR-438). Nothing in the partition may
/// ship a province larger than this; `province_partition_harness` § P5a fails
/// if one does.
///
/// It is not enforced by a clamp, and deliberately so. Absorption picks a
/// singleton's CHEAPEST neighbour, and choosing a costlier one to respect a
/// size bound would contradict the cheapest-edge rule the whole growth model is
/// expressed in. So the cap is ASSERTED rather than imposed: it is a claim about
/// what the cost model produces, and it breaks loudly if that stops being true.
///
/// Headroom, measured: the shipped partition tops out at 16 tiles across the
/// 6-seed sweep, so the cap holds today with four tiles to spare. The over-12
/// share (4.9% at the time of the ruling) is REPORTED by the harness, never
/// asserted — "rare" is Ben's judgement to make against a number, and no
/// threshold for it has been chosen.
inline constexpr std::size_t k_province_hard_cap_tiles = 20;

/// Hard-target floor (Ben: "3-12 hard target"). A region takes its first three
/// tiles WHATEVER THEY COST — the growth rule refusing to stop early, which is
/// not the same thing as a repair pass, and nothing is ever merged. It can
/// still be missed: an island of two tiles ships at two tiles, because the land
/// genuinely ran out. That is the case "don't reject tiny provinces" protects,
/// and the harness reports how often it happens rather than asserting it away.
inline constexpr std::size_t k_province_hard_min_tiles = 3;

/// Minimum hex spacing between HINTERLAND seeds (and between a hinterland seed
/// and a population centre): a chosen seed blocks every land tile within
/// `this - 1` steps of it, measured geodesically over land so a strait counts
/// as the distance it really is.
///
/// THIS, NOT THE BUDGET, IS WHAT SETS HINTERLAND PROVINCE SIZE. Seeds are all
/// chosen before any of them grows and then grow simultaneously, so a province
/// is as big as the terrain lets it reach before meeting its neighbours.
///
/// PINNED STRUCTURALLY, and confirmed by measurement. Seeds at minimum
/// separation d tile a plane in cells of area (sqrt(3)/2) * d^2 tiles:
/// d = 3 gives 7.79, the only integer spacing whose ideal cell lands inside the
/// 7-12 band at all (d = 2 gives 3.46, d = 4 gives 13.86). Measured mean over
/// the 6-seed sweep in `province_partition_harness` section D: 7.87 tiles
/// against the ideal 7.79 — near enough that the lattice argument is the real
/// explanation of the size, which is the point of pinning it structurally.
inline constexpr int k_province_seed_spacing = 3;

// ---------------------------------------------------------------------------
// The SEA size band (BL-516) — the same shape of rule, at the sea's scale
// ---------------------------------------------------------------------------
// Ben gave ONE number: "not larger than say 80 tiles". Everything else here is
// derived from it by the same lattice argument that pins the land spacing, so
// no second threshold is invented and none is chosen for feel.
//
// COASTAL WATER TAKES THE LAND BAND EXACTLY — 7-12 soft, 12 preferred, 20 hard
// cap — because Ben named that band for it ("3-12 size coastal tile
// provinces"). It therefore has no constants of its own; it reuses the ones
// above, hard cap included.

/// PREFERRED ceiling on an OPEN-OCEAN province, and the clamp GROWTH obeys —
/// the exact counterpart of `k_province_max_tiles` on land, and the number Ben
/// chose. Growth stops here outright.
///
/// THERE IS DELIBERATELY NO SEPARATE SEA HARD CAP. Land needed one because Ben
/// ruled a preference (12) and a bound (20) as two different numbers; for the
/// sea he named one number, and inventing a second would be exactly the
/// threshold-nobody-chose this file keeps refusing to invent. What the harness
/// asserts instead is the EXACT IDENTITY — every tile above 80 arrived by
/// singleton absorption, never by growth — which needs no second constant and
/// is a stronger claim than a headroom bound. Section W reports the spread.
inline constexpr std::size_t k_sea_province_max_tiles = 80;

/// Minimum hex spacing between OPEN-OCEAN seeds, over open ocean. THIS, not the
/// budget, is what sets open-ocean province size — the same relation the land
/// spacing has to the land band.
///
/// PINNED BY MEASUREMENT, 2026-08-21, against the cap above.
///
/// THE PIN RULE: the LARGEST spacing at which the cap is still a GUARD rather
/// than a CLAMP. "Guard, not clamp" is not a matter of taste here — it has an
/// instrument. Seeds at minimum separation d tile a plane in cells of area
/// (sqrt(3)/2) * d^2, so the lattice PREDICTS a mean size; where growth is
/// running into the ceiling instead of meeting its neighbours, the measured mean
/// falls away from that prediction and provinces pile up on the cap exactly.
/// The sweep (6 seeds, `province_partition_harness` section W) is in
/// docs/generation/TILE_GENERATION.md § Provinces over water; at d = 8 one
/// province in nine sits exactly on 80 and the mean has already broken from its
/// prediction, while at d = 7 the measured mean still MATCHES the lattice —
/// which is the evidence that terrain and spacing set the size, not the cap.
inline constexpr int k_sea_province_seed_spacing = 7;

/// Soft growth budget for an open-ocean province: the ideal lattice cell of the
/// spacing above ((sqrt(3)/2) * 49 = 42.4), rounded. STRUCTURAL, and the exact
/// analogue of the land band's `k_province_min_tiles` (7) sitting at its own
/// ideal cell (7.79) — past this, a region annexes only water no harder to reach
/// than the water it already holds, exactly as on land.
inline constexpr std::size_t k_sea_province_soft_target = 42;

// ---------------------------------------------------------------------------
// The cost model — what makes an edge a border (BL-515)
// ---------------------------------------------------------------------------
// Growth is a cost-weighted flood fill. The cost of stepping from one tile to
// its hex neighbour is what draws the border: an expensive edge is one a
// province stops at. Costs are INTEGERS so a tie is exact and breaks on a
// stable key, never on float noise or container order.
//
//   cost(a, b) = ( base + river + elevation + jitter )  [ / road divisor ]
//
// Every coefficient below is either STRUCTURAL (read off a domain the codebase
// already defines) or PINNED BY MEASUREMENT (BL-463's discipline), and says
// which it is. None is picked for feel.
// ---------------------------------------------------------------------------

/// Plain ground: flat, riverless, unroaded. The unit every other term is quoted
/// against — the river cost is four of these, the jitter is half of one — so
/// changing it rescales the whole model rather than shifting one term.
inline constexpr int k_province_edge_base_cost = 10;

/// Crossing a river edge (`tile_component::river_edges`, a per-side bitmask —
/// already the right shape for a border). Four times plain ground: a river is
/// the strongest single boundary signal the terrain offers, and this is the
/// anchor the elevation coefficient is then pinned AGAINST rather than a
/// number chosen on its own.
inline constexpr int k_province_river_edge_cost = 40;

/// Elevation cost per unit of normalised height difference (BL-517's retained
/// `tile_component::height`, [0, 1]). The term is `round(|dh| * this)`, so it
/// scales with the gradient exactly as ruled.
///
/// PINNED BY MEASUREMENT, 2026-08-21, against the river anchor above: a
/// gradient at the 90th percentile of adjacent-land steepness should cost the
/// same to cross as a river. `province_partition_harness` section C measures
/// the adjacent-land |dh| distribution over a seed sweep and prints the implied
/// coefficient on every run for exactly this purpose. The measurement, 653,910
/// adjacent land edges pooled over 6 seeds plus the default world:
///
///   mean |dh| 0.0270   p50 0.0186   p90 0.0586   p99 0.1406
///   implied k = k_province_river_edge_cost / p90 = 40 / 0.0586 = 682.7
///
/// The value below is that implied coefficient, rounded. What it makes true, in
/// the terms the ruling is written in: the steepest tenth of edges are borders
/// as strong as rivers, the median edge (0.0186 -> 13) costs a shade over plain
/// ground, and a cliff (p99, 0.1406 -> 96) is nearly impassable to growth.
///
/// It is measured on TERRAIN ALONE, so re-pinning it does not chase its own
/// tail: the |dh| distribution does not depend on the partition it feeds.
inline constexpr int k_province_height_cost = 683;

/// A ROAD BINDS (Ben, 2026-08-21). When BOTH tiles carry a road, the whole edge
/// cost is divided by this — the road does not annihilate the terrain (a bridge
/// over a gorge is still a gorge), it makes the link that much more binding.
///
/// PINNED BY MEASUREMENT, 2026-08-21.
///
/// THE PIN RULE: the SMALLEST divisor at which a road link is more binding than
/// plain ground EVEN WHERE IT CROSSES A RIVER — so "a road binds" holds against
/// the strongest boundary the terrain offers, which is what the ruling says,
/// while the road stays as weak as that requirement allows. Read off the
/// measured median edge: plain ground is base 10 + median elevation 13 + mean
/// jitter 2 = 25, and a roaded river edge is 10 + 40 + 13 + 2 = 65 before the
/// divide. 65 / d < 25 needs d >= 3; 4 is the next power of two and leaves
/// margin for the seeds whose relief runs above the median.
///
/// THE MEASURED EFFECT, so the rule is not taken on trust. The instrument is
/// the share of roaded adjacent-land edges that end up INTERNAL to a province
/// rather than on its border, against the same share for plain edges;
/// `province_partition_harness` C2 prints the shipped row on every run, and
/// changing this constant and re-running produces the rest of the table
/// (default world, k_province_height_cost = 683):
///
///   divisor   roaded internal   plain internal   lift      world mean size
///      1          56.85%            56.20%       +0.65pp        8.00
///      2          67.96%            55.85%      +12.11pp        7.94
///      4          74.81%            55.34%      +19.47pp        7.87
///      8          78.89%            55.19%      +23.70pp        7.86
///
/// At 1 the road does not bind at all (+0.65pp is noise) — which is the honest
/// evidence that the divisor, not the base cost, is what carries the ruling.
/// The lift is concave with NO sharp knee, so the table alone would not pick a
/// value; the pin rule above is what picks it, and the table is what confirms
/// the choice buys a real effect.
inline constexpr int k_province_road_bind_divisor = 4;

/// Seeded jitter added to every edge cost, as `fold(seed, lo, hi) % this` — so
/// 0..4 on a base of 10.
///
/// STRUCTURAL: half the base cost. Enough to break a tie between two equally
/// plausible borders, so borders are irregular rather than perfectly
/// cost-optimal (organic is the point of the item); never enough to outweigh a
/// river or a real slope. It is also what keeps the partition SEED load-bearing
/// now that the fill is otherwise a pure function of terrain.
///
/// No RNG stream is consumed: the draw is a stateless fold, symmetric in the
/// two tile ids, so the cost of an edge does not depend on which side it is
/// read from.
inline constexpr int k_province_edge_jitter = 5;

/// What pass 3 (singleton absorption) did, so the pass is measurable rather
/// than merely asserted. Filled on request by `build_province_partition`;
/// gathering it changes nothing about the partition that is built.
struct province_absorption_stats
{
    /// Size-1 regions present when pass 3 started — the population of the
    /// problem Ben's ruling names.
    int singletons_before = 0;

    /// How many of those were absorbed into a neighbour.
    int absorbed = 0;

    /// How many remain because they have NO LAND NEIGHBOUR: true islands, kept
    /// by ruling. `singletons_before == absorbed + true_islands` always.
    int true_islands = 0;

    /// Fixpoint iterations run (>= 1 whenever any singleton existed).
    int passes = 0;

    /// Absorptions that pushed a survivor PAST the PREFERRED ceiling
    /// `k_province_max_tiles` (or `k_sea_province_max_tiles` on the open ocean).
    /// Reported, never prevented — permitted by ruling (Ben, 2026-08-21) up to
    /// `k_province_hard_cap_tiles`, which IS asserted.
    /// See the pass-3 note on build_province_partition.
    int over_ceiling_created = 0;

    /// The same count SPLIT BY DOMAIN, indexed by `province_kind` (BL-516).
    /// Sums to `over_ceiling_created`.
    ///
    /// It exists so each domain's ceiling can be asserted as the exact identity
    /// "every tile over the ceiling arrived by absorption" rather than as a
    /// weaker inequality that a breach in one domain could hide behind another's
    /// headroom. Ben's 80-tile sea ceiling is the one number he chose for the
    /// water, and it carries no separate hard cap, so it needs a check that
    /// cannot be satisfied by accident.
    int over_ceiling_by_domain[3] = { 0, 0, 0 };
};

/// One province — a contiguous run of land tiles on a single body.
/// Which DOMAIN a province was drawn over (BL-516). Exclusive: a province holds
/// tiles of exactly one domain, because the three substrate predicates are
/// exclusive and each domain is partitioned on its own.
///
/// NOT SERIALISED, and deliberately not a field on `province`: it is DERIVED
/// from the substrate of any member tile, so it cannot desynchronise from the
/// tiles it describes, and the flat-binary province section keeps its v1 layout
/// (no version bump, so every stream written before this item still loads).
enum class province_kind : uint8_t
{
    land          = 0, ///< Dry ground. The only domain anything can currently stand in.
    coastal_water = 1, ///< The shoreline ring and the lakes. Land's band, hard cap included.
    open_ocean    = 2, ///< Open sea. `k_sea_province_*` band, growth capped at 80 tiles.
};

struct province
{
    /// Stable derived identity: THE LOWEST ENTITY ID AMONG `tiles`.
    ///
    /// DERIVED, NEVER ALLOCATED (Ben, 2026-08-21, revising the allocated-id
    /// answer). This dissolves the determinism hazard rather than guarding
    /// against it — an id that is not handed out cannot be handed out in the
    /// wrong order. Nothing new is serialised: no allocator state, no next-id
    /// counter, no save-format surface. And it is unique by construction, since
    /// every tile belongs to exactly one province.
    ///
    /// IT IS NEVER 0, because `null_entity` is 0 and no tile carries that id.
    /// That makes `province_of`'s 0-for-absent return structurally safe rather
    /// than merely unlikely — it was a real-id collision under the old block
    /// layout, and is not one now.
    ///
    /// The cost it carries: a derived id CHANGES when the province changes
    /// shape. Acceptable because borders move during GENERATION ONLY — ids
    /// churn while the Era -1 sim redraws them (BL-518) and are frozen before
    /// anything outside generation can hold one. A battle record, a march order
    /// or a save only ever sees the settled id.
    ///
    /// Ascending id order is therefore ascending lowest-member-tile order. It
    /// is a stable spatial walk, but it is NO LONGER guaranteed body-major:
    /// that was a property of the old body-rank bit field, and nothing depends
    /// on it (`province::body` names the body explicitly).
    uint32_t id = 0;

    /// The body every tile in this province sits on. A LAND province never
    /// spans bodies, and never spans water. (BL-516 landed the sea provinces
    /// that claim anticipated: it is now NARROWED to land provinces rather than
    /// gone — NR-428 — and the matching claim for a sea province is that it
    /// never spans bodies and never touches land.)
    entity_id body = null_entity;

    /// Member land tiles, ASCENDING entity id. Non-empty for every province in
    /// a built partition — so `tiles.front()` IS `id`.
    std::vector<entity_id> tiles;
};

/// The whole world's partition — every body's provinces in one ascending-id run.
struct province_partition
{
    /// The seed `build_province_partition` was called with. Stored so the
    /// partition can be recomputed and checked against itself (the replay
    /// guard) without the caller having to remember the world descriptor.
    uint32_t seed = 0;

    /// Every province, ASCENDING `province::id`. This is the iteration contract.
    std::vector<province> provinces;

    /// Land tile -> owning province id. Derived from `provinces` (rebuilt on
    /// read, never serialised separately) — every land tile appears exactly once.
    std::unordered_map<entity_id, uint32_t> tile_province;

    /// Owning province id for @p tile, or 0 when the tile is off-body or
    /// otherwise unpartitioned. WATER IS NO LONGER ONE OF THOSE CASES (BL-516):
    /// a sea tile has a province like any other, and a caller that wants land
    /// must ask `province_kind_of`, not this function's 0.
    /// Since BL-515 derived ids from the lowest member
    /// tile, 0 is STRUCTURALLY unreachable as a real id (no tile carries
    /// `null_entity`), so this accessor's 0 is an unambiguous "no province".
    uint32_t province_of(entity_id tile) const
    {
        const auto it = tile_province.find(tile);
        return (it != tile_province.end()) ? it->second : 0u;
    }

    /// The province with id @p id, or nullptr. Binary search over the ascending
    /// `provinces` vector.
    const province* find(uint32_t id) const;
};

/// The domain @p pr was drawn over, read off the substrate of its first tile
/// (BL-516). O(1), and exact: a province holds one domain by construction.
///
/// A province with no tiles, or whose first tile is missing from @p w, reads as
/// `land` — the conservative answer, since land is the domain every pre-BL-516
/// caller assumed and the one that gates nothing away.
province_kind province_kind_of(const world& w, const province& pr);

/// The domain of the province with id @p id, or `land` when there is no such
/// province (the same conservative default as above).
province_kind province_kind_of(const world& w, uint32_t id);

/// Build @p w's province partition from its tiles, REPLACING `w.provinces`.
///
/// Pure in everything but its inputs: the result is a function of (@p seed, each
/// body's grid dimensions, its land mask, its tiles' height / river edges / road
/// level, and its population centres) alone. NO RNG STREAM IS CONSUMED — every
/// draw is a stateless fold from @p seed, the campaign_battle identity idiom, so
/// the partition can never perturb another generation pass's draws no matter
/// where it is called.
///
/// Two passes (BL-515's settled algorithm):
///
///   1. SETTLEMENT GROWTH. Every population centre on the body is a seed, in
///      ascending tile id, with a growth budget scaled by its centre scale
///      (1 -> 7 tiles .. 5 -> 12). All seeds grow SIMULTANEOUSLY as one
///      cost-weighted multi-source fill, so neighbouring centres meet on the
///      terrain between them rather than in the order they were listed. Every
///      region takes its first `k_province_hard_min_tiles` whatever they cost,
///      grows freely to its budget, then annexes only ground no harder to reach
///      than what it already holds, and stops at `k_province_max_tiles`
///      outright.
///
///   2. HINTERLAND. Land no centre reached is partitioned under the SAME cost
///      rules. Seeds are chosen from the LEAST-ACCESSIBLE unclaimed tile
///      onward — the tile whose six sides are the most expensive to cross,
///      coast counted as a border at least as strong as a river — each one
///      blocking `k_province_seed_spacing` around itself, and then they ALL
///      grow simultaneously. Choosing the whole seed set before any of it grows
///      is what makes a hinterland a shape the terrain chose; growing them one
///      at a time was measured to produce a spike at exactly the budget and a
///      flood of one-tile slivers wedged between finished regions.
///
///      Land the spaced fill still could not reach — pockets enclosed by
///      regions that hit the ceiling — is seeded in the same fixed order
///      afterwards. That is where a genuinely tiny province comes from, and it
///      is KEPT.
///
///   3. SINGLETON ABSORPTION (Ben, 2026-08-21, a NARROW retraction of "don't
///      reject tiny provinces", made on seeing the organic borders rendered:
///      "We can add a pass to capture all the 1 tile provinces. I suppose I was
///      wrong when I said 'don't reject'."). A province of EXACTLY ONE tile is
///      absorbed into the neighbour reached by the CHEAPEST edge under the same
///      cost model, so absorption follows the same terrain logic as growth.
///
///      THE SCOPE IS ONE TILE, NOT THE FLOOR. This is not the merge-to-floor
///      pass BL-515 deliberately removed — that pass produced a de-facto clamp
///      and is the thing the organic redesign exists to escape. A two-tile
///      province is still kept; only the hex-with-a-border-round-it is not.
///
///      Ties break on the candidate's province id (its lowest member tile),
///      never on container order. It runs to a FIXPOINT in ascending
///      singleton-tile order, because absorbing one can expose another.
///
///      A size-1 province with NO LAND NEIGHBOUR is a TRUE ISLAND and cannot be
///      absorbed. It is KEPT, and `province_absorption_stats` reports how many
///      remain — nothing reaches across water to place them.
///
///      IT CAN EXCEED THE PREFERRED CEILING, BY RULING. A survivor already
///      holding `k_province_max_tiles` that absorbs a singleton ships at 13.
///      That is reported (`over_ceiling_created`), never silently clamped:
///      clamping would mean choosing a costlier neighbour, which is exactly the
///      "boundaries win" logic the cheapest-edge rule exists to express.
///
///      Ben ruled on this directly (2026-08-21, NR-438): "We prefer up to 12
///      tiles, but up to 20 is permitted in rare cases." So 12 is the PREFERENCE
///      and `k_province_hard_cap_tiles` (20) is the bound — and the cheapest-edge
///      rule survives intact, which is what the ruling was chosen to protect. The
///      counterfactual that was costed at the time (prefer a neighbour WITH ROOM,
///      falling back to a full one) was measured, reported and then DELETED under
///      this ruling: it bought a tighter distribution by deliberately choosing a
///      costlier neighbour, and the ceiling breach was its only justification.
///
/// There is still NO merge-to-floor repair pass, by ruling: nothing else is
/// merged away to satisfy a floor, and a province that ran out of land at two
/// or three tiles ships at two or three tiles.
///
/// Columns wrap east-west (the cylinder every other body-grid consumer uses);
/// rows do not.
///
/// DETERMINISM. `world::bodies` and `world::population_centre_tile` are both
/// UNORDERED maps whose iteration order is a container-layout accident. Both
/// walks are sorted explicitly here; the fill's priority queue breaks every tie
/// on (cost, seed tile, tile), a total order over unique keys. No container
/// order reaches any decision.
///
/// @param w     World whose tiles are read and whose `provinces` is replaced.
/// @param seed  Partition seed. Callers derive this from the world seed with
///              their own XOR offset so the stream stays uncorrelated.
/// @param stats Optional: filled with the singleton-absorption tally. Purely an
///              instrument — it does not change what is built, so passing it or
///              not passing it produces byte-identical partitions.
void build_province_partition(world& w, uint32_t seed, province_absorption_stats* stats = nullptr);

/// The cost of stepping between two hex-adjacent LAND tiles, exposed so the
/// harness can measure the cost model rather than re-implement it (a border
/// check that re-derives its own costs proves nothing about the partition).
///
/// Symmetric in @p a and @p b: `edge = edge` however it is read.
///
/// @param w    World the tiles belong to.
/// @param seed The partition seed, for the jitter term.
/// @param a    One tile.
/// @param b    Its hex neighbour.
/// @param side The side index (0-5, odd-r) leading from @p a to @p b.
/// @return     The integer edge cost, always >= 1.
int province_edge_cost(const world& w, uint32_t seed, entity_id a, entity_id b, int side);


// ---------------------------------------------------------------------------
// Province building ceiling (BL-513) — "how much can this land sustain?"
// ---------------------------------------------------------------------------
// TWO LIMITS COEXIST, and they answer different questions. The per-tile stack
// cap (placement_rules::stack_capacity) asks "may this DEPOSIT support another
// site?" and answers from richness. This one asks "how much can this LAND
// sustain?" and answers from Ben's four inputs (2026-08-21):
//
//     area + infrastructure + habitability + population
//
// It is TYPE-AGNOSTIC by ruling — it bounds the TOTAL number of buildings
// standing in the province, and the mix inside it is the player's business:
// "it does not matter if you can build 60 buildings, whether they are 5 of one
// type and 5 of another, or 10 of one type."
//
// THE SHAPE, and where each of the four inputs enters:
//
//   sustain_units(province) = pop_factor * SUM over land tiles of
//                                 habitability_t * (1 + road_level_t / 3)
//
//   ceiling(province)       = max(1, round(k_province_buildings_per_sustain_unit
//                                          * sustain_units))
//
//   * AREA is the number of terms in the sum — a bigger province sustains more
//     simply by being bigger.
//   * HABITABILITY is each tile's weight. Land nobody can live on sustains
//     nothing, which is the honest reading of the input.
//   * INFRASTRUCTURE is the per-tile road multiplier, spanning exactly [1, 2]
//     across the road ladder's OWN defined domain (BL-172: 0 = none .. 3 =
//     Highway). The span is structural, not tuned: an unroaded tile gets its
//     bare habitability, a Highway tile gets twice it.
//   * POPULATION is the province-level multiplier, 1 + SUM(centre scale) / 5,
//     again over the scale's own defined domain (1 = village .. 5 = metropolis,
//     population_centre_component). A province with a metropolis sustains twice
//     what the same empty land would.
//
// Every band above is read off a domain the codebase already defines. That
// leaves exactly ONE free scalar — `k_province_buildings_per_sustain_unit` —
// and it is PINNED BY MEASUREMENT, never picked; see its own comment.
//
// COMPUTED ON DEMAND, NEVER CACHED. Roads are built during play, so this
// ceiling MOVES during play — the first placement bound in the game that is not
// fixed at generation. Whether that is intended is flagged to Ben (NR-406) and
// not yet answered; computing on demand keeps either answer cheap and adds NO
// persistent field, so the flat-binary save/load path is untouched by this item.
// ---------------------------------------------------------------------------

/// Top of the road ladder (BL-172: 1 = Track, 2 = Road, 3 = Highway). Structural:
/// it is the ladder's own maximum, so the infrastructure multiplier below spans
/// exactly [1, 2] over the domain road_level is defined on.
inline constexpr float k_road_ladder_max = 3.0f;

/// Top of the population scale (population_centre_component: 1 = village ..
/// 5 = metropolis). Structural for the same reason as k_road_ladder_max.
inline constexpr float k_population_scale_max = 5.0f;

/// Buildings per sustain unit — the heuristic's ONLY free coefficient.
///
/// PINNED BY MEASUREMENT, 2026-08-21, following BL-463's discipline on the
/// settlement divisor.
///
/// THE ANCHOR. It is set so the new ceiling's WORLD TOTAL matches the capacity
/// the world already grants under the existing pooled per-tile cap. That makes
/// the province ceiling a REDISTRIBUTION of capacity the world already has onto
/// the four sustaining inputs — not a new tighter or looser regime laid over the
/// top. The pooled per-tile cap is the only measured statement of "how much this
/// world already permits" that exists, so it is the only honest anchor available;
/// anything else would be a number chosen for feel.
///
/// THE MEASUREMENT. `province_capacity_probe`, 8 seeds, post-background-firms.
/// SUM(pooled per-tile cap) / SUM(sustain units), per seed:
///
///   seed 0  12.4733     seed 4  12.7495
///   seed 1  11.4694     seed 5  11.5268
///   seed 2  13.4241     seed 6  12.2953
///   seed 3  15.2588     seed 7  13.1463
///
///   aggregate 12.6468, spread 11.4694 .. 15.2588 (28.36% of the mean)
///
/// The value below is that aggregate. THE SPREAD IS WIDE AND IS REPORTED RATHER
/// THAN HIDDEN: seeds differ in how much of their land is habitable and how the
/// composition cap table falls across it, so the ratio genuinely moves by seed.
/// Pinning on the aggregate means a seed at the low end (1, 5) gets a ceiling
/// slightly above the capacity it already has and a seed at the high end (3)
/// slightly below — neither by enough to bind, at 0.65% utilisation.
///
/// Re-pin by running `province_capacity_probe`, which prints the ratio and its
/// spread on every run for exactly this purpose.
inline constexpr float k_province_buildings_per_sustain_unit = 12.6468f;

/// The heuristic's working, kept whole so the probe can report each term rather
/// than only the answer. A ceiling nobody can decompose is a picked number with
/// extra steps.
struct province_sustain
{
    int   land_tiles          = 0;    ///< AREA — land tiles in the province.
    float habitability_area   = 0.0f; ///< SUM of habitability over those tiles.
    float infrastructure_gain = 0.0f; ///< SUM of habitability * road_level / 3.
    float population_factor   = 1.0f; ///< 1 + SUM(centre scale) / 5.
    float units               = 0.0f; ///< (habitability_area + infrastructure_gain) * population_factor.
    int   ceiling             = 0;    ///< max(1, round(k * units)) on land; 0 for a sea province.
};

/// Measure @p pr's sustain terms and ceiling against @p w.
///
/// Deterministic: the tile sum walks `province::tiles`, which is ascending
/// entity id by contract, and the population term is gathered through an ordered
/// index so no unordered_map iteration order can reach the arithmetic.
///
/// BL-516: a province of WATER measures as ALL ZEROES, ceiling included. It does
/// not fall through the one-building floor — nothing stands on water, and the
/// floor is a claim about habitable land that a sea province does not make.
///
/// @param w  Read-only world state (tiles, population centres).
/// @param pr The province to measure.
/// @return   Every term plus the resulting ceiling.
province_sustain measure_province_sustain(const world& w, const province& pr);

/// The total-buildings ceiling for the province with id @p province_id, or -1
/// when there is no such province (id 0, an unpartitioned tile, a world whose
/// partition has not been built). **-1 means UNKNOWN, never "no room"** — the
/// same contract `tile_reach_cost` uses, so a rule is skipped rather than
/// guessed at.
int province_building_ceiling(const world& w, uint32_t province_id);

/// Buildings of EVERY type standing in the province with id @p province_id —
/// the total the ceiling bounds. Type-agnostic by ruling; extraction sites are
/// counted here too, and are bounded independently by their deposit's richness.
int province_buildings_standing(const world& w, uint32_t province_id);

// ---------------------------------------------------------------------------
// The province holder (BL-569, province holder) — a military fact, not a
// political one
// ---------------------------------------------------------------------------
// `world::province_holder` tracks who currently holds a province ACROSS
// battles, which `battle_dispatch::field_held_by` (erased with the battle it
// concluded) cannot: seeded at generation from `tile_to_nation`'s plurality,
// moved by a decisive battle's `field_held_by` (`run_battles`,
// battle_system.cpp), left untouched by a stalemate. `no_entity` means "no
// recorded holder" — a sea province, or a land province no nation's tiles
// reached a plurality in.
//
// Deliberately NARROW: `tile_to_nation` (the territorial map) does NOT follow
// the holder here — the political map changing hands is unowned work. A held
// province is a fact `condition_subject::province_held` (BL-570) reads.
//
// TIES BREAK ON ASCENDING NATION ENTITY ID, the codebase's standing
// convention (province.hpp's own id derivation, the terrain argmax in
// MILITARY.md, etc.) — never on container order.
// ---------------------------------------------------------------------------

/// Seed `w.province_holder` from `w.tile_to_nation`'s plurality over each land
/// province's tiles, called from `build_province_partition`'s CALLER once the
/// partition and every tile's nation assignment both exist.
///
/// One entity_id per province, POSITIONALLY ALIGNED with
/// `w.provinces.provinces` (ascending `province::id` order) — NOT indexed by
/// the raw id, which is a derived tile id and not compact. `no_entity` for a
/// non-land province (coastal water, open ocean) and for a land province none
/// of whose tiles carry a `tile_to_nation` entry.
///
/// DETERMINISTIC: `province::tiles` is already ascending entity id (the
/// partition's own contract), and the tally is walked in that order into an
/// ordered `std::map` keyed on nation id, so the plurality winner — and its
/// ascending-id tie-break — does not depend on any unordered container's
/// layout.
///
/// @param w World whose `provinces` partition is already built and whose
///          `tile_to_nation` is already populated; `province_holder` is
///          replaced.
void seed_province_holders(world& w);

/// The current holder of the province with id @p province_id, or `null_entity`
/// if there is no such province or no holder is recorded. O(log n): binary
/// search for the province (mirroring `province_partition::find`), then a
/// positional lookup into `w.province_holder`.
entity_id province_holder_for(const world& w, uint32_t province_id);

// ---------------------------------------------------------------------------
// Serialisation — the trailing section of the world's flat-binary stream
// ---------------------------------------------------------------------------
// The history log (history_log.{hpp,cpp}) is the project's flat-binary seam.
// The province section is APPENDED AFTER its last record and nowhere else:
// no existing record moves, so a stream written before BL-466 is a byte-exact
// PREFIX of one written after it, and reads back cleanly (the reader treats a
// clean end-of-stream as "no province section" rather than as corruption).
// tools/verify/province_partition_harness.cpp asserts all three properties.
// ---------------------------------------------------------------------------

/// Leading magic identifying a province section: the bytes 'I','O','P','V'.
inline constexpr uint32_t province_section_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('P') << 16) | (uint32_t('V') << 24);

/// Format version. Bump on any layout change to the section; the reader rejects
/// a mismatch rather than reinterpreting differently-shaped records.
inline constexpr uint32_t province_section_version = 1;

/// Sanity ceiling on the declared province count — guards an eager reserve
/// against a corrupt or malicious length prefix.
inline constexpr uint32_t province_section_max_provinces = 1u << 24;

/// Sanity ceiling on one province's declared tile count. A built province is a
/// handful of tiles — `k_province_max_tiles` (12) is the PREFERRED size and the
/// clamp growth obeys, `k_province_hard_cap_tiles` (20) is the absolute bound,
/// and the floor is whatever land was there — so this is orders of magnitude
/// clear of all three. It stays generous rather than tight to either constant so
/// a future algorithm change is a format-compatible one.
inline constexpr uint32_t province_section_max_tiles = 1u << 16;

/// Append @p p to @p out as: magic, version, seed, province count, then per
/// province (id, body, tile count, tiles). Binary, native byte order — the
/// project's settled flat-binary convention. `tile_province` is NOT written;
/// it is derived, and the reader rebuilds it.
///
/// @param p   Partition to write.
/// @param out Binary output stream, positioned at the append point.
void write_province_section(const province_partition& p, std::ostream& out);

/// Read a section written by write_province_section into @p out.
///
/// A CLEAN END OF STREAM IS SUCCESS: a stream written before this item simply
/// ends after its last history record, and must still load. In that case @p out
/// is left empty and the function returns true. Anything else that is not a
/// well-formed section — a wrong magic, a mismatched version, a truncated
/// record, a count past the ceilings above, ids out of ascending order — is a
/// rejection (returns false), leaving @p out unspecified for the caller to
/// discard.
///
/// @param out Partition filled on success (`tile_province` rebuilt from the
///            written provinces).
/// @param in  Binary input stream, positioned at the section start.
/// @return    True on a well-formed section OR a clean absent one; false on a
///            malformed section.
bool read_province_section(province_partition& out, std::istream& in);
