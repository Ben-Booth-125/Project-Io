# One-Shot Generation of Actions for a Language-Driven 4X Opponent

**A feasibility assessment — does the C-route compress, and does it run here?**

Research note, 2026-08-08. Companion to [`AI_OPPONENT.md`](AI_OPPONENT.md) § 10.

> **Ruled.** Ben adopted the recommendation in § 9 — the Cicero configuration — as Stage C's
> concrete shape (2026-08-08, NR-094; `AI_OPPONENT.md` § 10g). `AI_OPPONENT.md` § 10d stays the
> direction (MCP transport; small local runtime model; cloud inference as corpus generator, never
> as runtime); this note answers Ben's two feasibility questions — *can it be compressed?* and *is
> it technically possible on our machines?* — in the affirmative (§ 4, § 5), and § 7 argues that
> the diplomatic capability motivating the C-route is **separable from action generation**. The
> goal-layer recommendation in § 9 is parked as BL-336 (goal layer, myopia mitigation) pending
> evidence that the scorer exhibits the myopia the literature predicts.

---

## Abstract

This note assesses whether a language model can serve as the action-generating policy for the AI opponent in *Project Io*, a corporation-scale 4X strategy game, and whether such a policy can be compressed to run locally on commodity hardware. The assessment is framed by two disqualifying criteria set in advance: if the policy cannot be compressed to a small local model, or cannot meet the simulation's real-time budget on available machines, the language route is abandoned in favour of conventional reinforcement learning.

We find that the first criterion is likely met and the second is met with margin, but that the framing conceals a more consequential result. The strategic value the language route is being pursued for — diplomacy and higher-order strategy — is **separable from action generation**, and the strongest published system in this class (Cicero) obtains it while keeping actions under algorithmic control. We therefore recommend against making the language model the action generator, and in favour of adopting it as a *conditioned dialogue and goal layer* above the existing deterministic scorer. This preserves the stated motivation while avoiding the principal technical risk identified below, and it does not require abandoning the reinforcement-learning option — the two are complements, not alternatives.

A subsidiary finding is reported: the negative result obtained in the author's 2023 dissertation on entailment-tree generation does **not** transfer to this domain, and the reason it does not transfer is instructive for the design.

---

## 1. Motivation and decision criteria

The question posed is whether *play via language* is worth pursuing for Io's computer opponent. The stated intuition is that language-mediated play "opens up the door to diplomatic thinking and larger strategy" — that is, that its value lies not in per-move quality but in capabilities the current deterministic scorer structurally cannot express.

Two disqualifying criteria were set:

- **C1 (compressibility).** The policy must reduce to a model small enough to ship and run without cloud dependency.
- **C2 (tractability).** It must run on available hardware within the simulation's real-time budget.

Failing either, the fallback is "traditional RL methods."

This note evaluates C1 and C2 on the evidence, then argues that the decision they are meant to settle has been mis-scoped, and proposes a reframing.

---

## 2. The system under assessment

Project Io is a solo-developed C++/Lua 4X strategy game in which the player controls a corporation rather than a nation. Its AI-opponent architecture (documented in `docs/ai/AI_OPPONENT.md`) has already reached a state unusually well prepared for a language policy. Four components are relevant, all verified against the source at time of writing.

**The write channel.** `src/world/corp_command.hpp` defines the AI's action surface as a **fixed-arity flat record** over an append-only `corp_verb` enum — `build`, `demolish`, `set_recipe`, `set_workforce`, `idle`, `resume`, `place_road`, `survey`, `hire_unit`, the order-book pair, the procurement triple, the convoy pair, the four stance verbs, the three unit-march verbs and `withdraw_from_battle`; `corp_verb_count` is always the last enumerator plus one.

