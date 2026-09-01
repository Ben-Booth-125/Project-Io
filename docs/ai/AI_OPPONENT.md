# Project Io — AI Opponent

Authority doc for the AI-opponent thread (BL-199, AI opponent research). Ben's 2026-07-23 call:
AI-opponent development proceeds **alongside** the basic mechanics rather than waiting. This
document opens with the **state-of-the-art research** BL-199 mandates as its first activity; the
architecture, data strategy, and decomposition build on it.

**The architecture is accepted (Ben, 2026-07-26 — § 5):** the A → B utility core — a
deterministic scored-utility layer over the corp-command seam, then Victoria-3-style predictive
spending — is the rival. The standing-rule relaxation is earned **for the scoped path only**: AI-corp
strategic actions via the corp-command seam (`src/world/corp_ai.cpp`); the player's corp stays
untouched beyond the workforce dial.

**The direction above the core (Ben, 2026-08-03 — § 10):** the interface to any language model is
**MCP** (an out-of-process protocol wrapper over the three legs Io has: blackboard export, action
dictionary, corp-command seam); the runtime model is **small and local**; **cloud inference is a
corpus generator, not a runtime**, producing the supervised traces a small model is fine-tuned on.
**The model occupies Stage C — a dialogue layer over the scorer's decisions — and never the
action-generating seat (Ben, 2026-08-08 — § 10g).** Read § 10 before any work on the language
layer.

**The goal (Ben, 2026-08-31).** A computer opponent that plays like a **person**, not like an
optimiser. Three properties: **skilled**, **diplomatic**, and **restrained** — it knows when to
hold back.

**Skilled.** A genuine rival across Io's loop — extraction, trade, conflict — whose moves are
**legible**: they read as sensible, not alien and not scripted. Skill is never dialled down. The
scorer plays as well as it knows how.

**Diplomatic.** It deals rather than merely competes. Stance, contracts and lobbying are positions
it takes toward named parties, for reasons it can state (§ 7).

**Restrained.** No corporation runs away with the game. The intended shape of a campaign is a
**close race** — a leader ahead by a slim margin, not by an era.

### Where restraint comes from — the world, not the agent (Ben, 2026-08-31)

This is the load-bearing decision in this document, and it **supersedes the objective-term framing
first written on the same day**. Ben's ruling:

> *"The angle we are taking here is a literal handicap. I prefer to consider it as always a force
> from within the game system. With 7 corporations, we have plenty of room for alliances to form
> against leaders, and we haven't built a critical system which is climate. Therefore, I prefer if
> an agent's decision to slow down can be framed as usually motivated by systems."*

**A term in the scorer that says "ease off, I am ahead" is a handicap wearing a costume.** It is
invisible, the player cannot interact with it, cannot counter it, cannot cause it, and cannot be
its beneficiary in any way they could name. It also earns its slim margin by making the AI *worse*,
which is the one thing § "The goal" says it must not do.

**So the agent does not hold back. The world holds it back.** Every corporation plays flat out for
its own position. The margin stays slim because **leading is expensive** — a fact about the world
that applies to whoever leads, the player included, and that the player can see, use and suffer.

The margin is therefore an **emergent property of a healthy system, not a target any actor aims
at**. Nothing in the scorer knows what the margin is.

**The brake in the prototype is coalitions, and it carries this alone.**

**Coalitions form against the leader.** Seven corporations is enough room for this to be real. A
corp's stance scoring reads *who is winning*, so the leader accumulates enemies and the field
accumulates each other — the brake is applied **to** the leader **by others**, never by the leader to
itself. This is diplomacy doing the load-bearing work rather than decorating it, and it is the reason
§ 7's medium matters: a coalition that forms in a channel the player can read is a coalition the
player can join, split, or provoke.

**A second brake exists in the design and is NOT available here.** Climate is a commons that the
largest operator strains most, and it would brake a runaway without naming anyone. But climate is the
**next Era's catastrophe** (`docs/economy/ERAS.md` § The point of an Era; designed ahead in
`docs/CLIMATE.md`) and sits **outside prototype scope** — so nothing here may lean on it. Stated
plainly because it changes what this design is claiming: in the prototype, if coalitions do not brake
a runaway, **nothing else does**, and that is a finding about coalitions rather than an argument for
reaching for the opt-in dial.

The Era's own **Alarm** scalar is a genuine second pressure and a different one — it bears on
*nations*, not on a leading corporation, and it is a catastrophe to avoid rather than a brake on
standing. Do not conflate them.

Coalitions are legible, have a cause, and **feed Trade or Conflict** — which an objective term
does not, and which every system in Io is required to do.

### The constraint on restraint (Ben, 2026-08-31)

> *"'how far restraint goes' should never exclude extension and construction. If a player loses
> out, they should be able to see that the world doesn't wait for them."*

**A pressured corporation still builds, still expands, still extends.** No brake in either mode may
veto construction or extension outright. Pressure makes leading *costly*; it never makes the world
*stop*. A player who falls behind must find a world that carried on without them — that is the
honest consequence the whole design is protecting, and a frozen leader destroys it.

So a brake scales what a corp does. It never forbids a category of what it does.

### The second mode, kept but not the default

The scorer-side margin term is **retained as an explicit, opt-in mode** — Ben, 2026-08-31: *"we can
keep both modes."* Where it applies, it applies as an **even scaling across every candidate**, never
as a carve-out that exempts or forbids one family of action, per the constraint above.

It is a **difficulty knob**, and it is named as one: this is exactly § 1 Area 4's "small,
transparent economic handicap", which that section permits as a knob and rejects as a foundation.
The systemic route is the foundation. This is the dial on top of it.

### Standing — what the margin is measured in

A corporation's standing is a **composite**, not its bank balance (Ben, 2026-08-31: *"an aggregate
of net worth, research, military strength… maybe others too"*):

| Component | Quantity |
|---|---|
| Economic | Net worth — cash plus the assessed value of buildings and held stock |
| Research | The corp's accumulated `science` (the BL-332 accumulator: reached, never spent) |
| Military | Summed `unit_strength` over the corp's fielded units |

Balance alone is the wrong measure and would misread the game constantly — a corp that has just
spent its treasury on a smelter is not behind, and a corp hoarding cash while its rivals arm is not
ahead. The composite is what a coalition scores against, and what the skill harness bands.

**The list is open, so the shape must let it grow.** A component is an entry in
`standing_component`, its weight an entry in `corp_ai_params::standing_weights`, and the total a
loop over the two — so a fourth component is an addition and never a rewrite, and **tuning the
composite is a data change** (the `trade_hold_threshold` discipline).

