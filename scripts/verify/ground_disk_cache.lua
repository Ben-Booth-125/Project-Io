-- BL-1259 (ground disk cache) — RENDERING.md § Chunks, cache and invalidation,
-- the resident-set bullet: the home master is pinned in RAM; another body's
-- master that must leave RAM spills to a disk cache and streams back on a
-- revisit, re-baking only the chunks whose content hash moved; a missing or
-- corrupt file falls back to a re-bake; the cache is safe to delete.
--
-- Rows (the --verify SYNCHRONOUS path: every spill is written on the main
-- thread and every restore waited for, so the run is deterministic):
--   R1 a spilled-then-restored master is BYTE-IDENTICAL to the one that left
--      (a digest over every ready chunk's master piece and all its mip pieces),
--      its RAM was freed, every chunk came back as a load and none re-baked;
--   R2 a corrupted chunk file falls back to a re-bake, silently, and the
--      result is still byte-identical; a re-spill rewrites nothing unchanged;
--   R3 a chunk whose content hash moved while spilled (a building staged on
--      it) re-bakes instead of loading, and the restored master equals a
--      from-scratch bake of the moved world;
--   R4 the home body is pinned: a zero RAM budget never takes it.
--
-- Run: ProjectIo --verify scripts/verify/ground_disk_cache.lua
-- (the live pool path's timings are ground_disk_cache_bench.lua's).

local kMinZoom = 1.2531328 -- body_surface_canvas.cpp kMinZoom

local function counters() return verify.ground_bake_counters() end
local function digest() return verify.ground_master_digest() end
local function line(fmt, ...) print(string.format("[ground_disk_cache] " .. fmt, ...)) end

verify.window(1720, 1080)
verify.goto_surface("home")
verify.set_overlay("none")
local g = verify.stage_gallery()
verify.center_tile(g.col, g.row, kMinZoom * 4)
verify.frames(3)

-- R1: spill, restore, byte-identical.
local d0 = digest()
verify.expect(d0.chunks > 0, "R1: the view baked master chunks to spill")
local c0 = counters()
local sp = verify.ground_spill()
verify.expect(sp.written == d0.chunks, string.format("R1: spill wrote every ready chunk (%d of %d)",
                                                     sp.written, d0.chunks))
local st = verify.ground_stats()
verify.expect(st.slots[1].ram_bytes == 0, "R1: the master's RAM is freed after the spill")
local cache = verify.ground_cache()
local home_id = verify.ground_body_id("home")
local entry
for _, b in ipairs(cache.bodies) do if b.body == home_id then entry = b end end
verify.expect(entry ~= nil and entry.files == d0.chunks and entry.bytes > 0,
              "R1: the cache holds one file per spilled chunk")
line("R1 cache root %s; %d files, %.1f MB on disk for %.1f MB raw (ratio %.3f)", cache.root,
     entry and entry.files or -1, (entry and entry.bytes or 0) / 1048576, (entry and entry.raw_bytes or 0) / 1048576,
     entry and entry.raw_bytes > 0 and entry.bytes / entry.raw_bytes or 0)
local r = verify.ground_restore()
local c1 = counters()
verify.expect(r.ok, "R1: restore completed")
verify.expect(r.loaded == d0.chunks and r.rebaked == 0 and r.failed == 0,
              string.format("R1: every chunk loaded (loaded %d rebaked %d failed %d of %d)",
                            r.loaded, r.rebaked, r.failed, d0.chunks))
verify.expect(c1.chunk_bakes == c0.chunk_bakes, "R1: nothing re-baked")
local d1 = digest()
verify.expect(d1.digest == d0.digest and d1.chunks == d0.chunks,
              string.format("R1: restored master byte-identical (%s vs %s)", d1.digest, d0.digest))
line("R1 %d chunks spilled and restored in %.0f ms; digest %s == %s", d0.chunks, r.ms, d1.digest, d0.digest)

-- R2: a corrupt file falls back to a re-bake.
local bad = verify.ground_cache_corrupt()
verify.expect(bad >= 0, "R2: a cached chunk file was corrupted")
local sp2 = verify.ground_spill()
verify.expect(sp2.written == 0 and sp2.skipped == d0.chunks,
              string.format("R2: a re-spill of an unchanged master writes nothing (written %d skipped %d)",
                            sp2.written, sp2.skipped))
local c2 = counters()
local r2 = verify.ground_restore()
local c3 = counters()
verify.expect(r2.ok and r2.failed == 1 and r2.loaded == d0.chunks - 1,
              string.format("R2: the corrupt chunk failed its load (loaded %d failed %d)", r2.loaded, r2.failed))
verify.expect(c3.chunk_bakes - c2.chunk_bakes == 1, "R2: exactly the corrupt chunk re-baked")
local d2 = digest()
verify.expect(d2.digest == d0.digest, "R2: the fallback bake is byte-identical")
line("R2 chunk %d corrupted: %d loaded, %d re-baked; digest %s", bad, r2.loaded, c3.chunk_bakes - c2.chunk_bakes, d2.digest)

-- R3: a moved hash re-bakes; the result equals a fresh bake of the moved world.
local sp3 = verify.ground_spill()
verify.expect(sp3.written == 1, string.format("R3: only the re-baked chunk is rewritten (%d)", sp3.written))
local staged = verify.stage_building(g.col + 2, g.row, "extraction", "iron_ore")
verify.expect(staged ~= 0, "R3: a building staged under the view")
local c4 = counters()
local r3 = verify.ground_restore()
local c5 = counters()
verify.expect(r3.ok and r3.rebaked >= 1 and r3.failed == 0 and r3.loaded + r3.rebaked == d0.chunks,
              string.format("R3: moved chunks re-baked, the rest loaded (loaded %d rebaked %d failed %d)",
                            r3.loaded, r3.rebaked, r3.failed))
verify.expect(c5.load_skipped - c4.load_skipped == r3.rebaked, "R3: a moved chunk is never loaded")
local d3 = digest()
verify.expect(d3.digest ~= d0.digest, "R3: the staged building moved the ground")
verify.ground_drop()
verify.frames(1) -- a from-scratch bake of the same view
local d4 = digest()
verify.expect(d4.digest == d3.digest and d4.chunks == d3.chunks,
              string.format("R3: restored == fresh bake (%s vs %s, %d vs %d chunks)",
                            d3.digest, d4.digest, d3.chunks, d4.chunks))
line("R3 %d moved chunks re-baked, %d loaded; restored %s == fresh %s", r3.rebaked, r3.loaded, d3.digest, d4.digest)

-- R4: the home body is pinned.
local prev = verify.ground_set_ram_budget(0)
verify.goto_surface("moon")
verify.frames(2)
verify.goto_surface("home")
verify.frames(2)
local cache2 = verify.ground_cache()
local home_entry
for _, b in ipairs(cache2.bodies) do if b.body == home_id then home_entry = b end end
verify.expect(home_entry ~= nil and home_entry.pinned and home_entry.resident,
              "R4: home stays resident under a zero budget")
local d5 = digest()
verify.expect(d5.chunks >= d4.chunks, "R4: home's chunks were not dropped")
verify.ground_set_ram_budget(prev)
line("R4 home pinned and resident under a 0 GB budget (%d chunks)", d5.chunks)

line("PASS")
