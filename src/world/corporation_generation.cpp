#include "corporation_generation.hpp"

#include "world/hard_coded_world.hpp" // generation_progress — the BL-305 tap

#include "world/economy_system.hpp"
#include "world/placement_rules.hpp"
#include "world/settlement.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <random>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Internal helpers — all in anonymous namespace
// ---------------------------------------------------------------------------

namespace {

// ---------------------------------------------------------------------------
// Pass 1 helpers — nation assignment
// ---------------------------------------------------------------------------

/// Compute a weight for assigning a new corporation to each nation.
/// Nations whose economic_focus matches the general biases of a corporate
/// economy receive higher base weight; a balancing term penalises nations that
/// already host many corporations, preventing a single nation from monopolising
/// the entire corporate population.
///
/// @param nc           Nation component to evaluate.
/// @param corps_in_nation Number of corporations already assigned to this nation.
/// @param total_corps  Total corporations placed so far (used for relative balance).
/// @param rng          Seeded RNG for a small jitter to break ties.
/// @return             A non-negative weight; higher means more likely to be chosen.
float nation_weight(const nation_component& nc,
                    int corps_in_nation,
                    int total_corps,
                    std::mt19937& rng)
{
    // Base weight from economic focus — all foci are viable but trade and
    // extraction are slightly preferred because they attract the most distinct
    // corporate archetypes.
    float base = 1.0f;
    switch (nc.focus)
    {
        case economic_focus::extraction:  base = 1.3f; break;
        case economic_focus::processing:  base = 1.0f; break;
        case economic_focus::trade:       base = 1.2f; break;
    }

    // Balancing penalty: a nation hosting many corps already is less likely to
    // attract more. Uses a soft 1/(1+k) decay so no nation is entirely excluded.
    const float balance = (total_corps > 0)
        ? 1.0f / (1.0f + static_cast<float>(corps_in_nation))
        : 1.0f;

    // Small jitter prevents perfectly deterministic tie-breaking (which could
    // produce unnatural clustering in symmetric worlds).
    std::uniform_real_distribution<float> jitter(0.95f, 1.05f);

    return base * balance * jitter(rng);
}

/// Choose a home nation for one corporation using weighted sampling over all
/// nations in the world.
///
/// @param w                 World containing the nation map.
/// @param nation_ids        Ordered list of nation entity ids.
/// @param corp_counts       How many corps have been assigned to each nation so
///                          far (parallel to nation_ids).
/// @param total_placed      Total corps placed so far across all nations.
/// @param rng               Seeded RNG.
/// @return                  Index into nation_ids of the chosen nation.
int pick_home_nation(const world& w,
                     const std::vector<entity_id>& nation_ids,
                     const std::vector<int>& corp_counts,
                     int total_placed,
                     std::mt19937& rng)
{
    const int n = static_cast<int>(nation_ids.size());

    // Build a weight array and a cumulative sum for weighted sampling.
    std::vector<float> weights(static_cast<std::size_t>(n));
    float total_w = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const auto it = w.nations.find(nation_ids[static_cast<std::size_t>(i)]);
        if (it == w.nations.end())
        {
            weights[static_cast<std::size_t>(i)] = 0.0f;
            continue;
        }
        const float w_val = nation_weight(
            it->second,
            corp_counts[static_cast<std::size_t>(i)],
            total_placed,
            rng);
        weights[static_cast<std::size_t>(i)] = w_val;
        total_w += w_val;
    }

    if (total_w <= 0.0f)
        return 0; // fallback — pick first nation

    std::uniform_real_distribution<float> draw(0.0f, total_w);
    float cursor = draw(rng);
    for (int i = 0; i < n; ++i)
    {
        cursor -= weights[static_cast<std::size_t>(i)];
        if (cursor <= 0.0f)
            return i;
    }
    return n - 1; // floating-point rounding fallback
}

// ---------------------------------------------------------------------------
// Pass 2 helpers — industrial focus assignment
// ---------------------------------------------------------------------------

/// Draw an industrial_focus for a corporation whose home nation has the given
/// economic_focus. The nation's focus biases toward the matching corporate
/// focus; the running distribution of already-generated foci exerts a mild
/// diversifying pull so there is always a mix.
///
/// @param nation_ef    Home nation's economic_focus.
/// @param focus_counts Running count of each focus already assigned
///                     (indexed: 0=extraction, 1=processing, 2=trade).
/// @param rng          Seeded RNG.
/// @return             The chosen industrial_focus.
industrial_focus pick_focus(economic_focus nation_ef,
                            const std::array<int, 3>& focus_counts,
                            std::mt19937& rng)
{
    // Base probabilities for each focus given the nation's economic character.
    std::array<float, 3> probs = { 1.0f, 1.0f, 1.0f };
    switch (nation_ef)
    {
        case economic_focus::extraction:
            probs = { 2.0f, 0.8f, 0.6f }; break;
        case economic_focus::processing:
            probs = { 0.8f, 2.0f, 0.8f }; break;
        case economic_focus::trade:
            probs = { 0.6f, 0.8f, 2.0f }; break;
    }

    // Diversity nudge: a focus that is already over-represented is modestly
    // down-weighted so the generated set does not collapse to one type.
    const int total_placed = focus_counts[0] + focus_counts[1] + focus_counts[2];
    if (total_placed > 0)
    {
        for (int fi = 0; fi < 3; ++fi)
        {
            const float share = static_cast<float>(focus_counts[static_cast<std::size_t>(fi)])
                              / static_cast<float>(total_placed);
            // Reduce probability proportional to over-representation above 1/3.
            const float excess = share - (1.0f / 3.0f);
            if (excess > 0.0f)
                probs[static_cast<std::size_t>(fi)] *= std::max(0.3f, 1.0f - excess * 1.5f);
        }
    }

    const float total_w = probs[0] + probs[1] + probs[2];
    std::uniform_real_distribution<float> draw(0.0f, total_w);
    const float r = draw(rng);

    if (r < probs[0])
        return industrial_focus::extraction;
    if (r < probs[0] + probs[1])
        return industrial_focus::processing;
    return industrial_focus::trade;
}

// ---------------------------------------------------------------------------
// Pass 3 helpers — starting asset placement
// ---------------------------------------------------------------------------

/// Total resource deposit score for a tile — used to rank tiles by richness
/// when placing extraction sites. (The ocean check and the extractable-deposit
/// helpers now live in the reusable `placement_rules` seam.)
float total_deposit(const tile_component& tc)
{
    float sum = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
        sum += tc.resource_deposit[r];
    return sum;
}

// --- Holdings footprint ------------------------------------------------------
//
// A corporation is an industrial power, not a single shed: it opens with a small
// *cluster* of assets rather than one building. Corporations are specialists, so
// the holdings *count* is shaped by focus rather than a single flat range
// (holdings_range, below): an extractor wants a few deposit tiles (3–4), a
// processor pairs with a little feed (2–3), and a trade operator wants ~one depot
// (1–2). These lean counts keep even the busiest body (eight corps drawing from
// these ranges, ~16–32 tiles total) comfortably inside Kepler's tile budget
// alongside the pre-authored installations, and no corp monopolises its nation's
// land. The mix within that count is shaped by focus (focus_asset_pattern, below)
// and the tiles still cluster around a single focus-scored anchor — the lean
// counts ride on top of the retained anchor/neighbourhood clustering, so a corp's
// holdings read as one contiguous operation, not a scatter across the body.

/// Inclusive holdings-count range a corporation of the given focus opens with.
/// Counts are lean and focus-shaped (specialists, not generalists): extraction
/// corps want a few deposit tiles, processors a little feed, trade operators about
/// one depot. See docs/generation/CORPORATION_GENERATION.md § Pass 3.
///
/// @param focus Corporate industrial focus.
/// @return      {min, max} holdings count, inclusive on both ends.
std::pair<int, int> holdings_range(industrial_focus focus)
{
    switch (focus)
    {
    case industrial_focus::extraction: return { 3, 4 };
    case industrial_focus::processing: return { 2, 3 };
    // BL-435 (2026-08-16, Ben): trade was { 1, 2 }, and a 1-draw opened a corp
    // holding NOTHING BUT A PORT. Ports carry base_rate 0 — they produce nothing —
    // so that corp had no income source at all: not a lean start, a dead one. It
    // reached the player on 3 of 24 seeds (9, 12, 22 read "0 proc, 0 extr").
    // Ben's ruling: "we shouldn't be seeding such corporations as the default one,
    // which has no way of making money at all."
    //
    // A minimum of 2 makes the trade pattern's second slot — an extraction site —
    // guaranteed, so every trade corp can earn from tick one. The 3-draw also
    // reaches the pattern's processing slot, which is why this lifts processor
    // coverage as a side effect rather than by design.
    case industrial_focus::trade:      return { 2, 3 };
    }
    return { 2, 3 }; // defensive default — keep a corp a going concern
}

