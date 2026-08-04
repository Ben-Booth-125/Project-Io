# Environment — the 0 A.D. test bench (2026-08-03)

> **Retired 2026-08-04 (Ben, NR-060).** "0 AD" means the *year* — Io's own Era −1 sandbox
> (BL-271), not Wildfire Games' RTS. The RTS was installed and uninstalled the same day; the
> arena is Project Io's word interface, with the Era −1 sandbox as its destination map. This
> doc is kept as the record of the RTS bench that briefly existed; nothing below is live.

## Install state

**Not installed** (checked 2026-08-03: Program Files, LOCALAPPDATA, `Documents\My Games\0ad`,
registry uninstall keys, PATH — all empty; the game has never run under this user).

**Settled 2026-08-03 (Ben, NR-043): Ben installs it himself.** Not an in-session download.
Install **Release 28 "Boiorix"** (Feb 2026, the first non-alpha release) from
https://play0ad.com/download/ — ~1.5 GB — and run it once so
`%USERPROFILE%\Documents\My Games\0ad\` exists. A Project-Rival session then opens Year 1
against it; the headless rehearsal command below is the smoke test.

Until that lands, Year 1 is blocked and everything else is ready: the seed docs, the yearly
rite with its R1–R8 conformance checks, the annal format, and `annals/campaigns.json`.

The engine is `pyrogenesis.exe` (under `<install>\binaries\system\`); all game logic runs in an
embedded SpiderMonkey JS VM, which is why the flags below can drive it unattended.

## Match template

**Rome (us) versus Han China (the built-in Petra bot)**, fixed seed, land map. Difficulty
starts at 3 (medium) and moves only when the annals justify it. *(Flipped 2026-08-03, NR-042 —
seeded the other way round.)*

Two modes, used for different things:

**Text (the campaign mode — re-based 2026-08-04, NR-057).** The engine ships an official agent
seam: `--rl-interface=127.0.0.1:6000`, an HTTP loop the in-tree `zero_ad` Python client
(`source/tools/rlclient/python`) drives — JSON scenario config in, full JSON game state per
step, programmatic commands, exact step control. Year marks become exact ticks and pausing is
free; the JSON state is the blackboard. The seam is lightly maintained research tooling, so it
is pinned to Release 28 and guarded by a conformance smoke test.

**Visual (the fallback mode).** Claude plays with mouse and keyboard via computer-use; the
screen is the blackboard. Kept for visual play and for anything the RL seam does not expose —
no longer the default. *(The 2026-08-03 premise that 0 A.D. exposes no agent interface was
wrong; see NR-057.)*

**Headless (the rehearsal mode).** Bot-vs-bot, no graphics, for testing setups, seeds, and
match length before spending a campaign on them (PowerShell, the shell campaign sessions
actually run):

```powershell
& "<install>\binaries\system\pyrogenesis.exe" `
  -autostart="random/jebel_barkal" -autostart-nonvisual `
  -autostart-seed=42 -autostart-players=2 `
  -autostart-civ=1:rome -autostart-civ=2:han `
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
