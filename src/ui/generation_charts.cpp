#include "generation_charts.hpp"

#include "charts.hpp"
#include "presentation.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ui {

namespace {

constexpr ImU32 col_bright = IM_COL32(225, 230, 240, 255);
constexpr ImU32 col_dim    = IM_COL32(120, 128, 145, 255);
constexpr ImU32 col_gate   = IM_COL32(220, 170,  90, 255); // a gate reads amber, not as a gridline
constexpr ImU32 col_home   = IM_COL32(150, 235, 160, 255); // homeworld, in the tile graphs' subject green

/// One colour per body, so a body keeps its identity across every chart.
constexpr ImU32 k_body_cols[4] = {
    IM_COL32(150, 160, 190, 255), IM_COL32(190, 175, 140, 255),
    IM_COL32(170, 140, 190, 255), IM_COL32(140, 185, 205, 255),
};

} // namespace

const chain_round& chain_round_at(int r)
{
    static const chain_round rounds[chain_round_count] = {
        { "The System",  "What kind of world is this, and what is it made of?",
          chain_stage::system, chain_stage::engine },
        { "Life",        "What happened on it, and what did that leave in the rocks?",
          chain_stage::water,  chain_stage::green  },
        { "Inheritance", "What did the era before you already take?",
          chain_stage::legacy, chain_stage::spend  },
    };
    if (r < 0)                 r = 0;
    if (r >= chain_round_count) r = chain_round_count - 1;
    return rounds[r];
}

const char* stage_explainer(chain_stage s)
{
    switch (s)
    {
        case chain_stage::system: return
            "Everything heavier than helium is supernova debris, so one nebula hands the "
            "same metallicity to every body it forms. Stellar mass fixes luminosity, and "
            "with it where liquid water is possible at all.";
        case chain_stage::accretion: return
            "Mass sets radius, and the two together set escape velocity. That single "
            "number decides what a body can hold on to for the next four billion years, "
            "so it is chosen here and never revisited.";
        case chain_stage::air: return
            "A body keeps its air if gravity beats sunlight. Escape velocity to the "
            "fourth power over instellation separates every world with an atmosphere "
            "from every world without. No magnetic field required.";
        case chain_stage::engine: return
            "Radiogenic heat is made in the mantle and lost through the surface, so the "
            "interior clock runs on volume over area. A hot interior keeps plates moving, "
            "and plate margins are where copper concentrates.";
        case chain_stage::water: return
            "Liquid water has an irreversible failure on each side of it: a runaway "
            "greenhouse above, an ice-albedo lock below. The band between them is narrow, "
            "and neither edge lets a world back.";
        case chain_stage::spark: return
            "Abiogenesis has happened once, here, at a sample size of one - so its "
            "probability is unconstrained by orders of magnitude. The prototype fires it "
            "wherever the chemistry allows, which is a design choice, not a measurement.";
        case chain_stage::breath: return
            "Photosynthesis and respiration are exact inverses, so a biosphere only "
            "leaves free oxygen behind when carbon is buried out of contact with it. "
            "Until then dissolved iron absorbs every molecule.";
        case chain_stage::green: return
            "About ninety percent of Earth's coal comes from one climatic window: everwet "
            "equatorial mires over subsiding basins. The cause is climate and tectonics, "
            "not anything about the plants themselves.";
        case chain_stage::legacy: return
            "Fossil resources key off the biosphere's PEAK and survive its extinction; "
            "living resources key off what is alive now and die with it. That split is "
            "what leaves a dead world still worth mining.";
        case chain_stage::spend: return
            "A richer world industrialises earlier, so it is further drawn down when you "
            "arrive. The ore is still in the ground; the cheap ore is not. This is the "
            "generated reason a corporation goes to space.";
        default: return "";
    }
}

