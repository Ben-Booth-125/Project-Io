-- Visual verification for the campaign-start lens default (GUI lens-behaviour fix).
-- Confirms the canvas opens on NO lens (overlay_mode::none, plain terrain) rather
-- than the former Corporation default — a click only updates the Selection element,
-- so the canvas itself should start unskinned and let the player pick a lens.
--
--   D1 the Planetary surface at campaign start renders plain terrain (no overlay
--      tint imposed) — the harness sets the same default as the live app
--   D2 the overlay strip still re-skins on demand (pick Corporation explicitly),
--      proving the lens machinery is intact, only the *default* changed
--
-- Driver: the harness shares setup_world() with the live app, so the default
-- overlay it seeds IS the campaign-start default. We capture before touching it.
-- Run with: ProjectIo --verify scripts/verify/default_lens.lua

verify.goto_surface("home")

-- Warm-up frame: capture() renders exactly one frame, and ImGui positions a
-- bottom-pivoted AlwaysAutoResize window (the lens strip) only once it has measured
-- the content — i.e. from the *second* frame. A throwaway capture settles the strip
-- so the meaningful captures below show it. (Harness characteristic, not the lens
-- default: the live app renders continuously, so the strip is always visible.)
verify.capture("default_lens_warmup")

-- D1: campaign-start canvas, no overlay set — should be plain terrain, lens strip
-- present with no glyph highlighted.
verify.capture("default_lens_plain")

-- D2: the strip still works — explicitly pick the Nations lens (BL-1406: the
-- Corporation lens retired).
verify.set_overlay("nations")
verify.capture("default_lens_nations_picked")

-- Back to none — re-confirms the plain canvas is a reachable state.
verify.set_overlay("none")
verify.capture("default_lens_cleared")

-- D3 (Ben, 2026-10-24): THE DEFAULT VIEW DOES NOT MARK THE PLAYER'S OWN BUILDINGS.
-- Framed close on one of the player's buildings: with no lens the tile must carry no
-- identity wash and no outline - the building drawn as every building is. The same
-- frame under the Nations lens follows, where the player's footprint outline is still
-- drawn (a lens the player picked may mark ownership), so the pair shows the outline
-- is gone from the default view only. Capture-read: the verify API has no pixel probe.
local own = nil
for _, b in ipairs(verify.buildings()) do
  if b.player and b.complete then own = b break end
end
verify.expect(own ~= nil, "D3: the player holds a completed building to frame")
if own then
  print("[default_lens] framing the player's " .. tostring(own.type) .. " at "
        .. own.x .. "," .. own.y)
  verify.set_overlay("none")
  frame_tile(own.x, own.y, 12)
  shot("default_lens_own_building_plain")
  verify.set_overlay("nations")
  shot("default_lens_own_building_nations")
  verify.set_overlay("none")
end
