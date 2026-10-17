#pragma once

#include "tongue.hpp"
#include "world.hpp"

#include <cstdint>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Procedural nation generation
//
// Deterministic, five-pass political-layer generation for a single body.
// Runs over an already-generated tile map; reads tile compositions from `w`
// and writes nation_component entries plus tile ownership back into `w`.
// See docs/generation/NATION_GENERATION.md for the design authority and
// per-pass rules.
// ---------------------------------------------------------------------------

/// Tunable parameters for a single nation generation run.
///
/// There is deliberately **no nation-count knob**: the number of nations is a
/// *consequence* of the body's geography, not a target. Seeds scale with the
/// habitable land area (`land_tiles_per_seed`) and the merge pass absorbs every
/// nation that fails to clear a minimum viable territory (`min_nation_tiles`).
/// A bigger, drier, or less fragmented homeworld therefore ends up with a
/// different political map than a small, ocean-heavy one.
struct nation_params
{
    /// **Seed density** — one Pass-1 seed per this many habitable (non-ocean)
    /// tiles on the body. The pre-merge seed count is `land_tiles / this`
    /// (at least one on any body with land), so the seed budget grows and
    /// shrinks with the landmass. Lower = more, tighter-packed seeds. A seed is
    /// only a *candidate* core: most are absorbed by the merge pass below, so
    /// this is deliberately far denser than the expected surviving count.
    int land_tiles_per_seed = 80;

    /// Minimum grid-distance (Chebyshev, column-wrapped) between any two nation
    /// seeds placed in Pass 1. Raise to spread nations further apart. Acts as a
    /// hard ceiling on how many seeds actually fit, whatever the density asks for.
    int min_seed_separation = 6;

    /// **Minimum viable territory**, in tiles. Pass 2c absorbs any nation holding
    /// fewer tiles than this into its largest adjacent neighbour — smallest first
    /// — and stops once every survivor clears the floor. The final nation count
    /// falls out of that, rather than being counted down to a target. 0 disables
    /// the merge pass entirely (every placed seed keeps its Voronoi cell).
    ///
    /// Set equal to `land_tiles_per_seed` by default, which reads as: a nation
    /// must end up holding at least the land its own seed was budgeted. The
    /// resulting count is ladder-derived: the history ladder (BL-221) modulates
    /// seeding, so the default Kepler seed lands well above the pre-ladder figure
    /// (43 nations at the current default seed). Halve the habitable area and
    /// the count still roughly halves with it.
    int min_nation_tiles = 80;

    /// **Explicit seed tiles** (raster indices, `row * gw + col`) — when this is
    /// non-empty Pass 1's random placement is SKIPPED and these are used verbatim,
    /// in order, as the nation cores.
    ///
    /// This is the whole of BL-218's "seeding changes, expansion does not": the
    /// settlement pass (`src/world/settlement.{hpp,cpp}`) places regions on
    /// ground people would actually have settled and hands the anchors over here,
    /// so the BFS/growth machinery BL-053 tuned is reused untouched and only its
    /// starting points become historical. Empty (the default) preserves the
    /// pre-BL-218 random placement bit-for-bit for every body without a
    /// settlement pass.
    ///
    /// Entries outside the grid, on ocean, or duplicated are dropped by Pass 1;
    /// `min_seed_separation` is NOT re-applied to them (the settlement pass owns
    /// its own separation rule and re-filtering here would silently discard
    /// history).
    std::vector<int> seed_tiles;

    /// Parallel to `seed_tiles`: the tongue of the culture that settled each
    /// anchor (BL-290). Pass 5 names a nation in the speech of the region it
    /// grew from, so a realm, its gods and its cities share one sound system as
    /// a consequence of the generation chain rather than by coincidence. Short,
    /// empty, or unusable entries fall back to a tongue rolled in Pass 5.
    std::vector<tongue> seed_tongues;

    /// Parallel to `seed_tiles`: the id of the POLITY that held each anchored
    /// region at the epoch, or -1 for ground no polity ended up holding
    /// (`settlement_seed_polities`). BL-769.
    ///
    /// THIS IS THE PHASE 4 / PHASE 5 BOUNDARY MOVING. Without it Pass 1 grew an
    /// independent realm out of every region anchor and the history's own
    /// political map — which `run_history_sim` spent the whole run drawing — was
    /// discarded and re-invented from the anchors alone. With it, Pass 2d folds
    /// the seeds of one polity into ONE nation, so the campaign inherits the
    /// empires the history actually produced.
    ///
    /// The carve is UNTOUCHED, deliberately, and for the same reason BL-218
    /// left it alone: where exactly the boundary between two of a polity's own
    /// regions falls is a geometric question the history never answered. What
    /// changes is which cells end up under one flag.
    ///
    /// Empty (the default) keeps the pre-BL-769 behaviour bit-for-bit: no fold,
    /// one candidate nation per seed, as for any body with no settlement pass.
    std::vector<int> seed_polities;

