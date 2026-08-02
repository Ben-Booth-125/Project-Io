#pragma once

// ---------------------------------------------------------------------------
// Settlement & industrialisation — HISTORY.md Stages 3-4, made mechanical
// (BL-218 nations rewrite + BL-219's province read; docs/lore/HISTORY.md,
// docs/generation/NATION_GENERATION.md).
//
// The ladder (BL-221) counted how many independent peoples the land supported.
// The creeds (BL-235) gave each of them a tongue and a pantheon. This pass is
// the rung between those and the political map: it settles PROVINCES, carries
// each one's founding CULTURE forward, industrialises the ones the ground can
// pay for, and then hands the political pass its seeds.
//
// THREE THINGS THIS PASS IS FOR:
//
//  1. BELIEF MAPPED ONTO GROUND. A province is not a blob with a name; it is
//     a place a named people settled, and it carries their gods. The culture
//     it inherits is the nearest cradle's, so the pantheon distribution is a
//     map of who walked where. A province in an ancient ore window whose
//     culture raised a FORGE god industrialises earlier than one that did not
//     — the creed and the deposit are the same historical fact seen twice.
//
//  2. CHARACTER AS AN OUTPUT, NOT A DRAW. NATION_GENERATION's Pass 4 drew
//     ideology/expansionism/economic_focus from an independent RNG. Here each
//     axis derives from one quantity this pass produced (BL-218, settled
//     2026-08-02): expansionism <- the border-contest integral, economic_focus
//     <- the dominant resource class of the provinces settled DURING
//     industrialisation, ideology <- industrialisation timing against
//     neighbours.
//
//  3. THE RECORD IS NOT SAFE. Wars redraw borders, spread the victor's gods
//     over the loser's provinces, and ERASE part of the loser's history —
//     replaced by a dated lacuna, not silently dropped (Ben, 2026-08-02:
//     "don't be afraid to have parts of the record erased when two nations go
//     to war"). A history that cannot be damaged is a history nobody fought
//     over.
//
// THE PASS DRIVES, IT DOES NOT NARRATE — the BL-221/BL-235 rule, inherited.
// Provinces are placed BEFORE the political map and become the nation seeds;
// the BFS/growth machinery BL-053 tuned is kept and only its INPUTS change
// (BL-218 § 1: "Seeding changes. Expansion does not.").
//
// DETERMINISM. Integer scoring with explicit index tie-breaks, fresh stage
// tags, every container walk in raster or sorted-id order, no transcendentals.
// The rupture checkpoints reuse planetology.hpp's class-agnostic
// `resolve_checkpoint` rather than inventing a second branch mechanism —
// BL-217 named this pass as its intended second client.
// ---------------------------------------------------------------------------

#include "creeds.hpp"
#include "history_ladder.hpp"
#include "planetology.hpp"
#include "world.hpp"

#include <cstdint>
#include <string>
#include <vector>

/// What a province's ground is mainly good for — the ANCIENT endowment, read
/// once from the deposits under it and never re-read. This is the quantity
/// BL-218 asks economic_focus to derive from and BL-219 asks corporate focus
/// to derive from, so it is deliberately coarse and shared.
enum class province_class : uint8_t
{
    none = 0,  ///< Nothing worth naming; a subsistence province.
    farm,      ///< Agricultural produce dominates.
    ore,       ///< Iron/copper/rare-earth — the forge god's country.
    energy,    ///< Coal and petroleum: the Stage 4 fuel that breaks the ceiling.
    port,      ///< Coastal, deposit-poor: it lives on what passes through.
};

/// One settled region — the unit of settlement history, and the unit BL-219
/// reads a corporation's focus from.
struct province
{
    int anchor = -1;        ///< Raster index (row * gw + col) of the core tile.
    int col = 0;
    int row = 0;

    /// Index into `creed_state::cultures` — WHOSE GODS this province keeps.
    /// Starts as the nearest cradle's and can be overwritten by conquest.
    int culture = -1;
    /// The culture that FOUNDED it. Never overwritten, so a conquered province
    /// still records who built it — the erasure is of the record, not of this.
    int founding_culture = -1;
    /// True once a conqueror's pantheon replaced the founders' here.
    bool creed_conquered = false;

    std::string name;       ///< "<people>'s <region>", in the founders' tongue.

    int settle_score_q = 0; ///< 0-1000 — how strongly the ground invited settlement.

    /// Ancient endowment windows, 0-1000, surveyed once over the province's
    /// neighbourhood. These are the deposits that were already in the ground
    /// before anybody arrived; industrialisation spends them.
    int farm_q = 0;
    int ore_q = 0;
    int energy_q = 0;
    int port_q = 0;
    province_class dominant = province_class::none;

    int64_t founded_year = 0;     ///< Calendar year settled (negative = before epoch year 0).
    int64_t industrial_year = 0;  ///< Calendar year the furnaces lit; 0 when never.
    bool    industrialised = false;

