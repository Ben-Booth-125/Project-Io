# Environment — the 0 A.D. test bench (2026-08-03)

## Install state

**Not installed** (checked 2026-08-03: Program Files, LOCALAPPDATA, `Documents\My Games\0ad`,
registry uninstall keys, PATH — all empty; the game has never run under this user).

First step of the first session: install **Release 28 "Boiorix"** (Feb 2026, the first
non-alpha release) from https://play0ad.com/download/ — ~1.5 GB. Installation is a Ben action
or a Ben-approved download; run once so `%USERPROFILE%\Documents\My Games\0ad\` exists.

The engine is `pyrogenesis.exe` (under `<install>\binaries\system\`); all game logic runs in an
embedded SpiderMonkey JS VM, which is why the flags below can drive it unattended.

## Match template

Han China (us) versus Rome (the built-in Petra bot), fixed seed, land map. Difficulty starts
at 3 (medium) and moves only when the annals justify it.

Two modes, used for different things:

**Visual (the campaign mode).** Claude plays with mouse and keyboard via computer-use — the
NR-040 (computer-use steer) model. Prompts pause at year marks; the screen is the blackboard.
This is where annals come from.

**Headless (the rehearsal mode).** Bot-vs-bot, no graphics, for testing setups, seeds, and
match length before spending a campaign on them (PowerShell, the shell campaign sessions
actually run):

```powershell
& "<install>\binaries\system\pyrogenesis.exe" `
  -autostart="random/jebel_barkal" -autostart-nonvisual `
  -autostart-seed=42 -autostart-players=2 `
  -autostart-civ=1:han -autostart-civ=2:rome `
  -autostart-ai=1:petra -autostart-ai=2:petra `
  -autostart-aidiff=1:3 -autostart-aidiff=2:3 `
  -autostart-player=-1
```

(Map path syntax is `TYPEDIR/MAPNAME` with TYPEDIR one of `random`, `skirmishes`, `scenarios`;
map ids are snake_case. `random/jebel_barkal` is the one id verified in research — confirm map
and civ ids against the installed build before first use, and keep player count matched to the
slots configured.)

## Records

Every match writes a replay — plain-text `commands.txt` plus metadata JSON — to
`%USERPROFILE%\Documents\My Games\0ad\replays\<version>\<timestamp>\`. The sim is
deterministic: a seed plus its commands.txt is a complete, reproducible record of a campaign
year, and the annal cites it.

`-replay=PATH` re-runs a replay headless (verification); `-replay-visual=PATH` watches it.
`-ooslog` dumps per-turn state when a dispute needs arbitration.

## The campaign year

A match runs continuous time; years are imposed by the rite, not the engine.

**Default cadence: one campaign year ≈ 5 minutes of match time.** At each year mark: pause,
run the six stations (see `docs/CAMPAIGN.md`), unpause. A standard match yields roughly a decade —
enough annal to judge a doctrine.

**Chaining:** a full war may span several matches. Each match is a named war in the chronicle;
outcomes carry forward narratively (territory, standing, doctrine), not mechanically — the RTS
has no persistent map, and we do not pretend otherwise.

Both numbers are defaults, not law: if five minutes proves too thin a year, the annal that
shows it earns the change.

## Later, if earned

A test mod (`Documents\My Games\0ad\mods\<name>\`, activated with `-mod=NAME`) could add
trigger-script telemetry, or a custom JS bot reading orders from a file. Neither is needed for
the campaign rite; both violate proportionality until an annal demonstrates the lack.
No official external RPC exists — driving a live match from outside means playing it, which is
the point.
