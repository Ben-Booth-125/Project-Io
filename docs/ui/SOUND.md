# Project Io — Sound

> **Settles:** what sound is for and what it may never carry · the sonic palette and how a
> tone is made · which events make a sound and which stay silent · whose events are heard ·
> how events far from the camera are treated · how repeated events sound at fast time · what
> plays underneath when nothing happens · which controls the player has over it · where audio
> sits against the simulation.
> **Not here:** music (deferred) · what raises an alert (the owning system's doc) · the alert
> strip itself (HEADER) · the canvases (CANVASES) · time controls (TIME_CONTROLS).
> **Confused with:** HEADER.md, TIME_CONTROLS.md, RENDERING.md.

Authoritative spec for the game's **sound**: short synthesised tones that confirm a press or
mark an event, and one quiet bed underneath. Music is out of this doc's scope and deferred
(Ben, 2026-11-22). Every choice below that is not a dated ruling is a delegated default, and
calibration figures are tuned by ear.

See also: [HEADER.md](HEADER.md) (the alert strip), [TIME_CONTROLS.md](TIME_CONTROLS.md),
[CANVASES.md](CANVASES.md), `docs/tech/TECH_FOUNDATIONS.md` § Framework — SDL3.

---

## 1. What sound is for

Sound **confirms** and **draws the eye**; it never informs. Every cue answers an event the
screen already shows: a press landed, a thing finished, something needs attention. A player
with the sound off loses nothing but speed of notice.

Two rules follow.

- **No sound-only information.** If a cue would tell the player something no surface shows,
  the fix is the surface.
- **Silence is the norm.** A cue earns its place by naming a moment the player would want to
  turn toward. Steady-state processes — production running, goods flowing — are silent.

## 2. Palette — instrument panel (Ben, 2026-11-22)

The game sounds like a **near-future console**: clean sine and triangle blips, short soft
envelopes, small pitch glides. Filtered noise appears only where something physical moves — a
click's edge, a thruster's burn (Ben, 2026-11-22) — never as a rumble under a tone. Nothing harsh and
nothing sampled.

**Every sound is made in code (Ben, 2026-11-22).** There are no audio asset files. A cue is a
small parameter set — waveform, pitch (and glide), envelope, filter (and its sweep), a few layered partials —
rendered to a buffer once at start-up. All cues live in one table, so the palette is tuned in
one place.

The palette has a grammar so cues stay legible:

| Meaning | Shape |
|---|---|
| Done, positive | Rising interval, bright, short |
| Stopped, negative | Falling interval, duller |
| Needs attention | Two-note figure, repeats once |
| Neutral press | Single short blip |
| Refused press | Low, muted, very short |
| Conflict | Lower register, stacked minor stabs |
| Ledger open / close | Two short clicks, rising or falling — not a slide |
| Launch / arrival | A thruster hiss whose filter opens as it climbs, or closes as it settles |

## 3. Which events sound (Ben, 2026-11-22)

Seven families make sound. **Trade is silent**: market clearing, convoys and trade flows
make no cue (Ben, 2026-11-22). A trade problem that needs the player reaches them as an alert.

| Family | Cues |
|---|---|
| **UI presses** | press · toggle on · toggle off · refused press · ledger open/close |
| **Alerts** | one cue as an alert *first* appears, per severity tier — not per kind, not on repaint |
| **Construction** | building complete · construction stalled |
| **Conflict** | war declared · battle begins — a battle's end is silent (Ben, 2026-11-22) |
| **Research** | research complete |
| **Time** | pause · resume · speed change — **no per-tick sound** |
| **Space** | launch · arrival in orbit or at a body |

**Alert tiers.** High: at war, low runway. Low: research not set. Every other alert is mid.
A count rising within one alert (one stalled site becoming two) does not sound again.

**One moment, one cue.** A stall heard on the canvas speaks for the stalled-construction
alert it raises in the same frame. A press that opens a ledger, flips a toggle or changes the
speed plays that cue, not the plain press as well.

**Density is the middle setting (Ben, 2026-11-22, 3 on a 1-5 scale).** Every press makes a
sound, as does every event in the families above that is the player's own. Nothing sounds
continuously, nothing sounds per unit or per tick, and no cue plays for something the player
can't act on.

## 4. Whose events are heard — own only (Ben, 2026-11-22)

Only the **player's own events** sound. An event is the player's own when the player's
nation is a party to it: their building, their research, their launch, their battle.

Rivals' events are silent. A rival event that *is* the player's business — a war declared
on them, a battle against their units — is a party event and sounds.

## 5. The camera — canvas only (Ben, 2026-11-22)

A world event sounds only when its place is on the **active canvas**. There is no panning or
distance fade: a cue is either heard or not, at full level.

- **Shell sounds are global.** UI presses, alerts and time controls always sound, wherever the
  camera is.
- **Off-canvas world events are silent.** If one matters, its alert carries it, and the alert
  sounds.
- **Events with no place are global.** A war and a finished technology belong to the nation,
  not to a tile, so they always sound.

What counts as on the canvas follows the rung: on the Planetary canvas, the active body; on
the Circumplanetary canvas, the active body and what orbits it; on the Solar canvas, everything.

This keeps one route for "something happened elsewhere": the alert strip.

## 6. Fast time — merge into one (Ben, 2026-11-22)

At high speed, many events of the same kind can land in one real-time instant. **Repeats of
one cue inside a merge window play once.** The window runs on wall-clock time, not ticks, so
it is the same at every speed. Its length is calibration (start near a quarter second).

A merged cue is never louder or longer than a single one. Distinct cues in the same window
each play, with a small cap on simultaneous voices; past the cap, the lowest-priority cue drops.
Priority order: conflict, alerts, construction stalled, the rest.

## 7. The bed — one quiet bed (Ben, 2026-11-22)

One **low, slowly evolving tone bed** plays under everything, the same on every canvas. It is
in the palette: soft filtered tones that drift slowly, never a melody. It **ducks** briefly
under cues and is silent while the game is paused at the menu. It is not music.

## 8. Player controls

The settings carry a master volume, a volume per group (interface, events, bed) and a mute.
Sound is quieted when the window loses focus. The settings persist with the player's other
preferences, never in a save.

## 9. Where audio sits

**Audio is a view.** It reads what the tick and the UI already produced and writes nothing
back. It draws no number from the world's seeded streams, and a replay plays out the same
whether sound is on, off, or missing.

- **The source of a cue** is an event the frame already has: a UI press, a new alert, a
  report from the tick. Audio subscribes; no system calls audio.
- **No device, no failure.** If no output device opens, the game runs silent and says so once
  in the log.
- **Entering a game is silent.** What already stands when a game starts or loads — open
  alerts, wars, sites under construction — is the baseline, not news.
- **Headless runs are silent.** `--verify` and every harness open no audio device. A check
  that a cue fires reads the cue requests, never the speakers.

## 10. Where the cues live

The cue table is `scripts/sounds.lua`: every cue's notes, group, priority, level and a
plain-words line saying what fires it. It is the one roster of the game's sounds. The listening
page is built from the same file (`node tools/session/sound_cues.js --page <out.html>`), and
`--check` validates it. A cue is tuned in the table, never in code.
