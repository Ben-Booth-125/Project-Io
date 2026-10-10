#include "corporation_generation.hpp"

#include "province.hpp"

#include "world/hard_coded_world.hpp" // generation_progress — the BL-305 tap

#include "world/construction.hpp"    // demolish_building — seat_clean_slate (BL-1206)
#include "world/economy_system.hpp"
#include "world/logistics.hpp"      // invalidate_logistics_caches — remove_specialist_roster
#include "world/market_clearing.hpp" // market_for_tile — NR-913's market pool
#include "world/placement_rules.hpp"
#include "world/planetology.hpp"    // checkpoint_rng — charter_web_from_budget's keyed streams
#include "world/settlement.hpp"
#include "world/history_sim.hpp"      // trade_record_cell — the retrofit's record (BL-1268)
#include "world/world_gen_config.hpp" // trade_retrofit_params (BL-1268)
#include "world/input_reach.hpp"   // the one reach rule (BL-1185 / BL-1187)
#include "world/spawn_seat.hpp"    // repoint_player — enforce_chain_feasible_roster's seat

#include <algorithm>
#include <cassert>
#include <iterator>
#include <map>
#include <set>
#include <memory>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
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

/// How dry and firm a tile's ground is, as a placement multiplier. Used by
/// background-industry placement to prefer workable ground.
///
/// SPLIT BY MEANING (BL-519), and it multiplies rather than switches: the
/// SUBSTRATE says how firm the ground is, the COVER says how wet or awkward what
/// sits on it is, and the pre-split table was the product of the two collapsed
/// into one slot. Every old composition reproduces its old number exactly —
/// barren/rocky/regolith 1.3 (dry × bare), grassland 1.0, forest 0.8,
/// wetland 0.5, tundra 1.0, metallic 1.0, volcanic 0.7, icy 0.6, ocean 0.001 —
/// and pairs the old model could not name now fall out of the product (a
/// forested rocky upland is 1.3 × 0.8 = 1.04, firmer than a floodplain and
/// slightly worse than bare rock).
///
/// @param sub The tile's substrate.
/// @param cov The tile's cover.
/// @return    A positive multiplier, 1.0 for ground with no strong lean.
float dryness_lean(terrain_substrate sub, terrain_cover cov)
{
    float ground = 1.0f;
    switch (sub)
    {
        case terrain_substrate::barren:
        case terrain_substrate::rocky:
        case terrain_substrate::regolith:    ground = 1.3f;   break;
        case terrain_substrate::metallic:
        case terrain_substrate::sedimentary: ground = 1.0f;   break;
        case terrain_substrate::volcanic:    ground = 0.7f;   break;
        case terrain_substrate::icy:         ground = 0.6f;   break;
        case terrain_substrate::ocean:       // BL-516: every water kind is the same non-answer here.
        case terrain_substrate::coast:
        case terrain_substrate::lake:        return 0.001f; // can_place rejects these anyway
    }

    float on_top = 1.0f;
    switch (cov)
    {
        case terrain_cover::marsh:  on_top = 0.5f; break; // the old `wetland` factor
        case terrain_cover::snow:   on_top = 0.7f; break;
        case terrain_cover::forest: on_top = 0.8f; break; // the old `forest` factor
        case terrain_cover::ash:
        case terrain_cover::dunes:  on_top = 0.9f; break;
        case terrain_cover::salt:   on_top = 1.1f; break; // a dry crust IS firm ground
        case terrain_cover::grass:
        case terrain_cover::scrub:
        case terrain_cover::urban:
        case terrain_cover::none:   on_top = 1.0f; break;
    }
    return ground * on_top;
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
/// A WELL TILE (BL-1197 / BL-1198) carries no deposit — its water comes from the
/// river or lake beside it — so scoring it by deposit would rank every Well site
/// at the floor. A Well is scored by the tile's habitability instead: the water
/// is wanted where people live, and habitable ground is where they settled. A
/// Fishing Wharf tile (BL-1208) carries no produce deposit either — the sea is
/// its ground — and is scored the same way, for the same reason.
///
/// @param tc        The candidate tile.
/// @param btype     The building type proposed for it.
/// @param target    The extraction target, when the caller names one (a gap
///                  firm digging its good); null means the tile's richest.
/// @param depositless True when the tile is a Well or Wharf site for @p target
///                  (`placement_rules::is_depositless_site`) — the caller has the world.
/// @return          A positive placement score; larger is more preferred.
float tile_score_for(const tile_component& tc, building_type btype,
                     const resource_type* target = nullptr, bool depositless = false)
{
    float score = 1.0f;
    switch (btype)
    {
        case building_type::extraction_site:
            // Rank by extractable deposit so the site lands where it can work;
            // tiny floor keeps a deposit-poor tile still placeable (it will fail
            // can_place separately and be skipped there). A Well or a Wharf by
            // habitability.
            if (depositless)
                score = 0.05f + tc.habitability;
            else if (target != nullptr)
                score = tc.resource_deposit[static_cast<std::size_t>(*target)] + 0.001f;
            else
                score = placement_rules::extractable_deposit(tc) + 0.001f;
            break;
        case building_type::processing_facility:
            // Mild lean toward tiles with some deposit (own feedstock nearby).
            score = total_deposit(tc) * 0.5f + 1.0f;
            break;
        case building_type::launchpad:
            // Flat and dry: a pad wants easy ground and no weather. Plains best,
            // slope penalised hard; dry ground preferred over wet/icy ground.
            score = landform_lean(tc.landform, 1.6f, 1.0f, 0.35f, 0.5f, 1.1f, 0.9f, 0.4f);
            score *= dryness_lean(tc.substrate, tc.cover);
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
/// @param target An explicit extraction target (BL-1197: a water-gap firm's Well
///               or ice site authors `water`; a Well tile has no deposit for the
///               richest-deposit rule to find); null means the richest.
/// @return      Entity id of the created building.
entity_id author_building(world& w,
                          entity_id tid,
                          building_type btype,
                          std::unordered_set<entity_id>& occupied_tiles,
                          const resource_type* target = nullptr)
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
        if (target != nullptr)
            bc.target_resource = *target;
        else if (const auto tit = w.tiles.find(tid); tit != w.tiles.end())
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

/// How a tile can host an extraction site digging @p dig (BL-1197, gap firm digs
/// the gap): `none`, a `well` (water only — a fresh-water-adjacent tile with no
/// ice deposit, `placement_rules::is_well_site`), a `wharf` (agricultural
/// produce only — a coastal tile with no produce deposit,
/// `placement_rules::is_wharf_site`; BL-1208), or a `deposit` of @p dig. All
/// pass `can_place` (never water ground, never urban). Read-only, draws nothing.
enum class dig_site { none, well, wharf, deposit };
dig_site dig_site_of(const world& w, entity_id tid, const tile_component& tc,
                     resource_type dig)
{
    if (!placement_rules::can_place(tc, building_type::extraction_site, dig))
        return dig_site::none;
    if (tc.resource_deposit[static_cast<std::size_t>(dig)] > 0.0f)
        return dig_site::deposit;
    if (placement_rules::is_well_site(w, tid, dig))
        return dig_site::well;
    if (placement_rules::is_wharf_site(w, tid, dig))
        return dig_site::wharf;
    return dig_site::none;
}

/// THE DIG LADDER for @p dig, in the order its tiers are tried (BL-1197 /
/// BL-1208): water a Well site before an ice deposit (the water is wanted where
/// people live); agricultural produce a produce deposit before a coastal Wharf
/// site (the Farm is the good's own ground, the sea its fallback). Only the
/// goods `gap_firm_digs` names have a ladder.
std::array<dig_site, 2> dig_ladder(resource_type dig)
{
    if (dig == resource_type::agricultural_produce)
        return { dig_site::deposit, dig_site::wharf };
    return { dig_site::well, dig_site::deposit };
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
/// DIGGING A NAMED GOOD (BL-1197, gap firm digs the gap). With @p dig and a
/// @p dig_tier other than `none`, an extraction anchor stands ONLY on a tile of
/// that tier for @p dig (`dig_site_of`) — a Well or Wharf site, scored by habitability, or
/// a deposit of the good, scored by that deposit — and extracts @p dig; a window
/// with no such tile places nothing and draws nothing, so the caller can try the
/// next tier as it would the next rung. Each later extraction slot digs @p dig
/// where its tile can, and otherwise takes the tile's richest deposit as ever.
/// Without @p dig the placement is exactly the richest-deposit rule above.
///
/// @param w              World to write the new entities into.
/// @param home_nation    The nation_component of the home nation.
/// @param focus          Corporate industrial focus (determines the asset mix and tile preference).
/// @param occupied_tiles Tiles already taken by another building; never reused.
/// @param rng            Seeded RNG for the anchor weighted draw and holdings count.
/// @param anchor_window  Tiles the anchor may sit on; null/empty means the whole nation.
/// @param dig            The good an extraction firm digs (BL-1197); null for none.
/// @param dig_tier       Which ground the anchor must stand on to dig it.
/// @return               Entity ids of every building placed (may be empty when the
///                       window holds no tile that can host the anchor type — the
///                       caller's cue to widen a rung).
std::vector<entity_id> place_starting_assets(world& w,
                                             const nation_component& home_nation,
                                             industrial_focus focus,
                                             std::unordered_set<entity_id>& occupied_tiles,
                                             std::mt19937& rng,
                                             const std::vector<entity_id>* anchor_window = nullptr,
                                             const resource_type* dig = nullptr,
                                             dig_site dig_tier = dig_site::none)
{
    std::vector<entity_id> placed;
    if (home_nation.tiles.empty())
        return placed;

    const std::vector<entity_id>& anchor_pool =
        (anchor_window && !anchor_window->empty()) ? *anchor_window : home_nation.tiles;

    const std::vector<building_type>& pattern = focus_asset_pattern(focus);
    const building_type anchor_type = pattern.front();
    // BL-1197: digging only applies to an extraction anchor on a named tier.
    const bool digging = dig != nullptr && dig_tier != dig_site::none
                      && anchor_type == building_type::extraction_site;

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
        if (digging)
        {
            // BL-1197: only the named tier's ground for the dug good.
            if (dig_site_of(w, tid, tc, *dig) != dig_tier)
                continue;
            anchors.push_back({ tid, tile_score_for(tc, anchor_type, dig,
                                                    dig_tier == dig_site::well
                                                    || dig_tier == dig_site::wharf) });
            continue;
        }
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

    placed.push_back(author_building(w, anchor_tid, anchor_type, occupied_tiles,
                                     digging ? dig : nullptr));

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
        if (placement_rules::is_water_tile(it->second.substrate))
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
            // BL-1197: a digging firm's extraction slot digs its good where the
            // tile can (a Well or Wharf site or a deposit of it — the slot walk
            // has the world, so the fresh-water and coastal gates are tested
            // here), else the richest.
            if (digging && btype == building_type::extraction_site
                && dig_site_of(w, tid, tc, *dig) != dig_site::none)
            {
                placed.push_back(author_building(w, tid, btype, occupied_tiles, dig));
                ++cursor_tile;
                placed_this_slot = true;
                break;
            }
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

/// The good a background extraction firm chartered for @p gap digs at its own
/// ground (BL-1197, gap firm digs the gap): water and agricultural produce, the
/// two goods with a depositless tier — water from a Well where people live
/// (BL-1198), produce from a Farm deposit or else a coastal Fishing Wharf
/// (BL-1199 / BL-1208). Every other gap keeps the richest-deposit rule. Returns
/// false when the firm digs no named good.
bool gap_firm_digs(std::size_t gap, bool go_processing, resource_type& dig_out)
{
    if (go_processing)
        return false;
    if (gap == static_cast<std::size_t>(resource_type::water))
        dig_out = resource_type::water;
    else if (gap == static_cast<std::size_t>(resource_type::agricultural_produce))
        dig_out = resource_type::agricultural_produce;
    else
        return false;
    return true;
}

/// THE DIG LADDER (BL-1197 / BL-1208, `dig_ladder`): each tier tried over the
/// whole of @p anchor_window before the next, a tier with no ground drawing
/// nothing. There is NO fallback to another deposit: a window with no ground
/// for the good places nothing, and the miss is the good's.
std::vector<entity_id> place_digging_assets(world& w,
                                            const nation_component& home_nation,
                                            industrial_focus focus,
                                            std::unordered_set<entity_id>& occupied_tiles,
                                            std::mt19937& rng,
                                            const std::vector<entity_id>* anchor_window,
                                            resource_type dig)
{
    for (const dig_site tier : dig_ladder(dig))
    {
        std::vector<entity_id> assets = place_starting_assets(
            w, home_nation, focus, occupied_tiles, rng, anchor_window, &dig, tier);
        if (!assets.empty())
            return assets;
    }
    return {};
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

/// BL-1173 — WORKING CAPITAL, x the opening stock's value at base price.
///
/// FINANCE.md § Debt interest: "Every firm opens with it". A background firm is
/// handed a BL-116 opening stockpile and used to be handed no money at all, so
/// the first quarter's wages and maintenance put it below zero before it had
/// sold a unit, and interest compounded the rest — 57 % of the field started
/// one bad quarter from the spiral the 400-credit `base_capital` exists to
/// stop for the seat and the specialists.
///
/// THE RULE: cash = this fraction x the value of the stock, each good valued at
/// its BASE price in the market the stock is pooled in. Base, never the live
/// price: an opening position is priced by what a good is worth, not by the
/// first tick's scarcity, and base is fixed at generation so the result cannot
/// move with clearing order. A quarter of the stock's worth is "a firm keeps
/// one part in four of its working assets liquid" — enough to carry the
/// opening quarters' wages and maintenance while it sells the stock it holds,
/// without minting a balance larger than what the firm actually owns.
constexpr float k_background_working_capital_of_stock = 0.25f;

/// The opening cash a background firm is handed: the rule above. `pool_key` is
/// the key its stock was pooled under (`corp_home_pool_key`): a market prices
/// it at that market's base; a body-level key (no market carved yet) prices it
/// at the LOWEST-ID market on that body, a fixed choice so it cannot depend on
/// container order; a body with no market at all prices it at 0, since there
/// is nowhere the stock could be sold. Pure; deterministic; no RNG.
static float background_working_capital(const world& w, entity_id pool_key,
                                        const std::array<float, resource_count>& stock)
{
    const market_component* m = nullptr;
    if (const auto it = w.markets.find(pool_key); it != w.markets.end())
        m = &it->second;
    else
    {
        entity_id best = null_entity;
        for (const auto& [mid, mc] : w.markets)
            if (mc.body == pool_key && (best == null_entity || mid < best))
                best = mid;
        if (best != null_entity)
            m = &w.markets.at(best);
    }
    if (m == nullptr)
        return 0.0f;

    double value = 0.0; // resource index order: a fixed summation order
    for (std::size_t r = 0; r < resource_count; ++r)
        if (stock[r] > 0.0f && m->base_price[r] > 0.0f)
            value += static_cast<double>(stock[r]) * m->base_price[r];
    return static_cast<float>(value * k_background_working_capital_of_stock);
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
    // ASCENDING BUILDING ID (BL-1050). `production[r] +=` is a cross-building
    // float sum and float addition does not associate, so walked in
    // `w.buildings`' bucket order this vector — and the gap loop that chooses
    // the next firm from it — would depend on that store's layout, which a
    // save/load rebuilds (world_save.cpp re-inserts in id order) and another
    // standard library lays out differently again.
    //
    // The eligibility test is order-free, so it runs first and the sort is over
    // this body's buildings rather than the world's.
    std::vector<entity_id> bids;
    bids.reserve(w.buildings.size());
    for (const auto& [bid, b] : w.buildings)
    {
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        if (b.decommissioned)
            continue;
        bids.push_back(bid);
    }
    std::sort(bids.begin(), bids.end());

    for (const entity_id bid : bids)
    {
        const building_component& b = w.buildings.at(bid);
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

/// DERIVED DEMAND (Ben, 2026-10-05; BL-1197 round 4; CORPORATION_GENERATION.md
/// § Pass 6, "A processor's inputs are wanted too"): what every processor
/// standing on @p body_id draws of its recipe's inputs, at the same nominal
/// rate `accumulate_body_production` credits its outputs (generation form, no
/// report) — so a placed steel works wants its iron ore and coal, a machinery
/// plant its steel and refined copper, in the units the gap already reads.
/// Walked in ascending building id for the same reason as production (a float
/// sum must not depend on the store's layout). Recomputed by every caller each
/// firm, so a processor chartered by the walk raises its inputs' gaps for the
/// firms after it.
std::array<float, resource_count> body_processor_input_demand(const world& w,
                                                              const recipe_registry& reg,
                                                              entity_id body_id)
{
    std::array<float, resource_count> out = {};
    std::vector<entity_id> bids;
    bids.reserve(w.buildings.size());
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.type != building_type::processing_facility || b.decommissioned)
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        bids.push_back(bid);
    }
    std::sort(bids.begin(), bids.end());
    const float batches = nominal_processing_batches(reg);
    for (const entity_id bid : bids)
    {
        const recipe* rcp = reg.get_recipe(w.buildings.at(bid).recipe);
        if (!rcp)
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (rcp->inputs[r] > 0.0f)
                out[r] += batches * rcp->inputs[r];
    }
    return out;
}

/// BL-1197 round 5 — which goods @p body_id can produce AT ALL: an extractable
/// with a deposit of it on some land tile of the body; water also wherever a
/// Well site stands (`placement_rules::is_well_site`); agricultural produce also
/// wherever a Fishing Wharf site stands (`placement_rules::is_wharf_site` — any
/// coast; a coastal produce deposit is a Farm, counted above). Exactly the
/// ground `dig_ladder` reaches, so a good this calls producible is one a
/// digging firm can find ground for (BL-1208); and, to a fixed point, any good some
/// processing recipe makes from inputs that are all themselves producible here.
/// Existence tests over the body's tiles and the registry: order-free, draws
/// nothing, deterministic.
std::array<bool, resource_count> body_producible(const world& w, const recipe_registry& reg,
                                                 entity_id body_id)
{
    std::array<bool, resource_count> can{};
    const std::size_t water = static_cast<std::size_t>(resource_type::water);
    const std::size_t food  = static_cast<std::size_t>(resource_type::agricultural_produce);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body_id || placement_rules::is_water_tile(tc.substrate))
            continue;
        for (const resource_type e : placement_rules::k_extractable)
        {
            const std::size_t r = static_cast<std::size_t>(e);
            if (!can[r] && tc.resource_deposit[r] > 0.0f)
                can[r] = true;
        }
        if (!can[water] && placement_rules::is_well_site(w, tid, resource_type::water))
            can[water] = true;
        if (!can[food] && placement_rules::is_wharf_site(w, tid, resource_type::agricultural_produce))
            can[food] = true;
    }
    const int n = reg.recipe_count(building_type::processing_facility);
    for (bool grew = true; grew;)
    {
        grew = false;
        for (int i = 0; i < n; ++i)
        {
            const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
            bool fed = true;
            for (std::size_t r = 0; r < resource_count && fed; ++r)
                if (rc.inputs[r] > 0.0f && !can[r])
                    fed = false;
            if (!fed)
                continue;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rc.outputs[r] > 0.0f && !can[r])
                {
                    can[r] = true;
                    grew   = true;
                }
        }
    }
    return can;
}

/// BL-708 — the body's INDUSTRIAL demand: what the buildings standing on it draw
/// as upkeep each tick (`run_building_upkeep`, economy_system.cpp), resolved
/// through the SAME `building_upkeep_goods` free function the live pass and the
/// census compose the era bands with, so this cannot drift from what is actually
/// drawn.
///
/// WHY THE SEEDER HAS TO SEE IT. `body_demand` below sizes background production
/// against what a body CONSUMES, and until this existed it counted only the two
/// consumer-side baskets — households and the background stopgap. A building's
/// upkeep draw is consumption too, and leaving it out meant the seeder happily
/// declared a body provisioned while every firm on it was starving.
///
/// That gap is invisible while every authored upkeep rate is zero, which is
/// exactly how it survived: it becomes load-bearing the moment ANY rate is
/// turned on. With power it is decisive, because the corp AI's build scorer
/// cannot cover for it — that scorer maximises NET MARGIN per site, and power is
/// the cheapest good in the industrial roster, so it loses every comparison to a
/// price-ceiled electronics or alloys and a plant is never built. The seeder
/// chooses on ABSOLUTE SHORTFALL instead (`biggest_gap_resource` /
/// `best_recipe_for_gaps`), which is the selection rule a cheap, universally
/// needed good can actually win under. Generation provisions the utility; the
/// market decides everything downstream of it.
///
/// Zero while no rate is authored, so every pre-BL-708 world generates
/// byte-identically — including the whole ancient band, which has no power.
std::array<float, resource_count> body_upkeep_demand(const world& w, const recipe_registry& reg,
                                                     entity_id body_id)
{
    std::array<float, resource_count> demand = {};
    const building_upkeep_params& up = reg.building_upkeep();

    // One resolved basket per type, not per building — the basket is per (type,
    // band) and nothing here can change either.
    std::array<std::array<float, resource_count>, building_type_count> basket{};
    for (std::size_t t = 0; t < building_type_count; ++t)
        basket[t] = building_upkeep_goods(up, static_cast<building_type>(t), reg.era());

    // ASCENDING BUILDING ID (BL-1050), and the comment that stood here was
    // WRONG: it argued that because the SET of addends does not depend on the
    // walk order, neither does the sum. Float addition does not associate, so
    // the same set added in two orders is two different numbers — which is
    // exactly the defect, because `w.buildings`' layout is rebuilt by a
    // save/load (world_save.cpp re-inserts in id order) and laid out
    // differently again by another standard library. The set really is
    // order-free, so the eligibility filter runs unordered and only the sum is
    // sorted.
    std::vector<entity_id> bids;
    bids.reserve(w.buildings.size());
    for (const auto& [bid, b] : w.buildings)
    {
        // The live pass's own eligibility: a building under construction draws
        // through the CONSTRUCTION channel instead, and a decommissioned one is
        // not operating. Sizing against either would provision for demand that
        // is not there.
        if (b.ticks_remaining > 0 || b.decommissioned)
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        if (static_cast<std::size_t>(b.type) >= building_type_count)
            continue;
        bids.push_back(bid);
    }
    std::sort(bids.begin(), bids.end());

    for (const entity_id bid : bids)
    {
        const std::size_t ti = static_cast<std::size_t>(w.buildings.at(bid).type);
        for (std::size_t r = 0; r < resource_count; ++r)
            demand[r] += basket[ti][r];
    }
    return demand;
}

} // namespace

// BL-1232: the per-grid power measure is generation's (the charter walk sizes
// power on it). It is exported (corporation_generation.hpp) so harnesses read
// the walk's own figure; the scorer half that would also read it is HELD (Ben,
// 2026-10-08) and kept on branch bl1232-scorer-gate.

/// BL-1232 (power plants per grid; PRODUCTION.md, "Generation is sized per
/// grid, not per body", Ben 2026-10-07): @p body_id's POWER GAP, read per wired
/// grid (`tile_power_grid`, LOGISTICS.md § 3a) rather than across the body.
/// Power moves only within a grid, so a surplus on one grid cannot cancel a
/// deficit on another: the gap is the sum over the body's grids of
/// max(0, need - output), where need is the operating buildings' power upkeep
/// on the grid (`body_upkeep_demand`'s own eligibility and basket), keyed by
/// the building's own tile grid, and output is the generators FEEDING the grid
/// — keyed by their market centre's grid (`tile_feed_power_grid`), where their
/// listings land — at `accumulate_body_production`'s nominal rate. A building
/// in a dark province (grid 0) draws no power (economy_system.cpp, "no wire, no
/// draw") and a generator whose market centre is dark feeds nothing, so
/// neither counts. @p unpowered_short, when given, receives the short grids
/// fed by no generator at all (Unpowered grids first, Ben 2026-10-08).
///
/// A grid whose need is under HALF of one plant's output (@p plant_output: the
/// most power any in-band recipe makes, at the nominal rate) is left to a road
/// that joins it to a bigger grid, not given a plant of its own: its gap is
/// not counted and it is not short. @p short_grids receives every grid whose
/// gap is counted — the grids a power firm may stand on.
///
/// Sums walk ascending building id (BL-1050: float addition does not
/// associate); grids are keyed in a std::map. Deterministic.
float body_power_grid_gap(world& w, const recipe_registry& reg, entity_id body_id,
                          float plant_output, std::set<std::uint32_t>& short_grids,
                          std::set<std::uint32_t>* unpowered_short,
                          std::map<std::uint32_t, std::pair<float, float>>* need_output)
{
    short_grids.clear();
    if (unpowered_short != nullptr)
        unpowered_short->clear();
    const std::size_t pw = static_cast<std::size_t>(resource_type::power);
    const building_upkeep_params& up = reg.building_upkeep();
    std::array<float, building_type_count> need_of{};
    for (std::size_t t = 0; t < building_type_count; ++t)
        need_of[t] = building_upkeep_goods(up, static_cast<building_type>(t), reg.era())[pw];
    const float batches = nominal_processing_batches(reg);

    std::vector<entity_id> bids;
    bids.reserve(w.buildings.size());
    for (const auto& [bid, b] : w.buildings)
    {
        if (b.decommissioned)
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        bids.push_back(bid);
    }
    std::sort(bids.begin(), bids.end());

    std::map<std::uint32_t, std::pair<float, float>> grids;   // grid -> (need, output)
    for (const entity_id bid : bids)
    {
        const building_component& b = w.buildings.at(bid);
        // NEED is the draw side: a building draws on its OWN tile's grid.
        if (const std::uint32_t g = tile_power_grid(w, b.tile);
            g != 0 && b.ticks_remaining <= 0 && static_cast<std::size_t>(b.type) < building_type_count)
        {
            const float n = need_of[static_cast<std::size_t>(b.type)];
            if (n > 0.0f)
                grids[g].first += n;
        }
        // OUTPUT is the feed side (review round): a generator lists into its
        // tile's market, whose shelf is on the grid of the market's CENTRE
        // (`tile_feed_power_grid`) — that is the grid it serves.
        if (b.type == building_type::processing_facility)
            if (const recipe* rcp = reg.get_recipe(b.recipe); rcp && rcp->outputs[pw] > 0.0f)
                if (const std::uint32_t gf = tile_feed_power_grid(w, b.tile); gf != 0)
                    grids[gf].second += batches * rcp->outputs[pw];
    }

    if (need_output != nullptr)
        *need_output = grids;
    float gap = 0.0f;
    for (const auto& [g, no] : grids)
    {
        if (!(no.first > no.second) || no.first < 0.5f * plant_output)
            continue;
        gap += no.first - no.second;
        short_grids.insert(g);
        // Unpowered grids first (Ben, 2026-10-08): a short grid with NO
        // generation feeding it at all.
        if (unpowered_short != nullptr && !(no.second > 0.0f))
            unpowered_short->insert(g);
    }
    return gap;
}

/// The most power one plant makes: the largest power output of any in-band
/// processing recipe, at `nominal_processing_batches` — the plant the walk's
/// power firm is chartered with (`best_recipe_for_gaps` takes the most output
/// of the good). 0 where no recipe makes power.
float one_power_plant_output(const recipe_registry& reg)
{
    const std::size_t pw = static_cast<std::size_t>(resource_type::power);
    float best = 0.0f;
    const int n = reg.recipe_count(building_type::processing_facility);
    for (int i = 0; i < n; ++i)
        best = std::max(best, reg.recipe_at(building_type::processing_facility, i).outputs[pw]);
    return nominal_processing_batches(reg) * best;
}

float power_grids_to_serve(world& w, const recipe_registry& reg, entity_id body_id,
                           float plant_output, std::set<std::uint32_t>& serve,
                           const std::set<std::uint32_t>* chartered,
                           const std::set<std::uint32_t>* reachable)
{
    std::set<std::uint32_t> unpowered;
    const float gap = body_power_grid_gap(w, reg, body_id, plant_output, serve, &unpowered);
    // UNPOWERED GRIDS FIRST (PRODUCTION.md, Ben 2026-10-08: "every grid gets a
    // plant before any gets a second"): while any short grid has no power firm
    // chartered on it — and no generation feeding it — a power firm may serve
    // only such grids. A grid counts as powered the moment a power firm is
    // CHARTERED on it (@p chartered), live output or not, so the core grid
    // cannot take every capped firm before a second grid gets one.
    if (chartered != nullptr)
        for (auto it = unpowered.begin(); it != unpowered.end();)
            it = chartered->count(*it) != 0 ? unpowered.erase(it) : std::next(it);
    // An unpowered grid no tile in the deciding centre's windows can FEED
    // (@p reachable: the feed grids of its windows' tiles) never holds up the
    // others — it is dropped, and if none is left every short grid is served.
    // Otherwise an unreachable grid would narrow the serve set to itself and
    // starve every power firm (main session's reading of Ben's ruling).
    if (reachable != nullptr)
        for (auto it = unpowered.begin(); it != unpowered.end();)
            it = reachable->count(*it) == 0 ? unpowered.erase(it) : std::next(it);
    if (!unpowered.empty())
        serve = std::move(unpowered);
    return gap;
}

bool power_sized_per_grid(const recipe_registry& reg)
{
    if (!(one_power_plant_output(reg) > 0.0f))
        return false;
    const std::size_t pw = static_cast<std::size_t>(resource_type::power);
    for (std::size_t t = 0; t < building_type_count; ++t)
        if (building_upkeep_goods(reg.building_upkeep(), static_cast<building_type>(t),
                                  reg.era())[pw] > 0.0f)
            return true;
    return false;
}

namespace {

/// BL-709 — the body's CONSTRUCTION demand: how much construction capacity a
/// world with this many buildings standing on this body wants per tick, so the
/// pre-game seeder provisions yards for it (docs/economy/PRODUCTION.md
/// § Construction as a rate: "because generation can SEED CONSTRUCTION CAPACITY,
/// the demand for its inputs is non-zero from tick 0").
///
/// WHY THIS IS NOT `body_upkeep_demand`. It was, first, and it is worth
/// recording what happened, because the measurement is the argument. Authoring
/// construction capacity as an ordinary per-building UPKEEP draw — BL-708's
/// exact shape, which is the obvious thing to reach for — collapsed operating
/// firms from 198 of 328 to 33 of 317 on the ancient band, and cutting the rate
/// five-fold barely moved it (38 of 315). It is a CLIFF, not a curve, so it is
/// not a magnitude problem: a brand-new universal draw is unmet on tick 1 in
/// every market, `supply_factor_permille` decays before any yard's output can
/// reach a shelf, and the reflex tier decommissions the firm while the market is
/// still catching up. That is the BL-641 collapse arriving through the cold
/// start rather than through the rate — and it is why this is a SEEDER-SIDE
/// ESTIMATE with no live draw behind it. Nothing in the tick reads
/// `seed_capacity_per_building`.
///
/// WHAT IT ESTIMATES. The live consumer of capacity is the build project
/// (`run_construction`), which is EPISODIC — that is the whole defect this item
/// exists to fix — so it reads ~zero at generation time and the seeder cannot
/// size against it. The standing BUILDING STOCK is the steady-state proxy: a
/// world of N buildings replaces and extends itself at some rate, and that rate
/// is what a construction sector is for. It is MARKETS.md property 1's own
/// "more buildings" scaling, read as a provisioning target.
///
/// Counts sites UNDER CONSTRUCTION too, unlike `body_upkeep_demand` — deliberately.
/// A site under construction is precisely what consumes capacity; excluding it
/// would provision against everything except the actual consumer.
///
/// Generation provisions the sector; the market decides everything downstream of
/// it. Zero while no rate is authored, so every pre-BL-709 world generates
/// byte-identically.
///
/// Deterministic: `w.buildings` is unordered, but this walk only COUNTS over a
/// set that does not depend on order, so the finished figure is the same
/// whatever order it was reached in.
std::array<float, resource_count> body_construction_demand(const world& w,
                                                           const recipe_registry& reg,
                                                           entity_id body_id)
{
    std::array<float, resource_count> demand = {};
    const float per = reg.construction().seed_capacity_per_building;
    if (per <= 0.0f)
        return demand;

    int standing = 0;
    for (const auto& [bid, b] : w.buildings)
    {
        (void)bid;
        if (b.decommissioned)
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        ++standing;
    }
    demand[static_cast<std::size_t>(resource_type::construction_capacity)] =
        per * static_cast<float>(standing);
    return demand;
}

/// `body_id`'s aggregate demand: population_demand_params + BL-340's
/// background_demand_params baskets, weighted by every population centre's
/// `scale` on the body — the same two consumer-side pulls `clear_markets`
/// injects every tick (`inject_population_demand` / `inject_background_demand`,
/// market_clearing.cpp). Read here at pre-game generation time as the target
/// the measured stop condition below sizes background production against.
///
/// BL-708: the building-upkeep draw is added by the CALLER, per iteration, not
/// folded in here — it grows as the seeder places firms, so it has to be
/// re-measured rather than captured once. See the loop below.
std::array<float, resource_count> body_demand(const world& w, const recipe_registry& reg,
                                              entity_id body_id)
{
    // ASCENDING CENTRE ID (BL-1050). `total_scale` is a float sum — today over
    // integer scales small enough that every partial stays exact, so no shipped
    // world's number moves — but it is arithmetic in `population_centres`'
    // bucket order all the same, and that order is not part of the state (a
    // save/load re-inserts in id order; another standard library lays it out
    // differently again). Not one of BL-1050's five named readers: found
    // alongside them, same file, same gap loop, fixed the same way.
    std::array<float, resource_count> demand = {};
    std::vector<entity_id> centre_ids;
    centre_ids.reserve(w.population_centres.size());
    for (const auto& [cid, pcc] : w.population_centres)
    {
        (void)pcc;
        const auto tile_it = w.population_centre_tile.find(cid);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto tit = w.tiles.find(tile_it->second);
        if (tit == w.tiles.end() || tit->second.body != body_id)
            continue;
        centre_ids.push_back(cid);
    }
    std::sort(centre_ids.begin(), centre_ids.end());

    float total_scale = 0.0f;
    for (const entity_id cid : centre_ids)
        total_scale += static_cast<float>(w.population_centres.at(cid).scale);
    if (total_scale <= 0.0f)
        return demand;

    // BL-640: the ERA-RESOLVED baskets, the same vectors the two injectors
    // multiply by. Sizing background production against the unbanded tranche
    // would have generation chase demand the campaign's band never injects.
    const population_demand_params&          pd = reg.population_demand();
    const background_demand_params&          bd = reg.background_demand();
    const std::array<float, resource_count>& pb = reg.population_demand_basket();
    const std::array<float, resource_count>& bb = reg.background_demand_basket();
    for (std::size_t r = 0; r < resource_count; ++r)
        demand[r] = total_scale * (pd.demand_scale * pb[r]
                                  + bd.demand_scale * bb[r]);
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

/// BL-709 — the IN-BAND construction method that makes the most capacity per
/// batch, or -1 if this band authors none. Returned as a BROWSE index into
/// `recipe_at(processing_facility, i)`, which is the space the seeder's own
/// `recipe_i` already speaks.
///
/// Deterministic: registry order, strict `>`, so the first of two equal outputs
/// wins and the answer cannot depend on anything but the authored file.
int best_construction_recipe(const recipe_registry& reg)
{
    const std::size_t cap = static_cast<std::size_t>(resource_type::construction_capacity);
    const int n = reg.recipe_count(building_type::processing_facility);
    int   best_i = -1;
    float best_q = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float q = reg.recipe_at(building_type::processing_facility, i).outputs[cap];
        if (q > best_q)
        {
            best_q = q;
            best_i = i;
        }
    }
    return best_i;
}

// ---------------------------------------------------------------------------
// BL-1185 / BL-1188 — CHAIN-FEASIBLE PLACEMENT (CORPORATION_GENERATION.md
// § Pass 3, "Chain-feasible"; Ben, 2026-10-04: a HARD RULE, not a search score)
// ---------------------------------------------------------------------------
// No processor is placed unless every input of its recipe has a producer WITHIN
// REACH of the processor's market: an extractor on a deposit of that good, or a
// processor whose recipe makes it.
//
// WITHIN REACH is ONE DEFINITION, `input_reach` (src/world/input_reach.hpp;
// docs/ai/AI_OPPONENT.md § Build only what runs): placement asks the same
// `market_within_reach` the play-time scorer does, with no economy report — the
// generation form: the producer's market IS the consumer's, or the dispatcher's
// own market leg is viable and its gate passes with the destination short,
// `R x base_dest - haul > (1 + dispatch_margin) x base_src`, each market at its
// own base, R = reservation_mult (ceil_mult where the ceiling is off). A
// producer is `building_produces` (nominal, BL-437 co-extracts included). WHEN
// reach is read: the node set is taken at the pass's start, and each market pair
// is priced once, the first time a placement asks, and memoised for the rest of
// the pass — so a port chartered later in the walk widens a pair only if that
// pair had not yet been asked (placement may miss a lane the finished world has,
// never assume one it lacks).
//
// WHICH PRODUCERS COUNT is this file's, not the helper's: the buildings standing
// at the moment the processor's recipe is decided — the base installations,
// every corporation placed before this one, and this corporation's own holdings
// (a firm's assets are all placed before any of its processors is checked, so
// its own feed mines count, and a processor checked earlier in its own asset
// list counts for a later one). Nothing placed later is foreseen. So the scan
// below walks the buildings standing now and asks the helper only the per-
// building producer test and the per-pair reach; it never uses the helper's
// lazy producer index, which describes the buildings when it was first built.
//
// Deterministic: every answer is an existence test over a set (order-free), and
// the leg memo is an ordered map. The leg calls warm the logistics caches, so
// every pass that builds a reach context clears them again when it ends
// (`invalidate_logistics_caches`) — its caller may tick next.

/// The reach context one placement pass shares: `input_reach`, with no report.
using chain_reach = input_reach;

chain_reach make_chain_reach(const world& w, const recipe_registry& reg)
{
    return make_input_reach(w, reg);
}

/// How near an input's nearest producer stands, best first.
enum chain_tier : int
{
    chain_tier_own    = 0, ///< one of the consumer corporation's own holdings, same market
    chain_tier_market = 1, ///< any producer in the consumer's market
    chain_tier_reach  = 2, ///< a producer in another market within reach
    chain_tier_none   = 3, ///< no producer within reach: infeasible
};

/// True when @p b produces resource @p r — the one producer test
/// (`building_produces`, input_reach.hpp: nominal, generation reading).
bool chain_produces(const world& w, const recipe_registry& reg, const building_component& b,
                    std::size_t r)
{
    return building_produces(w, reg, b, r);
}

/// The tier of input @p r for a processor @p self in market @p consumer_market.
/// @p own, when given, is the consumer corporation's holdings.
int chain_input_tier(world& w, const recipe_registry& reg, chain_reach& cr, entity_id self,
                     entity_id consumer_market, std::size_t r, const std::vector<entity_id>* own)
{
    if (consumer_market == null_entity)
        return chain_tier_none;
    int best = chain_tier_none;
    std::vector<entity_id> far;
    for (const auto& [bid, b] : w.buildings)
    {
        if (bid == self || !chain_produces(w, reg, b, r))
            continue;
        const entity_id mp = market_for_tile(w, b.tile);
        if (mp == null_entity)
            continue;
        if (mp == consumer_market)
        {
            const bool mine = own != nullptr && std::find(own->begin(), own->end(), bid) != own->end();
            best = std::min(best, mine ? static_cast<int>(chain_tier_own)
                                       : static_cast<int>(chain_tier_market));
        }
        else
            far.push_back(mp);
    }
    if (best != chain_tier_none)
        return best;

    std::sort(far.begin(), far.end());
    far.erase(std::unique(far.begin(), far.end()), far.end());
    for (const entity_id mp : far)
        if (market_within_reach(w, reg, cr, mp, consumer_market, r))
            return chain_tier_reach;
    return chain_tier_none;
}

/// A recipe's tier at a processor: its worst input's (a recipe with no inputs is
/// `own`, it needs nothing).
int chain_recipe_tier(world& w, const recipe_registry& reg, chain_reach& cr, entity_id self,
                      entity_id consumer_market, const recipe& rc, const std::vector<entity_id>* own)
{
    // Propellant routes follow the body's air (Ben, 2026-10-09): a recipe the
    // processor's body cannot run is `none` here, so every generation walk that
    // ranks recipes through this tier (make_chain_feasible, the rung walk, the
    // refused-draw read) passes it over.
    if (const auto sb = w.buildings.find(self);
        sb != w.buildings.end() && !recipe_runs_at_tile(w, rc, sb->second.tile))
        return chain_tier_none;
    int worst = chain_tier_own;
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (!(rc.inputs[r] > 0.0f))
            continue;
        worst = std::max(worst, chain_input_tier(w, reg, cr, self, consumer_market, r, own));
        if (worst == chain_tier_none)
            break;
    }
    return worst;
}

// BL-1233 (processors to inputs; Pass 3 "Sized to its inputs", Ben, 2026-10-07):
// a producer within reach is necessary, not sufficient. A processor is placed
// only where the SPARE reachable output of each input covers its draw at t_idle,
// spare being the producers' output in reach less the draw of every processor
// already standing there — the play-time scorer's own supply clause
// (`recipe_inputs_supplied` -> `input_supply_covers`, input_reach.hpp), judged at
// the labour a new plant is judged at (`judged_batches`). The stock clause is not
// asked: an opening shelf is eaten through, a producer is not.
//
// WHICH BUILDINGS COUNT is still this file's rule (the ones standing when the
// recipe is decided), so the helper's producer/draw index is brought up to the
// buildings standing before every processor's decision (`chain_begin_decision`
// -> `input_reach_refresh`, which re-reads only what changed: a full rebuild per
// decision cost the landscape search ~5x, 115k rebuilds and 20 s on seed 0).
// The haul memo is kept for the pass, as reach is read.

/// Bring the producer/draw index up to the buildings standing now. Call once
/// before deciding each processor.
void chain_begin_decision(const world& w, const recipe_registry& reg, chain_reach& cr)
{
    input_reach_refresh(w, reg, cr);
}

/// The sized test for recipe @p rc at processor @p self (see above).
bool chain_recipe_sized(world& w, const recipe_registry& reg, chain_reach& cr, entity_id self,
                        entity_id consumer_market, const recipe& rc)
{
    const auto bit = w.buildings.find(self);
    if (bit == w.buildings.end())
        return false;
    return recipe_inputs_supplied(w, reg, cr, consumer_market, rc,
                                  judged_batches(reg, bit->second), self);
}

/// A recipe's PLACEMENT tier: its chain tier, or `none` where an input's spare
/// reachable supply does not cover the plant's draw (BL-1233).
int chain_recipe_placeable(world& w, const recipe_registry& reg, chain_reach& cr, entity_id self,
                           entity_id consumer_market, const recipe& rc,
                           const std::vector<entity_id>* own)
{
    // The sized test first: it is the cheaper read (the reach-set memo), and it
    // implies a producer within reach — its supply counts only producers other
    // than @p self in markets within reach of C, one of which must land a unit —
    // so a recipe it passes is never `none` by the chain tier, which then only
    // ranks it (own, market, reach).
    if (!chain_recipe_sized(w, reg, cr, self, consumer_market, rc))
        return chain_tier_none;
    return chain_recipe_tier(w, reg, cr, self, consumer_market, rc, own);
}

/// Browse indices of the in-band processing recipes that output @p good, in the
/// gap selection's own preference: most output of the good first, ties to
/// registry order (`best_recipe_for_gaps` on that good alone ranks the same way).
std::vector<int> chain_recipes_for_good(const recipe_registry& reg, std::size_t good)
{
    std::vector<int> out;
    const int n = reg.recipe_count(building_type::processing_facility);
    for (int i = 0; i < n; ++i)
        if (reg.recipe_at(building_type::processing_facility, i).outputs[good] > 0.0f)
            out.push_back(i);
    std::stable_sort(out.begin(), out.end(), [&](int a, int b) {
        return reg.recipe_at(building_type::processing_facility, a).outputs[good]
             > reg.recipe_at(building_type::processing_facility, b).outputs[good];
    });
    return out;
}

/// True when one of @p assets produces an input of some recipe that makes
/// @p good — a landing that can bring a chain-refused good within reach.
bool chain_firm_feeds_good(const world& w, const recipe_registry& reg,
                           const std::vector<entity_id>& assets, std::size_t good)
{
    for (const int i : chain_recipes_for_good(reg, good))
    {
        const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (!(rc.inputs[r] > 0.0f))
                continue;
            for (const entity_id bid : assets)
            {
                const auto bit = w.buildings.find(bid);
                if (bit != w.buildings.end() && chain_produces(w, reg, bit->second, r))
                    return true;
            }
        }
    }
    return false;
}

/// Undo one authored building: `author_building` writes the building, its
/// stockpile and the occupancy, and all three go.
void chain_unplace(world& w, entity_id bid, std::unordered_set<entity_id>& occupied)
{
    const auto bit = w.buildings.find(bid);
    if (bit != w.buildings.end())
        occupied.erase(bit->second.tile);
    w.buildings.erase(bid);
    w.stockpiles.erase(bid);
}

/// BL-1217 D6 (Ben, 2026-10-09; CORPORATION_GENERATION.md § Pass 6, "No
/// processor is placed beyond its output's want") — THE ONE WANT TEST every
/// generation path that gives a processor a recipe reads.
///
/// WANT per good is final demand plus the derived demand of what stands or is
/// chartered: operating upkeep, construction, every standing processor's
/// inputs at the nominal rate, and the prospective draws of processors the
/// sized rule refused (BL-1233). PRODUCTION is at that same nominal rate
/// (`accumulate_body_production`) — the units the walk's gap reads. Inside the
/// walk the caller hands in the walk's OWN per-firm arrays, so "short" here is
/// the walk's gap exactly. POWER is sized per grid where the band sizes it so
/// (`power_sized_per_grid`), as the walk does: a power recipe is short only
/// on a processor that feeds a short grid.
struct output_want
{
    entity_id                         body = null_entity;
    std::array<float, resource_count> want{};
    std::array<float, resource_count> production{};
    bool                              power_by_grid = false;
    std::set<std::uint32_t>           short_grids; ///< power: the grids still short

    /// Is @p rc's primary output still short for a processor on @p tile? A
    /// good no consumer wants is never short, so it gets no maker for its sake.
    bool short_for(world& w, const recipe& rc, entity_id tile) const
    {
        const std::size_t p = static_cast<std::size_t>(primary_output_resource(rc));
        if (power_by_grid && p == static_cast<std::size_t>(resource_type::power))
        {
            const std::uint32_t g = tile_feed_power_grid(w, tile);
            return g != 0 && short_grids.count(g) != 0;
        }
        return want[p] > production[p];
    }

    /// Book a processor running @p rc (@p sign -1 takes it out): its outputs
    /// into production and its inputs into want, at the nominal rate. A power
    /// recipe re-reads the short grids off the world, IN BOTH DIRECTIONS, so
    /// the caller writes the world first: booking in, the processor's recipe
    /// is already set; taking out, it is already cleared (`make_chain_feasible`
    /// clears it around the take-out and restores it for the re-decision).
    void book(world& w, const recipe_registry& reg, const recipe& rc, float sign = 1.0f)
    {
        const float batches = nominal_processing_batches(reg);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (rc.outputs[r] > 0.0f)
                production[r] += sign * batches * rc.outputs[r];
            if (rc.inputs[r] > 0.0f)
                want[r] += sign * batches * rc.inputs[r];
        }
        if (power_by_grid && body != null_entity
            && rc.outputs[static_cast<std::size_t>(resource_type::power)] > 0.0f)
        {
            short_grids.clear();
            (void)body_power_grid_gap(w, reg, body, one_power_plant_output(reg), short_grids);
        }
    }
};

