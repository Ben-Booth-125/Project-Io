-- Verify the research panel's Early Space tree (BL-1613): the next age's tree on
-- the research canvas behind an age switch, locked before the gate into space and
-- queued with the ring's own press idiom after it (RESEARCH.md § The research
-- queue; ERAS.md § Era 1 -> Era 2 gate).
--
-- Captures (screenshots/):
--   early_space_locked   the Early Space tab before the gate: every node locked,
--                        the caption naming what the gate still needs, each branch
--                        header saying what keys it.
--   early_space_open     after verify.space_ledger_fixture() (every Digitisation
--                        node held, a staffed Launchpad, propellant on the shelf):
--                        the tree open, each header "Keyed".
--   early_space_queued   one shift-click on Remote Prospecting queued it with its
--                        two requirements ahead, on the Early Space tail; the
--                        column lists them with arrival quarters.
--   early_space_hover    the shared hover card on a queued node.
--   early_space_unqueue  a shift-click on queued Cryogenics II took it off with
--                        Remote Prospecting, which needs it.
--
-- Every press is a REAL press through ImGui's hit-test, aimed by store id through
-- verify.tech_tree_node and at the tabs through verify.tech_tree_age.

local function count(t) local n = 0 for _ in pairs(t) do n = n + 1 end return n end
local function away() verify.hover(20, 20, 2) end
local function open_panel() return verify.pointer_target()["open_panel"] end

local function node(id)
    local x, y = verify.tech_tree_node(id)
    verify.expect(x ~= nil and x >= 0, "the tree drew " .. id)
    return x, y
end

local function tab(which)
    local a = verify.tech_tree_age()
    verify.expect(a.drawn, "the age switch is drawn")
    if which == "es" then return a.es_x, a.es_y end
    return a.dg_x, a.dg_y
end

verify.window(1920, 1080)
verify.frames(2)
verify.show_panel("tech_tree", true)
verify.frames(3)
verify.expect(verify.tech_tree_age().age == 0, "the panel opens on the Digitisation ring")

-- The Early Space tab, before the gate.
local x, y = tab("es")
verify.click(x, y)
verify.frames(3)
local a = verify.tech_tree_age()
verify.expect(a.age == 1, "the Early Space tab switches the canvas to the Early Space tree")
verify.expect(a.locked, "before the gate the tree draws locked")
verify.expect(a.lock_reason:find("gate into space") ~= nil, "the caption names the gate: " .. a.lock_reason)
local dx = verify.tech_tree_node("DG-PR-EX-T1")
verify.expect(dx == -1, "the Digitisation ring is not drawn while the Early Space tree is")
away()
verify.capture("early_space_locked")

-- A press before the gate is refused and queues nothing.
local before = count(verify.tech_tree_queue())
x, y = node("ES-PR-IP-T1")
verify.shift_click(x, y)
verify.frames(2)
verify.expect(count(verify.tech_tree_queue()) == before, "a press before the gate queues nothing")
local msg = verify.research_message()
verify.expect(msg:find("locked") ~= nil, "the refusal says why: " .. msg)

-- Past the gate: every Digitisation node held, a staffed Launchpad, propellant.
local fx = verify.space_ledger_fixture()
verify.expect(fx.ok, "the space fixture staged the gate (pad and shelf)")
verify.frames(3)
a = verify.tech_tree_age()
verify.expect(not a.locked, "past the gate the tree is open")
away()
verify.capture("early_space_open")

-- Shift-click queues a node with its requirements ahead, on the tail.
before = count(verify.tech_tree_queue())
x, y = node("ES-PR-IP-A1")
verify.click(x, y)
verify.frames(2)
verify.expect(count(verify.tech_tree_queue()) == before, "a plain click queues nothing")
verify.shift_click(x, y)
verify.frames(2)
local q = verify.tech_tree_queue()
local n = count(q)
verify.expect(n == before + 3, "one shift-click queued 3 Early Space nodes (" .. n .. ")")
verify.expect(q[n - 2] == "ES-PR-IP-T1" and q[n - 1] == "ES-PR-IP-T2" and q[n] == "ES-PR-IP-A1",
              "requirements ahead of the node, at the tail of the one queue")
local arr = verify.tech_tree_arrivals()
verify.expect(count(arr) == n, "one arrival per entry, the tail included (" .. count(arr) .. ")")
away()
verify.capture("early_space_queued")

x, y = node("ES-PR-IP-T2")
verify.hover(x, y, 40)
verify.capture("early_space_hover")

-- The toggle: a shift-click on a queued node takes it off with what needs it.
verify.shift_click(x, y)
verify.frames(2)
q = verify.tech_tree_queue()
verify.expect(count(q) == before + 1 and q[count(q)] == "ES-PR-IP-T1",
              "unqueue took Cryogenics II and Remote Prospecting, kept Cryogenics I (" .. count(q) .. ")")
away()
verify.capture("early_space_unqueue")

-- Back to the ring, then the toggle rule: the active tab again closes the panel.
x, y = tab("dg")
verify.click(x, y)
verify.frames(2)
verify.expect(verify.tech_tree_age().age == 0, "the Digitisation tab switches back to the ring")
x, y = tab("dg")
verify.click(x, y)
verify.frames(2)
verify.expect(open_panel() ~= "tech_tree", "re-clicking the active tab closes the research panel")
