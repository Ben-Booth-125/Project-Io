#include "pinned_panel.hpp"

#include "entity_summary.hpp" // the shared stat block a watch card carries
#include "front_view.hpp"     // fronts_of: the player's land fronts, cached
#include "icons.hpp"
#include "presentation.hpp"
#include "selection.hpp"      // selection_kind_of
#include "shell_metrics.hpp"  // pinned_panel_rect, lens_chrome_this_frame
#include "view_nav.hpp"       // focus_on_entity / focus_on_tile / focus_on_surface

#include "world/fronts.hpp"   // tile_hex_steps
#include "world/province.hpp" // province_kind_of, province_anchor_tile
#include "world/war.hpp"      // war_score, province_owner

#include <algorithm>
#include <climits>
#include <cmath>
#include <map>
#include <cstdio>
#include <string>

namespace ui {

namespace {

/// The kind word a card's header carries, beside its glyph.
const char* kind_word(selection_kind k)
{
    switch (k)
    {
        case selection_kind::body:     return "Body";
        case selection_kind::tile:     return "Tile";
        case selection_kind::building: return "Building";
        case selection_kind::market:   return "Market";
        case selection_kind::unit:     return "Unit";
        case selection_kind::nation:   return "Nation";
        case selection_kind::wing:     return "Wing";
        default:                       return "Pin";
    }
}

/// The kind glyph: the vocabulary's own glyph where one exists (icons.hpp).
void kind_glyph(const world& w, ImDrawList* dl, selection_kind k, entity_id id, ImVec2 c, float r)
{
    const ImU32 col = palette::selection;
    switch (k)
    {
        case selection_kind::body:   dl->AddCircleFilled(c, r * 0.8f, col, 20); break;
        case selection_kind::tile:
        {
            ImVec2 hex[6];
            for (int i = 0; i < 6; ++i)
            {
                const float a = 3.14159265f / 3.0f * static_cast<float>(i) + 3.14159265f / 6.0f;
                hex[i] = { c.x + r * 0.85f * std::cos(a), c.y + r * 0.85f * std::sin(a) };
            }
            dl->AddPolyline(hex, 6, col, ImDrawFlags_Closed, 1.5f);
            break;
        }
        case selection_kind::building:
        {
            const auto it = w.buildings.find(id);
            if (it != w.buildings.end())
                icons::building(dl, c, r, it->second.type, it->second.target_resource, col);
            else
                dl->AddRectFilled({ c.x - r * 0.7f, c.y - r * 0.7f }, { c.x + r * 0.7f, c.y + r * 0.7f }, col, 2.0f);
            break;
        }
        case selection_kind::market: icons::market(dl, c, r, col); break;
        case selection_kind::unit:   icons::helmet(dl, c, r, col); break;
        case selection_kind::nation:
            icons::country(dl, c, r, palette::nation_colour(id));
            break;
        case selection_kind::wing:   icons::aircraft(dl, c, r, col); break;
        default:                     icons::pin(dl, c, r, palette::pinned); break;
    }
}

std::string nation_name(const world& w, entity_id n)
{
    const auto it = w.nations.find(n);
    return it != w.nations.end() && !it->second.name.empty() ? it->second.name : std::string("A nation");
}

constexpr float k_card_gap  = 6.0f;
constexpr float k_card_pad  = 6.0f;
constexpr float k_glyph_r   = 8.0f;
/// A card's stat block stops here; the rest is clipped (a market's table would
/// otherwise fill the column — a pin is a watch card, the band carries the rest).
constexpr float k_card_max_lines = 6.0f;

/// Begin one card: a framed group whose press selects (single) or navigates
/// (double). Returns the card's top-left so `end_card` can frame and hit-test it.
struct card_frame
{
    ImVec2 p0;
    float  w;
};

card_frame begin_card(float inner_w)
{
    card_frame cf;
    cf.p0 = ImGui::GetCursorScreenPos();
    cf.w  = inner_w;
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
    ImGui::SetCursorScreenPos({ cf.p0.x + k_card_pad, cf.p0.y + k_card_pad });
    ImGui::BeginGroup();
    return cf;
}

/// Close the card; paint its frame under it. Returns 1 on a single click, 2 on a
/// double click, 0 otherwise. Records the drawn rect into @p out.
int end_card(const card_frame& cf, ImU32 edge, pinned_items_state::card& out)
{
    ImGui::EndGroup();
    const ImVec2 gmax = ImGui::GetItemRectMax();
    const ImVec2 p1   = { cf.p0.x + cf.w, gmax.y + k_card_pad };
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->ChannelsSetCurrent(0);
    // ChildWindows: a card's stat block is a child window, which would otherwise
    // swallow the hover from the panel it sits in.
    const bool hov = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)
                  && ImGui::IsMouseHoveringRect(cf.p0, p1);
    dl->AddRectFilled(cf.p0, p1, hov ? IM_COL32(44, 48, 62, 255) : IM_COL32(30, 32, 42, 255), 4.0f);
    dl->AddRect(cf.p0, p1, edge, 4.0f, 0, 1.0f);
    dl->ChannelsMerge();
    ImGui::SetCursorScreenPos({ cf.p0.x, p1.y });
    ImGui::Dummy({ cf.w, k_card_gap });

