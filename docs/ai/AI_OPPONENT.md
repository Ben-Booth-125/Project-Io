# Project Io — AI Opponent

Authority doc for the AI-opponent thread (BL-199). Ben's 2026-07-23 call: AI-opponent
development may now proceed **alongside** the basic mechanics rather than waiting. This
document opens with the **state-of-the-art research** BL-199 mandates as its first
activity; the architecture proposal, data strategy, and follow-on backlog decomposition
build on it.

**Status.** SOTA map + shortlist + staged path + data strategy: drafted 2026-07-23 (deep-research
harness). **Architecture accepted by Ben 2026-07-26** (§ 5) — the A → B utility core is the
target. The decision decomposition (§ 5), the corp-command seam + state export (§ 6), the
communication/diplomacy principle (§ 7), and the follow-on decomposition (§ 8) landed the same
session. The standing-rule relaxation is now earned **for the scoped path only**: AI-corp
strategic actions via the corp-command seam (BL-202 onward); the player's corp stays untouched
beyond the BL-181 workforce dial.

**The goal (unchanged from BL-199).** A computer opponent of **roughly human skill** — a
genuine rival across Io's loop (extraction, trade, later conflict), **beatable** by a decent
human, and **legible** (moves read as sensible, not alien or scripted). Explicitly *not* a
superhuman optimiser and *not* a handicap-only fake.

---

## 1. SOTA map

### Area 1 — the classical game-AI toolkit

The shipped-game consensus is blunt: **strategy games rarely use search-based planners for the
strategic layer.** The combinatorial explosion across economy, logistics, and combat makes
forward planning (GOAP, HTN) impractical at the top level; shipped titles use **prioritised
reactive scoring** — evaluate current needs, pick the highest-scoring action, repeat next tick.

| Technique | Good at | Bad at (for an economic 4X) |
|---|---|---|
| **Utility / scoring AI** | Cheap per-tick; degrades gracefully; tunable weights; legible ("built a refinery because refined-goods scored highest"); handles continuous trade-offs (workforce dial, recipe choice) | Weight-tuning is manual craft; can oscillate/thrash without hysteresis; no lookahead |
| **Behaviour trees** | Authoring, debugging, reactivity; good for *modes* and sequencing | Tree size explodes with combinatorial economic options; poor at "which of 50 build sites is best" (a scoring problem, not control-flow) |
| **GOAP** | Emergent action chains toward a goal | Combinatorial blow-up across domains; strategy games "rarely use GOAP" |
| **HTN** | Faster than GOAP in *well-structured* domains (decomposition prunes search) | "Can only find plans the author anticipated" — brittle to novel economic states |
| **Hierarchical strategic/operational/tactical decomposition** | The load-bearing idea; each layer uses the right tool | Needs clean layer interfaces; risk of layers fighting each other |
| **Scripted opening / build-order books** | Cheap, legible, human-like early game; sidesteps cold-start weakness | Exploitable once learned; doesn't generalise across seeds/bodies |

The **dominant shipped pattern is a hybrid**: a small state machine / BT for major modes, with
**utility-scored selectors inside**, and hierarchical decomposition (strategic desires →
operational goals → per-asset assignments). The canonical worked example for a *space 4X* is a
four-phase "generate tasks → enumerate viable assignments → score & sort → assign greedily"
loop, with a scoring formula like `score = (priority + context modifier) / distance`, and the
**same scoring machinery reused for the production/build queue**.

### Area 2 — what Paradox / Civ / Stellaris ship, and their documented failure modes

**What they ship:** weighted utility scoring, universally. Paradox's Clausewitz engine exposes
four primitives — *events, triggers, effects, data weights* — and the AI is "set the weights,
the rest handles itself." **Victoria 3** is the most transparent published economic-AI design:
three parallel **strategies** (Administrative / Political / Diplomatic) chosen by weighted
preference, plus a **Priority-and-Score** spending system — priority buckets ("Must-Have" …
"Should-not-Have") gate spending, scores tie-break within a bucket, and **Spending Variables**
predict economic trajectory so the AI knows a queued workshop will resolve a shortage rather
than over-building. Performance is managed via **tick tasks** — operations tagged with frequency
and dependencies so expensive updates don't run every tick.

**Documented failure modes:**
- **Can't run its own economy → gets handed resources.** Stellaris's AI is widely described as
  unable to build a functioning economy; the shipped remedy was flat resource bonuses and
  effectively spawning credits to launder via the market — "given unlimited alloys so players
  have something to shoot at."
