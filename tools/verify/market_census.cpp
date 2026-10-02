// market_census — how many markets the world hands the player, where each one
// came from, what it covers, and whether it trades; and how many generated
// firms straddle a catchment line, with the landscape search's own validation
// run on the same world.
//
//   BL-1125 (markets can die), task M1 — THE CENSUS the mechanism is ruled
//   against (Rule 0b: measure first). Per curated seed, on the flipped 1960
//   world the player is handed:
//     * markets per body, split by source: capital shell / carve, and the
//       junction-lowered share of the carve;
//     * shells whose maker is dead at 1960, or no longer holds that capital;
//     * per market: catchment tiles (`market_for_tile` names it) and catchment
//       population; the distribution and the smallest ten;
//     * the share that clear nothing over the first play year (four quarterly
//       ticks after the finish, the player seated);
//     * nearest-neighbour distance between market centres, in tiles and in
//       traversal cost.
//   BL-1003 (pools per market), requirement R6 — task M2. How many generated
//   corporations hold buildings in more than one catchment, and how many
//   markets each spans; and whether the search's winner still validates.
//
// A READING, NOT A GATE ON THE ECONOMY. Nothing here asserts a market count,
// a catchment size or an idle share: those are what Ben rules the mechanism
// against, and a harness that pinned them would be making that call. The rows
// that DO fail the run (exit 1) are the instrument's own honesty checks — the
// shell identification agreeing three ways, the fixture being populated — and
// the search-validation rows V1-V5 below, which are the search's own
// invariants and not a judgement about the landscape.
//
// READ-ONLY OVER src/world/*. It calls the world's own functions and nothing in
// src/ changes for it. It restates two short mirrors from harness_params.hpp
// (see THE WORLD below), and it takes the world's logistics caches warm at the
// very end, after every reading is taken, on a world that is then discarded.
//
// ---------------------------------------------------------------------------
// THE WORLD — player_seed_sweep's app-order path, one pointer added
// ---------------------------------------------------------------------------
// The world is built exactly as player_seed_sweep's build_and_seat builds its
// shipped-arc world (harness_params.hpp, BL-1030): build_app_base_world, then
// apply_app_start_landscape (the search, the winner, the second recipe pass),
// then the validation settle (run_app_validation_tick x12, which is exactly
// run_app_validation_settle), then seat_player_corporation on the winner's
// static score. Then the FIRST PLAY YEAR: four live econ ticks, as
// run_app_live_window runs them.
//
// ONE THING DIFFERS, and it is a capture: `build_app_base_world` is restated
// here so `make_hard_coded_world` can be handed an `era_minus_one_fixture`.
// Every use of the fixture inside make_hard_coded_world is a WRITE into it
// (grep `fixture` there: nothing reads it back), so the world is byte-identical
// with or without it. That is checked rather than asserted: the harness prints
// D_land and D_settle — the BL-1031 digest recipes, world_digest.hpp — and they
// are compared against player_seed_sweep's k_shipped_digest_pins, which were
// taken through the unmodified helper. The live loop is restated from
// run_app_live_window for the same reason (a read hook needs a per-tick call);
// it is the same five lines in the same order.
//
// NOT MIRRORED, stated: the app's finish (finish_campaign_world) also calls
// date_chartered_firms after the winner's apply (BL-1099), which
// harness_params' mirror does not. It stamps `founded_year` on chartered firms,
// read only by the seat briefing's origin sentence, so no market, catchment,
// trade or straddle figure here can see it.
//
// ---------------------------------------------------------------------------
// HOW EACH READING IS TAKEN
// ---------------------------------------------------------------------------
// CAPITAL SHELLS. `market_component` carries no origin, so a shell is told
// apart three independent ways and the three must agree (a FAIL otherwise):
//   (1) ORDER + ANCHOR — generation spawns the shells (hard_coded_world.cpp,
//       the BL-910 block) before the carve, one per `region::has_market`
//       region in region order, centred on the region's anchor tile, and
//       entity ids are monotonic. So the S lowest-id home-body markets must be
//       centred, in order, on the anchor tiles of the S has_market regions of
//       the 1960 table (`world::gen_settlement`, and the fixture's own copy);
//   (2) THE PRICE — every shell's base price is the carve template's x the
//       capital premium (1.25) on every non-endemic good; a carve market's is
//       the template itself. A market is voted a shell by its goods;
//   (3) THE COUNT — the has_market regions in `world::gen_settlement` equal
//       the fixture's 1960 region table, region for region.
//
// WHICH CLOSE MADE IT, AND DID THE MAKER LIVE. `has_market` is set at the end
// of EVERY run_history_sim call for every living polity's capital and never
// cleared, and generation runs three (Empires to 1200, Exploration to 1660,
// Industrialisation to 1960). The fixture holds the region table and the
// polity table at each close (pre_exploration_settlement /
// pre_exploration_polities at 1200, exploration_handoff at 1660,
// industrialisation_handoff at 1960). A shell's CLOSE is the first table that
// marks its region; its MAKER is the polity alive at that close whose
// `capital` is that region. At 1960 the maker either still holds it as its
// capital, is alive with its capital elsewhere, or is dead (`alive` false);
// and separately the region either is or is not some living polity's capital
// at 1960 (a successor may hold it).
//
// JUNCTION-LOWERED CARVE. `generation_report::markets_from_trade`, the carve's
// own exact count. It is a COUNT, not a set: which carve markets they are
// cannot be recovered read-only, because the nation gate the junction lowered
// is computed from the corporation roster AT THE CARVE (hard_coded_world.cpp,
// `corps_in_nation`), which the landscape search has since replaced. Naming
// them per market would need a write-only field — a per-market origin tag, or
// the report carrying the junction markets' ids — which this lane may not add.
//
// CATCHMENT. Every home-body tile is routed with `market_for_tile`, the
// function the sim routes with. Population is the sum of
// `population_centre_component::population` (thousands) over the centres whose
// tile routes to the market. `market_for_tile` measures squared grid distance
// WITHOUT the column wrap; the census also counts the tiles the wrapped
// (physical) nearest centre would route differently, as a diagnostic. A market
// centred on the same tile as a lower-id market is a TWIN: `market_for_tile`
// breaks every distance tie to the lowest id, so a twin can never win a tile
// its elder can reach, and the census names which market shadows which.
//
// CLEARS NOTHING. `world::exchanges` gets one row per exchange, every clearing
// path (auto-surplus to the market, input draws from it, matched and
// auto-cleared standing orders), with a zero-quantity row dropped. A read hook
// after the clear lap of every tick takes the rows that clear appended; a
// market with no row in any of the four play ticks cleared nothing. The ring
// holds 8192 rows: a clear that appends more loses its oldest, and the harness
// counts them (`lost`) — a non-zero count makes the idle share an UPPER bound.
//
// NEAREST NEIGHBOUR. Grid distance is Euclidean with the column wrap (the
// surface is a cylinder). Traversal cost is `intra_body_path` between centre
// tiles — the convoy's own cost, landform x road x river — taken at the very
// end of the run, after the play year, so the flood fields it caches reach no
// reading.
//
// STRADDLE (M2). A corporation's buildings on the home body, each routed with
// `market_for_tile`; a corporation spans the number of distinct markets named.
// Read twice: the moment the winner is laid, and at the handoff (after the
// settle and the seat).
//
// SEARCH VALIDATION (M2). The search's own invariants, as landscape_search_
// harness states them for its R1 block, taken on THIS world's walk instead of
// that harness's no-prehistory world, and adjusted for a budget world (which
// skips the roster axis):
//   V1 evaluations == 1 + rounds x live axes (fixed rounds, no convergence exit)
//   V2 the path holds every proposal of every round
//   V3 the winner never scores below the seed candidate
//   V4 each accepted step strictly beat the incumbent it replaced, and the
//      reported winner IS the last accepted step (R1.5 + R1.6)
//   V5 THE WINNER RE-LAYS AND RE-SCORES TO THE SCORE THE SEARCH GAVE IT: a
//      faithful copy of the base world (taken before the search), the winner
//      applied with the same budget and spend the app's apply gets, scored with
//      the search's own params, is bit-identical on every scored term.
//   V6 (REPORTED, NOT GATED) the world as the player receives it — after the
//      apply AND the second recipe pass — re-scored the same way. The search
//      scores a candidate straight after its apply; apply_landscape_candidate
//      runs its recipe pass BEFORE it lays the firms, so a processor a firm
//      authored has no recipe in the scored world and gets one in the laid
//      world. Whether that moves the score is what this row says.
//
// Build:  bash tools/verify/build_lua_harness.sh market_census
// Run (repo root; it loads scripts/*.lua and the seed library by path):
//         ./build_gen/verify/market_census.exe [--seeds 46,28,...] [--live-ticks 4]
//                                              [--no-traversal] [--list 10]
//   Default seeds: docs/generation/seed_library.json, in library order.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/corporation_generation.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/landscape_score.hpp"
#include "world/landscape_search.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/orbital_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/settlement.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"
#include "world_digest.hpp"
#include "market_readings.hpp"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;
double secs_since(clk::time_point t0)
{
    return std::chrono::duration<double>(clk::now() - t0).count();
}

int g_failures = 0;

void check(bool ok, const char* id, const std::string& what)
{
    std::printf("  [%s] %s  %s\n", ok ? "PASS" : "FAIL", id, what.c_str());
    if (!ok)
        ++g_failures;
}

