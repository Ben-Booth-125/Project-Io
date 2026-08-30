#include "nav_pane.hpp"

#include "icons.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace ui {

namespace {

/// A rail slot's hover text: the slot's name, then one line saying what the
/// system is FOR (BL-174). The rail is icon-only per LAYOUT.md container 8 — at
/// 56 px there is no room for a label without silently truncating it — so the
/// tooltip is where a slot explains itself, and it is the only place it can.
///
/// Drawn through BeginItemTooltip + PushTextWrapPos rather than SetItemTooltip:
/// container 8 declares "tooltips wrap", but SetItemTooltip sizes the box to the
/// text and never wraps, so the rail did not implement its own stated policy.
void slot_tooltip(const char* title, const char* blurb, bool reserved)
{
    if (!ImGui::BeginItemTooltip())
        return;
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 16.0f);
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::TextWrapped("%s", blurb);
    if (reserved)
        ImGui::TextDisabled("Not yet built.");
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

/// Where each rail slot was drawn on the last frame, in slot order (index 0 =
/// slot 1). File-static rather than ui_state: it is a harness affordance owned by
/// the rail, and no game code reads it.
std::vector<ImVec2> g_slot_centres;

} // namespace

bool nav_slot_centre(int slot, float& out_x, float& out_y)
{
    if (slot < 1 || static_cast<std::size_t>(slot) > g_slot_centres.size())
        return false;
    const ImVec2 c = g_slot_centres[static_cast<std::size_t>(slot - 1)];
    out_x = c.x;
    out_y = c.y;
    return true;
}

/// Close every nav-pane panel so at most one is open at a time. Call before
/// toggling a slot ON; skip when toggling OFF (the toggle handles that itself).
void close_all_panels(ui_state& state)
{
    state.show_corporation_panel = false;
    state.show_corporations_table = false;
    state.show_balance_ledger    = false;
    state.show_market_ledger     = false;
    state.show_convoys_ledger    = false; // Convoys ledger (BL-689), slot 7
    state.show_construction_panel = false;
    state.show_tile_ledger       = false;
    state.show_generation_ledger = false;
    // BL-666: the two OWNER-SURFACE SUBJECTS clear with the panels they aim.
    // Both are "which firm is this surface about", and a subject that outlives
    // its surface is worse than no subject at all: the dossier field takes strict
    // priority over `selected_entity` in the corporations table's row highlight,
    // so a stale one pins the highlight to a firm the player has moved off and
    // makes a row click read as doing nothing.
    state.selected_company             = null_entity;
    state.selected_corporation_dossier = null_entity;
    state.show_company_ledger    = false; // Company ledger (BL-666) — same shape: a column
                                          // occupant reached by a canvas click, no rail slot
    state.show_decision_feed     = false; // AI decision feed (BL-407)
    state.show_strategy_readout  = false; // Strategy readout (BL-411)
    state.show_acquisitions_ledger = false; // Acquisitions ledger (slot 5)
    // BL-310 round 4: the tech-tree era-selector menu is now a real shell-
    // column occupant (draw_tech_tree_menu), so it must yield to every other
    // ledger the same way they yield to it — its canvas takeover closes with it.
    state.show_tech_tree         = false;
}

bool any_panel_open(const ui_state& state)
{
    return state.show_corporation_panel || state.show_corporations_table ||
           state.show_balance_ledger    ||
           state.show_market_ledger     || state.show_convoys_ledger ||
           state.show_construction_panel ||
           state.show_tile_ledger       ||
           state.show_generation_ledger || state.show_tech_tree ||
           state.show_decision_feed     || state.show_strategy_readout ||
           state.show_acquisitions_ledger ||
           // Company ledger (BL-666). Counted here even though it has no rail
           // slot, because its only consumer asks "does something already own
           // the fold-out column?" — and it does. Leaving it out would let the
           // tile build ledger draw over it.
           state.show_company_ledger;
}

