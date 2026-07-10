#define IMGUI_DEFINE_MATH_OPERATORS
#include "body_surface_canvas.hpp"

#include "entity_summary.hpp"
#include "highlight.hpp"
#include "hover_card.hpp"
#include "hover_content.hpp"
#include "icons.hpp"
#include "market_ledger.hpp" // market_city_name (Market lens catchment key, BL-015)
#include "nav_pane.hpp"
#include "presentation.hpp"
#include "world/logistics.hpp"       // intra_body_path (convoy vision beam, BL-152)
#include "world/market_clearing.hpp" // market_for_tile (Scarcity catchment, prices)
#include "world/placement_rules.hpp"
#include "world/survey_system.hpp"   // survey_tile_visible (region mask, BL-067)
#include "world/workforce.hpp"       // workforce_efficiency (Population lens, BL-069)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ui {

namespace {

constexpr float kSqrt3 = 1.7320508f;
constexpr float kPi    = 3.14159265f;

// Zoom is normalised to grid height: at zoom = 1 the grid fills kFitMargin of the
// canvas height. The zoom floor must therefore be derived from these constants,
// not guessed — at the minimum the viewport spans kMinZoomHeadroom of the grid
// height (the full grid plus ~20% headroom), i.e. zoom = 1 / (headroom * margin).
constexpr float kFitMargin       = 0.95f; // grid fills 95% of the canvas height at zoom = 1
constexpr float kMinZoomHeadroom = 1.2f;  // at min zoom the viewport shows ~120% of the grid height
constexpr float kMaxZoom         = 20.0f;
constexpr float kMinZoom         = 1.0f / (kMinZoomHeadroom * kFitMargin); // ~0.877

// Identity fill colour for a tile's composition. The single source of truth for
// surface tinting; landform is conveyed by overlay glyphs (deferred), not hue.
ImU32 terrain_colour(terrain_composition t)
{
    switch (t)
    {
        case terrain_composition::barren:    return IM_COL32(170, 145, 100, 255);
        case terrain_composition::rocky:     return IM_COL32(112, 105,  95, 255);
        case terrain_composition::volcanic:  return IM_COL32(135,  55,  28, 255);
        case terrain_composition::icy:       return IM_COL32(200, 224, 236, 255);
        case terrain_composition::tundra:    return IM_COL32(140, 140, 118, 255);
        case terrain_composition::grassland: return IM_COL32( 96, 150,  72, 255);
        case terrain_composition::forest:    return IM_COL32( 48, 102,  56, 255);
        case terrain_composition::wetland:   return IM_COL32( 78, 120,  92, 255);
        case terrain_composition::ocean:     return IM_COL32( 40,  80, 160, 255);
        case terrain_composition::regolith:  return IM_COL32(138, 130, 120, 255);
        case terrain_composition::metallic:  return IM_COL32(158, 150, 140, 255);
    }
    return IM_COL32( 60,  60,  60, 255);
}

/// Fills `out[6]` with the screen-space vertices of a pointy-top hexagon
/// centred at (cx, cy) with circumradius r.
void hex_vertices(ImVec2 out[6], float cx, float cy, float r)
{
    for (int i = 0; i < 6; ++i)
    {
        const float angle = kPi / 6.0f + kPi / 3.0f * static_cast<float>(i);
        out[i] = { cx + r * std::cos(angle), cy + r * std::sin(angle) };
    }
}

/// World-space centre of a hex at (col, row) in odd-r offset coordinates,
/// relative to the grid top-left, using the given circumradius.
ImVec2 hex_local_centre(int col, int row, float hex_size)
{
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    return {
        col_step * static_cast<float>(col) + ((row & 1) ? col_step * 0.5f : 0.0f),
        row_step * static_cast<float>(row),
    };
}

/// Per-channel linear blend of two opaque colours: result = a·(1−t) + b·t, alpha
/// forced opaque. Used by the Resource lens to composite a deposit hue over terrain
/// at a magnitude-driven opacity, and by the Market lens's diverging wash.
ImU32 lerp_colour(ImU32 a, ImU32 b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const float u = 1.0f - t;
    auto ch = [](ImU32 c, int shift) -> float { return static_cast<float>((c >> shift) & 0xFFu); };
    const int r = static_cast<int>(ch(a, IM_COL32_R_SHIFT) * u + ch(b, IM_COL32_R_SHIFT) * t);
    const int g = static_cast<int>(ch(a, IM_COL32_G_SHIFT) * u + ch(b, IM_COL32_G_SHIFT) * t);
    const int bl= static_cast<int>(ch(a, IM_COL32_B_SHIFT) * u + ch(b, IM_COL32_B_SHIFT) * t);
    return IM_COL32(r, g, bl, 255);
}

/// Shared red→yellow→green ramp for every "red to green" lens (Opportunity,
/// Population/workforce, Production). `t` in [0, 1]: 0 = red (low), 0.5 = yellow
/// (mid), 1 = green (high). Routing all these lenses through one helper keeps a
/// mid value reading **yellow** rather than the muddy brown a direct red→green
/// blend (or the former grey neutral) produces. Ben's directive 2026-07-10.
ImU32 ryg_colour(float t)
{
    constexpr ImU32 red    = IM_COL32(216, 100,  96, 255);
    constexpr ImU32 yellow = IM_COL32(228, 200,  84, 255);
    constexpr ImU32 green  = IM_COL32(110, 200, 120, 255);
    t = std::clamp(t, 0.0f, 1.0f);
    return t < 0.5f ? lerp_colour(red,    yellow, t * 2.0f)
                    : lerp_colour(yellow, green,  (t - 0.5f) * 2.0f);
}

/// Diverging warm↔cool colour for a ratio relative to 1.0 (defined below); forward
/// declared so the Production key (above its definition) can sample the same band.
ImU32 diverging_colour(float ratio);

/// Diverging red→green colour for a ratio relative to 1.0 (defined below); forward
/// declared so the Production key (above its definition) can sample the same band.
/// Distinct ramp from diverging_colour (BL-137) — dedicated so the Market lens's
/// cool/warm scale is untouched.
ImU32 production_colour(float ratio);

/// Shared chrome for an on-canvas lens key: a rounded dark panel of @p box_w ×
/// @p body_h at the left edge (inset past the nav rail), vertically centred —
/// clear of the Selection panel, the header/Explorer, and the lens control strip.
/// Returns the inner top-left and the inner content width via @p out_x/@p out_y/
/// @p out_w. Pure ImDrawList — no ImGui widget state.
void begin_lens_key(ImDrawList* dl, ImVec2 anchor, float box_w,
                    float body_h, float pad, float& out_x, float& out_y, float& out_w)
{
    // Anchored so the box's RIGHT edge sits at anchor.x (the minimap's left edge) and
    // the box is vertically centred on anchor.y — it reads as a drawer folding out from
    // the left side of the minimap. anchor is passed in from app.cpp (lens_key_anchor).
    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);
    out_x = p0.x + pad;
    out_y = p0.y + pad * 0.5f;
    out_w = box_w - 2.0f * pad;
}

/// The lens-local resource/good selector for the Resource, Market, and Scarcity
/// lenses (BL-134): all three pick "which resource" from the same `lens_resource`
/// field (LENSES.md says the selectors share a form), so one combo serves them.
/// Now lives at the top of the on-canvas legend (moved off the minimap strip,
/// which the former popup button docked in) — a real scrollable ImGui::BeginCombo,
/// hosted in a small borderless window since the legend itself paints on the
/// background draw list rather than a live ImGui window.
constexpr float kLensComboH = 22.0f;

void draw_lens_resource_combo(ui_state& state, ImVec2 pos, float w)
{
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, kLensComboH}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse  | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin("##lens_key_resource_combo", nullptr, flags);
    ImGui::SetNextItemWidth(w);
    if (ImGui::BeginCombo("##lens_key_resource", presentation_of(state.lens_resource).name))
    {
        for (std::size_t i = 0; i < resource_count; ++i)
        {
            const resource_type r = static_cast<resource_type>(i);
            if (ImGui::Selectable(presentation_of(r).name, r == state.lens_resource))
                state.lens_resource = r;
        }
        ImGui::EndCombo();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

/// On-canvas legend for the Resource lens (BL-019): the selected resource's name
/// and identity swatch, plus a note that the fill marks the contiguous deposit.
/// Flat, not a gradient — the lens shows deposit *shape*, not magnitude.
void draw_resource_key(ImDrawList* dl, ImVec2 anchor,
                       ui_state& state)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float body_h = pad + kLensComboH + 4.0f + line_h + 4.0f + line_h + 4.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, anchor, 168.0f, body_h, pad, x, y, bar_w);

    draw_lens_resource_combo(state, {x, y}, bar_w);
    y += kLensComboH + 4.0f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Resource deposit");
    y += line_h + 4.0f;
    dl->AddRectFilled({x, y + 2.0f}, {x + 10.0f, y + 12.0f},
                      presentation_of(state.lens_resource).colour);
    dl->AddText({x + 14.0f, y}, IM_COL32(235, 235, 235, 255),
                presentation_of(state.lens_resource).name);
    y += line_h + 4.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "filled = deposit present");
}

/// On-canvas legend for the Opportunity lens (BL-136): a body-relative red→green
/// rank bar over the volume-weighted unmet-demand-gap field — each market's
/// demand-gap × demand-volume score, ranked against the body max (mirrors the
/// Scarcity lens's per-market normalisation). Standard key width — the former
/// "(unmet demand)" qualifier that widened this box is gone (BL-136).
void draw_opportunity_key(ImDrawList* dl, ImVec2 anchor)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;
    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, anchor, 168.0f, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Opportunity");
    y += line_h + 4.0f;
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t = static_cast<float>(i) / (segs - 1);
        const ImU32 c = ryg_colour(t);
        dl->AddRectFilled({ x + bar_w * static_cast<float>(i) / segs, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low");
    const ImVec2 ts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - ts.x, y}, IM_COL32(170, 175, 185, 255), "high");
}

/// On-canvas legend for the Production lens (BL-009): a diverging cool→warm bar
/// (below/above the body's mean output value) over the production-intensity surface.
void draw_production_key(ImDrawList* dl, ImVec2 anchor)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;
    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, anchor, 168.0f, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Production intensity");
    y += line_h + 4.0f;
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t = static_cast<float>(i) / (segs - 1);
        const ImU32 c = production_colour(std::pow(4.0f, t * 2.0f - 1.0f));
        dl->AddRectFilled({ x + bar_w * static_cast<float>(i) / segs, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low");
    const ImVec2 ts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - ts.x, y}, IM_COL32(170, 175, 185, 255), "high");
}

