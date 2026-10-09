#pragma once

#include "era_timelapse.hpp" // history_corridor / history_road_node (BL-768)
#include "world.hpp"

#include <cstdint>
#include <map>
#include <vector>

// The loading screen's progress sink (BL-1072), passed by pointer only.
struct generation_progress;

// ---------------------------------------------------------------------------
// Road-network generation (BL-146 — follow-on from the BL-077 logistics core)
// ---------------------------------------------------------------------------
// Builds each nation's intra-body road lattice, run AFTER nation + population
// generation (the pass needs cities to connect). Deterministic from the campaign
// seed — a pure function of the already-generated tiles/nations/centres, with no
// clock or RNG: every collection is sorted by entity id before use and every
// tie-break is explicit, so the stamped road_level field is reproducible.
//
// Per nation, over its population centres (nodes):
//   0. Local streets  — every centre's own tile gets at least a TRACK (Sprint B2). A nation
//      with a single centre owns no inter-city edge, and used to end generation with no road
//      at all; a settlement has streets regardless, and this is what puts such a nation on
//      the lattice for the border links and for the player's place_road to extend.
//   1. Weighted graph  — terrain-weighted A* cost (intra_body_path) between each
//      TOWN-AND-UP pair (scale >= 2; BL-620 — at demography density all-centre
//      pairs were the generation cost wall), one-off at generation. Only a pair that
//      can be LAID is an edge (NR-945): a route across open sea is never a road, so a
//      nation the sea divides builds one tree per landmass.
//   2. Backbone        — Kruskal MST over that graph, tie-broken by
//      (cost, lo-tile-id, hi-tile-id); then the DETOUR TEST (BL-1119): a further
//      link is laid only where the network's own route between its two towns
//      costs more than kDetourRatio x the direct route. No lattice: a second
//      road beside a serviceable one is never built.
//   3. Three-tier      — tier per edge from the two towns' scales (BL-172):
//      HIGHWAY (road_level 3) between two major centres (scale >= 3, City /
//      Metropolis); ROAD (2) when at least one endpoint is Town+ (scale >= 2);
//      TRACK (1) otherwise.
//   4. Rasterise       — each edge is stamped along its A* path, taking the max
//      road_level on overlap and skipping ocean tiles (roads are a land feature);
//      the path already respects the east-west cylinder wrap. An edge whose route crosses
//      OPEN ocean (a water run longer than a strait) is not stamped at all — that is a sea
//      route, and stamping it would scatter road fragments on distant shores.
//   5. Village spurs   — each village (scale 1) AT OR ABOVE the size floor
//      (BL-1119, `village_spur_size`) lays one TRACK to its nearest same-nation tile
//      already JOINED to the backbone (a town's street, the backbone raster, or an
//      earlier spur that reached one), chosen from a grid-distance-prefiltered
//      candidate set (BL-620): spur tracks, not lattice membership. A village is on
//      its nation's network only once its spur reaches such a tile; one below the
//      floor keeps only its local street.
// Then, across nations: one TRACK border link between the nearest centre pair of
// each territorially-adjacent nation pair, so the continent-wide network connects —
// chosen only among centres ON their nation's network, towns and villages that laid a
// spur (Ben, 2026-09-25): a link ending on a bare village street joined nothing.
// Territorial adjacency tolerates a short unowned gap (a strait or an unclaimed margin),
// so a coastal or island nation is reachable rather than silently left off the lattice.
//
// road_level lowers a tile's A* traversal cost via road_traversal_multiplier
// (logistics.cpp), so the follow-on dispatch path needs no change — it simply
// finds the roaded corridors cheaper. Tuning (Ben, 2026-07-11): Track=1, Road=2,
// Highway=3; major-centre threshold scale>=3, Town+ threshold scale>=2.
//
// ---------------------------------------------------------------------------
// BL-1119 (roads tree and detour) — the two numbers the network is shaped by
// ---------------------------------------------------------------------------
// Ben, 2026-09-25, walking round 6: "it should be heavily discouraged to build a
// lattice of roads". LOGISTICS.md § 4 carries the rule.

/// THE DETOUR TEST. After the Kruskal MST, a candidate link between two towns is
/// laid only when the network's route between them — the sum of the direct A*
/// costs of the links it walks, over the tree plus every loop already admitted —
/// costs MORE than this multiple of the direct A* route. Candidates are walked
/// cheapest-first, so a loop that is admitted shortens the network for every
/// later candidate and two parallel loops are never both laid. The percentile
/// rationing (BL-618/BL-621) then keeps the cheapest fraction of the loops this
/// test admits.
inline constexpr double kDetourRatio = 2.0;