`corp_command` carries typed fields (`subject`, `tile`, `province`, `type`, `target`, `recipe`, `workforce`, `road_tier`, `unit_type`, `quantity`, `floor_price`, `order`, `counterparty`), of which at most four are meaningful for any given verb. Application is through `apply_corp_command`, which routes to the same player-grade validation seams and returns one `corp_command_result`: `applied`, or a typed rejection (`rejected_no_corp`, `rejected_not_owner`, `rejected_invalid`, `rejected_placement`, `rejected_funds`, `rejected_state`, the three lock rejections, the four procurement declines and `rejected_cooldown`). A rejection mutates nothing.

**The read channel.** `export_corp_blackboard` produces a visibility-honest, schema-versioned, deterministically ordered list of `corp_fact` records — tuples of `(tick, subject, predicate, value, confidence, provenance)`. It is, structurally, a flat fact list.

**The meaning channel.** `docs/ai/ACTIONS.json` catalogues every control as `{press, typed args, preconditions, expected_output, reason_to_select}`. The gameplay entries are transcribed from `corp_command.hpp` rather than authored independently.

**The transport.** An MCP server (BL-278, Io MCP server; `tools/mcp/`) wraps all three legs, exposing its tools over the headless `ProjectIo --serve` mode.

Additionally, `src/world/corp_ai.cpp` implements a deterministic scored-utility policy with priority buckets and predictive spending, emitting `corp_decision` records of the form `{command, winning_score, runner_up, reason}` into a 256-entry ring buffer.

---

## 3. Prior result and why it does not transfer

The author's 2023 dissertation, *Generating Entailment Trees via Iterative Application of Small Text-to-Text Transformers*, tested whether a beam-search-based iterative algorithm over a small model (T5-small) would outperform single-pass generation on multi-hop explanation construction. It found no statistically significant difference at 95% confidence, and the null hypothesis was retained.

The diagnosis was specific and is the load-bearing part for present purposes. **Selection, not inference, was the bottleneck.** Inferences were usually correct whenever their input selection was correct; selection accuracy degraded sharply with tree length, because the number of candidate fact-pairings grows factorially and the model had no oracle for which pairings were admissible. It could only be supervised against a single gold tree.

Four properties of the entailment task caused that failure, and each is inverted in Io:

| Entailment trees (2023) | Io action generation |
|---|---|
| Output is a recursive tree of unbounded depth | Output is a **flat record**, ≤4 meaningful fields |
| Candidate pairings grow factorially | Candidates are bounded and pre-filtered (`top_m_sites = 8`) |
| Admissibility must be **learned** from data | Admissibility is **computable** — `placement_rules::can_place_in_world`, ownership, solvency |
| Correctness = match one gold tree | Correctness = typed rejection codes plus downstream economic outcome |

The failure mode that defeated beam entailment is therefore structurally absent here, and absent for a concrete engineering reason: Io has already implemented, in C++, the legality oracle the dissertation had to approximate statistically. The prior negative result should not be read as evidence against the present proposal. If anything it prescribes the design — *do not ask the model to perform selection blind when selection can be enumerated and supplied.*

This aligns with the field's convergent practice. Grounding generation against an explicit legal-primitive set, with illegal transitions masked, is now standard across LLM game agents [1,2], and chess is used as a canonical testbed precisely because "legal moves are enumerable and both action validity and move quality can be checked deterministically" [3].

---

## 4. Criterion C1 — compressibility

The evidence that a small model suffices is stronger than expected, and comes from three directions.

**The bar is low because it has already been cleared without fine-tuning.** Vox Deorum evaluated a hybrid LLM architecture on Civilization V across 2,327 complete games, using open-weight models (`gpt-oss-120b`, `GLM-4.6`) with a simple prompt and no task-specific training. It reached a mean survival rate of ~97%, statistically tied with the tuned algorithmic baseline, sustaining stateful play over ~375–400 turns [4,5]. Io's stated goal is a "roughly human, beatable, legible" opponent — explicitly not a superhuman one. The fine-tune's task is therefore to bring a *small* model to a bar that a *mid-size open-weight* model cleared untrained, which is a compression problem, not a capability problem.

