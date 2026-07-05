#include "corporation_generation.hpp"

#include "world/placement_rules.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
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
    case industrial_focus::trade:      return { 1, 2 };
    }
    return { 1, 2 }; // defensive default — keep a corp a going concern
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
    static const std::vector<building_type> extraction_mix = {
        building_type::extraction_site,     // anchor
        building_type::extraction_site,
        building_type::extraction_site,
        building_type::processing_facility, // consumes some own output
    };
    static const std::vector<building_type> processing_mix = {
        building_type::processing_facility, // anchor
        building_type::processing_facility,
        building_type::extraction_site,     // feeds the processors
        building_type::extraction_site,
    };
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

/// Score one tile for hosting a building of `btype`, higher is better. Mirrors the
/// per-focus preference the single-asset placer used: extraction wants the richest
/// *extractable* deposit (a site must be able to work its tile), processing/port
/// want any workable land but lean mildly toward some deposit nearby; every type
/// shies away from hazardous tiles. Returns a strictly positive score for any
/// non-ocean tile so a candidate is always placeable.
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
        case building_type::port:
        case building_type::none:
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
    // action in L3, so they stay unstaffed.
    bc.workforce_assigned = (btype == building_type::port) ? 0.0f : 0.5f;

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
/// @param w              World to write the new entities into.
/// @param home_nation    The nation_component of the home nation.
/// @param focus          Corporate industrial focus (determines the asset mix and tile preference).
/// @param occupied_tiles Tiles already taken by another building; never reused.
/// @param rng            Seeded RNG for the anchor weighted draw and holdings count.
/// @return               Entity ids of every building placed (may be empty for a
///                       degenerate nation with no usable land).
std::vector<entity_id> place_starting_assets(world& w,
                                             const nation_component& home_nation,
                                             industrial_focus focus,
                                             std::unordered_set<entity_id>& occupied_tiles,
                                             std::mt19937& rng)
{
    std::vector<entity_id> placed;
    if (home_nation.tiles.empty())
        return placed;

    const std::vector<building_type>& pattern = focus_asset_pattern(focus);
    const building_type anchor_type = pattern.front();

    // --- choose the anchor tile (weighted by anchor-type score) ---------------
    // Candidates are the nation's non-ocean, unoccupied tiles that can validly host
    // the anchor type. The nation's `tiles` vector is stored in a stable order, so
    // building the candidate list by iterating it is deterministic.
    struct candidate { entity_id tid; float score; };
    std::vector<candidate> anchors;
    anchors.reserve(home_nation.tiles.size());
    for (entity_id tid : home_nation.tiles)
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
        return placed; // no valid anchor in this nation — corp opens asset-light

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

} // namespace

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

std::vector<entity_id> generate_corporations(
    world& w,
    const corporation_params& params,
    uint32_t seed)
{
    if (w.nations.empty())
        return {};

    // Distinct xor-offset seeds keep each pass's RNG stream independent,
    // mirroring the pattern used in nation_generation.cpp.
    const uint32_t seed_assign  = seed ^ 0xB1C2D3E4u;
    const uint32_t seed_focus   = seed ^ 0x2F3E4D5Cu;
    const uint32_t seed_asset   = seed ^ 0x9D8C7B6Au;
    const uint32_t seed_capital = seed ^ 0x5A4B3C2Du;
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

        corp_assets[static_cast<std::size_t>(c)] = place_starting_assets(
            w, it->second,
            corp_focuses[static_cast<std::size_t>(c)],
            occupied_tiles,
            asset_rng);
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
    }

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

        cc.assets = std::move(corp_assets[static_cast<std::size_t>(c)]);

        const entity_id corp_id = w.create_entity();
        corp_ids.push_back(corp_id);
        w.corporations[corp_id] = std::move(cc);
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
    }

    return corp_ids;
}
