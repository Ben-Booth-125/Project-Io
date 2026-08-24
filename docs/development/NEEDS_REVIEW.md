# Project Io — Needs Review

**Ben's review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json),
which is canonical — the JSON wins on any disagreement.

> **Generated file.** Produced by `node tools/session/render_needs_review.js`.
> Edit the JSON, then re-run; hand edits here are overwritten.

Things here are waiting on **your judgement**, not on work. Three kinds:

| Kind | Meaning |
|---|---|
| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |
| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |
| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |

**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list —
items blocked on a visual artifact only you can produce; work there cannot proceed at all.
[`backlog.json`](backlog.json) is *work*. Entries here are neither: they are questions and
reversible calls. If an answer creates work, file a backlog item and resolve the entry with
that item's id.

This queue is **transient**: resolved entries are pruned promptly rather than kept for
posterity — the reasoning lands in code, an authority doc, or a backlog item at the moment
the work happens, and that is the durable record. What stays here is what is still open.

*22 entries — 21 open, 1 resolved.*

---

## Open

### NR-164 — Re-stress generation and time-lapse feel when playable
*question · raised 2026-08-11 · from 2026-08-11 notes session with Ben.*

Ben wants to re-stress-test world generation and the staged-generation time-lapse once the game is playable, to judge whether it feels alive rather than just correct. RULING 2026-08-13 (Ben, elicitation form): keep open until playable - not scheduled, not folded into another item.

*Files: `docs/ui/STARTUP.md`*

### NR-532 — The design register is live — 41 open calls across ten sections, as a form
*observation · raised 2026-08-22 · from Ben, 2026-08-22: "now please revisit each one and open forms for answering the open questions."*

Published at https://claude.ai/code/artifact/debe7b8f-7315-429a-a805-0e295e9405bc. Every open question across the eight new authority docs plus SYSTEMS.md § The progression chain plus four cross-cutting calls, gathered into one form: 41 questions in 10 sections. Each carries its evidence, 3-5 options with one marked as suggested, and a free-text field that overrides the options. Progress is tracked per section; "Copy all answers" puts everything on the clipboard in one block. It is a LIVE DOC - radios and contenteditable fields are captured as edits, so answers reach a watching session directly; deliberately no <textarea> and no <select>, neither of which is captured. Answers also persist to localStorage as a per-viewer draft.

The generator is committed rather than being a one-off (CLAUDE.md § Tool creation is skill creation): tools/session/register/questions.js is the canonical question set and build.js emits the HTML. Verified to regenerate byte-identically. Republish to the same URL to keep answers in place.

**Why it matters.** The open questions were the point of writing the docs as capture rather than design, and they were spread across ten files. A form is the difference between 41 questions that get answered and 41 that get skimmed. It also means the answers arrive in one structured block that can be propagated in a single pass, the way the six nations rulings were.

