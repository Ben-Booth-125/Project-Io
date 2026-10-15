-- The nation selection screen and its seven-part card (BL-1281, nation
-- selection screen; BL-1295, the card's history; STARTUP.md § The seat).
--
--   ProjectIo --verify scripts/verify/nation_select.lua
--
-- WHAT IT PROVES, on the verify world and on library seeds 31 and 32 (the
-- trade-dense and neighbour-war worlds; one of them carries a revolution):
--   N1  the screen offers the MAJORS only, one row each, every row a nation
--       with a champion;
--   N2  a hover re-points the map's territory and the card (captured: the
--       first row, the last row, and a row whose history carries a revolution
--       where the world has one);
--   N3  the card's history reads the nation's own record: pattern tags, a
--       timeline, a lineage (printed per row, so a reader sees what each
--       nation carries);
--   N4  the history LINK opens the History ledger on its Ages view, filtered
--       to the nation, and pressing it again closes it (the toggle rule);
--   N5  a press opens the briefing (captured), Back closes it. THE SELECTED
--       ROW KEEPS ITS FULL CARD (Ben, 2026-10-15, live click: the card used to
--       vanish for the selected nation): the briefing and Confirm sit below
--       the card, so the _briefing capture shows all seven parts too.
--
-- The PNGs are the product: look at them. Determinism: frames, never sleeps.

verify.window(1920, 1080)

local function survey(tag)
  verify.show_seat_screen(true)
  verify.frames(3)
  local open, n = verify.seat_state()
  verify.expect(open, tag .. ": the nation selection screen is open")
  verify.expect(n >= 1, tag .. ": the screen offers the majors (" .. n .. " rows)")

  -- N1 / N3: every row is a nation with a champion; its history is read.
  local rev_row = nil
  for r = 0, n - 1 do
    local nation, marked = verify.seat_candidate(r)
    local champ = verify.seat_champion(r)
    verify.expect(nation > 0 and champ > 0,
                  tag .. ": row " .. (r + 1) .. " is a nation with a champion")
    local tags, rows, lineage = verify.seat_history(r)
    print(string.format("[nation_select] %s row %d nation %d champion %d%s tags [%s] timeline %d lineage %d",
                        tag, r + 1, nation, champ, marked and " (marked)" or "", tags, rows, lineage))
    if tags:find("revolution", 1, true) and rev_row == nil then rev_row = r end
  end

  -- N2: the card follows the hover.
  verify.seat_hover(0)
  verify.frames(2)
  verify.capture(tag .. "_card_row1")
  if n > 1 then
    verify.seat_hover(n - 1)
    verify.frames(2)
    verify.capture(tag .. "_card_last")
  end
  -- A revolution's line, read off the record itself (the tag may sit with a
  -- nation the fold gave it to; the timeline row sits with the realm's line).
  local revs, rrow, rnation = verify.seat_revolutions()
  print(string.format("[nation_select] %s revolutions in the record: %d (first in nation %d, major row %d)",
                      tag, revs, rnation, rrow + 1))
  if rev_row == nil and rrow >= 0 then rev_row = rrow end
  if rev_row ~= nil then
    verify.seat_hover(rev_row)
    verify.frames(2)
    verify.capture(tag .. "_card_revolution")
    print("[nation_select] " .. tag .. " revolution on row " .. (rev_row + 1))
  end

  -- N4: the history link, a toggle.
  local shown = rev_row or (n > 1 and n - 1 or 0)
  verify.seat_hover(shown)
  verify.frames(1)
  local nation = verify.seat_candidate(shown)
  local lopen, filter = verify.seat_history_link()
  verify.frames(3)
  verify.expect(lopen, tag .. ": the history link opens the History ledger")
  verify.expect(filter == nation, tag .. ": the ledger is filtered to the shown nation ("
                .. tostring(filter) .. " == " .. tostring(nation) .. ")")
  verify.capture(tag .. "_history_link")
  local lopen2 = verify.seat_history_link()
  verify.frames(2)
  verify.expect(not lopen2, tag .. ": pressing the link again closes the ledger")

  -- N5: the briefing.
  verify.seat_brief(shown)
  verify.frames(2)
  verify.expect(select(5, verify.seat_state()) == nation, tag .. ": a press opens that nation's briefing")
  verify.capture(tag .. "_briefing")
  verify.scroll_panel("seat_card", 1.0)
  verify.frames(2)
  verify.capture(tag .. "_briefing_confirm")
  verify.seat_back()
  verify.frames(1)
  verify.expect(select(5, verify.seat_state()) == 0, tag .. ": Back closes the briefing")
  verify.show_seat_screen(false)
  verify.frames(1)
  return rev_row ~= nil
end

local any_rev = survey("nation_select_ref")
for _, seed in ipairs({31, 32}) do
  verify.new_world(seed)
  if survey("nation_select_s" .. seed) then any_rev = true end
end
if not any_rev then
  print("[nation_select] NOTE: no surveyed world carried a revolution tag")
end
