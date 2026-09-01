# Project Io — Menus (Navigation Pane)

The **navigation pane** is a fixed, full-height **icon rail** pinned to the left edge of the shell (below the profile, `nav_pane_width` 56 px). It is the home for the game's menus and ledgers. See `LAYOUT.md` for placement.

---

## Structure

- A vertical strip of **thirteen square icon slots** (`nav_pane.cpp`, `tab_count`; the rail's legibility model is BL-174, nav-rail legibility): the **ten curated player slots** below, then a **three-slot developer / observability tail** (§ The tail), which is the foot of the rail. Of the ten, **eight carry their own subject** — Corporation overview, Budget, Construction, Acquisitions, Market Ledger, Convoys, Diplomacy, History. **One carries a provisional occupant** — a surface standing in for the slot's subject, because that surface would otherwise have no door: Research hosts the **tech-tree design mock** (BL-310, tech-tree mock). It keeps its real subject's name and glyph, lit like any other live slot, so the rail does not teach the wrong vocabulary; the tooltip says what the slot holds for now. **One is reserved** — Corp. Strategy — disabled, but carrying its own dimmed glyph so the rail teaches the shape of the game rather than showing a blank.
- Each slot shows a **vector glyph** (`src/ui/icons.hpp`) instead of a worded label; the slot's name plus a one-line blurb is shown in a wrapping hover tooltip. The rail is deliberately narrow — the profile above keeps its own (wider) `profile_panel_width` rather than matching the rail.
- Each slot toggles a panel open/closed; the open slot lights its glyph in the selection accent — the same idiom as the minimap lens bar, so the two icon strips read as one vocabulary.
- Opened menus **fold out into the shell column** to the rail's right (`foldout_begin`, `src/ui/foldout_column.hpp` — see `LAYOUT.md` § Ledger windows). **Nothing floats and there is no ✕**: closing is the toggle — re-click the slot, re-click the active sub-view tab (the toggle rule, `.claude/rules/io-standing-rules.md`), or open another slot (accordion, `close_all_panels`).

### Policy: ledgers start closed

**Every ledger starts closed on a fresh session.** None are shown until the
player opens them from the navigation pane. This keeps the canvas unobstructed
by default and makes opening a ledger a deliberate act. New ledgers must follow
this — default their open-state to closed.

### The tail — developer / observability slots

Slots 11–13 sit after the curated player slots and never displace them; the tail is where surfaces that are **not player systems** live, and new ones go there rather than interleaving. Each is a live slot with its own glyph and tooltip, toggling like any other:

| # | Slot | Surface |
|---|---|---|
| 11 | **Generation Ledger** (`icons::continent`) | why a tile generated as it did — the per-pass derivation and the body's histograms (BL-303, generation ledger; `../generation/GENERATION_LEDGER.md`) |
| 12 | **AI decisions** (`icons::strategy`, lit) | the rival scorer's rationale — what each corporation decided and how close the call was (BL-407, decision feed) |
| 13 | **Strategy readout** (`icons::readout`) | the feed's aggregate companion — verb mix, spend buckets and reason tally per corp over the recent run (BL-411, emergent strategy readout) |

Slot 12 borrows the pennant glyph slot 8 draws dim, because its subject is exactly the strategic decision that slot is reserved for; slot 13 has its own glyph because two *lit* slots must not share a silhouette.

### Appending versus inserting

The tail is the foot of the rail: **slot 13 is the last slot, and nothing sits below it.**

Where a new surface goes is nonetheless a real decision, and the rail has both routes.
**Insertion** puts a surface among the curated slots at the point the curated sequence would
put it — Acquisitions (§ Menu set and ordering, slot 5) was inserted above Market on Ben's
call (2026-08-29), because buying a firm reads before the market it is priced from.
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
| 1 | **Corporation overview** (dashboard) | 0 | the player corporation at a glance |
| 2 | **Budget** | 2 | Budget ([A4]) |
| 3 | **Construction** | 3 | Infrastructure |
| 4 | **Research** *(provisionally hosts the tech-tree mock)* | 7 | Research |
| 5 | **Acquisitions** | 2 | Finance ([A4]) — whole-firm buyout (`../economy/FINANCE.md` § Whole-firm acquisition) |
| 6 | **Market Ledger** | 1 | Trade ([A4]) |
| 7 | **Corp. Strategy** *(reserved)* | 8 | Policy |
| 8 | **Diplomacy** *(provisionally hosts the all-corporations balance table)* | 9 | Diplomacy |
| 9 | **History** | 6 | Environment |

Three slots are **named more broadly than their source ledger** — Budget (not Balance Ledger),
Corp. Strategy (not Policy), History (not Tile Ledger); each widening is settled by Q&A
(2026-06-15) and recorded per-slot below.

Notes on the mapping:

- **Resources, Supply, Conflict and Workforce/Population do not get their own slot.** Resource
  detail lives in the Market ledger and the Selection element; Supply folds into
  Construction/Market; Conflict has no broad ledger; the labour read is a roll-up on the
  Corporation overview dashboard (slot 1) rather than a rail subject of its own. The rail scales
  with *systems that have a broad surface*, per the menus-are-broad-ledgers rule.

- **Slot 5 — Acquisitions (Ben, 2026-08-29).** Which firms may be bought outright and at what
  price, plus a full-canvas fold-out over every corporation's filed return. It **earns a slot on
  the menus-are-broad-ledgers test**: an overview across the whole field of firms, not a targeted
  action on one — the targeted read of a single corporation remains the Selection element's.

  It is **inserted above Market rather than appended**, which shifts every slot from Market down
  by one. Buying a firm is the largest single thing a player's money does, and it reads before
  the market it is priced from; appending it instead would have placed a curated player system
  below the developer/observability tail and taught the rail's order to mean nothing.

  **The surface is small, and that is a generated fact rather than a design gap.** A buyout needs
  a target that is `publicly_held` *and* has filed (`../economy/FINANCE.md` § Whole-firm
  acquisition), and ownership class is generated from the home region's industrialisation
  timing — so most firms are `closed` and publish nothing. The ledger therefore states the size
  of the field it is drawing ("N of M firms file and can be priced") rather than letting a short
  list read as a defect. Its two groups — **Purchasable** (priced within the player's balance,
  carrying the Buy press) and **Possible** (priced, beyond it today, deliberately carrying no
  press) — exist for that contrast: the second group is how a player sees what the next rung
  costs.

  **Two doors reach it**: this slot, and the Company lens's click destination (`LENSES.md`
  § Company lens) — a background firm's holdings resolve through to the firm, and the question a
  player has about a background firm is whether they can buy it.
