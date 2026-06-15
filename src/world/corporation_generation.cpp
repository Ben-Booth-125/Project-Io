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
    switch (nc.economic_focus)
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

/// Place one starting building for a corporation inside its home nation's tiles.
/// Returns the entity id of the created building, or null_entity if no suitable
/// tile could be found.
///
/// Placement rules:
///   extraction   → prefer highest-deposit land tile (non-ocean).
///   processing   → prefer a land tile adjacent to or within extraction clusters;
///                  in the prototype falls back to any non-ocean land tile.
///   trade        → prefer low-deposit coastal-adjacent tiles or any land tile.
///
/// Collision: never places on a tile already in `occupied_tiles`.
///
/// @param w              World to write the new entities into.
/// @param home_nation    The nation_component of the home nation.
/// @param focus          Corporate industrial focus (determines building_type and tile preference).
/// @param occupied_tiles Set of tile entity ids already occupied by another building.
/// @param rng            Seeded RNG for tie-breaking.
/// @return               Entity id of the placed building, or null_entity.
entity_id place_starting_asset(world& w,
                                const nation_component& home_nation,
                                industrial_focus focus,
                                std::unordered_set<entity_id>& occupied_tiles,
                                std::mt19937& rng)
{
    if (home_nation.tiles.empty())
        return null_entity;

    // Determine which building_type to place.
    building_type btype = building_type::none;
    switch (focus)
    {
        case industrial_focus::extraction:  btype = building_type::extraction_site;     break;
        case industrial_focus::processing:  btype = building_type::processing_facility; break;
        case industrial_focus::trade:       btype = building_type::port;                break;
    }

    // Score every tile in the home nation; skip ocean and already-occupied tiles.
    // Score semantics differ by focus:
    //   extraction — higher total deposit is better.
    //   processing — lower deposit (near but not on the richest spots) is fine;
    //                use a moderate score based on deposit / hazard.
    //   trade      — neutral; score = 1 everywhere land is available, so the
    //                pick is effectively random among valid tiles.

    struct candidate
    {
        entity_id tid;
        float     score;
    };
    std::vector<candidate> candidates;
    candidates.reserve(home_nation.tiles.size());

    for (entity_id tid : home_nation.tiles)
    {
        if (occupied_tiles.count(tid))
            continue;

        const auto it = w.tiles.find(tid);
        if (it == w.tiles.end())
            continue;

        const tile_component& tc = it->second;
        if (placement_rules::is_ocean_tile(tc.composition))
            continue;

        float score = 1.0f;
        switch (focus)
        {
            case industrial_focus::extraction:
                // Rank by *extractable* deposit so an extraction site lands on a
                // tile it can actually work (S1 placement fix). The tiny floor keeps
                // a degenerate nation with no extractable tiles still placeable.
                score = placement_rules::extractable_deposit(tc) + 0.001f;
                break;
            case industrial_focus::processing:
                // Prefer tiles with some deposit but not the absolute maximum.
                // A moderate positive score keeps candidates reasonably uniform.
                score = total_deposit(tc) * 0.5f + 1.0f;
                break;
            case industrial_focus::trade:
                // Trade buildings just need any land tile.
                score = 1.0f;
                break;
        }

        // Penalise very hazardous tiles slightly so we avoid volcanic hellscapes.
        score *= (1.0f - tc.hazard_level * 0.3f);

        candidates.push_back({ tid, score });
    }

    if (candidates.empty())
        return null_entity;

    // Weighted random sample: compute cumulative weights, draw uniformly.
    float total_w = 0.0f;
    for (const auto& c : candidates)
        total_w += c.score;

    entity_id chosen = null_entity;
    if (total_w > 0.0f)
    {
        std::uniform_real_distribution<float> draw(0.0f, total_w);
        float cursor = draw(rng);
        for (const auto& c : candidates)
        {
            cursor -= c.score;
            if (cursor <= 0.0f)
            {
                chosen = c.tid;
                break;
            }
        }
        if (chosen == null_entity)
            chosen = candidates.back().tid; // floating-point fallback
    }
    else
    {
        // All scores are zero (degenerate world); pick uniformly.
        std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
        chosen = candidates[pick(rng)].tid;
    }

    // Create the building entity and populate components.
    const entity_id bld_id = w.create_entity();

    building_component bc;
    bc.tile               = chosen;
    bc.type               = btype;
    // Staff producing buildings so the Layer 3 economy actually runs from the
    // authored starting assets (an unstaffed building produces nothing). Ports
    // take no production action in L3, so they stay unstaffed.
    bc.workforce_assigned = (btype == building_type::port) ? 0.0f : 0.5f;

    // Author the extraction target from the tile's richest extractable deposit.
    // The processing recipe is authored later from the loaded registry (the
    // recipe id is a registry index, not known here); it stays no_recipe for now.
    if (btype == building_type::extraction_site)
    {
        const auto tit = w.tiles.find(chosen);
        if (tit != w.tiles.end())
        {
            bool any = false;
            const resource_type tgt = placement_rules::richest_extractable(tit->second, any);
            bc.target_resource = tgt; // defaults to iron_ore when the tile has none
        }
    }
    w.buildings[bld_id]  = bc;

    stockpile_component sc;
    w.stockpiles[bld_id] = sc;

    // Mark tile as occupied so later corps do not land here.
    occupied_tiles.insert(chosen);

    return bld_id;
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
            ? it->second.economic_focus
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

    // corp_assets[c] = building entity id placed for corp c (or null_entity).
    std::vector<entity_id> corp_assets(static_cast<std::size_t>(corp_count), null_entity);

    for (int c = 0; c < corp_count; ++c)
    {
        const entity_id home_nid = nation_ids[static_cast<std::size_t>(
            home_nation_idx[static_cast<std::size_t>(c)])];

        const auto it = w.nations.find(home_nid);
        if (it == w.nations.end())
            continue;

        const entity_id bld_id = place_starting_asset(
            w, it->second,
            corp_focuses[static_cast<std::size_t>(c)],
            occupied_tiles,
            asset_rng);

        corp_assets[static_cast<std::size_t>(c)] = bld_id;
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

        const entity_id asset = corp_assets[static_cast<std::size_t>(c)];
        if (asset != null_entity)
            cc.assets.push_back(asset);

        const entity_id corp_id = w.create_entity();
        corp_ids.push_back(corp_id);
        w.corporations[corp_id] = std::move(cc);
    }

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