// --- seeds ------------------------------------------------------------------

/// The library's seeds in library order — the same minimal scan
/// industrialisation_sim_harness uses, so both read one list.
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

// --- distributions ------------------------------------------------------------

/// min / quartiles / max by NEAREST RANK on the sorted values: q(p) is the
/// element at index floor(p x (n-1)). Stated so a reader can re-derive a row.
struct dist5
{
    std::size_t n = 0;
    double min = 0, q1 = 0, med = 0, q3 = 0, max = 0, mean = 0;
};

dist5 summarise(std::vector<double> v)
{
    dist5 d;
    d.n = v.size();
    if (v.empty()) return d;
    std::sort(v.begin(), v.end());
    const auto at = [&](double p) {
        return v[static_cast<std::size_t>(std::floor(p * static_cast<double>(v.size() - 1)))];
    };
    d.min = v.front();
    d.q1  = at(0.25);
    d.med = at(0.50);
    d.q3  = at(0.75);
    d.max = v.back();
    double s = 0;
    for (const double x : v) s += x;
    d.mean = s / static_cast<double>(v.size());
    return d;
}

void print_dist(const char* label, const dist5& d, const char* fmt_unit = "")
{
    std::printf("    %-34s n=%-4zu min %9.1f  q1 %9.1f  med %9.1f  q3 %9.1f  max %10.1f  mean %9.1f %s\n",
                label, d.n, d.min, d.q1, d.med, d.q3, d.max, d.mean, fmt_unit);
}

// --- the world: player_seed_sweep's path, one capture pointer added ---------

/// harness_params.hpp `build_app_base_world`, restated line for line so
/// make_hard_coded_world can be handed @p fx. The fixture is write-only inside
/// generation; D_land / D_settle against player_seed_sweep's shipped pins is the
/// proof the world did not move. If the helper changes, this changes with it.
void build_census_base_world(lua_state& lua, const world_params& params,
                             app_start_world& out, era_minus_one_fixture* fx)
{
    out.params = params;
    lua.load("scripts/world_gen.lua");
    out.cfg = world_gen_config{};
    out.cfg.load_from_lua(lua);
    if (out.works.size() == 0)
    {
        lua.load("scripts/works.lua");
        out.works.load_from_lua(lua);
    }
    out.report = generation_report{};
    out.w = make_hard_coded_world(params, &out.report, out.cfg, /*progress=*/nullptr,
                                  &out.works, fx);
    seed_genesis_history(out.w, out.report);
    init_survey_states(out.w);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    out.reg.load_from_lua(lua);
    band_registry_from_world(out.reg, out.w);
    assign_default_recipes(out.w, out.reg);
}

// --- the exchange tap: a READ hook after the clear lap ------------------------

int clear_lap_index()
{
    for (int i = 0; i < k_campaign_settle_lap_count; ++i)
        if (std::strcmp(k_campaign_settle_lap_names[i], "clear_markets") == 0)
            return i;
    return -1;
}

struct exchange_tap
{
    int                          clear_lap  = -1;
    std::size_t                  last_total = 0;
    std::size_t                  lost       = 0;   ///< rows overwritten before the hook read them
    std::size_t                  max_rows   = 0;   ///< the largest single clear
    std::size_t                  rows_total = 0;
    int                          ticks      = 0;
    std::map<entity_id, double>  volume;           ///< quantity, all ticks
    std::map<entity_id, double>  value;            ///< quantity x unit price, all ticks
    std::map<entity_id, int>     rows;
    std::vector<std::set<entity_id>> active_by_tick;
};

void tap_after_lap(const world& w, int lap, void* ctx)
{
    auto* t = static_cast<exchange_tap*>(ctx);
    if (lap != t->clear_lap)
        return;
    const std::size_t total = w.exchanges.total;
    const std::size_t fresh = total - t->last_total;
    const std::size_t held  = std::min(fresh, w.exchanges.size());
    t->lost      += fresh - held;
    t->max_rows   = std::max(t->max_rows, fresh);
    t->rows_total += fresh;
    t->active_by_tick.emplace_back();
    std::set<entity_id>& active = t->active_by_tick.back();
    const std::size_t n = w.exchanges.size();
    for (std::size_t i = n - held; i < n; ++i)
    {
        const exchange_record& e = w.exchanges.oldest_first(i);
        t->volume[e.market] += static_cast<double>(e.quantity);
        t->value[e.market]  += static_cast<double>(e.quantity) * static_cast<double>(e.unit_price);
        ++t->rows[e.market];
        active.insert(e.market);
    }
    t->last_total = total;
    ++t->ticks;
}

// --- the straddle reading (M2) ------------------------------------------------

struct straddle_reading
{
    int corps          = 0;   ///< every corporation in the world
    int corps_home     = 0;   ///< with at least one building on the home body
    int straddlers     = 0;   ///< spanning two or more markets
    int bg_home        = 0, bg_straddlers   = 0;
    int spec_home      = 0, spec_straddlers = 0;
    int max_span       = 0;
    int buildings_home = 0;
    int buildings_outside_first = 0; ///< buildings of straddlers outside their modal market
    std::map<int, int> hist;         ///< markets spanned -> corporations
    struct wide { entity_id corp; bool bg; bool player; int buildings; int span; };
    std::vector<wide> widest;
    int player_span    = -1;
};

straddle_reading read_straddle(const world& w)
{
    straddle_reading r;
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    r.corps = static_cast<int>(ids.size());
    for (const entity_id cid : ids)
    {
        const corporation_component& cc = w.corporations.at(cid);
        std::map<entity_id, int> by_market;
        int home_buildings = 0;
        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end()) continue;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit == w.tiles.end() || tit->second.body != w.home_body) continue;
            ++home_buildings;
            ++by_market[market_for_tile(w, bit->second.tile)];
        }
        if (home_buildings == 0) continue;
        const int span = static_cast<int>(by_market.size());
        ++r.corps_home;
        r.buildings_home += home_buildings;
        ++r.hist[span];
        r.max_span = std::max(r.max_span, span);
        if (cc.is_background) ++r.bg_home; else ++r.spec_home;
        if (cc.is_player) r.player_span = span;
        if (span >= 2)
        {
            ++r.straddlers;
            if (cc.is_background) ++r.bg_straddlers; else ++r.spec_straddlers;
            int modal = 0;
            for (const auto& [m, k] : by_market) modal = std::max(modal, k);
            r.buildings_outside_first += home_buildings - modal;
            r.widest.push_back({ cid, cc.is_background, cc.is_player, home_buildings, span });
        }
    }
    std::sort(r.widest.begin(), r.widest.end(), [](const auto& a, const auto& b) {
        if (a.span != b.span) return a.span > b.span;
        if (a.buildings != b.buildings) return a.buildings > b.buildings;
        return a.corp < b.corp;
    });
    if (r.widest.size() > 5) r.widest.resize(5);
    return r;
}

void print_straddle(const char* when, const straddle_reading& s)
{
    const double share = s.corps_home > 0 ? 100.0 * s.straddlers / s.corps_home : 0.0;
    std::printf("    %-8s corps %d (%d with home-body buildings, %d buildings): STRADDLERS %d (%.1f%%)"
                "  background %d/%d  specialist %d/%d  max span %d",
                when, s.corps, s.corps_home, s.buildings_home, s.straddlers, share,
                s.bg_straddlers, s.bg_home, s.spec_straddlers, s.spec_home, s.max_span);
    if (s.player_span >= 0) std::printf("  player spans %d", s.player_span);
    std::printf("\n             markets spanned -> corps:");
    for (const auto& [span, n] : s.hist) std::printf("  %d:%d", span, n);
    std::printf("   buildings outside the straddler's modal market: %d\n", s.buildings_outside_first);
    for (const auto& x : s.widest)
        std::printf("             widest: corp %u (%s%s) %d buildings over %d markets\n",
                    x.corp, x.bg ? "background" : "specialist", x.player ? ", PLAYER" : "",
                    x.buildings, x.span);
}

// --- the search's own validation (M2) -------------------------------------------

bool same_score(const landscape_score& a, const landscape_score& b)
{
    return a.composite            == b.composite
        && a.realisation          == b.realisation
        && a.mean_actual          == b.mean_actual
        && a.mean_completeness    == b.mean_completeness
        && a.mean_balance         == b.mean_balance
        && a.mean_reach           == b.mean_reach
        && a.completeness_spread  == b.completeness_spread
        && a.balance_spread       == b.balance_spread
        && a.reach_spread         == b.reach_spread
        && a.spread               == b.spread
        && a.market_count         == b.market_count
        && a.markets.size()       == b.markets.size();
}

struct search_validation
{
    bool v1 = false, v2 = false, v3 = false, v4 = false, v5 = false;
    bool v6_same = false;             ///< reported, not gated
    bool budget_world = false;
    int  live_axes = 0, rounds = 0, evaluations = 0, path = 0, accepted = 0;
    double seed_composite = 0, winner_composite = 0, relaid_composite = 0, laid_composite = 0;
    int  score_markets = 0, laid_markets = 0;
    bool all() const { return v1 && v2 && v3 && v4 && v5; }
};