    out.x = (cf.p0.x + p1.x) * 0.5f;
    out.y = (cf.p0.y + p1.y) * 0.5f;
    out.w = p1.x - cf.p0.x;
    out.h = p1.y - cf.p0.y;
    if (!hov)
        return 0;
    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        return 2;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        return 1;
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Front pins
// ---------------------------------------------------------------------------

std::vector<front_pin> derive_front_pins(const world& w)
{
    std::vector<front_pin> out;
    const entity_id me = player_nation(w);
    if (me == null_entity || w.wars.empty())
        return out;

    // The enemies, in war-key order (one war per pair; no alliances).
    std::vector<std::pair<entity_id, const war_record*>> enemies;
    for (const auto& [key, war] : w.wars)
    {
        if (war.declarer == me)
            enemies.push_back({ war.defender, &war });
        else if (war.defender == me)
            enemies.push_back({ war.declarer, &war });
    }
    if (enemies.empty())
        return out;

    const auto& provs = w.provinces.provinces;
    const bool aligned = w.province_holder.size() == provs.size();
    if (!aligned)
        return out;

    // Owners change only with the territory map: cached on its generation stamp.
    static const world*            s_world = nullptr;
    static std::uint64_t           s_gen   = ~std::uint64_t{ 0 };
    static std::vector<entity_id>  s_owner;
    if (s_world != &w || s_gen != w.territory_generation || s_owner.size() != provs.size())
    {
        s_owner.assign(provs.size(), null_entity);
        for (std::size_t i = 0; i < provs.size(); ++i)
            if (province_kind_of(w, provs[i]) == province_kind::land)
                s_owner[i] = province_owner(w, provs[i].id);
        s_world = &w;
        s_gen   = w.territory_generation;
    }

    for (const auto& [enemy, war] : enemies)
    {
        // Per body, in ascending body id: what each side holds.
        std::map<entity_id, front_pin> by_body;
        for (std::size_t i = 0; i < provs.size(); ++i)
        {
            if (province_kind_of(w, provs[i]) != province_kind::land)
                continue;
            const entity_id holder = w.province_holder[i];
            const entity_id owner  = s_owner[i];
            if (holder != me && holder != enemy && owner != me && owner != enemy)
                continue;
            front_pin& f = by_body[provs[i].body];
            f.body  = provs[i].body;
            f.enemy = enemy;
            if (holder == me)                       ++f.mine_held;
            if (holder == enemy)                    ++f.theirs_held;
            if (owner == me && holder == enemy)     ++f.mine_lost;
            if (owner == enemy && holder == me)     ++f.theirs_taken;
        }
        const int score = war_score(w, *war);
        for (auto& [body, f] : by_body)
        {
            if (f.mine_held == 0 || f.theirs_held == 0)
                continue; // no front on this body: one side holds nothing there
            f.war_score = war->declarer == me ? score : -score;

            // Where the double click lands. 1: the player's land front toward the
            // enemy on this body, at the middle of its span.
            for (const front_view& fv : fronts_of(w, me))
                if (fv.f.neighbour == enemy && fv.body == body && !fv.f.span.empty())
                {
                    f.target_tile = fv.f.span[fv.f.span.size() / 2];
                    f.front_key   = fv.f.key;
                    break;
                }
            // 2: a contested province (one side holds the other's ground).
            // 3: the enemy's province nearest the player's own held ground.
            if (f.target_tile == null_entity)
            {
                entity_id mine_anchor = null_entity, contested = null_entity;
                for (std::size_t i = 0; i < provs.size() && contested == null_entity; ++i)
                {
                    if (provs[i].body != body || province_kind_of(w, provs[i]) != province_kind::land)
                        continue;
                    const entity_id holder = w.province_holder[i];
                    const entity_id owner  = s_owner[i];
                    if ((owner == me && holder == enemy) || (owner == enemy && holder == me))
                        contested = province_anchor_tile(w, provs[i]);
                    else if (holder == me && mine_anchor == null_entity)
                        mine_anchor = province_anchor_tile(w, provs[i]);
                }
                f.target_tile = contested;
                // "The player's ground" is measured from its capital when the
                // capital stands on this body; else from its lowest held province.
                if (const auto n = w.nations.find(me); n != w.nations.end())
                    if (const auto t = w.tiles.find(n->second.capital_tile);
                        t != w.tiles.end() && t->second.body == body)
                        mine_anchor = n->second.capital_tile;
                if (f.target_tile == null_entity)
                {
                    int best = INT_MAX;
                    for (std::size_t i = 0; i < provs.size(); ++i)
                    {
                        if (provs[i].body != body || w.province_holder[i] != enemy
                            || province_kind_of(w, provs[i]) != province_kind::land)
                            continue;
                        const entity_id a = province_anchor_tile(w, provs[i]);
                        const int d = mine_anchor != null_entity ? tile_hex_steps(w, mine_anchor, a) : 0;
                        if (d < best)
                        {
                            best = d;
                            f.target_tile = a;
                        }
                    }
                }
            }
            out.push_back(f);
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Player pins
// ---------------------------------------------------------------------------

bool is_pinned(const ui_state& ui, entity_id id)
{
    return id != null_entity
        && std::find(ui.pinned.pins.begin(), ui.pinned.pins.end(), id) != ui.pinned.pins.end();
}

void toggle_pin(ui_state& ui, entity_id id)
{
    if (id == null_entity)
        return;
    auto& p = ui.pinned.pins;
    const auto it = std::find(p.begin(), p.end(), id);
    if (it != p.end())
        p.erase(it);
    else
        p.push_back(id);
}

entity_id pin_subject(const world& w, const ui_state& ui)
{
    if (ui.has_front_selection() || ui.has_battle_selection() || ui.has_deposit_selection()
        || ui.has_plate_selection() || ui.selected_convoy != 0)
        return null_entity;
    const entity_id id = ui.selected_entity;
    switch (selection_kind_of(w, id))
    {
        case selection_kind::body:
        case selection_kind::tile:
        case selection_kind::building:
        case selection_kind::market:
        case selection_kind::unit:
        case selection_kind::nation:
        case selection_kind::wing:
            return id;
        default:
            return null_entity;
    }
}

void navigate_to_pin(const world& w, ui_state& ui, entity_id id)
{
    if (w.tiles.count(id))
    {
        focus_on_tile(w, ui, id);
        return;
    }
    if (const auto n = w.nations.find(id); n != w.nations.end())
    {
        if (n->second.capital_tile != null_entity)
            focus_on_tile(w, ui, n->second.capital_tile);
        return;
    }
    focus_on_entity(w, ui, id);
}

void navigate_to_front(const world& w, ui_state& ui, const front_pin& f)
{
    if (f.target_tile != null_entity && w.tiles.count(f.target_tile))
        focus_on_tile(w, ui, f.target_tile);
    else
        focus_on_surface(w, ui, f.body);
}

// ---------------------------------------------------------------------------
// The panel
// ---------------------------------------------------------------------------

void draw_pinned_panel(const world& w, ui_state& ui, ImVec2 disp)
{
    pinned_items_state& ps = ui.pinned;
    ps.drawn.panel = false;
    ps.drawn.cards.clear();

    // A pin outlives nothing it names: a removed entity drops off the list.
    ps.pins.erase(std::remove_if(ps.pins.begin(), ps.pins.end(),
                                 [&](entity_id id) {
                                     const selection_kind k = selection_kind_of(w, id);
                                     return k == selection_kind::none || k == selection_kind::corporation;
                                 }),
                  ps.pins.end());

    const std::vector<front_pin> fronts = derive_front_pins(w);
    if (ps.pins.empty() && fronts.empty())
        return;

    shell_rect r = pinned_panel_rect(disp, ui.time_panel_h);
    shell_rect lens{};
    if (lens_chrome_this_frame(lens))
        r.h = std::min(r.h, lens.y - shell_margin - r.y);
    if (r.h < ImGui::GetFrameHeight() * 2.0f)
        return; // a lens key has taken the whole residual

    ImGui::SetNextWindowPos({ r.x, r.y }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ r.w, r.h }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
                                     | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings
                                     | ImGuiWindowFlags_NoFocusOnAppearing
                                     | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 8.0f });
    if (ImGui::Begin("##pinned_items", nullptr, flags))
    {
        ps.drawn.panel = true;
        ps.drawn.x = r.x; ps.drawn.y = r.y; ps.drawn.w = r.w; ps.drawn.h = r.h;
        ImGui::TextDisabled("Pinned");
        ImGui::Separator();

        const float inner_w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // --- Front pins: placed by the war, above the player's own ---
        for (std::size_t i = 0; i < fronts.size(); ++i)
        {
            const front_pin& f = fronts[i];
            ImGui::PushID(static_cast<int>(i) + 100000);
            const card_frame cf = begin_card(inner_w);
            const ImVec2 gp = ImGui::GetCursorScreenPos();
            const float  lh = ImGui::GetTextLineHeight();
            icons::battle(dl, { gp.x + k_glyph_r, gp.y + lh * 0.5f }, k_glyph_r, palette::front_hostile);
            ImGui::Dummy({ k_glyph_r * 2.0f + 4.0f, lh });
            ImGui::SameLine();
            const auto b = w.bodies.find(f.body);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::front_hostile), "Front");
            ImGui::SameLine();
            ImGui::TextUnformatted(b != w.bodies.end() ? b->second.name.c_str() : "?");
            ImGui::Text("vs %s", nation_name(w, f.enemy).c_str());
            ImGui::TextDisabled("You hold %d  \xc2\xb7  they hold %d", f.mine_held, f.theirs_held);
            if (f.mine_lost > 0 || f.theirs_taken > 0)
                ImGui::Text("Occupied: %d of yours  \xc2\xb7  %d of theirs", f.mine_lost, f.theirs_taken);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                                   f.war_score > 0 ? palette::positive
                                   : f.war_score < 0 ? palette::negative
                                                     : palette::selection),
                               "War score %+d", f.war_score);
            pinned_items_state::card c;
            c.front = true;
            c.id    = f.body;
            c.enemy = f.enemy;
            const int press = end_card(cf, palette::front_hostile, c);
            ps.drawn.cards.push_back(c);
            if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(
                    { c.x - c.w * 0.5f, c.y - c.h * 0.5f }, { c.x + c.w * 0.5f, c.y + c.h * 0.5f }))
                ImGui::SetTooltip("A front of your war with %s. Placed by the war.\nClick: select the front. Double-click: go to it.",
                                  nation_name(w, f.enemy).c_str());
            if (press != 0)
            {
                // A single click selects what the front pin names: the player's
                // front toward the enemy on this body (the front card), else the
                // enemy nation. A double click selects it too, then navigates.
                ui.clear_battle_selection();
                ui.clear_lens_region_selection();
                ui.selected_convoy   = 0;
                ui.selected_province = 0;
                if (f.front_key != null_entity)
                {
                    ui.selected_entity       = null_entity;
                    ui.selected_front_nation = player_nation(w);
                    ui.selected_front_key    = f.front_key;
                }
                else
                {
                    ui.clear_front_selection();
                    ui.selected_entity = f.enemy;
                }
            }
            if (press == 2)
                navigate_to_front(w, ui, f);
            ImGui::PopID();
        }

        // --- The player's pins, in pin order ---
        entity_id select = null_entity, go = null_entity;
        for (std::size_t i = 0; i < ps.pins.size(); ++i)
        {
            const entity_id id = ps.pins[i];
            const selection_kind k = selection_kind_of(w, id);
            ImGui::PushID(static_cast<int>(i));
            const card_frame cf = begin_card(inner_w);
            const ImVec2 gp = ImGui::GetCursorScreenPos();
            const float  lh = ImGui::GetTextLineHeight();
            kind_glyph(w, dl, k, id, { gp.x + k_glyph_r, gp.y + lh * 0.5f }, k_glyph_r);
            ImGui::Dummy({ k_glyph_r * 2.0f + 4.0f, lh });
            ImGui::SameLine();
            ImGui::TextDisabled("%s", kind_word(k));
            ImGui::SameLine(inner_w - k_card_pad * 2.0f - k_glyph_r * 2.0f);
            {
                const ImVec2 pp = ImGui::GetCursorScreenPos();
                icons::pin(dl, { pp.x + k_glyph_r, pp.y + lh * 0.5f }, k_glyph_r * 0.8f, palette::pinned);
                ImGui::Dummy({ k_glyph_r * 2.0f, lh });
            }
            const float max_h = lh * k_card_max_lines + ImGui::GetStyle().ItemSpacing.y * k_card_max_lines;
            ImGui::SetNextWindowSizeConstraints({ 0.0f, 0.0f }, { inner_w - k_card_pad * 2.0f, max_h });
            if (ImGui::BeginChild("##stat", { inner_w - k_card_pad * 2.0f, 0.0f },
                                  ImGuiChildFlags_AutoResizeY,
                                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                                      | ImGuiWindowFlags_NoBackground))
            {
                switch (k)
                {
                    case selection_kind::body:     draw_body_summary(w, ui, id); break;
                    case selection_kind::tile:     draw_tile_summary(w, id);     break;
                    case selection_kind::building: draw_building_summary(w, id); break;
                    case selection_kind::market:   draw_market_summary(w, id);   break;
                    case selection_kind::unit:     draw_unit_summary(w, id);     break;
                    case selection_kind::nation:   draw_nation_summary(w, id);   break;
                    default:                       ImGui::TextDisabled("\xe2\x80\x94"); break;
                }
            }
            ImGui::EndChild();
            pinned_items_state::card c;
            c.id = id;
            const ImU32 edge = id == ui.selected_entity ? palette::selection : IM_COL32(80, 84, 100, 255);
            const int press2 = end_card(cf, edge, c);
            ps.drawn.cards.push_back(c);
            if (press2 == 1)
                select = id;
            else if (press2 == 2)
                go = id;
            ImGui::PopID();
        }
        // A press on a pin selects it as a canvas press would: the band then has
        // one thing to draw (no battle, front, deposit, plate or convoy beside it).
        const entity_id pick = go != null_entity ? go : select;
        if (pick != null_entity)
        {
            ui.clear_battle_selection();
            ui.clear_front_selection();
            ui.clear_lens_region_selection();
            ui.selected_convoy   = 0;
            ui.selected_entity   = pick;
            ui.selected_province = w.tiles.count(pick) ? w.provinces.province_of(pick) : 0u;
        }
        if (go != null_entity)
            navigate_to_pin(w, ui, go);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void draw_pin_toggle(const world& w, ui_state& ui, ImVec2 band_origin, ImVec2 band_size)
{
    ui.pinned.drawn.toggle_x = ui.pinned.drawn.toggle_y = -1.0f;
    ui.pinned.drawn.toggle_on = false;
    ui.pinned.drawn.toggle_subject = null_entity;
    // A layout with no header row (the tile element) names its own row end this
    // frame; read and clear it whether or not the toggle draws.
    const float ax = ui.pinned.header_anchor_x, ay = ui.pinned.header_anchor_y;
    ui.pinned.header_anchor_x = ui.pinned.header_anchor_y = -1.0f;
    const entity_id subject = pin_subject(w, ui);
    if (subject == null_entity)
        return;

    // On the header row (the band's 10 px padding down), immediately LEFT of the
    // header's 'go to' button, which every entity layout with a header ends its row
    // with at the band's right edge: one frame-height square plus the band padding
    // and the scrollbar gutter the content leaves. The two never overlap.
    const float  size  = ImGui::GetFrameHeight();
    const float  pad   = 10.0f; // selection_card.cpp kPad
    const float  go_w  = size + ImGui::GetStyle().ScrollbarSize; // the go-to square + gutter
    ImVec2 pos = { band_origin.x + band_size.x - pad - go_w - 4.0f - size, band_origin.y + pad };
    if (ax >= 0.0f)
        pos = { ax - size, ay - 2.0f };
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ size, size }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                                     | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
                                     | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings
                                     | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing
                                     | ImGuiWindowFlags_NoBackground;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    if (ImGui::Begin("##pin_toggle", nullptr, flags))
    {
        const bool on = is_pinned(ui, subject);
        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const bool pressed = ImGui::InvisibleButton("##pin", { size, size });
        const bool hov = ImGui::IsItemHovered();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p1 = { p0.x + size, p0.y + size };
        dl->AddRectFilled(p0, p1, on ? IM_COL32(70, 58, 30, 255) : (hov ? IM_COL32(52, 56, 72, 255)
                                                                         : IM_COL32(34, 36, 48, 255)), 4.0f);
        dl->AddRect(p0, p1, on ? palette::pinned : IM_COL32(90, 95, 110, 255), 4.0f);
        icons::pin(dl, { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f }, size * 0.34f,
                   on ? palette::pinned : IM_COL32(170, 175, 190, 255));
        if (hov)
            ImGui::SetTooltip(on ? "Unpin: take this off the watch list."
                                 : "Pin: keep this on the watch list, right of the canvas.");
        if (pressed)
            toggle_pin(ui, subject);
        ui.pinned.drawn.toggle_x = (p0.x + p1.x) * 0.5f;
        ui.pinned.drawn.toggle_y = (p0.y + p1.y) * 0.5f;
        ui.pinned.drawn.toggle_on = is_pinned(ui, subject);
        ui.pinned.drawn.toggle_subject = subject;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace ui