/// Fill @p o's power half off the world: per grid where the band sizes power
/// per grid, the grids `body_power_grid_gap` counts short.
void measure_power_grids(world& w, const recipe_registry& reg, output_want& o)
{
    o.power_by_grid = power_sized_per_grid(reg);
    o.short_grids.clear();
    if (o.power_by_grid)
        (void)body_power_grid_gap(w, reg, o.body, one_power_plant_output(reg), o.short_grids);
}

/// The want test's arrays measured off the world for @p body, with the
/// caller's FINAL demand (the walk's `consumer_demand`; `body_demand` where a
/// path has no such capture). Prospective draws are the caller's to raise
/// (`prospective_total` + `raise_by_want`): only a walk holds a book.
output_want measure_output_want(world& w, const recipe_registry& reg, entity_id body,
                                const std::array<float, resource_count>& final_demand)
{
    output_want o;
    o.body = body;
    o.want = final_demand;
    const std::array<float, resource_count> upkeep       = body_upkeep_demand(w, reg, body);
    const std::array<float, resource_count> construction = body_construction_demand(w, reg, body);
    const std::array<float, resource_count> inputs       = body_processor_input_demand(w, reg, body);
    for (std::size_t r = 0; r < resource_count; ++r)
        o.want[r] += upkeep[r] + construction[r] + inputs[r];
    accumulate_body_production(w, reg, body, o.production);
    measure_power_grids(w, reg, o);
    return o;
}

/// Make one corporation's freshly placed holdings chain-feasible, in place.
///
/// Every processor in @p assets (in asset order) is given a recipe whose inputs
/// are all within reach of its market, or is UNPLACED:
///  * @p serve given (a firm chartered for one good): the first recipe of
///    @p serve that is feasible there — the firm's good, by the gap selection's
///    preference. A firm none of whose processors can serve its good is not a
///    firm for that good: EVERY holding is unplaced and the call answers false.
///  * @p serve null (a specialist's processors, and the incidental processor of
///    an extraction or trade mix): the feasible recipe of the best tier — the
///    corporation's own feed first, then its own market, then reach — ties to
///    browse order (the band's default recipe is browse index 0).
/// A processor that already carries a recipe is re-decided all the same: the
/// rule reads the ground, not what an earlier pass wrote.
///
/// A corporation whose ANCHOR — its defining asset, the first placed — is a
/// processor (the processing mix) is a processing corporation: left with no
/// processor it is not that corporation, so it too is unplaced whole rather
/// than handed on as a mine wearing a processing focus. And a charter whose
/// ANCHOR is unplaced is rejected whole: every gate upstream (the province cap,
/// the rung, the region) tested the anchor, not the holding behind it.
///
/// @p whole false (a roster that already exists, `enforce_chain_feasible_roster`)
/// only unplaces the infeasible processors and never rejects the rest.
///
/// BL-1217 D6 (no processor beyond its output's want): every recipe, on either
/// path, is admitted only while `output_want::short_for` says its primary
/// output is still short; the want is the caller's (@p want_in) or measured
/// here (`measure_output_want` with `body_demand`) on the first processor's
/// body. Each decision books its processor, so a firm's second processor sees
/// the first. A processor of this call that already carries a recipe (a
/// re-decision) is taken out first, output and inputs. ONE EXCEPTION (Ben,
/// 2026-10-09): on the @p serve path a works after the first one placed is
/// admitted whatever the want — a chartered processing firm keeps its second
/// works — and a sized-rule refusal of it still leaves its prospective draw.
///
/// @return false when nothing is left placed (the caller treats the charter as
///         a placement that found no feasible ground).
bool make_chain_feasible(world& w, const recipe_registry& reg, chain_reach& cr,
                         std::vector<entity_id>& assets, std::unordered_set<entity_id>& occupied,
                         const std::vector<int>* serve, bool whole = true,
                         refused_draw* refused = nullptr,
                         const output_want* want_in = nullptr)
{
    const int n = reg.recipe_count(building_type::processing_facility);
    output_want ow;
    bool        ow_ready = false;
    const auto  ensure_want = [&](entity_id body) {
        if (ow_ready)
            return;
        ow       = (want_in != nullptr) ? *want_in
                                        : measure_output_want(w, reg, body, body_demand(w, reg, body));
        ow.body  = body;
        ow_ready = true;
        for (const entity_id a : assets)
        {
            const auto ait = w.buildings.find(a);
            if (ait == w.buildings.end() || ait->second.type != building_type::processing_facility
                || ait->second.decommissioned)
                continue;
            const auto tit = w.tiles.find(ait->second.tile);
            if (tit == w.tiles.end() || tit->second.body != body)
                continue;
            // A re-decision (every caller hands freshly authored holdings, which
            // carry no recipe, so this is defensive): taken out with its recipe
            // CLEARED for the grid re-read, then restored — the re-decision
            // below still reads siblings' recipes for their chain tiers.
            const uint16_t had_id = ait->second.recipe;
            if (const recipe* had = reg.get_recipe(had_id))
            {
                ait->second.recipe = no_recipe;
                ow.book(w, reg, *had, -1.0f);
                ait->second.recipe = had_id;
            }
        }
    };
    bool processing_anchor = false;
    if (!assets.empty())
        if (const auto ait = w.buildings.find(assets.front()); ait != w.buildings.end())
            processing_anchor = ait->second.type == building_type::processing_facility;
    std::vector<entity_id> kept;
    kept.reserve(assets.size());
    int serving = 0;
    for (const entity_id bid : assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        if (bit->second.type != building_type::processing_facility)
        {
            kept.push_back(bid);
            continue;
        }
        const entity_id market = market_for_tile(w, bit->second.tile);
        const entity_id ptile  = bit->second.tile;
        uint16_t chosen = no_recipe;
        if (const auto tit = w.tiles.find(ptile); tit != w.tiles.end())
            ensure_want(tit->second.body);
        // BL-1217 D6: a recipe is a candidate only while its output is short.
        const auto wanted = [&](const recipe& rc) { return !ow_ready || ow.short_for(w, rc, ptile); };
        // THE SECOND WORKS IS KEPT (Ben, 2026-10-09; CORPORATION_GENERATION.md
        // § Pass 6, the exceptions): a firm chartered FOR a good (@p serve)
        // keeps its works after the first even once the first covers the gap —
        // the firm's authored pair stands whole. Only that, and only ONE extra:
        // the works placed while exactly one serves (the pair's second) is
        // admitted whatever the want; the FIRST works, any works past the pair,
        // an incidental processor and a specialist's stay bounded by want. A
        // kept second works the sized rule refuses still leaves its draw.
        const bool second_works = serve != nullptr && serving == 1;
        const auto admitted = [&](const recipe& rc) { return second_works || wanted(rc); };
        chain_begin_decision(w, reg, cr); // BL-1233: spare is read over what stands now
        if (serve != nullptr)
        {
            for (const int i : *serve)
            {
                const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                if (!admitted(rc))
                    continue;
                if (chain_recipe_placeable(w, reg, cr, bid, market, rc, &assets) != chain_tier_none)
                {
                    chosen = reg.recipe_id(rc.name);
                    break;
                }
            }
        }
        else
        {
            int best_tier = chain_tier_none;
            for (int i = 0; i < n && best_tier != chain_tier_own; ++i)
            {
                const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                if (!wanted(rc))
                    continue;
                const int t = chain_recipe_placeable(w, reg, cr, bid, market, rc, &assets);
                if (t < best_tier)
                {
                    best_tier = t;
                    chosen    = reg.recipe_id(rc.name);
                }
            }
        }
        if (chosen == no_recipe)
        {
            // BL-1233 ruling A: a firm's processor turned away by the SIZED rule
            // — some recipe for its good has a producer of every input within
            // reach (chain tier not `none`), none has the spare — leaves its
            // prospective draw: the first such recipe in the good's preference.
            // A processor with no producer at all leaves none (a cold start is
            // not begun). Read before it is unplaced, on the ground as it stands.
            // BL-1217 D6: a recipe refused for want (its good already covered)
            // was not refused by the sized rule, and leaves no draw; a kept
            // second works is admitted whatever the want, so its refusal is
            // the sized rule's and its draw stands.
            if (refused != nullptr && !refused->set && serve != nullptr)
            {
                const float batches = nominal_processing_batches(reg);
                for (const int i : *serve)
                {
                    const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
                    if (!admitted(rc))
                        continue;
                    if (chain_recipe_tier(w, reg, cr, bid, market, rc, &assets) == chain_tier_none)
                        continue;
                    refused->set    = true;
                    refused->market = market;
                    for (std::size_t r = 0; r < resource_count; ++r)
                        refused->draw[r] = rc.inputs[r] > 0.0f ? batches * rc.inputs[r] : 0.0f;
                    break;
                }
            }
            chain_unplace(w, bid, occupied);
            continue;
        }
        bit->second.recipe = chosen;
        if (ow_ready)
            if (const recipe* took = reg.get_recipe(chosen))
                ow.book(w, reg, *took); // BL-1217 D6: the next processor sees this one
        kept.push_back(bid);
        ++serving;
    }
    // THE ANCHOR STAYS OR THE CHARTER GOES (BL-1185 review): the anchor is what
    // every gate before this one tested — its province against the province
    // cap, its rung, its region. Promoting the next holding in its place would
    // land a firm the caps never saw, so an unplaced anchor rejects the charter.
    const bool anchor_lost = !assets.empty()
        && (kept.empty() || kept.front() != assets.front());
    assets = std::move(kept);
    if (!whole)
        return !assets.empty();

    if (anchor_lost || ((serve != nullptr || processing_anchor) && serving == 0) || assets.empty())
    {
        for (const entity_id bid : assets)
            chain_unplace(w, bid, occupied);
        assets.clear();
        return false;
    }
    return true;
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

bool corporation_has_opening_force(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    if (it == w.corporations.end())
        return false;
    for (const entity_id bid : it->second.assets)
    {
        const auto b = w.buildings.find(bid);
        if (b != w.buildings.end() && b->second.type == building_type::military_base)
            return true;
    }
    return false;
}

void arm_rivals(world& w)
{
    // Ascending corporation id (std::map), so the bases claim ground in one
    // order on every machine; occupancy from every building standing.
    std::map<entity_id, bool> rivals;
    for (const auto& [cid, cc] : w.corporations)
        if (!cc.is_background && cid != w.player_entity)
            rivals[cid] = true;
    std::unordered_set<entity_id> occupied;
    occupied.reserve(w.buildings.size() * 2);
    for (const auto& [bid, bc] : w.buildings)
        occupied.insert(bc.tile);
    for (const auto& [cid, _] : rivals)
        if (!corporation_has_opening_force(w, cid))
            seed_starting_military(w, cid, occupied);
}

void arm_corporation(world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    if (it == w.corporations.end() || it->second.is_background
        || corporation_has_opening_force(w, corp))
        return;
    std::unordered_set<entity_id> occupied;
    occupied.reserve(w.buildings.size() * 2);
    for (const auto& [bid, bc] : w.buildings)
        occupied.insert(bc.tile);
    seed_starting_military(w, corp, occupied);
}

float seat_clean_slate(world& w, entity_id corp)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return 0.0f;
    std::vector<entity_id> sites;
    for (const entity_id bid : cit->second.assets)
        if (const auto b = w.buildings.find(bid); b != w.buildings.end() && b->second.ticks_remaining > 0)
            sites.push_back(bid);
    std::sort(sites.begin(), sites.end());
    float refund = 0.0f;
    for (const entity_id bid : sites)
    {
        // What run_construction charged this site and nothing else: never an
        // estimate, so the refund cannot exceed what the firm paid in.
        const float paid = std::max(0.0f, w.buildings.at(bid).construction_paid);
        if (demolish_building(w, corp, bid))
            refund += paid;
    }
    if (refund > 0.0f)
    {
        corporation_component& cc = w.corporations.at(corp);
        if (cc.refund_unbooked == 0.0f)
            cc.refund_opening = cc.balance; // the exact pre-refund balance
        cc.balance         += refund;
        cc.refund_unbooked += refund;
    }
    return refund;
}

int seat_release_dial_idled(world& w, entity_id corp)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return 0;
    int released = 0;
    for (const entity_id bid : cit->second.assets) // per-building write: order-free
    {
        const auto b = w.buildings.find(bid);
        if (b == w.buildings.end() || !dial_idled(b->second))
            continue;
        b->second.workforce_auto = true;
        ++released;
    }
    return released;
}

void move_seat_force(world& w, entity_id previous, entity_id corp)
{
    if (previous == corp)
        return;
    disarm_corporation(w, corp);
    if (previous != null_entity)
        arm_corporation(w, previous);
}

