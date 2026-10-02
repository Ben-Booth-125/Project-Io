// centre_census — how many population centres a generated world carries, how
// much of its land they cover, how large they stand, and whether each one fits
// the ground of the region that grew it.
//
//   BL-1130 (centres consolidate), task 1 — THE CENSUS the rule is measured
//   against (Rule 0b: measure first). The roads lane found (BL-1119 D1) that
//   centres covered 49-100% of land on the curated seeds, so every centre being
//   a street made the road field a lattice. POPULATION.md § Generation, "Growth
//   consolidates", wants a few cities over many towns over a train of villages,
//   and no region carrying more centres than its own cell of the settlement
//   partition holds. Per curated seed, on the home body:
//     * THE SIM RECORD — regions, living regions, regions standing centres; the
//       sum of `region::centres` (what the carve will try to materialise) and of
//       `urban_population`; the largest region count and how many regions sit
//       at `region_centre_limit`.
//     * THE CARVE — carved centres founded (`world::gen_carve_centres`), carved
//       centres dropped (`world::gen_carve_dropped`, by reason), and the two
//       region-blind passes' foundings (province anchors / national coverage).
//     * DENSITY — every centre on the body, the land tiles hosting one (the
//       share of land that is a centre, i.e. a street), and the land stamped
//       urban by the footprints (`land_use` urban).
//     * SCALE — every centre by `k_population_for_scale` rung: village (10k),
//       town (50k), city (200k), metropolis (1M), megacity (5M).
//     * FIT — SPILLS: carved centres standing outside their source region's
//       cell (`nearest_region` of the centre's tile differs from the carve
//       slot's region). NOMINAL OVERFULL: regions whose carved centres'
//       footprints BY SCALE (`k_urban_footprint_tiles` of the carve's
//       body-wide rank-size scale) add up to more land than the cell holds —
//       what the centres would pave uncut; the one-centre regions split out.
//       PAVED ACROSS: urban tiles no centre of their own cell stands on or
//       beside — paving that crossed a cell edge (zero once the stamp stops a
//       footprint at the cell, BL-1130 review fix), and the cells it fell in.
//       Regions carrying more carved centres than their cell has placeable
//       tiles; PLACELESS and GROUNDLESS regions (living, a cell with no
//       standable tile) and how many still carry a centre; and THE CAP: the
//       SIM RECORD against the final partition by `region_centres_fit` on the
//       cell's standable ground — a FAIL row, with the groundless-standing
//       count, since no region may carry more centres than its ground holds.
//     * THE STOCKPILE (BL-1042) — the points the carve's slots receive and
//       every unspent reason; a region that earned points the carve gave no
//       slot lands in the `no_carved_centre` residual.
//     * THE HISTORY — per span (Empires, Exploration, Industrialisation), the
//       battles, conquests and foundings the generation report carries, since
//       the Era -1 sim reads `region::centres` (BL-1130 round 2 attributes the
//       move the rule makes in them).
//     * WHERE THE DENSITY IS — living regions whose cell is ONE land tile
//       (the settle verb founds on the nearest free tile, so settled cores
//       pack a region to a tile), and how many carved centres stand in such
//       cells, against those in larger cells and the region-blind foundings.
//     * WHAT FOLLOWS — road tiles on the body (`road_level > 0`), split into
//       STREETS (a road on a centre's own tile) and the network; and markets on
//       the body (`world::markets`), which the carve gates on centres.
//     * THE PROVINCES (C7/C8; BL-1133, a province is its centre's ground) —
//       the home body's land provinces against its centres: how many hold a
//       SEED centre (any centre not an anchor founding), how many hold more
//       than one centre, how many rest on an anchor founding alone; and the
//       size distribution in land tiles (median, p90, max), overall and by
//       the ANCHOR's scale — the anchor being the highest summed centre scale
//       standing in the province, ties to the lowest tile id, exactly as
//       `seed_province_holders` derives it — C8 over SEEDED provinces only,
//       so an anchor-only island never reads as a village's ground (BL-1145).
//       C7's `unanchored` is a FAIL row (BL-1145): at generation every land
//       province on the settled home body holds a centre (PROVINCES.md
//       ruling 3; one can outlive it only in play, NR-952). C7b reads the anchor-only
//       provinces (the uncentred islands) on their own, with WHY each is left
//       (BL-1150): touching its own nation's land (the retired settlement
//       lock's islands), a nation lock, water, or no nation at all.
//     * THE SPACING LADDER ROW (C9; BL-1132, settle spacing) — one compact row per
//       seed of what a settle-spacing candidate moves: regions, living, living
//       regions by cell land (1 / 2-4 / 5-9 / 10-24 / 25+ tiles), centres, the
//       land share under them, the land provinces, and the three spans'
//       battles / conquests / foundings; pooled below it. The header names the spacing the binary was
//       built with (`generation_settle_spacing_tiles`), so a ladder is a set of
//       runs of builds that differ in that one constant.
//     * THE SPACING ITSELF (C10; BL-1132 review fix) — region pairs whose seats
//       stand nearer than `generation_settle_spacing_tiles` (Chebyshev, columns
//       wrapping: `region_distance`'s metric), where at least one of the pair
//       was founded by the sim. The sim only appends to the region list, so the
//       sim-founded regions are exactly its last F entries, F being the three
//       spans' reported foundings (scheduled + Settle). The rule's own invariant,
//       so a non-zero count FAILS the run, like the cap. Pairs among the
//       settlement pass's own regions are printed beside it, not gated: that
//       pass spaces by its own `sep` (>= 3 tiles), not by this constant. A
//       SCHEDULED founding counts as sim-founded, and it too was placed by that
//       pass's `sep`; so the check holds for it only while the spacing is at or
//       under `sep` (the shipped 3 is). Measured: a spacing-10 build on seed 12
//       (sep 7) counts 4 such pairs. Negative control: counting one tile wider
//       than the spacing on a spacing-3 world fires (seeds 28/40: 672/1,291).
//
// A READING, NOT A GATE ON DENSITY. Nothing here asserts a centre count, a land
// share or a scale mix: those are what the rule is ruled against, and a harness
// that pinned them would be making that call. Rows that FAIL the run (exit 1):
// the instrument's own honesty check (a world with no settlement record, or no
// carved centre, would make every fit row vacuous), THE CAP (BL-1130 review
// fix) — the rule's own invariant on the saved sim record, not a density —
// THE SPACING (C10), THE COST MODEL ON THE SHIPPED WORLDS (C13), and THE
// PROVINCE ANCHOR (C7 `unanchored`, BL-1145). C16 reads where the nation
// garrisons stand against the lowest-id tile they used to (a reading).
//
//     * THE COST MODEL ON THE SHIPPED WORLDS (C13; BL-1150 review) —
//       province_partition_harness C2a/C2b, per body and like for like, on the
//       worlds the app ships rather than the harness's no-prehistory fixture:
//       on every body with rivers a step ACROSS a course (onto or off a course
//       tile sideways, the step the cost model charges since BL-1156, a river
//       divides its banks) is a province border more often than that body's
//       plain ground AND than a step ALONG the course (C2a; the along class is
//       the control that shares the centres-on-rivers confound), and on every
//       body with steep edges a steep edge beats plain ground (C2b). Steep is
//       |dh| at or above the 90th percentile of this world's adjacent-land |dh|
//       (the harness's nearest-rank index); plain is none of the others.
//       REPORTED beside it: BANKS — home-body course sites (one inflow, one
//       outflow) whose two banks lie in one province, how many provinces do
//       that, and the one doing it most. A C13 FAIL is a call against the
//       numbers, never a tolerance to widen.
//     * THE CONSERVATION LEDGER (C15, BL-1137 rebuild review): the ceiling the
//       urbanisation stream moved sums to exactly zero, and every region holds
//       0 <= farm-fed <= its farm-fed ceiling and industrial <= urban <=
//       population. And THE WORKS WIRING (C14, BL-1149 review): with the works
//       table loaded and held works employing, the span earned scale credit and
//       moved heads.
//
// READ-ONLY OVER src/world/*. It calls the world's own functions and nothing in
// src/ changes for it.
//
// ---------------------------------------------------------------------------
// THE WORLD — harness_params' app-order builder, first half
// ---------------------------------------------------------------------------
// `build_app_base_world` (harness_params.hpp): the parsed world_gen config and
// the works table handed to `make_hard_coded_world`, then setup_world's two
// world writes and load_economy's recipe pass — the app's own sequence up to
// the landscape search. The search and the settle are NOT run: every quantity
// read here (the centre set, the footprints, the road field, the carve's
// markets) is written by generation and is what the search starts from. The
// search lays firms and a road tier over it; it founds no centre.
//
// CELL. A region is an anchor, not a stored tile set; its extent is the
// Voronoi `nearest_region` reads (the SAME call the carve partitions its
// candidates with), taken on the FINAL region table (`world::gen_settlement`).
// A cell's LAND is its non-water tiles; its PLACEABLE ground is the tiles that
// pass `placement_rules::can_place_population_centre`, the carve's own gate —
// and the ground the sim measures (`region::urban_ground`), so the cap is read
// on the same tiles the rule was held to.
//
// Build:  bash tools/verify/build_lua_harness.sh centre_census
// Run:    ./build_gen/verify/centre_census.exe [--seeds 46,28] [--map DIR]
//         (from the repo root: it loads scripts/ and the seed library by
//         relative path). --map DIR writes roads_seed<S>.ppm, one pixel a
//         tile, in gen_step_costs' --roads-map colours: water dark blue, bare
//         land tan, a STREET (a road on a centre's tile) orange, Track brown,
//         Road red, Highway black; and centres_seed<S>.ppm, every centre by
//         origin: carved village orange, town red, city or larger dark red,
//         province anchor yellow, national coverage violet.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/hex_neighbors.hpp"
#include "world/placement_rules.hpp"
#include "world/era_minus_one.hpp"         // BL-1132: generation_settle_spacing_tiles
#include "world/population_generation.hpp"
#include "world/province.hpp"
#include "world/settlement.hpp"
#include "world/stockpile_budget.hpp"
#include "world/world.hpp"

#include <sol/sol.hpp>

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;

std::vector<uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

/// `region::industrial_heads` (BL-1137, industrial urbanisation) where the tree
/// carries it, -1 where it does not — so the census still builds on a tree
/// without the stream, which is where its BEFORE reading is taken.
template <typename R>
int64_t industrial_heads_of(const R& p)
{
    if constexpr (requires { p.industrial_heads; })
        return static_cast<int64_t>(p.industrial_heads);
    else
        return -1;
}

/// THE CONSERVATION LEDGER'S READS (BL-1137 rebuild review, NR-958), where the
/// tree carries them, so the same gate builds on a tree without the stream (it
/// holds trivially there) and on the stream's FIRST build (8cbdbc9d~1), whose
/// arithmetic it exists to catch:
///   carried  — the ceiling the stream MOVED onto a region: `capacity_carried`
///              where the tree carries it (+ in, - out, so it sums to zero), and
///              on the first build `industrial_heads` itself, which is exactly
///              what that build added to each destination's ceiling while no
///              source gave any up;
///   farm-fed ceiling — `region_farm_fed_ceiling` where the tree carries it; on
///              any other tree the works-aware farm ceiling (on the first build
///              its ceiling was K_farm + I, so K_farm + I - I).
template <typename R>
int64_t carried_of(const R& p)
{
    if constexpr (requires { p.capacity_carried; })
        return static_cast<int64_t>(p.capacity_carried);
    else if constexpr (requires { p.industrial_heads; })
        return static_cast<int64_t>(p.industrial_heads);
    else
        return 0;
}
template <typename R>
int64_t farm_fed_ceiling_of(const R& p)
{
    if constexpr (requires { p.capacity_carried; })
        return region_farm_fed_ceiling(p);
    else
        return region_carrying_capacity(p.farm_q, p.work_capacity_mod);
}

