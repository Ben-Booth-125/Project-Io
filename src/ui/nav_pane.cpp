#include "nav_pane.hpp"

#include "icons.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <cstdio>

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

} // namespace

/// Close every nav-pane panel so at most one is open at a time. Call before
/// toggling a slot ON; skip when toggling OFF (the toggle handles that itself).
void close_all_panels(ui_state& state)
{
    state.show_corporation_panel = false;
    state.show_corporations_table = false;
    state.show_balance_ledger    = false;
    state.show_market_ledger     = false;
    state.show_construction_panel = false;
    state.show_tile_ledger       = false;
    state.show_generation_ledger = false;
    state.show_economy_panel     = false;
    state.show_build_ledger      = false; // tile build ledger (BL-162) is a column occupant too
    state.show_decision_feed     = false; // AI decision feed (BL-407)
    // BL-310 round 4: the tech-tree era-selector menu is now a real shell-
    // column occupant (draw_tech_tree_menu), so it must yield to every other
    // ledger the same way they yield to it — its canvas takeover closes with it.
    state.show_tech_tree         = false;
}

bool any_panel_open(const ui_state& state)
{
    return state.show_corporation_panel || state.show_corporations_table ||
           state.show_balance_ledger    ||
           state.show_market_ledger     || state.show_construction_panel ||
           state.show_tile_ledger       || state.show_economy_panel ||
           state.show_generation_ledger || state.show_tech_tree ||
           state.show_decision_feed;
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

    // Nine slots from MENU.md § Menu set and ordering. Slots 1–9 match the
    // MENU.md curated sequence; live slots toggle their panel. Corp. Strategy is
    // the one still-disabled slot, and it carries its OWN glyph so the rail
    // teaches the shape of the game rather than showing a row of identical
    // blanks (BL-174). Workforce, Research and Diplomacy are unbuilt subjects
    // whose slots provisionally host a homeless surface — the Economy panel
    // (BL-292), the tech-tree mock (BL-310) and the corporations table (NR-012)
    // — keeping their real subject's name and glyph.
    //
    // BL-174 dropped the former slot 10 — a disabled placeholder with no glyph
    // and no tooltip, so it was pure noise a new player could not interpret.
    //
    // Slot 10 is BACK, but as the opposite of what BL-174 removed: a LIVE slot
    // with its own glyph and tooltip, hosting the Generation Ledger (BL-303).
    // It sits last deliberately — it is a developer tuning surface rather than a
    // player system, so it takes the tail of the rail instead of displacing any
    // of MENU.md's curated nine.
    //
    // Slot 11 (BL-407, the AI decision feed) follows the same precedent: an
    // observability surface, not a player system, so it takes the tail rather
    // than displacing any of the curated nine. The tail is now where the
    // developer/observability surfaces live, which is a legible rule — keep new
    // ones going there rather than interleaving them.
    constexpr int tab_count = 11;

    // Square slots; Selectable treats a nonzero size as literal, so derive the
    // rail width explicitly rather than passing -1.
    const float slot_size = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl        = ImGui::GetWindowDrawList();

    for (int slot = 1; slot <= tab_count; ++slot)
    {
        const ImVec2 p0 = ImGui::GetCursorScreenPos();

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
        // Workforce is still unbuilt, but the slot is no longer disabled: it hosts the
        // Economy panel, provisionally. That panel has been drawn every frame with
        // nothing able to open it — no rail slot, no action entry — so a player could
        // not reach it at all (BL-292). Slot 3 is the least-wrong host because the
        // panel's Corps view already carries the labour table this slot is eventually
        // for, and because reusing a reserved slot leaves the rail's length free for
        // BL-094's law/force slots. Same idiom as slot 8: the slot keeps its real
        // subject's name and glyph so the rail does not start teaching
        // "Workforce = the economy tables".
        case 3: // Workforce — provisional host for the Economy panel (BL-292)
            if (ImGui::Selectable(id, state.show_economy_panel, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_economy_panel;
                close_all_panels(state);
                state.show_economy_panel = !was_open;
            }
            slot_tooltip("Workforce", "Not yet built. For now: the Economy panel - corporation balances, labour, holdings and markets.", false);
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
        case 5: // Market Ledger (BL-027)
            if (ImGui::Selectable(id, state.show_market_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_market_ledger;
                close_all_panels(state);
                state.show_market_ledger = !was_open;
            }
            slot_tooltip("Market Ledger", "Prices, supply and demand on the bodies you trade on.", false);
            break;
        case 6: // Building (BL-029, renamed BL-143)
            if (ImGui::Selectable(id, state.show_construction_panel, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_construction_panel;
                close_all_panels(state);
                state.show_construction_panel = !was_open;
            }
            slot_tooltip("Construction", "What you are building now, and the buildings you own.", false);
            break;
        case 7: // Corp. Strategy — placeholder
            ImGui::BeginDisabled();
            ImGui::Selectable(id, false, 0, {slot_size, slot_size});
            ImGui::EndDisabled();
            slot_tooltip("Corp. Strategy", "Standing policy - wage levels, military posture.", true);
            break;
        // Diplomacy is still unbuilt, but the slot is no longer empty: it hosts the
        // all-corporations balance table, provisionally. That table was BL-248's one
        // deletion and Ben restored it (NR-012) — it needs a door, and every other
        // slot is spoken for. Slot 8 is the least-wrong host because the table IS a
        // rival-comparison read, which is what this slot is eventually for. It moves
        // when Diplomacy is designed; the slot stays labelled by its real subject so
        // the rail does not start teaching "Diplomacy = a balance table".
        case 8: // Diplomacy — provisional host for the corporations table (NR-012)
            if (ImGui::Selectable(id, state.show_corporations_table, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_corporations_table;
                close_all_panels(state);
                state.show_corporations_table = !was_open;
            }
            slot_tooltip("Diplomacy", "Not yet built. For now: every corporation's balance, side by side.", false);
            break;
        case 9: // History (Tile Ledger lives here per MENU.md renaming)
            if (ImGui::Selectable(id, state.show_tile_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_tile_ledger;
                close_all_panels(state);
                state.show_tile_ledger = !was_open;
            }
            slot_tooltip("History", "How this world was generated - its biography and its numbers.", false);
            break;
        case 10: // Generation Ledger (BL-303) — the developer tuning surface
            if (ImGui::Selectable(id, state.show_generation_ledger, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_generation_ledger;
                close_all_panels(state);
                state.show_generation_ledger = !was_open;
            }
            slot_tooltip("Generation Ledger", "Why a tile generated as it did - the per-pass derivation and the body's histograms.", false);
            break;
        case 11: // AI decisions (BL-407) — the scorer's rationale, finally readable
            if (ImGui::Selectable(id, state.show_decision_feed, 0, {slot_size, slot_size})) {
                const bool was_open = state.show_decision_feed;
                close_all_panels(state);
                state.show_decision_feed = !was_open;
            }
            slot_tooltip("AI decisions", "What the rival corporations decided, and how close the call was.", false);
            break;
        default: // Unreachable — tab_count is 11 and every slot is handled above.
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
        // Slot 3 is live only because it provisionally hosts the Economy panel
        // (BL-292); the subject it is NAMED for is still unbuilt, so the glyph lights
        // like any other live slot rather than staying dim — same as slot 8.
        case 3: icons::population(dl, centre, r, lit(state.show_economy_panel));        break;
        case 4: icons::research(dl, centre, r, lit(state.show_tech_tree)); break;  // Research (BL-310)
        case 5: icons::market(dl, centre, r, lit(state.show_market_ledger));           break;
        // Slot 6 draws the factory, NOT building(processing_facility): that glyph
        // is a plain filled square, and slot 1's corporation seal is the same
        // square with a ~4.6 px dot — at the rail's radius the two were one
        // silhouette, which is exactly the collision BL-174 exists to fix.
        case 6: icons::industry(dl, centre, r, lit(state.show_construction_panel));    break;
        case 7: icons::strategy(dl, centre, r, dim);     break;  // Corp. Strategy (reserved)
        // Slot 8 is live only because it provisionally hosts the corporations table
        // (NR-012); the subject it is NAMED for is still unbuilt, so the glyph lights
        // like any other live slot rather than staying dim.
        case 8: icons::diplomacy(dl, centre, r, lit(state.show_corporations_table)); break;
        case 9: icons::history(dl, centre, r, lit(state.show_tile_ledger));            break;
        // The plate glyph: this ledger's subject is the generated ground itself,
        // and the Continent lens already teaches that shape as "how the surface
        // came to be" rather than as a live economic read.
        case 10: icons::continent(dl, centre, r, lit(state.show_generation_ledger));   break;
        // Slot 11 borrows the strategy glyph, which slot 7 draws DIM as its
        // reserved Corp. Strategy placeholder. The two never collide visually —
        // one is always dim, one lights — and the shape is right: this ledger's
        // subject is exactly the strategic decision that slot is reserved for,
        // read rather than set. Revisit if slot 7 is ever built.
        case 11: icons::strategy(dl, centre, r, lit(state.show_decision_feed));        break;
        default: icons::placeholder(dl, centre, r, dim); break;
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace ui