/// THE SPUR FLOOR, in HEADS (BL-1119). Only a village whose size is AT OR ABOVE
/// this lays a spur; below it a village keeps its local street alone.
///
/// WHAT "SIZE" READS, AND WHY NOT `population`. At road time every village's
/// `population_centre_component::population` is exactly scale 1's rung
/// (`k_population_for_scale[0]`, 10 thousand) — the founding writes the rung and
/// nothing moves it until the economy ticks — so a floor on it is all or nothing.
/// The size a village does carry at generation is its CARVE SLOT KEY
/// (`world::gen_carve_centres`, BL-1042): its region's urban heads divided by its
/// rank inside that region, the key the rank-size carve ordered every centre on.
/// A centre with no slot (a coverage founding, a province anchor) reads its own
/// headcount, `population * 1000` heads. See `village_spur_size`.
///
/// 40,000 heads (Ben, 2026-09-25, the density form; LOGISTICS.md § 4), read off the
/// measured ladder (BL-1119 D1, `gen_step_costs --roads`): about the 90th percentile
/// of village size on the curated seeds (p50 ~16k, p90 ~38-46k), so the top tenth of
/// villages spur. Above ~15,000 the spurs add only tens of road tiles, so the floor
/// buys pass time rather than map density. A floor of 0 lays a spur from every
/// village, the pre-BL-1119 behaviour.
inline constexpr long long kVillageSpurFloorHeads = 40000;

/// THE BRIDGE CAP (Ben, 2026-10-03, playing the build: "bridges can cross a further
/// distance than I expected -- let's put a cap on that"; he ruled two). The longest
/// contiguous run of water tiles any road may cross: a strait of at most this many
/// SHORE cells (`coast` or lake, never open `ocean`), LOGISTICS.md § 4. Every writer
/// of `road_level` honours it -- the national lattice, tree, spurs and border links
/// (stamp_edge), the ancient corridors (stamp_history_roads, through stamp_edge), and
/// the market joins, pulls and trunk (lay_market_roads' walk) -- and the wizard
/// lapse never draws a road corridor across a longer run. Was three (Sprint B2 /
/// BL-516), which let a bridge span three open tiles of channel.
inline constexpr int kMaxCrossingTiles = 2;

/// BL-1119 STALE-FLOOD MEASUREMENT PROBE — verify-only, never set by shipped code.
/// Within a pass the flood fields and the pair cache are reused and never refreshed
/// after a stamp, so a route is priced on the road field as it stood when its
/// destination's flood was first built. When this is true, `stamp_edge` (the one
/// stamper `generate_roads` and `stamp_history_roads` share) clears every logistics
/// cache after any stamp that raised a tile, so each later route is priced on the field
/// as it stands (LOGISTICS.md § 4). OFF (the default) the passes are byte-identical to
/// the shipped ones: the flag is read in two places, and only to clear caches and to
/// copy the laid path into the write-only traces. road_stale_flood_probe measures it;
/// road_generation_harness --fresh-floods runs its rows on it.
extern bool g_road_probe_fresh_floods;

/// BL-1252 (no parallel roads) MEASUREMENT SWITCH -- verify-only, never set by shipped
/// code. THE SNAP (road_generation.cpp § road_snapper; LOGISTICS.md § 4): the floods stay
/// reused, and before a route is stamped every stretch that would lay two or more
/// consecutive NEW land tiles beside a road already on the field (Chebyshev 1, columns
/// wrapping) is re-walked over that road, kept only when it lays fewer new tiles. When
/// this is true the snap is off and every route is laid as priced, exactly as the passes
/// did before it (road_stale_flood_probe --compare unsnapped reads its effect in one run,
/// on one machine). The BL-1119 fresh-flood probe also runs without the snap, so its
/// reading stays the reference it was measured as.
extern bool g_road_probe_no_snap;

/// The longest contiguous run of WATER tiles (any kind) along @p path, in tiles.
/// The one measure the cap is read against; exposed so a harness asks it of every
/// laid route from outside.
int longest_water_run(const world& w, const std::vector<entity_id>& path);

/// A village's size for the spur floor, in heads — see kVillageSpurFloorHeads.
long long village_spur_size(const world& w, entity_id centre);