    /// BL-769 — CITY STATES SURVIVE THE SIZE FLOOR.
    ///
    /// A polity that reached the epoch holding a single region and standing a
    /// population centre on it is a city state, and Pass 2c would otherwise
    /// absorb it into whichever neighbour grew fattest — losing exactly the
    /// detail Ben's point 5 asks the History phase to keep ("it is fine to
    /// consider city states as population centres"). Such a nation is exempt
    /// from being absorbed; it is not exempt from the floor in any other sense,
    /// and it never absorbs anybody.
    ///
    /// False disables the exemption and every undersized realm merges as before.
    bool keep_city_states = true;

    /// BL-975 — THE LAST CLOSE'S TREASURIES CROSS THE FOLD.
    ///
    /// Indexed by POLITY ID (the same ids `seed_polities` carries): the
    /// treasury each polity held at the last simulated span's close — the
    /// Exploration span's 1660 one, or the Industrialisation span's 1960 one when
    /// that span ran (BL-1053) — in the sim's material currency, summed over
    /// every region flying its flag (`region::treasury` sits on the ground,
    /// and the flag over the ground at that close owns the chest —
    /// settlement.hpp's own transfer-by-ownership rule).
    /// Pass 2d credits each polity's whole sum ONCE, to the seed it folds to,
    /// and the credit follows that seed through the size-floor merge, so a
    /// realm absorbed for being small hands its chest to the realm that
    /// absorbed it. Converted by `treasury_credit_per_mille` below.
    ///
    /// Empty (the default) credits nothing: every nation starts on
    /// `treasury_floor`, exactly as before this item, and a body with no
    /// settlement pass is unchanged bit-for-bit.
    std::vector<int64_t> polity_treasuries;

    /// BL-975 — THE ONE STATED CONVERSION, sim material currency -> campaign
    /// credits, expressed per mille: credits = material x per_mille / 1000.
    /// Named in docs/generation/NATION_GENERATION.md § Pass 7 and nowhere
    /// else; the doc owns the figure and the reasoning, this field only
    /// carries it.
    float treasury_credit_per_mille = 0.01f;

    /// BL-975 — where a nation with NO folded polity starts (a Voronoi cell
    /// the history never held, or a body with no settlement pass): the same
    /// zero NATIONS.md names for a treasury nothing has credited.
    float treasury_floor = 0.0f;

    /// BL-1089 — THE REALMS' NAMES CROSS THE FOLD (NATION_GENERATION.md § Pass
    /// 5; Ben, 2026-09-24). Indexed by POLITY ID, the same ids `seed_polities`
    /// carries: `polity::name` as the last span's close left it, coined once
    /// at the realm's founding and never re-coined. Pass 5 gives a nation the
    /// name of its fold representative's polity VERBATIM, following that
    /// representative through the size-floor merge (the absorber keeps its
    /// name — merge rule A), and coins only for ownerless ground. An empty
    /// entry (a tongue that could not coin) falls back to coining, as every
    /// nation did before this item. Empty (the default) keeps the pre-BL-1089
    /// naming bit-for-bit.
    std::vector<std::string> polity_names;

    /// BL-1278 — THE FLOOR RAN IN THE HISTORY (NATION_GENERATION.md § Pass
    /// 2c; Ben, 2026-10-11). True when the Industrialisation span's closing
    /// rounds (`closing_rounds.hpp`) already absorbed every realm below
    /// `min_nation_tiles` as events of the span and reconciled the total:
    /// Pass 2c then does not run, so the fold's count is the count the
    /// closing rounds planned. False (the default, and every world with no
    /// history) keeps the post-pass exactly as it was.
    bool floor_in_history = false;

    /// BL-1310 — THE CESSIONS' CARVE (NATION_GENERATION.md § Pass 2e). The
    /// closing rounds cede exclaves at the history's grain: a province moves
    /// realm by moving its region, which `seed_polities` already carries. Ground
    /// a region move cannot express — an islet of one carve cell ceded to
    /// another nation, or a province ceded to ground no realm held — is a
    /// RELABEL of the carve: per pair, a land tile (raster index) and the
    /// anchor tile of the region whose cell now holds it. Applied to the carve
    /// before the polity fold, after which the water re-derives from the
    /// shore. Empty (the default, and every world with no history) changes
    /// nothing.
    std::vector<std::pair<int, int>> cell_overrides;

