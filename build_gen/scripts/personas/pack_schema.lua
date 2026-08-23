-- pack_schema.lua — pure-Lua validator for persona counsel packs (BL-207).
-- No external libs; deterministic; safe under the pack sandbox.

local M = {}

local BENCHES = { hearth = true, banner = true, mountain = true, verdict = true }
local VERDICTS = { SUFF = true, INSF = true, AMMIT = true, NOTYET = true, UNWG = true }

local function fail(fmt, ...) return nil, string.format(fmt, ...) end

--- validate(pack) -> true | nil, err
-- Checks the required pack fields and their types.
function M.validate(pack)
  if type(pack) ~= "table" then return fail("pack is not a table") end
  if type(pack.id) ~= "string" or #pack.id == 0 then return fail("pack.id missing") end
  if not BENCHES[pack.bench] then
    return fail("pack %s: bench %q not one of hearth/banner/mountain/verdict",
      pack.id, tostring(pack.bench))
  end
  if type(pack.seal) ~= "string" or #pack.seal == 0 then
    return fail("pack %s: seal missing", pack.id)
  end
  if type(pack.budget) ~= "number" or pack.budget ~= math.floor(pack.budget)
    or pack.budget <= 0 then
    return fail("pack %s: budget must be a positive integer", pack.id)
  end
  for _, fn in ipairs({ "extract", "policy", "phrase" }) do
    if type(pack[fn]) ~= "function" then
      return fail("pack %s: %s is not a function", pack.id, fn)
    end
  end
  if type(pack.failure_condition) ~= "string" or #pack.failure_condition < 40 then
    return fail("pack %s: failure_condition missing or too thin", pack.id)
  end
  if pack.bench == "verdict" and type(pack.aggregate) ~= "function" then
    return fail("pack %s: verdict bench requires aggregate()", pack.id)
  end
  return true
end

--- validate_opinion(rec) -> true | nil, err
-- Checks the codebook opinion grammar {p, m, on, c, w, v?}.
function M.validate_opinion(rec)
  if type(rec) ~= "table" then return fail("opinion is not a table") end
  if type(rec.p) ~= "string" or #rec.p == 0 then return fail("opinion.p missing") end
  if type(rec.m) ~= "string" or not rec.m:match("^%u%u%u$") then
    return fail("opinion.m must be a 3-letter move code, got %q", tostring(rec.m))
  end
  if rec.on == nil then return fail("opinion.on missing") end
  if type(rec.c) ~= "string" or #rec.c == 0 then return fail("opinion.c missing") end
  if #rec.c > 140 then return fail("opinion.c is %d chars (cap 140)", #rec.c) end
  if type(rec.w) ~= "number" or rec.w ~= math.floor(rec.w)
    or rec.w < 0 or rec.w > 3 then
    return fail("opinion.w must be an integer 0..3, got %s", tostring(rec.w))
  end
  if rec.v ~= nil and not VERDICTS[rec.v] then
    return fail("opinion.v %q not in SUFF/INSF/AMMIT/NOTYET/UNWG", tostring(rec.v))
  end
  return true
end

--- validate_verdict(rec) -> true | nil, err
-- Checks an aggregate() output record {v, on, baseline, c}.
function M.validate_verdict(rec)
  if type(rec) ~= "table" then return fail("verdict is not a table") end
  if not VERDICTS[rec.v] then
    return fail("verdict.v %q not in SUFF/INSF/AMMIT/NOTYET/UNWG", tostring(rec.v))
  end
  if rec.on == nil then return fail("verdict.on missing") end
  if rec.v ~= "UNWG" and (type(rec.baseline) ~= "string" or #rec.baseline == 0) then
    return fail("verdict.baseline must be a named string (UNWG alone may omit it)")
  end
  if type(rec.c) ~= "string" or #rec.c == 0 or #rec.c > 140 then
    return fail("verdict.c missing or over 140 chars")
  end
  return true
end

return M