**Distillation to that scale is now routine.** Reported results in this class include a 1.7B on-task specialist outperforming a substantially larger frontier model on its target task, and Llama-3.2-3B improving from 0.76% to roughly 52–55% on GSM8K purely from frontier-model traces [6]. Standard practice in 2026 is LoRA/QLoRA on 1K–100K task-specific examples on a single GPU [7].

**Structure-aware distillation closes the remaining gap, and does so specifically for the smallest students.** Structured Agent Distillation segments teacher trajectories into `[REASON]` and `[ACT]` spans and applies segment-specific losses rather than uniform token-level imitation. Gains are largest at small scale (+4.5% task success at 1.3B), and the authors' explanation is directly pertinent: token-level imitation alone fails to preserve the structure of reasoning–action workflows [8]. This is precisely the reasoning-versus-action distinction at issue here, and its finding is that the reasoning should be *supervised separately*, not discarded.

Io holds two assets that improve on the standard recipe. First, the validator is free and exact: `corp_command_result`'s typed rejections mechanically identify any trace containing an illegal command, which is the filter step the distillation recipe calls for. Second — and under-exploited in the current plan — `corp_ai.cpp` is already a deterministic teacher. Each `corp_decision` record pairs a command with its winning score, runner-up score, and reason code. That is an SFT record in all but serialisation, obtainable at simulation speed with no cloud spend.

**Assessment: C1 is likely satisfied at the 3–8B scale.** A caveat applies at 1–2B and is addressed in §6.

---

## 5. Criterion C2 — tractability on local hardware

This can be computed rather than estimated, because Io's timing constants are explicit.

From `src/core/sim_loop.hpp`: `econ_tick_days = 90` and `seconds_per_day_1x = 2.0`, with a speed multiplier table of `{0.25, 0.5, 1, 4, 16}`. One economic quarter therefore costs `90 × 2.0 / multiplier` real seconds:

| Speed tier | Multiplier | Real seconds per econ tick |
|---|---|---|
| I | 0.25× | 720 |
| II | 0.5× | 360 |
| III | 1× | 180 |
| IV | 4× | 45 |
| V | 16× | 11.25 |

From `corp_ai.hpp`, strategic evaluation is staggered: `cadence_k = 4`, so each corporation evaluates once every four econ ticks at offset `index(c) % 4`. With *N* rival corporations spread evenly, approximately *N*/4 decisions are required per econ tick, giving a per-decision budget of `4 × quarter_seconds / N`.

For *N* = 8 rivals:

| Speed tier | Per-decision budget |
|---|---|
| III (1×) | ~90 s |
| IV (4×) | ~22 s |
| V (16×) | ~5.6 s |

Against this, measured local inference throughput for 8B-class models at Q4 quantisation on consumer GPUs is 42 tok/s (RTX 3060), 52 tok/s (RTX 4070) and 104 tok/s (RTX 4090); Q4 quantisation yields a 3.5–4.0× speedup over FP16 for 1–3% perplexity degradation [9]. Decode is memory-bandwidth-bound; prefill is compute-bound and comparatively cheap, which matters because Io's blackboard is compact and its dictionary slice is stable across calls and therefore cacheable.

A decision comprising a short rationale plus an action chunk of up to four commands is on the order of 300 output tokens, i.e. roughly 3 s on a 4090, 6 s on a 4070, 7 s on a 3060. Every tier through IV clears comfortably on all three; tier V clears on a 4090 and is marginal below it.

**Two observations make this budget less binding than it appears.**

First, the planner is out-of-process by design and the deterministic scorer runs every tick regardless. A late language decision therefore does not block the simulation — the scorer simply acts that tick. Latency does not gate correctness; it gates only *how stale the macro layer is permitted to be*. This is a materially weaker requirement than a hard per-tick deadline.

Second, the comparable published system tolerates far worse. Vox Deorum reports approximately one minute per decision for `gpt-oss-120b` and completes 400-turn games regardless [4].

**Assessment: C2 is satisfied with margin at ordinary play speeds, and gracefully degrades rather than failing at maximum speed.** This conclusion is parametric in *N* (the rival-corporation count), which could not be determined from the generation documentation and should be confirmed; and the target machine's specification was not available to this assessment.

