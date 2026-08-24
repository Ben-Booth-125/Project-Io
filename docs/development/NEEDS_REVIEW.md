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

*25 entries — 25 open, 0 resolved.*

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

### NR-589 — Do we still want versioned releases? Item-level versioning is already gone
*question · raised 2026-08-24 · from Ben, 2026-08-24, opening the ROADMAP trim: 'I don't think we need versioned releases right now.'*

Raised rather than acted on, because dropping the version ladder is a bigger change than the trim he asked for. The observation stands on its own: item-level versioning ALREADY ended on 2026-08-23, when the backlog purge left the hot file holding one sprint of items with no version field at all - so nothing queryable maps a minor to its work any more, and the roadmap was the last place the mapping lived (in prose). Tags v0.0.4..v0.1.15 are real and stay; the themes are live and stay. What is in doubt is the NUMBERING and the per-minor cut ceremony around it. Three readings, all defensible: (a) keep the ladder, cut tags as before; (b) keep themes, drop numbers, and let a sprint close be the only unit of completion - which is what practice already does, since the nation/province/watch lanes landed real work under no tag; (c) keep numbering only for the eventual commercial cut of the ancient product. The trimmed ROADMAP.md holds the question in a banner and behaves as (b) in the meantime - themes named, sequencing delegated to SPRINTS.md.

**Why it matters.** The done-definition-at-the-cut rule (NR-103) is attached to the version ladder. If minors stop being cut, that rule needs a new anchor or the failure it prevents - a theme with no test for finished absorbing items indefinitely - comes back. NR-102 (sequencing decoupling) is the same hazard from the other side.

*Files: `docs/development/ROADMAP.md`, `docs/development/SPRINTS.md`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-590 — A loaded save does not RENDER as the world it was saved from -- the day-tick mirror is rewound and the canvas comes back dimmed
*decision taken on your behalf · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Measured in both directions. The WORLD half of the save is whole: player_balance comes back bit-identical (1182.7651367188 -> 1182.7651367188). But world::state_hash does NOT round-trip once any econ tick has run (7161C70488A767CA -> 62CAB4412DEDC7E0), while a fresh world with zero ticks DOES round-trip exactly -- which localises it. Cause: verify.econ_step increments m_world.current_day_tick directly and never advances m_sim_loop, so app::save_game_to writes env.day_tick = m_sim_loop.day_tick() (still 0 under --verify) and app::load_game_from then sets m_world.current_day_tick = env.day_tick. The hash is keyed on that tick, hence the divergence. The VISIBLE consequence is the one that matters: the activity fog reads every glimpse as stale, so the loaded canvas is dimmed. ui_fixture_live.png beside ui_fixture_loaded.png is the pair. DECISION TAKEN: shell_pass.lua stages its own world (stage_ui_fixture in lib.lua) rather than loading the snapshot, because reviewing a layout against a degraded picture of it is worse than paying the generate cost. The fix is Ben's call because it is not local: making econ_step drive sim_loop::advance_days would start advancing dates, orbits and surveys in EVERY existing verify capture.

**Why it matters.** Ben's stated sequence for the UI minor was save -> load -> bless goldens. Goldens blessed from a load would pin a fog state the live campaign never shows. It also means save_load.lua's 'state_hash unchanged across the round trip' assertion passes only because that script never ticks the economy.

*Files: `src/core/verify_api.cpp`, `src/core/app.cpp`, `scripts/verify/ui_shell_fixture.lua`, `scripts/verify/save_load.lua`*

### NR-591 — The comms log is not in the save envelope, so the dock comes back empty after any load
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

save_game.hpp's exclusion list justifies dropping 'hover and chat state' as transient view state. That is true of hover and is not true of the LOG: the comms dock is one of the always-on surfaces, and its content is the record of what happened in the campaign, not a view preference. After a load the dock holds only the lines the load itself posts. Two options: put the message vector in the envelope, or derive the dock from history_log on load -- the cheaper answer if the log already carries the same events.

**Why it matters.** A player who saves and reloads loses the campaign's whole narrative surface. It is also the second reason a loaded world does not render as the saved one (NR-590).

*Files: `src/core/save_game.hpp`, `src/core/save_game.cpp`, `docs/ui/CHAT.md`*

