# Project Io

Project Io is a near-future, space-based grand strategy game. You begin as a **corporation**
— not a nation — competing across an Earth-like solar system through resource extraction,
trade, and military conflict. Unlike a nation-state, a corporation starts with no territory
and no standing force: every asset must justify itself through economic or strategic return.
The design arc points beyond that start, and it is deliberately still moving: the player is
being redefined as a **mercenary company** — hired to make a fact about the world true, paid
for doing it, and buying its equipment from companies it does not own.

That last clause is becoming a system of its own. **Ownership is separating from identity**:
not every firm is publicly held, and owning one is divorced from being one. A **corporation**
is an operating firm — one of roughly ninety, each already running itself, trading and growing
on the same deterministic scored-utility AI the rivals use. A **syndicate** is an owner — there
are seven, and you are one of them — that holds equity in corporations, allocates capital, and
lives off the profit.

A minority stake buys a claim on earnings and nothing else. Take a firm past half, though, and
you inherit its decisions along with its profits — so control costs attention, and the question
the game asks an owner is how much of it to spend. Designed, not yet built.

The campaign world is **generated, history included**. A pre-campaign simulation (Era −1)
plays out an institutional ladder — polities rising, fragmenting, and collapsing — to produce
the saturated, market-based, non-hegemonic world the campaign opens on (epoch 0 CE); its nations, cultures,
names, and background corporations all fall out of that seeded run. In play, the game
progresses through **Eras** — the home planet first (Era 0, Terrestrial), the solar system
once the gate to Era 1 (Early Space) is met — across three nested **canvases** (Solar →
Circumplanetary → Planetary) and a tick-based economy where prices resolve per body.

You are not alone in it. Rival corporations build, trade, hire, and take stances through a
**deterministic scored-utility AI** over the same command seam the player uses; the longer
arc sockets a **local language-model opponent** through the game's word interface (blackboard
export + action dictionary + MCP server), with a hard invariant that the engine ships no
cloud dependency.

The project is in **prototype phase**, solo-developed in C++ with Lua scripting. Design and
technical documentation lives under [`docs/`](docs/) — start with
[`docs/CONCEPT.md`](docs/CONCEPT.md) and [`docs/SYSTEMS.md`](docs/SYSTEMS.md); the milestone
arc — the live ancient band, and the parked space arc beyond it — lives in [`docs/development/ROADMAP.md`](docs/development/ROADMAP.md).

## Building & running

The game is a single CMake project. Its C++ dependencies — **SDL3**, **Lua 5.4**, **sol2**, and
**Dear ImGui** — are fetched and built automatically by CMake's `FetchContent` on the first
configure, so there is nothing to install by hand beyond a toolchain, CMake, and (on Linux) the
system libraries SDL3 needs.

### Prerequisites

- **CMake ≥ 3.25** and a **C++20** compiler (GCC 13+, Clang, or MSVC / Visual Studio Build Tools).
- A network connection for the first configure (dependencies are downloaded then).
- **Linux only** — the development headers SDL3 links against:

  ```sh
  sudo apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev \
    libxinerama-dev libxss-dev \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libegl1-mesa-dev libgl1-mesa-dev \
    libpulse-dev libasound2-dev
  ```

### Configure & build

From the repository root:

```sh
# Configure (downloads and builds dependencies on the first run)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j
```

On **Windows** with the Visual Studio generator, pass the config at build time instead:

```sh
cmake -B build
cmake --build build --config Release
```

The build copies the `scripts/` and `assets/` directories next to the executable, so the binary
is self-contained once built.

### Run

Launch the game from the repository root:

```sh
# Linux / macOS
./build/ProjectIo

# Windows (Visual Studio generator places the exe under the config directory)
build\Release\ProjectIo.exe
```

### Headless verification harnesses (optional)

The SDL/Lua/ImGui-free logic harnesses under `tools/verify/` are built as extra CMake targets and
exercise the pure simulation code (economy arithmetic, tile generation, placement audits):

```sh
cmake --build build --target econ_bankruptcy survey_harness visibility_harness workforce_harness
./build/econ_bankruptcy
```

There is also a headless **visual-verify** mode that renders a canvas script offscreen and compares
it against a committed golden image:

```sh
./build/ProjectIo --verify scripts/verify/corporation_lens.lua
```

