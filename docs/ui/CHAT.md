# Project Io — Comms Chat Log

The **comms log** is the channel-based chat panel on the right edge of the shell — below the time column, above the minimap (`src/ui/chat_panel.{hpp,cpp}`). It replaced the Explorer placeholder (BL-205, 2026-07-26; see `EXPLORER.md` for the supersession note).

It is the surface of the **diplomacy-as-communication principle** (`docs/ai/AI_OPPONENT.md` § 7, Ben 2026-07-26): since every rival corporation is AI, inter-corp coordination happens in a visible communication medium — corps message publicly or in private groups to form plans — rather than in hidden state. In multiplayer the same channels carry human players; the medium is actor-agnostic.

---

## Channels

- **Public** — channel 0, always present, every corporation. Opens with a deterministic epoch line (corporation count) so the panel is never an empty shell.
- **Groups** — arbitrary player-created channels over any corp subset (the `+` tab: name + member checkboxes; the creator is an implicit member). Session-local in slice 1.
- Later: AI-created groups (plan-forming between AI corps, unreadable by the player unless a future intelligence mechanic exposes intercepts — a Discovery-model extension).

## Message sources

| Source | Slice | Determinism |
|---|---|---|
| Epoch system line (d0) | 1 (live) | Pure function of generation |
| BL-079 agency reflexes — recipe rescue, idle-a-loser — posted to Public | 1 (live) | Templated rendering of `economy_report::agency_events` |
| Player input (message box, active channel) | 1 (live) | No mechanical effect yet — the AI C-route hook |
| BL-202 corp-command stream (strategic AI decisions) | with BL-202 | Templated from the decision log |
| In-character LLM chat (C-route personality layer) | post-utility-core | Out-of-process, coarse-grained, replay-logged (AI_OPPONENT.md § 2C) |

The chat is deliberately the **AI-observability surface first**: what the background corps do mechanically surfaces as messages before any AI "speaks" for real.

## State & persistence (slice 1)

UI-side, app-owned (`ui::chat_state`): a capped message ring (300 lines) + channel definitions. **Not serialised** — messages re-derive from deterministic sim events; groups are session-local. Serialisation joins the BL-202 landing, when commands become world state.

## Presentation

- Corp lines carry the corp's **identity colour** (`palette::corp_identity_colour`) on the name; system lines render dimmed (`palette::text_secondary`).
- Messages are day-stamped (`d<N>`); the log pins to the newest line unless the player scrolls back.
- ASCII-only message text (the UI font has no em-dash/ellipsis glyphs).

## Related

- `docs/ai/AI_OPPONENT.md` § 7 — the principle; § 5–6 — the command stream that will feed it.
- `LAYOUT.md` — placement in the shell.
- `EXPLORER.md` — the superseded placeholder this panel replaced.