    /// BL-1310 — each realm's CORE as the closing rounds read it, indexed by
    /// polity id: the anchor tile of its capital (its seat at the close) where
    /// that region still has a carve cell, else of the lowest region it holds;
    /// -1 for a polity with none. § Pass 2e's "group holding its capital" is
    /// the realm's capital, so the tail's pass anchors a nation's territory
    /// here, and Pass 5 seats the nation on the lowest seed INSIDE that
    /// territory (the representative's own tile wherever it lies there, which
    /// is nearly always). Empty (no history, or no closing rounds): the core
    /// is the representative seed's tile, as Pass 5 has always read it.
    std::vector<int> polity_core_tiles;
};

/// BL-1310 — THE STRAIT (NATION_GENERATION.md § Pass 2e; Ben, 2026-10-16): two
/// of a nation's land groups are joined when a path between them crosses at
/// most this many water tiles and no land another nation holds. ≈ 380 km at
/// `body_km_per_tile`.
inline constexpr int strait_tiles = 3;

/// BL-1310 — ONE CESSION under the strait rule (§ Pass 2e), as the tile-level
/// rule finds it. Owner KEYS are the caller's (nation indices in the tail,
/// fold groups in the closing rounds).
struct exclave_cession
{
    int from = -1;          ///< the key that held the exclave
    int to   = -1;          ///< the key it is ceded to
    std::vector<int> tiles; ///< the exclave's land tiles, ascending
    /// The receiver's land tile that decided it: the lowest-index tile of the
    /// receiver touching the exclave (a land-bordered exclave), or the nearest
    /// across the water (an island).
    int contact = -1;
    /// 0 = land-bordered (most shared edges), 1 = an island ceded to the
    /// nearest nation's ground in strait reach, 2 = an islet of a remote
    /// archipelago consolidated to the owner of its largest island.
    int kind = 0;
};

/// BL-1310 — find the NEXT cession the strait rule asks for, or return false
/// when every land group of every key is one territory with its core or the
/// remote island. PURE: ascending walks, fixed tie-breaks, no draw.
///
/// @param owner     per raster tile the owner key on LAND (>= 0), -1 elsewhere
///                  (water must be -1: the rule reads land groups).
/// @param water     per raster tile 1 where the ground is water (a strait leg
///                  may cross it), 0 for land and for no tile at all.
/// @param core_tile per key: the tile whose group is the key's core (its
///                  capital); -1 or a tile it does not hold -> its largest group.
/// @param order     per key: the tie-break rank (lower wins) — the nation index.
///
/// Order of business: land-bordered exclaves first (lowest tile first; to the
/// key sharing the most cardinal land edges, ties the lower rank then the lower
/// tile), then islands within strait reach of another key's TERRITORY (to the
/// nearest by water tiles, same ties), then islets of a remote archipelago
/// whose islands several keys hold (to the owner of its largest island). An
/// island farther than `strait_tiles` from every other key's ground is the
/// remote island and is never ceded.
bool next_exclave_cession(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                          const std::vector<int>& core_tile, const std::vector<int>& order,
                          exclave_cession& out);

/// BL-1310 — the floor's island case (§ Pass 2c, § Pass 2e): the key whose land
/// is fewest water tiles from @p from's land, among keys @p eligible admits
/// (ties: lower @p order, then lower tile). -1 when the water reaches none.
/// @p owner and @p water as for `next_exclave_cession`.
int nearest_key_across_water(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                             int from, const std::vector<char>& eligible, const std::vector<int>& order);

/// BL-1310 — is every land group of @p key joined to its core (the group
/// holding @p core_tile) by land or straits, save the remote island? The
/// secession's test that both sides stay one territory.
bool key_is_one_territory(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                          int key, int core_tile);

/// BL-1310 — per raster tile, 1 where the tile is land of a key AND lies in
/// that key's territory (its core group, or joined to it by land or chained
/// straits). @p core_tile per key as for `next_exclave_cession`.
std::vector<char> territory_tiles(const std::vector<int>& owner, const std::vector<char>& water, int gw, int gh,
                                  const std::vector<int>& core_tile);

/// BL-1310 — re-derive the water of a carve from its land (NR-792's shore
/// rule, `generate_nations`' own derivation): every water tile is cleared and
/// re-owned from the shore. A carve whose land did not change is returned
/// unchanged, so it is safe to call after any relabel.
void rederive_carve_water(std::vector<int>& owner_map, const std::vector<terrain_substrate>& sub, int gw, int gh);

