#include "header_panel.hpp"

#include "corporation_dashboard.hpp" // BL-1415: nation_operating_net
#include "format.hpp"
#include "presentation.hpp"
#include "text_fit.hpp"

#include "icons.hpp"                 // BL-1472: the theatre glyphs

#include "world/economy_system.hpp" // BL-577: economy_report, for budgets[player].subsidies
#include "world/recipe_registry.hpp"
#include "world/recruit_params.hpp"
#include "world/recruits.hpp"       // BL-1472: recruit_pool / recruit_cap / recruit_quarter_output

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <unordered_map>

namespace ui {

namespace {

header_recruits_record& recruits_record()
{
    static header_recruits_record r;
    return r;
}

/// One theatre's pool as the header reads it (MILITARY.md § Recruits).
struct recruit_readout
{
    int held   = 0; ///< recruits in the nation's pool
    int cap    = 0; ///< 15 x the quarter's nominal output
    int output = 0; ///< recruits its running sources make a quarter
    int rate   = 0; ///< the authored rate per source (bases = output / rate)
};

recruit_readout read_recruit_pool(const world& w, const recipe_registry& reg, entity_id nation,
                                  recruit_theatre t, int rate)
{
    return {recruit_pool(w, nation, t), recruit_cap(w, reg, nation, t),
            recruit_quarter_output(w, reg, nation, t), std::max(0, rate)};
}

/// The SEA pool (MILITARY.md § Recruits, "Ports make sea recruits"). The world's
/// `recruit_theatre::sea` is landing in a parallel lane; until it merges the sea
/// readout stands at 0 / 0 with an empty bar, as HEADER.md says a theatre with no
/// source reads. At merge this body becomes one line:
///   return read_recruit_pool(w, reg, nation, recruit_theatre::sea, reg.military().recruits.sea_per_port);
recruit_readout sea_recruit_readout(const world& w, const recipe_registry& reg, entity_id nation)
{
    (void)w; (void)reg; (void)nation;
    return {};
}

/// BL-1406 (HEADER.md § Contents): the player's money is its NATION's treasury
/// once the world is folded — `purse_of(player_actor)` answers either owner, so
/// an unfolded world (a fixture) still reads the seat corporation's balance.
float player_treasury(const world& w)
{
    const float* p = purse_of(w, player_actor(w));
    return p != nullptr ? *p : 0.0f;
}

/// The last economy tick's OPERATING net for the player nation — BL-1415 (Ben,
/// 2026-10-26, NR-1002 B): the Balance card's own "Net this quarter", read
/// through `nation_operating_net`, so the strip and the card agree. Refunds (the
/// seat's cancelled construction credited back) and flat construction are not
/// operating flows; the treasury's close-less-open would fold both in. False
/// when the nation holds no state-ledger row this tick — no state industry ran —
/// so the caller falls back to the treasury series.
bool ledger_net(const world& w, float& net)
{
    const entity_id nation = player_nation(w);
    if (nation == null_entity) // BL-1407: every world is the nations'
        return false;
    return nation_operating_net(w, nation, net);
}

/// The last economy tick's actual TREASURY MOVEMENT for the player nation — its
/// state-ledger row's close less open, construction and refunds included. The
/// RUNWAY reads this, not the operating NET: "how long the treasury lasts at the
/// current burn" is about cash, and a nation building at +10 operating while
/// construction takes 200 a quarter is burning (BL-1415 review). False with no row.
bool ledger_burn(const world& w, float& movement)
{
    const entity_id nation = player_nation(w);
    if (nation == null_entity)
        return false;
    const auto it = w.state_ledger.find(nation);
    if (it == w.state_ledger.end())
        return false;
    movement = it->second.treasury_close - it->second.treasury_open;
    return true;
}

// ---- BL-1472: the recruit readouts (HEADER.md § Contents, Recruits) ----------------
// Land, air, sea at the top right: each the theatre's glyph, held / cap, a thin bar of
// held over cap beneath in the stat grid's idiom (stat_grid.cpp: a dark track, the
// unit_hp fill). What a pool is, its quarter's output and that a full pool takes none
// is the readout's tooltip.

constexpr float k_recruit_glyph_r  = 8.0f;  ///< glyph half-extent
constexpr float k_recruit_text_gap = 5.0f;  ///< glyph to its numbers
constexpr float k_recruit_gap      = 16.0f; ///< between two readouts
constexpr float k_recruit_lead     = 20.0f; ///< the least room left of the three
constexpr float k_recruit_bar_h    = 3.0f;

const char* const k_recruit_theatre_names[3] = {"Land", "Air", "Sea"};

/// The readout's numbers at fit step @p fit: "120 / 750", "120", or nothing.
std::string recruit_numbers(const recruit_readout& r, int fit)
{
    char buf[48];
    if (fit == 0)      std::snprintf(buf, sizeof buf, "%d / %d", r.held, r.cap);
    else if (fit == 1) std::snprintf(buf, sizeof buf, "%d", r.held);
    else               buf[0] = '\0';
    return buf;
}

float recruit_block_width(const recruit_readout& r, int fit)
{
    const float glyph = 2.0f * k_recruit_glyph_r;
    if (fit >= 2)
        return glyph;
    return glyph + k_recruit_text_gap + ImGui::CalcTextSize(recruit_numbers(r, fit).c_str()).x;
}

float recruit_strip_width(const recruit_readout (&r)[3], int fit)
{
    return recruit_block_width(r[0], fit) + recruit_block_width(r[1], fit) +
           recruit_block_width(r[2], fit) + 2.0f * k_recruit_gap;
}

void recruit_glyph(ImDrawList* dl, int theatre, ImVec2 c, ImU32 col)
{
    switch (theatre)
    {
    case 0: icons::unit_marker(dl, c, k_recruit_glyph_r, col, false); break; // the soldier
    case 1: icons::aircraft(dl, c, k_recruit_glyph_r, col); break;
    default: icons::anchor(dl, c, k_recruit_glyph_r, col); break;
    }
}

void recruit_tooltip(int theatre, const recruit_readout& r)
{
    static const char* const k_what[3] = {
        "The men every land unit is raised with and refills from. Each running military base "
        "takes them from its province's people, every quarter.",
        "The crews every wing is raised with and refills from. Each running airbase takes them "
        "from its province's people, every quarter.",
        "The crews ships will be raised with. Each port takes them from its province's people, "
        "every quarter.",
    };
    static const char* const k_source[3][2] = {
        {"running military base", "running military bases"},
        {"running airbase", "running airbases"},
        {"port", "ports"},
    };
    ImGui::BeginTooltip();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s recruits: %d / %d",
                       k_recruit_theatre_names[theatre], r.held, r.cap);
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
    ImGui::TextUnformatted(k_what[theatre]);
    const int sources = r.rate > 0 ? r.output / r.rate : 0;
    if (r.rate > 0)
        ImGui::TextDisabled("%d %s x %d = %d a quarter; the pool holds at most %d, fifteen quarters of it.",
                            sources, k_source[theatre][sources == 1 ? 0 : 1], r.rate, r.output, r.cap);
    else
        ImGui::TextDisabled("%d a quarter; the pool holds at most %d, fifteen quarters of it.", r.output,
                            r.cap);
    ImGui::TextDisabled("A full pool takes none: its sources make no recruits that quarter.");
    if (r.cap > 0 && r.held >= r.cap)
        ImGui::TextDisabled("Full now: nothing is made until a hire spends from it.");
    else if (r.cap == 0 && r.held == 0)
        ImGui::TextDisabled("No %s: nothing fills it.", theatre == 0 ? "running military base"
                                                        : theatre == 1 ? "running airbase" : "port");
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

/// Draw the three readouts right-aligned to @p right, the row centred on @p cy.
void draw_recruit_readouts(const recruit_readout (&r)[3], int fit, float right, float cy)
{
    header_recruits_record& rec = recruits_record();
    rec.frame = ImGui::GetFrameCount();
    rec.fit   = fit;

    ImDrawList* dl   = ImGui::GetWindowDrawList();
    const float line = ImGui::GetTextLineHeight();
    const float gy   = cy - 2.0f;                                   // glyph and numbers
    const float by   = gy + k_recruit_glyph_r + 2.0f;               // the bar beneath
    const float top  = gy - k_recruit_glyph_r - 1.0f;
    const float h    = by + k_recruit_bar_h + 1.0f - top;
    float x = right - recruit_strip_width(r, fit);
    for (int t = 0; t < 3; ++t)
    {
        const float bw = recruit_block_width(r[t], fit);
        const float share = r[t].cap > 0
                                ? std::clamp(static_cast<float>(r[t].held) / static_cast<float>(r[t].cap), 0.0f, 1.0f)
                                : (r[t].held > 0 ? 1.0f : 0.0f);

        ImGui::SetCursorScreenPos({x, top});
        ImGui::PushID(t + 0x1472);
        ImGui::InvisibleButton("##recruits", {bw, h});
        const bool hot = ImGui::IsItemHovered();
        ImGui::PopID();

        const ImU32 ink = hot ? palette::selection : palette::text_secondary;
        recruit_glyph(dl, t, {x + k_recruit_glyph_r, gy}, ink);
        if (fit < 2)
        {
            const std::string nums = recruit_numbers(r[t], fit);
            dl->AddText({x + 2.0f * k_recruit_glyph_r + k_recruit_text_gap, gy - line * 0.5f},
                        r[t].held > 0 ? ImGui::GetColorU32(ImGuiCol_Text) : palette::neutral, nums.c_str());
        }
        dl->AddRectFilled({x, by}, {x + bw, by + k_recruit_bar_h}, IM_COL32(60, 64, 74, 255), 1.0f);
        if (share > 0.0f)
            dl->AddRectFilled({x, by}, {x + bw * share, by + k_recruit_bar_h}, palette::unit_hp, 1.0f);
        if (hot)
            recruit_tooltip(t, r[t]);

        rec.theatre[t] = {r[t].held, r[t].cap, r[t].output, x + bw * 0.5f, top + h * 0.5f, share, hot};
        x += bw + k_recruit_gap;
    }
}

} // namespace

const header_recruits_record& header_recruits_drawn() { return recruits_record(); }

// Declared in header_panel.hpp; shared with the Budget ledger (BL-171) so both the
// header strip's CARGO figure and the ledger's Cargo Value use one valuation.
float player_cargo_value(const world& w)
{
    // BL-1265/BL-1269: no owner holds a stockpile any more — what it makes lands
    // on a market's shelf and is sold there (MARKETS.md § The shelf economy). The
    // goods the player still OWNS are its trades' cargo in transit, bought at the
    // source and sold on landing (TRADE.md § A trade), so that is what this
    // values: each in-flight convoy's cargo at its destination market's current
    // price — the price it will land into. BL-1406: the convoy's owner is the
    // player's ACTOR (its nation once folded); the seat corporation's id is kept
    // for a convoy dispatched before the fold.
    const entity_id actor = player_actor(w);
    float value = 0.0f;
    for (const convoy_component& c : w.convoys)
    {
        if ((c.corp != actor && c.corp != w.player_entity) || c.cargo_qty <= 0.0f)
            continue;
        const auto mit = w.markets.find(c.dest_market);
        if (mit == w.markets.end())
            continue;
        value += c.cargo_qty * mit->second.price[static_cast<std::size_t>(c.cargo_resource)];
    }
    return value;
}

void draw_header_panel(const world& w,
                       const recipe_registry& reg,
                       const std::vector<float>& balance_history,
                       const economy_report& report,
                       float left,
                       float right)
{
    const float width = right - left;
    if (width <= 0.0f)
        return; // window too narrow to show the strip

    // Top-align with the identity card (y = 0) so the two read as one level band.
    ImGui::SetNextWindowPos({left, 0.0f});
    ImGui::SetNextWindowSize({width, header_panel_height});

    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar          |
        ImGuiWindowFlags_NoResize            |
        ImGuiWindowFlags_NoMove              |
        ImGuiWindowFlags_NoCollapse          |
        ImGuiWindowFlags_NoScrollbar         |
        ImGuiWindowFlags_NoNav               |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##header_panel", nullptr, flags);

    // Vertically centre the single content row within the tall strip. Every element
    // on the row (text + sparkline) is drawn at one frame height, so centring the
    // start Y centres the whole row.
    ImGui::SetCursorPosY((header_panel_height - ImGui::GetFrameHeight()) * 0.5f);

    // BL-1406: the TREASURY of the player's nation (HEADER.md § Contents). A
    // nation never runs into debt, so the figure has no negative state and the
    // old `[in debt]` flag retires with the corporation balance it flagged.
    const float treasury  = player_treasury(w);
    const float valuation = player_cargo_value(w);

    // Last-tick operating net: the Balance card's net (BL-1415, refunds excluded);
    // with no row this tick, the most recent step of the treasury series.
    const float series_step = balance_history.size() >= 2
                                  ? balance_history.back() - balance_history[balance_history.size() - 2]
                                  : 0.0f;
    float net = 0.0f;
    if (!ledger_net(w, net))
        net = series_step;
    // The runway's burn: the treasury's own movement (close less open), never the
    // operating NET — construction spends cash the operating figure leaves out.
    float burn = 0.0f;
    if (!ledger_burn(w, burn))
        burn = series_step;

    // Guaranteed-fit strip (LAYOUT.md container vocabulary): the strip never wraps
    // to a second line. The three treasury/cargo/net figures are bounded-length
    // (fmt::credits/rate abbreviate to a handful of characters) and always drawn in
    // full; the sparkline is the genuine last resort to drop when the available
    // width is too narrow, so a load-bearing number is never silently clipped.
    const ImVec2 item_spacing = ImGui::GetStyle().ItemSpacing;
    const float  spark_width  = 96.0f;

    // BL-177 — the runway read: how long the treasury lasts at the current burn.
    // Shown only while it MEANS something — burning cash. A growing treasury shows
    // nothing rather than a lying "infinite quarters" figure. When the treasury
    // runs out a losing building idles (NATIONS.md § What a nation owns); the
    // nation does not borrow, so there is no "already underwater" state.
    // BL-577: this tick's contract income, named in the tooltip because a one-off
    // payment breaks the "assumes the burn holds steady" premise.
    float subsidies = 0.0f;
    if (const auto bit = report.budgets.find(player_actor(w)); bit != report.budgets.end())
        subsidies = bit->second.subsidies;

    std::string runway_text;
    std::string runway_tip;
    bool        runway_urgent = false;
    const bool  runway_meaningful = burn < 0.0f;
    if (runway_meaningful)
    {
        const int quarters = static_cast<int>(std::max(0.0f, treasury) / -burn);
        char      buf[32];
        std::snprintf(buf, sizeof(buf), "%dq", quarters);
        runway_text = buf;
        // Under two years of cushion is the point it needs acting on.
        runway_urgent = quarters < 8;
        char tip[320];
        if (subsidies > 0.0f)
            std::snprintf(tip, sizeof(tip),
                          "About %d quarters of cushion: treasury divided by how far it fell\n"
                          "last quarter, construction included. Assumes the burn holds steady - it moves as\n"
                          "prices, wages and upkeep change. Includes Cr %.0f of contract\n"
                          "income this quarter - a one-off payment, not a steady rate.\n"
                          "When the treasury runs out, a losing building idles.",
                          quarters, static_cast<double>(subsidies));
        else
            std::snprintf(tip, sizeof(tip),
                          "About %d quarters of cushion: treasury divided by how far it fell\n"
                          "last quarter, construction included. Assumes the burn holds steady - it moves as\n"
                          "prices, wages and upkeep change.\n"
                          "When the treasury runs out, a losing building idles.", quarters);
        runway_tip = tip;
    }

    float used_width = ImGui::CalcTextSize("TREASURY").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(treasury).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   CARGO").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::credits(valuation).c_str()).x + item_spacing.x +
                       ImGui::CalcTextSize("   |   NET").x + item_spacing.x +
                       ImGui::CalcTextSize(fmt::rate(net, "qtr").c_str()).x;

    const float available  = ImGui::GetContentRegionAvail().x;
    bool        show_spark = balance_history.size() >= 2;

    // BL-1472 — the recruit pools (HEADER.md § Contents, Recruits), read first: their
    // glyph-and-bar floor is RESERVED before the runway is fitted, because the bars
    // never drop (HEADER.md § Fit and degradation). The runway degrades into what is
    // left; it never draws under the readouts.
    const entity_id         nation = player_nation(w);
    const recruit_params&   rp     = reg.military().recruits;
    const recruit_readout   pools[3] = {
        read_recruit_pool(w, reg, nation, recruit_theatre::land, rp.land_per_base),
        read_recruit_pool(w, reg, nation, recruit_theatre::air, rp.air_per_base),
        sea_recruit_readout(w, reg, nation),
    };
    const float recruit_floor = k_recruit_lead + recruit_strip_width(pools, 2);
    const float money_room    = available - recruit_floor; // what the left run may take

    // BL-177 runway fit, resolved BEFORE the decorative drop because the runway
    // is load-bearing and the sparkline is not. Container 4 (LAYOUT.md) forbids
    // silently truncating a header figure, so this degrades in three honest steps:
    //   full   "   |   RUNWAY  12q"   - label and value
    //   value  "   |   12q"           - label dropped, value kept (tooltip explains)
    //   none                          - folded into the NET tooltip instead
    const float runway_sep_w   = ImGui::CalcTextSize("   |   ").x;
    const float runway_label_w = ImGui::CalcTextSize("RUNWAY").x + item_spacing.x;
    const float runway_val_w   = ImGui::CalcTextSize(runway_text.c_str()).x;
    bool show_runway       = runway_meaningful;
    bool show_runway_label = false;
    if (show_runway)
    {
        if (used_width + runway_sep_w + runway_label_w + runway_val_w <= money_room)
        {
            show_runway_label = true;
            used_width += runway_sep_w + runway_label_w + runway_val_w;
        }
        else if (used_width + runway_sep_w + runway_val_w <= money_room)
        {
            used_width += runway_sep_w + runway_val_w;
        }
        else
        {
            show_runway = false; // no room at all — NET's tooltip carries it
        }
    }

    // BL-1472 — the recruit readouts, step 2 of the ladder (HEADER.md § Fit and
    // degradation): resolved after the runway and before the sparkline. "held / cap",
    // then the held figure alone, then glyph and bar only (the numbers move into each
    // readout's tooltip). The last step is the floor reserved above.
    int recruit_fit = 2;
    for (int f = 0; f < 2; ++f)
        if (used_width + k_recruit_lead + recruit_strip_width(pools, f) <= available)
        {
            recruit_fit = f;
            break;
        }
    used_width += k_recruit_lead + recruit_strip_width(pools, recruit_fit);

    if (show_spark && used_width + item_spacing.x + spark_width > available)
        show_spark = false; // the sparkline is the last decorative element to drop

    // --- Treasury ---
    ImGui::TextDisabled("TREASURY");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Your nation's treasury. A nation never runs into debt.");
    ImGui::SameLine();
    {
        // The figures draw through fit_text (BL-215): the drop ladder above runs
        // first, so elision is the genuine last resort container 4 permits.
        // A nation's treasury is never negative; an unfolded fixture's seat
        // corporation balance can be, and is not shown in the positive colour.
        ImGui::PushStyleColor(ImGuiCol_Text, treasury < 0.0f ? palette::negative : palette::positive);
        fit_text(text_box::header_strip, "header.treasury", fmt::credits(treasury).c_str(),
                 ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();
    }

    // --- Cargo in transit, valued where it lands ---
    ImGui::SameLine();
    ImGui::TextDisabled("   |   CARGO");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Your trades' goods in transit, at the price where they land.");
    ImGui::SameLine();
    fit_text(text_box::header_strip, "header.cargo", fmt::credits(valuation).c_str(),
             ImGui::GetContentRegionAvail().x);

    // --- Last-tick net + a sparkline of recent treasuries ---
    ImGui::SameLine();
    ImGui::TextDisabled("   |   NET");
    ImGui::SameLine();
    {
        const ImU32 col = value_colour(fmt::sign_of(net));
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        fit_text(text_box::header_strip, "header.net", fmt::rate(net, "qtr").c_str(),
                 ImGui::GetContentRegionAvail().x);
        ImGui::PopStyleColor();
    }
    // NET carries the runway when there was no room to draw it (see the fit
    // resolution above), so the fact is never simply lost.
    if (runway_meaningful && !show_runway && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", runway_tip.c_str());

    // --- BL-177: runway — how long the treasury lasts at the current burn ---
    if (show_runway)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(show_runway_label ? "   |   RUNWAY" : "   |  ");
        ImGui::SameLine();
        const ImU32 col = runway_urgent ? palette::negative
                                        : ImGui::GetColorU32(ImGuiCol_Text);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s", runway_text.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", runway_tip.c_str());
    }

    if (show_spark)
    {
        ImGui::SameLine();
        ImGui::PlotLines("##balance_spark",
                         balance_history.data(),
                         static_cast<int>(balance_history.size()),
                         0, nullptr,
                         FLT_MAX, FLT_MAX,
                         {spark_width, ImGui::GetFrameHeight()});
    }

    // --- BL-1472: the recruit readouts, flush to the strip's right edge ---
    {
        const ImVec2 wp    = ImGui::GetWindowPos();
        const float  right = wp.x + ImGui::GetWindowContentRegionMax().x;
        const float  cy    = wp.y + header_panel_height * 0.5f;
        draw_recruit_readouts(pools, recruit_fit, right, cy);
    }

    ImGui::End();
}

} // namespace ui