/// The building-type mix a corporation of the given focus opens with, expressed
/// as the order in which asset slots are filled. The first entry is the anchor
/// (the corp's defining asset — placed on the best-scoring tile); later entries
/// are the supporting mix and repeat to fill out the holdings count. The mix
/// reflects the focus: extraction corps are mostly extraction sites with a single
/// processor to consume some of their own output; processors are mostly
/// processing facilities fed by a couple of their own mines; trade corps centre
/// on ports with light extraction/processing to give the port something to move.
/// See docs/generation/CORPORATION_GENERATION.md § Pass 2 (asset type table).
///
/// @param focus Corporate industrial focus.
/// @return      Ordered building-type slots; index 0 is the anchor type. Cycled
///              (with wrap) when more slots are needed than the pattern lists.
const std::vector<building_type>& focus_asset_pattern(industrial_focus focus)
{
    // BL-435 (2026-08-16): the processor moved from slot 3 to slot 2. It is not a
    // rebalance — it makes this pattern do what the comment above it has always
    // promised. Extraction holdings draw 3-4 (holdings_range), so at slot 3 the
    // "single processor to consume some of their own output" only ever landed on
    // a 4-draw: half of all extraction corps opened as pure mines, and the code
    // silently disagreed with its own documentation.
    //
    // MEASURED, not assumed: across 24 seeds only 2.96 of the 8 selectable
    // specialists (37%) opened with any processing facility, and on one seed in
    // 24 NOT ONE did — so on that seed BL-435's selection screen would have had
    // nothing better to offer. A 4-draw is unchanged (3 extraction + 1 processor);
    // a 3-draw now opens 2 + 1 instead of 3 + 0.
    static const std::vector<building_type> extraction_mix = {
        building_type::extraction_site,     // anchor
        building_type::extraction_site,
        building_type::processing_facility, // consumes some own output
        building_type::extraction_site,
    };
    static const std::vector<building_type> processing_mix = {
        building_type::processing_facility, // anchor
        building_type::processing_facility,
        building_type::extraction_site,     // feeds the processors
        building_type::extraction_site,
    };
    // Trade is DELIBERATELY left alone by BL-435, and the processor at slot 2 is
    // therefore still unreachable (trade holdings draw 1-2). A pure-trade opening
    // — a port and maybe one mine, buying and moving what others make — is a real
    // archetype, and the item's whole point is a choice worth making. Giving all
    // three foci a processor would raise the count by flattening the very variety
    // the selection screen exists to present. The slot stays so that a future
    // widening of trade's holdings range reaches it naturally.
    static const std::vector<building_type> trade_mix = {
        building_type::port,                // anchor
        building_type::extraction_site,
        building_type::processing_facility,
        building_type::port,
    };
    switch (focus)
    {
        case industrial_focus::extraction: return extraction_mix;
        case industrial_focus::processing: return processing_mix;
        case industrial_focus::trade:      return trade_mix;
    }
    return extraction_mix;
}

/// Pick one of seven per-landform weights. A small readability helper so the
/// infrastructure siting preferences below read as a table rather than a switch
/// nested in a switch. Weights are given in `terrain_landform` declaration order.
///
/// @param lf The tile's landform.
/// @return   The weight matching `lf` (1.0 for an unknown value).
float landform_lean(terrain_landform lf,
                    float plains, float highland, float mountain,
                    float canyon, float valley, float crater, float rift)
{
    switch (lf)
    {
        case terrain_landform::plains:   return plains;
        case terrain_landform::highland: return highland;
        case terrain_landform::mountain: return mountain;
        case terrain_landform::canyon:   return canyon;
        case terrain_landform::valley:   return valley;
        case terrain_landform::crater:   return crater;
        case terrain_landform::rift:     return rift;
    }
    return 1.0f;
}

/// How dry and firm a composition's ground is, as a placement multiplier. Used by
/// the launchpad preference: dust and rock are good pad ground, marsh and ice are
/// not. Not a hazard measure — hazard is applied separately for every type.
///
/// @param comp The tile's composition.
/// @return     A positive multiplier, 1.0 for compositions with no strong lean.
float dryness_lean(terrain_composition comp)
{
    switch (comp)
    {
        case terrain_composition::barren:
        case terrain_composition::rocky:
        case terrain_composition::regolith:  return 1.3f;
        case terrain_composition::metallic:
        case terrain_composition::tundra:
        case terrain_composition::grassland: return 1.0f;
        case terrain_composition::forest:    return 0.8f;
        case terrain_composition::volcanic:  return 0.7f;
        case terrain_composition::icy:       return 0.6f;
        case terrain_composition::wetland:   return 0.5f;
        case terrain_composition::ocean:     return 0.001f; // can_place rejects these anyway
    }
    return 1.0f;
}

/// Score one tile for hosting a building of `btype`, higher is better. Mirrors the
/// per-focus preference the single-asset placer used: extraction wants the richest
/// *extractable* deposit (a site must be able to work its tile), processing/port
/// want any workable land but lean mildly toward some deposit nearby; every type
/// shies away from hazardous tiles. Returns a strictly positive score for any
/// non-ocean tile so a candidate is always placeable.
///
/// The three infrastructure types carry a first-cut siting lean (Ben, 2026-08-12,
/// resolving NR-126 — "lazily evaluate these however you like"): launchpads want
/// flat dry ground, logistics hubs flat peopled ground, military bases commanding
/// ground near a population worth garrisoning. No generated world moves today —
/// `focus_asset_pattern` never proposes these types — so the lean is dormant until
/// something places them, and is tuning data rather than a settled design.
///
/// @param tc    The candidate tile.
/// @param btype The building type proposed for it.
/// @return      A positive placement score; larger is more preferred.
float tile_score_for(const tile_component& tc, building_type btype)
{
    float score = 1.0f;
    switch (btype)
    {
        case building_type::extraction_site:
            // Rank by extractable deposit so the site lands where it can work;
            // tiny floor keeps a deposit-poor tile still placeable (it will fail
            // can_place separately and be skipped there).
            score = placement_rules::extractable_deposit(tc) + 0.001f;
            break;
        case building_type::processing_facility:
            // Mild lean toward tiles with some deposit (own feedstock nearby).
            score = total_deposit(tc) * 0.5f + 1.0f;
            break;
        case building_type::launchpad:
            // Flat and dry: a pad wants easy ground and no weather. Plains best,
            // slope penalised hard; dry compositions preferred over wet/icy ones.
            score = landform_lean(tc.landform, 1.6f, 1.0f, 0.35f, 0.5f, 1.1f, 0.9f, 0.4f);
            score *= dryness_lean(tc.composition);
            break;
        case building_type::inland_logistics_hub:
            // Flat and peopled: a depot wants ground a road can cross and traffic
            // worth carrying, so it leans on habitability rather than on deposits.
            score = landform_lean(tc.landform, 1.5f, 1.0f, 0.3f, 0.5f, 1.3f, 0.7f, 0.6f);
            score *= (0.7f + tc.habitability * 0.6f);
            break;
        case building_type::military_base:
            // Commanding ground near something worth garrisoning: elevation is an
            // advantage here rather than a cost, but an empty peak is not a posting.
            score = landform_lean(tc.landform, 1.0f, 1.5f, 1.2f, 1.1f, 0.9f, 1.0f, 1.0f);
            score *= (0.8f + tc.habitability * 0.4f);
            break;
        case building_type::port:
        case building_type::none:
            // Deliberately neutral — ports are already sited by can_place's coastal
            // test, and `none` is not a real sited type.
            score = 1.0f;
            break;
    }
    // Avoid volcanic hellscapes for every type.
    score *= (1.0f - tc.hazard_level * 0.3f);
    return std::max(score, 0.001f);
}

/// Author a building of `btype` onto `tid` for the world, including the extraction
/// target (richest extractable resource on the tile) for extraction sites. Marks
/// the tile occupied. Assumes the tile already passed `placement_rules::can_place`.
///
/// @param w     World to write the new entity into.
/// @param tid   Tile to place on (must be a valid, unoccupied land tile).
/// @param btype Building type to author.
/// @param occupied_tiles Occupancy set; `tid` is inserted.
/// @return      Entity id of the created building.
entity_id author_building(world& w,
                          entity_id tid,
                          building_type btype,
                          std::unordered_set<entity_id>& occupied_tiles)
{
    const entity_id bld_id = w.create_entity();

    building_component bc;
    bc.tile               = tid;
    bc.type               = btype;
    // Staff producing buildings so the Layer 3 economy runs from the authored
    // assets (an unstaffed building produces nothing). Ports take no production
    // action in L3, so they stay unstaffed; military_base is passive muster
    // infrastructure (BL-325) and produces nothing either, so it matches — see
    // construction.cpp's own zero-staff condition, which lists both.
    bc.workforce_assigned =
        (btype == building_type::port || btype == building_type::military_base
         || btype == building_type::research_institute) ? 0.0f : 0.5f; // BL-332: passive, like military_base

    if (btype == building_type::extraction_site)
    {
        const auto tit = w.tiles.find(tid);
        if (tit != w.tiles.end())
        {
            bool any = false;
            bc.target_resource = placement_rules::richest_extractable(tit->second, any);
        }
    }
    // Processing recipes are authored later from the loaded registry (the recipe id
    // is a registry index, unknown here); the field stays no_recipe until
    // app::load_economy assigns the default.

    w.buildings[bld_id]  = bc;
    w.stockpiles[bld_id] = stockpile_component{};
    occupied_tiles.insert(tid);
    return bld_id;
}