/// BL-1089 — WHAT THE FOLD DID, per nation, in the order `generate_nations`
/// returns the nation ids. Written for the caller to put on the generation
/// report, so the seat card and the per-world colour table can read which
/// realm a nation is and which it absorbed, without re-running the carve.
struct nation_fold_record
{
    /// Per nation: the polity id of its founding realm (its fold
    /// representative's polity), or -1 for a nation of ownerless ground.
    std::vector<int32_t> polity;
    /// Per nation: the polity ids of the realms the size floor folded INTO it
    /// (merge rule A: it kept its own name; these are listed on its card), in
    /// absorption order. Flattened: `[absorbed_first[n], absorbed_first[n] +
    /// absorbed_count[n])` indexes `absorbed`.
    std::vector<int32_t> absorbed_first;
    std::vector<int32_t> absorbed_count;
    std::vector<int32_t> absorbed;
    /// BL-1310: exclaves the tail's tile-level pass ceded (§ Pass 2e). On a
    /// world whose closing rounds ran, the history ceded them all, so a
    /// non-zero count here is drift between the plan and the carve.
    int32_t cessions = 0;
};

/// Generate nations over the tile map of one body and register all results in @p w.
///
/// Runs the five-pass nation generation pipeline (seed placement, territory
/// expansion, resource profiling, political character, naming) described in
/// docs/generation/NATION_GENERATION.md. The grid wraps horizontally (columns)
/// and does not wrap vertically (rows), matching the tile layout produced by
/// generate_body_tiles.
///
/// Deterministic in @p seed: the same seed, tile map, and params always produce
/// the same political map. Does not use std::random_device or global state.
///
/// @param w        World holding the tile components; receives the new nation
///                 entities and tile_to_nation ownership entries.
/// @param body_id  Body whose tiles are being given political ownership.
/// @param tile_ids Raster-order tile IDs from generate_body_tiles (index = row * gw + col).
/// @param gw       Grid width (columns).
/// @param gh       Grid height (rows).
/// @param params   Tunable generation parameters.
/// @param seed     Per-run RNG seed for independent, reproducible results.
/// @param progress Optional progress sink (BL-305). When non-null, Pass 2
///                 publishes each tile as it is claimed and Pass 2c republishes
///                 the merged map, so a loading screen can show the carve
///                 happening. A pure TAP: write-only, consumes no randomness,
///                 changes no branch — passing null (the default, and every
///                 headless caller) produces a byte-identical political map.
///                 Defined in world/hard_coded_world.hpp.
/// @param fold_out Optional (BL-1089): receives what the fold did per nation,
///                 in the returned order. A pure READ of the passes — filling
///                 it consumes no randomness and changes no branch.
/// @return         Nation entity IDs in seed-placement order (one per nation created).
std::vector<entity_id> generate_nations(
    world& w,
    entity_id body_id,
    const std::vector<entity_id>& tile_ids,
    int gw, int gh,
    const nation_params& params,
    uint32_t seed,
    struct generation_progress* progress = nullptr,
    nation_fold_record* fold_out = nullptr);

/// BL-1278 — THE GROUND THE CARVE READS, and nothing else: per raster tile
/// its substrate (water is unclaimable; the shore derivation reads it) and its
/// landform's expansion step cost. A value snapshot, so a carve can be drawn
/// with no world in hand — the closing rounds' spec carries one, and a replay
/// of the Industrialisation span from its fixture draws the same carve.
struct nation_carve_ground
{
    int gw = 0;
    int gh = 0;
    std::vector<terrain_substrate> sub;       ///< barren where no tile stands
    std::vector<float>             step_cost; ///< 1.0 where no tile stands
};

/// The snapshot `generate_nations` itself carves from.
nation_carve_ground nation_carve_ground_of(const world& w,
                                          const std::vector<entity_id>& tile_ids,
                                          int gw, int gh);

/// BL-1278 — THE CARVE, PREVIEWED. Passes 1 (historical seeds), 1b, 2, 2b and
/// the water derivation of `generate_nations`, run on the same code with the
/// same seed, from @p seed_tiles (the anchors, as `settlement_seed_tiles`
/// gives them) — and nothing else: no fold, no floor, no world write. Returns
/// per raster tile the index INTO @p seed_tiles of the anchor whose cell holds
/// it (-1 = unclaimed). The closing rounds plan against it, so the history
/// and the carve read one geometry. @p seed is `generate_nations`' own seed.
std::vector<int> preview_nation_carve(const nation_carve_ground& ground,
                                      const std::vector<int>& seed_tiles,
                                      uint32_t seed);

