-- Do units survive the quarters in the --verify world?
-- Every unit was seen gone by the fourth econ_step (sprint 63). This prints the census per
-- quarter, whose units they are and how many men, so the loss has a shape. Read-only.
local function census(label)
  local us = verify.units() or {}
  local mine, rivals, men = 0, 0, 0
  for _, u in ipairs(us) do
    if u.mine or u.player or u.own then mine = mine + 1 else rivals = rivals + 1 end
    men = men + (u.count or 0)
  end
  print(string.format("UNIT_CENSUS %s total=%d mine=%d rivals=%d men=%d", label, #us, mine, rivals, men))
  if us[1] and label == "start" then
    local line = "UNIT_FIELDS"
    for k, v in pairs(us[1]) do line = line .. " " .. k .. "=" .. tostring(v) end
    print(line)
  end
end

census("start")
for q = 1, 6 do
  verify.econ_step(1)
  census("q" .. q)
end