**Denominated in credits.** Three components in three unrelated units cannot be added without a
conversion, and a conversion nobody can state is a magic number. Each weight is therefore *credits
per unit of its component*, anchored on something the world already prices: research on what a
science point costs to produce (a research institute's per-tick running cost), military on what a
fielded regiment costs to raise (`hire_unit`'s own price against `unit_strength`'s scale). Net
worth is already credits, so its weight is 1 by definition.

**Read at the tick boundary, never inside the scorer's walk.** Standing is read after the budget
pass has written balances and clearing has resolved the prices that value held stock, and *before*
any corp's strategic evaluation mutates the world. `run_corp_strategic_step` walks corps in sorted
id order and applies as it goes, so a standing read from inside that walk answers differently for
the first corp than for the last — the evaluation cadence would become the tiebreak of every
comparison built on it. A consumer that needs the whole field snapshots it once at the boundary
and scores against the snapshot.

**Not the Corporations panel's `corp_standing`** (`src/world/standing.hpp`), which is a
disclosure-gated *profile* — reach, capital, market share, each shown or withheld by the observed
firm's filing status. That answers what a player may READ about a rival. This answers who is
ahead, from full world state, and is not player-facing.

**What follows.** A rout is a failure of the design, in the same way a collapse is. Superhuman play
is a regression, not an achievement. And the measure of this AI is the **spread across the field** —
not an absolute wealth band, which says nothing about whether anyone was played with.

---

## 1. SOTA map

> ### Sweep record
>
> The cut ritual runs a **diff against the last recorded sweep, never a from-scratch survey**
> (`DEVELOPMENT_PRACTICES.md` § Cutting a release; NR-167, ruled 2026-08-13). This block is the
> baseline the next one diffs against — keep it at the top, one entry per sweep, newest first.
>
> **2026-08-26 (v0.1.20).** Verdict: **the map below stands as written; no amendment required.**
> Window 2026-08-13 → 2026-08-26, thirteen days, and a diff that short should mostly be empty — it
> was. Areas 1, 2 and 4: no material change. Area 3: one in-window finding, **Qwen3.8-27B** (released
> 2026-08-14/15, Apache 2.0, 27B dense, ~262K context, a 4-bit build ≈ 17 GB so it fits a single
> 24 GB consumer card; reported 61.7 SWE-bench Pro / 84.3 OSWorld-Verified / 73.0 Terminal-Bench 2.1
> — figures as reported by the sweep's sources, not independently verified here). It narrows § 10c.1's
> "120B → runs on Ben's machine" gap without fine-tuning and **widens the margin on the C1 gate**
> (`LANGUAGE_POLICY_FEASIBILITY.md`). It **changes no decision**: § 10g's ruling that Stage A/B stays
> the action generator rests on determinism, legibility and *"distilling `corp_ai.cpp` yields
> `corp_ai.cpp`"* — capability was never load-bearing for it, so a more capable local model does not
> make a non-deterministic action generator acceptable. Nothing found required a cloud model, so
> there were no out-of-scope-by-construction findings to weigh.
>
> *Limits of the 2026-08-26 sweep, recorded because the next one inherits them.* The agent's own
> knowledge cutoff sat before the window, so it distinguished "new" from "new to it" **by published
> date alone** and excluded anything it could not date — one candidate (a llama.cpp
> multi-token-prediction merge reported at ~2.2–2.4× on dense 27B locally) was dropped for exactly
> that reason and may belong in the next window. It could not enumerate arXiv new-submission listings
> directly, and keyword search is ranked toward well-cited material, which **structurally
> under-samples papers a fortnight old**. Read the emptiness of Areas 1, 2 and 4 with that in mind:
> it is consistent with a quiet fortnight, and it is not proof of one.


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
  **directly aligned with Io's out-of-process-policy-over-compact-state architecture.**
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

### A. Scored utility AI over the existing local-agency seam — **the first build**
Extend the background-corp per-building triggers (BL-079, background-corp agency) into a
**strategic scoring layer**: enumerate candidate actions (build X on tile T, switch recipe, set
workforce dial, list surplus), score each with tunable weighted heuristics, apply hysteresis,
execute top-N within a per-tick action budget.
- **Dev cost:** low–medium (natural extension of what exists). **Determinism:** perfect (pure
  function of world state; seed the tie-break RNG). **Legibility:** excellent. **Skill ceiling:**
  medium — enough for a genuine, beatable rival with good weights + predictive spending.
  **Data needs:** none.

### B. Layered utility with a predictive economic model (Victoria-3-style) — **the second**
Add a **strategy layer** (extraction specialist vs trade arbitrageur), **priority buckets**
(Must-Have solvency vs Nice-to-Have expansion), and **spending variables** that forecast the
market impact of queued buildings before committing. This is what makes the AI *solvent* rather
than needing a cheat.
- **Dev cost:** medium. **Determinism:** perfect. **Legibility:** high. **Skill ceiling:** high
  for economic play. **Data:** none.

### C. Out-of-process language layer over scored primitives (Vox-Deorum-style) — **the differentiator**
The external policy reads Io's compact state export; the model works among an explicit
legal-primitive set (masked to prevent hallucination); the utility layer (A/B) executes
tactically. Gives varied, legible, human-like *personalities* per corp.
- **Dev cost:** medium glue, plus latency. **Determinism:** protected by keeping the model
  out-of-process, coarse-grained, replay-logged — never per-tick, never in the sim. **Legibility:**
  high (can self-explain). **Skill ceiling:** competitive end-game. **Data:** none for the
  zero-shot path; the direction *chooses* to collect data, to shrink the model (§ 10d).
- Transport is **MCP**, and the runtime target is **local** — cloud play is a corpus-generation
  activity (BL-279, trace corpus), not a per-decision dependency. Which seat the model occupies is
  § 10g's ruling: the dialogue layer, not the action generator.

### D. Offline RL over logged self-play — **furthest out; only if A–C plateau**
Deterministic seeds generate massive logged datasets; train an offline-RL policy (CQL-class) as an
out-of-process policy over the state export.
- **Dev cost:** high + research risk. **Determinism:** good (greedy inference). **Legibility:** low
  (the weakness — risks alien play). **Skill ceiling:** potentially super-human (must be *capped*).
  **Data:** large, but self-generable.

### The staged path
1. **A** — scored utility over the local-agency seam. The load-bearing 80%.
2. **B** — strategy selection + priority buckets + predictive spending, so the AI is genuinely
   solvent and needs no cheat, with a **seed-set skill-regression harness** (bot-vs-bot
   solvency/net-worth goldens) alongside.
3. **Difficulty knobs:** transparent, small, gradual economic multipliers only — never info/rule
   cheats.
4. **C** for personality and variety above the utility core; out-of-process, coarse-grained,
   replay-logged to protect determinism.
5. **Only if needed:** **D**, capped, as a top-difficulty tier.

---

## 3. Data strategy — exploiting infinite deterministic seeds

- **Benchmark seed-set (golden):** freeze N seeds spanning body/terrain/market diversity; the AI's
  economic outcomes become regression goldens (net-worth curve, survival rate, win-rate vs a
  reference AI). Re-run headlessly on every AI change — the direct analogue of `verifier-headless`.
  **The primary metric is the MARGIN, not the level.** § "The goal" makes a close race the
  intended shape of a campaign, and an absolute wealth band cannot score that: a run where every
  corp grew together and a run where one corp ran away both sit inside a generous net-worth band,
  and only the second is a failure. The banded quantity is therefore the **spread in composite
  standing between the leading corporation and the field** — held narrow is a pass, a widening gap
  is the regression, and an absolute band survives only as a solvency floor underneath it.
  Note what this measures and what it does not: no actor aims at the margin, so the band is scoring
  whether the WORLD'S brakes work, not whether an agent obeyed an instruction.
  `tools/verify/ai_skill_harness.cpp` is that harness.
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

The **LLM-over-scored-primitives** route (Vox Deorum) is the one modern approach worth keeping on
the roadmap — it maps cleanly onto Io's out-of-process-policy-over-state-export architecture and
adds legible *personality* — but it sits **on top of** a solid utility core, kept coarse-grained and
replay-logged, not as the first build.

**Bottom line:** build **A → B** with a seed-based skill-golden harness; reserve **C** for variety
and **D** (capped) for a top tier only if the heuristic core plateaus. Do **not** start with
learning; do **not** ship resource cheats as a substitute for competence.

---

## 5. Accepted architecture — the decision decomposition (Ben, 2026-07-26)

Ben accepted the **A → B staged path** (scored utility over the BL-079 seam, then Victoria-3-style
predictive spending). This section is the concrete decomposition, implemented in
`src/world/corp_ai.{hpp,cpp}` with its tunables in `corp_ai_params`.

### Cadence — where the AI thinks

The AI evaluates at the **econ-tick boundary inside `run_economy_step`**, in the slot the BL-079
agency block occupies; that block is the scorer's **reflex tier** (tier 0: recipe rescue,
idle-a-loser). Strategic evaluation is **staggered**: corp `c` evaluates every `cadence_k` (4)
ticks when `tick % k == index(c) % k`, the index taken over the **sorted corp set** (Victoria-3's
tick-task idea — bounded per-tick cost, fully deterministic). Admitting or removing a corp from
evaluation never shifts another corp's slot, because the index is over the sorted set.

### The candidate-action set (legal primitives)

The AI's verbs are exactly the player's, through the same validation — **no bypass, no cheats by
construction**. Every verb is a `corp_verb` on the corp-command seam (§ 6); the scorer's
candidate set is this subset of it:

| Candidate | Engine seam | Notes |
|---|---|---|
| `build(type, tile, target, recipe)` | `construct_building` (placement_rules, build cost, `stack_capacity`) | The big decision; three types — extraction site, processing facility, military base; candidate sites bounded (below) |
| `set_recipe(building, recipe)` | `apply_corp_command` → component write | Tier-0 rescue generalises to margin-chasing |
| `set_workforce(building, target)` | `solve_workforce_target` | The solver reports its own modelled gain, so the dial is scoreable in both directions |
| `idle(building)` / `resume` | `decommissioned` flag | Tier-0 loss-streak rule, reversible |
| `survey(body)` | survey_system | The AI pays for discovery like the player |
| `hire_unit(tile, unit_type)` | `hire_unit` at the corp's own completed `military_base` | Availability gated on stockpile/market access, never on cash; spend subject to the solvency gate |
| `place_sell_order(body, target, quantity, floor)` | the order book | § 2C |

The seam carries further verbs the scorer does not enumerate — `demolish`, `place_road`, the
convoy pair, the procurement triple, the stance verbs, the unit verbs and `withdraw_from_battle`
— which an agent on the seam (§ 10) issues directly. Scoring roads and demolition is BL-447
(scorer never demolishes or roads); scoring stance is BL-450 (rivals score stance).

**Candidate enumeration is bounded**: build sites come from **surveyed tiles the corp can see**
(its own fog state), pre-filtered to the top-M (`top_m_sites = 8`) by static suitability (terrain
affinity × deposit richness × an **input-demand pull**, `input_demand_pull`, that weights a
tile's extractable deposits by what the economy's recipes want and cannot get — divided by the
sites already targeting that resource, so the pull decays as the shortage is answered); recipes
and dials enumerate per owned building. Bounded enumeration is what keeps the per-tick cost flat.

**The processing-facility candidate** (BL-439, AI builds processors) runs on the same score curve
and the same solvency, glut and reserve-floor gates as the extraction candidate, and differs only
where a processor genuinely differs from a mine:

- **Siting.** No deposit ranks a processor, so `ranked_sites` offers it nothing. It is sited on the
  **corp's own asset tiles** — the same body, so the same stockpile pool its extraction feeds; the
  same tile, so the same market. Cheap (`O(assets)`, not another `O(tiles)` scan) and legible.
  One processor per tile, or a corp would stack them on its best tile every evaluation.
- **Recipe choice.** Walks the **browse** space (this era's roster) and crosses to the **absolute**
  id through `recipe_id(name)` — the two id spaces `recipe_registry.hpp` keeps apart. The recipe
  travels **with** the command, because `construct_building` substitutes steel for `no_recipe`.
- **Reachability.** Two gates a mine never needs: the **tech** gate (asked here rather than
  discovered as a seam rejection — a candidate that can only ever be refused costs a build slot to
  learn nothing), and **input access** — pool + local market inventory measured against the
  production tick's own coverage threshold, since a processor with no reachable input is an
  immediate loss-maker. The scorer asks exactly what the seam asks and no more: mirroring a lock
  the seam does not apply would make the rival stricter than the rules it plays by.
- **Pricing.** Priced by `estimate_prospective_profit` rather than the extraction candidate's
  inline revenue-minus-wages sum. The inline model survives on the extraction side because
  switching it would move every blessed golden for no player-visible gain; a new candidate had no
  golden to protect and no reason to inherit the weaker model.

The candidate exposes the economy it runs in: processors realise far less than the estimator
predicts on the same buildings, which is a substrate defect owned by BL-436 (processing
under-earns extraction) and deliberately not hidden by the scorer.

### Selection must be scale-free (Ben, 2026-08-31)

**A pre-selection step that ranks candidates by an absolute quantity, in a domain whose values span
orders of magnitude, is a category exclusion rather than a ranking.**

Every quantity the scorer might pre-rank on — deposit magnitude, net margin, price — is long-tailed,
and for reasons unrelated to desirability. Ore deposits are physically larger numbers than clay
deposits. A high-value low-volume good carries a fatter margin than a cheap universal one. Neither
difference means *the world needs more of this*.

So an argmax or a global top-M over such a quantity does not rank the field; it deletes most of it
before scoring begins. The scorer then appears to be choosing correctly while never having been
offered a real choice, and the symptom surfaces far downstream — as processors built on inputs
nobody mines, or as a building type no rival ever constructs.

**The rule:** a pre-filter may narrow *within* a category; it may not decide *between* categories.
Rank scale-free — normalise within resource, within recipe family, within building class — or take a
per-category top-K rather than a global top-M, so every category reaches the scorer and **the scorer
decides**. That is what the scorer is for.

**A cheap good can never win an absolute contest, however badly the world needs it.** A good wanted
by every building on the map is precisely the profile of one with a thin per-unit margin, so any
selection that ranks on margin alone will refuse to produce exactly the goods the economy most
depends on.

This is the trap BL-440's own comment named — *"pre-selecting the richest was a TILE-LOCAL heuristic
answering a WORLD-level question"* — and the reason it is stated here as a rule rather than left as
that item's note is that fixing one instance does not prevent the next: the same sentence written at
a different altitude is the same defect.

### A dial tunes; it does not repurpose

**A recipe switch is a tune within a facility's group. Becoming a different facility is a build**
**decision, and it is scored as one.**

`recipe::group` is what a processing facility IS — a Metal Foundry, a Power Generation plant, a
Construction yard — and the seam has enforced this since Ben's BL-434 retraction (2026-08-16): a
switch that crosses a group is **refused outright**, because "switching methods can mean changing
to a different building type", and the only route to a different type is dismantle and rebuild.

The scorer must be told what the seam already decided. A margin chase that ranks every recipe in
the roster is the scale-free rule's defect (§ above) with an extra cost on top: because a facility
gets **one** switch proposal per evaluation, an argmax that lands out-of-group spends that proposal
on a command that cannot apply, and **starves the legal within-group switch that would have**. The
failure is silent — the refusal is a seam result, not a red row — and it looks from the outside
like a facility that simply never retools.

The general shape, and the reason this sits beside § Selection must be scale-free rather than
inside it: **a scorer that proposes what its seam forbids is not merely wasteful, it is blind.** It
cannot tell a refusal from an absence, so the candidate it should have offered is never scored at
all. Where a seam has a hard precondition, the candidate generator carries it too.

### One dial, one owner (Ben, 2026-08-31)

**No two agency tiers may write the same building state.** The reflex tier (§ the BL-079 grant) and
the strategic scorer both act on buildings, and where their authority overlaps on a single dial —
an operating/idle flag most obviously — neither can see what the other did, and the pair oscillates:
one tier switches a building off on its own criterion, the other switches it back on for its own,
indefinitely.

The cost is not only the churn. The strategic tier's action budget is finite, so decisions spent
undoing another layer's work are decisions not spent playing, and the oscillation is **invisible in
the decision log** — the reflex tier is not a scored decision and leaves no row, so the log shows
only the half that reverses it and reads as inexplicable indecision.

**The rule:** every dial has exactly one owner. Where both tiers have a legitimate interest, the
reflex tier's action must be a *state the strategic tier can read and respect*, not a silent write
it must discover by consequence.

### Scoring

The design intent is `score(action) = expected_net_per_tick / payback_ticks × strategy_weight`,
terms computed from **existing functions only** (`estimate_building_profit`, placement
affinities, live market prices, wage rate, logistics cost to nearest market, build-cost
amortisation) — no new oracles. With `payback = capex / net` the build score is **`net² / capex`**,
and `corp_ai.cpp` writes it out that way. It reads as capital efficiency and behaves as a
**margin bias**: doubling the margin quadruples the score, doubling the cost only halves it.
**The bias is retained, deliberately**: `focus_weight`, `jitter` and the glut multiplier are tuned
against this curve, and every blessed golden records a world evolved under it. Replacing it with
an explicit linear metric is a re-tune plus a golden reshuffle — BL-417 (build score is
quadratic), Ben's call.

The `strategy_weight` biases toward the corp's generated **industrial focus** (specialist premise,
CORPORATION_GENERATION.md), giving distinct-but-legible personalities for free. A **solvency
gate** (cash − committed spend > reserve floor, `corp_reserve_floor`) vetoes any spend that breaks
the floor; stage B layers priority buckets and predictive spending over it (§ 2B).

### 2B. Stage B — strategy, priority buckets, predictive spending

- **Strategy layer.** `corp_strategy` is a named alias of `industrial_focus` — the corp's
  generated specialist premise (extraction / processing / trade) IS its strategy, kept as its
  own concept so the bias is legible and can diverge from the generation-time focus later
  without a signature break. `focus_weight` biases build/survey scores by this strategy.
- **Priority buckets** (`corp_priority_bucket`: `must_have` / `should_have` / `nice_to_have`),
  derived deterministically from each candidate's `corp_decision_reason` via `bucket_for_reason`
  — `dial_idle` is Must-Have (stops a sustained loss's wage/maintenance bleed); `dial_recipe` /
  `dial_workforce` / `dial_resume` / `trade_surplus` are Should-Have (tune or restore a running
  asset, or bring cash in); `best_build` / `survey_expand` / `hire_available` are Nice-to-Have
  (expansion). Candidates sort bucket-ascending before score-descending, so a Must/Should-Have
  action is never starved by a higher-scoring Nice-to-Have one. Only Nice-to-Have candidates carry
  capex (dials are free), so the "never starve a higher bucket" rule is enforced by gating
  build/survey/hire spend against a **stricter** floor: `corp_should_have_buffer` sums
  `estimate_building_profit(...).input_cost` over the corp's own running processing facilities —
  the cash needed to keep feeding them this tick — and `nice_to_have_floor = reserve_floor +
  should_have_buffer` is the gate a build must clear, on top of (not instead of) the reserve
  floor.
- **Predictive spending.** `forecast_glut_multiplier` forecasts a candidate build's added supply
  (`base_rate × richness × workforce × (1 − hazard)`) over a horizon of
  `build_duration_ticks + forecast_clearing_ticks` (1 — "one clearing pass") against the **local
  market's PUBLIC `supply`/`demand` aggregates only** — the same facts `export_corp_blackboard`
  would show a rival (BL-068, competitor visibility; DISCOVERY.md), never a private read. No
  public demand signal (`demand <= 0`) yields no penalty (the AI cannot forecast against a fact
  it cannot see); the projected supply/demand ratio is unpenalised at or below
  `glut_taper_ratio` (1.0), tapers the build's score linearly to zero at `glut_veto_ratio` (2.0),
  and vetoes (removes the candidate entirely) at or above it. Applied only to build candidates;
  dials and survey are unaffected (a body's total surveyed area doesn't glut a market).

Verified by `tools/verify/corp_ai_predictive_harness.cpp` (R1: the reason→bucket mapping; R2: the
Should-Have buffer is well-defined and never loosens the floor; R3: the forecast is
visibility-honest, monotone, tapers, and vetoes at the documented ratios; R4: an end-to-end
saturated-market scene vetoes a build the plain stage-A scorer would have taken) and
regression-checked against `corp_ai_harness.cpp`.

### 2C. Trading

Ben, 2026-08-07, resolving NR-083: *"Order book needs to be a background process, the AI must be
able to trade as a player does."* A player-only fence over the trade verbs was proposed and
explicitly rejected, so the scorer reaches the order book exactly as it reaches build and survey
— `place_sell_order` is a `corp_verb`, and rival corps are the corps that drive the seam.

**This is a grant of reach, not of skill, and the distinction is the design.** "Can trade" is not
"trades well": a scorer that dumps stock at the floor price is genuinely *worse* than one that
does not trade, because it drags the resolved price down for everyone including itself — and the
auto-surplus path clears that stock at the reference price anyway. So the rule is the narrowest
thing that is still trading:

- **Candidate**: for each `(corp, body)` pool, each resource the local market prices, stock above
  `trade_hold_threshold` (50 units) — well clear of any processor's per-tick draw, so listing can
  never compete with feeding the corp's own chain.
- **Quantity**: `trade_release_fraction` (0.5) of the excess. It meters its release rather than
  emptying the pool into one quarter's clearing.
- **Floor**: `trade_floor_multiple` × the market's `base_price` — the rarity-derived value floor,
  the closest per-resource cost reference the world exposes. The authored value is **0.25**,
  which is the price band's own floor (the lowest price a glutted market can resolve), so surplus
  always clears at whatever the market resolves; anything above 0.25 makes the corp hold on a
  deep glut — a strategy call, not a default.
- **Score**: expected cash valued *at the floor*, not at the current price. The conservative
  estimate, so a listing on a crashed market cannot outscore a genuinely profitable dial.
- **Bucket**: Should-Have. Listing accumulated stock carries no capex — it brings cash *in* — so
  it can never starve a higher bucket, which is the only test the buckets apply.
- **Anti-thrash**: never a second order on a `(corp, body, resource)` that already has one, and
  at most `max_trades` (1) order-book command per evaluation. A trade command's subject is a body,
  not a building, so it takes no dial slot and records no building cooldown.

All three numbers are `corp_ai_params` fields, so tuning is a data change. Two limits are part of
the shape: `base_price` is a rarity floor and not a production cost, so on a resource whose real
cost sits above its rarity floor the AI will sell at a loss (the blackboard's lack of a reference
price is BL-385, blackboard exports no reference price); and the book is **one-sided** — a corp
can release stock and cannot bid for it, the dormant buy side being BL-383 (remove dormant buy
side). A real strategy (price trend, timed release, targeting a rival's shortage) is later work.

Verified by `tools/verify/order_book_harness.cpp` § R5, which asserts the conservatism as
behaviour rather than as intent: never below the rarity floor, never on a pool under the
threshold, never a duplicate, and never on the player's own corp.

### Hysteresis & action budget

- **Do-nothing bias**: a dial candidate must beat the incumbent (or doing nothing) by a relative
  margin `theta` (0.15) — the anti-thrash rule. Applied inside the **dial** enumeration only;
  build, survey, hire and trade candidates pass through no hysteresis test.
- **Cooldowns**: a building that changed recipe/workforce/state holds for `cooldown_evals` (4)
  evaluations. **Both tiers set it** — the reflex tier idles a building directly on
  `building_component` and records the cooldown its state change warrants, so the strategic tier
  cannot reverse an eight-tick-loss idling on its next evaluation. Two tiers own the
  `decommissioned` flag; the cooldown is what keeps them from fighting.
- **One estimator for both directions.** Idle and resume score on the same model —
  `estimate_prospective_profit`, which also makes idled *processors* resumable. Three details make
  that call honest: it takes the **existing** building and reads its real workforce dial rather
  than pricing a fresh probe at construction defaults; it takes the **stack rank the building
  actually holds** rather than charging one further step of stack decay against itself; and it
  credits only the **labour share** of maintenance as saved by idling, since the fixed material
  share survives decommissioning (the BL-049 wage/maintenance split). The idle side carries the
  mirror image. `ai_skill_harness` counts the reflex tier's idlings alongside commands, so
  idle/resume oscillation is readable from the instrument, and its dial-thrash ceilings are
  blessed from runs without it.
- **The workforce dial is scoreable both ways.** `solve_workforce_target` reports its own
  modelled gain (an out-param; it computes both endpoints), so a profitable building can be
  scored for cutting its target and a loss-maker for raising it; a sign taken from the building's
  current variable margin would only ever find one direction.
- **Budget**: per evaluation, at most `max_builds` (1) construction + `max_dials` (3) dial
  changes + `max_trades` (1) order-book command + one hire per corp; total committed spend capped
  by the solvency gate, each accepted candidate reserving its spend against the later ones in the
  same evaluation.
- **Determinism**: stable iteration (sorted `corp_ids`, stored asset order, tile-index order);
  ties break on lowest entity id; the only randomness is `corp_personality_jitter` — a per-corp
  hash of `personality_seed` in [0.9, 1.1] used as a fixed personality jitter on weights —
  constant per campaign, deterministic by construction.

**The decision record.** Each applied command is a `corp_decision` — `{tick, corp, command,
winning_score, runner_up, reason}` — in `corp_decision_ring`, a 256-entry ring that is derived
observability, not save-format state. `runner_up` is **the best candidate the corp did not take**
in that evaluation (rejected by a budget, the one-touch rule or the solvency gate) — the foregone
option belongs to the evaluation, not the command (NR-232).

**A command the seam refused is not a foregone option.** `apply_corp_command` mutates nothing on
refusal, so a refused candidate was never available to take, and it belongs in no counterfactual.
This is not a corner case: the recipe margin-chase is deliberately enumerated without the switch
cost and without the cooldown the seam applies, so the same high-scoring chases are proposed and
refused every evaluation — enough, on the shipped world, to make the top refusal the runner-up of
every decision a corp took.

**Scored within one budget family, never across.** The candidate families — build, dial, survey,
hire, trade, dispatch — are the six action budgets above, and each is scored by **one formula in
one unit**: a build by `net² / capex`, a dial by the estimator's modelled per-tick gain, a trade
by `quantity × floor`, a dispatch by `revenue − leg cost`. Those scales are unrelated, so a
comparison across families states nothing, and an evaluation-wide maximum makes
`runner_up ≥ winning_score` the ordinary case — which is a decision surface that reports the same
thing about every decision. The runner-up is therefore **the best option foregone in the winner's
own family**: the competition this command actually won. Because candidates sort by bucket before
score, `runner_up` can still legitimately exceed `winning_score` within a family (NR-226) —
a Must-Have idle displacing a higher-scoring Should-Have dial; aggregators must not treat the two
as a contest.

---

## 6. The corp-command seam & state export (shared with multiplayer)

The scorer does not mutate the world directly. It emits **`corp_command`** records —
`{tick, corp, verb, args}` — applied at the tick boundary through the player-grade validation
above. This is deliberate triple-duty:

- **AI**: the command stream *is* the decision log. The ring of commands + score rationale
  (winning score vs best foregone) is the AI's legibility surface, its replay artifact, and the
  skill-harness's input.
- **Multiplayer**: `MULTIPLAYER_PRINCIPLES.md` § Preserve now #2 — lockstep exchanges exactly
  this: small serialisable intents tagged with their tick. An AI corp is a local command source;
  a network player is a remote one; the seam is identical. (`canvas_command` stays
  navigation-only; `corp_command` is its sim-mutating sibling.)
- **Out-of-process policies (C/D)**: an external policy consumes the state export and returns
  commands — the same contract, across a process boundary.

**State export** (`export_corp_blackboard`, `--export-blackboard <corp|all>`; BL-206, blackboard
export). A compact, tick-tagged, per-corp view of `corp_fact` records — `(tick, subject,
predicate, value, confidence, provenance)`, schema-versioned, deterministically ordered
(subject-kind section, then entity id, then predicate), emitted as JSONL — that is
**visibility-honest**: it contains only what that corp could see under the BL-068 rules and its
own fog state — own buildings/pools/cash in full; public market prices/aggregates; rival
*buildings* but not their internals; its own routes and survey state. The AI reads through the
same information asymmetry the player does; anything else is the fog cheat the calibration
research warns against.

**The session actor.** `--serve` turns this seam into an external input surface, and validation
written for the trusted in-process caller does not transfer to it. Authority lives at the
**protocol layer**, not in `apply_corp_command` (the in-process scorer legitimately commands every
rival and must keep doing so): a serve session has one **actor** (`--as <corp>`, default the
player corp), `COMMAND` refuses any other corp with `rejected_not_owner`, and `BLACKBOARD` serves
only the actor's visibility-honest view. `--as any` is the explicit research opt-in (bot-vs-bot,
the trace corpus) — permissive mode exists but must be asked for and is visible in the
invocation. Every wire field is range-checked **as the value that lands in the destination**
before any narrowing cast, and a violation rejects the whole command with nothing mutated
(`verb=` is gated to `[0, corp_verb_count)`). The general rule lives in the standing rules: **an
AI-facing seam is an untrusted input boundary; validate the value that lands in the field, at the
boundary.**

---

## 6a. The action dictionary — the word interface's third leg

The seam above gives an out-of-process policy its **write** channel and the state export
its **read** channel. What neither states is *meaning*: which press to choose, and why.
**`docs/ai/ACTIONS.json`** (BL-270, action dictionary; readable mirror `ACTIONS.md`, generated by
`tools/session/render_actions.js`) closes that gap — every control in the game, catalogued
as `{press, typed args, preconditions, expected_output, reason_to_select}` across five
families (gameplay / canvas / lens / ledger / chrome).

Two properties matter for the AI use:

- **The gameplay entries are transcribed, not authored.** Their `args` and `preconditions`
  mirror `corp_command.hpp`'s verbs and `corp_command_result` rejections exactly; where the
  dictionary and the seam disagree, the dictionary is wrong and the fix is mechanical.
- **`reason_to_select` is the policy prior.** It states, in words, why a press exists —
  the design intent a language model conditions on before world state ever enters the picture.
  This is what makes word-based play (and word-driven generation, and the difficulty-level
  work — Ben, 2026-08-02) a prompt-assembly problem rather than a fine-tuning one: blackboard
  (read) + dictionary (meaning) + command seam (write).

The catalogue axes, for orientation: `user_stories.json` = player **intent**;
`question_log.json` (BL-260, UI justification store) = what a readout **answers**; `ACTIONS` =
what a press **does**.

**Consumption model (Ben, 2026-08-02).** A language agent never absorbs the store
whole: it holds the generated **`ACTIONS_INDEX.json`** (`[id, surface]` per action)
in context and fetches full entries on demand via
`tools/session/actions_query.js` (by id, family, or keyword) — the lookup the
MCP server wraps as the model's dictionary tool.

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

- **Surface**: the **chat panel** (`src/ui/chat_panel.{hpp,cpp}`; BL-205, corp chat log) in the
  right shell band, middle slot. Channels: **Public** (every corp), plus **arbitrary groups** (any
  subset of corps, created from the panel). The player reads Public and any group they belong to.
- **Stage A**: messages are **templated, deterministic renderings of the decision log** — the
  reflex-tier agency events ("Meridian idled Ironworks — sustained losses") and the strategic
  tier's command stream. The chat is the AI-observability surface first; personality prose is
  Stage C's.
- **Stage C** (BL-334, Stage C dialogue layer): a small model speaks in-character in channels,
  conditioned on the scorer's decisions (§ 10g); AI↔AI private groups form plans the player
  cannot read — which is principled, because the *medium* is uniform and an intelligence mechanic
  (Discovery-model extension) can expose intercepts as content.
- **Determinism**: stage-A message text derives purely from deterministic events. Free-text model
  chat arrives only under the C-route rules (out-of-process, coarse-grained, replay-logged).
- **Player input**: a message box posts to the selected channel. It has **no mechanical effect**
  — it is the hook Stage C consumes (the player negotiating with AI corps in language).

---

## 8. Decomposition

The build work of BL-199 is carried by:

| Item | Carries |
|---|---|
| **BL-202** (corp AI scored utility) | Stage A: the scorer, the corp-command seam + decision log, the state export |
| **BL-203** (corp AI predictive spending) | Stage B: strategy layer, priority buckets, spending forecast — the solvency answer (§ 2B) |
| **BL-204** (AI skill harness) | Seed-set skill-regression harness (bot-vs-bot goldens: solvency, net-worth curves) + the tick-boundary **state hash** (doubles as the multiplayer desync primitive) |
| **BL-205** (corp chat log) | The § 7 surface: chat panel, channels + groups, agency-event feed |

Economics the scorer prices, settled alongside: **BL-153** (convoy pay by distance) and
**BL-193** (stack diminishing returns); **BL-160** (auto exchange policy) / **BL-161**
(counterparty allow/deny) are the AI's trade-policy primitives — the AI authors the same policy
objects via commands.

---

## 8a. The world history log

The project's first flat-binary serialisation seam (`src/world/history_log.{hpp,cpp}`; BL-208,
world history log): a **single interleaved, append-only, tagged, serialised** world log — not
per-body/per-corp logs, which fail the moment a corporation acts on a body (every interesting
event) and would need a join with no shared ordering.

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
epoch) — no conversion. `decision`/`agency`/`trade_route` entries — which only ever occur during
the live simulation, strictly after the genesis chapter's one-time bulk-insert at world setup —
instead carry the sim day tick. A reader branches on `topic` to know which clock it is reading;
nothing needs the two to compare numerically, because the vector's append order is already the
true chronological order end-to-end.