/// BL-1149 (scale credit from the works): the heads the works in @p mask employ.
/// Read off the registry where the tree carries `employed_heads_mask`, and on a
/// tree without it off scripts/works.lua's own `employs` column (the loaded Lua
/// global, row i+1 for bit i), so the census's BEFORE reading applies the same
/// table to the world that ignored it.
template <typename W>
int64_t employed_heads_of(const W& works, lua_state& lua, uint32_t mask)
{
    if constexpr (requires { works.employed_heads_mask(mask); })
        return works.employed_heads_mask(mask);
    else
    {
        sol::state& L = lua.state();
        const sol::optional<sol::table> rows = L["works"];
        if (!rows) return 0;
        int64_t heads = 0;
        for (int b = 0; b < 32; ++b)
        {
            if ((mask & (1u << b)) == 0) continue;
            const sol::optional<sol::table> row = (*rows)[b + 1];
            if (!row) continue;
            const sol::optional<int64_t> e = (*row)["employs"];
            if (e) heads += *e;
        }
        return heads;
    }
}

/// BL-1149 (the review fix): the Industrialisation span's scale credit, heads
/// moved and inert rounds off the report, where the tree records them; -1 where
/// it does not (a tree before the fix), so the census still builds there.
struct span_industry { int64_t from_scale = -1, moved = -1, inert_rounds = -1, within = -1; };
template <typename G>
span_industry span_industry_of(const G& rep)
{
    span_industry s;
    if constexpr (requires { rep.industrialisation_points_from_scale; rep.industrialisation_stream_moved;
                             rep.industrialisation_scale_inert_rounds; })
    {
        s.from_scale   = rep.industrialisation_points_from_scale;
        s.moved        = rep.industrialisation_stream_moved;
        s.inert_rounds = rep.industrialisation_scale_inert_rounds;
    }
    if constexpr (requires { rep.industrialisation_stream_within; })
        s.within = rep.industrialisation_stream_within;
    return s;
}

/// Spearman's rho over paired samples, ties at their mean rank. 0 on fewer
/// than two samples or a constant side.
double spearman(const std::vector<int64_t>& a, const std::vector<int64_t>& b)
{
    const std::size_t n = a.size();
    if (n < 2 || b.size() != n) return 0.0;
    const auto ranks = [n](const std::vector<int64_t>& v) {
        std::vector<std::size_t> idx(n);
        for (std::size_t i = 0; i < n; ++i) idx[i] = i;
        std::stable_sort(idx.begin(), idx.end(), [&v](std::size_t x, std::size_t y) { return v[x] < v[y]; });
        std::vector<double> r(n, 0.0);
        for (std::size_t i = 0; i < n;)
        {
            std::size_t j = i;
            while (j + 1 < n && v[idx[j + 1]] == v[idx[i]]) ++j;
            const double mean = (static_cast<double>(i) + static_cast<double>(j)) / 2.0 + 1.0;
            for (std::size_t k = i; k <= j; ++k) r[idx[k]] = mean;
            i = j + 1;
        }
        return r;
    };
    const std::vector<double> ra = ranks(a), rb = ranks(b);
    double ma = 0, mb = 0;
    for (std::size_t i = 0; i < n; ++i) { ma += ra[i]; mb += rb[i]; }
    ma /= static_cast<double>(n); mb /= static_cast<double>(n);
    double sab = 0, saa = 0, sbb = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        sab += (ra[i] - ma) * (rb[i] - mb);
        saa += (ra[i] - ma) * (ra[i] - ma);
        sbb += (rb[i] - mb) * (rb[i] - mb);
    }
    return (saa > 0 && sbb > 0) ? sab / std::sqrt(saa * sbb) : 0.0;
}

std::vector<uint32_t> parse_seed_list(const std::string& s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty())
            out.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

struct seed_record
{
    uint32_t seed = 0;
    double   build_s = 0.0;
    bool     ok = false;
    bool     cap_ok = true; ///< the sim record fits its final cells (BL-1130 review fix)
    /// BL-1132 review fix: the spacing holds for every sim-founded region.
    bool     spacing_ok = true;
    int      sim_founded = 0;        ///< the last F regions: the three spans' foundings
    int      close_pairs_sim = 0;    ///< seats nearer than the spacing, a sim-founded member
    int      close_pairs_pass = 0;   ///< ...both from the settlement pass (not gated)
    int      close_pairs_stacked = 0;///< of close_pairs_sim, two seats on ONE tile

    // The sim record.
    int     regions = 0, living = 0, standing = 0;
    int64_t sim_centres = 0, sim_urban = 0;
    int64_t sim_population = 0, sim_urban_max = 0;
    int64_t sim_industrial = -1; ///< -1: the tree carries no `region::industrial_heads`
    // THE CONSERVATION LEDGER (BL-1137 rebuild review; a FAIL row), over EVERY
    // region at the close, living or not.
    int64_t ledger_carried_sum = 0;  ///< sum of the ceiling the stream moved: exactly 0
    int     ledger_farm_over = 0;    ///< regions whose farm-fed heads pass their farm-fed ceiling
    int     ledger_farm_negative = 0;///< regions with more industrial heads than people
    int     ledger_ind_over_urban = 0;///< industrial heads over urban heads
    int     ledger_urban_over_pop = 0;///< urban heads over population
    bool    ledger_ok = true;
    // BL-1149 — THE WORKS AND THE POINTS (C14; readings, never gated).
    int64_t points_close = 0;          ///< industry points standing on regions at the close
    int     rho_regions = 0;           ///< regions with points or centres (the rho set)
    double  rho_points_urban = 0.0;    ///< Spearman(points, urban heads) over the rho set
    double  rho_points_employed = 0.0; ///< Spearman(points, employed heads) over the rho set
    int64_t heads_urban_centred = 0;   ///< urban heads on living regions standing centres
    int64_t heads_employed = 0;        ///< ...of them within what their works employ
    int64_t migrants = -1;             ///< industrial heads standing at the close (-1: no stream)
    int     receiving = 0;             ///< regions holding any
    double  migrants_top_decile = 0.0; ///< share held by the top tenth of receiving regions
    double  migrants_to_worked = 0.0;  ///< share on the top tenth of centred regions by employed heads
    int64_t open_work_left = 0;        ///< jobs the works of living centred regions leave unfilled at the close
    double  migrants_on_open = 0.0;    ///< share of standing migrants on regions with open work left
    // BL-1149 review fix — THE WIRING, ASSERTED (FAIL rows). With the works
    // table loaded, a world whose held works employ anyone must have earned
    // scale credit and moved heads; and the span must never have run inert.
    int64_t span_from_scale = -1;      ///< the span's scale credit (-1: not recorded by this tree)
    int64_t span_moved = -1;           ///< heads the stream moved (-1: not recorded)
    int64_t span_within = -1;          ///< ...of which a centre took from its own countryside
    int64_t span_inert_rounds = -1;    ///< rounds run with no works table (-1: not recorded)
    int64_t held_employed = 0;         ///< heads the works of HELD regions employ
    int     works_rows = 0;            ///< rows in the loaded works table
    bool    wiring_ok = true;
    int     max_region_centres = 0, at_limit = 0;
    int     region_centre_hist[6] = {}; // 0, 1, 2-4, 5-9, 10-19, 20+

    // The body.
    int land = 0, placeable = 0;
    int cell_land_median = 0, cell_land_max = 0;

    // The carve and the two region-blind passes.
    int carved = 0, dropped_built_out = 0, dropped_no_tile = 0;
    int anchors = 0, coverage = 0;

    // Density.
    int centres = 0, centre_tiles = 0, urban_tiles = 0;
    int scale_count[5] = {};

    // Fit.
    int spilled = 0;
    int overfull_regions = 0;       ///< carved footprint sum > cell land
    int overfull_single = 0;        ///< ...of which the region carries ONE carved centre
    int64_t overfull_excess = 0;    ///< sum over those regions of (footprint - cell land)
    int overcount_regions = 0;      ///< carved count > cell placeable tiles
    int placeless_regions = 0;      ///< living, standing centres, cell with no placeable tile
    int placeless_water = 0;        ///< ...whose anchor tile is water (a shore or lake founding)
    int placeless_stacked = 0;      ///< ...whose anchor tile a lower-index region already holds
    int sim_over_cell = 0;          ///< living, centres > what its final cell's ground holds (FAIL)
    int groundless_regions = 0;     ///< living, cell with no standable ground
    int groundless_standing = 0;    ///< ...whose sim record still carries a centre (FAIL)
    int paved_across = 0;           ///< urban tiles no centre of their own cell touches
    int cells_paved_into = 0;       ///< distinct cells those tiles lie in

    // The stockpile (BL-1042) the carve's slots receive, and where the rest went.
    int64_t stock_total = 0, stock_to_centres = 0;
    int64_t stock_unspent[stockpile_unspent_reason_count] = {};
    /// The `no_carved_centre` residual by WHY the region carved nothing: its
    /// people are gone (population 0), its cell holds no standable ground, its
    /// centres went to zero on ground it still has, or it stands centres yet
    /// the carve gave it no slot.
    int64_t resid_emptied = 0, resid_groundless = 0, resid_no_centre = 0, resid_other = 0;

    // The history, per span: Empires (the report's prehistory), Exploration,
    // Industrialisation.
    int64_t battles[3] = {}, conquests[3] = {}, foundings[3] = {};
    int64_t army_close = 0;  ///< men under arms at the close, summed over living regions
    int     garrisons  = 0;  ///< living regions standing any army at the close

    // Where the density is.
    int single_tile_cells = 0;     ///< living regions whose cell holds exactly one land tile
    /// BL-1132: living regions by cell land — 1, 2-4, 5-9, 10-24, 25+ tiles
    /// (0 land, the groundless, is C4's row).
    int cell_land_hist[5] = {};
    int carved_in_single = 0;      ///< carved centres whose source region's cell is one land tile

