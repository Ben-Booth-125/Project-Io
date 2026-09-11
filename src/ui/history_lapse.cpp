/// @file history_lapse.cpp
/// Round 4's time-lapse map and its top-16 board (BL-829 / BL-830).
/// See history_lapse.hpp for what this surface is and what it deliberately
/// refuses to draw.

#include "history_lapse.hpp"

#include "generation_preview.hpp" // preview_pack / preview_substrate — the land mask
#include "presentation.hpp"       // identity colours; they live there, never here
#include "text_fit.hpp"           // the board's name cell, elided AND recorded

#include "world/components.hpp"   // is_water

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <deque>

namespace ui {

namespace {

// The map's non-identity ground. These are BACKDROP, not identity: sea, and land
// nobody has reached yet. Identity colours are ui::palette's and are not
// duplicated here (presentation.hpp owns them).
constexpr ImU32 col_sea      = IM_COL32( 16,  24,  38, 255);
constexpr ImU32 col_void     = IM_COL32( 10,  11,  15, 255);
constexpr ImU32 col_wild     = IM_COL32( 46,  48,  44, 255); ///< Land, unclaimed.
constexpr ImU32 col_frontier = IM_COL32(  8,   9,  12, 200); ///< The line between holders.
constexpr ImU32 col_dim      = IM_COL32(120, 128, 145, 255);
constexpr ImU32 col_bright   = IM_COL32(225, 230, 240, 255);

/// The colour one polity index is drawn in, everywhere on this surface — the map
/// swatch and the board row are the same call, so a row and its territory cannot
/// disagree. `+1` matches the Ages view's own keying (a polity index of 0 is a
/// real polity; `null_entity` is not a colour input).
ImU32 polity_colour(uint16_t owner)
{
    return palette::nation_colour(static_cast<entity_id>(owner + 1));
}

} // namespace

std::string lapse_year_label(int year)
{
    char buf[32];
    if (year < 0) std::snprintf(buf, sizeof buf, "%d BCE", -year);
    else          std::snprintf(buf, sizeof buf, "%d CE",   year);
    return buf;
}

void finish_history_lapse(history_lapse& h, const uint8_t* packed, std::size_t packed_len)
{
    if (h.derived() || h.empty()) return;
    if (h.grid_w <= 0 || h.grid_h <= 0) return;

    const std::size_t n = static_cast<std::size_t>(h.grid_w)
                        * static_cast<std::size_t>(h.grid_h);
    if (packed == nullptr || packed_len != n) return; // the surface is not built yet

    const int gw = h.grid_w, gh = h.grid_h;

    h.tile_region.assign(n, -1);
    h.region_area.assign(h.region_col.size(), 0);

    // A multi-source breadth-first walk from every region anchor at once, over
    // LAND only. Every tile ends on the region whose anchor is nearest by walking
    // distance rather than by straight line — which is the honest measure here,
    // because a region across a strait is not next door however close it looks.
    std::deque<int> frontier;
    for (std::size_t r = 0; r < h.region_col.size(); ++r)
    {
        const int c = h.region_col[r], w = h.region_row[r];
        if (c < 0 || c >= gw || w < 0 || w >= gh) continue;
        const int idx = w * gw + c;
        if (is_water(preview_substrate(packed[static_cast<std::size_t>(idx)]))) continue;
        if (h.tile_region[static_cast<std::size_t>(idx)] >= 0) continue; // two anchors, one tile
        h.tile_region[static_cast<std::size_t>(idx)] = static_cast<int32_t>(r);
        frontier.push_back(idx);
    }

    while (!frontier.empty())
    {
        const int idx = frontier.front();
        frontier.pop_front();
        const int32_t owner = h.tile_region[static_cast<std::size_t>(idx)];
        const int col = idx % gw, row = idx / gw;

        // Columns WRAP as the equator does; rows do not. The same rule the
        // world's own grid keeps (hard_coded_world.hpp § the homeworld grid).
        const int steps[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
        for (const auto& s : steps)
        {
            int nc = col + s[0], nr = row + s[1];
            if (nr < 0 || nr >= gh) continue;
            if (nc < 0)   nc += gw;
            if (nc >= gw) nc -= gw;
            const std::size_t ni = static_cast<std::size_t>(nr * gw + nc);
            if (h.tile_region[ni] >= 0) continue;
            if (is_water(preview_substrate(packed[ni]))) continue;
            h.tile_region[ni] = owner;
            frontier.push_back(static_cast<int>(ni));
        }
    }

    for (std::size_t i = 0; i < n; ++i)
    {
        const int32_t r = h.tile_region[i];
        if (r < 0) continue;
        ++h.region_area[static_cast<std::size_t>(r)];
        ++h.claimed_land;
    }

    // Every polity's seat: the first region the record ever shows it holding.
    // `changes` is in year order, so the first appearance IS the founding claim.
    for (const owner_change& c : h.lapse.changes)
    {
        if (c.owner == owner_none) continue;
        if (h.polity_seat.size() <= c.owner)
            h.polity_seat.resize(static_cast<std::size_t>(c.owner) + 1, -1);
        if (h.polity_seat[c.owner] < 0)
            h.polity_seat[c.owner] = static_cast<int32_t>(c.region);
    }
}

// ---------------------------------------------------------------------------
// The map
// ---------------------------------------------------------------------------

void draw_lapse_map(const history_lapse& h, const std::vector<uint16_t>& slice,
                    int year)
{
    const ImVec2 avail  = ImGui::GetContentRegionAvail();
    ImDrawList*  dl     = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();

    if (avail.x <= 1.0f || avail.y <= 1.0f) return;

    if (!h.derived())
    {
        // Nothing painted before the record lands — the shell's own background
        // is the right empty state, and flooding the pane with `col_void` was
        // the same black frame the drawn map has now dropped.
        ImGui::Dummy(avail);
        return;
    }

    const int gw = h.grid_w, gh = h.grid_h;

    // Fit the raster into the pane, aspect preserved: a political map stretched
    // to a pane is a map of a different world's shape.
    const float scale = std::min(avail.x / static_cast<float>(gw),
                                 avail.y / static_cast<float>(gh));
    const float mw = scale * static_cast<float>(gw);
    const float mh = scale * static_cast<float>(gh);

    // NO LETTERBOX (Ben, 2026-09-09: "so that our timelapse doesn't contain
    // black bars"). The pane used to be flooded with `col_void` and the map laid
    // on top of it, so a 261x121 raster in a much taller pane wore a black band
    // above and below — a frame around the subject that said nothing. Only the
    // map's own rect is painted now, and the shell's background carries the
    // rest, so the map reads as a map rather than as a picture of one.
    //
    // NUDGED RIGHT (Ben, same): centred in the pane it sat visually left of the
    // space it was given, because the round's left column ends well before the
    // pane begins. The offset is authored rather than derived — it is a framing
    // judgement about this screen, and deriving it from some other quantity
    // would only disguise that.
    constexpr float map_nudge_x = 120.0f;
    const ImVec2 tl{origin.x + (avail.x - mw) * 0.5f + map_nudge_x,
                    origin.y + (avail.y - mh) * 0.5f};

    dl->AddRectFilled(tl, {tl.x + mw, tl.y + mh}, col_sea);

    // One colour per tile, then RUN-MERGED along the row. A political map is long
    // runs of one colour, so this collapses ~31,500 cells to on the order of a
    // thousand rects — which is what keeps the whole map inside ImGui's 16-bit
    // draw indices without dropping to a coarser grid than the frontier deserves.
    std::vector<ImU32> row(static_cast<std::size_t>(gw));
    for (int r = 0; r < gh; ++r)
    {
        for (int c = 0; c < gw; ++c)
        {
            const std::size_t i = static_cast<std::size_t>(r * gw + c);
            const int32_t reg = h.tile_region[i];
            if (reg < 0) { row[static_cast<std::size_t>(c)] = 0; continue; } // sea shows through
            const uint16_t o = (static_cast<std::size_t>(reg) < slice.size())
                                   ? slice[static_cast<std::size_t>(reg)] : owner_none;
            row[static_cast<std::size_t>(c)] = (o == owner_none) ? col_wild : polity_colour(o);
        }

        const float y0 = tl.y + static_cast<float>(r) * scale;
        const float y1 = y0 + scale + 0.5f; // half a pixel of bleed: no seam rows
        int c = 0;
        while (c < gw)
        {
            const ImU32 col = row[static_cast<std::size_t>(c)];
            int e = c + 1;
            while (e < gw && row[static_cast<std::size_t>(e)] == col) ++e;
            if (col != 0)
            {
                dl->AddRectFilled({tl.x + static_cast<float>(c) * scale, y0},
                                  {tl.x + static_cast<float>(e) * scale + 0.5f, y1}, col);
                // The frontier LINE. Twelve identity colours cover many more
                // polities, so two neighbours can share one — without a separating
                // line a border between them would simply not exist on screen, and
                // the border is this round's whole subject.
                if (c > 0 && row[static_cast<std::size_t>(c - 1)] != col
                          && row[static_cast<std::size_t>(c - 1)] != 0)
                    dl->AddLine({tl.x + static_cast<float>(c) * scale, y0},
                                {tl.x + static_cast<float>(c) * scale, y1},
                                col_frontier, 1.0f);
            }
            c = e;
        }
    }

    // The year, over the map's own corner. It is the one thing a watcher needs
    // without looking away from the frontier.
    {
        const std::string label = lapse_year_label(year);
        const ImVec2 ts = ImGui::CalcTextSize(label.c_str());
        const ImVec2 at{tl.x + 8.0f, tl.y + 6.0f};
        dl->AddRectFilled({at.x - 5.0f, at.y - 3.0f},
                          {at.x + ts.x + 5.0f, at.y + ts.y + 3.0f},
                          IM_COL32(8, 9, 12, 190));
        dl->AddText(at, col_bright, label.c_str()); // fit-exempt: a year stamp sized by CalcTextSize
    }

    ImGui::Dummy(avail);
}

// ---------------------------------------------------------------------------
// The board
// ---------------------------------------------------------------------------

namespace {

struct board_row
{
    uint16_t owner   = owner_none;
    int32_t  land    = 0; ///< Tiles held.
    int      regions = 0;
};

/// Total the land each polity holds in one slice, ordered by it.
///
/// SHARE OF LAND is the ordering metric, and it is the honest default for a round
/// about borders (BL-830): it is the one quantity the ownership record alone can
/// answer, and share of LAND rather than of surface — an ocean nobody can hold
/// would otherwise compress every polity into the bottom of the axis.
std::vector<board_row> rank_slice(const history_lapse& h,
                                  const std::vector<uint16_t>& slice)
{
    std::vector<board_row> rows;
    for (std::size_t r = 0; r < slice.size() && r < h.region_area.size(); ++r)
    {
        const uint16_t o = slice[r];
        if (o == owner_none) continue;
        auto it = std::find_if(rows.begin(), rows.end(),
                               [&](const board_row& b) { return b.owner == o; });
        if (it == rows.end()) { rows.push_back({o, 0, 0}); it = rows.end() - 1; }
        it->land += h.region_area[r];
        ++it->regions;
    }
    // Deterministic: land descending, then polity index, so two equal holders do
    // not swap places frame to frame.
    std::sort(rows.begin(), rows.end(), [](const board_row& a, const board_row& b) {
        if (a.land != b.land) return a.land > b.land;
        return a.owner < b.owner;
    });
    return rows;
}

constexpr int k_board_rows = 16;

} // namespace

void draw_lapse_scoreboard(const history_lapse& h,
                           const std::vector<uint16_t>& slice,
                           const std::vector<uint16_t>& lagged)
{
    const std::vector<board_row> now  = rank_slice(h, slice);
    const std::vector<board_row> then = rank_slice(h, lagged);

    int32_t held = 0;
    for (const board_row& b : now) held += b.land;
    if (held <= 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("No ground is held yet. The first claims appear as the "
                           "time-lapse reaches the founding centuries.");
        ImGui::PopStyleColor();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
    ImGui::Text("%d powers hold ground; the top %d are listed.",
                static_cast<int>(now.size()), k_board_rows);
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // POPULATION and MILITARY COLUMNS GO HERE (BL-817, in flight). They need a
    // per-polity sample series the ownership record does not carry; the record is
    // a change list over regions and nothing else. Add them as two columns beside
    // Land, ordered still by Land unless Ben rules otherwise.
    //
    // A RESEARCH COLUMN DOES NOT GO HERE, and that is a standing refusal rather
    // than a deferral: research points accrue from population (BL-822), so the
    // column would restate population under a second name and show a correlation
    // it never measured — on the board Ben is judging the research levers with.
    if (!ImGui::BeginTable("##lapse_board", 4,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        return;

    // "Rgn", not "Regions": the header is drawn by ImGui's own TableHeadersRow,
    // which neither elides nor reports, so a header wider than its column simply
    // clips ("Re...") with nothing recording that it did.
    ImGui::TableSetupColumn("#",    ImGuiTableColumnFlags_WidthFixed, 22.0f);
    ImGui::TableSetupColumn("Seat", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Land", ImGuiTableColumnFlags_WidthFixed, 52.0f);
    ImGui::TableSetupColumn("Rgn",  ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableHeadersRow();

    const int shown = std::min(k_board_rows, static_cast<int>(now.size()));
    for (int i = 0; i < shown; ++i)
    {
        const board_row& b = now[static_cast<std::size_t>(i)];
        ImGui::TableNextRow();

        // Rank, and the movement against the lagged board. A polity CLIMBING INTO
        // the board and DROPPING OUT of it is the whole story of the round, so the
        // entry marker is not decoration — without it a re-ranked list reads as a
        // list that merely reordered.
        int was = -1;
        for (std::size_t j = 0; j < then.size(); ++j)
            if (then[j].owner == b.owner) { was = static_cast<int>(j); break; }
        const bool entered = (was < 0 || was >= k_board_rows);

        ImGui::TableSetColumnIndex(0);
        ImGui::PushStyleColor(ImGuiCol_Text, entered ? col_bright : col_dim);
        ImGui::Text("%d", i + 1);
        ImGui::PopStyleColor();

        ImGui::TableSetColumnIndex(1);
        // MEASURED BEFORE THE SWATCH, not after. `GetContentRegionAvail` inside a
        // table cell is taken from the CURSOR, and the cursor is past the column's
        // right edge once a Dummy + SameLine has run — which reported a NEGATIVE
        // width and failed every row on a column that was fitting fine.
        const float name_avail = ImGui::GetContentRegionAvail().x;
        float swatch_w = 0.0f;
        {
            // The identity swatch: the same colour this polity is painted in on
            // the map, so a row and its territory are matched by eye.
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float  s = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled(
                {p.x, p.y + 2.0f}, {p.x + s * 0.55f, p.y + s - 1.0f}, polity_colour(b.owner));
            swatch_w = s * 0.55f + 5.0f + ImGui::GetStyle().ItemSpacing.x;
            ImGui::Dummy({s * 0.55f + 5.0f, s});
            ImGui::SameLine();
        }
        {
            const int32_t seat = (b.owner < h.polity_seat.size())
                                     ? h.polity_seat[b.owner] : -1;
            const char* nm = (seat >= 0 && static_cast<std::size_t>(seat) < h.region_name.size()
                              && !h.region_name[static_cast<std::size_t>(seat)].empty())
                                 ? h.region_name[static_cast<std::size_t>(seat)].c_str()
                                 : "unnamed seat";
            // A generated seat name in a third-width column is exactly the shape
            // that overruns, so it is drawn through `ui::fit_text`: it elides with
            // the full name a hover away, and — the half that matters here — the
            // draw is RECORDED, so `verify.expect_no_clipping` over this surface
            // measures something instead of passing vacuously (BL-714).
            char cell[192];
            const int delta = entered ? 0 : (was - i);
            if (entered)        std::snprintf(cell, sizeof cell, "%s  *", nm);
            else if (delta > 0) std::snprintf(cell, sizeof cell, "%s  +%d", nm, delta);
            else if (delta < 0) std::snprintf(cell, sizeof cell, "%s  %d",  nm, delta);
            else                std::snprintf(cell, sizeof cell, "%s", nm);

            if (entered) ImGui::PushStyleColor(ImGuiCol_Text, col_bright);
            ui::fit_text(text_box::table_cell, "wizard.round4.seat", cell,
                         std::max(24.0f, name_avail - swatch_w));
            if (entered) ImGui::PopStyleColor();
        }

        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.1f%%", 100.0f * static_cast<float>(b.land)
                                     / static_cast<float>(held));

        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%d", b.regions);
    }
    ImGui::EndTable();

    if (static_cast<int>(now.size()) > shown)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::Text("and %d more below the board.",
                    static_cast<int>(now.size()) - shown);
        ImGui::PopStyleColor();
    }
}


// ---------------------------------------------------------------------------
// BL-891 -- the arc, read off the record
// ---------------------------------------------------------------------------
//
// A ROLLED WORLD CANNOT BE JUDGED BY A SNAPSHOT. BL-890 makes every arrival at
// the wizard a fresh world; without a readout a reroll is a blind redraw,
// because the scoreboard beside this shows who leads RIGHT NOW and says nothing
// about whether an empire ever formed or fell. Ben, 2026-09-10: rolling should
// give "the player the ability to see if their world will contain these types
// of structures".
//
// EVERY DEFINITION HERE IS history_sweep's, deliberately, so the panel and the
// harness cannot drift into disagreeing about the same world.

lapse_arc summarise_lapse_arc(const history_lapse& h)
{
    lapse_arc out;
    const int stride = h.lapse.region_stride > 0 ? h.lapse.region_stride : 1;

    // Per polity: first holding, peak holding, last holding. `polity` ids are
    // sparse, so a map keyed by id would iterate in an order the record does not
    // define — a vector indexed by id keeps this deterministic.
    std::vector<int> first_r, peak_r, last_r;
    for (const polity_sample& s : h.lapse.samples)
    {
        const std::size_t id = s.polity;
        if (id >= first_r.size())
        {
            first_r.resize(id + 1, -1);
            peak_r.resize(id + 1, 0);
            last_r.resize(id + 1, 0);
        }
        const int r = static_cast<int>(s.regions);
        if (first_r[id] < 0) first_r[id] = r;
        if (r > peak_r[id])  peak_r[id]  = r;
        last_r[id] = r;
    }

    int biggest_end = 0, smallest_end = 0;
    for (std::size_t id = 0; id < first_r.size(); ++id)
    {
        if (first_r[id] < 0) continue;       // never seen
        if (peak_r[id] <= 0) continue;       // never held ground
        ++out.polities;

        const int pq = (peak_r[id] * 1000) / stride;
        if (pq > out.peak_share_q) out.peak_share_q = pq;

        if (last_r[id] <= 0) { ++out.eliminated; continue; }

        // THE SWEEP'S OWN SHAPE TEST: doubled AND gained at least three
        // regions, then ended at or under 60% of the peak.
        const bool rose = peak_r[id] >= first_r[id] * 2 && peak_r[id] >= first_r[id] + 3;
        const bool fell = last_r[id] * 100 <= peak_r[id] * 60;
        if (rose && fell) ++out.rose_and_fell;

        if (last_r[id] > biggest_end) biggest_end = last_r[id];
        if (smallest_end == 0 || last_r[id] < smallest_end) smallest_end = last_r[id];
    }
    out.biggest_end_q = (biggest_end * 1000) / stride;
    out.smallest_end  = smallest_end;
    return out;
}

void draw_lapse_arc(const history_lapse& h)
{
    const lapse_arc a = summarise_lapse_arc(h);
    if (a.polities <= 0) return;

    const ImU32 col_dim = IM_COL32(150, 158, 175, 255);
    ImGui::SeparatorText("What happened here");

    // STATED AS PROSE, NOT A TABLE. The player is deciding whether to keep this
    // world, which is a judgement about SHAPE; a table of six numbers makes them
    // do the reading. The numbers are still all present.
    if (a.eliminated == 0 && a.rose_and_fell == 0 && a.peak_share_q < 100)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("A quiet age. %d peoples held ground and none of them "
                           "grew large or was destroyed.", a.polities);
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::Text("%d polities, %d destroyed.", a.polities, a.eliminated);
        ImGui::Text("The largest empire held %d%% of the world; %d rose and fell back.",
                    a.peak_share_q / 10, a.rose_and_fell);
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("At the end the greatest holds %d%%, the least %d region%s.",
                           a.biggest_end_q / 10, a.smallest_end,
                           a.smallest_end == 1 ? "" : "s");
        ImGui::PopStyleColor();
    }
}

} // namespace ui