- **Slot 1 — Corporation ledger (BL-248, corporation dashboard; BL-691, corp: how am I doing).**
  Its question is **"how well am I doing?"**, and it holds **one card, Balance**, folded out into
  the shell column (`src/ui/corporation_dashboard.{hpp,cpp}`). The card **rests as one verdict
  line** (`±X/qtr, balance Y`) **over its chart** — a two-column graph, earnings against the
  quarter's expenses stacked by flow, with the net beneath it. `›` gives it the canvas
  (BL-214, drill-through; `LAYOUT.md` § Drill-through); there is no expand-in-place control,
  because the card already shows its content at rest. There is no chart question log on the card
  (NR-018; see `LAYOUT.md` § The chart question log).

  **Three cards went, and where their questions went** (Ben, 2026-08-29): **Production** and
  **Workforce** are answered by the **Construction ledger's Buildings tab**, which holds the
  estate *and* the per-building levers; **Trade** by the **Market** and **Convoys** ledgers. The
  per-item drills went with them — with one card there is no item list to drill, and the card's
  subject is its own flows. Every figure is derived from the live world and the last
  `economy_report`. The chart is `ui::charts::draw_stacked_columns`, shared with the building
  card's Revenue / Expenses graph at the building grain.

  **"Balance" here is a sub-header, not a surface name** (Ben, 2026-08-29): the ledger is called
  Corporation, and the overlap with slot 2's **Budget** is explicitly accepted rather than
  tolerated, with Budget to be revisited. See `ledgers/corporation.md` § Slot 1.

  > **The MVP four, and what became of them (Ben, 2026-07-31).** An earlier cut named **balance +
  > last-tick delta**, a **holdings roll-up**, **workforce contention** and **alerts**; Ben chose
  > four roll-ups over those when asked directly, and the 2026-08-29 cut then reduced those four
  > to one. The MVP's **click-through into the relevant per-system ledger** survives as the
  > **host axis** (`[>]` / `focus_on_entity`), orthogonal per BL-214's three-axis model. There is
  > no separate **alert** mechanism here: a negative net reads as a red verdict on the card,
  > which is the same signal without a second mechanism to maintain (player alerts as a system
  > are BL-261, player alerts).

  The **rival-field comparison** — corporations grouped by stance, with the stance verbs on each
  row — lives on **slot 9** (Diplomacy), not here. That slot stopped being provisional when its
  surface became the diplomatic read its label always promised; the financial half it used to
  carry moved to the Acquisitions ledger's profitability fold-out.

- **Buildings have no dedicated ledger (settled 2026-06-15, [F4]).** A standalone buildings
  overview is more "good to know" than goal-driving, so it earns no reserved slot. Buildings are
  read in the two places a player cares about them: **own buildings** ("good for me") in the
  **Corporation overview dashboard** (slot 1); **competitors' buildings** ("competition") in the
  **Market Ledger**. Slot 3 is construction *in progress*, not a buildings inventory.
- **Corp. Strategy** is the one reserved placeholder. **Diplomacy** is no longer reserved in name
  only — it hosts a real stance surface. Both follow the *ledgers-start-closed* convention above.
- **Scope-widening names (settled by Q&A 2026-06-15).**
  — **Convoys** (slot 7): **cargo in transit** — one row per convoy in flight, its route, its ETA
    in qtr and the haulage it has cost. Added 2026-08-29 on Ben's call, lifted out of the Market
    Ledger where it had been a third tab. It was never a market question: `MARKETS.md` owns
    clearing and the order book, and a convoy is `SUPPLY.md`'s subject — *Logistics is the road,
    Supply is the traffic*. Placing it directly after Market keeps the commercial run of the rail
    together (Acquisitions, Market, Convoys) while giving the logistics read its own door and its
    own lens: a tab strip can arm only one lens, and the old third tab always got the price wash
    when it wanted `supply_routes`. It draws `icons::convoy`, the marker the canvas already uses
    for cargo in transit, so slot and canvas share a silhouette. Design: `docs/ui/ledgers/convoys.md`.
  — **Budget** (slot 2): the full **Budget-system** surface — income vs expenditure broken down
    (sales, input purchases, maintenance, wages) **plus budget allocation** across research /
    military / workforce contracts, not just the running balance. Same money-loop data as the
    [A4] "Balance Ledger", wider remit.
  — **Corp. Strategy** (slot 8): a **standing-strategy** surface — the player's "laws" / pre-set
    options, with **wage levels** and **military posture** the archetypal levers, and possibly a
    goal / quest system later. Scope deliberately left **open** beyond the standing-rules core.
  — **History** (slot 10): **generation history**, not a live event log (BL-211, player-facing
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
