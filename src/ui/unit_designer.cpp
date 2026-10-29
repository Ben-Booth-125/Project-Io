#include "unit_designer.hpp"

#include "detail_level.hpp"
#include "presentation.hpp"
#include "unit_art.hpp"

#include "world/corp_command.hpp"           // unit_design_check
#include "world/digitisation_tree_data.hpp" // io::dg::find_node — a locked item's node, named
#include "world/unit_designs.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ui {

namespace {

const char* class_label(land_class c)
{
    static const char* const k_names[land_class_count] = {
        "Infantry", "Motorised infantry", "Mechanised infantry", "Armour",
        "Light armour", "Artillery", "Air defence", "Not a land fighter",
    };
    const int i = static_cast<int>(c);
    return (i >= 0 && i < land_class_count) ? k_names[i] : "Unknown";
}

/// The non-zero entries of a stat delta, signed: "+10 soft attack, +15% hardness".
/// "No change" when every entry is zero.
std::string delta_text(const land_combat_stats& d)
{
    std::string out;
    char        buf[48];
    auto add = [&](int v, const char* name, bool pct) {
        if (v == 0) return;
        if (pct) std::snprintf(buf, sizeof buf, "%+d%% %s", v / 10, name);
        else     std::snprintf(buf, sizeof buf, "%+d %s", v, name);
        if (!out.empty()) out += ", ";
        out += buf;
    };
    add(d.soft, "soft attack", false);
    add(d.hard, "hard attack", false);
    add(d.defence, "defence", false);
    add(d.breakthrough, "breakthrough", false);
    add(d.hardness, "hardness", true);
    add(d.recon, "recon", false);     // BL-1453 (MILITARY.md § Recon)
    add(d.stealth, "stealth", false);
    return out.empty() ? std::string("No change") : out;
}

land_combat_stats minus(const land_combat_stats& a, const land_combat_stats& b)
{
    land_combat_stats d;
    d.soft         = a.soft - b.soft;
    d.hard         = a.hard - b.hard;
    d.hardness     = a.hardness - b.hardness;
    d.defence      = a.defence - b.defence;
    d.breakthrough = a.breakthrough - b.breakthrough;
    d.recon        = a.recon - b.recon;
    d.stealth      = a.stealth - b.stealth;
    return d;
}

/// An item's goods per unit: "steel 2, machinery 1"; "no goods" for base kit.
std::string item_goods_text(const kit_item& it)
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

/// A design's goods per unit, from `kit_goods`.
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

/// The age-tree node an item needs, named: the node's name when the tree holds it,
/// else its store id (a node the tree does not hold yet).
std::string node_text(const char* node)
{
    if (node == nullptr)
        return {};
    const int i = io::dg::find_node(node);
    if (i >= 0)
        return io::dg::nodes[static_cast<std::size_t>(i)].name;
    return node;
}

/// The seam's refusal, in the player's words. Empty for `applied`.
std::string refusal_text(corp_command_result r, const kit_item& it)
{
    switch (r)
    {
        case corp_command_result::applied:             return {};
        case corp_command_result::rejected_tech_locked: return "Locked: needs research " + node_text(it.research_node);
        case corp_command_result::rejected_state:      return "Already fitted";
        case corp_command_result::rejected_not_owner:  return "Only your own nation's design can be changed";
        default:                                       return "Does not fit this slot";
    }
}

/// Draw @p text clipped to one line of width @p w at @p p.
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

} // namespace

bool unit_designer_open_on(const ui_state& s, land_class c)
{
    return is_expanded(s, detail_surface::unit_designer, static_cast<int>(c));
}

void toggle_unit_designer(ui_state& s, land_class c)
{
    if (unit_designer_open_on(s, c))
        fold(s);
    else if (kit_slot_count(c) > 0)
        expand(s, detail_surface::unit_designer, static_cast<int>(c));
}

