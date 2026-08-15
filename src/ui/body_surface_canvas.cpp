#include "body_surface_canvas.hpp"
#include "text_fit.hpp"

#include "entity_summary.hpp"
#include "highlight.hpp"
#include "hover_card.hpp"
#include "hover_content.hpp"
#include "hex_render.hpp"
#include "icons.hpp"
#include "market_ledger.hpp" // market_city_name (Market lens catchment key, BL-015)
#include "nav_pane.hpp"
#include "presentation.hpp"
#include "world/hex_neighbors.hpp"   // canonical odd-r neighbour offsets (BL-363)
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
constexpr float kMinZoomHeadroom = 1.2f;  // the old floor's viewport spanned ~120% of the grid height
// Zoom-out cap (Ben, 2026-08-12): the full-grid view lags on the 3x map — 45,240
// hexes in one draw pass — so the widest view is restricted to 0.7x the old
// maximum extent (extent scales as 1/zoom, hence the division below). The
// default framing (4/3, view_nav.cpp) sits above this floor, so only the last
// stretch of zoom-out is lost, not any framing the app itself sets.
constexpr float kMaxViewScale    = 0.7f;
constexpr float kMaxZoom         = 20.0f;
constexpr float kMinZoom         = 1.0f / (kMaxViewScale * kMinZoomHeadroom * kFitMargin); // ~1.253

// terrain_colour, hex_vertices, and hex_local_centre now live in ui/hex_render.hpp
// (shared with the Selection band's zoomed tile-neighbourhood view, BL-194) so both
// surfaces draw from one terrain palette and one hex geometry rather than diverging.

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

/// The intra-body reach fog's wash (BL-151/152/154): dim a colour toward the fog dark
/// by how little the player sees the tile — `vision` 1 = fully lit, 0 = unreached.
/// Alpha is preserved (lerp_colour forces opaque, which would change how a translucent
/// road span reads). Every layer the fog covers goes through this one function — the
/// lens fill and, since BL-185, the road spans — so the fog reads as a single wash
/// rather than a dark ground with brightly-lit roads laid over it.
ImU32 fog_dim(ImU32 c, float vision)
{
    if (vision >= 1.0f)
        return c;
    const ImU32 alpha = (c >> IM_COL32_A_SHIFT) & 0xFFu;
    const ImU32 dim   = lerp_colour(c, IM_COL32(8, 10, 16, 255), 0.5f * (1.0f - vision));
    return (dim & ~(0xFFu << IM_COL32_A_SHIFT)) | (alpha << IM_COL32_A_SHIFT);
}

/// Fill colour of a tile that carries a building — its **plate**. A built tile is
/// swapped out of the terrain palette entirely (Ben, 2026-07-22): it fills with a
/// desaturated wash of the owning corporation's identity colour, so ownership — the
/// most load-bearing read on this canvas — carries at a glance and agrees with the
/// marker/emblem convention (palette::corp_identity_colour). A building with no
/// corporate owner gets the neutral industrial plate.
///
/// This is the ONE place the plate choice lives, so retuning it (flat neutral, or a
/// building-type hue) is a single-function edit.
ImU32 built_plate_colour(bool has_owner, ImU32 owner_colour)
{
    // Deep machine grey — deliberately outside the terrain hues so a built tile never
    // reads as ground, even before the owner tint is mixed in.
    constexpr ImU32 industrial = IM_COL32(50, 52, 60, 255);
    return has_owner ? lerp_colour(industrial, owner_colour, 0.55f) : industrial;
}

/// Building silhouette radius as a fraction of the hex circumradius. The silhouette
/// is now the tile's *content*, not a marker pinned on terrain, so it scales to the
/// hex; the value leaves the lower-right corner free for the owner emblem tag and
/// keeps the widest glyph (the square) inside the hexagon's inradius. Shared with the
/// construction ghost so the armed preview matches what actually lands.
constexpr float kBuiltSilhouetteScale = 0.48f;

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

/// Diverging red→green colour for a ratio relative to 1.0 (defined below); forward
/// declared so the Production key (above its definition) can sample the same band.
ImU32 production_colour(float ratio);

/// Shared chrome for an on-canvas lens key: a rounded dark panel of @p box_w ×
/// @p body_h at the left edge (inset past the nav rail), vertically centred —
/// clear of the Selection panel, the header/comms log, and the lens control strip.
/// Returns the inner top-left and the inner content width via @p out_x/@p out_y/
/// @p out_w. Pure ImDrawList — no ImGui widget state.
/// @p bg is the panel fill; a key that floats over a *window* rather than the
/// canvas passes an opaque one (BL-376 — see draw_continent_key's call site).
void begin_lens_key(ImDrawList* dl, ImVec2 anchor, float box_w,
                    float body_h, float pad, float& out_x, float& out_y, float& out_w,
                    ImU32 bg = IM_COL32(18, 18, 24, 210))
{
    // Anchored so the box's RIGHT edge sits at anchor.x (the minimap's left edge) and
    // the box is vertically centred on anchor.y — it reads as a drawer folding out from
    // the left side of the minimap. anchor is passed in from app.cpp (lens_key_anchor).
    const ImVec2 p0 = { anchor.x - box_w, anchor.y - body_h * 0.5f };
    const ImVec2 p1 = { p0.x + box_w, p0.y + body_h };
    dl->AddRectFilled(p0, p1, bg, 4.0f);
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

/// Marker glyph for a count-driven lens-key row: a filled swatch (Country / Market),
/// a small dot (Reach), or a thickness bar (Supply). Lets one scrollable renderer
/// serve every list-shaped legend.
enum class key_marker { swatch, dot, bar };

/// One row of a count-driven lens key: a marker + a label, each with its own colour
/// (swatch legends label in neutral grey; the connection legends colour the label by
/// recency tier). `bar_frac` is the [0,1] fill only key_marker::bar reads.
struct key_row
{
    ImU32       marker_colour;
    ImU32       label_colour;
    std::string label;
    key_marker  marker;
    float       bar_frac = 0.0f;
};

/// Shared chrome for a count-driven, potentially-overflowing lens key (BL-163/164):
/// a dark panel with a fixed header (and an optional good-selector combo, for the
/// Market lens) over a bounded, smoothly-scrolling body that hosts the rows. The
/// box's right edge sits at @p anchor.x; its height is capped to the canvas vertical
/// span [@p top_limit, @p bottom_limit] so a long list never overruns the canvas —
/// the rows scroll (wheel/drag) inside a borderless ImGui child instead of the box
/// growing off-screen. Must run inside the ImGui frame (it opens child windows, like
/// draw_lens_resource_combo). @p combo_state != nullptr draws the resource selector.
void draw_scroll_list_key(ImVec2 anchor, float top_limit, float bottom_limit,
                          const char* id, const char* header, float box_w,
                          const std::vector<key_row>& rows, const char* empty_note,
                          ui_state* combo_state)
{
    const float pad      = 8.0f;
    const float line_h   = ImGui::GetTextLineHeight();
    const float swatch   = line_h;
    const float row_h    = line_h + 2.0f;              // matches the legacy per-row advance
    const float header_h = line_h + 4.0f;
    const float combo_h  = combo_state ? (kLensComboH + 4.0f) : 0.0f;
    const float bar_max  = 40.0f;                      // key_marker::bar full length

    const int   n         = std::max(1, static_cast<int>(rows.size()));
    const float content_h = static_cast<float>(n) * row_h;

    // Height budget: fixed chrome (pads + optional combo + header) plus as much of the
    // row list as the canvas span allows; the remainder scrolls.
    const float chrome    = pad + combo_h + header_h + pad;
    const float avail     = std::max(row_h, (bottom_limit - top_limit) - chrome);
    const float body_h    = std::min(content_h, avail);
    const float box_h     = chrome + body_h;

    // Vertically centre on the anchor, then clamp into the canvas span so it never
    // spills past the top or bottom edge.
    float top = anchor.y - box_h * 0.5f;
    top       = std::clamp(top, top_limit, std::max(top_limit, bottom_limit - box_h));
    const ImVec2 p0 = { anchor.x - box_w, top };
    const ImVec2 p1 = { p0.x + box_w, p0.y + box_h };

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 24, 210), 4.0f);
    dl->AddRect      (p0, p1, IM_COL32(80, 80, 90, 255), 4.0f);

    const float x = p0.x + pad;
    float       y = p0.y + pad * 0.5f;

    if (combo_state)
    {
        draw_lens_resource_combo(*combo_state, {x, y}, box_w - 2.0f * pad);
        y += kLensComboH + 4.0f;
    }

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), header); // fit-exempt: legend box sized to its measured entries (container 2)
    y += header_h;

    if (rows.empty())
    {
        dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), empty_note); // fit-exempt: legend box sized to its measured entries (container 2)
        return;
    }

    // Scrollable body: a borderless child overlaying the row area (the combo pattern),
    // so overflow scrolls with a clean scrollbar rather than overrunning the canvas.
    const ImVec2 body_pos = { x, y };
    const float  body_w   = box_w - 2.0f * pad;
    ImGui::SetNextWindowPos(body_pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ body_w, body_h }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags wflags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav   | ImGuiWindowFlags_NoSavedSettings;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});
    ImGui::Begin(id, nullptr, wflags);
    ImGui::BeginChild("##rows", { body_w, body_h }, false, ImGuiWindowFlags_NoBackground);
    ImDrawList* wdl = ImGui::GetWindowDrawList();
    for (const key_row& r : rows)
    {
        const ImVec2 c = ImGui::GetCursorScreenPos();
        switch (r.marker)
        {
            case key_marker::swatch:
                wdl->AddRectFilled(c, { c.x + swatch, c.y + swatch }, r.marker_colour);
                break;
            case key_marker::dot:
                wdl->AddCircleFilled({ c.x + 4.0f, c.y + line_h * 0.5f }, 3.5f, r.marker_colour);
                break;
            case key_marker::bar:
            {
                const float th = 3.0f + (bar_max - 3.0f) * std::clamp(r.bar_frac, 0.0f, 1.0f);
                wdl->AddRectFilled({ c.x, c.y + line_h * 0.5f - 2.0f },
                                   { c.x + th, c.y + line_h * 0.5f + 2.0f }, r.marker_colour);
                break;
            }
        }
        const float text_x = (r.marker == key_marker::bar) ? (bar_max + 6.0f) : (swatch + 4.0f);
        wdl->AddText({ c.x + text_x, c.y }, r.label_colour, r.label.c_str()); // fit-exempt: legend box sized to its measured entries (container 2)
        ImGui::Dummy({ body_w, row_h });
    }
    ImGui::EndChild();
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

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Resource deposit"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 4.0f;
    dl->AddRectFilled({x, y + 2.0f}, {x + 10.0f, y + 12.0f},
                      presentation_of(state.lens_resource).colour);
    dl->AddText({x + 14.0f, y}, IM_COL32(235, 235, 235, 255), // fit-exempt: legend box sized to its measured entries (container 2)
                presentation_of(state.lens_resource).name);
    y += line_h + 4.0f;
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "filled = deposit present"); // fit-exempt: legend box sized to its measured entries (container 2)
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

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Opportunity"); // fit-exempt: legend box sized to its measured entries (container 2)
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
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 ts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - ts.x, y}, IM_COL32(170, 175, 185, 255), "high"); // fit-exempt: legend box sized to its measured entries (container 2)
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

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Production intensity"); // fit-exempt: legend box sized to its measured entries (container 2)
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
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 ts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - ts.x, y}, IM_COL32(170, 175, 185, 255), "high"); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// Diverging red→yellow→green colour for a ratio relative to 1.0 (BL-137, Production
/// lens): `ratio = value / mean`; 1.0 is the yellow mid-tone, < 1 (below mean) trends
/// red, > 1 (above mean) trends green. Centred on the log of the ratio so the
/// symmetric band `[0.25×, 4×]` maps to the full span, routed through the shared
/// ryg_colour ramp.
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
void draw_market_key(ImVec2 anchor, float top_limit, float bottom_limit, const world& w,
                     ui_state& state,
                     const std::unordered_map<entity_id, ImU32>& catchment_colours)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float swatch = line_h;
    // Stable legend order — the source map iterates arbitrarily; sort by market id so the
    // swatch list does not reshuffle frame to frame (colours stay keyed to their market).
    std::vector<std::pair<entity_id, ImU32>> entries(catchment_colours.begin(),
                                                     catchment_colours.end());
    std::sort(entries.begin(), entries.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Size the box to the widest label so long city names are not clipped. One
    // market_city_name call per market: the row keeps the string the width pass
    // measured (it used to be built twice per market per frame — BL-362).
    float label_w = ui::fit_width("Market catchments");
    std::vector<key_row> rows;
    rows.reserve(entries.size());
    for (const auto& [mid, col] : entries)
    {
        std::string name = market_city_name(w, mid);
        label_w = std::max(label_w, swatch + 4.0f + ui::fit_width(name.c_str()));
        rows.push_back({ col, IM_COL32(220, 220, 220, 255), std::move(name),
                         key_marker::swatch, 0.0f });
    }
    const float box_w = std::max(140.0f, label_w + 2.0f * pad);

    draw_scroll_list_key(anchor, top_limit, bottom_limit, "##lens_key_market",
                         "Market catchments", box_w, rows, "No markets", &state);
}

