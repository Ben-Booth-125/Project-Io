# Project Io — Menus (Navigation Pane)

> **Settles:** what the navigation rail is, how many slots it holds, and what
> separates a player slot from the developer tail · what a slot's press does and how
> a ledger closes · what state a ledger starts in · where a new surface is appended
> and what would justify inserting it instead · which ledgers exist and in what
> order · what the application menu offers for session and display.
> **Not here:** each ledger's own design (ledgers/*) · where the rail and column
> sit (LAYOUT) · the Selection band, which has no rail slot (SELECTION).
> **Confused with:** LAYOUT.md, ledgers/README.md, SELECTION.md.

The **navigation pane** is a fixed, full-height **icon rail** pinned to the left edge of the shell (below the profile, `nav_pane_width` 56 px). It is the home for the game's menus and ledgers. See `LAYOUT.md` for placement.

---

## Structure

- A vertical strip of **twelve square icon slots** (`nav_pane.cpp`, `tab_count`; the rail's legibility model is BL-174, nav-rail legibility): the **nine curated player slots** below, then a **three-slot developer / observability tail** (§ The tail), which is the foot of the rail. Of the nine, **six carry their own subject** — Nation overview, Budget, Construction, Market Ledger, Convoys, History; **Diplomacy** lost its corporation stance surface (Ben, 2026-10-23) and carries the nation-grain Diplomacy ledger (Ben, 2026-10-30). **Acquisitions** retired with the buyout (Ben, 2026-10-23), and its slot is not on the rail: every slot after it moved up one (Ben, 2026-10-24). **One carries a provisional occupant** — a surface standing in for the slot's subject, because that surface would otherwise have no door: Research hosts the **tech-tree design mock** (BL-310, tech-tree mock). It keeps its real subject's name and glyph, lit like any other live slot, so the rail does not teach the wrong vocabulary; the tooltip says what the slot holds for now. **Military** holds slot 7 (Ben, 2026-10-27), where the reserved Corp. Strategy stood — its subject, a corporation's standing strategy, retired with the corporations. **Diplomacy** holds slot 9 under its own glyph (Ben, 2026-10-30; `ledgers/diplomacy.md`).
- Each slot shows a **vector glyph** (`src/ui/icons.hpp`) instead of a worded label; the slot's name plus a one-line blurb is shown in a wrapping hover tooltip. The rail is deliberately narrow — the profile above keeps its own (wider) `profile_panel_width` rather than matching the rail.
- Each slot toggles a panel open/closed; the open slot lights its glyph in the selection accent — the same idiom as the minimap lens bar, so the two icon strips read as one vocabulary.
- Opened menus **fold out into the shell column** to the rail's right (`foldout_begin`, `src/ui/foldout_column.hpp` — see `LAYOUT.md` § Ledger windows). **Nothing floats and there is no ✕**: closing is the toggle — re-click the slot, re-click the active sub-view tab (the toggle rule, `.claude/rules/io-standing-rules.md`), or open another slot (accordion, `close_all_panels`).

### Policy: ledgers start closed

**Every ledger starts closed on a fresh session.** None are shown until the
player opens them from the navigation pane. This keeps the canvas unobstructed
by default and makes opening a ledger a deliberate act. New ledgers must follow
this — default their open-state to closed.

### The tail — developer / observability slots

Slots 10–12 sit after the curated player slots and never displace them; the tail is where surfaces that are **not player systems** live, and new ones go there rather than interleaving. Each is a live slot with its own glyph and tooltip, toggling like any other:

| # | Slot | Surface |
|---|---|---|
| 10 | **Generation Ledger** (`icons::continent`) | why a tile generated as it did — the per-pass derivation and the body's histograms (BL-303, generation ledger; `../generation/GENERATION_LEDGER.md`) |
| 11 | **AI decisions** (`icons::strategy`, lit) | the rival scorer's rationale — what each AI nation decided and how close the call was (BL-407, decision feed) |
| 12 | **Strategy readout** (`icons::readout`) | the feed's aggregate companion — verb mix, spend buckets and reason tally per nation over the recent run (BL-411, emergent strategy readout) |

Slot 11 draws the pennant glyph the retired Corp. Strategy slot drew dim, because its subject is the strategic decision that slot was reserved for; slot 7, **Military**, draws the filled shield — the military base's own glyph (`icons::building`) — so the ledger and the building it raises force at share a silhouette. Slot 12 has its own glyph because two *lit* slots must not share a silhouette.

### Appending versus inserting

The tail is the foot of the rail: **slot 12 is the last slot, and nothing sits below it.**

Where a new surface goes is nonetheless a real decision, and the rail has both routes.
**Insertion** puts a surface among the curated slots at the point the curated sequence would
put it — Acquisitions was inserted above Market on Ben's call (2026-08-29), because buying a
firm read before the market it was priced from. The ledger has since retired, and its slot with
it (Ben, 2026-10-24).
**Appending** puts it below the developer tail, outside both the curated set and the tail.

The rule is the subject, not the convenience: a surface whose subject is one of the curated
systems belongs in the curated sequence; only a surface that sits outside those systems is a
candidate for appending. Deciding by which is cheaper to implement is how a rail stops
teaching anything.

**An appended slot is the weaker position, and it should be earned rather than defaulted to.**
It puts a player system below the developer/observability surfaces, which teaches the rail's
own ordering wrong — the tail's stated character is "not a player system", and a player
system beneath it contradicts that at a glance.

## Menus are broad ledgers

A nav-rail slot is **reserved UI**, and reserved UI is justified only by a **broad** ledger —
a surface that gives an overview *across many entities*: all of the player's buildings, all of
a body's market, the whole budget. **Specific, targeted ledgers do not get a reserved menu
slot.** A targeted action — building on *one* tile, inspecting *one* market listing, managing
*one* building — is reached **contextually**, through the Selection info element
(`SELECTION.md`) or a transient popup (`LAYOUT.md` § UI popup elements), not through the rail.

The test when deciding whether a new surface earns a slot: *is this a broad overview, or a
targeted action?* Broad → a ledger with a slot. Targeted → the Selection element / a popup,
no slot. This keeps the curated slots scaling with the *systems* the game has, not with the number
of things a player can do to a single entity — e.g. the per-tile "build here" flow lives in the
tile Selection element, while the broad **buildings overview** is what earns the construction
slot.

## Menu set and ordering (settled 2026-06-15, [C1])

The slots are derived from the game systems (`docs/SYSTEMS.md`), filtered through the
**menus-are-broad-ledgers** rule above: each slot is a broad overview surface, never a targeted
action. The order is a **curated player-facing sequence** — a deliberate gameplay order, not
strict SYSTEMS.md tier order. The curated set is **nine slots**: Exploration is not on the rail (the
comms surface is `CHAT.md`; there is no Explorer, `EXPLORER.md`). The `tier-idx` column records
each slot's position in the SYSTEMS.md tier list, so the curation is auditable.

| # | Slot | tier-idx | System / source |
|---|---|---|---|
| 1 | **Nation overview** (dashboard) | 0 | the player nation at a glance |
| 2 | **Budget** | 2 | Budget ([A4]) |
| 3 | **Construction** | 3 | Infrastructure |
| 4 | **Research** *(provisionally hosts the tech-tree mock)* | 7 | Research |
| 5 | **Market Ledger** | 1 | Trade ([A4]) |
| 6 | **Convoys** | — | Supply (`SUPPLY.md`; added 2026-08-29, see below) |
| 7 | **Military** | 5 | Conflict (`../military/MILITARY.md`; Ben, 2026-10-27, see below) |
| 8 | **Diplomacy** | 9 | Diplomacy (`../politics/RELATIONS.md`; Ben, 2026-10-30, see below) |
| 9 | **History** | 6 | Environment |

Two slots are **named more broadly than their source ledger** — Budget (not Balance Ledger) and
History (not Tile Ledger); each widening is settled by Q&A (2026-06-15) and recorded per-slot
below. A third, Corp. Strategy (not Policy), retired with the corporations; Military took its slot.

Notes on the mapping:

- **Resources, Supply, Conflict and Workforce/Population do not get their own slot.** Resource
  detail lives in the Market ledger and the Selection element; Supply folds into
  Construction/Market; Conflict's broad ledger is **Military** (slot 7: what force can be raised,
where, at what cost, and with what kit — Ben, 2026-10-27), while where units stand and how they
fare stays on the canvas; the labour read is a roll-up on the
  Nation overview dashboard (slot 1) rather than a rail subject of its own. The rail scales
  with *systems that have a broad surface*, per the menus-are-broad-ledgers rule.