/// Place a clustered set of starting buildings for one corporation inside its home
/// nation's territory. An *anchor* tile is chosen first (focus-weighted, richest
/// for extraction); the remaining assets are placed on the valid, unoccupied tiles
/// nearest the anchor in grid space, so a corporation's holdings group spatially
/// rather than scatter across the body. Every placed asset is validated with
/// `placement_rules::can_place` (never ocean; an extraction site only on a tile
/// carrying a matching extractable deposit) — tiles that fail are skipped, so a
/// deposit-poor anchor neighbourhood simply yields fewer extraction sites.
///
/// BL-283 constrains the ANCHOR search to the corporation's home region via
/// @p anchor_window; the neighbourhood the remaining slots walk stays national, so
/// a corp whose own region is small spills into the ground next door rather than
/// opening short. The window is a *subset of `home_nation.tiles` in the same order*,
/// so nothing about the search's determinism changes.
///
/// @param w              World to write the new entities into.
/// @param home_nation    The nation_component of the home nation.
/// @param focus          Corporate industrial focus (determines the asset mix and tile preference).
/// @param occupied_tiles Tiles already taken by another building; never reused.
/// @param rng            Seeded RNG for the anchor weighted draw and holdings count.
/// @param anchor_window  Tiles the anchor may sit on; null/empty means the whole nation.
/// @return               Entity ids of every building placed (may be empty when the
///                       window holds no tile that can host the anchor type — the
///                       caller's cue to widen a rung).
std::vector<entity_id> place_starting_assets(world& w,
                                             const nation_component& home_nation,
                                             industrial_focus focus,
                                             std::unordered_set<entity_id>& occupied_tiles,
                                             std::mt19937& rng,
                                             const std::vector<entity_id>* anchor_window = nullptr)
{
    std::vector<entity_id> placed;
    if (home_nation.tiles.empty())
        return placed;

    const std::vector<entity_id>& anchor_pool =
        (anchor_window && !anchor_window->empty()) ? *anchor_window : home_nation.tiles;

    const std::vector<building_type>& pattern = focus_asset_pattern(focus);
    const building_type anchor_type = pattern.front();

    // --- choose the anchor tile (weighted by anchor-type score) ---------------
    // Candidates are the window's non-ocean, unoccupied tiles that can validly host
    // the anchor type. The nation's `tiles` vector is stored in a stable order and the
    // window preserves it, so building the candidate list by iterating is deterministic.
    struct candidate { entity_id tid; float score; };
    std::vector<candidate> anchors;
    anchors.reserve(anchor_pool.size());
    for (entity_id tid : anchor_pool)
    {
        if (occupied_tiles.count(tid))
            continue;
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;
        const tile_component& tc = it->second;
        // An extraction anchor must sit on a workable deposit; for processing/port
        // anchors can_place reduces to "non-ocean land".
        bool any = false;
        const resource_type tgt = placement_rules::richest_extractable(tc, any);
        if (!placement_rules::can_place(tc, anchor_type, tgt))
            continue;
        anchors.push_back({ tid, tile_score_for(tc, anchor_type) });
    }
    if (anchors.empty())
        return placed; // nothing anchorable in this window — the caller widens, or the
                       // corp opens asset-light once the nation rung is also empty.
                       // No RNG has been consumed on this path, so a widened retry is
                       // indistinguishable from having started at the wider rung.

    float total_w = 0.0f;
    for (const auto& c : anchors)
        total_w += c.score;

    entity_id anchor_tid = anchors.back().tid;
    std::uniform_real_distribution<float> draw(0.0f, total_w);
    float cursor = draw(rng);
    for (const auto& c : anchors)
    {
        cursor -= c.score;
        if (cursor <= 0.0f) { anchor_tid = c.tid; break; }
    }

    const tile_component& anchor_tc = w.tiles.at(anchor_tid);
    const int anchor_x = anchor_tc.grid_x;
    const int anchor_y = anchor_tc.grid_y;

    placed.push_back(author_building(w, anchor_tid, anchor_type, occupied_tiles));

    // --- order the rest of the nation's tiles by distance to the anchor -------
    // Squared grid distance keeps holdings contiguous; ties break on tile id so
    // the order is fully deterministic. We only need the cluster neighbourhood, so
    // the whole sorted list is the search space for the remaining asset slots.
    struct ring { long long dist2; entity_id tid; };
    std::vector<ring> neighbourhood;
    neighbourhood.reserve(home_nation.tiles.size());
    for (entity_id tid : home_nation.tiles)
    {
        if (tid == anchor_tid || occupied_tiles.count(tid))
            continue;
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;
        if (placement_rules::is_ocean_tile(it->second.composition))
            continue;
        const long long dx = it->second.grid_x - anchor_x;
        const long long dy = it->second.grid_y - anchor_y;
        neighbourhood.push_back({ dx * dx + dy * dy, tid });
    }
    std::sort(neighbourhood.begin(), neighbourhood.end(),
              [](const ring& a, const ring& b) {
                  if (a.dist2 != b.dist2) return a.dist2 < b.dist2;
                  return a.tid < b.tid;
              });

    // --- holdings count and the remaining slots -------------------------------
    const auto [count_min, count_max] = holdings_range(focus);
    std::uniform_int_distribution<int> count_pick(count_min, count_max);
    const int target_count = count_pick(rng);

    std::size_t cursor_tile = 0;
    int slot = 1; // slot 0 was the anchor
    while (static_cast<int>(placed.size()) < target_count
           && cursor_tile < neighbourhood.size())
    {
        const building_type btype =
            pattern[static_cast<std::size_t>(slot) % pattern.size()];

        // Walk outward from the anchor for the next tile that can host this type.
        bool placed_this_slot = false;
        for (; cursor_tile < neighbourhood.size(); ++cursor_tile)
        {
            const entity_id tid = neighbourhood[cursor_tile].tid;
            if (occupied_tiles.count(tid))
                continue;
            const tile_component& tc = w.tiles.at(tid);
            bool any = false;
            const resource_type tgt = placement_rules::richest_extractable(tc, any);
            if (!placement_rules::can_place(tc, btype, tgt))
                continue; // e.g. extraction slot on a deposit-poor tile — try next ring
            placed.push_back(author_building(w, tid, btype, occupied_tiles));
            ++cursor_tile;
            placed_this_slot = true;
            break;
        }
        if (!placed_this_slot)
            break; // ran out of valid tiles in the neighbourhood
        ++slot;
    }

    return placed;
}

/// The grid width of the body a nation's territory sits on, or 0 when it has no
/// resolvable tiles. Needed because `nearest_region` works in raster space and
/// the column axis wraps.
int nation_grid_width(const world& w, const nation_component& home_nation)
{
    for (entity_id tid : home_nation.tiles)
    {
        const auto t = w.tiles.find(tid);
        if (t == w.tiles.end()) continue;
        const auto b = w.bodies.find(t->second.body);
        if (b == w.bodies.end()) continue;
        return b->second.grid_width;
    }
    return 0;
}

/// The nation's tiles that fall inside any of @p accepted, in the nation's own
/// stored order. "Inside a region" is `nearest_region` — regions are anchor
/// points, not stored tile sets, so the nearest anchor IS the region a tile
/// belongs to (the same rule the settlement pass itself reads by).
std::vector<entity_id> region_window(const world& w,
                                       const nation_component& home_nation,
                                       const settlement_state& ss,
                                       const std::vector<int>& accepted,
                                       int gw)
{
    std::vector<entity_id> window;
    if (gw <= 0 || accepted.empty())
        return window;
    window.reserve(home_nation.tiles.size() / 4 + 1);
    for (entity_id tid : home_nation.tiles)
    {
        const auto t = w.tiles.find(tid);
        if (t == w.tiles.end()) continue;
        const int pi = nearest_region(ss, t->second.grid_x, t->second.grid_y, gw);
        if (pi < 0) continue;
        if (std::find(accepted.begin(), accepted.end(), pi) != accepted.end())
            window.push_back(tid);
    }
    return window;
}

/// The @p k regions of the same nation whose anchors lie nearest @p home_pi,
/// @p home_pi itself first. The second rung of BL-283's fallback ladder: a corp
/// whose own region cannot host its anchor looks next door before it gives up
/// on the region meaning anything at all. Column-wrapped distance; ties break on
/// the lower region index, so the result is a pure function of the settlement
/// record.
std::vector<int> home_and_nearest_regions(const settlement_state& ss,
                                            int home_pi,
                                            int nation_idx,
                                            int gw,
                                            int k)
{
    std::vector<int> out{ home_pi };
    if (home_pi < 0 || home_pi >= static_cast<int>(ss.regions.size()) || gw <= 0)
        return out;

    const region& home = ss.regions[static_cast<std::size_t>(home_pi)];

    struct near { long long d2; int pi; };
    std::vector<near> others;
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        if (static_cast<int>(i) == home_pi) continue;
        const region& p = ss.regions[i];
        if (p.nation != nation_idx) continue;
        long long dc = std::abs(p.col - home.col);
        if (dc > gw / 2) dc = gw - dc;
        const long long dr = p.row - home.row;
        others.push_back({ dc * dc + dr * dr, static_cast<int>(i) });
    }
    std::sort(others.begin(), others.end(), [](const near& a, const near& b) {
        if (a.d2 != b.d2) return a.d2 < b.d2;
        return a.pi < b.pi;
    });
    for (int i = 0; i < k && i < static_cast<int>(others.size()); ++i)
        out.push_back(others[static_cast<std::size_t>(i)].pi);
    return out;
}

// ---------------------------------------------------------------------------
// Pass 4 helpers — financial profile
// ---------------------------------------------------------------------------