/// What one `generate_roads` call laid (BL-1119 D1's reading). WRITE-ONLY: the
/// pass fills it when given one and never reads it back, so a measured call lays
/// exactly the network an unmeasured one does.
struct road_generation_stats
{
    int towns                = 0; ///< backbone nodes (scale >= 2), all nations
    int mst_links            = 0; ///< Kruskal tree links chosen
    int loop_candidates      = 0; ///< reachable non-tree town pairs put to the detour test
    int loops_admitted       = 0; ///< candidates the detour test admitted
    int loops_kept           = 0; ///< admitted loops the percentile ration kept (laid)
    int villages             = 0; ///< scale-1 centres on the body
    int villages_below_floor = 0; ///< villages under the floor: street only, no spur tried
    int spurs_laid           = 0; ///< villages that laid a spur
    int spurs_failed         = 0; ///< at/above the floor, but no target within the cap / by land
    int border_pairs         = 0; ///< territorially-adjacent nation pairs walked
    /// Adjacent pairs where one side holds no centre on its network (no town, no
    /// spurring village), so no link is tried (Ben, 2026-09-25: a border link ends only
    /// on a town or a spurring village).
    int border_pairs_no_endpoint = 0;
    int border_links         = 0; ///< cross-nation Track links laid
    /// Of them, ending on a centre OFF its nation's network (a bare village street).
    /// Zero by rule since the 2026-09-25 ruling; kept so a harness can assert it.
    int border_links_street_only = 0;
    int majors               = 0; ///< centres at scale >= 3 (City+) on the body
    int links_two_major      = 0; ///< backbone links laid (tree or kept loop) between two City+
    int links_highway        = 0; ///< of them, laid at the Highway tier (percentile-gated)
    /// Routes refused because a water run on them is longer than kMaxCrossingTiles (the
    /// bridge cap): town-pair candidates dropped, spur candidates and border links refused.
    int candidates_long_crossing = 0;
    int spurs_long_crossing      = 0;
    int border_long_crossing     = 0;
    /// The loading bar's plan (BL-1072), in the units `report_sub` counts: one per
    /// backbone town PAIR, one per spurring village, kBorderUnitsPerNation per nation.
    long long units_backbone = 0;
    long long units_spurs    = 0;
    long long units_border   = 0;
    int       nation_groups  = 0; ///< nations holding a centre on the body (unowned = one group)
    /// Whole-body flood fields cached when the pass ends — its WORK, independent of
    /// the machine's load: nearly every spur and every A* anchor costs one (a Dijkstra
    /// over the whole body, logistics.cpp § flood_field_for). Includes any field a
    /// caller left cached before the call.
    long long flood_fields   = 0;
    /// Of them, cached before the border walk began (backbone + spurs); the rest are
    /// the border links' probes.
    long long flood_fields_before_border = 0;
    /// Reachable town pairs whose route crosses open sea, so they cannot be laid and
    /// never enter the tree or the detour test (NR-945, LOGISTICS.md § 4).
    int candidates_unlayable = 0;
    /// Backbone links chosen (tree / kept loop) that stamp_edge then refused. Zero by
    /// construction once only layable links are candidates; kept as the check.
    int tree_links_refused   = 0;
    int loops_refused        = 0;
    /// Whole-body flood fields BUILT, per call site (BL-1119 round 4: a path is directed
    /// and answered from its destination's field, so which pairs share a destination
    /// is the pass's cost). Deltas of `world::logistics_flood_fields` around each site.
    long long floods_town_pairs   = 0; ///< the backbone's town-pair costs (MST + detour test)
    long long floods_backbone_lay = 0; ///< stamping the chosen tree links and loops
    long long floods_spurs        = 0; ///< the village spurs (every candidate tried)
    long long floods_border       = 0; ///< the border probes and links
    /// BL-1252 (no parallel roads): route stretches the snap re-walked onto the road
    /// they ran beside.
    long long snaps               = 0;
};

/// What one `stamp_history_roads` call did (BL-1119 round 4). WRITE-ONLY.
struct history_road_stats
{
    int       corridors   = 0; ///< corridors with both ends on the body
    int       laid        = 0; ///< of them, stamped (reachable, no open-sea crossing)
    int       destinations = 0; ///< distinct tiles the corridors were priced TOWARD
    long long floods      = 0; ///< whole-body flood fields built by the call
    /// Of the corridors not laid, refused because a water run on the route is longer
    /// than kMaxCrossingTiles (the bridge cap) -- the rest crossed open ocean or were
    /// unreachable.
    int       refused_long_crossing = 0;
    /// BL-1252: corridor stretches the snap re-walked onto the road they ran beside.
    long long snaps                 = 0;
};

