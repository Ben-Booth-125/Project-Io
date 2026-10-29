#include "unit_designer.hpp"

#include "detail_level.hpp"
#include "presentation.hpp"
#include "unit_art.hpp"

#include "world/corp_command.hpp"           // unit_design_check
#include "world/digitisation_tree_data.hpp" // io::dg::find_node — a locked item's node, named
#include "world/unit_designs.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
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

static_assert(std::tuple_size<decltype(unit_designer_state::draft)>::value == k_kit_slot_max,
              "the designer's draft holds one item per kit slot");

unit_design unit_designer_draft_design(const world& w, const ui_state& s, entity_id nation, land_class c)
{
    unit_design out = design_of(w, nation, c);
    if (s.designer.draft_cls != static_cast<int>(c))
        return out;
    const int slots = kit_slot_count(c);
    for (int sl = 0; sl < slots && sl < static_cast<int>(k_kit_slot_max); ++sl)
    {
        const uint16_t it = s.designer.draft[static_cast<std::size_t>(sl)];
        if (it != k_no_kit_item)
            out.fit[static_cast<std::size_t>(sl)] = kit_variant_of(it);
    }
    return out;
}

land_combat_stats unit_designer_draft_stats(const world& w, const ui_state& s, entity_id nation, land_class c)
{
    return kit_stats(c, unit_designer_draft_design(w, s, nation, c));
}

int unit_designer_changed_slots(const world& w, const ui_state& s, entity_id nation)
{
    const int key = s.designer.draft_cls;
    if (key < 0 || key >= land_class_count)
        return 0;
    const land_class c = static_cast<land_class>(key);
    int n = 0;
    for (int sl = 0; sl < kit_slot_count(c) && sl < static_cast<int>(k_kit_slot_max); ++sl)
    {
        const uint16_t it = s.designer.draft[static_cast<std::size_t>(sl)];
        if (it != k_no_kit_item && it != design_fitted(w, nation, c, sl))
            ++n;
    }
    return n;
}

namespace {

void clear_draft(unit_designer_state& d)
{
    d.draft_cls = -1;
    d.draft.fill(k_no_kit_item);
}

/// Save: queue one pick per changed slot for app::render, then drop the draft.
void save_draft(const world& w, unit_designer_state& d, entity_id nation)
{
    const int key = d.draft_cls;
    if (key >= 0 && key < land_class_count)
    {
        const land_class c = static_cast<land_class>(key);
        for (int sl = 0; sl < kit_slot_count(c) && sl < static_cast<int>(k_kit_slot_max); ++sl)
        {
            const uint16_t it = d.draft[static_cast<std::size_t>(sl)];
            if (it != k_no_kit_item && it != design_fitted(w, nation, c, sl))
                d.pending.push_back({key, sl, it});
        }
    }
    clear_draft(d);
}

/// Answer the close prompt: carry the interrupted close to where it was going.
void finish_close(ui_state& s)
{
    unit_designer_state& d = s.designer;
    d.confirm_close = false;
    if (d.after_surface == detail_surface::none)
        fold(s);
    else
        expand(s, d.after_surface, d.after_key);
}

} // namespace

