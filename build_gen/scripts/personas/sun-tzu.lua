-- sun-tzu.lua — mountain bench, move DTR (deterrence and position).
-- "Where was this war decided before it began?" Counts ground, not battles.
-- Compiled from Pantheon/data/personas/sun-tzu.md (BL-207 slice 1).

local BUDGET = 512

-- Deterministic string hash (no math.random anywhere in a pack).
local function hash(s)
  local h = 5381
  for i = 1, #s do
    h = (h * 31 + string.byte(s, i)) % 1000003
  end
  return h
end

local function pick(bank, key) return bank[(hash(key) % #bank) + 1] end

-- Bucketed by finding kind so the chosen line always matches the meaning;
-- selection within a bucket is deterministic by hash.
local phrase_bank = {
  ["ground.strong"] = {
    "At %s the rival stands thick. Do not test held ground — be elsewhere.",
    "The ground at %s was decided before anyone marched, and not for us.",
    "Every contested tile at %s is a battle waiting to confess failure.",
    "I refuse any plan for %s whose success requires the rival to be foolish.",
  },
  ["ground.weak"] = {
    "The rival at %s is visible and thin. Position, and no battle is needed.",
    "Hold %s so visibly that no one tests it.",
    "Supreme excellence at %s: subdue without a single building razed.",
    "Where you are strong at %s, appear absent; where absent, appear strong.",
  },
  ["ground.unscouted"] = {
    "%s is unscouted ground. He who counts first has already won it.",
    "Count %s: supply, season, exits. The counting is the war.",
    "The exits from %s are unknown. Never take ground you cannot leave.",
  },
  generic = {
    "%s asks for a demonstration. Ask King Helü what demonstrations cost.",
    "The war over %s was decided before it began. I am only reading the decision.",
  },
}

local function extract(facts)
  local findings, seen = {}, 0
  local own, rival, unscouted = {}, {}, {}
  for i = 1, #facts do
    if seen >= BUDGET then break end
    local f = facts[i]
    if type(f) == "table" and type(f.predicate) == "string" then
      seen = seen + 1
      local subj = tostring(f.subject or "?")
      if f.predicate:sub(1, 9) == "building." then
        if f.provenance == "own_asset" then
          own[subj] = (own[subj] or 0) + 1
        elseif f.provenance == "rival_visible" then
          rival[subj] = (rival[subj] or 0) + 1
          if (f.confidence or 0) < 0.5 then unscouted[subj] = true end
        end
      end
    end
  end
  for subj, r in pairs(rival) do
    local o = own[subj] or 0
    local total = o + r
    if r > o then
      findings[#findings + 1] = { kind = "ground.strong", subject = subj,
        strength = r / total,
        evidence = string.format("rival %d vs own %d buildings", r, o) }
    else
      findings[#findings + 1] = { kind = "ground.weak", subject = subj,
        strength = o / total,
        evidence = string.format("own %d vs rival %d buildings", o, r) }
    end
  end
  for subj in pairs(unscouted) do
    findings[#findings + 1] = { kind = "ground.unscouted", subject = subj,
      strength = 0.4, evidence = "rival presence seen at low confidence" }
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
    local w = fd.strength >= 0.8 and 2 or 1
    local c
    if fd.kind == "ground.strong" then
      c = string.format("ground %s already decided against us (%s); do not test it",
        fd.subject, fd.evidence)
    elseif fd.kind == "ground.weak" then
      c = string.format("ground %s favours us (%s); hold it visibly, no battle needed",
        fd.subject, fd.evidence)
    else
      c = string.format("ground %s uncounted; count before any plan touches it", fd.subject)
      w = 1
    end
    out[#out + 1] = { p = "sun-tzu", m = "DTR", on = "body#" .. tostring(fd.subject),
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
  id = "sun-tzu",
  bench = "mountain",
  seal = "Wins wars before they begin; every battle fought confesses failure.",
  budget = BUDGET,
  extract = extract,
  policy = policy,
  phrase = phrase,
  failure_condition = "Sun Tzu counts position and refuses every plan that requires the "
    .. "rival to be foolish — so he cannot be maneuvered, only forced. A player who "
    .. "offers no position to read (no visible buildings, no counted ground) and simply "
    .. "strikes — the two beheadings before King Helü — walks straight through him: his "
    .. "doctrine of not-fighting has no answer to an opponent who accepts the cost of "
    .. "fighting anyway. Beat him with the sudden, uneconomical, already-paid-for blow.",
}