/// V1-V4 on the walk. V5 is taken by the caller on a copy of the laid world.
search_validation validate_walk(const app_start_world& out, const landscape_search_params& sp)
{
    search_validation v;
    const landscape_search_result& r = out.land.search;
    v.budget_world = out.land.stockpile_path && !out.land.stockpile.budget.empty()
                  && !r.charter_refused && !r.charter_fell_back;
    v.live_axes   = v.budget_world ? landscape_axis_count - 1 : landscape_axis_count;
    v.rounds      = sp.rounds;
    v.evaluations = r.evaluations;
    v.path        = static_cast<int>(r.path.size());
    v.accepted    = r.accepted;
    v.seed_composite   = r.seed_score.composite;
    v.winner_composite = r.winner_score.composite;
    v.score_markets    = r.winner_score.market_count;
    v.v1 = r.evaluations == 1 + sp.rounds * v.live_axes;
    v.v2 = static_cast<int>(r.path.size()) == sp.rounds * v.live_axes;
    v.v3 = compare_landscape(r.winner_score, r.seed_score) >= 0;
    landscape_score incumbent = r.seed_score;
    bool strict = true;
    int  taken  = 0;
    for (const landscape_search_step& s : r.path)
    {
        if (!s.accepted) continue;
        ++taken;
        if (compare_landscape(s.score, incumbent) <= 0) strict = false;
        incumbent = s.score;
    }
    v.v4 = strict && taken == r.accepted && same_score(incumbent, r.winner_score);
    return v;
}

// --- per-seed record --------------------------------------------------------------

enum class shell_fate : int
{
    maker_holds  = 0, ///< the polity that made it is alive at 1960 and it is still its capital
    maker_moved  = 1, ///< the maker is alive at 1960 with its capital elsewhere
    maker_dead   = 2, ///< the maker holds no ground at 1960
    no_maker     = 3, ///< no polity at the marking close names it capital (an anomaly)
};

struct market_row
{
    uint32_t  seed = 0;
    entity_id id = null_entity;
    entity_id body = null_entity;
    bool      shell = false;
    bool      anchored = false;
    int       close = 0;            ///< shells: 1200 / 1660 / 1960
    shell_fate fate = shell_fate::no_maker;
    bool      living_capital_1960 = false; ///< shells: some living polity's capital at 1960
    int       tiles = 0;
    int       land_tiles = 0;
    int64_t   pop_k = 0;
    int       centres = 0;
    int       buildings = 0;
    double    nn_tiles = -1.0;
    entity_id nn_id = null_entity;
    /// The lowest-id OTHER market centred on the same tile, when that id is
    /// lower than this one's: `market_for_tile` breaks a distance tie to the
    /// lowest id, so a shadowed market can never win a tile its twin can.
    entity_id shadowed_by = null_entity;
    bool      shadow_is_shell = false;
    double    nn_carve_tiles = -1.0;  ///< shells: grid distance to the nearest CARVE market
    double    nn_cost = -1.0;
    bool      nn_ocean = false;
    double    play_volume = 0.0;
    double    play_value  = 0.0;
    double    settle_volume = 0.0;
    int       active_quarters = 0;
};

struct seed_record
{
    uint32_t seed = 0;
    bool     ok = false;
    // counts
    int home_markets = 0, shells = 0, carve = 0, unanchored = 0;
    int64_t junction_report = 0, junctions = 0, corridors = 0;
    int offworld_markets_handoff = 0, offworld_markets_year = 0;
    int markets_year_end = 0;
    int shells_by_close[3] = { 0, 0, 0 };
    int fate[4] = { 0, 0, 0, 0 };
    int lost_living_capital = 0;   ///< maker lost it (moved or dead) AND no living capital there at 1960
    int lost_successor = 0;        ///< maker lost it but another living polity's capital stands there
    int has_market_offgrid = 0;
    int index_unstable = 0;
    // BL-1125 (markets can die)
    int shells_marked = 0, shells_folded = 0, marks_cleared = 0;
    int64_t folded_twins = 0, folded_gravity = 0;
    int64_t conquest_destroyed[3] = { 0, 0, 0 };
    double  goods_before = 0.0, goods_after = 0.0;
    int64_t tiles_moved = 0, misrouted = 0, across_water = 0;
    int     folded_records = 0;
    int     span_lost[3] = { 0, 0, 0 };      ///< marks set at a span's start and gone at its close
    int     span_remarked[3] = { 0, 0, 0 };  ///< destroyed in the span, marked again at its close
    bool    conquest_tie = true;             ///< I6: every lost mark is a recorded destruction
    int major_cities = 0, major_no_market = 0; ///< k_major_city_scale+ centres; of them, no market within k_major_city_radius
    bool id_tile_match = false, price_agrees = false, count_agrees = false;
    int price_shell_votes = 0;
    int zero_tile = 0, colocated = 0;
    int shared_tiles = 0;              ///< centre tiles carrying two or more markets
    int carve_under_shell = 0, carve_under_carve = 0, shell_under_shell = 0;
    int zero_tile_shadowed = 0;        ///< zero-tile markets that are shadowed twins
    int seam_misroutes = 0, home_tiles = 0;
    int raster_mismatch = 0; ///< R1: tiles the catchment raster routes unlike the scan
    int idle_play = 0, idle_play_shell = 0, idle_play_carve = 0;
    int idle_settle = 0, idle_both = 0;
    int no_building = 0, no_building_idle = 0;
    std::vector<int> active_per_quarter;
    std::size_t lost_rows_play = 0, lost_rows_settle = 0, max_rows_clear = 0;
    double play_volume_total = 0.0;
    straddle_reading at_land, at_handoff;
    search_validation sv;
    uint64_t d_land = 0, d_settle = 0;
    double t_gen = 0, t_land = 0, t_settle = 0, t_year = 0, t_census = 0, t_traversal = 0, t_total = 0;
    std::vector<market_row> rows;
};

/// Wrapped (cylinder) Euclidean grid distance.
double grid_dist(const tile_component& a, const tile_component& b, int gw)
{
    int dc = std::abs(a.grid_x - b.grid_x);
    if (dc > gw / 2) dc = gw - dc;
    const int dr = std::abs(a.grid_y - b.grid_y);
    return std::sqrt(static_cast<double>(dc) * dc + static_cast<double>(dr) * dr);
}

const char* fate_name(shell_fate f)
{
    switch (f)
    {
    case shell_fate::maker_holds: return "maker holds it";
    case shell_fate::maker_moved: return "maker alive, capital elsewhere";
    case shell_fate::maker_dead:  return "maker dead";
    default:                      return "no maker (anomaly)";
    }
}

// --- one seed, end to end ---------------------------------------------------------