## Latest releases — v0.1.19, v0.1.20, v0.1.21 and v0.1.22

**v0.1.22 — the ledgers get read.** Every nav-rail ledger was reviewed against live captures and most were rebuilt: Diplomacy became a stance surface rather than a balance table, Acquisitions and Convoys were created, Construction gained the estate and its levers, and the Market ledger flattened into one row per good with its price history drawn in the row. The sprint's real yield was not the surfaces, though — it was finding **four designed mechanisms that had never run in a played world**: off-world markets (one market-bearing body at four hundred ticks), rival acquisition (asserted by the docs in the present tense, absent from the code), mercenary contracts (client nations never fund their own offers), and chain depth as a growth gate (half the ungated roster already produces nothing). Two were retired on the spot. The release also records that roughly thirty harnesses could not compile from a clean configure — warm trees passed on stale objects, and a release was cut over it.

**v0.1.21 — the loop closes.** A corporation now saves up over about two in-game years, buys another firm outright, and is still earning twenty quarters later — measured, not asserted. Spawn viability went from three seeds in twelve to twelve in twelve with no interest charged anywhere, and the demand baskets learned which era they are in, which removed the 65.7 % of demand that had been pointing at goods the ancient world cannot make. The release also records what it does not have: a guard deliberately red on eight goods still waiting for a buyer, and a free-firm trap where the cheapest company on the market is the one whose debt sinks you.

**v0.1.20 — the books open.** Every corporation now files a quarterly return, and whether you may read it is a generated fact: a firm's ownership class decides whether it publishes at all. A public firm can be bought outright, priced from its own filed books. Retiring the old banded read means a rival's figures are exact where the firm files and absent where it does not — a dash means *this firm does not file*, never *you have not earned this*. The release also records a measured negative: spawn viability is **not** solved, because 65.7 % of all demand at epoch 0 names goods that era cannot make, and the demand side of the economy is the next sprint's work.

**v0.1.19 — the world reads lived-in.** Population density becomes a consequence of the
simulated Era −1 history (~1,450 centres, one per ~6 land tiles), every land province is
anchored by a centre that decides its nation, urban ground is stamped at generation, and the
economy gains its first non-purchasable constraints: a per-nation qualification fraction that
deep methods consume, wage-competition labour clearing, and stance-gated migration that
carries qualified workers with it. Road generation scales to the new density (2.9 s → 0.7 s)
with era-relative tier gates — the antiquity world keeps its Roads backbone.

**v0.1.18 — Logistic Points land with their consumers.** The network gets its ceiling: the
bifold city throughput rate with the priced march and convoy admissibility, rival road/hub
building and directed dispatch under the 2026-08-24 grants, the Throughput lens, the dispatch
form, and the Port gating the sea leg.


**v0.1.17 — the ancient roster becomes a ladder.** Seven named buildings is no longer a roster:
Tannery, Weaver, Potter's Kiln, Sawmill, Stonemason, Toolmaker and Shipwright grow it to 23,
across five new goods with their own construction materials rather than a shared steel default.
Progression runs through methods, not tiers — a genuine alternate production route wins by market
fit under either a cheap-fuel or a dear-fuel price regime, never both, and a tech can now unlock a
specific recipe rather than only a whole building type. A fresh campaign opens onto a
deliberately narrow start gate, and the corporation dashboard now reads the chain depth reached,
the good that set it, and what's missing to build the next rung — the growth track is visible,
not just a gate.

**v0.1.15 — the mercenary vertical slice.** A polity hires the company, the company fights, the
company is paid — playable end-to-end. Nations field static garrisons sized off their own
treasury; a threatened nation's budget derives real mercenary offers against its highest-grudge
neighbour's weakest border province; accepting an offer through the new Contracts ledger's force
picker and marching that force into the target province is itself what starts the fight against
the nation's garrison, no separate declaration needed. Every terminal event posts to the Public
channel and a "Contract income" line now credits the Balance ledger and header runway the tick a
payout lands.

**v0.1.14 — procurement, and the goods it is about.** The refocus's actual mechanic: a
corporation can request a quote from a named supplier, accept it into a running contract (a
deposit up front, the rest paced across an agreed lead time), or cancel one in flight — and a
supplier can refuse, legibly, for one of four reasons (no capacity, no input access, an embargo,
a poor trading history). Backed by seven new tradeable resources (silicon through spacecraft
components) that close the gap between what the world can dig up and what the market will
actually price, plus military points and a Research Institute — the production half of "how does
tech get done", accruing passively for every corporation.

