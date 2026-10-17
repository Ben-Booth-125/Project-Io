-- BL-1311 (ground paint perf) — the paint in miniature. NOT a capture check:
-- it takes no captures and asserts nothing. A reading, for comparing builds
-- and changes to the bake without a generation run (--bench-ground is the
-- whole-app reading; this one starts in seconds).
--
-- The home body's whole master is baked on the pool against a snapshot with
-- every third installed tile stripped of its installation, then the true
-- snapshot lands and the master re-bakes as a round boundary that adds
-- installations makes it (the partial re-bake: windows). ground_layer prints
-- GROUND_PAINT lines with the worker time per pass; this prints the totals.
--
-- Kept out of scripts/verify/ so --verify-all does not pay a whole master.
-- Run: ProjectIo --verify scripts/bench/ground_paint_bench.lua

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
-- The top rung first, so the frame below bakes only a handful of chunks.
verify.set_zoom(20)
verify.frames(2)
local r = verify.ground_paint_bench(3)
print(string.format("GROUND_PAINT_BENCH ok=%s first bake %.1f s, re-bake %.1f s (%d of %d chunks moved; %d installations dropped)",
                    tostring(r.ok), r.first_ms / 1000, r.rebake_ms / 1000, r.moved, r.chunks, r.dropped))