- **Visible cheating breaks the illusion.** Civ's historical sins — free units under fog, instant
  Wonders — "broke the game's established mechanics."
- **Emergent-but-wrong behaviour.** Civ AIs formed apparent "tech-whoring cartels" via inter-AI
  trade; devs had to **intentionally cripple AI-to-AI trading** to stop it.
- **Stalling / difficulty-via-bonus.** Genre-wide, higher difficulty = bigger economic handicaps,
  not smarter play, because authoring genuinely competent economic play is hard.

**Lesson for Io:** the hard part is not the decision framework — it is making the AI actually
**solvent** in the same economy the human plays. Victoria 3's predictive-spending + priority-bucket
model is the directly transferable idea.

### Area 3 — modern learning approaches

- **Self-play deep RL (AlphaStar, OpenAI Five).** Reaches super-human play but is **prohibitively
  heavy for a solo dev** (AlphaStar: ~3,072 TPU + ~50k CPU cores, 44 days, ~$3M to replicate;
  OpenAI Five: 256 GPU + 128k CPU cores, 770+ PFlops/s-days). Both also aim at the *wrong target*
  for Io — alien, super-human, non-legible play. **Ruled out.**
- **Imitation / behavioural cloning.** Cheap and stable, but needs **expert demonstrations** (Io
  has none) and suffers **covariate shift → compounding errors** off-distribution; cannot exceed
  its demonstrator. Poor fit *now*; plausible *later* if a good heuristic AI generates trajectories.
- **Offline RL.** Extracts good policies even from **suboptimal** logged data and (e.g. CQL) beats
  BC. The learning family that fits Io's assets — a deterministic sim can generate huge logged
  datasets cheaply — but a significant engineering lift and research risk for a solo dev.
- **LLM-planner-over-scored-primitives / hybrid.** The most exciting recent direction and
  **directly aligned with Io's proposed out-of-process-policy-over-compact-state architecture.**
  *Vox Deorum* (Civ V + Vox Populi) puts an LLM on *macro-strategic reasoning* and delegates
  tactics to algorithmic subsystems; across 2,327 games open-source LLMs reached "competitive
  end-game play" with **human-varied play styles**. Broader hybrid work (LLM planner over
  scored/skill primitives, with an **action knowledge base masking illegal transitions to prevent
  hallucination**) reports parity with hand-crafted behaviour trees while beating flat RL. Key
  constraint: **ground the LLM against an explicit set of legal primitives.**

### Area 4 — human-skill calibration & exploiting deterministic seeds