void draw_unit_designer(const world& w, ui_state& s)
{
    unit_designer_state& d = s.designer;
    d.boxes.clear();
    d.items.clear();
    d.back_x = d.back_y = -1.0f;
    d.stale_units = 0;
    d.drawn_cls   = -1;

    if (s.expanded.surface != detail_surface::unit_designer)
    {
        d.slot_open = -1;
        d.open_cls  = -1;
        return;
    }
    const int key = s.expanded.key;
    if (key < 0 || key >= land_class_count || kit_slot_count(static_cast<land_class>(key)) == 0)
    {
        fold(s);
        return;
    }
    const land_class cls   = static_cast<land_class>(key);
    const int        slots = kit_slot_count(cls);
    if (d.open_cls != key)
    {
        d.open_cls  = key;
        d.slot_open = -1;
        d.last_message.clear();
    }
    if (d.slot_open >= slots)
        d.slot_open = -1;

    char title[96];
    std::snprintf(title, sizeof title, "Unit designer \xe2\x80\x94 %s", class_label(cls));
    if (!fold_overlay_begin(s, detail_surface::unit_designer, key, title))
        return;
    {
        // The return control `‹` fold_overlay_begin drew: the window's top-left
        // content corner (its padding is 36 x 28), one frame-height square.
        const ImVec2 wp = ImGui::GetWindowPos();
        const float  fh = ImGui::GetFrameHeight();
        d.back_x = wp.x + 36.0f + fh * 0.5f;
        d.back_y = wp.y + 28.0f + fh * 0.5f;
    }
    d.drawn_cls = key;

    const entity_id    nation = player_nation(w);
    const unit_design  design = design_of(w, nation, cls);
    const auto&        cat    = kit_catalogue();
    const unit_art&    art    = unit_art_for(cls);
    ImDrawList*        dl     = ImGui::GetWindowDrawList();
    const float        line   = ImGui::GetTextLineHeight();
    const float        pad    = 6.0f;

    for (const auto& [uid, u] : w.units)
        if (u.owner == nation && unit_land_class(u) == cls && unit_design_stale(w, u))
            ++d.stale_units;

    // ── Layout: image in the middle, slot boxes in columns either side, the
    //    summary strip underneath. ─────────────────────────────────────────────
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
    const float aw = art.width  > 0 ? static_cast<float>(art.width)  : 640.0f;
    const float ah = art.height > 0 ? static_cast<float>(art.height) : 400.0f;
    const float scale = std::min(img_w_room / aw, main_h / ah);
    const float iw = aw * scale, ih = ah * scale;
    const ImVec2 i0 = {img_x0 + (img_w_room - iw) * 0.5f, o.y + (main_h - ih) * 0.5f};
    const ImVec2 i1 = {i0.x + iw, i0.y + ih};
    if (art.tex != nullptr)
        dl->AddImage(reinterpret_cast<ImTextureID>(art.tex), i0, i1);
    else
    {
        dl->AddRect(i0, i1, k_box_border, 4.0f, 0, 1.5f);
        const char* msg = "No image for this class";
        const ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText({(i0.x + i1.x - ts.x) * 0.5f, (i0.y + i1.y - ts.y) * 0.5f},
                    palette::text_secondary, msg);
    }

    // Anchors on screen, and which side each slot's box takes: the side its
    // anchor is nearer, so leader lines do not cross the image.
    struct slot_pos { int slot; ImVec2 anchor; bool right; float top; };
    std::vector<slot_pos> pos;
    for (int sl = 0; sl < slots; ++sl)
    {
        const ImVec2 a = {i0.x + art.anchor_x[sl] * iw, i0.y + art.anchor_y[sl] * ih};
        pos.push_back({sl, a, art.anchor_x[sl] >= 0.5f, 0.0f});
    }
    for (int side = 0; side < 2; ++side)
    {
        std::vector<slot_pos*> col;
        for (slot_pos& p : pos)
            if (p.right == (side == 1))
                col.push_back(&p);
        std::sort(col.begin(), col.end(),
                  [](const slot_pos* a, const slot_pos* b) { return a->anchor.y < b->anchor.y; });
        // Aim each box at its anchor's height, then push down to clear the one
        // above, then pull the column back up if it ran off the bottom.
        float next = o.y;
        for (slot_pos* p : col)
        {
            p->top = std::max(next, std::clamp(p->anchor.y - box_h * 0.5f, o.y, o.y + main_h - box_h));
            next   = p->top + box_h + 10.0f;
        }
        float over = (col.empty() ? 0.0f : col.back()->top + box_h) - (o.y + main_h);
        for (auto it = col.rbegin(); it != col.rend() && over > 0.0f; ++it)
        {
            (*it)->top -= over;
            if (it + 1 != col.rend())
                over = ((*(it + 1))->top + box_h + 10.0f) - (*it)->top;
            else
                over = 0.0f;
        }
    }

    // ── The boxes ───────────────────────────────────────────────────────────
    float open_box_top = 0.0f;
    bool  open_box_right = false;
    float open_box_x = 0.0f;
    for (const slot_pos& p : pos)
    {
        const int      sl     = p.slot;
        const uint16_t fitted = design_fitted(w, nation, cls, sl);
        const kit_item* fi    = fitted < cat.size() ? &cat[fitted] : nullptr;
        const float    bx     = p.right ? o.x + avail.x - box_w : o.x;
        const ImVec2   b0     = {bx, p.top};
        const ImVec2   b1     = {bx + box_w, p.top + box_h};

        ImGui::SetCursorScreenPos(b0);
        ImGui::PushID(sl);
        const bool pressed = ImGui::InvisibleButton("##slot_box", {box_w, box_h});
        const bool hot     = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool open = d.slot_open == sl;
        if (pressed)
            d.slot_open = open ? -1 : sl; // the toggle rule: the press that opened it closes it
        if (hot && fi != nullptr)
            ImGui::SetTooltip("%s: %s\n%s\nPress to %s the choices.", kit_slot_name(cls, sl), fi->name,
                              delta_text(fi->delta).c_str(), open ? "close" : "see");

        const bool  lit  = hot || d.slot_open == sl;
        const ImU32 edge = lit ? palette::hover : k_box_border;
        dl->AddRectFilled(b0, b1, k_box_bg, 4.0f);
        dl->AddRect(b0, b1, edge, 4.0f, 0, lit ? 2.0f : 1.0f);

        // Leader line from the box's inner edge to its anchor.
        const ImVec2 from = {p.right ? b0.x : b1.x, (b0.y + b1.y) * 0.5f};
        dl->AddLine(from, p.anchor, lit ? palette::hover : k_leader, lit ? 2.0f : 1.2f);
        dl->AddCircleFilled(p.anchor, lit ? 6.0f : 4.0f, lit ? palette::hover : k_leader);
        if (lit)
            dl->AddCircle(p.anchor, 11.0f, palette::hover, 0, 1.5f);

        const float tw = box_w - 2.0f * pad;
        float       ty = b0.y + pad;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::text_secondary, kit_slot_name(cls, sl));
        ty += line + 2.0f;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::selection, fi ? fi->name : "\xe2\x80\x94");
        ty += line + 2.0f;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::text_secondary,
                     fi && fi->goods_count > 0 ? delta_text(fi->delta) : "Base kit");

        unit_designer_state::drawn_box db;
        db.slot = sl;
        db.name = kit_slot_name(cls, sl);
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

    // ── The open slot's item list, beside its box, over the image ──────────
    uint16_t preview = k_no_kit_item;
    if (d.slot_open >= 0)
    {
        const int                    sl     = d.slot_open;
        const std::vector<uint16_t>& items  = kit_items_for(cls, sl);
        const uint16_t               fitted = design_fitted(w, nation, cls, sl);
        const land_combat_stats      now    = kit_stats(cls, design);
        const float row_h3 = 3.0f * line + 8.0f; // a locked row takes a fourth line: the node that opens it
        const float list_w = std::min(440.0f, std::max(240.0f, img_x1 - img_x0 + gap));
        const float want_h = ImGui::GetFrameHeightWithSpacing() + static_cast<float>(items.size()) * (row_h3 + line + 4.0f) + 16.0f;
        const float list_h = std::min(want_h, main_h);
        const float lx     = open_box_right ? open_box_x - 12.0f - list_w : open_box_x + 12.0f;
        const float ly     = std::clamp(open_box_top, o.y, o.y + main_h - list_h);

        ImGui::SetCursorScreenPos({lx, ly});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 22, 28, 250));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(palette::hover));
        ImGui::BeginChild("##designer_items", {list_w, list_h}, ImGuiChildFlags_Borders);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s \xe2\x80\x94 choose kit",
                           kit_slot_name(cls, sl));
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const float rw  = ImGui::GetContentRegionAvail().x;
        for (const uint16_t id : items)
        {
            if (id >= cat.size())
                continue;
            const kit_item& it  = cat[id];
            const bool is_fit   = id == fitted;
            const corp_command_result why =
                unit_design_check(w, nation, static_cast<uint32_t>(key), static_cast<uint16_t>(sl), id);
            const bool locked  = !kit_item_researched(w, nation, id);
            const bool enabled = why == corp_command_result::applied;
            const float row_h  = locked ? row_h3 + line : row_h3;

            // The item's stats against the FITTED item: what this pick would change.
            unit_design probe = design;
            probe.fit[static_cast<std::size_t>(sl)] = kit_variant_of(id);
            const land_combat_stats diff = minus(kit_stats(cls, probe), now);

            ImGui::PushID(static_cast<int>(id));
            const ImVec2 r0 = ImGui::GetCursorScreenPos();
            ImGui::BeginDisabled(!enabled);
            const bool picked = ImGui::Selectable("##kit_item", is_fit, ImGuiSelectableFlags_None, {rw, row_h});
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

            // Line 1: name, then the badge — Fitted or LOCKED — at the right.
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
            // Line 2: what it changes against the fitted item.
            clipped_text(cdl, {tx, r0.y + 3.0f + line}, rw - 12.0f, palette::text_secondary,
                         is_fit ? std::string("Fitted now") : delta_text(diff));
            // Line 3: goods per unit. Line 4, when locked: the node that opens it.
            clipped_text(cdl, {tx, r0.y + 3.0f + 2.0f * line}, rw - 12.0f, palette::text_secondary,
                         item_goods_text(it) + " per unit");
            if (locked)
                clipped_text(cdl, {tx, r0.y + 3.0f + 3.0f * line}, rw - 12.0f, palette::negative,
                             "Needs research " + node_text(it.research_node));

            unit_designer_state::drawn_item di;
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

    // ── Summary: the design's stats and kit goods, previewing a hovered pick ──
    ImGui::SetCursorScreenPos({o.x, o.y + main_h + 6.0f});
    ImGui::Separator();
    {
        const land_combat_stats now = kit_stats(cls, design);
        unit_design next = design;
        const bool has_preview = preview != k_no_kit_item && d.slot_open >= 0
                              && preview != design_fitted(w, nation, cls, d.slot_open);
        if (has_preview)
            next.fit[static_cast<std::size_t>(d.slot_open)] = kit_variant_of(preview);
        const land_combat_stats then = kit_stats(cls, next);

        struct stat_col { const char* name; int a, b; bool pct; };
        const stat_col cols[] = {
            {"Soft attack", now.soft, then.soft, false},
            {"Hard attack", now.hard, then.hard, false},
            {"Defence", now.defence, then.defence, false},
            {"Breakthrough", now.breakthrough, then.breakthrough, false},
            {"Hardness", now.hardness, then.hardness, true},
        };
        const float col_w = avail.x / 5.0f;
        const float x0    = ImGui::GetCursorPosX();
        for (int i = 0; i < 5; ++i)
        {
            if (i) ImGui::SameLine(x0 + col_w * static_cast<float>(i));
            const stat_col& c = cols[i];
            const int a = c.pct ? (c.a + 5) / 10 : c.a;
            const int b = c.pct ? (c.b + 5) / 10 : c.b;
            if (has_preview && c.a != c.b)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(c.b > c.a ? palette::positive : palette::negative),
                                   "%s %d%s \xe2\x86\x92 %d%s", c.name, a, c.pct ? "%" : "", b, c.pct ? "%" : "");
            else
                ImGui::Text("%s %d%s", c.name, a, c.pct ? "%" : "");
        }
        const std::string goods_now = design_goods_text(kit_goods(cls, design));
        if (has_preview)
            ImGui::TextDisabled("Kit goods per unit: %s  \xe2\x86\x92  %s", goods_now.c_str(),
                                design_goods_text(kit_goods(cls, next)).c_str());
        else
            ImGui::TextDisabled("Kit goods per unit: %s", goods_now.c_str());

        // The save model, said once: a pick IS the live design.
        char stale[160];
        if (d.stale_units > 0)
            std::snprintf(stale, sizeof stale, "%d unit%s in the field on an older design \xe2\x80\x94 refit from the unit card.",
                          d.stale_units, d.stale_units == 1 ? "" : "s");
        else
            std::snprintf(stale, sizeof stale, "A pick is the live design: new units are raised with it, units in the field take it by refit.");
        ImGui::TextDisabled("%s", stale);
        if (!d.last_message.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", d.last_message.c_str());
        }
    }

    fold_overlay_end(s);
}

} // namespace ui