/// Diverging warm↔cool colour for a price relative to its base (floor) price.
/// `ratio = price / base_price`: 1.0 is the neutral mid-tone, < 1 (cheap) trends
/// cool, > 1 (dear) trends warm. Centred on the log of the ratio so the symmetric
/// price band `[0.25×, 4×]` (the market clamp) maps to the full [cool, warm] span.
ImU32 diverging_colour(float ratio)
{
    ratio = std::clamp(ratio, 0.25f, 4.0f);
    const float d = std::log(ratio) / std::log(4.0f); // [-1, 1]
    constexpr ImU32 neutral = IM_COL32(205, 205, 210, 255);
    constexpr ImU32 cool    = IM_COL32( 70, 140, 225, 255);
    constexpr ImU32 warm    = IM_COL32(232, 120,  60, 255);
    return d < 0.0f ? lerp_colour(neutral, cool, -d) : lerp_colour(neutral, warm, d);
}

/// Diverging red→yellow→green colour for a ratio relative to 1.0 (BL-137, Production
/// lens): `ratio = value / mean`; 1.0 is the yellow mid-tone, < 1 (below mean) trends
/// red, > 1 (above mean) trends green. Same log-of-ratio centring as diverging_colour,
/// but routed through the shared ryg_colour ramp — diverging_colour stays untouched for
/// the Market lens.
ImU32 production_colour(float ratio)
{
    ratio = std::clamp(ratio, 0.25f, 4.0f);
    const float d = std::log(ratio) / std::log(4.0f); // [-1, 1]
    // Map the diverging axis onto the shared red→yellow→green ramp so the mean
    // (d = 0) reads yellow, below-mean red, above-mean green — one vocabulary
    // across the red-to-green lenses (Ben's directive 2026-07-10).
    return ryg_colour((d + 1.0f) * 0.5f);
}

/// On-canvas legend for the Market lens: a diverging cheap↔dear gradient bar plus
/// the selected good's name and its current price ratio (or an "untraded" note when
/// the body's market has no entry for it). Same left-edge placement as the Resource key.
// BL-015: market lens is now a catchment-boundary tint (one colour per market).
// The key shows a colour swatch per market labelled with its city name (matching the
// ledger / selection / CSV — never a bare ordinal, which the player never sees elsewhere).
void draw_market_key(ImDrawList* dl, ImVec2 anchor, const world& w,
                     ui_state& state,
                     const std::unordered_map<entity_id, ImU32>& catchment_colours)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float swatch = line_h;
    const int   n      = static_cast<int>(catchment_colours.size());
    // Stable legend order — the source map iterates arbitrarily; sort by market id so the
    // swatch list does not reshuffle frame to frame (colours stay keyed to their market).
    std::vector<std::pair<entity_id, ImU32>> entries(catchment_colours.begin(),
                                                     catchment_colours.end());
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Size the box to the widest label so long city names are not clipped.
    float label_w = ImGui::CalcTextSize("Market catchments").x;
    for (const auto& [mid, col] : entries)
        label_w = std::max(label_w,
            swatch + 4.0f + ImGui::CalcTextSize(market_city_name(w, mid).c_str()).x);
    const float box_w = std::max(140.0f, label_w + 2.0f * pad);

    const float body_h = pad + kLensComboH + 4.0f + line_h + 4.0f
                       + static_cast<float>(std::max(n, 1)) * (swatch + 2.0f)
                       + pad;

    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    float x = p0.x + pad;
    float y = p0.y + pad * 0.5f;

    draw_lens_resource_combo(state, {x, y}, box_w - 2.0f * pad);
    y += kLensComboH + 4.0f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Market catchments");
    y += line_h + 4.0f;

    if (entries.empty())
    {
        dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "No markets");
        return;
    }

    for (const auto& [mid, col] : entries)
    {
        dl->AddRectFilled({x, y}, {x + swatch, y + swatch}, col);
        const std::string label = market_city_name(w, mid);
        dl->AddText({x + swatch + 4.0f, y}, IM_COL32(220, 220, 220, 255), label.c_str());
        y += swatch + 2.0f;
    }
}

/// On-canvas legend for the Country lens (BL-133): one colour swatch + nation name
/// per nation present on the active body, sorted by nation id for a stable order.
/// Modelled on draw_market_key; the box auto-sizes to the widest name so every
/// label guaranteed-fits (CalcTextSize pattern draw_market_key already uses).
/// Colour source is palette::nation_colour — the same source the tile tint itself
/// uses (the country lens's tile-tint pass, above).
void draw_country_key(ImDrawList* dl, ImVec2 anchor, const world& w, const ui_state& state)
{
    std::vector<entity_id> present;
    for (const auto& [tid, nid] : w.tile_to_nation)
    {
        const auto tile_it = w.tiles.find(tid);
        if (tile_it == w.tiles.end() || tile_it->second.body != state.active_body)
            continue;
        if (std::find(present.begin(), present.end(), nid) == present.end())
            present.push_back(nid);
    }
    std::sort(present.begin(), present.end());

    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float swatch = line_h;

    float name_w = ImGui::CalcTextSize("Countries").x;
    for (const entity_id nid : present)
    {
        const auto nat_it = w.nations.find(nid);
        if (nat_it == w.nations.end())
            continue;
        name_w = std::max(name_w, ImGui::CalcTextSize(nat_it->second.name.c_str()).x);
    }
    const float box_w  = pad * 2.0f + swatch + 4.0f + name_w;
    const int   n      = static_cast<int>(present.size());
    const float body_h = pad + line_h + 4.0f
                       + static_cast<float>(std::max(n, 1)) * (swatch + 2.0f)
                       + pad;

    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    float x = p0.x + pad;
    float y = p0.y + pad * 0.5f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Countries");
    y += line_h + 4.0f;

    if (present.empty())
    {
        dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "No nations");
        return;
    }

    for (const entity_id nid : present)
    {
        const auto nat_it = w.nations.find(nid);
        if (nat_it == w.nations.end())
            continue;
        dl->AddRectFilled({x, y}, {x + swatch, y + swatch}, palette::nation_colour(nid));
        dl->AddText({x + swatch + 4.0f, y}, IM_COL32(220, 220, 220, 255), nat_it->second.name.c_str());
        y += swatch + 2.0f;
    }
}

/// On-canvas legend for the Population lens: a low→high habitability gradient bar
/// (dark substrate → liveable green). Same left-edge placement as the other keys.
void draw_population_key(ImDrawList* dl, ImVec2 anchor)
{
    const float pad    = 8.0f;
    const float box_w  = 156.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x     = p0.x + pad;
    const float bar_w = box_w - 2.0f * pad;
    float       y     = p0.y + pad * 0.5f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Workforce efficiency");
    y += line_h + 4.0f;

    // Red→green rank bar (BL-135): mirrors the per-tile value-mark dot the lens
    // now draws instead of a full-tile tint — low efficiency reads red, full
    // labour reads green.
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = ryg_colour(t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low");
    const ImVec2 hts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - hts.x, y}, IM_COL32(170, 175, 185, 255), "high");
}

/// On-canvas legend for the Industry lens (BL-084): a low→high amber gradient bar
/// mapping the substrate-throughput tint (terrain hue → industrial amber), so the
/// field reads as "where the existing industry is densest". Same placement as the others.
void draw_industry_key(ImDrawList* dl, ImVec2 anchor)
{
    const float pad    = 8.0f;
    const float box_w  = 156.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x     = p0.x + pad;
    const float bar_w = box_w - 2.0f * pad;
    float       y     = p0.y + pad * 0.5f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Industry throughput");
    y += line_h + 4.0f;

    // Gradient mirrors the tile tint lerp (terrain hue -> industrial amber at
    // 0.15 + 0.6t), anchored on a neutral dark base so the bar reads standalone.
    constexpr ImU32 ind  = IM_COL32(210, 150, 70, 255); // industrial amber (tile pass)
    constexpr int   segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = lerp_colour(IM_COL32(40, 40, 48, 255), ind, 0.15f + 0.6f * t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low");
    const ImVec2 hts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - hts.x, y}, IM_COL32(170, 175, 185, 255), "high");
}

/// On-canvas legend for the Scarcity lens: an abundant→scarce gradient bar (no tint
/// → hot) plus the selected resource's name and swatch. Same placement as the others.
void draw_scarcity_key(ImDrawList* dl, ImVec2 anchor,
                       ui_state& state)
{
    const float pad    = 8.0f;
    const float box_w  = 156.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + kLensComboH + 4.0f
                       + line_h + 4.0f + bar_h + 2.0f + line_h + 4.0f + line_h + pad;
    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x     = p0.x + pad;
    const float bar_w = box_w - 2.0f * pad;
    float       y     = p0.y + pad * 0.5f;

    draw_lens_resource_combo(state, {x, y}, bar_w);
    y += kLensComboH + 4.0f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Market scarcity");
    y += line_h + 4.0f;

    // Met (substrate, no tint) → scarce (hot). Mirrors the per-market composite.
    constexpr ImU32 substrate = IM_COL32(40, 40, 48, 255);
    constexpr ImU32 hot       = IM_COL32(220, 70, 55, 255);
    constexpr int segs = 24;
    for (int i = 0; i < segs; ++i)
    {
        const float t0 = static_cast<float>(i) / segs;
        const ImU32 c  = lerp_colour(substrate, hot, t0);
        dl->AddRectFilled({ x + bar_w * t0, y },
                          { x + bar_w * static_cast<float>(i + 1) / segs, y + bar_h }, c);
    }
    y += bar_h + 2.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "met");
    const ImVec2 sts = ImGui::CalcTextSize("scarce");
    dl->AddText({x + bar_w - sts.x, y}, IM_COL32(170, 175, 185, 255), "scarce");
    y += line_h + 4.0f;

    dl->AddRectFilled({x, y + 2.0f}, {x + 10.0f, y + 12.0f},
                      presentation_of(state.lens_resource).colour);
    dl->AddText({x + 14.0f, y}, IM_COL32(235, 235, 235, 255),
                presentation_of(state.lens_resource).name);
}

/// Recency-tier colour shared by the Reach and Supply-routes keys, mirroring the
/// activity-fog convention (DISCOVERY.md, BL-089): fresh reads green, gone-cold
/// reads greyed. (No `visible` case here — a trade_route-only reach has no live
/// convoy/ownership signal to promote it past `known`.)
ImU32 reach_tier_colour(activity_vis tier)
{
    return (tier == activity_vis::known) ? palette::activity_known : palette::activity_stale;
}

/// A body this canvas's active body is connected to via a player `trade_route`,
/// tiered by recency. Shared shape for the Reach lens's pre-pass and key.
struct reach_link { entity_id other_body; activity_vis tier; };

/// One aggregated player trade lane touching the active body. Shared shape for
/// the Supply-routes lens's pre-pass and key.
struct supply_edge { entity_id other_body; int convoy_count; activity_vis tier; };