// ---------------------------------------------------------------------------
// BL-571 — nation garrisons (docs/military/MILITARY.md § Nation garrisons)
// ---------------------------------------------------------------------------
// Nations own units. `unit_component::owner` accepts a nation entity exactly
// as it accepts a corp's. A garrison is seeded at generation, not hired: one
// in the nation's capital province, plus one in each border province it
// shares with its single highest-grudge neighbour (`nation_grudge`,
// nation_ai.hpp). This is the FOURTH unit writer — alongside the hard-coded
// stub, `seed_starting_military` and `hire_unit` (MILITARY.md § The unit
// model) — and the only one whose owner is a nation rather than a corp.
//
// Garrisons are STATIC: they hold their seeded tile and never march. A nation
// that wants ground HIRES — that is the whole point of the mercenary
// contract, and a garrison that marched would make the nation its own
// mercenary.

/// Tunable parameters for garrison sizing, treasury-scaled against BL-543's
/// value anchor (unit equipment costs ~ 2x annual salary). PLACEHOLDER
/// NUMBERS: BL-544 (unit wage reference), the item that would give this a
/// measured figure to derive from, has not landed — see
/// `seed_nation_garrisons`'s own comment and BL-571's report for the flag to
/// Ben.
struct nation_garrison_params
{
    /// Floor a garrison never falls below, however poor the nation — "a poor
    /// one a token one" (MILITARY.md), never nothing. The count a nation on
    /// `nation_params::treasury_floor` gets (BL-975: a Voronoi cell the
    /// history never held; every nation before that item).
    int min_count = 20;

    /// Additional garrison head per credit of `nation_component::treasury`.
    float count_per_credit = 0.05f;

    /// Ceiling so a large treasury cannot field an unbounded garrison.
    int max_count = 200;

    /// Roster row every garrison uses — row 0, the same default
    /// `seed_starting_military` gives a corp's opening unit.
    uint16_t roster_row = 0;
};

/// Seed one static garrison per nation, per `nation_garrison_params` above.
///
/// MUST run AFTER `build_province_partition` and `seed_province_holders`
/// (province.hpp) — the capital-province lookup reads `w.provinces`, and the
/// border-province test reads `province_holder_for` (BL-569), consumed here
/// exactly as this item's brief names. `w.tile_to_nation` must also already
/// be populated (`generate_nations` writes it).
///
/// DETERMINISTIC: nations walked in ascending entity id (`w.nations` is
/// unordered); a nation's candidate neighbours and its target provinces are
/// both gathered into `std::set`s (ascending id/province id) before any
/// argmax or creation, so ties break on ascending id without a second sort
/// and unit-creation order does not depend on either unordered container's
/// layout.
///
/// @param w      World whose provinces, province_holder and tile_to_nation
///               are already built; receives new nation-owned unit entities.
/// @param params Sizing and roster tuning.
void seed_nation_garrisons(world& w, const nation_garrison_params& params = {});

/// A BORDER garrison's post (Ben, 2026-10-03; BL-1145): the tile of @p pr
/// facing @p guarded — the member tile (not itself @p guarded's) with the most
/// hex sides onto @p guarded's ground, ties to the lowest tile id. Facing is
/// what the nation scorer reads (nation_ai.cpp build_force counts a unit as
/// border force only through its own tile's hex neighbours), so a garrison
/// posted here is one its nation's threat term can see. `null_entity` when no
/// member tile touches @p guarded — never for a province seed_nation_garrisons
/// picked as a border province, which it picks by that same adjacency.
/// Exposed so centre_census C16 asserts the rule against the same function.
entity_id garrison_border_post(world& w, const province& pr, entity_id guarded);

/// Summed `unit_strength` (BL-459, unit_roster.hpp) of every nation-owned
/// unit standing in the province with id @p province_id — the garrison-
/// strength-per-province query this item provides (alongside the units
/// themselves) for a later consumer (a contract's target-strength read,
/// a UI surface) to read without re-deriving the ownership test.
///
/// At most one nation garrisons any given province BY CONSTRUCTION —
/// `seed_nation_garrisons` places at most one — so this does not need to
/// name which nation; a province with none, or id 0 (unpartitioned ground),
/// returns 0.
///
/// ORDER-INDEPENDENT BY CONSTRUCTION: `unit_strength` is an integer, so a
/// sum over the unordered `w.units` is associative and cannot depend on the
/// map's layout (the same property `condition_subject::military_strength`
/// already relies on — MILITARY.md § The unit model).
///
/// @param w           World whose `w.units` and `w.nations` are read.
/// @param province_id Province to sum over.
/// @return            Summed strength (`unit_strength`'s own `int64_t`), or 0.
int64_t garrison_strength_in(const world& w, uint32_t province_id);
