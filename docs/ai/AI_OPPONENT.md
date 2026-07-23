# Project Io — AI Opponent

Authority doc for the AI-opponent thread (BL-199). Ben's 2026-07-23 call: AI-opponent
development may now proceed **alongside** the basic mechanics rather than waiting. This
document opens with the **state-of-the-art research** BL-199 mandates as its first
activity; the architecture proposal, data strategy, and follow-on backlog decomposition
build on it.

**Status.** SOTA map + shortlist + staged path + data strategy: drafted below (deep-research
harness, 2026-07-23). **Still owed** before the standing-rule relaxation is *earned* (the rule
still defers AI faction behaviour beyond the stub — see `.claude/rules/io-standing-rules.md`):
Ben's acceptance of a target architecture, and decomposition into follow-on backlog items.

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

## 5. Remaining BL-199 deliverables (owed)

The research above covers BL-199's SOTA map, shortlist, data strategy, and staged path. Still to do
before AI behaviour is built:

1. **Ben's acceptance** of the target architecture (the A → B utility core is the recommendation).
2. A concrete **decision decomposition** for Io's A-tier scorer: the exact candidate-action set, the
   scoring terms per action, the hysteresis/action-budget model, and how it extends the BL-079
   `run_economy_step` agency seam.
3. The **compact state-export schema** (the JSON an out-of-process policy would read) — designed
   now even if C/D are deferred, since it shapes A's internal state too.
4. Decomposition into **follow-on backlog items** (the A scorer; the B predictive-spending/priority
   model; the seed-set skill-regression harness), each targeting v0.2.0 per BL-199's version goal.

Only once (1)–(2) land does the standing-rule relaxation take effect for the corresponding scope.

---

## 6. Citations

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
