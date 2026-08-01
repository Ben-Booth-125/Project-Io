-- Reusable verify-script helpers — Phase 2 of the visual-verification harness.
--
-- The harness (app::run_verify) auto-loads this file from its own directory before
-- running a verify script, so these helpers are available as globals with no
-- `require` (the Lua `package` library is not opened). They layer high-level,
-- parameterised operations over the low-level `verify` API so a new check is
-- "call a helper with the body/lens you care about" rather than a from-scratch
-- script with hand-computed pans. See docs/development/DEVELOPMENT_PRACTICES.md
-- § Visual verification.

-- Settle, then capture. Use this instead of a bare `verify.capture` after anything
-- that OPENS or RESIZES a window, switches a ledger view, or changes a layout.
--
-- Why (Ben, 2026-08-01, from a real miss): `verify.capture` composites exactly ONE
-- frame, while ImGui settles auto-layout over the following frame or two — a child's
-- content region, a table's column widths and a fresh window's scroll state are all
-- provisional on the frame they first appear. Capturing immediately therefore records
-- a half-laid-out frame, and because that frame is *deterministic* it blesses and
-- re-passes at 0.0000% forever: a stable golden of the wrong picture. Four of the ten
-- BL-214 fold captures moved once settled, which is how this was found.
--
--   name   : capture name, as verify.capture.
--   frames : optional settle frames before the shot (default 4).
function shot(name, frames)
    verify.frames(frames or 4)
    verify.capture(name)
end

-- Frame a Planetary tile: set the zoom and centre the tile using the canvas's own
-- transform (verify.center_tile). Replaces the hand-computed
-- pan = (grid_centre - tile_local) * zoom that early scripts carried.
--   col, row : grid coordinates of the tile to centre.
--   zoom     : optional zoom factor (defaults to the current zoom).
function frame_tile(col, row, zoom)
    verify.center_tile(col, row, zoom)
end

-- Capture the active surface under each overlay lens, for side-by-side comparison.
--   prefix : base name; each capture is "<prefix>_<NN>_<lens>".
--   lenses : optional list of lens names; defaults to the meaningful set
--            (plain terrain, faction territory, corporate ownership).
function sweep_overlays(prefix, lenses)
    lenses = lenses or {"none", "faction", "corporation"}
    for i, lens in ipairs(lenses) do
        verify.set_overlay(lens)
        verify.capture(string.format("%s_%02d_%s", prefix, i - 1, lens))
    end
end

-- Centre and capture each corporation building in turn, using the positions from
-- verify.buildings() so the caller never hand-lists coordinates. Captures are
-- named "building_<tag>_<x>_<y>", where <tag> is "player" for the player's corp
-- or "corp<id>" for a rival.
--   zoom : optional zoom for each capture (defaults to a close framing of 20).
function tour_buildings(zoom)
    zoom = zoom or 20
    for _, b in ipairs(verify.buildings()) do
        frame_tile(b.x, b.y, zoom)
        local tag = b.player and "player" or ("corp" .. b.corp)
        verify.capture(string.format("building_%s_%d_%d", tag, b.x, b.y))
    end
end