/// Which round a stage belongs to — the accordion it is one item of. Derived
/// rather than passed, so a full-canvas takeover can show the WHOLE accordion
/// (BL-265 change 3) without either caller having to hand its round bounds down.
/// Null only if the stage tables and `chain_round_at` ever disagree.
static const chain_round* round_of(chain_stage s)
{
    const int si = static_cast<int>(s);
    for (int r = 0; r < chain_round_count; ++r)
    {
        const chain_round& cr = chain_round_at(r);
        if (si >= static_cast<int>(cr.first) && si <= static_cast<int>(cr.last))
            return &cr;
    }
    return nullptr;
}

void draw_stage_fold(const generation_chart_source& src, chain_stage s,
                     ui_state& ui, detail_surface surface)
{
    const int key = static_cast<int>(s);

    // ── Folded: Stage name  ·  verdict … ⌄ › ──
    // The verdict is a GLANCE-class figure — a label and a list, no sentences — so
    // it survives at the resting level. The explainer paragraph and the charts are
    // what the controls buy. Both controls sit in the row's right gutter (BL-265
    // change 4); the chevron used to lead the row here, which is exactly the split
    // placement Ben called disorienting.
    ImGui::PushID(key);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "%s", chain_stage_name(s));
    ImGui::SameLine();
    const std::string verdict = stage_verdict(src, s);
    gutter_text(ImGui::GetColorU32(ImGuiCol_TextDisabled), verdict.c_str());
    disclosure_controls(ui, surface, key);
    ImGui::PopID();

    // ── Expanded in place: this one stage, where it sits ──
    // Indented under its own verdict line so the accordion reads as a list with
    // something opened inside it, not as a new section.
    if (is_open_in_place(ui, surface, key))
    {
        ImGui::PushID(key);
        ImGui::Indent();
        ImGui::TextWrapped("%s", chain_stage_title(s));
        ImGui::Spacing();
        draw_stage_charts(src, s, false, &ui);
        ImGui::Unindent();
        ImGui::PopID();
    }

    // ── Full canvas: the whole ROUND, every stage open, scrolled ──
    // Not just this stage. "Anything full screen deserves its full space" (Ben,
    // 2026-08-02) — a takeover that showed one item of an accordion and left its
    // siblings folded behind it was spending the largest rectangle in the app on a
    // subset. The in-place set is deliberately not consulted: every stage renders
    // open, so the view does not depend on what the player happened to unfold first.
    if (!is_expanded(ui, surface, key))
        return;

    const chain_round* cr    = round_of(s);
    const char*        title = cr ? cr->name : chain_stage_name(s);
    if (fold_overlay_begin(ui, surface, key, title))
    {
        if (cr)
        {
            ImGui::TextDisabled("%s", cr->question);
            ImGui::Spacing();
        }
        const int first = cr ? static_cast<int>(cr->first) : key;
        const int last  = cr ? static_cast<int>(cr->last)  : key;
        for (int si = first; si <= last; ++si)
        {
            ImGui::PushID(si);
            // heading = true: each stage names itself, so the scroll reads as a
            // chain of headed sections rather than one undifferentiated run.
            draw_stage_charts(src, static_cast<chain_stage>(si), true, &ui);
            ImGui::PopID();
        }
        fold_overlay_end(ui);
    }
}

std::string stage_verdict(const generation_chart_source& src, chain_stage s)
{
    if (src.bodies == nullptr || src.count == 0)
        return "no bodies";

    const int   n_bodies = static_cast<int>(std::min<std::size_t>(src.count, 8));
    std::string lost;
    for (int i = 0; i < n_bodies; ++i)
    {
        const generation_chart_body& b = src.bodies[static_cast<std::size_t>(i)];
        if (b.state == nullptr || b.state->died_at != s)
            continue;
        if (!lost.empty())
            lost += ", ";
        lost += b.name;
    }
    return lost.empty() ? std::string("all passed") : ("Lost here: " + lost);
}