### NR-592 — Every date in the UI reads 1960, and three readouts of the same clock disagree
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Three things, one subject. (1) campaign_epoch_year is a hard-coded 1960 in BOTH src/ui/format.hpp:82 and src/world/planetology.hpp:275, tied together by a static_assert, while world_params::epoch_year defaults to 0 -- and no file under src/ui reads epoch_year at all. So a 0 CE campaign renders '1960 Jan 1st [Q1]' in the time panel and '1960 Jan 13th' in the AI decision feed. (2) On one frame the header reads '19q' elapsed, the time panel reads Jan 1st Q1, and the decision feed reads Jan 13th. Part of that spread is the --verify artifact in NR-590, but the epoch itself is live-app behaviour. (3) The launch screen's tagline still reads 'Near-future corporate...', which is the space arc's framing rather than the 0 CE product's.

**Why it matters.** The date strip is the most-read piece of chrome in the game and it currently contradicts the product the roadmap says we are building. It is also the cheapest tell that the 0 CE refocus is unfinished.

*Files: `src/ui/format.hpp`, `src/world/planetology.hpp`, `src/ui/time_panel.cpp`, `src/ui/startup_screens.cpp`, `docs/ui/HEADER.md`, `docs/ui/TIME_CONTROLS.md`*

### NR-593 — The Selection band's resting state is ~90% empty, and its action grid is six near-identical circles in every kind
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Four observations on one surface, all visible in _band_strip.png. (a) The RESTING state -- no selection, so the band rests on the player's own corporation (BL-266) -- is a title row plus 'Open its ledger via [>].' over ~200px of black, and that is the state the band is in most of the time. (b) The right-hand action grid is a 2x3 of six slots, and across tile / province / own building / rival building most slots draw as an empty circle; an empty circle is indistinguishable from a disabled action and from an unimplemented one. (c) The left cell has no consistent subject: a hex neighbourhood for tile and province, a large flat grey silhouette for the Military Base, a bare X for the Extraction Site -- the last two read as missing artwork rather than as designed glyphs. (d) A click on a tile drives the BAND; no click-opened sticky card appeared, so whether BL-194/BL-195's card still exists after BL-266 made Selection always-open is a question the authority does not answer.

**Why it matters.** The band is permanent chrome and the second-largest region on screen. Its resting state is the shell's biggest single piece of dead space.

*Files: `docs/ui/SELECTION.md`, `docs/ui/ledgers/selection.md`, `src/ui/selection_panel.cpp`, `src/ui/selection_card.cpp`*

### NR-594 — Internal identifiers are leaking into player-facing UI
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Four sites found in one pass. The Budget ledger prints 'Policy levers - not yet wired (BL-155)'. The Research panel opens with 'BL-087 design mock - read-only.' The tech ladder offers 'Era 2 (unauthored)' -- an authoring state, offered as a choice. The Contracts ledger names its subject 'Province #20886' and repeats it as 'holds province #20886'; provinces are the grain the player clicks and they carry no player-facing name here at all. A backlog id in a shipped surface is a different category from a placeholder: it is the development process addressing the player.

**Why it matters.** These are invisible to us and the first thing an outside player sees. The province one is more than cosmetic -- it means the contract surface cannot say WHERE the job is.

*Files: `src/ui/balance_ledger.cpp`, `src/ui/tech_tree_panel.cpp`, `src/ui/contracts_ledger.cpp`, `docs/ui/ledgers/balance.md`, `docs/economy/CONTRACTS.md`*

### NR-595 — Nav slot 8's corporations table draws its name column one character wide
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The table's columns are 'C | Reach | Capital | Share | Stance'. The first is the corporation NAME, allotted roughly ten pixels, so every row reads as a single letter (F, E, C, G, T, N, Y, Q, H, Z, B, S) and the header 'C' is itself the truncated word. Beside that, sixteen of seventeen rows read 'Minor | Minor | Negligible | Negligible | Neutral', so the table's information content is close to zero, while one row has a different shape entirely ('G | 1 bodies | 957.8 | 2% | -'). This is the slot that had NO verify hook until this pass, which is why nothing had ever captured it.

**Why it matters.** It is the provisional host for Diplomacy and the only side-by-side rival comparison in the game. It is also direct evidence for the rule that a surface with no scripted path is a surface nobody has looked at.

*Files: `src/ui/corporation_panel.cpp`, `docs/ui/MENU.md`*

