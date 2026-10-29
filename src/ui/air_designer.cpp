#include "air_designer.hpp"

#include "detail_level.hpp"
#include "icons.hpp"
#include "presentation.hpp"
#include "wing_card.hpp" // air_class_label

#include "world/air_combat.hpp"
#include "world/digitisation_tree_data.hpp" // io::dg::find_node — a locked item's node, named
#include "world/unit_designs.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace ui {

namespace {

/// The design's numbers: the class's own (Lua, `air_params`) plus its kit's deltas.
air_combat_stats air_numbers(const recipe_registry& reg, air_class c, const unit_design& d)
{
    const air_class_stats& cs = reg.military().air.cls[static_cast<std::size_t>(c)];
    const air_combat_stats k  = air_kit_delta(c, d);
    air_combat_stats out;
    out.hp        = cs.hp + k.hp;
    out.air       = cs.air + k.air;
    out.soft      = cs.soft + k.soft;
    out.hard      = cs.hard + k.hard;
    out.strategic = cs.strategic + k.strategic;
    out.recon     = cs.recon + k.recon;
    out.range     = cs.range + k.range;
    return out;
}

std::string delta_text(const air_combat_stats& d)
{
    std::string out;
    char        buf[48];
    auto add = [&](int v, const char* name) {
        if (v == 0) return;
        std::snprintf(buf, sizeof buf, "%+d %s", v, name);
        if (!out.empty()) out += ", ";
        out += buf;
    };
    add(d.hp, "HP");
    add(d.air, "air attack");
    add(d.soft, "soft attack");
    add(d.hard, "hard attack");
    add(d.strategic, "strategic");
    add(d.recon, "recon");
    add(d.range, "range");
    return out.empty() ? std::string("No change") : out;
}

air_combat_stats minus(const air_combat_stats& a, const air_combat_stats& b)
{
    air_combat_stats d;
    d.hp = a.hp - b.hp;  d.air = a.air - b.air;  d.soft = a.soft - b.soft;  d.hard = a.hard - b.hard;
    d.strategic = a.strategic - b.strategic;  d.recon = a.recon - b.recon;  d.range = a.range - b.range;
    return d;
}

std::string item_goods_text(const air_kit_item& it)
{
    if (it.goods_count == 0)
        return "no goods";
    std::string out;
    char        buf[64];
    for (int g = 0; g < it.goods_count; ++g)
    {
        std::snprintf(buf, sizeof buf, "%s%s %g", g ? ", " : "", resource_name(it.goods[g].res),
                      static_cast<double>(it.goods[g].qty));
        out += buf;
    }
    return out;
}

std::string design_goods_text(const std::array<float, resource_count>& goods)
{
    std::string out;
    char        buf[64];
    for (int r = 0; r < resource_count; ++r)
        if (goods[static_cast<std::size_t>(r)] > 0.0f)
        {
            std::snprintf(buf, sizeof buf, "%s%s %g", out.empty() ? "" : ", ",
                          resource_name(static_cast<resource_type>(r)),
                          static_cast<double>(goods[static_cast<std::size_t>(r)]));
            out += buf;
        }
    return out.empty() ? std::string("none (base kit)") : out;
}

std::string node_text(const char* node)
{
    if (node == nullptr)
        return {};
    const int i = io::dg::find_node(node);
    if (i >= 0)
        return io::dg::nodes[static_cast<std::size_t>(i)].name;
    return node;
}

std::string refusal_text(corp_command_result r, const air_kit_item& it)
{
    switch (r)
    {
        case corp_command_result::applied:              return {};
        case corp_command_result::rejected_tech_locked: return "Locked: needs research " + node_text(it.research_node);
        case corp_command_result::rejected_state:       return "Already fitted";
        case corp_command_result::rejected_not_owner:   return "Only your own nation's design can be changed";
        default:                                        return "Does not fit this slot";
    }
}

void clipped_text(ImDrawList* dl, ImVec2 p, float w, ImU32 col, const std::string& text)
{
    const float h = ImGui::GetTextLineHeight();
    dl->PushClipRect(p, {p.x + w, p.y + h}, true);
    dl->AddText(p, col, text.c_str());
    dl->PopClipRect();
}

constexpr ImU32 k_box_bg     = IM_COL32(24, 26, 33, 255);
constexpr ImU32 k_box_border = IM_COL32(90, 94, 106, 255);
constexpr ImU32 k_leader     = IM_COL32(120, 126, 140, 255);

/// Where each slot sits on the aircraft (image-relative): the airframe at the
/// wing root, the armament under the wing, the avionics at the nose.
constexpr float k_anchor_x[k_air_kit_slot_count] = {0.30f, 0.76f, 0.52f};
constexpr float k_anchor_y[k_air_kit_slot_count] = {0.58f, 0.66f, 0.20f};

} // namespace