/// On-canvas legend for the Country lens (BL-133): one colour swatch + nation name
/// per nation present on the active body, sorted by nation id for a stable order.
/// Modelled on draw_market_key; the box auto-sizes to the widest name so every
/// label guaranteed-fits (CalcTextSize pattern draw_market_key already uses).
/// Colour source is palette::nation_colour — the same source the tile tint itself
/// uses (the country lens's tile-tint pass, above).
void draw_country_key(ImVec2 anchor, float top_limit, float bottom_limit,
                      const world& w, const ui_state& state)
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

    float name_w = ui::fit_width("Countries");
    for (const entity_id nid : present)
    {
        const auto nat_it = w.nations.find(nid);
        if (nat_it == w.nations.end())
            continue;
        name_w = std::max(name_w, ui::fit_width(nat_it->second.name.c_str()));
    }
    const float box_w = pad * 2.0f + swatch + 4.0f + name_w;

    std::vector<key_row> rows;
    rows.reserve(present.size());
    for (const entity_id nid : present)
    {
        const auto nat_it = w.nations.find(nid);
        if (nat_it == w.nations.end())
            continue;
        rows.push_back({ palette::nation_colour(nid), IM_COL32(220, 220, 220, 255),
                         nat_it->second.name, key_marker::swatch, 0.0f });
    }

    draw_scroll_list_key(anchor, top_limit, bottom_limit, "##lens_key_country",
                         "Countries", box_w, rows, "No nations", nullptr);
}

/// On-canvas legend for the Population lens: a low→high habitability gradient bar
/// (dark substrate → liveable green). Same left-edge placement as the other keys.
void draw_population_key(ImDrawList* dl, ImVec2 anchor)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, anchor, 156.0f, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Workforce efficiency"); // fit-exempt: legend box sized to its measured entries (container 2)
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
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 hts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - hts.x, y}, IM_COL32(170, 175, 185, 255), "high"); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// Colour for tectonic plate @p index (Continent lens, BL-226). A dedicated
/// palette rather than palette::nation_colour, because plates are substrate and
/// not identity — but a *categorical* palette all the same, so the first draft's
/// near-monochrome mineral tones are deliberately abandoned: they blended into
/// one grey wash and the lens could not do the one job it has (tell one plate
/// from the next). These keep the earthy cast that separates them from the
/// nation wheel while carrying real hue AND lightness separation, alternating
/// light/dark so adjacent slots differ even in greyscale. Ten slots covers
/// run_continents' clamp of 4..10 mobile plates; the modulo is belt-and-braces.
ImU32 plate_colour(int index)
{
    static constexpr ImU32 table[10] = {
        IM_COL32(196, 152,  92, 255), // sandstone (light warm)
        IM_COL32( 62,  88, 110, 255), // deep slate (dark cool)
        IM_COL32(182, 108,  86, 255), // clay red (light warm)
        IM_COL32( 70, 110,  92, 255), // dark serpentine (dark cool)
        IM_COL32(206, 190, 128, 255), // pale marl (very light)
        IM_COL32( 96,  76, 116, 255), // dark porphyry (dark violet)
        IM_COL32(150, 176, 106, 255), // olivine (light green)
        IM_COL32( 58,  76,  84, 255), // basalt (very dark)
        IM_COL32(206, 146, 160, 255), // rose quartz (light pink)
        IM_COL32(104,  92,  60, 255), // dark ochre
    };
    return table[static_cast<std::size_t>(((index % 10) + 10) % 10)];
}

/// The Continent lens key. Unlike the gradient keys, this legend has no scale to
/// explain — the tint is categorical — so it explains the one thing that is not
/// self-evident: that the BRIGHT tiles are plate boundaries, and why they matter.
///
/// BL-376: the key keeps this position but is drawn on the FOREGROUND list by its
/// caller, so it floats over the always-open Selection band instead of being buried
/// by it. Purely a draw-order fix — nothing here moves. Because the band, not the
/// canvas, is now what sits underneath, the panel takes an opaque fill: the 210-alpha
/// default let the band's own background bleed through and muddy the plate swatches,
/// which are the one thing this key exists to show.
void draw_continent_key(ImDrawList* dl, ImVec2 anchor, const continent_state* plates)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float sw     = 11.0f; // swatch edge

    const float body_h = pad + line_h + 6.0f + sw + 4.0f + sw + 4.0f + line_h + pad;
    float x, y, inner_w;
    begin_lens_key(dl, anchor, 176.0f, body_h, pad, x, y, inner_w,
                   IM_COL32(18, 18, 24, 255));
    (void)inner_w;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Tectonic plates"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 6.0f;

    if (!plates)
    {
        dl->AddText({x, y}, IM_COL32(170, 170, 180, 255), "No plate record"); // fit-exempt: legend box sized to its measured entries (container 2)
        y += line_h + 4.0f;
        dl->AddText({x, y}, IM_COL32(150, 150, 160, 255), "for this body."); // fit-exempt: legend box sized to its measured entries (container 2)
        return;
    }

    const int n = static_cast<int>(plates->plates.size());
    if (n <= 1)
    {
        dl->AddText({x, y}, IM_COL32(170, 170, 180, 255), "Stagnant lid:"); // fit-exempt: legend box sized to its measured entries (container 2)
        y += line_h + 4.0f;
        dl->AddText({x, y}, IM_COL32(150, 150, 160, 255), "one immobile plate."); // fit-exempt: legend box sized to its measured entries (container 2)
        return;
    }

    // A row of plate swatches — the count is the readable fact, not the identity
    // of any one plate, so they run as an unlabelled strip.
    float sx = x;
    for (int i = 0; i < n; ++i)
    {
        dl->AddRectFilled({ sx, y }, { sx + sw, y + sw }, plate_colour(i));
        sx += sw + 3.0f;
    }
    y += sw + 4.0f;

    // The boundary swatch carries the same white lift the map applies, so the
    // legend shows the actual treatment rather than describing it.
    dl->AddRectFilled({ x, y }, { x + sw, y + sw },
                      lerp_colour(plate_colour(0), IM_COL32(255, 255, 245, 255), 0.45f));
    dl->AddText({ x + sw + 6.0f, y - 1.0f }, IM_COL32(220, 220, 228, 255), "pale = boundary"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += sw + 4.0f;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d plates - uplift & rift", n);
    dl->AddText({x, y}, IM_COL32(170, 170, 180, 255), buf); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// On-canvas legend for the Industry lens (BL-084; re-pointed BL-373): a low→high
/// amber gradient bar mapping the tile tint, so the field reads as "where the
/// industry I did not build is densest". Same placement as the others. (This
/// comment previously drifted onto plate_colour, which it does not describe.)
void draw_industry_key(ImDrawList* dl, ImVec2 anchor)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + line_h + 4.0f + bar_h + 2.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, anchor, 156.0f, body_h, pad, x, y, bar_w);

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Background industry"); // fit-exempt: legend box sized to its measured entries (container 2)
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
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "low"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 hts = ImGui::CalcTextSize("high");
    dl->AddText({x + bar_w - hts.x, y}, IM_COL32(170, 175, 185, 255), "high"); // fit-exempt: legend box sized to its measured entries (container 2)
}