> **Recommendation:** Answer in any order. The four that change the most downstream: LOGISTICS Q1 (what generates LP), EVENTS Q4 (drive the collapse metagame or express it - it decides the system's size), PEOPLE Q2 (one bias or several - cheapest to overturn now), and NATIONS Q1 (whether the grant reaches a rival, which blocks the player-facing halves of two items).

*Files: `tools/session/register/questions.js`, `tools/session/register/build.js`*

### NR-577 — BL-572 concurrent-offer escrow split — a tick's contracted_force share fills open offers oldest-first, not a rule Ben stated
*decision taken on your behalf · raised 2026-08-23 · from BL-571/BL-572 design ratification, elicitation form (Ben, 2026-08-23).*

Ben's elicitation answer chose 'several open concurrently' over the proposed one-offer-at-a-time cadence for BL-572, but the form did not ask how one tick's `contracted_force` spendable share is allocated when more than one offer is open and filling at once. Taken on his behalf: oldest-issued offer fills first, exhausting the tick's share before a younger offer sees any of it, rather than splitting the share proportionally across open offers. Ratified into CONTRACTS.md § Where offers come from and BL-572's design field. (Renumbered from NR-576 on integrating a concurrent session's push, which had already used that id for an unrelated review-queue-purge entry now archived.)

**Why it matters.** The split rule changes how fast a province under sustained multi-front threat gets its offers funded — oldest-first means a nation's oldest want is always serviced before a newer, possibly more urgent one; a proportional split would fund all fronts slower but evenly. Either is deterministic and legal; this is a pacing/feel call, not a correctness one.

- Confirm oldest-issued-first.
- Switch to a proportional split across all open offers each tick.
- Switch to highest-value-first (by the offer's scored fee).

> **Recommendation:** Confirm oldest-issued-first — it is the simplest deterministic rule, matches a FIFO reading of 'the nation gets to what it can afford', and needs no additional per-offer priority field.

### NR-578 — BL-575 live-click pass confirmed March is reachable and dispatches, but did not confirm the unit visibly moving
*observation · raised 2026-08-23 · from Main-session live verification (computer-use) of BL-575 before closing Wave 1.*

Opened the built app, selected the starting unit, pressed March, and clicked a confirmed-different province (verified via the repeat-click cycle showing its own province id). The mode disarmed without falling through to normal building/tile selection — the Selection panel stayed on Unit — which is strong evidence the corp_command dispatched rather than the click landing on ordinary marker resolution, and corp_command.cpp's march_unit case reads correct on inspection. What was NOT achieved: relocating the marker after the order to watch its tile position actually change. Two compounding factors: the economy/march tick in this build is QUARTERLY-cadence (the header counts down 'd to Q2/Q3'), not daily, despite the calendar advancing daily, so only one quarter boundary elapsed during the pass; and re-finding one unit marker on a freshly generated, unfamiliar world via manual pan/zoom proved impractical within reasonable effort. Halt and Disband were not individually live-clicked (same code pattern as March, confirmed by reading, not by pressing). (Renumbered from NR-577 on integrating a concurrent session's push.)

**Why it matters.** This satisfies the standing rule's core concern — BL-449 shipped clean on both compile and harness while a press rendered unreachable, and this pass proves March IS reachable and does dispatch. But it stops short of the full 'watch it work' bar the rule aspires to, and per the rule's own reasoning (a scripted capture is not enough), neither is a click that merely disarms without a rejection message.

> **Recommendation:** Optional: a follow-up live session with more sim-time (past a quarter boundary, unit pre-located before issuing the march) to directly watch a unit's marker move. Not a blocker for Wave 1 close-out given the code-path and headless evidence.

### NR-579 — scripts/contracts.lua's fee_mult and deadline_ticks (take 1.0/180, hold 1.25/90) are legible placeholders, not measured
*decision taken on your behalf · raised 2026-08-23 · from BL-570 (CONDITION_PROVINCE_SUBJECT), authoring scripts/contracts.lua's first two template rows.*

BL-570's task is the vocabulary + template table shape, not a priced contract -- nothing reads fee_mult or deadline_ticks yet (BL-572 derives the offer/fee, BL-573 builds the live contract), so there was nothing to measure against. Authored round, legible defaults instead, matching economy.lua's own stated discipline for a first cut: take = 1.0x fee, 180-tick deadline; hold = 1.25x fee (a premium for standing watch), 90-tick deadline (a defensive window is naturally shorter than an offensive one). Both numbers are a guess at the right SHAPE (hold costs more, hold is shorter), not a derived magnitude.

**Why it matters.** BL-572 (contract offers) is the first item that actually prices a fee off these multipliers against a nation's contracted_force budget line, and BL-573 (contract record) is the first to run out a real deadline. If either lands without revisiting these two rows, the mercenary vertical slice's first offers will be priced/paced on an unexamined guess rather than a playtested number.

> **Recommendation:** Re-derive or explicitly re-affirm fee_mult and deadline_ticks once BL-572 can measure them against a real nation treasury and BL-573 against a real contract's pacing -- same discipline CONTRACTS.md and economy.lua already apply elsewhere (iterate by playtest, not by authoring once and forgetting).

*Files: `scripts/contracts.lua`*

### NR-580 — Zero nation treasury at generation now flattens BOTH garrison sizing (BL-571) and will flatten contract-offer funding (BL-572) — the same gap seen from a second system
*observation · raised 2026-08-23 · from BL-571 (nation garrisons) landing; the treasury-scaling design called for it to differentiate garrison size by nation wealth.*

nation_component::treasury is 0.0 for every nation at generation, by existing settled design (NATIONS.md: 'zero at generation, deliberately'). BL-571's garrison sizing (nation_garrison_params: min_count=20, count_per_credit=0.05, max_count=200) was meant to scale with treasury, but since every nation starts at 0, every garrison lands on the 20-count floor with no differentiation until something credits a treasury (a levy or tariff enacted) before generation's garrison-seeding pass runs -- which nothing in the generation chain does today. This is the SAME gap the retired NR-572 named from the contract-offer side (a nation's exploration/contracted_force budget share needs a non-zero treasury to fund anything): two independent Sprint 16 systems (garrison sizing now, contract-offer funding next in BL-572) are both about to hit the same wall from opposite directions.

**Why it matters.** BL-572 (contract offers, wave 3 of this batch) derives an offer's escrow from a nation's contracted_force budget share -- if treasuries stay at 0 through generation, BL-572 will build correctly and still never produce a live offer in a default world, the same 'wired but not playing' gap NR-572 (now retired) already diagnosed. Worth deciding before BL-578 (the slice playthrough) hits it live, not after.

- A) Wire nation income before generation's garrison/budget passes run -- enact a starting levy or tariff during generation (not just the campaign tick), so treasury is non-zero from tick 0.
- B) Accept for Sprint 16: garrisons stay floor-sized and contract offers stay rare/small until the FIRST in-campaign levy tick credits a treasury: funding arrives, just not instantly at generation. File nation-income-at-generation as separate, later work.
- C) Re-scale BL-571's garrison-size anchor off something already non-zero at generation (e.g. resource_abundance or tile count) instead of treasury, decoupling garrison size from the funding question entirely.