/// Compute starting capital for one corporation.
/// The base is scaled by a seeded factor in [1−variance, 1+variance]; corps
/// focused on processing or trade receive an additional ×1.15 premium.
///
/// @param base_capital  Campaign-wide baseline capital.
/// @param variance      Fractional spread (e.g. 0.4 → ±40 %).
/// @param focus         Corporate industrial focus.
/// @param rng           Seeded RNG.
/// @return              Final starting capital value.
float compute_capital(float base_capital,
                      float wealth_variance,
                      industrial_focus focus,
                      std::mt19937& rng)
{
    std::uniform_real_distribution<float> spread(
        1.0f - wealth_variance, 1.0f + wealth_variance);

    float capital = base_capital * spread(rng);

    // Processing and trade corps pay more to operate (no direct resource access)
    // and start with a slight premium to compensate.
    if (focus == industrial_focus::processing || focus == industrial_focus::trade)
        capital *= 1.15f;

    return capital;
}

/// The opening resource stockpile a corporation is seeded with on its home body,
/// so build / production / trade have materials from turn one rather than an
/// empty pool warmed only by the pre-game ticks (CORPORATION_GENERATION.md
/// § Pre-game operating history; addresses the construction deadlock a712b05).
///
/// BL-116 — generated from industrial focus + financial profile (replaces the
/// BL-115 fixed give). Per-focus weights over the seven-resource prototype
/// subset (RESOURCES.md) read as "who holds what at the gate": extraction
/// hoards the raws it mines; processing pairs feedstock with refined output;
/// trade carries finished goods and thinner raws. Magnitude scales with
/// starting capital; a seeded per-resource jitter varies it without disturbing
/// the focus ordering. Deterministic: draws from `rng` once per prototype
/// resource in a fixed order (every corp advances the stream identically).
std::array<float, resource_count> generate_starting_stockpile(
    industrial_focus focus, float capital, float base_capital, std::mt19937& rng)
{
    // Four raws (iron ore, petroleum, water, agricultural produce) then three
    // refined (steel, refined fuel, food rations). Columns: extraction /
    // processing / trade weights; weight 1.0 → ~base_units before scaling.
    struct focus_stock { resource_type res; float extraction, processing, trade; };
    static const focus_stock table[] = {
        { resource_type::iron_ore,             2.0f, 1.0f, 0.5f },
        { resource_type::petroleum,            1.5f, 0.8f, 0.5f },
        { resource_type::water,                1.5f, 0.5f, 0.5f },
        { resource_type::agricultural_produce, 1.5f, 0.8f, 0.5f },
        { resource_type::steel,                0.3f, 1.2f, 1.2f },
        { resource_type::refined_fuel,         0.3f, 1.0f, 1.2f },
        { resource_type::food_rations,         0.3f, 0.8f, 1.0f },
    };

    constexpr float base_units = 100.0f;
    float cap_scalar = (base_capital > 0.0f) ? capital / base_capital : 1.0f;
    if (cap_scalar < 0.5f) cap_scalar = 0.5f;   // clamp the wealth spread so a
    if (cap_scalar > 2.0f) cap_scalar = 2.0f;   // poor/rich corp stays sane

    std::uniform_real_distribution<float> jitter(0.85f, 1.15f);

    std::array<float, resource_count> s = {};
    for (const focus_stock& fs : table)         // fixed order → deterministic draws
    {
        const float w = (focus == industrial_focus::extraction) ? fs.extraction
                      : (focus == industrial_focus::processing) ? fs.processing
                      :                                            fs.trade;
        const float j = jitter(rng);            // drawn for every resource, in order
        s[static_cast<std::size_t>(fs.res)] = base_units * w * cap_scalar * j;
    }
    return s;
}

/// The body a corporation opens on: the body of its first placed asset. Holdings
/// cluster to one nation's territory (a single body in the prototype), so any
/// asset resolves the home body. Returns null_entity if the corp placed nothing
/// (a deposit-poor nation may yield zero holdings) — such a corp gets no pool.
entity_id corp_home_body(const world& w, const std::vector<entity_id>& assets)
{
    for (const entity_id b : assets)
    {
        const auto bit = w.buildings.find(b);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end())
            return tit->second.body;
    }
    return null_entity;
}

// ---------------------------------------------------------------------------
// Pass 5 helpers — procedural naming
// ---------------------------------------------------------------------------

// Corporate name parts — distinct from the nation phoneme pools so corporation
// and nation names feel different even though they share the same structural
// approach.

static const char* const k_corp_onsets[] = {
    "Aex", "Bor", "Cal", "Dyn", "Exo", "Far", "Gen", "Hex", "Int", "Jor",
    "Kal", "Lux", "Mar", "Nex", "Orb", "Pax", "Qua", "Rex", "Sol", "Ter",
    "Ulv", "Vec", "Wen", "Xer", "Yel", "Zen", "Ath", "Bry", "Cor", "Del",
};
static constexpr int k_corp_onset_count = 30;

static const char* const k_corp_suffixes[] = {
    "ex", "an", "or", "ix", "on", "ar", "us", "is", "el", "en",
    "yx", "ax", "om", "os", "ur",
};
static constexpr int k_corp_suffix_count = 15;

// Structural suffix pool — the "type" part of the corporate name.
static const char* const k_corp_types[] = {
    "Holdings",
    "Industries",
    "Extraction Co.",
    "Logistics",
    "Resources",
    "Processing Group",
    "Ventures",
    "Dynamics",
    "Systems",
    "International",
    "Enterprises",
    "Operations",
};
static constexpr int k_corp_type_count = 12;

/// Build a short identifier word for the corporation (1–2 syllable phoneme).
std::string make_corp_identifier(std::mt19937& rng)
{
    std::uniform_int_distribution<int> pick_onset(0, k_corp_onset_count - 1);
    std::uniform_int_distribution<int> pick_suffix(0, k_corp_suffix_count - 1);
    std::uniform_int_distribution<int> pick_two_syl(0, 1);

    std::string word = k_corp_onsets[static_cast<std::size_t>(pick_onset(rng))];
    word += k_corp_suffixes[static_cast<std::size_t>(pick_suffix(rng))];

    // Optionally add a second syllable for longer-feeling names.
    if (pick_two_syl(rng))
    {
        word += k_corp_onsets[static_cast<std::size_t>(pick_onset(rng))];
        word += k_corp_suffixes[static_cast<std::size_t>(pick_suffix(rng))];
    }

    return word;
}

/// Generate a procedural corporation name using one of three structural templates:
///   0 — "<identifier> <type>"                  e.g. "Borex Holdings"
///   1 — "<nation_prefix> <type>"               e.g. "Kal Resources"
///   2 — "<identifier>-<identifier> <type>"     e.g. "Dynor-Nexis Logistics"
///
/// @param home_nation_name  Name of the home nation; used as inspiration for
///                          template 1 (we take the first word/syllable).
/// @param rng               Seeded RNG.
/// @return                  Generated corporate name string.
std::string make_corp_name(const std::string& home_nation_name, std::mt19937& rng)
{
    std::uniform_int_distribution<int> pick_tmpl(0, 2);
    std::uniform_int_distribution<int> pick_type(0, k_corp_type_count - 1);

    const std::string type_str = k_corp_types[static_cast<std::size_t>(pick_type(rng))];
    const int form = pick_tmpl(rng);

    switch (form)
    {
        case 0:
        {
            // "<identifier> <type>"
            return make_corp_identifier(rng) + ' ' + type_str;
        }
        case 1:
        {
            // "<nation_prefix> <type>" — take the first word of the nation name
            // (up to 6 chars) as a geographic identifier.
            std::string prefix = home_nation_name;
            const std::size_t space = prefix.find(' ');
            if (space != std::string::npos)
                prefix = prefix.substr(0, space); // first word only
            if (prefix.size() > 6)
                prefix = prefix.substr(0, 6);     // cap length
            return prefix + ' ' + type_str;
        }
        default: // 2
        {
            // "<identifier>-<identifier> <type>"
            return make_corp_identifier(rng) + '-' + make_corp_identifier(rng)
                 + ' ' + type_str;
        }
    }
}

/// Odd-r offset (col,row) -> unit-hex local centre (hex_size = 1). Mirrors
/// `ui::hex_local_centre` (src/ui/hex_render.cpp): sqrt(3) column step, 1.5 row
/// step, odd rows shifted half a column. Kept here so a generated influence_range
/// is in the *same* metric the border renderer measures — the pixel radius is then
/// `influence_range * hex_size * zoom` and matches exactly. Duplicated (2 lines) to
/// keep the world layer free of a UI dependency; the constants must stay in step.
std::pair<float, float> hex_unit_centre(int col, int row)
{
    constexpr float kSqrt3 = 1.7320508075688772f;
    const float x = kSqrt3 * static_cast<float>(col)
                  + ((row & 1) ? kSqrt3 * 0.5f : 0.0f);
    const float y = 1.5f * static_cast<float>(row);
    return { x, y };
}

/// Designate a corporation's HQ (seat) and HQ-projected border range from its
/// holdings on the home body (BL-182 foundation). The HQ is the holding nearest the
/// holdings centroid; the range is the furthest holding's distance from that HQ plus
/// a fixed projected reach, all in unit-hex distance. A corp with no holdings on the
/// home body yields {null_entity, 0} — no border. Deterministic: pure function of the
/// placed holdings, so it adds no non-determinism.
struct hq_designation { entity_id building = null_entity; float range = 0.0f; };

