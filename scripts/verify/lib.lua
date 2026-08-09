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

-- Centre and capture ONE REPRESENTATIVE building per (type, ownership) pair,
-- using verify.buildings() so the caller never hand-lists coordinates. Captures
-- are named "building_<player|rival>_<type>".
--
-- This used to capture EVERY building on the map: 146 goldens and 514 MB, 40% of
-- the whole golden set, from this one helper. That is not 146 checks — it is one
-- check run 146 times against different data, because what it verifies is the
-- SILHOUETTE (extraction = faceted chunk, processing = square, port = triangle),
-- and a silhouette does not vary per instance. The cost was real: every UI detail
-- change re-blessed all 146, and 514 MB of PNG churned through git each time.
--
-- Two properties matter in the naming, and neither held before:
--   * STABLE ACROSS GENERATION. The old names carried grid coordinates and corp
--     ids, both of which move when world generation changes — so a generation
--     change orphaned all 146 goldens at once rather than diffing them. Type and
--     ownership survive.
--   * DETERMINISTIC PICK. verify.buildings() walks an unordered_map, so "the
--     first of each type" is only stable if we sort first. Sorted by (x, y).
--
--   zoom : optional zoom for each capture (defaults to a close framing of 20).
function tour_buildings(zoom)
    zoom = zoom or 20

    -- NOTE: the verify Lua sandbox exposes no `table` library, so this sorts by
    -- hand. Both passes below exist for determinism, not tidiness:
    -- verify.buildings() walks an unordered_map, so "the first of each type" is
    -- only reproducible if the winner is chosen by a total order on the data.
    local best, keys, nkeys = {}, {}, 0
    for _, b in ipairs(verify.buildings()) do
        -- Type names arrive human-readable ("Extraction Site"); fold to
        -- lower_snake so a golden filename never carries a space.
        local ty = string.lower(string.gsub(b.type or "unknown", " ", "_"))
        local key = (b.player and "player" or "rival") .. "_" .. ty
        local cur = best[key]
        if cur == nil then
            nkeys = nkeys + 1
            keys[nkeys] = key
            best[key] = b
        elseif b.x < cur.x or (b.x == cur.x and b.y < cur.y) then
            best[key] = b -- lowest (x, y) wins, whatever order the map handed them over in
        end
    end

    -- Insertion sort the keys so the capture ORDER is stable too — a reordered
    -- run would still produce identical goldens, but a stable order keeps the
    -- log readable and any future diff-by-sequence honest.
    for i = 2, nkeys do
        local k, j = keys[i], i - 1
        while j >= 1 and keys[j] > k do
            keys[j + 1] = keys[j]
            j = j - 1
        end
        keys[j + 1] = k
    end

    for i = 1, nkeys do
        local b = best[keys[i]]
        frame_tile(b.x, b.y, zoom)
        verify.capture("building_" .. keys[i])
    end
end