void draw_nav_pane(ui_state& state, float top_offset)
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    ImGui::SetNextWindowPos({0.0f, top_offset});
    ImGui::SetNextWindowSize({nav_pane_width, disp.y - top_offset});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    // Tight padding so square icon slots fill the narrow rail.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{6.0f, 8.0f});
    ImGui::Begin("##nav_pane", nullptr, flags);

    // TEN curated slots from MENU.md § Menu set and ordering, then a
    // three-slot developer/observability tail (11-13). The tail is the foot of
    // the rail; nothing is appended below it.
    // Live slots toggle their panel. Corp. Strategy is the one still-disabled
    // slot, and it carries its OWN glyph so the rail teaches the shape of the
    // game rather than showing a row of identical blanks (BL-174). Research and
    // Diplomacy are unbuilt subjects whose slots provisionally host a homeless
    // surface — the tech-tree mock (BL-310) and the corporations table (NR-012)
    // — keeping their real subject's name and glyph.
    //
    // BL-174 dropped a disabled placeholder that had no glyph and no tooltip, so
    // it was pure noise a new player could not interpret.
    //
    // Slot 11 is the opposite of what BL-174 removed: a LIVE slot with its own
    // glyph and tooltip, hosting the Generation Ledger (BL-303). It heads the
    // tail deliberately — it is a developer tuning surface rather than a player
    // system, so it sits after MENU.md's curated player slots rather than
    // displacing any of them.
    //
    // Slot 12 (BL-407, the AI decision feed) follows the same precedent: an
    // observability surface, not a player system, so it takes the tail rather
    // than displacing any of the curated player slots. The tail is where the
    // developer/observability surfaces live, which is a legible rule — keep new
    // ones going there rather than interleaving them.
    //
    // Slot 13 (BL-411, the Strategy readout) is the third tail occupant under
    // that rule: the feed's aggregate companion — verb mix, spend buckets and
    // reason tally per corp, the SHAPE of a run where the feed lists the moves.
    // It is the LAST slot; see MENU.md's slot table.
    //
    // ── ACQUISITIONS IN, ECONOMY OUT (Ben, 2026-08-29) ────────────────────
    // Acquisitions is INSERTED at slot 5, above Market. That is an insertion
    // rather than another append, and Ben's call, because the ledger is a
    // *curated player system* and appending it would have put it after the
    // developer/observability tail — teaching the rail that a player system sits
    // below the tuning surfaces. Above Market is where it belongs on its own
    // terms too: buying a firm is the largest single thing a player's money
    // does, and it reads before the market it is priced from.
    //
    // The Economy panel is GONE, not relocated: each of its three views turned
    // out to be answered better elsewhere, and two of them disclosed rival
    // figures the competitor-visibility rule forbids. It had provisionally
    // occupied slot 3 under the name of its unbuilt subject, Workforce; with
    // the panel deleted that subject has no occupant and no reason to hold a
    // slot, so Construction takes 3 and everything below shifts up one.
    //
    // ── CONVOYS IN AT SLOT 7 (Ben, 2026-08-29: "give convoys a slot, put it
    // after market") ──────────────────────────────────────────────────────────
    // A second INSERTION rather than an append, for the same reason Acquisitions
    // was one: Convoys is a curated player system, and appending it would have
    // put it below the developer/observability tail. Directly after Market keeps
    // the commercial run of the rail together — Acquisitions, Market, Convoys —
    // while giving the logistics read its own door and its own lens.
    //
    // It was the Market ledger's third tab and had to leave: the Goods
    // flattening (BL-686) deletes that tab strip, so without a slot Convoys is a
    // regression. Everything from Corp. Strategy down shifts one — Corp.
    // Strategy 8, Diplomacy 9, History 10, the developer tail 11-13. See
    // MENU.md's slot table, which already describes this.
    //
    // ── CONTRACTS OUT (BL-693) ────────────────────────────────────────────────
    // The mercenary contract — the SELL side of CONTRACTS.md — is retired, so
    // its ledger and its slot go with it. It had been APPENDED after the
    // developer tail at 14 rather than inserted, which is why its removal is the
    // one rail change of this sprint that renumbers nothing: every slot below it
    // keeps the number it had. Procurement, the BUY side, is untouched — it has
    // never had a rail slot and CONCEPT.md's player identity rests on it.
    //
    // The rail is thirteen slots: ten curated, plus the 11-13 developer tail.
    constexpr int tab_count = 13;

    // Square slots; Selectable treats a nonzero size as literal, so derive the
    // rail width explicitly rather than passing -1.
    // THE RAIL MUST FIT ITS SLOT COUNT AT THE 1280x720 FLOOR, and at fourteen
    // slots it no longer did by default. Measured 2026-08-30, before this clamp:
    // the pitch was a fixed 48 px (a 44 px square plus 4 px spacing), slot 1's
    // centre sat at y=122, and the fourteenth landed at y=746 against a 720 px
    // screen — 26 px past the bottom edge. That slot was DRAWN and unreachable;
    // ImGui's hit-test rejects a press outside the window, so it did nothing at
    // all. At 1920x1080 the identical layout was fine, which is why a capture at
    // one resolution could never have shown it.
    //
    // The count has since come back down, so the clamp does not currently bind
    // at 720p. It stays because the failure it prevents is a property of the
    // FIXED pitch, not of any particular count — `nav_rail_fit.lua` is the check.
    //
    // So the square is derived from the height the rail actually has, CAPPED at
    // the width (its natural size). Nothing changes where there is room — at
    // 1080p the cap binds and the rail is pixel-identical — and where there is
    // not, the slots compress instead of the last one falling off the screen.
    // That also makes the next insertion safe by construction rather than by
    // someone remembering to re-measure.
    const float avail_x   = ImGui::GetContentRegionAvail().x;
    const float avail_y   = ImGui::GetContentRegionAvail().y;
    const float spacing_y = ImGui::GetStyle().ItemSpacing.y;
    const float fit_size  = avail_y / static_cast<float>(tab_count) - spacing_y;
    const float slot_size = std::max(16.0f, std::min(avail_x, fit_size));
    ImDrawList* dl        = ImGui::GetWindowDrawList();

    g_slot_centres.clear();
    g_slot_centres.reserve(static_cast<std::size_t>(tab_count));

    for (int slot = 1; slot <= tab_count; ++slot)
    {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        // Recorded for `nav_slot_centre` — the drawn position, so a verify press
        // exercises the real slot->surface mapping rather than a recomputation of
        // it (NR-716).
        g_slot_centres.push_back({p0.x + slot_size * 0.5f, p0.y + slot_size * 0.5f});

        char id[16];
        std::snprintf(id, sizeof(id), "##nav%d", slot);

        switch (slot)
        {
        case 1: // Corporation overview (BL-022)
            if (ImGui::Selectable(id, state.show_corporation_panel, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_corporation_panel;
                close_all_panels(state);
                state.show_corporation_panel = !was_open;
            }
            slot_tooltip("Corporation overview", "Your corporation at a glance - balance, holdings, alerts.", false);
            break;
        case 2: // Budget (BL-028)
            if (ImGui::Selectable(id, state.show_balance_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_balance_ledger;
                close_all_panels(state);
                state.show_balance_ledger = !was_open;
            }
            slot_tooltip("Budget", "Income, costs, and where the money went.", false);
            break;
        // Construction moves UP to slot 3 (Ben, 2026-08-29). It sits third
        // because it is what the player does with the money slots 1 and 2 have
        // just accounted for — own, budget, build — and because the slot's
        // former occupant was a provisional host for a panel that no longer
        // exists.
        case 3: // Construction (BL-029, renamed BL-143)
            if (ImGui::Selectable(id, state.show_construction_panel, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_construction_panel;
                close_all_panels(state);
                state.show_construction_panel = !was_open;
            }
            slot_tooltip("Construction", "What you are building now, and the buildings you own.", false);
            break;
        case 4: // Research (BL-310, 2026-08-06): opens the F9 mock tech-tree
                // viewer, same flag F9 drives. Was a hard-disabled placeholder;
                // the real research system stays post-prototype (BL-087
                // resolution 6) — this slot shows the design mock, not gameplay.
            if (ImGui::Selectable(id, state.show_tech_tree, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_tech_tree;
                close_all_panels(state);
                state.show_tech_tree = !was_open;
            }
            slot_tooltip("Research", "Technology unlocks and the route to space (design mock).", false);
            break;
        case 5: // Acquisitions ledger — inserted above Market (Ben, 2026-08-29)
            if (ImGui::Selectable(id, state.show_acquisitions_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_acquisitions_ledger;
                close_all_panels(state);
                state.show_acquisitions_ledger = !was_open;
            }
            slot_tooltip("Acquisitions",
                         "Which firms you can buy outright, and what they cost.", false);
            break;
        case 6: // Market Ledger (BL-027)
            if (ImGui::Selectable(id, state.show_market_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_market_ledger;
                close_all_panels(state);
                state.show_market_ledger = !was_open;
                // ARM THE PRICE WASH ON OPEN (market.md § 3; Ben, 2026-08-30:
                // "when we open the market ledger, we should also activate the
                // market lens"). The catchment tint is the map twin of this list
                // — LENSES.md's routing table already sends a click on it BACK
                // here, so this closes a loop that was only ever built one way.
                //
                // Fixed across both views, not sub-view-following: Goods and
                // Trades are both single-market reads the wash reinforces. Armed
                // only on OPEN, and never disarmed on close — slot 7's rule,
                // followed deliberately rather than re-decided (NR-722 is still
                // the open question about what that rule SHOULD be).
                if (state.show_market_ledger)
                    state.overlay = overlay_mode::market;
            }
            slot_tooltip("Market Ledger", "Prices, supply and demand on the bodies you trade on.", false);
            break;
        case 7: // Convoys ledger (BL-689) — inserted directly after Market
            if (ImGui::Selectable(id, state.show_convoys_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_convoys_ledger;
                close_all_panels(state);
                state.show_convoys_ledger = !was_open;
                // ARM THE LANE OVERLAY ON OPEN (convoys.md § 3). The list and the
                // lens answer the same question in two registers, and this
                // pairing is the argument for the split: a tab strip arms one
                // lens for every tab, so hosting Convoys in the Market ledger
                // armed the price wash instead. Armed only on OPEN — closing the
                // ledger leaves the player's lens where they left it rather than
                // yanking the canvas back.
                if (state.show_convoys_ledger)
                    state.overlay = overlay_mode::supply_routes;
            }
            slot_tooltip("Convoys",
                         "What is on its way, and what it is costing you.", false);
            break;
        case 8: // Corp. Strategy — placeholder
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
            slot_tooltip("Corp. Strategy", "Standing policy - wage levels, military posture.", true);
            break;
        // Slot 9 STOPPED BEING PROVISIONAL on 2026-08-29 (it was slot 8 until
        // Convoys was inserted at 7). It used to host the
        // all-corporations balance table as a stand-in, and the note here said the
        // slot kept its real subject's name so the rail would not start teaching
        // "Diplomacy = a balance table". BL-674 (diplomacy by stance) made the
        // surface match the label instead: the list is filtered to corporations,
        // grouped Friends / Hostile / Neutral, and each row carries the stance
        // verbs. The financial half it used to carry moved to the Acquisitions
        // ledger's profitability fold-out, which is why this could stop being a
        // balance table at all.
        case 9: // Diplomacy — the stance-grouped corporations list (BL-674)
            if (ImGui::Selectable(id, state.show_corporations_table, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_corporations_table;
                close_all_panels(state);
                state.show_corporations_table = !was_open;
            }
            slot_tooltip("Diplomacy", "Where you stand with each corporation - friends, hostiles, and the rest.", false);
            break;
        case 10: // History (Tile Ledger lives here per MENU.md renaming)
            if (ImGui::Selectable(id, state.show_tile_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_tile_ledger;
                close_all_panels(state);
                state.show_tile_ledger = !was_open;
            }
            slot_tooltip("History", "How this world was generated - its biography and its numbers.", false);
            break;
        case 11: // Generation Ledger (BL-303) — the developer tuning surface
            if (ImGui::Selectable(id, state.show_generation_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_generation_ledger;
                close_all_panels(state);
                state.show_generation_ledger = !was_open;
            }
            slot_tooltip("Generation Ledger", "Why a tile generated as it did - the per-pass derivation and the body's histograms.", false);
            break;
        case 12: // AI decisions (BL-407) — the scorer's rationale, finally readable
            if (ImGui::Selectable(id, state.show_decision_feed, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_decision_feed;
                close_all_panels(state);
                state.show_decision_feed = !was_open;
            }
            slot_tooltip("AI decisions", "What the rival corporations decided, and how close the call was.", false);
            break;
        case 13: // Strategy readout (BL-411) — the shape of each corp's run
            if (ImGui::Selectable(id, state.show_strategy_readout, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_strategy_readout;
                close_all_panels(state);
                state.show_strategy_readout = !was_open;
            }
            slot_tooltip("Strategy readout", "What strategy is emerging - each corporation's decision mix, spend priorities and reasons over the recent run.", false);
            break;
        default: // Unreachable — tab_count is 13 and every slot is handled above.
            break;
        }

        // Glyph centred on the slot. Every slot draws its OWN glyph (BL-174):
        // live slots in the bright stroke, reserved slots in the dim one so
        // "not yet available" is carried by colour while the shape still says
        // WHICH system the slot is for. The tooltips ("Research (coming)", …)
        // already named them; the glyphs now agree.
        const ImVec2 centre = {p0.x + slot_size * 0.5f, p0.y + slot_size * 0.5f};
        const float  r      = slot_size * 0.30f;
        const ImU32  dim    = IM_COL32(110, 116, 132, 255);
        const ImU32  closed = IM_COL32(225, 228, 235, 255);
        const ImU32  open   = palette::selection;

        // Which system am I in? The Selectable's ImGuiCol_Header fill is nearly
        // invisible, so the OPEN slot lights its glyph in the selection accent —
        // the same idiom the minimap lens strip already uses for its active lens,
        // so the two icon strips read as one vocabulary rather than two (BL-174).
        const auto lit = [&](bool is_open) { return is_open ? open : closed; };

        switch (slot)
        {
        case 1: icons::corporation(dl, centre, r, lit(state.show_corporation_panel));  break;
        case 2: icons::ledger(dl, centre, r, lit(state.show_balance_ledger));          break;
        // Slot 3 draws the factory, NOT building(processing_facility): that glyph
        // is a plain filled square, and slot 1's corporation seal is the same
        // square with a ~4.6 px dot — at the rail's radius the two were one
        // silhouette, which is exactly the collision BL-174 exists to fix.
        case 3: icons::industry(dl, centre, r, lit(state.show_construction_panel));    break;
        case 4: icons::research(dl, centre, r, lit(state.show_tech_tree)); break;  // Research (BL-310)
        // Slot 5's own glyph: two outlined squares with an arrow from the small
        // into the large — "a firm absorbed whole". A pair is a silhouette no
        // other rail slot draws, which is what BL-174 requires of two LIT slots;
        // it deliberately does NOT clone slot 1's solid corporation seal or
        // slot 6's market bars, the two it sits between and would be confused
        // with at rail radius.
        case 5: icons::acquisition(dl, centre, r, lit(state.show_acquisitions_ledger)); break;
        case 6: icons::market(dl, centre, r, lit(state.show_market_ledger));           break;
        // Slot 7 draws `icons::convoy` — the same rightward chevron the canvases
        // already use to mark cargo in transit, so the rail slot and the thing it
        // lists share a silhouette (convoys.md § 5). No new glyph: BL-174 asks
        // each slot for its own shape, and this one already exists and already
        // means exactly this. It cannot collide with slot 6's market bars or
        // slot 8's dim strategy pennant at rail radius.
        case 7: icons::convoy(dl, centre, r, lit(state.show_convoys_ledger));          break;
        case 8: icons::strategy(dl, centre, r, dim);     break;  // Corp. Strategy (reserved)
        // Slot 9 is live only because it provisionally hosts the corporations table
        // (NR-012); the subject it is NAMED for is still unbuilt, so the glyph lights
        // like any other live slot rather than staying dim.
        case 9: icons::diplomacy(dl, centre, r, lit(state.show_corporations_table)); break;
        case 10: icons::history(dl, centre, r, lit(state.show_tile_ledger));            break;
        // The plate glyph: this ledger's subject is the generated ground itself,
        // and the Continent lens already teaches that shape as "how the surface
        // came to be" rather than as a live economic read.
        case 11: icons::continent(dl, centre, r, lit(state.show_generation_ledger));   break;
        // Slot 12 borrows the strategy glyph, which slot 8 draws DIM as its
        // reserved Corp. Strategy placeholder. The two never collide visually —
        // one is always dim, one lights — and the shape is right: this ledger's
        // subject is exactly the strategic decision that slot is reserved for,
        // read rather than set. Revisit if slot 8 is ever built.
        case 12: icons::strategy(dl, centre, r, lit(state.show_decision_feed));        break;
        // The readout gets its OWN glyph (the axis-and-tally-bars readout mark)
        // rather than a third borrow of `strategy` — slot 12 already lights that
        // pennant, and two lit slots sharing a silhouette is exactly the
        // collision BL-174 exists to prevent.
        case 13: icons::readout(dl, centre, r, lit(state.show_strategy_readout));      break;
        default: icons::placeholder(dl, centre, r, dim); break;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace ui