/// On-canvas legend for the Scarcity lens: an abundant→scarce gradient bar (no tint
/// → hot) plus the selected resource's name and swatch. Same placement as the others.
void draw_scarcity_key(ImDrawList* dl, ImVec2 anchor,
                       ui_state& state)
{
    const float pad    = 8.0f;
    const float line_h = ImGui::GetTextLineHeight();
    const float bar_h  = 10.0f;

    const float body_h = pad + kLensComboH + 4.0f
                       + line_h + 4.0f + bar_h + 2.0f + line_h + 4.0f + line_h + pad;
    float x, y, bar_w;
    begin_lens_key(dl, anchor, 156.0f, body_h, pad, x, y, bar_w);

    draw_lens_resource_combo(state, {x, y}, bar_w);
    y += kLensComboH + 4.0f;

    dl->AddText({x, y}, IM_COL32(235, 235, 235, 255), "Market scarcity"); // fit-exempt: legend box sized to its measured entries (container 2)
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
    dl->AddText({x, y}, IM_COL32(170, 175, 185, 255), "met"); // fit-exempt: legend box sized to its measured entries (container 2)
    const ImVec2 sts = ImGui::CalcTextSize("scarce");
    dl->AddText({x + bar_w - sts.x, y}, IM_COL32(170, 175, 185, 255), "scarce"); // fit-exempt: legend box sized to its measured entries (container 2)
    y += line_h + 4.0f;

    dl->AddRectFilled({x, y + 2.0f}, {x + 10.0f, y + 12.0f},
                      presentation_of(state.lens_resource).colour);
    dl->AddText({x + 14.0f, y}, IM_COL32(235, 235, 235, 255), // fit-exempt: legend box sized to its measured entries (container 2)
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
void draw_reach_key(ImVec2 anchor, float top_limit, float bottom_limit, const world& w,
                    const std::vector<reach_link>& links)
{
    const float box_w = 176.0f;
    std::vector<key_row> rows;
    rows.reserve(links.size());
    for (const reach_link& link : links)
    {
        const auto  it   = w.bodies.find(link.other_body);
        const char* name = (it != w.bodies.end()) ? it->second.name.c_str() : "unknown body";
        const ImU32 c    = reach_tier_colour(link.tier);
        rows.push_back({ c, c, name, key_marker::dot, 0.0f });
    }
    draw_scroll_list_key(anchor, top_limit, bottom_limit, "##lens_key_reach",
                         "Reach (your trade network)", box_w, rows,
                         "no routes from this body", nullptr);
}

/// On-canvas legend for the Supply-routes lens (BL-014): one row per aggregated
/// lane touching the active body — a thickness bar log-scaled from convoy_count
/// stands in for the edge-thickness encoding the design specifies for the
/// (out-of-scope-here) Solar-canvas graph rendering, and colour is the shared
/// recency tier.
void draw_supply_routes_key(ImVec2 anchor, float top_limit, float bottom_limit, const world& w,
                            const std::vector<supply_edge>& edges)
{
    const float box_w = 176.0f;
    std::vector<key_row> rows;
    rows.reserve(edges.size());
    for (const supply_edge& edge : edges)
    {
        const auto  it   = w.bodies.find(edge.other_body);
        const char* name = (it != w.bodies.end()) ? it->second.name.c_str() : "unknown body";
        const ImU32 c    = reach_tier_colour(edge.tier);
        // Log-scaled thickness (BL-014, settled): a bare completion reads as a thin
        // sliver; heavy repeat traffic saturates toward the bar's full length rather
        // than dominating the row linearly. Expressed as a [0,1] fraction of bar_max
        // (37 = bar_max 40 − base 3) so the shared renderer paints the bar.
        const float frac = std::clamp(6.0f * std::log(1.0f + static_cast<float>(edge.convoy_count)) / 37.0f,
                                      0.0f, 1.0f);
        rows.push_back({ c, c, name, key_marker::bar, frac });
    }
    draw_scroll_list_key(anchor, top_limit, bottom_limit, "##lens_key_supply",
                         "Supply routes", box_w, rows, "no lanes from this body", nullptr);
}

/// BL-362: bumps whenever the logistics caches were cleared since the last look.
/// invalidate_logistics_caches fires on every build/demolish/completion/road event,
/// so a shrink of world.astar_cost_cache is the one cheap signal that covers them
/// all; growth (ordinary path queries filling the cache) never bumps.
std::uint32_t logistics_generation(const world& w)
{
    static std::size_t   last_size = 0;
    static std::uint32_t gen       = 0;
    const std::size_t size = w.astar_cost_cache.size();
    if (size < last_size) ++gen;
    last_size = size;
    return gen;
}

/// BL-362 rebuild stamp for the per-frame derived views (vision model, marker
/// maps): their inputs move only on a body switch, an econ/day tick, a building
/// or convoy change, or a logistics invalidation — never mid-frame.
struct body_frame_stamp
{
    entity_id     body      = null_entity;
    int           day_tick  = -1;
    std::size_t   buildings = 0;
    std::size_t   convoys   = 0;
    std::uint32_t logi_gen  = ~0u; // default never matches a live stamp
    bool operator==(const body_frame_stamp&) const = default;
};

body_frame_stamp make_body_frame_stamp(const world& w, entity_id body)
{
    return { body, w.current_day_tick, w.buildings.size(), w.convoys.size(),
             logistics_generation(w) };
}

/// Nearest-wins marker hit resolution shared by hover and click (BL-031/BL-059):
/// building outranks market_centre within the glyph radii; the tile fallback
/// stays with the callers. Extracted so the two paths cannot drift (BL-362).
entity_id resolve_marker_hit(const std::vector<marker_hit_zone>& zones, float mx, float my)
{
    float     best_bld_d2 = std::numeric_limits<float>::max();
    float     best_mkt_d2 = std::numeric_limits<float>::max();
    entity_id bld = null_entity;
    entity_id mkt = null_entity;
    for (const marker_hit_zone& hz : zones)
    {
        const float dx = mx - hz.centre.x;
        const float dy = my - hz.centre.y;
        const float d2 = dx * dx + dy * dy;
        if (d2 > hz.radius * hz.radius)
            continue;
        if (hz.kind == marker_hit_zone::kind::building && d2 < best_bld_d2)
        {
            best_bld_d2 = d2;
            bld         = hz.id;
        }
        else if (hz.kind == marker_hit_zone::kind::market_centre && d2 < best_mkt_d2)
        {
            best_mkt_d2 = d2;
            mkt         = hz.id;
        }
    }
    return bld != null_entity ? bld : mkt;
}

} // namespace

void update_body_vision(world& w, ui_state& state, double now_days)
{
    state.sim_now_days = now_days;

    // BL-362: the flood fills, market_for_tile scans and A* corridor walks below
    // depend only on the stamp's inputs — skip the rebuild while none has moved.
    static body_frame_stamp s_vision_stamp;
    const body_frame_stamp stamp = make_body_frame_stamp(w, state.active_body);
    if (stamp == s_vision_stamp)
        return;
    s_vision_stamp = stamp;

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
                const int (*off)[2] = hex_neighbors::offsets(t.grid_y);
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
            const logistics_path& lp = intra_body_path(w, body, centre, mc);
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
        const logistics_path& lp = intra_body_path(w, body, st, dt);
        if (!lp.reachable || lp.tiles.empty()) continue;
        std::vector<entity_id> path = lp.tiles; // copied: reversed below, cache stays canonical
        if (st != std::min(st, dt)) std::reverse(path.begin(), path.end());
        state.convoy_beams.push_back(
            { std::move(path), std::clamp(cv.progress, 0.0f, 1.0f), std::max(cv.speed, 0.0f) });
    }
}