void disarm_corporation(world& w, entity_id corp)
{
    auto it = w.corporations.find(corp);
    if (it == w.corporations.end())
        return;
    // Its units first (ascending id, so the erase order is fixed), then its
    // military bases with their stockpiles, out of its asset list too.
    std::vector<entity_id> units;
    for (const auto& [uid, uc] : w.units)
        if (uc.owner == corp)
            units.push_back(uid);
    std::sort(units.begin(), units.end());
    for (const entity_id uid : units)
        w.units.erase(uid);
    std::vector<entity_id>& assets = it->second.assets;
    std::vector<entity_id> kept;
    kept.reserve(assets.size());
    for (const entity_id bid : assets)
    {
        const auto b = w.buildings.find(bid);
        if (b != w.buildings.end() && b->second.type == building_type::military_base)
        {
            w.buildings.erase(bid);
            w.stockpiles.erase(bid);
            continue;
        }
        kept.push_back(bid);
    }
    assets = std::move(kept);
}

// ---------------------------------------------------------------------------
// Pass 2b — ownership class (BL-631, re-pointed by BL-638). One more mapping
// over an existing signal — and BL-638 is the story of which one.
//
// BL-631 derived the class from Stage 4's industrialisation timing. That read
// looks equivalent to Stage 1's and is not, and its own measured row exposed the
// difference: at the default 0 CE epoch all 64 corporations and all 1298 regions
// across 8 seeds classed `closed`, because settlement.cpp breaks out of Stage 4
// before lighting a furnace on an antiquity world. The mapping was correct; the
// input was degenerate.
//
// So the signal is the ENFORCEABLE PROMISE. The institutions that make a
// contract enforceable are the institutions that make a share transferable —
// which is what Pass 2b's justifying sentence always said — and Stage 1 runs in
// EVERY era. See `settlement.hpp`'s `charter_reach` for the frame itself.
// ---------------------------------------------------------------------------

namespace {

/// Does this national character HOLD THE FIRM CLOSE — the design's rung-2
/// polity, which has the promise and will not let a stranger own a share of it?
///
/// Only `authoritarian`, and the exclusion is the load-bearing half.
/// `nation_component::politics` is settlement.cpp's industrialisation-timing
/// tercile ("ideology <- industrialisation timing against neighbours"), so each
/// enumerator carries a furnace fact as well as a political word:
///
///   * `authoritarian` — the late tercile, AND the enum's own "centralised
///     state authority". This is the design's *statist* case and the one rung 2
///     names: a polity that lived the promise but keeps the books.
///   * `isolationist` — the NEVER tercile, and NOT admitted here. On an
///     antiquity world no furnace lights anywhere, so every nation is
///     isolationist; reading that as "statist" would demote every region in
///     every default campaign back to `private` and re-create, one rung down,
///     exactly the degenerate distribution BL-638 exists to remove. "Nobody
///     built a furnace" is a statement about industry, not about corporate law
///     — treating it as one would smuggle Stage 4 back in through the polity
///     term after the region term stopped reading it.
///   * `mercantile` / `technocratic` — early and mid movers, neither statist.
///
/// The nation term is deliberately a DEMOTION and never a promotion: the region
/// stays the primary discriminator, so two corps in one nation still differ.
bool holds_the_firm_close(ideology politics)
{
    return politics == ideology::authoritarian;
}

} // namespace

ownership_class ownership_from_region(const region& p, const charter_reach& ch,
                                      ideology politics)
{
    // NEVER REACHED THE PROMISE -> closed. No charter, no filing, no market in
    // the firm at all. Ground the charter never travelled to, or a world where
    // no charter was ever written.
    if (!charter_copied(ch, p))
        return ownership_class::closed;

    // REACHED, BUT ONLY AS A COPY -> private. The institution arrived as a
    // foreign practice rather than as this people's own oath: a firm can be a
    // firm here and it trades, but its books stay its own.
    if (!charter_lived(ch, p))
        return ownership_class::privately_held;

    // LIVED IT. The institutions that make a contract enforceable are the ones
    // that make a share transferable — so a firm here can have owners who are
    // not its operators, unless the polity holds it close.
    return holds_the_firm_close(politics) ? ownership_class::privately_held
                                          : ownership_class::publicly_held;
}

ownership_class ownership_from_character(ideology politics)
{
    switch (politics)
    {
        // never industrialised -> closed
        case ideology::isolationist:  return ownership_class::closed;
        // the late tercile, and the statist one -> private
        case ideology::authoritarian: return ownership_class::privately_held;
        // the MID tercile. With no region to say "early", the nation's own
        // timing is the only timing there is, and mid is not an early mover.
        case ideology::technocratic:  return ownership_class::privately_held;
        // the EARLY tercile, and the enum's own open-markets rung -> public
        case ideology::mercantile:    return ownership_class::publicly_held;
    }
    return ownership_class::closed;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

std::vector<entity_id> generate_corporations(
    world& w,
    const corporation_params& params,
    uint32_t seed,
    const settlement_state* settle,
    generation_progress* progress,
    const recipe_registry* reg)
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

    // Pass 2b (BL-631) - one class per corp, filled by the SAME walk that fills
    // the focus above. Parallel to corp_focuses, not a second pass: both are
    // reads of one region record, so a corp cannot end up with a focus from one
    // attempt and a class from another.
    std::vector<ownership_class> corp_ownership(
        static_cast<std::size_t>(corp_count), ownership_class::closed);

    if (settle && !settle->regions.empty())
    {
        // BL-219 — focus DERIVED from the corp's home region, then a
        // world-level reject-and-reroll against a diversity floor. A floor on
        // the SET is not a quota on any MEMBER, so no individual corporation's
        // focus ever becomes inexplicable; the reroll re-picks which regions
        // the corps anchor to, it never patches a corp's focus directly.
        // BL-631 -- IS THE PUBLIC FLOOR MEETABLE AT ALL? Answered once, before
        // any attempt, because the answer cannot change between attempts: a
        // reroll re-picks WHICH region each corp anchors to, never which regions
        // exist or where the charter reached.
        //
        // BL-638 MADE THIS RARE AND DID NOT MAKE IT DEAD. Under the retired
        // Stage 4 read it fired on every default (0 CE) campaign: an antiquity
        // world skips the energy transition, so `median_industrial_year` stayed 0,
        // every region was never-industrialised and every corporation classed
        // `closed` whichever region it anchored to. Re-pointing to Stage 1 removes
        // that whole class of world, because the ladder picks a charter cradle in
        // every era. What survives is the genuine case the design names: a world
        // with no agrarian cradle wrote no charter at all, and a nation whose every
        // region sits beyond the charter's reach can host no public firm however
        // the reroll shuffles it.
        //
        // The floor is WAIVED there rather than burned against. Without this
        // pre-pass such a world would spend all six attempts and stand on attempt
        // 5's region set instead of the first attempt that met the FOCUS floor --
        // silently relocating every corporation in it, to satisfy a condition that
        // was unsatisfiable before the first draw. It is exactly the waiver the
        // focus floor already carries for `corp_count < 3` ("unmeetable by
        // construction, so it does not apply"), applied to the second condition.
        // Where public IS reachable the floor bites normally.
        bool public_reachable = false;
        for (int c = 0; c < corp_count && !public_reachable; ++c)
        {
            const entity_id home_nid = nation_ids[static_cast<std::size_t>(
                home_nation_idx[static_cast<std::size_t>(c)])];
            const auto nit = w.nations.find(home_nid);
            const ideology pol = (nit != w.nations.end())
                ? nit->second.politics : ideology::isolationist;

            bool any_region = false;
            for (const region& p : settle->regions)
            {
                if (p.nation < 0 || p.nation >= nation_count) continue;
                if (nation_ids[static_cast<std::size_t>(p.nation)] != home_nid) continue;
                any_region = true;
                if (ownership_from_region(p, settle->charter, pol)
                        == ownership_class::publicly_held)
                {
                    public_reachable = true;
                    break;
                }
            }
            if (!any_region
             && ownership_from_character(pol) == ownership_class::publicly_held)
                public_reachable = true;
        }

        constexpr int max_attempts = 6;
        for (int attempt = 0; attempt < max_attempts; ++attempt)
        {
            std::mt19937 attempt_rng(seed_focus ^ (static_cast<uint32_t>(attempt) * 0x9E3779B1u));
            focus_counts = { 0, 0, 0 };
            int public_count = 0;

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
                    // BL-631: the class takes the SAME fallback the focus just
                    // took - national character, no region invented for it.
                    const ideology pol = (it != w.nations.end())
                        ? it->second.politics : ideology::isolationist;
                    const ownership_class oc = ownership_from_character(pol);
                    corp_ownership[static_cast<std::size_t>(c)] = oc;
                    if (oc == ownership_class::publicly_held) ++public_count;
                    continue;
                }

                std::uniform_int_distribution<int> pick(0, static_cast<int>(options.size()) - 1);
                const int pi = options[static_cast<std::size_t>(pick(attempt_rng))];
                home_region_idx[static_cast<std::size_t>(c)] = pi;

                const region& home_p = settle->regions[static_cast<std::size_t>(pi)];

                const industrial_focus f = focus_from_region(
                    home_p, settle->median_industrial_year);
                corp_focuses[static_cast<std::size_t>(c)] = f;
                focus_counts[static_cast<std::size_t>(f)]++;

                // BL-631 - Pass 2b, off the same region record and the same
                // median. The home nation supplies only the enforceable-promise
                // term; the region stays the primary discriminator.
                const auto nit = w.nations.find(home_nid);
                const ideology pol = (nit != w.nations.end())
                    ? nit->second.politics : ideology::isolationist;
                const ownership_class oc = ownership_from_region(
                    home_p, settle->charter, pol);
                corp_ownership[static_cast<std::size_t>(c)] = oc;
                if (oc == ownership_class::publicly_held) ++public_count;
            }

            // TWO conditions on ONE reroll (BL-631 joined BL-219 here).
            //
            //  1. No focus class wholly unrepresented across the world. With
            //     fewer corps than classes that floor is unmeetable by
            //     construction, so it does not apply.
            //  2. At least one PUBLIC specialist. If nobody is public then
            //     nothing in the world files a return and nothing is buyable,
            //     which disables two whole FINANCE.md surfaces at once. It is NOT
            //     waived by the corp_count < 3 case -- one corp can be public --
            //     but it IS waived when no region any corp could anchor to would
            //     yield a public firm (`public_reachable`, computed above).
            //
            // Neither condition patches a corporation. The reroll re-picks which
            // regions the corps anchor to and derives every field again from
            // scratch; an unmet floor after the attempt cap STANDS, on the same
            // reasoning the focus floor always used - an honest unmet floor beats
            // a hand-fixed corp.
            const bool focus_floor_met =
                corp_count < 3
                || (focus_counts[0] > 0 && focus_counts[1] > 0 && focus_counts[2] > 0);
            const bool public_floor_met = public_count > 0 || !public_reachable;
            if (focus_floor_met && public_floor_met) break;
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

            // BL-631: no settlement pass means no region for anyone, so every
            // corp on this path takes the national-character read - the same
            // fallback the rung-3 case takes above. Consumes no randomness, so
            // the pre-BL-631 RNG stream on this path is untouched.
            corp_ownership[static_cast<std::size_t>(c)] = ownership_from_character(
                (it != w.nations.end()) ? it->second.politics : ideology::isolationist);
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

    // BL-1188 (seat kit runs) — CHAIN-FEASIBLE (Pass 3). With a registry, every
    // placement's processors are given a recipe whose inputs are produced within
    // reach, or are unplaced, and a rung left with nothing tries the next rung.
    // Without one (world generation's own call, which runs before any registry
    // exists; a budget world's walk replaces that roster), processors keep
    // `no_recipe` exactly as before.
    std::unique_ptr<chain_reach> chain;
    if (reg != nullptr)
        chain = std::make_unique<chain_reach>(make_chain_reach(w, *reg));
    const auto chain_kept = [&](std::vector<entity_id>& a) {
        if (a.empty())
            return false;
        return chain == nullptr || make_chain_feasible(w, *reg, *chain, a, occupied_tiles, nullptr);
    };

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
                if (chain_kept(assets))
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
            chain_kept(assets);
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
    std::vector<int> corp_slot; // the slot c each created corporation came from
    corp_slot.reserve(static_cast<std::size_t>(corp_count));

    for (int c = 0; c < corp_count; ++c)
    {
        // BL-1185 review: with a registry (the chain-feasible path) a slot whose
        // every rung was refused is NOT a corporation — no holdless specialist is
        // created, so none can be seated. Its stockpile draw is still taken, so
        // every later slot draws exactly what it would have.
        if (reg != nullptr && corp_assets[static_cast<std::size_t>(c)].empty())
        {
            (void)generate_starting_stockpile(corp_focuses[static_cast<std::size_t>(c)],
                                              corp_capitals[static_cast<std::size_t>(c)],
                                              params.base_capital, stock_rng);
            continue;
        }
        corp_slot.push_back(c);
        corporation_component cc;
        cc.name             = std::move(corp_names[static_cast<std::size_t>(c)]);
        cc.home_nation      = nation_ids[static_cast<std::size_t>(
                                  home_nation_idx[static_cast<std::size_t>(c)])];
        cc.focus            = corp_focuses[static_cast<std::size_t>(c)];
        cc.ownership_class  = corp_ownership[static_cast<std::size_t>(c)];
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
        // receive stock. BL-1265: held until the markets stand, then placed on
        // the shelves of the markets it sits in (`place_opening_stock`,
        // CORPORATION_GENERATION.md § Pass 4b).
        const auto stock = generate_starting_stockpile(
            corp_focuses[static_cast<std::size_t>(c)],
            corp_capitals[static_cast<std::size_t>(c)],
            params.base_capital, stock_rng);
        if (home_body != null_entity)
        {
            seed_opening_stock(w, corp_id, stock);
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
        // BL-1185 review: the pick is over the corporations that EXIST (every
        // slot without a registry, so that path draws exactly as before).
        const int created = static_cast<int>(corp_ids.size());
        if (created > 0)
        {
            std::uniform_int_distribution<int> pick_player(0, created - 1);
            const int player_idx = pick_player(player_rng);

            const entity_id player_corp_id = corp_ids[static_cast<std::size_t>(player_idx)];
            w.corporations[player_corp_id].is_player = true;
            w.player_entity = player_corp_id;

            // BL-305: which ledger row the player ends up as is only decided here,
            // after every row is already published — so it is a separate field
            // rather than a column on the row.
            if (progress)
                progress->player_slot.store(corp_slot[static_cast<std::size_t>(player_idx)],
                                            std::memory_order_relaxed);
        }
        else
        {
            // Every slot refused (only reachable with a registry): no seat. The
            // caller reports it (apply_landscape_candidate).
            w.player_entity = null_entity;
        }

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
    // BL-324's opening force, now OPT-IN (`corporation_params::seed_starting_force`,
    // default false from 2026-08-26). Gated rather than deleted: the seeding is
    // still the only thing that knows where a muster base belongs relative to an
    // HQ, and rival_military_seeding_harness still drives it by setting the flag.
    //
    // Skipping it consumes no randomness — seed_starting_military draws none, it
    // is a nearest-valid-tile search — so a world generated with the flag off is
    // identical to one generated with it on minus the bases and units, and every
    // downstream RNG stream is untouched.
    //
    // BL-1154 (Ben, 2026-10-01, NR-963 A; MILITARY.md § "BL-476 rivals start
    // armed"): RIVALS ARE ALWAYS ARMED; the flag now governs the SEAT only. The
    // player keeps opening unarmed (BL-635's cause stays off for the player);
    // `seed_starting_force` still arms it too, for
    // rival_military_seeding_harness. Order unchanged: the player (when armed)
    // first, then the rivals in corp_ids order.
    {
        std::unordered_set<entity_id> military_occupied_tiles;
        military_occupied_tiles.reserve(w.buildings.size());
        for (const auto& [bld_id, bc] : w.buildings)
            military_occupied_tiles.insert(bc.tile);

        if (params.seed_starting_force && w.corporations.count(w.player_entity) != 0)
            seed_starting_military(w, w.player_entity, military_occupied_tiles);

        for (entity_id corp_id : corp_ids)
            if (corp_id != w.player_entity)
                seed_starting_military(w, corp_id, military_occupied_tiles);
    }

    if (chain != nullptr)
        invalidate_logistics_caches(w); // the reach legs warmed them (chain_reach)
    return corp_ids;
}

// ---------------------------------------------------------------------------
// BL-977 — remove_specialist_roster
// ---------------------------------------------------------------------------

int remove_specialist_roster(world& w)
{
    std::vector<entity_id> gone;
    for (const auto& kv : w.corporations)
        if (!kv.second.is_background)
            gone.push_back(kv.first);
    std::sort(gone.begin(), gone.end());
    if (gone.empty())
        return 0;

    for (const entity_id cid : gone)
    {
        const corporation_component& cc = w.corporations.at(cid);
        for (const entity_id bid : cc.assets)
        {
            w.buildings.erase(bid);
            w.stockpiles.erase(bid);
        }
        if (cc.hq_building != null_entity)
        {
            w.buildings.erase(cc.hq_building);   // always among the assets; stated anyway
            w.stockpiles.erase(cc.hq_building);
        }

        // BL-1265: its opening stock goes with it — taken back off the shelves
        // if it was already placed, then erased with the corporation.
        unplace_opening_stock(w, cid);
        w.gen_opening_stock.erase(cid);

        // Units are keyed by their own id; collect then erase so the map is not
        // mutated under its iterator. Order-insensitive: every erase is by key.
        std::vector<entity_id> owned;
        for (const auto& kv : w.units)
            if (kv.second.owner == cid)
                owned.push_back(kv.first);
        for (const entity_id uid : owned)
            w.units.erase(uid);

        w.earned_techs.erase(cid);
        w.corp_modifiers.erase(cid);
        w.corp_embargo_conditions.erase(cid);

        if (w.player_entity == cid)
            w.player_entity = null_entity;
        w.corporations.erase(cid);
    }

    // A specialist port or inland hub was a supply anchor; the reach field that
    // still counts it would let a candidate score ground nobody can now reach.
    invalidate_logistics_caches(w);
    return static_cast<int>(gone.size());
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

    // THERE IS NO PRODUCTION-TO-DEMAND TARGET (CORPORATION_GENERATION.md § Pass 6;
    // Ben's ruling, 2026-08-26). This loop once stopped on a 0.90 basket-weighted
    // production/demand ratio inherited from the deleted BL-078 substrate model
    // (economy.substrate.clearing_fraction). Measured, it never bound on any
    // generated world: the per-resource cap below binds on every demanded
    // resource, so the caps are the design and the ratio is gone. The ratio is
    // still READABLE — measure_production_ratio, the seam at the end of this
    // file — it is just not a stop.
    // TWO-LEVEL FIRM BUDGET (Ben, 2026-08-20: "we should have two levels, per
    // resource caps, and per province caps").
    //
    // What this replaces, and why. max_firms_per_body was 40, and its own comment
    // called it a "hard bound - never infinite-loop": a safety valve, never a
    // design target. province_capacity_probe measured what it was actually doing.
    // The cap BINDS on 5 of 8 seeds, and on 3 of those the body stops far below
    // its own coverage target - 0.271, 0.631 and 0.637 against a target of 0.900.
    // A market that opens with 27% of demand covered is the thin opening market,
    // and a safety valve was causing it.
    //
    // So the flat body cap stops being the shaping constraint and becomes what it
    // always claimed to be - an anti-runaway bound set far above any real world.
    // Shaping moves to two caps that each say something:
    //
    //   PER RESOURCE - no single resource may absorb the whole budget. Without it
    //   one huge absolute gap (biggest_gap_resource picks by absolute shortfall)
    //   can take every firm, leaving a body that makes one good in quantity and
    //   nothing else.
    //
    //   PER PROVINCE - no single province may absorb the whole budget. Without it
    //   firms pile into whichever province holds the best anchor tiles, and the
    //   density arrives as one blot instead of as industry spread over the map.
    //   The province is the right grain because it is already the partition the
    //   world is carved into (BL-466).
    //
    // BOTH NUMBERS ARE PROVISIONAL AND UNPINNED - a first cut chosen to be
    // measured, not derived. Shipping an unpinned number quietly is exactly what
    // BL-463's report-then-tune discipline warns against, so they are called out
    // here and in the review log, and they belong in economy.lua as tunables once
    // the shape is agreed.
    constexpr int   max_firms_per_body      = 200;  // anti-runaway only
    constexpr int   per_resource_firm_cap   = 8;    // provisional - measure, then pin
    // A FLAT 2, AND IT STAYS FLAT (BL-1146 review, 2026-09-27): the cap that
    // scales with the province's centre (NR-960, `province_firm_cap`) is a
    // BUDGET-WORLD ruling (INDUSTRIALISATION.md § 1). This pass runs on every
    // world without a budget, whose bytes are BL-1031's pinned contract.
    constexpr int   per_province_firm_cap   = 2;    // provisional - measure, then pin
    constexpr int   max_iterations_per_body = 3 * max_firms_per_body; // slack for placement misses

    // Distinct xor-offset seeds, independent of generate_corporations' own
    // streams (seed_asset etc.) so the two passes cannot collide even though
    // both draw from placement RNG.
    std::mt19937 asset_rng(seed ^ 0x3D6F9A11u);
    std::mt19937 name_rng(seed ^ 0x6E17C4B0u);
    std::mt19937 stock_rng(seed ^ 0x1A2B3C4Du);

    // BL-1185: the reach every firm's processors are checked against (Pass 3
    // "Chain-feasible"), its node set read once, here.
    chain_reach chain = make_chain_reach(w, reg);

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

        // BL-708: the CONSUMER half, fixed for the body — households and the
        // background stopgap, both weighted by population scale, neither of
        // which this loop can move. The INDUSTRIAL half is re-measured inside
        // the loop, because placing a firm creates its own upkeep draw.
        const std::array<float, resource_count> consumer_demand = body_demand(w, reg, body_id);

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
        // The two shaping tallies, per body. Both are keyed by a stable id and
        // are read and written in the loop's own deterministic order, so neither
        // introduces an iteration-order dependence.
        std::array<int, resource_count> firms_by_resource = {};
        std::map<uint32_t, int>         firms_by_province;
        // BL-1185: per good, the chain-rejected placements since the last firm
        // landed, and whether the good is masked out of the selection for it.
        std::array<int, resource_count>  chain_misses = {};
        std::array<bool, resource_count> chain_masked = {};
        // BL-1197: per good, the placements that found no ground to dig it
        // since a firm for it last landed, and whether that masks it.
        std::array<int, resource_count>  dig_misses = {};
        std::array<bool, resource_count> dig_masked = {};
        // BL-1233 ruling A: the prospective draws of firms the sized rule refused.
        prospective_draws prospective;
        for (int iter = 0; iter < max_iterations_per_body && firms_this_body < max_firms_per_body; ++iter)
        {
            std::array<float, resource_count> production = {};
            accumulate_body_production(w, reg, body_id, production);

            // BL-708 — RE-MEASURED EACH ITERATION, and that is the whole point.
            // Every firm this loop places adds its own upkeep draw to the body,
            // so provisioning it is a moving target: place ten mines and the
            // body now wants power it did not want a moment ago. Capturing the
            // draw once, before any firm existed, would size generation against
            // a demand that no longer applies by the time the loop finishes.
            //
            // This is a genuine feedback loop and it is self-limiting: the
            // upkeep of the plants themselves is counted too, so it converges
            // rather than chasing its own tail — a plant's draw is a fraction of
            // its output. Under an all-zero upkeep table it adds exactly zero and
            // every pre-BL-708 world generates byte-identically.
            std::array<float, resource_count> demand = consumer_demand;
            const std::array<float, resource_count> upkeep =
                body_upkeep_demand(w, reg, body_id);
            for (std::size_t r = 0; r < resource_count; ++r)
                demand[r] += upkeep[r];

            // BL-709 — the CONSTRUCTION half, re-measured on the same schedule
            // and for the same reason: every firm this loop places raises the
            // body's construction demand, because a bigger world builds more.
            // Self-limiting in the same way, since a yard is itself a building
            // and so counts toward the target it helps fill.
            const std::array<float, resource_count> construction_need =
                body_construction_demand(w, reg, body_id);
            for (std::size_t r = 0; r < resource_count; ++r)
                demand[r] += construction_need[r];

            // BL-1197 round 4 — DERIVED DEMAND, re-measured on the same
            // schedule: every processor standing (and every one this loop has
            // placed) wants its recipe's inputs (`body_processor_input_demand`).
            const std::array<float, resource_count> input_need =
                body_processor_input_demand(w, reg, body_id);
            for (std::size_t r = 0; r < resource_count; ++r)
                demand[r] += input_need[r];
            // BL-1233 ruling A: and the draw of each firm the sized rule refused,
            // while its good is still wanted.
            {
                std::array<bool, resource_count> capped{};
                for (std::size_t r = 0; r < resource_count; ++r)
                    capped[r] = firms_by_resource[r] >= per_resource_firm_cap;
                add_prospective_draws(w, reg, chain, prospective, /*centre_market=*/null_entity,
                                      demand, production, capped);
            }

            // PER-RESOURCE CAP. Mask out every resource that has already taken
            // its share of this body's firms, then ask for the biggest remaining
            // gap. Masking by lifting the resource's apparent production to its
            // demand makes it read as 'met' to biggest_gap_resource - the same
            // language that function already speaks, so there is no second
            // selection rule to drift out of step with the first.
            std::array<float, resource_count> selectable = production;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (firms_by_resource[r] >= per_resource_firm_cap || chain_masked[r]
                    || dig_masked[r])
                    selectable[r] = std::max(selectable[r], demand[r]);

            // BL-709 — THE CONSTRUCTION SECTOR IS PROVISIONED FIRST, and this
            // is deliberately a SECOND selection rule rather than a weight
            // inside the first. It is worth saying why, because this loop's own
            // comment argues against exactly that.
            //
            // `biggest_gap_resource` ranks on ABSOLUTE shortfall. Measured, that
            // rule cannot reach construction capacity on the ancient band at any
            // honest target: the band's household gaps run to ~350 a tick, so a
            // capacity target sized to what builds ACTUALLY consume (~40) never
            // wins the argmax, and a target large enough to win would be a
            // ten-fold over-provision that floods the band with yards eating the
            // timber and planks everything else wants. Raising it to 0.30 per
            // building was tried and measured: ancient capacity production
            // stayed at 0.0 while industrial operating firms fell 72 -> 9.
            //
            // The two questions are genuinely different, which is what makes two
            // rules right here rather than a fudge. "Fill the biggest gap" is a
            // question about a body's TRADEABLE OUTPUT. "Does this body have a
            // construction sector at all" is a question about INFRASTRUCTURE —
            // the same distinction BL-708 drew when it said generation
            // provisions the utility and the market decides everything
            // downstream of it. A world with no yard cannot build, at any price,
            // because capacity is not cargo and cannot be imported.
            //
            // BOUNDED BY THE SAME CAPS as every other resource: it stops at the
            // measured target (`body_construction_demand`) and at
            // `per_resource_firm_cap`, so it can neither run away nor starve the
            // gap fill of firm slots. When it is satisfied — or when the band
            // authors no construction method at all — the loop falls through to
            // the ordinary rule unchanged.
            std::size_t gap_r    = resource_count;
            int         recipe_i = -1;
            {
                const std::size_t cap_i =
                    static_cast<std::size_t>(resource_type::construction_capacity);
                const int ci = best_construction_recipe(reg);
                if (ci >= 0 && firms_by_resource[cap_i] < per_resource_firm_cap
                    && !chain_masked[cap_i])
                {
                    // BOUNDED BY A COUNT OF YARDS, NOT BY MEASURED PRODUCTION,
                    // and that is the difference between a provisioning pass and
                    // a runaway. Measured: bounding it by "until production meets
                    // demand" burned the whole per-resource firm cap on the
                    // ancient band, because the yards it placed could not run at
                    // all — `clay` is produced 0.0 in that band and `planks` are
                    // thin, the same "the ancient chain does not convert" defect
                    // MARKETS.md § Three properties records — so production never
                    // rose, the condition never cleared, and ~35 dead firms cost
                    // the band 38 operating buildings.
                    //
                    // A COUNT cannot run away. One yard's batch output divides
                    // the body's target, so a body wants a fixed handful and gets
                    // exactly that many whether they thrive or starve; if they
                    // starve, the market kills them and the loss is bounded at
                    // four or five firms instead of a cap's worth.
                    //
                    // Deliberately ignores the workforce and richness scalars
                    // that decide a yard's ACTUAL batch count, so it is an
                    // order-of-magnitude bound rather than a solve. That is the
                    // right precision for a provisioning target: generation puts
                    // a plausible number of yards on the ground, and the market
                    // decides which of them survive.
                    const float per_yard =
                        reg.recipe_at(building_type::processing_facility, ci).outputs[cap_i];
                    const int want_yards =
                        (per_yard > 0.0f)
                            ? static_cast<int>(std::ceil(demand[cap_i] / per_yard))
                            : 0;
                    // BOTH BOUNDS, and each catches what the other misses. The
                    // COUNT stops the ancient runaway (yards that cannot run
                    // never raise production, so a production-only test never
                    // clears); the MEASURED SHORTFALL stops the industrial
                    // over-provision (yards that CAN run cover the body long
                    // before the count is exhausted, and placing the rest would
                    // spend firm slots on capacity nobody needs).
                    if (firms_by_resource[cap_i] < want_yards
                        && demand[cap_i] > selectable[cap_i])
                    {
                        gap_r    = cap_i;
                        recipe_i = ci;
                    }
                }
            }

            if (gap_r == resource_count)
            {
                gap_r = biggest_gap_resource(selectable, demand);
                if (gap_r == resource_count)
                    break; // no resource genuinely short — nothing left worth filling

                // Prefer processing when some recipe's output actually relieves the
                // gap resource (a refined good — silicon, machinery, ...);
                // otherwise the firm extracts the gap resource as a raw directly.
                recipe_i = best_recipe_for_gaps(reg, selectable, demand);
            }

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

            // BL-1197 (gap firm digs the gap): a water-gap firm digs water at a
            // Well site, else an ice deposit; a produce-gap firm (BL-1208) a
            // produce deposit, else a Wharf site — and never anything else. A
            // nation with no such ground left places nothing for it: the miss is
            // the GOOD's, so it counts toward that good's mask — once it has
            // missed as many times as the body has nations (the cursor takes
            // them in turn) it is masked for the pass, and the next gap is taken.
            resource_type dig_r = resource_type::water;
            const bool    digs  = gap_firm_digs(gap_r, go_processing, dig_r);
            std::vector<entity_id> assets = digs
                ? place_digging_assets(w, nit->second, focus, occupied_tiles, asset_rng,
                                       nullptr, dig_r)
                : place_starting_assets(w, nit->second, focus, occupied_tiles, asset_rng);
            if (assets.empty())
            {
                if (digs && ++dig_misses[gap_r] >= static_cast<int>(nation_ids.size()))
                    dig_masked[gap_r] = true;
                continue; // this nation had nothing left to anchor on this round; try the next
            }

            // PER-PROVINCE CAP. The firm's province is the one its FIRST asset
            // stands in - place_starting_assets clusters a corp's holdings around
            // a single anchor, so the first asset names the operation's home and
            // the rest sit with it. A province already at its cap gives the firm
            // back and takes the next nation in the cursor, rather than breaking:
            // another nation's turn may anchor somewhere with room.
            uint32_t anchor_province = 0;
            if (!assets.empty())
            {
                const auto abit = w.buildings.find(assets.front());
                if (abit != w.buildings.end())
                    anchor_province = w.provinces.province_of(abit->second.tile);
            }
            if (anchor_province != 0
                && firms_by_province[anchor_province] >= per_province_firm_cap)
            {
                // Hand the tiles back so a later, better-placed firm can use them -
                // otherwise a refused placement silently sterilises good ground.
                for (const entity_id bid : assets)
                {
                    const auto bit = w.buildings.find(bid);
                    if (bit != w.buildings.end())
                        occupied_tiles.erase(bit->second.tile);
                    w.buildings.erase(bid);
                    // author_building writes BOTH w.buildings and w.stockpiles;
                    // erasing only the first orphans a stockpile keyed to a
                    // building that no longer exists - the pool-leak shape
                    // BL-482 already tracks. Undo the whole authoring.
                    w.stockpiles.erase(bid);
                }
                continue;
            }

            // Author a recipe onto every processing facility placed, targeted at
            // the gap this firm exists to fill — generation-time processors
            // otherwise stay `no_recipe` until app::load_economy's blanket
            // steel default, which has already run by the time this function is
            // called (see the header's ordering note), so a gap-targeted recipe
            // here is strictly better than losing every background processor to
            // the same steel default.
            //
            // BL-1185 (chain-feasible placement; Pass 3 "Chain-feasible"): the
            // recipe must also have every input produced within reach of the
            // processor's market — the gap selection's recipe first, then the
            // good's other recipes by output; a processor with none is unplaced,
            // and a firm none of whose processors can make its good is given back
            // whole. An extraction firm's incidental processor takes the nearest
            // feasible recipe or is unplaced (`make_chain_feasible`).
            refused_draw refused; // BL-1233: read on failure AND on landing (below)
            {
                std::vector<int> serve;
                if (go_processing)
                {
                    serve.push_back(recipe_i);
                    for (const int i : chain_recipes_for_good(reg, gap_r))
                        if (i != recipe_i)
                            serve.push_back(i);
                }
                // BL-1217 D6: this firm's own measurement (prospective draws
                // included). POWER BODY-WIDE HERE, deliberately: Pass 6 selects
                // power on body-wide demand and sites its anchor with no grid
                // cut, so a per-grid bound would refuse a power firm the path
                // itself chose and mask it as chain-infeasible. The bound reads
                // the gap the path read. (The walk sizes AND sites per grid, so
                // it reads per grid.)
                output_want firm_want;
                firm_want.body          = body_id;
                firm_want.want          = demand;
                firm_want.production    = production;
                firm_want.power_by_grid = false;
                if (!make_chain_feasible(w, reg, chain, assets, occupied_tiles,
                                         go_processing ? &serve : nullptr, /*whole=*/true,
                                         &refused, &firm_want))
                {
                    if (refused.set)
                        record_refused_draw(prospective, gap_r, refused);
                    // Given back whole. Another nation's turn may anchor in reach,
                    // so the good is masked once it has missed as many times as
                    // the body has nations (the cursor takes them in turn). The
                    // mask is STICKY for the pass: only a firm that produces one
                    // of the good's inputs can bring it within reach, so only
                    // such a landing clears it (below) — an unrelated landing
                    // does not, and an input-less good never burns the pass's
                    // iterations again.
                    if (++chain_misses[gap_r] >= static_cast<int>(nation_ids.size()))
                        chain_masked[gap_r] = true;
                    continue;
                }
                // The anchor survived (an unplaced anchor rejects the firm), so
                // `anchor_province`, the one the cap test above read, stands.
            }

            // Financial profile: opened at zero here and handed its working
            // capital (BL-1173, background_working_capital) once its opening
            // stock is generated below, since the capital is priced from it.
            corporation_component cc;
            cc.name          = make_corp_name(nit->second.name, name_rng);
            cc.home_nation   = home_nid;
            cc.focus         = focus;
            // BL-678 (companies are open) — A COMPANY IS `publicly_held`,
            // ALWAYS. Ownership class keeps its whole Pass 2b derivation for
            // CORPORATIONS; a company is part of the commercial population the
            // player trades with, not a rival whose books are its own, so it
            // files its return and it can be bought.
            //
            // THE OVERRIDE IS APPLIED AFTER THE DERIVATION, NOT INSIDE IT, and
            // that shape is the point rather than a stylistic choice.
            // `ownership_from_character` is pure — it reads one ideology, holds
            // no state and draws nothing — so evaluating it and then discarding
            // its result consumes exactly the randomness the pre-BL-678 path
            // consumed: none. The RNG stream through this loop is untouched and
            // no downstream generation shifts; only the stored value changes.
            // The derived class is kept named because it is still the honest
            // answer to "what would a CORPORATION seated in this nation carry",
            // which is the question Pass 2b answers and this override does not
            // retire.
            [[maybe_unused]] const ownership_class corporation_class =
                ownership_from_character(nit->second.politics);
            cc.ownership_class = ownership_class::publicly_held;
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
            ++firms_by_resource[gap_r];
            dig_misses[gap_r] = 0; // BL-1197: a firm for the good landed
            if (anchor_province != 0)
                ++firms_by_province[anchor_province];
            // BL-1185: a firm FOR good g just landed, so g is feasible from some
            // nation — its misses no longer stand.
            chain_misses[gap_r] = 0;
            chain_masked[gap_r] = false;
            prospective.erase(gap_r); // BL-1233: its processor is placed
            // BL-1233 (BL-1217 D6 review): a later works the SIZED rule refused
            // leaves its draw though the firm stands.
            if (refused.set)
                record_refused_draw(prospective, gap_r, refused);
            // A landing that produces an input of a missed good may bring it
            // within reach — clear that good's count and mask, and only that.
            for (std::size_t g = 0; g < resource_count; ++g)
                if (chain_misses[g] > 0
                    && chain_firm_feeds_good(w, reg, w.corporations.at(corp_id).assets, g))
                {
                    chain_misses[g] = 0;
                    chain_masked[g] = false;
                }

            // Starting stockpile — the same BL-116 generator every generated
            // corp uses, so a background firm opens with materials from turn
            // one exactly like a rival does.
            if (home_body != null_entity)
            {
                const auto stock = generate_starting_stockpile(
                    focus, /*capital=*/0.0f, /*base_capital=*/0.0f, stock_rng);
                // The market its stock is priced at for working capital: its
                // home market there, else (none carved yet) the body itself.
                const entity_id home_mkt = corp_home_market(w, corp_id, home_body);
                const entity_id pool_key = (home_mkt != null_entity) ? home_mkt : home_body;
                seed_opening_stock(w, corp_id, stock); // BL-1265: placed on the shelves later

                // BL-1173: the firm opens with working capital priced from the
                // stock it was just handed (background_working_capital, above).
                // Set after the stock so the stock stream is drawn exactly as
                // before; no RNG is read here.
                corporation_component& opened = w.corporations.at(corp_id);
                opened.starting_capital = background_working_capital(w, pool_key, stock);
                opened.balance          = opened.starting_capital;
            }
        }
    }

    invalidate_logistics_caches(w); // the reach legs warmed them (chain_reach)
    return firm_ids;
}

namespace {

struct unplace_tally
{
    int unplaced = 0; ///< processors removed
    int holdless = 0; ///< corporations left with no seatable holding
    int player_holdless = 0; ///< of which the player (`is_player`)
};

/// Unplace @p gone's processors (corp -> buildings) at world build and re-seat
/// each touched corporation: the building and its stockpile go
/// (`chain_unplace`, @p occupied kept in step), it leaves the corporation's
/// `assets`, the HQ is re-designated over the NON-MILITARY holdings left
/// (BL-1154's muster base is not a seat) — so `hq_building` never names a
/// removed building — and the corporation's opening pools are re-keyed to that
/// HQ, the held opening stock with them (`rehome_opening_pools`, this corp
/// only). WORLD BUILD ONLY, before any tick. Ascending corp id (std::map).
/// Shared by the roster's chain enforcement and the default-recipe pass.
unplace_tally unplace_and_reseat(world& w, const std::map<entity_id, std::vector<entity_id>>& gone,
                                 std::unordered_set<entity_id>& occupied)
{
    unplace_tally out;
    for (const auto& [cid, bids] : gone)
    {
        corporation_component& corp = w.corporations.at(cid);
        for (const entity_id bid : bids)
        {
            chain_unplace(w, bid, occupied);
            corp.assets.erase(std::remove(corp.assets.begin(), corp.assets.end(), bid),
                              corp.assets.end());
            ++out.unplaced;
        }
        std::vector<entity_id> seatable;
        for (const entity_id bid : corp.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit != w.buildings.end() && bit->second.type != building_type::military_base)
                seatable.push_back(bid);
        }
        const entity_id home_body = corp_home_body(w, seatable);
        const hq_designation hq   = designate_hq(w, seatable, home_body);
        corp.hq_building     = hq.building;
        corp.influence_range = hq.range;
        if (seatable.empty())
        {
            ++out.holdless;
            if (corp.is_player)
                ++out.player_holdless;
        }
        // BL-1265: its opening stock follows its holdings — taken back off the
        // shelves it was placed on, held until the next `place_opening_stock`
        // places it by the buildings it keeps.
        unplace_opening_stock(w, cid);
    }
    return out;
}

} // namespace

