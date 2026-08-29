-- Visual verification for the Throughput lens (BL-598).
--
-- LOGISTICS.md § Logistic Points, "Refusal, surface and determinism":
--   "Throughput is a lens, extending Reach. The Reach lens shows a binary field;
--    throughput is that field with a magnitude, so it is a small step from an
--    existing surface rather than a new one."
--
-- The three claims this check photographs:
--   T1  the BINARY half — ground an anchor reaches is tinted the logistics cyan,
--       ground no anchor reaches goes dark. That is the reach field, drawn
--       through the CONST `tile_reach_cost` read; the canvas never runs the
--       Dijkstra itself.
--   T2  the MAGNITUDE half — every supply anchor (city, built-and-active port or
--       inland hub) carries a disc sized and coloured by its active LP this tick.
--   T3  the key is READABLE. Six of the seven gradient keys are drawn on the
--       background list and buried by the Selection band; this one takes the
--       Continent key's foreground list and opaque fill (BL-376), so the last
--       capture deliberately opens the Selection band UNDER it.
--
-- Run with: ProjectIo --verify scripts/verify/throughput_lens.lua

verify.goto_surface("home")
verify.set_overlay("throughput")

-- T1 — the whole grid. The envelope is the subject at this zoom: the reach field
-- has shape, and its shape is what "Reach with a magnitude" extends.
shot("throughput_lens_full")

-- Find the home body, then its largest city. Never hard-coded coordinates — the
-- world moves under a generation change and a fixed pair silently frames empty
-- ground (the trap industry_lens.lua and settlement_labels.lua both record).
local home_body = nil
for _, b in ipairs(verify.buildings()) do
  if b.player then home_body = b.body break end
end
if not home_body then
  error("throughput_lens: no player building, so the home body cannot be identified")
end

local ax, ay, ascale = nil, nil, -1
for _, pc in ipairs(verify.population_centres()) do
  if pc.body == home_body and pc.scale > ascale then
    ascale, ax, ay = pc.scale, pc.x, pc.y
  end
end
if not ax then
  -- A body with no anchor at all is a FINDING, not a scene to photograph: the
  -- lens would render an all-dark envelope and an honest "no anchor generates
  -- active LP" key, and that would bless clean forever.
  error("throughput_lens: the home body carries no population centre — "
        .. "there is no anchor to draw a magnitude on")
end

print("[throughput_lens] framing the largest anchor at " .. ax .. "," .. ay
      .. " (scale " .. ascale .. ")")

-- T2 — in close on the biggest anchor, where the disc and its ring read at size
-- against the served ground around it.
frame_tile(ax, ay, 9)
shot("throughput_lens_anchor")

-- T3 — the key over an OPEN Selection band. This is the capture that fails if the
-- key ever moves back to the background draw list.
verify.select_tile(ax, ay)
shot("throughput_lens_key_over_selection")