bool air_designer_open_on(const ui_state& s, air_class c)
{
    return is_expanded(s, detail_surface::air_designer, static_cast<int>(c));
}

void toggle_air_designer(ui_state& s, air_class c)
{
    if (air_designer_open_on(s, c))
        fold(s);
    else
        expand(s, detail_surface::air_designer, static_cast<int>(c));
}

void draw_air_designer(const world& w, const recipe_registry& reg, ui_state& s)
{
    air_designer_state& d = s.air_designer;
    d.boxes.clear();
    d.items.clear();
    d.back_x = d.back_y = -1.0f;
    d.drawn_cls = -1;

    if (s.expanded.surface != detail_surface::air_designer)
    {
        d.slot_open = -1;
        d.open_cls  = -1;
        return;
    }
    const int key = s.expanded.key;
    if (key < 0 || key >= air_class_count)
    {
        fold(s);
        return;
    }
    const air_class cls   = static_cast<air_class>(key);
    const int       slots = k_air_kit_slot_count;
    if (d.open_cls != key)
    {
        d.open_cls  = key;
        d.slot_open = -1;
        d.last_message.clear();
    }

    char title[96];
    std::snprintf(title, sizeof title, "Unit designer \xe2\x80\x94 %s", air_class_label(cls));
    if (!fold_overlay_begin(s, detail_surface::air_designer, key, title))
        return;
    {
        const ImVec2 wp = ImGui::GetWindowPos();
        const float  fh = ImGui::GetFrameHeight();
        d.back_x = wp.x + 36.0f + fh * 0.5f;
        d.back_y = wp.y + 28.0f + fh * 0.5f;
    }
    d.drawn_cls = key;

    const entity_id   nation = player_nation(w);
    const unit_design design = air_design_of(w, nation, cls);
    const auto&       cat    = air_kit_catalogue();
    ImDrawList*       dl     = ImGui::GetWindowDrawList();
    const float       line   = ImGui::GetTextLineHeight();
    const float       pad    = 6.0f;

    int stale = 0;
    for (const auto& [wid, wg] : w.wings)
        if (wg.owner == nation && wing_class(wg) == cls && !(wg.kit == design))
            ++stale;

    // ── Layout, as the land designer's ──
    const ImVec2 o     = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float  summary_h = 3.0f * ImGui::GetTextLineHeightWithSpacing() + 10.0f;
    const float  main_h    = std::max(120.0f, avail.y - summary_h);
    const float  box_w     = std::clamp(avail.x * 0.24f, 180.0f, 300.0f);
    const float  box_h     = 3.0f * line + 2.0f * pad + 4.0f;
    const float  gap       = 28.0f;

    const float img_x0 = o.x + box_w + gap;
    const float img_x1 = o.x + avail.x - box_w - gap;
    const float img_w_room = std::max(40.0f, img_x1 - img_x0);
    const float aw = 640.0f, ah = 400.0f;
    const float scale = std::min(img_w_room / aw, main_h / ah);
    const float iw = aw * scale, ih = ah * scale;
    const ImVec2 i0 = {img_x0 + (img_w_room - iw) * 0.5f, o.y + (main_h - ih) * 0.5f};
    const ImVec2 i1 = {i0.x + iw, i0.y + ih};
    // No air art yet: the aircraft glyph, large, on the placeholder panel.
    dl->AddRect(i0, i1, k_box_border, 4.0f, 0, 1.5f);
    const ImVec2 ic = {(i0.x + i1.x) * 0.5f, (i0.y + i1.y) * 0.5f};
    icons::aircraft(dl, ic, std::min(iw, ih) * 0.42f,
                    palette::building_kind_colour(building_type::airbase));

    struct slot_pos { int slot; ImVec2 anchor; bool right; float top; };
    std::vector<slot_pos> pos;
    for (int sl = 0; sl < slots; ++sl)
        pos.push_back({sl, {i0.x + k_anchor_x[sl] * iw, i0.y + k_anchor_y[sl] * ih},
                       k_anchor_x[sl] >= 0.5f, 0.0f});
    for (int side = 0; side < 2; ++side)
    {
        std::vector<slot_pos*> col;
        for (slot_pos& p : pos)
            if (p.right == (side == 1))
                col.push_back(&p);
        std::sort(col.begin(), col.end(),
                  [](const slot_pos* a, const slot_pos* b) { return a->anchor.y < b->anchor.y; });
        float next = o.y;
        for (slot_pos* p : col)
        {
            p->top = std::max(next, std::clamp(p->anchor.y - box_h * 0.5f, o.y, o.y + main_h - box_h));
            next   = p->top + box_h + 10.0f;
        }
    }

    // ── The boxes ──
    float open_box_top = 0.0f, open_box_x = 0.0f;
    bool  open_box_right = false;
    for (const slot_pos& p : pos)
    {
        const int           sl     = p.slot;
        const uint16_t      fitted = air_design_fitted(w, nation, cls, sl);
        const air_kit_item* fi     = fitted < cat.size() ? &cat[fitted] : nullptr;
        const float  bx = p.right ? o.x + avail.x - box_w : o.x;
        const ImVec2 b0 = {bx, p.top};
        const ImVec2 b1 = {bx + box_w, p.top + box_h};

        ImGui::SetCursorScreenPos(b0);
        ImGui::PushID(sl);
        const bool pressed = ImGui::InvisibleButton("##air_slot_box", {box_w, box_h});
        const bool hot     = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool open = d.slot_open == sl;
        if (pressed)
            d.slot_open = open ? -1 : sl; // the toggle rule
        if (hot && fi != nullptr)
            ImGui::SetTooltip("%s: %s\n%s\nPress to %s the choices.", air_kit_slot_name(sl), fi->name,
                              delta_text(fi->delta).c_str(), open ? "close" : "see");

        const bool  lit  = hot || d.slot_open == sl;
        dl->AddRectFilled(b0, b1, k_box_bg, 4.0f);
        dl->AddRect(b0, b1, lit ? palette::hover : k_box_border, 4.0f, 0, lit ? 2.0f : 1.0f);
        const ImVec2 from = {p.right ? b0.x : b1.x, (b0.y + b1.y) * 0.5f};
        dl->AddLine(from, p.anchor, lit ? palette::hover : k_leader, lit ? 2.0f : 1.2f);
        dl->AddCircleFilled(p.anchor, lit ? 6.0f : 4.0f, lit ? palette::hover : k_leader);
        if (lit)
            dl->AddCircle(p.anchor, 11.0f, palette::hover, 0, 1.5f);

        const float tw = box_w - 2.0f * pad;
        float       ty = b0.y + pad;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::text_secondary, air_kit_slot_name(sl));
        ty += line + 2.0f;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::selection, fi ? fi->name : "\xe2\x80\x94");
        ty += line + 2.0f;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::text_secondary,
                     fi && fi->goods_count > 0 ? delta_text(fi->delta) : "Base kit");

        air_designer_state::drawn_box db;
        db.slot = sl;
        db.name = air_kit_slot_name(sl);
        db.fitted = fi ? fi->name : "";
        db.x = (b0.x + b1.x) * 0.5f;
        db.y = (b0.y + b1.y) * 0.5f;
        db.anchor_x = p.anchor.x;
        db.anchor_y = p.anchor.y;
        d.boxes.push_back(db);
        if (d.slot_open == sl)
        {
            open_box_top   = b0.y;
            open_box_right = p.right;
            open_box_x     = p.right ? b0.x : b1.x;
        }
    }

    // ── The open slot's list ──
    uint16_t preview = k_no_kit_item;
    if (d.slot_open >= 0)
    {
        const int                    sl     = d.slot_open;
        const std::vector<uint16_t>& items  = air_kit_items_for(cls, sl);
        const uint16_t               fitted = air_design_fitted(w, nation, cls, sl);
        const air_combat_stats       now    = air_numbers(reg, cls, design);
        const float row_h3 = 3.0f * line + 8.0f;
        const float list_w = std::min(440.0f, std::max(240.0f, img_x1 - img_x0 + gap));
        const float want_h = ImGui::GetFrameHeightWithSpacing() + static_cast<float>(items.size()) * (row_h3 + line + 4.0f) + 16.0f;
        const float list_h = std::min(want_h, main_h);
        const float lx     = open_box_right ? open_box_x - 12.0f - list_w : open_box_x + 12.0f;
        const float ly     = std::clamp(open_box_top, o.y, o.y + main_h - list_h);

        ImGui::SetCursorScreenPos({lx, ly});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 22, 28, 250));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(palette::hover));
        ImGui::BeginChild("##air_designer_items", {list_w, list_h}, ImGuiChildFlags_Borders);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s \xe2\x80\x94 choose kit",
                           air_kit_slot_name(sl));
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const float rw  = ImGui::GetContentRegionAvail().x;
        for (const uint16_t id : items)
        {
            if (id >= cat.size())
                continue;
            const air_kit_item& it = cat[id];
            const bool is_fit  = id == fitted;
            const corp_command_result why =
                air_design_check(w, nation, static_cast<uint32_t>(key), static_cast<uint16_t>(sl), id);
            const bool locked  = !air_kit_item_researched(w, nation, id);
            const bool enabled = why == corp_command_result::applied;
            const float row_h  = locked ? row_h3 + line : row_h3;

            unit_design probe = design;
            probe.fit[static_cast<std::size_t>(sl)] = air_kit_variant_of(id);
            const air_combat_stats diff = minus(air_numbers(reg, cls, probe), now);

            ImGui::PushID(static_cast<int>(id));
            const ImVec2 r0 = ImGui::GetCursorScreenPos();
            ImGui::BeginDisabled(!enabled);
            const bool picked = ImGui::Selectable("##air_kit_item", is_fit, ImGuiSelectableFlags_None, {rw, row_h});
            ImGui::EndDisabled();
            const bool hot = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
            ImGui::PopID();
            if (picked && enabled)
            {
                d.pending_cls  = key;
                d.pending_slot = sl;
                d.pending_item = id;
                d.last_message.clear();
            }
            if (hot)
            {
                preview = id;
                if (!enabled)
                    ImGui::SetTooltip("%s", refusal_text(why, it).c_str());
            }

            const float tx = r0.x + 6.0f;
            const char* badge = is_fit ? "Fitted" : locked ? "LOCKED" : nullptr;
            const float badge_w = badge ? ImGui::CalcTextSize(badge).x + 10.0f : 0.0f;
            if (badge)
            {
                const ImVec2 bp = {r0.x + rw - badge_w, r0.y + 3.0f};
                const ImU32  bc = is_fit ? palette::positive : palette::negative;
                cdl->AddRect({bp.x - 2.0f, bp.y - 1.0f}, {bp.x + badge_w - 6.0f, bp.y + line + 1.0f}, bc, 3.0f);
                cdl->AddText({bp.x + 2.0f, bp.y}, bc, badge);
            }
            clipped_text(cdl, {tx, r0.y + 3.0f}, rw - badge_w - 14.0f,
                         locked ? palette::text_secondary : palette::selection, it.name);
            clipped_text(cdl, {tx, r0.y + 3.0f + line}, rw - 12.0f, palette::text_secondary,
                         is_fit ? std::string("Fitted now") : delta_text(diff));
            clipped_text(cdl, {tx, r0.y + 3.0f + 2.0f * line}, rw - 12.0f, palette::text_secondary,
                         item_goods_text(it) + " per wing");
            if (locked)
                clipped_text(cdl, {tx, r0.y + 3.0f + 3.0f * line}, rw - 12.0f, palette::negative,
                             "Needs research " + node_text(it.research_node));

            air_designer_state::drawn_item di;
            di.item = id;
            di.name = it.name;
            di.x = r0.x + rw * 0.5f;
            di.y = r0.y + row_h * 0.5f;
            di.fitted = is_fit;
            di.enabled = enabled;
            di.locked = locked;
            di.reason = refusal_text(why, it);
            d.items.push_back(di);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    // ── Summary ──
    ImGui::SetCursorScreenPos({o.x, o.y + main_h + 6.0f});
    ImGui::Separator();
    {
        const air_combat_stats now = air_numbers(reg, cls, design);
        unit_design next = design;
        const bool has_preview = preview != k_no_kit_item && d.slot_open >= 0
                              && preview != air_design_fitted(w, nation, cls, d.slot_open);
        if (has_preview)
            next.fit[static_cast<std::size_t>(d.slot_open)] = air_kit_variant_of(preview);
        const air_combat_stats then = air_numbers(reg, cls, next);

        struct stat_col { const char* name; int a, b; };
        const stat_col cols[] = {
            {"HP", now.hp, then.hp},             {"Air attack", now.air, then.air},
            {"Soft attack", now.soft, then.soft}, {"Hard attack", now.hard, then.hard},
            {"Strategic", now.strategic, then.strategic}, {"Recon", now.recon, then.recon},
            {"Range", now.range, then.range},
        };
        constexpr int n = static_cast<int>(sizeof cols / sizeof cols[0]);
        const float col_w = avail.x / static_cast<float>(n);
        const float x0    = ImGui::GetCursorPosX();
        for (int i = 0; i < n; ++i)
        {
            if (i) ImGui::SameLine(x0 + col_w * static_cast<float>(i));
            const stat_col& c = cols[i];
            if (has_preview && c.a != c.b)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(c.b > c.a ? palette::positive : palette::negative),
                                   "%s %d \xe2\x86\x92 %d", c.name, c.a, c.b);
            else
                ImGui::Text("%s %d", c.name, c.a);
        }
        const std::string goods_now = design_goods_text(air_kit_goods(cls, design));
        if (has_preview)
            ImGui::TextDisabled("Kit goods per wing: %s  \xe2\x86\x92  %s", goods_now.c_str(),
                                design_goods_text(air_kit_goods(cls, next)).c_str());
        else
            ImGui::TextDisabled("Kit goods per wing: %s", goods_now.c_str());
        if (stale > 0)
            ImGui::TextDisabled("%d wing%s flying an older design.", stale, stale == 1 ? "" : "s");
        else
            ImGui::TextDisabled("A pick is the live design: new wings are raised with it.");
        if (!d.last_message.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", d.last_message.c_str());
        }
    }

    fold_overlay_end(s);
}

} // namespace ui