**Calibration (Soren Johnson's "playing to lose").** There is **no truly "fair" difficulty** where
the AI plays the identical game, so **perception is what matters.** Findings that transfer to Io:
- Players tolerate **small, gradual, invisible bonuses** but revolt at **rule-breaking cheats**
  (acting under fog, free stuff, instant results).
- **Transparency is the antidote** (Mario Kart's rubber-banding is accepted because explicit).
- Players **misattribute** ordinary luck to cheating; a statistically fair system can feel unfair.
- Fairness ordering for Io: **genuine skill > mild transparent economic bonus > information
  asymmetry > rule-breaking cheat.** Io's goal means leaning on genuine skill first, reserving
  small *transparent* handicaps as difficulty knobs — never fog cheats.

**Exploiting infinite deterministic seeds — Io's structural advantage.** `make_hard_coded_world(seed)`
+ fixed ticks + the headless verify harness enable: **scenario generation** (an inexhaustible,
reproducible corpus of economic situations); **bot-vs-bot rollouts** (AI-vs-AI across thousands of
seeds to measure solvency, survival, net-worth curves); **deterministic offline policy evaluation**
(same seed, two policies, exact A/B); and **skill-regression testing** — freeze a seed-set
benchmark and assert the AI still hits its solvency/net-worth/win-rate bands after each change, the
AI analogue of Io's `verifier-*` culture (with stochastic Pass/Fail/Inconclusive verdicts for the
non-deterministic parts).

---

## 2. Shortlist for THIS codebase

### A. Scored utility AI over the existing local-agency seam — **recommended first build**
Extend the background-corp per-building triggers (BL-079) into a **strategic scoring layer**:
enumerate candidate actions (build X on tile T, switch recipe, set workforce dial, open/close a
trade route), score each with tunable weighted heuristics, apply hysteresis, execute top-N within
a per-tick action budget.
- **Dev cost:** low–medium (natural extension of what exists). **Determinism:** perfect (pure
  function of world state; seed the tie-break RNG). **Legibility:** excellent. **Skill ceiling:**
  medium — enough for a genuine, beatable rival with good weights + predictive spending.
  **Data needs:** none.

### B. Layered utility with a predictive economic model (Victoria-3-style) — **recommended second**
Add a **strategy layer** (extraction specialist vs trade arbitrageur), **priority buckets**
(Must-Have solvency vs Nice-to-Have expansion), and **spending variables** that forecast the
market impact of queued buildings before committing. This is what makes the AI *solvent* rather
than needing a cheat.
- **Dev cost:** medium. **Determinism:** perfect. **Legibility:** high. **Skill ceiling:** high
  for economic play. **Data:** none.

### C. Out-of-process LLM planner over scored primitives (Vox-Deorum-style) — **later, optional differentiator**
The external policy reads Io's compact JSON state export; the LLM picks **strategy / goals** among
an explicit legal-primitive set (masked to prevent hallucination); the utility layer (A/B) executes
tactically. Gives varied, legible, human-like *personalities* per corp.
- **Dev cost:** medium glue, but adds latency, non-determinism, API cost/dependency.
  **Determinism:** poor unless the LLM call is cached/replay-logged per decision — mitigate by
  making the LLM choose only coarse strategy occasionally, not per-tick. **Legibility:** high (can
  self-explain). **Skill ceiling:** competitive end-game. **Data:** none (zero/few-shot).

### D. Offline RL over logged self-play — **furthest out; only if A–C plateau**
Deterministic seeds generate massive logged datasets; train an offline-RL policy (CQL-class) as an
out-of-process policy over the JSON state.
- **Dev cost:** high + research risk. **Determinism:** good (greedy inference). **Legibility:** low
  (the weakness — risks alien play). **Skill ceiling:** potentially super-human (must be *capped*).
  **Data:** large, but self-generable.

### Recommended staged path
1. **Now:** build **A** — scored utility over the local-agency seam. The load-bearing 80%.
2. **Next:** layer **B** — strategy selection + priority buckets + predictive spending, so the AI
   is genuinely solvent and needs no cheat. Add a **seed-set skill-regression harness** (bot-vs-bot
   solvency/net-worth goldens) alongside.
3. **Difficulty knobs:** transparent, small, gradual economic multipliers only — never info/rule
   cheats.
4. **Later / optional:** **C** for personality and variety once the utility core is solid; keep it
   out-of-process, coarse-grained, replay-logged to protect determinism.
5. **Only if needed:** **D**, capped, as a top-difficulty tier.

---

## 3. Data strategy — exploiting infinite deterministic seeds

- **Benchmark seed-set (golden):** freeze N seeds spanning body/terrain/market diversity; the AI's
  economic outcomes become regression goldens (net-worth curve, survival rate, win-rate vs a
  reference AI). Re-run headlessly on every AI change — the direct analogue of `verifier-headless`.
- **Bot-vs-bot rollouts** as the primary skill signal.
- **Deterministic offline policy evaluation:** same seed, two policies, exact A/B.
- **Scenario generation for weakness-hunting:** search seeds for insolvency/thrash; those become
  new regression fixtures.
- **Trajectory logging** turns every rollout into future BC/offline-RL training data at near-zero
  cost — banking data now keeps option **D** open later.

---

## 4. For Project Io specifically — the verdict

Io is **unusually well-suited to a strong *classical* AI.** Its decision surface (where to build,
what recipe, workforce dial, supply routing, order-book trading) is exactly the discrete/continuous
mix that **weighted utility scoring handles best**, and it already has the substrate — background
corps taking narrow deterministic per-building actions (BL-079). The correct move is to **grow that
seam into a scored strategic layer, not to bolt on a planner or a network.**

The single most important design import is **Victoria 3's predictive-spending + priority-bucket
model** — the documented answer to the genre's central failure (an AI that can't stay solvent and
therefore gets handed resources). It lets Io's AI hit its "beatable human rival, no fake handicap"
goal *honestly*.

Io's deterministic seeds + headless harness + golden-and-verify culture let it **regression-test AI
skill** better than any shipped 4X — a genuine advantage a solo dev should exploit from day one.
Legibility (an Io requirement) also favours utility scoring: every move is a top-scoring choice with
a readable reason, the opposite of AlphaStar's alien play.