> **Recommendation:** B for Sprint 16 -- the mercenary slice's playthrough (BL-578) plays over enough ticks that a levy has time to credit treasuries before the slice needs a contract offer to fire; re-verify that assumption when BL-572/BL-578 land rather than gold-plating generation now. Revisit A as its own item if the playthrough shows the wait is too long to feel alive.

### NR-581 — BL-572's offer fee/deadline are hardcoded, not read from the BL-570 template table — deliberately deferred to BL-573; plus one tested same-tick expire-reopen interaction
*observation · raised 2026-08-23 · from BL-572 (CONTRACT_OFFERS) landing; contract_offer_params (nation_step.hpp).*

Two things worth a look, neither blocking: (1) `contract_offer_params` hardcodes fee=400/deadline_ticks=180/template_index=0 rather than reading scripts/contracts.lua's authored 'take' row through `contract_template_registry`, because that registry needs sol2/Lua and its loader is excluded from the Lua-free world/* superset derive_contract_offers lives in (the same reason recipe_registry is threaded in as a plain parameter rather than loaded by world/* itself). The values match the authored row today, and BL-573 (accept_offer) is the natural point to thread a real contract_template_registry through — the same way recipe_registry already reaches world/* — since accepting an offer needs the template's actual predicate anyway. (2) Tested and asserted (nation_scorer_harness R8e2): because escrow refund runs before the funding gate is re-read in the same tick, an offer that expires while its nation still has budget and an eligible target reopens the SAME province in the SAME tick, funded by its own just-refunded escrow. Deterministic, conserves money exactly, not a bug — but a dispatch feed (BL-577) that later announces 'offer expired' then 'offer issued' in the same message batch would read oddly for something that never actually stopped being available.

**Why it matters.** (1) is pure sequencing information for whoever builds BL-573 — worth a one-line pointer in REFINED.md so it isn't rediscovered. (2) only matters once BL-577 (contract messages) exists to surface it; flagging now so it isn't a surprise mid-implementation.

> **Recommendation:** No action needed now. When BL-573 lands, thread contract_template_registry through derive_contract_offers's call chain the way recipe_registry already is. When BL-577 lands, consider collapsing an expire+reopen on the same province in the same tick into one 'renewed' message rather than two.

### NR-582 — contract_failed's sentiment magnitude (-4.0 Trust) is authored double contract_cancelled's (-2.0), a ratio not derived from CONTRACTS.md
*decision taken on your behalf · raised 2026-08-23 · from BL-573 (CONTRACT_RECORD_AND_VERBS), authoring sentiment_factor_kind::contract_failed in scripts/economy.lua.*

CONTRACTS.md's Q2 table settles the ORDERING — failed is 'down hardest', cancelled/abandoned is 'down, but less' — but names no numbers. contract_failed was authored at -4.0 Trust, exactly double contract_cancelled's existing -2.0, a legible round ratio rather than a measured or playtested one.

**Why it matters.** This is the first real magnitude on the mercenary loop's downside; BL-578's playthrough is the first point anyone will actually feel whether a failed contract stings twice as much as walking away, or whether that ratio reads as arbitrary. Cheap to retune (one Lua constant) if it doesn't.

- Confirm 2x.
- Pick a different ratio.
- Defer to playtest feel once BL-578's slice is playable.

