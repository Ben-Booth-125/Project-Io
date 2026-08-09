# Project Io — Comms Chat Log

The **comms log** is the channel-based chat panel docked **bottom-left** of the shell (`src/ui/chat_panel.{hpp,cpp}`; placement below). It replaced the Explorer placeholder (BL-205, 2026-07-26; see `EXPLORER.md` for the supersession note) and briefly lived on the right edge — below the time column, above the minimap — before BL-227 (comms dock bottom-left, 2026-07-30) moved it down.

It is the surface of the **diplomacy-as-communication principle** (`docs/ai/AI_OPPONENT.md` § 7, Ben 2026-07-26): since every rival corporation is AI, inter-corp coordination happens in a visible communication medium — corps message publicly or in private groups to form plans — rather than in hidden state. In multiplayer the same channels carry human players; the medium is actor-agnostic.

---

## Placement (BL-227, landed 2026-07-30)

The panel owns the **bottom-left tile of the screen's bottom strip**
(`comms_dock_rect` in `src/ui/foldout_column.cpp`):

- **Top edge and height shared with the Selection band** — both are
  `selection_band_height` (itself derived from the minimap height), so the bottom
  strip reads as one level band: comms dock → Selection band → minimap.
- **Width is 0.75 of the fold-out column's width** (Ben, 2026-07-30) — comms is
  ambient chatter, not a decision surface; the quarter it gives back goes to the
  Selection band, which starts at the dock's right edge so the strip stays solid
  with no canvas sliver punched between the two.
- **Layout consequence:** `foldout_column_rect` now stops at the dock's top edge,
  so **every menu and ledger in the fold-out column is permanently shorter** by
  exactly the Selection band's height. Deliberate (Ben's call, 2026-07-30), not
  a regression.

The dock's **neighbour** to the right — the Selection band — takes its rect from
`ui::selection_band_rect` (`src/ui/shell_metrics.hpp`, BL-216), which reads this dock's
right edge rather than re-deriving it. The two share one seam and cannot disagree about
where it falls; there is deliberately **no gutter** between them, the bottom strip being
flush against the icon rail and the right chrome at either end.

Resolved widths (dock / band), at the supported display sizes:

| Display | Shell column `W` | Dock | Selection band | Strip height |
|---|---|---|---|---|
| 1280 × 720  | 380 | 243 | 637  | 260 |
| 1600 × 900  | 380 | 243 | 957  | 260 |
| 1720 × 1080 | 380 | 243 | 1077 | 260 |
| 1920 × 1080 | 384 | 246 | 1274 | 260 |
| 2560 × 1440 | 460 | 303 | 1790 | 310 |

The dock never rails or collapses: its width is a pure function of the shell column's,
so there is no hidden state to get stuck in and no threshold to remember. At the 640×480
settings floor the band's width clamps to 0 — the same fail-soft the strip already had,
not a new failure mode.

---

## Layout in the dock (BL-216)

The dock is ~243 px wide and 260 px tall at 1280–1920, against the ~336 × 698 px
right-column band comms first occupied. Width barely changed; **rows more than halved**.
So the scarce axis is vertical, and every change here buys rows. All of it stays inside
container 1's wrap + vertical-scroll policy (`LAYOUT.md`) — no measured boxes, no new
text policy.

- **One control row.** The standalone `COMMS` label and its separator are folded into
  the channel row as a leading dim label. *Buys a text line plus a separator.*
- **One-line message form.** A message is a single wrapped paragraph — dim `d<N>`, the
  speaker in its identity colour, `:`, then the body — rather than the old two-line
  stanza (day+speaker on one line, an indented body under it). *Buys roughly 40% of the
  log's rows.* The body falls to the next row only when the prefix would leave it less
  than a third of the content width, which a long nation name can do at this width;
  wrapping into a negative run degrades to one character per line, so the fallback is a
  guard, not a preference. System lines (`from == null_entity`) stay one dim wrapped line.
- **Channel selector, not a tab chain.** The channels sit in a combo plus the `+`
  group-create button, on one fixed row. The old `SameLine` chain of small buttons had
  **no overflow handling at all** and clipped silently once the channels ran past the
  panel width — a real defect the narrower dock exposes. A channel selector is a
  cross-cutting selector, so the standing toggle rule explicitly exempts it; the change
  costs no rule complexity and holds an unbounded channel count.
- **Capped group popup.** The `+` popup is size-constrained (`220–320 × ≤260`) with the
  per-corp checkbox list in its own scrolling child. Bottom-anchored in the corner, an
  uncapped list grows *upwards* without bound — one row per corp, off the top of the
  screen at any real corp count.
- **Floor guard.** The panel draws nothing below 120 px of height: under that the log
  child's height goes negative, ImGui clamps it to a 4 px sliver and the input row is
  pushed out of the window. Defensive only — `selection_band_height` never resolves
  that low at a supported size.

## Channels

- **Public** — channel 0, always present. Since the BL-212 slice (nation-voiced comms, landed 2026-07-28) the only speakers ever posted here are **nations**, never corporations. Opens with a deterministic epoch line (now a **nation** count) so the panel is never an empty shell.
- **Groups** — arbitrary player-created channels over any corp subset (the `+` tab: name + member checkboxes; the creator is an implicit member). Session-local in slice 1.
- Later: AI-created groups (plan-forming between AI corps, unreadable by the player unless a future intelligence mechanic exposes intercepts — a Discovery-model extension).

## Message sources

| Source | Slice | Determinism |
|---|---|---|
| Epoch system line (d0) | 1 (live) | Pure function of generation |
| **Nation-voiced agency comms** (BL-212 slice, landed 2026-07-28) — the old per-corp/per-building BL-079 reflex lines were a standing violation of `DISCOVERY.md`'s competitor-visibility rule (they leaked rival internals); `step_economy` now aggregates **one heaviest event per (nation, tick)** and posts it under the corp's `home_nation`, phrased in first person, with **no building or corp specifics** | 1 (live) | Templated from `economy_report::agency_events`, deterministic aggregation |
| Player input (message box, active channel) | 1 (live) | No mechanical effect yet — the AI C-route hook |
| BL-202 corp-command stream (strategic AI decisions) | with BL-202 | Templated from the decision log |
| In-character LLM chat (C-route personality layer) | post-utility-core | Out-of-process, coarse-grained, replay-logged (AI_OPPONENT.md § 2C) |

The chat is deliberately the **AI-observability surface first**: what the background corps do mechanically surfaces as messages before any AI "speaks" for real.

## State & persistence (slice 1)

UI-side, app-owned (`ui::chat_state`): a capped message ring (300 lines) + channel definitions. **Not serialised** — messages re-derive from deterministic sim events; groups are session-local. Serialisation joins the BL-202 landing, when commands become world state.

## Presentation

- Speaker lines carry the author's **identity colour** on the name — `speaker_name` / `speaker_colour` (`chat_panel.cpp`, BL-212) try the **nation** map first (`palette::nation_colour`; Public is nation-authored), falling back to the corp lookup (`palette::corp_identity_colour`; Counsel/group channels stay corp-authored). System lines render dimmed (`palette::text_secondary`).
- Messages are day-stamped (`d<N>`) and render as one wrapped paragraph per message (§ Layout in the dock); the log pins to the newest line unless the player scrolls back.
- ASCII-only message text (the UI font has no em-dash/ellipsis glyphs).

## Related

- `docs/ai/AI_OPPONENT.md` § 7 — the principle; § 5–6 — the command stream that will feed it.
- `LAYOUT.md` — placement in the shell.
- `EXPLORER.md` — the superseded placeholder this panel replaced.
