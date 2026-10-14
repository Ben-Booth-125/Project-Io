-- BL-1315 (MARKET_NAMES_UNIQUE): every market on a body carries its own name.
--
-- MARKETS.md § Market centres and seeding: a market is named for the largest
-- population centre whose tile clears to it (`market_for_tile`); a market with
-- no centre in its catchment reads "<body> market N", never the bare body name.
-- Prints the distinct-name count per body and fails if two markets on one body
-- share a name or any market reads as its bare body name.
--
--   ProjectIo --verify scripts/verify/market_names.lua

verify.goto_surface("home")
verify.econ_step(2)

local total, dupes, bare, fallback = 0, 0, 0, 0
for _, b in ipairs(verify.market_bodies()) do
    local seen, n, distinct = {}, 0, 0
    for _, mid in ipairs(verify.markets_on_body(b.body)) do
        local name = verify.market_name(mid)
        n = n + 1
        if seen[name] then
            dupes = dupes + 1
        else
            seen[name] = true
            distinct = distinct + 1
        end
        if name == b.name then bare = bare + 1 end
        if name:find(" market %d+$") then fallback = fallback + 1 end
    end
    total = total + n
    print(string.format("market_names: body %s (%d): %d markets, %d distinct names",
                        b.name, b.body, n, distinct))
end
print(string.format("market_names: %d markets, %d duplicate names, %d bare body names, %d \"<body> market N\"",
                    total, dupes, bare, fallback))
assert(dupes == 0, "market_names: two markets on one body share a name")
assert(bare == 0, "market_names: a market reads as its bare body name")
