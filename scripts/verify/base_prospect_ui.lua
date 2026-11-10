-- BL-1610 (base and prospect UI): founding a base from a landed Lander and
-- watching prospecting on the Planetary canvas (docs/ui/SELECTION.md § The
-- space-unit element; docs/ui/PLANETARY.md, the Survey region mask row;
-- docs/ui/DISCOVERY.md § Mapping, then prospecting; docs/politics/NATIONS.md
-- § Off-world bases).
--
-- verify.base_prospect_fixture() stages, straight into the stores (no launch,
-- research or haul verb), on the home body's first gridded moon: the player
-- nation's Off-World Bases key; the moon MAPPED whole for the player and
-- prospected nowhere; a passenger Lander landed there with the founding's
-- colonists aboard, a cargo Lander landed there holding the base kit, and an
-- empty passenger Lander idle in the home low orbit. verify.stage_prospect(n)
-- prospects n regions by the prospecting rule itself.
--
-- Frames: the moon mapped and unprospected (the slate wash everywhere, the
-- Resource lens offering nothing), part-prospected, the Lander's card with
-- Found base armed and a site chosen, the moon after the founding, and the home
-- Lander's card with Embark colonists armed.

verify.goto_surface("home")
verify.frames(2)
local staged = verify.base_prospect_fixture()
verify.expect(staged.staged, "the fixture staged the moon, its Landers and the key")
if not staged.staged then return end
print(string.format("base_prospect_ui: staged on %s: passenger %d, carrier %d, home lander %d",
    staged.body, staged.passenger, staged.carrier, staged.at_home))

-- ── The two-step mask: mapped, nothing prospected ──
verify.goto_surface(staged.body)
verify.mouse(-1, -1)
verify.frames(3)
local pm = verify.prospect_mask()
print(string.format("base_prospect_ui: mapped %d/%d, prospected %d, washed %d tiles, all=%s",
    pm.mapped, pm.total, pm.prospected, pm.washed, tostring(pm.all)))
verify.expect(not pm.all, "the moon reads the two-step mask, not 'everything known'")
verify.expect(pm.mapped == pm.total and pm.total > 0, "the moon is mapped whole")
verify.expect(pm.prospected == 0, "nothing is prospected yet")
verify.expect(pm.washed > 0, "mapped, unprospected ground draws under the slate wash")
verify.capture("base_prospect_mapped")

verify.set_overlay("resource")
verify.frames(3)
pm = verify.prospect_mask()
print(string.format("base_prospect_ui: resource lens key offers %d goods before prospecting", pm.key_rows))
verify.expect(pm.key_rows == 0, "the Resource lens offers no good on unprospected ground")
verify.capture("base_prospect_lens_unprospected")

-- ── Part-prospected ──
local half = math.max(1, math.floor(pm.total / 2))
local done = verify.stage_prospect(half)
verify.frames(3)
local pm2 = verify.prospect_mask()
print(string.format("base_prospect_ui: prospected %d (asked %d): washed %d tiles, key offers %d goods",
    pm2.prospected, half, pm2.washed, pm2.key_rows))
verify.expect(done == half and pm2.prospected == half, "the staged regions read prospected")
verify.expect(pm2.washed < pm.washed, "prospected ground leaves the wash")
verify.capture("base_prospect_lens_partial")

-- Everything prospected: the wash is gone and the body reads surveyed.
verify.stage_prospect(pm2.total)
verify.frames(3)
local pm3 = verify.prospect_mask()
print(string.format("base_prospect_ui: all prospected: washed %d, key offers %d goods, all=%s",
    pm3.washed, pm3.key_rows, tostring(pm3.all)))
verify.expect(pm3.washed == 0, "no wash once every region is prospected")
verify.expect(pm3.key_rows >= pm2.key_rows, "the key offers at least what it did part-prospected")
verify.set_overlay("none")
verify.frames(2)

-- ── The landed Lander's card: Found base ──
verify.command("ascend")
verify.frames(3)
local mark = nil
for _, f in ipairs(verify.circum_fleets()) do
    print(string.format("  fleet lead=%d %s x%d at %s", f.lead, f.hull, f.count, f.node))
    if f.lead == staged.passenger then mark = f end
end
verify.expect(mark ~= nil, "the landed Landers draw as a fleet on the moon's rung")
if mark == nil then return end
verify.click(mark.x, mark.y)
verify.frames(2)
verify.expect(verify.circum_selected() == staged.passenger, "a press on the mark selects the passenger Lander")
verify.mouse(-1, -1)
verify.frames(2)

local function presses()
    local p = {}
    for _, x in ipairs(verify.fleet_card().presses) do p[x.name] = x end
    return p
end
local press = presses()
for name, p in pairs(press) do
    print(string.format("  press %-14s (%.0f,%.0f) %s %s", name, p.x, p.y, p.enabled and "on" or "off", p.reason))