chain_roster_enforcement enforce_chain_feasible_roster(world& w, const recipe_registry& reg,
                                                       std::uint32_t seed)
{
    chain_roster_enforcement out;
    std::vector<entity_id> ids;
    for (const auto& [cid, corp] : w.corporations)
        if (!corp.is_background)
            ids.push_back(cid);
    std::sort(ids.begin(), ids.end());
    std::unordered_set<entity_id> occupied;
    occupied.reserve(w.buildings.size() * 2);
    for (const auto& kv : w.buildings)
        occupied.insert(kv.second.tile);
    chain_reach cr = make_chain_reach(w, reg);
    // The roster's processors, in (corp id, asset order) — the order every
    // step below walks.
    std::vector<std::pair<entity_id, entity_id>> procs; // (corp, building)
    for (const entity_id cid : ids)
        for (const entity_id bid : w.corporations.at(cid).assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit != w.buildings.end() && bit->second.type == building_type::processing_facility
                && !bit->second.decommissioned)
                procs.emplace_back(cid, bid);
        }

    // 1. KEEP WHAT ALREADY RUNS — the greatest set of processors whose CURRENT
    //    recipes are feasible against producers that are themselves kept (or
    //    are not the roster's: base installations, background firms). A
    //    processor whose recipe is infeasible is suspended (no recipe, so it
    //    supplies nobody) and the test is repeated until nothing more falls:
    //    a fixed point, independent of the order the roster is walked in,
    //    because suspending a processor can only take producers away.
    //
    //    BL-1233 (sized to its inputs): a kept recipe must also find its inputs'
    //    SPARE reachable supply covering its draw. Suspending a consumer frees
    //    supply as well as taking a producer away, so a sweep is no longer
    //    monotone in walk order: each sweep tests every kept processor against
    //    the SAME standing set and suspends the failures together — the walk
    //    order still decides nothing.
    //
    // 2. DECIDE THE SUSPENDED AS FRESH PLACEMENT DOES, in (corp id, asset
    //    order): the feasible recipe nearest its feed (own, market, reach)
    //    against everything standing with a recipe now. Suspending together can
    //    cascade (a plant suspended only because its feed was), so the
    //    suspended are swept REPEATEDLY until a sweep admits none: a feed
    //    re-admitted in one sweep lets its consumer in on the next — inputs
    //    before their consumers. What no sweep admits is unplaced.
    //
    //    A decision is sized against the spare left after every standing draw,
    //    but an admission ADDS a draw, which can take an earlier plant's margin.
    //    So steps 1 and 2 alternate until step 1 suspends nothing: every kept
    //    plant is then placeable against the final roster and nothing left
    //    suspended can be admitted, so a second call changes nothing. Each round
    //    is bounded; past the bound (never measured to bind) the last step 1's
    //    fixed point stands and whatever it suspended is unplaced.
    //
    // BL-1217 D6 (no processor beyond its output's want; `output_want`): both
    // steps read the one want test. Step 1 suspends only the EXCESS: the
    // feasible kept plants of one good on one body (of one power grid, where
    // power is sized per grid) are taken in ASCENDING BUILDING ID — the oldest
    // holding first — and each is kept while the good is still short before
    // it: want > the production of everything outside the group plus the
    // group plants already kept. The plants past that point are the excess and
    // fall together with the infeasible. So a covered chain stays intact (its
    // consumers keep their want, so its feeders keep theirs), and the order is
    // stated, not the walk's. A power plant on a grid that wants power but
    // needs under half a plant — the walk leaves such a grid to roads — is
    // still kept while it is that grid's FIRST generator: a sole feeder is not
    // excess. Step 2 admits only a recipe whose output is short, booking each
    // admission so the next decision sees it. The want is measured off the
    // world per body (`body_demand` as final demand: a roster has no walk's
    // capture), fresh for every sweep.
    enum : int { st_kept = 0, st_suspended = 1 };
    std::vector<int>  state(procs.size(), st_kept);
    std::vector<bool> ever_suspended(procs.size(), false);
    const auto body_of = [&](entity_id bid) {
        const auto tit = w.tiles.find(w.buildings.at(bid).tile);
        return tit == w.tiles.end() ? null_entity : tit->second.body;
    };
    const auto measure_bodies = [&]() {
        std::map<entity_id, output_want> by_body;
        for (const auto& pr : procs)
        {
            const entity_id body = body_of(pr.second);
            if (body != null_entity && by_body.find(body) == by_body.end())
                by_body.emplace(body, measure_output_want(w, reg, body, body_demand(w, reg, body)));
        }
        return by_body;
    };
    const std::size_t power_r = static_cast<std::size_t>(resource_type::power);
    const float       batches_n = nominal_processing_batches(reg);
    const float       plant_out = one_power_plant_output(reg);
    // The excess among @p feasible (indices into procs), per the rule above.
    const auto excess_of = [&](const std::vector<std::size_t>& feasible) {
        std::map<entity_id, output_want> wants = measure_bodies();
        std::map<entity_id, std::map<std::uint32_t, std::pair<float, float>>> grid_no;
        // Groups: (body, good, grid) -> processor indices; grid 0 off power.
        std::map<std::tuple<entity_id, std::size_t, std::uint32_t>, std::vector<std::size_t>> groups;
        for (const std::size_t i : feasible)
        {
            const entity_id bid  = procs[i].second;
            const entity_id body = body_of(bid);
            const auto wit = wants.find(body);
            if (wit == wants.end())
                continue;
            const recipe& rc = *reg.get_recipe(w.buildings.at(bid).recipe);
            const std::size_t p = static_cast<std::size_t>(primary_output_resource(rc));
            std::uint32_t g = 0;
            if (wit->second.power_by_grid && p == power_r)
            {
                g = tile_feed_power_grid(w, w.buildings.at(bid).tile);
                if (grid_no.find(body) == grid_no.end())
                {
                    std::set<std::uint32_t> ignored;
                    (void)body_power_grid_gap(w, reg, body, plant_out, ignored, nullptr, &grid_no[body]);
                }
            }
            groups[{body, p, g}].push_back(i);
        }
        std::vector<std::size_t> excess;
        for (auto& [key, members] : groups)
        {
            const auto [body, p, g] = key;
            std::sort(members.begin(), members.end(), [&](std::size_t a, std::size_t b) {
                return procs[a].second < procs[b].second;
            });
            const output_want& ow = wants.at(body);
            const auto own = [&](std::size_t i) {
                return batches_n * reg.get_recipe(w.buildings.at(procs[i].second).recipe)->outputs[p];
            };
            float group_out = 0.0f;
            for (const std::size_t i : members)
                group_out += own(i);
            const bool by_grid = ow.power_by_grid && p == power_r;
            float need = 0.0f, running = 0.0f;
            if (by_grid)
            {
                const auto& no = grid_no.at(body);
                const auto it  = (g != 0) ? no.find(g) : no.end();
                need    = (it != no.end()) ? it->second.first : 0.0f;
                running = (it != no.end()) ? it->second.second - group_out : 0.0f;
            }
            else
            {
                need    = ow.want[p];
                running = ow.production[p] - group_out;
            }
            for (const std::size_t i : members)
            {
                bool keep = need > running;
                if (by_grid && keep && need < 0.5f * plant_out && running > 0.0f)
                    keep = false; // a sub-half grid keeps its FIRST generator only
                if (keep)
                    running += own(i);
                else
                    excess.push_back(i);
            }
        }
        return excess;
    };
    const auto keep_sweep = [&]() {
        bool any = false;
        for (bool changed = true; changed;)
        {
            changed = false;
            chain_begin_decision(w, reg, cr);
            std::vector<std::size_t> failing, feasible_kept;
            for (std::size_t i = 0; i < procs.size(); ++i)
            {
                if (state[i] != st_kept)
                    continue;
                building_component& b = w.buildings.at(procs[i].second);
                const recipe* rc = reg.get_recipe(b.recipe);
                const bool feasible = rc != nullptr
                    && chain_recipe_placeable(w, reg, cr, procs[i].second,
                                              market_for_tile(w, b.tile), *rc, nullptr)
                           != chain_tier_none;
                if (!feasible)
                    failing.push_back(i);
                else
                    feasible_kept.push_back(i);
            }
            const auto suspend = [&](std::size_t i) {
                w.buildings.at(procs[i].second).recipe = no_recipe;
                state[i]          = st_suspended;
                ever_suspended[i] = true;
                changed = any     = true;
            };
            // The infeasible are suspended BEFORE the excess is read (BL-1217
            // D6 final review): a plant about to go must not count toward the
            // production (or a grid's output) that makes a feasible sibling
            // look like excess, nor toward the derived demand its inputs read.
            // Feasibility above was tested against one standing set for all.
            for (const std::size_t i : failing)
                suspend(i);
            for (const std::size_t i : excess_of(feasible_kept))
                suspend(i);
        }
        return any;
    };
    const int n = reg.recipe_count(building_type::processing_facility);
    const auto readmit = [&]() {
        for (bool admitted = true; admitted;)
        {
            admitted = false;
            std::map<entity_id, output_want> wants = measure_bodies(); // BL-1217 D6
            for (std::size_t i = 0; i < procs.size(); ++i)
            {
                if (state[i] != st_suspended)
                    continue;
                const entity_id cid = procs[i].first;
                const entity_id bid = procs[i].second;
                building_component& b = w.buildings.at(bid);
                const entity_id market = market_for_tile(w, b.tile);
                const std::vector<entity_id>& own = w.corporations.at(cid).assets;
                const auto wit = wants.find(body_of(bid));
                uint16_t chosen    = no_recipe;
                int      best_tier = chain_tier_none;
                chain_begin_decision(w, reg, cr); // BL-1233: spare over what stands now
                for (int k = 0; k < n && best_tier != chain_tier_own; ++k)
                {
                    const recipe& rc = reg.recipe_at(building_type::processing_facility, k);
                    if (wit != wants.end() && !wit->second.short_for(w, rc, b.tile))
                        continue; // BL-1217 D6: no maker for a good with no want left
                    const int t = chain_recipe_placeable(w, reg, cr, bid, market, rc, &own);
                    if (t < best_tier)
                    {
                        best_tier = t;
                        chosen    = reg.recipe_id(rc.name);
                    }
                }
                if (chosen != no_recipe)
                {
                    b.recipe = chosen;
                    state[i] = st_kept;
                    admitted = true;
                    if (wit != wants.end())
                        if (const recipe* took = reg.get_recipe(chosen))
                            wit->second.book(w, reg, *took);
                }
            }
        }
    };
    keep_sweep();
    for (std::size_t round = 0; round <= procs.size(); ++round)
    {
        readmit();
        if (!keep_sweep())
            break;
    }
    std::map<entity_id, std::vector<entity_id>> unplaced; // corp -> buildings
    for (std::size_t i = 0; i < procs.size(); ++i)
    {
        if (state[i] == st_suspended)
            unplaced[procs[i].first].push_back(procs[i].second);
        else if (ever_suspended[i])
            ++out.processors_redecided;
    }

    // 3. Unplace the processors no recipe could feed, and re-seat each touched
    //    corporation (`unplace_and_reseat`).
    const unplace_tally ut = unplace_and_reseat(w, unplaced, occupied);
    out.processors_unplaced += ut.unplaced;
    out.holdless            += ut.holdless;

    // 4. THE SEAT (the no-player path): if the seated specialist is now
    //    holdless, or a processing corporation left with no processor, it is
    //    drawn again over the specialists that still qualify, with the world-gen
    //    pick's own stream (seed ^ 0xF0E1D2C3). The app's player picks later.
    const auto qualifies = [&](entity_id cid) {
        const corporation_component& corp = w.corporations.at(cid);
        bool any = false, proc = false;
        for (const entity_id bid : corp.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end() || bit->second.type == building_type::military_base)
                continue;
            any = true;
            if (bit->second.type == building_type::processing_facility)
                proc = true;
        }
        return any && (corp.focus != industrial_focus::processing || proc);
    };
    const auto seat = w.corporations.find(w.player_entity);
    if (seat != w.corporations.end() && !seat->second.is_background && !qualifies(w.player_entity))
    {
        std::vector<entity_id> pool;
        for (const entity_id cid : ids)
            if (qualifies(cid))
                pool.push_back(cid);
        out.seat_redrawn = true;
        if (!pool.empty())
        {
            std::mt19937 player_rng(seed ^ 0xF0E1D2C3u);
            std::uniform_int_distribution<int> pick(0, static_cast<int>(pool.size()) - 1);
            repoint_player(w, pool[static_cast<std::size_t>(pick(player_rng))]);
        }
        else
        {
            for (auto& [cid, corp] : w.corporations)
                corp.is_player = false;
            w.player_entity = null_entity;
        }
    }
    out.seat = w.player_entity;

    invalidate_logistics_caches(w);
    return out;
}

chain_feasibility_audit audit_chain_feasibility(world& w, const recipe_registry& reg)
{
    chain_feasibility_audit out;
    std::unordered_set<entity_id> held;
    for (const auto& [cid, corp] : w.corporations)
    {
        (void)cid;
        for (const entity_id bid : corp.assets)
            held.insert(bid);
    }
    chain_reach cr = make_chain_reach(w, reg);
    // Ascending id: the leg memo fills in a fixed order (the counts are order-free).
    std::vector<entity_id> procs;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::processing_facility && !b.decommissioned)
            procs.push_back(bid);
    std::sort(procs.begin(), procs.end());
    for (const entity_id bid : procs)
    {
        const building_component& b = w.buildings.at(bid);
        const bool is_held = held.count(bid) != 0;
        ++out.processors;
        if (is_held)
            ++out.processors_held;
        const recipe* rc = reg.get_recipe(b.recipe);
        if (rc == nullptr)
        {
            ++out.no_recipe;
            continue;
        }
        if (chain_recipe_tier(w, reg, cr, bid, market_for_tile(w, b.tile), *rc, nullptr)
            == chain_tier_none)
        {
            ++out.infeasible;
            if (is_held)
                ++out.infeasible_held;
        }
    }
    return out;
}

void record_refused_draw(prospective_draws& book, std::size_t good, const refused_draw& refused)
{
    if (!refused.set || good >= resource_count)
        return;
    prospective_entry& e = book[good]; // SET, never added: a retry refused again counts once
    e.market = refused.market;
    e.draw   = refused.draw;
}

/// A positive in-reach want of r is a SHORTAGE OF ITS OWN (review round 3): the
/// refused plant can be fed only from within its reach, so a glut of r elsewhere
/// on the body does not answer it. r's demand is lifted to at least the body's
/// production of r before the want is added, so the selection reads r short by
/// at least the want whatever the body makes of it — and only a producer landing
/// in reach (which raises the plant's spare) shrinks the want.
static void raise_by_want(std::array<float, resource_count>& demand,
                          const std::array<float, resource_count>& production,
                          const std::array<float, resource_count>& wv)
{
    for (std::size_t r = 0; r < resource_count; ++r)
        if (wv[r] > 0.0f)
            demand[r] = std::max(demand[r], production[r]) + wv[r];
}

/// One entry's want of each input at a centre whose market is @p centre_market
/// (see `add_prospective_draws`); @p ir must already describe what stands.
static std::array<float, resource_count> prospective_want(world& w, const recipe_registry& reg,
                                                          input_reach& ir,
                                                          const prospective_entry& e,
                                                          entity_id centre_market)
{
    std::array<float, resource_count> wv{};
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (!(e.draw[r] > 0.0f))
            continue;
        if (centre_market != null_entity && centre_market != e.market
            && !market_within_reach(w, reg, ir, centre_market, e.market, r))
            continue;
        const float spare = reachable_supply(w, reg, ir, e.market, r, null_entity).spare;
        wv[r] = std::max(0.0f, e.draw[r] - std::max(0.0f, spare));
    }
    return wv;
}

void add_prospective_draws(world& w, const recipe_registry& reg, input_reach& ir,
                           prospective_draws& book, entity_id centre_market,
                           std::array<float, resource_count>& demand,
                           const std::array<float, resource_count>& production,
                           const std::array<bool, resource_count>& capped)
{
    if (book.empty())
        return;
    input_reach_refresh(w, reg, ir);
    // Each entry's want of each input: its draw less the spare reachable at its
    // plant's market, offered only at a centre that is or reaches that market.
    std::map<std::size_t, std::array<float, resource_count>> want;
    for (const auto& [good, e] : book)
        want.emplace(good, prospective_want(w, reg, ir, e, centre_market));
    std::array<float, resource_count> full = demand;
    for (const auto& [good, wv] : want)
        for (std::size_t r = 0; r < resource_count; ++r)
            full[r] += wv[r];
    for (auto it = book.begin(); it != book.end();)
    {
        const std::size_t good = it->first;
        if (capped[good] || !(full[good] > production[good]))
        {
            want.erase(good);
            it = book.erase(it);
        }
        else
            ++it;
    }
    std::array<float, resource_count> total{};
    for (const auto& [good, wv] : want)
        for (std::size_t r = 0; r < resource_count; ++r)
            total[r] += wv[r];
    raise_by_want(demand, production, total);
}

/// BL-1217 D6: `add_prospective_draws` READ ONLY — the same entries, the same
/// filter (capped, or not short even with the book's want), the same raise,
/// and the book left as it was. For a want test measured outside the turn's
/// own per-firm read (the walk's specialist), which must not prune the book
/// the turn reads next.
static void raise_by_prospective(world& w, const recipe_registry& reg, input_reach& ir,
                                 const prospective_draws& book, entity_id centre_market,
                                 std::array<float, resource_count>& demand,
                                 const std::array<float, resource_count>& production,
                                 const std::array<bool, resource_count>& capped)
{
    if (book.empty())
        return;
    input_reach_refresh(w, reg, ir);
    std::map<std::size_t, std::array<float, resource_count>> want;
    for (const auto& [good, e] : book)
        want.emplace(good, prospective_want(w, reg, ir, e, centre_market));
    std::array<float, resource_count> full = demand;
    for (const auto& [good, wv] : want)
        for (std::size_t r = 0; r < resource_count; ++r)
            full[r] += wv[r];
    std::array<float, resource_count> total{};
    for (const auto& [good, wv] : want)
    {
        if (capped[good] || !(full[good] > production[good]))
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            total[r] += wv[r];
    }
    raise_by_want(demand, production, total);
}