hq_designation designate_hq(const world& w, const std::vector<entity_id>& assets,
                            entity_id home_body)
{
    // The projected reach an HQ provides beyond its holdings hull — the "some range"
    // even a single-holding corp has. In unit-hex distance (tiles). One constant now
    // sizes both player and rival borders (was two ad-hoc render constants).
    constexpr float kProjectedReachUnits = 2.5f;

    if (home_body == null_entity)
        return {};

    std::vector<std::pair<entity_id, std::pair<float, float>>> pts; // building -> unit centre
    pts.reserve(assets.size());
    for (entity_id bid : assets)
    {
        const auto b = w.buildings.find(bid);
        if (b == w.buildings.end())
            continue;
        const auto t = w.tiles.find(b->second.tile);
        if (t == w.tiles.end() || t->second.body != home_body)
            continue;
        pts.emplace_back(bid, hex_unit_centre(t->second.grid_x, t->second.grid_y));
    }
    if (pts.empty())
        return {};

    float cx = 0.0f, cy = 0.0f;
    for (const auto& p : pts) { cx += p.second.first; cy += p.second.second; }
    cx /= static_cast<float>(pts.size());
    cy /= static_cast<float>(pts.size());

    // HQ = the holding nearest the centroid (the "seat").
    entity_id             hq   = pts.front().first;
    std::pair<float, float> hq_c = pts.front().second;
    float best = std::numeric_limits<float>::max();
    for (const auto& p : pts)
    {
        const float dx = p.second.first - cx, dy = p.second.second - cy;
        const float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; hq = p.first; hq_c = p.second; }
    }

    // Range = furthest holding from the HQ + the fixed projected reach.
    float max_d = 0.0f;
    for (const auto& p : pts)
    {
        const float dx = p.second.first - hq_c.first, dy = p.second.second - hq_c.second;
        max_d = std::max(max_d, std::sqrt(dx * dx + dy * dy));
    }
    return { hq, max_d + kProjectedReachUnits };
}

// ---------------------------------------------------------------------------
// BL-365 helpers — generate_background_firms measurement + placement
// ---------------------------------------------------------------------------

/// Nominal batches/tick a processing_facility with the standard 0.5
/// workforce_assigned (author_building's own default) runs at a full (100%)
/// workforce target — mirrors run_processing's `batches_full` formula
/// (economy_system.cpp) without needing a live building_component's
/// workforce_target; every freshly-authored building opens at 100%.
float nominal_processing_batches(const recipe_registry& reg)
{
    constexpr float default_workforce_assigned = 0.5f;
    return reg.economics(building_type::processing_facility).base_rate
         * default_workforce_assigned;
}

/// Accumulate `body_id`'s current REAL production into `production`
/// (resource-indexed), from every building actually in the world — extraction
/// via the exported `extraction_nominal` (economy_system.hpp) at contention 1.0
/// (nominal, uncontended), processing via its assigned recipe's outputs scaled
/// by `nominal_processing_batches`. A processor with no recipe assigned yet
/// (no_recipe) contributes nothing; this function always assigns one to any
/// processing building it itself authors before the next measurement.
void accumulate_body_production(const world& w, const recipe_registry& reg,
                                entity_id body_id,
                                std::array<float, resource_count>& production)
{
    for (const auto& [bid, b] : w.buildings)
    {
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        if (b.decommissioned)
            continue;
        if (b.type == building_type::extraction_site)
        {
            const float nominal = extraction_nominal(w, reg, b, 1.0f);
            if (nominal > 0.0f)
                production[static_cast<std::size_t>(b.target_resource)] += nominal;
        }
        else if (b.type == building_type::processing_facility)
        {
            const recipe* rcp = reg.get_recipe(b.recipe);
            if (!rcp)
                continue;
            const float batches = nominal_processing_batches(reg);
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rcp->outputs[r] > 0.0f)
                    production[r] += batches * rcp->outputs[r];
        }
    }
}

/// `body_id`'s aggregate demand: population_demand_params + BL-340's
/// background_demand_params baskets, weighted by every population centre's
/// `scale` on the body — the same two consumer-side pulls `clear_markets`
/// injects every tick (`inject_population_demand` / `inject_background_demand`,
/// market_clearing.cpp). Read here at pre-game generation time as the target
/// the measured stop condition below sizes background production against.
std::array<float, resource_count> body_demand(const world& w, const recipe_registry& reg,
                                              entity_id body_id)
{
    std::array<float, resource_count> demand = {};
    float total_scale = 0.0f;
    for (const auto& [cid, pcc] : w.population_centres)
    {
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tit = w.tiles.find(tile_it->second);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        total_scale += static_cast<float>(pcc.scale);
    }
    if (total_scale <= 0.0f)
        return demand;

    const population_demand_params&  pd = reg.population_demand();
    const background_demand_params&  bd = reg.background_demand();
    for (std::size_t r = 0; r < resource_count; ++r)
        demand[r] = total_scale * (pd.demand_scale * pd.demand_basket[r]
                                  + bd.demand_scale * bd.demand_basket[r]);
    return demand;
}

/// Basket-weighted mean production/demand ratio over the body's tradeable set —
/// the measured 90% stop condition reads this. Mirrors the met_ratio calculation
/// the population-growth step (economy_system.cpp) uses for its own basket-wide
/// gate: a mean of per-resource production/demand, each clamped at 1.0 so an
/// oversupplied resource cannot mask a genuine gap elsewhere. A body with no
/// measurable demand reads as fully met (1.0) — nothing to fill.
float production_ratio(const std::array<float, resource_count>& production,
                       const std::array<float, resource_count>& demand)
{
    float acc = 0.0f, weight = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (demand[r] <= 0.0f)
            continue;
        acc    += std::min(1.0f, production[r] / demand[r]);
        weight += 1.0f;
    }
    return (weight > 0.0f) ? (acc / weight) : 1.0f;
}

/// The resource with the largest ABSOLUTE shortfall (demand − production).
/// Returns resource_count (out of range — the caller's "nothing left to fill"
/// signal) if no resource has a positive shortfall.
std::size_t biggest_gap_resource(const std::array<float, resource_count>& production,
                                 const std::array<float, resource_count>& demand)
{
    std::size_t best     = resource_count;
    float       best_gap = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        const float gap = demand[r] - production[r];
        if (gap > best_gap)
        {
            best_gap = gap;
            best     = r;
        }
    }
    return best;
}

/// The processing recipe whose outputs best relieve the current per-resource
/// shortfall vector: highest Σ(positive gap × recipe.outputs). Returns -1 if no
/// recipe relieves any current gap at all (the caller then falls back to
/// extraction — the gap resource is presumably a raw, not a processed good).
int best_recipe_for_gaps(const recipe_registry& reg,
                         const std::array<float, resource_count>& production,
                         const std::array<float, resource_count>& demand)
{
    const int n = reg.recipe_count(building_type::processing_facility);
    int   best_i     = -1;
    float best_score = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const recipe& cand = reg.recipe_at(building_type::processing_facility, i);
        float score = 0.0f;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float gap = demand[r] - production[r];
            if (gap > 0.0f && cand.outputs[r] > 0.0f)
                score += gap * cand.outputs[r];
        }
        if (score > best_score)
        {
            best_score = score;
            best_i     = i;
        }
    }
    return best_i;
}