The **LLM-planner-over-scored-primitives** route (Vox Deorum) is the one modern approach worth
keeping on the roadmap — it maps cleanly onto Io's out-of-process-policy-over-JSON architecture and
adds legible *personality* — but it sits **on top of** a solid utility core, kept coarse-grained and
replay-logged, not as the first build.

**Bottom line:** build **A → B** with a seed-based skill-golden harness; reserve **C** for variety
and **D** (capped) for a top tier only if the heuristic core plateaus. Do **not** start with
learning; do **not** ship resource cheats as a substitute for competence.

---

## 5. Accepted architecture — the decision decomposition (Ben, 2026-07-26)

Ben accepted the **A → B staged path** (scored utility over the BL-079 seam, then Victoria-3-style
predictive spending). This section is the concrete decomposition the acceptance unlocks.

### Cadence — where the AI thinks

The AI evaluates at the **econ-tick boundary inside `run_economy_step`**, in the slot the BL-079
agency block occupies today; that block folds in as the scorer's **reflex tier** (tier 0: recipe
rescue, idle-a-loser — unchanged behaviour, now emitting commands). Strategic evaluation is
**staggered**: corp `c` evaluates every `K` ticks at offset `id(c) % K` (Victoria-3's tick-task
idea — bounded per-tick cost, fully deterministic).

### The candidate-action set (legal primitives)

The AI's verbs are exactly the player's, through the same validation — **no bypass, no cheats by
construction**:

| Verb | Engine seam | Notes |
|---|---|---|
| `build(type, tile, target, recipe)` | `construct_building` (placement_rules, build cost, `stack_capacity`) | The big decision; candidate sites bounded (below) |
| `demolish(building)` | `demolish_building` | Rare; scored against salvage vs sustained loss |
| `set_recipe(building, recipe)` | direct component write (BL-079 idiom) | Tier-0 rescue generalises to margin-chasing |
| `set_workforce(building, target)` | `solve_workforce_target` (BL-181) | AI corps simply run `workforce_auto` on |
| `idle(building)` / `resume` | `decommissioned` flag | Tier-0 loss-streak rule, now reversible |
| `dispatch_convoy(src, dst, res, qty, mode)` | `dispatch_convoys` seam | Strategic hauls beyond the shortfall auto-dispatch |
| `place_road(a, b, tier)` | `place_road` | Infrastructure investment; scored on route throughput |
| `survey(body, region)` | survey_system | The AI pays for discovery like the player |
| `set_exchange_policy` / `set_counterparty` | BL-160 / BL-161 once landed | The AI authors the same policy tables |

**Candidate enumeration is bounded**: build sites come from **surveyed tiles the corp can see**
(its own fog state), pre-filtered to the top-M by static suitability (terrain affinity × deposit
richness), M small; recipes and dials enumerate per owned building; routes enumerate over
known-body pairs. Bounded enumeration is what keeps the per-tick cost flat.

### Scoring

`score(action) = expected_net_per_tick / payback_ticks × strategy_weight`, terms computed from
**existing functions only** (`estimate_building_profit`, placement affinities, live market prices,
wage rate, logistics cost to nearest market, build-cost amortisation) — no new oracles. The
`strategy_weight` biases toward the corp's generated **industrial focus** (specialist premise,
CORPORATION_GENERATION.md), giving distinct-but-legible personalities for free. A **solvency
gate** (cash − committed spend > reserve floor) vetoes any spend that breaks the floor; stage B
replaces this crude floor with priority buckets + predictive spending (landed 2026-07-31, see
§ 2B below).

### 2B. Stage B — strategy, priority buckets, predictive spending (BL-203, landed 2026-07-31)

Implemented in `src/world/corp_ai.{hpp,cpp}`, extending BL-202's scorer rather than replacing it:

- **Strategy layer.** `corp_strategy` is a named alias of `industrial_focus` — the corp's
  generated specialist premise (extraction / processing / trade) IS its strategy, kept as its
  own concept so the bias is legible and can diverge from the generation-time focus later
  without a signature break. `focus_weight` (BL-202) already biases build/survey scores by this
  strategy; stage B does not change that mechanism, only names it.