int assign_default_recipes(world& w, const recipe_registry& reg, const char* site)
{
    // BL-429: era-aware, so an ancient campaign does not default every processor
    // to an industrial recipe it could never run.
    const uint16_t default_recipe = reg.default_recipe_id();
    const recipe*  def            = reg.get_recipe(default_recipe);

    // BL-1217 D6: ascending building id — each assignment is booked before the
    // next is tested, so the order decides who keeps the default.
    std::vector<entity_id> ids;
    for (const auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            ids.push_back(id);
    if (ids.empty())
        return 0;
    std::sort(ids.begin(), ids.end());
    if (def == nullptr)
    {
        // No default in the band (an unloaded or empty registry): nothing to
        // test a want against, so the pre-ruling assignment stands.
        for (const entity_id id : ids)
            w.buildings.at(id).recipe = reg.default_recipe_id_at(w, w.buildings.at(id).tile);
        return 0;
    }

    std::map<entity_id, entity_id> owner;
    for (const auto& [cid, cc] : w.corporations)
        for (const entity_id a : cc.assets)
            owner.emplace(a, cid);

    std::map<entity_id, output_want>             wants;    // per body, measured lazily
    std::map<entity_id, std::vector<entity_id>> gone;     // corp -> unplaced
    std::vector<entity_id>                      ownerless;
    int given = 0;
    for (const entity_id id : ids)
    {
        building_component& b = w.buildings.at(id);
        const auto tit = w.tiles.find(b.tile);
        // The body's air (Ben, 2026-10-09; PRODUCTION.md § Chemical Plant): the
        // default is the first recipe this tile's body can run -- carry this
        // call across any rewrite of this function.
        const uint16_t rid  = reg.default_recipe_id_at(w, b.tile);
        const recipe*  rdef = reg.get_recipe(rid);
        // THE PRE-AUTHORED INSTALLATION KEEPS ITS DEFAULT (Ben, 2026-10-09;
        // CORPORATION_GENERATION.md § Pass 6, the exceptions): it is the one
        // steel maker the chain-feasibility test can see at the start, and
        // unplacing it left no steel chain on any seed. Exactly the id
        // `make_hard_coded_world` recorded when it authored it
        // (`world::authored_processor`) is given the default whatever the want,
        // booked like any other so the processors after it read its output.
        const bool authored = (id == w.authored_processor);
        // Every other recipe-less processor is a corporation's: the exemption
        // once keyed on "no owner", and this holds that it was the same set.
        assert(authored || owner.find(id) != owner.end());
        bool keep = (rdef != nullptr);
        if (keep && authored)
        {
            b.recipe = rid;
            if (tit != w.tiles.end())
            {
                const entity_id body = tit->second.body;
                auto wit = wants.find(body);
                if (wit == wants.end())
                    wit = wants.emplace(body, measure_output_want(w, reg, body, body_demand(w, reg, body))).first;
                wit->second.book(w, reg, *rdef);
            }
        }
        else if (keep && tit != w.tiles.end())
        {
            const entity_id body = tit->second.body;
            auto wit = wants.find(body);
            if (wit == wants.end())
                wit = wants.emplace(body, measure_output_want(w, reg, body, body_demand(w, reg, body))).first;
            keep = wit->second.short_for(w, *rdef, b.tile);
            if (keep)
            {
                b.recipe = rid;
                wit->second.book(w, reg, *rdef);
            }
        }
        else if (keep)
            b.recipe = rid; // no body to measure: the pre-ruling default
        if (keep)
        {
            ++given;
            continue;
        }
        if (const auto oit = owner.find(id); oit != owner.end())
            gone[oit->second].push_back(id);
        else
            ownerless.push_back(id); // release builds: the assert above did not hold
    }

    std::unordered_set<entity_id> occupied; // generation's occupancy sets are local to their passes
    const unplace_tally ut = unplace_and_reseat(w, gone, occupied);
    for (const entity_id id : ownerless)
        chain_unplace(w, id, occupied);
    const int removed = ut.unplaced + static_cast<int>(ownerless.size());
    if (removed > 0 && site != nullptr)
        std::printf("[assign_default_recipes] %s: %d processors unplaced (default output unwanted; "
                    "%d corps left holdless, the player %s), %d given the default\n",
                    site, removed, ut.holdless, ut.player_holdless > 0 ? "AMONG THEM" : "not", given);
    return removed;
}

// ---------------------------------------------------------------------------
// Measurement seam (2026-08-20)
// ---------------------------------------------------------------------------
// `generate_background_firms` stops on its caps — per resource, per province and
// the anti-runaway `max_firms_per_body` — never on a coverage target (the 0.90
// ratio it once tested never bound; CORPORATION_GENERATION.md § Pass 6). The
// basket-weighted production/demand ratio is still the reading behind "do
// markets open with the goods they need", and until now nothing outside this
// file could ask it: the three helpers are file-private.
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

// ---------------------------------------------------------------------------
// BL-1032 — charter_web_from_budget
// ---------------------------------------------------------------------------
// The budget path. NEW CODE beside `generate_corporations` and
// `generate_background_firms`, never a refactor of either: both bodies are
// untouched, because their bytes are the contract a world with no budget keeps
// (BL-1031's pins) and a shared refactor is exactly how those bytes would move —
// a refused firm there consumes entity ids and advances the nation cursor, and a
// helper that "tidied" that would change every legacy world. So the pieces this
// path needs are COPIED where they are inline in a legacy body (the gap
// selection, the firm assembly) and CALLED where they are already file-local
// helpers (placement, capital, stockpile, naming, HQ, the demand measurements).

namespace {

// THE SALTS. New, and each checked against every stream generation already
// keys (the legacy corporation passes' B1C2D3E4 2F3E4D5C 9E3779B1 9D8C7B6A
// 5A4B3C2D C3D4E5F6 E1F2031C F0E1D2C3, Pass 6's 3D6F9A11 6E17C4B0 1A2B3C4D, the
// search's 8A21F00D and world-gen's 4A71012). Nothing on this path runs
// alongside the legacy passes — it replaces them — but a colliding salt would
// still correlate a budget world's draws with a legacy world's on the same seed.
constexpr uint32_t k_charter_salt_spec_asset   = 0xC7A2E1B3u;
constexpr uint32_t k_charter_salt_spec_capital = 0x51D0C8A7u;
constexpr uint32_t k_charter_salt_spec_stock   = 0xA9E4B26Du;
constexpr uint32_t k_charter_salt_spec_name    = 0x3B8F7C15u;
constexpr uint32_t k_charter_salt_firm_asset   = 0xD26A9F43u;
constexpr uint32_t k_charter_salt_firm_name    = 0x6F1B4E89u;
constexpr uint32_t k_charter_salt_firm_stock   = 0x84C3D75Bu;
constexpr uint32_t k_charter_salt_player       = 0x2E97A3F1u;

// THE BUDGET PATH'S CAPS ARE ITS OWN (BL-1039). The body guard and the per-good
// cap are no longer restated from Pass 6 here: they are `charter_spend_params`
// fields with no shipped default (`max_firms_per_body`, `per_resource_firm_cap`,
// with `resource_cap_rule` and `density_ceiling`), so a budget world's density is
// set by its caller and ruled on the cost table, and nothing here follows Pass
// 6's function-local constants in `generate_background_firms` — which stay
// exactly as they are for every world without a budget.
//
// The per-province cap is NOT a number held here any more (BL-1146, Ben,
// 2026-09-27, NR-960 B, superseding NR-910's flat 2 on a budget world): it
// scales with the province's centre, two firms per rung (`province_firm_cap`,
// corporation_generation.hpp). It is the budget path's own rule, NOT a copy
// that tracks Pass 6's: Pass 6 keeps its flat 2 in `generate_background_firms`
// for every world without a budget, as above. The flat 2 was ruled against
// provinces of <= 20 tiles; a province is now its centre's whole ground, and a
// flat 2 pushed a city's industry out into its villages.

/// A fresh std::mt19937 for one (centre, role): a KEYED draw, the checkpoint
/// idiom — the (seed ^ role salt, centre id) pair keys a splitmix64 state, one
/// step past it seeds the Mersenne stream. Keyed rather than sequenced, so a
/// centre's draws never depend on how many draws another centre made, nor on
/// whether its charters succeeded.
std::mt19937 charter_stream(uint32_t seed, uint32_t role_salt, entity_id centre)
{
    checkpoint_rng key(seed ^ role_salt, static_cast<uint32_t>(centre));
    key.unit();
    return std::mt19937(static_cast<uint32_t>(key.s >> 32));
}

/// settlement.cpp's `grid_dist` (file-local there): column-wrapped Chebyshev,
/// the metric `nearest_region` reads by — restated so "the nation's nearest
/// region" means nearest by the same rule.
int charter_region_dist(int c0, int r0, int c1, int r1, int gw)
{
    int dc = std::abs(c0 - c1);
    if (gw > 0 && dc > gw / 2) dc = gw - dc;
    return std::max(dc, std::abs(r0 - r1));
}

/// One budgeted centre, resolved once.
struct charter_centre
{
    entity_id    centre = null_entity;
    int32_t      points = 0;

    entity_id    tile   = null_entity;
    entity_id    body   = null_entity;
    int          x = 0, y = 0, gw = 0;

    entity_id    nation     = null_entity;   ///< null -> the whole budget is `no_nation`
    int          nation_idx = -1;            ///< into the sorted nation ids (region.nation's space)

    /// TWO REGIONS, because they answer two different questions.
    ///
    /// CHARACTER (a specialist's focus and ownership): `nearest_region`
    /// reconciled to the centre's nation — on a mismatch, the nation's own
    /// nearest region, however far; with none, -1 and the national-character
    /// fallback. A character is a reading of the nation's settlement record, so
    /// distance does not disqualify it.
    int          character_region_idx = -1;
    /// ANCHORING (rung 2's window): `nearest_region` ONLY when that region is
    /// the centre nation's own, else -1 and rung 2 is empty. Never the
    /// reconciled region: a charter stays near its centre (INDUSTRIALISATION.md § 1),
    /// and the nation's nearest region can be anywhere in the nation.
    int          anchor_region_idx    = -1;
    /// ORIGIN (BL-1099, `corporation_component::origin_region`): the settlement
    /// region whose stock this centre's budget was split from -- the carve
    /// slot's own (`world::gen_carve_centres`), which is "the region of that
    /// centre" in the one sense the charter budget knows. A centre the carve
    /// index does not name (a hand-built fixture) falls to its anchor region,
    /// then its character region, then -1 (no origin).
    int          origin_region_idx    = -1;

    /// Points left for background firms once the specialist price is taken —
    /// UNROUNDED, so its remainder below one firm price is booked `remainder`.
    /// Not B's per-centre term (`charter_centre_firm_points`, whole firm
    /// charters), which is summed into the body's `firm_points`.
    int32_t      points_after_specialist = 0;

    /// NR-913 (`charter_spend_params::pool`; 0 unless the spend pools): the
    /// structural remainder this centre SENDS to its group's richest centre,
    /// and the group's pooled remainders it RECEIVES as that richest centre
    /// (its own included). Its firm budget is `points_after_specialist -
    /// pool_out + pool_in`; the specialist test never reads either.
    int64_t      pool_out = 0;
    int64_t      pool_in  = 0;

    std::array<int32_t, charter_unspent_reason_count> unspent{};

    /// NR-905: the points this centre's FIRMS left when the turn stopped because
    /// every good still under its share could not be placed here, and the
    /// placement reason they were booked under — the pool `share_unplaced` is
    /// drawn from once the walk ends. 0 for every other stop.
    int32_t                firm_stop_points = 0;
    charter_unspent_reason firm_stop_reason = charter_unspent_reason::window_exhausted;

    /// The two anchor windows, built lazily and then held: neither the centre,
    /// its nation's tiles nor the settlement record move during a spend.
    bool                   centre_window_built = false;
    std::vector<entity_id> centre_window;
    bool                   region_window_built = false;
    std::vector<entity_id> region_tiles;
};

/// Rung 1: the centre nation's tiles within @p radius of the centre tile on the
/// column-WRAPPED squared metric, in the nation's stored order (so the window is
/// an order-preserving subset, as `place_starting_assets` requires).
const std::vector<entity_id>& charter_centre_window(const world& w, const nation_component& nc,
                                                    charter_centre& cc, int radius)
{
    if (cc.centre_window_built)
        return cc.centre_window;
    cc.centre_window_built = true;
    const long long r2 = static_cast<long long>(radius) * static_cast<long long>(radius);
    for (entity_id tid : nc.tiles)
    {
        const auto t = w.tiles.find(tid);
        if (t == w.tiles.end() || t->second.body != cc.body)
            continue;
        long long dx = std::abs(t->second.grid_x - cc.x);
        if (cc.gw > 0 && dx > cc.gw / 2)
            dx = cc.gw - dx;
        const long long dy = t->second.grid_y - cc.y;
        if (dx * dx + dy * dy <= r2)
            cc.centre_window.push_back(tid);
    }
    return cc.centre_window;
}

/// Rung 2: the centre nation's tiles inside the centre's ANCHOR region
/// (`region_window`, BL-283's reading of "inside a region"). Empty when the
/// centre's nearest region is not its nation's own (`anchor_region_idx` -1).
const std::vector<entity_id>& charter_region_window(const world& w, const nation_component& nc,
                                                    const settlement_state* settle,
                                                    charter_centre& cc)
{
    if (cc.region_window_built)
        return cc.region_tiles;
    cc.region_window_built = true;
    if (settle != nullptr && cc.anchor_region_idx >= 0 && cc.gw > 0)
        cc.region_tiles = region_window(w, nc, *settle, std::vector<int>{ cc.anchor_region_idx },
                                        cc.gw);
    return cc.region_tiles;
}

/// @p window minus every tile standing in a province already at the firm cap —
/// Pass 6's per-province cap, applied to where the ANCHOR may stand (the anchor's
/// province is the firm's province, exactly the tile Pass 6 reads the cap at).
/// The cap is the province's own (BL-1146): `province_firm_cap_of` its centre
/// rung in @p province_rungs, the table `province_centre_rungs` builds once.
/// Filtering before placement rather than refusing after it keeps a capped
/// province from consuming draws; the admissible set is the same.
std::vector<entity_id> charter_under_province_cap(const world& w,
                                                  const std::vector<entity_id>& window,
                                                  const std::map<uint32_t, int>& by_province,
                                                  const std::map<uint32_t, int>& province_rungs)
{
    std::vector<entity_id> out;
    out.reserve(window.size());
    for (entity_id tid : window)
    {
        const uint32_t prov = w.provinces.province_of(tid);
        if (prov != 0)
        {
            // BL-1146: the province's cap is its centre's rung x 2.
            const auto it = by_province.find(prov);
            if (it != by_province.end()
                && it->second >= province_firm_cap_of(province_rungs, prov))
                continue;
        }
        out.push_back(tid);
    }
    return out;
}

/// True when @p window holds a tile `place_starting_assets` could anchor @p focus
/// on — its candidate filter restated, READ-ONLY and drawing nothing (that
/// function authors a building on success, so it cannot be used as the probe).
/// Only asked after a placement failed, to name WHY it failed.
bool charter_window_anchorable(const world& w, const std::vector<entity_id>& window,
                               industrial_focus focus,
                               const std::unordered_set<entity_id>& occupied,
                               const resource_type* dig = nullptr)
{
    const building_type anchor_type = focus_asset_pattern(focus).front();
    for (entity_id tid : window)
    {
        if (occupied.count(tid))
            continue;
        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;
        // BL-1197: a digging firm anchors only on ground for its good.
        if (dig != nullptr && anchor_type == building_type::extraction_site)
        {
            if (dig_site_of(w, tid, it->second, *dig) != dig_site::none)
                return true;
            continue;
        }
        bool any = false;
        const resource_type tgt = placement_rules::richest_extractable(it->second, any);
        if (placement_rules::can_place(it->second, anchor_type, tgt))
            return true;
    }
    return false;
}

constexpr charter_rung k_charter_rungs[] = { charter_rung::centre_window,
                                             charter_rung::region_window };

/// A rung's UNFILTERED window (built lazily, then held on @p cc).
const std::vector<entity_id>& charter_rung_window(const world& w, const nation_component& nc,
                                                  charter_centre& cc,
                                                  const settlement_state* settle,
                                                  const charter_spend_params& spend,
                                                  charter_rung rung)
{
    return (rung == charter_rung::centre_window)
        ? charter_centre_window(w, nc, cc, spend.window_radius)
        : charter_region_window(w, nc, settle, cc);
}

/// Why a placement of @p focus at @p cc FAILS, read on the ground as it stands
/// — READ-ONLY, drawing nothing: `province_cap` when some rung's UNFILTERED
/// window holds anchorable ground and the province-cap filter took all of it,
/// otherwise `window_exhausted`. With no cap in force always the latter. Asked
/// only once a placement is known to fail (`charter_place`'s own failure, or a
/// focus that already failed at this centre — see the turn).
charter_unspent_reason charter_place_failure_reason(const world& w, const nation_component& nc,
                                                    industrial_focus focus,
                                                    const std::unordered_set<entity_id>& occupied,
                                                    charter_centre& cc,
                                                    const settlement_state* settle,
                                                    const charter_spend_params& spend,
                                                    const std::map<uint32_t, int>* by_province,
                                                    const resource_type* dig = nullptr)
{
    if (by_province != nullptr)
        for (const charter_rung rung : k_charter_rungs)
            if (charter_window_anchorable(w, charter_rung_window(w, nc, cc, settle, spend, rung),
                                          focus, occupied, dig))
                return charter_unspent_reason::province_cap;
    return charter_unspent_reason::window_exhausted;
}

/// Place one charter's holdings on the two anchor rungs, and NOTHING wider.
///
/// AN EMPTY WINDOW IS NEVER PASSED to `place_starting_assets`: that function
/// reads a null or empty window as the WHOLE NATION, which is precisely the
/// scatter a budget charter must not do. An empty rung is skipped. A rung that
/// finds no anchorable tile consumes no randomness (place_starting_assets
/// returns before its first draw), so rung 2 draws as if it had been first.
///
/// THE PLACEMENT SEES THE GOOD ONLY FOR A DIGGING FIRM (@p dig, BL-1197: a
/// water- or produce-gap firm, `gap_firm_digs`); otherwise only @p focus: an extraction
/// anchor takes the richest extractable deposit on its tile, whatever good the
/// firm was chartered for, and a processing anchor any workable land. So whether
/// a charter can land is a property of (centre, focus) and the ground as it
/// stands, and a centre whose windows hold no free deposit tile OF ANY KIND can
/// place no extraction firm at all. A digging firm's landing is a property of
/// (centre, good) instead: it stands only on its dig ladder's ground (a Well or
/// Wharf site, or a deposit of its good — `dig_ladder`).
///
/// On failure @p fail_out names why (`charter_place_failure_reason`).
///
/// BL-1217 D6: @p want, when given, is the caller's want test, handed to every
/// rung's `make_chain_feasible` as a fresh copy (a rejected rung unplaces its
/// processors, so the next starts from the same ground).
///
/// CHAIN-FEASIBLE (BL-1185 / BL-1188; Pass 3 "Chain-feasible"): with @p cr
/// given, a rung's placement is kept only if `make_chain_feasible` keeps it — every processor
/// gets a recipe whose inputs are produced within reach, or is unplaced, and a
/// charter left with nothing (or, with @p serve, a firm none of whose
/// processors can make its good) is unplaced whole and the NEXT rung is tried.
/// A rung that had ground but no feasible charter sets @p chain_rejected, and a
/// charter that fails after one names `chain_infeasible`: the windows held
/// ground, none of it within reach of the chain's inputs. Such a failure is a
/// property of the GOOD, not of the focus — the caller must not read it as the
/// focus having no ground.
std::vector<entity_id> charter_place(world& w, const nation_component& nc,
                                     industrial_focus focus,
                                     std::unordered_set<entity_id>& occupied,
                                     std::mt19937& rng,
                                     charter_centre& cc,
                                     const settlement_state* settle,
                                     const charter_spend_params& spend,
                                     const std::map<uint32_t, int>* by_province,
                                     const std::map<uint32_t, int>* province_rungs,
                                     charter_rung& rung_out,
                                     charter_unspent_reason& fail_out,
                                     const recipe_registry& reg,
                                     chain_reach* cr,
                                     const std::vector<int>* serve,
                                     bool& chain_rejected,
                                     const resource_type* dig = nullptr,
                                     refused_draw* refused = nullptr,
                                     const std::set<std::uint32_t>* grids = nullptr,
                                     const output_want* want = nullptr)
{
    chain_rejected = false;
    bool grid_cut = false;   // BL-1232 review: the grid filter took a rung's ground
    // BL-1197 (gap firm digs the gap): with @p dig THE DIG LADDER runs
    // outermost (`dig_ladder`: water a Well site then an ice deposit, produce a
    // produce deposit then a Wharf site) — its first tier in either window, else
    // its second in either — so "where its windows hold one" reads both windows
    // before the next tier. There is no fallback to another deposit: a centre
    // with no ground for the good places nothing for it. Each tier with no
    // ground draws nothing. Without @p dig only the `none` tier runs: the rule
    // above, verbatim.
    const bool digging = dig != nullptr
                      && focus_asset_pattern(focus).front() == building_type::extraction_site;
    const std::array<dig_site, 2> ladder =
        digging ? dig_ladder(*dig) : std::array<dig_site, 2>{ dig_site::none, dig_site::none };
    const std::size_t n_tiers = digging ? ladder.size() : 1u;
    for (std::size_t ti = 0; ti < n_tiers; ++ti)
    {
        const dig_site tier = ladder[ti];
        for (const charter_rung rung : k_charter_rungs)
        {
            const std::vector<entity_id>& base = charter_rung_window(w, nc, cc, settle, spend, rung);
            std::vector<entity_id> window = (by_province != nullptr && province_rungs != nullptr)
                ? charter_under_province_cap(w, base, *by_province, *province_rungs)
                : base;
            // BL-1232: a power firm stands only where it FEEDS a grid in
            // @p grids — the grid of its tile's market centre
            // (`tile_feed_power_grid`, where its listings land; review round),
            // never merely on a short grid's ground whose market lists
            // elsewhere. The window filtered to them, order kept, before any
            // draw. A rung whose capped window held anchorable ground the
            // grid cut took all of is remembered for the miss's reason.
            if (grids != nullptr)
            {
                const bool held = !window.empty()
                    && charter_window_anchorable(w, window, focus, occupied,
                                                 tier != dig_site::none ? dig : nullptr);
                window.erase(std::remove_if(window.begin(), window.end(),
                                            [&](entity_id t) {
                                                const std::uint32_t g = tile_feed_power_grid(w, t);
                                                return g == 0 || grids->count(g) == 0;
                                            }),
                             window.end());
                if (held && !charter_window_anchorable(w, window, focus, occupied,
                                                       tier != dig_site::none ? dig : nullptr))
                    grid_cut = true;
            }
            if (window.empty())
                continue;
            std::vector<entity_id> assets = place_starting_assets(
                w, nc, focus, occupied, rng, &window,
                tier != dig_site::none ? dig : nullptr, tier);
            if (!assets.empty())
            {
                // BL-1233 (BL-1217 D6 final review): each rung's refusal is its
                // own. A firm that LANDS books only its landing rung's (its own
                // refused works, at its own market) — never a rejected rung's;
                // a charter that fails on every rung books the first refusal.
                refused_draw rung_refused;
                if (cr != nullptr
                    && !make_chain_feasible(w, reg, *cr, assets, occupied, serve, /*whole=*/true,
                                            refused != nullptr ? &rung_refused : nullptr, want))
                {
                    if (refused != nullptr && !refused->set && rung_refused.set)
                        *refused = rung_refused;
                    chain_rejected = true;
                    continue;
                }
                if (refused != nullptr)
                    *refused = rung_refused;
                rung_out = rung;
                return assets;
            }
        }
    }

    // Nothing placed: `occupied` is as it was (a chain-rejected rung unplaced
    // everything it authored), and the reason reads the same ground the rungs
    // just did — unless a rung had ground and the chain refused it.
    fail_out = chain_rejected
        ? charter_unspent_reason::chain_infeasible
        : charter_place_failure_reason(w, nc, focus, occupied, cc, settle, spend, by_province,
                                       digging ? dig : nullptr);
    // BL-1232 review: a miss the window would NOT have had but for the grid cut
    // — anchorable ground under the province cap, none of it feeding a wanted
    // grid — is its own reason, not `window_exhausted`.
    if (grid_cut && fail_out == charter_unspent_reason::window_exhausted)
        fail_out = charter_unspent_reason::no_short_grid_ground;
    return {};
}

/// Per-body state of the firm charters — Pass 6's per-body tallies, and the
/// body's density rule, FIXED BEFORE THE WALK (BL-1039).
struct charter_body_state
{
    std::array<float, resource_count> consumer_demand{};
    std::array<int, resource_count>   firms_by_resource{};
    std::map<uint32_t, int>           firms_by_province;
    int                               firms = 0;

    /// B: the body's points for FIRMS (`charter_centre_firm_points`, summed over
    /// its nation-resolved centres) — the same units as B_ref.
    int64_t                           firm_points = 0;
    /// B in whole firm charters, each centre's at its own price (BL-1168).
    int64_t                           firm_charters = 0;
    /// G: goods with demand before the walk (resource indices, ascending).
    std::vector<std::uint16_t>        goods;
    /// BL-1197 round 5: the goods the body can produce at all (`body_producible`);
    /// G is filtered by it. A short good it rules out books `unproducible` where
    /// it stops a centre — a real want no firm on the body could serve — and a
    /// short good outside G that the body CAN produce books `late_shortfall`.
    std::array<bool, resource_count>  producible{};
    /// B_ref = c x |G| x firm price.
    int64_t                           reference_points = 0;
    /// Firms per good per body; -1 = no per-good cap (`lifted`).
    int32_t                           per_good_cap = -1;
    /// BL-1233 ruling A: the prospective draws of firms the sized rule refused.
    prospective_draws                 prospective;
    /// Background firms per body before `density_ceiling`; 0 = no ceiling.
    int32_t                           density_ceiling = 0;

    /// THE TURN (`sqrt_capital` only): the goods filled in turn — G without
    /// construction capacity, ascending resource index — and the position the
    /// next pass resumes at. The cursor is per BODY, so it carries across the
    /// body's centres: a second centre continues the pass the first one left.
    bool                              in_turn = false;
    std::vector<std::uint16_t>        turn;
    std::size_t                       turn_cursor = 0;

    /// THE YARDS' PLACES (NR-906; `charter_body_record::yard_places`): the most
    /// yards the walk can provision on this body, taken off the ceiling before
    /// the shares are cut. See `charter_yard_places`.
    int32_t                           yard_places = 0;
    /// `want_yards` at the walk's most-built extreme, uncapped — report only.
    /// NOT what the refusal reads (BL-1060 round 4, found by the cold review):
    /// it is the want BEFORE the per-good cap and the cover bound, so it runs
    /// orders of magnitude past the yards the walk can stand up, and a refusal
    /// on it turned ordinary worlds down.
    int64_t                           yard_want_bound = 0;
    /// THE SHARES COULD NOT BE CUT: the ceiling can bind on this body (its
    /// charters outrun the ceiling and the turn's caps sum past it) and yet the
    /// ceiling less the yards' places leaves under one firm per turn good. The
    /// spend is REFUSED whole — the walk and `charter_spend_world_refusal` both
    /// read this flag, so neither can proceed where NR-905's reservation cannot
    /// hold.
    bool                              share_uncuttable = false;
    /// THE EVEN SHARE (NR-905, NR-906; `charter_body_record::even_share`): 0
    /// where the ceiling does not bind. `share[r]` is turn good r's RESERVATION
    /// of the ceiling — (ceiling - yard_places) / |turn|, the remainder one more
    /// each to the first goods of the turn — never a cap: see the turn.
    int32_t                           even_share = 0;
    int32_t                           even_share_extra = 0;
    std::array<int32_t, resource_count> share{};
};

/// Holdings a corporation can open with: the longest `focus_asset_pattern`
/// (`holdings_range` tops out at 4). The bound the yards' places are read at.
constexpr int64_t k_charter_max_holdings = 4;

/// THE YARDS' PLACES (NR-906): how many construction yards the walk can
/// provision on a body, read BEFORE the walk so their places come off the
/// ceiling before the shares are cut.
///
/// THE EXACT COUNT IS NOT KNOWABLE BEFORE THE WALK: the yard step wants yards
/// against the building stock, which grows with every charter by as many
/// holdings as that charter's draw gives it, and stops when the yards' own
/// output covers the want. So this is an UPPER BOUND, from the yard step's
/// three tests at the walk's most-built extreme:
///   * the per-good cap (the step never passes it);
///   * `want_yards` at the most buildings the walk can stand up — every
///     building standing now plus `k_charter_max_holdings` for every
///     corporation it can charter here (the centres that afford a specialist,
///     and firms up to the ceiling) — at the construction seed rate per building
///     (the consumer and upkeep draws of construction capacity as they stand);
///   * the yards it takes to cover that demand from today's output, each yard
///     adding at least its anchor's (`nominal_processing_batches` x the yard
///     recipe's output), since the step stops once output covers demand.
/// WHY AN UPPER BOUND, NOT A GUESS: reserving too few would let a late yard
/// take a place a good was promised; reserving too many costs the fill nothing,
/// because a share is a reservation, not a cap — the room nobody claims goes to
/// whichever short good is next in turn. 0 where no in-band recipe makes
/// capacity, the seed rate is 0, or no demand can outrun today's output.
///
/// @p want_bound_out receives the SECOND test alone (`want_yards` at the
/// most-built extreme, uncapped), for the report. IT IS NOT WHAT ANY REFUSAL
/// READS (BL-1060 round 4, found by the cold review): uncapped it runs orders
/// of magnitude past the places the walk can fill — at a seed rate of 0.30, the
/// rate `scripts/economy.lua` records as measured, it turned an ordinary world
/// down and the search fell back to the legacy no-budget world in silence. The
/// refusal reads `charter_body_state::share_uncuttable`, cut from the places
/// this function returns, on the world the walk itself is about to spend on.
int32_t charter_yard_places(const world& w, const recipe_registry& reg, entity_id body_id,
                            float consumer_now, float upkeep_now, int32_t per_good_cap,
                            int64_t corps_max, int64_t& want_bound_out)
{
    want_bound_out = 0;
    const std::size_t cap_i = static_cast<std::size_t>(resource_type::construction_capacity);
    const int ci = best_construction_recipe(reg);
    if (ci < 0 || per_good_cap <= 0)
        return 0;
    const float per_yard = reg.recipe_at(building_type::processing_facility, ci).outputs[cap_i];
    const float per      = reg.construction().seed_capacity_per_building;
    if (!(per_yard > 0.0f))
        return 0;

    int64_t standing = 0;   // body_construction_demand's own count
    for (const auto& [bid, b] : w.buildings)
    {
        (void)bid;
        if (b.decommissioned)
            continue;
        const auto t = w.tiles.find(b.tile);
        if (t != w.tiles.end() && t->second.body == body_id)
            ++standing;
    }
    // THE UPKEEP HALF IS EXTRAPOLATED TOO (BL-1060 round 4, found by the cold
    // review). `upkeep_now` is the construction capacity the buildings STANDING
    // NOW draw in upkeep; the walk re-measures it per firm, so it grows with
    // every building the walk stands up. Reading it flat made the bound an
    // upper bound only while `building_upkeep_goods` draws no capacity
    // (scripts/economy.lua, both bands) — a data tune would have let the walk
    // provision more yards than the places reserved, over-subscribing the
    // ceiling. So it is carried per standing building to the same extreme.
    const int64_t built_max     = standing + k_charter_max_holdings * corps_max;
    const float   upkeep_per    = standing > 0
                                    ? upkeep_now / static_cast<float>(standing)
                                    : 0.0f;
    const float   upkeep_at_max = standing > 0
                                    ? upkeep_per * static_cast<float>(built_max)
                                    : upkeep_now;
    const float demand_max = consumer_now + upkeep_at_max
        + per * static_cast<float>(built_max);

    const double want = std::ceil(static_cast<double>(demand_max) / static_cast<double>(per_yard));
    want_bound_out = static_cast<int64_t>(std::min<double>(want, 1e9));

    std::array<float, resource_count> production = {};
    accumulate_body_production(w, reg, body_id, production);
    if (!(demand_max > production[cap_i]))
        return 0;   // the step wants a yard only while demand outruns output

    int64_t places = static_cast<int64_t>(std::min<double>(want, static_cast<double>(per_good_cap)));
    const float one_yard = nominal_processing_batches(reg) * per_yard;
    if (one_yard > 0.0f)
    {
        const double cover = std::ceil(static_cast<double>(demand_max - production[cap_i])
                                       / static_cast<double>(one_yard));
        places = std::min<int64_t>(places, static_cast<int64_t>(std::min<double>(cover, 1e9)));
    }
    return static_cast<int32_t>(std::max<int64_t>(0, places));
}

/// BL-1197 review — each good's ANCHOR ROUTE on @p body_id: the browse index of
/// its cheapest in-band processing recipe by marginal cost per unit of primary
/// output (inputs at base price + the per-batch wage, PRODUCTION.md § The recipe
/// margin anchor), among the recipes whose inputs are all @p producible on the
/// body; -1 for a good with no such recipe, and for every extractable raw
/// (extraction is a raw's anchor route). Ties go to the earlier recipe. Base
/// price is the lowest positive base price any market on the body quotes — the
/// authored table, before a market's premiums. A body with no market prices
/// inputs at 0 and ranks routes by wage per unit alone. Deterministic: ordered
/// scans and an order-free min.
std::array<int, resource_count> body_anchor_routes(const world& w, const recipe_registry& reg,
                                                   entity_id body_id,
                                                   const std::array<bool, resource_count>& producible)
{
    std::array<float, resource_count> price{};
    for (const auto& [mid, m] : w.markets)
    {
        if (m.body != body_id)
            continue;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (m.base_price[r] > 0.0f && (price[r] <= 0.0f || m.base_price[r] < price[r]))
                price[r] = m.base_price[r];
    }
    const building_economics& pe = reg.economics(building_type::processing_facility);
    const double wage_pb = (pe.base_rate > 0.0f)
        ? static_cast<double>(pe.base_wage) / static_cast<double>(pe.base_rate) : 0.0;

    std::array<int, resource_count>    best;
    std::array<double, resource_count> best_mc{};
    best.fill(-1);
    const int n = reg.recipe_count(building_type::processing_facility);
    for (int i = 0; i < n; ++i)
    {
        const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
        const std::size_t po = static_cast<std::size_t>(primary_output_resource(rc));
        if (!(rc.outputs[po] > 0.0f) || placement_rules::is_extractable(static_cast<resource_type>(po)))
            continue;
        bool runs = true;
        double inp = 0.0;
        for (std::size_t r = 0; r < resource_count && runs; ++r)
        {
            if (!(rc.inputs[r] > 0.0f))
                continue;
            if (!producible[r])
                runs = false;
            inp += static_cast<double>(rc.inputs[r]) * static_cast<double>(price[r]);
        }
        if (!runs)
            continue;
        const double mc = (inp + wage_pb) / static_cast<double>(rc.outputs[po]);
        if (best[po] < 0 || mc < best_mc[po])
        {
            best[po]    = i;
            best_mc[po] = mc;
        }
    }
    return best;
}

/// One body's density rule, FIXED BEFORE THE WALK (BL-1039, NR-905, NR-906):
/// G and the consumer demand, B_ref, the per-good cap, the ceiling, and under
/// `sqrt_capital` the turn, the yards' places and the even share. @p bs holds
/// B (`firm_points`) on entry; @p specialists counts the body's
/// nation-resolved centres that afford a specialist. ONE function, called by
/// the walk and by `charter_spend_world_refusal`, so a refusal reads exactly
/// the numbers the walk would.
void charter_fix_body_rules(const world& w, const recipe_registry& reg,
                            const charter_spend_params& spend, entity_id body_id,
                            int64_t specialists, charter_body_state& bs)
{
    bs.consumer_demand = body_demand(w, reg, body_id);
    std::array<float, resource_count> demand = bs.consumer_demand;
    const std::array<float, resource_count> upkeep = body_upkeep_demand(w, reg, body_id);
    for (std::size_t r = 0; r < resource_count; ++r)
        demand[r] += upkeep[r];
    const std::array<float, resource_count> construction_need =
        body_construction_demand(w, reg, body_id);
    for (std::size_t r = 0; r < resource_count; ++r)
        demand[r] += construction_need[r];
    // BL-1197 — G, THE GOODS THE WALK SERVES (CORPORATION_GENERATION.md § Pass 6,
    // "A processor's inputs are wanted too"). Three steps, all fixed here:
    //  1. DEMAND, final or derived: the three halves above plus what the
    //     processors standing now draw of their inputs (round 4).
    //  2. PRODUCIBLE ONLY (round 5): a good the body cannot produce at all
    //     (`body_producible` — an off-world raw on an earthlike body) is out; it
    //     would take a share of the ceiling no firm could ever fill.
    //  3. THE ANCHOR-ROUTE CLOSURE (review, main session 2026-10-05): because G
    //     is fixed before the walk while the walk's own processors raise their
    //     inputs' gaps as they land, G also holds the inputs of each member's
    //     ANCHOR ROUTE — its cheapest in-band recipe by marginal cost per unit of
    //     primary output (PRODUCTION.md § The recipe margin anchor), chosen among
    //     the routes the body can run — to a fixed point. Not every alternative
    //     recipe, and never through a good the body cannot produce. Steel enters
    //     through machinery's route; iron ore and coal through steel's. A closure
    //     good no processor wants yet is not short, so the turn passes over it.
    const std::array<float, resource_count> input_need =
        body_processor_input_demand(w, reg, body_id);
    bs.producible = body_producible(w, reg, body_id);
    std::array<bool, resource_count> in_g{};
    for (std::size_t r = 0; r < resource_count; ++r)
        in_g[r] = (demand[r] + input_need[r]) > 0.0f && bs.producible[r];
    const std::array<int, resource_count> anchor = body_anchor_routes(w, reg, body_id,
                                                                      bs.producible);
    for (bool grew = true; grew;)
    {
        grew = false;
        for (std::size_t g = 0; g < resource_count; ++g)
        {
            if (!in_g[g] || anchor[g] < 0)
                continue;
            const recipe& rc = reg.recipe_at(building_type::processing_facility, anchor[g]);
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rc.inputs[r] > 0.0f && !in_g[r])
                {
                    in_g[r] = true;   // producible: the route was chosen among those
                    grew    = true;
                }
        }
    }
    for (std::size_t r = 0; r < resource_count; ++r)
        if (in_g[r])
            bs.goods.push_back(static_cast<std::uint16_t>(r));

    const int g = static_cast<int>(bs.goods.size());
    // 0 under `lifted`: its c is refused unless 0 (it applies no per-good cap).
    bs.reference_points = (g > 0)
        ? static_cast<int64_t>(spend.per_resource_firm_cap) * g
              * static_cast<int64_t>(spend.firm_price_points)
        : 0;
    switch (spend.resource_cap_rule)
    {
    case charter_cap_rule::fixed:
        bs.per_good_cap = spend.per_resource_firm_cap;
        break;
    case charter_cap_rule::lifted:
        bs.per_good_cap = -1;
        break;
    case charter_cap_rule::sqrt_capital:
    {
        // BL-1168: read in WHOLE CHARTERS (a firm price of 1), each centre's
        // at its own reach price. Where one price holds this is the old
        // c x B / (|G| x fp) exactly, since B is whole charters times fp.
        bs.per_good_cap = charter_sqrt_per_good_cap(spend.per_resource_firm_cap,
                                                    bs.firm_charters, g, 1);
        // BL-1204: the ceiling per good served, read once G is fixed (above).
        bs.density_ceiling = charter_density_ceiling(spend, g);
        // The turn: G ascending, construction capacity out (it keeps its own
        // provisioning step, which runs before the turn — see the walk).
        const std::uint16_t cap_good =
            static_cast<std::uint16_t>(resource_type::construction_capacity);
        bs.in_turn = true;
        for (const std::uint16_t r : bs.goods)
            if (r != cap_good)
                bs.turn.push_back(r);

        // THE YARDS' PLACES and THE EVEN SHARE (Ben, 2026-09-19, NR-905;
        // NR-906). The ceiling BINDS when the body holds more firm charters
        // than the ceiling AND the turn's per-good caps sum past it: only then
        // can the turn outrun the ceiling. Then the ceiling, less the yards'
        // places, is reserved in equal parts to the goods of the turn, the
        // remainder one more each to the first of them (ascending resource
        // index, the turn's own order). Where it does not bind there are no
        // shares and the turn fills every good to its cap, as before.
        const int64_t n_turn   = static_cast<int64_t>(bs.turn.size());
        const int64_t charters = bs.firm_charters;
        const int64_t ceiling  = bs.density_ceiling;
        const std::size_t cap_i = static_cast<std::size_t>(cap_good);
        bs.yard_places = charter_yard_places(w, reg, body_id,
                                             bs.consumer_demand[cap_i], upkeep[cap_i],
                                             bs.per_good_cap,
                                             specialists + std::min(charters, ceiling),
                                             bs.yard_want_bound);
        const int64_t room = ceiling - bs.yard_places;
        const bool    binds = n_turn > 0 && charters > ceiling
                              && n_turn * static_cast<int64_t>(bs.per_good_cap) > ceiling;
        // BL-1060 round 4: where the ceiling binds and the room left will not
        // give every turn good a firm, the reservation NR-905 rules cannot be
        // cut at all — the spend is refused whole rather than run without it.
        bs.share_uncuttable = binds && room < n_turn;
        if (binds && room >= n_turn)
        {
            bs.even_share       = static_cast<int32_t>(room / n_turn);
            bs.even_share_extra = static_cast<int32_t>(room % n_turn);
            for (std::size_t i = 0; i < bs.turn.size(); ++i)
                bs.share[bs.turn[i]] = bs.even_share
                    + (static_cast<int64_t>(i) < bs.even_share_extra ? 1 : 0);
        }
        break;
    }
    }
}

/// NR-913 — THE POOLED REMAINDERS (`charter_spend_params::pool`), planned
/// before anything is chartered, on the walk's own resolution of each centre.
/// Every budgeted centre on a nation's tile whose group resolves sends its
/// STRUCTURAL remainder — its points net of the specialist price it affords,
/// mod the firm price, exactly what the walk would book `remainder` — to the
/// group's richest centre (its own points, ties to the lower id), which spends
/// the pool on background firms. Empty under `charter_pool::none`.
/// Deterministic: groups in key order, members in centre-id order.
struct charter_pool_plan
{
    std::map<entity_id, int64_t> in;   ///< each group's richest centre: the pool it receives
    std::map<entity_id, int64_t> out;  ///< each pooling centre: its own remainder, sent
    std::vector<charter_pool_transfer> transfers;   ///< from != to, ascending from
};

charter_pool_plan plan_charter_pool(const world& w, const charter_budget& budget,
                                    const charter_spend_params& spend)
{
    charter_pool_plan plan;
    if (spend.pool == charter_pool::none || budget.empty() || spend.firm_price_points <= 0)
        return plan;
    std::map<int64_t, std::vector<std::pair<entity_id, int32_t>>> groups;
    for (const auto& [centre_id, pts] : budget.points())
    {
        const auto tile_it = w.population_centre_tile.find(centre_id);
        if (tile_it == w.population_centre_tile.end() || w.tiles.count(tile_it->second) == 0)
            continue;
        const auto own = w.tile_to_nation.find(tile_it->second);
        if (own == w.tile_to_nation.end() || w.nations.count(own->second) == 0)
            continue;   // no nation: its whole budget is `no_nation`, nothing to pool
        int64_t key = -1;
        switch (spend.pool)
        {
        case charter_pool::region:
        {
            const auto slot = w.gen_carve_centres.find(centre_id);
            if (slot != w.gen_carve_centres.end() && slot->second.region >= 0)
                key = slot->second.region;
            break;
        }
        case charter_pool::market:
        {
            const entity_id m = market_for_tile(w, tile_it->second);
            if (m != null_entity)
                key = static_cast<int64_t>(m);
            break;
        }
        case charter_pool::nation:
            key = static_cast<int64_t>(own->second);
            break;
        case charter_pool::none:
            break;
        }
        if (key >= 0)
            groups[key].push_back({ centre_id, pts });
    }

    for (const auto& [key, members] : groups)
    {
        (void)key;
        entity_id richest     = null_entity;
        int32_t   richest_pts = -1;
        int64_t   pooled      = 0;
        for (const auto& [c, pts] : members)   // ascending id: a tie keeps the lower id
        {
            if (pts > richest_pts)
            {
                richest     = c;
                richest_pts = pts;
            }
            // BL-1168: each member's remainder at its own price.
            const int64_t fp = spend.firm_price_of(c);
            const int64_t sp = spend.specialist_price_of(c);
            int64_t left = pts;
            if (sp > 0 && left >= sp)
                left -= sp;
            const int64_t r = left % fp;
            if (r > 0)
            {
                plan.out[c] = r;
                pooled += r;
            }
        }
        if (pooled <= 0)
            continue;
        plan.in[richest] = pooled;
        for (const auto& [c, pts] : members)
        {
            (void)pts;
            const auto o = plan.out.find(c);
            if (o != plan.out.end() && c != richest)
                plan.transfers.push_back({ c, richest, o->second });
        }
    }
    std::sort(plan.transfers.begin(), plan.transfers.end(),
              [](const charter_pool_transfer& a, const charter_pool_transfer& b) { return a.from < b.from; });
    return plan;
}

/// A centre's pooled points in WHOLE firm charters — what B counts of them.
/// The richest centre's own firm budget net of its remainder is already whole,
/// so its B term is `charter_centre_firm_points` plus this.
int64_t charter_pool_whole(const charter_pool_plan& plan, entity_id centre,
                           const charter_spend_params& spend)
{
    const auto it = plan.in.find(centre);
    if (it == plan.in.end() || spend.firm_price_points <= 0)
        return 0;
    const int64_t fp = spend.firm_price_of(centre);   // the receiver spends at its own price
    return (it->second / fp) * fp;
}

} // namespace