    // What follows.
    // The provinces (BL-1133): the home body's LAND provinces.
    int land_provinces = 0;      ///< every land province on the body
    int seeded_provinces = 0;    ///< ...holding at least one seed (non-anchor) centre
    int multi_centre = 0;        ///< ...holding more than one centre of any kind
    int anchor_only = 0;         ///< ...holding an anchor founding and no seed centre
    int unanchored = 0;          ///< ...holding no centre at all (the invariant says 0)
    int anchor_only_unsettled = 0; ///< anchor-only provinces WHOLLY on ground the colonisation never settled
    /// BL-1150 (a centre's fill crosses the settled line): WHY each anchor-only
    /// province — an uncentred island — is left, read off its land neighbours
    /// outside it. `own_nation`: it touches land of its OWN nation, which the
    /// fill should have reached (0 once the settled line is no lock; the
    /// settlement lock's islands, before). `unowned`: no nation holds it.
    /// `nation_lock`: its only land neighbours are another nation's.
    /// `water`: it touches no other land at all.
    int why_own_nation = 0, why_unowned = 0, why_nation_lock = 0, why_water = 0;
    std::vector<int> anchor_only_sizes; ///< land tiles per anchor-only province
    std::vector<int> sizes;      ///< land tiles per province
    std::vector<int> sizes_by_anchor[5]; ///< ...split by the anchor's scale (1-5), SEEDED provinces only (BL-1145)
    /// C16 (BL-1145, a province's readers take its anchor): the nation
    /// garrisons over every body, read against the rule they replaced — the
    /// province's lowest-id tile. `moved`: the garrison stands off that tile.
    /// `*_unsettled`: the tile is ground the colonisation never settled.
    int nat_garrisons = 0, garrisons_at_capital = 0, garrisons_moved = 0;
    int garrison_old_unsettled = 0, garrison_new_unsettled = 0;
    int garrison_on_centre = 0;  ///< ...whose post hosts a centre (else the centreless fallback)
    int garrison_in_water = 0;   ///< ...in a water province (no centre: the lowest-id fallback)
    int garrison_left_wild = 0;  ///< lowest tile never-settled, post settled
    int garrison_into_wild = 0;  ///< lowest tile settled, post never-settled
    /// The partition's own ledger, read off a rebuild ON A COPY from the stored
    /// seed (province_partition_harness P6: the rebuild IS the shipped
    /// partition): uncentred islands, and centre singletons absorbed.
    province_absorption_stats part_stats;

    int road_tiles = 0, street_tiles = 0;
    int markets = 0;

    // C13, the cost model on this shipped world (BL-1150 review).
    struct edge_class { int64_t n = 0, border = 0, on_line = 0; };
    edge_class c13_river, c13_cross, c13_steep, c13_plain; ///< the HOME body's land edges
    /// BL-1156: home-body course tiles with one inflow and one outflow whose two
    /// banks (the land beside the course on either side of the flow) are both
    /// readable, and of those how many have the two banks in ONE province.
    int64_t c13_bank_sites = 0, c13_banks_joined = 0;
    int     c13_joining_provinces = 0;   ///< distinct provinces joining two banks somewhere
    int     c13_joining_max_sites = 0;   ///< the most sites one province joins
    uint32_t c13_joining_max_id = 0;     ///< ...that province
    int     c13_joining_max_tiles = 0;   ///< ...and its land tiles
    /// C14: water provinces on every body (BL-1156 review: the river cost is a
    /// land rule, so these must not move with it), and a digest of their tiles.
    int     water_coast = 0, water_lake = 0, water_ocean = 0;
    uint64_t water_digest = 1469598103934665603ull;
    float c13_p90 = 0.0f;                       ///< this world's p90 of adjacent-land |dh|
    int   c13_river_bodies = 0, c13_steep_bodies = 0; ///< bodies carrying each class
    std::vector<std::string> c13_fail;          ///< one line per body failing C2a or C2b
};

/// The centre map: every centre by origin, so a reader can see what the density
/// is made of. Water dark blue, bare land tan; a CARVED centre by scale —
/// village orange, town red, city and larger dark red; a province anchor yellow;
/// a national-coverage founding violet.
void write_centre_map(const world& w, entity_id body, const std::string& path)
{
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end()) return;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0) return;
    std::vector<unsigned char> px(static_cast<std::size_t>(gw) * gh * 3, 0);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        unsigned char r = 200, g = 188, b = 150;
        if (is_water(tc.substrate)) { r = 28; g = 48; b = 88; }
        const std::size_t i = (static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x) * 3;
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        const auto pit = w.population_centres.find(cid);
        if (tit == w.tiles.end() || pit == w.population_centres.end()) continue;
        const tile_component& tc = tit->second;
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        unsigned char r, g, b;
        if (w.gen_carve_centres.count(cid) != 0)
        {
            const int s = pit->second.scale;
            if (s <= 1)      { r = 235; g = 140; b = 30; }
            else if (s == 2) { r = 210; g = 40;  b = 30; }
            else             { r = 110; g = 0;   b = 10; }
        }
        else if (pit->second.province_anchor) { r = 250; g = 225; b = 40; }
        else                                  { r = 150; g = 60;  b = 200; }
        const std::size_t i = (static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x) * 3;
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
    if (FILE* f = std::fopen(path.c_str(), "wb"))
    {
        std::fprintf(f, "P6\n%d %d\n255\n", gw, gh);
        std::fwrite(px.data(), 1, px.size(), f);
        std::fclose(f);
    }
}

void write_road_map(const world& w, entity_id body, const std::set<entity_id>& centre_tiles,
                    const std::string& path)
{
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end()) return;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    if (gw <= 0 || gh <= 0) return;
    std::vector<unsigned char> px(static_cast<std::size_t>(gw) * gh * 3, 0);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        unsigned char r = 200, g = 188, b = 150;                    // bare land
        if (is_water(tc.substrate)) { r = 28; g = 48; b = 88; }     // water
        if (tc.road_level > 0)
        {
            if (centre_tiles.count(tid) != 0) { r = 235; g = 140; b = 30; } // street
            else if (tc.road_level == 1)      { r = 95;  g = 60;  b = 25; } // Track
            else if (tc.road_level == 2)      { r = 200; g = 25;  b = 25; } // Road
            else                              { r = 0;   g = 0;   b = 0;  } // Highway
        }
        const std::size_t i = (static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x) * 3;
        px[i] = r; px[i + 1] = g; px[i + 2] = b;
    }
    if (FILE* f = std::fopen(path.c_str(), "wb"))
    {
        std::fprintf(f, "P6\n%d %d\n255\n", gw, gh);
        std::fwrite(px.data(), 1, px.size(), f);
        std::fclose(f);
    }
}

