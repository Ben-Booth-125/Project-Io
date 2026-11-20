-- Verify the research panel (BL-1276): the Digitisation tree drawn as three
-- trees in a ring, its hover card, and the queue (Ben, 2026-10-10: no Plan mode;
-- SHIFT + left click adds to the queue, and a shift-click on a queued node takes it
-- off with the entries that need it. Ben, 2026-11-11: a plain click makes the node
-- the research target, the old queue behind it; hover alone inspects).
--
-- Captures (screenshots/):
--   tech_tree_inspect      the ring: 211 nodes in ten branches, four cost rings, AND links in
--                          the accent, pairs outlined red, quests as amber
--                          diamonds, placeholders dashed; each tree's name in
--                          its own sector; the legend; the queue column.
--   tech_tree_hover_major  the shared hover card on a major (Offshore Drilling).
--   tech_tree_hover_quest  the card on a quest (Strategic Reserve): deed, keys.
--   tech_tree_hover_naval  the card on a Naval major (Reactor Propulsion): the kit it opens.
--   tech_tree_queue        one plain click on Seismic Survey queued it with its
--                          two requirements ahead of it: 3 entries with arrivals.
--   tech_tree_unqueue      a shift-click on queued Field Practice II took it off
--                          with Seismic Survey, which needs it (the toggle).
--   tech_tree_refused      a shift-click on a pick-one pair's other side, refused
--                          with its reason.
--   tech_tree_far_plan     one shift-click on a FAR node, Diplomatic Standing,
--                          whose route crosses a pick-one choice: the planner
--                          takes one side, never both, and every entry arrives.
--
-- Every press is a REAL press through ImGui's hit-test (verify.shift_click /
-- verify.click), aimed at a node by its store id through verify.tech_tree_node.

local function node(id)
    local x, y = verify.tech_tree_node(id)
    verify.expect(x ~= nil and x >= 0, "ring drew " .. id)
    return x, y
end

local function count(t) local n = 0 for _ in pairs(t) do n = n + 1 end return n end
local function has(t, id) for _, v in ipairs(t) do if v == id then return true end end return false end
local function away() verify.hover(20, 20, 2) end

-- 1920x1080, the design-review resolution (ledger_pass's reasoning).
verify.window(1920, 1080)
verify.frames(2)
verify.show_panel("tech_tree", true)
verify.frames(3)
verify.capture("tech_tree_inspect")

-- Hover long enough for the card to appear (0.5 s = 30 verify frames), short
-- of sticking (2.5 s).
local x, y = node("DG-PR-EX-A")
verify.hover(x, y, 40)
verify.capture("tech_tree_hover_major")
x, y = node("DG-PR-EX-Q")
verify.hover(x, y, 40)
verify.capture("tech_tree_hover_quest")
x, y = node("DG-ST-NV-A")
verify.hover(x, y, 40)
verify.capture("tech_tree_hover_naval")

-- A plain click makes the node the research target, its requirements ahead of it
-- (Ben, 2026-11-11). Shift-click adds to the queue (tested below, far plan).
verify.expect(count(verify.tech_tree_queue()) == 0, "queue starts empty")
x, y = node("DG-PR-EX-A1")
verify.click(x, y)
verify.frames(2)
local q = verify.tech_tree_queue()
verify.expect(count(q) == 2, "one plain click queued 2 nodes (" .. count(q) .. ")")
verify.expect(q[1] == "DG-PR-EX-T1" and q[2] == "DG-PR-EX-A1",
              "requirements queued ahead of the node, head first")
away()
verify.capture("tech_tree_queue")

-- Shift-click on a queued node toggles it off, with every later entry needing it.
x, y = node("DG-PR-EX-A1")
verify.shift_click(x, y)
verify.frames(2)
q = verify.tech_tree_queue()
verify.expect(count(q) == 1 and q[1] == "DG-PR-EX-T1",
              "unqueue took Seismic Survey, kept Field Practice I ("
              .. count(q) .. ")")
away()
verify.capture("tech_tree_unqueue")

-- A pick-one pair: queue one side, then shift-click the other.
x, y = node("DG-ST-AC-Aa")
verify.shift_click(x, y)
verify.frames(2)
local before = count(verify.tech_tree_queue())
verify.expect(before == 4, "Free-Trade Accords queued with its path (" .. before .. ")")
x, y = node("DG-ST-AC-Ab")
verify.shift_click(x, y)
verify.frames(2)
verify.expect(count(verify.tech_tree_queue()) == before, "the pair's other side is refused, nothing queued")
local msg = verify.research_message()
verify.expect(msg:find("closed") ~= nil, "refusal says why: " .. msg)
verify.hover(x, y, 40)
verify.capture("tech_tree_refused")

-- R2: a FAR node whose route passes a pick-one choice. Diplomatic Standing needs
-- (Strategic Deterrent OR Survivable Forces) + Arms Control Verification.
x, y = node("DG-ST-AC-M")
verify.shift_click(x, y)
verify.frames(2)
q = verify.tech_tree_queue()
verify.expect(count(q) == 11, "the far press queued its 7-node route (" .. count(q) .. " total)")
verify.expect(q[count(q)] == "DG-ST-AC-M", "the far node is last, its route ahead of it")
verify.expect(q[1] == "DG-PR-EX-T1", "a shift-click adds to the queue and keeps the target")
verify.expect(has(q, "DG-ST-AC-C") and has(q, "DG-ST-AC-C2") and has(q, "DG-ST-AC-B1"),
              "both AND groups' routes are queued")
verify.expect(has(q, "DG-ST-AC-Ba") ~= has(q, "DG-ST-AC-Bb"),
              "the pick-one choice took exactly one side")
local arr = verify.tech_tree_arrivals()
verify.expect(count(arr) == count(q), "one arrival per entry")
local finite, prev = true, 0
for i = 1, count(arr) do
    if arr[i] == nil or arr[i] < 1 then finite = false end
end
verify.expect(finite, "every entry has a finite arrival quarter (last: " .. tostring(arr[count(arr)]) .. ")")
away()
verify.capture("tech_tree_far_plan")

-- A plain click on a queued node makes it the target: its route moves to the front
-- and the rest of the queue follows, nothing lost (Ben, 2026-11-11).
x, y = node("DG-ST-AC-M")
verify.click(x, y)
verify.frames(2)
q = verify.tech_tree_queue()
local function at(id) for i = 1, count(q) do if q[i] == id then return i end end return nil end
verify.expect(count(q) == 11, "retargeting kept every entry (" .. count(q) .. ")")
verify.expect(at("DG-ST-AC-M") ~= nil and at("DG-PR-EX-T1") ~= nil and at("DG-ST-AC-M") < at("DG-PR-EX-T1"),
              "the clicked node's route now runs ahead of the old target")