- **Priority buckets** (`corp_priority_bucket`: `must_have` / `should_have` / `nice_to_have`),
  derived deterministically from each candidate's existing `corp_decision_reason` via
  `bucket_for_reason` — `dial_idle` is Must-Have (stops a sustained loss's wage/maintenance
  bleed); `dial_recipe` / `dial_workforce` / `dial_resume` are Should-Have (tune or restore a
  running asset); `best_build` / `survey_expand` are Nice-to-Have (expansion). Candidates sort
  bucket-ascending before score-descending, so a Must/Should-Have action is never starved by a
  higher-scoring Nice-to-Have one. Concretely, only Nice-to-Have candidates carry capex in this
  codebase (dials are free), so the "never starve a higher bucket" rule is enforced by gating
  build/survey spend against a **stricter** floor: `corp_should_have_buffer` sums
  `estimate_building_profit(...).input_cost` over the corp's own running processing facilities
  — the cash needed to keep feeding them this tick — and `nice_to_have_floor = reserve_floor +
  should_have_buffer` is the gate a build/survey must clear, on top of (not instead of) BL-202's
  existing reserve floor.
- **Predictive spending.** `forecast_glut_multiplier` forecasts a candidate build's added supply
  (`base_rate × richness × workforce × (1 − hazard)`) over a horizon of
  `build_duration_ticks + forecast_clearing_ticks` (1 by default — "one clearing pass") against
  the **local market's PUBLIC `supply`/`demand` aggregates only** — the same facts
  `export_corp_blackboard` would show a rival (BL-068/DISCOVERY.md), never a private read. No
  public demand signal (`demand <= 0`) yields no penalty (the AI cannot forecast against a fact
  it cannot see); the projected supply/demand ratio is unpenalised at or below
  `glut_taper_ratio` (1.0), tapers the build's score linearly to zero at `glut_veto_ratio` (2.0),
  and vetoes (removes the candidate entirely) at or above it. Applied only to build candidates;
  dials and survey are unaffected (a body's total surveyed area doesn't glut a market).

Verified by `tools/verify/corp_ai_predictive_harness.cpp` (R1: the reason→bucket mapping; R2: the
Should-Have buffer is well-defined and never loosens the floor; R3: the forecast is
visibility-honest, monotone, tapers, and vetoes at the documented ratios; R4: an end-to-end
saturated-market scene actually vetoes a build the plain BL-202 scorer would have taken) and
regression-checked against `corp_ai_harness.cpp` (BL-202), all green.

### Hysteresis & action budget

- **Do-nothing bias**: a candidate must beat the incumbent (or doing nothing) by a relative
  margin θ (~15%) — the anti-thrash rule.
- **Cooldowns**: a building that changed recipe/workforce/state holds for C ticks; reversals ride
  loss/gain streaks (the BL-079 `loss_streak` idiom, generalised).
- **Budget**: per evaluation, at most **1 construction + a small number of dial changes** per
  corp; total committed spend capped by the solvency gate.
- **Determinism**: stable iteration (sorted `corp_ids`, stored asset order, tile-index order);
  ties break on lowest entity id; the only randomness is a per-corp hash of the world seed used
  as a fixed personality jitter on weights — constant per campaign, deterministic by construction.

---

## 6. The corp-command seam & state export (shared with multiplayer)

The scorer does not mutate the world directly. It emits **`corp_command`** records —
`{tick, corp, verb, args}` — applied at the tick boundary through the player-grade validation
above. This is deliberate triple-duty:

- **AI**: the command stream *is* the decision log. A ring buffer of commands + score rationale
  (winning score vs runner-up) is the AI's legibility surface, its replay artifact, and the
  skill-harness's input.
- **Multiplayer**: `MULTIPLAYER_PRINCIPLES.md` § Preserve now #2 — lockstep exchanges exactly
  this: small serialisable intents tagged with their tick. An AI corp is a local command source;
  a network player is a remote one; the seam is identical. (`canvas_command` stays
  navigation-only; `corp_command` is its sim-mutating sibling.)
- **Out-of-process policies (C/D)**: an external policy consumes the state export and returns
  commands — the same contract, across a process boundary.

**State export (schema designed now, implemented with BL-202).** A compact, tick-tagged,
per-corp JSON view that is **visibility-honest**: it contains only what that corp could see under
the BL-068 rules and its own fog state — own buildings/pools/cash in full; public market
prices/aggregates; rival *buildings* but not their internals; its own routes and survey state.
The AI reads through the same information asymmetry the player does; anything else is the fog
cheat the calibration research warns against. Version the schema from day one.