/// Every corridor one `stamp_history_roads` call LAID, whole (the bridge-cap row reads
/// each route's water runs from outside). WRITE-ONLY, like the stats.
struct history_road_trace
{
    struct route
    {
        entity_id from = null_entity; ///< the tile the corridor was priced from
        entity_id to   = null_entity; ///< the tile it was priced toward (the busier end)
        std::uint8_t tier = 0;
        std::vector<entity_id> path;  ///< the whole route laid, water tiles included
    };
    std::vector<route> routes;
};

/// BL-1119 round 3: every route one `generate_roads` call LAID, whole, for a harness to
/// rebuild the network's connectivity independently of the pass's own bookkeeping.
/// WRITE-ONLY like the stats: filled when given one, never read back.
struct road_generation_trace
{
    enum class kind : std::uint8_t { tree, loop, spur, border };
    struct route
    {
        kind      k      = kind::tree;
        entity_id from   = null_entity; ///< first endpoint tile (a town; the spurring village; a border centre)
        entity_id to     = null_entity; ///< second endpoint tile (a town; the spur's target; a border centre)
        entity_id nation = null_entity; ///< the laying nation (a border link: its lower-id nation)
        std::vector<entity_id> path;    ///< the whole A* route, water tiles included, lo -> hi
    };
    std::vector<route>     routes;
    std::vector<entity_id> on_network; ///< centres the pass counts ON their nation's network
};

// @param progress  Optional loading-screen sink (BL-1072): the village spur walk,
//                  most of this pass at a low spur floor, is reported through
//                  `report_sub`. Write-only; null (every caller but generation)
//                  publishes nothing.
// @param spur_floor_heads  The village spur floor (BL-1119), in heads; defaults to
//                  kVillageSpurFloorHeads. A parameter so a harness can measure a
//                  ladder of floors on one built world without a recompile.
// @param stats     Optional, write-only: what the call laid (BL-1119 D1).
// @param trace     Optional, write-only: every route the call laid, whole (BL-1119 round 3).
void generate_roads(world& w, entity_id body, generation_progress* progress = nullptr,
                    long long spur_floor_heads = kVillageSpurFloorHeads,
                    road_generation_stats* stats = nullptr,
                    road_generation_trace* trace = nullptr);

// ---------------------------------------------------------------------------
// Roads pull toward markets (BL-1138; Ben, 2026-09-25; NR-950)
// ---------------------------------------------------------------------------
//
// LOGISTICS.md § 4, "Roads pull toward markets". Read after the market folds
// (BL-1125) leave the home body its final markets, as one pass over the laid network
// (the national lattice and the ancient corridors):
//
//   0. NEIGHBOURS FIRST. The trunk's pairs are chosen on the network as the pass
//      finds it: each market centre's kMarketTrunkNeighbours nearest centres by
//      direct route, unioned over both ends (NR-950's delegated reading: a
//      Delaunay-like set, never all pairs). A pair is priced from a LAND end; a pair
//      no land end can price (both centres off land, or no route) is unpriced and
//      dropped.
//   1. JOIN — every market centre on its nation's own backbone. A centre whose nation
//      (the nation holding its tile) has towns, and which no road of that nation joins
//      to one of them, lays one link to the nearest roaded tile of that nation that is
//      joined, along the cheapest route over the nation's OWN land (and the straits
//      between) — a join over foreign ground would leave it off its own backbone. A road of another nation never counts —
//      border links are Tracks ending on a town or a spurring village, not part of a
//      backbone. A centre whose nation holds no town (or no nation holds) has no
//      backbone to join: it is counted, never joined elsewhere. Tier: the backbone's
//      Town+ rule under the percentile gate (`edge_tier`).
//   2. PULL — a town weighed by how much nearer the link brings it to its market (the
//      market its tile clears at, when that market's centre lies in the town's own
//      nation). A town whose network route fails the detour test (more than
//      kDetourRatio x the direct route) is a candidate, weighted by the GAIN, network
//      route minus direct route; per market the heaviest is laid first, the network
//      re-read, and the walk repeats until none fails, so one spoke serves its
//      neighbours. Tier: `edge_tier` of the two ends. NOT rationed by the percentile
//      (a reading, LOGISTICS.md § 4).
//   PER TILE (Ben, 2026-10-03): each tile of a join, pull or trunk takes the tier of the
//   nation whose land it crosses — the link's tier, capped by the gate at that nation's
//   percentile — so a sub-0.40 nation's ground stays Track and unowned land reads Track.
//   3. TRUNK — the pairs, cheapest first by their price at step 0; the detour test,
//      re-read at the pair's turn, refuses a pair the network already serves. Tier:
//      `edge_tier` of the two market ends at the LOWER of their two nations'
//      percentiles (Ben, 2026-10-03: where a nation falls under 0.40 its roads stay
//      Track — newer than NR-950's "trunk at Road").
//
// ONE COST MODEL (LOGISTICS.md § 1). Every route is priced with `tile_traversal_cost`
// on the field as it stands (landform or sea weight x road x lane), in integer
// millionths per cell; an edge costs the sum of its two cells. The DIRECT route walks
// any land and any shore-water run of at most three cells (a strait, the road stamp's
// crossing rule), never open ocean; the NETWORK route walks the same but only roaded
// land — the road network as a convoy prices it, any nation's roads included. A link
// is laid along the direct route, so it reuses the roads that already shorten it. The
// river discount, which is directed, is not read: these routes are symmetric.
//
// AFTER THE FOLDS, AND IT CANNOT LOOP. The gravity fold read traversal cost before
// this pass ran, and a catchment is grid-nearest (`market_for_tile`), so the roads
// laid here move no catchment and no fold.
//
// Deterministic: markets in ascending id, towns in ascending tile, pairs in (price, lo
// id, hi id); every Dijkstra orders its frontier on (cost, state index) over integer
// costs. Purely additive (the stamp takes the max).