void draw_unit_designer(const world& w, ui_state& s)
{
    unit_designer_state& d = s.designer;
    d.boxes.clear();
    d.items.clear();
    d.back_x = d.back_y = -1.0f;
    d.save_x = d.save_y = d.revert_x = d.revert_y = -1.0f;
    d.prompt_save_x = d.prompt_save_y = d.prompt_discard_x = d.prompt_discard_y = -1.0f;
    d.prompt_keep_x = d.prompt_keep_y = -1.0f;
    d.changed_slots = 0;
    d.stale_units = 0;
    d.drawn_cls   = -1;

    const entity_id nation = player_nation(w);

    // THE CLOSE CATCH. Every close path — the toggle press (toggle_unit_designer),
    // the return control (fold_overlay_begin), Escape (app.cpp's rung), a press
    // opening another takeover — ends in `fold` or `expand` on `ui_state::expanded`,
    // and all of them run before this draw. So one check here covers them all
    // without each knowing about drafts: if the draft has unsaved changes and the
    // takeover is no longer the draft's class, put the designer back, remember
    // where the close was going, and ask.
    if (d.draft_cls >= 0 && !is_expanded(s, detail_surface::unit_designer, d.draft_cls))
    {
        if (unit_designer_changed_slots(w, s, nation) > 0)
        {
            d.after_surface = s.expanded.surface;
            d.after_key     = s.expanded.key;
            d.confirm_close = true;
            s.expanded.surface = detail_surface::unit_designer;
            s.expanded.key     = d.draft_cls;
            s.expanded.focus   = false;
        }
        else
        {
            clear_draft(d);
            d.confirm_close = false;
        }
    }

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
    if (d.draft_cls != key)
    {
        clear_draft(d); // the catch above has already asked about a changed one
        d.draft_cls = key;
    }
    d.changed_slots = unit_designer_changed_slots(w, s, nation);
    if (d.changed_slots == 0)
        d.confirm_close = false;
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

    const unit_design  design = unit_designer_draft_design(w, s, nation, cls); // what is shown
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
    // Three summary lines and the Save / Revert row under them.
    const float  summary_h = 3.0f * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + 10.0f;
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
        const uint16_t live_it = design_fitted(w, nation, cls, sl);
        const uint16_t fitted  = kit_item_at(cls, sl, design.fit[static_cast<std::size_t>(sl)]);
        const bool     changed = fitted != live_it;
        const kit_item* fi    = fitted < cat.size() ? &cat[fitted] : nullptr;
        const float    bx     = p.right ? o.x + avail.x - box_w : o.x;
        const ImVec2   b0     = {bx, p.top};
        const ImVec2   b1     = {bx + box_w, p.top + box_h};

        ImGui::SetCursorScreenPos(b0);
        ImGui::PushID(sl);
        ImGui::BeginDisabled(d.confirm_close);
        const bool pressed = ImGui::InvisibleButton("##slot_box", {box_w, box_h});
        ImGui::EndDisabled();
        const bool hot     = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool open = d.slot_open == sl;
        if (pressed)
            d.slot_open = open ? -1 : sl; // the toggle rule: the press that opened it closes it
        if (hot && fi != nullptr && !d.confirm_close)
        {
            if (changed)
                ImGui::SetTooltip("%s: %s (changed, not saved; the live design has %s)\n%s\nPress to %s the choices.",
                                  kit_slot_name(cls, sl), fi->name,
                                  live_it < cat.size() ? cat[live_it].name : "\xe2\x80\x94",
                                  delta_text(fi->delta).c_str(), open ? "close" : "see");
            else
                ImGui::SetTooltip("%s: %s\n%s\nPress to %s the choices.", kit_slot_name(cls, sl), fi->name,
                                  delta_text(fi->delta).c_str(), open ? "close" : "see");
        }

        const bool  lit  = hot || d.slot_open == sl;
        const ImU32 edge = lit ? palette::hover : changed ? palette::pinned : k_box_border;
        dl->AddRectFilled(b0, b1, k_box_bg, 4.0f);
        dl->AddRect(b0, b1, edge, 4.0f, 0, lit || changed ? 2.0f : 1.0f);

        // Leader line from the box's inner edge to its anchor.
        const ImVec2 from = {p.right ? b0.x : b1.x, (b0.y + b1.y) * 0.5f};
        dl->AddLine(from, p.anchor, lit ? palette::hover : k_leader, lit ? 2.0f : 1.2f);
        dl->AddCircleFilled(p.anchor, lit ? 6.0f : 4.0f, lit ? palette::hover : k_leader);
        if (lit)
            dl->AddCircle(p.anchor, 11.0f, palette::hover, 0, 1.5f);

        const float tw = box_w - 2.0f * pad;
        float       ty = b0.y + pad;
        if (changed)
        {
            // The changed mark: this slot's draft is not the live design yet.
            const char*  mark = "Changed";
            const ImVec2 ms   = ImGui::CalcTextSize(mark);
            dl->AddText({b1.x - pad - ms.x, ty}, palette::pinned, mark);
            clipped_text(dl, {b0.x + pad, ty}, tw - ms.x - 6.0f, palette::text_secondary, kit_slot_name(cls, sl));
        }
        else
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
        db.live = live_it < cat.size() ? cat[live_it].name : "";
        db.changed = changed;
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
        // `fitted` is the DRAFT's item (what the box shows: the one-of-n selector's
        // choice); `live_it` is the live design's, which a pick can return to.
        const uint16_t               fitted  = kit_item_at(cls, sl, design.fit[static_cast<std::size_t>(sl)]);
        const uint16_t               live_it = design_fitted(w, nation, cls, sl);
        const land_combat_stats      now     = kit_stats(cls, design);
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
            // The seam's check is against the LIVE design, so it calls the live item
            // "already fitted"; in a draft that differs, the live item is a live pick
            // (it returns the slot to the live design). The draft's own item is the
            // one disabled "Fitted".
            corp_command_result why =
                unit_design_check(w, nation, static_cast<uint32_t>(key), static_cast<uint16_t>(sl), id);
            if (is_fit)
                why = corp_command_result::rejected_state;
            else if (id == live_it && why == corp_command_result::rejected_state)
                why = corp_command_result::applied;
            const bool locked  = !kit_item_researched(w, nation, id);
            const bool enabled = why == corp_command_result::applied && !d.confirm_close;
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
                // A pick drafts: it changes only the open design's draft. Picking
                // the live item clears the slot's draft, so it no longer reads changed.
                d.draft_cls = key;
                d.draft[static_cast<std::size_t>(sl)] = id == live_it ? k_no_kit_item : id;
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
            const bool  is_live = id == live_it && !is_fit; // the saved item, when the draft moved off it
            const char* badge = is_fit ? "Fitted" : locked ? "LOCKED" : is_live ? "Saved" : nullptr;
            const float badge_w = badge ? ImGui::CalcTextSize(badge).x + 10.0f : 0.0f;
            if (badge)
            {
                const ImVec2 bp = {r0.x + rw - badge_w, r0.y + 3.0f};
                const ImU32  bc = is_fit ? palette::positive : is_live ? palette::text_secondary : palette::negative;
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
        const land_combat_stats now = unit_designer_draft_stats(w, s, nation, cls);
        unit_design next = design;
        const bool has_preview = preview != k_no_kit_item && d.slot_open >= 0 && !d.confirm_close
                              && kit_variant_of(preview) != design.fit[static_cast<std::size_t>(d.slot_open)];
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

        // The save model, said once: a pick drafts; Save makes it the live design.
        char stale[200];
        if (d.stale_units > 0)
            std::snprintf(stale, sizeof stale, "%d unit%s in the field on an older design \xe2\x80\x94 refit from the unit card.",
                          d.stale_units, d.stale_units == 1 ? "" : "s");
        else
            std::snprintf(stale, sizeof stale, "A pick is a draft. Save makes it the live design: new units are raised with it, units in the field take it by refit.");
        ImGui::TextDisabled("%s", stale);

        // The Save / Revert row. Both act on the whole draft; both are live only
        // while a slot is changed. Actions, not states, so the toggle rule does
        // not apply.
        ImGui::BeginDisabled(d.changed_slots == 0 || d.confirm_close);
        if (ImGui::Button("Save design"))
            save_draft(w, d, nation);
        {
            const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            d.save_x = (a.x + b.x) * 0.5f;
            d.save_y = (a.y + b.y) * 0.5f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert"))
        {
            clear_draft(d);
            d.draft_cls = key;
            d.last_message.clear();
        }
        {
            const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            d.revert_x = (a.x + b.x) * 0.5f;
            d.revert_y = (a.y + b.y) * 0.5f;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (d.changed_slots > 0)
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned), "%d slot%s changed, not saved.",
                               d.changed_slots, d.changed_slots == 1 ? "" : "s");
        else if (!d.last_message.empty())
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", d.last_message.c_str());
        else
            ImGui::TextDisabled("No changes.");
    }

    // ── The close prompt: a close was asked for with an unsaved draft ─────────
    int answer = 0; // 1 save, 2 discard, 3 keep editing
    if (d.confirm_close)
    {
        const float pw = std::min(460.0f, avail.x - 40.0f);
        const float ph = 2.0f * ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + 24.0f;
        ImGui::SetCursorScreenPos({o.x + (avail.x - pw) * 0.5f, o.y + (main_h - ph) * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 22, 28, 252));
        ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(palette::pinned));
        ImGui::BeginChild("##designer_close_prompt", {pw, ph}, ImGuiChildFlags_Borders);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Unsaved changes to the %s design",
                           class_label(cls));
        ImGui::TextDisabled("%d slot%s changed. Save them before closing?", d.changed_slots,
                            d.changed_slots == 1 ? "" : "s");
        auto press = [](const char* label, float& x, float& y) {
            const bool hit = ImGui::Button(label);
            const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            x = (a.x + b.x) * 0.5f;
            y = (a.y + b.y) * 0.5f;
            return hit;
        };
        if (press("Save", d.prompt_save_x, d.prompt_save_y))           answer = 1;
        ImGui::SameLine();
        if (press("Discard", d.prompt_discard_x, d.prompt_discard_y))  answer = 2;
        ImGui::SameLine();
        if (press("Keep editing", d.prompt_keep_x, d.prompt_keep_y))   answer = 3;
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    fold_overlay_end(s);

    // Answered after the window ends: Save and Discard carry the close on to
    // where it was going (the ledger or card the designer was opened from, or
    // another takeover); Keep editing stays.
    if (answer == 1)
    {
        save_draft(w, d, nation);
        finish_close(s);
    }
    else if (answer == 2)
    {
        clear_draft(d);
        finish_close(s);
    }
    else if (answer == 3)
        d.confirm_close = false;
}

} // namespace ui
