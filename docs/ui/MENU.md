# Project Io — Menus (Navigation Pane)

The **navigation pane** is a fixed, full-height **icon rail** pinned to the left edge of the shell (below the profile, `nav_pane_width` 56 px). It is the home for the game's menus and ledgers. See `LAYOUT.md` for placement.

---

## Structure

- A vertical strip of **fourteen square icon slots** (`nav_pane.cpp`, `tab_count`; the rail's legibility model is BL-174, nav-rail legibility): the **ten curated player slots** below, a **three-slot developer / observability tail** (§ The tail), then **one further player-system slot** (§ The appended player slot — Contracts) after the tail rather than inside it. Of the ten, **six carry their own subject** — Corporation overview, Budget, Acquisitions, Market Ledger, Construction, History. **Three carry a provisional occupant** — a surface standing in for the slot's subject, because that surface would otherwise have no door: Workforce hosts the **Economy panel** (BL-292, economy panel door), Research the **tech-tree design mock** (BL-310, tech-tree mock), Diplomacy the **all-corporations balance table** (NR-012). Each of the three keeps its real subject's name and glyph, lit like any other live slot, so the rail does not teach the wrong vocabulary; the tooltip says what the slot holds for now. **One is reserved** — Corp. Strategy — disabled, but carrying its own dimmed glyph so the rail teaches the shape of the game rather than showing a blank.
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

### The appended player slot — Contracts (BL-576)

The curated player slots (§ Menu set and ordering) and the developer tail (§ The tail,
slots 11–13) were both already full when the Contracts ledger (`docs/economy/CONTRACTS.md`)
needed a door. Contracts is a **broad ledger** by the menus-are-broad-ledgers test — an
overview across every open offer and every contract the player holds, not a targeted
per-entity action — so it earns a slot rather than living only in the Selection element.
It is **not** a developer/observability surface, so it does not belong inside the tail's
own stated character; it is appended as slot 14, the rail's one slot outside both the
curated set and the tail.

**Appending is the default, not the only route.** Acquisitions (§ Menu set and ordering,
slot 5) was *inserted* among the curated slots instead, on Ben's call (2026-08-29), and the
distinction is worth keeping: a surface whose subject is one of the curated systems belongs
in the curated sequence at the point that sequence would put it, while a surface that sits
outside those systems — as Contracts does — is appended. Deciding by which is cheaper to
implement is how a rail stops teaching anything.

| # | Slot | tier-idx | System / source |
|---|---|---|---|
| 14 | **Contracts** (`icons::contract`) | 5 | Contracts ([SYSTEMS.md]) — offers, active contracts and terminal history for the mercenary contract (`docs/economy/CONTRACTS.md`) |

Three views, the standing button-strip split (§ One-question-per-view splits,
`LAYOUT.md`): **Offers** — open `mercenary_offer`s the fog admits, with an Accept press
that opens a force picker over the player's own uncommitted units; **Active** — the
player's own live `mercenary_contract`s, predicate rendered via `condition_text`, with an
Abandon press that shows its reputation cost before it commits; **History** — the
player's terminal-state contracts and what they paid.

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
strict SYSTEMS.md tier order. The curated set is **ten slots**: Exploration is not on the rail (the
comms surface is `CHAT.md`; there is no Explorer, `EXPLORER.md`). The `tier-idx` column records
each slot's position in the SYSTEMS.md tier list, so the curation is auditable.

| # | Slot | tier-idx | System / source |
|---|---|---|---|
| 1 | **Corporation overview** (dashboard) | 0 | the player corporation at a glance |
| 2 | **Budget** | 2 | Budget ([A4]) |
| 3 | **Workforce / Population Ledger** *(provisionally hosts the Economy panel)* | 4 | Workforce ([A4]) / Population ([S4]) |
| 4 | **Research** *(provisionally hosts the tech-tree mock)* | 7 | Research |
| 5 | **Acquisitions** | 2 | Finance ([A4]) — whole-firm buyout (`../economy/FINANCE.md` § Whole-firm acquisition) |
| 6 | **Market Ledger** | 1 | Trade ([A4]) |
| 7 | **Construction** | 3 | Infrastructure |
| 8 | **Corp. Strategy** *(reserved)* | 8 | Policy |
| 9 | **Diplomacy** *(provisionally hosts the all-corporations balance table)* | 9 | Diplomacy |
| 10 | **History** | 6 | Environment |

Three slots are **named more broadly than their source ledger** — Budget (not Balance Ledger),
Corp. Strategy (not Policy), History (not Tile Ledger); each widening is settled by Q&A
(2026-06-15) and recorded per-slot below.

Notes on the mapping:

- **Resources, Supply, and Conflict do not get their own slot.** Resource detail lives in the
  Market ledger and the Selection element; Supply folds into Construction/Market; Conflict has
  no broad ledger. The rail scales with *systems that have a broad surface*, per the
  menus-are-broad-ledgers rule.

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
- **Slot 1 — Corporation overview dashboard (BL-248, corporation dashboard).** Four roll-up
  cards — **Production**, **Trade**, **Workforce**, **Finance** — folded out into the shell
  column (`src/ui/corporation_dashboard.{hpp,cpp}`). Each card **rests as one verdict line**
  and expands, through the shared drill-through chevron (BL-214, drill-through; `LAYOUT.md`
  § Drill-through), to a **full-screen view** carrying its chart and a **per-item drill** with a
  breadcrumb back to the roll-up. There is no chart question log on the cards (NR-018; see
  `LAYOUT.md` § The chart question log).

  The four drills are deliberately four *different shapes*, not one generic detail panel: a
  building's operating economics (through the shared `draw_building_profit` builder), a lane's
  completed convoys and recency, a building's assigned-against-effective labour, and — for
  Finance, whose subject is not per-item — the budget's five flows. Every figure is derived from
  the live world and the last `economy_report`.

  > **Four roll-ups, not the MVP four (Ben, 2026-07-31).** An earlier cut named **balance +
  > last-tick delta**, a **holdings roll-up**, **workforce contention** and **alerts**; Ben chose
  > the four roll-ups over those when asked directly. The MVP's **click-through into the relevant
  > per-system ledger** survives as the **host axis** (`[>]` / `focus_on_entity`), available
  > *alongside* the drill rather than instead of it, the two being orthogonal per BL-214's
  > three-axis model. There is no separate **alert** mechanism on the dashboard: an idle building
  > and a negative net read as red verdicts on their own cards, which is the same signal without
  > a second mechanism to maintain (player alerts as a system are BL-261, player alerts).

  The **all-corporations balance table** — a cross-corp comparison surface, and the same table
  the **Economy panel's Corps view** carries — lives on **slot 9** (Diplomacy, provisionally;
  NR-012), not here. The Economy panel itself has its door on **slot 3**.

- **Buildings have no dedicated ledger (settled 2026-06-15, [F4]).** A standalone buildings
  overview is more "good to know" than goal-driving, so it earns no reserved slot. Buildings are
  read in the two places a player cares about them: **own buildings** ("good for me") in the
  **Corporation overview dashboard** (slot 1); **competitors' buildings** ("competition") in the
  **Market Ledger**. Slot 7 is construction *in progress*, not a buildings inventory.
- **Corp. Strategy** is the one reserved placeholder, and **Diplomacy** is reserved in name only
  (it hosts the corporations table provisionally). Both follow the *ledgers-start-closed* and
  reserved-placeholder conventions above.
- **Scope-widening names (settled by Q&A 2026-06-15).**
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
  — **Construction** (slot 7): **construction-in-progress** — a build-queue / progress surface
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
