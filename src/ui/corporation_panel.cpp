#include "corporation_panel.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122)
#include "presentation.hpp"

#include "world/corp_command.hpp" // BL-449: the stance presses issue corp_command records
#include "world/stance.hpp"       // BL-448: is_hostile / are_friends / has_pending_friendship_offer

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace ui {

namespace {

/// The rows the last draw actually emitted, with each control's screen rect. Read
/// back by the verify seam (`corporation_panel_last_rows`) — see the header for why
/// a capture cannot stand in for it.
std::vector<corp_panel_row> g_rows;

/// Which row's action strip is open, and which row is mid-confirm on a hostility
/// declaration. Both are purely presentational, ephemeral, and single-valued: the
/// column is ~324 px wide, so one open strip at a time is the only arrangement that
/// leaves the strip room to draw its presses at full width. They are panel-local
/// rather than `ui_state` members because nothing outside this file reads them and
/// nothing in the simulation depends on them.
entity_id g_expanded_row    = null_entity;
entity_id g_confirm_hostile = null_entity;

/// The firm's ownership class as one display word. UI-local on purpose: this is a
/// presentation string for the non-filing tooltip, not a world-layer fact. The enumerators
/// carry a trailing qualifier because `public`/`private` are C++ keywords; the design's
/// words for them are *public*, *private* and *closed* (components.hpp).
const char* ownership_class_label(ownership_class oc)
{
    switch (oc)
    {
        case ownership_class::publicly_held:  return "public";
        case ownership_class::privately_held: return "private";
        case ownership_class::closed:         return "closed";
    }
    return "closed";
}

/// WHICH WAY the hostility runs, as one line. Hostility is DIRECTED
/// (`RELATIONS.md` § 1 Stance: *"a corp can be at war and not know it yet"*), so a
/// group that merely says "hostile" throws away the half of the fact the player acts
/// on — whether they started it, or are the one being interdicted. `is_hostile` is
/// asked twice, once per direction, and never collapsed with `are_friends` behind a
/// single accessor (invariant 3 of the same section).
const char* hostility_direction(bool out, bool in)
{
    if (out && in) return "hostile both ways";
    if (out)       return "you declared hostility";
    if (in)        return "hostility declared on you";
    return "";
}

/// Centre of the item just submitted, in ImGui screen pixels. What the verify seam
/// aims a synthesised press at — and what makes a clipped control FAIL rather than
/// pass, since ImGui's hit-test rejects a press outside the window's clip rect.
void record_item_centre(float& out_x, float& out_y)
{
    const ImVec2 a = ImGui::GetItemRectMin();
    const ImVec2 b = ImGui::GetItemRectMax();
    out_x = (a.x + b.x) * 0.5f;
    out_y = (a.y + b.y) * 0.5f;
}

/// `TextDisabled` that WRAPS. ImGui has no such overload, and the difference is not
/// cosmetic here: a sentence of explanatory prose in a ~310 px column is wider than
/// the column every time, and an unwrapped one is silently cut at the clip rect —
/// where `expect_no_clipping` cannot see it (NR-663). Every prose line this panel
/// draws goes through this, so the column's width decides where it breaks.
void text_disabled_wrapped(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void queue_stance_command(ui_state& s, entity_id player, corp_verb verb, entity_id counterparty)
{
    corp_command cmd;
    cmd.corp         = player;
    cmd.verb         = verb;
    cmd.counterparty = counterparty;
    s.pending_order_commands.push_back(cmd);
}

} // namespace

const std::vector<corp_panel_row>& corporation_panel_last_rows()
{
    return g_rows;
}

void draw_corporation_panel(const world& w, const std::vector<corp_standing>& standings,
                             ui_state& s, bool& open)
{
    g_rows.clear();

    if (!open)
        return;

    // Re-hosted into the shell fold-out column (BL-122); closed via the nav rail.
    if (!ui::foldout_begin("Corporations"))
    {
        ui::foldout_end();
        return;
    }

    // WHAT THIS SURFACE ANSWERS, and why the shape changed.
    //
    // The question is "who am I standing with, and against?" — the diplomacy read. It
    // was previously served by a five-column table (Corporation | Reach | Capital |
    // Share | Stance) over EVERY corporation in the world, and it failed the question
    // twice over:
    //
    //   1. POPULATION. Every background firm (BL-365, background firms) was listed —
    //      eighty-odd rows on the default fixture — and a background firm is not a
    //      party you can stand toward. The handful of named corporations that ARE such
    //      parties were buried among them. Filtering to `!is_background` (plus the
    //      player's own row) is the substantive fix here, not a refinement of one.
    //
    //   2. WIDTH. 62+62+62+220 px of FIXED column width does not fit the shell
    //      fold-out column (`shell_column_width` less the icon rail — ~324 px of
    //      content at 1720 wide, and the floor is 380 px of column at 1280). The
    //      stretch column absorbed the deficit, so the firm's NAME collapsed to a
    //      single letter and "Declare Hostile" clipped to "De...". Nothing here uses a
    //      literal pixel width: every extent is taken from
    //      `ImGui::GetContentRegionAvail()`, which is the host's own budget.
    //
    // The shape that answers it: three collapsing groups — Friends / Hostile /
    // Neutral — each carrying its count, each with an explicit empty state. The empty
    // state is load-bearing rather than polish: Friends is normally empty at campaign
    // start, and an omitted section reads as a missing feature rather than as an
    // answered question.
    //
    // COLUMNS KEPT: the firm's NAME (the thing this surface has never once been able
    // to show) and Capital. Reach and Share are dropped — both are standing figures,
    // both belong to the profitability read, and neither is worth a firm's name at
    // this width. Capital honours FINANCE.md § Disclosure unchanged: exact where the
    // firm files, a dash where it does not, and exact always for the player's own row
    // (a corporation always reads its own books). The bands are retired and none is
    // reintroduced here.
    //
    // THE PRESSES SURVIVE. This ledger is BL-449's (stance needs a surface) only host,
    // so the four stance verbs move out of the deleted 220 px column into the row's
    // own action strip: a disclosure arrow expands the row and the presses draw at the
    // strip's FULL width, stacked. That is what makes them unclippable — a button
    // sized from the available region cannot render past an edge it was measured
    // against. Declare Hostile keeps its confirm; the confirm is inline in the same
    // strip rather than a floating popup, for the same reason.
    //
    // NO DISCOVERY GATE. The old panel hid a rival's stance behind a per-building
    // survey check and printed "Unknown", citing NR-350 (*a hostile declaration stays
    // silent*). RELATIONS.md § 1 Stance OVERTURNS that ruling — "A declaration against
    // the player is SIGNALLED" (Ben, 2026-08-22), recorded there as a reversal with
    // its cost stated. Newest-dated wins, so the gate is gone and a declaration
    // against the player groups and reads immediately. Directedness itself is
    // untouched: it is still shown, per row, as which way the hostility runs.

    const entity_id player = w.player_entity;

    // Population: the named field. `is_player` and `is_background` are never both true
    // (components.hpp), so the player's row falls out of the same predicate.
    struct listed
    {
        entity_id                     corp = null_entity;
        const corporation_component*  cc   = nullptr;
        const corp_standing*          cs   = nullptr;
        corp_stance_group             group = corp_stance_group::neutral;
        bool                          hostile_out = false;
        bool                          hostile_in  = false;
    };

    std::vector<listed> rows;
    rows.reserve(standings.size());

    int background_hidden = 0;

    // Walk `standings` rather than `w.corporations` — it is already in a sorted
    // entity_id walk (standing.hpp), and iterating the unordered_map directly would
    // make the row order vary between runs.
    for (const corp_standing& cs : standings)
    {
        const auto cit = w.corporations.find(cs.corp);
        if (cit == w.corporations.end())
            continue;
        const corporation_component& cc = cit->second;

        if (cc.is_background && !cc.is_player)
        {
            ++background_hidden;
            continue;
        }

        listed row;
        row.corp = cs.corp;
        row.cc   = &cc;
        row.cs   = &cs;

        if (!cs.is_player && player != null_entity)
        {
            row.hostile_out = is_hostile(w, player, cs.corp);
            row.hostile_in  = is_hostile(w, cs.corp, player);
            const bool friends = are_friends(w, player, cs.corp);

            // Hostility is checked FIRST and in both directions. The two tables cannot
            // disagree — declare_hostile dissolves a friendship atomically (invariant 2)
            // — but reading hostility first means a row can never be filed as a friend
            // by a stale friendship row if that invariant were ever broken.
            if (row.hostile_out || row.hostile_in) row.group = corp_stance_group::hostile;
            else if (friends)                      row.group = corp_stance_group::friends;
            else                                   row.group = corp_stance_group::neutral;
        }

        rows.push_back(row);
    }

    int counts[3] = {0, 0, 0};
    for (const listed& r : rows)
        ++counts[static_cast<int>(r.group)];

    const ImGuiStyle& style = ImGui::GetStyle();

    // One row of the list: [caret] [name, stretched] [capital, right-aligned], then
    // the direction line and the action strip when the row is open.
    const auto draw_row = [&](const listed& r) {
        const entity_id id = r.corp;
        const corporation_component& cc = *r.cc;
        const corp_standing&         cs = *r.cs;

        corp_panel_row rec;
        rec.corp          = id;
        rec.name          = cc.name;
        rec.group         = r.group;
        rec.is_player     = cs.is_player;
        rec.is_background = cc.is_background;

        ImGui::PushID(static_cast<int>(id));

        // The budget, taken fresh from the host every row. `avail` is the fold-out
        // column's content width; nothing below is a literal.
        const float avail    = ImGui::GetContentRegionAvail().x;
        const float row_x0   = ImGui::GetCursorPosX();
        const float caret_w  = ImGui::GetFrameHeight();
        const float spacing  = style.ItemSpacing.x;

        char capital[32];
        if (cs.capital_disclosed)
            std::snprintf(capital, sizeof(capital), "%.1f", static_cast<double>(cs.capital_balance));
        else
            std::snprintf(capital, sizeof(capital), "-");
        const float capital_w = ImGui::CalcTextSize(capital).x;

        const bool expanded = (g_expanded_row == id);
        rec.expanded = expanded;

        if (ImGui::ArrowButton("##expand", expanded ? ImGuiDir_Down : ImGuiDir_Right))
        {
            // Disclosure, and a toggle like every other control whose active state is
            // visible: pressing an open row's arrow closes it.
            g_expanded_row    = expanded ? null_entity : id;
            g_confirm_hostile = null_entity;
        }
        record_item_centre(rec.caret_x, rec.caret_y);
        ImGui::SameLine();

        // The name gets everything the other two do not need. It is the field this
        // surface exists to show, so it is the one that absorbs the slack.
        float name_w = avail - caret_w - capital_w - spacing * 2.0f;
        if (name_w < 48.0f)
            name_w = 48.0f;

        // BL-666: the dossier field first, the live selection as the fallback. A canvas
        // press under the Corporation lens names the firm this ledger is ABOUT, and that
        // must outlive the player clicking a row here or selecting something else.
        const bool selected = (s.selected_corporation_dossier != null_entity)
                              ? (s.selected_corporation_dossier == id)
                              : (s.selected_entity == id);

        if (cs.is_player)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::selection));
        const bool name_pressed =
            ImGui::Selectable(cc.name.c_str(), selected, 0, ImVec2(name_w, 0.0f));
        if (cs.is_player)
            ImGui::PopStyleColor();
        record_item_centre(rec.x, rec.y);

        if (name_pressed)
        {
            s.selected_entity = id;
            // Retarget the dossier too, or this press would be overruled by its own
            // highlight: the expression above gives the dossier field strict priority.
            s.selected_corporation_dossier = id;
        }

        // Capital, flushed to the column's right edge.
        ImGui::SameLine(row_x0 + avail - capital_w);
        if (cs.capital_disclosed)
        {
            const ImU32 col = (cs.capital_balance < 0.0f) ? palette::negative : palette::positive;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s", capital);
        }
        else
        {
            ImGui::TextDisabled("%s", capital);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Does not file — %s corporations publish no return.",
                    ownership_class_label(cc.ownership_class));
        }

        ImGui::Indent(caret_w);

        // Which way the hostility runs. Directed, so the group alone does not say it.
        if (r.group == corp_stance_group::hostile)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::negative));
            ImGui::TextUnformatted(hostility_direction(r.hostile_out, r.hostile_in));
            ImGui::PopStyleColor();
        }

        if (expanded)
        {
            const float strip_w = ImGui::GetContentRegionAvail().x;
            const ImVec2 full(strip_w, 0.0f);

            if (cs.is_player || player == null_entity)
            {
                // A corp cannot stance itself — stance.hpp's valid_pair rejects from == to.
                text_disabled_wrapped("Your own corporation.");
            }
            else if (g_confirm_hostile == id)
            {
                // Declare Hostile keeps its confirm (SELECTION.md's Dismantle precedent):
                // not literally irreversible, but not unilaterally reversible by the
                // target either, so it earns the same friction. Inline rather than a
                // floating popup — a popup is the one thing here whose width the column
                // does not budget.
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(palette::negative));
                ImGui::TextWrapped("Declare %s hostile?", cc.name.c_str());
                ImGui::PopStyleColor();
                text_disabled_wrapped("Dissolves any friendship. Not unilaterally reversible.");

                const float half = (strip_w - style.ItemSpacing.x) * 0.5f;
                if (ImGui::Button("Declare", ImVec2(half, 0.0f)))
                {
                    queue_stance_command(s, player, corp_verb::declare_hostile, id);
                    g_confirm_hostile = null_entity;
                }
                record_item_centre(rec.confirm_x, rec.confirm_y);
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(half, 0.0f)))
                    g_confirm_hostile = null_entity;
            }
            else
            {
                const bool friends           = are_friends(w, player, id);
                const bool offer_pending_out = has_pending_friendship_offer(w, player, id);
                const bool offer_pending_in  = has_pending_friendship_offer(w, id, player);

                if (!r.hostile_out)
                {
                    if (ImGui::Button("Declare Hostile", full))
                        g_confirm_hostile = id;
                    record_item_centre(rec.declare_x, rec.declare_y);
                }

                if (!friends && !r.hostile_out)
                {
                    if (offer_pending_in)
                    {
                        // The other firm already offered — answer it.
                        if (ImGui::Button("Accept Friendship", full))
                            queue_stance_command(s, player, corp_verb::accept_friendship, id);
                        record_item_centre(rec.accept_x, rec.accept_y);
                    }
                    else if (offer_pending_out)
                    {
                        ImGui::TextDisabled("Offer sent");
                    }
                    else
                    {
                        if (ImGui::Button("Offer Friendship", full))
                            queue_stance_command(s, player, corp_verb::offer_friendship, id);
                        record_item_centre(rec.offer_x, rec.offer_y);
                    }
                }

                if (r.hostile_out || friends)
                {
                    if (ImGui::Button("Return to Neutral", full))
                        queue_stance_command(s, player, corp_verb::return_to_neutral, id);
                    record_item_centre(rec.neutral_x, rec.neutral_y);
                }
            }
        }

        ImGui::Unindent(caret_w);
        ImGui::PopID();

        g_rows.push_back(std::move(rec));
    };

    // The three groups. `###` keeps each header's ImGui id stable while its visible
    // label carries a count that changes every time a press moves a row.
    //
    // A group header is DISCLOSURE, not a sub-view tab, so the toggle rule's
    // tab clause does not apply to it — collapsing a group does not close the ledger.
    // It is still a toggle in its own right: the header shows its open state and
    // clicking it again reverses it, which is what the rule actually asks for.
    struct group_def { corp_stance_group group; const char* header; const char* empty; };
    static const group_def groups[3] = {
        { corp_stance_group::friends, "Friends (%d)###stance_friends",
          "No friendships. Open a firm's row to offer one." },
        { corp_stance_group::hostile, "Hostile (%d)###stance_hostile",
          "No hostility declared, in either direction." },
        { corp_stance_group::neutral, "Neutral (%d)###stance_neutral",
          "No neutral firms." },
    };

    for (const group_def& g : groups)
    {
        char header[64];
        std::snprintf(header, sizeof(header), g.header, counts[static_cast<int>(g.group)]);

        if (ImGui::CollapsingHeader(header, ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (counts[static_cast<int>(g.group)] == 0)
            {
                // An empty group states its emptiness. Friends is normally empty at
                // campaign start, and a section that simply vanished would read as a
                // missing feature rather than as an answered question.
                text_disabled_wrapped(g.empty);
            }
            else
            {
                for (const listed& r : rows)
                    if (r.group == g.group)
                        draw_row(r);
            }
        }
    }

    ImGui::Separator();
    // The footer names what is NOT listed, so the filter is legible rather than silent.
    ImGui::TextDisabled("%d listed · %d background firms not shown",
        static_cast<int>(rows.size()), background_hidden);

    ui::foldout_end();
}

} // namespace ui