- **Acquisitions — retired, and off the rail (Ben, 2026-10-23; 2026-10-24).** It sat at slot 5
  and listed which firms could be bought outright and at what price (Ben, 2026-08-29). The
  buyout and the firms are retired (`../GLOSSARY.md` § Buyout). The slot first stayed inert so
  nothing renumbered; Ben then retired it — *"we can retire rail slot 5, and move everything
  up"* (2026-10-24) — so Market is 5 and every later slot moved up one. Nothing persists a slot
  index (a save carries no UI state), so the renumber reaches no save. See
  `ledgers/acquisitions.md`.
- **Slot 1 — Nation overview (formerly the Corporation ledger; BL-248, corporation dashboard;
  BL-691, corp: how am I doing).** Its question is **"how well am I doing?"**, asked of the
  player's own nation (re-subjected, Ben, 2026-10-23), and it holds **one card, Balance**, folded
  out into the shell column (`src/ui/corporation_dashboard.{hpp,cpp}`). The card **rests as one verdict
  line** (`±X/qtr, treasury Y`) **over its chart** — a two-column graph, earnings against the
  quarter's expenses stacked by flow, with the net beneath it. `›` gives it the canvas
  (BL-214, drill-through; `DRILL_THROUGH.md`); there is no expand-in-place control,
  because the card already shows its content at rest. There is no chart question log on the card
  (NR-018; see `DRILL_THROUGH.md` § No chart question log).

  **Three cards went, and where their questions went** (Ben, 2026-08-29): **Production** and
  **Workforce** are answered by the **Construction ledger's Buildings tab**, which holds the
  estate *and* the per-building levers; **Trade** by the **Market** and **Convoys** ledgers. The
  per-item drills went with them — with one card there is no item list to drill, and the card's
  subject is its own flows. Every figure is derived from the live world and the last
  `economy_report`. The chart is `ui::charts::draw_stacked_columns`, shared with the building
  card's Revenue / Expenses graph at the building grain.

  **"Balance" here is a sub-header, not a surface name** (Ben, 2026-08-29), and the overlap
  with slot 2's **Budget** is explicitly accepted rather than tolerated, with Budget to be
  revisited. See `ledgers/corporation.md` § Slot 1.

  > **The MVP four, and what became of them (Ben, 2026-07-31).** An earlier cut named **balance +
  > last-tick delta**, a **holdings roll-up**, **workforce contention** and **alerts**; Ben chose
  > four roll-ups over those when asked directly, and the 2026-08-29 cut then reduced those four
  > to one. The MVP's **click-through into the relevant per-system ledger** survives as the
  > **host axis** (`[>]` / `focus_on_entity`), orthogonal per BL-214's three-axis model. There is
  > no separate **alert** mechanism here: a negative net reads as a red verdict on the card,
  > which is the same signal without a second mechanism to maintain (player alerts as a system
  > are BL-261, player alerts).

  The **rival-field comparison** — corporations grouped by stance — **retired** with the
  corporation (Ben, 2026-10-23), and the Acquisitions ledger that carried its financial half
  retired with the buyout.

