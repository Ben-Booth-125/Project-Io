-- scales-of-maat.lua — verdict bench, move VRD. Gathers nothing; only weighs.
-- "Compared to what — and when the beam settles, is this sufficient to act?"
-- Compiled from Pantheon/data/personas/scales-of-maat.md (BL-207 slice 1).

local BUDGET = 512

local function hash(s)
  local h = 5381
  for i = 1, #s do
    h = (h * 31 + string.byte(s, i)) % 1000003
  end
  return h
end

local function pick(bank, key) return bank[(hash(key) % #bank) + 1] end

-- Bucketed by verdict code; selection within a bucket is deterministic by hash.
local phrase_bank = {
  SUFF = {
    "The feather for %s is named. The beam settles: sufficient. Act.",
    "Eloquence does not move the beam on %s. Only mass does — and the mass is here.",
  },
  INSF = {
    "On %s the record is thin. Insufficient — return to the hunt.",
    "I gather nothing on %s; that labor belongs to the hunters' benches. Bring more.",
  },
  AMMIT = {
    "The claim on %s is heavier than the feather. Ammit is fed; the case is closed.",
    "%s is devoured. Worse than the null itself; write it dead.",
  },
  NOTYET = {
    "%s is held at the door. Not weighed yet; bring mass, not ornament.",
    "Two hearts disagree on %s. I weigh them against the feather, not each other.",
  },
  UNWG = {
    "No feather can be named for %s. Unweighable — the beam swings free.",
    "Compared to what, %s? This heart answers: nothing. Write that the beam once held level.",
  },
  generic = {
    "Compared to what, %s? Every heart must answer before I weigh it.",
    "Judgment that joins the fray at %s becomes a combatant, and a combatant cannot acquit.",
  },
}

-- The Scales gather nothing: extract is the empty office (persona-runtime.md).
local function extract(facts) ---@diagnostic disable-line: unused-local
  return {}
end

-- The Scales emit opinions only through aggregate; policy passes verdicts through.
local function policy(findings) ---@diagnostic disable-line: unused-local
  return {}
end

--- aggregate(opinions) -> array of verdict records {v, on, baseline, c}.
-- Weighs the benches' opinion records per subject ("on"); resolves opposed moves
-- on the same subject by explicit verdict. Deterministic; bounded by BUDGET.
local function aggregate(opinions)
  local by_on, order = {}, {}
  for i = 1, math.min(#opinions, BUDGET) do
    local o = opinions[i]
    if type(o) == "table" and o.on ~= nil and type(o.w) == "number" then
      local key = tostring(o.on)
      if not by_on[key] then by_on[key] = {}; order[#order + 1] = key end
      local bucket = by_on[key]
      bucket[#bucket + 1] = o
    end
  end
  table.sort(order)
  local verdicts = {}
  for _, key in ipairs(order) do
    local bucket = by_on[key]
    -- The feather: the heaviest single claim names the baseline standard.
    local mass, top, moves = 0, nil, {}
    for _, o in ipairs(bucket) do
      mass = mass + o.w
      moves[o.m] = (moves[o.m] or 0) + o.w
      if not top or o.w > top.w or (o.w == top.w and tostring(o.p) < tostring(top.p)) then
        top = o
      end
    end
    local opposed = 0
    for _ in pairs(moves) do opposed = opposed + 1 end
    local baseline = top and string.format("%s per %s", top.m, tostring(top.p)) or nil
    local v, c
    if not top or top.w == 0 then
      v, baseline = "UNWG", nil
      c = key .. ": no feather named; the beam swings free"
    elseif opposed >= 2 then
      -- Benches disagree: the heavier move carries, explicitly.
      local best_m, best_w = nil, -1
      for m, w in pairs(moves) do
        if w > best_w or (w == best_w and m < best_m) then best_m, best_w = m, w end
      end
      local rest = mass - best_w
      if best_w > rest then
        v = "SUFF"
        c = string.format("%s: benches opposed; %s outweighs (%d vs %d); act", key,
          best_m, best_w, rest)
      elseif best_w == rest then
        v = "NOTYET"
        c = key .. ": the beam holds level between opposed benches; held at the door"
      else
        v = "INSF"
        c = key .. ": disagreement heavier than any single claim; return to the hunt"
      end
    elseif mass >= 3 then
      v = "SUFF"
      c = string.format("%s: mass %d against the feather; sufficient to act", key, mass)
    elseif mass >= 2 then
      v = "NOTYET"
      c = string.format("%s: mass %d; not weighed yet, bring more", key, mass)
    else
      v = "INSF"
      c = string.format("%s: the record is thin (mass %d); insufficient", key, mass)
    end
    verdicts[#verdicts + 1] = { v = v, on = key, baseline = baseline, c = c:sub(1, 140) }
  end
  return verdicts
end

local function phrase(x)
  local subj = tostring(x.subject or x.on or "?")
  local kind = tostring(x.v or x.kind or x.m or "")
  local bank = phrase_bank[kind] or phrase_bank.generic
  local line = pick(bank, subj .. kind)
  return (line:gsub("%%s", subj, 1))
end

return {
  id = "scales-of-maat",
  bench = "verdict",
  seal = "Weighs claims against the feather; ends examinations: sufficient, insufficient, or devoured.",
  budget = BUDGET,
  extract = extract,
  policy = policy,
  phrase = phrase,
  aggregate = aggregate,
  failure_condition = "The Scales refuse to weigh where no feather is named, and refuse to "
    .. "gather evidence themselves — so any situation the hunting benches never file "
    .. "returns UNWG and the examination cannot conclude. A player who keeps their true "
    .. "line of play off every bench's lens (novel, unbaselined, filed under nothing) "
    .. "faces a corp whose court can only swing its beam free: refusal is a verdict, and "
    .. "verdicts require a feather. Beat the Scales with Isfet — the unweighable.",
}