/// On-canvas legend for the Reach lens (BL-011): this canvas only ever shows the
/// active body's tile grid (never other bodies), so the "highlight the connected
/// bodies" the design calls for reads here as a list of the active body's own
/// connections (name + recency tier) — the body-marker glow the design describes
/// belongs on the Solar canvas, out of this lens work's file scope.
void draw_reach_key(ImDrawList* dl, ImVec2 anchor, const world& w,
                    const std::vector<reach_link>& links)
{
    const float pad     = 8.0f;
    const float box_w   = 176.0f;
    const float line_h  = ImGui::GetTextLineHeight();
    const int   n       = static_cast<int>(links.size());
    const float body_h  = pad + line_h + 4.0f + std::max(1, n) * (line_h + 2.0f) + pad;
    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x = p0.x + pad;
    float       y = p0.y + pad * 0.5f;
    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Reach (your trade network)");
    y += line_h + 4.0f;

    if (links.empty())
    {
        dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "no routes from this body");
        return;
    }
    for (const reach_link& link : links)
    {
        const auto  it   = w.bodies.find(link.other_body);
        const char* name = (it != w.bodies.end()) ? it->second.name.c_str() : "unknown body";
        const ImU32 c    = reach_tier_colour(link.tier);
        dl->AddCircleFilled({x + 4.0f, y + line_h * 0.5f}, 3.5f, c);
        dl->AddText({x + 12.0f, y}, c, name);
        y += line_h + 2.0f;
    }
}

/// On-canvas legend for the Supply-routes lens (BL-014): one row per aggregated
/// lane touching the active body — a thickness bar log-scaled from convoy_count
/// stands in for the edge-thickness encoding the design specifies for the
/// (out-of-scope-here) Solar-canvas graph rendering, and colour is the shared
/// recency tier.
void draw_supply_routes_key(ImDrawList* dl, ImVec2 anchor, const world& w,
                            const std::vector<supply_edge>& edges)
{
    const float pad     = 8.0f;
    const float box_w   = 176.0f;
    const float line_h  = ImGui::GetTextLineHeight();
    const float bar_max = 40.0f;
    const int   n       = static_cast<int>(edges.size());
    const float body_h  = pad + line_h + 4.0f + std::max(1, n) * (line_h + 2.0f) + pad;
    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x = p0.x + pad;
    float       y = p0.y + pad * 0.5f;
    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Supply routes");
    y += line_h + 4.0f;

    if (edges.empty())
    {
        dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "no lanes from this body");
        return;
    }
    for (const supply_edge& edge : edges)
    {
        const auto  it   = w.bodies.find(edge.other_body);
        const char* name = (it != w.bodies.end()) ? it->second.name.c_str() : "unknown body";
        const ImU32 c    = reach_tier_colour(edge.tier);
        // Log-scaled thickness (BL-014, settled): a bare completion reads as a
        // thin sliver; heavy repeat traffic saturates toward bar_max rather than
        // dominating the row linearly.
        const float thickness = std::clamp(3.0f + 6.0f * std::log(1.0f + static_cast<float>(edge.convoy_count)),
                                            3.0f, bar_max);
        dl->AddRectFilled({x, y + line_h * 0.5f - 2.0f}, {x + thickness, y + line_h * 0.5f + 2.0f}, c);
        dl->AddText({x + bar_max + 6.0f, y}, c, name);
        y += line_h + 2.0f;
    }
}

} // namespace

void update_body_vision(world& w, ui_state& state, double now_days)
{
    state.sim_now_days = now_days;
    state.permanent_vision.clear();
    state.convoy_beams.clear();

    const entity_id body = state.active_body;
    if (body == null_entity) return;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end()) return;
    const int gw = std::max(1, bit->second.grid_width);
    const int gh = std::max(1, bit->second.grid_height);
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (static_cast<int>(grid.size()) < gw * gh) return;

    // Odd-r offset neighbours (col, row deltas), matching the surface draw's grid.
    static const int even_off[6][2] =
        {{+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1}};
    static const int odd_off[6][2] =
        {{+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1}};

    // Flood `radius` hops out from `seeds` into `out` (odd-r neighbours, column wrap).
    auto flood = [&](const std::vector<entity_id>& seeds, int radius,
                     std::unordered_set<entity_id>& out) {
        std::vector<entity_id> frontier;
        for (const entity_id s : seeds)
            if (s != null_entity && out.insert(s).second) frontier.push_back(s);
        for (int r = 0; r < radius && !frontier.empty(); ++r)
        {
            std::vector<entity_id> next;
            for (const entity_id tid : frontier)
            {
                const auto tit = w.tiles.find(tid);
                if (tit == w.tiles.end()) continue;
                const tile_component& t = tit->second;
                const int (*off)[2] = (t.grid_y & 1) ? odd_off : even_off;
                for (int k = 0; k < 6; ++k)
                {
                    const int nrow = t.grid_y + off[k][1];
                    if (nrow < 0 || nrow >= gh) continue;
                    int ncol = (t.grid_x + off[k][0]) % gw;
                    if (ncol < 0) ncol += gw;
                    const entity_id nb = grid[static_cast<std::size_t>(nrow) * gw + ncol];
                    if (nb != null_entity && out.insert(nb).second) next.push_back(nb);
                }
            }
            frontier.swap(next);
        }
    };

    // The player's building tiles on this body + the markets whose catchment they sit
    // in — the markets the corp operates in (stable; buildings don't move). The centre
    // of operation is the lowest-id player building tile on the body (mirrors
    // market_clearing's file-local representative_tile, which is not header-visible).
    std::vector<entity_id> player_building_tiles;
    std::unordered_set<entity_id> operated_markets;
    entity_id centre         = null_entity;
    entity_id centre_building = null_entity;
    const auto pit = w.corporations.find(w.player_entity);
    if (pit != w.corporations.end())
    {
        for (const entity_id bid : pit->second.assets)
        {
            const auto bld = w.buildings.find(bid);
            if (bld == w.buildings.end()) continue;
            const entity_id t = bld->second.tile;
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end() || tit->second.body != body) continue;
            player_building_tiles.push_back(t);
            const entity_id mid = market_for_tile(w, t);
            if (mid != null_entity) operated_markets.insert(mid);
            if (centre_building == null_entity || bid < centre_building)
            {
                centre_building = bid;
                centre          = t;
            }
        }
    }

    // Layer 1 (permanent): radius-2 pockets around the player's own installations —
    // your buildings are always visible.
    flood(player_building_tiles, 2, state.permanent_vision);

    // Layer 2 (permanent): 3-wide corridors from the corp centre of operation to each
    // market centre it operates in. A 3-wide corridor is the path tiles flooded one hop
    // to each side.
    if (centre != null_entity)
    {
        for (const entity_id mid : operated_markets)
        {
            const auto mk = w.markets.find(mid);
            if (mk == w.markets.end()) continue;
            const entity_id mc = mk->second.centre_tile;
            if (mc == null_entity) continue;
            const logistics_path lp = intra_body_path(w, body, centre, mc);
            if (!lp.reachable || lp.tiles.empty()) continue;
            flood(lp.tiles, 1, state.permanent_vision);
        }
    }

    // Layer 3 (moving): the tile path + progress/speed of each live player intra-body
    // convoy, oriented src→dst. The renderer interpolates a head along it and trails a
    // dimming tail one econ tick's travel behind.
    for (const auto& cv : w.convoys)
    {
        if (cv.corp != w.player_entity) continue;
        const auto sm = w.markets.find(cv.source_market);
        const auto dm = w.markets.find(cv.dest_market);
        if (sm == w.markets.end() || dm == w.markets.end()) continue;
        if (sm->second.body != body || dm->second.body != body) continue;
        const entity_id st = sm->second.centre_tile;
        const entity_id dt = dm->second.centre_tile;
        if (st == null_entity || dt == null_entity) continue;
        const logistics_path lp = intra_body_path(w, body, st, dt);
        if (!lp.reachable || lp.tiles.empty()) continue;
        std::vector<entity_id> path = lp.tiles;
        if (st != std::min(st, dt)) std::reverse(path.begin(), path.end());
        state.convoy_beams.push_back(
            { std::move(path), std::clamp(cv.progress, 0.0f, 1.0f), std::max(cv.speed, 0.0f) });
    }
}

