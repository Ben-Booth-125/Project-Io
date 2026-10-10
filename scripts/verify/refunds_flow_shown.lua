-- BL-1215 (refunds flow shown): the Corporation ledger's Balance card names
-- the quarterly return's `refunds` flow (BL-1206, the seat clean slate).
--
--   ProjectIo --verify scripts/verify/refunds_flow_shown.lua
--   Inspect: screenshots/refunds_flow_shown*.png
--
-- STAGING. A refund has one source: taking a seat cancels the firm's
-- construction in progress and credits what it paid (seat_clean_slate). So
-- the script lets the background firms start building, then seats each
-- candidate in turn through the real seat seam until one of them had
-- construction underway, steps one quarter so the return books the refund,
-- and captures the card.
--
--   R1  a seated firm with construction underway files a positive refund
--   R2  the card's earnings column carries a "Refunds" segment of that figure
--   R3  (captured) the card shows the "Refunds:" line under the net

verify.window(1920, 1080)
verify.econ_step(24) -- background firms start building (6 was enough before sprint 50 made firms build only what runs)

verify.show_seat_screen(true)
verify.frames(3)
local _, n = verify.seat_state()

local card = nil
for row = 0, n - 1 do
    verify.show_seat_screen(true)
    verify.frames(2)
    verify.seat_brief(row)
    verify.frames(1)
    local r = verify.seat_confirm()
    verify.frames(2)
    if r == "applied" then
        verify.econ_step(1) -- the next return books the refund
        local c = verify.corp_balance_card()
        if c.refunds > 0 then
            card = c
            print(string.format("[refunds_flow_shown] row %d refunded %.1f", row, c.refunds))
            break
        end
    end
end

verify.expect(card ~= nil, "R1 some seated firm had construction underway and was refunded")
if card then
    local seg = nil
    for _, s in ipairs(card.earnings) do
        if s.label == "Refunds" then seg = s end
    end
    verify.expect(seg ~= nil, "R2 the earnings column names a Refunds segment")
    if seg then
        verify.expect(math.abs(seg.value - card.refunds) < 0.01,
            string.format("R2 the segment is the return's refund: %.2f vs %.2f",
                          seg.value, card.refunds))
    end
end

verify.show_panel("corporation", true)
verify.fold()
verify.frames(2)
verify.capture("refunds_flow_shown")