- **Buildings have no dedicated ledger (settled 2026-06-15, [F4]).** A standalone buildings
  overview is more "good to know" than goal-driving, so it earns no reserved slot. Buildings are
  read in the two places a player cares about them: **own buildings** ("good for me") in the
  **Nation overview dashboard** (slot 1); **competitors' buildings** ("competition") in the
  **Market Ledger**. Slot 3 is construction *in progress*, not a buildings inventory.
- **Military** (slot 7; Ben, 2026-10-27) is two tabs, **Recruit** and **Design**
  (`ledgers/military.md`). **Diplomacy** (Ben, 2026-10-30) is two tabs, **Nations** and **Wars**
  (`ledgers/diplomacy.md`): every nation's sentiment, stance and power toward the player, and every
  war in the world. The corporation stance surface it once hosted retired with the corporation
  (Ben, 2026-10-23; `ledgers/corporation.md` § Slot 8). It follows the *ledgers-start-closed*
  convention above.
- **Scope-widening names (settled by Q&A 2026-06-15).**
  — **Convoys** (slot 6): **cargo in transit** — one row per convoy in flight, its route, its ETA
    in qtr and the haulage it has cost. Added 2026-08-29 on Ben's call, lifted out of the Market
    Ledger where it had been a third tab. It was never a market question: `MARKETS.md` owns
    clearing and the order book, and a convoy is `SUPPLY.md`'s subject — *Logistics is the road,
    Supply is the traffic*. Placing it directly after Market keeps the commercial run of the rail
    together (Market, Convoys) while giving the logistics read its own door and its
    own lens: a tab strip can arm only one lens, and the old third tab always got the price wash
    when it wanted `supply_routes`. It draws `icons::convoy`, the marker the canvas already uses
    for cargo in transit, so slot and canvas share a silhouette. Design: `docs/ui/ledgers/convoys.md`.
  — **Budget** (slot 2): the full **Budget-system** surface — income vs expenditure broken down
    (sales, input purchases, maintenance, wages) **plus budget allocation** across research /
    military / workforce contracts, not just the running treasury. Same money-loop data as the
    [A4] "Balance Ledger", wider remit.
  — **Corp. Strategy** (slot 7) **retired** with the corporations (Ben, 2026-10-27): its slot went
    to **Military**. Its standing-strategy levers have homes elsewhere — wage and tax levels in the
    Budget, military posture on the Military ledger.
  — **History** (slot 9): **generation history**, not a live event log (BL-211, player-facing
    history ledger). It surfaces the **procedural generation as a number-crunch** — the world as
    generated — in two views, **Story / Chain**: the body's dated biography, and the generation
    stage charts (one fold per chain stage, under the wizard's three round tabs). The charts come
    from `ui::generation_charts`, shared verbatim with the New World wizard, so the plots a
    player decided a world on are the plots they can reopen mid-campaign. The two views share one
    premise — *how this world came to be* — and a current-state view does not belong in it
    (BL-281, Ben via NR-020: a tile/building/market table about *now* inside a ledger about the
    past is a defect, not a naming problem; buildings are on the canvas and in the Selection
    element, market data in the market surfaces). The ledger's two further reads are
    **exploration-gating** (you see the history of what you have explored, as a distinct
    informational set) and a **post-generation advisory** read — the state of resources and
    workforce after generation, and advice approached from it. Closely relates to the
    **Generation Ledger** (`../generation/GENERATION_LEDGER.md`) — likely the same rail surface.
  — **Construction** (slot 3): **construction-in-progress** — a build-queue / progress surface
    (what is being built, its cost and progress). Own-building *inventory* stays in the
    dashboard; this slot is the *active construction* view (the Construction Ledger of the [A4]
    family).

## Application / system menu — session control

The session menu is BL-070 (in-app system menu). Separate from the nav-rail ledgers above
(per-system overviews) and from the display-options surface below. This is the **session
shell** — start/pause/exit — the application-level control the player reaches without knowing a
hotkey (`src/ui/time_panel.cpp`, `src/ui/ui_state.hpp`).

- **Surface — corner gear (hamburger) button.** A small three-line glyph pinned to the **top-right**,
  just left of the time column, clear of the header content and the time panel. Lowest layout
  disruption, no permanent chrome, reuses the icon-rail visual language. (The top menu-bar and
  the centred pause-modal alternatives were rejected for the prototype minimum — the centred
  modal is the right long-term home from a title screen.)
- **Contents.** **Pause / Resume** and **Exit Game**. Pause mirrors the Space-hotkey pause —
  popup and keyboard drive **one** shared session-control flag (routed through the same
  `pause_toggle` path / `sim_loop` speed-0 + remembered speed), not two. Exit is **destructive**
  (there is no in-session save), so it arms an **inline "Really quit?" confirm** in the same
  popup rather than quitting on the first click.
- **Keyboard parity.** **Esc** toggles the same popup (handled ahead of the ImGui keyboard guard so
  it works while the popup or another panel holds focus); an armed exit-confirm backs out first, so
  Esc cancels the confirm before it closes the menu.
- **Reserved home.** This popup is the reserved session-control home: New/Restart — routing back to
  the title screen (`STARTUP.md`) — and **Save/Load** (BL-536, world snapshot save) belong here.

## Application / system menu — display options

The display-settings surface is BL-076 (display options). Separate from the nav-rail ledgers
and from the session menu above.

- **Options window** — toggled by **F10** (`canvas_command::options_toggle`; appears in the F1
  cheat-sheet). A floating ImGui window with a **Display** section: resolution preset combo
  (1280×720 / 1600×900 / 1920×1080 / 2560×1440, "Custom" when the live size matches no preset),
  **Fullscreen** (borderless-desktop) and **VSync** toggles, a live `Window: W×H` readout, and
  Close. Resolution is disabled while fullscreen is on.
- **Persistence** — settings are stored in a flat `options.cfg` (key=value: `window_w`, `window_h`,
  `fullscreen`, `vsync`) in the working directory, loaded at interactive startup only (never in the
  `--verify` path, so golden captures stay at the fixed default). Preset/toggle changes save
  immediately; a free drag-resize of the window frame is remembered and flushed on clean exit.
- **Follow-ons** — this surface is the natural home for later non-display options (UI scale, font
  size, monitor selection) and sits beside the session menu and Save/Load.

## Settled questions

- **All menus dock.** Every ledger folds out into the shell column (BL-122, docked ledgers). No
  floating ledgers.
- **No Explorer.** Exploration stays off the rail; the comms surface (`CHAT.md`) took the
  Explorer's place (`EXPLORER.md`).

## Related

- `STARTUP.md` — the app's entry screens (main menu → New World wizard → in-game).
- `LAYOUT.md` — placement in the shell.
- `CANVASES.md` and `SELECTION.md` — where the per-body buildings read lives: the canvas, and
  the Selection element.
- `CHAT.md` — the comms surface.
