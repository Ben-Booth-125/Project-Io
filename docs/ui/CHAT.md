# Project Io — Comms Chat Log

The **comms log** is the channel-based chat panel docked **bottom-left** of the shell (`src/ui/chat_panel.{hpp,cpp}`; placement below). The panel is BL-205 (comms chat log); its dock placement is BL-227 (comms dock bottom-left). `EXPLORER.md` records the placeholder surface it replaced.

It is the surface of the **diplomacy-as-communication principle** (`docs/ai/AI_OPPONENT.md` § 7, Ben 2026-07-26): since every rival corporation is AI, inter-corp coordination happens in a visible communication medium — corps message publicly or in private groups to form plans — rather than in hidden state. In multiplayer the same channels carry human players; the medium is actor-agnostic.

---

## Placement

The panel owns the **bottom-left tile of the screen's bottom strip**
(`comms_dock_rect` in `src/ui/foldout_column.cpp`):

- **Top edge and height shared with the Selection band** — both are
  `selection_band_height` (itself derived from the minimap height), so the bottom
  strip reads as one level band: comms dock → Selection band → minimap.
- **Width is 0.75 of the fold-out column's width** (Ben, 2026-07-30) — comms is
  ambient chatter, not a decision surface; the quarter it gives back goes to the
  Selection band, which starts at the dock's right edge so the strip stays solid
  with no canvas sliver punched between the two.
- **Layout consequence:** `foldout_column_rect` stops at the dock's top edge,
  so **every menu and ledger in the fold-out column is shorter** by exactly the
  Selection band's height. Deliberate (Ben's call, 2026-07-30).

The dock's **neighbour** to the right — the Selection band — takes its rect from
`ui::selection_band_rect` (`src/ui/shell_metrics.hpp`), which reads this dock's right edge
rather than re-deriving it. The two share one seam and cannot disagree about where it falls;
there is deliberately **no gutter** between them, the bottom strip being flush against the icon
rail and the right chrome at either end.

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
settings floor the band's width clamps to 0 — the strip's fail-soft, not a separate failure mode.

---

## Layout in the dock

The dock is ~243 px wide and 260 px tall at 1280–1920. The scarce axis is vertical, and every
choice here buys rows. All of it stays inside container 1's wrap + vertical-scroll policy
(`LAYOUT.md`) — no measured boxes, no separate text policy. The fit pass is BL-216 (comms dock
fit).

- **One control row.** The `COMMS` label is a leading dim label on the channel row, with no
  separator of its own. *Buys a text line plus a separator.*
- **One-line message form.** A message is a single wrapped paragraph — dim `d<N>`, the
  speaker in its identity colour, `:`, then the body — rather than a two-line stanza (day+speaker
  on one line, an indented body under it). *Buys roughly 40% of the log's rows.* The body falls
  to the next row only when the prefix would leave it less than a third of the content width,
  which a long nation name can do at this width; wrapping into a negative run degrades to one
  character per line, so the fallback is a guard, not a preference. System lines
  (`from == null_entity`) stay one dim wrapped line.
- **Channel selector, not a tab chain.** The channels sit in a combo plus the `+`
  group-create button, on one fixed row. A `SameLine` chain of small buttons has no overflow
  handling and clips silently once the channels run past the panel width. A channel selector is
  a cross-cutting selector, so the standing toggle rule explicitly exempts it; it holds an
  unbounded channel count at no rule cost.
- **Capped group popup.** The `+` popup is size-constrained (`220–320 × ≤260`) with the
  per-corp checkbox list in its own scrolling child. Bottom-anchored in the corner, an
  uncapped list grows *upwards* without bound — one row per corp, off the top of the
  screen at any real corp count.
- **Floor guard.** The panel draws nothing below 120 px of height: under that the log
  child's height goes negative, ImGui clamps it to a 4 px sliver and the input row is
  pushed out of the window. Defensive only — `selection_band_height` never resolves
  that low at a supported size.

## Channels