/// How many nearest market centres each centre's trunk reaches toward (NR-950).
inline constexpr int kMarketTrunkNeighbours = 3;

/// What one `lay_market_roads` call did. WRITE-ONLY.
struct market_road_stats
{
    int markets               = 0; ///< anchored markets on the body
    int markets_unowned       = 0; ///< of them, centred on a tile no nation holds
    int off_backbone_before   = 0; ///< centres no road of their nation joins to its towns
    int off_backbone_after    = 0; ///< ...after the pass
    int no_backbone           = 0; ///< of the markets, whose nation holds no town (or none holds)
    int joins_laid            = 0;
    int joins_failed          = 0; ///< a backbone exists, but no route over the nation's own land and straits reaches it
    int towns                 = 0; ///< towns on the body
    int towns_in_nation       = 0; ///< of them, whose market's centre lies in their nation
    int pull_candidates       = 0; ///< towns failing the detour test on the first reading
    int pull_laid             = 0;
    int trunk_pairs           = 0; ///< neighbour pairs, unioned
    int trunk_unpriced        = 0; ///< of them, no land end prices them: dropped
    int trunk_refused         = 0; ///< pairs the detour test refused (a serviceable route exists)
    int trunk_laid            = 0; ///< laid, with a land tile on the route
    int trunk_empty           = 0; ///< admitted, but the route held no land tile: nothing laid
    int tiles_raised          = 0; ///< distinct land tiles whose road_level the pass raised
    long long walks           = 0; ///< Dijkstra walks the pass ran (its cost)
};

/// Every link one call considered, whole, for a harness. WRITE-ONLY.
struct market_road_trace
{
    /// pull_settled: not a link — the moment a market's pull walk ended (no town of it
    /// failed the test then), at `raises_before`; `market_a` is the market.
    enum class kind : std::uint8_t { join, pull, trunk, pull_settled };
    struct link
    {
        kind         k        = kind::trunk;
        entity_id    from     = null_entity; ///< tile the route starts on: the centre (join), the town (pull), the pricing (land) end (trunk)
        entity_id    to       = null_entity; ///< tile it ends on: the joined tile (join), the centre (pull), the other end (trunk)
        entity_id    market_a = null_entity; ///< the market (join, pull); the lower-id market (trunk)
        entity_id    market_b = null_entity; ///< trunk: the higher-id market
        std::int64_t pair_q    = -1;         ///< trunk: the pair's price at step 0 (-1: unpriced)
        std::int64_t direct_q  = -1;         ///< direct route cost at the test (-1: none)
        std::int64_t network_q = -1;         ///< network route cost at the test (-1: none within the bound)
        bool         laid      = false;
        std::uint8_t tier      = 0;
        /// How many `raises` had happened when this link was tested: undoing the raises
        /// from the end down to this index gives the field the test read.
        std::size_t  raises_before = 0;
        std::vector<entity_id> path;         ///< the direct route, from -> to (laid links only)
    };
    std::vector<link> links;
    /// Every raise the pass made, in order: (tile, road_level before the raise). Undoing
    /// them all, last first, gives the field exactly as the pass found it.
    std::vector<std::pair<entity_id, std::uint8_t>> raises;
};

