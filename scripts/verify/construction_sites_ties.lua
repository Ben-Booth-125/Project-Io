-- How often the site ledger's rows TIE on estimated net (Ben, 2026-10-25). A
-- measurement, not a check: it asserts nothing and prints one line per (seed,
-- building) and a per-seed summary, for Ben's decision on whether province-level
-- local prices are worth building to break ties.
--
-- A tie is a row whose DISPLAYED net (whole credits) another row shares. Three
-- readings, each with the ledger's own figures (construction_sites.cpp):
--   full   - the estimate at full Logistic Point cover (the per-tile factors before
--            this item: deposit richness and reserve, stacks; prices per market)
--   cover  - the same with each tile's routed Logistic Point cover applied
--   +cost  - net and cost together (cost carries the landform / reach / stack site
--            multiplier), i.e. the ties left if cost were the second key
-- Labour reach is not in the estimate (it assumes no labour shortage), so it cannot
-- break a tie here.
--
-- Run with: ProjectIo --verify scripts/verify/construction_sites_ties.lua

local seeds = { 0, 46, 28 }

-- One pass over every producing option, summed. `tag` names the pass.
local function pass(seed, tag)
  local rows, t_full, t_cover, t_cost, flagged, cov, t_cov = 0, 0, 0, 0, 0, 0, 0
  for _, o in ipairs(verify.site_options()) do
    if o.category == "Extraction" or o.category == "Processing" then
      verify.site_pick(o.name)
      verify.frames(1)
      local s = verify.site_finder()
      if s.n_rows > 0 then
        print(string.format("TIES seed %d %s %-28s %-10s rows %4d  full %4d  cover %4d  +cost %4d  "
              .. "no-LP %4d  covered %4d (tied %4d)  eval %.1f ms",
              seed, tag, o.name, o.category, s.n_rows, s.ties_full_cover, s.ties_profit,
              s.ties_profit_cost, s.no_lp_rows, s.covered_rows, s.ties_covered, s.eval_ms))
        rows    = rows + s.n_rows
        t_full  = t_full + s.ties_full_cover
        t_cover = t_cover + s.ties_profit
        t_cost  = t_cost + s.ties_profit_cost
        flagged = flagged + s.no_lp_rows
        cov     = cov + s.covered_rows
        t_cov   = t_cov + s.ties_covered
      end
    end
  end
  print(string.format("TIES_SUMMARY seed %d %s: %d rows; tied at full cover %.1f%%, with cover %.1f%%, "
        .. "net+cost %.1f%%; no-LP rows %.1f%%; among the %d covered rows %.1f%% tie",
        seed, tag, rows, 100 * t_full / math.max(1, rows), 100 * t_cover / math.max(1, rows),
        100 * t_cost / math.max(1, rows), 100 * flagged / math.max(1, rows), cov,
        100 * t_cov / math.max(1, cov)))
  verify.site_clear()
end

for si, seed in ipairs(seeds) do
  if si > 1 then verify.new_world(seed) end
  verify.goto_surface("home")
  verify.econ_step(2)
  verify.set_balance(5000000.0)
  verify.show_panel("build", true)
  verify.frames(3)
  pass(seed, "as-generated")
  -- The --verify world can give the seat no running source (BL-1410), which reads as
  -- every row at zero cover. A second pass with ONE connected hub staged beside the
  -- capital shows how a real cover field splits the rows.
  local cs = verify.capital_stars()
  if cs.x then
    verify.stage_building(cs.x, cs.y - 1, "logistics_hub")
    verify.econ_step(1)
    verify.set_balance(5000000.0)
    verify.frames(2)
    pass(seed, "hub-staged")
  end
end