---

## 6a. The action dictionary (BL-270, 2026-08-02) — the word interface's third leg

The seam above gives an out-of-process policy its **write** channel and the state export
its **read** channel. What neither states is *meaning*: which press to choose, and why.
**`docs/ai/ACTIONS.json`** (readable mirror `ACTIONS.md`, generated by
`tools/session/render_actions.js`) closes that gap — every control in the game, catalogued
as `{press, typed args, preconditions, expected_output, reason_to_select}` across five
families (gameplay / canvas / lens / ledger / chrome).

Two properties matter for the AI use:

- **The gameplay entries are transcribed, not authored.** Their `args` and `preconditions`
  mirror `corp_command.hpp`'s verbs and `corp_command_result` rejections exactly; where the
  dictionary and the seam disagree, the dictionary is wrong and the fix is mechanical.
- **`reason_to_select` is the policy prior.** It states, in words, why a press exists —
  the design intent an LLM policy conditions on before world state ever enters the picture.
  This is what makes word-based play (and later word-driven generation, and the
  difficulty-level work — Ben, 2026-08-02) a prompt-assembly problem rather than a
  fine-tuning one: blackboard (read) + dictionary (meaning) + command seam (write).

The catalogue axes, for orientation: `user_stories.json` = player **intent**;
`UX_QUESTIONS` (BL-260) = what a readout **answers**; `ACTIONS` = what a press **does**.

**Consumption model (Ben, 2026-08-02).** A language agent never absorbs the store
whole: it holds the generated **`ACTIONS_INDEX.json`** (`[id, surface]` per action)
in context and fetches full entries on demand via
`tools/session/actions_query.js` (by id, family, or keyword) — the lookup the
word-play harness wraps as the LLM's dictionary tool.

**Priority is NOT dictionary material (Ben, 2026-08-02).** Entries deliberately
carry no urgency or importance score. The live AI scores each *candidate* action
on a **2D urgency × importance map** at decision time, against blackboard state —
urgency (how much each tick of delay costs *now*) and importance (how much the
outcome matters to the strategy) are properties of the situation, not of the
verb. A static per-entry score was considered and rejected the same day: it
would claim `idle` is always urgent, when it is urgent only for a building
hemorrhaging cash. The dictionary supplies meaning; the policy supplies priority.

---

## 7. Communication & diplomacy — the chat principle (Ben, 2026-07-26)

Ben's steer: **since every rival is AI, inter-corp coordination should happen in a communication
medium, not hidden state.** Corps message **publicly or in private groups** to form plans;
diplomacy is *legible messaging*, not behind-the-scenes flags. This is a better diplomacy
principle than invisible opinion modifiers — and in multiplayer the same channels carry human
players, so the medium is actor-agnostic.

