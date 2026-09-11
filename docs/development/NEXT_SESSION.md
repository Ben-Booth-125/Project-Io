# Next session — sprint 38 closed; the phase's primary lever is inert

Written 2026-09-11 at the close of a batch delivery. **Sprint 38 was the PHASE, not a fixed item
list** (Ben, 2026-09-11) — and the phase now produces what `CIVILISATION.md` asks of it. The nine
items that opened this session are all closed or superseded.

## Read this first: the reach gate refuses nothing

Three agents, working different items with no contact between them, independently measured the same
line on `history_sweep --epoch 0`:

```
REFUSED reach gate   median 0   (BL-837)
```

`BL-823`'s own resolution note promoted reach-gating from one lever among six to **the primary
lever**, and `CIVILISATION.md` § The road is the empire's skeleton is built on that claim. It
refuses zero campaigns. Whatever is holding these worlds multipolar at 0/16 hegemony, it is not
this.

**It already cost two items their result:**

- `BL-887`'s centre-chain relay is correct, cheap and moves nothing — cheaper reach can only unlock
  ground a price was keeping shut. Tripling the rebate reproduced the default figures **exactly**,
  which is what turns this from a tuning question into a finding.
- `BL-839`'s turbulence lean pulls three forces and **two are inert**: its reach-cost term is a toll
  on a road nobody is stopped on, and `w_fear_q = 400` leans a median of zero candidates.

`BL-905` owns it, at priority A, with three candidate causes to measure and an explicit instruction:
**do not raise a floor until the cause is known.** Three wrong diagnoses have been published against
this file and every one was caught by one number contradicting another on the same page.

## Where the phase stands, measured

16 seeds, `--epoch 0`, zero harness failures, 21 gating checks green:

| | |
|---|---|
| hegemony rate | **0 / 16** worlds at a 50% share |
| largest share | median **12%**, range 6–19% |
| worlds showing rise → peak → fall | **15 / 16** |
| secessions | median **2**/world, 14 regions walking away |
| materials sinks | **8%** of production |
| universalising creeds | **2 of 16** worlds |
| fear-of-next leans | **6 of 16** worlds |

**The caveat that must travel with every one of these numbers:** the phase runs **400 of its
designed 1,600 years**. Round 5 / pass 2 are not built, so this is a quarter of the span.

## The six open items

**Blocked on the machine, not on thinking:**

- **`BL-891`** (round 4 arc readout) — *partial*. The readout renders and reads well on a headless
  capture. The scripted walk **cannot reach round 4**: every Next after round 3 misses.
- **`BL-904`** (wizard footer reachability) — priority A, and the downside case is a release
  blocker. Either the footer has been pushed below the fold at 1080p and the wizard is *blocked at
  round 3*, or a human can scroll to it and this is a scripted-walk problem. **No script can tell
  you which**: `verify.scroll_panel` resolves only named ledger windows and knows nothing about the
  wizard column. Also flags that the verify window reports **1720×1080 while captures come out
  1920×1080** — if those are different spaces, every wizard coordinate ever read off a capture was
  read in the wrong one, and the ones that pass do so by luck.

**Real work, unblocked:**

- **`BL-905`** (reach gate refuses nothing) — read the section above. This is the one that matters.
- **`BL-903`** (communication rung) — split out of `BL-823` on closing it; the last of its six
  levers. Earns its place twice: an anti-hegemon lever *and* the second rung of Ben's own arc.
  **Design is owed before build**, and the first test it must pass is articulating a difference from
  reach that shows up in the numbers rather than in a comment.
- **`BL-901`** (culture crossed water) — `colonisation.cpp` knows it coined a daughter across water,
  but `culture_spawn` drops the fact one struct short of its consumer, so `BL-899` shipped sea legs
  on two of the three facts Ben named. Small repair; arguably the best of the three, being the only
  one that is a deed rather than a circumstance.
- **`BL-902`** (pass_one_handoff fixture red) — seven rows red, pre-existing. The fixture stopped
  producing a war as the sim was reshaped, so the assertions are right and the world under them is
  wrong. **Do not tune the fixture until a war appears** — that is fitting a fixture to its
  assertions. `BL-898`'s harness re-pointed at the real generated era instead; generalise that.

## Awaiting Ben's judgement

`NR-827` (trade is still 0.25% of production — a shape problem, not a magnitude one) ·
`NR-828` (no launched crossing starves; the sea-legs floor is redundant with the port gate) ·
`NR-829` (an inherited grudge decays away in ~3 campaign years) ·
`NR-830` (`BL-838`'s hegemony criterion asks for a fall from a floor) ·
`NR-831` (`w_aggr_q`'s lean sits **inside** the season loop and compounds — if unintended,
`BL-868`'s hard-won magnitude includes a doubling nobody wrote down) ·
`NR-832` (save format 11 → 12; `lean::any` representable but meaningless).

## Two operational notes

- **The saved agent definitions were broken, and are now fixed.** `generation-dev`, `ui-dev` and
  `economy-dev` carried `tools: "All tools except Agent"` — prose where a list belongs — so every
  spawn came up with `Agent` as its only tool. That is the "subagents were unusable" note from the
  last handoff: a config fault, not a brief problem. Replaced with real lists; definitions are
  cached at session start, so this session's five slices ran as `general-purpose` instead.
- **Computer-use resolves `ProjectIo` to a stale worktree exe.** On 2026-09-11 it pointed at a
  **two-day-old** binary while the real build sat in `build/`. Nothing looks broken — a live check
  taken without comparing that path's mtime would verify the wrong exe and look fine doing it.