> **Recommendation:** Defer to playtest feel — this is exactly the kind of number CONTRACTS.md itself says should be iterated, not authored once and forgotten (the same discipline NR-579's fee_mult/deadline_ticks placeholders are held to).

### NR-584 — BL-574's own requirement text says a failed 'hold' contract has 'the escrow returned' -- the code does not return one (renumbered from NR-583 on merge, collision with an unrelated entry already on main)
*observation · raised 2026-08-24 · from BL-574 (CONTRACT_HARNESS), req/requirements.json's R1 (M4): "a hold-template failed on the first lost tick with the escrow returned and reputation lower than a cancel".*

Read literally against src/world/nation_step.cpp's run_mercenary_contract_tick, this is not what happens on a mercenary_contract's failure. A mercenary_contract carries no escrow field at all -- only fee/deposit_paid (world.hpp) -- and the failure branch's own comment says outright: 'the reserved remainder is not disbursed to anyone; it was already spent, from the client's perspective, the moment the escrow that funded it left the treasury' (corp_command.cpp's accept_offer draws the same conclusion for why the deposit is a direct transfer, not a fresh budget claim). The only 'escrow returned' mechanic anywhere in the mercenary-contract code is on a still-open mercenary_offer's TTL expiry, BEFORE acceptance (nation_scorer_harness.cpp's own R8e) -- a different record, a different event, already covered there. tools/verify/mercenary_contract_harness.cpp's M4 case (BL-574) asserts the ACTUAL observed behaviour instead -- nothing beyond the already-paid deposit moves on a hold-contract's failure -- rather than the requirement's literal wording, and says so in the harness's own file-header comment.

**Why it matters.** Either the requirement text is loose paraphrase of a mechanic that was never built this way (most likely -- carried over from offer-TTL-expiry's refund, or procurement's deposit-forfeit shape), or a real refund-on-failure was intended and BL-573 never implemented it. This item's own scope is headless verification only, so it observes and reports rather than silently reinterpreting the requirement or adding an unasked refund path.

- Confirm the requirement text is loose paraphrase, not a literal spec -- the observed behaviour (deposit only, no refund) is correct, and matches CONTRACTS.md's own Q2 table ('failed: deposit forfeit, nothing paid').
- Decide a mercenary_contract's failure SHOULD refund something to the client nation's treasury (a real behaviour change to nation_step.cpp, not a harness fix), and file it as its own backlog item.

> **Recommendation:** The first option -- CONTRACTS.md's own Q2 table for 'failed' already reads 'deposit forfeit, nothing paid', matching the code and not the requirement text's 'escrow returned'. Correcting requirements.json's imprecise wording in the same change that closes this entry.

### NR-585 — mercenary_contract::abandoned_event_posted is deliberately unserialized -- a saved-then-reloaded abandoned contract re-announces itself once
*decision taken on your behalf · raised 2026-08-24 · from BL-577 (CONTRACT_MESSAGES_AND_INCOME), the dispatch one-shot flag for the abandoned event.*

accept_offer/abandon_contract run outside any report-producing tick pass, so 'accepted' reuses the existing accepted_tick field for free, but 'abandoned' needed a new marker to avoid re-announcing on every subsequent tick. Rather than a real persistent field (a world_save_version bump), abandoned_event_posted was added as a bool that is NOT written to the save stream -- 'derived convenience, not authoritative', the same framing world::current_day_tick already carries in world_save.hpp's bucket-2 list. The cost: a contract already abandoned before a save re-announces itself once on the first tick after that save reloads -- one duplicate chat line, never a duplicate payment or a determinism divergence (the flag gates only the dispatch-text call, not the state transition itself).

**Why it matters.** This is a genuine, if minor, exception to 'the serialisation seam travels with the change' -- worth a second look given how load-bearing that rule is everywhere else in src/world/. The field lives inside an otherwise-fully-serialized struct (mercenary_contract, world_save.cpp's P12 round-trip), so it is not obviously in the same bucket as current_day_tick (which world.cpp fully rebuilds from other state -- this one cannot be rebuilt, it is just accepted as a one-time repeat).

- Confirm the trade-off -- a save-version bump for one cosmetic bool is not worth it.
- Serialize it properly (bump world_save_version to 9) if a duplicate chat line after a reload is judged worse than it sounds.

> **Recommendation:** Confirm the trade-off. A duplicate 'contract abandoned' line after loading an old save is a one-time, low-stakes cosmetic repeat, and the bar for a save-version bump should stay high -- every prior bump this batch (v4 through v8) was for state that actually needs to round-trip correctly for the SIMULATION to be right, not for a UI notification flag.

### NR-586 — The Contracts ledger took nav-rail slot 13 -- the curated nine plus the 10-12 developer tail were both full
*decision taken on your behalf · raised 2026-08-24 · from BL-576 (CONTRACTS_LEDGER), placing the ledger's rail icon.*

The nav rail's existing slots (a curated nine, plus a developer tail at 10-12) had no free slot for the Contracts ledger, so it was appended as a new 13th slot rather than displacing an existing one. Documented as a deliberate exception in docs/ui/MENU.md § Slot 13.

**Why it matters.** This is the first time the rail has run past its originally curated model -- worth Ben's eyes on whether 13 is a one-off (Sprint 16 earns its slot, nothing else does for a while) or the first sign the curated/tail split itself needs revisiting (e.g. a fold-out overflow menu, or re-curating which of the original nine still earn a permanent slot).

- Confirm slot 13 as a one-off addition, no structural change.
- Revisit the rail's slot model now, before a 14th item makes the question harder to answer cleanly.

> **Recommendation:** Confirm as a one-off for now -- Sprint 16 is the first Batch Delivery to reach the UI layer this deep, and one slot added under real pressure is better evidence than redesigning the rail speculatively. Revisit if a near-term item needs slot 14.

### NR-587 — Every real battle tried this session opened AND concluded inside one econ_step call -- an in-progress battle card may be unobservable by construction
*observation · raised 2026-08-24 · from BL-578 (MERCENARY_SLICE_PLAYTHROUGH), building scripts/verify/mercenary_slice.lua's six-capture playthrough.*

Capture 4 ("the battle card") was meant to reuse battle_card.lua's own proven declare_hostile+march+step-until-select_battle-succeeds technique. It did not work -- not for the contract's own corp-vs-nation garrison fight, and not for a fresh, evenly-matched corp-vs-corp fight built from scratch with the exact same technique battle_card.lua itself uses. Re-running battle_card.lua UNMODIFIED against the current build also fails ("no battle opened within 120 ticks of marching into contact"), so this is not specific to this item's own setup. Root-caused with temporary debug prints (reverted, not committed): run_battles' discovery correctly finds the hostile/contracted pair co-located, opens an active_battle, and campaign_battle_params::rounds_per_tick (3) is enough for BOTH matchups tried to reach a terminal state (attacker/defender broken) inside that SAME call -- so the battle is created and erased within one economy tick, every time, and `verify.select_battle` -- which can only observe world state BETWEEN whole ticks -- never once returns true. The fight still happens for real (Field-channel dispatch text, real casualties, real province_holder flips); only the MODAL CARD's own live-open window is what could not be reached.

**Why it matters.** Two readings, and I could not settle which from this session's evidence alone. (1) This could be entirely expected/seed-dependent: battle_card.lua's own historical pass may simply have caught a closer, slower-resolving matchup by luck, and any two heavily- or evenly-matched sides in THIS build's resolver just happen to decide within <=3 rounds most of the time. If so, nothing is broken, but a `visual` requirement resting on select_battle succeeding is fragile by construction and worth a documented caveat. (2) This could be a real pacing regression -- rounds_per_tick, or something in the swing-draw tuning, changed to resolve faster than when battle_card.lua last passed -- in which case the ORIGINAL BL-469 requirement ("open the app, click the battle, see it") is currently unmeetable for most real fights, which is a live-verification gap on a previously-verified surface, not a new one.

- Treat as expected/low-priority: note the fragility in battle_card.lua and mercenary_slice.lua's own comments (already done for the latter) and move on.
- Investigate rounds_per_tick / swing-draw tuning history to confirm or rule out a regression against BL-469's original passing runs.
- Add a verify hook that can hand-seed an active_battle mid-fight for capture purposes (the M3 harness idiom in mercenary_contract_harness.cpp), so a capture no longer depends on catching a live, fast-resolving fight.

> **Recommendation:** Investigate before the next combat-adjacent item lands (option 2) -- battle_card.lua going from a passing, committed check to a failing one without anyone noticing is exactly the kind of silent drift the project's verify suite exists to prevent. Low urgency otherwise: the underlying combat and its consequences are unaffected, only the modal card's OWN live-open capture is blocked.

*Files: `scripts/verify/battle_card.lua`, `scripts/verify/mercenary_slice.lua`, `src/world/battle_system.cpp`*

### NR-588 — A real mercenary offer's deadline takes ~10 real hours to reach at max game speed -- no live human session can wait out a payout today
*decision taken on your behalf · raised 2026-08-24 · from BL-578 (MERCENARY_SLICE_PLAYTHROUGH) R2, the live human playthrough.*

Accepted a REAL, nation-issued offer (Zeithketh, province #21928, fee 400cr) live through the Contracts ledger's force picker -- offer, accept, and march all confirmed reachable and correct by mouse. Its deadline read 161 econ ticks. Measured directly: at the game's maximum time control (Speed V, 16x, ~11s/quarter nominal), the deadline counter decremented roughly once per 4 real minutes of wall-clock -- 161 ticks would take on the order of 10 hours to reach. run_mercenary_contract_tick (nation_step.cpp) only evaluates a 'take' contract's predicate AT the deadline, not continuously, so there is no way to get paid sooner than that regardless of how fast the province is actually held (it was held within the first couple of ticks). Ben ruled (2026-08-24) that the mechanical proof -- offer/accept/march/active all confirmed live -- satisfies BL-578's R2 in place of literally waiting out the payout.

**Why it matters.** This is a different, sharper problem than NR-579/580's 'these are placeholder numbers' framing: it is not that the deadline is untuned, it is that the CURRENT tuning makes reaching a paid mercenary contract through ordinary play functionally impossible in a single sitting, for every offer, since offer deadlines all sit in the same 160-170 tick band (confirmed across all four Offers visible in the live session). The sprint's own stated goal -- 'a polity hires the company, the company fights, the company is paid, playable end-to-end, however rough' -- is not actually playable end-to-end today outside a script or a very patient multi-session save/reload.

- Shorten deadline_in in contract_template_registry (scripts/economy.lua) to something reachable in minutes at max speed -- a straightforward data change, no code.
- Raise the game's maximum time-acceleration ceiling generally -- a bigger change with effects outside contracts.
- Accept as a known rough edge for this slice (per the item's own 'however rough' framing) and revisit at the next contracts-focused item.

> **Recommendation:** Shorten deadline_in as a small, low-risk follow-up -- it is a single authored value, and the sprint's whole point was proving the loop plays, not that it plays at a punishing pace. Low urgency: nothing here blocks other work, and the mechanics underneath are all sound.

*Files: `scripts/economy.lua`, `src/world/nation_step.cpp`*

### NR-589 — A ruling was taken on a stale doc paragraph — the four "dominated" recipe pairs are false positives, and the real finding is the opposite
*decision taken on your behalf · raised 2026-08-23 · from Sprint 17 authoring session. Ben ruled "delete the dominated sibling" on evidence this session supplied from docs/economy/PRODUCTION.md § Alternate production methods.*

The doc paragraph names four sibling pairs as dominated on both axes and calls it Ben's open call (NR-243). It is stale. recipe_switch_harness.cpp's header records that its R1 grouping was RETRACTED on 2026-08-16 and moved to chain_depth.cpp's R2 row, which buckets every same-output sibling pair as (a) a supply route with disjoint raws, (b) a named precondition pair, or (c) a genuine interchangeable method — and only compares (c) on price. All four pairs fall in (a) or (b). Acting on the ruling would delete the coal Smelter, the Charcoal Burner, the Potter & Weaver and the airless propellant route, and would strand sand, peat and iron_nickel_ore as orphans, failing chain_depth's own R1 in the same pass. Not acted on. What the replacement guard actually shows is sharper and is now BL-587 (interchangeable methods exist): bucket (c) is EMPTY, so the 2026-08-15 alternate-methods ruling is mechanism with no content behind it.

**Why it matters.** Two things. The ruling stands unfulfilled unless Ben re-rules, and he ruled on a premise this session gave him — so the correction is owed to him directly, not buried in an item. And PRODUCTION.md is stating as an open question something settled seven days earlier, which is exactly the doc-truth failure the 2026-08-23 state-independence sweep exists to catch; the sweep's own holes audit even lists this line as a live hole pointing at an open item.

> **Recommendation:** Take BL-587 (author the missing methods) instead of a deletion, and let it correct PRODUCTION.md § Alternate production methods in the same pass. Re-rule the deletion only if a specific pair is wanted gone for a reason the guard does not model.

*Files: `docs/economy/PRODUCTION.md`, `tools/verify/chain_depth.cpp`, `tools/verify/recipe_switch_harness.cpp`*

### NR-590 — Sprint 17 authored: ten items, six rulings, and four calls taken inside them
*decision taken on your behalf · raised 2026-08-23 · from Ben, 2026-08-23: "Let's get sprint 17 started too ... author items in backlog.js" plus the six-question elicitation form answered the same day.*

BL-585..BL-594 filed for Sprint 17 (v0.1.17), authored against the code rather than against the archived designs. Four calls taken without asking: (1) the sprint keeps the economy-breadth THEME but none of its original items — three landed, three were absorbed by BL-434/BL-460/BL-436 and the recipe-switch work, so re-filing them would have re-litigated finished work; (2) the "new goods" ruling is split into its own first item (BL-585) so the enum append and save bump happen once and every later item stays in Lua; (3) Ben's progression steer is implemented as a THIRD ARM on tech_effect (unlock_recipe) rather than as a new lock kind — the union already has two arms and one authored gate, and a recipe lock is the arm it is missing, not a new system; (4) the guard rows go into chain_depth rather than a new harness, because R1 was moved OUT of recipe_switch_harness in 2026-08-16 precisely to stop two harnesses answering one question with two reference-price tables.

**Why it matters.** The sprint spends a save-format bump and opens the tech-effect union, both of which are cheap now and expensive later. If any of the four calls is wrong, it is cheapest to overturn before BL-585 lands.

> **Recommendation:** No action needed unless one of the four reads wrong. BL-585 is the item to hold if the enum append should wait.

*Files: `docs/development/backlog.json`, `docs/development/sprints.json`*

### NR-591 — The tech tree is ~150 nodes of inert data with exactly one gate that resolves, and Sprint 17 is the first work to lean on it
*observation · raised 2026-08-23 · from Measured while authoring BL-588 (unlock_recipe tech arm).*

scripts/tech_tree.lua opens with "DATA ONLY — the tech system is post-prototype; nothing in the simulation reads this", and prototype_tech_gates() in tech_gate.cpp returns a single gate, E0-ML-01, which unlocks the military base. The tech_effect union has two arms: unlock a building_type, or move a scalar. So the only lock the economy earns today is depth_locked, and it applies only to recipes tagged era = "ancient". NR-490 already observed the same shape from the progression side ("one earnable gate in 150 tech nodes"). RESOLVED IN BL-588 (2026-08-24): the gate table names its own fresh ids — E0-EC-01 (Toolmaker) and E1-EC-01 (Bessemer Converter) — neither transcribed from nor reconciled against tech_tree.lua's existing sketch/derived nodes. The second option this entry named, taken rather than the first.

**Why it matters.** BL-588 authors gates for the first time as content rather than as a proof. That is the moment the tree stops being a picture of a system, and it is worth knowing that the node list in tech_tree.lua is still marked status="sketch" for Era 1 and status="derived" for Era -1 — neither is a ratified content set. Authoring gates against ids in that file binds real behaviour to nodes Ben has explicitly not reviewed.

> **Recommendation:** Either author BL-588's first gates against a small, deliberately-chosen set of Era 0 node ids, or let the gate table name its own ids and reconcile with tech_tree.lua later. State which at promotion; do not let the Lua node list become authoritative by accident.

*Files: `scripts/tech_tree.lua`, `src/world/tech_gate.cpp`, `src/world/tech_gate.hpp`*

### NR-592 — corp_ai.cpp never prices resource_build_cost when scoring a build candidate — pre-existing, not caused by BL-590
*observation · raised 2026-08-24 · from Measured while authoring BL-590 (per-building materials): the build-candidate loops price only building_economics::build_cost (ex.build_cost / pe.build_cost / mex.build_cost), never resource_build_cost.*

BL-590 gave named buildings materially different resource_build_cost baskets (ancient buildings now cost timber/stone, not steel). The AI scorer's capex estimate never priced that array before this item and still does not after it — a candidate scores on cash alone, so a rival can propose a build whose MATERIALS it cannot actually reach even with sufficient cash. construct_building's own affordability gate refuses it cleanly at apply time (no mutation), so this is a missed-opportunity gap for the scorer, not a correctness bug: the seam already enforces the real cost, same shape as the recipe-switch scorer's own documented gap (NR-242, PRODUCTION.md § Alternate production methods).

**Why it matters.** Was already true before BL-590 (every type shared one steel basket, so the gap was invisible — steel is cheap and plentiful for most rivals). BL-590 makes it visible for the first time: an ancient rival with no timber stockpile could now score a Sawmill it cannot actually place. Not urgent (the gate protects correctness), but worth knowing before BL-589's start-gate audit or BL-594's playthrough, in case a rival's build thrash traces back to this rather than to something either of those items would otherwise suspect.

> **Recommendation:** Leave as documented debt unless BL-589/BL-594 measure it costing something real (excess refused-build churn, a rival stalling on a candidate it can never place). If so, pricing resource_build_cost_for into the scorer's capex estimate is a small, contained addition — the same shape corp_ai.cpp already applies to build_cost.

*Files: `src/world/corp_ai.cpp`, `docs/economy/PRODUCTION.md`*

### NR-593 — BL-591's growth-track readout is render-confirmed, not click-confirmed — no computer-use access this session
*decision taken on your behalf · raised 2026-08-24 · from Ben, 2026-08-24, asked directly: how to close BL-591 given no computer-use access this session. Answered: accept the render proof, file this entry.*

The standing rule (io-standing-rules.md, "A UI requirement needs a live check") asks for an actual mouse click before marking a visual requirement complete, not just a scripted capture. This session's computer-use MCP disconnected mid-session (unrelated to this item), so no literal click was possible. What WAS confirmed: scripts/verify/corp_dashboard.lua's corp_rollup_production capture, driven by verify.fold("corp_rollup", 0), renders the three growth-track lines correctly with real content (a fresh corp: "Reached depth 0, via Iron Ore / Next: Bloomery / Needs: Charcoal"). This drives the identical rollup_body() code path a real click reaches -- same function, same data -- and the fold/expand CONTROL itself is not new (BL-248, already live-verified in earlier work); BL-591 only added content inside an already-reachable surface. Ben's call: this is close enough to accept, following BL-575's own precedent (shipped complete with a named residual verification gap, not blocked).

**Why it matters.** If a future session finds the growth-track lines are NOT actually visible when a human opens the Production card (e.g. a layout overflow, a z-order issue, a fold-state bug the script's scripted path does not exercise the same way a click does), this is the record explaining why that was not caught now.

> **Recommendation:** A quick live-click spot check the next time computer-use (or equivalent interactive access) is available in a Project Io session -- open the Corporation dashboard, expand Production, confirm the three lines read as shown in the capture.

*Files: `src/ui/corporation_dashboard.cpp`, `scripts/verify/corp_dashboard.lua`*

### NR-594 — tech_gate_harness only exercises E0-ML-01 — the three economy gates (E0-EC-01/02/03) are proven live but not covered by a dedicated T-row
*observation · raised 2026-08-24 · from Measured while authoring BL-589 (start-gate audit): tech_gate_harness.cpp's full T1-T7 suite only names "E0-ML-01" (the original garrison gate).*

BL-588 authored E0-EC-01 (Toolmaker) and E1-EC-01 (Bessemer Converter); BL-589 added E0-EC-03 (refined_copper). None of the three has its own T1-style existence/predicate-isolation/determinism coverage the way E0-ML-01 does — they are proven correct only indirectly: tech_effect_union_harness exercises the unlock_recipe ARM generically, chain_depth's G5 row proves E0-EC-03 specifically resolves under its own predicate, and the full-suite pass (construction_gate_harness, corp_ai_harness, etc.) never regressed. That is real coverage, but it is scattered across three files rather than living in the one harness whose whole job is this table.

**Why it matters.** If a future edit to any of the three gates' predicates breaks its intended isolation (e.g. E0-EC-01's structure condition accidentally also satisfies E1-EC-01), nothing named after this file catches it directly — same failure class E1-EC-01's own surplus-only first draft was caught by (T3's incidental collision, not a dedicated E1-EC-01 test).

> **Recommendation:** A follow-on pass extending tech_gate_harness.cpp with T-rows per economy gate (T8-ish for each), or folding them into chain_depth's roster-breadth guard (BL-592) if that is a better home. Not urgent — no gap has actually bitten yet — but worth doing before a fourth gate makes the collision surface bigger.

*Files: `tools/verify/tech_gate_harness.cpp`, `src/world/tech_gate.cpp`*

### NR-595 — BL-593's tech-lock filter is render-confirmed only partially — the tile build ledger cannot be scrolled into view via existing verify bindings, and computer-use still cannot reach the custom ProjectIo.exe
*decision taken on your behalf · raised 2026-08-24 · from Continuing the same live-click constraint NR-593 recorded for BL-591, now compounded by a second, distinct gap found while trying to verify BL-593.*

Two separate verification limits, not one: (1) computer-use reconnected mid-session but request_access does not resolve a custom .exe outside the Windows Start-Menu app registry ("ProjectIo" returns notInstalled) -- the same gap NR-593 hit. (2) NEW: scripts/verify/build_door_wide_roster.lua (authored this item) selects a tile and opens the Build door, but the ledger window's title is dynamic ("Construct [x, y]"), which does not match any of scroll_panel's five fixed window-name strings (Tile Ledger/Market Ledger/Economy/Balance Ledger/Corporations/Building) -- so the column cannot be scrolled to bring Metal Foundry (where the fix actually applies) into frame. Both captures (build_door_select, build_door_wide_roster) show only Extraction and Infrastructure, cut off before Processing.

**Why it matters.** The code fix itself is low-risk -- it extends an existing, already-verified removal predicate (era_locked/depth_locked) with one more clause (tech_locked), same shape, same call site -- but it has not been SEEN rendering correctly, only reasoned about from source and confirmed not to crash. If a future change to the ledger's layout or the recipe_unlocked call silently breaks this, no visual check catches it until scroll_panel gains a case for the Build ledger's window (or that window is given a name scroll_panel already knows).

> **Recommendation:** Add a scroll_panel case for the tile build ledger's window (likely needs the dynamic "Construct [x, y]" title matched by prefix, or the window renamed to something fixed scroll_panel can target) the next time this surface needs a full-content capture. Separately, a live-click pass the next time computer-use can reach this project's built exe (a Start-Menu shortcut, or an update to how request_access resolves custom binaries) would close both this and NR-593 at once.

*Files: `scripts/verify/build_door_wide_roster.lua`, `src/core/verify_api.cpp`, `src/ui/selection_panel.cpp`*

### NR-597 — The corp-selection screen’s per-row "Choose" buttons do not respond to clicks
*novel-work · raised 2026-08-24 · from BL-594’s live playthrough (two separate game sessions, two different generated worlds) — every individual "Choose" button on the "Choose your corporation" screen was clicked and did nothing, reproduced 2-for-2; only "Surprise me" (the seed-default pick) responded.*

A real, reproducible UI defect on scripts/verify/corp_choice.lua’s live surface (BL-435). Not investigated further — this item’s scope is the ancient-roster playthrough, not this pre-existing screen. Ben’s ruling: not urgent, keep it out of the backlog for now (KNOWN_BUGS.md is retired per CLAUDE.md, so this is the durable record instead of a filed item).

**Why it matters.** A player who wants a SPECIFIC starting corporation (not the seed default) currently cannot choose one — the screen offers 8 rows and only the escape hatch works.

> **Recommendation:** File a proper BL- item and fix when it becomes a priority — likely a click-handler wired to the row highlight but not the button itself, given corp_choice.lua’s own note that the stage is reached via a re-entry path (verify.show_corp_choice) rather than the normal flow.

*Files: `src/ui/*.cpp (corp choice screen, not yet located)`, `scripts/verify/corp_choice.lua`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

### NR-596 — spectator_determinism’s R1 A/B family-coverage check retired — bit-identical RNG-stream determinism is not a property this harness needs to hold
*decision taken on your behalf · raised 2026-08-24 · from BL-586 slice 2 (Tannery/Weaver/Shipwright): widening resource_count 42->47 shifted an RNG stream and broke the "seated+spectated reaches every family it reached as a rival" assertion, unrelated to the new content itself (differentially confirmed: identical failure with recipes.lua/economy.lua reverted to pre-slice-2, enum width unchanged).*

The check compared as_seated (family coverage in a seated+spectated 300-tick rollout) against as_rival (the same corp’s coverage as an ordinary rival) and required as_seated >= as_rival. Put to Ben rather than silently patched (per the standing rule against weakening a failing test) or silently left failing. Ruled: bit-identical RNG-stream determinism is not the property this harness needs to prove beyond what R2/R3 already assert — saves carry the actual state, not a replay-from-seed, and occasional randomness is a deliberate strategy lever, not a defect. The check is retired (commented out with the ruling and full provenance trail, not deleted) rather than weakened to pass; the two properties io-standing-rules.md’s BL-409 section actually cites by name (defaults false, no rival’s cadence slot shifts) are untouched and still pass.

**Why it matters.** io-standing-rules.md’s Determinism & data model section cites spectator_determinism.cpp by name as the harness proving the BL-409 spectator grant’s two load-bearing properties. This ruling narrows what that citation is understood to guarantee — worth a standing-rules note so a future reader does not assume EVERY assertion in that file is load-bearing.

> **Recommendation:** Add a one-line clarifying note to io-standing-rules.md’s BL-409 paragraph: the harness proves the two named properties (default-false, no cadence-slot shift), not RNG-stream-identical behavioural outcomes across a content change — that expectation was explicitly ruled out 2026-08-24.

> **RESOLVED.** Note added to io-standing-rules.md’s BL-409 paragraph (2026-08-24): the harness guarantees the two named properties (defaults false, no cadence-slot shift), not RNG-stream-identical behaviour across a content change. The retired check itself stays retired in spectator_determinism.cpp with its own provenance comment.

*Files: `tools/verify/spectator_determinism.cpp`, `.claude/rules/io-standing-rules.md`*

