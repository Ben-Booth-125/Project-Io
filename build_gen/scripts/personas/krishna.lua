-- krishna.lua — mountain bench, move ALN (alignment; the arithmetic of standing).
-- "Before one spear is counted — who stands with whom, and which side wins merely
--  because we stand there?" Also: is this corp acting per its own declared line?
-- Compiled from Pantheon/data/personas/krishna.md (BL-207 slice 1).

local BUDGET = 512

local function hash(s)
  local h = 5381
  for i = 1, #s do
    h = (h * 31 + string.byte(s, i)) % 1000003
  end
  return h
end

local function pick(bank, key) return bank[(hash(key) % #bank) + 1] end

-- Bucketed by finding kind; selection within a bucket is deterministic by hash.
local phrase_bank = {
  ["pivotal.seat"] = {
    "%s is the seat nobody thought to fight over. Its occupant decides the field.",
    "Both sides stand at %s. Whoever is beside the other first becomes the winner.",
    "%s: presence there makes a side the winner before anything is fought.",
    "Ignore the stockpiles at %s. Draw the map of who stands where; the spears are a formality.",
    "Give away what can be counted at %s; keep only the seat that decides.",
  },
  misaligned = {
    "At %s we build against our own line. Duryodhana also chose mass over meaning.",
    "The million spears at %s feel like certainty. Certainty is what Duryodhana took.",
    "Count nothing at %s that can be counted. Keep the one thing that decides.",
    "You are weeping over %s, which means you have understood.",
  },
  generic = {
    "I will hold the reins at %s and bear no weapon. That is the whole of my office.",
    "Price every ally at %s in obligation and defection before the first spear moves.",
    "%s serves the declared duty. Ride on — the war there is already over; we go merely to inform it.",
  },
}

local function extract(facts)
  local findings, seen = {}, 0
  local type_count, own_at, rival_at = {}, {}, {}
  local own_total = 0
  for i = 1, #facts do
    if seen >= BUDGET then break end
    local f = facts[i]
    if type(f) == "table" and type(f.predicate) == "string" then
      seen = seen + 1
      local subj = tostring(f.subject or "?")
      if f.predicate == "building.type" then
        if f.provenance == "own_asset" then
          local ty = tostring(f.value)
          type_count[ty] = (type_count[ty] or 0) + 1
          own_total = own_total + 1
          own_at[subj] = ty
        elseif f.provenance == "rival_visible" then
          rival_at[subj] = true
        end
      end
    end
  end
  -- The declared line: the corp's dominant building type (its industrial focus in the data).
  local line, line_n = nil, 0
  for ty, n in pairs(type_count) do
    if n > line_n or (n == line_n and line and ty < line) then line, line_n = ty, n end
  end
  if line and own_total > 0 then
    for subj, ty in pairs(own_at) do
      if ty ~= line then
        findings[#findings + 1] = { kind = "misaligned", subject = subj,
          strength = 1 - (type_count[ty] or 0) / own_total,
          evidence = string.format("%s built off the %s line", ty, line) }
      end
    end
  end
  for subj in pairs(rival_at) do
    if own_at[subj] then
      findings[#findings + 1] = { kind = "pivotal.seat", subject = subj,
        strength = 0.8, evidence = "both sides stand here; the seat decides the field" }
    end
  end
  table.sort(findings, function(a, b)
    if a.strength ~= b.strength then return a.strength > b.strength end
    return tostring(a.subject) < tostring(b.subject)
  end)
  return findings
end

local function policy(findings)
  local out = {}
  for _, fd in ipairs(findings) do
    local w, c
    if fd.kind == "pivotal.seat" then
      w = 2
      c = string.format("seat %s decides the field: %s; stand there first", fd.subject,
        fd.evidence)
    else
      w = fd.strength >= 0.7 and 2 or 1
      c = string.format("%s strays from duty (%s); mass over meaning is how the world is lost",
        fd.subject, fd.evidence)
    end
    out[#out + 1] = { p = "krishna", m = "ALN", on = "building#" .. tostring(fd.subject),
      c = c:sub(1, 140), w = w }
  end
  return out
end

local function phrase(x)
  local subj = tostring(x.subject or x.on or "?")
  local kind = tostring(x.kind or x.m or "")
  local bank = phrase_bank[kind] or phrase_bank.generic
  local line = pick(bank, subj .. kind)
  return (line:gsub("%%s", subj, 1))
end

return {
  id = "krishna",
  bench = "mountain",
  seal = "Count who stands where; the spears are a formality.",
  budget = BUDGET,
  extract = extract,
  policy = policy,
  phrase = phrase,
  failure_condition = "Krishna reads only the arithmetic of standing — who is beside whom, "
    .. "which seat decides — and dismisses the soldiers as formality. A player who wins by "
    .. "counted mass, not position — overwhelming logistics, raw output, spears that simply "
    .. "arrive in numbers no seat can outweigh — refutes him: he promised no victory to "
    .. "those who choose mass over meaning, so when mass wins anyway he has nothing to say. "
    .. "Beat him with the army Duryodhana took, wielded without needing him to be wrong.",
}
