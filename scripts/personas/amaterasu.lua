-- amaterasu.lua — mountain bench, move WDR (withdrawal leverage).
-- "What goes dark the day we are simply not there — and who bears it longer?"
-- Compiled from Pantheon/data/personas/amaterasu.md (BL-207 slice 1).

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
  ["absence.cost"] = {
    "The day we are not at %s, this goes dark: count it before anyone threatens to leave.",
    "I write the ledger of absence for %s before the door is touched.",
    "Withdrawal from %s is a weapon that fires backward. Here are its casualties.",
    "%s cannot bear our absence a single season. That is leverage — spend it unopened.",
    "I refuse to bless a withdrawal from %s whose cost lands on the innocent.",
  },
  ["already.dark"] = {
    "%s has gone stale. Someone already left, and the dark fell where it pleased.",
    "Eight hundred gods could not reopen %s once the famine had chosen its farmers.",
    "The dark at %s fell on everyone but the offender. Absence does not aim.",
  },
  illuminated = {
    "%s is lit by our routes alone. Absence there does not aim — it falls on everyone.",
    "What is illuminated at %s grows; what we stop lighting, demons take.",
    "Who laughs at %s in a world we darkened? Look — it is our own trade going on without us.",
  },
  generic = {
    "Before pricing a fight at %s, price the day of simply being elsewhere.",
    "%s: I ask not who would win, but what goes dark the day we are simply not there.",
  },
}

local function extract(facts)
  local findings, seen = {}, 0
  local volume, activity = {}, {}
  for i = 1, #facts do
    if seen >= BUDGET then break end
    local f = facts[i]
    if type(f) == "table" and type(f.predicate) == "string" then
      seen = seen + 1
      local subj = tostring(f.subject or "?")
      if f.predicate:sub(1, 6) == "route." and type(f.value) == "number" then
        volume[subj] = (volume[subj] or 0) + f.value
      elseif f.predicate == "body.activity" then
        activity[subj] = tostring(f.value)
      end
    end
  end
  local max_v = 0
  for _, v in pairs(volume) do if v > max_v then max_v = v end end
  for subj, v in pairs(volume) do
    if max_v > 0 and v > 0 then
      findings[#findings + 1] = { kind = "absence.cost", subject = subj,
        strength = v / max_v,
        evidence = string.format("route volume %g; goes dark without us", v) }
    end
  end
  for subj, tier in pairs(activity) do
    if tier == "Stale" or tier == "Unknown" then
      findings[#findings + 1] = { kind = "already.dark", subject = subj,
        strength = tier == "Unknown" and 0.7 or 0.4,
        evidence = "activity tier " .. tier .. "; the light left before we did" }
    elseif tier == "Visible" then
      findings[#findings + 1] = { kind = "illuminated", subject = subj,
        strength = 0.6, evidence = "fully lit by our presence" }
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
    local w, c = 1, nil
    if fd.kind == "absence.cost" then
      w = fd.strength >= 0.7 and 2 or 1
      c = string.format("ledger of absence, %s: %s; who bears it longer than we bear withholding?",
        fd.subject, fd.evidence)
    elseif fd.kind == "already.dark" then
      c = string.format("%s already dark (%s); withdrawal there aims at no one", fd.subject,
        fd.evidence)
    else
      c = string.format("%s lit wholly by us; leverage held best unspent", fd.subject)
    end
    out[#out + 1] = { p = "amaterasu", m = "WDR", on = "body#" .. tostring(fd.subject),
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
  id = "amaterasu",
  bench = "mountain",
  seal = "She shut the door once; now she prices every absence.",
  budget = BUDGET,
  extract = extract,
  policy = policy,
  phrase = phrase,
  failure_condition = "Amaterasu prices only what her own light touches: routes recorded, "
    .. "activity already lit. Absence does not aim — and neither does her ledger. A player "
    .. "who builds where she has never shone (no routes, no visible activity) accrues "
    .. "position she literally cannot cost; by the time the new ground registers on the "
    .. "blackboard, the withdrawal leverage she counsels hoarding is already worthless. "
    .. "Beat her in the dark, where there is nothing for her to withhold.",
}
