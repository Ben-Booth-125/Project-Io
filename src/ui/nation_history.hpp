#pragma once

#include "world/entity.hpp"
#include "world/hard_coded_world.hpp" // generation_report::body_entry

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// nation_history -- a nation's HISTORY, read off the generation report
// (BL-1295; STARTUP.md § The seat, GENERATION_STRATEGY.md § History patterns)
// ---------------------------------------------------------------------------
//
// The nation selection card's seventh part and the History ledger's nation
// filter read ONE definition of "this nation's history" from here, so the card
// and the ledger it links to can never disagree about which realms are the
// nation's:
//
//   * its LINEAGE -- the realm the nation is (the polity fold's
//     `nation_polity`), the realms the size floor folded into it
//     (`nation_absorbed`), and the line each of those broke away from
//     (`broke_away` / `schism` / `seceded_at_close` parents, walked back across
//     the three records -- a polity id is one table across them). "A successor
//     carries its parent line's" (Ben, 2026-10-12) is the tag rule; the card
//     tells the same line.
//   * its PATTERN TAGS -- `body_entry::nation_pattern_tags` (BL-1286), labelled.
//   * a short TIMELINE of key events -- the lapse records' own events whose
//     subject is in the lineage, phrased from the records' own name tables.
//
// READ-ONLY AND RECORD-ONLY. Everything is folded out of what the report
// already carries; no new sim state, no re-run, nothing written.

namespace ui {

/// The nation's slot in the homeworld entry's fold arrays, or -1.
int nation_fold_slot(const generation_report::body_entry& be, entity_id nation);

/// The homeworld's report entry (the one carrying the fold record), or null.
const generation_report::body_entry* homeworld_entry(const generation_report& rep);

/// The nation's lineage: sorted, unique polity ids. Empty for a nation of
/// ground no realm held, or with no fold record.
std::vector<uint16_t> nation_lineage(const generation_report::body_entry& be, entity_id nation);

/// The pattern tags, as on-screen words joined by ", " ("rose and fell,
/// world war"), or "" when the nation carries none.
std::string nation_pattern_tags_text(const generation_report::body_entry& be, entity_id nation);

/// One timeline row: its year and its prose.
struct nation_history_row
{
    int32_t     year = 0;
    std::string text;
};

/// Up to @p max_rows key events of the nation's lineage, oldest first.
/// Foundings, secessions, subjection and freedom, closing-round absorptions,
/// revolutions and champion charters come first; conquests fill what is left.
std::vector<nation_history_row> nation_timeline(const generation_report::body_entry& be,
                                                entity_id nation, std::size_t max_rows = 8);

} // namespace ui