seed_record run_seed(lua_state& lua, uint32_t seed, const std::string& map_dir)
{
    seed_record r;
    r.seed = seed;

    world_params params{};
    params.seed = seed;
    app_start_world out;
    const clk::time_point t0 = clk::now();
    build_app_base_world(lua, params, out);
    r.build_s = std::chrono::duration<double>(clk::now() - t0).count();
    r.battles[0]   = out.report.prehistory_battles;
    r.conquests[0] = out.report.prehistory_conquests;
    r.foundings[0] = out.report.prehistory_foundings;
    r.battles[1]   = out.report.exploration_battles;
    r.conquests[1] = out.report.exploration_conquests;
    r.foundings[1] = out.report.exploration_foundings;
    r.battles[2]   = out.report.industrialisation_battles;
    r.conquests[2] = out.report.industrialisation_conquests;
    r.foundings[2] = out.report.industrialisation_foundings;

    const world& w = out.w;
    const entity_id body = w.home_body;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || !w.gen_settlement)
        return r;
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const settlement_state& ss = *w.gen_settlement;

    // --- the spacing itself (BL-1132 review fix) ---------------------------
    // Bucketed by anchor tile, so the pair scan is (2D - 1)^2 tiles a region
    // rather than every pair: a pair nearer than D shares a D - 1 neighbourhood.
    {
        const int n = static_cast<int>(ss.regions.size());
        const int64_t f = r.foundings[0] + r.foundings[1] + r.foundings[2];
        const int first_sim = static_cast<int>(std::clamp<int64_t>(n - f, 0, n));
        r.sim_founded = n - first_sim;
        const int reach = std::max(1, generation_settle_spacing_tiles) - 1;
        std::vector<std::vector<int>> at(static_cast<std::size_t>(std::max(0, gw * gh)));
        const auto wrapc = [gw](int c) { return gw > 0 ? ((c % gw) + gw) % gw : c; };
        for (int i = 0; i < n; ++i)
        {
            const region& a = ss.regions[static_cast<std::size_t>(i)];
            for (int dr = -reach; dr <= reach; ++dr)
            {
                const int rr = a.row + dr;
                if (rr < 0 || rr >= gh) continue;
                for (int dc = -reach; dc <= reach; ++dc)
                {
                    const int cc = wrapc(a.col + dc);
                    if (cc < 0 || cc >= gw) continue;
                    for (const int j : at[static_cast<std::size_t>(rr * gw + cc)])
                    {
                        const bool sim = i >= first_sim || j >= first_sim;
                        if (sim)
                        {
                            ++r.close_pairs_sim;
                            if (dr == 0 && dc == 0) ++r.close_pairs_stacked;
                        }
                        else
                            ++r.close_pairs_pass;
                    }
                }
            }
            const int ar = a.row, ac = wrapc(a.col);
            if (ar >= 0 && ar < gh && ac >= 0 && ac < gw)
                at[static_cast<std::size_t>(ar * gw + ac)].push_back(i);
        }
        r.spacing_ok = r.close_pairs_sim == 0;
    }

    // --- BL-1149: the works and the points ----------------------------------
    {
        const span_industry si = span_industry_of(out.report);
        r.span_from_scale   = si.from_scale;
        r.span_moved        = si.moved;
        r.span_inert_rounds = si.inert_rounds;
        r.span_within       = si.within;
        r.works_rows        = static_cast<int>(out.works.size());
        for (const region& p : ss.regions)
            if (p.nation >= 0 && p.population > 0)
                r.held_employed += employed_heads_of(out.works, lua, p.works_built);
        // Recorded by this tree: the table is loaded and some held work employs,
        // so the span must have earned scale credit, moved heads, and never run
        // inert. A world whose call site dropped the table fails here.
        if (si.from_scale >= 0 && r.works_rows > 0 && r.held_employed > 0)
            r.wiring_ok = si.from_scale > 0 && si.moved > 0 && si.inert_rounds == 0;
    }
    {
        std::vector<int64_t> pts, urb, emp;
        std::vector<std::pair<int64_t, int64_t>> by_employed; // (employed, migrants) on centred regions
        std::vector<int64_t> received;
        int64_t mig_total = 0;
        bool has_stream = false;
        for (const region& p : ss.regions)
        {
            r.points_close += p.industry_points;
            const int64_t e = employed_heads_of(out.works, lua, p.works_built);
            const int64_t ind = industrial_heads_of(p);
            if (ind >= 0) has_stream = true;
            if (p.industry_points > 0 || p.centres > 0)
            {
                pts.push_back(p.industry_points);
                urb.push_back(p.urban_population);
                emp.push_back(e);
            }
            if (p.population > 0 && p.centres > 0)
            {
                const int64_t open = e > p.urban_population ? e - p.urban_population : 0;
                r.open_work_left += open;
                if (open > 0 && ind > 0) r.migrants_on_open += static_cast<double>(ind);
                r.heads_urban_centred += p.urban_population;
                r.heads_employed += std::min(p.urban_population, e);
                by_employed.push_back({ e, std::max<int64_t>(ind, 0) });
            }
            if (ind > 0) { received.push_back(ind); mig_total += ind; }
        }
        r.rho_regions = static_cast<int>(pts.size());
        r.rho_points_urban = spearman(pts, urb);
        r.rho_points_employed = spearman(pts, emp);
        if (has_stream)
        {
            r.migrants = mig_total;
            r.receiving = static_cast<int>(received.size());
            std::sort(received.begin(), received.end(), std::greater<int64_t>());
            const std::size_t top = (received.size() + 9) / 10;
            int64_t top_sum = 0;
            for (std::size_t k = 0; k < top && k < received.size(); ++k) top_sum += received[k];
            r.migrants_top_decile = mig_total > 0 ? static_cast<double>(top_sum) / mig_total : 0.0;
            std::stable_sort(by_employed.begin(), by_employed.end(),
                             [](const std::pair<int64_t, int64_t>& a, const std::pair<int64_t, int64_t>& b)
                             { return a.first > b.first; });
            const std::size_t wtop = (by_employed.size() + 9) / 10;
            int64_t worked = 0;
            for (std::size_t k = 0; k < wtop && k < by_employed.size(); ++k) worked += by_employed[k].second;
            r.migrants_to_worked = mig_total > 0 ? static_cast<double>(worked) / mig_total : 0.0;
            r.migrants_on_open   = mig_total > 0 ? r.migrants_on_open / static_cast<double>(mig_total) : 0.0;
        }
    }

    // --- the sim record -----------------------------------------------------
    r.regions = static_cast<int>(ss.regions.size());
    for (const region& p : ss.regions)
    {
        // THE LEDGER: people and ceiling conserved, every region.
        {
            const int64_t ind = std::max<int64_t>(industrial_heads_of(p), 0);
            const int64_t farm_fed = p.population - ind;
            r.ledger_carried_sum += carried_of(p);
            if (farm_fed < 0) ++r.ledger_farm_negative;
            else if (farm_fed > farm_fed_ceiling_of(p)) ++r.ledger_farm_over;
            if (ind > p.urban_population) ++r.ledger_ind_over_urban;
            if (p.urban_population > p.population) ++r.ledger_urban_over_pop;
        }
        if (p.population > 0) ++r.living;
        if (p.population > 0 && p.centres > 0) ++r.standing;
        if (p.population > 0)
        {
            r.sim_centres += p.centres;
            r.sim_urban += p.urban_population;
            r.sim_population += p.population;
            r.sim_urban_max = std::max(r.sim_urban_max, p.urban_population);
            r.army_close += p.army_stock;
            if (p.army_stock > 0) ++r.garrisons;
            const int64_t ind = industrial_heads_of(p);
            if (ind >= 0) r.sim_industrial = std::max<int64_t>(r.sim_industrial, 0) + ind;
        }
        r.max_region_centres = std::max(r.max_region_centres, p.centres);
        if (p.centres >= region_centre_limit) ++r.at_limit;
        const int c = p.population > 0 ? p.centres : 0;
        const int b = c <= 0 ? 0 : c == 1 ? 1 : c <= 4 ? 2 : c <= 9 ? 3 : c <= 19 ? 4 : 5;
        ++r.region_centre_hist[b];
    }

    // --- the body, in raster order, and each tile's cell ---------------------
    std::vector<const tile_component*> grid(static_cast<std::size_t>(gw) * gh, nullptr);
    std::map<entity_id, int> tile_raster;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body) continue;
        const int idx = tc.grid_y * gw + tc.grid_x;
        if (idx < 0 || idx >= gw * gh) continue;
        grid[static_cast<std::size_t>(idx)] = &tc;
        tile_raster[tid] = idx;
    }
    std::vector<int> cell_land(ss.regions.size(), 0), cell_place(ss.regions.size(), 0);
    std::vector<int> cell_of(static_cast<std::size_t>(gw) * gh, -1);
    for (int idx = 0; idx < gw * gh; ++idx)
    {
        const tile_component* tc = grid[static_cast<std::size_t>(idx)];
        if (tc == nullptr || is_water(tc->substrate)) continue;
        ++r.land;
        const bool place = placement_rules::can_place_population_centre(*tc);
        if (place) ++r.placeable;
        const int ri = nearest_region(ss, idx % gw, idx / gw, gw);
        if (ri < 0 || ri >= static_cast<int>(ss.regions.size())) continue;
        cell_of[static_cast<std::size_t>(idx)] = ri;
        ++cell_land[static_cast<std::size_t>(ri)];
        if (place) ++cell_place[static_cast<std::size_t>(ri)];
    }
    {
        std::vector<int> sorted;
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
            if (ss.regions[i].population > 0)
            {
                sorted.push_back(cell_land[i]);
                if (cell_land[i] == 1) ++r.single_tile_cells;
                const int cl = cell_land[i];
                if (cl >= 1)
                    ++r.cell_land_hist[cl == 1 ? 0 : cl <= 4 ? 1 : cl <= 9 ? 2 : cl <= 24 ? 3 : 4];
            }
        std::sort(sorted.begin(), sorted.end());
        if (!sorted.empty())
        {
            r.cell_land_median = sorted[sorted.size() / 2];
            r.cell_land_max    = sorted.back();
        }
    }

    // --- every centre on the body ---------------------------------------------
    std::set<entity_id> centre_tiles;
    std::vector<int64_t> region_footprint(ss.regions.size(), 0);
    std::vector<int>     region_carved(ss.regions.size(), 0);
    for (const auto& [cid, tid] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end() || tit->second.body != body) continue;
        const auto pit = w.population_centres.find(cid);
        if (pit == w.population_centres.end()) continue;
        ++r.centres;
        centre_tiles.insert(tid);
        const int scale = std::clamp(pit->second.scale, 1, 5);
        ++r.scale_count[scale - 1];

        const auto cit = w.gen_carve_centres.find(cid);
        if (cit == w.gen_carve_centres.end())
        {
            if (pit->second.province_anchor) ++r.anchors; else ++r.coverage;
            continue;
        }
        ++r.carved;
        const int src = cit->second.region;
        const int stood = nearest_region(ss, tit->second.grid_x, tit->second.grid_y, gw);
        if (stood != src) ++r.spilled;
        if (src >= 0 && src < static_cast<int>(ss.regions.size()))
        {
            region_footprint[static_cast<std::size_t>(src)] += k_urban_footprint_tiles[scale - 1];
            ++region_carved[static_cast<std::size_t>(src)];
            if (cell_land[static_cast<std::size_t>(src)] == 1) ++r.carved_in_single;
        }
    }
    r.centre_tiles = static_cast<int>(centre_tiles.size());
    for (const carve_dropped_slot& d : w.gen_carve_dropped)
    {
        if (d.reason == carve_drop_reason::body_built_out) ++r.dropped_built_out;
        else ++r.dropped_no_tile;
    }
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        if (region_footprint[i] > cell_land[i])
        {
            ++r.overfull_regions;
            if (region_carved[i] == 1) ++r.overfull_single;
            r.overfull_excess += region_footprint[i] - cell_land[i];
        }
        if (region_carved[i] > cell_place[i]) ++r.overcount_regions;
        const region& p = ss.regions[i];
        if (p.population > 0 && p.centres > 0 && cell_place[i] == 0)
        {
            ++r.placeless_regions;
            const int a = (p.row >= 0 && p.row < gh) ? p.row * gw + ((p.col % gw) + gw) % gw : -1;
            const tile_component* at = (a >= 0) ? grid[static_cast<std::size_t>(a)] : nullptr;
            bool stacked = false;
            for (std::size_t j = 0; j < i && !stacked; ++j)
                stacked = ss.regions[j].col == p.col && ss.regions[j].row == p.row;
            if (stacked) ++r.placeless_stacked;
            else if (at != nullptr && is_water(at->substrate)) ++r.placeless_water;
        }
        // THE CAP, ASSERTED (BL-1130 review fix; a FAIL row): the sim record
        // itself against the FINAL partition's standable ground, by the sim's
        // own rule -- no living region carries more centres than its cell
        // holds, and none on a cell with no ground carries one.
        // BL-1141 (a region deepens into one place) is NOT re-asserted here:
        // "at most one centre" is `region_centres_fit`'s own clamp, so a row
        // counting regions over one is true by construction and proves nothing
        // (the centres cold review, 2026-09-26). What this row does test is the
        // sim's ground against the census's own re-measure of the final cells.
        if (p.population > 0 && p.centres > 0
            && region_centres_fit(p.centres, cell_place[i]) < p.centres)
            ++r.sim_over_cell;
        if (p.population > 0 && cell_place[i] == 0)
        {
            ++r.groundless_regions;
            if (p.centres > 0) ++r.groundless_standing;
        }
    }

    // PAVED ACROSS A CELL EDGE (BL-1130 review fix): an urban tile that no
    // centre of its own cell stands on or beside was paved by a centre in a
    // neighbouring cell. A lower bound (a tile paved across an edge next to
    // one of its own cell's centres is not counted); zero once footprints stop
    // at the cell.
    {
        std::vector<char> hosts(static_cast<std::size_t>(gw) * gh, 0);
        for (const auto& [cid, tid] : w.population_centre_tile)
        {
            const auto rit = tile_raster.find(tid);
            if (rit != tile_raster.end()) hosts[static_cast<std::size_t>(rit->second)] = 1;
        }
        std::set<int> into;
        for (const auto& [tid, lu] : w.land_use)
        {
            if (lu.use != land_use_component::type::urban) continue;
            const auto rit = tile_raster.find(tid);
            if (rit == tile_raster.end()) continue;
            const int idx = rit->second;
            const int c = cell_of[static_cast<std::size_t>(idx)];
            if (c < 0 || hosts[static_cast<std::size_t>(idx)]) continue;
            bool touched = false;
            for (int sd = 0; sd < 6 && !touched; ++sd)
            {
                const auto [nx_raw, ny] = hex_neighbors::neighbour(idx % gw, idx / gw, sd);
                if (ny < 0 || ny >= gh) continue;
                const int nx = ((nx_raw % gw) + gw) % gw;
                const int ni = ny * gw + nx;
                touched = hosts[static_cast<std::size_t>(ni)] && cell_of[static_cast<std::size_t>(ni)] == c;
            }
            if (!touched) { ++r.paved_across; into.insert(c); }
        }
        r.cells_paved_into = static_cast<int>(into.size());
    }

    // THE STOCKPILE the carve's slots receive (BL-1042), and every reason the
    // rest went unspent -- a region earning points the carve gives no slot
    // lands in the `no_carved_centre` residual.
    {
        const stockpile_budget sb = build_stockpile_budget(w);
        r.stock_total      = sb.points_total;
        r.stock_to_centres = sb.points_to_centres;
        for (int k = 0; k < stockpile_unspent_reason_count; ++k)
            r.stock_unspent[k] = sb.unspent[static_cast<std::size_t>(k)];

        std::vector<char> slotted(ss.regions.size(), 0);
        for (const auto& [cid, slot] : w.gen_carve_centres)
            if (slot.region >= 0 && static_cast<std::size_t>(slot.region) < slotted.size())
                slotted[static_cast<std::size_t>(slot.region)] = 1;
        for (const carve_dropped_slot& d : w.gen_carve_dropped)
            if (d.region >= 0 && static_cast<std::size_t>(d.region) < slotted.size())
                slotted[static_cast<std::size_t>(d.region)] = 1;
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
        {
            const region& p = ss.regions[i];
            if (p.industry_points <= 0 || slotted[i] || p.centres_razed > 0) continue;
            if (p.population <= 0)          r.resid_emptied    += p.industry_points;
            else if (cell_place[i] == 0)    r.resid_groundless += p.industry_points;
            else if (p.centres <= 0)        r.resid_no_centre  += p.industry_points;
            else                            r.resid_other      += p.industry_points;
        }
    }

    // --- urban ground, roads, markets -----------------------------------------
    for (const auto& [tid, lu] : w.land_use)
    {
        if (lu.use != land_use_component::type::urban) continue;
        const auto tit = w.tiles.find(tid);
        if (tit != w.tiles.end() && tit->second.body == body) ++r.urban_tiles;
    }
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.road_level == 0) continue;
        ++r.road_tiles;
        if (centre_tiles.count(tid) != 0) ++r.street_tiles;
    }
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body) ++r.markets;

    // --- the provinces (BL-1133) --------------------------------------------
    {
        std::vector<entity_id> id_at(static_cast<std::size_t>(gw) * gh, null_entity);
        for (const auto& [tid, idx] : tile_raster) id_at[static_cast<std::size_t>(idx)] = tid;
        std::map<entity_id, int> scale_on_tile, seeds_on_tile, centres_on_tile;
        for (const auto& [cid, tid] : w.population_centre_tile)
        {
            const auto pit = w.population_centres.find(cid);
            if (pit == w.population_centres.end()) continue;
            scale_on_tile[tid] += pit->second.scale;
            ++centres_on_tile[tid];
            if (!pit->second.province_anchor) ++seeds_on_tile[tid];
        }
        for (const province& pr : w.provinces.provinces)
        {
            if (pr.body != body || province_kind_of(w, pr) != province_kind::land) continue;
            ++r.land_provinces;
            int centres_here = 0, seeds_here = 0, anchor_scale = 0;
            for (const entity_id t : pr.tiles) // ascending: strictly-greater keeps the lowest id
            {
                const auto cit = centres_on_tile.find(t);
                if (cit == centres_on_tile.end()) continue;
                centres_here += cit->second;
                const auto sit = seeds_on_tile.find(t);
                if (sit != seeds_on_tile.end()) seeds_here += sit->second;
                const int s = scale_on_tile[t];
                if (s > anchor_scale) anchor_scale = s;
            }
            if (seeds_here > 0) ++r.seeded_provinces;
            if (centres_here > 1) ++r.multi_centre;
            if (centres_here > 0 && seeds_here == 0)
            {
                ++r.anchor_only;
                r.anchor_only_sizes.push_back(static_cast<int>(pr.tiles.size()));
                // WHOLLY never-settled (BL-1150: a province may now span the
                // line, so its first tile no longer speaks for all of it).
                bool wholly_unsettled = true;
                for (const entity_id t : pr.tiles)
                    if (w.tile_settled.count(t) != 0) { wholly_unsettled = false; break; }
                if (wholly_unsettled) ++r.anchor_only_unsettled;

                // WHY the island is left (BL-1150), off its outside land neighbours.
                const auto nat_of = [&](entity_id t) {
                    const auto it = w.tile_to_nation.find(t);
                    return it == w.tile_to_nation.end() ? null_entity : it->second;
                };
                const entity_id own = nat_of(pr.tiles.front());
                const std::set<entity_id> mine(pr.tiles.begin(), pr.tiles.end());
                bool same = false, foreign = false;
                for (const entity_id t : pr.tiles)
                {
                    const auto rit = tile_raster.find(t);
                    if (rit == tile_raster.end()) continue;
                    const int idx = rit->second;
                    for (int sd = 0; sd < 6; ++sd)
                    {
                        const auto [nx_raw, ny] = hex_neighbors::neighbour(idx % gw, idx / gw, sd);
                        if (ny < 0 || ny >= gh) continue;
                        const int ni = ny * gw + ((nx_raw % gw) + gw) % gw;
                        const entity_id nb = id_at[static_cast<std::size_t>(ni)];
                        if (nb == null_entity || mine.count(nb) != 0) continue;
                        const tile_component* ntc = grid[static_cast<std::size_t>(ni)];
                        if (ntc == nullptr || is_water(ntc->substrate)) continue;
                        if (nat_of(nb) == own) same = true; else foreign = true;
                    }
                }
                if (same)                     ++r.why_own_nation;
                else if (own == null_entity)  ++r.why_unowned;
                else if (foreign)             ++r.why_nation_lock;
                else                          ++r.why_water;
            }
            if (centres_here == 0) { ++r.unanchored; continue; }
            const int n = static_cast<int>(pr.tiles.size());
            r.sizes.push_back(n);
            // BL-1145: an anchor-only province (an uncentred island, C7b) is
            // NOT a village's ground — its scale-1 anchor was founded to hold
            // it, not grown from — so it is read in C7b and kept out of C8's
            // village column, which reads the fill's ordering by scale.
            if (seeds_here == 0) continue;
            r.sizes_by_anchor[std::clamp(anchor_scale, 1, 5) - 1].push_back(n);
        }

        // --- C16 the garrisons' posts (BL-1145) -------------------------------
        // The home body only, like every province row here: `tile_settled` is
        // the home body's colonisation record.
        for (const auto& [uid, u] : w.units)
        {
            const auto nit = w.nations.find(u.owner);
            if (nit == w.nations.end() || u.position == null_entity) continue;
            const auto ptit = w.tiles.find(u.position);
            if (ptit == w.tiles.end() || ptit->second.body != body) continue;
            const province* pr = w.provinces.find(w.provinces.province_of(u.position));
            if (pr == nullptr || pr->tiles.empty()) continue;
            ++r.nat_garrisons;
            if (province_kind_of(w, *pr) != province_kind::land) ++r.garrison_in_water;
            if (u.position == nit->second.capital_tile) ++r.garrisons_at_capital;
            if (centres_on_tile.count(u.position) != 0) ++r.garrison_on_centre;
            const entity_id old_post = pr->tiles.front();
            if (u.position != old_post) ++r.garrisons_moved;
            const bool old_wild = w.tile_settled.count(old_post) == 0;
            const bool new_wild = w.tile_settled.count(u.position) == 0;
            if (old_wild) ++r.garrison_old_unsettled;
            if (new_wild) ++r.garrison_new_unsettled;
            if (old_wild && !new_wild) ++r.garrison_left_wild;
            if (!old_wild && new_wild) ++r.garrison_into_wild;
        }
        world wc = w; // read-only over the census world: the rebuild runs on a copy
        build_province_partition(wc, w.provinces.seed, &r.part_stats);
    }

    // --- C14 the water provinces (BL-1156 review) ---------------------------
    // Counted per domain over every body, and folded (FNV-1a over each water
    // province's id and tile list, in the partition's ascending order) so a
    // re-partition of the same count is visible too.
    for (const province& pr : w.provinces.provinces)
    {
        const province_kind k = province_kind_of(w, pr);
        if (k == province_kind::land) continue;
        if (k == province_kind::open_ocean) ++r.water_ocean;
        else if (is_lake(w.tiles.at(pr.tiles.front()).substrate)) ++r.water_lake;
        else ++r.water_coast;
        const auto mix = [&](uint64_t v) { r.water_digest = (r.water_digest ^ v) * 1099511628211ull; };
        mix(pr.id);
        for (const entity_id t : pr.tiles) mix(t);
    }

    // --- C13 the cost model on the shipped world (BL-1150 review) ----------
    // province_partition_harness C2a/C2b's classes, re-derived here from the
    // tiles: every adjacent pair of land tiles on one body, once, from its
    // lower id (the harness's collect_land_edges).
    {
        std::map<entity_id, std::pair<int, int>>    bdims;
        std::map<entity_id, std::vector<entity_id>> rasters;
        for (const auto& [bid, bc] : w.bodies)
            if (bc.grid_width > 0 && bc.grid_height > 0)
            {
                bdims[bid] = { bc.grid_width, bc.grid_height };
                rasters[bid].assign(static_cast<std::size_t>(bc.grid_width) * bc.grid_height,
                                    null_entity);
            }
        for (const auto& [tid, tc] : w.tiles)
        {
            const auto rit = rasters.find(tc.body);
            if (rit == rasters.end()) continue;
            const auto& d = bdims.at(tc.body);
            if (tc.grid_x < 0 || tc.grid_x >= d.first || tc.grid_y < 0 || tc.grid_y >= d.second)
                continue;
            rit->second[static_cast<std::size_t>(tc.grid_y) * d.first + tc.grid_x] = tid;
        }
        // `river`: ALONG a course (the river bit on the step). `cross`: a step
        // onto or off a course sideways — one end a course tile, the step not
        // along the flow. BL-1156 (a river divides its banks; Ben, 2026-10-01,
        // NR-962 B): the crossing is what the cost model charges, so C2a
        // asserts the CROSSING class; the along class is reported.
        struct land_edge { entity_id body; float dh; bool river, cross, border, on_line; };
        std::vector<land_edge> edges;
        for (const auto& [bid, raster] : rasters) // ascending body id
        {
            const int bw = bdims.at(bid).first, bh = bdims.at(bid).second;
            for (const entity_id t : raster)
            {
                if (t == null_entity) continue;
                const tile_component& tc = w.tiles.at(t);
                if (is_water(tc.substrate)) continue;
                for (int s = 0; s < 6; ++s)
                {
                    const auto [nx_raw, ny] = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, s);
                    if (ny < 0 || ny >= bh) continue;
                    const int nx = ((nx_raw % bw) + bw) % bw;
                    const entity_id n = raster[static_cast<std::size_t>(ny) * bw + nx];
                    if (n == null_entity || n <= t) continue;
                    const tile_component& nt = w.tiles.at(n);
                    if (is_water(nt.substrate)) continue;
                    land_edge e;
                    e.body    = bid;
                    e.dh      = std::fabs(tc.height - nt.height);
                    e.river   = ((tc.river_edges >> s) & 1u) != 0u
                                || ((nt.river_edges >> ((s + 3) % 6)) & 1u) != 0u;
                    e.cross   = !e.river && (tc.river_edges != 0 || nt.river_edges != 0);
                    e.border  = w.provinces.province_of(t) != w.provinces.province_of(n);
                    e.on_line = (w.tile_settled.count(t) != 0) != (w.tile_settled.count(n) != 0);
                    edges.push_back(e);
                }
            }
        }
        // BOTH BANKS IN ONE PROVINCE (BL-1156). A course tile with exactly one
        // inflow and one outflow side splits its other four sides into two
        // arcs, one per bank. A site is JOINED when some land, non-course tile
        // on each arc lies in the same province.
        if (const auto hit = rasters.find(body); hit != rasters.end())
        {
            const int bw = bdims.at(body).first, bh = bdims.at(body).second;
            std::map<uint32_t, int> joined_by_province;
            for (const entity_id t : hit->second)
            {
                if (t == null_entity) continue;
                const tile_component& tc = w.tiles.at(t);
                if (is_water(tc.substrate) || tc.river_edges == 0) continue;
                int sides[2], k = 0;
                for (int s = 0; s < 6; ++s)
                    if ((tc.river_edges >> s) & 1u) { if (k < 2) sides[k] = s; ++k; }
                if (k != 2) continue;
                std::set<uint32_t> arc[2];
                for (int s = 0; s < 6; ++s)
                {
                    if (s == sides[0] || s == sides[1]) continue;
                    const int a = (s > sides[0] && s < sides[1]) ? 0 : 1;
                    const auto [nx_raw, ny] = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, s);
                    if (ny < 0 || ny >= bh) continue;
                    const entity_id n = hit->second[static_cast<std::size_t>(ny) * bw + ((nx_raw % bw) + bw) % bw];
                    if (n == null_entity) continue;
                    const tile_component& nt = w.tiles.at(n);
                    if (is_water(nt.substrate) || nt.river_edges != 0) continue;
                    arc[a].insert(w.provinces.province_of(n));
                }
                if (arc[0].empty() || arc[1].empty()) continue;
                ++r.c13_bank_sites;
                for (const uint32_t p : arc[0])
                    if (arc[1].count(p) != 0) { ++r.c13_banks_joined; ++joined_by_province[p]; break; }
            }
            r.c13_joining_provinces = static_cast<int>(joined_by_province.size());
            for (const auto& [pid, n] : joined_by_province)
                if (n > r.c13_joining_max_sites) { r.c13_joining_max_sites = n; r.c13_joining_max_id = pid; }
            if (const province* jp = r.c13_joining_max_sites > 0
                                         ? w.provinces.find(r.c13_joining_max_id) : nullptr)
                r.c13_joining_max_tiles = static_cast<int>(jp->tiles.size());
        }

        std::vector<float> dh;
        dh.reserve(edges.size());
        for (const land_edge& e : edges) dh.push_back(e.dh);
        std::sort(dh.begin(), dh.end());
        if (!dh.empty())
            r.c13_p90 = dh[std::min(dh.size() - 1, static_cast<std::size_t>(0.9 * double(dh.size())))];

        using ec = seed_record::edge_class;
        struct classes { ec river, cross, steep, plain; };
        std::map<entity_id, classes> by_body;
        for (const land_edge& e : edges)
        {
            classes& c = by_body[e.body];
            const auto bump = [&](ec& k) { ++k.n; if (e.border) ++k.border; if (e.on_line) ++k.on_line; };
            if (e.river) bump(c.river);
            if (e.cross) bump(c.cross);
            if (e.dh >= r.c13_p90) bump(c.steep);
            if (!e.river && !e.cross && e.dh < r.c13_p90) bump(c.plain);
        }
        const auto share = [](const ec& k) { return k.n ? double(k.border) / double(k.n) : 0.0; };
        for (const auto& [bid, c] : by_body)
        {
            if (bid == body)
            {
                r.c13_river = c.river;
                r.c13_cross = c.cross;
                r.c13_steep = c.steep;
                r.c13_plain = c.plain;
            }
            char line[192];
            if (c.cross.n > 0)
            {
                ++r.c13_river_bodies;
                // Against plain ground AND the step along the course: the
                // control that shares the centres-on-rivers confound (harness C2a).
                if (!(share(c.cross) > share(c.plain)) || !(share(c.cross) > share(c.river)))
                {
                    std::snprintf(line, sizeof line,
                                  "C2a body %u%s: river crossing %.2f%% (%" PRId64 ") not above plain"
                                  " %.2f%% (%" PRId64 ") and along %.2f%% (%" PRId64 ")",
                                  bid, bid == body ? " (home)" : "", 100.0 * share(c.cross),
                                  c.cross.n, 100.0 * share(c.plain), c.plain.n,
                                  100.0 * share(c.river), c.river.n);
                    r.c13_fail.emplace_back(line);
                }
            }
            if (c.steep.n > 0)
            {
                ++r.c13_steep_bodies;
                if (!(share(c.steep) > share(c.plain)))
                {
                    std::snprintf(line, sizeof line,
                                  "C2b body %u%s: steep %.2f%% (%" PRId64 ") <= plain %.2f%% (%" PRId64 ")",
                                  bid, bid == body ? " (home)" : "", 100.0 * share(c.steep),
                                  c.steep.n, 100.0 * share(c.plain), c.plain.n);
                    r.c13_fail.emplace_back(line);
                }
            }
        }
        if (r.c13_river_bodies == 0) r.c13_fail.emplace_back("C2a no body carries a river edge");
        if (r.c13_steep_bodies == 0) r.c13_fail.emplace_back("C2b no body carries a steep edge");
    }

    if (!map_dir.empty())
    {
        char name[64];
        std::snprintf(name, sizeof name, "/roads_seed%u.ppm", seed);
        write_road_map(w, body, centre_tiles, map_dir + name);
        std::snprintf(name, sizeof name, "/centres_seed%u.ppm", seed);
        write_centre_map(w, body, map_dir + name);
    }

    r.ok = ss.urban_map_drawn && r.carved > 0;
    r.cap_ok = r.sim_over_cell == 0 && r.groundless_standing == 0;
    r.ledger_ok = r.ledger_carried_sum == 0 && r.ledger_farm_over == 0 && r.ledger_farm_negative == 0
               && r.ledger_ind_over_urban == 0 && r.ledger_urban_over_pop == 0;
    return r;
}