/// BL-476 — seed one `military_base` + one starting `unit_component` for
/// `corp_id`, exactly the logic BL-330 originally scoped to the player only.
/// Placed on the nearest valid land tile to the corp's HQ (falling back to the
/// nation's first valid tile if there is no HQ to anchor on); a land-poor
/// nation with no valid tile is skipped gracefully — no crash, no seeding.
/// `occupied_tiles` is the live occupancy set, updated in place as buildings
/// are authored, so callers can invoke this once per corp in a stable order
/// without re-deriving occupancy each time.
void seed_starting_military(world& w, entity_id corp_id,
                             std::unordered_set<entity_id>& occupied_tiles)
{
    const corporation_component& cc = w.corporations.at(corp_id);
    const auto nation_it = w.nations.find(cc.home_nation);
    if (nation_it == w.nations.end() || nation_it->second.tiles.empty())
        return;

    entity_id hq_tile = null_entity;
    const auto hq_bld_it = w.buildings.find(cc.hq_building);
    if (hq_bld_it != w.buildings.end())
        hq_tile = hq_bld_it->second.tile;

    // Nearest unoccupied, can_place-valid tile to the HQ (falling back to the
    // nation's first such tile if there is no HQ to anchor on) — narratively
    // the muster building sits beside the corp's seat. military_base needs no
    // deposit, so can_place reduces to "non-ocean land" here
    // (placement_rules.cpp, military_base case).
    entity_id best_tid = null_entity;
    long long best_dist2 = std::numeric_limits<long long>::max();

    long long anchor_x = 0, anchor_y = 0;
    bool have_anchor = false;
    if (hq_tile != null_entity)
    {
        const auto hq_tile_it = w.tiles.find(hq_tile);
        if (hq_tile_it != w.tiles.end())
        {
            anchor_x = hq_tile_it->second.grid_x;
            anchor_y = hq_tile_it->second.grid_y;
            have_anchor = true;
        }
    }

    for (entity_id tid : nation_it->second.tiles)
    {
        if (occupied_tiles.count(tid))
            continue;
        const auto tile_it = w.tiles.find(tid);
        if (tile_it == w.tiles.end())
            continue;
        const tile_component& tc = tile_it->second;
        bool any = false;
        const resource_type tgt = placement_rules::richest_extractable(tc, any);
        if (!placement_rules::can_place(tc, building_type::military_base, tgt))
            continue;

        if (!have_anchor)
        {
            best_tid = tid;
            break; // no HQ to measure from — first valid tile is fine
        }
        const long long dx = tc.grid_x - anchor_x;
        const long long dy = tc.grid_y - anchor_y;
        const long long dist2 = dx * dx + dy * dy;
        if (dist2 < best_dist2 || (dist2 == best_dist2 && tid < best_tid))
        {
            best_dist2 = dist2;
            best_tid = tid;
        }
    }

    // A degenerate world (no valid land tile at all) simply skips the seeding
    // rather than crashing — mirrors place_starting_assets's own
    // graceful-empty behaviour for a deposit/land-poor nation.
    if (best_tid == null_entity)
        return;

    const entity_id base_id =
        author_building(w, best_tid, building_type::military_base, occupied_tiles);
    // The base must appear in the corp's own asset list to be treated as
    // owned by the economy/UI.
    w.corporations[corp_id].assets.push_back(base_id);

    // One basic unit, seeded on the same tile as the base. Roster index 0 is
    // the cheapest/first roster row — a deterministic starting choice, not a
    // tuned one. The manpower-per-batch figure mirrors hire_unit's own
    // hire_batch_manpower constant (corp_command.cpp) so a starting unit
    // reads the same size as a player-hired one.
    constexpr int starting_unit_manpower = 50;
    const entity_id unit_id = w.create_entity();
    // BL-459: no `strength` — it was a duplicate of `count` and is derived
    // now (unit_strength, unit_roster.hpp). BL-454's `muster_base` records
    // the base this unit was raised at, so demolishing that base disbands it
    // rather than orphaning it.
    w.units[unit_id] = unit_component{
        .position = best_tid,
        .owner    = corp_id,
        .count    = starting_unit_manpower,
        .type     = 0,
        .supply_factor_permille = 1000,
        .muster_base = base_id,
    };
}

} // namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