/// BL-621: each nation's qualification PERCENTILE among the world's nations, mid-rank on
/// ties — (count below + half the tied group) / N. The ONE formula every road gate reads
/// (generate_roads, lay_market_roads); exposed so a harness re-derives a tier from outside.
std::map<entity_id, float> road_qualification_percentiles(const world& w);

/// Lay the market pulls and the trunk on @p body (see above). Generation calls it
/// once, after the market folds. @p stats / @p trace optional and write-only.
void lay_market_roads(world& w, entity_id body, market_road_stats* stats = nullptr,
                      market_road_trace* trace = nullptr);

/// Undo every raise @p trace recorded, last first: the road field as `lay_market_roads`
/// found it. Drops the logistics caches. For instruments reading the pre-pass network
/// (market_gravity_ladder folds on it; road_generation_harness reads "before").
void undo_market_roads(world& w, const market_road_trace& trace);

// ---------------------------------------------------------------------------
// Ancient roads, STAMPED FROM the history (BL-768)
// ---------------------------------------------------------------------------
//
// Ben, the 2026-09-03 eight-phase reorder, point 4: *"We should also be laying
// simple roads to supply provinces."* This is that, and the emphasis is on
// FROM: the roads are not laid inside the Era -1 sim — a feasibility pass came
// back blocked on three independent structural grounds (era_timelapse.hpp
// § The ancient road record) — they are stamped afterwards from what the sim
// RECORDED walking. Each corridor is a campaign's staging-to-objective supply
// line or a founding party's parent-to-daughter route, so the network's shape
// is the history's own trunk routes rather than a plausible-looking lattice.
//
// AN ERA-APPROPRIATE TIER RULE, WHICH THIS ITEM OWED. `generate_roads`' gates
// read a nation's qualification PERCENTILE (LOGISTICS.md § Roads), which is a
// 1960-era field derived from industrialisation timing; an ancient road cannot
// borrow it, and an antiquity world has no spread in it to read anyway. The
// ancient rule keys on the two things the history does produce:
//
//   TRAFFIC       — how many times the corridor was actually used. A line an
//                   empire supplied four campaigns and a dozen foundings along
//                   is a Road; one walked once is a Track.
//   WORKS         — whether BOTH ends raised something reach-bearing. A work
//                   promotes the corridor one rung, and it is the ONLY route to
//                   a Highway before the industrial era, so an ancient trunk
//                   highway means traffic AND the stations to carry it.
//
// That keeps LOGISTICS.md's antiquity shape intact — "Roads on every Town+
// backbone, Highways nowhere" for a world that built nothing — while giving the
// works roster a payoff that persists onto the campaign map, which is what
// BL-757's zero-works finding left it without.
//
// PURELY ADDITIVE, and deliberately run AFTER `generate_roads`. Stamping takes
// the max on overlap, so no modern road is ever downgraded and the ancient
// network appears exactly where the nation lattice did not already reach or
// reached lower. Running it BEFORE was the other option and is rejected here:
// the modern pass decides its MST and its tiers on terrain-weighted A* costs,
// so pre-stamped ancient roads would silently re-route the whole national
// lattice — a far larger change than this item's aim, and one that would move
// every road-shaped measurement at once for a reason unrelated to the history.
//
// Deterministic: `corridors` arrives sorted by (a, b), the tier rule is pure
// integer arithmetic over the corridor's own fields, and the stamp takes the max
// per tile — so neither the walk order nor the overlap order can vary the field.
// Roads are a land feature here exactly as in `generate_roads`: water tiles are
// skipped and a corridor whose route crosses open ocean is not stamped at all.
//
// @param nodes      Indexed by region — where each region stood, and its
//                   accumulated works reach. A corridor naming an index past
//                   the end of this array is skipped.
// @param corridors  `history_sim_state::supply_corridors`. Empty (a world with
//                   no Era -1 pass) makes the whole call a no-op.
void stamp_history_roads(world& w, entity_id body,
                         const std::vector<history_road_node>& nodes,
                         const std::vector<history_corridor>&  corridors,
                         generation_progress* progress = nullptr, // BL-1072: per corridor
                         history_road_stats* stats = nullptr,     // BL-1119 round 4, write-only
                         history_road_trace* trace = nullptr);    // the bridge cap's row, write-only

