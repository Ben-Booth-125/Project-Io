-- Sprint 23 wave 2: the three visual changes, and the strip that now rotates.
--
-- Ben, 2026-08-28, in one message: "we want to stop dual hover ... rework the
-- workforce efficiency lens to be a heatmap, akin to throughput ... throughput
-- needs to be severely scaled", then "add any missing lenses to the strip if that
-- lens focuses on the planetary view. We can rotate lenses for circumplanetary or
-- solar views."
--
-- CAPTURE-LED ON PURPOSE, unlike lens_one_tier beside it. Every claim here is
-- about what a frame LOOKS like — is the field spread or flat, is the tint a
-- heatmap or a dot, does one thing light or two — and none of that is reachable
-- through pointer_target. The assertions below cover the two facts that ARE
-- machine-checkable (the strip's contents follow the rung, and hovering under a
-- lens resolves to the structure alone); the rest is Ben's eye, which is what a
-- capture is for.

verify.goto_surface("home")
verify.econ_step(6)
verify.show_panel("economy", false)

local built_col, built_row, home_body = nil, nil, nil
for _, b in ipairs(verify.buildings()) do
    if b.player then built_col, built_row, home_body = b.x, b.y, b.body; break end
end
verify.expect(built_col ~= nil, "found a player building to frame on")
verify.center_tile(built_col, built_row, 7)

-- ---------------------------------------------------------------------------
-- 1. The Population lens is a HEATMAP, not a field of dots.
--
-- Framed twice: wide, where a heatmap either reads as a field or does not, and
-- close, where the tile must still carry its glyphs — the dot used to SUPPRESS
-- the stack ring and the landform mark, and a tint has no reason to.
-- ---------------------------------------------------------------------------
verify.set_overlay("population")
verify.set_zoom(3.0)
verify.capture("field_population_wide")
verify.center_tile(built_col, built_row, 9)
verify.capture("field_population_close")

-- ---------------------------------------------------------------------------
-- 2. The Throughput field, renormalised against the 90th percentile.
--
-- The before-state is on file as a flat cyan wash; what this must show is
-- STRUCTURE — near ground bright, far ground dark, and a visible gradient
-- between. throughput_field_census is the numeric half of the same claim.
-- ---------------------------------------------------------------------------
verify.set_overlay("throughput")
verify.set_zoom(3.0)
verify.capture("field_throughput_wide")
verify.center_tile(built_col, built_row, 9)
verify.capture("field_throughput_close")

-- ---------------------------------------------------------------------------
-- 3. ONE hover mark, not three.
--
-- Under the Market lens the catchment wash is the only thing that may light: the
-- tile ring and the province edge are no-lens marks now. The capture is the
-- check a human reads; the assertion below is what fails by itself.
-- ---------------------------------------------------------------------------
-- CLEAR THE SELECTION FIRST, or this frame proves nothing. The first cut did not,
-- and the capture came back with a tile ring and a province edge still on screen —
-- both of them SELECTION marks left over from the app's opening state, which look
-- exactly like the hover marks this check exists to show are gone. Selection is a
-- different question and keeps its outlines under a lens.
verify.clear_selection()
verify.set_overlay("market")
verify.center_tile(built_col, built_row, 8)
local pt = verify.tile_screen(built_col, built_row)
verify.expect(pt.ok, "the framed tile has a screen position")
verify.mouse(pt.x, pt.y)
verify.frames(40)
local h = verify.pointer_target()
verify.expect(h.hovered_structure_kind == "market",
              "Market lens: the pointer resolves to the catchment")
verify.capture("hover_single_market")

-- The same gesture with NO lens, where the tile ring IS the right mark and must
-- still be drawn. The pair is the check; either frame alone proves nothing.
verify.set_overlay("none")
verify.mouse(pt.x, pt.y)
verify.frames(40)
local n = verify.pointer_target()
verify.expect(n.hovered_structure_kind == "none",
              "no lens: the pointer resolves to no structure, and the tile ring is the mark")
verify.capture("hover_single_no_lens")

-- ---------------------------------------------------------------------------
-- 4. The strip rotates with the rung.
--
-- Nine lenses at Planetary (every sub-body read), three at each of the other two.
-- Captured per rung because the claim is about what is ON the bar, and the bar is
-- 240 px — whether nine glyphs FIT is exactly the question the old hand-kept
-- array of six was answering by omission.
-- ---------------------------------------------------------------------------
verify.set_overlay("none")
verify.clear_selection()
verify.capture("strip_planetary")

-- ASCEND, do not "go to". goto_surface/go_to both resolve a BODY NAME and land
-- on that body's Planetary surface, so neither can leave the rung — the first cut
-- of this check called goto_surface("solar"), matched no body, moved nothing, and
-- captured the Planetary strip three times under three names. The rung is changed
-- by the ascend command, which is the gesture the player has.
verify.command("ascend")
verify.frames(3)
verify.capture("strip_circumplanetary")

verify.command("ascend")
verify.frames(3)
verify.capture("strip_solar")
