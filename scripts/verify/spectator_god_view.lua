-- BL-408 spectator god view (requirements.json § spectator-god-view, capture side).
--
-- What this proves: the flag-gated render paths. OFF legs show today's BL-068
-- redactions (the baseline the flag must not disturb); ON legs show the same
-- surfaces with the internals open and the survey mask lifted to its tell.
-- What this does NOT prove: that the system-menu toggle is reachable by a
-- click — R1's live click-through is the main session's job (io-standing-rules:
-- a UI requirement needs a live check), and this script sets the flag through
-- the verify seam, not through the control.
--
-- Run with: ProjectIo --verify scripts/verify/spectator_god_view.lua

-- Spectate BEFORE stepping (same rule as decision_feed.lua): step_economy reads
-- the flag per call. God view itself has no such ordering constraint (draw-time
-- only), but the session should BE a spectated one end to end.
verify.spectate(true)
verify.econ_step(8)

-- Find a rival building on the home body: the player's first asset names home,
-- then the first non-player building on that body is the subject. Deterministic
-- (verify.buildings() walks the sorted corp map).
local home, rival
for _, b in ipairs(verify.buildings()) do
    if b.player and home == nil then home = b.body end
end
for _, b in ipairs(verify.buildings()) do
    if (not b.player) and b.body == home then rival = b; break end
end
verify.expect(rival ~= nil, "a rival building exists on the home body")

verify.goto_surface("home")
verify.frames(2)

-- OFF leg: the redacted rival card — 'Production: private / Stockpile: private',
-- Status as the only page. This is the byte-identical baseline (R1's off half).
verify.select_building(rival.x, rival.y)
verify.frames(2)
verify.capture("god_view_off_rival_building")

-- ON leg: same selection, internals open — Production/Stockpile rows carry real
-- values, and the read-only Profitability page joins the pager (controls stay
-- off: god view grants sight, never hands).
verify.god_view(true)
verify.frames(2)
verify.capture("god_view_on_rival_building")

-- Corp card: the god-facts readout (cash, reserve floor, should-have buffer,
-- pools, running production) in the facts column that is empty today.
verify.select_corp(rival.corp)
verify.frames(2)
verify.capture("god_view_on_rival_corp")

-- Hover card over the rival's marker: owner attribution plus the player-card
-- detail lines, why-line 'Competitor - god view'.
--
-- Zoom in first: hover_tile centres the tile on the CANVAS centre but parks the
-- mouse at the DISPLAY centre, ~half the title bar (~10 px) higher - about one
-- row at the default zoom, which is how this capture's first run photographed
-- the neighbouring tile's terrain card instead. At zoom 8 a row is ~66 px, so
-- the fixed offset stays comfortably inside the hovered tile and its marker.
verify.set_zoom(8.0)
verify.hover_tile(rival.x, rival.y)
verify.frames(31) -- past kHoverAppearDelaySec on the fixed verify clock
verify.capture("god_view_on_rival_hover")

-- OFF again: the redaction returns — the flag is presentation state, nothing
-- latched (R2's no-residue half).
verify.god_view(false)
verify.frames(2)
verify.capture("god_view_off_again_rival_hover")

-- Survey-mask lift on an unsurveyed body ('inner' starts hidden): OFF = the
-- flat locked fill; ON = terrain readable through the heavy lock-colour tell,
-- with the header's UNSURVEYED suffix still up (the corps' own blindness stays
-- legible - the item's 'tell').
verify.clear_selection()
verify.goto_surface("inner")
verify.frames(2)
verify.capture("god_view_off_unsurveyed_body")
verify.god_view(true)
verify.frames(2)
verify.capture("god_view_on_unsurveyed_body")

-- NO GOLDEN (same rationale as decision_feed.lua): these surfaces read live AI
-- economy state, so a golden would go red on every corp_ai tuning or world-gen
-- change. Capture-only, run when someone touches the god-view paths.
