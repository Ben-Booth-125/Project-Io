-- BL-1519 (escape menu) and BL-1520 (escape queue): the session menu, through its
-- real presses.
--
-- Esc and the gear open it and stop the clock; closing it restores the remembered
-- speed, and a clock already stopped stays stopped. Exit to Menu warns, drops the
-- campaign to the main menu, and Load opens the save again, twice over. The Esc queue
-- closes Menu, then Ledger, then Selection, one press a rung.
--
-- Save names itself in a form (Ben, 2026-11-09): Save opens it, its Save writes, an
-- existing name asks first, Esc or Cancel step back. Load lists saves, and a row opens one.
--
-- The name is left at its default, as verify has no way to type into the field; the
-- name rule itself is tools/verify/save_names_check.cpp.
--
-- Run:     ProjectIo --verify scripts/verify/escape_menu.lua
-- Writes:  a quicksave beside the exe. Captures: esc_*.png.

local failed = 0
local function ok(m) print("escape_menu: PASS " .. m) end
local function check(c, m)
    if c then ok(m) else failed = failed + 1; print("escape_menu: FAIL " .. m) end
end
local function press(name)
    local m = verify.escape_menu()
    local p = m[name]
    check(p.x >= 0, name .. " is drawn")
    verify.click(p.x, p.y)
    verify.capture("esc_frame")
end

-- Esc until the menu is up: whatever the queue must close first, it closes first.
local function esc_to_menu()
    for _ = 1, 5 do
        if verify.escape_menu().open then return end
        verify.press_escape()
        verify.capture("esc_frame")
    end
end

verify.window(1280, 720)
verify.goto_surface("home")
verify.econ_step(2)
verify.capture("esc_00_shell")

-- 1. The gear opens it and pauses; the gear closes it and restores the speed.
verify.set_speed(3)
press("gear")
local m = verify.escape_menu()
check(m.open and m.speed == 0, "the gear opens the menu and stops the clock")
verify.capture("esc_01_menu")
press("gear")
m = verify.escape_menu()
check(not m.open and m.speed == 3, "the gear, pressed again, closes it and restores speed III")

-- 2. Esc does the same, and a stopped clock stays stopped.
verify.set_speed(0)
esc_to_menu()
check(verify.escape_menu().open, "Esc with nothing open opens the menu")
verify.press_escape()
verify.capture("esc_frame")
m = verify.escape_menu()
check(not m.open and m.speed == 0, "a clock already stopped stays stopped after the menu")

-- 3. The Options press toggles the window; it stands over the menu, beside its presses.
esc_to_menu()
press("options")
check(verify.escape_menu().options_open, "Options opens the window")
verify.capture("esc_02_options")
press("options")
check(not verify.escape_menu().options_open, "Options, pressed again, closes it")
press("options")
check(verify.escape_menu().options_open, "Options opens again, in the same place")
verify.capture("esc_02b_options_again")
press("options")
check(not verify.escape_menu().options_open, "and closes again")

-- 4. Save, then the exit warning and its way back.
-- Save names itself: the form replaces the presses, and steps back with Cancel or Esc.
press("save")
verify.capture("esc_03a_save_form")
m = verify.escape_menu()
check(m.options.x < 0 and m.warn_cancel.x >= 0, "Save opens the name form in place of the presses")
press("warn_cancel")
check(verify.escape_menu().options.x >= 0, "Cancel backs out of the form")
press("save")
verify.press_escape()
verify.capture("esc_frame")
m = verify.escape_menu()
check(m.open and m.options.x >= 0, "Esc backs out of the form and leaves the menu open")

-- The first write of the run may or may not meet an existing quicksave.
local function write_save()
    press("save")
    press("save")
    if verify.escape_menu().warn_confirm.x >= 0 then
        verify.capture("esc_03b_overwrite")
        press("warn_confirm")
    end
    check(verify.escape_menu().options.x >= 0, "the form closes once the save lands")
end
write_save()
press("save")
press("save")
check(verify.escape_menu().warn_confirm.x >= 0, "an existing name asks before it overwrites")
verify.capture("esc_03b_overwrite")
press("warn_cancel")
m = verify.escape_menu()
check(m.warn_confirm.x < 0 and m.options.x < 0, "Cancel goes back to the name, not the menu")
press("save")
press("warn_confirm")
check(verify.escape_menu().options.x >= 0, "Overwrite writes the save")
verify.capture("esc_03_saved")
press("exit_menu")
m = verify.escape_menu()
check(m.warning and verify.screen() == "in_game", "Exit to Menu warns first")
verify.capture("esc_04_warning")
press("warn_cancel")
check(not verify.escape_menu().warning and verify.screen() == "in_game", "Cancel backs out")
press("exit_menu")
verify.press_escape()
verify.capture("esc_frame")
m = verify.escape_menu()
check(m.open and not m.warning, "Esc backs out of the warning and leaves the menu open")

-- 5. Exit, Load, and the same again: the second time round is where a stale reset shows.
for round = 1, 2 do
    esc_to_menu()
    press("exit_menu")
    press("warn_confirm")
    verify.capture("esc_05_menu_" .. round)
    check(verify.screen() == "menu", "round " .. round .. ": the warning's Exit to Menu leaves for the main menu")
    press("main_load")
    verify.capture("esc_05b_list_" .. round)
    check(verify.screen() == "menu", "round " .. round .. ": Load lists the saves first")
    press("main_load")
    verify.capture("esc_06_loaded_" .. round)
    check(verify.screen() == "in_game", "round " .. round .. ": Load opens the save")
    verify.econ_step(1)
    verify.capture("esc_07_stepped_" .. round)
end

-- 6. The queue (BL-1520): Menu, then Ledger, then Selection, one press a rung.
verify.select_tile(70, 38)
verify.show_panel("balance", true)
verify.capture("esc_frame")
verify.press_escape()                                   -- nothing on the menu rung: the ledger
verify.capture("esc_08_queue_ledger")
local q = verify.escape_queue()
check(not q.ledger and q.selection and not verify.escape_menu().open, "Esc closes the ledger, the selection stands")
verify.press_escape()
verify.capture("esc_09_queue_selection")
q = verify.escape_queue()
check(not q.selection and not verify.escape_menu().open, "Esc clears the selection to the resting nation")
verify.press_escape()
verify.capture("esc_frame")
check(verify.escape_menu().open, "with nothing open, Esc opens the menu")
verify.show_panel("balance", true)
verify.select_tile(70, 38)
verify.capture("esc_frame")
verify.press_escape()
verify.capture("esc_frame")
q = verify.escape_queue()
check(not verify.escape_menu().open and q.ledger and q.selection, "the menu closes first, ledger and selection standing")
verify.show_panel("balance", false)

if failed > 0 then error("escape_menu: " .. failed .. " failed") end
print("escape_menu: OK")
