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
- Messages are day-stamped (`d<N>`); the log pins to the newest line unless the player scrolls back.
- ASCII-only message text (the UI font has no em-dash/ellipsis glyphs).

## Related

- `docs/ai/AI_OPPONENT.md` § 7 — the principle; § 5–6 — the command stream that will feed it.
- `LAYOUT.md` — placement in the shell.
- `EXPLORER.md` — the superseded placeholder this panel replaced.
