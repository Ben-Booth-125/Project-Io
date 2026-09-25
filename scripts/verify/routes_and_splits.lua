-- BL-1092 -- ROUTES AND SPLITS ON THE MAP: the migration shows its roads
-- (Ben, 2026-09-24, sprint-47 rulings R11; COLONISATION.md § The route record).
--
--   ProjectIo --verify scripts/verify/routes_and_splits.lua
--
-- WHAT THIS CHECKS, on the Culture round's own record:
--   R1  Each founding bakes a KIN ARROW from its people's previous region
--       (ui::history_lapse::kin_segs), dashed where the line crosses water.
--       The reference world's migration crosses none, so the hop capture is
--       taken on library seed 13 (part 2, at the end): the first over-water
--       arrow's year is read off the bake and the map is captured there, so
--       the dashed hop is in the frame rather than swept for.
--   R2  A recultured range comes apart AT the split year, not before: for
--       every `culture_split` the daughter holds no ground the year before
--       its line (the parent-then-daughter fold in build_migration_timelapse),
--       captured on the first split that took ground.
--   R3  Folded daughters leave the ticker and the board carries the census:
--       no ticker row names a folded daughter's split at the record's end, and
--       `history_census` reads the settlement's own coined / folded counts.
--   R4  The 400 BCE clamp is captured on an overrun seed by its sibling script,
--       culture_overrun.lua, which names the seed; this script only reads that
--       the reference world's record does not overrun.
--
-- The run is synchronous under --verify (history_run adopts the harness's own
-- record), playback is frozen, and `history_year` is the only thing that
-- moves the playhead. 1920x1080.

verify.window(1920, 1080)

local function shot(name)
    verify.frames(2)
    verify.capture(name)
end

verify.history_run(0)
verify.frames(4)
verify.capture("routes_and_splits_warmup")

local first, last = verify.history_span()
verify.expect(last > first, "the Culture record has a span")

-- ── R1 (part 1). The kin arrows on the reference world ───────────────────
verify.history_year(last)
verify.frames(2) -- a frame must draw the round before the bake exists
local drawn, water, first_water, total = verify.history_kin()
verify.expect(total > 0, "the record baked kin arrows (" .. tostring(total) .. ")")
-- A record adopted from a FINISHED report (this path) also carries the
-- regions the later spans founded, dated past the span's end; the playhead
-- never reaches them, so the arrows drawn by the last year are a prefix.
verify.expect(drawn > 0 and drawn <= total,
              "arrows are drawn by the span's end (" .. drawn .. " of " .. total .. ")")
-- Early in the span, where the arrows are dense: the routes as a picture.
-- (The reference world's migration crosses no water at all -- 0 of its
-- arrows dash under any rule -- so the hop capture is taken on seed 13 at
-- the end of this script, after the reference-world checks below.)
verify.history_year(first + math.floor((last - first) * 0.15))
shot("routes_and_splits_2_routes")