seed_record run_seed(lua_state& lua, uint32_t seed, int live_ticks, bool traversal, int list_n)
{
    seed_record rec;
    rec.seed = seed;
    const clk::time_point t_all = clk::now();

    std::printf("\n=== seed %u =====================================================\n", seed);
    std::fflush(stdout);

    world_params p = arc_params(world_arc::shipped);
    p.seed = seed;
    auto out = std::make_unique<app_start_world>();
    auto fx  = std::make_unique<era_minus_one_fixture>();

    // --- 1. generation + the base (app::begin_new_game, start_new_game_prelude to load_economy)
    clk::time_point t0 = clk::now();
    build_census_base_world(lua, p, *out, fx.get());
    rec.t_gen = secs_since(t0);
    // The base the search walks from, kept for V5's re-lay (a faithful copy, BL-1034).
    auto base_copy = std::make_unique<world>(out->w);

    // --- 2. the landscape: the search, the winner, the second recipe pass
    t0 = clk::now();
    apply_app_start_landscape(*out);
    rec.t_land = secs_since(t0);
    {
        fnv1a64 f;
        hash_snapshot(f, out->w);
        rec.d_land = f.h;
    }
    world& w = out->w;
    const entity_id home = w.home_body;

    // V1-V4 on the walk; V5 re-lays the winner on the base; V6 re-scores the laid world.
    const landscape_search_params sp = shipped_search_params(seed, out->cfg.corporation_count);
    rec.sv = validate_walk(*out, sp);
    {
        // The app's apply gets the stockpile budget and its spend (harness_params
        // apply_shipped_landscape); an empty budget forwards to the legacy call.
        const charter_budget* budget = out->land.stockpile_path ? &out->land.stockpile.budget : nullptr;
        world relay = *base_copy;
        apply_landscape_candidate(relay, out->reg, out->land.search.winner,
                                  /*regenerate_specialists=*/true, budget,
                                  out->land.stockpile_spend, /*report=*/nullptr);
        const landscape_score relaid = score_landscape(relay, out->reg, sp.score);
        rec.sv.relaid_composite = relaid.composite;
        rec.sv.v5 = same_score(relaid, out->land.search.winner_score);
    }
    base_copy.reset();
    {
        world copy = w;
        const landscape_score laid = score_landscape(copy, out->reg, sp.score);
        rec.sv.laid_composite = laid.composite;
        rec.sv.laid_markets   = laid.market_count;
        rec.sv.v6_same = same_score(laid, out->land.search.winner_score);
    }
    rec.at_land = read_straddle(w);

    // --- 3. the validation settle: run_app_validation_settle, tick by tick, read-hooked
    exchange_tap settle_tap;
    settle_tap.clear_lap  = clear_lap_index();
    settle_tap.last_total = w.exchanges.total;
    t0 = clk::now();
    for (int econ_step = 0; econ_step < k_app_validation_ticks; ++econ_step)
        run_app_validation_tick(w, out->reg, econ_step, /*timing=*/nullptr, tap_after_lap, &settle_tap);
    rec.t_settle = secs_since(t0);
    rec.d_settle = world_state_digest(w);

    // --- 4. the seat (app::seat_player), on the winner's static score
    const spawn_seat_result seat =
        seat_player_corporation(w, seed, out->land.search.winner_score);
    rec.at_handoff = read_straddle(w);

    // --- 5. THE CENSUS AT THE HANDOFF --------------------------------------
    t0 = clk::now();
    const body_component& hb = w.bodies.at(home);
    const int gw = hb.grid_width, gh = hb.grid_height;

    std::vector<entity_id> grid(static_cast<std::size_t>(gw) * gh, null_entity);
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == home && tc.grid_x >= 0 && tc.grid_x < gw && tc.grid_y >= 0 && tc.grid_y < gh)
            grid[static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x] = tid;

    std::vector<entity_id> home_ids;
    std::map<entity_id, int> offworld_by_body;
    for (const auto& [mid, mc] : w.markets)
    {
        if (mc.body == home) home_ids.push_back(mid);
        else ++offworld_by_body[mc.body];
    }
    std::sort(home_ids.begin(), home_ids.end());
    rec.home_markets = static_cast<int>(home_ids.size());
    for (const auto& [b, n] : offworld_by_body) rec.offworld_markets_handoff += n;

    // -- the shells: (3) the count, (1) order + anchor --
    const bool fixture_ok = fx->ran && fx->exploration_ran && fx->industrialisation_ran;
    check(fixture_ok, "F1", "the fixture captured all three closes (1200, 1660, 1960)");
    const std::vector<region>& t1200 = fx->pre_exploration_settlement.regions;
    const std::vector<region>& t1660 = fx->exploration_handoff.regions;
    const std::vector<region>& t1960 = fx->industrialisation_handoff.regions;
    const std::vector<polity>& p1200 = fx->pre_exploration_polities;
    const std::vector<polity>& p1660 = fx->exploration_handoff.polities;
    const std::vector<polity>& p1960 = fx->industrialisation_handoff.polities;

    std::vector<int>       shell_region;
    std::vector<entity_id> shell_anchor;
    for (std::size_t i = 0; i < t1960.size(); ++i)
    {
        const region& rg = t1960[i];
        if (!rg.has_market) continue;
        if (rg.row < 0 || rg.row >= gh || rg.col < 0 || rg.col >= gw) { ++rec.has_market_offgrid; continue; }
        entity_id a = grid[static_cast<std::size_t>(rg.row) * gw + rg.col];
        if (a == null_entity) { ++rec.has_market_offgrid; continue; }
        // A MARKET STANDS ON LAND (BL-1138 review): a water anchor's shell stands on
        // the nearest land tile — squared grid distance, column wrap, ties to the
        // lower raster index — asked here from the grid, not from generation.
        if (is_water(w.tiles.at(a).substrate))
        {
            long long best_d2 = -1;
            entity_id best = null_entity;
            for (int r = 0; r < gh; ++r)
                for (int cc = 0; cc < gw; ++cc)
                {
                    const entity_id t = grid[static_cast<std::size_t>(r) * gw + cc];
                    if (t == null_entity || is_water(w.tiles.at(t).substrate)) continue;
                    int dc = std::abs(cc - rg.col);
                    dc = std::min(dc, gw - dc);
                    const long long dr = r - rg.row;
                    const long long d2 = static_cast<long long>(dc) * dc + dr * dr;
                    if (best_d2 < 0 || d2 < best_d2) { best_d2 = d2; best = t; }
                }
            if (best != null_entity) a = best;
        }
        shell_region.push_back(static_cast<int>(i));
        shell_anchor.push_back(a);
    }
    // BL-1125: a shell can FOLD at the carve (twins, gravity), so the shells
    // standing are a SUBSEQUENCE of the marked regions, in region order, and
    // the report says how many folded. The S' lowest-id home markets must be
    // centred, in order, on a subsequence of the marked anchors, with
    // S' = marked - folded.
    rec.shells_marked = static_cast<int>(shell_region.size());
    rec.shells_folded = static_cast<int>(out->report.shells_folded);
    const int shells_live = rec.shells_marked - rec.shells_folded;
    rec.id_tile_match = shells_live >= 0 && shells_live <= rec.home_markets;
    {
        std::vector<int>       live_region;
        std::vector<entity_id> live_anchor;
        std::size_t s = 0;
        for (int k = 0; rec.id_tile_match && k < shells_live; ++k)
        {
            const entity_id centre = w.markets.at(home_ids[static_cast<std::size_t>(k)]).centre_tile;
            while (s < shell_anchor.size() && shell_anchor[s] != centre) ++s;
            if (s == shell_anchor.size()) { rec.id_tile_match = false; break; }
            live_region.push_back(shell_region[s]);
            live_anchor.push_back(shell_anchor[s]);
            ++s;
        }
        if (rec.id_tile_match) { shell_region = live_region; shell_anchor = live_anchor; }
    }
    rec.shells = std::max(0, shells_live);
    {
        bool same = w.gen_settlement != nullptr && w.gen_settlement->regions.size() == t1960.size();
        for (std::size_t i = 0; same && i < t1960.size(); ++i)
            same = w.gen_settlement->regions[i].has_market == t1960[i].has_market;
        rec.count_agrees = same;
    }

    // -- (2) the price vote, every home market --
    constexpr float k_premium = 1.25f; // hard_coded_world.cpp kCapitalMarketPricePremium (NR-916)
    int price_disagree = 0;
    for (std::size_t k = 0; k < home_ids.size(); ++k)
    {
        const market_component& mc = w.markets.at(home_ids[k]);
        int prem = 0, plain = 0;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float tmpl = out->cfg.kepler_base_price[r];
            if (tmpl <= 0.0f) continue;
            if (mc.base_price[r] == tmpl * k_premium) ++prem;
            else if (mc.base_price[r] == tmpl) ++plain;
        }
        const bool voted_shell = prem > plain;
        if (voted_shell) ++rec.price_shell_votes;
        const bool is_shell = static_cast<int>(k) < rec.shells;
        if (voted_shell != is_shell) ++price_disagree;
    }
    rec.price_agrees = price_disagree == 0;

    // -- which close made each shell, and the maker's 1960 fate --
    const auto capital_of = [](const std::vector<polity>& ps, int region_idx) -> int {
        for (const polity& q : ps)
            if (q.alive && q.capital == region_idx) return q.id;
        return -1;
    };
    std::vector<int>        shell_close(shell_region.size(), 0);
    std::vector<shell_fate> shell_fates(shell_region.size(), shell_fate::no_maker);
    std::vector<bool>       shell_living(shell_region.size(), false);
    for (std::size_t s = 0; s < shell_region.size(); ++s)
    {
        const int R = shell_region[s];
        const std::size_t u = static_cast<std::size_t>(R);
        int close = 1960;
        const std::vector<polity>* makers = &p1960;
        if (u < t1200.size() && t1200[u].has_market)      { close = 1200; makers = &p1200; }
        else if (u < t1660.size() && t1660[u].has_market) { close = 1660; makers = &p1660; }
        shell_close[s] = close;
        ++rec.shells_by_close[close == 1200 ? 0 : close == 1660 ? 1 : 2];
        const int maker = capital_of(*makers, R);
        shell_fate f = shell_fate::no_maker;
        if (maker >= 0)
        {
            if (maker >= static_cast<int>(p1960.size()) || !p1960[static_cast<std::size_t>(maker)].alive)
                f = shell_fate::maker_dead;
            else if (p1960[static_cast<std::size_t>(maker)].capital == R)
                f = shell_fate::maker_holds;
            else
                f = shell_fate::maker_moved;
        }
        shell_fates[s] = f;
        ++rec.fate[static_cast<int>(f)];
        shell_living[s] = capital_of(p1960, R) >= 0;
        if (f == shell_fate::maker_moved || f == shell_fate::maker_dead)
        {
            if (shell_living[s]) ++rec.lost_successor;
            else ++rec.lost_living_capital;
        }
    }
    // Index stability: region i is the same PLACE in every table (its anchor
    // does not move), unless the region table was reindexed. BL-1125: a mark
    // is no longer sticky -- conquest clears it -- so stability is read off the
    // anchors, and a mark set earlier and gone by 1960 is counted as cleared.
    for (std::size_t i = 0; i < t1200.size(); ++i)
    {
        if (i >= t1960.size() || t1200[i].row != t1960[i].row || t1200[i].col != t1960[i].col)
            ++rec.index_unstable;
        else if (t1200[i].has_market && !t1960[i].has_market) ++rec.marks_cleared;
    }
    for (std::size_t i = 0; i < t1660.size(); ++i)
    {
        if (i >= t1960.size() || t1660[i].row != t1960[i].row || t1660[i].col != t1960[i].col)
            ++rec.index_unstable;
        else if (t1660[i].has_market && !t1960[i].has_market
                 && !(i < t1200.size() && t1200[i].has_market)) ++rec.marks_cleared;
    }
    rec.folded_twins   = out->report.markets_folded_twins;
    rec.folded_gravity = out->report.markets_folded_gravity;
    for (int k = 0; k < 3; ++k) rec.conquest_destroyed[k] = out->report.markets_destroyed_by_conquest[k];
    rec.goods_before = out->report.market_fold_goods_before;
    rec.goods_after  = out->report.market_fold_goods_after;
    rec.tiles_moved  = out->report.market_fold_tiles_moved;
    rec.misrouted    = out->report.market_fold_misrouted;
    rec.across_water = out->report.markets_folded_across_water;
    // I6 -- THE CONQUEST TIE. Within a span a mark is only ever CLEARED (by
    // conquest) and only SET at the close, so for each span: every region marked
    // at its start and unmarked at its close is one the span recorded destroying,
    // every recorded destruction stood on a region marked at the start, no region
    // is destroyed twice, and lost + destroyed-then-remarked == destroyed.
    {
        const std::vector<region>* start[3] = { nullptr, &t1200, &t1660 };
        const std::vector<region>* close[3] = { &t1200, &t1660, &t1960 };
        for (int k = 0; k < 3; ++k)
        {
            const std::vector<int32_t>& d = out->report.markets_destroyed_regions[k];
            std::set<int32_t> ds(d.begin(), d.end());
            if (ds.size() != d.size()) rec.conquest_tie = false;
            if (static_cast<int64_t>(d.size()) != rec.conquest_destroyed[k]) rec.conquest_tie = false;
            if (start[k] == nullptr) { if (!d.empty()) rec.conquest_tie = false; continue; }
            const std::vector<region>& a = *start[k];
            const std::vector<region>& b = *close[k];
            for (const int32_t ri : d)
            {
                const std::size_t u = static_cast<std::size_t>(ri);
                if (ri < 0 || u >= a.size() || !a[u].has_market) { rec.conquest_tie = false; continue; }
                if (u < b.size() && b[u].has_market) ++rec.span_remarked[k];
            }
            for (std::size_t u = 0; u < a.size(); ++u)
                if (a[u].has_market && !(u < b.size() && b[u].has_market))
                {
                    ++rec.span_lost[k];
                    if (ds.count(static_cast<int32_t>(u)) == 0) rec.conquest_tie = false;
                }
            if (rec.span_lost[k] + rec.span_remarked[k] != static_cast<int>(d.size()))
                rec.conquest_tie = false;
        }
    }

    rec.carve = 0;
    for (std::size_t k = static_cast<std::size_t>(rec.shells); k < home_ids.size(); ++k)
    {
        if (w.markets.at(home_ids[k]).centre_tile == null_entity) ++rec.unanchored;
        else ++rec.carve;
    }
    rec.junction_report = out->report.markets_from_trade;
    rec.junctions       = out->report.prehistory_junctions;
    rec.corridors       = out->report.prehistory_corridors;

    // -- the rows --
    std::map<entity_id, std::size_t> row_of;
    for (std::size_t k = 0; k < home_ids.size(); ++k)
    {
        market_row r;
        r.seed = seed;
        r.id   = home_ids[k];
        r.body = home;
        r.shell = static_cast<int>(k) < rec.shells;
        r.anchored = w.markets.at(r.id).centre_tile != null_entity;
        if (r.shell)
        {
            r.close = shell_close[k];
            r.fate  = shell_fates[k];
            r.living_capital_1960 = shell_living[k];
        }
        row_of[r.id] = rec.rows.size();
        rec.rows.push_back(r);
    }

    // -- catchment tiles, and the wrap diagnostic --
    std::vector<std::pair<entity_id, const tile_component*>> centres; // standing, anchored, ascending id
    for (const entity_id mid : home_ids)
    {
        const auto cit = w.tiles.find(w.markets.at(mid).centre_tile);
        if (cit != w.tiles.end()) centres.emplace_back(mid, &cit->second);
    }
    // BL-1125: the wrap diagnostic routes over the ORIGINAL centres -- standing
    // and folded -- in ascending id; a folded winner routes to its absorber, as
    // market_for_tile does. (`centres` above stays standing-only: the spacing
    // rows below read it.)
    std::vector<std::pair<entity_id, const tile_component*>> route_centres = centres;
    for (const auto& [fid, fm] : w.folded_markets)
    {
        if (fm.body != home) continue;
        const auto cit = w.tiles.find(fm.centre_tile);
        if (cit != w.tiles.end()) route_centres.emplace_back(fid, &cit->second);
    }
    std::sort(route_centres.begin(), route_centres.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    rec.folded_records = static_cast<int>(w.folded_markets.size());
    for (std::size_t idx = 0; idx < grid.size(); ++idx)
    {
        const entity_id tid = grid[idx];
        if (tid == null_entity) continue;
        const tile_component& tc = w.tiles.at(tid);
        ++rec.home_tiles;
        const entity_id m = market_for_tile(w, tid);
        if (market_for_tile_scan(w, tid) != m) ++rec.raster_mismatch; // R1
        const auto rit = row_of.find(m);
        if (rit == row_of.end()) continue;
        market_row& r = rec.rows[rit->second];
        ++r.tiles;
        if (!is_water(tc.substrate)) ++r.land_tiles;
        // The wrapped nearest (ties -> lowest id), against market_for_tile's unwrapped one.
        entity_id best = null_entity;
        long long best_d = 0;
        for (const auto& [mid, ct] : route_centres)
        {
            long long dc = std::abs(ct->grid_x - tc.grid_x);
            if (dc > gw / 2) dc = gw - dc;
            const long long dr = ct->grid_y - tc.grid_y;
            const long long d  = dc * dc + dr * dr;
            if (best == null_entity || d < best_d) { best = mid; best_d = d; }
        }
        if (best != null_entity)
        {
            const auto fit = w.folded_markets.find(best);
            if (fit != w.folded_markets.end()) best = fit->second.into;
        }
        if (best != null_entity && best != m) ++rec.seam_misroutes;
    }

    // -- catchment population and buildings --
    for (const auto& [cid, pcc] : w.population_centres)
    {
        const auto tit = w.population_centre_tile.find(cid);
        if (tit == w.population_centre_tile.end()) continue;
        const auto tc = w.tiles.find(tit->second);
        if (tc == w.tiles.end() || tc->second.body != home) continue;
        const auto rit = row_of.find(market_for_tile(w, tit->second));
        if (rit == row_of.end()) continue;
        rec.rows[rit->second].pop_k += pcc.population;
        ++rec.rows[rit->second].centres;
        // BL-1125: a MAJOR city (market_readings.hpp, shared with the ladder)
        // with no market standing within k_major_city_radius grid tiles of it.
        if (pcc.scale >= k_major_city_scale)
        {
            ++rec.major_cities;
            bool near = false;
            for (const entity_id mid : home_ids)
            {
                const auto ct = w.tiles.find(w.markets.at(mid).centre_tile);
                if (ct != w.tiles.end() && grid_dist(ct->second, tc->second, gw) <= k_major_city_radius) { near = true; break; }
            }
            if (!near) ++rec.major_no_market;
        }
    }
    for (const auto& [bid, b] : w.buildings)
    {
        const auto tc = w.tiles.find(b.tile);
        if (tc == w.tiles.end() || tc->second.body != home) continue;
        const auto rit = row_of.find(market_for_tile(w, b.tile));
        if (rit != row_of.end()) ++rec.rows[rit->second].buildings;
    }

    // -- nearest neighbour, grid --
    for (market_row& r : rec.rows)
    {
        if (!r.anchored) continue;
        const tile_component& a = w.tiles.at(w.markets.at(r.id).centre_tile);
        for (const auto& [mid, ct] : centres)
        {
            if (mid == r.id) continue;
            const double d = grid_dist(a, *ct, gw);
            if (r.nn_tiles < 0 || d < r.nn_tiles) { r.nn_tiles = d; r.nn_id = mid; }
            const bool other_is_shell = rec.rows[row_of.at(mid)].shell;
            if (r.shell && !other_is_shell && (r.nn_carve_tiles < 0 || d < r.nn_carve_tiles))
                r.nn_carve_tiles = d;
        }
        if (r.nn_tiles == 0.0) ++rec.colocated;
    }
    // -- co-located centres: which market shadows which --
    {
        std::map<entity_id, std::vector<entity_id>> by_centre; // centre tile -> markets, ascending id
        for (const entity_id mid : home_ids)
        {
            const entity_id ct = w.markets.at(mid).centre_tile;
            if (ct != null_entity) by_centre[ct].push_back(mid);
        }
        for (const auto& [ct, ms] : by_centre)
        {
            if (ms.size() < 2) continue;
            ++rec.shared_tiles;
            const market_row& owner = rec.rows[row_of.at(ms.front())];
            for (std::size_t i = 1; i < ms.size(); ++i)
            {
                market_row& r = rec.rows[row_of.at(ms[i])];
                r.shadowed_by     = owner.id;
                r.shadow_is_shell = owner.shell;
                if (owner.shell && r.shell) ++rec.shell_under_shell;
                else if (owner.shell)       ++rec.carve_under_shell;
                else                        ++rec.carve_under_carve;
            }
        }
    }
    rec.t_census = secs_since(t0);

    // -- the settle's activity, per market --
    for (market_row& r : rec.rows)
    {
        const auto it = settle_tap.volume.find(r.id);
        r.settle_volume = it != settle_tap.volume.end() ? it->second : 0.0;
    }
    rec.lost_rows_settle = settle_tap.lost;

    // --- 6. THE FIRST PLAY YEAR: run_app_live_window, restated for the read hook
    exchange_tap play_tap;
    play_tap.clear_lap  = clear_lap_index();
    play_tap.last_total = w.exchanges.total;
    t0 = clk::now();
    {
        constexpr int k_econ_tick_days = 90; // sim_loop::econ_tick_days (harness_params.hpp)
        settle_tick_hooks hooks;
        hooks.after_lap = tap_after_lap;
        hooks.ctx       = &play_tap;
        for (int k = 1; k <= live_ticks; ++k)
        {
            const int day = k * k_econ_tick_days;
            advance_orbits(w, static_cast<double>(k_econ_tick_days));
            advance_surveys(w, k_econ_tick_days);
            w.current_day_tick = day;
            (void)run_settle_tick(w, out->reg, k_app_validation_ticks + (k - 1), day,
                                  /*spectating=*/false, &hooks);
        }
    }
    rec.t_year = secs_since(t0);
    rec.lost_rows_play = play_tap.lost;
    rec.max_rows_clear = std::max(settle_tap.max_rows, play_tap.max_rows);
    for (const auto& s : play_tap.active_by_tick)
    {
        int n = 0;
        for (const entity_id m : s)
            if (row_of.count(m)) ++n;
        rec.active_per_quarter.push_back(n);
    }
    for (market_row& r : rec.rows)
    {
        const auto vit = play_tap.volume.find(r.id);
        r.play_volume = vit != play_tap.volume.end() ? vit->second : 0.0;
        const auto xit = play_tap.value.find(r.id);
        r.play_value = xit != play_tap.value.end() ? xit->second : 0.0;
        for (const auto& s : play_tap.active_by_tick)
            if (s.count(r.id)) ++r.active_quarters;
        rec.play_volume_total += r.play_volume;
        const bool idle = r.play_volume <= 0.0;
        if (idle)
        {
            ++rec.idle_play;
            if (r.shell) ++rec.idle_play_shell; else ++rec.idle_play_carve;
        }
        if (r.settle_volume <= 0.0) ++rec.idle_settle;
        if (idle && r.settle_volume <= 0.0) ++rec.idle_both;
        if (r.tiles == 0)
        {
            ++rec.zero_tile;
            if (r.shadowed_by != null_entity) ++rec.zero_tile_shadowed;
        }
        if (r.buildings == 0) { ++rec.no_building; if (idle) ++rec.no_building_idle; }
    }
    {
        int n = 0, off = 0;
        for (const auto& [mid, mc] : w.markets) { if (mc.body == home) ++n; else ++off; }
        rec.markets_year_end      = n;
        rec.offworld_markets_year = off;
    }

    // --- 7. traversal cost between centres — LAST, after every reading ------
    t0 = clk::now();
    if (traversal)
    {
        for (market_row& r : rec.rows)
        {
            if (!r.anchored) continue;
            const entity_id mt = w.markets.at(r.id).centre_tile;
            for (const auto& [mid, ct] : centres)
            {
                if (mid == r.id) continue;
                const entity_id nt = w.markets.at(mid).centre_tile;
                const logistics_path& lp = intra_body_path(w, home, nt, mt);
                if (!lp.reachable) continue;
                const double c = static_cast<double>(lp.cost);
                if (r.nn_cost < 0 || c < r.nn_cost) { r.nn_cost = c; r.nn_ocean = lp.crosses_ocean; }
            }
        }
    }
    rec.t_traversal = secs_since(t0);
    rec.t_total = secs_since(t_all);

    // --- print the seed --------------------------------------------------------
    std::printf("[time] generation+base %.1f s  landscape %.1f s  settle %.1f s  play year %.1f s  "
                "census %.1f s  traversal %.1f s  total %.1f s\n",
                rec.t_gen, rec.t_land, rec.t_settle, rec.t_year, rec.t_census, rec.t_traversal,
                rec.t_total);
    std::printf("[digest] D_land %016" PRIX64 "  D_settle %016" PRIX64
                "   (compare player_seed_sweep k_shipped_digest_pins)\n", rec.d_land, rec.d_settle);
    std::printf("[seat] corp %u%s\n", seat.seated, seat.floor_unmet ? "  (FLOOR UNMET)" : "");

    std::printf("[M1] home body %u (%s): %d markets = %d capital shells + %d carve%s"
                "   | junction-lowered at the carve, before the folds (report.markets_from_trade): %" PRId64
                "   | corridors %" PRId64 ", junction regions %" PRId64 "\n",
                home, hb.name.c_str(), rec.home_markets, rec.shells, rec.carve,
                rec.unanchored ? " + unanchored fallback" : "",
                rec.junction_report, rec.corridors, rec.junctions);
    if (offworld_by_body.empty())
        std::printf("     off-world bodies with markets at the handoff: none");
    else
        for (const auto& [b, n] : offworld_by_body)
            std::printf("     off-world body %u: %d markets", b, n);
    std::printf("   | at the end of the play year: home %d, off-world %d\n",
                rec.markets_year_end, rec.offworld_markets_year);
    std::printf("     shells marked at the 1200 close %d, at 1660 %d, at 1960 %d"
                "   (has_market regions off the grid: %d, index-unstable marks: %d)\n",
                rec.shells_by_close[0], rec.shells_by_close[1], rec.shells_by_close[2],
                rec.has_market_offgrid, rec.index_unstable);
    std::printf("     shell fate at 1960: %s %d, %s %d, %s %d, %s %d\n",
                fate_name(shell_fate::maker_holds), rec.fate[0],
                fate_name(shell_fate::maker_moved), rec.fate[1],
                fate_name(shell_fate::maker_dead), rec.fate[2],
                fate_name(shell_fate::no_maker), rec.fate[3]);
    std::printf("     of the %d whose maker lost it: %d now stand on NO living capital, %d on a "
                "successor's capital\n",
                rec.fate[1] + rec.fate[2], rec.lost_living_capital, rec.lost_successor);
    check(rec.id_tile_match, "I1", "the lowest-id home markets sit, in order, on the has_market "
                                   "regions' anchor tiles, or the nearest land tile to a water anchor"
                                   " (shells spawn before the carve)");
    char buf[160];
    std::snprintf(buf, sizeof buf, "the price vote agrees market for market (%d voted shell, "
                                   "premium x%.2f on every non-endemic good)",
                  rec.price_shell_votes, static_cast<double>(k_premium));
    check(rec.price_agrees, "I2", buf);
    check(rec.count_agrees, "I3", "world::gen_settlement's has_market regions ARE the fixture's "
                                  "1960 table, region for region");
    check(rec.fate[3] == 0, "I4", "every shell names a maker at the close that marked it");
    check(rec.index_unstable == 0, "I5", "every region of the 1200 and 1660 tables is the same place "
                                         "in the 1960 table (the region index is stable across the spans)");
    check(rec.conquest_tie, "I6", "conquest ties to the closes: per span, every mark lost is a recorded "
                                  "destruction, and lost + re-marked == destroyed");
    std::printf("     per span lost/re-marked: Exploration %d/%d, Industrialisation %d/%d\n",
                rec.span_lost[1], rec.span_remarked[1], rec.span_lost[2], rec.span_remarked[2]);
    std::printf("     BL-1125 deaths: conquest destroyed %" PRId64 " (Empires: structurally 0, nothing is marked before the 1200 close) %" PRId64 " (Exploration) %" PRId64
                " (Industrialisation); marks set earlier and gone by 1960 %d | at the carve: twins folded %" PRId64
                ", gravity folded %" PRId64 ", of them shells %d (of %d marked)\n",
                rec.conquest_destroyed[0], rec.conquest_destroyed[1], rec.conquest_destroyed[2],
                rec.marks_cleared, rec.folded_twins, rec.folded_gravity, rec.shells_folded, rec.shells_marked);
    std::printf("     gravity folds across water (both centres ported): %" PRId64 "; fold records %d\n",
                rec.across_water, rec.folded_records);
    std::printf("     major cities (scale >= %d) %d, with no market centre within %.0f tiles %d\n",
                k_major_city_scale, rec.major_cities, k_major_city_radius, rec.major_no_market);
    check(rec.conquest_destroyed[0] == 0, "C0", "the Empires span destroys no market (structurally: none is marked before 1200)");
    {
        const double tol = 1e-6 * std::max(1.0, std::fabs(rec.goods_before));
        char cb[240];
        // A PLUMBING CHECK, said as one: the fold pass compares market_for_tile
        // with itself before and after, so this proves the fold map is written
        // and read back through routing, and that goods move with the fold. It
        // is not independent evidence that a catchment passes -- R2 below is.
        std::snprintf(cb, sizeof cb, "the fold map is written and read: %" PRId64 " tiles moved with "
                                     "their market, %" PRId64 " routed anywhere but their absorber; goods "
                                     "(inventory + pools) %.3f -> %.3f",
                      rec.tiles_moved, rec.misrouted, rec.goods_before, rec.goods_after);
        check(std::fabs(rec.goods_after - rec.goods_before) <= tol && rec.misrouted == 0, "C1", cb);
    }

    {
        std::vector<double> tiles, land, pop, nn, nnc, shell_carve;
        for (const market_row& r : rec.rows)
        {
            tiles.push_back(r.tiles);
            land.push_back(r.land_tiles);
            pop.push_back(static_cast<double>(r.pop_k));
            if (r.nn_tiles >= 0) nn.push_back(r.nn_tiles);
            if (r.nn_cost >= 0) nnc.push_back(r.nn_cost);
            if (r.shell && r.nn_carve_tiles >= 0) shell_carve.push_back(r.nn_carve_tiles);
        }
        print_dist("catchment tiles (all)", summarise(tiles));
        print_dist("catchment tiles (land)", summarise(land));
        print_dist("catchment population (thousands)", summarise(pop));
        print_dist("nearest centre, grid tiles", summarise(nn));
        if (traversal) print_dist("nearest centre, traversal cost", summarise(nnc));
        print_dist("shell -> nearest carve centre, tiles", summarise(shell_carve));
        std::printf("    zero-tile markets %d, of them shadowed twins %d   | %d centre tiles carry 2+ "
                    "markets; shadowed: carve under a shell %d, carve under a carve %d, shell under a "
                    "shell %d\n",
                    rec.zero_tile, rec.zero_tile_shadowed, rec.shared_tiles, rec.carve_under_shell,
                    rec.carve_under_carve, rec.shell_under_shell);
        std::printf("    tiles the wrapped (physical) nearest centre would route elsewhere than "
                    "market_for_tile: %d of %d\n", rec.seam_misroutes, rec.home_tiles);
    }
    {
        char rb[200];
        std::snprintf(rb, sizeof rb, "the catchment raster routes as the scan does: %d of %d tiles differ",
                      rec.raster_mismatch, rec.home_tiles);
        check(rec.raster_mismatch == 0, "R1", rb);
        // THE INDEPENDENT ROUTING CHECK: this harness's own wrapped-nearest over
        // every ORIGINAL centre (standing and folded) and its own read of the
        // fold map, against market_for_tile.
        std::snprintf(rb, sizeof rb, "independent routing (own wrapped nearest over original centres, "
                                     "then the fold map) agrees with market_for_tile: %d of %d differ",
                      rec.seam_misroutes, rec.home_tiles);
        check(rec.seam_misroutes == 0, "R2", rb);
    }
    std::printf("    CLEARS NOTHING over the first play year (%d ticks): %d of %d (%.1f%%)"
                "  — shells %d/%d, carve %d/%d\n",
                live_ticks, rec.idle_play, rec.home_markets,
                rec.home_markets ? 100.0 * rec.idle_play / rec.home_markets : 0.0,
                rec.idle_play_shell, rec.shells, rec.idle_play_carve,
                rec.home_markets - rec.shells);
    std::printf("    active markets per quarter:");
    for (const int n : rec.active_per_quarter) std::printf(" %d", n);
    std::printf("   | idle through the settle too: %d   | no building in catchment: %d "
                "(%d of them idle)\n", rec.idle_both, rec.no_building, rec.no_building_idle);
    std::printf("    exchange ring: largest single clear %zu rows, rows lost settle %zu / play %zu"
                "%s\n", rec.max_rows_clear, rec.lost_rows_settle, rec.lost_rows_play,
                rec.lost_rows_play ? "  (idle share is an UPPER bound)" : "");

    // The smallest N by catchment tiles (every market), then — because the zero-
    // tile twins fill that list on a dense seed — the smallest N by tiles and by
    // population among the markets that route at least one tile.
    const auto print_smallest = [&](const char* by, bool with_tiles_only, auto key) {
        std::vector<const market_row*> v;
        for (const market_row& r : rec.rows)
            if (!with_tiles_only || r.tiles > 0) v.push_back(&r);
        std::sort(v.begin(), v.end(), [&](const market_row* a, const market_row* b) {
            const double ka = key(*a), kb = key(*b);
            if (ka != kb) return ka < kb;
            return a->id < b->id;
        });
        std::printf("    smallest %d by %s%s:\n", list_n, by,
                    with_tiles_only ? " (markets routing at least one tile)" : "");
        std::printf("      market   src    tiles  land   pop(k)  ctrs  bldg  nn-tiles  nn-cost   "
                    "play-vol  settle-vol  notes\n");
        for (int i = 0; i < list_n && i < static_cast<int>(v.size()); ++i)
        {
            const market_row& r = *v[static_cast<std::size_t>(i)];
            std::printf("      %-8u %-5s %6d %5d %8" PRId64 " %5d %5d %9.1f %8.1f %10.1f %11.1f",
                        r.id, r.shell ? "shell" : "carve", r.tiles, r.land_tiles, r.pop_k,
                        r.centres, r.buildings, r.nn_tiles, r.nn_cost, r.play_volume,
                        r.settle_volume);
            if (r.shadowed_by != null_entity)
                std::printf("  twin of %s %u", r.shadow_is_shell ? "shell" : "carve", r.shadowed_by);
            if (r.shell)
                std::printf("  shell %d / %s%s", r.close, fate_name(r.fate),
                            r.living_capital_1960 ? " (a living capital at 1960)" : "");
            std::printf("\n");
        }
    };
    print_smallest("catchment tiles", false,
                   [](const market_row& r) { return static_cast<double>(r.tiles); });
    print_smallest("catchment tiles", true,
                   [](const market_row& r) { return static_cast<double>(r.tiles); });
    print_smallest("catchment population", true,
                   [](const market_row& r) { return static_cast<double>(r.pop_k); });

    std::printf("[M2] straddle — corporations whose home-body buildings route to two or more markets\n");
    print_straddle("laid", rec.at_land);
    print_straddle("handoff", rec.at_handoff);
    std::printf("[M2] the search's own validation (%s world: %d live axes x %d rounds)\n",
                rec.sv.budget_world ? "budget" : "no-budget", rec.sv.live_axes, rec.sv.rounds);
    std::snprintf(buf, sizeof buf, "evaluations %d == 1 + %d x %d", rec.sv.evaluations,
                  rec.sv.rounds, rec.sv.live_axes);
    check(rec.sv.v1, "V1", buf);
    std::snprintf(buf, sizeof buf, "path holds %d steps == %d x %d", rec.sv.path, rec.sv.rounds,
                  rec.sv.live_axes);
    check(rec.sv.v2, "V2", buf);
    std::snprintf(buf, sizeof buf, "winner %.6f never below the seed candidate %.6f",
                  rec.sv.winner_composite, rec.sv.seed_composite);
    check(rec.sv.v3, "V3", buf);
    std::snprintf(buf, sizeof buf, "%d accepted steps, each a strict improvement; the winner is the "
                                   "last one", rec.sv.accepted);
    check(rec.sv.v4, "V4", buf);
    std::snprintf(buf, sizeof buf, "the winner re-laid on the base re-scores bit-identically "
                                   "(%.6f vs %.6f, %d scored markets)",
                  rec.sv.relaid_composite, rec.sv.winner_composite, rec.sv.score_markets);
    check(rec.sv.v5, "V5", buf);
    std::printf("  [%s] V6  (reported) the world as handed over, after the second recipe pass, "
                "re-scores %.6f vs the search's %.6f (%d scored markets)\n",
                rec.sv.v6_same ? "SAME" : "DIFFERS", rec.sv.laid_composite, rec.sv.winner_composite,
                rec.sv.laid_markets);
    std::fflush(stdout);

    rec.ok = fixture_ok && rec.id_tile_match && rec.price_agrees && rec.count_agrees
          && rec.fate[3] == 0 && rec.index_unstable == 0 && rec.sv.all();
    return rec;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    int  live_ticks = 4;
    bool traversal  = true;
    int  list_n     = 10;
    bool from_args  = false;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc)      { seeds = parse_seed_list(argv[++a]); from_args = true; }
        else if (s == "--live-ticks" && a + 1 < argc) live_ticks = std::max(1, std::atoi(argv[++a]));
        else if (s == "--no-traversal")          traversal = false;
        else if (s == "--list" && a + 1 < argc)  list_n = std::max(1, std::atoi(argv[++a]));
        else
        {
            std::printf("usage: market_census [--seeds a,b,c] [--live-ticks N] [--no-traversal] [--list N]\n");
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

    std::printf("market_census — BL-1125 M1 (the market census) and BL-1003 R6 (the straddle and "
                "the search's validation)\n");
    std::printf("world: the shipped arc (world_params{}, the 1960 close), built in app order as "
                "player_seed_sweep builds it; a %d-tick play year after the seat\n", live_ticks);
    std::printf("seeds (%s, %zu):", from_args ? "--seeds" : "docs/generation/seed_library.json",
                seeds.size());
    for (const uint32_t s : seeds) std::printf(" %u", s);
    std::printf("\n");
    std::fflush(stdout);

    lua_state lua; // one long-lived state, as the app keeps m_lua
    const clk::time_point t_all = clk::now();
    std::vector<seed_record> recs;
    for (const uint32_t seed : seeds)
        recs.push_back(run_seed(lua, seed, live_ticks, traversal, list_n));

    // --- the tables -------------------------------------------------------------
    std::printf("\n=== M1 per seed: markets by source, and the shells' 1960 fate ===\n");
    std::printf("seed  markets  shells  carve  junc  | shells@1200 @1660 @1960 | maker holds  moved  "
                "dead | lost->no living cap  successor | off-world\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %6d  %5d  %4" PRId64 "  | %11d %5d %5d | %11d %6d %5d | %19d %10d | %d\n",
                    r.seed, r.home_markets, r.shells, r.carve, r.junction_report,
                    r.shells_by_close[0], r.shells_by_close[1], r.shells_by_close[2],
                    r.fate[0], r.fate[1], r.fate[2], r.lost_living_capital, r.lost_successor,
                    r.offworld_markets_year);

    std::printf("\n=== M1 per seed: catchments, activity, spacing ===\n");
    std::printf("seed  tiles min/med/max     pop(k) min/med/max        zero-tile  idle %%(play yr)  "
                "idle shell/carve  nn-tiles med/min  nn-cost med  ring-lost  twins c<s/c<c  no-bldg\n");
    for (const seed_record& r : recs)
    {
        std::vector<double> t, pp, nn, nc;
        for (const market_row& m : r.rows)
        {
            t.push_back(m.tiles);
            pp.push_back(static_cast<double>(m.pop_k));
            if (m.nn_tiles >= 0) nn.push_back(m.nn_tiles);
            if (m.nn_cost >= 0) nc.push_back(m.nn_cost);
        }
        const dist5 dt = summarise(t), dp = summarise(pp), dn = summarise(nn), dc = summarise(nc);
        std::printf("%4u  %5.0f/%5.0f/%6.0f   %6.0f/%7.0f/%8.0f   %9d  %14.1f  %7d/%-7d   %6.1f/%-6.1f  %11.1f  %9zu  %6d/%-6d  %7d\n",
                    r.seed, dt.min, dt.med, dt.max, dp.min, dp.med, dp.max, r.zero_tile,
                    r.home_markets ? 100.0 * r.idle_play / r.home_markets : 0.0,
                    r.idle_play_shell, r.idle_play_carve, dn.med, dn.min, dc.med,
                    r.lost_rows_play, r.carve_under_shell, r.carve_under_carve, r.no_building);
    }

    std::printf("\n=== BL-1125 per seed: how markets died, conservation, major cities ===\n");
    std::printf("seed  markets  shells(marked)  conquest E/X/I   cleared  twins  gravity  shells-folded  "
                "goods before->after        tiles moved->misrouted major  no-mkt\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %7d  %6d(%5d)  %4" PRId64 "/%3" PRId64 "/%3" PRId64 "  %7d  %5" PRId64 "  %7" PRId64
                    "  %13d  %11.1f->%-11.1f  %9" PRId64 "->%-9" PRId64 "  %5d  %9d\n",
                    r.seed, r.home_markets, r.shells, r.shells_marked,
                    r.conquest_destroyed[0], r.conquest_destroyed[1], r.conquest_destroyed[2],
                    r.marks_cleared, r.folded_twins, r.folded_gravity, r.shells_folded,
                    r.goods_before, r.goods_after, r.tiles_moved, r.misrouted,
                    r.major_cities, r.major_no_market);

    std::printf("\n=== M2 per seed: straddlers (home-body buildings over 2+ markets) and the search ===\n");
    std::printf("seed  laid: corps straddle  max | handoff: corps straddle  bg  spec  max  player | V1-V5  "
                "V6 laid-world score\n");
    for (const seed_record& r : recs)
        std::printf("%4u  %11d %8d %4d | %14d %8d %3d %5d %4d %7d | %-5s  %s (%.6f vs %.6f)\n",
                    r.seed, r.at_land.corps_home, r.at_land.straddlers, r.at_land.max_span,
                    r.at_handoff.corps_home, r.at_handoff.straddlers, r.at_handoff.bg_straddlers,
                    r.at_handoff.spec_straddlers, r.at_handoff.max_span, r.at_handoff.player_span,
                    r.sv.all() ? "PASS" : "FAIL", r.sv.v6_same ? "same" : "DIFFERS",
                    r.sv.laid_composite, r.sv.winner_composite);

    // --- pooled -----------------------------------------------------------------
    std::printf("\n=== POOLED over %zu seeds ===\n", recs.size());
    {
        long long mk = 0, sh = 0, cv = 0, jn = 0, c12 = 0, c16 = 0, c19 = 0, f0 = 0, f1 = 0, f2 = 0,
                  lnl = 0, lsu = 0, idle = 0, idle_s = 0, idle_c = 0, zt = 0, col = 0, nob = 0,
                  seam = 0, ht = 0, stc_l = 0, st_l = 0, stc_h = 0, st_h = 0, idle_both = 0,
                  shared = 0, cus = 0, cuc = 0, sus = 0, zts = 0, nob_idle = 0;
        std::map<int, int> hist_h, hist_l;
        std::vector<double> t, land, pp, nn, nc, spop, cpop, stiles, ctiles;
        for (const seed_record& r : recs)
        {
            mk += r.home_markets; sh += r.shells; cv += r.carve; jn += r.junction_report;
            c12 += r.shells_by_close[0]; c16 += r.shells_by_close[1]; c19 += r.shells_by_close[2];
            f0 += r.fate[0]; f1 += r.fate[1]; f2 += r.fate[2];
            lnl += r.lost_living_capital; lsu += r.lost_successor;
            idle += r.idle_play; idle_s += r.idle_play_shell; idle_c += r.idle_play_carve;
            idle_both += r.idle_both;
            zt += r.zero_tile; col += r.colocated; nob += r.no_building;
            shared += r.shared_tiles; cus += r.carve_under_shell; cuc += r.carve_under_carve;
            sus += r.shell_under_shell; zts += r.zero_tile_shadowed; nob_idle += r.no_building_idle;
            seam += r.seam_misroutes; ht += r.home_tiles;
            stc_l += r.at_land.corps_home; st_l += r.at_land.straddlers;
            stc_h += r.at_handoff.corps_home; st_h += r.at_handoff.straddlers;
            for (const auto& [k, n] : r.at_handoff.hist) hist_h[k] += n;
            for (const auto& [k, n] : r.at_land.hist) hist_l[k] += n;
            for (const market_row& m : r.rows)
            {
                t.push_back(m.tiles);
                land.push_back(m.land_tiles);
                pp.push_back(static_cast<double>(m.pop_k));
                (m.shell ? spop : cpop).push_back(static_cast<double>(m.pop_k));
                (m.shell ? stiles : ctiles).push_back(m.tiles);
                if (m.nn_tiles >= 0) nn.push_back(m.nn_tiles);
                if (m.nn_cost >= 0) nc.push_back(m.nn_cost);
            }
        }
        const double n = static_cast<double>(std::max<std::size_t>(1, recs.size()));
        // BL-1125: the junction count is the carve's, taken BEFORE the folds,
        // so it is no longer a share of the carve that stands.
        std::printf("  markets %lld (%.1f a seed): shells %lld (%.1f%%), carve %lld (%.1f%%); "
                    "junction-lowered at the carve, before the folds, %lld\n",
                    mk, mk / n, sh, mk ? 100.0 * sh / mk : 0.0, cv, mk ? 100.0 * cv / mk : 0.0, jn);
        std::printf("  shells marked at 1200 %lld, 1660 %lld, 1960 %lld\n", c12, c16, c19);
        std::printf("  shell fate at 1960: maker holds %lld, maker alive capital elsewhere %lld, maker "
                    "dead %lld; of the lost %lld: no living capital %lld, a successor's %lld\n",
                    f0, f1, f2, f1 + f2, lnl, lsu);
        print_dist("catchment tiles (all)", summarise(t));
        print_dist("catchment tiles (land)", summarise(land));
        print_dist("catchment tiles, shells", summarise(stiles));
        print_dist("catchment tiles, carve", summarise(ctiles));
        print_dist("catchment population (k)", summarise(pp));
        print_dist("catchment population (k), shells", summarise(spop));
        print_dist("catchment population (k), carve", summarise(cpop));
        print_dist("nearest centre, grid tiles", summarise(nn));
        if (traversal) print_dist("nearest centre, traversal cost", summarise(nc));
        std::printf("  zero-tile markets %lld (%lld of them shadowed twins); %lld centre tiles carry 2+ "
                    "markets (%lld markets co-located); shadowed: carve under a shell %lld, carve under "
                    "a carve %lld, shell under a shell %lld\n",
                    zt, zts, shared, col, cus, cuc, sus);
        std::printf("  markets with no building in their catchment at the handoff %lld (%lld of them idle "
                    "over the play year); tiles the wrapped nearest routes elsewhere %lld of %lld\n",
                    nob, nob_idle, seam, ht);
        std::printf("  CLEARS NOTHING over the play year: %lld of %lld (%.1f%%) — shells %lld of %lld "
                    "(%.1f%%), carve %lld of %lld (%.1f%%); idle through the settle as well: %lld\n",
                    idle, mk, mk ? 100.0 * idle / mk : 0.0, idle_s, sh, sh ? 100.0 * idle_s / sh : 0.0,
                    idle_c, mk - sh, (mk - sh) ? 100.0 * idle_c / (mk - sh) : 0.0, idle_both);
        std::printf("  STRADDLERS laid %lld of %lld (%.1f%%); at the handoff %lld of %lld (%.1f%%)\n",
                    st_l, stc_l, stc_l ? 100.0 * st_l / stc_l : 0.0, st_h, stc_h,
                    stc_h ? 100.0 * st_h / stc_h : 0.0);
        std::printf("  markets spanned -> corps, laid:");
        for (const auto& [k, c] : hist_l) std::printf("  %d:%d", k, c);
        std::printf("\n  markets spanned -> corps, handoff:");
        for (const auto& [k, c] : hist_h) std::printf("  %d:%d", k, c);
        std::printf("\n");
    }
    int bad = 0;
    for (const seed_record& r : recs) if (!r.ok) ++bad;
    std::printf("\nrun time %.1f s over %zu seeds; %d check failure(s); %d seed(s) not clean\n",
                secs_since(t_all), recs.size(), g_failures, bad);
    std::printf("%s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
