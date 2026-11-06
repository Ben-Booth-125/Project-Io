-- BL-1519 (escape menu): the session menu, through its
-- real presses.
--
-- Esc and the gear open it and stop the clock; closing it restores the remembered
-- speed, and a clock already stopped stays stopped. Exit to Menu warns, drops the
-- campaign to the main menu, and Load opens the save again, twice over.
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
press("save")
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
    verify.capture("esc_06_loaded_" .. round)
    check(verify.screen() == "in_game", "round " .. round .. ": Load opens the save")
    verify.econ_step(1)
    verify.capture("esc_07_stepped_" .. round)
end

if failed > 0 then error("escape_menu: " .. failed .. " failed") end
print("escape_menu: OK")