void draw_body_surface_canvas(const world& w, ui_state& state, const recipe_registry& reg,
                              const economy_report& report, ImVec2 origin, ImVec2 size,
                              bool input_enabled, ImVec2 lens_key_anchor)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    dl->AddRectFilled(origin, origin + size, IM_COL32(18, 18, 24, 255));

    // The surface canvas is the bottom rung of the ladder, so it is only ever
    // drawn as the primary view — never the minimap.
    const float min_dim    = std::min(size.x, size.y);
    const bool  draw_title = min_dim > 320.0f;

    auto body_it = w.bodies.find(state.active_body);
    if (body_it == w.bodies.end())
    {
        if (draw_title)
        {
            const char* msg = "No body selected";
            const ImVec2 ts = ImGui::CalcTextSize(msg);
            dl->AddText(origin + (size - ts) * 0.5f, IM_COL32(150, 150, 150, 255), msg);
        }
        return;
    }

    const body_component& body = body_it->second;

    float title_h = 0.0f;
    if (draw_title)
    {
        title_h = ImGui::GetTextLineHeightWithSpacing();
        // Survey status suffix (BL-067): a surveyed body shows none; others read
        // their phase so the locked surface is explained in the header.
        char survey_note[48] = "";
        switch (body.survey.phase)
        {
            case survey_phase::hidden:
                std::snprintf(survey_note, sizeof survey_note, "  -  UNSURVEYED"); break;
            case survey_phase::in_transit:
                std::snprintf(survey_note, sizeof survey_note, "  -  Survey en route"); break;
            case survey_phase::scanning:
                std::snprintf(survey_note, sizeof survey_note, "  -  Surveying %d/%d",
                    body.survey.regions_done, body.survey.regions_total); break;
            case survey_phase::surveyed: break;
        }
        char title[176];
        std::snprintf(title, sizeof(title), "%s  -  %s  (%dx%d)%s",
            body.name.c_str(), body_type_name(body.type), body.grid_width, body.grid_height, survey_note);
        dl->AddText(origin + ImVec2{4.0f, 2.0f}, IM_COL32(235, 235, 235, 255), title);
    }

    const int gw = std::max(1, body.grid_width);
    const int gh = std::max(1, body.grid_height);

    // Grid occupies the region below the title bar.
    const ImVec2 grid_area_origin = origin + ImVec2{0.0f, title_h};
    const ImVec2 grid_area_size   = { size.x, size.y - title_h };
    const ImVec2 canvas_centre    = grid_area_origin + grid_area_size * 0.5f;

    // --- Hex size at zoom=1 ---
    // zoom=1 is defined as "the full grid height fills the canvas height." The
    // grid width will typically exceed the canvas at this scale — the player pans
    // horizontally. This definition makes zoom=4/3 mean exactly "3/4 of the grid
    // height is visible" regardless of canvas size or grid aspect ratio, so the
    // same zoom value reads correctly on both the primary view and the minimap.
    const float fit_by_y = grid_area_size.y / (1.5f * static_cast<float>(gh) + 0.5f);
    const float hex_size = fit_by_y * kFitMargin;

    // Grid centre in local (unzoomed) world space — used to centre the grid
    // on the canvas at zoom=1 with no pan.
    const float col_step = kSqrt3 * hex_size;
    const float row_step = 1.5f * hex_size;
    const float grid_cx  = (static_cast<float>(gw) - 0.5f) * col_step * 0.5f;
    const float grid_cy  = static_cast<float>(gh - 1) * row_step * 0.5f;

    // --- View transform (pan/zoom) ---
    // The surface canvas is always primary, so pan and zoom always apply.
    const float  zoom        = std::clamp(state.planetary_zoom, kMinZoom, kMaxZoom);

    // Pending centre request (verify.center_tile): now that the exact grid
    // transform is in hand, set the pan that places the requested tile's local
    // centre on the canvas centre — pan = (grid_centre − tile_local) · zoom. This
    // is the one place the centring math lives; Lua callers never replicate it.
    if (state.planetary_center_pending)
    {
        const ImVec2 lc = hex_local_centre(state.planetary_center_col,
                                           state.planetary_center_row, hex_size);
        state.planetary_pan_x = (grid_cx - lc.x) * zoom;
        state.planetary_pan_y = (grid_cy - lc.y) * zoom;
        state.planetary_center_pending = false;
    }

    const ImVec2 view_origin = ImVec2{ canvas_centre.x + state.planetary_pan_x,
                                       canvas_centre.y + state.planetary_pan_y };

    // Local world space → screen space.
    auto to_screen = [&](ImVec2 lp) -> ImVec2 {
        return {
            view_origin.x + (lp.x - grid_cx) * zoom,
            view_origin.y + (lp.y - grid_cy) * zoom,
        };
    };

    // Rebuild per-frame marker hit-zone list (BL-059). Cleared here so every draw
    // call starts fresh; the tile loop and the market-centre pass below fill it.
    state.marker_hit_zones.clear();

    // Clip to the grid area so hexes don't overdraw the title bar or the solar canvas.
    dl->PushClipRect(grid_area_origin, grid_area_origin + grid_area_size, true);

    // Tiles that carry a building, mapped to their type so the marker pass can
    // draw the type-specific glyph. Also track the building entity per tile for
    // hit-zone registration (BL-059).
    std::unordered_map<entity_id, building_type> built_tiles;
    std::unordered_map<entity_id, entity_id>     tile_to_bld; // tile_id → building entity
    for (const auto& [bld_id, bld] : w.buildings)
    {
        auto tile_it = w.tiles.find(bld.tile);
        if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
        {
            built_tiles[bld.tile] = bld.type;
            tile_to_bld[bld.tile] = bld_id;
        }
    }

    // Spatial index: tile id keyed by its packed (col, row) on the active body,
    // so the Faction-lens border pass can find a tile's grid neighbours without
    // scanning w.tiles. Key packs row-major: row * gw + col (col < gw always).
    std::unordered_map<long long, entity_id> tile_at;
    for (const auto& [id, tile] : w.tiles)
    {
        if (tile.body == state.active_body)
            tile_at[static_cast<long long>(tile.grid_y) * gw + tile.grid_x] = id;
    }

    // Nation owner of a tile, or null_entity when the tile is absent from
    // tile_to_nation (unclaimed). Used by the border pass to compare adjacent
    // owners; "unclaimed" is treated as its own distinct owner.
    auto nation_of = [&](entity_id tile_id) -> entity_id {
        const auto it = w.tile_to_nation.find(tile_id);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };

    // Corporation owner of a tile, keyed by tile id. Built only when corporations
    // exist; drives both the building-marker colour and the Corporation lens tint.
    std::unordered_map<entity_id, entity_id> tile_to_corp;
    if (!w.corporations.empty())
    {
        for (const auto& [corp_id, corp] : w.corporations)
        {
            for (entity_id bld_id : corp.assets)
            {
                const auto bld_it = w.buildings.find(bld_id);
                if (bld_it != w.buildings.end())
                    tile_to_corp[bld_it->second.tile] = corp_id;
            }
        }
    }

    // Identity colour for a corporation: delegates to the shared palette source of
    // truth (BL-090) so the marker pass, the Corporation lens, the identity card,
    // the Selection header, and the corp emblem tags all agree.
    auto corp_identity = [&](entity_id corp_id) -> ImU32 {
        return palette::corp_identity_colour(corp_id, w.player_entity);
    };

    // Resource lens (BL-019): always single-resource. The lens fills the whole
    // contiguous deposit of the selected resource as a flat, uniform colour — the
    // *shape* of the deposit, not a magnitude gradient. A tile is part of the
    // deposit when it carries any of the resource (deposit > 0); contiguous tiles
    // form one blob (8-connected), but since the fill is uniform the per-tile
    // threshold is visually identical, so no flood-fill grouping is needed. No
    // normalisation maxima — the only state the pass needs is the selected index.

    // Market lens pre-pass (BL-015): assign each market on the active body a
    // distinct catchment colour (using corp palette slots, cycling if there are
    // more markets than slots). Tiles are tinted by their catchment market.
    // The old price-wash readout is relocated to the Market Ledger.
    std::unordered_map<entity_id, ImU32> market_catchment_colour;
    if (state.overlay == overlay_mode::market)
    {
        int slot = 0;
        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            market_catchment_colour[mid] =
                palette::corp_colour(slot % palette::corp_slot_count);
            ++slot;
        }
    }

    // Placement-suitability pre-pass (BL-010). A surface tied to *construction mode*,
    // not bare selection: it activates only while a build is armed
    // (state.construction.active) and is keyed to the armed building type/target, so a
    // plain inspection click never re-skins the map (it used to fire on any tile
    // selection, reading as a spurious lens change). Tints every other tile by how
    // well the *armed* building would do there.
    const bool          suitability_active = state.construction.active;
    const building_type suitability_btype  = state.construction.type;
    const resource_type suitability_target = state.construction.target;
    // "Affine" (thrives-here) applies to extraction only: a tile whose own richest
    // extractable resource is the armed target reads as optimal. Other building types
    // carry no terrain-affinity signal — valid-but-not-affine tiles stay uncoloured.
    const bool          suitability_affine_kind =
        (suitability_btype == building_type::extraction_site);

    // Scarcity lens pre-pass (BL-018): a market-level field, not a per-tile one.
    // Scarcity is driven by **supply shortfall** — how much demand outran supply
    // last tick for the selected good, per market (independent of price). Each
    // market's catchment tiles read as one chunky block (market_for_tile). We
    // collect per-market shortfall and the body-max for normalisation; a tile then
    // looks up its market's shortfall and tints uniformly across the catchment.
    std::unordered_map<entity_id, float> scar_shortfall; // market id → shortfall of sel
    float scar_max_shortfall = 0.0f;
    if (state.overlay == overlay_mode::scarcity)
    {
        const std::size_t sel = static_cast<std::size_t>(state.lens_resource);
        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            const float shortfall = std::max(0.0f, mk.demand[sel] - mk.supply[sel]);
            scar_shortfall[mid] = shortfall;
            scar_max_shortfall  = std::max(scar_max_shortfall, shortfall);
        }
    }

    // Production lens pre-pass (BL-009): per producing tile, the sell value of its
    // building's outputs this tick = Σ(output qty × resolved price). Read from the
    // economy report (output_quantity) + the tile's market prices; a processor's
    // total output is split across its recipe's products by their batch
    // proportions. Idle / exhausted buildings produce nothing → no entry → cold.
    // The geometric-style mean of producing tiles anchors the log scale.
    std::unordered_map<entity_id, float> prod_value; // tile id → output sell value
    float prod_log_sum = 0.0f;
    int   prod_count   = 0;
    if (state.overlay == overlay_mode::production)
    {
        for (const building_report& br : report.buildings)
        {
            if (br.body != state.active_body || br.output_quantity <= 0.0f || !br.active)
                continue;
            const auto bld_it = w.buildings.find(br.building);
            if (bld_it == w.buildings.end())
                continue;
            const entity_id tile_id = bld_it->second.tile;
            const entity_id mid     = market_for_tile(w, tile_id);
            const auto      mk_it   = w.markets.find(mid);
            if (mk_it == w.markets.end())
                continue;
            const auto& price = mk_it->second.price;

            float value = 0.0f;
            if (br.type == building_type::extraction_site)
            {
                value = br.output_quantity * price[static_cast<std::size_t>(br.target_resource)];
            }
            else if (const recipe* rec = reg.get_recipe(br.recipe))
            {
                float out_total = 0.0f, weighted = 0.0f;
                for (std::size_t r = 0; r < resource_count; ++r)
                {
                    out_total += rec->outputs[r];
                    weighted  += rec->outputs[r] * price[r];
                }
                if (out_total > 0.0f)
                    value = br.output_quantity * weighted / out_total;
            }
            if (value > 0.0f)
            {
                prod_value[tile_id] += value;
                prod_log_sum += std::log(value);
                ++prod_count;
            }
        }
    }
    const float prod_mean = prod_count > 0 ? std::exp(prod_log_sum / static_cast<float>(prod_count)) : 0.0f;

    // Opportunity lens pre-pass (BL-136): a body-relative, volume-weighted rank of
    // the unmet-demand gap, replacing the old max-absolute price/base ratio (which
    // saturated green in the saturated economy — every market bids over its floor).
    // For each market: sum the per-good demand gap (demand outrunning supply, like
    // the Scarcity pre-pass) and weight it by the market's total demand volume, so
    // a large market's real gap outranks a tiny market's high-ratio blip. Scores are
    // then ranked against the body max (mirrors the Scarcity lens's normalisation) —
    // strongest gaps read green, met markets read low/red.
    std::unordered_map<entity_id, float> opp_score; // market id → volume-weighted demand-gap score
    float opp_max_score = 0.0f;
    if (state.overlay == overlay_mode::opportunity)
    {
        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            float gap = 0.0f, volume = 0.0f;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                gap    += std::max(0.0f, mk.demand[r] - mk.supply[r]);
                volume += mk.demand[r];
            }
            const float score = gap * volume;
            opp_score[mid] = score;
            opp_max_score  = std::max(opp_max_score, score);
        }
    }

    // Supply lens pre-pass: check whether the active body has any player convoys
    // (source or destination). Used inside the tile loop to gate the per-tile glyph.
    // w.convoys is empty until the dispatch system lands; supply_active stays false.
    bool supply_active = false;
    if (state.overlay == overlay_mode::supply)
    {
        for (const auto& cv : w.convoys)
        {
            if (cv.corp != w.player_entity)
                continue;
            entity_id src_body = null_entity;
            entity_id dst_body = null_entity;
            const auto smk = w.markets.find(cv.source_market);
            const auto dmk = w.markets.find(cv.dest_market);
            if (smk != w.markets.end()) src_body = smk->second.body;
            if (dmk != w.markets.end()) dst_body = dmk->second.body;
            if (src_body == state.active_body || dst_body == state.active_body)
            {
                supply_active = true;
                break;
            }
        }
    }

    // Industry lens pre-pass (BL-084): render the already-live nation-owned
    // substrate (tile.substrate_density, injected as background supply/demand each
    // tick) as an economic-throughput field. Raw density ripples out from population
    // centres and so reads collinear with the Population lens; weighting occupation
    // by the tile's terrain resource richness decouples it — the field reads "where
    // the existing *industry* is", brightest where dense occupation meets rich
    // terrain. Pure rendering: no change to substrate_density or the market maths.
    std::unordered_map<entity_id, float> industry_field;
    float industry_max = 0.0f;
    if (state.overlay == overlay_mode::industry)
    {
        float max_dep = 0.0f;
        for (const auto& [tid, tile] : w.tiles)
        {
            if (tile.body != state.active_body)
                continue;
            float ds = 0.0f;
            for (const float d : tile.resource_deposit) ds += d;
            max_dep = std::max(max_dep, ds);
        }
        for (const auto& [tid, tile] : w.tiles)
        {
            if (tile.body != state.active_body || tile.substrate_density <= 0.0f)
                continue;
            float ds = 0.0f;
            for (const float d : tile.resource_deposit) ds += d;
            const float dep_norm = (max_dep > 0.0f) ? ds / max_dep : 0.0f;
            const float thr = tile.substrate_density * (0.35f + 0.65f * dep_norm);
            industry_field[tid] = thr;
            industry_max = std::max(industry_max, thr);
        }
    }

    // Reach lens pre-pass (BL-011): `trade_route` is body-level, not per-tile, and
    // this canvas only ever shows the *active* body's tile grid (never other
    // bodies), so the "body-to-body highlight" the design calls for reads here as
    // a per-body connectivity readout for the active body — every other endpoint
    // it is connected to, tiered by recency. Player's own routes only, per the
    // competitor-visibility rule (rival lanes stay private). No falloff/distance
    // model: trade_route carries none to derive one from.
    std::vector<reach_link> reach_links;
    if (state.overlay == overlay_mode::reach)
    {
        for (const trade_route& route : w.trade_routes)
        {
            if (route.corp != w.player_entity)
                continue;
            entity_id other = null_entity;
            if (route.body_a == state.active_body)      other = route.body_b;
            else if (route.body_b == state.active_body) other = route.body_a;
            else                                        continue;
            const bool fresh = (w.current_day_tick - route.last_tick) <= route_fresh_ticks_default;
            reach_links.push_back({other, fresh ? activity_vis::known : activity_vis::known_stale});
        }
    }

    // Supply-routes lens pre-pass (BL-014): one aggregated edge per unordered
    // (body_a, body_b) pair touching the active body, built from `w.trade_routes`
    // at render time (not per-frame convoy positions). `trade_route` is already
    // upserted per (pair, corp), so filtering to the player's corp yields at most
    // one entry per pair directly — no further aggregation needed. Thickness is
    // log-scaled convoy_count; colour/alpha is the same recency tier as Reach.
    std::vector<supply_edge> supply_edges;
    if (state.overlay == overlay_mode::supply_routes)
    {
        for (const trade_route& route : w.trade_routes)
        {
            if (route.corp != w.player_entity)
                continue;
            entity_id other = null_entity;
            if (route.body_a == state.active_body)      other = route.body_b;
            else if (route.body_b == state.active_body) other = route.body_a;
            else                                        continue;
            const bool fresh = (w.current_day_tick - route.last_tick) <= route_fresh_ticks_default;
            supply_edges.push_back({other, route.convoy_count,
                                    fresh ? activity_vis::known : activity_vis::known_stale});
        }
    }

    // Intra-body vision — the moving convoy beam (BL-152/154). Permanent vision
    // (state.permanent_vision: radius-2 pockets around the player's buildings + 3-wide
    // corridors from the corp centre of operation to each market centre it operates in)
    // is precomputed each frame in update_body_vision. Here we add the *moving* beam:
    // for each live player convoy we interpolate a HEAD along its path by the fraction
    // through the current econ tick — so it glides smoothly between quarterly steps —
    // and trail a dimming TAIL one econ tick's travel behind it. beam_intensity holds
    // each lit tile's brightness (0..1); the tile loop blends it with permanent vision
    // to size the fog wash. Survey fog (BL-067) still owns unrevealed tiles.
    std::unordered_map<entity_id, float> beam_intensity;
    {
        // Fraction through the current econ tick (90 days); mirrors sim_loop::econ_tick_days.
        const float frac = static_cast<float>(
            std::fmod(std::max(0.0, state.sim_now_days), 90.0) / 90.0);
        constexpr int beam_radius = 2; // Ben's spec: a radius-2 beam of vision.
        static const int even_off[6][2] =
            {{+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1}};
        static const int odd_off[6][2] =
            {{+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1}};

        // Flood radius-2 around a path tile, marking each at `inten` (keeping the max).
        auto light = [&](entity_id centre_tile, float inten) {
            if (centre_tile == null_entity) return;
            std::unordered_set<entity_id> seen;
            std::vector<entity_id> frontier{ centre_tile };
            seen.insert(centre_tile);
            { float& v = beam_intensity[centre_tile]; if (inten > v) v = inten; }
            for (int r = 0; r < beam_radius && !frontier.empty(); ++r)
            {
                std::vector<entity_id> next;
                for (const entity_id tid : frontier)
                {
                    const auto tit = w.tiles.find(tid);
                    if (tit == w.tiles.end()) continue;
                    const tile_component& t = tit->second;
                    const int (*off)[2] = (t.grid_y & 1) ? odd_off : even_off;
                    for (int k = 0; k < 6; ++k)
                    {
                        const int nrow = t.grid_y + off[k][1];
                        if (nrow < 0 || nrow >= gh) continue;
                        int ncol = (t.grid_x + off[k][0]) % gw;
                        if (ncol < 0) ncol += gw;
                        const auto nb = tile_at.find(static_cast<long long>(nrow) * gw + ncol);
                        if (nb == tile_at.end()) continue;
                        if (seen.insert(nb->second).second)
                        {
                            next.push_back(nb->second);
                            float& v = beam_intensity[nb->second]; if (inten > v) v = inten;
                        }
                    }
                }
                frontier.swap(next);
            }
        };

        for (const auto& cb : state.convoy_beams)
        {
            const int n = static_cast<int>(cb.path.size());
            if (n == 0) continue;
            // Head glides: last econ-step progress + this tick's fraction of a step.
            const float p    = std::clamp(cb.progress + cb.speed * frac, 0.0f, 1.0f);
            const int   head = std::clamp(static_cast<int>(std::lround(p * (n - 1))), 0, n - 1);
            // Tail = one econ tick's travel in tiles (>=1), dimming to 0 at its far end.
            const int   tail = std::max(1, static_cast<int>(std::lround(cb.speed * (n - 1))));
            for (int i = head; i >= 0 && i >= head - tail; --i)
            {
                const float inten = 1.0f - static_cast<float>(head - i) / static_cast<float>(tail);
                if (inten > 0.0f) light(cb.path[static_cast<std::size_t>(i)], inten);
            }
        }
    }

    const ImVec2 mouse = state.mouse.active
                         ? ImVec2{state.mouse.x, state.mouse.y}
                         : ImVec2{-1.0f, -1.0f}; // off-screen sentinel suppresses hover

    // Hover resolves to a single tile copy. Adjacent hexes' circular hit-tests
    // overlap, and the cylinder draws several wrap copies of each tile, so more
    // than one (tile, copy) can satisfy the hover test at once. The nearest hex
    // centre to the cursor wins; its outline is drawn after the grid pass so only
    // one tile highlights (see the highlight tie convention in highlight.hpp).
    entity_id hovered_tile     = null_entity;
    bool      hovered_selected = false;
    bool      have_hover       = false;
    float     best_d2          = std::numeric_limits<float>::max();
    ImVec2    hover_verts[6]   = {};

    // Slightly shrink the drawn hex so the background shows through as a border.
    // Use the full circumradius for hit-testing so small hexes stay clickable.
    const float draw_r = hex_size * zoom - 1.0f;
    const float hit_r  = hex_size * zoom;

    // --- Infinite horizontal scroll ---
    // The grid is a cylinder: column gw wraps onto column 0. In screen space the
    // grid repeats every `period_px = gw * col_step * zoom`. Each tile is drawn
    // (and hit-tested) at every integer wrap offset k whose copy falls within the
    // canvas, so panning past either edge continues seamlessly from the far side.
    const float period_px = static_cast<float>(gw) * col_step * zoom;
    const float visible_left  = grid_area_origin.x - hit_r;
    const float visible_right = grid_area_origin.x + grid_area_size.x + hit_r;

    // Draw the active body's tiles in a deterministic order. `w.tiles` is an
    // unordered_map, whose iteration order varies between process runs (hash
    // seeding) — that reorders overlapping antialiased hex edges and markers and
    // makes full-body golden captures flake by ~1–2%. Sorting the tile ids fixes
    // the draw order so a capture is reproducible run-to-run. (`tile_at` already
    // holds exactly the active body's tiles, keyed by packed grid coordinate.)
    std::vector<entity_id> draw_order;
    draw_order.reserve(tile_at.size());
    for (const auto& [key, tid] : tile_at)
        draw_order.push_back(tid);
    std::sort(draw_order.begin(), draw_order.end());

    for (const entity_id id : draw_order)
    {
        const tile_component& tile = w.tiles.at(id);
        if (tile.body != state.active_body)
            continue;

        const ImVec2 lc   = hex_local_centre(tile.grid_x, tile.grid_y, hex_size);
        const ImVec2 sc   = to_screen(lc);

        // Fill is the tile's terrain colour, except under the Faction lens where
        // a tile owned by a nation is tinted that nation's identity colour. The
        // tint is a direct replacement (no blend); unclaimed tiles — absent from
        // tile_to_nation, e.g. ocean — keep their terrain hue so the political
        // map still reads as terrain underneath.
        ImU32 fill = terrain_colour(tile.composition);
        if (state.overlay == overlay_mode::country)
        {
            const auto nat_it = w.tile_to_nation.find(id);
            if (nat_it != w.tile_to_nation.end())
                fill = palette::nation_colour(nat_it->second);
        }
        // Corporation lens: tint a tile that carries a corporate building with its
        // owning corp's colour (direct replacement, like the faction tint). Tiles
        // with no corporate building keep their terrain hue — no nation underlay.
        else if (state.overlay == overlay_mode::corporation)
        {
            const auto corp_it = tile_to_corp.find(id);
            if (corp_it != tile_to_corp.end())
                fill = corp_identity(corp_it->second);
        }
        // Resource lens (BL-019): flat, uniform fill over the contiguous deposit of
        // the selected resource — the *shape* of the deposit, no magnitude gradient.
        // Any tile carrying the resource (deposit > 0) is part of the deposit and
        // takes the resource's identity colour at a fixed opacity; a tile without it
        // keeps its terrain hue. Intensity lives in tile detail, not the lens.
        else if (state.overlay == overlay_mode::resource)
        {
            const std::size_t sel = static_cast<std::size_t>(state.lens_resource);
            if (tile.resource_deposit[sel] > 0.0f)
                fill = lerp_colour(fill, presentation_of(state.lens_resource).colour, 0.8f);
        }
        // Market lens (BL-015): tint each tile with its catchment market's colour
        // so the boundary between markets reads as a colour boundary. Same visual
        // logic as the Corporation lens. Price readout is in the Market Ledger.
        else if (state.overlay == overlay_mode::market)
        {
            const entity_id mid = market_for_tile(w, id);
            const auto col_it   = market_catchment_colour.find(mid);
            if (col_it != market_catchment_colour.end())
                fill = lerp_colour(fill, col_it->second, 0.55f);
        }
        // Population (Workforce) and Opportunity lenses (BL-135): no longer a
        // full-tile tint — each reads as a per-tile red→green dot mark, drawn
        // below alongside the building glyph (workforce_efficiency / the
        // body-relative demand-gap rank respectively). Tiles keep terrain hue here.
        // Production lens (BL-009): tint a producing tile by output sell value this
        // tick, log-scaled relative to the body's producing-tile mean (above mean
        // warm, below cool). Idle / exhausted / unbuilt tiles read cold (no tint).
        else if (state.overlay == overlay_mode::production)
        {
            const auto it = prod_value.find(id);
            if (it != prod_value.end() && prod_mean > 0.0f)
                fill = lerp_colour(fill, production_colour(it->second / prod_mean), 0.6f);
        }
        // Scarcity lens (BL-018): a market-level shortfall field. Every tile in a
        // market's catchment reads as one chunky block tinted by that market's
        // supply shortfall of the selected good (demand outran supply last tick),
        // normalised across the body's markets. Hot where scarce; no tint where met.
        else if (state.overlay == overlay_mode::scarcity)
        {
            const auto sf_it = scar_shortfall.find(market_for_tile(w, id));
            if (sf_it != scar_shortfall.end() && scar_max_shortfall > 0.0f)
            {
                const float scar = std::clamp(sf_it->second / scar_max_shortfall, 0.0f, 1.0f);
                constexpr ImU32 hot = IM_COL32(220, 70, 55, 255);
                fill = lerp_colour(fill, hot, 0.6f * scar);
            }
        }
        // Industry lens (BL-084): tint a tile by the nation-substrate throughput field
        // (occupation weighted by terrain richness, pre-pass above), normalised to the
        // body max — brightest amber where the existing industry is densest. Tiles with
        // no substrate keep their terrain hue.
        else if (state.overlay == overlay_mode::industry)
        {
            const auto it = industry_field.find(id);
            if (it != industry_field.end() && industry_max > 0.0f)
            {
                const float t = std::clamp(it->second / industry_max, 0.0f, 1.0f);
                constexpr ImU32 ind = IM_COL32(210, 150, 70, 255); // industrial amber
                fill = lerp_colour(fill, ind, 0.15f + 0.6f * t);
            }
        }
        // Player-owned tile? Drives the persistent, lens-independent ownership
        // accent: a subtle wash at the plain default plus an outline under every
        // lens (drawn in the border pass), so "these tiles are mine" reads without
        // the player having to pick the Corporation lens.
        bool is_player_tile = false;
        if (!tile_to_corp.empty())
        {
            const auto pc_it = tile_to_corp.find(id);
            is_player_tile = (pc_it != tile_to_corp.end() && pc_it->second == w.player_entity);
        }
        // Plain-default wash: tint the player's own tiles with the player identity
        // colour when no lens is active. Suppressed under any lens — the outline
        // carries identity there, so the wash never fights a lens fill.
        if (is_player_tile && state.overlay == overlay_mode::none)
            fill = lerp_colour(fill, corp_identity(w.player_entity), 0.30f);

        const auto   built_it  = built_tiles.find(id);
        const bool   built     = built_it != built_tiles.end();
        const building_type built_type = built ? built_it->second : building_type::none;
        const bool   selected  = (id == state.selected_entity);

        // Placement-suitability overlay (BL-010): active only in construction mode.
        // Invalid tiles (can't build) darken; affine tiles (the armed target is their
        // richest extractable) tint green. Skips the selected tile (already outlined).
        if (suitability_active && !selected)
        {
            const bool placeable = placement_rules::can_place(
                tile, suitability_btype, suitability_target);
            if (!placeable)
                fill = lerp_colour(fill, IM_COL32(0, 0, 0, 255), 0.35f);
            else if (suitability_affine_kind)
            {
                bool any_dep = false;
                const resource_type best = placement_rules::richest_extractable(tile, any_dep);
                if (any_dep && best == suitability_target)
                    fill = lerp_colour(fill, IM_COL32(100, 200, 100, 255), 0.24f);
            }
        }

        // Survey mask (BL-067): tiles in regions not yet revealed render as a dark
        // locked overlay with no lens detail, borders, markers, or hit-testing — the
        // locked fill is drawn, then the per-copy detail is skipped. A fully surveyed
        // body (home, the star, or a completed survey) reveals everything.
        const bool revealed = survey_tile_visible(body.survey, gw, gh, tile.grid_x, tile.grid_y);
        if (!revealed)
            fill = IM_COL32(12, 14, 20, 255);

        // Intra-body vision fog (BL-151/152/154): dim any *revealed* tile by how little
        // the player sees it. Vision = 1 in permanent vision (building pockets + the
        // corp-centre→market corridors, from update_body_vision); otherwise the moving
        // convoy beam's contribution (bright at the head, dimming down the tail). The
        // fog wash scales with (1 − vision), so the surface reads mostly unknown, lit
        // along the player's corridors, with a convoy's beam gliding and trailing over
        // them. Survey mask owns unrevealed tiles, so this skips them.
        if (revealed)
        {
            float vision = (state.permanent_vision.find(id) != state.permanent_vision.end())
                               ? 1.0f : 0.0f;
            if (vision < 1.0f)
            {
                const auto bi = beam_intensity.find(id);
                if (bi != beam_intensity.end())
                    vision = std::max(vision, bi->second);
            }
            if (vision < 1.0f)
                fill = lerp_colour(fill, IM_COL32(8, 10, 16, 255), 0.5f * (1.0f - vision));
        }

        // Range of wrap copies that land inside the canvas horizontally.
        const int k_min = (period_px > 0.0f)
            ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
        const int k_max = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;

        for (int k = k_min; k <= k_max; ++k)
        {
            const float cx = sc.x + static_cast<float>(k) * period_px;
            const float cy = sc.y;

            ImVec2 verts[6];
            hex_vertices(verts, cx, cy, draw_r);
            dl->AddConvexPolyFilled(verts, 6, fill);

            // Masked region: locked fill only — no borders, markers, selection, or
            // hit-testing for this copy. This single gate also *is* the rival-marker
            // visibility rule (BL-068): a marker — yours or a rival's — shows iff its
            // tile sits in a survey-revealed region, so no separate per-owner gate is
            // needed on the marker pass below. The competitor information asymmetry
            // lives at read time in the hover card and selection panel, not here.
            if (!revealed)
                continue;

            // Nation borders (Country lens only). Draw a dark line on every hex
            // edge shared with a neighbour of a different owner — including the
            // claimed/unclaimed boundary. The grid is odd-r offset, so the six
            // neighbour offsets differ between even and odd rows.
            if (state.overlay == overlay_mode::country)
            {
                const entity_id own_nation = nation_of(id);

                // Standard odd-r neighbour offsets (col, row deltas).
                static const int even_off[6][2] =
                    {{+1, 0}, {0, -1}, {-1, -1}, {-1, 0}, {-1, +1}, {0, +1}};
                static const int odd_off[6][2] =
                    {{+1, 0}, {+1, -1}, {0, -1}, {-1, 0}, {0, +1}, {+1, +1}};
                const int (*off)[2] = (tile.grid_y & 1) ? odd_off : even_off;

                for (int n = 0; n < 6; ++n)
                {
                    const int nrow = tile.grid_y + off[n][1];
                    if (nrow < 0 || nrow >= gh)
                        continue; // Off the top/bottom edge: no neighbour tile.

                    // Columns wrap on the horizontal cylinder.
                    int ncol = (tile.grid_x + off[n][0]) % gw;
                    if (ncol < 0)
                        ncol += gw;

                    const auto nb_it = tile_at.find(static_cast<long long>(nrow) * gw + ncol);
                    if (nb_it == tile_at.end())
                        continue;
                    if (nation_of(nb_it->second) == own_nation)
                        continue; // Same owner: interior edge, no border.

                    // Draw the shared edge via the midpoint-perpendicular method:
                    // place the segment at the midpoint of the centre-to-centre
                    // line, perpendicular to it, with length equal to one hex side
                    // (== circumradius draw_r for a regular hexagon). This avoids
                    // mapping neighbour directions to per-vertex pairs, which the
                    // offset-row vertex ordering makes error-prone. The neighbour's
                    // screen centre is taken at the SAME wrap offset k as this tile.
                    const ImVec2 nb_lc = hex_local_centre(ncol, nrow, hex_size);
                    ImVec2 nb_sc = to_screen(nb_lc);
                    nb_sc.x += static_cast<float>(k) * period_px;

                    float dirx = nb_sc.x - cx;
                    float diry = nb_sc.y - cy;
                    const float len = std::sqrt(dirx * dirx + diry * diry);
                    if (len <= 0.0f)
                        continue;
                    dirx /= len;
                    diry /= len;

                    const float mx = (cx + nb_sc.x) * 0.5f;
                    const float my = (cy + nb_sc.y) * 0.5f;
                    const float px = -diry; // perpendicular to the centre line
                    const float py =  dirx;
                    const float half = draw_r * 0.5f;

                    dl->AddLine({mx - px * half, my - py * half},
                                {mx + px * half, my + py * half},
                                IM_COL32(20, 20, 20, 200), 1.5f);
                }
            }

            // Persistent player footprint: outline the player's own tiles under
            // EVERY lens (and the plain default), so "these are mine" never
            // disappears when a lens is picked. Under the Corporation lens the fill
            // is already the player colour, so the bright selection accent keeps the
            // edge visible; elsewhere the player identity colour reads as ownership
            // against terrain and value fills. Rival tiles carry no ring.
            if (is_player_tile)
            {
                const ImU32 ring = (state.overlay == overlay_mode::corporation)
                    ? palette::selection : corp_identity(w.player_entity);
                dl->AddPolyline(verts, 6, ring, ImDrawFlags_Closed, 2.0f);
            }

            if (built)
            {
                const float mr = std::max(2.0f, draw_r * 0.22f);

                // Building markers carry their owning corporation's colour (the
                // player's corp gets faction slot 0). Always on, independent of
                // the lens. Tiles with no corporate owner stay white.
                ImU32 marker_col = IM_COL32(255, 255, 255, 255);
                const auto corp_it = tile_to_corp.find(id);
                if (corp_it != tile_to_corp.end())
                    marker_col = corp_identity(corp_it->second);

                // The Workforce (Population lens) and Opportunity lenses replace the
                // building silhouette with the per-tile value mark drawn below
                // (BL-135) — the mark reads the tile's rank, not its installation.
                if (state.overlay != overlay_mode::population &&
                    state.overlay != overlay_mode::opportunity)
                    icons::building(dl, {cx, cy}, mr, built_type, marker_col);

                // Owner-identity tag (BL-090): a small corp emblem tucked at the
                // building glyph's lower-right, for BOTH player and rival buildings —
                // the owning corp is public under the BL-068 visibility model, so this
                // adds no leak. Shape + colour route through the shared palette source
                // of truth, so the tag matches the identity card and the Selection
                // header. Kept small (~0.55x the building r) and offset so it never
                // occludes the silhouette; does not affect hit-testing.
                if (corp_it != tile_to_corp.end())
                {
                    const entity_id owner = corp_it->second;
                    const float     er    = std::max(1.5f, mr * 0.55f);
                    icons::corp_emblem(dl, {cx + mr * 0.9f, cy + mr * 0.9f}, er,
                                       palette::corp_emblem_shape(owner),
                                       palette::corp_identity_colour(owner, w.player_entity));
                }

                // Register hit zone (BL-059). Only the k==0 copy per tile so
                // wrap copies don't produce duplicate zones; the single zone
                // records the canonical screen position for this frame.
                if (k == 0)
                {
                    const auto bld_it = tile_to_bld.find(id);
                    if (bld_it != tile_to_bld.end())
                    {
                        marker_hit_zone hz;
                        hz.id     = bld_it->second;
                        hz.kind   = marker_hit_zone::kind::building;
                        hz.centre = {cx, cy};
                        hz.radius = mr * 2.0f;
                        state.marker_hit_zones.push_back(hz);
                    }
                }
            }

            // Value-lens tile marks (BL-135): Workforce (Population lens) and
            // Opportunity replace their old full-tile tint with a per-tile red→green
            // dot on every BUILDABLE tile (valid terrain for activity — ocean
            // excluded), not just occupied ones. Workforce reads
            // workforce_efficiency(habitability); Opportunity reads the same
            // body-relative demand-gap rank as its (now-removed) tile tint (BL-136).
            // Drawn instead of, not blended with, the building glyph on occupied
            // tiles (suppressed above).
            if ((state.overlay == overlay_mode::population ||
                 state.overlay == overlay_mode::opportunity) &&
                !placement_rules::is_ocean_tile(tile.composition))
            {
                float t = 0.0f; // body-relative rank, [0, 1], red(low) -> green(high)
                if (state.overlay == overlay_mode::population)
                {
                    t = workforce_efficiency(std::clamp(tile.habitability, 0.0f, 1.0f));
                }
                else // opportunity
                {
                    const auto it = opp_score.find(market_for_tile(w, id));
                    if (it != opp_score.end() && opp_max_score > 0.0f)
                        t = std::clamp(it->second / opp_max_score, 0.0f, 1.0f);
                }
                const float mr = std::max(2.0f, draw_r * 0.22f);
                icons::value_mark(dl, {cx, cy}, mr, ryg_colour(t));
            }

            // Supply lens: draw a convoy glyph on every tile when the active body
            // has a player convoy passing through it. supply_active is false when
            // w.convoys is empty, so this is a no-op until dispatch is wired.
            if (supply_active)
            {
                constexpr ImU32 supply_col = IM_COL32(80, 200, 255, 200);
                const float gr = std::max(2.0f, draw_r * 0.28f);
                icons::convoy(dl, {cx, cy}, gr, supply_col);
            }

            // Selection outline is drawn on every visible copy of the selected
            // tile; hover is deferred to a single nearest copy, resolved below.
            draw_hex_highlight(dl, verts,
                resolve_highlight(selected, /*hovered=*/false, /*pinned=*/false));

            // Hit-test: distance to hex centre < circumradius (approximate,
            // sufficient for usability). Scoped per wrap copy so the highlight
            // lands on the copy actually under the cursor; nearest centre wins.
            if (input_enabled)
            {
                const float dx = mouse.x - cx;
                const float dy = mouse.y - cy;
                const float d2 = dx * dx + dy * dy;
                const bool in_area = mouse.x >= grid_area_origin.x &&
                                     mouse.x <= grid_area_origin.x + grid_area_size.x &&
                                     mouse.y >= grid_area_origin.y &&
                                     mouse.y <= grid_area_origin.y + grid_area_size.y;
                if (in_area && d2 <= hit_r * hit_r && d2 < best_d2)
                {
                    best_d2          = d2;
                    hovered_tile     = id;
                    hovered_selected = selected;
                    have_hover       = true;
                    std::copy(std::begin(verts), std::end(verts), std::begin(hover_verts));
                }
            }
        }
    }

    // Hover outline for the single resolved copy. Skipped when the hovered tile is
    // also the selection — selection outranks hover, and its ring is already drawn.
    if (have_hover && !hovered_selected)
        draw_hex_highlight(dl, hover_verts, highlight::hovered);

    // Market-centre markers (BL-059). Draw a circle+cross glyph at each market's
    // centre tile position and register a hit zone for click-selection (BL-031).
    // Drawn after the tile loop so markers sit above all tile chrome. Only markets
    // anchored to the active body (and with a valid centre_tile) are shown.
    {
        constexpr ImU32 mkt_col = IM_COL32(255, 220, 80, 220);
        const float mkt_r = std::max(3.0f, draw_r * 0.28f);

        for (const auto& [mid, mk] : w.markets)
        {
            if (mk.body != state.active_body)
                continue;
            if (mk.centre_tile == null_entity)
                continue;
            const auto ctc_it = w.tiles.find(mk.centre_tile);
            if (ctc_it == w.tiles.end())
                continue;
            const tile_component& ctc = ctc_it->second;
            const ImVec2 lc = hex_local_centre(ctc.grid_x, ctc.grid_y, hex_size);
            const ImVec2 sc = to_screen(lc);

            // Draw on every visible wrap copy.
            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const ImVec2 mc = {sc.x + static_cast<float>(k) * period_px, sc.y};
                icons::market_centre(dl, mc, mkt_r, mkt_col);
            }

            // Hit zone on the canonical copy (k==0).
            marker_hit_zone hz;
            hz.id     = mid;
            hz.kind   = marker_hit_zone::kind::market_centre;
            hz.centre = sc;
            hz.radius = mkt_r * 2.0f;
            state.marker_hit_zones.push_back(hz);
        }
    }

    // Population-centre markers (BL-083). The generated settlements are drawn as
    // always-on civic chrome (not lens-gated) so the surface reads as inhabited, not
    // "resources with industry on top". Contiguous centres are clustered into
    // conurbations for display — a handful of legible cities + towns rather than a
    // dust of villages — each rendered at its highest-scale member with a tier glyph
    // whose size grows with scale; only City+ conurbations are labelled to avoid
    // clutter. Colour is civic-neutral except under the Country lens, where the host
    // nation's tint applies (tier is carried by size, keeping colour out of ownership).
    if (!w.population_centres.empty())
    {
        struct pop_centre { int col; int row; int scale; entity_id tile; };
        std::vector<pop_centre> pcs;
        pcs.reserve(w.population_centres.size());
        for (const auto& [pid, pc] : w.population_centres)
        {
            const auto tit = w.population_centre_tile.find(pid);
            if (tit == w.population_centre_tile.end())
                continue;
            const auto til = w.tiles.find(tit->second);
            if (til == w.tiles.end() || til->second.body != state.active_body)
                continue;
            pcs.push_back({ til->second.grid_x, til->second.grid_y, pc.scale, tit->second });
        }

        // Cluster contiguous centres (transitive, Chebyshev grid distance <= 3,
        // cylinder-wrapped in columns). Small counts (20-40/body) so O(n^2) is fine.
        constexpr int cluster_dist = 3;
        std::vector<int> cl(pcs.size(), -1);
        int cluster_count = 0;
        for (std::size_t i = 0; i < pcs.size(); ++i)
        {
            if (cl[i] != -1)
                continue;
            cl[i] = cluster_count;
            std::vector<std::size_t> stack{ i };
            while (!stack.empty())
            {
                const std::size_t a = stack.back();
                stack.pop_back();
                for (std::size_t j = 0; j < pcs.size(); ++j)
                {
                    if (cl[j] != -1)
                        continue;
                    int dcol = std::abs(pcs[a].col - pcs[j].col);
                    dcol = std::min(dcol, gw - dcol); // east-west cylinder wrap
                    const int drow = std::abs(pcs[a].row - pcs[j].row);
                    if (std::max(dcol, drow) <= cluster_dist)
                    {
                        cl[j] = cluster_count;
                        stack.push_back(j);
                    }
                }
            }
            ++cluster_count;
        }

        // Per cluster: anchor at the highest-scale member; tier = that max scale.
        struct conurbation { int anchor = -1; int tier = 0; };
        std::vector<conurbation> cons(cluster_count);
        for (std::size_t i = 0; i < pcs.size(); ++i)
        {
            conurbation& c = cons[cl[i]];
            if (c.anchor == -1 || pcs[i].scale > c.tier)
            {
                c.anchor = static_cast<int>(i);
                c.tier   = pcs[i].scale;
            }
        }

        // A small settlement-name bank; a City+ conurbation is named deterministically
        // from its anchor tile id so the label is stable per campaign. (Deriving names
        // from the host nation is a noted refinement; a stable bank suffices here.)
        static const char* const settlement_names[] = {
            "Meridian", "Ashford", "Kepler", "Halcyon", "Cordera", "Blackmere",
            "Veranov", "Selwyn", "Marchand", "Tarsis", "Oberon", "Calderis",
            "Ravensford", "Solene", "Highmark", "Dunmoor",
        };
        constexpr int settlement_name_count =
            static_cast<int>(sizeof(settlement_names) / sizeof(settlement_names[0]));

        for (const conurbation& c : cons)
        {
            if (c.anchor < 0)
                continue;
            const pop_centre& a = pcs[c.anchor];

            ImU32 col = palette::settlement;
            if (state.overlay == overlay_mode::country)
            {
                const auto nit = w.tile_to_nation.find(a.tile);
                if (nit != w.tile_to_nation.end())
                    col = palette::nation_colour(nit->second);
            }

            const float sr = std::max(3.0f, draw_r * (0.30f + 0.11f * static_cast<float>(c.tier)));
            const ImVec2 lc = hex_local_centre(a.col, a.row, hex_size);
            const ImVec2 sc = to_screen(lc);

            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const ImVec2 mc = { sc.x + static_cast<float>(k) * period_px, sc.y };
                icons::settlement(dl, mc, sr, c.tier, col);

                // Label City+ conurbations (tier >= 4) only, to keep the map legible.
                if (c.tier >= 4)
                {
                    const char* name = settlement_names[
                        (static_cast<uint32_t>(a.tile) * 2654435761u) % settlement_name_count];
                    const ImVec2 tp{ mc.x + sr + 3.0f, mc.y - sr };
                    dl->AddText({ tp.x + 1.0f, tp.y + 1.0f }, IM_COL32(20, 22, 28, 200), name); // shadow
                    dl->AddText(tp, IM_COL32(236, 230, 214, 255), name);
                }
            }
        }
    }

    // Home presence (BL-085): on the player's home body, draw a ring enclosing the
    // holdings cluster ("my region") and an HQ star on the building nearest the
    // cluster centroid ("my origin"). Always-on identity chrome that composes with
    // (does not duplicate) the per-tile ownership outline already drawn above; the
    // camera-focus + ownership-accent halves shipped earlier (start-framing QOL).
    if (state.active_body == w.home_body && w.home_body != null_entity)
    {
        std::vector<std::pair<entity_id, ImVec2>> mine; // player building tile -> local centre
        for (const auto& [tid, corp] : tile_to_corp)
        {
            if (corp != w.player_entity)
                continue;
            const auto til = w.tiles.find(tid);
            if (til == w.tiles.end() || til->second.body != state.active_body)
                continue;
            mine.emplace_back(tid, hex_local_centre(til->second.grid_x, til->second.grid_y, hex_size));
        }
        if (!mine.empty())
        {
            ImVec2 cen{ 0.0f, 0.0f };
            for (const auto& [tid, lc] : mine) { cen.x += lc.x; cen.y += lc.y; }
            cen.x /= static_cast<float>(mine.size());
            cen.y /= static_cast<float>(mine.size());

            float max_d = 0.0f;
            entity_id hq_tile = mine.front().first;
            float hq_best = std::numeric_limits<float>::max();
            for (const auto& [tid, lc] : mine)
            {
                const float dx = lc.x - cen.x, dy = lc.y - cen.y;
                const float d = std::sqrt(dx * dx + dy * dy);
                max_d = std::max(max_d, d);
                if (d < hq_best) { hq_best = d; hq_tile = tid; }
            }

            const ImU32 accent = corp_identity(w.player_entity);
            const ImVec2 cen_s = to_screen(cen);
            const float ring_r = (max_d + hex_size * 0.9f) * zoom;

            // HQ local centre for the pip.
            ImVec2 hq_lc = cen;
            for (const auto& [tid, lc] : mine) if (tid == hq_tile) { hq_lc = lc; break; }
            const ImVec2 hq_s = to_screen(hq_lc);
            const float hq_r = std::max(4.0f, draw_r * 0.5f);

            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - cen_s.x - ring_r) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - cen_s.x + ring_r) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const float off = static_cast<float>(k) * period_px;
                // "My region" ring — soft translucent fill + a crisp player-colour edge.
                dl->AddCircle({ cen_s.x + off, cen_s.y }, ring_r,
                              IM_COL32(80, 150, 230, 120), 0, 2.5f);
                // HQ star at the origin building.
                icons::hq(dl, { hq_s.x + off, hq_s.y }, hq_r, accent);
            }
        }
    }

    // Building-placement ghost preview. When construction mode is active and a tile
    // is hovered, draw a translucent-intent marker of the chosen building type at the
    // hovered copy's centre, tinted green when the placement-rules seam accepts the
    // tile and red when it rejects it. The marker reads placement_rules::can_place
    // and mutates nothing — the click below is what enqueues the build. The
    // hovered copy's centre is the centroid of its six vertices (the wrap copy the
    // cursor actually resolved to); the marker radius mirrors the built-marker pass.
    if (have_hover && state.construction.active)
    {
        float gx = 0.0f, gy = 0.0f;
        for (const ImVec2& v : hover_verts)
        {
            gx += v.x;
            gy += v.y;
        }
        gx /= 6.0f;
        gy /= 6.0f;

        const float mr = std::max(2.0f, draw_r * 0.22f);

        // Full world-level check (coastal / launchpad, not just terrain) so the
        // ghost is red — and the reason legible — *before* the click, not after
        // construct_building refuses it (BL-071).
        const placement_rules::placement_result pr = placement_rules::can_place_in_world(
            w, hovered_tile, state.construction.type, state.construction.target);
        const ImU32 ghost_col = pr ? palette::positive : palette::negative;

        icons::building(dl, {gx, gy}, mr, state.construction.type, ghost_col);

        // 'Why not here': the rejection reason follows the cursor while build mode
        // is armed, sat just below the ghost with a dark backdrop for contrast.
        if (!pr)
        {
            const char* why = pr.message();
            const ImVec2 tsz = ImGui::CalcTextSize(why);
            const ImVec2 tp{gx - tsz.x * 0.5f, gy + mr + 3.0f};
            dl->AddRectFilled({tp.x - 3.0f, tp.y - 1.0f},
                              {tp.x + tsz.x + 3.0f, tp.y + tsz.y + 1.0f},
                              IM_COL32(0, 0, 0, 180), 2.0f);
            dl->AddText(tp, palette::negative, why);
        }
    }

    dl->PopClipRect();

    // On-canvas lens key (drawn unclipped, flush-left of the minimap so it reads as a
    // drawer folding out from it — anchor passed in as lens_key_anchor; before the
    // input early-out so it shows in headless captures too).
    if (state.overlay == overlay_mode::country)
        draw_country_key(dl, lens_key_anchor, w, state);
    else if (state.overlay == overlay_mode::resource)
        draw_resource_key(dl, lens_key_anchor, state);
    else if (state.overlay == overlay_mode::market)
        draw_market_key(dl, lens_key_anchor, w, state, market_catchment_colour);
    else if (state.overlay == overlay_mode::population)
        draw_population_key(dl, lens_key_anchor);
    else if (state.overlay == overlay_mode::opportunity)
        draw_opportunity_key(dl, lens_key_anchor);
    else if (state.overlay == overlay_mode::production)
        draw_production_key(dl, lens_key_anchor);
    else if (state.overlay == overlay_mode::scarcity)
        draw_scarcity_key(dl, lens_key_anchor, state);
    else if (state.overlay == overlay_mode::industry)
        draw_industry_key(dl, lens_key_anchor);
    else if (state.overlay == overlay_mode::reach)
        draw_reach_key(dl, lens_key_anchor, w, reach_links);
    else if (state.overlay == overlay_mode::supply_routes)
        draw_supply_routes_key(dl, lens_key_anchor, w, supply_edges);

    if (!input_enabled)
        return;

    // Hover-card (BL-060, BL-020). Resolve the hovered entity in marker-priority
    // order (building > market_centre > tile — mirroring click priority). Track
    // stable hover ticks and show the lens-contextual "why not what" card after
    // kHoverDelay frames of rest on the same entity.
    {
        // Resolve the highest-priority entity under the cursor.
        entity_id hover_eid    = null_entity;
        float     best_bld_d2  = std::numeric_limits<float>::max();
        float     best_mkt_d2  = std::numeric_limits<float>::max();
        entity_id bld_hover    = null_entity;
        entity_id mkt_hover    = null_entity;

        for (const marker_hit_zone& hz : state.marker_hit_zones)
        {
            const float dx = mouse.x - hz.centre.x;
            const float dy = mouse.y - hz.centre.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 > hz.radius * hz.radius)
                continue;
            if (hz.kind == marker_hit_zone::kind::building && d2 < best_bld_d2)
            {
                best_bld_d2 = d2;
                bld_hover   = hz.id;
            }
            else if (hz.kind == marker_hit_zone::kind::market_centre && d2 < best_mkt_d2)
            {
                best_mkt_d2 = d2;
                mkt_hover   = hz.id;
            }
        }

        if (bld_hover != null_entity)
            hover_eid = bld_hover;
        else if (mkt_hover != null_entity)
            hover_eid = mkt_hover;
        else
            hover_eid = hovered_tile;

        if (hover_eid != state.hovered_entity)
        {
            state.hovered_entity = hover_eid;
            state.hover_ticks    = 0;
        }
        else if (hover_eid != null_entity)
        {
            ++state.hover_ticks;
        }

        if (hover_eid != null_entity)
        {
            draw_hover_card(mouse, state.hover_ticks, [&]() {
                draw_hover_content(w, state, hover_eid);
            });
        }
    }

    // Click handling. The surface is the bottom rung, so there is nothing to
    // descend into: a single left-click simply selects the hovered tile (null
    // clears the selection on empty space) and fills the Selection info element.
    // No view change; the player ascends via the minimap. See SELECTION.md.
    // Construction mode suppresses selection: in placement mode a left-click is a
    // construction gesture, not a selection one, so it must not retarget the
    // Selection info element.
    // BL-031: marker hit zones take priority over tile selection (building >
    // market_centre; closest-wins tie-break within a kind).
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (!state.construction.active)
        {
            // Resolve marker hit zones in priority order (BL-031).
            entity_id marker_hit = null_entity;
            float best_bld_d2  = std::numeric_limits<float>::max();
            float best_mkt_d2  = std::numeric_limits<float>::max();
            entity_id bld_hit  = null_entity;
            entity_id mkt_hit  = null_entity;

            for (const marker_hit_zone& hz : state.marker_hit_zones)
            {
                const float dx = mouse.x - hz.centre.x;
                const float dy = mouse.y - hz.centre.y;
                const float d2 = dx * dx + dy * dy;
                if (d2 > hz.radius * hz.radius)
                    continue;
                if (hz.kind == marker_hit_zone::kind::building && d2 < best_bld_d2)
                {
                    best_bld_d2 = d2;
                    bld_hit     = hz.id;
                }
                else if (hz.kind == marker_hit_zone::kind::market_centre && d2 < best_mkt_d2)
                {
                    best_mkt_d2 = d2;
                    mkt_hit     = hz.id;
                }
            }
            // Building outranks market-centre; both outrank tile.
            if (bld_hit != null_entity)
                marker_hit = bld_hit;
            else if (mkt_hit != null_entity)
                marker_hit = mkt_hit;

            state.selected_entity = (marker_hit != null_entity) ? marker_hit : hovered_tile;
            // A fresh click is an explicit select gesture: re-show the panel even
            // when it re-selects the same entity the player had dismissed (close
            // hides, does not destroy — SELECTION.md).
            state.selection_hidden_for = null_entity;
        }
        else if (hovered_tile != null_entity)
        {
            // Placement mode: queue a construction request on the hovered tile.
            // app::render executes it against the mutable world (this canvas holds
            // only const world&) and clears it. See SELECTION.md / construction.hpp.
            state.construction.pending_tile   = hovered_tile;
            state.construction.pending_type   = state.construction.type;
            state.construction.pending_target = state.construction.target;
        }
    }

    // Pan and zoom. Middle mouse button pans; scroll wheel zooms, anchored at
    // the cursor so the point under the mouse stays fixed.
    {
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            state.planetary_pan_x += io.MouseDelta.x;
            state.planetary_pan_y += io.MouseDelta.y;
        }

        // Keep horizontal pan bounded to one grid period. The grid wraps, so
        // this is visually identical but stops pan_x growing without limit.
        if (period_px > 0.0f)
            state.planetary_pan_x = std::fmod(state.planetary_pan_x, period_px);

        if (io.MouseWheel != 0.0f)
        {
            const float new_zoom = std::clamp(zoom * std::pow(1.1f, io.MouseWheel), kMinZoom, kMaxZoom);
            // World point under the cursor, kept fixed across the zoom change.
            const ImVec2 wp = { (mouse.x - view_origin.x) / zoom + grid_cx,
                                (mouse.y - view_origin.y) / zoom + grid_cy };
            state.planetary_pan_x = mouse.x - (wp.x - grid_cx) * new_zoom - canvas_centre.x;
            state.planetary_pan_y = mouse.y - (wp.y - grid_cy) * new_zoom - canvas_centre.y;
            state.planetary_zoom  = new_zoom;
        }
    }
}

} // namespace ui
