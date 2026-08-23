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

void draw_stage_charts(const generation_chart_source& src, chain_stage s, bool heading)
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
    auto chart_row = [&](const char* id, const char* title, float h, int span, auto&& body) {
        const bool full = (span >= cols);
        if (cell != 0 && !full)
            ImGui::SameLine(0.0f, style.ItemSpacing.x);
        ImGui::BeginChild(id, {full ? 0.0f : col_w, charts::chart_row_height(h)}, true,
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        ImGui::Indent(charts::gutter);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", title);
        ImGui::Unindent(charts::gutter);
        const ImVec2 p  = ImGui::GetCursorScreenPos();
        const float  cw = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy({cw, h});
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
            chart_row("##c_inst", "Instellation (S, Earth = 1)", 120.0f, 1,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                charts::threshold_line(dl, mn, mx, 1.0512f, c, col_gate, "runaway greenhouse");
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
            chart_row("##c_corridor", "Homeworld corridor (S, Earth = 1)", 120.0f, 1,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, hb, 1, cc, "%.2f");
                charts::threshold_line(dl, mn, mx, 1.0512f, cc, col_gate, "runaway greenhouse");
                charts::threshold_line(dl, mn, mx, 0.3438f, cc, col_gate, "freeze-out");
            });
            break;
        }

        case chain_stage::accretion:
        {
            const std::size_t n = body_bars([](const planetology_state& st) { return st.v_esc_kms; });
            const float c = charts::tight_ceil(bars_peak(n));
            chart_row("##c_vesc", "Escape velocity (km/s)", 140.0f, 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
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
                      120.0f, sn > 0 ? 1 : 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                charts::threshold_line(dl, mn, mx, 1.5f, c, col_gate, "retains air");
            });

            if (sn > 0)
            {
                float peak = 0.0f;
                for (std::size_t i = 0; i < sn; ++i)
                    peak = std::max(peak, sub[i].value);
                const float cs = charts::tight_ceil(std::max(peak, 0.06f));
                chart_row("##c_shore_lo", "Below the shoreline (same axis, rescaled)", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, sub, sn, cs, "%.3f");
                    charts::threshold_line(dl, mn, mx, 0.05f, cs, col_gate, "airless below");
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
            chart_row("##c_theta", "Interior heat budget (Earth now = 1)", 140.0f, 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
                charts::threshold_line(dl, mn, mx, 0.55f, c, col_gate, "mobile lid floor");
                charts::threshold_line(dl, mn, mx, 2.20f, c, col_gate, "mobile lid ceiling");
            });
            break;
        }

        case chain_stage::water:
        {
            const std::size_t n = body_bars([](const planetology_state& st) { return st.surface_temp_k; });
            const float c = charts::tight_ceil(std::max(bars_peak(n), 400.0f));
            chart_row("##c_temp", "Surface temperature (K)", 120.0f, 1,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.0f");
                charts::threshold_line(dl, mn, mx, 273.0f, c, col_gate, "water freezes");
                charts::threshold_line(dl, mn, mx, 373.0f, c, col_gate, "water boils");
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
            chart_row("##c_ocean", "Homeworld surface split", 120.0f, 1,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, wb, 2, 100.0f, "%.0f%%");
            });
            break;
        }

        case chain_stage::spark:
        {
            // HOW FAR THE CHEMISTRY CLIMBED, not how far the biology got. These are
            // different questions and the second one used to stand in for the
            // first: `peak` life_stage tops out at "microbial" for every world that
            // sparked at all, so it could not distinguish a world that stopped at
            // phosphorylation from one that built a coded genome. `abiogenesis_depth`
            // is the axis BL-209 added for exactly this, and its eight rungs are the
            // seven distinguishable failures plus success.
            const std::size_t n = body_bars([](const planetology_state& st) {
                return static_cast<float>(st.depth);
            });
            chart_row("##c_depth", "Abiogenesis reached (no feedstock 0 -> coded genome 7)",
                      140.0f, 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, 7.0f, "%.0f");
            });

            std::string ladder;
            for (int i = 0; i < n_bodies; ++i)
            {
                if (!ladder.empty())
                    ladder += "   ";
                ladder += name_of(i);
                ladder += ": ";
                ladder += abiogenesis_depth_name(state_of(i).depth);
            }
            dim_para(ladder.c_str());

            // The cofactor metals. Neither is a resource and neither ever will be
            // (that would shift resource_count and every array width in the model),
            // but each one gates a biochemical step outright — so a reader tuning
            // seeds needs to see them, or the photosystem fork looks like a coin
            // toss rather than a consequence.
            const std::size_t nmn = body_bars([](const planetology_state& st) {
                return st.crustal_mn;
            });
            chart_row("##c_mn", "Crustal manganese (Earth = 1.0) - gates the Z-scheme at S6b",
                      120.0f, 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, nmn, 3.0f, "%.2f");
            });

            const std::size_t nmo = body_bars([](const planetology_state& st) {
                return st.crustal_mo;
            });
            chart_row("##c_mo", "Crustal molybdenum (Earth = 1.0) - gates nitrogen fixation, and so soil",
                      120.0f, 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, nmo, 3.0f, "%.2f");
            });
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
            chart_row("##c_trade", "The iron / coal trade (homeworld endowment)", 120.0f, 1,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
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
            chart_row("##c_windows", "Anoxic windows (Gyr)", 120.0f, 1,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, gb, 2, cg, "%.2f");
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
                chart_row("##c_green", "What the land era leaves (homeworld endowment)", 120.0f, 1,
                          [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
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
            chart_row("##c_arable", "Arable share of land", 120.0f, n > 0 ? 1 : 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, ab, 1, ca, "%.0f%%");
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
                chart_row(id, title, h, span, [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
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
                          130.0f, 2, [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                    charts::draw_bars(dl, mn, mx, bars, n, c, "%.2f");
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
            chart_row("##c_spend", "Formed against left (homeworld endowment)", 175.0f, 2,
                      [&](ImDrawList* dl, ImVec2 mn, ImVec2 mx) {
                charts::draw_bars(dl, mn, mx, sb, sn, c, "%.2f");
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

// ---------------------------------------------------------------------------
// At-a-glance heuristics
// ---------------------------------------------------------------------------

namespace {

/// The gates the spine shows, in chain order, with the short labels a compact row
/// can afford. S5a-S5g is the abiogenesis chain; S6a-S6c the photosystem fork.
struct spine_gate
{
    chem::substage gate;
    const char*    tag;   ///< Two characters — the row has to stay one line.
    const char*    what;  ///< Tooltip: the question this gate asks.
};

const spine_gate k_spine[] = {
    { chem::substage::s5a_feedstock,      "Fd", "Feedstock - anything to build with? (HCN, formaldehyde)" },
    { chem::substage::s5b_reductant,      "Rd", "Reductant - free energy of the right shape? (serpentinisation)" },
    { chem::substage::s5c_phosphorylation,"Ph", "Phosphorylation - could it make a nucleotide? (the hard one)" },
    { chem::substage::s5d_concentration,  "Cn", "Concentration - dense enough to polymerise? (the 0.1 M threshold)" },
    { chem::substage::s5e_replicator,     "Rp", "Replicator - did a polymer copy itself AND survive the heat?" },
    { chem::substage::s5f_compartment,    "Cm", "Compartment - did it become an individual?" },
    { chem::substage::s5g_code,           "Cd", "Code - did DNA lift the genome ceiling?" },
    { chem::substage::s6a_anoxygenic,     "Ax", "Anoxygenic photosynthesis - banded iron with no free oxygen" },
    { chem::substage::s6b_oxygenic,       "Ox", "Oxygenic photosynthesis - the Z-scheme. Gates on manganese" },
    { chem::substage::s6c_nitrogen,       "N2", "Nitrogen fixation - nitrogenase. Gates on molybdenum, and so on soil" },
};

/// Recover a gate's verdict from the trace.
///
/// Reads ONLY the event flagged `event_gate_verdict`. An earlier version took the
/// worst outcome across every event the gate emitted, which was wrong: a gate
/// records its contributing and competing reactions too, and those carry their own
/// outcomes. On a healthy homeworld that reported four of seven gates as marginal
/// — because the impact reduction, the schreibersite route, the hydrolysis and the
/// cation load were each individually marginal — which drains the word of meaning.
chem::outcome gate_outcome(const planetology_state& st, chem::substage g)
{
    for (const chem::molecular_event& e : st.trace)
    {
        if ((e.flags & chem::event_gate_verdict) == 0)
            continue;
        if (chem::info(static_cast<chem::process>(e.process_id)).gate != g)
            continue;
        return static_cast<chem::outcome>(e.outcome_id);
    }
    return chem::outcome::bypassed; // never evaluated — an earlier gate ended it
}

} // namespace

void draw_chain_spine(const planetology_state& st)
{
    std::string marginal_at;
    bool        any = false;

    // TWO ROWS, not one. The ledger panel is ~310 px and a single row of ten tags
    // plus a label clipped its last gate — which is the worst possible thing to
    // lose off the end of a causal chain. Splitting on the S5/S6 boundary also
    // matches what the groups mean.
    ImGui::TextDisabled("S5 chain");
    ImGui::SameLine();

    for (std::size_t i = 0; i < sizeof(k_spine) / sizeof(k_spine[0]); ++i)
    {
        const chem::outcome oc = gate_outcome(st, k_spine[i].gate);

        if (k_spine[i].gate == chem::substage::s6a_anoxygenic)
        {
            ImGui::TextDisabled("S6 fork ");
            ImGui::SameLine();
        }

        ImVec4 col;
        switch (oc)
        {
            case chem::outcome::fired:    col = ImVec4(0.42f, 0.78f, 0.46f, 1.0f); break; // green
            case chem::outcome::marginal: col = ImVec4(0.90f, 0.72f, 0.30f, 1.0f); break; // amber
            case chem::outcome::failed:   col = ImVec4(0.85f, 0.36f, 0.34f, 1.0f); break; // red
            default:                      col = ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]; break;
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, col);
        ImGui::TextUnformatted(oc == chem::outcome::bypassed ? "--" : k_spine[i].tag);
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s\n%s", k_spine[i].what, chem::outcome_name(oc));
        }

        if (oc == chem::outcome::marginal)
        {
            if (!marginal_at.empty())
                marginal_at += ", ";
            marginal_at += k_spine[i].tag;
            any = true;
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("-> %s", abiogenesis_depth_name(st.depth));

    // The margin. A world that only just cleared a gate is a world that nearly
    // became something else, and that is the most interesting thing about it.
    //
    // Wrapped, not TextDisabled: the unwrapped form ran off the edge of the panel
    // and the sentence was unreadable.
    if (any)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("marginal at %s - came close to stopping there", marginal_at.c_str());
        ImGui::PopStyleColor();
    }
}

void draw_denial_list(const planetology_state& st)
{
    // Ordered by how much each absence constrains PLAY, not by chain order: a
    // reader scanning seeds cares first about what they cannot build.
    std::string lacks;
    const auto add = [&lacks](const char* s) {
        if (!lacks.empty())
            lacks += ", ";
        lacks += s;
    };

    if (st.peak < life_stage::land)        add("no coal");
    else if (st.land_burial_gyr < 0.05f)   add("negligible coal");

    if (st.peak < life_stage::microbial)   add("no fossil carbon at all");
    else if (st.marine_anoxia_gyr < 0.10f) add("no petroleum");

    if (st.ferruginous_gyr < 0.10f)        add("no banded iron");
    if (!st.mobile_lid)                    add("no porphyry copper (stagnant lid)");
    if (st.o2_fraction < 0.01f)            add("no free oxygen");

    if (st.arable_share < 0.08f)           add("not farmable");
    else if (st.arable_share < 0.15f)      add("thin soils");

    if (st.crustal_mo < 0.45f)             add("molybdenum-poor (caps nitrogen fixation)");
    if (st.crustal_mn < 0.45f)             add("manganese-poor (blocks the Z-scheme)");

    // Wrapped in both branches — the panel is narrow enough that a SameLine label
    // plus a long list clipped the list mid-word.
    if (lacks.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
        ImGui::TextWrapped("lacks nothing decisive - the full set is here");
        ImGui::PopStyleColor();
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.55f, 0.40f, 1.0f));
    ImGui::TextWrapped("lacks %s", lacks.c_str());
    ImGui::PopStyleColor();
}

std::string citation_summary(const hist::entry& e)
{
    // Citation domain 0x01 is chemistry; see history_log.hpp for the key space.
    int outcome_id = -1, yield_q = -1, venue_id = -1;
    for (const hist::citation& c : e.citations)
    {
        switch (c.key)
        {
            case 0x0102: outcome_id = c.value; break;
            case 0x0103: yield_q    = c.value; break;
            case 0x0104: venue_id   = c.value; break;
            default: break;
        }
    }

    if (outcome_id < 0)
        return {};

    char buf[128];
    const char* oc = chem::outcome_name(static_cast<chem::outcome>(outcome_id));
    const char* vn = venue_id > 0 ? chem::venue_name(static_cast<chem::venue>(venue_id)) : "";

    if (yield_q >= 0 && vn[0] != '\0')
        std::snprintf(buf, sizeof(buf), "%-8s %.2f  %s", oc, yield_q / 1000.0f, vn);
    else if (yield_q >= 0)
        std::snprintf(buf, sizeof(buf), "%-8s %.2f", oc, yield_q / 1000.0f);
    else
        std::snprintf(buf, sizeof(buf), "%-8s", oc);
    return buf;
}

} // namespace ui