    int nation = -1;   ///< Index into the nation-id list, once the political pass has run.
    int contest_q = 0; ///< 0-1000 — how hard this province's frontier was pressed.
};

/// What the settlement pass computed for one body.
struct settlement_state
{
    std::vector<province> provinces;      ///< In placement order (best ground first).
    std::vector<history_event> history;   ///< Dated settlement / furnace / war lines.
    std::vector<checkpoint_record> checkpoints; ///< Rupture branch decisions (BL-217 shape).

    /// How many history lines the wars destroyed. Every one is replaced by a
    /// dated lacuna line, so this is a count of holes the player can SEE.
    int lacunae = 0;

    /// The world-median industrialisation year over industrialised provinces,
    /// or 0 when none industrialised. BL-219's "early vs late" pivot reads it.
    int64_t median_industrial_year = 0;
};

/// Settle the body: place provinces, inherit each one's cradle culture, survey
/// its ancient endowment, and industrialise the ones the ground can pay for.
///
/// Pure deterministic function of its inputs. Must run AFTER `run_creeds` /
/// `record_tribal_conflict` (it reads the surviving cultures) and BEFORE
/// `generate_nations` (it supplies the seeds).
///
/// @param pl        Body planetology state (arable share, endemics).
/// @param hl        The ladder — cradle positions, fragmentation, charter cradle.
/// @param cs        The creeds — one culture per cradle, with its pantheon.
/// @param w         World holding the tiles. Read-only.
/// @param tile_ids  Raster-order tile IDs (index = row * gw + col).
/// @param gw, gh    Grid dimensions (columns wrap; rows do not).
/// @param target_provinces  How many provinces to aim for; the political pass's
///                  own seed budget, so the two agree by construction. Clamped
///                  to what the land can actually hold at the separation rule.
/// @param seed      Per-body seed, already folded with the campaign seed.
settlement_state run_settlement(const planetology_state& pl,
                                const history_ladder_state& hl,
                                const creed_state& cs,
                                const world& w,
                                const std::vector<entity_id>& tile_ids,
                                int gw, int gh,
                                int target_provinces,
                                uint32_t seed);

/// The province anchors, as raster indices, in placement order — the seed list
/// `nation_params::seed_tiles` takes. This is the whole of "seeding changes,
/// expansion does not": the Voronoi/BFS growth machinery is untouched, it just
/// starts from places people actually settled.
std::vector<int> settlement_seed_tiles(const settlement_state& ss);

/// Attribute every province to the nation that ended up holding it, compute the
/// border-contest integral, and DERIVE the three political axes from the
/// settlement record (BL-218 § 2). Overwrites whatever Pass 4 drew.
///
/// Appends the Stage 4 industrialisation lines, which can only be attributed to
/// a nation once one exists — the same two-entry-point shape the ladder uses.
///
/// @param nation_ids  Nation entity ids in generation order (generate_nations'
///                    return value); provinces store indices into this list.
void derive_national_character(settlement_state& ss,
                               const creed_state& cs,
                               world& w,
                               const std::vector<entity_id>& nation_ids,
                               const std::vector<entity_id>& tile_ids,
                               int gw, int gh);

/// The historical-rupture checkpoint class (BL-218 § 2a) — collapse, war and
/// revolution as TRANSFORMS on nation state, never as narration alone.
///
///  * **Collapse** — the nation's peripheral provinces are lost to their
///    nearest neighbour and their industrial clock resets; abundance falls.
///  * **War** — the contested border redraws toward the stronger; the loser's
///    expansionism rises (grievance); the victor's gods are planted on the
///    provinces taken; and PART OF THE LOSER'S RECORD IS DESTROYED, replaced
///    by a dated lacuna.
///  * **Revolution** — territory untouched; the ideology axis flips; abundance
///    takes a one-off hit.
///
/// Branch eligibility is a FILTER, never a weight (BL-217's rule): a nation
/// with no land neighbour cannot go to war, a single-province nation cannot
/// collapse. Every attempt appends a `checkpoint_record`, failures included.
///
/// Mutates @p ss (records, redaction, province culture) and @p w (tile
/// ownership, nation character, resource abundance).
void resolve_historical_ruptures(settlement_state& ss,
                                 const creed_state& cs,
                                 world& w,
                                 const std::vector<entity_id>& nation_ids,
                                 const std::vector<entity_id>& tile_ids,
                                 int gw, int gh,
                                 uint32_t seed);

/// Index of the province whose anchor is nearest (col,row), or -1 when there
/// are none. Column-wrapped; ties break on the lowest province index.
int nearest_province(const settlement_state& ss, int col, int row, int gw);

/// BL-219's derivation: a corporation anchored in @p p operates at the tier its
/// province's history earned. The ancient endowment sets the base class; the
/// **movement up the value chain** — early industrialisers refined, late ones
/// stayed raw — shifts it one tier when the province industrialised before the
/// world median.
///
/// @param p        The corporation's home province.
/// @param median   `settlement_state::median_industrial_year` (0 = nobody did).
industrial_focus focus_from_province(const province& p, int64_t median);