- **Surface**: a **chat log window replaces the Explorer placeholder** (right shell band, middle
  slot — the Explorer's pinning concept was never wired and is superseded; LAYOUT.md updated).
  Channels: **Public** (every corp), plus **arbitrary groups** (any subset of corps, created from
  the panel). The player reads Public and any group they belong to.
- **Stage A (now)**: messages are **templated, deterministic renderings of the decision log** —
  the BL-079 reflex events today ("Meridian idled Ironworks — sustained losses"), the BL-202
  command stream when it lands. The chat is the AI-observability surface first; personality
  prose comes later.
- **Stage C (later)**: the LLM planner speaks in-character in channels; AI↔AI private groups
  form plans the player cannot read — which is principled, because the *medium* is uniform and a
  future intelligence mechanic (Discovery-model extension) can expose intercepts as content.
- **Determinism**: stage-A message text derives purely from deterministic events. Free-text LLM
  chat arrives only under the C-route rules (out-of-process, coarse-grained, replay-logged).
- **Player input**: a message box posts to the selected channel. It has **no mechanical effect
  yet** — it is the hook the C-route consumes (the player negotiating with AI corps in language).

---

## 8. Follow-on decomposition (filed 2026-07-26)

BL-199 closes with this decomposition; the build work is carried by:

| Item | Carries | Requires |
|---|---|---|
| **BL-202** `CORP_AI_SCORED_UTILITY` | Stage A: the scorer, the corp-command seam + decision log, state-export implementation | — |
| **BL-203** `CORP_AI_PREDICTIVE_SPENDING` | Stage B: strategy layer, priority buckets, spending forecast — the solvency answer (**landed 2026-07-31**, § 2B) | BL-202 |
| **BL-204** `AI_SKILL_HARNESS` | Seed-set skill-regression harness (bot-vs-bot goldens: solvency, net-worth curves) + the tick-boundary **state hash** (doubles as the multiplayer desync primitive) | BL-202 |
| **BL-205** `CORP_CHAT_LOG` | The § 7 surface: chat window replacing the Explorer, channels + groups, agency-event feed (first slice lands with the item's filing) | — |

Economics the scorer prices, settled alongside (same session): **BL-153** (convoy freight
premium) and **BL-193** (stack diminishing returns); **BL-160/161** confirmed as the AI's trade
primitives (the AI authors the same policy objects via commands).

---

## 8a. The world history log (BL-208, landed 2026-08-02)

The project's first flat-binary serialisation seam (`src/world/history_log.{hpp,cpp}`),
answering backlog.json § BL-208's settled design: a **single interleaved, append-only, tagged,
serialised** world log — not per-body/per-corp logs, which fail the moment a corporation acts on
a body (every interesting event) and would need a join with no shared ordering.

**The shape**, in `src/world/world.hpp`:

```cpp
enum class history_topic : uint8_t { genesis, checkpoint, decision, agency, trade_route };

struct world_history_entry {
    int64_t        timestamp;   // unit depends on topic — see below
    history_topic  topic;
    entity_id      body = null_entity;  // tag, null_entity if not applicable
    entity_id      corp = null_entity;  // tag, null_entity if not applicable
    std::string    event;
    std::string    consequence;         // may be empty
};

std::vector<world_history_entry> history_log;  // on `world`
```

**Timestamp is topic-scoped, deliberately.** `genesis`/`checkpoint` entries reuse
`history_event::years_before_epoch` exactly (positive = the deep/historical past, 0 = the 1960
epoch) — no conversion, per the item's own settled design. `decision`/`agency`/`trade_route`
entries — which only ever occur during the live simulation, strictly after the genesis chapter's
one-time bulk-insert at world setup — instead carry the sim day tick. A reader branches on
`topic` to know which clock it is reading; nothing needs the two to compare numerically, because
the vector's append order is already the true chronological order end-to-end.

**The four sources, all additive to their existing consumers** (no behaviour change to anything
that already read `ai_decisions`, `agency_events`, or `trade_routes`):

- **genesis** — `seed_genesis_history(world&, const generation_report&)` copies each body's dated
  `history_event` lines into the log at world setup (`app::setup_world`, right after
  `make_hard_coded_world`), tagged by that body's entity id. This is the first bridge from
  `generation_report` (presentation-only; per `hard_coded_world.hpp` it never otherwise reaches
  `world`) into world state.
- **checkpoint** — the same call migrates `planetology_state::checkpoints` alongside genesis, per
  body, merged and stable-sorted into one chronological run. `checkpoint_record` carries no
  timestamp of its own; the resolution rule (a documented simplification, not an exact per-line
  pairing — see `history_log.cpp`'s `resolve_checkpoint_timestamp` and the NEEDS_REVIEW entry it
  cites) takes the LAST dated history line at or before the checkpoint's own chain stage.
- **decision** — `corp_ai.cpp`'s strategic-tier push site additionally logs a one-line narration of
  each applied `corp_decision` (verb + reason + score). `corp_decision_ring` (the existing 256-cap
  ring) is untouched.
- **agency** — both the BL-079 reflex tier (`economy_system.cpp`'s recipe-rescue and idle-a-loser
  sites) and the BL-202 strategic tier (`corp_ai.cpp`) additionally log a narrated `agency` entry.
  `economy_report::agency_events` (the chat feed's existing source) is untouched.
- **trade_route** — `supply_system.cpp`'s `credit_arrived_convoys` logs an entry only when a
  body-pair lane is **first established** (the `rit == w.trade_routes.end()` branch), never on a
  repeat completion (which only bumps the existing `trade_route`). `world::trade_routes` and
  `body_activity_visibility` are untouched, byte-for-byte.

**Serialisation** (`write_history_log`/`read_history_log`): a leading 4-byte magic (`"IOHL"`) plus
a `uint32_t` version (BL-107's "first thing to add when the serialiser lands" rule), then an entry
count, then each entry length-prefixed. `read_history_log` rejects — rather than misreads — a
wrong magic, an unsupported version, an unknown topic byte, or a truncated stream, leaving the
destination world's `history_log` untouched on any rejection. Round-trip is field-identical.
Verified by `tools/verify/history_log_harness.cpp` (mirrors `history_ladder_harness.cpp`'s style:
built over the real generated world, not hand-fabricated entries) and an added determinism check
in `tools/verify/determinism_harness.cpp` (two identical-seed generations produce an identical
genesis+checkpoint chapter).

Not built (deliberately out of scope, per the item's brief): a save-game menu/UI. The serialiser
is a library function a harness exercises; the one live hook is the genesis-chapter bridge at
world setup.

BL-218 (nations rewrite) and BL-219 (corporations rewrite) are expected to write into this same
log — the substrate this item exists to hand them, per the item's own resequencing rationale.

---

## 9. Citations

Sources gathered by the deep-research harness (2026-07-23). Each supports the claim noted.

- DeepMind — AlphaStar (Grandmaster StarCraft II): self-play RL ceiling + alien-play concern. https://deepmind.google/blog/alphastar-grandmaster-level-in-starcraft-ii-using-multi-agent-reinforcement-learning/
- OpenAI Five — Dota 2 with Large-Scale Deep RL: scale of self-play RL. https://arxiv.org/pdf/1912.06680
- Soren Johnson — "Game AI & Our Cheatin' Hearts": fair vs unfair cheats, transparency, playing-to-lose. https://www.gamedeveloper.com/game-platforms/analysis-game-ai-our-cheatin-hearts
- Victoria 3 Dev Diary #59 — AI: strategies + Priority-and-Score spending + predictive Spending Variables. https://www.paradoxinteractive.com/games/victoria-3/news/victoria-3-dev-diary-59-ai
- Victoria 3 Dev Diary #76 — Performance: tick-task model for budgeting economic updates. https://www.paradoxinteractive.com/games/victoria-3/news/dev-diary-76-performance
- Stellaris — "Is it true the AI cheats?" (Paradox forums): economy-failure-via-resource-handout. https://forum.paradoxplaza.com/forum/threads/is-it-true-that-the-ai-cheats.1139965/
- Game Developer — Designing AI Algorithms for Turn-Based Strategy Games: the 4X-space scoring loop. https://www.gamedeveloper.com/design/designing-ai-algorithms-for-turn-based-strategy-games
- Socratopia — Beyond State Machines (Utility AI / BT / GOAP): why strategy games avoid GOAP. https://www.socratopia.app/library/game-code-anatomy-en/chapter-12
- Tono — Game AI Planning (GOAP / Utility / Behavior Trees): comparative strengths/weaknesses. https://tonogameconsultants.com/game-ai-planning/
- Anbeeld — Designing AI for Strategy Games Through Modding: Clausewitz weighted-utility practice. https://anbeeld.com/articles/designing-ai-for-strategy-games-through-modding
- Vox Deorum — Hybrid LLM Architecture for 4X/Grand Strategy: LLM macro-strategy + subsystem tactics; 2,327 games. https://arxiv.org/abs/2512.18564
- Hierarchical Control in Multi-Agent Games (LLM planning + RL execution): action-KB masking to prevent illegal actions. https://arxiv.org/abs/2606.20014v1
- When Should We Prefer Offline RL Over Behavioral Cloning?: offline RL beats BC, tolerates suboptimal data. https://arxiv.org/abs/2204.05618
- Is Behavior Cloning All You Need? (horizon in imitation learning): covariate shift / compounding error. https://arxiv.org/pdf/2407.15007
- Metagame Autobalancing for Competitive Multiplayer Games: bot-vs-bot self-play to measure balance. https://arxiv.org/pdf/2006.04419
- Toward Automated Game Balance: self-play bots reduce manual testing / assess difficulty. https://www.researchgate.net/publication/355109124
- AgentAssay — Regression Testing for Non-Deterministic Agents: Pass/Fail/Inconclusive statistical verdicts. https://arxiv.org/pdf/2603.02601

**Two specifics the research could not pin down:** Vox Deorum's exact state-export schema, per-decision
latency, and token cost (the architecture and results are confirmed; the low-level interface numbers
are not); and a numeric per-tick AI CPU budget from Paradox (the tick-task *mechanism* is documented,
concrete budget values are not published). Both matter for designing Io's option C and should be
resolved from full-text sources before committing to an LLM-planner build.
