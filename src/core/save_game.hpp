#pragma once

#include "ui/plot_history.hpp"
#include "ui/ui_state.hpp"
#include "world/hard_coded_world.hpp" // world_params, generation_report
#include "world/world.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// Save game -- the world snapshot plus its app-layer envelope (BL-536)
// ---------------------------------------------------------------------------
// `world_save.{hpp,cpp}` writes the WORLD. This writes a SAVE: that snapshot
// embedded whole, followed by the state that lives outside `world` and without
// which a load would open a campaign that is subtly not the one that was saved.
//
// The split is not bureaucratic. `world/*` must stay SDL-, ImGui- and Lua-free
// so the headless harnesses can link it; `ui_state.hpp` pulls in ImGui. Putting
// the envelope here is what keeps `save_roundtrip.cpp` able to exercise the
// world half on its own.
//
// FOUR PARTS, all named by Ben on 2026-08-22 when he took the full-envelope
// option over the narrow one:
//
//   1. The CLOCK -- sim/day/econ ticks, elapsed days, speed. Without it a load
//      resumes at tick 0 and every date on screen is wrong.
//   2. `world_params` -- the descriptor the world was built from, shown as "the
//      seed used", and the only record of what the player asked for.
//   3. The `generation_report` -- IN FULL. It is declared in
//      hard_coded_world.hpp as presentation data that "never enters `world`, so
//      it stays off the serialisation seam". That remains true of the WORLD
//      seam and is why it is here instead. Drop it and a loaded campaign opens
//      with a blank body biography, an empty Generation Ledger and a Continent
//      lens with no plates -- precisely the surfaces a design pass wants to
//      capture, which is what motivated this item.
//   4. The app-owned per-tick HISTORIES and a narrow `ui_state` slice.
//
// WHAT IS DELIBERATELY LEFT OUT, and why each is not an oversight:
//   - `m_last_econ_report` / `m_last_corp_standings` -- recomputed on the next
//      econ tick, and already documented on `app` as transient runtime caches.
//   - The BL-198 resource-deposit series, whose own comment in app.hpp says
//     "app-owned, capped, UNSERIALISED... survives the session only". Honouring
//     that beats quietly overturning it in a different item.
//   - Everything else on `ui_state` -- panel open/closed flags, paging indices,
//     drill stacks, hover and chat state. Transient view state has no business
//     surviving a load, and freezing 632 lines of it into a format would make
//     every UI tweak a save-format change.
// ---------------------------------------------------------------------------

/// Leading magic identifying a save game: the bytes 'I','O','S','G'.
/// Distinct from `world_save_magic` ('IOSV') because these are different
/// things: a caller handed the wrong one should be told so, not part-way
/// through reading it.
inline constexpr uint32_t save_game_magic =
    (uint32_t('I')) | (uint32_t('O') << 8) | (uint32_t('S') << 16) | (uint32_t('G') << 24);

/// Format version for the ENVELOPE. Independent of `world_save_version`: the
/// two halves change for different reasons, and a UI slice gaining a field
/// should not invalidate saves whose world half is unchanged. A load checks
/// both, so a mismatch in either still rejects the whole file.
///
/// Bumped to 2 when `continent_state` gained its `divergent` raster: the
/// continents record (`w_continents` / `r_continents` in save_game.cpp) gains
/// one byte-vector immediately after `convergent`, ahead of `history`. That is
/// a MID-RECORD gap, so a v1 stream's per-body continents record misreads
/// `history` and every envelope section after it — refused whole on the same
/// strict-equality contract `read_save_game` already applies, rather than
/// part-read into a campaign that looks valid and is not.
///
/// Refusal is the whole compatibility story here, by the format's own design:
/// `read_save_game` compares this constant for equality and rejects on any
/// mismatch. There is no upgrade path to write, and adding one for a single
/// raster would be inventing a scheme this file does not have.
inline constexpr uint32_t save_game_version = 3; // NR-733: the report carries the Era -1 time-lapse

/// Default extension for a save file. One place, so the CLI, the quick-save
/// binding and the verify API cannot disagree about it.
inline constexpr const char* save_game_extension = ".iosave";

/// Everything a save carries that does not live on `world`.
///
/// A plain aggregate rather than a class: it is a transport record between
/// `app` and the serialiser, and giving it behaviour would put app policy in a
/// file whose job is bytes.
struct save_envelope
{
    // --- the clock ----------------------------------------------------------
    uint64_t sim_tick     = 0;
    uint64_t day_tick     = 0;
    uint64_t econ_tick    = 0;
    double   elapsed_days = 0.0;
    /// Speed index as saved. Restored as-is rather than forced to paused --
    /// see `sim_loop::restore`.
    int      speed        = 0;

    /// The descriptor the live world was built from.
    world_params params{};

    /// Per-body Planetology results, the continents raster, the settlement
    /// pass and the tile-pass inputs the Generation Ledger regenerates from.
    generation_report report{};

    // --- app-owned per-tick histories ---------------------------------------
    std::vector<float>      balance_history;
    std::vector<float>      income_history;
    std::vector<float>      expenditure_history;
    ui::market_plot_history market_history;

    /// The Budget ledger's rank-change column reads a rolling window of
    /// per-building profit ranks. Held as a `std::deque` on `app` (it pushes
    /// one end and drops the other); flattened to a vector on the seam, because
    /// the container choice is an app implementation detail and the FORMAT
    /// should not inherit it. Order is oldest-first, as on `app`.
    std::vector<std::unordered_map<entity_id, int>> building_rank_hist;

    // --- the ui_state slice -------------------------------------------------
    // What the player was LOOKING AT: which rung, at what framing, with what
    // selected and which lens lit. Nothing else.
    canvas_level     primary_level     = canvas_level::solar;
    overlay_mode     overlay           = overlay_mode::none;
    entity_id        active_body       = null_entity;
    entity_id        selected_entity   = null_entity;
    uint32_t         selected_province = 0;

    float solar_zoom      = 1.0f, solar_pan_x = 0.0f, solar_pan_y = 0.0f;
    float circum_zoom     = 1.0f, circum_pan_x = 0.0f, circum_pan_y = 0.0f;
    float planetary_zoom  = 4.0f / 3.0f, planetary_pan_x = 0.0f, planetary_pan_y = 0.0f;
};

/// Write @p w and @p env to @p path as a complete save game.
///
/// Creates or truncates the file. Writes the IOSG header, then the embedded
/// world snapshot, then the envelope sections.
///
/// @param path Destination file path.
/// @param w    World to snapshot.
/// @param env  Envelope to write alongside it.
/// @return     True if the file was opened and written without a stream error.
bool write_save_game(const std::string& path, const world& w, const save_envelope& env);

/// Read a save game from @p path into @p w and @p env.
///
/// ATOMIC ON FAILURE, on both halves: `read_world_snapshot` already builds into
/// a scratch world, and the envelope is read into a scratch `save_envelope` and
/// moved out only on full success. A rejected load leaves BOTH arguments
/// exactly as they were, so a caller can offer the failure to the player
/// without having lost the campaign already in memory.
///
/// @param path Source file path.
/// @param w    Destination world, untouched unless the whole read succeeds.
/// @param env  Destination envelope, same guarantee.
/// @return     True if the save was read whole; false on any rejection.
bool read_save_game(const std::string& path, world& w, save_envelope& env);
