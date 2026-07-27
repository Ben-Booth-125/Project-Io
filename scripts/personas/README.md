# Persona counsel packs (BL-207)

A pack is a Lua 5.4 module returning a table — pre-seeded advisory personas compiled from
Pantheon seeds (`Pantheon/data/personas/<id>.md`). LLM at authoring time; data at runtime.

Required fields:
- `id` (string), `bench` ("hearth" | "banner" | "mountain" | "verdict"), `seal` (string, from the seed frontmatter).
- `budget` (int) — max facts examined per evaluation; the fixed compute-fairness quota.
- `extract(facts)` → array of findings `{kind, subject, strength (0-1), evidence}`. Pure,
  deterministic, bounded by `budget`. `facts` is the corp blackboard (BL-206): a flat array of
  `{t, subject, predicate, value, confidence, provenance}`. Match predicates by prefix; never index by assumption.
- `policy(findings)` → array of opinion records `{p, m, on, c (≤140 chars), w (0-3), v?}`
  per the Pantheon codebook grammar. `v` only on verdict-bench records.
- `phrase(finding_or_opinion)` → one in-voice line, selected deterministically (hash, never `math.random`).
- `failure_condition` (string) — the persona's designed, play-reachable blind spot.

Sandbox rules: no `io`, `os`, `package`, `require`, `math.random`, or clock reads. Packs read
ONLY the blackboard passed in — never world state — and every function is a pure function of its inputs.
Verdict packs (`scales-of-maat`) additionally expose `aggregate(opinions)` → verdict records
`{v, on, baseline, c}` resolving inter-persona disagreement against a named baseline.

Seating: at corporation generation each corp seats one bench consistent with its industrial
focus; the seated personas run at the staggered strategic-eval boundary and their opinions nudge
the BL-202 `strategy_weight` — counsel biases scoring, never bypasses validation.
`pack_schema.lua` is the validator: `validate(pack)` and `validate_opinion(rec)`.