Channel order is **appended, never inserted** — `app::m_counsel_channel` caches channel indices,
so a standing channel anywhere but the end silently repoints them. The standing channels are
named constants (`chat_state::k_public_channel`, `chat_state::k_field_channel`) so posters do not
spell the index.

- **Public** — channel 0, always present. The only speakers ever posted here are **nations**,
  never corporations (BL-212, nation-voiced comms). Opens with a deterministic epoch line (a
  nation count) so the panel is never an empty shell.
- **Field** — channel 1, always present (BL-468, battle dispatch stream; Ben's call, 2026-08-19).
  Battle traffic: one dispatch line per battle per tick, plus the aftermath line of any that
  concluded — which is the only place the aftermath *can* live, since a concluded battle is erased
  at the end of the tick it ends. Restricted to the **player's own fights**, on the BL-212
  precedent: a line naming two rivals' strengths would leak internals through comms by another
  route (NR-470). It is a channel of its own rather than more Public traffic because its volume is
  driven by **simulation intensity** rather than by scripted events. Muting a channel is open
  (NR-471).
- **Counsel** — one per corp, created lazily, player-only (BL-207, persona packs): a corp's
  advisor channel. Indices cached in `app::m_counsel_channel`.
- **Groups** — arbitrary player-created channels over any corp subset (the `+` tab: name + member
  checkboxes; the creator is an implicit member). Session-local.
- AI-created groups (plan-forming between AI corps, unreadable by the player unless an
  intelligence mechanic exposes intercepts — a Discovery-model extension) are a designed
  extension of the same channel model.

## Message sources

| Source | Determinism |
|---|---|
| Epoch system line (d0) | Pure function of generation |
| **Nation-voiced agency comms** (BL-212) — per-corp/per-building reflex lines would violate `DISCOVERY.md`'s competitor-visibility rule by leaking rival internals, so `step_economy` aggregates **one heaviest event per (nation, tick)** and posts it under the corp's `home_nation`, phrased in first person, with **no building or corp specifics** | Templated from `economy_report::agency_events`, deterministic aggregation |
| **Battle dispatches** (BL-468) — one line per the player's own battle per tick to the **Field** channel, phrased from six seeded banks in `src/core/battle_dispatch_text.cpp` | Pure function of `economy_report::battle_dispatches`. **Consumes no draw**: phrase selection folds `stream_seed` with the round index (the BL-290 tongue-bank idiom), so the dispatch layer is structurally unable to move the simulation rather than merely disciplined not to |
| Player input (message box, active channel) | No mechanical effect — the AI C-route hook |
| Corp-command stream (strategic AI decisions, BL-202 rival scorer) | Templated from the decision log |
| In-character LLM chat (C-route personality layer) | Out-of-process, coarse-grained, replay-logged (AI_OPPONENT.md § 2C) |

The chat is deliberately the **AI-observability surface first**: what the background corps do mechanically surfaces as messages before any AI "speaks" for real.

## State & persistence

UI-side, app-owned (`ui::chat_state`): a capped message ring (`k_message_cap`, 300 lines) + channel definitions. **Not serialised** — messages re-derive from deterministic sim events; groups are session-local. Serialisation belongs with the command stream, when commands become world state.

## Presentation

- Speaker lines carry the author's **identity colour** on the name — `speaker_name` / `speaker_colour` (`chat_panel.cpp`) try the **nation** map first (`palette::nation_colour`; Public is nation-authored), falling back to the corp lookup (`palette::corp_identity_colour`; Counsel/group channels are corp-authored). System lines render dimmed (`palette::text_secondary`).
- Messages are day-stamped (`d<N>`) and render as one wrapped paragraph per message (§ Layout in the dock); the log pins to the newest line unless the player scrolls back.
- ASCII-only message text (the UI font has no em-dash/ellipsis glyphs).

## Related

- `docs/ai/AI_OPPONENT.md` § 7 — the principle; § 5–6 — the command stream that feeds it.
- `LAYOUT.md` — placement in the shell.
- `EXPLORER.md` — the placeholder this panel replaced.