**The four sources, all additive to their existing consumers** (`ai_decisions`, `agency_events`
and `trade_routes` keep their own readers unchanged):

- **genesis** — `seed_genesis_history(world&, const generation_report&)` copies each body's dated
  `history_event` lines into the log at world setup (`app::setup_world`, right after
  `make_hard_coded_world`), tagged by that body's entity id. This is the bridge from
  `generation_report` (presentation-only; per `hard_coded_world.hpp` it never otherwise reaches
  `world`) into world state.
- **checkpoint** — the same call migrates `planetology_state::checkpoints` alongside genesis, per
  body, merged and stable-sorted into one chronological run. `checkpoint_record` carries no
  timestamp of its own; the resolution rule (a documented simplification, not an exact per-line
  pairing — `history_log.cpp`'s `resolve_checkpoint_timestamp`) takes the LAST dated history line
  at or before the checkpoint's own chain stage.
- **decision** — `corp_ai.cpp`'s strategic-tier push site logs a one-line narration of each
  applied `corp_decision` (verb + reason + score). `corp_decision_ring` is untouched.
- **agency** — both the reflex tier (`economy_system.cpp`'s recipe-rescue and idle-a-loser sites)
  and the strategic tier (`corp_ai.cpp`) log a narrated `agency` entry. The narration knows every
  verb; verbs the chat feed has no vocabulary for (the procurement triple, the convoy pair) are
  logged here and pushed to no feed event, rather than mis-narrated as the feed's kind 0.
- **trade_route** — `supply_system.cpp`'s `credit_arrived_convoys` logs only when a body-pair lane
  is **first established**, never on a repeat completion (which only bumps the existing
  `trade_route`). `world::trade_routes` and `body_activity_visibility` are untouched.

  **Two entries per establishment** (BL-282, route log tags both bodies). A new route is a
  *two-body* event but `world_history_entry` carries one body tag, so tagging only the
  destination would make a body-scoped "what happened at X" filter miss the route from its
  **source** side. The branch pushes **two** entries — one tagged `src_body`, one tagged
  `dest_body` — carrying the *same* narration naming both endpoints. Order within the tick is
  fixed **source then destination**, not iteration-dependent, so replays stay byte-identical.

  *Why not widen the struct.* A `body_b` field would change a struct four other call sites depend
  on, for one topic's need, and would leave every other topic carrying a field it never sets. Two
  tagged entries keep the one-body invariant and keep every existing reader correct unmodified.

  *The cost.* Route establishment doubles in log volume — bounded by **body-pair count**, not
  convoy traffic (a route is logged once, not per delivery), so the absolute growth is small: with
  *n* bodies the ceiling is `2 × n(n−1)/2` entries per corp instead of `n(n−1)/2`. The real price
  is duplicated narration: a reader rendering chronologically sees the same sentence twice with
  different body tags. Mitigation if that reads badly — keep the narration identical (it is) and
  let the renderer de-duplicate on `(tick, event)` when it is *not* filtering by body.

**Serialisation** (`write_history_log`/`read_history_log`): a leading 4-byte magic (`"IOHL"`) plus
a `uint32_t` version (BL-107's magic-and-version rule), then an entry count, then each entry
length-prefixed. `read_history_log` rejects — rather than misreads — a wrong magic, an
unsupported version, an unknown topic byte, or a truncated stream, leaving the destination
world's `history_log` untouched on any rejection. Round-trip is field-identical. The whole-world
snapshot (`world_save.cpp`) embeds this stream whole, nested magic and all, rather than
re-defining its bytes. Verified by `tools/verify/history_log_harness.cpp` (built over the real
generated world, not hand-fabricated entries) and a determinism check in
`tools/verify/determinism_harness.cpp` (two identical-seed generations produce an identical
genesis+checkpoint chapter).

The nation and corporation generation passes write into this same log — the substrate it exists
to hand them.

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

**Two numbers the C-route sizing rests on.** Vox Deorum's per-decision latency and token cost are
published: **~1 minute per decision** (model-dependent) and **20.35M input / 555k output tokens per
complete game** for `gpt-oss-120b`. Paradox's per-tick AI CPU budget is not published (the
tick-task *mechanism* is) and is moot for the C-route — the model runs out-of-process and off the
sim's tick budget entirely.

---

## 10. MCP, the public field, and the small-local-model direction

A wide survey of publicly available LLM-grand-strategy work (2026-08-03), and the direction Ben
set on reading it. It answers NR-040's "what plumbing?" question.

### 10a. What MCP is, and why it is the answer here

**MCP (Model Context Protocol)** is an open protocol for connecting a language model to context
and actions. It is a *cable standard*, not a capability — it does not make a model smarter, it
standardises the socket a model plugs into.

The shape: a **server** exposes a resource (here, the game); a **client** is whatever agent
runtime is driving (Claude Code, Claude Desktop, a Gemini/Codex CLI, an Ollama-backed local
loop, or a bespoke harness). They speak **JSON-RPC 2.0 over stdio** (the server is a
subprocess) or over HTTP. The server offers three primitives:

- **tools** — functions the model may call, with typed arguments and enumerated failures.
- **resources** — read-only data the model may fetch by URI.
- **prompts** — reusable templates that tell the model how to use the above.

Four properties are what make it the right seam for Io:

1. **The model side stays entirely out of process.** `ProjectIo.exe` ships **no HTTP client, no
   API key, no cloud dependency**. The MCP server is a local process; with no client attached
   the game is byte-identical. This is the exact thing NR-039's walk-back rejected — a live API
   call inside the econ tick — avoided by construction rather than by discipline.
2. **Model-agnostic by design.** The same server serves a cloud frontier model and a
   locally-hosted small model, with no change on the Io side. That swap *is* the direction in
   § 10d, so the interface must not care which model is attached.
3. **Fair by construction.** Tools are the only write channel, so the model plays through the
   same validation and the same visibility rules as the player. The field converged on this
   independently — `civ6-mcp` routes every agent action through Civilization VI's own
   rule-enforcing Lua APIs rather than mutating state.
4. **It is the field standard.** Vox Deorum, `civ6-mcp`, `civStation` and CivBench all
   arrived at an MCP wrapper over a fixed verb list, separately. Adopting it costs nothing in
   originality and buys every existing client for free.

**Io's three legs fill the primitives near-mechanically:**

| MCP primitive | The Io asset that fills it |
|---|---|
| **tools** | `corp_command.hpp`'s verbs — typed, validated, rejection-enumerated (`corp_command_result`) |
| **resources** | `--export-blackboard` JSONL (BL-206) — visibility-honest, deterministic ordering, schema-versioned |
| **prompts** | `reason_to_select` in `ACTIONS.json` (BL-270) — the design-intent prior, in words |
| the lookup | `ACTIONS_INDEX.json` + `actions_query.js` — the hold-the-index, fetch-on-demand pattern an MCP tool wraps |

The plumbing NR-040 asked about is **one wrapper**, not a subsystem.

### 10b. The public field

| Project | What it is | The transferable lesson |
|---|---|---|
| **Cicero** (Meta, 2022) | LLM dialogue + strategic planner for full-press *Diplomacy*; top-10% on webDiplomacy | Language and planning as **separate modules** with the planner controlling the language model — still the reference architecture for negotiation |
| **Vox Deorum** | LLM macro-strategy over Civ V + Vox Populi; **2,327 full games** | The load-bearing result — see § 10c.1 |
| **civ6-mcp / CivBench (Civ VI)** | MCP server (76 tools) over Civ VI's FireTuner debug protocol; open benchmark over frontier models | Names the two dominant failure modes (§ 10c.3) |
| **CivBench (Civ V)** | A different project of the same name, from the Vox Deorum authors: victory-probability estimators trained on turn-level state across 307 games | Progress-based evaluation — scoring a game before it ends |
| **civStation** | Voice-commanded Civ VI agent over a layered MCP | Human-sets-strategy / agent-executes — the counsel model, not the autopilot model |
| **CivAgent** (fuxiAIlab) | LLM digital player inside *Unciv*; explicitly built as a **data flywheel** | The play-to-collect-traces pattern Io's § 10d adopts |
| **CivRealm** (BIGAI) | Freeciv-web env with a server-proxy-client harness, RL *and* LLM agent interfaces | Turn-based pacing suits LLM latency; a proxy is the clean isolation seam |
| **SAGA** | Scene-graph + tool-augmented planner + dual-horizon feedback, over CivRealm | The best-documented recipe for *fixing* long-horizon play (§ 10c.4) |
| **Richelieu** | Self-evolving Diplomacy agent: memory, reflection, self-play, **no human data** | Self-play as the corpus source when no expert demonstrations exist — Io's exact position |
| **Agents of Change / HexMachina** | Catan agents that rewrite their own prompts and player code | Separating environment *discovery* from strategy *refinement* |
| **DSGBench / WarAgent** | Six-game strategic benchmark; multi-agent WWI/WWII/Warring-States sim | Fine-grained per-dimension scoring beats a single win-rate number |

### 10c. What the research actually says about *strategy*

The question is **not** "can an AI beat a human" — that was settled by AlphaStar and is not Io's
goal (§ "The goal": roughly human skill, beatable, legible). The useful findings are about *how*
these agents play, what breaks, and what a **small, local, text-driven** model can be expected
to do.

**1. Open-weight models already reached parity with a tuned algorithmic 4X AI — with a simple
prompt and no fine-tuning.** Vox Deorum's 2,327 games ran **`gpt-oss-120b` and `GLM-4.6`**, not
frontier cloud models: **97.5% survival vs the algorithmic baseline's 97.3%**, with all observed
differences statistically non-significant. This is the single most encouraging result for Ben's
plan — the bar Io wants (a genuine, beatable rival) was cleared by open weights, out of the box.
**The caveat is size, not openness**: 120B-class weights are open but not *small*. The gap to
close is 120B → a model that runs on Ben's machine, and that is a distillation problem (§ 10c.6),
not a capability problem.

**2. The LLM should hold only the macro layer.** Every project that worked delegated tactics to
algorithmic subsystems. Vox Deorum "decapitates" the game's algorithmic AI — replaces its
top-level strategic module and leaves every micro-tactical system in place. This is the A → B
utility core with a coarse layer above it that § 2 recommends, with 2,327 games of evidence
behind it rather than one paper.

**3. Personality is emergent and free.** Vox Deorum's two models developed distinguishable
styles unprompted — `gpt-oss-120b` leaned domination (**+31.5% domination victories** vs
baseline), `GLM-4.6` weighted conquest and culture evenly. Io does not need to author
personalities; it needs to *not flatten* the ones that appear. That is a strong argument for
keeping the strategy prompt thin and letting `focus_weight` (§ 5) supply the bias.

**4. The failure modes are consistent everywhere, and none of them is about raw intelligence.**

- **Myopia / step-wise greed.** Step-by-step reasoning is structurally a *greedy* policy: fine
  over short horizons, and it makes early commitments that amplify over time and cannot be
  recovered from. The stated minimum fixes are **explicit future evaluation, backward value
  propagation, and limited commitment** — and even *one-step lookahead* escapes traps where all
  step-wise strategies provably fail.
- **The sensorium effect** (CivBench, Civ VI). Agents miss information they never think to query.
  A purely *pull*-based tool interface silently punishes an agent for not knowing what to ask.
- **The knowing-doing gap** (CivBench, Civ VI). Models articulate the right strategy and then fail
  to execute it. The worked example: Opus-controlled Portugal spent 50 turns planning and
  executing a nuclear campaign to stop a French cultural victory, struck twice — and lost anyway,
  to a *diplomatic* victory it had stopped tracking.
- **The observation-belief and belief-action gaps** (studied on Llama-3.1, Qwen3 and `gpt-oss` —
  precisely the open-weight class Io is targeting). Models' *internal* beliefs about hidden state
  are measurably **more accurate than their own verbal reports**; those beliefs degrade with
  multi-hop reasoning, show primacy and recency bias, and drift away from Bayesian coherence over
  long interactions. Practical reading: **never ask a small model to carry world state in its
  head across a campaign — hand it the state, every time.**
- **Spatial blindness.** Tile maps defeat flat text context. SAGA's fix is a **map-semantic scene
  graph** that renders typed spatial relations as short per-unit natural-language context,
  instead of inflating the global token budget with the whole map.

**5. What measurably improves play, cheapest first.** This is the actionable ranking:

1. **Push state to the model rather than making it pull** — kills the sensorium effect outright.
   Io's blackboard export is a push artifact; the MCP server leads with it rather than making the
   model discover it.
2. **Pre-render context into words**, including spatial relations (SAGA's scene graph). Io's
   `expected_output` / `reason_to_select` fields are this.
3. **Mask to legal primitives.** Universal across every project; Io gets it free from
   `corp_command`'s validation.
4. **Add goal persistence with periodic re-evaluation, plus cross-game post-mortem** — SAGA's
   dual-horizon loop, Richelieu's memory-and-reflection, Agents-of-Change's self-rewriting
   prompts. This is the documented answer to both myopia and the knowing-doing gap
   (`STRATEGIES.md` carries Io's version; a goal layer above the scorer is BL-336, goal layer /
   myopia mitigation, parked until the scorer is observed exhibiting the failure mode).
5. **Only then, a bigger model.** Every project reports the scaffolding mattering more than the
   model tier.

**6. Distilling to a small local model is now a routine recipe, and on-task specialists beat
generalists.** Reported results in this class: a **1.7B specialist trained from traces beating a
744B frontier model on its target task**; Llama-3.2-3B going from 0.76% to ~55% on GSM8K purely
from frontier-model traces; 7B/8B supervised fine-tunes reaching 60% accuracy within ~1,000 steps
on a card-selection task. The recipe is consistent — **collect teacher traces → have the teacher
expand them into ~10k in-domain synthetic examples → filter with rule-based validators for
format, schema and novelty → SFT**. Io's advantage is that the validator is already built and
free: `corp_command_result`'s typed rejections mechanically identify any trace containing an
illegal or invalid command.

**7. The cost number that sizes the data plan.** `gpt-oss-120b` averaged **20.35M input tokens
and 555k output tokens per complete Civ V game**. Read it twice: it is why per-tick cloud
inference is not a shipping architecture, *and* it is the yield — one game produces ~555k output
tokens of decision trace. A few hundred logged games is a serious supervised corpus.

### 10d. The accepted direction (Ben, 2026-08-03)

**MCP is sanctioned.** Io has an MCP server over the three legs — blackboard export (read),
action dictionary (meaning), corp-command seam (write). This replaces the computer-use reading
NR-040 recorded as the interim plan.

**The runtime target is a small, local model.** Not a cloud dependency, not a frontier model —
a model that runs on Ben's own machine, playing through text. Cloud inference is **not** the
shipped runtime and never becomes one.

**Cloud's only role is corpus generation.** A frontier model plays Io through the *same* MCP
interface; every decision is logged as an input/output pair — the blackboard state and dictionary
slice that went in, the command and its rationale that came out. Those pairs are the supervised
fine-tuning corpus for the local model. This is CivAgent's data-flywheel pattern and Richelieu's
no-human-data self-play, applied to a game whose deterministic seeds can generate the scenarios
for free (§ 3). Which layer that corpus trains is § 10g's ruling.

**The goal remains "fair, beatable, legible", not "strong".** Note that § "The goal" puts the
ceiling somewhere a fine-tune cannot reach it either way: the close race is produced by the world's
brakes, not by the agent's restraint, so a model that played *harder* would be answered by the same
coalitions and the same commons as any other leader. § 10c.1 is what makes this
credible: parity with a tuned algorithmic 4X AI was reached *without any fine-tuning at all*, so
the fine-tune's job is to get a **small** model to that bar — not to exceed it. Superhuman play
is explicitly not the target and would be a regression against the § "The goal" statement.

**What this does not change.** The simulation stays deterministic: the model is out-of-process,
commands still apply at the tick boundary through player-grade validation, and NR-039's proposed
determinism carve-out stays reverted. Trace logging gives replay for free — the corpus and the
replay log are the same artifact.

### 10e. The MCP server and the serve protocol

Two items carry the direction: **BL-278** (Io MCP server) — the § 10a wrapper — and **BL-279**
(trace corpus) — trace logging, corpus format, and the play → SFT-dataset pipeline (§ 10d, as
rescoped by § 10g).

**The serve mode.** `ProjectIo --serve [--ticks N] [--as <corp-id|any>]` (`src/main.cpp`) is the
persistent headless mode: it builds the canonical world once, then reads one request per line
from stdin and writes one response per line. The opcodes — `TICK`, `BLACKBOARD corp=<id>
ticks=<n>`, `COMMAND corp=<id> verb=<n> ...`, `CORPS`, `BODIES`, `SHUTDOWN` — live in
`src/core/agent_protocol.{hpp,cpp}`, shared verbatim with the live seam (§ 10j), and reuse
`export_corp_blackboard`/`to_jsonl` and `apply_corp_command` (no bypass) underneath. `COMMAND`
parses every argument key every verb reads, and every `corp_command_result` has a name on the
wire, so a business decline ("the supplier holds no capacity", "you are embargoed") is never
reported as a syntax error — typed, enumerated failure is the property § 10a leans on for
self-correction. A `TICK` runs the whole tick sequence, surveys included, so a dispatched survey
progresses and reveals tiles an agent can then build on.

**Enumeration.** `CORPS` returns one JSON line per corp (`id`, `name`, `is_player`,
`home_nation`) then `END`, because corp ids in a generated world are non-obvious (NR-061).
`BODIES` is its sibling: `survey`, `place_sell_order` and `request_quote` all take a body id as
`subject`, and the blackboard keys pool facts by the corp's own `(corp, body)` pool and market
facts by **market** id — so without `BODIES` an agent could never name a body it has no pool and
no activity on, which is every body worth surveying.

**The wrapper.** `tools/mcp/server.js` spawns that process and speaks MCP-over-stdio to it —
hand-rolled JSON-RPC 2.0 (no SDK dependency) covering `initialize`, `tools/list`, `tools/call`,
`resources/templates/list` and `resources/read` (`blackboard://<corp>`). Seven tools:
`get_blackboard`, `issue_command`, `advance_tick`, `lookup_action`, `list_actions`, `list_corps`,
`list_bodies`. `get_blackboard` always returns the full current-tick blackboard — the
push-not-pull call from § 10c.5. The `reason_to_select` leg is served through
`lookup_action`/`list_actions` rather than as MCP `prompts/*`. `server.js` resolves the binary
across the Linux (`ProjectIo`) and MSVC (`ProjectIo.exe`) layouts; `--attach <port>` connects to a
live session instead of spawning a child (§ 10j).

**The committed checks.** `tools/mcp/smoke.js` drives `--serve` over the raw line protocol and
asserts the *shape* rather than the economics: every opcode answers, every verb reaches the seam
and returns a code that is actually in `corp_command_result`, a well-formed sell order is
distinguishable from a malformed one, and `SHUTDOWN` is acknowledged. It asserts nothing about
whether a given command *should* succeed — that is the `tools/verify/` harnesses' business.
`tools/mcp/session.js` is its sibling for play (§ 10h). The rule both encode: **a seam nobody
exercises is a seam nobody can trust.**

### 10h. AI play as a bug-finding instrument (Ben, 2026-08-13)

Ben: *"It seems like pushing for AI play will expose more bugs and give us actionable
improvements now."*

**This is a reframe of why the word interface matters, and it is worth stating separately from
§ 10d's runtime argument.** § 10d justifies MCP and a local model as the road to a *rival*. This
says the interface pays before any of that ships, because **an agent playing the game is a test
oracle no harness replicates**. Three sources of defects, in ascending order of yield:

1. **Running an existing instrument** — the skill harness's action tally, once it names every
   verb, reads an oscillation that had been the AI's dominant behaviour.
2. **Reading the seam** — protocol defects that never fail a run because nothing re-runs it.
3. **Asking what a player would actually try** — how a verb whose effect never arrives, an
   unnameable body, or a collapsed decline code surfaces. Nobody had played it.

A harness asserts what its author already suspected. A player discovers what nobody thought to
assert — and an *agent* player does it repeatably, at machine speed, and writes down what it tried.
Io's determinism makes this unusually strong: the world rebuilds identically, so a play transcript
is a replay artifact, and "append a move and re-run" reproduces the whole session byte-for-byte
before extending it.

**The instrument is `tools/mcp/session.js`**, alongside `smoke.js`. The division is deliberate:
`smoke.js` asks whether the protocol *answers* (shape, every verb, typed results), `session.js`
asks what happens when someone *plays* (agenda, consequence, surprise). Both are committed.

**What this does NOT change.** The § 10g ruling stands: the deterministic scorer remains the
action generator for the shipped rival, and an agent playing a corp is the research use of the
MCP server that § 10g preserves. What changes is the *value* assigned to that research use — it
is diagnostics, not spectacle.

### 10i. Spectator mode — the seat, not the exception (BL-409, spectator mode)

Ben, 2026-08-14, asked for a way to **watch the AI play in real time** and discover what
strategies emerge — explicitly as a way to describe and explain the project's aims, not as
a feature. Most of what that needs exists: the clock runs to 16× (a quarter every ~11 real
seconds), rivals act through `corp_command`, and every decision records `winning_score` /
`runner_up` / `reason`. What it adds is a **window** (the decision feed, BL-407, and the Strategy
readout, BL-411), and one rule.

**The rule is not excepted; its subject is removed.** The item's draft asked whether the scorer
could be granted the player's corp under a spectate flag — a fourth narrow exception alongside
the reflex tier, the workforce dial and the rival tier. Ben rejected the framing: *"In spectator
mode, there is no need to mark a corp as played by a human. 'Who plays your corp' collapses as a
question."*

That is the better model and a smaller change. The standing prohibition protects a corp
**because a human owns it**. A spectated session has no human owner, so the precondition is
absent rather than waived. `world::player_entity` survives only as a camera and ledger
anchor — a **viewpoint**, carrying no ownership meaning. Concretely, `corp_ai_params
::spectating` makes the two guards in `corp_ai.cpp` conditional, and nothing else in the
evaluation loop distinguishes the player's corp, so past those guards it is simply another
corp on the same staggered cadence.

**Two properties keep it honest**, both asserted by `tools/verify/spectator_determinism.cpp`:

- The flag **defaults false**, and an unspectated run is byte-identical to a build without the
  guards — the harness carries the dated `state_hash` provenance, and a spectated run hashes
  differently, so "spectating moves the world" has teeth too.
- Admitting one more corp **shifts no rival's cadence slot**, because the cadence index is
  over the sorted corp set, which does not change.

**The way in is a launch flag, and only a launch flag** (BL-695, live spectate route).
`ProjectIo --spectate` opens the session with nobody seated; it composes with the `--autostart`
family, with `--load` and with `--host-agent`. There is deliberately **no in-game control** that
enters or leaves the mode. The reason follows directly from the paragraph above: because the
prohibition's *precondition* is what spectate removes, having-no-owner is a property of the
**whole session**, not a view a watcher steps into and out of. A mid-run flip would change,
halfway through a campaign, which corporations the scorer may legally act on — a different
argument, and one nobody has made. Off by default, so an ordinary played session never reaches
any of this.

Two companions complete the seat: player-press affordances (construction placement, order entry,
workforce dials) are disabled under spectate — BL-413 (spectate disables player presses) — and
the spectated viewpoint opens on a corp worth watching rather than whichever corp the player would
have been — BL-418 (spectate viewpoint default).

### 10j. The live agent control seam (BL-412, live agent control seam)

`--serve` is headless-only by construction: `main.cpp` branches to the line-protocol loop
*instead of* `app::run`, so an MCP-attached agent plays invisibly and the only way to observe its
play is to read a transcript afterwards. Spectating the deterministic scorer (§ 10i) and
spectating a *language model* are therefore different jobs. This seam is the second one: the
**rendered app hosts an agent**, so a human watches an external model play a corp on the live
canvas.

**Transport.** `ProjectIo --host-agent [port]` (default 7717, composes with
`--autostart-play`) opens a loopback TCP listener once a campaign is running —
`src/core/agent_seam.{hpp,cpp}`, polled non-blocking from the frame loop, no worker thread.
The engine only **listens**: no outbound connection, no HTTP client, no API key — the § 10
invariant, untouched. The protocol is `--serve`'s line protocol *verbatim*: both hosts
(`run_serve` for headless use, and the seam) call the same parser in `agent_protocol.cpp`, the
same field validation, the same actor gate. The session actor is the player corp — the seat the
agent occupies (this is *not* § 10i's no-seat mode; the seat is occupied, just not by a human).
Client side, `tools/mcp/server.js --attach <port>` connects to a live session instead of spawning
a child.

**Clock ownership — the agent gates the clock.** Attaching pauses the sim; a `TICK` request
releases exactly one econ tick (`sim_loop::advance_days(90)` through the same
day/econ-boundary crossings live play uses). The session stalls while the model thinks —
honest, and right for watching. The human keeps override: the speed keys still work, and
with the clock running a `TICK` waits for the next natural boundary instead.

**Determinism — commands land at tick boundaries only.** A wall-clock session where commands
apply on whichever *day* the bytes arrive is not replayable, which forfeits § 10h's central
property. So the seam defers every `COMMAND` to the econ boundary: applied there in arrival
order, against the post-step world of the tick just completed, stamped with that tick, and
**recorded** — `(tick, corp, verb, args, result)` in memory and `agent_transcript.log` on
detach/exit. `tools/verify/agent_seam_harness.cpp` proves the contract headless: a
socket-delivered schedule hashes identically to the same schedule applied in-process, the
transcript replays to the same `state_hash`, and an out-of-domain command is rejected whole
with the hash untouched. Request order is preserved on the wire (reads answer immediately
only while nothing is deferred ahead of them; `TICK` is a sequence point), so a lockstep
client frames responses exactly as it does against `--serve`.

**Scope.** § 10g stands: the deterministic scorer remains the shipped rival's action
generator. This seam is the research and diagnostics configuration § 10h argues for —
frontier-or-local models playing a corp interactively, watchable.

### 10f. Sources for § 10

- Vox Deorum — hybrid LLM architecture for 4X, 2,327 games, open-weight parity, per-game token cost. https://arxiv.org/abs/2512.18564 · https://github.com/CIVITAS-John/vox-deorum
- CivBench (Civ VI, `civ6-mcp`) — MCP-driven Civ VI benchmark; the sensorium effect and the knowing-doing gap. https://tasolabs.com/blog/ai/introducing-civbench-season-001
- CivBench (Civ V) — *Progress-Based Evaluation for LLMs' Strategic Decision-Making in Civilization V*, from the Vox Deorum authors; a different project sharing the name. https://arxiv.org/html/2604.07733v1
- civ6-mcp — MCP server over Civ VI's FireTuner protocol; rule-enforcing API as the write channel. https://github.com/lmwilki/civ6-mcp
- civStation — layered MCP, human-sets-strategy/agent-executes. https://github.com/NomaDamas/civStation
- CivAgent — LLM digital player in Unciv; the data-flywheel framing. https://github.com/fuxiAIlab/CivAgent · https://arxiv.org/html/2502.20807v1
- CivRealm — Freeciv env with server-proxy-client harness for RL and LLM agents. https://github.com/bigai-ai/civrealm · https://arxiv.org/abs/2401.10568
- SAGA — scene graph, tool-augmented planner, dual-horizon feedback on CivRealm. https://arxiv.org/abs/2606.29932
- Richelieu — self-evolving Diplomacy agents; memory, reflection, self-play without human data. https://arxiv.org/abs/2407.06813
- Agents of Change / HexMachina — self-evolving agents rewriting their own prompts and code. https://arxiv.org/abs/2506.04651
- Why Reasoning Fails to Plan — step-wise greed, myopic deviation, and the three requirements for long-horizon coherence. https://arxiv.org/abs/2601.22311
- Why Do LLMs Struggle in Strategic Play? — observation-belief and belief-action gaps in open-weight models. https://arxiv.org/abs/2605.00226
- DSGBench — six-game strategic benchmark with per-dimension scoring. https://arxiv.org/abs/2503.06047
- WarAgent — LLM multi-agent simulation of historical conflicts. https://arxiv.org/abs/2311.17227
- Cicero — human-level full-press Diplomacy; controllable dialogue model + planning engine. https://ai.meta.com/research/cicero/ · https://github.com/facebookresearch/diplomacy_cicero
- Small Language Models for Efficient Agentic Tool Calling — targeted fine-tunes beating much larger models. https://arxiv.org/abs/2512.15943
- Model Context Protocol — the specification. https://modelcontextprotocol.io

### 10g. Ruling on the C-route feasibility note (Ben, 2026-08-08 — NR-094)

`docs/ai/LANGUAGE_POLICY_FEASIBILITY.md` ran the two feasibility gates § 10d implicitly set — can
the runtime model be compressed, and does it run in budget — and both pass (3–8B compresses to
Vox Deorum's already-open-weight-parity bar; the per-decision budget is ~90s at 1×, ~5.6s at 16×
for 8 rivals, computed from `sim_loop`'s own constants, against ~3–7s of measured 8B-Q4 decode).
Neither gate disqualifies the local-model runtime target.

**The note also fixed which seat the model occupies.** A "small local model plays through text"
framing reads as the model calling `issue_command` directly — the model AS Stage A/B, replacing
the scorer's action-emission. That was never the plan § 7 set: **Stage A/B (the deterministic
scorer) is the action generator; Stage C is the model speaking in-character in channels** — a
dialogue layer over the decision stream, not a replacement for it. MCP and the local-model target
both stand; the ruling is explicit about the Stage.

**Ruling: adopt the Cicero configuration the note recommends, as Stage C's concrete shape.**

- **Stage A/B stays the action generator, indefinitely.** `corp_ai.cpp`'s scored-utility core
  (or a future RL policy, § 8 of the note — not disqualified, just not chosen) keeps emitting
  `corp_command`. Distilling `corp_ai.cpp` yields, at best, `corp_ai.cpp` — there is no skill
  upside to buy, only legibility and determinism to lose, and both are named requirements
  (§ "The goal").
- **Stage C is a conditioned dialogue layer over the `corp_decision` ring**, not an independent
  planner. Cicero's shape exactly: the scorer's winning command + reason code IS the "intent" a
  small (Cicero's own reference point: 2.7B) model is conditioned on to speak in Public/private
  channels (§ 7). This is where the diplomacy capability lives — separable from action
  generation, per the note's § 7. BL-334 (Stage C dialogue layer) owns it.
- **A goal layer above the scorer stays a live, separate option** — BL-336 (goal layer, myopia
  mitigation), for when/if step-wise myopia (§ 10c.4's documented failure mode) is actually
  observed rather than assumed.
- **The constraint tax (the note's § 6) is the reason this isn't merely a style preference.**
  Small models measured dropping from 91.5% to 48.0% executable accuracy under a hard schema,
  with the damage entering where instructions suppress deliberation — exactly the failure mode a
  model-emits-`corp_command`-directly design sits on. Keeping the scorer as the action generator
  avoids the risk instead of mitigating it.
- **The trace corpus (BL-279) trains Stage C.** Its corpus is bootstrapped from `corp_ai.cpp`'s
  own decision ring first (free, no cloud spend, exactly the note's § 9 "bootstrap from the
  scorer" instruction) and trains the **dialogue layer**, not an action-emitting model. Cloud
  play through the MCP server remains valuable for a DIFFERENT reason: an external agent
  (frontier or otherwise) playing a corp interactively is the research and diagnostics use of the
  server (§ 10h), not the shipped rival AI's architecture.
- **MCP, the server, and the local-model-as-runtime-target all stand exactly as § 10d states.**
  This ruling changes which Stage the model occupies, not the transport or the
  no-cloud-dependency invariant.

**What this does not settle.** Whether Stage C ships before or after the v0.2.0 corp-AI arc
closes, and the model size/quantisation for Stage C specifically (Cicero's 2.7B is a reference
point, not a spec) — both inside BL-334's remainder. The note's ~300-token-per-decision assumption
was measured separately (BL-335, measure decision token cost): a minimal decision round is
~19–20K input tokens, a naive one ~26K, with output under 300 tokens; the compact encoding that
brings a round to ~1.5–5K tokens is BL-481 (compact blackboard encoding).