void draw_stage_charts(const generation_chart_source& src, chain_stage s, bool heading,
                       ui_state* log_ui)
{
    if (src.bodies == nullptr || src.count == 0)
        return;

    const ImGuiStyle& style    = ImGui::GetStyle();
    const int         n_bodies = static_cast<int>(std::min<std::size_t>(src.count, 8));
    const std::size_t home     = std::min(src.home, src.count - 1);

    const planetology_state& hs = *src.bodies[home].state;
    const planetology_state& hu = src.bodies[home].undrawn ? *src.bodies[home].undrawn : hs;
    const bool               have_undrawn = (src.bodies[home].undrawn != nullptr);

    auto body_colour = [&](int i) -> ImU32 {
        return (static_cast<std::size_t>(i) == home) ? col_home : k_body_cols[i & 3];
    };
    auto state_of = [&](int i) -> const planetology_state& {
        return *src.bodies[static_cast<std::size_t>(i)].state;
    };
    auto name_of = [&](int i) -> const char* {
        return src.bodies[static_cast<std::size_t>(i)].name;
    };

    // Scratch column buffer. Every chart draws immediately inside chart_row, so one
    // buffer is enough and no bar outlives the call that filled it.
    charts::bar bars[8];

    // One column per body, reading a single scalar off that body's chain state.
    auto body_bars = [&](auto&& pick) -> std::size_t {
        for (int i = 0; i < n_bodies; ++i)
        {
            bars[i].value  = pick(state_of(i));
            bars[i].colour = body_colour(i);
            bars[i].label  = name_of(i);
            bars[i].hollow = false;
        }
        return static_cast<std::size_t>(n_bodies);
    };
    auto bars_peak = [&](std::size_t n) {
        float p = 0.0f;
        for (std::size_t i = 0; i < n; ++i)
            p = std::max(p, bars[i].value);
        return p;
    };
    auto endow = [](const planetology_state& st, resource_type r) {
        return st.endowment[static_cast<std::size_t>(r)];
    };

    // Columns from a resource list, skipping anything the body does not have at all.
    // Names and colours come from presentation_of, so these endowment charts and the
    // in-game ledgers agree on both.
    auto resource_bars = [&](const planetology_state& st,
                             const resource_type* list, std::size_t count) -> std::size_t {
        std::size_t n = 0;
        for (std::size_t i = 0; i < count && n < 8; ++i)
        {
            const float v = endow(st, list[i]);
            if (v <= 0.0f)
                continue;
            const resource_presentation& rp = presentation_of(list[i]);
            bars[n].value  = v;
            bars[n].colour = rp.colour;
            bars[n].label  = rp.name;
            bars[n].hollow = false;
            ++n;
        }
        return n;
    };

    // Prose wrapped to a readable measure rather than to the panel edge. The wizard
    // panel is as wide as the charts want, which is far too wide for a paragraph;
    // the fold-out column is narrower than the measure, so the min() keeps it honest
    // in both hosts.
    auto dim_para = [&](const char* text) {
        const float avail = ImGui::GetContentRegionAvail().x;
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::min(820.0f, avail));
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    };

    // --- Chart cells ---
    // Two charts sit side by side whenever the region is wide enough. That is what
    // keeps a round to roughly a screen and a half of scroll rather than three. A
    // narrow host (the shell fold-out column) falls back to one column.
    int   cols  = 1;
    float col_w = 0.0f;
    {
        const float avail = ImGui::GetContentRegionAvail().x;
        cols  = (avail >= 780.0f) ? 2 : 1;
        col_w = std::floor((avail - style.ItemSpacing.x * static_cast<float>(cols - 1))
                           / static_cast<float>(cols)) - 1.0f;
    }
    int cell = 0; // 0 == the cursor is at the start of a chart line

    // One bordered chart, laid out exactly like the tile-selection graphs: the
    // header indents to the plot origin, the plot is reserved with a Dummy, and the
    // body draws into the CHILD-LOCAL draw list so everything clips to the box.
    // A @p span of 2 takes the whole line — for a chart that needs the room (eight
    // clustered columns) or is the odd one out in its stage.
    //
    // These rows used to carry a per-chart question log (BL-247): an "Answers/Because"
    // pair, authored beside the data, revealed by a toggle. REMOVED 2026-08-02 (Ben,
    // NEEDS_REVIEW NR-018) — "the why shouldn't leak into the UI, players don't need
    // us telling them what they ought to ask of the game." The pairs themselves are
    // not lost and are not being abandoned: they survive in commit d143aa4 and are
    // BL-260's input, which relocates them to a JSON store read by developers.
    // BL-215: chart geometry is measured BEFORE the box is reserved. row_place /
    // row_reserve carry the resolved legend placement into the body lambdas, so
    // draw_bars and threshold_line read the same reserve the row height used.
    charts::legend_place row_place   = charts::legend_place::beside;
    float                row_reserve = 0.0f;

    auto chart_row = [&](const char* id, const char* title, float h, int span,
                         const charts::bar* row_bars, std::size_t row_n,
                         const char* row_fmt, auto&& body) {
        const bool full = (span >= cols);
        if (cell != 0 && !full)
            ImGui::SameLine(0.0f, style.ItemSpacing.x);
        const float box_w = full ? ImGui::GetContentRegionAvail().x : col_w;
        const charts::chart_metrics m =
            charts::measure_chart(box_w, title, row_bars, row_n, row_fmt, h);
        row_place   = m.place;
        row_reserve = (m.place == charts::legend_place::beside)
                      ? charts::legend_width(row_bars, row_n, row_fmt) : 0.0f;
        ImGui::BeginChild(id, {full ? 0.0f : col_w, charts::chart_row_height(m)}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImGui::Indent(charts::gutter);
        // Title: wrap to at most two lines, then elide with a tooltip — a title
        // is prose, and shortening the string is the fix BL-215 forbids.
        {
            const float tw = ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::ColorConvertU32ToFloat4(palette::selection));
            if (m.title_lines <= 1)
            {
                ImGui::TextUnformatted(title);
            }
            else
            {
                const float two_lines = ImGui::GetTextLineHeight() * 2.5f;
                std::string shown = title;
                if (ImGui::CalcTextSize(title, nullptr, false, tw).y > two_lines)
                {
                    while (!shown.empty() &&
                           ImGui::CalcTextSize((shown + "...").c_str(), nullptr,
                                               false, tw).y > two_lines)
                        shown.pop_back();
                    shown += "...";
                }
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + tw);
                ImGui::TextUnformatted(shown.c_str());
                ImGui::PopTextWrapPos();
                if (shown != title)
                    ImGui::SetItemTooltip("%s", title);
            }
            ImGui::PopStyleColor();
        }
        ImGui::Unindent(charts::gutter);
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  cw = ImGui::GetContentRegionAvail().x;
        const float  extra = (m.legend_rows > 0)
                             ? static_cast<float>(m.legend_rows) * 20.0f + 6.0f : 0.0f;
        ImGui::Dummy({cw, h + extra});
        body(cdl, p, ImVec2{p.x + cw, p.y + h});
        ImGui::EndChild();
        cell = full ? 0 : (cell + 1) % cols;
    };

    // Start the next chart on a fresh line.
    auto chart_break = [&]() { cell = 0; };

    // --- The stage's own heading, physics, and casualties ---
    // Its name and the question it answers as the heading, the physics it encodes
    // beneath, which bodies it killed, then its charts.
    if (heading)
    {
        char head[160];
        std::snprintf(head, sizeof head, "%s - %s", chain_stage_name(s), chain_stage_title(s));
        ImGui::SeparatorText(head);
    }
    dim_para(stage_explainer(s));

    // Which bodies this gate kills. The chain's interesting output is WHICH GATE A
    // BODY DIED AT, so it is named at the gate and not only in the verdict.
    {
        std::string lost;
        for (int i = 0; i < n_bodies; ++i)
        {
            if (state_of(i).died_at != s)
                continue;
            if (!lost.empty())
                lost += ", ";
            lost += name_of(i);
        }
        if (!lost.empty())
            dim_para(("Lost at this gate: " + lost).c_str());
    }
    ImGui::Spacing();

    switch (s)
    {
        case chain_stage::system:
        {
            const std::size_t n = body_bars([](const planetology_state& st) { return st.instellation; });
            const float c = charts::tight_ceil(std::max(bars_peak(n), 1.1f));
            chart_row("##c_inst", "Instellation (S, Earth = 1)", 120.0f, 1, bars, n, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
                charts::threshold_line(dl, mn, mx, 1.0512f, c, col_gate, "runaway greenhouse", row_reserve);
            });

            // The homeworld on its own axis. The two gates that decide whether the
            // world you inherit can hold liquid water sit within a factor of three
            // of each other, and they are unreadable on an axis stretched by an
            // inner body taking six suns.
            charts::bar hb[1];
            hb[0].value  = hs.instellation;
            hb[0].colour = col_home;
            hb[0].label  = name_of(static_cast<int>(home));
            hb[0].hollow = false;
            const float cc = std::max(2.0f, charts::tight_ceil(hs.instellation));
            chart_row("##c_corridor", "Homeworld corridor (S, Earth = 1)", 120.0f, 1, hb, 1, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, hb, 1, cc, "%.2f", 0.0f, row_place);
                charts::threshold_line(dl, mn, mx, 1.0512f, cc, col_gate, "runaway greenhouse", row_reserve);
                charts::threshold_line(dl, mn, mx, 0.3438f, cc, col_gate, "freeze-out", row_reserve);
            });
            break;
        }

        case chain_stage::accretion:
        {
            const std::size_t n = body_bars([](const planetology_state& st) { return st.v_esc_kms; });
            const float c = charts::tight_ceil(bars_peak(n));
            chart_row("##c_vesc", "Escape velocity (km/s)", 140.0f, 2, bars, n, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
            });
            break;
        }

        case chain_stage::air:
        {
            // The losers get their own scale. The shoreline spans three orders of
            // magnitude, so the airless margin is invisible beside a body that
            // cleared the gate seven times over - and the margin is the interesting
            // part, because it says which failure was close. Gathered first because
            // whether it exists decides how wide the chart above it should be.
            charts::bar sub[8];
            std::size_t sn = 0;
            for (int i = 0; i < n_bodies && sn < 8; ++i)
            {
                const planetology_state& st = state_of(i);
                if (st.shore >= 1.5f)
                    continue;
                sub[sn].value  = st.shore;
                sub[sn].colour = body_colour(i);
                sub[sn].label  = name_of(i);
                sub[sn].hollow = false;
                ++sn;
            }

            const std::size_t n = body_bars([](const planetology_state& st) { return st.shore; });
            const float c = charts::tight_ceil(std::max(bars_peak(n), 1.6f));
            chart_row("##c_shore", "Retention shoreline (v_esc^4 / S, Mars = 1)",
                      120.0f, sn > 0 ? 1 : 2, bars, n, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
                charts::threshold_line(dl, mn, mx, 1.5f, c, col_gate, "retains air", row_reserve);
            });

            if (sn > 0)
            {
                float peak = 0.0f;
                for (std::size_t i = 0; i < sn; ++i)
                    peak = std::max(peak, sub[i].value);
                const float cs = charts::tight_ceil(std::max(peak, 0.06f));
                chart_row("##c_shore_lo", "Below the shoreline (same axis, rescaled)", 120.0f, 1, sub, sn, "%.3f",
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, sub, sn, cs, "%.3f", 0.0f, row_place);
                    charts::threshold_line(dl, mn, mx, 0.05f, cs, col_gate, "airless below", row_reserve);
                });
            }
            break;
        }

        case chain_stage::engine:
        {
            const std::size_t n = body_bars([](const planetology_state& st) { return st.theta; });
            // The ceiling is forced past the upper gate so both edges of the
            // mobile-lid band render; a threshold above the axis top is dropped.
            const float c = charts::tight_ceil(std::max(bars_peak(n), 2.4f));
            chart_row("##c_theta", "Interior heat budget (Earth now = 1)", 140.0f, 2, bars, n, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
                charts::threshold_line(dl, mn, mx, 0.55f, c, col_gate, "mobile lid floor", row_reserve);
                charts::threshold_line(dl, mn, mx, 2.20f, c, col_gate, "mobile lid ceiling", row_reserve);
            });
            break;
        }

        case chain_stage::water:
        {
            const std::size_t n = body_bars([](const planetology_state& st) { return st.surface_temp_k; });
            const float c = charts::tight_ceil(std::max(bars_peak(n), 400.0f));
            chart_row("##c_temp", "Surface temperature (K)", 120.0f, 1, bars, n, "%.0f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.0f", 0.0f, row_place);
                charts::threshold_line(dl, mn, mx, 273.0f, c, col_gate, "water freezes", row_reserve);
                charts::threshold_line(dl, mn, mx, 373.0f, c, col_gate, "water boils", row_reserve);
            });

            // The land/sea split the ocean lean actually produced - it is the
            // generated water fraction, not the preference, that the tile pass reads.
            charts::bar wb[2];
            wb[0].value  = hs.profile.water_fraction * 100.0f;
            wb[0].colour = IM_COL32(90, 150, 210, 255);
            wb[0].label  = "Ocean";
            wb[0].hollow = false;
            wb[1].value  = 100.0f - wb[0].value;
            wb[1].colour = IM_COL32(150, 190, 120, 255);
            wb[1].label  = "Land";
            wb[1].hollow = false;
            chart_row("##c_ocean", "Homeworld surface split", 120.0f, 1, wb, 2, "%.0f%%",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, wb, 2, 100.0f, "%.0f%%", 0.0f, row_place);
            });
            break;
        }

        case chain_stage::spark:
        {
            // How far up the ladder each body got. Ordinal, so the ceiling is the
            // top of the enum rather than a nice_ceil of the data.
            const std::size_t n = body_bars([](const planetology_state& st) {
                return static_cast<float>(st.peak);
            });
            chart_row("##c_peak", "Peak biology reached (sterile 0 -> civilised 6)", 140.0f, 2, bars, n, "%.0f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, 6.0f, "%.0f", 0.0f, row_place);
            });

            std::string ladder;
            for (int i = 0; i < n_bodies; ++i)
            {
                if (!ladder.empty())
                    ladder += "   ";
                ladder += name_of(i);
                ladder += ": ";
                ladder += life_stage_name(state_of(i).peak);
            }
            dim_para(ladder.c_str());
            break;
        }

        case chain_stage::breath:
        {
            // The money chart: one lean, two resources, opposite directions. Iron
            // wants a long ferruginous ocean; coal wants the land era that only
            // starts once that ocean has closed.
            static constexpr resource_type k_trade[] = {
                resource_type::iron_ore, resource_type::coal };
            const std::size_t n = resource_bars(hs, k_trade, 2);
            const float c = charts::tight_ceil(std::max(bars_peak(n), 1.0f));
            chart_row("##c_trade", "The iron / coal trade (homeworld endowment)", 120.0f, 1, bars, n, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
            });

            // The two windows those resources are actually measured from.
            charts::bar gb[2];
            gb[0].value  = hs.ferruginous_gyr;
            gb[0].colour = presentation_of(resource_type::iron_ore).colour;
            gb[0].label  = "Ferruginous ocean";
            gb[0].hollow = false;
            gb[1].value  = hs.marine_anoxia_gyr;
            gb[1].colour = presentation_of(resource_type::petroleum).colour;
            gb[1].label  = "Marine anoxia";
            gb[1].hollow = false;
            const float cg = charts::tight_ceil(std::max({gb[0].value, gb[1].value, 1.0f}));
            chart_row("##c_windows", "Anoxic windows (Gyr)", 120.0f, 1, gb, 2, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, gb, 2, cg, "%.2f", 0.0f, row_place);
            });

            dim_para("Iron is banded-iron deposits laid down while the ocean was still "
                     "anoxic; coal needs the land biosphere that only follows once "
                     "oxygen has won. Buying one spends the other.");
            break;
        }

        case chain_stage::green:
        {
            static constexpr resource_type k_green[] = {
                resource_type::coal, resource_type::timber,
                resource_type::agricultural_produce };
            const std::size_t n = resource_bars(hs, k_green, 3);
            if (n > 0)
            {
                const float c = charts::tight_ceil(std::max(bars_peak(n), 1.0f));
                chart_row("##c_green", "What the land era leaves (homeworld endowment)", 120.0f, 1, bars, n, "%.2f",
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
                });
            }
            else
            {
                dim_para("No land biosphere - no coal, no timber, no crops.");
            }

            charts::bar ab[1];
            ab[0].value  = hs.arable_share * 100.0f;
            ab[0].colour = presentation_of(resource_type::agricultural_produce).colour;
            ab[0].label  = "Arable";
            ab[0].hollow = false;
            const float ca = charts::tight_ceil(std::max(ab[0].value, 10.0f));
            chart_row("##c_arable", "Arable share of land", 120.0f, n > 0 ? 1 : 2, ab, 1, "%.0f%%",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, ab, 1, ca, "%.0f%%", 0.0f, row_place);
            });
            break;
        }

        case chain_stage::legacy:
        {
            // The payoff. Every non-zero endowment the homeworld carries, split into
            // three charts only because a single clustered chart's legend cannot
            // stack sixteen rows inside one plot - the coverage is the full set.
            static constexpr resource_type k_metals[] = {
                resource_type::iron_ore, resource_type::copper_ore,
                resource_type::rare_earth_ore, resource_type::iron_nickel_ore,
                resource_type::platinum_group_metals };
            static constexpr resource_type k_carbon[] = {
                resource_type::coal, resource_type::petroleum, resource_type::peat,
                resource_type::timber, resource_type::agricultural_produce };
            static constexpr resource_type k_bulk[] = {
                resource_type::stone, resource_type::silica, resource_type::sand,
                resource_type::clay, resource_type::regolith, resource_type::water };

            const auto group = [&](const char* id, const char* title, float h, int span,
                                   const resource_type* list, std::size_t count) {
                const std::size_t n = resource_bars(hs, list, count);
                if (n == 0)
                    return;
                const float c = charts::tight_ceil(std::max(bars_peak(n), 1.0f));
                chart_row(id, title, h, span, bars, n, "%.2f",
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
                });
            };
            group("##c_metals", "Metals (homeworld endowment, 1.0 = Earth-typical)",
                  120.0f, 1, k_metals, sizeof k_metals / sizeof k_metals[0]);
            group("##c_carbon", "Carbon and living resources",
                  120.0f, 1, k_carbon, sizeof k_carbon / sizeof k_carbon[0]);

            // C -> D: what this biosphere evolved that grows nowhere else (BL-191).
            // These do not live in `endowment` like the industrial raws — an endemic
            // good has no global abundance to scale, only an origin — so they are
            // charted from the endemic set itself.
            if (!hs.endemics.empty())
            {
                std::size_t n = 0;
                for (const endemic_good& e : hs.endemics)
                {
                    if (n >= 8) break;
                    const resource_presentation& rp = presentation_of(e.good);
                    bars[n].value  = e.richness;
                    bars[n].colour = rp.colour;
                    bars[n].label  = rp.name;
                    bars[n].hollow = false;
                    ++n;
                }
                const float c = charts::tight_ceil(std::max(bars_peak(n), 1.0f));
                chart_row("##c_endemic",
                          "Endemic trade goods - worth more the further you carry them",
                          130.0f, 2, bars, n, "%.2f",
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f", 0.0f, row_place);
                });

                // Where each one grows. The band and region ARE the value, so they
                // are stated rather than left implicit in the bar.
                ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
                for (const endemic_good& e : hs.endemics)
                {
                    const float mid = (e.lat_lo + e.lat_hi) * 0.5f;
                    const char* band = mid < 0.25f ? "tropical"
                                     : mid < 0.45f ? "subtropical"
                                     : mid < 0.60f ? "temperate"
                                     : "polar";
                    ImGui::TextWrapped("%s grows only in one %s region - %.0f%% of the way round the globe.",
                                       presentation_of(e.good).name, band,
                                       static_cast<double>(e.sector_centre * 100.0f));
                }
                ImGui::PopStyleColor();
            }

            group("##c_bulk",   "Sedimentary and bulk",
                  130.0f, 2, k_bulk,   sizeof k_bulk   / sizeof k_bulk[0]);

            // What each body has and lacks, in one line apiece — the reason to go
            // anywhere other than home, stated before the campaign starts.
            chart_break();
            ImGui::Spacing();
            static constexpr resource_type k_core[] = {
                resource_type::iron_ore, resource_type::coal, resource_type::petroleum,
                resource_type::copper_ore, resource_type::water,
                resource_type::agricultural_produce, resource_type::timber,
                resource_type::platinum_group_metals };
            for (int i = 0; i < n_bodies; ++i)
            {
                ImGui::PushID(i);
                const planetology_state& st = state_of(i);

                ImGui::PushStyleColor(ImGuiCol_Text, body_colour(i));
                ImGui::TextUnformatted(name_of(i));
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, col_bright);
                ImGui::TextUnformatted(archetype_name(st.archetype));
                ImGui::PopStyleColor();

                std::string rich, lacks;
                for (const resource_type rt : k_core)
                {
                    const float v = endow(st, rt);
                    const char* nm = presentation_of(rt).name;
                    if (v >= 1.3f)      { if (!rich.empty())  rich  += ", "; rich  += nm; }
                    else if (v <= 0.0f) { if (!lacks.empty()) lacks += ", "; lacks += nm; }
                }
                std::string strip;
                if (!rich.empty())  strip  = "Rich: " + rich;
                if (!lacks.empty()) strip += (strip.empty() ? "" : "   ") + std::string("Lacks: ") + lacks;
                if (strip.empty())  strip  = "Nothing exceptional either way.";
                dim_para(strip.c_str());

                ImGui::PopID();
            }
            break;
        }

        case chain_stage::spend:
        {
            // Before / after, clustered in pairs: the hollow column is the endowment
            // the chain formed, the filled one is what is left after a prior
            // industrial era took the cheap half of it.
            static constexpr resource_type k_spend[] = {
                resource_type::coal, resource_type::petroleum,
                resource_type::iron_ore, resource_type::copper_ore };
            charts::bar sb[8];
            char        sl[8][32];
            std::size_t sn   = 0;
            float       peak = 0.0f;
            for (const resource_type rt : k_spend)
            {
                const resource_presentation& rp = presentation_of(rt);
                const float before = endow(hu, rt);
                const float after  = endow(hs, rt);
                if (have_undrawn)
                {
                    std::snprintf(sl[sn], sizeof sl[sn], "%s formed", rp.abbrev);
                    sb[sn].value = before; sb[sn].colour = rp.colour;
                    sb[sn].label = sl[sn]; sb[sn].hollow = true;  ++sn;
                }
                std::snprintf(sl[sn], sizeof sl[sn], "%s left", rp.abbrev);
                sb[sn].value = after;  sb[sn].colour = rp.colour;
                sb[sn].label = sl[sn]; sb[sn].hollow = false; ++sn;
                peak = std::max(peak, std::max(before, after));
            }
            const float c = charts::tight_ceil(std::max(peak, 1.0f));
            chart_row("##c_spend", "Formed against left (homeworld endowment)", 175.0f, 2, sb, sn, "%.2f",
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, sb, sn, c, "%.2f", 0.0f, row_place);
            });
            if (have_undrawn)
                dim_para("The hollow column is what the chain formed; the filled one is what "
                         "is left. Nothing was destroyed - the accessible half was already "
                         "mined, which is why the next tonne has to come from somewhere else.");
            else
                dim_para("What is left after a prior industrial era took the accessible half. "
                         "Nothing was destroyed - the cheap ore was already mined, which is "
                         "why the next tonne has to come from somewhere else.");
            break;
        }

        default:
            break;
    }

    chart_break();
}

} // namespace ui