const char* charter_spend_world_refusal(const world& w, const recipe_registry& reg,
                                        const charter_budget& budget,
                                        const charter_spend_params& spend)
{
    if (budget.empty() || spend.resource_cap_rule != charter_cap_rule::sqrt_capital
        || charter_spend_refusal(budget, spend) != nullptr)
        return nullptr;   // not a sqrt budget world, or already refused on its params
    const charter_pool_plan pool = plan_charter_pool(w, budget, spend);   // NR-913
    std::map<entity_id, charter_body_state> bodies;   // std::map: ascending body id
    std::map<entity_id, int64_t> specialists;
    for (const auto& [centre_id, pts] : budget.points())
    {
        // The walk's own resolution: the centre's tile, its body, and the nation
        // owning that tile (a centre without one buys nothing).
        const auto tile_it = w.population_centre_tile.find(centre_id);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto t = w.tiles.find(tile_it->second);
        if (t == w.tiles.end())
            continue;
        const auto own = w.tile_to_nation.find(tile_it->second);
        if (own == w.tile_to_nation.end() || w.nations.count(own->second) == 0)
            continue;
        const int64_t fpts = charter_centre_firm_points(pts, spend, centre_id)
                           + charter_pool_whole(pool, centre_id, spend);
        bodies[t->second.body].firm_points   += fpts;
        bodies[t->second.body].firm_charters += fpts / spend.firm_price_of(centre_id);
        if (static_cast<int64_t>(pts) >= spend.specialist_price_of(centre_id))
            ++specialists[t->second.body];
    }
    // THE TEST: every good of the turn must be able to keep a share of at least
    // one firm beside the yards. The yards are read at the want bound — never
    // below the places the shares are cut from, and never larger after the
    // budget apply removes world-gen's roster than before it (G, too, only
    // shrinks), so this answer, asked on the world as the apply receives it,
    // is never reversed by the walk re-asking it after the removal.
    for (auto& [body_id, bs] : bodies)
    {
        charter_fix_body_rules(w, reg, spend, body_id, specialists[body_id], bs);
        if (bs.share_uncuttable)
            return "density_ceiling is smaller than a body's turn goods plus its yards' places, so "
                   "the goods could not each keep a share of it (NR-905)";
    }
    return nullptr;
}

bool charter_budget_affords_specialist(const world& w, const charter_budget& budget,
                                       const charter_spend_params& spend)
{
    for (const auto& [centre_id, pts] : budget.points())
    {
        if (static_cast<int64_t>(pts) < spend.specialist_price_of(centre_id))
            continue;
        // The walk's own resolution: the centre's tile, and the nation owning
        // it (a centre without one charters nothing).
        const auto tile_it = w.population_centre_tile.find(centre_id);
        if (tile_it == w.population_centre_tile.end() || w.tiles.count(tile_it->second) == 0)
            continue;
        const auto own = w.tile_to_nation.find(tile_it->second);
        if (own != w.tile_to_nation.end() && w.nations.count(own->second) != 0)
            return true;
    }
    return false;
}

std::map<uint32_t, int> province_centre_rungs(const world& w)
{
    // THE ANCHOR IS province.cpp's (`province_anchors`), the one derivation
    // `seed_province_holders` reads too; the rung is its summed scale on the
    // ladder's 1-5.
    std::map<uint32_t, int> rungs;
    for (const auto& [prov, anchor] : province_anchors(w))
        rungs[prov] = std::clamp(anchor.scale, 1, 5);
    return rungs;
}

int province_firm_cap_of(const std::map<uint32_t, int>& rungs, uint32_t province)
{
    const auto it = rungs.find(province);
    return province_firm_cap(it == rungs.end() ? 1 : it->second);
}

budget_world_reading read_budget_world(const world& w, const charter_budget* budget,
                                       const charter_spend_params& spend,
                                       const recipe_registry* reg)
{
    budget_world_reading r;
    // 1. No budget, or an empty one: today's world, nothing else read.
    if (budget == nullptr || budget->empty())
        return r;
    // 2 and 3. The params' refusal, then the world's — the latter only with a
    // registry (bump 11 has none; see the header for why that cannot mislead it).
    r.refusal = charter_spend_refusal(*budget, spend);
    if (r.refusal == nullptr && reg != nullptr)
        r.refusal = charter_spend_world_refusal(w, *reg, *budget, spend);
    if (r.refusal != nullptr)
    {
        r.kind = budget_world_kind::refused;
        return r;
    }
    // 4. The no-specialist world (NR-910).
    if (!charter_budget_affords_specialist(w, *budget, spend))
    {
        r.kind = budget_world_kind::no_specialist;
        return r;
    }
    r.kind = budget_world_kind::budget;
    return r;
}

bool is_budget_world(const world& w, const charter_budget* budget,
                     const charter_spend_params& spend, const recipe_registry* reg)
{
    return read_budget_world(w, budget, spend, reg).budget_world();
}

std::map<entity_id, charter_nation_plan> plan_charters_by_nation(
    const world& w, const charter_budget& budget, const charter_spend_params& spend)
{
    std::map<entity_id, charter_nation_plan> out;   // std::map: ascending nation id
    if (budget.empty() || spend.firm_price_points <= 0)
        return out;

    // The walk's resolution of every budgeted centre: its tile, its body and the
    // nation owning the tile. A centre without one charters nothing.
    struct planned_centre
    {
        entity_id centre = null_entity;
        int32_t   points = 0;
        entity_id body   = null_entity;
        entity_id nation = null_entity;
    };
    std::vector<planned_centre> centres;
    centres.reserve(budget.points().size());
    for (const auto& [centre_id, pts] : budget.points())
    {
        const auto tile_it = w.population_centre_tile.find(centre_id);
        if (tile_it == w.population_centre_tile.end())
            continue;
        const auto t = w.tiles.find(tile_it->second);
        if (t == w.tiles.end())
            continue;
        const auto own = w.tile_to_nation.find(tile_it->second);
        if (own == w.tile_to_nation.end() || w.nations.count(own->second) == 0)
            continue;
        centres.push_back({ centre_id, pts, t->second.body, own->second });
    }

    // The walk's spend order: budget DESCENDING, ties to the lower centre id.
    std::sort(centres.begin(), centres.end(), [](const planned_centre& a, const planned_centre& b) {
        if (a.points != b.points)
            return a.points > b.points;
        return a.centre < b.centre;
    });

    // The walk's stop per body: the runaway guard, and under `sqrt_capital` the
    // density ceiling below it (`charter_fix_body_rules` sets the ceiling only
    // there). A body at its stop charters no further firm.
    //
    // BL-1204: the shipped ceiling is per good SERVED, and |G| needs a recipe
    // registry this plan does not have (generation is registry-free; the carve
    // asks it at bump 11). So the plan reads the ceiling at the most goods a
    // body could serve, `resource_count` — under the guard that is the guard
    // less one. The plan stays an UPPER BOUND per body (the walk's ceiling, at
    // its real |G|, is never above it); what it gives up is closeness on a body
    // whose budget outruns its real ceiling. A fixed ceiling (an instrument's
    // row) is read as before.
    int64_t body_stop = spend.max_firms_per_body;
    const int32_t plan_ceiling =
        charter_density_ceiling(spend, static_cast<int>(resource_count));
    if (plan_ceiling > 0)
        body_stop = std::min<int64_t>(body_stop, plan_ceiling);
    body_stop = std::max<int64_t>(0, body_stop);

    // NR-913: the walk's own pooled-remainder plan (empty under `none`).
    const charter_pool_plan pool = plan_charter_pool(w, budget, spend);

    std::map<entity_id, int64_t> firms_on_body;   // planned so far, per body
    for (const planned_centre& pc : centres)
    {
        const int64_t fp               = spend.firm_price_of(pc.centre);       // BL-1168
        const int64_t specialist_price = spend.specialist_price_of(pc.centre);
        int64_t after_specialist = pc.points;
        int64_t specialists      = 0;
        if (static_cast<int64_t>(pc.points) >= specialist_price)
        {
            specialists       = 1;
            after_specialist -= specialist_price;
        }
        int64_t firm_budget = after_specialist;
        if (const auto o = pool.out.find(pc.centre); o != pool.out.end())
            firm_budget -= o->second;
        if (const auto i = pool.in.find(pc.centre); i != pool.in.end())
            firm_budget += i->second;
        int64_t firms = firm_budget > 0 ? firm_budget / fp : 0;
        int64_t& on_body = firms_on_body[pc.body];
        firms = std::clamp<int64_t>(firms, 0, std::max<int64_t>(0, body_stop - on_body));
        on_body += firms;
        if (specialists == 0 && firms == 0)
            continue;
        charter_nation_plan& p = out[pc.nation];
        p.specialists += specialists;
        p.firms       += firms;
    }
    return out;
}

void publish_charter_web(generation_progress* progress, const world& w,
                         const charter_spend_report& rep)
{
    if (progress == nullptr)
        return;
    // `rep.charters` is sorted by corporation id, and the walk hands ids out
    // monotonically, richest centre first — so this is charter order.
    int slot = 0;
    for (const charter_record& cr : rep.charters)
    {
        if (!cr.specialist)
            continue;
        if (slot >= generation_progress::max_corp_slots)
            break;   // overflow dropped, as the tap's own bound says
        const auto cit = w.corporations.find(cr.corp);
        if (cit == w.corporations.end())
            continue;
        for (const entity_id tile : cr.holdings)
        {
            const auto tit = w.tiles.find(tile);
            if (tit == w.tiles.end() || tit->second.body != w.home_body)
                continue;   // the carve is published over the home body's grid
            progress->mark_asset(tit->second.grid_x, tit->second.grid_y, slot);
        }
        progress->add_corp_row(slot, static_cast<int>(cit->second.focus),
                               static_cast<int>(cr.holdings.size()),
                               cit->second.starting_capital);
        ++slot;
    }
}

