-- BL-1241 (structures baked) — requirement group `structures-baked` row R3:
-- the chunk content hash folds installations and NOTHING tick-rate, so a
-- re-bake follows a construction event, never a tick. Steps the sim one day
-- at a time with a frame per day (under --verify every bake is synchronous)
-- and checks re-bakes PER CHUNK (F34 re-key): every chunk that re-baked on a
-- day must have had its OWN inputs move that day — its installation hash, or
-- (counted separately) its terrain — and a chunk whose own hashes held still
-- on a day the body's installation digest moved elsewhere must not re-bake.
-- verify.ground_chunk_probe() lists each ready MASTER chunk under the view
-- (tier 0 — BL-1246: one 96 px/hex master per body; a re-bake re-derives only
-- that chunk's mip pieces) plus the far page (tier -1) as
-- { tier, key, bakes, installations, region }.
--
-- The far page hashes the whole grid, so it re-bakes on every day anything
-- on the body moved; its cost is MEASURED here (bakes per 30 days, bake ms
-- from ground_stats slot 0 — per 512 px piece since BL-1246), not judged.
--
-- A play year is 365 days; a Debug econ tick on the reference world runs
-- seconds and the background firms roughly double the building count in a
-- month, so the default span is 30 days. Pass a longer one by editing `days`.
verify.goto_surface("home")
verify.set_overlay("none")
local g = verify.stage_gallery()
verify.center_tile(g.col, g.row, 5)
verify.frames(3)
local days = 30

local function index(probe)
    local t = {}
    for _, r in ipairs(probe) do
        t[tostring(r.tier) .. ":" .. string.format("%.0f", r.key)] = r
    end
    return t
end

verify.ground_stats_reset()
local s0 = verify.ground_bake_counters()
local prev_digest = s0.installations
local before = index(verify.ground_chunk_probe())

local moved_days = 0
local chunk_rebakes, inst_rebakes, terrain_rebakes, unexplained = 0, 0, 0, 0
local quiet_chunk_days, quiet_rebaked = 0, 0
local far_rebakes, far_unexplained = 0, 0
local chunks_seen = 0
for d = 1, days do
    verify.econ_step(1)
    verify.frames(1)
    local counters = verify.ground_bake_counters()
    local digest_moved = counters.installations ~= prev_digest
    if digest_moved then moved_days = moved_days + 1 end
    prev_digest = counters.installations

    local after = index(verify.ground_chunk_probe())
    for k, a in pairs(after) do
        local b = before[k]
        if b then
            local db = a.bakes - b.bakes
            local inst_moved = a.installations ~= b.installations
            local reg_moved  = a.region ~= b.region
            if a.tier < 0 then
                if db > 0 then
                    far_rebakes = far_rebakes + 1
                    if not reg_moved then far_unexplained = far_unexplained + 1 end
                end
            else
                chunks_seen = chunks_seen + 1
                if db > 0 then
                    chunk_rebakes = chunk_rebakes + 1
                    if inst_moved then
                        inst_rebakes = inst_rebakes + 1
                    elseif reg_moved then
                        terrain_rebakes = terrain_rebakes + 1
                    else
                        unexplained = unexplained + 1
                        print(string.format("REBAKE unexplained day %d chunk %s (bakes %d -> %d)",
                                            d, k, b.bakes, a.bakes))
                    end
                end
                if digest_moved and not inst_moved and not reg_moved then
                    quiet_chunk_days = quiet_chunk_days + 1
                    if db > 0 then quiet_rebaked = quiet_rebaked + 1 end
                end
            end
        end
    end
    before = after
end

local s1 = verify.ground_bake_counters()
local st = verify.ground_stats()
local far = st.slots[0]
print(string.format("structures rebake: %d days, installation digest moved on %d; buildings %d -> %d",
    days, moved_days, s0.buildings, s1.buildings))
print(string.format("REBAKE chunk-days observed %d; chunk re-bakes %d = installation %d + terrain-only %d + unexplained %d",
    chunks_seen, chunk_rebakes, inst_rebakes, terrain_rebakes, unexplained))
print(string.format("REBAKE chunk-days where the body digest moved but the chunk's own hashes held: %d (re-baked: %d)",
    quiet_chunk_days, quiet_rebaked))
print(string.format("REBAKE far page: %d re-bakes in %d days (%.1f per 30 days), %d without a region move; far bake ms %.1f over %d bakes (%.1f ms each)",
    far_rebakes, days, far_rebakes * 30.0 / days, far_unexplained,
    far.bake_ms, far.bakes, far.bakes > 0 and far.bake_ms / far.bakes or 0))
verify.expect(unexplained == 0, "every chunk re-bake follows a move in that chunk's own inputs")
verify.expect(quiet_rebaked == 0, "no chunk re-baked on a day the digest moved elsewhere but its own hashes held")
verify.expect(far_unexplained == 0, "the far page re-bakes only when its region hash moved")