---

## 6. The principal technical risk: the constraint tax

The most decision-relevant finding is a negative one, and it bears directly on the naive form of the proposal.

Imposing hard schema constraints on generation degrades small models substantially. Qwen2.5-1.5B has been measured at 91.5% executable accuracy under prompt-only JSON but **48.0% under a hard tool-call schema** [10]. A parallel study finds that JSON-Schema constraints compiled into grammar-based token masks can render tool-call tokens unreachable during decoding, causing the tool-execution stage to be skipped while format-compliant output continues to be produced [11].

Critically, a third study locates where the damage originates: across six open-weight models, "the dominant cost for format degradation enters at the prompt level when instructions suppress reasoning, rather than at the decoder constraint level," and recent closed-weight models show little to no such tax — indicating a gap specific to open weights [12].

The implication is precise. Constrained decoding is not itself the problem; **suppressing deliberation in order to emit a constrained record is**. A design that asks a small local model to emit a schema-valid `corp_command` in a single undeliberated pass sits exactly on the failure mode. The mitigation proposed in the literature — separating execution from schema-constrained response generation [11] — is, with some irony, the selection/inference decomposition of the 2023 dissertation arriving from the opposite direction.

**Design consequence.** One-shot generation should apply to the *record*, not to the *decision*: a single call emitting a short unconstrained rationale span followed by a constrained action span. This remains one-shot in the operative sense — no multi-turn agent loop, one forward pass per decision — while avoiding the measured degradation. It also matches Structured Agent Distillation's `[REASON]`/`[ACT]` segmentation [8], making the runtime format and the training format the same artifact.

**A second design consequence: chunk the commands.** `corp_ai_params` already caps each evaluation at `max_builds = 1` and `max_dials = 3` — an action chunk of at most four. Robotics has converged independently on emitting a chunk of *H* future actions per forward pass rather than one at a time; the reported benefits are reduced compounding error over long horizons, preserved temporal correlation between successive actions, and amortisation of one expensive forward pass across multiple control steps, with latency improvements up to two orders of magnitude at chunk size 50 [13,14]. Io's per-evaluation budget is already a chunk; emitting it as one is close to free.

---

## 7. The diplomacy question, and why it is separable

The stated motivation for the language route is diplomacy and higher-order strategy. This deserves separate treatment, because it is the strongest argument in its favour *and* because the evidence shows it does not depend on the action generator being a language model.

Cicero — still the reference system for full-press *Diplomacy*, in which negotiation is the game — is built as **two modules with the planner in charge**. A strategic reasoning module, using a planning algorithm regularised toward human-like play by reinforcement learning, predicts opponents' policies from board state and dialogue and selects actions. Those selected actions become *intents*, on which a controllable dialogue model is conditioned to generate free-form language [15,16]. The division of labour is explicit: conditioning dialogue on planner intents enhances strategic alignment "while offloading the responsibility of learning game legality and strategy to other modules" [15].

Two facts about that dialogue model are decisive here. It was **R2C2, a 2.7B-parameter encoder-decoder** [15] — comfortably inside C1 and C2 with room to spare. And it did not choose the moves.

This reframes the decision. The proposition "language play opens the door to diplomatic thinking" is well supported. The proposition "therefore the language model should generate the actions" does not follow, and the best evidence in the negotiation domain runs against it. Supporting results at small scale continue to accumulate: a 7B negotiation model with dynamic personas has been reported to outperform baselines more than ten times its size on negotiation efficiency [17].

Io is already positioned for the Cicero shape. `AI_OPPONENT.md` §7 establishes public and private channels as the diplomacy medium — "diplomacy is legible messaging, not behind-the-scenes flags" — and the corp decision ring already produces exactly the intent stream a conditioned dialogue model would consume. The corps' generated `industrial_focus` supplies persona bias without authoring.

---

## 8. The reinforcement-learning alternative

The fallback is not disqualified, and should not be characterised as second-best on all axes.