**v0.1.10 — generation & content: what the world is called and what it is made of.** Every
generated proper noun is now coined from its own culture's phonology, and the kinship between names
is *emergent* rather than authored — cultures that share a realm word or a settlement morpheme do so
because the word-coiner is a pure function of the tongue. Body identity stops being a display string
across twelve sites, and the system catalogue is generated. Corporations anchor in their **home
province**, so where a company is says something about where it came from. Wetland exists again
(12 tiles → 159) once elevation was given a say in composition. Territorial fragmentation is
**measured and attributed**, settling a claim the world generator's architecture was bought on. Plus
propellant as a real resource with per-launch consumption, and an econ tick roughly halved.


**v0.1.9 — shell & legibility: the standing UI set, finished.** The always-on canvas layers become
decodable (roads name their tier on selection, and dim with the commercial-reach fog like everything
else); **building stacks become a decision** rather than a dominant strategy — site *k* yields
`0.8^(k-1)` of a lone site while still drawing real reserve, so a full stack drains the deposit
faster than its output multiple; screen geometry gets **one owner** instead of five hand-derived
copies; and disclosure splits into **two controls** — expand in place, or take the canvas — so a
full-screen no longer costs you the header, the clock and your selection. Also retires the History
ledger's Tiles view and gives the orphaned Economy panel a door.


**v0.1.8 — build health: the gate stops lying.** No player-facing content. `ctest` had been
reporting **ten failures of which exactly one was a real failing assertion**; the rest were
harnesses exceeding a bound they were never sized against, open-ended research sweeps that never
finish, and wall-clock benchmarks failing under machine load. Three test tiers plus `sweep` and
`bench` labels fix that. Also: `next_id.js` stopped silently defending nothing (it was issuing
backlog ids 25 below the true ceiling — the account of how eight items each landed twice), the
GCC goldens were re-blessed, and a seeded FetchContent cache lets fresh worktrees configure
offline.


**v0.1.1 — the word interface.** An agent can read the world state (blackboard export), look up
what every one of the 115 controls *means* (the action dictionary, transcribed from the command
seam rather than authored beside it), and drive the game over a socket — `ProjectIo --serve` plus
`tools/mcp/`. No HTTP client, no API key, no cloud dependency in the engine. Ships with the sticky
detail-card family, the corporation dashboard, commercial-activity fog, and the radial tech-tree
viewer. The write leg is deliberately partial — standing sell orders are not yet issuable by
command (BL-293).

**v0.1.2 — buildings rework: remoteness stops being free.** Placement now enforces a **logistical maximum
range** from a supply anchor, so siting is a trade-off rather than a lookup of the richest tile on
the map; **build time scales with the site** (landform, distance from an anchor, established
stack); and **construction reads as a process** on the canvas with its remaining time legible. Also
widens extraction from 4 to 15 targets, groups the construction ledger, and warns that a build will
starve *before* you commit. The processing half of the roster is deliberately out of scope
(BL-340). Full history in [`CHANGELOG.md`](CHANGELOG.md).

## Previous release — v0.1.0

**The prototype cut.** The economy loop, validated and playable end-to-end: construction,
population-grounded workforce, spatial price divergence via supply convoys, gated discovery
(survey + competitor-intelligence scoping), a legible budget, and a green, performance- and
data-growth-audited build. Adds the generated **road & logistics lattice**, the **continent lens**
and Continents/Drift generation pass, **terrain landform render** with graded terrain combat
costs, the **tile construction ledger**, and three new quality-audit instruments (frame budget,
econ-tick scaling, data-creep). Everything past this cut — laws, techs, military, politics, the AI
opponent — is the v0.1.x → v0.3.0 arc, not this release. Full history in
[`CHANGELOG.md`](CHANGELOG.md).

## Versioning & releases

Version history is tracked with annotated git tags (`vX.Y.Z`); each tag is a recoverable
release point and `CHANGELOG.md` carries its summary. Releases are produced by the **Cut**
process — see [`docs/development/DEVELOPMENT_PRACTICES.md`](docs/development/DEVELOPMENT_PRACTICES.md)
§ Cutting a release.