// ---------------------------------------------------------------------------
// Sea lanes, STAMPED FROM the lane record (BL-1098)
// ---------------------------------------------------------------------------
//
// The water analogue of `stamp_history_roads` (LOGISTICS.md § 4b; EXPLORATION.md
// § The colonial tie is a sea lane). The history spans record every sea leg they
// crossed -- a wet campaign, a purchase party, a metropole's standing traffic and
// trade across water (the four writers) -- as one row per pair of seats with a use
// count (`sea_leg`). A leg at or over `history_sim_params::sea_lane_tier1_uses` has
// earned its lane, and this pass lays it onto the water as `tile_component::lane_level`
// on every sea tile the lane's path crosses; traversal cost then reads it
// (`tile_traversal_cost`), so the lane discounts the sea for every consumer.
//
// THE PATH IS A WATER-ONLY WALK, never a straight raster: a straight line between two
// shores crosses land on any concave coast, and the road stamp's strait rule refuses
// open ocean outright. The walker is a Dijkstra over the body's SEA tiles (`is_sea`:
// ocean and coast; a lake is not the ocean) on FOUR CARDINAL STEPS with columns
// wrapping -- the grid every traversal reader walks (logistics.cpp, LOGISTICS.md § 2),
// so every step of a lane is a step a traveller can take and a lane is ridden at its
// full discount on a diagonal as on a straight. Each step is PRICED WITH THE CURRENT
// (EXPLORATION.md § Currents, "Where currents bite"): 1000 times the leg cost
// `ocean_current_leg_cost_q` of the step's direction against the entered tile's ocean
// region current, at the spans' own weight -- so a lane bends along the water that
// carries it rather than hugging the straight line.
//
// ENDPOINTS: each seat's PORT is its nearest sea tile within `kSeaLanePortRadius`
// (Chebyshev, ties to the lower raster index). A REALM'S PORT IS ITS NEAREST COASTAL
// REGION'S SEAT (BL-1153; Ben, 2026-09-27, NR-955 B): a lane end whose seat has no sea
// that near moves to the nearest region OF THE SAME REALM whose seat has one, and the
// lane starts at that seat's nearest sea tile. "Nearest" is the sim's own region
// measure (`region_distance`: Chebyshev between the two seats, columns wrapping),
// ties to the lower region index. The realm is the one the caller hands per region
// (the polity holding it at the history's last close). An end held by no realm, or
// whose realm holds no coastal region at all, still carries no lane, and the leg is
// counted rather than stamped. The port picked for a seat stays its nearest sea tile;
// a port facing the partner (NR-955's option E) was not taken. A leg whose two ends
// land on ONE port tile lays nothing and is counted apart (the ancient road stamp
// skips a corridor whose two ends are one tile, for the same reason: no water lies
// between them).
//
// DIRECTION: the record carries no direction (`sea_leg` is `a < b`), so the walk is
// priced TOWARD THE BUSIER END -- the PORT TILE more walkable lanes land on, counted
// after the moves -- as the ancient road stamp keys its degree by the tile a road
// lands on. A tie walks from the lower raster index. The direction is therefore a
// pure function of the port pair, so one port pair is never walked both ways (which
// with the current on would lay two different paths). Lanes radiate from metropoles
// and entrepots, and what a lane carries (tribute, trade) flows to them, so the walk
// runs the way the cargo does.
//
// TRUNKS: lanes reuse lanes (`kSeaLaneReuseCostQ`). The walkable lanes are walked
// busiest first -- most uses, ties by the leg's (a, b) -- and each walk enters a tile
// an earlier lane laid at a discount, so a later lane into a port the busier one
// already reaches joins its water and rides it in. The field therefore depends on the
// walk order, which is a pure function of the record.
//
// Deterministic: the order above is total, the Dijkstra orders its frontier on the
// pair (cost, raster index), which is unique, and the stamp takes the max per tile.
// Purely additive and purely water: no land tile, and no road, is touched.

struct ocean_current_field; // ocean_currents.hpp

/// How far from its seat a lane may find its port, in tiles (the Era -1 sim's own
/// neighbour radius, `history_sim_params::neighbour_radius` = 9: a seat's region
/// reaches that far).
inline constexpr int kSeaLanePortRadius = 9;

