-- Visual verification for the Trade-flow lens (overlay_mode::trade_flow, BL-1222).
-- Confirms requirements.json § trade-flow-lens:
--   R2 the lens draws the player's market-to-market shipment arrows (width by
--      units per tick over the trailing window) and a marker beside each short
--      market coloured by its best refusal class, with the class key and the
--      flow-width key in the minimap header; no rival flow appears (the lens
--      reads only the player corporation's dispatcher record).
--   R3 (partly) the lens is reachable by name; the keyboard cycle and the
--      hovers are a LIVE click (Ben), not this script.
--
-- Driver: seat the player on the verify world (the seat screen's top-ranked
-- candidate, as seat_pick.lua does), run play ticks so the dispatcher has
-- passes to record, arm the lens, capture the body and a closer framing.
-- Run with: ProjectIo --verify scripts/verify/trade_flow_lens.lua

verify.show_seat_screen(true)
verify.frames(3)
local firm = verify.seat_candidate(0)
verify.show_seat_screen(false)
verify.frames(1)
local r = verify.take_seat(firm)
print("[trade_flow_lens] seat " .. tostring(firm) .. " -> " .. tostring(r))

verify.goto_surface("home")

-- Play ticks: each runs the dispatcher once, so the lens's four-pass trailing
-- window is full and the newest pass's refusals are this tick's.
verify.econ_step(12)

verify.set_overlay("trade_flow")
verify.frames(2)
verify.expect(verify.overlay_name() == "trade_flow", "the lens arms by its script name")
verify.capture("trade_flow_lens_body")

-- The Selection band covers the canvas's lower half; pan so the southern
-- markets clear it.
verify.add_pan(0, -250)
verify.frames(2)
verify.capture("trade_flow_lens_south")
verify.add_pan(0, 250)

-- East: on the default verify world the player's one shipping lane at this
-- tick count runs off the right edge of the body framing.
verify.add_pan(-750, -250)
verify.frames(2)
verify.capture("trade_flow_lens_east")
-- Hover reads, at this framing's pixels on the default verify world: the shaft
-- midpoint, then the refusal marker beside the source market. Capture-only;
-- the live hover is R3's click, not this.
verify.hover(878, 245, 2)
verify.capture("trade_flow_lens_hover_arrow")
verify.hover(830, 421, 2)
verify.capture("trade_flow_lens_hover_marker")
verify.hover(-1, -1, 1)
verify.add_pan(750, 250)

-- Open the key's region is fixed-height (no dropdown); a closer framing on the
-- home market area so arrows and markers read at glyph scale.
verify.command("zoom_in")
verify.command("zoom_in")
verify.frames(2)
verify.capture("trade_flow_lens_zoom")

-- The lens drew something for the seated corporation before the seat moves —
-- otherwise the "nothing after a seat change" row below would prove nothing.
do
    local a, m, p = verify.trade_flow_counts()
    print(string.format("[trade_flow_lens] seat %d: arrows %d markers %d passes %d", firm, a, m, p))
    verify.expect(p > 0 and (a + m) > 0, "the held corporation's passes draw (" .. p .. " passes)")
end

-- SEAT CHANGE (review round 1). The window still holds the corporation left
-- behind, now a rival; the lens must draw none of it until the newly held
-- corporation's first dispatcher pass.
local other = verify.seat_candidate(1)
if other == firm then other = verify.seat_candidate(2) end
local r2 = verify.take_seat(other)
print("[trade_flow_lens] reseat " .. tostring(other) .. " -> " .. tostring(r2))
verify.expect(r2 == "applied", "the second seat applies")
verify.goto_surface("home")
verify.set_overlay("trade_flow")
verify.frames(2)
do
    local a, m, p = verify.trade_flow_counts()
    verify.expect(a == 0 and m == 0 and p == 0,
                  string.format("after a seat change the lens draws nothing (arrows %d markers %d passes %d)", a, m, p))
end
verify.capture("trade_flow_lens_reseat_empty")

verify.econ_step(1)
verify.frames(2)
do
    local a, m, p = verify.trade_flow_counts()
    print(string.format("[trade_flow_lens] after one pass for %d: arrows %d markers %d passes %d", other, a, m, p))
    verify.expect(p == 1, "the new corporation's first pass is the only one the lens reads (" .. p .. ")")
end
verify.capture("trade_flow_lens_reseat_first_pass")