double pct(int64_t a, int64_t b) { return b > 0 ? 100.0 * static_cast<double>(a) / static_cast<double>(b) : 0.0; }

/// n, median, p90 and max of a size list (nearest-rank on the sorted list).
struct size_summary { int n = 0, median = 0, p90 = 0, max = 0; };
size_summary summarise(std::vector<int> v)
{
    size_summary s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.n = static_cast<int>(v.size());
    s.median = v[v.size() / 2];
    s.p90 = v[std::min(v.size() - 1, static_cast<std::size_t>(0.9 * static_cast<double>(v.size())))];
    s.max = v.back();
    return s;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    std::string map_dir;
    bool from_args = false;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc)    { seeds = parse_seed_list(argv[++a]); from_args = true; }
        else if (s == "--map" && a + 1 < argc) map_dir = argv[++a];
        else
        {
            std::printf("usage: centre_census [--seeds a,b,c] [--map DIR]\n");
            return 2;
        }
    }
    if (seeds.empty())
    {
        seeds = library_seeds("docs/generation/seed_library.json");
        if (seeds.empty())
        {
            std::printf("FATAL  docs/generation/seed_library.json not found or carries no seeds "
                        "(run from the repo root)\n");
            return 2;
        }
    }

    std::printf("centre_census — BL-1130 (centres consolidate): density, scale and fit of the "
                "population centres\n");
    std::printf("world: the shipped arc (world_params{}), harness_params' build_app_base_world "
                "(generation + setup + recipes; no search)\n");
    std::printf("settle spacing: %d tiles (generation_settle_spacing_tiles, BL-1132)\n",
                generation_settle_spacing_tiles);
    std::printf("seeds (%s, %zu):", from_args ? "--seeds" : "docs/generation/seed_library.json",
                seeds.size());
    for (const uint32_t s : seeds) std::printf(" %u", s);
    std::printf("\n");
    std::fflush(stdout);

    lua_state lua; // one long-lived state, as the app keeps m_lua
    std::vector<seed_record> recs;
    for (const uint32_t seed : seeds)
    {
        recs.push_back(run_seed(lua, seed, map_dir));
        const seed_record& r = recs.back();
        std::printf("  seed %u built in %.1f s: %d centres on %d land tiles\n", r.seed, r.build_s,
                    r.centres, r.land);
        std::fflush(stdout);
    }

    std::printf("\n=== C0 the history (generation report, per span) ===\n");
    std::printf("seed | empires: battles conquests foundings | exploration: battles conquests "
                "foundings | industrialisation: battles conquests foundings | regions | armies at the close: men, garrisons\n");
    for (const seed_record& r : recs)
        std::printf("%4u | %16" PRId64 " %9" PRId64 " %9" PRId64 " | %20" PRId64 " %9" PRId64
                    " %9" PRId64 " | %26" PRId64 " %9" PRId64 " %9" PRId64 " | %7d | %12" PRId64 " %6d\n",
                    r.seed, r.battles[0], r.conquests[0], r.foundings[0], r.battles[1],
                    r.conquests[1], r.foundings[1], r.battles[2], r.conquests[2], r.foundings[2],
                    r.regions, r.army_close, r.garrisons);

    std::printf("\n=== C1b urbanisation (living regions, the sim record at the close) ===\n");
    std::printf("seed    population   urban_heads  urban%%  industrial_heads  largest_region_urban\n");
    for (const seed_record& r : recs)
    {
        char ind[32];
        if (r.sim_industrial < 0) std::snprintf(ind, sizeof ind, "n/a");
        else std::snprintf(ind, sizeof ind, "%" PRId64, r.sim_industrial);
        std::printf("%4u  %12" PRId64 "  %12" PRId64 "  %6.1f  %16s  %20" PRId64 "\n", r.seed,
                    r.sim_population, r.sim_urban, pct(r.sim_urban, r.sim_population), ind,
                    r.sim_urban_max);
    }

    std::printf("\n=== C1 the sim record (living regions) ===\n");
    std::printf("seed  regions  living  standing  sum_centres   urban_heads  max  @limit | "
                "per-region centres: 0  1  2-4  5-9  10-19  20+\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %6d  %8d  %11" PRId64 "  %12" PRId64 "  %3d  %6d | %22d %2d %4d %4d %6d %4d\n",
                    r.seed, r.regions, r.living, r.standing, r.sim_centres, r.sim_urban,
                    r.max_region_centres, r.at_limit, r.region_centre_hist[0],
                    r.region_centre_hist[1], r.region_centre_hist[2], r.region_centre_hist[3],
                    r.region_centre_hist[4], r.region_centre_hist[5]);

    std::printf("\n=== C2 density on the home body ===\n");
    std::printf("seed   land  placeable  cell_land(med/max) | centres  carved  anchors  coverage  "
                "dropped(built_out/no_tile) | centre_tiles  land%%   urban_tiles  urban%%\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %5d  %9d  %8d/%-8d | %7d  %6d  %7d  %8d  %13d/%-7d | %12d  %5.1f  %11d  %5.1f\n",
                    r.seed, r.land, r.placeable, r.cell_land_median, r.cell_land_max, r.centres,
                    r.carved, r.anchors, r.coverage, r.dropped_built_out, r.dropped_no_tile,
                    r.centre_tiles, pct(r.centre_tiles, r.land), r.urban_tiles,
                    pct(r.urban_tiles, r.land));

    std::printf("\n=== C3 scale (every centre, by k_population_for_scale rung) ===\n");
    std::printf("seed  village(10k)  town(50k)  city(200k)  metropolis(1M)  megacity(5M)\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %12d  %9d  %10d  %14d  %12d\n", r.seed, r.scale_count[0],
                    r.scale_count[1], r.scale_count[2], r.scale_count[3], r.scale_count[4]);

    std::printf("\n=== C4 fit (carved centres against their source region's cell) ===\n");
    std::printf("seed  spilled  spilled%%  | nominal: overfull (one centre)  excess_tiles | "
                "PAVED ACROSS: tiles  cells | regions_over_placeable  placeless(water/stacked/other) | "
                "groundless (standing) | sim_record_over_cell\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %7.1f  | %17d (%10d)  %12" PRId64 " | %19d  %5d | %22d  %9d (%d/%d/%d) | "
                    "%10d (%8d) | %20d\n",
                    r.seed, r.spilled, pct(r.spilled, r.carved), r.overfull_regions,
                    r.overfull_single, r.overfull_excess, r.paved_across, r.cells_paved_into,
                    r.overcount_regions, r.placeless_regions,
                    r.placeless_water, r.placeless_stacked,
                    r.placeless_regions - r.placeless_water - r.placeless_stacked,
                    r.groundless_regions, r.groundless_standing, r.sim_over_cell);

    std::printf("\n=== C11 the stockpile (BL-1042): points, to carved centres, and unspent by reason ===\n");
    std::printf("seed  points_total  to_centres  | carve_dropped  carve_no_tile  razed  no_carved_centre  rejected"
                "  | residual by why: emptied  groundless  no_centre_on_ground  other\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %12" PRId64 "  %10" PRId64 "  | %13" PRId64 "  %13" PRId64 "  %5" PRId64
                    "  %16" PRId64 "  %8" PRId64 "  | %24" PRId64 "  %10" PRId64 "  %19" PRId64 "  %5" PRId64 "\n",
                    r.seed, r.stock_total, r.stock_to_centres, r.stock_unspent[0], r.stock_unspent[1],
                    r.stock_unspent[2], r.stock_unspent[3], r.stock_unspent[4], r.resid_emptied,
                    r.resid_groundless, r.resid_no_centre, r.resid_other);

    std::printf("\n=== C5 where the density is: regions packed one to a tile ===\n");
    std::printf("seed  living  single_tile_cells  | centres  carved_in_single_tile_cells  "
                "carved_elsewhere  anchors+coverage\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %6d  %17d  | %7d  %27d  %16d  %16d\n", r.seed, r.living,
                    r.single_tile_cells, r.centres, r.carved_in_single,
                    r.carved - r.carved_in_single, r.anchors + r.coverage);

    std::printf("\n=== C6 what follows: roads and markets on the home body ===\n");
    std::printf("seed  road_tiles  road%%land  streets  network | markets\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %10d  %9.1f  %7d  %7d | %7d\n", r.seed, r.road_tiles,
                    pct(r.road_tiles, r.land), r.street_tiles, r.road_tiles - r.street_tiles,
                    r.markets);

    std::printf("\n=== C7 provinces on the home body (BL-1133, a province is its centre's ground) ===\n");
    std::printf("seed  land_provinces  centres  seeded  multi_centre  anchor_only  unanchored |"
                "  size: median  p90   max | road_tiles | uncentred_islands  singletons_absorbed\n");
    for (const seed_record& r : recs)
    {
        const size_summary s = summarise(r.sizes);
        std::printf("%4u  %14d  %7d  %6d  %12d  %11d  %10d | %13d  %4d  %4d | %10d | %17d  %19d\n",
                    r.seed, r.land_provinces, r.centres, r.seeded_provinces, r.multi_centre,
                    r.anchor_only, r.unanchored, s.median, s.p90, s.max, r.road_tiles,
                    r.part_stats.uncentred_regions, r.part_stats.covered_centre_singletons_absorbed);
    }

    std::printf("\nC7b the provinces resting on an anchor founding alone (the uncentred islands)\n");
    std::printf("seed  anchor_only  wholly_unsettled | size: median  p90   max  one_tile |"
                " why left: own_nation  nation_lock  water  unowned\n");
    int64_t why[4] = {};
    for (const seed_record& r : recs)
    {
        const size_summary s = summarise(r.anchor_only_sizes);
        const int one = static_cast<int>(
            std::count(r.anchor_only_sizes.begin(), r.anchor_only_sizes.end(), 1));
        std::printf("%4u  %11d  %16d | %12d  %4d  %4d  %8d | %20d  %11d  %5d  %7d\n", r.seed,
                    r.anchor_only, r.anchor_only_unsettled, s.median, s.p90, s.max, one,
                    r.why_own_nation, r.why_nation_lock, r.why_water, r.why_unowned);
        why[0] += r.why_own_nation; why[1] += r.why_nation_lock;
        why[2] += r.why_water; why[3] += r.why_unowned;
    }
    std::printf("pool  why left: own_nation %" PRId64 "  nation_lock %" PRId64 "  water %" PRId64
                "  unowned %" PRId64 "  (own_nation is the settlement lock's islands; 0 once the"
                " fill crosses the settled line, BL-1150)\n",
                why[0], why[1], why[2], why[3]);

    std::printf("\n=== C8 province size (land tiles) by the ANCHOR's scale: n / median / p90 / max ===\n");
    std::printf("seed  %-22s  %-22s  %-22s  %-22s  %-22s\n", "village(1)", "town(2)", "city(3)",
                "metropolis(4)", "megacity(5)");
    std::vector<int> pooled_by_anchor[5];
    for (const seed_record& r : recs)
    {
        std::printf("%4u", r.seed);
        for (int k = 0; k < 5; ++k)
        {
            const size_summary s = summarise(r.sizes_by_anchor[k]);
            char cell[64];
            std::snprintf(cell, sizeof cell, "%d/%d/%d/%d", s.n, s.median, s.p90, s.max);
            std::printf("  %-22s", cell);
            pooled_by_anchor[k].insert(pooled_by_anchor[k].end(), r.sizes_by_anchor[k].begin(),
                                       r.sizes_by_anchor[k].end());
        }
        std::printf("\n");
    }
    std::printf(" ALL");
    for (int k = 0; k < 5; ++k)
    {
        const size_summary s = summarise(pooled_by_anchor[k]);
        char cell[64];
        std::snprintf(cell, sizeof cell, "%d/%d/%d/%d", s.n, s.median, s.p90, s.max);
        std::printf("  %-22s", cell);
    }
    std::printf("\n");

    std::printf("\n=== C16 the nation garrisons' posts on the home body (BL-1145: the capital, else the anchor;"
                " a reading) ===\n");
    std::printf("seed  garrisons  in_water  at_capital  on_centre  off_lowest_tile | lowest_tile_unsettled"
                "  post_unsettled  left_wild  into_wild\n");
    {
        int64_t g = 0, wt = 0, cap = 0, oc = 0, mv = 0, ou = 0, nu = 0, lw = 0, iw = 0;
        for (const seed_record& r : recs)
        {
            std::printf("%4u  %9d  %8d  %10d  %9d  %15d | %21d  %14d  %9d  %9d\n", r.seed,
                        r.nat_garrisons, r.garrison_in_water, r.garrisons_at_capital,
                        r.garrison_on_centre, r.garrisons_moved, r.garrison_old_unsettled,
                        r.garrison_new_unsettled, r.garrison_left_wild, r.garrison_into_wild);
            g += r.nat_garrisons; wt += r.garrison_in_water; cap += r.garrisons_at_capital;
            oc += r.garrison_on_centre; mv += r.garrisons_moved; ou += r.garrison_old_unsettled;
            nu += r.garrison_new_unsettled; lw += r.garrison_left_wild; iw += r.garrison_into_wild;
        }
        std::printf(" ALL  %9" PRId64 "  %8" PRId64 "  %10" PRId64 "  %9" PRId64 "  %15" PRId64 " | %21" PRId64
                    "  %14" PRId64 "  %9" PRId64 "  %9" PRId64 "\n", g, wt, cap, oc, mv, ou, nu, lw, iw);
    }

    std::printf("\n=== C13 the cost model on the shipped worlds (province_partition_harness C2a/C2b,"
                " per body, like for like; BL-1150 review, BL-1156) ===\n");
    std::printf("home body: border share of each edge class and its ratio to plain. CROSS = a step onto or"
                " off a course sideways (C2a's class, BL-1156); ALONG = a step along a course"
                " (reported); plain excludes both. BANKS: course sites (one in, one out) with both"
                " banks in one province\n");
    std::printf("seed  p90|dh| | cross%%    (n)  along%%   (n)  plain%%     (n)  c/p   a/p | steep%%    (n)  s/p |"
                " banks joined/sites  provs  max(sites,tiles) | C2a  C2b\n");
    seed_record::edge_class pr_river, pr_cross, pr_steep, pr_plain;
    int64_t pr_sites = 0, pr_joined = 0;
    const auto shr = [](const seed_record::edge_class& k) {
        return k.n ? 100.0 * double(k.border) / double(k.n) : 0.0;
    };
    const auto ratio = [](double a, double b) { return b > 0.0 ? a / b : 0.0; };
    const auto add = [](seed_record::edge_class& to, const seed_record::edge_class& k) {
        to.n += k.n; to.border += k.border; to.on_line += k.on_line;
    };
    int c2a_red = 0;
    for (const seed_record& r : recs)
    {
        bool a_ok = true, b_ok = true;
        for (const std::string& f : r.c13_fail)
        {
            if (f.rfind("C2a", 0) == 0) a_ok = false;
            if (f.rfind("C2b", 0) == 0) b_ok = false;
        }
        if (!a_ok) ++c2a_red;
        std::printf("%4u  %.4f | %6.2f %6" PRId64 "  %6.2f %5" PRId64 "  %6.2f %8" PRId64 "  %4.2f  %4.2f | %6.2f %6"
                    PRId64 "  %4.2f | %6" PRId64 "/%-6" PRId64 " %5d  %4d,%-5d | %-4s %-4s\n",
                    r.seed, r.c13_p90, shr(r.c13_cross), r.c13_cross.n, shr(r.c13_river), r.c13_river.n,
                    shr(r.c13_plain), r.c13_plain.n, ratio(shr(r.c13_cross), shr(r.c13_plain)),
                    ratio(shr(r.c13_river), shr(r.c13_plain)), shr(r.c13_steep), r.c13_steep.n,
                    ratio(shr(r.c13_steep), shr(r.c13_plain)), r.c13_banks_joined, r.c13_bank_sites,
                    r.c13_joining_provinces, r.c13_joining_max_sites, r.c13_joining_max_tiles,
                    a_ok ? "ok" : "FAIL", b_ok ? "ok" : "FAIL");
        add(pr_river, r.c13_river); add(pr_cross, r.c13_cross);
        add(pr_steep, r.c13_steep); add(pr_plain, r.c13_plain);
        pr_sites += r.c13_bank_sites; pr_joined += r.c13_banks_joined;
    }
    std::printf("pool          | %6.2f %6" PRId64 "  %6.2f %5" PRId64 "  %6.2f %8" PRId64 "  %4.2f  %4.2f | %6.2f %6"
                PRId64 "  %4.2f | %6" PRId64 "/%-6" PRId64 " (%.1f%%) | C2a red on %d of %zu seeds\n",
                shr(pr_cross), pr_cross.n, shr(pr_river), pr_river.n, shr(pr_plain), pr_plain.n,
                ratio(shr(pr_cross), shr(pr_plain)), ratio(shr(pr_river), shr(pr_plain)),
                shr(pr_steep), pr_steep.n, ratio(shr(pr_steep), shr(pr_plain)), pr_joined, pr_sites,
                pr_sites ? 100.0 * double(pr_joined) / double(pr_sites) : 0.0, c2a_red, recs.size());

    std::printf("\n=== C14 water provinces, every body (BL-1156 review; reported) ===\n");
    std::printf("seed  coastal  lake  open_ocean  water_digest\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %4d  %10d  %016" PRIX64 "\n", r.seed, r.water_coast, r.water_lake,
                    r.water_ocean, r.water_digest);

    std::printf("\n=== C9 the settle-spacing ladder row (BL-1132; spacing %d tiles) ===\n",
                generation_settle_spacing_tiles);
    std::printf("seed  regions  living | cells by land: 1  2-4  5-9  10-24  25+ | centres  land%%  provinces | "
                "empires b/c/f | exploration b/c/f | industrialisation b/c/f\n");
    int64_t lr_regions = 0, lr_living = 0, lr_hist[5] = {}, lr_centres = 0, lr_ctiles = 0, lr_land = 0;
    int64_t lr_provinces = 0;
    int64_t lr_b[3] = {}, lr_c[3] = {}, lr_f[3] = {};
    for (const seed_record& r : recs)
    {
        std::printf("%4u  %7d  %6d | %17d %4d %4d %6d %4d | %7d  %5.1f  %9d | %6" PRId64 "/%" PRId64 "/%" PRId64
                    " | %6" PRId64 "/%" PRId64 "/%" PRId64 " | %6" PRId64 "/%" PRId64 "/%" PRId64 "\n",
                    r.seed, r.regions, r.living, r.cell_land_hist[0], r.cell_land_hist[1],
                    r.cell_land_hist[2], r.cell_land_hist[3], r.cell_land_hist[4], r.centres,
                    pct(r.centre_tiles, r.land), r.land_provinces, r.battles[0], r.conquests[0], r.foundings[0],
                    r.battles[1], r.conquests[1], r.foundings[1], r.battles[2], r.conquests[2],
                    r.foundings[2]);
        lr_regions += r.regions; lr_living += r.living; lr_centres += r.centres;
        lr_ctiles += r.centre_tiles; lr_land += r.land; lr_provinces += r.land_provinces;
        for (int k = 0; k < 5; ++k) lr_hist[k] += r.cell_land_hist[k];
        for (int k = 0; k < 3; ++k) { lr_b[k] += r.battles[k]; lr_c[k] += r.conquests[k]; lr_f[k] += r.foundings[k]; }
    }
    std::printf("pool  %7" PRId64 "  %6" PRId64 " | %17" PRId64 " %4" PRId64 " %4" PRId64 " %6" PRId64
                " %4" PRId64 " | %7" PRId64 "  %5.1f  %9" PRId64 " | %6" PRId64 "/%" PRId64 "/%" PRId64 " | %6" PRId64
                "/%" PRId64 "/%" PRId64 " | %6" PRId64 "/%" PRId64 "/%" PRId64 "\n",
                lr_regions, lr_living, lr_hist[0], lr_hist[1], lr_hist[2], lr_hist[3], lr_hist[4],
                lr_centres, pct(lr_ctiles, lr_land), lr_provinces, lr_b[0], lr_c[0], lr_f[0], lr_b[1], lr_c[1],
                lr_f[1], lr_b[2], lr_c[2], lr_f[2]);

    std::printf("\n=== C10 the spacing itself (BL-1132; seats nearer than %d tiles) ===\n",
                generation_settle_spacing_tiles);
    std::printf("seed  regions  sim_founded | close pairs, a sim-founded member (of them stacked) | "
                "close pairs within the settlement pass (not gated)\n");
    int64_t cp_sim = 0, cp_stacked = 0, cp_pass = 0, cp_founded = 0;
    for (const seed_record& r : recs)
    {
        std::printf("%4u  %7d  %11d | %39d (%d) | %d\n", r.seed, r.regions, r.sim_founded,
                    r.close_pairs_sim, r.close_pairs_stacked, r.close_pairs_pass);
        cp_sim += r.close_pairs_sim; cp_stacked += r.close_pairs_stacked;
        cp_pass += r.close_pairs_pass; cp_founded += r.sim_founded;
    }
    std::printf("pool  %7s  %11" PRId64 " | %39" PRId64 " (%" PRId64 ") | %" PRId64 "\n", "",
                cp_founded, cp_sim, cp_stacked, cp_pass);

    // THE CONSERVATION LEDGER (BL-1137 rebuild review; NR-958: a migrant carries
    // its food with it, a sack never lowers a ceiling). A FAIL row: the stream
    // moves ceiling, never makes it, so the ceiling it moved sums to exactly
    // zero over the world; and every region holds 0 <= farm-fed <= its
    // farm-fed ceiling and industrial <= urban <= population.
    std::printf("\n=== C15 the conservation ledger (every region at the close; a FAIL row) ===\n");
    std::printf("seed  carried_sum | farm_fed>ceiling  farm_fed<0  industrial>urban  urban>population | ok\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %11" PRId64 " | %16d  %10d  %16d  %16d | %s\n", r.seed, r.ledger_carried_sum,
                    r.ledger_farm_over, r.ledger_farm_negative, r.ledger_ind_over_urban,
                    r.ledger_urban_over_pop, r.ledger_ok ? "ok" : "FAIL");

    // BL-1149 (scale credit from the works; INDUSTRIALISATION.md sec 1, "a head
    // earns scale credit only where a work employs it"). Readings, never gated.
    std::printf("\n=== C14 the works and the points (BL-1149; readings, and the wiring FAIL row) ===\n");
    std::printf("seed  points_close  rho_set  rho(p,urban)  rho(p,employed) | urban_centred  employed  "
                "employed%% | migrants  receiving  top_decile%%  to_top_worked%% | span: from_scale  "
                "moved  inert_rounds  held_employed  wiring | open_work_left  migrants_on_open%% | within  along\n");
    for (const seed_record& r : recs)
    {
        char mig[24] = "n/a", top[16] = "n/a", worked[16] = "n/a";
        if (r.migrants >= 0)
        {
            std::snprintf(mig, sizeof mig, "%" PRId64, r.migrants);
            std::snprintf(top, sizeof top, "%.1f", 100.0 * r.migrants_top_decile);
            std::snprintf(worked, sizeof worked, "%.1f", 100.0 * r.migrants_to_worked);
        }
        std::printf("%4u  %12" PRId64 "  %7d  %12.3f  %15.3f | %13" PRId64 "  %8" PRId64 "  %9.1f | "
                    "%8s  %9d  %11s  %14s | %16" PRId64 "  %10" PRId64 "  %12" PRId64 "  %13" PRId64 "  %s | %14" PRId64 "  %17.1f | %10" PRId64 "  %10" PRId64 "\n",
                    r.seed, r.points_close, r.rho_regions, r.rho_points_urban, r.rho_points_employed,
                    r.heads_urban_centred, r.heads_employed, pct(r.heads_employed, r.heads_urban_centred),
                    mig, r.receiving, top, worked, r.span_from_scale, r.span_moved, r.span_inert_rounds,
                    r.held_employed, r.span_from_scale < 0 ? "n/a" : r.wiring_ok ? "ok" : "FAIL",
                    r.open_work_left, 100.0 * r.migrants_on_open, r.span_within,
                    (r.span_within >= 0 && r.span_moved >= 0) ? r.span_moved - r.span_within : -1);
        if (r.span_inert_rounds > 0)
            std::printf("      seed %u: Industrialisation scale credit inert: no works table (%" PRId64
                        " rounds)\n", r.seed, r.span_inert_rounds);
    }

    // Pooled.
    int64_t land = 0, centres = 0, ctiles = 0, spilled = 0, carved = 0, roads = 0, markets = 0;
    int64_t scales[5] = {};
    int fails = 0;
    for (const seed_record& r : recs)
    {
        land += r.land; centres += r.centres; ctiles += r.centre_tiles;
        spilled += r.spilled; carved += r.carved; roads += r.road_tiles; markets += r.markets;
        for (int s = 0; s < 5; ++s) scales[s] += r.scale_count[s];
        if (!r.ok) ++fails;
        if (!r.cap_ok) ++fails;
        if (!r.spacing_ok) ++fails;
        if (!r.ledger_ok) ++fails;
        if (!r.wiring_ok) ++fails;
        if (!r.c13_fail.empty()) ++fails;
        if (r.unanchored != 0) ++fails; // C7, BL-1145: every settled-land province holds a centre
    }
    // POPULATION.md § Generation: "a 1960 world aims at roughly 500 centres" --
    // an aim the forces are calibrated against, never a count any rule
    // enforces. Printed as a reading against it, never gated.
    std::printf("\n=== C12 against the aim (~500 centres a 1960 world; an aim, not a gate) ===\n");
    std::printf("seed  centres  x_aim  | carved  anchors+coverage\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %5.1f  | %6d  %16d\n", r.seed, r.centres, r.centres / 500.0,
                    r.carved, r.anchors + r.coverage);

    std::printf("\n=== pooled over %zu seeds ===\n", recs.size());
    std::printf("centres %" PRId64 " on %" PRId64 " land tiles (%.1f%% of land hosts a centre); "
                "scales %" PRId64 "/%" PRId64 "/%" PRId64 "/%" PRId64 "/%" PRId64
                "; spilled %.1f%% of carved; road tiles %" PRId64 "; markets %" PRId64 "\n",
                centres, land, pct(ctiles, land), scales[0], scales[1], scales[2], scales[3],
                scales[4], pct(spilled, carved), roads, markets);

    for (const seed_record& r : recs)
    {
        if (!r.ok)
            std::printf("FAIL  seed %u: no settlement record / urban map, or no carved centre -- "
                        "every fit row is vacuous\n", r.seed);
        // BL-1130 review fix, the cap asserted: the saved sim record against
        // its final cells by `region_centres_fit` (POPULATION.md "a region never
        // carries more centres than its ground holds"; "a region whose cell
        // holds no land carries no centre").
        if (!r.cap_ok)
            std::printf("FAIL  seed %u: %d living regions carry more centres than their cell's ground "
                        "holds, %d carry one on a cell with no standable ground\n",
                        r.seed, r.sim_over_cell, r.groundless_standing);
        // BL-1132 review fix, the spacing asserted: no sim-founded region's
        // seat stands nearer than the spacing to any other region's
        // (CIVILISATION.md § The unit is the city state).
        if (!r.spacing_ok)
            std::printf("FAIL  seed %u: %d region pairs stand nearer than %d tiles with a "
                        "sim-founded member (%d on one tile)\n",
                        r.seed, r.close_pairs_sim, generation_settle_spacing_tiles,
                        r.close_pairs_stacked);
        if (!r.wiring_ok)
            std::printf("FAIL  seed %u: the works table is loaded (%d rows) and held works employ %" PRId64
                        " heads, but the span earned %" PRId64 " points by scale, moved %" PRId64
                        " heads and ran %" PRId64 " rounds inert -- the table did not reach the accrual\n",
                        r.seed, r.works_rows, r.held_employed, r.span_from_scale, r.span_moved,
                        r.span_inert_rounds);
        if (!r.ledger_ok)
            std::printf("FAIL  seed %u: the conservation ledger breaks -- the stream moved a net "
                        "%" PRId64 " heads of ceiling into being; %d regions over their farm-fed "
                        "ceiling, %d with more industrial heads than people, %d industrial over "
                        "urban, %d urban over population\n",
                        r.seed, r.ledger_carried_sum, r.ledger_farm_over, r.ledger_farm_negative,
                        r.ledger_ind_over_urban, r.ledger_urban_over_pop);
        // BL-1150 review, the partition's cost model on the shipped world
        // (province_partition_harness C2a/C2b, per body): rivers and slopes
        // divide. Ben's call against the numbers (NR-962), never a tolerance.
        for (const std::string& f : r.c13_fail)
            std::printf("FAIL  seed %u: C13 %s\n", r.seed, f.c_str());
        // BL-1145, the anchor asserted (PROVINCES.md ruling 3): at generation
        // every land province on the settled home body holds a centre. A
        // province can outlive its centre only IN PLAY (NR-952).
        if (r.unanchored != 0)
            std::printf("FAIL  seed %u: C7 %d of %d land provinces on the home body hold no centre "
                        "at generation\n", r.seed, r.unanchored, r.land_provinces);
    }
    int c13_seeds = 0, other_fails = 0;
    for (const seed_record& r : recs)
    {
        if (!r.c13_fail.empty()) ++c13_seeds;
        if (!r.ok || !r.cap_ok || !r.spacing_ok || !r.ledger_ok || !r.wiring_ok
            || r.unanchored != 0) ++other_fails;
    }
    if (fails == 0)
        std::printf("\ncentre_census: OK (the cap, the spacing, the cost model, the conservation ledger, the works wiring"
                    " and the province anchor hold; no density is asserted)\n");
    else
        std::printf("\ncentre_census: FAIL (see the FAIL rows: %d seed(s) fail C13, the cost model;"
                    " %d fail the honesty check, the cap, the spacing, the ledger, the wiring or the"
                    " C7 anchor)\n",
                    c13_seeds, other_fails);
    return fails == 0 ? 0 : 1;
}