void draw_body_surface_canvas(const world& w, ui_state& state, const recipe_registry& reg,
                              const economy_report& report, const generation_report& gen,
                              ImVec2 origin, ImVec2 size,
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
            dl->AddText(origin + (size - ts) * 0.5f, IM_COL32(150, 150, 150, 255), msg); // fit-exempt: legend box sized to its measured entries (container 2)
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
        dl->AddText(origin + ImVec2{4.0f, 2.0f}, IM_COL32(235, 235, 235, 255), title); // fit-exempt: legend box sized to its measured entries (container 2)
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
    // hit-zone registration (BL-059), and the corp owning each built tile (marker
    // colour + Corporation lens). BL-362: all three are cached behind the shared
    // stamp — the building/asset sets only move on the events the stamp tracks —
    // and scoped to the active body (queries only ever ask about its tiles).
    static body_frame_stamp s_marker_stamp;
    static std::unordered_map<entity_id, building_type> built_tiles;
    static std::unordered_map<entity_id, entity_id>     tile_to_bld;  // tile_id → building entity
    static std::unordered_map<entity_id, entity_id>     tile_to_corp; // tile_id → owning corp
    // BL-367: how many buildings stand on the tile — the "+N" badge count, and
    // the signal that a marker click should land on the TILE (grouped stack
    // list) rather than assuming the whole hex is one installation.
    static std::unordered_map<entity_id, int>            tile_bld_count;
    const body_frame_stamp marker_stamp = make_body_frame_stamp(w, state.active_body);
    if (!(marker_stamp == s_marker_stamp))
    {
        s_marker_stamp = marker_stamp;
        built_tiles.clear();
        tile_to_bld.clear();
        tile_to_corp.clear();
        tile_bld_count.clear();
        for (const auto& [bld_id, bld] : w.buildings)
        {
            auto tile_it = w.tiles.find(bld.tile);
            if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
            {
                ++tile_bld_count[bld.tile];
                // Lowest building id wins the tile. w.buildings is an unordered_map, so a
                // plain last-writer-wins assignment would let its iteration order pick the
                // representative — fine while a tile holds one building, not once they
                // stack. Lowest-id is stable across frames and across runs.
                const auto prev = tile_to_bld.find(bld.tile);
                if (prev != tile_to_bld.end() && prev->second <= bld_id)
                    continue;
                built_tiles[bld.tile] = bld.type;
                tile_to_bld[bld.tile] = bld_id;
            }
        }
        for (const auto& [corp_id, corp] : w.corporations)
        {
            for (entity_id bld_id : corp.assets)
            {
                const auto bld_it = w.buildings.find(bld_id);
                if (bld_it == w.buildings.end())
                    continue;
                const auto tile_it = w.tiles.find(bld_it->second.tile);
                if (tile_it != w.tiles.end() && tile_it->second.body == state.active_body)
                    tile_to_corp[bld_it->second.tile] = corp_id;
            }
        }
    }

    // Spatial index (BL-268): the per-body raster logistics already caches on
    // world.body_tile_index (grid_y*gw + grid_x -> tile entity, null_entity for an
    // absent cell). app::render ensures it is built for the active body before this
    // draw, so the const world& here just reads it — replacing the per-frame
    // unordered_map rebuild that scanned every body's tiles each frame. An absent
    // entry (a tileless body) yields an empty raster, which draws nothing — loud
    // in a golden, never a silent slow path.
    static const std::vector<entity_id> no_raster;
    const auto raster_it = w.body_tile_index.find(state.active_body);
    const std::vector<entity_id>& raster =
        raster_it != w.body_tile_index.end() ? raster_it->second : no_raster;
    const bool raster_ok = raster.size() == static_cast<std::size_t>(gw) * gh;

    // Grid-coordinate lookup into the raster; bounds-checked so callers can pass
    // an out-of-range row (rows do not wrap) and get null_entity, exactly as the
    // old map's find-miss did.
    auto tile_at_rc = [&](int col, int row) -> entity_id {
        if (!raster_ok || col < 0 || col >= gw || row < 0 || row >= gh)
            return null_entity;
        return raster[static_cast<std::size_t>(row) * gw + col];
    };

    // Nation owner of a tile, or null_entity when the tile is absent from
    // tile_to_nation (unclaimed). Used by the border pass to compare adjacent
    // owners; "unclaimed" is treated as its own distinct owner.
    auto nation_of = [&](entity_id tile_id) -> entity_id {
        const auto it = w.tile_to_nation.find(tile_id);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };

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
    // w.convoys is populated by dispatch_convoys (supply_system.cpp) each tick.
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

    // Continent lens pre-pass (BL-226): the Continents/Drift pass (BL-210) decided
    // where the land ended up, then folded its verdict into Pass 1's heightmap and
    // vanished — by the time a tile exists, the plate that raised it is unreadable
    // from the terrain. The generation report retains the plate field precisely so
    // this lens can put it back on the map. Matched by ENTITY ID (BL-257) — body
    // names are generated and display-only, so the old name match was a display
    // string standing in for an identity.
    const continent_state* plates = nullptr;
    if (state.overlay == overlay_mode::continent)
    {
        for (const auto& be : gen.bodies)
            if (be.id == state.active_body) { plates = &be.continents; break; }
        // A world generated before the field was retained (or a body absent from
        // the report) leaves this null — the lens then draws plain terrain and
        // says so in its key, rather than inventing plates.
        if (plates && plates->plate_id.size() !=
                static_cast<std::size_t>(body.grid_width) * static_cast<std::size_t>(body.grid_height))
            plates = nullptr;
    }

    // Industry lens pre-pass (BL-084; re-pointed BL-373). The lens used to tint by
    // tile.substrate_density — an abstract occupation scalar left over from the
    // nation-substrate injection. BL-365 replaced that substrate with REAL background
    // corporations that own real buildings, so the lens now reads the thing its name
    // always promised: the background firms' plant standing on each tile. The question
    // it answers is "where is the industry I did not build?" — deliberately distinct
    // from the Production lens, which ranks output intensity INCLUDING the player's own
    // holdings. substrate_density survives untouched; it just stops feeding this lens.
    //
    // Per tile: Σ over background-owned buildings of (0.5 + 0.5 × output share). The
    // 0.5 floor keeps an idle or still-building background plant visible — its presence
    // is the fact the lens reports — while output separates a token works from a real
    // one, and two buildings on a tile stack.
    std::unordered_map<entity_id, float> industry_field;
    float industry_max = 0.0f;
    if (state.overlay == overlay_mode::industry)
    {
        const auto background_owned = [&w](entity_id corp) {
            const auto cit = w.corporations.find(corp);
            return cit != w.corporations.end() && cit->second.is_background;
        };

        // Body-local output ceiling, so the share term is relative to this body's own
        // background industry rather than a cross-body absolute.
        float max_out = 0.0f;
        for (const building_report& br : report.buildings)
            if (br.body == state.active_body && background_owned(br.corp))
                max_out = std::max(max_out, br.output_quantity);

        for (const building_report& br : report.buildings)
        {
            if (br.body != state.active_body || !background_owned(br.corp))
                continue;
            const auto bld_it = w.buildings.find(br.building);
            if (bld_it == w.buildings.end())
                continue;
            const float share = (max_out > 0.0f)
                                    ? std::clamp(br.output_quantity / max_out, 0.0f, 1.0f)
                                    : 0.0f;
            float& thr = industry_field[bld_it->second.tile];
            thr += 0.5f + 0.5f * share;
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
                    const int (*off)[2] = hex_neighbors::offsets(t.grid_y);
                    for (int k = 0; k < 6; ++k)
                    {
                        const int nrow = t.grid_y + off[k][1];
                        if (nrow < 0 || nrow >= gh) continue;
                        int ncol = (t.grid_x + off[k][0]) % gw;
                        if (ncol < 0) ncol += gw;
                        const entity_id nb = tile_at_rc(ncol, nrow);
                        if (nb == null_entity) continue;
                        if (seen.insert(nb).second)
                        {
                            next.push_back(nb);
                            float& v = beam_intensity[nb]; if (inten > v) v = inten;
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

    // A tile's vision scalar (0..1): 1 inside the permanent layers (building pockets +
    // the corp-centre->market corridors), else the moving beam's intensity. Hoisted out
    // of the tile loop's fill block for BL-185, because the road pass now needs the same
    // number for a *neighbour* tile — the fog must size identically wherever it is read.
    auto tile_vision = [&](entity_id tid) -> float {
        if (state.permanent_vision.find(tid) != state.permanent_vision.end())
            return 1.0f;
        const auto bi = beam_intensity.find(tid);
        return (bi != beam_intensity.end()) ? bi->second : 0.0f;
    };

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

    // --- Fill level-of-detail (BL-269) ------------------------------------------
    // MEASURED 2026-08-09, pan_perf on the real renderer, Kepler (15,120 tiles):
    //
    //   zoom            vertices   submit ms
    //   0.9 whole grid   157,084      14.25
    //   1.1                6,172       0.47
    //   3.0               23,620       1.95
    //
    // A 25x vertex cliff at the whole-grid view, and submit alone blows the 8 ms
    // frame budget there. Panning is NOT the cost -- the static and panning phases
    // measured identically (157,084 vs 158,407) -- which is why this is a draw-cost
    // fix and not an input fix. Panning only makes the hitches visible, because a
    // frame that takes 3x as long applies 3x the accumulated MouseDelta at once:
    // the right destination by a jumpy route.
    //
    // The cause is `AddConvexPolyFilled(verts, 6, ...)`: with anti-aliasing on, a
    // 6-gon costs ~10 vertices once the AA fringe is counted, times every tile.
    // `AddRectFilled` with no rounding costs 4 and emits no fringe.
    //
    // The threshold is DERIVED, not picked. A hexagon differs from its inscribed
    // rect by the corner cut, `draw_r * (1 - sqrt(3)/2)` = `draw_r * 0.134`. That
    // difference stops being drawable when it falls under one pixel, i.e. at
    // draw_r < 1 / 0.134 = 7.46 px. Round down to 7: at that radius the corner cut
    // is 0.94 px, and the per-tile landform icons (drawn at 0.42 * draw_r, so ~3 px)
    // are already unreadable.
    //
    // The first attempt used 5 px and did not fire at all — at a 1080-tall window
    // the whole-grid view sits at draw_r ~5.1-5.7, just the wrong side of it. Worth
    // recording, because it is the argument for deriving the bound from the geometry
    // rather than eyeballing a constant that happens to work at one window size.
    //
    // Terrain colour, relief shading, the survey mask and the fog wash are all
    // UNAFFECTED — they are colour, not geometry — so the analytic read the
    // whole-grid view exists for is exactly as legible.
    constexpr float k_lod_radius_px = 7.0f;
    const bool      coarse_fill     = draw_r <= k_lod_radius_px;

    // --- Infinite horizontal scroll ---
    // The grid is a cylinder: column gw wraps onto column 0. In screen space the
    // grid repeats every `period_px = gw * col_step * zoom`. Each tile is drawn
    // (and hit-tested) at every integer wrap offset k whose copy falls within the
    // canvas, so panning past either edge continues seamlessly from the far side.
    const float period_px = static_cast<float>(gw) * col_step * zoom;
    const float visible_left  = grid_area_origin.x - hit_r;
    const float visible_right = grid_area_origin.x + grid_area_size.x + hit_r;

    // Draw the active body's tiles in a deterministic order — row-major over the
    // raster. This IS the old sorted-by-id order (tile generation creates each
    // body's tiles rows-outer with sequential create_entity ids), so overlapping
    // antialiased hex edges and markers land exactly as before and no golden
    // moves. BL-268 replaces the old per-frame collect-and-sort with the cached
    // raster, and culls to the visible ROW band before any per-tile work: rows do
    // not wrap, so the band falls straight out of the clip rect, mirroring the
    // ±hit_r margin the horizontal wrap-window below already uses. Column
    // visibility is the wrap-window test itself, hoisted to the top of the loop
    // body — an off-screen tile costs one bounds test and one multiply-compare.
    const float row_pitch = 1.5f * hex_size; // hex_local_centre's vertical pitch
    const int row_lo = raster_ok ? std::max(
        0, static_cast<int>(std::floor(
               ((grid_area_origin.y - hit_r - view_origin.y) / zoom + grid_cy) / row_pitch)))
        : 0;
    const int row_hi = raster_ok ? std::min(
        gh - 1, static_cast<int>(std::ceil(
               ((grid_area_origin.y + grid_area_size.y + hit_r - view_origin.y) / zoom + grid_cy) / row_pitch)))
        : -1;

    for (int t_row = row_lo; t_row <= row_hi; ++t_row)
    for (int t_col = 0; t_col < gw; ++t_col)
    {
        const entity_id id = raster[static_cast<std::size_t>(t_row) * gw + t_col];
        if (id == null_entity)
            continue;
        const tile_component& tile = w.tiles.at(id);

        const ImVec2 lc   = hex_local_centre(tile.grid_x, tile.grid_y, hex_size);
        const ImVec2 sc   = to_screen(lc);

        // Column cull: the horizontal wrap-window (computed once here, reused by
        // the draw below). No wrap copy of this tile lands inside the canvas —
        // skip before the built/owner/lens work, which is the expensive part.
        const int k_min = (period_px > 0.0f)
            ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
        const int k_max = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;
        if (k_min > k_max)
            continue;

        // Does this tile carry a building, and who owns it? Resolved before the fill
        // so a built tile can start from its owner plate instead of terrain, and so
        // the marker pass below reuses the one lookup.
        const auto   built_it   = built_tiles.find(id);
        const bool   built      = built_it != built_tiles.end();
        const building_type built_type = built ? built_it->second : building_type::none;
        const auto   corp_it    = tile_to_corp.find(id);
        const bool   has_owner  = corp_it != tile_to_corp.end();
        const ImU32  owner_col  = has_owner ? corp_identity(corp_it->second)
                                            : IM_COL32(255, 255, 255, 255);

        // BL-429: the representative building's extraction target / processing
        // primary output, so the on-canvas marker draws the same named-building
        // glyph as the Build door and the Buildings tab. Reuses tile_to_bld's
        // lowest-id-wins representative (BL-367) — resolved once per tile, ahead
        // of the k-loop below, so every wrap copy shares one identity.
        resource_type marker_identity = resource_type::iron_ore;
        if (built)
        {
            if (const auto ctb_it = tile_to_bld.find(id); ctb_it != tile_to_bld.end())
                if (const auto cbld_it = w.buildings.find(ctb_it->second); cbld_it != w.buildings.end())
                {
                    const building_component& rep = cbld_it->second;
                    marker_identity =
                        (rep.type == building_type::processing_facility
                         && reg.get_recipe(rep.recipe) != nullptr)
                            ? primary_output_resource(*reg.get_recipe(rep.recipe))
                            : rep.target_resource;
                }
        }

        // Fill starts as the tile's terrain colour — EXCEPT on a built tile, which is
        // swapped out wholesale for its owner plate (2026-07-22): a tile that carries a
        // building renders AS an installation, with no terrain showing through around
        // the glyph. Lens tints then composite over the plate exactly as they do over
        // terrain — a lens is a mode the player chose, so suppressing it where the
        // player's own assets sit would blind it where it matters most.
        //
        // Under the Faction lens a tile owned by a nation is tinted that nation's
        // identity colour. The tint is a direct replacement (no blend); unclaimed
        // tiles — absent from tile_to_nation, e.g. ocean — keep their terrain hue so
        // the political map still reads as terrain underneath.
        ImU32 fill = built ? built_plate_colour(has_owner, owner_col)
                           : terrain_colour(tile.composition);
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
            if (has_owner)
                fill = owner_col;
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
        // Industry lens (BL-084/BL-373): tint a tile by the background-firm plant
        // standing on it (pre-pass above), normalised to the body max — brightest amber
        // where the industry the player did not build is densest. Tiles carrying no
        // background building keep their terrain hue.
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
        // Continent lens (BL-226): tint each tile by the tectonic plate that owns
        // it, and brighten the tiles that touch another plate. The boundary is the
        // point of the lens — a plate interior is just a region, but a boundary is
        // where the mountains, the rifts and the porphyry copper came from, so it
        // gets the emphasis rather than reading as an incidental edge.
        else if (state.overlay == overlay_mode::continent && plates)
        {
            const int gw  = body.grid_width;
            const int gh  = body.grid_height;
            const int idx = tile.grid_x + tile.grid_y * gw;
            const int me  = plates->plate_id[static_cast<std::size_t>(idx)];

            // Columns wrap (the grid is a cylinder, TILE_GENERATION.md); rows do not.
            bool boundary = false;
            const int cols[2] = { (tile.grid_x + 1) % gw, (tile.grid_x + gw - 1) % gw };
            for (const int c : cols)
                if (plates->plate_id[static_cast<std::size_t>(c + tile.grid_y * gw)] != me) boundary = true;
            for (const int dy : { -1, 1 })
            {
                const int ry = tile.grid_y + dy;
                if (ry < 0 || ry >= gh) continue;
                if (plates->plate_id[static_cast<std::size_t>(tile.grid_x + ry * gw)] != me) boundary = true;
            }

            // The plate tint is near-opaque: this lens is about the plate field,
            // not the terrain under it, and a weak blend let the terrain's own
            // hues swamp the categories. The boundary reads on a SEPARATE channel
            // — a lift toward white on top of the plate colour — because "more of
            // the same colour" is not a visible difference, which is exactly how
            // the first draft's boundaries disappeared.
            fill = lerp_colour(fill, plate_colour(me), 0.80f);
            if (boundary)
                fill = lerp_colour(fill, IM_COL32(255, 255, 245, 255), 0.45f);
        }
        // Player-owned tile? Drives the persistent, lens-independent ownership
        // accent: a subtle wash at the plain default plus an outline under every
        // lens (drawn in the border pass), so "these tiles are mine" reads without
        // the player having to pick the Corporation lens.
        const bool is_player_tile = has_owner && corp_it->second == w.player_entity;
        // Plain-default wash: tint the player's own tiles with the player identity
        // colour when no lens is active. Suppressed under any lens — the outline
        // carries identity there, so the wash never fights a lens fill — and on a
        // built tile, whose owner plate already IS the player's identity colour.
        if (is_player_tile && !built && state.overlay == overlay_mode::none)
            fill = lerp_colour(fill, corp_identity(w.player_entity), 0.30f);

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

        // Landform relief (BL-231). Composited HERE — after every lens tint and the
        // suitability wash — because composition owns hue and the lenses composite over
        // that hue at 0.6–0.80 alpha; relief folded into the base fill would be buried
        // exactly when a lens is active. Landform drives movement cost (×1.0–×2.0), hazard,
        // habitability and mineral richness, and that cost applies whether or not a lens
        // is on, so this is always-on terrain chrome rather than an overlay_mode.
        //
        // Skipped on a built tile: that hex is swapped wholesale for its owner plate as
        // an identity signal, and shading it would muddy whose it is. No read is lost —
        // the elevation matters when SITING, and a built tile has already been sited.
        if (!built)
            fill = landform_relief(fill, tile.landform);

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
        // them. Survey mask owns unrevealed tiles, so this skips them. The road pass
        // below reads the same scalar (BL-185) — hence the hoist out of the branch.
        const float vision = tile_vision(id);
        if (revealed)
            fill = fog_dim(fill, vision);

        // Wrap copies inside the canvas: k_min/k_max were computed at the top of
        // the loop body (BL-268), where they double as the column cull.
        for (int k = k_min; k <= k_max; ++k)
        {
            const float cx = sc.x + static_cast<float>(k) * period_px;
            const float cy = sc.y;

            // The vertices are still needed below — hover and selection outline this
            // hex, and those read as hexes at any zoom. Computing them is arithmetic;
            // it is EMITTING them as a filled 6-gon that costs.
            ImVec2 verts[6];
            hex_vertices(verts, cx, cy, draw_r);

            // Coarse fill below the LOD threshold (BL-269): a rect instead of a
            // 6-gon, ~4 vertices against ~10 and no AA fringe.
            //
            // SIZED TO THE GRID STEP, not to the hex radius. Rows step by
            // 1.5 * hex_size while a hex is 2 * hex_size tall — consecutive rows
            // OVERLAP. A rect built from the radius is shorter than the row pitch and
            // the terrain renders as horizontal stripes, which is exactly what the
            // first attempt did. Because odd rows are offset by half a column, rects
            // of (col_step x row_step) brick-lay and tile the plane exactly.
            //
            // `hex_size * zoom` is recovered as `draw_r + 1` (draw_r is that minus the
            // 1 px border shrink), and the same 1 px is taken back off each axis so the
            // background still shows through as the grid texture the hexes give.
            if (coarse_fill)
            {
                const float step = draw_r + 1.0f; // hex_size * zoom
                const float hw   = kSqrt3 * step * 0.5f - 0.5f;
                const float hh   = 1.5f   * step * 0.5f - 0.5f;
                dl->AddRectFilled({ cx - hw, cy - hh }, { cx + hw, cy + hh }, fill);
            }
            else
                dl->AddConvexPolyFilled(verts, 6, fill);

            // Masked region: locked fill only — no borders, markers, selection, or
            // hit-testing for this copy. This single gate also *is* the rival-marker
            // visibility rule (BL-068): a marker — yours or a rival's — shows iff its
            // tile sits in a survey-revealed region, so no separate per-owner gate is
            // needed on the marker pass below. The competitor information asymmetry
            // lives at read time in the hover card and selection panel, not here.
            if (!revealed)
                continue;

            // Road network (BL-146/BL-172 generated + BL-147/BL-172 player-placed). Always-on
            // under every lens (roads are terrain, not an overlay). BL-172 span/symmetry fix:
            // each roaded tile draws its OWN half of every shared road edge — from its centre to
            // the MIDPOINT of the centre-to-neighbour line — toward each roaded, survey-revealed
            // cardinal neighbour (the 4 directions the intra-body A* actually traverses; ocean is
            // never roaded, so "land" is implicit). Two roaded tiles' halves meet exactly at the
            // shared-edge midpoint = one continuous span, identical whichever tile is "from", and
            // the survey fog clips cleanly (a masked neighbour draws nothing). A small centre cap
            // rounds junctions and keeps an isolated / just-placed road tile visible. Styled by
            // THIS tile's tier — Track(1) thin/dim, Road(2) medium, Highway(3) thick/bright — so a
            // tier change reads as a taper at the midpoint. Seam-crossing edges shift one period.
            //
            // BL-185: roads dim with the intra-body reach fog, through the same fog_dim wash
            // the lens fill takes, so an unreached road recedes with the ground under it rather
            // than reading as brightly as one on your own corridor. A road edge spans two tiles,
            // so the pair's vision is combined with MAX — a road is lit if EITHER end is reached.
            // Max is the choice for two reasons: it is SYMMETRIC, so both tiles' halves fog to the
            // same value and the span stays one continuous weight (the BL-172 no-from/to-asymmetry
            // property the geometry already guarantees); and reach is a flood outward from where
            // the player operates, so an edge touching a reached tile is inside that reach — a
            // corridor's roads should not darken one hop early at its rim. Survey (BL-067) still
            // owns genuinely unrevealed tiles; this is only the commercial-reach fog.
            if (tile.road_level > 0)
            {
                ImU32 col; float thick;
                switch (tile.road_level)
                {
                    case 1:  col = IM_COL32(175, 158, 120, 205); thick = std::max(1.2f, draw_r * 0.12f); break;
                    case 2:  col = IM_COL32(205, 188, 140, 225); thick = std::max(1.8f, draw_r * 0.18f); break;
                    default: col = IM_COL32(225, 205, 150, 238); thick = std::max(2.4f, draw_r * 0.24f); break;
                }

                // The junction cap takes the brightest edge meeting at this centre, so it
                // never reads as a dark blot on the end of a lit span.
                float cap_vision = vision;

                static const int card_off[4][2] = {{+1, 0}, {-1, 0}, {0, +1}, {0, -1}};
                for (int n = 0; n < 4; ++n)
                {
                    const int nrow = tile.grid_y + card_off[n][1];
                    if (nrow < 0 || nrow >= gh)
                        continue;
                    const int raw_col = tile.grid_x + card_off[n][0];
                    int ncol = raw_col % gw;
                    if (ncol < 0)
                        ncol += gw;

                    const entity_id nb_id = tile_at_rc(ncol, nrow);
                    if (nb_id == null_entity)
                        continue;
                    const auto nb_tile_it = w.tiles.find(nb_id);
                    if (nb_tile_it == w.tiles.end() || nb_tile_it->second.road_level == 0)
                        continue;
                    if (!survey_tile_visible(body.survey, gw, gh, ncol, nrow))
                        continue;

                    ImVec2 nb_sc = to_screen(hex_local_centre(ncol, nrow, hex_size));
                    nb_sc.x += static_cast<float>(k) * period_px;
                    if (raw_col >= gw)      nb_sc.x += period_px; // east across the cylinder seam
                    else if (raw_col < 0)   nb_sc.x -= period_px; // west across the seam

                    // This tile's half only: centre -> shared-edge midpoint (the neighbour draws
                    // its half, and the two meet — continuous and symmetric, no "from vs to").
                    const ImVec2 mid = {(cx + nb_sc.x) * 0.5f, (cy + nb_sc.y) * 0.5f};
                    const float edge_vision = std::max(vision, tile_vision(nb_id));
                    cap_vision = std::max(cap_vision, edge_vision);
                    dl->AddLine({cx, cy}, mid, fog_dim(col, edge_vision), thick);
                }
                // Centre cap: rounds junctions and keeps a lone / just-placed road tile visible.
                dl->AddCircleFilled({cx, cy}, std::max(1.5f, thick * 0.75f),
                                    fog_dim(col, cap_vision));
            }

            // Rivers (BL-170 data; this render is new, 2026-08-02). Always-on terrain like
            // roads. Drawn ONCE per edge from the UPSTREAM tile only — river_downstream bit
            // set on a side means that side is this tile's outflow direction, so checking
            // both river_edges and river_downstream picks exactly one of the two tiles
            // sharing the edge as its drawer, and the line direction (this tile -> neighbour)
            // is always the true flow direction with no separate lookup needed.
            if (tile.river_edges != 0)
            {
                const int (*r_off)[2] = hex_neighbors::offsets(tile.grid_y);

                const ImU32 river_col    = IM_COL32(90, 160, 235, 235);
                const ImU32 chevron_col  = IM_COL32(220, 235, 255, 245);
                const float river_thick  = std::max(1.5f, draw_r * 0.16f);

                for (int side = 0; side < 6; ++side)
                {
                    const auto bit = static_cast<std::uint8_t>(1u << side);
                    if (!(tile.river_edges & bit) || !(tile.river_downstream & bit))
                        continue; // no river here, or this tile is the downstream half (already drawn from upstream)

                    const int nrow = tile.grid_y + r_off[side][1];
                    if (nrow < 0 || nrow >= gh)
                        continue;
                    const int raw_col = tile.grid_x + r_off[side][0];
                    int ncol = raw_col % gw;
                    if (ncol < 0)
                        ncol += gw;

                    if (tile_at_rc(ncol, nrow) == null_entity)
                        continue;
                    if (!survey_tile_visible(body.survey, gw, gh, ncol, nrow))
                        continue;

                    ImVec2 nb_sc = to_screen(hex_local_centre(ncol, nrow, hex_size));
                    nb_sc.x += static_cast<float>(k) * period_px;
                    if (raw_col >= gw)      nb_sc.x += period_px;
                    else if (raw_col < 0)   nb_sc.x -= period_px;

                    float dirx = nb_sc.x - cx;
                    float diry = nb_sc.y - cy;
                    const float len = std::sqrt(dirx * dirx + diry * diry);
                    if (len <= 0.0f)
                        continue;
                    dirx /= len;
                    diry /= len;
                    const float px = -diry;
                    const float py =  dirx;

                    dl->AddLine({cx, cy}, nb_sc, river_col, river_thick);

                    // Chevron cadence ("every 2 tiles"): no per-edge distance-from-source is
                    // stored (generate_rivers traces tile-by-tile with no persisted path
                    // index), so this approximates the cadence with the upstream tile's grid
                    // parity — cheap, deterministic, and roughly-alternating rather than an
                    // exact along-river count.
                    if (((tile.grid_x + tile.grid_y) & 1) == 0)
                    {
                        const float mx   = (cx + nb_sc.x) * 0.5f;
                        const float my   = (cy + nb_sc.y) * 0.5f;
                        const float chev = std::max(2.5f, draw_r * 0.3f);
                        const ImVec2 tip   = {mx + dirx * chev * 0.5f, my + diry * chev * 0.5f};
                        const ImVec2 wing1 = {mx - dirx * chev * 0.3f + px * chev * 0.55f,
                                              my - diry * chev * 0.3f + py * chev * 0.55f};
                        const ImVec2 wing2 = {mx - dirx * chev * 0.3f - px * chev * 0.55f,
                                              my - diry * chev * 0.3f - py * chev * 0.55f};
                        dl->AddLine(wing1, tip, chevron_col, river_thick * 0.9f);
                        dl->AddLine(wing2, tip, chevron_col, river_thick * 0.9f);
                    }
                }
            }

            // Nation borders (Country lens only). Draw a dark line on every hex
            // edge shared with a neighbour of a different owner — including the
            // claimed/unclaimed boundary. The grid is odd-r offset, so the six
            // neighbour offsets differ between even and odd rows.
            if (state.overlay == overlay_mode::country)
            {
                const entity_id own_nation = nation_of(id);

                // Standard odd-r neighbour offsets (col, row deltas; canonical table, BL-363).
                const int (*off)[2] = hex_neighbors::offsets(tile.grid_y);

                for (int n = 0; n < 6; ++n)
                {
                    const int nrow = tile.grid_y + off[n][1];
                    if (nrow < 0 || nrow >= gh)
                        continue; // Off the top/bottom edge: no neighbour tile.

                    // Columns wrap on the horizontal cylinder.
                    int ncol = (tile.grid_x + off[n][0]) % gw;
                    if (ncol < 0)
                        ncol += gw;

                    const entity_id nb_id = tile_at_rc(ncol, nrow);
                    if (nb_id == null_entity)
                        continue;
                    if (nation_of(nb_id) == own_nation)
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
                // `mr` is the legacy marker radius, kept solely as the hit-zone scale
                // below so click routing is unchanged by the silhouette resize.
                const float mr    = std::max(2.0f, draw_r * 0.22f);
                // The silhouette is the tile's content now, so it scales to the hex.
                const float sil_r = std::max(3.0f, draw_r * kBuiltSilhouetteScale);

                // The structure reads PALE against its own (dark, owner-tinted) plate,
                // and still reads when a saturated lens fill is composited over that
                // plate. Lightening toward white keeps the owner hue, so identity
                // survives; an unowned tile's owner_col is already white, so it stays
                // white through the same blend.
                const ImU32 marker_col =
                    lerp_colour(owner_col, IM_COL32(255, 255, 255, 255), 0.5f);

                // BL-327 (replacing BL-323 S4's dimming, same-day: Ben found the
                // desaturated silhouette read as "faded", not "being built"): a
                // site with ticks_remaining > 0 draws the dedicated crane glyph
                // IN PLACE OF its type silhouette, at full owner-tinted colour —
                // identity still reads, the type does not, which is honest: the
                // installation is not that type yet. ticks_remaining is the single
                // source of truth economy_system counts down; no separate flag.
                bool under_construction = false;
                if (k == 0)
                {
                    const auto ctb_it = tile_to_bld.find(id);
                    if (ctb_it != tile_to_bld.end())
                    {
                        const auto cbld_it = w.buildings.find(ctb_it->second);
                        if (cbld_it != w.buildings.end() && cbld_it->second.ticks_remaining > 0)
                            under_construction = true;
                    }
                }

                // The Workforce (Population lens) and Opportunity lenses replace the
                // building silhouette with the per-tile value mark drawn below
                // (BL-135) — the mark reads the tile's rank, not its installation.
                if (state.overlay != overlay_mode::population &&
                    state.overlay != overlay_mode::opportunity)
                {
                    if (under_construction)
                        icons::under_construction(dl, {cx, cy}, sil_r, marker_col);
                    else
                        icons::building(dl, {cx, cy}, sil_r, built_type, marker_identity, marker_col);
                }

                // Owner-identity tag (BL-090): a small corp emblem tucked into the
                // hex's lower-right corner, for BOTH player and rival buildings —
                // the owning corp is public under the BL-068 visibility model, so this
                // adds no leak. Shape + colour route through the shared palette source
                // of truth, so the tag matches the identity card and the Selection
                // header. Parked past the enlarged silhouette (offsets are fractions of
                // the hex circumradius, chosen to sit inside the lower-right edges) and
                // backed by a dark disc so it never gets lost against plate, lens fill,
                // or glyph. Does not affect hit-testing.
                if (has_owner)
                {
                    const entity_id owner = corp_it->second;
                    const float     er    = std::max(1.5f, draw_r * 0.15f);
                    const ImVec2    ec    { cx + draw_r * 0.56f, cy + draw_r * 0.40f };
                    dl->AddCircleFilled(ec, er * 1.40f, IM_COL32(18, 20, 26, 225), 16);
                    icons::corp_emblem(dl, ec, er,
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
                        // BL-367: one marker still stands for the whole tile (no
                        // per-stack markers — the "+N" badge below is what tells
                        // the two apart), so a tile with more than one building no
                        // longer assumes the whole hex is a single installation —
                        // the click lands on the TILE (grouped stack list) instead
                        // of jumping into whichever building sorts lowest-id.
                        const int count = tile_bld_count.count(id) ? tile_bld_count.at(id) : 1;
                        marker_hit_zone hz;
                        hz.id     = (count > 1) ? id : bld_it->second;
                        hz.kind   = marker_hit_zone::kind::building;
                        hz.centre = {cx, cy};
                        hz.radius = mr * 2.0f;
                        state.marker_hit_zones.push_back(hz);

                        if (count > 1)
                        {
                            // "+N" badge, lower-right — staggered past the corp-identity
                            // tag (also lower-right) per ICONS.md's multi-badge offset
                            // convention, same k/N text idiom the survey badge uses.
                            char nbuf[8];
                            std::snprintf(nbuf, sizeof nbuf, "+%d", count - 1);
                            const ImVec2 bpos{ cx + draw_r * 0.56f, cy + draw_r * 0.68f };
                            const ImU32  bcol = IM_COL32(230, 230, 235, 235);
                            dl->AddCircleFilled(bpos, 8.0f, IM_COL32(18, 20, 26, 225), 12);
                            dl->AddText({bpos.x - 7.0f, bpos.y - 6.0f}, bcol, nbuf); // fit-exempt: on-canvas marker badge, no containing box
                        }
                    }
                }
            }

            // Landform glyph (BL-231): the categorical half of the landform channel.
            // Only the four DRAMATIC landforms draw — mountain, canyon, crater, rift —
            // measured at ≤1.5 % of land tiles each (world_audit § S3) and each carrying
            // a movement cost of ×1.3 or worse, so this is the set where an invisible
            // surprise is expensive. Plains, highland and valley draw nothing; between
            // them plains and valley are ~95 % of land, and an icon on nearly every tile
            // would be far denser than any other glyph family.
            //
            // Unbuilt tiles only: a built hex already carries an enlarged silhouette plus
            // a corp emblem tag, and its cost is spent. Suppressed under the two value
            // lenses, which claim the hex centre for their own mark below (BL-135). Ink
            // contrasts against the finished fill, so the glyph reads over any terrain
            // hue and any lens tint composited on top of it.
            //
            // BRIDGING (BL-232): a run of three mountains should read as ONE range, not
            // three identical icons, so a tile with a same-landform cardinal neighbour
            // draws SPANS instead of its centred glyph — this tile's half of each shared
            // edge, exactly as BL-172's roads do, so the neighbour's half meets it at the
            // midpoint with no cross-tile state and the survey fog clips it cleanly.
            // Measured (world_audit § S4): 71 % of mountain and 81 % of rift tiles have
            // such a neighbour, and modal run length is 2-3. Crater never spans — a basin
            // is a blob, not a line. The all-four-neighbours "filled interior" case that
            // was designed alongside this was CANCELLED on the same measurement: not one
            // tile in the system has four, so it would have been dead code on every seed.
            if (!built && state.overlay != overlay_mode::population &&
                state.overlay != overlay_mode::opportunity)
            {
                const ImU32 ink = contrast_ink(fill);
                bool        spanned = false;

                if (icons::landform_spans(tile.landform))
                {
                    const float amp   = std::max(1.5f, draw_r * 0.20f);
                    const float thick = std::max(1.0f, draw_r * 0.13f);

                    static const int card_off[4][2] = {{+1, 0}, {-1, 0}, {0, +1}, {0, -1}};
                    for (int n = 0; n < 4; ++n)
                    {
                        const int nrow = tile.grid_y + card_off[n][1];
                        if (nrow < 0 || nrow >= gh)
                            continue;
                        const int raw_col = tile.grid_x + card_off[n][0];
                        int ncol = raw_col % gw;
                        if (ncol < 0)
                            ncol += gw;

                        const entity_id nb_id = tile_at_rc(ncol, nrow);
                        if (nb_id == null_entity)
                            continue;
                        const auto nb_tile_it = w.tiles.find(nb_id);
                        if (nb_tile_it == w.tiles.end()
                            || nb_tile_it->second.landform != tile.landform)
                            continue;
                        if (!survey_tile_visible(body.survey, gw, gh, ncol, nrow))
                            continue;

                        ImVec2 nb_sc = to_screen(hex_local_centre(ncol, nrow, hex_size));
                        nb_sc.x += static_cast<float>(k) * period_px;
                        if (raw_col >= gw)    nb_sc.x += period_px; // east across the seam
                        else if (raw_col < 0) nb_sc.x -= period_px; // west across the seam

                        const ImVec2 mid = {(cx + nb_sc.x) * 0.5f, (cy + nb_sc.y) * 0.5f};
                        icons::landform_span(dl, {cx, cy}, mid, amp, thick, tile.landform, ink);
                        spanned = true;
                    }
                }

                // The lone tile keeps its centred glyph — the same role the road's centre
                // cap plays, and needed by 29 % of mountain and 50 % of canyon tiles.
                if (!spanned)
                    icons::landform(dl, {cx, cy}, std::max(3.0f, draw_r * 0.44f),
                                    tile.landform, ink);
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
            // no player convoy touches this body.
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
        struct pop_centre { int col; int row; int scale; entity_id tile; entity_id centre; };
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
            pcs.push_back({ til->second.grid_x, til->second.grid_y, pc.scale, tit->second, pid });
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

        // A City+ conurbation is labelled with its anchor centre's PERSISTED name —
        // generated by the seeded tongue system (generate_city_name, BL-290) and
        // re-named to the settling culture's speech in name_population_centres, so
        // the label is deterministic per campaign and speaks the world's own
        // language. (The old static Earth-flavoured bank was removed, BL-363.)

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

            const auto name_it = w.population_centre_name.find(a.centre);
            const char* name = (name_it != w.population_centre_name.end()
                                && !name_it->second.empty())
                ? name_it->second.c_str() : nullptr;

            const int k_min = (period_px > 0.0f)
                ? static_cast<int>(std::ceil((visible_left  - sc.x) / period_px)) : 0;
            const int k_max = (period_px > 0.0f)
                ? static_cast<int>(std::floor((visible_right - sc.x) / period_px)) : 0;
            for (int k = k_min; k <= k_max; ++k)
            {
                const ImVec2 mc = { sc.x + static_cast<float>(k) * period_px, sc.y };
                icons::settlement(dl, mc, sr, c.tier, col);

                // Label City+ conurbations (tier >= 4) only, to keep the map legible.
                if (c.tier >= 4 && name)
                {
                    const ImVec2 tp{ mc.x + sr + 3.0f, mc.y - sr };
                    dl->AddText({ tp.x + 1.0f, tp.y + 1.0f }, IM_COL32(20, 22, 28, 200), name); // shadow // fit-exempt: legend box sized to its measured entries (container 2)
                    dl->AddText(tp, IM_COL32(236, 230, 214, 255), name); // fit-exempt: legend box sized to its measured entries (container 2)
                }
            }
        }
    }

    // Corporate HQ marker (BL-182 foundation; the border RING retired BL-329,
    // 2026-08-08 — Ben's live critique: "retire the circle around corp
    // buildings. It doesn't show anything informative." The reach fog/lens
    // already shows supply reach properly; this fixed-radius, non-growing ring
    // duplicated that less accurately and added noise). What remains is just
    // the seat marker itself, from the PERSISTED hq_building (designated
    // deterministically at generation) — drawn on the corp's HOME body only
    // (the single-home model; branch offices are deferred with the full
    // BL-182 mechanic).
    auto draw_corp_hq = [&](entity_id corp_id, const corporation_component& cc)
    {
        if (cc.hq_building == null_entity)
            return;
        const auto b = w.buildings.find(cc.hq_building);
        if (b == w.buildings.end())
            return;
        const auto t = w.tiles.find(b->second.tile);
        if (t == w.tiles.end() || t->second.body != state.active_body)
            return;

        const ImU32  accent = corp_identity(corp_id);
        const ImVec2 hq_lc  = hex_local_centre(t->second.grid_x, t->second.grid_y, hex_size);
        const ImVec2 hq_s   = to_screen(hq_lc);
        const float  hq_r   = std::max(4.0f, draw_r * 0.5f);

        const int k_min = (period_px > 0.0f)
            ? static_cast<int>(std::ceil((visible_left  - hq_s.x - hq_r) / period_px)) : 0;
        const int k_max = (period_px > 0.0f)
            ? static_cast<int>(std::floor((visible_right - hq_s.x + hq_r) / period_px)) : 0;
        for (int k = k_min; k <= k_max; ++k)
        {
            const float off = static_cast<float>(k) * period_px;
            icons::hq(dl, { hq_s.x + off, hq_s.y }, hq_r, accent);
        }
    };

    // The player's HQ marker is always-on identity chrome (BL-085 lineage), drawn on
    // the player's home body regardless of the active lens.
    if (state.active_body == w.home_body && w.player_entity != null_entity)
    {
        const auto pc = w.corporations.find(w.player_entity);
        if (pc != w.corporations.end())
            draw_corp_hq(w.player_entity, pc->second);
    }

    // Rival HQ markers (BL-183 lineage): under the Corporation lens, every rival
    // corp's seat reads from the same persisted hq_building via the shared lambda
    // above. The player's own marker is the always-on chrome above (excluded here,
    // no double-draw). Each rival's marker is drawn on its OWN home body
    // (draw_corp_hq gates on the seat's body), so a rival only shows one when its
    // home body is the one on screen.
    if (state.overlay == overlay_mode::corporation)
    {
        for (const auto& [corp_id, cc] : w.corporations)
        {
            if (corp_id == w.player_entity)
                continue;
            draw_corp_hq(corp_id, cc);
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

        const float mr = std::max(3.0f, draw_r * kBuiltSilhouetteScale);

        // Full world-level check (coastal / launchpad, not just terrain) so the
        // ghost is red — and the reason legible — *before* the click, not after
        // construct_building refuses it (BL-071).
        const placement_rules::placement_result pr = placement_rules::can_place_in_world(
            w, hovered_tile, state.construction.type, state.construction.target,
            state.max_logistics_reach);
        const ImU32 ghost_col = pr ? palette::positive : palette::negative;

        // BL-429: only the extraction target is known here (armed placement mode
        // carries no recipe field, verify_api.cpp being its one setter today) — a
        // processing ghost falls through to the generic square, same as before
        // this item.
        icons::building(dl, {gx, gy}, mr, state.construction.type, state.construction.target, ghost_col);

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
            dl->AddText(tp, palette::negative, why); // fit-exempt: legend box sized to its measured entries (container 2)
        }
    }

    dl->PopClipRect();

    // On-canvas lens key (drawn unclipped, flush-left of the minimap so it reads as a
    // drawer folding out from it — anchor passed in as lens_key_anchor; before the
    // input early-out so it shows in headless captures too).
    // Count-driven keys (Country/Market/Reach/Supply) are bounded to the canvas
    // vertical span [key_top, key_bot] so a long entry list scrolls inside the box
    // rather than overrunning the canvas edges (BL-163/164).
    const float key_top = grid_area_origin.y + 8.0f;
    const float key_bot = origin.y + size.y - 8.0f;
    if (state.overlay == overlay_mode::country)
        draw_country_key(lens_key_anchor, key_top, key_bot, w, state);
    else if (state.overlay == overlay_mode::resource)
        draw_resource_key(dl, lens_key_anchor, state);
    else if (state.overlay == overlay_mode::market)
        draw_market_key(lens_key_anchor, key_top, key_bot, w, state, market_catchment_colour);
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
    // BL-376: foreground list, not `dl` (background) — the Selection band is an ImGui
    // window and always open, so a background-list key at this anchor is drawn under
    // it. Same position, higher z-order.
    else if (state.overlay == overlay_mode::continent)
        draw_continent_key(ImGui::GetForegroundDrawList(), lens_key_anchor, plates);
    else if (state.overlay == overlay_mode::reach)
        draw_reach_key(lens_key_anchor, key_top, key_bot, w, reach_links);
    else if (state.overlay == overlay_mode::supply_routes)
        draw_supply_routes_key(lens_key_anchor, key_top, key_bot, w, supply_edges);

    if (!input_enabled)
        return;

    // Hover-card (BL-060, BL-020). Resolve the hovered entity in marker-priority
    // order (building > market_centre > tile — mirroring click priority). Track
    // stable hover ticks and show the lens-contextual "why not what" card after
    // kHoverAppearDelaySec of rest on the same entity.
    {
        // Resolve the highest-priority entity under the cursor (shared with the
        // click path below — resolve_marker_hit, BL-362).
        const entity_id marker_hover =
            resolve_marker_hit(state.marker_hit_zones, mouse.x, mouse.y);

        // A built tile resolves to its BUILDING across the whole hex, not just inside
        // the glyph's radius: once a tile carries an installation, the installation is
        // what is there. The tile beneath it is no longer separately reachable.
        entity_id tile_bld = null_entity;
        if (hovered_tile != null_entity)
            if (const auto tb = tile_to_bld.find(hovered_tile); tb != tile_to_bld.end())
                tile_bld = tb->second;

        entity_id hover_eid = null_entity;
        if (marker_hover != null_entity)
            hover_eid = marker_hover;
        else if (tile_bld != null_entity)
            hover_eid = tile_bld;
        else
            hover_eid = hovered_tile;

        if (hover_eid != state.hovered_entity)
        {
            state.hovered_entity = hover_eid;
            state.hover_seconds  = 0.0f;
        }
        else if (hover_eid != null_entity)
        {
            // Interactive deltas are clamped: one long stall (world gen, alt-tab)
            // would otherwise dump seconds into the accumulator in a single frame
            // and snap a card straight to stuck.
            state.hover_seconds += state.fixed_frame_clock
                                       ? kVerifyFrameSeconds
                                       : std::min(ImGui::GetIO().DeltaTime, kHoverMaxFrameSeconds);
        }

        // Glance-then-stick hover (BL-228/230, retires BL-200's dwell-to-open).
        //
        // Hovering no longer OPENS the Selection band. Opening is the click's job
        // alone — one gesture, one meaning.
        //
        // What hover does now, in two phases: past kHoverAppearDelaySec the card
        // appears as a GLANCE and tracks the live cursor like an ordinary
        // tooltip, so it does not yet own the pointer. Past kHoverStickDelaySec it
        // STICKS — freezes at its current position, stops following the cursor,
        // and stays up until the pointer leaves its bounds. That makes a long
        // line readable (it cannot slide away mid-read) while still letting a
        // player who is only passing through dismiss it by moving on normally.
        if (state.hover_card_entity != null_entity && state.hover_card_stuck)
        {
            // Stuck: dismiss only when the pointer leaves the rect. The pad spans
            // the gap between the anchor and the card drawn above it, so the
            // card does not dismiss itself the frame it freezes.
            const bool inside =
                mouse.x >= state.hover_card_min.x - kHoverCardExitPadPx &&
                mouse.x <= state.hover_card_max.x + kHoverCardExitPadPx &&
                mouse.y >= state.hover_card_min.y - kHoverCardExitPadPx &&
                mouse.y <= state.hover_card_max.y + kHoverCardExitPadPx;

            if (!inside)
            {
                state.hover_card_entity = null_entity;
                state.hover_card_stuck  = false;
            }
        }
        else if (state.hover_card_entity != null_entity)
        {
            // Glancing: the card is not yet stuck, so it lives only as long as
            // the cursor keeps hovering the same entity that summoned it.
            if (hover_eid != state.hover_card_entity)
                state.hover_card_entity = null_entity;
        }

        // Summon a new glance card once the appear delay is met over an entity —
        // but never while one is already up (a glance or stuck card owns the slot
        // until dismissed), and never mid-placement, where a floating card would
        // sit over the ghost.
        if (state.hover_card_entity == null_entity &&
            hover_eid != null_entity &&
            !state.construction.active &&
            state.hover_seconds >= kHoverAppearDelaySec - kHoverThresholdEpsilon)
        {
            state.hover_card_entity = hover_eid;
            state.hover_card_stuck  = false;
        }

        // Promote glance -> stuck once the stick delay elapses, freezing the
        // card at its current (live-cursor) position.
        if (state.hover_card_entity != null_entity &&
            !state.hover_card_stuck &&
            state.hover_seconds >= kHoverStickDelaySec - kHoverThresholdEpsilon)
        {
            state.hover_card_stuck  = true;
            state.hover_card_anchor = { mouse.x, mouse.y };
            // Seed the rect around the anchor so the first frame's hit-test (which
            // runs before the card has been drawn at this position) cannot
            // dismiss it early.
            state.hover_card_min = { mouse.x, mouse.y };
            state.hover_card_max = { mouse.x, mouse.y };
        }

        // While glancing (not yet stuck), the card tracks the live cursor.
        if (state.hover_card_entity != null_entity && !state.hover_card_stuck)
            state.hover_card_anchor = { mouse.x, mouse.y };

        if (state.hover_card_entity != null_entity)
        {
            const entity_id card_eid = state.hover_card_entity;
            draw_hover_card(state.hover_card_anchor, [&]() {
                draw_hover_content(w, state, card_eid);
            }, &state.hover_card_min, &state.hover_card_max);
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
            // Resolve marker hit zones in priority order (BL-031): building
            // outranks market-centre; both outrank tile. Shared with hover.
            const entity_id marker_hit =
                resolve_marker_hit(state.marker_hit_zones, mouse.x, mouse.y);

            // Falling through to the tile means the click missed every marker glyph.
            // On a BUILT tile that still selects the building: the whole hex belongs to
            // the installation, so the tile element is unreachable there (Ben's
            // 2026-07-22 call). The tile element is the prospecting view for UNBUILT
            // ground — routing a built tile to it offers a Construct button that the
            // placement rules then refuse, which is the bug this closes.
            // BL-367: that "whole hex is one installation" assumption only holds for
            // exactly one building — once BL-366 lets a tile stack several, the hex
            // stays reachable as the grouped-by-stack tile view instead.
            entity_id fallback = hovered_tile;
            if (hovered_tile != null_entity)
            {
                const auto tb = tile_to_bld.find(hovered_tile);
                const auto cnt_it = tile_bld_count.find(hovered_tile);
                const int  count = (cnt_it != tile_bld_count.end()) ? cnt_it->second : 0;
                if (tb != tile_to_bld.end() && count <= 1)
                    fallback = tb->second;
            }

            state.selected_entity = (marker_hit != null_entity) ? marker_hit : fallback;
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