/// LANES REUSE LANES, as roads reuse roads: a sea tile an earlier lane of the same
/// call already carries is walked at this many per mille of its priced step. The
/// lanes are walked busiest first (most uses; ties by the leg's (a, b)), so the
/// busier trade lays the trunk and a later lane into the same port rides it in --
/// lanes into one port share their water near it and fan out far from it. 500 is
/// the lane's own traversal discount (x 0.50, LOGISTICS.md § 4b): the stamp reads
/// a laid lane as a traveller does. 1000 is no reuse: every lane walked alone.
/// A TRUNK IS SHARED ONLY WITH THE CURRENT (Ben, 2026-10-03): the discount applies
/// to a step that does not run against the entered tile's current
/// (`sea_walk_laned`); upstream the walk pays the full step, so a busy trunk never
/// carries a later lane against the water.
inline constexpr int kSeaLaneReuseCostQ = 500;

/// What one `stamp_sea_lanes` call did. WRITE-ONLY.
struct sea_lane_stats
{
    int       earned      = 0; ///< legs at or over the lane tier, both seats on the body
    int       laid        = 0; ///< of them, walked and stamped
    int       no_port     = 0; ///< an end with no port even from its realm's coast (the three below, summed)
    int       no_port_no_realms = 0; ///< of them, an end's seat is inland and no realm table reached it (a caller fault)
    int       no_port_unheld   = 0; ///< of them, an end's seat is inland and no realm holds it
    int       no_port_no_coast = 0; ///< of them, an end's seat is inland and its realm holds no coastal region
    int       same_port   = 0; ///< both ends' ports are one tile: no water between them, nothing laid
    int       unreachable = 0; ///< two distinct ports found, but no water joins them
    int       moved_ends  = 0; ///< lane ends (of legs reaching the walk) whose port came from their realm's coast
    long long path_tiles  = 0; ///< tiles over every laid path (a shared tile counts per lane)
    int       lane_tiles  = 0; ///< distinct tiles carrying a lane after the call
};

/// Every lane one call laid, whole, for a harness (the bend reading, the picture).
/// WRITE-ONLY like the stats.
struct sea_lane_trace
{
    struct lane
    {
        int              a = 0, b = 0;   ///< the leg's region indices (a < b)
        int              uses = 0;       ///< the leg's recorded uses
        int              from_port = -1; ///< raster index the walk started at
        int              to_port   = -1; ///< raster index it ended at
        /// The region whose seat each port was taken from, in walk order: the lane
        /// end itself, or (BL-1153) its realm's nearest coastal region.
        int              from_seat = -1;
        int              to_seat   = -1;
        std::vector<int> path;           ///< raster indices, from_port -> to_port
    };
    std::vector<lane> lanes;
};

/// A seat's port: the nearest sea tile to (@p col, @p row) within @p radius
/// (Chebyshev, columns wrapping), ties to the lower raster index; -1 if none.
/// @p sea is one byte per tile, raster order, 1 on sea.
int sea_lane_port(const std::vector<std::uint8_t>& sea, int gw, int gh, int col, int row, int radius);

/// The water-only walk from raster index @p from to @p to, both sea tiles, priced
/// with @p currents at @p weight_q (null or 0 = still water). The path in walk
/// order, both ends included; empty when no water joins them.
std::vector<int> sea_lane_walk(const std::vector<std::uint8_t>& sea, int gw, int gh,
                               const ocean_current_field* currents, int weight_q,
                               int from, int to);

/// Stamp every earned lane in @p legs onto @p body's sea tiles. @p nodes is indexed by
/// region (where each seat stood), as `stamp_history_roads` takes it. @p region_realm
/// is parallel to @p nodes: the realm holding each region, -1 for none (BL-1153 -- an
/// inland end moves to its realm's nearest coastal seat). A region it does not reach
/// (an empty table reaches none) has no realm handed, and an inland end there lays
/// nothing, counted as `no_port_no_realms`. @p lane_tier_uses
/// is the record's own threshold; @p current_weight_q and @p rotation_sense are the
/// spans' current params (`history_sim_params::sea_current_*`) -- the field is rebuilt
/// here from the body's tiles, the pure function the spans built it with. Empty @p legs
/// (no span ran) makes the call a no-op.
void stamp_sea_lanes(world& w, entity_id body,
                     const std::vector<history_road_node>& nodes,
                     const std::vector<sea_leg>&           legs,
                     const std::vector<int>&               region_realm,
                     int lane_tier_uses, int current_weight_q, int rotation_sense,
                     sea_lane_stats* stats = nullptr,
                     sea_lane_trace* trace = nullptr,
                     int reuse_cost_q = kSeaLaneReuseCostQ);