std::vector<entity_id> charter_web_from_budget(world& w,
                                               const recipe_registry& reg,
                                               const charter_budget& budget,
                                               const charter_spend_params& spend,
                                               uint32_t seed,
                                               const settlement_state* settle,
                                               charter_spend_report* report)
{
    std::vector<entity_id> chartered;

    // --- refused params charter NOTHING, and touch nothing ------------------
    // `apply_landscape_candidate`'s budget overload decides a refusal before any
    // mutation and never reaches here with one; this guard keeps the function
    // honest for any other caller. BL-1060: the world-read refusal too (a
    // ceiling too small for a body's turn and yards), read on the world as it
    // stands here — before anything below charters.
    const char* refusal = charter_spend_refusal(budget, spend);
    if (refusal == nullptr)
        refusal = charter_spend_world_refusal(w, reg, budget, spend);
    if (refusal != nullptr)
    {
        if (report != nullptr)
            *report = charter_refused_report(budget, refusal);
        return chartered;
    }

    charter_spend_report rep;
    rep.points_budgeted = budget.total();
    if (budget.empty())
    {
        rep.no_specialists = true;
        if (report != nullptr)
            *report = std::move(rep);
        return chartered;
    }

    // Prices are per centre (BL-1168, `firm_price_of`); a specialist's is
    // widened, the product of two int32 inputs, and a price above every int32
    // budget entry is simply never affordable.

    // Sorted nation ids — the index space `region::nation` speaks, exactly as
    // generate_corporations builds it.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());
    const int nation_count = static_cast<int>(nation_ids.size());

    // --- resolve every budgeted centre once, in id order ---------------------
    std::vector<charter_centre> centres;
    centres.reserve(budget.points().size());
    for (const auto& [centre_id, pts] : budget.points())
    {
        charter_centre cc;
        cc.centre = centre_id;
        cc.points = pts;

        const auto tile_it = w.population_centre_tile.find(centre_id);
        const auto t = (tile_it != w.population_centre_tile.end())
            ? w.tiles.find(tile_it->second) : w.tiles.end();
        if (t != w.tiles.end())
        {
            cc.tile = tile_it->second;
            cc.body = t->second.body;
            cc.x    = t->second.grid_x;
            cc.y    = t->second.grid_y;
            const auto b = w.bodies.find(cc.body);
            cc.gw = (b != w.bodies.end()) ? b->second.grid_width : 0;

            // HOME NATION = the owner of the centre's own tile. Nothing else:
            // no weighting, no balancing across nations (INDUSTRIALISATION.md § 1).
            const auto own = w.tile_to_nation.find(cc.tile);
            if (own != w.tile_to_nation.end() && w.nations.count(own->second) != 0)
            {
                cc.nation = own->second;
                cc.nation_idx = static_cast<int>(
                    std::lower_bound(nation_ids.begin(), nation_ids.end(), cc.nation)
                    - nation_ids.begin());
            }
        }

        if (cc.nation == null_entity)
        {
            // No tile, or a tile no nation owns: nothing can host a charter here.
            cc.unspent[static_cast<std::size_t>(charter_unspent_reason::no_nation)] += cc.points;
            centres.push_back(std::move(cc));
            continue;
        }

        // THE REGIONS. `nearest_region` is nation-AGNOSTIC: near a border the
        // nearest anchor can be a neighbour's.
        //  * ANCHORING takes it only when it is the centre nation's own; a
        //    neighbour's nearest region leaves rung 2 empty, so a charter that
        //    finds no ground in the centre window goes unspent rather than to
        //    some far region of its nation.
        //  * CHARACTER takes the nation's own nearest region to the centre on a
        //    mismatch (same wrapped metric, ties to the lower index); a nation
        //    with none keeps -1 and falls to the national character below.
        if (settle != nullptr && !settle->regions.empty() && cc.gw > 0)
        {
            int pi = nearest_region(*settle, cc.x, cc.y, cc.gw);
            const bool same_nation = pi >= 0
                && settle->regions[static_cast<std::size_t>(pi)].nation >= 0
                && settle->regions[static_cast<std::size_t>(pi)].nation < nation_count
                && nation_ids[static_cast<std::size_t>(
                       settle->regions[static_cast<std::size_t>(pi)].nation)] == cc.nation;
            cc.anchor_region_idx = same_nation ? pi : -1;
            if (!same_nation)
            {
                pi = -1;
                int best_d = std::numeric_limits<int>::max();
                for (std::size_t i = 0; i < settle->regions.size(); ++i)
                {
                    const region& rg = settle->regions[i];
                    if (rg.nation != cc.nation_idx)
                        continue;
                    const int d = charter_region_dist(cc.x, cc.y, rg.col, rg.row, cc.gw);
                    if (d < best_d)
                    {
                        best_d = d;
                        pi = static_cast<int>(i);
                    }
                }
            }
            cc.character_region_idx = pi;
        }
        // THE ORIGIN (BL-1099): the carve slot's region, else the fallbacks
        // the field documents. Read here, once, so both charter sites below
        // stamp the same answer on a centre's specialist and its firms.
        if (const auto slot = w.gen_carve_centres.find(cc.centre);
            slot != w.gen_carve_centres.end() && slot->second.region >= 0)
            cc.origin_region_idx = slot->second.region;
        else
            cc.origin_region_idx = cc.anchor_region_idx >= 0 ? cc.anchor_region_idx
                                                             : cc.character_region_idx;
        centres.push_back(std::move(cc));
    }

    // --- the spend order: budget DESCENDING, ties to the lower centre id -----
    std::vector<std::size_t> order(centres.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (centres[a].points != centres[b].points)
            return centres[a].points > centres[b].points;
        return centres[a].centre < centres[b].centre;
    });

    // Occupancy from every building already standing — the base world's
    // installations and nothing of the removed roster.
    std::unordered_set<entity_id> occupied;
    occupied.reserve(w.buildings.size() * 2);
    for (const auto& kv : w.buildings)
        occupied.insert(kv.second.tile);

    // BL-1146: each province's centre rung, read ONCE — the walk charters
    // buildings, never centres, so no rung moves under it.
    const std::map<uint32_t, int> province_rungs = province_centre_rungs(w);

    // BL-1185 / BL-1188: the reach every charter's processors are checked
    // against (Pass 3 "Chain-feasible"), its node set read once, here.
    chain_reach chain = make_chain_reach(w, reg);

    // BL-1232 (power plants per grid; PRODUCTION.md, "Generation is sized per
    // grid, not per body", Ben 2026-10-07): power's gap is read per wired grid
    // (`body_power_grid_gap`) and a power firm stands on a short grid. Only
    // where the band authors a power upkeep draw and some recipe makes power —
    // elsewhere the walk is the body-wide measure it was, byte for byte.
    const std::size_t power_i     = static_cast<std::size_t>(resource_type::power);
    const float       plant_output = one_power_plant_output(reg);
    const bool        power_per_grid = power_sized_per_grid(reg);
    // The walk's power measure, re-read per firm: the body-wide demand terms
    // other than upkeep (consumer, construction, processor inputs — all zero for
    // power in the shipped data), plus the per-grid gap over today's output, so
    // `demand - production` is exactly the grids' summed shortfall.
    // The grids a power firm has been CHARTERED on in this walk (the feed grid
    // of each of its generators): "powered" for unpowered-grids-first, live
    // output or not. Grid ids are world-unique, so one set serves every body.
    std::set<std::uint32_t> power_chartered;
    // The grids a centre's windows can FEED (the feed grid of every tile in its
    // two unfiltered rung windows), memoised per centre: markets and roads do
    // not move during the walk, and an unfiltered window is fixed.
    std::map<entity_id, std::set<std::uint32_t>> feedable_of;
    const auto size_power_per_grid =
        [&](entity_id body, const std::array<float, resource_count>& production,
            const std::array<float, resource_count>& consumer,
            const std::array<float, resource_count>& construction_need,
            const std::array<float, resource_count>& input_need,
            std::array<float, resource_count>& demand, std::set<std::uint32_t>& short_grids,
            const std::set<std::uint32_t>* reachable) {
            if (!power_per_grid)
                return;
            // `short_grids` comes back as the grids the firm may SERVE
            // (unpowered short grids first); the gap is every short grid's.
            const float gap = power_grids_to_serve(w, reg, body, plant_output, short_grids,
                                                   &power_chartered, reachable);
            demand[power_i] = production[power_i] + gap
                + (consumer[power_i] + construction_need[power_i] + input_need[power_i]);
        };

    // --- EACH BODY'S DENSITY RULE, FIXED BEFORE THE WALK (BL-1039) -----------
    // Every body holding a nation-resolved budgeted centre gets its state here,
    // before any charter lands, and three things are read ONCE:
    //
    //  * B, the body's points for FIRMS (Ben, 2026-09-18: B and B_ref in the
    //    same units): each nation-resolved centre's points net of the specialist
    //    price where it affords one, in whole firm charters
    //    (`charter_centre_firm_points`) — exactly what the walk below sets aside
    //    for that centre's firms, less the remainder no firm can be bought with.
    //    A `no_nation` centre's points can buy nothing on the body.
    //  * G, the GOODS WITH DEMAND on the body: every good whose demand is > 0
    //    across all three halves the gap selection below reads — the consumer
    //    baskets, building upkeep and construction. FIXED HERE, AND WHY: the
    //    selection re-measures upkeep and construction demand PER FIRM (BL-708,
    //    BL-709), because each firm it places draws upkeep and adds to the
    //    building stock. Read later, G would drift as the walk's own firms land
    //    — a mine that draws power would put power into G and raise the very
    //    reference the cap is measured against, mid-walk. Read now, it is the
    //    demand of the world the budget spends INTO (the base installations,
    //    the world-gen roster already removed), and B_ref cannot move under it.
    //    All three halves, not the consumer half alone, because the legacy cap
    //    spends 8 firms on an upkeep or construction good exactly as on a
    //    household one, and B_ref is what that cap would spend.
    //    THE COST OF FIXING IT (BL-1060): under `sqrt_capital` the turn serves
    //    G, so a good the walk's own firms make short — their upkeep draws a
    //    good no base installation did — gets no firm. That shortfall is real,
    //    so when it is all that is left a centre's rest is booked
    //    `late_shortfall`, never `no_gap` (which says there was none). The
    //    legacy rules pick from every good and have no such case.
    //    BL-1197 widens G past this: the processors' input demand (derived
    //    demand), the anchor-route input closure, and only goods the body can
    //    produce (`charter_fix_body_rules`). A short good the body CANNOT produce
    //    is out of G by that rule, and books `unproducible`, not
    //    `late_shortfall`.
    //  * B_ref = c x |G| x firm price and the per-good cap
    //    (`charter_sqrt_per_good_cap`: cap(B_ref) == c exactly, so a body whose
    //    firm spend is the legacy firm spend keeps the legacy cap), the density
    //    ceiling, and under `sqrt_capital` the TURN (see the selection below),
    //    the yards' places and the even share (NR-905, NR-906).
    // All of it is `charter_fix_body_rules`, the one function
    // `charter_spend_world_refusal` reads too.
    //
    // The consumer half is also what Pass 6 captures once per body; it reads
    // only the population centres, which no charter moves, so taking it here
    // rather than at the body's first firm reads the same numbers.
    // NR-913: the pooled remainders, planned now, before anything is chartered
    // (no pooling under the shipped `charter_pool::none`: every term below is 0).
    const charter_pool_plan pool = plan_charter_pool(w, budget, spend);
    for (charter_centre& cc : centres)
    {
        if (const auto o = pool.out.find(cc.centre); o != pool.out.end())
            cc.pool_out = o->second;
        if (const auto i = pool.in.find(cc.centre); i != pool.in.end())
            cc.pool_in = i->second;
    }
    rep.pool_transfers = pool.transfers;

    std::map<entity_id, charter_body_state> bodies;
    for (const charter_centre& cc : centres)
        if (cc.nation != null_entity)
        {
            const int64_t fpts = charter_centre_firm_points(cc.points, spend, cc.centre)
                               + charter_pool_whole(pool, cc.centre, spend);
            bodies[cc.body].firm_points   += fpts;
            bodies[cc.body].firm_charters += fpts / spend.firm_price_of(cc.centre);
        }
    // Specialists each body's centres can afford: the yards' places read them.
    std::map<entity_id, int64_t> specialists_on_body;
    for (const charter_centre& cc : centres)
        if (cc.nation != null_entity
            && static_cast<int64_t>(cc.points) >= spend.specialist_price_of(cc.centre))
            ++specialists_on_body[cc.body];
    for (auto& [body_id, bs] : bodies)
        charter_fix_body_rules(w, reg, spend, body_id, specialists_on_body[body_id], bs);

    const corporation_params capital_params;   // the draw: today's starting capital, 400 +/- 40%

    // `assets` is read BEFORE it is moved into the corporation, so the record
    // holds every holding's tile as placed (anchor first).
    auto record = [&](entity_id corp_id, const charter_centre& cc, bool specialist,
                      charter_rung rung, const std::vector<entity_id>& assets, int32_t price) {
        charter_record r;
        r.corp        = corp_id;
        r.centre      = cc.centre;
        r.specialist  = specialist;
        r.rung        = rung;
        r.anchor_tile = w.buildings.at(assets.front()).tile;
        r.price       = price;
        r.holdings.reserve(assets.size());
        for (const entity_id bid : assets)
            r.holdings.push_back(w.buildings.at(bid).tile);
        rep.charters.push_back(std::move(r));
        (specialist ? rep.specialists : rep.firms).push_back(corp_id);
        rep.points_spent += price;
        chartered.push_back(corp_id);
    };

    // --- ONE WALK (INDUSTRIALISATION.md § 1) ---------------------------------------
    // Each centre in spend order "charters exactly one specialist; what remains
    // buys background firms around it" — its specialist, then its firms, then
    // the next centre. So a richer centre's firms stand before a poorer centre's
    // specialist, and a centre's specialist stakes its ground before its own firms.
    for (const std::size_t oi : order)
    {
        charter_centre& cc = centres[oi];
        if (cc.nation == null_entity)
            continue;
        const nation_component& nc = w.nations.at(cc.nation);
        cc.points_after_specialist = cc.points;
        // BL-1168: this centre's prices, by its own trade reach.
        const int32_t fp               = spend.firm_price_of(cc.centre);
        const int64_t specialist_price = spend.specialist_price_of(cc.centre);

        // --- this centre's specialist, if it can afford one -------------------
        if (static_cast<int64_t>(cc.points) >= specialist_price)
        {
            const int32_t price = static_cast<int32_t>(specialist_price);   // <= points
            cc.points_after_specialist = cc.points - price;

            // Focus and ownership from the CHARACTER region, as Passes 2 and 2b
            // read them; with none, the national-character fallback
            // generate_corporations takes for a nation the settlement pass never
            // reached.
            industrial_focus focus;
            ownership_class  own;
            if (settle != nullptr && cc.character_region_idx >= 0)
            {
                const region& rg = settle->regions[static_cast<std::size_t>(cc.character_region_idx)];
                focus = focus_from_region(rg, settle->median_industrial_year);
                own   = ownership_from_region(rg, settle->charter, nc.politics);
            }
            else
            {
                focus = static_cast<industrial_focus>(static_cast<uint8_t>(nc.focus));
                own   = ownership_from_character(nc.politics);
            }

            std::mt19937 asset_rng = charter_stream(seed, k_charter_salt_spec_asset, cc.centre);
            charter_rung rung = charter_rung::centre_window;
            charter_unspent_reason why = charter_unspent_reason::window_exhausted;
            // BL-1188 (seat kit runs): the specialist's processors are given a
            // recipe whose inputs are produced within reach, or are not placed
            // (Pass 3 "Chain-feasible") — the seat is one of these.
            bool chain_rejected = false;
            // BL-1217 D6: the specialist's processors read the turn's want —
            // the body's captured consumer demand, the derived demand standing
            // now, and the refused draws' book (read only: the turn prunes it).
            output_want spec_want;
            const output_want* spec_want_p = nullptr;
            if (const auto bsit = bodies.find(cc.body); bsit != bodies.end())
            {
                const charter_body_state& sbs = bsit->second;
                spec_want = measure_output_want(w, reg, cc.body, sbs.consumer_demand);
                std::array<bool, resource_count> capped{};
                for (std::size_t r = 0; r < resource_count; ++r)
                    capped[r] = sbs.per_good_cap >= 0 && sbs.firms_by_resource[r] >= sbs.per_good_cap;
                raise_by_prospective(w, reg, chain, sbs.prospective, market_for_tile(w, cc.tile),
                                     spec_want.want, spec_want.production, capped);
                spec_want_p = &spec_want;
            }
            std::vector<entity_id> assets = charter_place(w, nc, focus, occupied, asset_rng, cc,
                                                          settle, spend, /*by_province=*/nullptr,
                                                          /*province_rungs=*/nullptr, rung, why,
                                                          reg, &chain, /*serve=*/nullptr,
                                                          chain_rejected, /*dig=*/nullptr,
                                                          /*refused=*/nullptr, /*grids=*/nullptr,
                                                          spec_want_p);
            if (assets.empty())
            {
                // No ground in either window: the specialist's price stays
                // unspent. Never an asset-light corp, never a nation-wide anchor.
                // The remainder still buys firms below.
                cc.unspent[static_cast<std::size_t>(why)] += price;
            }
            else
            {
                // THE CAPITAL IS TODAY'S DRAW (Ben, 2026-09-18): 400 +/- 40% with
                // the focus premium, CORPORATION_GENERATION.md Pass 4 as written.
                std::mt19937 capital_rng =
                    charter_stream(seed, k_charter_salt_spec_capital, cc.centre);
                const float capital = compute_capital(capital_params.base_capital,
                                                      capital_params.wealth_variance, focus,
                                                      capital_rng);
                std::mt19937 name_rng = charter_stream(seed, k_charter_salt_spec_name, cc.centre);

                corporation_component corp;
                corp.name             = make_corp_name(nc.name, name_rng);
                corp.home_nation      = cc.nation;
                corp.focus            = focus;
                corp.ownership_class  = own;
                corp.starting_capital = capital;
                corp.balance          = capital;
                corp.is_player        = false;
                corp.is_background    = false;
                // BL-1099: the origin is the centre's; the year is dated after
                // the walk against the record (`date_chartered_firms`).
                corp.origin_region    = cc.origin_region_idx;
                corp.founded_year     = 0;

                const entity_id home_body = corp_home_body(w, assets);
                const hq_designation hq   = designate_hq(w, assets, home_body);
                corp.hq_building     = hq.building;
                corp.influence_range = hq.range;

                const entity_id corp_id = w.create_entity();
                record(corp_id, cc, /*specialist=*/true, rung, assets, price);
                corp.assets = std::move(assets);
                w.corporations[corp_id] = std::move(corp);

                std::mt19937 stock_rng = charter_stream(seed, k_charter_salt_spec_stock, cc.centre);
                const auto stock = generate_starting_stockpile(focus, capital,
                                                               capital_params.base_capital,
                                                               stock_rng);
                if (home_body != null_entity) // BL-1265: placed on the shelves later
                    seed_opening_stock(w, corp_id, stock);
            }
        }

        // --- then what remains buys this centre's background firms -------------
        // NR-913: a pooling centre has sent its remainder to its group's richest
        // centre, which spends the group's here; both terms are 0 without pooling.
        const int64_t firm_budget =
            static_cast<int64_t>(cc.points_after_specialist) - cc.pool_out + cc.pool_in;
        if (firm_budget <= 0)
            continue;

        const int32_t n_firms  = static_cast<int32_t>(firm_budget / fp);
        const int32_t leftover = static_cast<int32_t>(firm_budget % fp);
        if (leftover > 0)
            cc.unspent[static_cast<std::size_t>(charter_unspent_reason::remainder)] += leftover;
        if (n_firms <= 0)
            continue;

        // Every nation-resolved centre's body was given its state before the
        // walk, the consumer half of its demand included (see above).
        charter_body_state& bs = bodies.at(cc.body);

        std::mt19937 asset_rng = charter_stream(seed, k_charter_salt_firm_asset, cc.centre);
        std::mt19937 name_rng  = charter_stream(seed, k_charter_salt_firm_name,  cc.centre);
        std::mt19937 stock_rng = charter_stream(seed, k_charter_salt_firm_stock, cc.centre);

        // NR-903 (the turn only): the goods skipped at THIS centre for the rest
        // of it, and the reason each skip named. Never set under a legacy rule,
        // whose first failed placement ends the centre.
        //
        // A PLACEMENT FAILS BY FOCUS, NOT BY GOOD (`charter_place` never sees the
        // good), and the ground only fills as the centre's firms land, so once a
        // focus has failed here every later good of that focus fails too.
        // `focus_failed` remembers it [extraction, processing], and a later good
        // of a failed focus is skipped without re-running the placement. Its
        // reason is still read on the ground as it stands then — `province_cap`
        // can decay to `window_exhausted` as the centre's own holdings occupy
        // the capped ground, never the reverse — so every skip names exactly
        // what the placement would have, and the walk is byte-identical to
        // attempting each good.
        std::array<bool, resource_count> skipped{};
        std::array<charter_unspent_reason, resource_count> skip_reason{};
        std::array<bool, 2> focus_failed{};
        // BL-1185: the goods `skipped` for want of a reachable input rather than
        // for want of ground — cleared, and retried, at this centre's next charter.
        std::array<bool, resource_count> chain_skipped{};
        std::array<charter_unspent_reason, 2> focus_reason{};
        // BL-1197 (gap firm digs the gap): the goods a digging firm found no
        // ground for here — this centre's windows hold none of its dig ladder's
        // ground (a Well or Wharf site, or a deposit of the good), and they only fill as firms land, so a later firm
        // for it would miss again. Passed over for the rest of this centre under
        // every rule (the turn also marks it skipped); a miss is the GOOD's, never
        // the focus's, so `focus_failed` is not set by it.
        std::array<bool, resource_count> dig_missed{};

        for (int32_t k = 0; k < n_firms; ++k)
        {
            const int32_t left = (n_firms - k) * fp;

            // The runaway guard first, so a runaway is always named as one; the
            // density ceiling sits below it (refused otherwise), so under
            // `sqrt_capital` the ceiling is what binds and the guard stays a
            // backstop. No legacy rule carries a ceiling.
            if (bs.firms >= spend.max_firms_per_body)
            {
                cc.unspent[static_cast<std::size_t>(charter_unspent_reason::body_cap)] += left;
                break;
            }
            if (bs.density_ceiling > 0 && bs.firms >= bs.density_ceiling)
            {
                cc.unspent[static_cast<std::size_t>(charter_unspent_reason::density_ceiling)] += left;
                break;
            }

            // --- THE GAP SELECTION, copied from generate_background_firms ----
            // (BL-708 upkeep and BL-709 construction re-measured per firm; the
            // per-resource cap masks; construction capacity provisioned first,
            // then the biggest absolute gap). See that body for the reasoning.
            //
            // THE PER-GOOD CAP is the body's, fixed before the walk (BL-1039):
            // `per_resource_firm_cap` under `fixed` (BL-1033's cap kept), none
            // under `lifted` (cap lifted — neither the mask below nor the yard's
            // cap test applies), the square-root rule's under `sqrt_capital`. The
            // yard's `want_yards` bound, the province cap and the body guard are
            // the same under every rule.
            //
            // THE ORDER: construction first under every rule; then biggest gap
            // first under the two legacy rules (Pass 6's, verbatim), or the TURN
            // under `sqrt_capital` (below), which alone skips a good it cannot
            // place (NR-903) and names a late shortfall (BL-1060).
            std::array<float, resource_count> production = {};
            accumulate_body_production(w, reg, cc.body, production);

            std::array<float, resource_count> demand = bs.consumer_demand;
            const std::array<float, resource_count> upkeep = body_upkeep_demand(w, reg, cc.body);
            for (std::size_t r = 0; r < resource_count; ++r)
                demand[r] += upkeep[r];
            const std::array<float, resource_count> construction_need =
                body_construction_demand(w, reg, cc.body);
            for (std::size_t r = 0; r < resource_count; ++r)
                demand[r] += construction_need[r];
            // BL-1197 round 4: derived demand, re-measured per firm (as upkeep).
            const std::array<float, resource_count> input_need =
                body_processor_input_demand(w, reg, cc.body);
            for (std::size_t r = 0; r < resource_count; ++r)
                demand[r] += input_need[r];
            // BL-1233 ruling A: and the draw of each firm the sized rule refused,
            // while its good is still wanted (not at its per-good cap, still short).
            {
                std::array<bool, resource_count> capped{};
                for (std::size_t r = 0; r < resource_count; ++r)
                    capped[r] = bs.per_good_cap >= 0 && bs.firms_by_resource[r] >= bs.per_good_cap;
                add_prospective_draws(w, reg, chain, bs.prospective, market_for_tile(w, cc.tile),
                                      demand, production, capped);
            }
            // BL-1232: power's gap is the short grids' summed shortfall, and
            // `short_grids` is where its firm may stand.
            std::set<std::uint32_t> short_grids;
            const std::set<std::uint32_t>* reachable = nullptr;
            if (power_per_grid)
            {
                auto fit = feedable_of.find(cc.centre);
                if (fit == feedable_of.end())
                {
                    std::set<std::uint32_t> fs;
                    for (const charter_rung rg : k_charter_rungs)
                        for (const entity_id t : charter_rung_window(w, nc, cc, settle, spend, rg))
                            if (const std::uint32_t fg = tile_feed_power_grid(w, t); fg != 0)
                                fs.insert(fg);
                    fit = feedable_of.emplace(cc.centre, std::move(fs)).first;
                }
                reachable = &fit->second;
            }
            size_power_per_grid(cc.body, production, bs.consumer_demand, construction_need,
                                input_need, demand, short_grids, reachable);

            std::array<float, resource_count> selectable = production;
            if (bs.per_good_cap >= 0)
                for (std::size_t r = 0; r < resource_count; ++r)
                    if (bs.firms_by_resource[r] >= bs.per_good_cap)
                        selectable[r] = std::max(selectable[r], demand[r]);
            for (std::size_t r = 0; r < resource_count; ++r)   // BL-1197
                if (dig_missed[r])
                    selectable[r] = std::max(selectable[r], demand[r]);

            // THE YARD, under every rule: construction capacity provisioned first,
            // bounded by `want_yards` and the per-good cap (Pass 6's step).
            const std::size_t cap_i = static_cast<std::size_t>(resource_type::construction_capacity);
            const int         ci    = best_construction_recipe(reg);
            bool yard_wanted = false;
            if (ci >= 0
                && (bs.per_good_cap < 0
                    || bs.firms_by_resource[cap_i] < bs.per_good_cap))
            {
                const float per_yard =
                    reg.recipe_at(building_type::processing_facility, ci).outputs[cap_i];
                const int want_yards =
                    (per_yard > 0.0f)
                        ? static_cast<int>(std::ceil(demand[cap_i] / per_yard))
                        : 0;
                yard_wanted = bs.firms_by_resource[cap_i] < want_yards
                           && demand[cap_i] > selectable[cap_i];
            }

            std::size_t gap_r    = resource_count;
            int         recipe_i = -1;
            // `skipped` is only ever set under the turn, so under a legacy rule
            // this is the yard step exactly as Pass 6 has it.
            if (yard_wanted && !skipped[cap_i])
            {
                gap_r    = cap_i;
                recipe_i = ci;
            }

            // THE TURN (`sqrt_capital`; Ben, 2026-09-18: "fill goods in turn: a
            // firm per good each pass, up to its cap; the ceiling trims every good
            // evenly"). Construction capacity keeps its step above exactly as it
            // is — provisioned FIRST, before the turn, bounded by `want_yards` and
            // the per-good cap — and is not a member of the turn: a yard never
            // takes a turn and never moves the cursor, so the yards are what the
            // provisioning bound says and nothing more. Every other good in G
            // takes one firm per pass, in ascending resource index, starting
            // where the last firm on this body left off: the first good at or
            // after the cursor that is under its cap AND still short (demand >
            // production, both re-measured this firm) gets it. A good at its cap,
            // or no longer short, is passed over. The order is a sorted vector and
            // an index — nothing depends on a container's layout.
            //
            // WHERE THE CEILING BINDS each good of the turn keeps an EVEN SHARE of
            // it (NR-905), and the share is a RESERVATION, not a cap (NR-906): a
            // good below its share takes a firm as ever; past its share only
            // while the room left in the ceiling still covers every other short
            // good's unfilled share, and never past its own per-good cap. So a
            // centre that has to skip the quarries cannot spend their places on
            // mills — the ore's reservation holds for a centre that has quarries
            // — while a good that stops being short releases what it did not use,
            // and the ceiling still fills. What a centre cannot spend waits: see
            // the share's gap after the walk.
            //
            // The good's firm serves THAT good: its recipe is the best recipe for
            // the good alone (the most output of it, ties to registry order), and
            // with none the firm is an extraction firm, exactly the legacy test.
            //
            // A GOOD THAT CANNOT BE PLACED IS SKIPPED, NOT FATAL (Ben, 2026-09-19,
            // NR-903; INDUSTRIALISATION.md § 1): when this centre's windows hold no
            // ground for its firm, the good is passed over for THE REST OF THIS
            // CENTRE and the turn moves on to the next, in this same firm. The
            // ground a firm needs is set by its FOCUS, not its good: an extraction
            // anchor takes whatever deposit its tile holds richest, so a window
            // with no free deposit tile OF ANY KIND — a dense city window often
            // has none — places no extraction good at all, and a window with no
            // free land places no works. The yard is a works and is skipped the
            // same way, so a centre with no room for one still charters the mines
            // it has ground for. Skipping for the rest of the centre is not a
            // shortcut: a failed placement moves nothing and draws nothing, and
            // the windows only fill as firms land, so a retry here could only fail
            // again (hence `focus_failed`, which skips the rest of a failed focus
            // without re-running it). The loop ends — every pass through it either
            // places, stops, or skips a good not skipped before — and THE CENTRE
            // STOPS ONLY WHEN NO GOOD IN THE TURN CAN PLACE: what is left is booked
            // `province_cap` if the cap took ground from any good still wanting a
            // firm, else `window_exhausted`.
            //
            // The cursor moves only when a firm is chartered, past the good just
            // served, so a good skipped at one centre waits for the turn to come
            // round again; the next centre on the body retries it then.
            //
            // When no good in the turn is short and under its cap (or share), the
            // rest is `no_gap` — unless a good OUTSIDE G is short, which only the walk's
            // own firms can have caused (their upkeep): the turn serves G alone,
            // so that is `late_shortfall`, a real shortfall left unserved (BL-1060).
            // A short good the body cannot produce at all is out of G by rule,
            // not by timing: `unproducible` (BL-1197 review).
            std::size_t            turn_at       = 0;
            bool                   from_turn     = false;
            bool                   go_processing = false;
            industrial_focus       focus         = industrial_focus::extraction;
            charter_rung           rung          = charter_rung::centre_window;
            std::vector<entity_id> assets;
            refused_draw           landed_refused;           // BL-1233: a landed firm's refused works
            bool                   stop          = false;
            bool                   chain_masked_any = false; // BL-1185, legacy rules only
            bool                   dig_missed_any = false;   // BL-1197, legacy rules only
            // BL-1197 review: the reason the dig misses named — `province_cap`
            // if the cap took the water ground of any of them, else
            // `window_exhausted` — kept, as a chain miss keeps its own.
            charter_unspent_reason dig_miss_why  = charter_unspent_reason::window_exhausted;
            for (;;)
            {
                if (gap_r == resource_count && bs.in_turn)
                {
                    // NR-906: what the shares still hold — every short good's
                    // unfilled share, max(0, share - firms held), re-read this
                    // firm — and the room the ceiling has left.
                    int64_t reserved = 0;
                    if (bs.even_share > 0)
                        for (const std::uint16_t h : bs.turn)
                            if (demand[h] > production[h] && bs.firms_by_resource[h] < bs.share[h])
                                reserved += bs.share[h] - bs.firms_by_resource[h];
                    const int64_t room = static_cast<int64_t>(bs.density_ceiling) - bs.firms;

                    const std::size_t n = bs.turn.size();
                    for (std::size_t step = 0; step < n; ++step)
                    {
                        const std::size_t at = (bs.turn_cursor + step) % n;
                        const std::size_t r  = bs.turn[at];
                        if (bs.firms_by_resource[r] >= bs.per_good_cap)
                            continue;
                        if (!(demand[r] > production[r]))
                            continue;
                        if (skipped[r])
                            continue;
                        // Past its own share only while the room left still
                        // covers every OTHER short good's reservation (its own is
                        // 0 by then): a share is a reservation, never a cap.
                        if (bs.even_share > 0 && bs.firms_by_resource[r] >= bs.share[r]
                            && room - 1 < reserved)
                            continue;
                        gap_r     = r;
                        turn_at   = at;
                        from_turn = true;
                        break;
                    }
                    if (gap_r == resource_count)
                    {
                        // No good in the turn can take a firm here. Name why.
                        bool unplaceable = false, capped = false, ground = false, gridless = false;
                        const auto still_wanted = [&](std::size_t r) {
                            if (!skipped[r])
                                return;
                            unplaceable = true;
                            if (skip_reason[r] == charter_unspent_reason::province_cap)
                                capped = true;
                            // BL-1232 review: a power skip for want of ground
                            // feeding a short grid books its own reason.
                            if (skip_reason[r] == charter_unspent_reason::no_short_grid_ground)
                                gridless = true;
                            // BL-1185: a skip for want of ground, not of a chain.
                            else if (skip_reason[r] != charter_unspent_reason::chain_infeasible)
                                ground = true;
                        };
                        for (const std::uint16_t r : bs.turn)
                            if (bs.firms_by_resource[r] < bs.per_good_cap && demand[r] > production[r])
                                still_wanted(r);
                        const bool turn_unplaceable = unplaceable;   // the yard holds no share
                        if (yard_wanted)
                            still_wanted(cap_i);

                        charter_unspent_reason stop_why = charter_unspent_reason::no_gap;
                        if (unplaceable)
                        {
                            stop_why = capped   ? charter_unspent_reason::province_cap
                                     : ground   ? charter_unspent_reason::window_exhausted
                                     : gridless ? charter_unspent_reason::no_short_grid_ground
                                                : charter_unspent_reason::chain_infeasible;
                            // NR-905: where the ceiling binds these points waited
                            // for shares this centre could not place; the walk's
                            // end decides how many of them are the share's gap.
                            if (bs.even_share > 0 && turn_unplaceable)
                            {
                                cc.firm_stop_points = left;
                                cc.firm_stop_reason = stop_why;
                            }
                        }
                        else
                        {
                            // A short good outside G: `late_shortfall` if the body
                            // can produce it (the walk's own firms made it short),
                            // `unproducible` if it cannot (BL-1197 review: out of G
                            // by rule, no firm here could ever serve it). A late
                            // shortfall outranks it: it is the one a firm could fill.
                            bool late = false, unproducible = false;
                            for (std::size_t r = 0; r < resource_count; ++r)
                            {
                                if (r == cap_i)   // the yard step serves it, in G or not
                                    continue;
                                if (std::binary_search(bs.goods.begin(), bs.goods.end(),
                                                       static_cast<std::uint16_t>(r)))
                                    continue;
                                if (bs.firms_by_resource[r] >= bs.per_good_cap)
                                    continue;
                                if (!(demand[r] > production[r]))
                                    continue;
                                if (bs.producible[r])
                                    late = true;
                                else
                                    unproducible = true;
                            }
                            if (late)
                                stop_why = charter_unspent_reason::late_shortfall;
                            else if (unproducible)
                                stop_why = charter_unspent_reason::unproducible;
                        }
                        cc.unspent[static_cast<std::size_t>(stop_why)] += left;
                        stop = true;
                        break;
                    }
                    std::array<float, resource_count> only = {};
                    only[gap_r] = demand[gap_r];
                    recipe_i = best_recipe_for_gaps(reg, production, only);
                }

                if (gap_r == resource_count)
                {
                    gap_r = biggest_gap_resource(selectable, demand);
                    if (gap_r == resource_count)
                    {
                        // Nothing on this body is short: the rest of this centre's
                        // budget has nothing to buy. BL-1185: unless a short good was
                        // masked for want of a reachable input — then the windows
                        // held ground and none of it could take the chain.
                        // BL-1197: or a short good had no ground to dig it.
                        cc.unspent[static_cast<std::size_t>(
                            chain_masked_any ? charter_unspent_reason::chain_infeasible
                            : dig_missed_any ? dig_miss_why
                                             : charter_unspent_reason::no_gap)] += left;
                        stop = true;
                        break;
                    }
                    recipe_i = best_recipe_for_gaps(reg, selectable, demand);
                }

                go_processing = (recipe_i >= 0)
                    && (reg.recipe_at(building_type::processing_facility, recipe_i).outputs[gap_r] > 0.0f);
                focus = go_processing ? industrial_focus::processing : industrial_focus::extraction;
                const std::size_t fi = go_processing ? 1 : 0;
                const std::map<uint32_t, int>* by_province =
                    spend.province_cap ? &bs.firms_by_province : nullptr;

                // --- placement: the centre's two windows and nothing wider --------
                charter_unspent_reason why = charter_unspent_reason::window_exhausted;
                // BL-1197 (gap firm digs the gap): a water-gap firm digs water at
                // a Well site, else an ice deposit; a produce-gap firm (BL-1208)
                // a produce deposit, else a Wharf site; and nothing else. Its
                // ground is not the focus's (a Well or Wharf needs no deposit, and
                // a Farm only a produce one), so an extraction
                // focus that failed here does not stand for it, and its own miss
                // does not mark the focus failed.
                resource_type dig_r = resource_type::water;
                const bool    digs  = gap_firm_digs(gap_r, go_processing, dig_r);
                if (bs.in_turn && focus_failed[fi] && !digs)
                {
                    // This focus already failed here and would fail again: no
                    // placement, only the reason, read now (see `focus_failed`).
                    // `window_exhausted` cannot turn back into `province_cap`.
                    if (focus_reason[fi] == charter_unspent_reason::province_cap)
                        focus_reason[fi] = charter_place_failure_reason(w, nc, focus, occupied, cc,
                                                                        settle, spend, by_province);
                    why = focus_reason[fi];
                }
                else
                {
                    // BL-1185 (chain-feasible placement): a firm for a good is
                    // chartered only where some recipe for that good has every
                    // input produced within reach — the gap selection's recipe
                    // first, then the good's other recipes by output — and an
                    // extraction firm's incidental processor takes the nearest
                    // feasible recipe or is unplaced (`make_chain_feasible`).
                    std::vector<int> serve;
                    if (go_processing)
                    {
                        serve.push_back(recipe_i);
                        for (const int i : chain_recipes_for_good(reg, gap_r))
                            if (i != recipe_i)
                                serve.push_back(i);
                    }
                    bool chain_rejected = false;
                    refused_draw refused;
                    // BL-1232: a power firm's windows are cut to the short grids.
                    const bool grid_sited = power_per_grid && gap_r == power_i;
                    // BL-1217 D6: the firm's processors read THIS firm's own
                    // measurement — the turn's demand and production, prospective
                    // draws included — so "short" is the gap that chose the good.
                    // Power want is the FULL short-grid set (`body_power_grid_gap`,
                    // via measure_power_grids), the key set every path reads: the
                    // turn's `short_grids` is narrowed to the unpowered reachable
                    // grids, which is where a power firm may STAND, not what is
                    // wanted.
                    output_want firm_want;
                    firm_want.body       = cc.body;
                    firm_want.want       = demand;
                    firm_want.production = production;
                    measure_power_grids(w, reg, firm_want);
                    assets = charter_place(w, nc, focus, occupied, asset_rng, cc, settle, spend,
                                           by_province, &province_rungs, rung, why,
                                           reg, &chain, go_processing ? &serve : nullptr,
                                           chain_rejected, digs ? &dig_r : nullptr, &refused,
                                           grid_sited ? &short_grids : nullptr, &firm_want);
                    if (!assets.empty())
                    {
                        // BL-1233 / BL-1217 D6 review: a firm that STANDS may still
                        // have had a later works refused by the sized rule; its
                        // draw is booked once the firm lands (below).
                        landed_refused = refused;
                        break;
                    }
                    if (chain_rejected && refused.set)
                    {
                        // BL-1233 ruling A: the refusal is a want of THIS selection
                        // too — the turn picks again from this firm's measurement,
                        // so the input that would feed the refused works is short
                        // now, not only at the next firm (a centre whose turn has
                        // nothing else short would otherwise stop before it). A
                        // good already in the book is in `demand` already.
                        //
                        // THE TURN ONLY (review round 3): the legacy rules mask a
                        // capped or missed good by lifting `selectable` to the
                        // demand measured before this raise, so raising it here
                        // could chart a capped input past its cap. Under them the
                        // refusal enters at the next firm, before the masks.
                        const bool fresh = bs.prospective.count(gap_r) == 0;
                        record_refused_draw(bs.prospective, gap_r, refused);
                        if (fresh && bs.in_turn)
                        {
                            input_reach_refresh(w, reg, chain);
                            const std::array<float, resource_count> wv = prospective_want(
                                w, reg, chain, bs.prospective.at(gap_r), market_for_tile(w, cc.tile));
                            raise_by_want(demand, production, wv);
                        }
                    }
                    if ((digs || grid_sited) && !chain_rejected)
                    {
                        // BL-1197: none of the good's dig-ladder ground in
                        // either window — a miss of the GOOD, not the focus. It is
                        // passed over for the rest of this centre and the same
                        // firm takes the next good: nothing is placed in its stead.
                        // BL-1232: so is a power firm whose windows hold no ground
                        // on a short grid — the GOOD's miss (a later good of the
                        // processing focus may still place), never the focus's.
                        dig_missed[gap_r] = true;
                        if (!bs.in_turn)
                        {
                            // The legacy rules mask it for this firm's selection
                            // and choose again, as a chain miss does.
                            selectable[gap_r] = std::max(selectable[gap_r], demand[gap_r]);
                            // BL-1232 review: the miss names the grid cut only
                            // while no other miss named a reason before it
                            // (province_cap outranks, a dig miss's ground too).
                            const bool first = !dig_missed_any;
                            dig_missed_any    = true;
                            if (why == charter_unspent_reason::province_cap)
                                dig_miss_why = charter_unspent_reason::province_cap;
                            else if (why == charter_unspent_reason::no_short_grid_ground && first)
                                dig_miss_why = charter_unspent_reason::no_short_grid_ground;
                            else if (why != charter_unspent_reason::no_short_grid_ground
                                     && dig_miss_why == charter_unspent_reason::no_short_grid_ground)
                                dig_miss_why = charter_unspent_reason::window_exhausted;
                            gap_r    = resource_count;
                            recipe_i = -1;
                            continue;
                        }
                    }
                    else if (chain_rejected)
                    {
                        // A failure of the GOOD, not of the focus: the windows
                        // held ground, none of it within reach of the good's
                        // chain. The focus is not marked failed — another good
                        // of it may place — and the good is passed over until
                        // this centre charters again (a new producer can bring
                        // it within reach).
                        if (!bs.in_turn)
                        {
                            // The legacy rules mask the good for this firm's
                            // selection and choose again (as the per-good cap
                            // masks: production lifted to demand).
                            selectable[gap_r] = std::max(selectable[gap_r], demand[gap_r]);
                            chain_masked_any  = true;
                            gap_r    = resource_count;
                            recipe_i = -1;
                            continue;
                        }
                        chain_skipped[gap_r] = true;
                    }
                    else
                    {
                        focus_failed[fi] = true;
                        focus_reason[fi] = why;
                    }
                }

                if (!bs.in_turn)
                {
                    // THE LEGACY RULES, verbatim: the selection, the windows and
                    // the province tallies are all unchanged by a failed placement
                    // (and it drew nothing), so every later firm here would fail
                    // identically: the rest of the budget is unspent, under the
                    // reason the placement named.
                    cc.unspent[static_cast<std::size_t>(why)] += left;
                    stop = true;
                    break;
                }

                // THE TURN (NR-903): skip this good for the rest of this centre and
                // pick again from the same measurement.
                skipped[gap_r]     = true;
                skip_reason[gap_r] = why;
                gap_r     = resource_count;
                recipe_i  = -1;
                from_turn = false;
            }
            if (stop)
                break;

            const entity_id anchor_tile = w.buildings.at(assets.front()).tile;
            const uint32_t anchor_province = w.provinces.province_of(anchor_tile);

            // The firm's processors already carry their recipes: `make_chain_feasible`
            // authored each one inside `charter_place` (BL-1185) — the firm's good by
            // the gap selection's preference, the first recipe feasible where that
            // processor stands.

            // The company, as Pass 6 authors it: open by construction (BL-678),
            // a stockpile from the BL-116 generator, and working capital priced from
            // it (BL-1173) once the stock is generated below.
            corporation_component corp;
            corp.name             = make_corp_name(nc.name, name_rng);
            corp.home_nation      = cc.nation;
            corp.focus            = focus;
            corp.ownership_class  = ownership_class::publicly_held;
            corp.starting_capital = 0.0f;
            corp.balance          = 0.0f;
            corp.is_player        = false;
            corp.is_background    = true;
            // BL-1099: as the specialist above -- the origin now, the year
            // after the walk (`date_chartered_firms`).
            corp.origin_region    = cc.origin_region_idx;
            corp.founded_year     = 0;

            const entity_id home_body = corp_home_body(w, assets);
            const hq_designation hq   = designate_hq(w, assets, home_body);
            corp.hq_building     = hq.building;
            corp.influence_range = hq.range;

            const entity_id corp_id = w.create_entity();
            record(corp_id, cc, /*specialist=*/false, rung, assets, fp);
            rep.charters.back().good = static_cast<std::uint16_t>(gap_r);
            corp.assets = std::move(assets);
            w.corporations[corp_id] = std::move(corp);
            ++bs.firms;
            ++bs.firms_by_resource[gap_r];
            bs.prospective.erase(gap_r); // BL-1233: its processor is placed
            // BL-1233 (BL-1217 D6 review): a later works of this firm the SIZED
            // rule refused (its good still short, its input's spare too thin)
            // leaves its prospective draw though the firm stands, so the walk
            // charters the extraction that would feed it. A works refused for
            // want leaves none (`make_chain_feasible`).
            if (landed_refused.set)
                record_refused_draw(bs.prospective, gap_r, landed_refused);
            // BL-1232: the grids this power firm now serves count as powered.
            if (power_per_grid && gap_r == power_i)
            {
                for (const entity_id a : w.corporations.at(corp_id).assets)
                {
                    const building_component& ab = w.buildings.at(a);
                    if (ab.type != building_type::processing_facility)
                        continue;
                    const recipe* arc = reg.get_recipe(ab.recipe);
                    if (arc == nullptr || !(arc->outputs[power_i] > 0.0f))
                        continue;
                    // Only a power-PRODUCING processor marks its grid: a power-gap
                    // firm that ended with none charters nothing on any grid.
                    if (const std::uint32_t fg = tile_feed_power_grid(w, ab.tile); fg != 0)
                        power_chartered.insert(fg);
                }
            }
            if (anchor_province != 0)
                ++bs.firms_by_province[anchor_province];
            if (from_turn)   // the pass moves on past the good that was just served
                bs.turn_cursor = (turn_at + 1) % bs.turn.size();
            // BL-1185: a good passed over for want of a reachable input is retried
            // now only if this charter's holdings produce one of its inputs — the
            // producer it may have lacked. An unrelated charter changes nothing.
            for (std::size_t r = 0; r < resource_count; ++r)
                if (chain_skipped[r]
                    && chain_firm_feeds_good(w, reg, w.corporations.at(corp_id).assets, r))
                {
                    chain_skipped[r] = false;
                    skipped[r]       = false;
                }

            if (home_body != null_entity)
            {
                const auto stock = generate_starting_stockpile(
                    focus, /*capital=*/0.0f, /*base_capital=*/0.0f, stock_rng);
                // The market its stock is priced at for working capital: its
                // home market there, else (none carved yet) the body itself.
                const entity_id home_mkt = corp_home_market(w, corp_id, home_body);
                const entity_id pool_key = (home_mkt != null_entity) ? home_mkt : home_body;
                seed_opening_stock(w, corp_id, stock); // BL-1265: placed on the shelves later

                // BL-1173: the firm opens with working capital priced from the
                // stock it was just handed (background_working_capital, above).
                // Set after the stock so the stock stream is drawn exactly as
                // before; no RNG is read here.
                corporation_component& opened = w.corporations.at(corp_id);
                opened.starting_capital = background_working_capital(w, pool_key, stock);
                opened.balance          = opened.starting_capital;
            }
        }
    }

    // --- NR-905: THE EVEN SHARE'S GAP ------------------------------------------
    // Where the ceiling binds, a centre that stopped because every good it could
    // still charter failed to place there — the others held back by the shares
    // still open — booked its rest under the placement's reason
    // (`firm_stop_points`). Only now is the body's gap known: the firms its
    // still-short turn goods lack of their shares at the end of the walk, within
    // the room the ceiling has left. That many firms' points move to
    // `share_unplaced`, drawn from those stops in spend order; the rest stays as
    // booked (it waited for a share a later centre filled, or for room the
    // ceiling never had). `share_unplaced` says a share went unfilled; it does
    // not claim no centre had ground for it (a centre might have, and run out of
    // points first). A booking moves between reasons within one centre, so every
    // balance holds.
    for (auto& [body_id, bs] : bodies)   // std::map: ascending body id
    {
        if (bs.even_share <= 0)
            continue;
        const int64_t room = std::max<int64_t>(0, static_cast<int64_t>(bs.density_ceiling) - bs.firms);
        if (room == 0)
            continue;
        std::array<float, resource_count> production = {};
        accumulate_body_production(w, reg, body_id, production);
        std::array<float, resource_count> demand = bs.consumer_demand;
        const std::array<float, resource_count> upkeep = body_upkeep_demand(w, reg, body_id);
        const std::array<float, resource_count> construction_need =
            body_construction_demand(w, reg, body_id);
        const std::array<float, resource_count> input_need =
            body_processor_input_demand(w, reg, body_id);
        for (std::size_t r = 0; r < resource_count; ++r)   // the walk's own sums, in its order
            demand[r] += upkeep[r];
        for (std::size_t r = 0; r < resource_count; ++r)
            demand[r] += construction_need[r];
        for (std::size_t r = 0; r < resource_count; ++r)
            demand[r] += input_need[r];
        std::set<std::uint32_t> short_grids;   // BL-1232: the walk's own power measure
        size_power_per_grid(body_id, production, bs.consumer_demand, construction_need,
                            input_need, demand, short_grids, nullptr);
        int64_t open = 0;
        for (const std::uint16_t r : bs.turn)
            if (demand[r] > production[r] && bs.firms_by_resource[r] < bs.share[r])
                open += bs.share[r] - bs.firms_by_resource[r];
        // BL-1168: the gap is counted in FIRMS and each stop gives up whole
        // firms at its own centre's price (its stop is whole firms at that
        // price). Where one price holds this is the old points arithmetic.
        int64_t gap_firms = std::min(room, open);
        for (const std::size_t oi : order)
        {
            if (gap_firms <= 0)
                break;
            charter_centre& cc = centres[oi];
            if (cc.nation == null_entity || cc.body != body_id || cc.firm_stop_points <= 0)
                continue;
            const int64_t cfp   = spend.firm_price_of(cc.centre);
            const int64_t firms = std::min<int64_t>(cc.firm_stop_points / cfp, gap_firms);
            const int32_t take  = static_cast<int32_t>(firms * cfp);
            cc.unspent[static_cast<std::size_t>(cc.firm_stop_reason)] -= take;
            cc.unspent[static_cast<std::size_t>(charter_unspent_reason::share_unplaced)] += take;
            gap_firms -= firms;
        }
    }

    // --- each body's rule and what the walk put on it (BL-1039) ---------------
    rep.cap_rule = spend.resource_cap_rule;
    for (const auto& [body_id, bs] : bodies)   // std::map: ascending body id
    {
        charter_body_record br;
        br.body              = body_id;
        br.firm_points       = bs.firm_points;
        br.firm_charters     = bs.firm_charters;
        br.goods_in_g = static_cast<int>(bs.goods.size());
        br.goods             = bs.goods;
        br.reference_points  = bs.reference_points;
        br.per_good_cap      = bs.per_good_cap;
        br.density_ceiling   = bs.density_ceiling;
        br.yard_places       = bs.yard_places;
        br.even_share        = bs.even_share;
        br.even_share_extra  = bs.even_share_extra;
        br.firms             = bs.firms;
        br.firms_by_good.assign(bs.firms_by_resource.begin(), bs.firms_by_resource.end());
        rep.bodies.push_back(std::move(br));
    }

    // --- the player: a seeded pick among the budget's specialists ------------
    // With none, nobody is picked and nothing forces one. The no-specialist
    // world (Ben, 2026-09-21, NR-910) never reaches here on the shipped seam:
    // `apply_landscape_candidate` lays the no-budget world when no centre
    // affords a specialist. What can still arrive is the RESIDUAL — a centre
    // afforded one and no specialist found ground — and it is reported here
    // (`no_specialists`) and counted by the instruments, never patched.
    std::sort(rep.specialists.begin(), rep.specialists.end());
    std::sort(rep.firms.begin(), rep.firms.end());
    std::sort(rep.charters.begin(), rep.charters.end(),
              [](const charter_record& a, const charter_record& b) { return a.corp < b.corp; });
    std::sort(chartered.begin(), chartered.end());
    if (rep.specialists.empty())
    {
        rep.no_specialists = true;
    }
    else
    {
        checkpoint_rng pick(seed ^ k_charter_salt_player, 0u);
        const entity_id seat = rep.specialists[static_cast<std::size_t>(
            pick.index(static_cast<int>(rep.specialists.size())))];
        w.corporations.at(seat).is_player = true;
        w.player_entity = seat;
        rep.player = seat;
    }

    // BL-1154 (Ben, 2026-10-01, NR-963 A): every chartered SPECIALIST but the
    // seat opens armed — a muster base beside its HQ and a 50-head unit
    // (`seed_starting_military`, MILITARY.md § "BL-476 rivals start armed").
    // Background firms stay unarmed. After the pick and every placement, so no
    // charter's ground or draw moves; the seeding draws no randomness.
    arm_rivals(w);

    // --- unspent, by (centre, reason) ----------------------------------------
    // `centres` is in budget-map order, i.e. ascending centre id, and the reasons
    // are walked in enum order, so the rows arrive sorted.
    for (const charter_centre& cc : centres)
        for (int r = 0; r < charter_unspent_reason_count; ++r)
            if (cc.unspent[static_cast<std::size_t>(r)] > 0)
            {
                rep.unspent.push_back({ cc.centre, static_cast<charter_unspent_reason>(r),
                                        cc.unspent[static_cast<std::size_t>(r)] });
                rep.points_unspent += cc.unspent[static_cast<std::size_t>(r)];
            }

    if (report != nullptr)
        *report = std::move(rep);
    invalidate_logistics_caches(w); // the reach legs warmed them (chain_reach)
    return chartered;
}

