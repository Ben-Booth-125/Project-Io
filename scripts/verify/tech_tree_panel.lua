-- Verify the research panel (BL-1276): the Digitisation tree drawn as three
-- trees in a ring, its hover card, and Plan mode's queue.
--
-- Captures (screenshots/):
--   tech_tree_inspect      the ring in Inspect mode: 160 nodes, four cost rings,
--                          AND links in the accent, pairs outlined in the danger
--                          red, quests as amber diamonds, placeholders dashed;
--                          the legend bottom-left; the column's Inspect strip.
--   tech_tree_hover_major  the shared hover card on a major (Offshore Drilling):
--                          name, tier, ring, RP cost, advantage, Requires A + B.
--   tech_tree_hover_quest  the card on a quest (Strategic Reserve): the deed and
--                          "Keys next age".
--   tech_tree_plan_queue   Plan mode after ONE press on Seismic Survey: the
--                          press queued it with its two unheld requirements
--                          ahead of it, so the queue reads 3 entries, each with
--                          its arrival quarter; the ring rings them 1-2-3.
--   tech_tree_plan_refused a press on a pick-one pair's other side, refused with
--                          its reason (Import Substitution while Free-Trade
--                          Accords is queued).
--
-- Every press is a REAL press through ImGui's hit-test (verify.click), aimed
-- at a node by its store id through verify.tech_tree_node — the ring publishes
-- where it drew each node, so the script never re-derives the layout.

local function node(id)
    local x, y = verify.tech_tree_node(id)
    verify.expect(x ~= nil and x >= 0, "ring drew " .. id)
    return x, y
end

local function count(t) local n = 0 for _ in pairs(t) do n = n + 1 end return n end

-- 1920x1080, the design-review resolution (ledger_pass's reasoning).
verify.window(1920, 1080)
verify.frames(2)
verify.show_panel("tech_tree", true)
verify.panel_view("tech_tree", 0)
verify.frames(3)
verify.capture("tech_tree_inspect")

-- Hover a major long enough for the card to appear (0.5 s = 30 verify frames)
-- and short of sticking (2.5 s).
local x, y = node("DG-PR-EX-A")
verify.hover(x, y, 40)
verify.capture("tech_tree_hover_major")

x, y = node("DG-PR-EX-Q")
verify.hover(x, y, 40)
verify.capture("tech_tree_hover_quest")

-- Plan mode: one press on a ring-1 minor queues it with its requirements.
verify.hover(20, 20, 2)
verify.panel_view("tech_tree", 1)
verify.frames(2)
local q0 = count(verify.tech_tree_queue())
verify.expect(q0 == 0, "queue starts empty (" .. q0 .. ")")
x, y = node("DG-PR-EX-A1")
verify.click(x, y)
verify.frames(2)
local q = verify.tech_tree_queue()
verify.expect(count(q) == 3, "one press queued 3 nodes (" .. count(q) .. ")")
verify.expect(q[1] == "DG-PR-EX-T1" and q[2] == "DG-PR-EX-T2" and q[3] == "DG-PR-EX-A1",
              "requirements queued ahead of the node, head first")
verify.hover(20, 20, 2)
verify.capture("tech_tree_plan_queue")

-- A pick-one pair: queue one side, then press the other.
x, y = node("DG-ST-AC-Aa")
verify.click(x, y)
verify.frames(2)
local before = count(verify.tech_tree_queue())
x, y = node("DG-ST-AC-Ab")
verify.click(x, y)
verify.frames(2)
verify.expect(count(verify.tech_tree_queue()) == before, "the pair's other side is refused, nothing queued")
local msg = verify.research_message()
verify.expect(msg:find("closed") ~= nil, "refusal says why: " .. msg)
verify.hover(x, y, 40)
verify.capture("tech_tree_plan_refused")
