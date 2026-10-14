-- Verify the research panel (BL-1276): the Digitisation tree drawn as three
-- trees in a ring, its hover card, and the shift-click queue (Ben, 2026-10-10:
-- no Plan mode; SHIFT + left click queues, a plain click/hover inspects, and a
-- shift-click on a queued node takes it off with the entries that need it).
--
-- Captures (screenshots/):
--   tech_tree_inspect      the ring: 160 nodes, four cost rings, AND links in
--                          the accent, pairs outlined red, quests as amber
--                          diamonds, placeholders dashed; each tree's name in
--                          its own sector; the legend; the queue column.
--   tech_tree_hover_major  the shared hover card on a major (Offshore Drilling).
--   tech_tree_hover_quest  the card on a quest (Strategic Reserve): deed, keys.
--   tech_tree_queue        one shift-click on Seismic Survey queued it with its
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

-- A plain click only inspects: nothing is queued.
verify.expect(count(verify.tech_tree_queue()) == 0, "queue starts empty")
x, y = node("DG-PR-EX-A1")
verify.click(x, y)
verify.frames(2)
verify.expect(count(verify.tech_tree_queue()) == 0, "a plain click queues nothing")

-- Shift-click queues the node with its requirements ahead of it.
verify.shift_click(x, y)
verify.frames(2)
local q = verify.tech_tree_queue()
verify.expect(count(q) == 3, "one shift-click queued 3 nodes (" .. count(q) .. ")")
verify.expect(q[1] == "DG-PR-EX-T1" and q[2] == "DG-PR-EX-T2" and q[3] == "DG-PR-EX-A1",
              "requirements queued ahead of the node, head first")
away()
verify.capture("tech_tree_queue")

-- Shift-click on a queued node toggles it off, with every later entry needing it.
x, y = node("DG-PR-EX-T2")
verify.shift_click(x, y)
verify.frames(2)
q = verify.tech_tree_queue()
verify.expect(count(q) == 1 and q[1] == "DG-PR-EX-T1",
              "unqueue took Field Practice II and Seismic Survey, kept Field Practice I ("
              .. count(q) .. ")")
away()
verify.capture("tech_tree_unqueue")

-- A pick-one pair: queue one side, then shift-click the other.
x, y = node("DG-ST-AC-Aa")
verify.shift_click(x, y)
verify.frames(2)
local before = count(verify.tech_tree_queue())
verify.expect(before == 5, "Free-Trade Accords queued with its path (" .. before .. ")")
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
verify.expect(count(q) == 15, "the far press queued its 10-node route (" .. count(q) .. " total)")
verify.expect(q[count(q)] == "DG-ST-AC-M", "the far node is last, its route ahead of it")
verify.expect(has(q, "DG-ST-AC-C") and has(q, "DG-ST-AC-C1") and has(q, "DG-ST-AC-C2"),
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