### NR-596 — expect_no_clipping's scope is draw-list text only, so '0 records' over the whole shell is not evidence of no clipping
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The 31-capture pass ends with 'expect_no_clipping PASS: 0 failure(s), 0 record(s) total' -- zero RECORDS, not zero failures -- while the Corporation dashboard visibly truncates 'Workforce 76% of labour demand n' mid-word, the Economy panel clips 'CalonIntus-PaxisAthen Venture:', and a table row is cut in half at the fold. The reason: ui::text_fit records only what routes through it, which is draw-list text (the skill's own coverage grep finds just 2 unexempted AddText sites), whereas ~524 ImGui::Text* calls in src/ui draw inside ImGui windows and are clipped by ImGui itself, invisible to the recorder. The check is not broken; its scope is far narrower than 'the shell does not clip', and the verifier-visual skill reads as the latter.

**Why it matters.** A green clipping verdict over a frame that visibly clips is worse than no verdict. Either the recorder grows to cover widget text, or the skill's wording narrows to what it actually proves.

*Files: `src/ui/text_fit.cpp`, `.claude/skills/verifier-visual/SKILL.md`, `docs/development/DEVELOPMENT_PRACTICES.md`*

### NR-597 — The system menu offers only Resume and Exit Game -- there is no way to save from the UI
*observation · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The header-corner popup holds exactly two buttons. Quick save and quick load shipped with BL-536 bound to F5/F6, and display options exist (BL-076), but none of the three is reachable from the menu -- so a player who does not know the function keys cannot save, and cannot discover that saving exists. The popup also draws over the header's NET and elapsed-quarter readouts.

**Why it matters.** Ben opened this session with 'first we should make a save game'. From inside the game, that is a keyboard secret.

*Files: `src/ui/time_panel.cpp`, `docs/ui/MENU.md`, `docs/ui/HEADER.md`*

### NR-598 — Two surfaces report different grid dimensions for the same body
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

On one frame, for Huhaidar: the Generation Ledger's PROFILE reads 'Grid 180x84 (15120 tiles)' -- and 180*84 does equal 15120 -- while the canvas's own caption reads 'Huhaidar Planet (261x121)'. The archived roadmap's ancient-refocus note describes the map as 3x at 312x145. Three numbers, one body; at most one of them is the grid. Worth settling before either surface is written into an authority.

**Why it matters.** The tile census, the placement rules and every per-body area calculation are read off whichever of these is real.

*Files: `src/ui/generation_ledger.cpp`, `src/ui/body_surface_canvas.cpp`, `docs/economy/TILES.md`, `docs/generation/TILE_GENERATION.md`*

### NR-599 — Every decision in the AI feed reads 'overridden' with a chosen score of 0.00
*question · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

The decision feed's visible entries are all one shape: a build candidate, the reason 'best available build site', a score line reading '0.00 v 2.08' or '0.00 v 0.08', and the word 'overridden' in red. If the first number is the chosen candidate's score, the scorer is taking a zero-scored option over a positive one in every logged case; if it is not, the readout does not say what it is. Either the feed is mislabelled or the scorer is doing something worth knowing about.

**Why it matters.** The feed is the only window onto rival reasoning, and 'the credible rival' is a named theme. A readout that always says the same thing cannot distinguish a working scorer from a broken one.

*Files: `src/ui/decision_feed.cpp`, `src/world/corp_ai.cpp`, `docs/ai/AI_OPPONENT.md`*

### NR-600 — Goldens deliberately NOT blessed for the shell pass, despite the exception granted
*decision taken on your behalf · raised 2026-08-24 · from The 2026-08-24 UI shell pass (scripts/verify/shell_pass.lua, 31 captures in build/screenshots).*

Ben granted an exception to the curated-world-independent golden policy for this pass, on the grounds that ubiquitous UI items are essentially world-independent. Held anyway, for two reasons. (1) The captures are FULL FRAMES and the canvas fills most of every one, so a golden over them is world-dependent whatever the chrome does -- a generation change would fail all 31 for reasons with nothing to do with the UI. (2) The pass exists to decide what several of these surfaces should become; blessing now pins the look we are about to change, and the first real alteration then re-blesses everything, which is the bulk re-bless the policy exists to prevent. Recommendation: bless after the alterations, and only over frames where chrome dominates -- or give the golden harness a capture REGION so chrome can be diffed without the canvas behind it.

**Why it matters.** The grant is cheap to spend once. Spending it on a state we are about to overwrite wastes it and leaves 31 stale goldens behind.

*Files: `.claude/skills/verifier-visual/SKILL.md`, `docs/development/DEVELOPMENT_PRACTICES.md`*

---

## Resolved

Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the
answer has landed in an authority doc.