end
verify.expect(press.found_base ~= nil and press.found_base.enabled, "Found base is offered on a landed Lander off the home world")
verify.expect(press.embark ~= nil and not press.embark.enabled, "Embark colonists is greyed away from the home body")
if press.found_base == nil then return end

verify.click(press.found_base.x, press.found_base.y)
verify.frames(2)
local bc = verify.base_card()
print(string.format("base_prospect_ui: Found base armed=%s: colonists %d of %d, kit %.0f of %.0f, %d sites",
    tostring(bc.found_armed), bc.colonists_here, bc.colonists_need, bc.kit_here, bc.kit_need, #bc.sites))
verify.expect(bc.found_armed, "Found base lights")
verify.expect(bc.colonists_here >= bc.colonists_need, "the card reads enough colonists landed")
verify.expect(bc.kit_here >= bc.kit_need, "the card reads enough base kit landed")
verify.expect(#bc.sites > 0, "the card lists mapped, unclaimed ground to found on")

-- Toggle: re-pressing the lit Found base disarms it.
press = presses()
verify.click(press.found_base.x, press.found_base.y)
verify.frames(2)
bc = verify.base_card()
verify.expect(not bc.found_armed and #bc.sites == 0, "re-pressing the lit Found base disarms it (the toggle rule)")

press = presses()
verify.click(press.found_base.x, press.found_base.y)
verify.frames(2)
bc = verify.base_card()
local site = bc.sites[1]
if site == nil then return end
for i = 1, math.min(4, #bc.sites) do
    local s = bc.sites[i]
    print(string.format("  site %-60s (%.0f,%.0f) tile (%d,%d)", s.name, s.x, s.y, s.col, s.row))
end
verify.click(site.x, site.y)
verify.frames(2)
bc = verify.base_card()
verify.expect(bc.sites[1] ~= nil and bc.sites[1].chosen, "pressing a site row chooses it")
press = presses()
verify.expect(press.confirm_found ~= nil and press.confirm_found.enabled, "Confirm found base is offered")
verify.mouse(-1, -1)
verify.frames(2)
verify.capture("base_prospect_found_armed")

-- Toggle: a chosen row un-picks.
verify.click(site.x, site.y)
verify.frames(2)
press = presses()
verify.expect(press.confirm_found == nil, "re-pressing the chosen row un-picks it")
verify.click(site.x, site.y)
verify.frames(2)
press = presses()

if press.confirm_found ~= nil then
    verify.click(press.confirm_found.x, press.confirm_found.y)
    verify.frames(3)
end
local msg = verify.fleet_card().message or ""
print("base_prospect_ui: after confirm: " .. msg)
verify.expect(msg:find("Found base: done") ~= nil, "the founding applies through the seam")
verify.expect(not verify.base_card().found_armed, "a confirmed founding disarms the list")

verify.goto_surface(staged.body)
verify.select_tile(site.col, site.row)
verify.mouse(-1, -1)
verify.frames(4)
verify.capture("base_prospect_founded")

-- ── The home Lander's card: Embark colonists ──
verify.goto_surface("home")
verify.command("ascend")
verify.frames(3)
local hm = nil
for _, f in ipairs(verify.circum_fleets()) do
    if f.lead == staged.at_home then hm = f end
end
verify.expect(hm ~= nil, "the home Lander draws on the home rung")
if hm == nil then return end
verify.click(hm.x, hm.y)
verify.frames(2)
verify.mouse(-1, -1)
verify.frames(2)
press = presses()
bc = verify.base_card()
print(string.format("base_prospect_ui: home Lander: colonists %d aboard, %d free berths; embark %s %s",
    bc.colonists_aboard, bc.free_berths, press.embark and (press.embark.enabled and "on" or "off") or "absent",
    press.embark and press.embark.reason or ""))
verify.expect(press.embark ~= nil, "Embark colonists is on a passenger Lander's card")
verify.expect(press.found_base == nil, "Found base is not offered at the home body")
verify.expect(bc.free_berths > 0, "the card reads the free berths")
if press.embark ~= nil and press.embark.enabled then
    verify.click(press.embark.x, press.embark.y)
    verify.frames(2)
    bc = verify.base_card()
    verify.expect(bc.embark_armed and bc.embark_count == bc.free_berths, "Embark lights with every free berth")
    verify.capture("base_prospect_embark_armed")
    press = presses()
    verify.expect(press.confirm_embark ~= nil, "Confirm embark is offered")
    if press.confirm_embark ~= nil then
        verify.click(press.confirm_embark.x, press.confirm_embark.y)
        verify.frames(3)
    end
    print("base_prospect_ui: after embark: " .. (verify.fleet_card().message or ""))
else
    -- The seed's home may hold no staffed Launchpad; the greyed reason is the check.
    verify.expect(press.embark ~= nil and press.embark.reason ~= "", "a greyed Embark names its reason")
    verify.capture("base_prospect_embark_refused")
end