-- ── R2. A range comes apart at its split year, not before ────────────────
local splits = verify.history_splits()
verify.expect(#splits > 0, "the record carries culture_split events (" .. #splits .. ")")

local function holds(id, year)
    verify.history_year(year)
    for _, o in ipairs(verify.history_holders()) do
        if o == id then return true end
    end
    return false
end

local early, checked, captured = 0, 0, false
for _, sp in ipairs(splits) do
    if sp.year > first then
        checked = checked + 1
        if holds(sp.daughter, sp.year - 1) then early = early + 1 end
        if not captured and holds(sp.daughter, sp.year) then
            -- The first split that took ground: the frame before and the
            -- frame of the split line, so the hue change is seen to land ON
            -- the line.
            verify.history_year(sp.year - 1)
            shot("routes_and_splits_3_before_split")
            verify.history_year(sp.year)
            shot("routes_and_splits_4_at_split")
            captured = true
        end
    end
end
verify.expect(early == 0, "no daughter holds ground before its split line ("
                          .. early .. " of " .. checked .. " early)")
verify.expect(captured, "a split that took ground was captured")

-- ── R3. Folded daughters leave the ticker; the board carries the census ──
-- At the record's end (where the most splits have happened) and at a mark a
-- third of the way in: no ticker row is a folded daughter's split. The
-- `shown` flag is the ticker's own selection (lapse_ticker_rows), read at
-- the parked year, so this is the draw's filter under test, not a copy.
local function folded_on_ticker(year)
    verify.history_year(year)
    local n, shown_any = 0, 0
    for _, sp in ipairs(verify.history_splits()) do
        if sp.shown then shown_any = shown_any + 1 end
        if sp.shown and sp.folded then n = n + 1 end
    end
    return n, shown_any
end
local bad_end, shown_end = folded_on_ticker(last)
local bad_mid, shown_mid = folded_on_ticker(first + math.floor((last - first) / 3))
verify.expect(bad_end == 0 and bad_mid == 0,
              "no folded daughter's split is on the ticker (" .. bad_end .. " at the end, "
              .. bad_mid .. " at a third)")
verify.expect(shown_end + shown_mid > 0,
              "the ticker shows living splits (" .. shown_end .. " + " .. shown_mid .. ")")
verify.history_year(last)
local cradles, coined, folded, overrun = verify.history_census()
verify.expect(cradles > 0, "the census counts the cradles (" .. cradles .. ")")
verify.expect(coined > 0, "the census counts the peoples coined on the march ("
                          .. coined .. ")")
verify.expect(folded <= coined, "folded daughters are a subset of the coined")
verify.expect(overrun == 0, "the reference world's migration does not overrun 400 BCE")
shot("routes_and_splits_5_board")

-- Read the record's own fold flags against the ticker: at the record's end
-- every shown split line belongs to a living daughter. The prose names the
-- daughter's people, so a folded name appearing would be the tell; the
-- binding gives the flag directly, so the check is on the flag.
local folded_total = 0
for _, sp in ipairs(splits) do if sp.folded then folded_total = folded_total + 1 end end
verify.expect(folded_total == folded,
              "the census's folded count is the record's own (" .. folded_total
              .. " vs " .. folded .. ")")

-- ── R1 (part 2). The first hop over water, on a seed that has one ────────
-- LIBRARY SEED 13, "the colonial world" (docs/generation/seed_library.json),
-- MEASURED 2026-09-25: 58 of its 249 in-span kin arrows cross two or more
-- water tiles (the dash rule; the reference world's cross none), the first
-- at 2374 BCE. A full Release build (~1-2 min) through verify.new_world,
-- which also moves the wizard's pending descriptor so the round rasters
-- this world's regions over this world's coast.
local HOP_SEED = 13
verify.show_generation(false)
verify.new_world(HOP_SEED)
verify.history_run(0)
verify.frames(4)
verify.capture("routes_and_splits_warmup_hop")
local hf, hl = verify.history_span()
verify.history_year(hl)
verify.frames(2)
local hd, hw, hfirst, htotal = verify.history_kin()
verify.expect(hw > 0, "seed " .. HOP_SEED .. " carries over-water kin arrows (" .. hw
                      .. " of " .. hd .. " drawn)")
-- `hfirst` is a signed year (2374 BCE is -2374), meaningful only when the
-- water count is above zero.
if hw > 0 then
    -- The first hop's own year. On seed 13 that is 2374 BCE, inside the
    -- globe's dissolve (the opening tenth of the span), so the dashed line
    -- sits under a still-thick globe here; the frame after is the picture.
    verify.history_year(hfirst)
    shot("routes_and_splits_6_hop")
    local d2, w2 = verify.history_kin()
    verify.expect(w2 >= 1, "a dashed over-water arrow is drawn at its hop year ("
                           .. hfirst .. ")")
    -- A tenth of the span later, past the dissolve, where several hops are
    -- in the frame at once.
    verify.history_year(hfirst + math.floor((hl - hf) * 0.10))
    shot("routes_and_splits_7_hops_later")
else
    verify.expect(false, "seed " .. HOP_SEED .. " has no over-water hop: re-measure the seed")
end

verify.expect_no_clipping("routes_and_splits")