// ---------------------------------------------------------------------------
// BL-1099 — date_chartered_firms
// ---------------------------------------------------------------------------

void date_chartered_firms(world& w, charter_spend_report& report,
                          const std::vector<lapse_event>& events, int32_t epoch_year)
{
    // Per region: the years of its `works_chartered` notes, in record order
    // (ascending by year, as every event list is), and its first furnace year.
    // Ordered maps, read by key only -- nothing here iterates them.
    std::map<int, std::vector<int32_t>> notes;
    std::map<int, int32_t>              furnace;
    for (const lapse_event& e : events)
    {
        if (e.region == lapse_event_none)
            continue;
        const int region = static_cast<int>(e.region);
        const auto kind  = static_cast<lapse_event_kind>(e.kind);
        if (kind == lapse_event_kind::works_chartered)
            notes[region].push_back(e.year);
        else if (kind == lapse_event_kind::furnace_lit)
            furnace.try_emplace(region, e.year); // the FIRST crossing is the region's
    }

    // THE PAIRING, in the report's own order -- richest centre first, its
    // specialist then its firms (INDUSTRIALISATION.md sec Beat 1): a region's
    // k-th charter takes its k-th note; past the notes, its furnace year;
    // never lit, the epoch. A firm with no origin (none the search chartered)
    // opens at the epoch and is not counted against any region.
    //
    // "The report's own order" is ASCENDING CORP ID (`charter_spend_report::
    // charters`), which is the walk's order only because `world::create_entity`
    // hands out ids monotonically and the walk charters richest centre first,
    // each centre's specialist before its firms. A walk that ever allocated
    // out of that order would pair a region's notes with the wrong firms;
    // the dependency is named here so that change knows what it moves.
    std::map<int, std::size_t> taken;
    for (charter_record& r : report.charters)
    {
        const auto cit = w.corporations.find(r.corp);
        if (cit == w.corporations.end())
        {
            // A charter whose corp the world no longer holds still carries a
            // year on the report row, so a reader of the report never sees an
            // unset founding (the close's fill skips the row anyway).
            r.founded_year = epoch_year;
            continue;
        }
        corporation_component& c = cit->second;
        int32_t year = epoch_year;
        if (c.origin_region >= 0)
        {
            std::size_t& k = taken[c.origin_region];
            const auto nit = notes.find(c.origin_region);
            if (nit != notes.end() && k < nit->second.size())
                year = nit->second[k];
            else if (const auto fit = furnace.find(c.origin_region); fit != furnace.end())
                year = fit->second;
            ++k;
        }
        c.founded_year = year;
        r.founded_year = year;
    }
}

// ---------------------------------------------------------------------------
// BL-1268 — THE RETROFIT (TRADE.md § Trade in generation)
// ---------------------------------------------------------------------------

gen_trade_record gen_trade_record_from_history(const std::vector<trade_record_cell>& record,
                                               const trade_retrofit_params&          rates)
{
    gen_trade_record out;
    out.points_per_marketplace = rates.points_per_marketplace;
    out.max_per_market         = rates.max_per_market;
    out.cells.reserve(record.size());
    for (const trade_record_cell& c : record) // already sorted by (polity, region)
    {
        const std::int64_t pts = c.flow_volume * rates.flow_points_per_1000 / 1000
                               + c.relation_years * rates.relation_points_per_year;
        if (pts <= 0)
            continue;
        out.cells.push_back(gen_trade_cell{c.polity, c.region, pts, c.flow_volume, c.relation_years});
    }
    return out;
}

marketplace_retrofit_report retrofit_marketplaces(world& w, const recipe_registry& reg)
{
    marketplace_retrofit_report rep;
    const gen_trade_record& rec = w.gen_trade_record;
    const settlement_state* ss  = w.gen_settlement.get();
    for (const gen_trade_cell& c : rec.cells)
        rep.points_total += c.points;
    if (ss == nullptr || rec.cells.empty())
        return rep;

    const auto b = w.bodies.find(w.home_body);
    if (b == w.bodies.end())
    {
        rep.points_no_market = rep.points_total;
        return rep;
    }
    const int gw = b->second.grid_width, gh = b->second.grid_height;
    if (gw <= 0 || gh <= 0)
    {
        rep.points_no_market = rep.points_total;
        return rep;
    }

    // The home body's raster by grid position (stockpile_region_reach's idiom):
    // no iteration order of the tile map reaches the answer.
    const std::size_t cells = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    std::vector<entity_id> tile_at(cells, null_entity);
    for (const auto& [tid, t] : w.tiles)
    {
        if (t.body != w.home_body || t.grid_x < 0 || t.grid_y < 0 || t.grid_x >= gw || t.grid_y >= gh)
            continue;
        tile_at[static_cast<std::size_t>(t.grid_y) * static_cast<std::size_t>(gw)
                + static_cast<std::size_t>(t.grid_x)] = tid;
    }

    // 1. Points per market, by each cell's seat region's anchor tile.
    std::map<entity_id, std::int64_t> market_points;
    std::map<entity_id, std::pair<std::int64_t, std::int64_t>> market_raw; // (flow, relation)
    for (const gen_trade_cell& c : rec.cells)
    {
        entity_id m = null_entity;
        if (c.region >= 0 && static_cast<std::size_t>(c.region) < ss->regions.size())
        {
            const region& rg = ss->regions[static_cast<std::size_t>(c.region)];
            if (rg.col >= 0 && rg.row >= 0 && rg.col < gw && rg.row < gh)
            {
                const entity_id tid = tile_at[static_cast<std::size_t>(rg.row) * static_cast<std::size_t>(gw)
                                              + static_cast<std::size_t>(rg.col)];
                if (tid != null_entity)
                    m = market_for_tile(w, tid);
            }
        }
        if (m == null_entity)
            rep.points_no_market += c.points;
        else
        {
            market_points[m] += c.points;
            market_raw[m].first  += c.flow_volume;
            market_raw[m].second += c.relation_years;
        }
    }
    rep.markets_with_record = static_cast<int>(market_points.size());

    // 2. What each market's record buys.
    const std::int64_t price = rec.points_per_marketplace;
    const int          cap   = std::max(0, rec.max_per_market);
    std::map<entity_id, int> wanted; // market -> Marketplaces bought
    for (const auto& [m, pts] : market_points)
    {
        marketplace_retrofit_row row;
        row.market = m;
        row.points = pts;
        row.flow_volume    = market_raw[m].first;
        row.relation_years = market_raw[m].second;
        if (price > 0)
        {
            const std::int64_t whole = pts / price;
            row.bought = static_cast<int>(std::min<std::int64_t>(whole, cap));
            rep.points_below_one += pts - whole * price;
            rep.points_over_cap  += (whole - row.bought) * price;
        }
        else
            rep.points_below_one += pts;
        rep.bought += row.bought;
        if (row.bought > 0)
            wanted[m] = row.bought;
        rep.rows.push_back(row);
    }
    const auto row_of = [&](entity_id m) -> marketplace_retrofit_row& {
        return *std::lower_bound(rep.rows.begin(), rep.rows.end(), m,
                                 [](const marketplace_retrofit_row& r, entity_id k) { return r.market < k; });
    };
    // 3. The owner of each market's Marketplaces: the corporation with the
    //    most buildings in its catchment, every building by the market its
    //    tile clears against; corporations in ascending id, so a tie stays with
    //    the lower. Found for EVERY market with a record (bought or not), so a
    //    reading can say who the record would reach at another rate.
    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        corp_ids.push_back(kv.first);
    std::sort(corp_ids.begin(), corp_ids.end());
    std::map<entity_id, std::map<entity_id, int>> holdings; // market -> corp -> buildings
    for (const entity_id cid : corp_ids)
    {
        if (cid == w.player_entity)
            continue; // never added for the seat (TRADE.md § Trade in generation)
        for (const entity_id bid : w.corporations.at(cid).assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const entity_id m = market_for_tile(w, bit->second.tile);
            if (market_points.count(m) != 0)
                ++holdings[m][cid];
        }
    }
    for (marketplace_retrofit_row& row : rep.rows)
    {
        int best = 0;
        if (const auto hit = holdings.find(row.market); hit != holdings.end())
            for (const auto& [cid, cnt] : hit->second)
                if (cnt > best)
                {
                    best      = cnt;
                    row.owner = cid;
                }
        row.owner_holdings = best;
    }

    if (wanted.empty() || !reg.building_available(building_type::planetary_marketplace))
    {
        for (const auto& [m, n] : wanted)
            rep.no_site += n;
        return rep;
    }

    // 4. The catchment tiles of every market that bought, nearest its centre
    //    first. One raster walk; a tile holding any building is passed over.
    std::unordered_set<entity_id> occupied;
    for (const auto& [bid, bc] : w.buildings)
        occupied.insert(bc.tile);
    struct site { long long d2; entity_id tile; };
    std::map<entity_id, std::vector<site>> sites;
    std::map<entity_id, std::pair<int, int>> centre_xy;
    for (const auto& [m, n] : wanted)
    {
        const auto mit = w.markets.find(m);
        int cx = -1, cy = -1;
        if (mit != w.markets.end() && mit->second.centre_tile != null_entity)
            if (const auto ct = w.tiles.find(mit->second.centre_tile); ct != w.tiles.end())
            {
                cx = ct->second.grid_x;
                cy = ct->second.grid_y;
            }
        centre_xy[m] = {cx, cy};
    }
    for (std::size_t i = 0; i < cells; ++i)
    {
        const entity_id tid = tile_at[i];
        if (tid == null_entity || occupied.count(tid) != 0)
            continue;
        const entity_id m = market_for_tile(w, tid);
        const auto cit = centre_xy.find(m);
        if (cit == centre_xy.end())
            continue;
        const long long x = static_cast<long long>(i % static_cast<std::size_t>(gw));
        const long long y = static_cast<long long>(i / static_cast<std::size_t>(gw));
        long long d2 = 0;
        if (cit->second.first >= 0)
        {
            long long dx = std::llabs(x - cit->second.first);
            dx = std::min<long long>(dx, gw - dx); // columns wrap
            const long long dy = y - cit->second.second;
            d2 = dx * dx + dy * dy;
        }
        sites[m].push_back(site{d2, tid});
    }

    // 5. Author.
    const placement_gate gate = reg.placement_gate_for(building_type::planetary_marketplace, no_recipe);
    bool any = false;
    for (const auto& [m, n] : wanted)
    {
        marketplace_retrofit_row& row = row_of(m);
        const entity_id owner = row.owner;
        if (owner == null_entity)
        {
            rep.no_owner += n;
            continue;
        }
        std::vector<site>& cand = sites[m];
        std::sort(cand.begin(), cand.end(), [](const site& a, const site& b2) {
            if (a.d2 != b2.d2) return a.d2 < b2.d2;
            return a.tile < b2.tile;
        });
        int placed = 0;
        for (const site& s : cand)
        {
            if (placed >= n)
                break;
            if (occupied.count(s.tile) != 0)
                continue;
            if (!placement_rules::can_place_in_world(w, s.tile, building_type::planetary_marketplace,
                                                     resource_type::iron_ore, /*max_reach=*/-1.0f,
                                                     owner, gate))
                continue;
            const entity_id bid = author_building(w, s.tile, building_type::planetary_marketplace, occupied);
            w.corporations.at(owner).assets.push_back(bid);
            ++placed;
            any = true;
        }
        row.placed = placed;
        rep.placed  += placed;
        rep.no_site += n - placed;
    }
    if (any)
        invalidate_logistics_caches(w);
    return rep;
}

void print_marketplace_retrofit(const marketplace_retrofit_report& r)
{
    std::printf("[trade_retrofit] %lld points: %d markets with record, %d Marketplaces bought, "
                "%d placed (no owner %d, no site %d); unspent: no market %lld, below one %lld, "
                "over cap %lld\n",
                static_cast<long long>(r.points_total), r.markets_with_record, r.bought, r.placed,
                r.no_owner, r.no_site, static_cast<long long>(r.points_no_market),
                static_cast<long long>(r.points_below_one), static_cast<long long>(r.points_over_cap));
    std::fflush(stdout);
}