Offline RL is the family that fits Io's assets. Deterministic seeds and a headless harness make logged trajectories nearly free, offline RL can extract good policies from suboptimal logged data, and it eliminates the continuous environment interaction that makes online RL sample-inefficient [18]. There is a recent existence proof in a competitive strategy domain: human-level competitive Pokémon play via scalable offline RL over transformers, trained on logged human play [19].

Its costs are equally well documented, and two matter for Io specifically. RL algorithms remain brittle, sample-inefficient and hyperparameter-sensitive outside tabular settings [20]. And — the decisive objection given Io's requirements — a learned policy is **not legible**. `AI_OPPONENT.md` states legibility as a project requirement and identifies it as a reason to prefer utility scoring, "the opposite of AlphaStar's alien play." An offline-RL policy provides no rationale, cannot speak in a chat channel, and cannot negotiate.

Crucially, RL and language are not competing answers to the same question. In Cicero they are the *same system*: RL-regularised planning selects actions, a small language model conducts diplomacy over those actions. Treating C1/C2 failure as a trigger to abandon language and adopt RL therefore commits a category error — the honest alternatives are "language for actions" versus "language for dialogue over an algorithmic core," and RL is a candidate for that core alongside the existing scorer.

---

## 9. Recommendation

**On C1 and C2:** both are satisfied on present evidence — C1 at 3–8B, C2 with margin at ordinary play speeds, degrading gracefully rather than failing at 16×. Neither disqualifying criterion fires. The `N`-corps parameter and the target machine specification should be confirmed to close the calculation.

**On the substantive question, we nevertheless recommend against making the language model the action generator**, for three reasons that are independent of C1 and C2:

1. **The skill argument is weak.** Distilling `corp_ai.cpp` yields, at best, `corp_ai.cpp` — behavioural cloning cannot exceed its demonstrator, as `AI_OPPONENT.md` §Area 3 already notes. The result would be a slower, less deterministic reimplementation of a working component.
2. **The constraint tax is a live risk** at exactly the scale being targeted (§6), and it is avoided rather than mitigated by not putting the model on the critical path for command emission.
3. **The value sought lies elsewhere.** Variety, legible personality, and negotiation are real and are the stated motivation; none of them require the model to choose the moves.

**We recommend instead the Cicero configuration**, staged:

- **Retain** the deterministic scorer (or a future RL policy) as the action generator, with its existing bounded enumeration and typed validation.
- **Add a conditioned dialogue layer** over the `corp_decision` intent stream, targeting the channels already specified in §7. This is where diplomacy lives, it is where the small-model evidence is strongest, and Cicero shows 2.7B is sufficient.
- **Add a goal layer above the scorer** if higher-order strategy is wanted. This is the correct locus: step-wise reasoning is structurally a greedy policy, and the documented minimum remedies — explicit future evaluation, backward value propagation, and limited commitment — belong at the goal horizon, not the per-command one [21]. Per-command generation is the wrong place to buy strategic depth.
- **Bootstrap the corpus from the scorer**, not the cloud. The decision ring is already labelled data; cloud traces should then be spent only where the scorer is demonstrably weak.

If, after this, one-shot action generation is still wanted — for personality variance or as a difficulty tier — it should be built as a *reasoning span followed by a constrained action chunk of up to four commands*, with legal candidates pre-enumerated and supplied in the prompt rather than inferred by the model.

---

## 10. Threats to validity

- The rival-corporation count *N* was not determined from source; §5's per-decision budgets scale as 1/*N*.
- The target machine's specification was unavailable; §5 uses published consumer-GPU benchmarks as a proxy.
- The ~300-token decision estimate is an assumption, not a measurement. It should be validated against a real trace through the existing MCP server before any training commitment — which the MCP server makes possible at essentially zero cost.
- Several sources were reachable only via search summaries and secondary hosts, as `arxiv.org` was blocked by network egress policy during preparation. Primary-source verification is advised before any of the quantitative claims in §4 and §6 are relied upon for a build decision.
- Cicero's dialogue model was trained on a large human corpus of *Diplomacy* messages. Io has no such corpus, and the transfer of the Cicero architecture to a domain without human dialogue data is an assumption, not a demonstrated result. Richelieu's self-play-without-human-data approach is the relevant mitigation and should be assessed separately.