std::vector<entity_id> generate_corporations(
    world& w,
    const corporation_params& params,
    uint32_t seed,
    const settlement_state* settle,
    generation_progress* progress)
{
    if (w.nations.empty())
        return {};

    // Distinct xor-offset seeds keep each pass's RNG stream independent,
    // mirroring the pattern used in nation_generation.cpp.
    const uint32_t seed_assign  = seed ^ 0xB1C2D3E4u;
    const uint32_t seed_focus   = seed ^ 0x2F3E4D5Cu;
    const uint32_t seed_asset   = seed ^ 0x9D8C7B6Au;
    const uint32_t seed_capital = seed ^ 0x5A4B3C2Du;
    const uint32_t seed_stock   = seed ^ 0xC3D4E5F6u;
    const uint32_t seed_name    = seed ^ 0xE1F2031Cu;

    const int corp_count = params.corporation_count;
    if (corp_count <= 0)
        return {};

    // Build an ordered snapshot of nation entity ids so indexing is stable.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        nation_ids.push_back(kv.first);
    // Sort for determinism — unordered_map iteration order is not guaranteed.
    std::sort(nation_ids.begin(), nation_ids.end());

    const int nation_count = static_cast<int>(nation_ids.size());

    // ---------------------------------------------------------------------------
    // Pass 1 — nation assignment
    // ---------------------------------------------------------------------------

    std::mt19937 assign_rng(seed_assign);

    // corp_counts[i] = number of corps already assigned to nation_ids[i].
    std::vector<int> corp_counts(static_cast<std::size_t>(nation_count), 0);

    // home_nation_idx[c] = index into nation_ids for corporation c.
    std::vector<int> home_nation_idx(static_cast<std::size_t>(corp_count));

    for (int c = 0; c < corp_count; ++c)
    {
        const int ni = pick_home_nation(w, nation_ids, corp_counts, c, assign_rng);
        home_nation_idx[static_cast<std::size_t>(c)] = ni;
        corp_counts[static_cast<std::size_t>(ni)]++;
    }

    // ---------------------------------------------------------------------------
    // Pass 2 — industrial focus assignment
    // ---------------------------------------------------------------------------

    std::mt19937 focus_rng(seed_focus);

    // focus_counts[f] = number of corps with focus f already assigned.
    std::array<int, 3> focus_counts = { 0, 0, 0 };

    std::vector<industrial_focus> corp_focuses(static_cast<std::size_t>(corp_count));

    // home_region_idx[c] = index into settle->regions, or -1. Only the
    // BL-219 path fills it; it is what makes two corps in the SAME nation differ
    // — a nation average would make every corp in a nation alike and destroy
    // the specialists premise this rewrite is required to preserve.
    std::vector<int> home_region_idx(static_cast<std::size_t>(corp_count), -1);

    if (settle && !settle->regions.empty())
    {
        // BL-219 — focus DERIVED from the corp's home region, then a
        // world-level reject-and-reroll against a diversity floor. A floor on
        // the SET is not a quota on any MEMBER, so no individual corporation's
        // focus ever becomes inexplicable; the reroll re-picks which regions
        // the corps anchor to, it never patches a corp's focus directly.
        constexpr int max_attempts = 6;
        for (int attempt = 0; attempt < max_attempts; ++attempt)
        {
            std::mt19937 attempt_rng(seed_focus ^ (static_cast<uint32_t>(attempt) * 0x9E3779B1u));
            focus_counts = { 0, 0, 0 };

            for (int c = 0; c < corp_count; ++c)
            {
                const entity_id home_nid = nation_ids[static_cast<std::size_t>(
                    home_nation_idx[static_cast<std::size_t>(c)])];

                // The regions this corp could anchor to, in placement order.
                std::vector<int> options;
                for (std::size_t pi = 0; pi < settle->regions.size(); ++pi)
                {
                    const region& p = settle->regions[pi];
                    if (p.nation < 0 || p.nation >= nation_count) continue;
                    if (nation_ids[static_cast<std::size_t>(p.nation)] != home_nid) continue;
                    options.push_back(static_cast<int>(pi));
                }

                if (options.empty())
                {
                    // A nation the settlement pass never reached (all its land
                    // arrived by conquest or orphan assignment). Fall back to
                    // the national character rather than inventing a region.
                    const auto it = w.nations.find(home_nid);
                    const economic_focus ef = (it != w.nations.end())
                        ? it->second.focus : economic_focus::extraction;
                    corp_focuses[static_cast<std::size_t>(c)] =
                        static_cast<industrial_focus>(static_cast<uint8_t>(ef));
                    focus_counts[static_cast<std::size_t>(corp_focuses[static_cast<std::size_t>(c)])]++;
                    continue;
                }

                std::uniform_int_distribution<int> pick(0, static_cast<int>(options.size()) - 1);
                const int pi = options[static_cast<std::size_t>(pick(attempt_rng))];
                home_region_idx[static_cast<std::size_t>(c)] = pi;

                const industrial_focus f = focus_from_region(
                    settle->regions[static_cast<std::size_t>(pi)],
                    settle->median_industrial_year);
                corp_focuses[static_cast<std::size_t>(c)] = f;
                focus_counts[static_cast<std::size_t>(f)]++;
            }

            // The floor: no focus class wholly unrepresented across the world.
            // With fewer corps than classes the floor is unmeetable by
            // construction, so it does not apply.
            if (corp_count < 3) break;
            if (focus_counts[0] > 0 && focus_counts[1] > 0 && focus_counts[2] > 0) break;
            // Otherwise: reroll the whole set. The last attempt's emergent set
            // stands rather than being patched — an unmet floor is honest, a
            // hand-fixed corp is not.
        }
    }
    else
    {
        for (int c = 0; c < corp_count; ++c)
        {
            const entity_id home_nid = nation_ids[static_cast<std::size_t>(
                home_nation_idx[static_cast<std::size_t>(c)])];

            const auto it = w.nations.find(home_nid);
            const economic_focus nation_ef = (it != w.nations.end())
                ? it->second.focus
                : economic_focus::extraction;

            const industrial_focus focus = pick_focus(nation_ef, focus_counts, focus_rng);
            corp_focuses[static_cast<std::size_t>(c)] = focus;
            focus_counts[static_cast<std::size_t>(focus)]++;
        }
    }

    // ---------------------------------------------------------------------------
    // Pass 3 — starting asset placement
    // ---------------------------------------------------------------------------

    std::mt19937 asset_rng(seed_asset);

    // Seed occupied_tiles from all buildings already in the world (pre-authored
    // Kepler installations and any other pre-existing buildings).
    std::unordered_set<entity_id> occupied_tiles;
    occupied_tiles.reserve(w.buildings.size() * 2);
    for (const auto& kv : w.buildings)
        occupied_tiles.insert(kv.second.tile);

    // corp_assets[c] = building entity ids placed for corp c (a clustered set).
    std::vector<std::vector<entity_id>> corp_assets(static_cast<std::size_t>(corp_count));

    for (int c = 0; c < corp_count; ++c)
    {
        const entity_id home_nid = nation_ids[static_cast<std::size_t>(
            home_nation_idx[static_cast<std::size_t>(c)])];

        const auto it = w.nations.find(home_nid);
        if (it == w.nations.end())
            continue;

        // BL-283 — the anchor is searched inside the corp's HOME PROVINCE, not
        // across its whole nation. The region already decides what the corp is
        // (Pass 2); this makes it decide where it is, so holdings cluster where the
        // corp's history says it came from instead of scattering nation-wide.
        //
        // The fallback ladder, in order, each rung deterministic:
        //   1. the home region alone;
        //   2. the home region plus the three nearest same-nation regions —
        //      a small region that cannot host the anchor type looks next door;
        //   3. the whole nation, the pre-BL-283 behaviour.
        // A rung that finds no anchorable tile consumes no randomness, so widening
        // costs nothing in stream terms. Rung 3 is reached only when the corp's own
        // corner of the map is unusable for its focus; it is the honest floor, not a
        // silent abandonment of the region.
        const int home_pi = home_region_idx[static_cast<std::size_t>(c)];

        std::vector<entity_id> assets;
        if (settle && home_pi >= 0)
        {
            const int gw = nation_grid_width(w, it->second);

            const std::vector<int> rung1{ home_pi };
            const std::vector<int> rung2 = home_and_nearest_regions(
                *settle, home_pi, home_nation_idx[static_cast<std::size_t>(c)], gw, 3);

            for (const std::vector<int>* accepted : { &rung1, &rung2 })
            {
                const std::vector<entity_id> window =
                    region_window(w, it->second, *settle, *accepted, gw);
                if (window.empty())
                    continue;
                assets = place_starting_assets(
                    w, it->second,
                    corp_focuses[static_cast<std::size_t>(c)],
                    occupied_tiles, asset_rng, &window);
                if (!assets.empty())
                    break;
            }
        }

        if (assets.empty())
        {
            assets = place_starting_assets(
                w, it->second,
                corp_focuses[static_cast<std::size_t>(c)],
                occupied_tiles,
                asset_rng);
        }

        corp_assets[static_cast<std::size_t>(c)] = std::move(assets);

        // BL-305 — the POSITIONAL half. Asset placement has a place, so it goes
        // on the map: each holding is published at the grid cell it just
        // claimed, corp by corp, so the player watches charters stake ground on
        // the political map the carve has only just finished drawing. (Every
        // generated corp is registered in a Kepler nation, so every holding sits
        // on the same grid the carve was published over.)
        if (progress)
        {
            for (const entity_id bid : corp_assets[static_cast<std::size_t>(c)])
            {
                const auto bit = w.buildings.find(bid);
                if (bit == w.buildings.end()) continue;
                const auto tit = w.tiles.find(bit->second.tile);
                if (tit == w.tiles.end()) continue;
                progress->mark_asset(tit->second.grid_x, tit->second.grid_y, c);
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Pass 4 — financial profile
    // ---------------------------------------------------------------------------

    std::mt19937 capital_rng(seed_capital);

    std::vector<float> corp_capitals(static_cast<std::size_t>(corp_count));
    for (int c = 0; c < corp_count; ++c)
    {
        corp_capitals[static_cast<std::size_t>(c)] = compute_capital(
            params.base_capital,
            params.wealth_variance,
            corp_focuses[static_cast<std::size_t>(c)],
            capital_rng);

        // BL-305 — the NON-POSITIONAL half. A financial profile is a derivation,
        // not a location: there is nowhere on the map for it to be, so it goes
        // to a ledger column instead of a canvas overlay. One row per corp as
        // the capital resolves, so the ledger fills in step with the markers
        // appearing beside it. Numbers only — the generated NAME is Pass 5's,
        // and a std::string cannot cross an atomics-only seam without a lock;
        // the names arrive with the finished world a moment later.
        if (progress)
            progress->add_corp_row(
                c,
                static_cast<int>(corp_focuses[static_cast<std::size_t>(c)]),
                static_cast<int>(corp_assets[static_cast<std::size_t>(c)].size()),
                corp_capitals[static_cast<std::size_t>(c)]);
    }

    // Independent stream for the generated starting stockpile (BL-116).
    std::mt19937 stock_rng(seed_stock);

    // ---------------------------------------------------------------------------
    // Pass 5 — naming
    // ---------------------------------------------------------------------------

    std::mt19937 name_rng(seed_name);

    std::vector<std::string> corp_names(static_cast<std::size_t>(corp_count));
    for (int c = 0; c < corp_count; ++c)
    {
        const entity_id home_nid = nation_ids[static_cast<std::size_t>(
            home_nation_idx[static_cast<std::size_t>(c)])];

        const auto it = w.nations.find(home_nid);
        const std::string& nation_name = (it != w.nations.end())
            ? it->second.name
            : std::string("Unknown");

        corp_names[static_cast<std::size_t>(c)] = make_corp_name(nation_name, name_rng);
    }

    // ---------------------------------------------------------------------------
    // Assemble corporation_components and register in the world
    // ---------------------------------------------------------------------------

    std::vector<entity_id> corp_ids;
    corp_ids.reserve(static_cast<std::size_t>(corp_count));

    for (int c = 0; c < corp_count; ++c)
    {
        corporation_component cc;
        cc.name             = std::move(corp_names[static_cast<std::size_t>(c)]);
        cc.home_nation      = nation_ids[static_cast<std::size_t>(
                                  home_nation_idx[static_cast<std::size_t>(c)])];
        cc.focus            = corp_focuses[static_cast<std::size_t>(c)];
        cc.starting_capital = corp_capitals[static_cast<std::size_t>(c)];
        cc.balance          = corp_capitals[static_cast<std::size_t>(c)]; // opens at starting capital
        cc.is_player        = false;

        // Home body resolved before the assets vector is moved into the component.
        const entity_id home_body =
            corp_home_body(w, corp_assets[static_cast<std::size_t>(c)]);

        // HQ + border range (BL-182 foundation): designate the seat and its
        // HQ-projected range from the holdings on the home body, before the assets
        // vector is moved out. Deterministic — pure function of the placed holdings.
        const hq_designation hq = designate_hq(
            w, corp_assets[static_cast<std::size_t>(c)], home_body);
        cc.hq_building     = hq.building;
        cc.influence_range = hq.range;

        cc.assets = std::move(corp_assets[static_cast<std::size_t>(c)]);

        const entity_id corp_id = w.create_entity();
        corp_ids.push_back(corp_id);
        w.corporations[corp_id] = std::move(cc);

        // Starting resource stockpile (BL-116): generate a focus / wealth-shaped
        // opening stockpile and seed it on the corp's home body so build /
        // production / trade have materials from turn one. Generated for every
        // corp (fixed RNG-draw order) so the stream stays deterministic even
        // when a holdless corp has no body to seed; only corps with a home body
        // receive a pool. Populates the existing corp_body_pools map (no new
        // save field). The pre-game warm-start then evolves this seed.
        const auto stock = generate_starting_stockpile(
            corp_focuses[static_cast<std::size_t>(c)],
            corp_capitals[static_cast<std::size_t>(c)],
            params.base_capital, stock_rng);
        if (home_body != null_entity)
        {
            stockpile_component& pool = w.pool_for(corp_id, home_body);
            for (std::size_t r = 0; r < resource_count; ++r)
                pool.quantities[r] += stock[r];
        }
    }

    // Pre-game profit (a simulated operating history seeding opening balances and
    // pools) is applied at app startup as the *long* economy warm-start — after the
    // Lua economy data is loaded, so it reuses the real registry rather than a
    // duplicated one. See app::run() (backlog.json § Corporation generation).

    // ---------------------------------------------------------------------------
    // Player corporation flag — deterministic pick (seeded first-corp selection)
    // ---------------------------------------------------------------------------
    // Use the name seed (already consumed) for a seeded index pick so the
    // player corporation varies with the seed rather than always being corp 0.
    {
        std::mt19937 player_rng(seed ^ 0xF0E1D2C3u);
        std::uniform_int_distribution<int> pick_player(0, corp_count - 1);
        const int player_idx = pick_player(player_rng);

        const entity_id player_corp_id = corp_ids[static_cast<std::size_t>(player_idx)];
        w.corporations[player_corp_id].is_player = true;
        w.player_entity = player_corp_id;

        // BL-305: which ledger row the player ends up as is only decided here,
        // after every row is already published — so it is a separate field
        // rather than a column on the row.
        if (progress)
            progress->player_slot.store(player_idx, std::memory_order_relaxed);

    }

    // ---------------------------------------------------------------------
    // Starting muster building + unit (BL-330; extended to every rival by
    // BL-476). Every occupied_tiles set built earlier in this function was
    // scoped to its own corp's placement pass and is long out of scope by
    // now; rebuild the true current occupancy from the authoritative source
    // (every building already on the board) rather than threading a stale
    // set through the whole function. One shared set threaded across every
    // corp's call below so a rival can never be placed on a tile another
    // rival (or the player) was just seeded onto.
    //
    // PLAYER FIRST, always — this is what keeps the player's own tile pick
    // byte-identical to the pre-BL-476 behaviour. Before this item, the
    // player was the ONLY corp seeded, so its candidate tile set was never
    // narrowed by another corp's military_base. `player_idx` is a seeded
    // pick that can land anywhere in `corp_ids`, so seeding rivals first (or
    // in raw corp_ids order) could let a rival claim the exact tile the
    // player would otherwise have picked — a real behaviour change, not
    // just a hypothetical one, since several corps can share a home nation.
    // Seeding the player before any rival reproduces the original
    // occupied_tiles snapshot for the player's own placement exactly, then
    // rivals fill in around it afterward, in stable corp_ids order.
    //
    // BL-365 background firms are never in `corp_ids` (they are created by
    // the separate generate_background_firms pass below), so iterating
    // `corp_ids` in its existing stable order already excludes them without
    // an explicit is_background check — arming them is a deliberate
    // non-goal (BL-476 design notes).
    // ---------------------------------------------------------------------
    {
        std::unordered_set<entity_id> military_occupied_tiles;
        military_occupied_tiles.reserve(w.buildings.size());
        for (const auto& [bld_id, bc] : w.buildings)
            military_occupied_tiles.insert(bc.tile);

        seed_starting_military(w, w.player_entity, military_occupied_tiles);

        for (entity_id corp_id : corp_ids)
            if (corp_id != w.player_entity)
                seed_starting_military(w, corp_id, military_occupied_tiles);
    }

    return corp_ids;
}

// ---------------------------------------------------------------------------
// BL-365 — generate_background_firms
// ---------------------------------------------------------------------------

std::vector<entity_id> generate_background_firms(
    world& w, const recipe_registry& reg, uint32_t seed)
{
    std::vector<entity_id> firm_ids;
    if (w.nations.empty())
        return firm_ids;

    // Distinct bodies carrying at least one population centre — the only bodies
    // with a real demand basket (body_demand, above) to size production
    // against. Sorted + de-duplicated for determinism (population_centres is
    // unordered_map-backed).
    std::vector<entity_id> body_ids;
    for (const auto& [cid, pcc] : w.population_centres)
    {
        (void)pcc;
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tit = w.tiles.find(tile_it->second);
        if (tit == w.tiles.end())
            continue;
        body_ids.push_back(tit->second.body);
    }
    std::sort(body_ids.begin(), body_ids.end());
    body_ids.erase(std::unique(body_ids.begin(), body_ids.end()), body_ids.end());

    // clearing_fraction — the exact figure the deleted BL-078 substrate model
    // used (economy.substrate.clearing_fraction, 0.90) to preserve the "live,
    // fillable opportunity gap" invariant BL-078/BL-112 depend on: real
    // background production covers most, not all, of demand, leaving room for
    // the player to fill the rest.
    constexpr float target_ratio           = 0.90f;
    constexpr int   max_firms_per_body     = 40; // hard bound — never infinite-loop
    constexpr int   max_iterations_per_body = 2 * max_firms_per_body; // slack for placement misses

    // Distinct xor-offset seeds, independent of generate_corporations' own
    // streams (seed_asset etc.) so the two passes cannot collide even though
    // both draw from placement RNG.
    std::mt19937 asset_rng(seed ^ 0x3D6F9A11u);
    std::mt19937 name_rng(seed ^ 0x6E17C4B0u);
    std::mt19937 stock_rng(seed ^ 0x1A2B3C4Du);

    for (const entity_id body_id : body_ids)
    {
        // Nations that actually own tiles on this body — a background firm only
        // opens where a nation exists to host it, exactly like generate_corporations.
        std::vector<entity_id> nation_ids;
        for (const auto& [nid, nc] : w.nations)
        {
            for (entity_id tid : nc.tiles)
            {
                const auto tit = w.tiles.find(tid);
                if (tit != w.tiles.end() && tit->second.body == body_id)
                {
                    nation_ids.push_back(nid);
                    break;
                }
            }
        }
        if (nation_ids.empty())
            continue;
        std::sort(nation_ids.begin(), nation_ids.end());

        const std::array<float, resource_count> demand = body_demand(w, reg, body_id);

        // Occupancy is rebuilt from the authoritative source each body (mirrors
        // generate_corporations' own player-muster-building block) — every
        // building already on the board, including this body's own prior
        // background firms placed earlier in this same loop.
        std::unordered_set<entity_id> occupied_tiles;
        occupied_tiles.reserve(w.buildings.size() * 2);
        for (const auto& kv : w.buildings)
            occupied_tiles.insert(kv.second.tile);

        int nation_cursor    = 0;
        int firms_this_body  = 0;
        for (int iter = 0; iter < max_iterations_per_body && firms_this_body < max_firms_per_body; ++iter)
        {
            std::array<float, resource_count> production = {};
            accumulate_body_production(w, reg, body_id, production);

            // MEASURED stop condition — real production vs real demand, not a
            // firm-count target.
            if (production_ratio(production, demand) >= target_ratio)
                break;

            const std::size_t gap_r = biggest_gap_resource(production, demand);
            if (gap_r == resource_count)
                break; // no resource genuinely short — nothing left worth filling

            // Prefer processing when some recipe's output actually relieves the
            // gap resource (a refined good — silicon, machinery, ...);
            // otherwise the firm extracts the gap resource as a raw directly.
            const int  recipe_i = best_recipe_for_gaps(reg, production, demand);
            const bool go_processing = (recipe_i >= 0)
                && (reg.recipe_at(building_type::processing_facility, recipe_i).outputs[gap_r] > 0.0f);
            const industrial_focus focus = go_processing ? industrial_focus::processing
                                                          : industrial_focus::extraction;

            const entity_id home_nid = nation_ids[static_cast<std::size_t>(
                nation_cursor % static_cast<int>(nation_ids.size()))];
            ++nation_cursor;
            const auto nit = w.nations.find(home_nid);
            if (nit == w.nations.end())
                continue;

            std::vector<entity_id> assets = place_starting_assets(
                w, nit->second, focus, occupied_tiles, asset_rng);
            if (assets.empty())
                continue; // this nation had nothing left to anchor on this round; try the next

            // Author a recipe onto every processing facility placed, targeted at
            // the gap this firm exists to fill — generation-time processors
            // otherwise stay `no_recipe` until app::load_economy's blanket
            // steel default, which has already run by the time this function is
            // called (see the header's ordering note), so a gap-targeted recipe
            // here is strictly better than losing every background processor to
            // the same steel default.
            if (go_processing && recipe_i >= 0)
            {
                const recipe&  chosen    = reg.recipe_at(building_type::processing_facility, recipe_i);
                const uint16_t chosen_id = reg.recipe_id(chosen.name);
                for (const entity_id bid : assets)
                {
                    const auto bit = w.buildings.find(bid);
                    if (bit != w.buildings.end()
                        && bit->second.type == building_type::processing_facility)
                        bit->second.recipe = chosen_id;
                }
            }

            // Financial profile: background firms open lean, same as every other
            // generated corp (corporation_params::base_capital defaults to 0 —
            // capital is earned through the pre-game warm start, not seeded).
            corporation_component cc;
            cc.name          = make_corp_name(nit->second.name, name_rng);
            cc.home_nation   = home_nid;
            cc.focus         = focus;
            cc.starting_capital = 0.0f;
            cc.balance          = 0.0f;
            cc.is_player     = false;
            cc.is_background = true;

            const entity_id home_body = corp_home_body(w, assets);
            const hq_designation hq   = designate_hq(w, assets, home_body);
            cc.hq_building     = hq.building;
            cc.influence_range = hq.range;
            cc.assets          = std::move(assets);

            const entity_id corp_id = w.create_entity();
            w.corporations[corp_id] = std::move(cc);
            firm_ids.push_back(corp_id);
            ++firms_this_body;

            // Starting stockpile — the same BL-116 generator every generated
            // corp uses, so a background firm opens with materials from turn
            // one exactly like a rival does.
            if (home_body != null_entity)
            {
                const auto stock = generate_starting_stockpile(
                    focus, /*capital=*/0.0f, /*base_capital=*/0.0f, stock_rng);
                stockpile_component& pool = w.pool_for(corp_id, home_body);
                for (std::size_t r = 0; r < resource_count; ++r)
                    pool.quantities[r] += stock[r];
            }
        }
    }

    return firm_ids;
}

void assign_default_recipes(world& w, const recipe_registry& reg)
{
    // BL-429: era-aware, so an ancient campaign does not default every processor
    // to an industrial recipe it could never run.
    const uint16_t default_recipe = reg.default_recipe_id();
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = default_recipe;
}

// ---------------------------------------------------------------------------
// Measurement seam (2026-08-20)
// ---------------------------------------------------------------------------
// `generate_background_firms` stops on a MEASURED condition — basket-weighted
// production/demand >= target_ratio — or on `max_firms_per_body`, whichever comes
// first. Which of those two actually fires is the whole question behind "do
// markets open with the goods they need", and until now nothing outside this file
// could ask it: the three helpers are file-private.
//
// These wrappers expose the shipped arithmetic rather than inviting a harness to
// re-implement it. That re-implementation is the hand-mirrored-table drift this
// project has now hit four times (world_audit.cpp B4 carries the latest). Pure
// reads over world + registry; they allocate nothing and change nothing.

std::array<float, resource_count> measure_body_demand(const world& w,
                                                     const recipe_registry& reg,
                                                     entity_id body_id)
{
    return body_demand(w, reg, body_id);
}

std::array<float, resource_count> measure_body_production(const world& w,
                                                          const recipe_registry& reg,
                                                          entity_id body_id)
{
    std::array<float, resource_count> production = {};
    accumulate_body_production(w, reg, body_id, production);
    return production;
}

float measure_production_ratio(const world& w, const recipe_registry& reg,
                               entity_id body_id)
{
    return production_ratio(measure_body_production(w, reg, body_id),
                            measure_body_demand(w, reg, body_id));
}