---

## References

[1] Hierarchical Control in Multi-Agent Games — LLM planning with action-knowledge-base masking. https://arxiv.org/abs/2606.20014

[2] XGrammar: Flexible and Efficient Structured Generation Engine for Large Language Models. https://arxiv.org/pdf/2411.15100

[3] VAM: Verbalized Action Masking for Controllable Exploration in RL Post-Training — A Chess Case Study. https://arxiv.org/html/2602.16833

[4] Vox Deorum: A Hybrid LLM Architecture for 4X / Grand Strategy Game AI — Lessons from Civilization V. https://arxiv.org/abs/2512.18564

[5] Vox Deorum — project page and architecture summary. https://civitas-john.github.io/portfolio/vox-deorum/

[6] Distillation in 2026 (so far): which frontier models use it and how. https://huggingface.co/blog/sergiopaniego/distillation-2026

[7] Small Language Models for Agentic AI in 2026: SLM Lineup + Build Guide. https://futureagi.com/blog/small-language-models-agentic-ai-2025/

[8] Structured Agent Distillation for Large Language Model Agents (AAMAS 2026). https://arxiv.org/html/2505.13820v5

[9] Local LLM Tokens-per-Second Benchmarks 2026. https://presenc.ai/research/local-llm-tokens-per-second-benchmarks-2026

[10] The Constraint Tax: Measuring Validity–Correctness Tradeoffs in Structured Outputs for Small Language Models. https://arxiv.org/abs/2605.26128

[11] Constraint Tax in Open-Weight LLMs: An Empirical Study of Tool Calling Suppression Under Structured Output Constraints. https://arxiv.org/html/2606.25605v1

[12] The Format Tax. https://arxiv.org/html/2604.03616

[13] Dissecting Action Chunking with Transformers (ACT). https://blog.phospho.ai/dissecting-action-chunking-with-transformers-act-precision-imitation-learning-for-robotic-manipulation/

[14] How Fast Can I Run My VLA? Demystifying VLA Inference Performance with VLA-Perf. https://arxiv.org/pdf/2602.18397

[15] CICERO: An AI agent that negotiates, persuades, and cooperates with people. https://ai.meta.com/research/cicero/

[16] Cicero — source release. https://github.com/facebookresearch/diplomacy_cicero

[17] EQ-Negotiator: Dynamic Emotional Personas Empower Small Language Models for Edge-Deployable Credit Negotiation. https://arxiv.org/html/2511.03370v1

[18] When Should We Prefer Offline RL Over Behavioral Cloning? https://arxiv.org/abs/2204.05618

[19] Human-Level Competitive Pokémon via Scalable Offline Reinforcement Learning with Transformers. https://arxiv.org/html/2504.04395v2

[20] A Survey of Reinforcement Learning For Economics. https://arxiv.org/abs/2603.08956

[21] Why Reasoning Fails to Plan — step-wise greed, myopic deviation, and long-horizon coherence. https://arxiv.org/abs/2601.22311

### Primary sources internal to Project Io

- `src/world/corp_command.hpp` — verb set, command record, rejection enum, decision ring
- `src/world/corp_ai.hpp` / `.cpp` — scorer parameters, priority buckets, blackboard export
- `src/core/sim_loop.hpp` / `.cpp` — calendar constants and speed-multiplier curve
- `docs/ai/AI_OPPONENT.md` — architecture, staged path, prior research sweeps
- `docs/ai/ACTIONS.json` — the action dictionary

### Prior work

- B. Booth (2023), *Generating Entailment Trees via Iterative Application of Small Text-to-Text Transformers*, BSc dissertation, University of Bath. Implementation and notes in the separate `Entailment-Trees` repository (`Beam_Entailment.ipynb`). § 3 draws on its § 5.3 and § 7.
