#include "unit_designer.hpp"

#include "detail_level.hpp"
#include "icons.hpp"
#include "presentation.hpp"
#include "unit_art.hpp"
#include "wing_card.hpp" // air_class_label

#include "world/air_combat.hpp"             // wing_class — an air class's stale wings
#include "world/corp_command.hpp"           // unit_design_check / air_design_check
#include "world/digitisation_tree_data.hpp" // io::dg::find_node — a locked item's node, named
#include "world/unit_designs.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace ui {

namespace {

// ===========================================================================
// THE THEATRE ADAPTER (BL-1450). The designer is one machine — the draft, Save,
// Revert, the close catch and prompt, the boxes and the item list — over two kit
// catalogues: the land classes' (`kit_catalogue`, `set_unit_design`) and the air
// classes' (`air_kit_catalogue`, `set_air_design`; MILITARY.md § Air, slots
// airframe / armament / avionics). Everything the draw reads about a class goes
// through this interface, so the draft/Save model (military.md § The designer,
// NR-1096) is written once and holds for both.
// ===========================================================================

/// One column of the summary: its label, the lower-case word a delta uses, the
/// value, and whether it is a per-mille shown as a percentage.
struct stat
{
    const char* label;
    const char* word;
    int         value;
    bool        pct;
};

/// "steel 2, machinery 1" from a goods array; "none (base kit)" when empty.
std::string goods_text(const std::array<float, resource_count>& goods)
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

/// An item's goods: "steel 2, machinery 1"; "no goods" for base kit.
template <class Item>
std::string item_goods_text(const Item& it)
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

/// The non-zero differences b - a, signed: "+10 soft attack, +15% hardness";
/// "No change" when none differ.
std::string stat_delta_text(const std::vector<stat>& a, const std::vector<stat>& b)
{
    std::string out;
    char        buf[48];
    for (std::size_t i = 0; i < a.size() && i < b.size(); ++i)
    {
        const int v = b[i].value - a[i].value;
        if (v == 0)
            continue;
        if (a[i].pct) std::snprintf(buf, sizeof buf, "%+d%% %s", v / 10, a[i].word);
        else          std::snprintf(buf, sizeof buf, "%+d %s", v, a[i].word);
        if (!out.empty()) out += ", ";
        out += buf;
    }
    return out.empty() ? std::string("No change") : out;
}

class theatre
{
public:
    virtual ~theatre() = default;
    virtual int                          slots() const = 0;
    virtual const char*                  slot_name(int sl) const = 0;
    virtual const std::vector<uint16_t>& items(int sl) const = 0;
    virtual uint16_t                     live(int sl) const = 0;      ///< The live design's item.
    virtual uint16_t                     item_at(int sl, uint8_t variant) const = 0;
    virtual uint8_t                      variant_of(uint16_t item) const = 0;
    virtual unit_design                  live_design() const = 0;
    virtual std::size_t                  catalogue_size() const = 0;
    virtual const char*                  item_name(uint16_t item) const = 0;
    virtual bool                         item_is_base(uint16_t item) const = 0;
    virtual std::string                  item_goods(uint16_t item) const = 0;
    virtual const char*                  research_node(uint16_t item) const = 0;
    virtual bool                         researched(uint16_t item) const = 0;
    virtual corp_command_result          check(int sl, uint16_t item) const = 0;
    virtual std::string                  item_delta(uint16_t item) const = 0; ///< Its own change, for the box.
    virtual std::vector<stat>            stats(const unit_design& d) const = 0;
    virtual std::string                  design_goods(const unit_design& d) const = 0;
    virtual const char*                  class_label() const = 0;
    virtual const char*                  per() const = 0;             ///< "unit" / "wing".
    virtual int                          stale() const = 0;           ///< Fielded on an older design.
    virtual std::string                  stale_text(int n) const = 0;
    virtual std::string                  model_text() const = 0;
    /// The class's image in [i0, i1] (or a placeholder) and its anchors.
    virtual void  draw_image(ImDrawList* dl, ImVec2 i0, ImVec2 i1) const = 0;
    virtual float image_w() const = 0;
    virtual float image_h() const = 0;
    virtual float anchor_x(int sl) const = 0;
    virtual float anchor_y(int sl) const = 0;
};

constexpr ImU32 k_box_bg     = IM_COL32(24, 26, 33, 255);
constexpr ImU32 k_box_border = IM_COL32(90, 94, 106, 255);
constexpr ImU32 k_leader     = IM_COL32(120, 126, 140, 255);

const char* land_label(land_class c)
{
    static const char* const k_names[land_class_count] = {
        "Infantry", "Motorised infantry", "Mechanised infantry", "Armour",
        "Light armour", "Artillery", "Air defence", "Not a land fighter",
    };
    const int i = static_cast<int>(c);
    return (i >= 0 && i < land_class_count) ? k_names[i] : "Unknown";
}

/// The land classes (`kit_catalogue`; MILITARY.md § The kit).
class land_theatre final : public theatre
{
public:
    land_theatre(const world& w, entity_id nation, land_class c) : w_(w), nation_(nation), c_(c) {}
    int slots() const override { return kit_slot_count(c_); }
    const char* slot_name(int sl) const override { return kit_slot_name(c_, sl); }
    const std::vector<uint16_t>& items(int sl) const override { return kit_items_for(c_, sl); }
    uint16_t live(int sl) const override { return design_fitted(w_, nation_, c_, sl); }
    uint16_t item_at(int sl, uint8_t v) const override { return kit_item_at(c_, sl, v); }
    uint8_t variant_of(uint16_t it) const override { return kit_variant_of(it); }
    unit_design live_design() const override { return design_of(w_, nation_, c_); }
    std::size_t catalogue_size() const override { return kit_catalogue().size(); }
    const char* item_name(uint16_t it) const override { return kit_catalogue()[it].name; }
    bool item_is_base(uint16_t it) const override { return kit_catalogue()[it].goods_count == 0; }
    std::string item_goods(uint16_t it) const override { return item_goods_text(kit_catalogue()[it]); }
    const char* research_node(uint16_t it) const override { return kit_catalogue()[it].research_node; }
    bool researched(uint16_t it) const override { return kit_item_researched(w_, nation_, it); }
    corp_command_result check(int sl, uint16_t it) const override
    {
        return unit_design_check(w_, nation_, static_cast<uint32_t>(c_), static_cast<uint16_t>(sl), it);
    }
    std::string item_delta(uint16_t it) const override
    {
        const land_combat_stats& d = kit_catalogue()[it].delta;
        static const std::vector<stat> zero = to_stats({});
        return stat_delta_text(zero, to_stats(d));
    }
    std::vector<stat> stats(const unit_design& d) const override { return to_stats(kit_stats(c_, d)); }
    std::string design_goods(const unit_design& d) const override { return goods_text(kit_goods(c_, d)); }
    const char* class_label() const override { return land_label(c_); }
    const char* per() const override { return "unit"; }
    int stale() const override
    {
        int n = 0;
        for (const auto& [uid, u] : w_.units)
            if (u.owner == nation_ && unit_land_class(u) == c_ && unit_design_stale(w_, u))
                ++n;
        return n;
    }
    std::string stale_text(int n) const override
    {
        char buf[160];
        std::snprintf(buf, sizeof buf, "%d unit%s in the field on an older design \xe2\x80\x94 refit from the unit card.",
                      n, n == 1 ? "" : "s");
        return buf;
    }
    std::string model_text() const override
    {
        return "A pick is a draft. Save makes it the live design: new units are raised with it, units in the field take it by refit.";
    }
    void draw_image(ImDrawList* dl, ImVec2 i0, ImVec2 i1) const override
    {
        const unit_art& art = unit_art_for(c_);
        if (art.tex != nullptr)
        {
            dl->AddImage(reinterpret_cast<ImTextureID>(art.tex), i0, i1);
            return;
        }
        dl->AddRect(i0, i1, k_box_border, 4.0f, 0, 1.5f);
        const char* msg = "No image for this class";
        const ImVec2 ts = ImGui::CalcTextSize(msg);
        dl->AddText({(i0.x + i1.x - ts.x) * 0.5f, (i0.y + i1.y - ts.y) * 0.5f}, palette::text_secondary, msg);
    }
    float image_w() const override { const unit_art& a = unit_art_for(c_); return a.width > 0 ? static_cast<float>(a.width) : 640.0f; }
    float image_h() const override { const unit_art& a = unit_art_for(c_); return a.height > 0 ? static_cast<float>(a.height) : 400.0f; }
    float anchor_x(int sl) const override { return unit_art_for(c_).anchor_x[sl]; }
    float anchor_y(int sl) const override { return unit_art_for(c_).anchor_y[sl]; }

private:
    static std::vector<stat> to_stats(const land_combat_stats& s)
    {
        return {{"Soft attack", "soft attack", s.soft, false},
                {"Hard attack", "hard attack", s.hard, false},
                {"Defence", "defence", s.defence, false},
                {"Breakthrough", "breakthrough", s.breakthrough, false},
                {"Hardness", "hardness", s.hardness, true},
                {"Recon", "recon", s.recon, false},       // BL-1453 (MILITARY.md § Recon)
                {"Stealth", "stealth", s.stealth, false}};
    }
    const world& w_;
    entity_id    nation_;
    land_class   c_;
};

/// The air classes (`air_kit_catalogue`; MILITARY.md § Air). No air art yet: the
/// image is the aircraft glyph on the placeholder panel, anchored at the wing root
/// (airframe), under the wing (armament) and the nose (avionics).
class air_theatre final : public theatre
{
public:
    air_theatre(const world& w, const recipe_registry& reg, entity_id nation, air_class c)
        : w_(w), reg_(reg), nation_(nation), c_(c) {}
    int slots() const override { return k_air_kit_slot_count; }
    const char* slot_name(int sl) const override { return air_kit_slot_name(sl); }
    const std::vector<uint16_t>& items(int sl) const override { return air_kit_items_for(c_, sl); }
    uint16_t live(int sl) const override { return air_design_fitted(w_, nation_, c_, sl); }
    uint16_t item_at(int sl, uint8_t v) const override { return air_kit_item_at(c_, sl, v); }
    uint8_t variant_of(uint16_t it) const override { return air_kit_variant_of(it); }
    unit_design live_design() const override { return air_design_of(w_, nation_, c_); }
    std::size_t catalogue_size() const override { return air_kit_catalogue().size(); }
    const char* item_name(uint16_t it) const override { return air_kit_catalogue()[it].name; }
    bool item_is_base(uint16_t it) const override { return air_kit_catalogue()[it].goods_count == 0; }
    std::string item_goods(uint16_t it) const override { return item_goods_text(air_kit_catalogue()[it]); }
    const char* research_node(uint16_t it) const override { return air_kit_catalogue()[it].research_node; }
    bool researched(uint16_t it) const override { return air_kit_item_researched(w_, nation_, it); }
    corp_command_result check(int sl, uint16_t it) const override
    {
        return air_design_check(w_, nation_, static_cast<uint32_t>(c_), static_cast<uint16_t>(sl), it);
    }
    std::string item_delta(uint16_t it) const override
    {
        return stat_delta_text(to_stats({}), to_stats(air_kit_catalogue()[it].delta));
    }
    std::vector<stat> stats(const unit_design& d) const override
    {
        const air_class_stats& cs = reg_.military().air.cls[static_cast<std::size_t>(c_)];
        const air_combat_stats k  = air_kit_delta(c_, d);
        air_combat_stats t;
        t.hp = cs.hp + k.hp;  t.air = cs.air + k.air;  t.soft = cs.soft + k.soft;  t.hard = cs.hard + k.hard;
        t.strategic = cs.strategic + k.strategic;  t.recon = cs.recon + k.recon;  t.range = cs.range + k.range;
        return to_stats(t);
    }
    std::string design_goods(const unit_design& d) const override { return goods_text(air_kit_goods(c_, d)); }
    const char* class_label() const override { return air_class_label(c_); }
    const char* per() const override { return "wing"; }
    int stale() const override
    {
        const unit_design live_d = live_design();
        int n = 0;
        for (const auto& [wid, wg] : w_.wings)
            if (wg.owner == nation_ && wing_class(wg) == c_ && wg.kit != live_d)
                ++n;
        return n;
    }
    std::string stale_text(int n) const override
    {
        char buf[120];
        std::snprintf(buf, sizeof buf, "%d wing%s flying an older design.", n, n == 1 ? "" : "s");
        return buf;
    }
    std::string model_text() const override
    {
        return "A pick is a draft. Save makes it the live design: new wings are raised with it.";
    }
    void draw_image(ImDrawList* dl, ImVec2 i0, ImVec2 i1) const override
    {
        dl->AddRect(i0, i1, k_box_border, 4.0f, 0, 1.5f);
        icons::aircraft(dl, {(i0.x + i1.x) * 0.5f, (i0.y + i1.y) * 0.5f},
                        std::min(i1.x - i0.x, i1.y - i0.y) * 0.42f,
                        palette::building_kind_colour(building_type::airbase));
    }
    float image_w() const override { return 640.0f; }
    float image_h() const override { return 400.0f; }
    float anchor_x(int sl) const override { static constexpr float x[3] = {0.30f, 0.76f, 0.52f}; return x[sl]; }
    float anchor_y(int sl) const override { static constexpr float y[3] = {0.58f, 0.66f, 0.20f}; return y[sl]; }

private:
    static std::vector<stat> to_stats(const air_combat_stats& s)
    {
        return {{"HP", "HP", s.hp, false},
                {"Air attack", "air attack", s.air, false},
                {"Soft attack", "soft attack", s.soft, false},
                {"Hard attack", "hard attack", s.hard, false},
                {"Strategic", "strategic", s.strategic, false},
                {"Recon", "recon", s.recon, false},
                {"Range", "range", s.range, false}};
    }
    const world&           w_;
    const recipe_registry& reg_;
    entity_id              nation_;
    air_class              c_;
};

bool is_air(detail_surface s) { return s == detail_surface::air_designer; }

/// Is (@p surface, @p key) a designer the theatre can open?
bool valid_key(detail_surface surface, int key)
{
    if (is_air(surface))
        return key >= 0 && key < air_class_count;
    return key >= 0 && key < land_class_count && kit_slot_count(static_cast<land_class>(key)) > 0;
}

/// The theatre of (@p surface, @p key). @p reg may be null for a land class only
/// (the land arithmetic needs none); an air class without one gets nullptr.
std::unique_ptr<theatre> make_theatre(const world& w, const recipe_registry* reg, entity_id nation,
                                      detail_surface surface, int key)
{
    if (!valid_key(surface, key))
        return nullptr;
    if (is_air(surface))
    {
        if (reg == nullptr)
            return nullptr;
        return std::make_unique<air_theatre>(w, *reg, nation, static_cast<air_class>(key));
    }
    return std::make_unique<land_theatre>(w, nation, static_cast<land_class>(key));
}

/// The draft's picks laid over @p t's live design.
unit_design drafted(const theatre& t, const unit_designer_state& d)
{
    unit_design out = t.live_design();
    for (int sl = 0; sl < t.slots() && sl < static_cast<int>(k_kit_slot_max); ++sl)
    {
        const uint16_t it = d.draft[static_cast<std::size_t>(sl)];
        if (it != k_no_kit_item)
            out.fit[static_cast<std::size_t>(sl)] = t.variant_of(it);
    }
    return out;
}

/// Slots of the open draft that differ from its live design. Item ids only, so no
/// registry is needed: an air draft is counted through a registry-free probe.
int changed_in(const world& w, const unit_designer_state& d, entity_id nation)
{
    if (d.draft_cls < 0 || !valid_key(d.draft_surface, d.draft_cls))
        return 0;
    int n = 0;
    const bool air   = is_air(d.draft_surface);
    const int  slots = air ? k_air_kit_slot_count : kit_slot_count(static_cast<land_class>(d.draft_cls));
    for (int sl = 0; sl < slots && sl < static_cast<int>(k_kit_slot_max); ++sl)
    {
        const uint16_t it = d.draft[static_cast<std::size_t>(sl)];
        if (it == k_no_kit_item)
            continue;
        const uint16_t live = air ? air_design_fitted(w, nation, static_cast<air_class>(d.draft_cls), sl)
                                  : design_fitted(w, nation, static_cast<land_class>(d.draft_cls), sl);
        if (it != live)
            ++n;
    }
    return n;
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
std::string refusal_text(corp_command_result r, const char* research)
{
    switch (r)
    {
        case corp_command_result::applied:              return {};
        case corp_command_result::rejected_tech_locked: return "Locked: needs research " + node_text(research);
        case corp_command_result::rejected_state:       return "Already fitted";
        case corp_command_result::rejected_not_owner:   return "Only your own nation's design can be changed";
        default:                                        return "Does not fit this slot";
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

void clear_draft(unit_designer_state& d)
{
    d.draft_cls = -1;
    d.draft.fill(k_no_kit_item);
}

/// Save: queue one pick per changed slot for app::render, then drop the draft.
void save_draft(const world& w, unit_designer_state& d, entity_id nation)
{
    if (d.draft_cls >= 0 && valid_key(d.draft_surface, d.draft_cls))
    {
        const bool air   = is_air(d.draft_surface);
        const int  slots = air ? k_air_kit_slot_count : kit_slot_count(static_cast<land_class>(d.draft_cls));
        for (int sl = 0; sl < slots && sl < static_cast<int>(k_kit_slot_max); ++sl)
        {
            const uint16_t it = d.draft[static_cast<std::size_t>(sl)];
            const uint16_t live = air ? air_design_fitted(w, nation, static_cast<air_class>(d.draft_cls), sl)
                                      : design_fitted(w, nation, static_cast<land_class>(d.draft_cls), sl);
            if (it != k_no_kit_item && it != live)
                d.pending.push_back({d.draft_cls, sl, it, air});
        }
    }
    clear_draft(d);
}

/// Carry the interrupted close to where it was going.
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

static_assert(std::tuple_size<decltype(unit_designer_state::draft)>::value == k_kit_slot_max,
              "the designer's draft holds one item per kit slot");
static_assert(k_air_kit_slot_count <= static_cast<int>(k_kit_slot_max),
              "an air design's slots fit the draft");

unit_design unit_designer_draft_design(const world& w, const ui_state& s, entity_id nation, land_class c)
{
    if (s.designer.draft_cls != static_cast<int>(c) || is_air(s.designer.draft_surface))
        return design_of(w, nation, c);
    return drafted(land_theatre(w, nation, c), s.designer);
}

land_combat_stats unit_designer_draft_stats(const world& w, const ui_state& s, entity_id nation, land_class c)
{
    return kit_stats(c, unit_designer_draft_design(w, s, nation, c));
}

int unit_designer_changed_slots(const world& w, const ui_state& s, entity_id nation)
{
    return changed_in(w, s.designer, nation);
}

void reset_unit_designer(ui_state& s)
{
    unit_designer_state& d = s.designer;
    clear_draft(d);
    d.draft_surface    = detail_surface::unit_designer;
    d.confirm_close    = false;
    d.after_surface    = detail_surface::none;
    d.after_key        = 0;
    d.pending.clear();
    d.refused.clear();
    d.save_answered    = false;
    d.close_after_save = false;
    d.last_message.clear();
    d.slot_open = -1;
    d.open_cls  = -1;
    d.open_surface = detail_surface::none;
}

void draw_unit_designer(const world& w, const recipe_registry& reg, ui_state& s)
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
    d.drawn_air   = false;

    const entity_id nation = player_nation(w);

    // THE SAVE'S ANSWER (app::render sent `pending` after the last draw). Refused
    // picks go back into the draft, so the Save row shows them still changed with
    // the seam's message beside them; a prompt Save closes only when all landed.
    if (d.save_answered)
    {
        d.save_answered = false;
        if (!d.refused.empty())
        {
            clear_draft(d);
            d.draft_cls     = d.refused.front().cls;
            d.draft_surface = d.refused.front().air ? detail_surface::air_designer : detail_surface::unit_designer;
            for (const auto& r : d.refused)
                if (r.cls == d.draft_cls && r.air == is_air(d.draft_surface)
                    && r.slot >= 0 && r.slot < static_cast<int>(k_kit_slot_max))
                    d.draft[static_cast<std::size_t>(r.slot)] = r.item;
            d.refused.clear();
            d.close_after_save = false; // stay open on what was refused
        }
        else if (d.close_after_save)
        {
            d.close_after_save = false;
            finish_close(s);
        }
    }

    // THE CLOSE CATCH. Every close path — the toggle press (toggle_*_designer),
    // the return control (fold_overlay_begin), Escape (app.cpp's rung), a press
    // opening another takeover (the other theatre's designer included) — ends in
    // `fold` or `expand` on `ui_state::expanded`, and all of them run before this
    // draw. So one check here covers them all without each knowing about drafts: if
    // the draft has unsaved changes and the takeover is no longer the draft's
    // (surface, class), put the designer back, remember where the close was going,
    // and ask.
    //
    // A close drawn by a surface that draws BEFORE this one (another takeover's
    // opening press) shows that surface for one frame before the catch puts the
    // designer back. Accepted: one frame, and only with an unsaved draft.
    if (d.draft_cls >= 0 && !is_expanded(s, d.draft_surface, d.draft_cls))
    {
        if (changed_in(w, d, nation) > 0)
        {
            // A second close over an open prompt (Escape again, the class press)
            // keeps the FIRST close's destination.
            if (!d.confirm_close)
            {
                d.after_surface = s.expanded.surface;
                d.after_key     = s.expanded.key;
            }
            d.confirm_close    = true;
            s.expanded.surface = d.draft_surface;
            s.expanded.key     = d.draft_cls;
            s.expanded.focus   = false;
        }
        else
        {
            clear_draft(d);
            d.confirm_close = false;
        }
    }

    const detail_surface surface = s.expanded.surface;
    if (surface != detail_surface::unit_designer && surface != detail_surface::air_designer)
    {
        d.slot_open    = -1;
        d.open_cls     = -1;
        d.open_surface = detail_surface::none;
        return;
    }
    const int key = s.expanded.key;
    const std::unique_ptr<theatre> tp = make_theatre(w, &reg, nation, surface, key);
    if (!tp)
    {
        fold(s);
        return;
    }
    const theatre& t     = *tp;
    const int      slots = t.slots();
    if (d.open_cls != key || d.open_surface != surface)
    {
        d.open_cls     = key;
        d.open_surface = surface;
        d.slot_open    = -1;
        d.last_message.clear();
    }
    if (d.draft_cls != key || d.draft_surface != surface)
    {
        clear_draft(d); // the catch above has already asked about a changed one
        d.draft_cls     = key;
        d.draft_surface = surface;
    }
    d.changed_slots = changed_in(w, d, nation);
    if (d.changed_slots == 0)
        d.confirm_close = false;
    if (d.slot_open >= slots)
        d.slot_open = -1;

    char title[96];
    std::snprintf(title, sizeof title, "Unit designer \xe2\x80\x94 %s", t.class_label());
    if (!fold_overlay_begin(s, surface, key, title))
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
    d.drawn_air = is_air(surface);

    const unit_design design = drafted(t, d); // what is shown
    ImDrawList*       dl     = ImGui::GetWindowDrawList();
    const float       line   = ImGui::GetTextLineHeight();
    const float       pad    = 6.0f;
    d.stale_units = t.stale();

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
    const float aw = t.image_w(), ah = t.image_h();
    const float scale = std::min(img_w_room / aw, main_h / ah);
    const float iw = aw * scale, ih = ah * scale;
    const ImVec2 i0 = {img_x0 + (img_w_room - iw) * 0.5f, o.y + (main_h - ih) * 0.5f};
    const ImVec2 i1 = {i0.x + iw, i0.y + ih};
    t.draw_image(dl, i0, i1);

    // Anchors on screen, and which side each slot's box takes: the side its
    // anchor is nearer, so leader lines do not cross the image.
    struct slot_pos { int slot; ImVec2 anchor; bool right; float top; };
    std::vector<slot_pos> pos;
    for (int sl = 0; sl < slots; ++sl)
    {
        const ImVec2 a = {i0.x + t.anchor_x(sl) * iw, i0.y + t.anchor_y(sl) * ih};
        pos.push_back({sl, a, t.anchor_x(sl) >= 0.5f, 0.0f});
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
    const std::size_t ncat = t.catalogue_size();
    float open_box_top = 0.0f;
    bool  open_box_right = false;
    float open_box_x = 0.0f;
    for (const slot_pos& p : pos)
    {
        const int      sl      = p.slot;
        const uint16_t live_it = t.live(sl);
        const uint16_t fitted  = t.item_at(sl, design.fit[static_cast<std::size_t>(sl)]);
        const bool     changed = fitted != live_it;
        const bool     has_fi  = fitted < ncat;
        const float    bx      = p.right ? o.x + avail.x - box_w : o.x;
        const ImVec2   b0      = {bx, p.top};
        const ImVec2   b1      = {bx + box_w, p.top + box_h};

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
        if (hot && has_fi && !d.confirm_close)
        {
            if (changed)
                ImGui::SetTooltip("%s: %s (changed, not saved; the live design has %s)\n%s\nPress to %s the choices.",
                                  t.slot_name(sl), t.item_name(fitted),
                                  live_it < ncat ? t.item_name(live_it) : "\xe2\x80\x94",
                                  t.item_delta(fitted).c_str(), open ? "close" : "see");
            else
                ImGui::SetTooltip("%s: %s\n%s\nPress to %s the choices.", t.slot_name(sl), t.item_name(fitted),
                                  t.item_delta(fitted).c_str(), open ? "close" : "see");
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
            clipped_text(dl, {b0.x + pad, ty}, tw - ms.x - 6.0f, palette::text_secondary, t.slot_name(sl));
        }
        else
            clipped_text(dl, {b0.x + pad, ty}, tw, palette::text_secondary, t.slot_name(sl));
        ty += line + 2.0f;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::selection, has_fi ? t.item_name(fitted) : "\xe2\x80\x94");
        ty += line + 2.0f;
        clipped_text(dl, {b0.x + pad, ty}, tw, palette::text_secondary,
                     has_fi && !t.item_is_base(fitted) ? t.item_delta(fitted) : std::string("Base kit"));

        unit_designer_state::drawn_box db;
        db.slot = sl;
        db.name = t.slot_name(sl);
        db.fitted = has_fi ? t.item_name(fitted) : "";
        db.live = live_it < ncat ? t.item_name(live_it) : "";
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
    const std::vector<stat> now_stats = t.stats(design);
    uint16_t preview = k_no_kit_item;
    if (d.slot_open >= 0)
    {
        const int                    sl     = d.slot_open;
        const std::vector<uint16_t>& items  = t.items(sl);
        // `fitted` is the DRAFT's item (what the box shows: the one-of-n selector's
        // choice); `live_it` is the live design's, which a pick can return to.
        const uint16_t fitted  = t.item_at(sl, design.fit[static_cast<std::size_t>(sl)]);
        const uint16_t live_it = t.live(sl);
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
                           t.slot_name(sl));
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const float rw  = ImGui::GetContentRegionAvail().x;
        for (const uint16_t id : items)
        {
            if (id >= ncat)
                continue;
            const bool is_fit = id == fitted;
            // The seam's check is against the LIVE design, so it calls the live item
            // "already fitted"; in a draft that differs, the live item is a live pick
            // (it returns the slot to the live design). The draft's own item is the
            // one disabled "Fitted".
            corp_command_result why = t.check(sl, id);
            if (is_fit)
                why = corp_command_result::rejected_state;
            else if (id == live_it && why == corp_command_result::rejected_state)
                why = corp_command_result::applied;
            const bool locked  = !t.researched(id);
            const bool enabled = why == corp_command_result::applied && !d.confirm_close;
            const float row_h  = locked ? row_h3 + line : row_h3;

            // The item's stats against the FITTED item: what this pick would change.
            unit_design probe = design;
            probe.fit[static_cast<std::size_t>(sl)] = t.variant_of(id);
            const std::string diff = stat_delta_text(now_stats, t.stats(probe));

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
                d.draft_cls     = key;
                d.draft_surface = surface;
                d.draft[static_cast<std::size_t>(sl)] = id == live_it ? k_no_kit_item : id;
                d.last_message.clear();
            }
            if (hot)
            {
                preview = id;
                if (!enabled)
                    ImGui::SetTooltip("%s", refusal_text(why, t.research_node(id)).c_str());
            }

            // Line 1: name, then the badge — Fitted, LOCKED or Saved — at the right.
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
                         locked ? palette::text_secondary : palette::selection, t.item_name(id));
            // Line 2: what it changes against the fitted item.
            clipped_text(cdl, {tx, r0.y + 3.0f + line}, rw - 12.0f, palette::text_secondary,
                         is_fit ? std::string("Fitted now") : diff);
            // Line 3: goods per unit / wing. Line 4, when locked: the node that opens it.
            clipped_text(cdl, {tx, r0.y + 3.0f + 2.0f * line}, rw - 12.0f, palette::text_secondary,
                         t.item_goods(id) + " per " + t.per());
            if (locked)
                clipped_text(cdl, {tx, r0.y + 3.0f + 3.0f * line}, rw - 12.0f, palette::negative,
                             "Needs research " + node_text(t.research_node(id)));

            unit_designer_state::drawn_item di;
            di.item = id;
            di.name = t.item_name(id);
            di.x = r0.x + rw * 0.5f;
            di.y = r0.y + row_h * 0.5f;
            di.fitted = is_fit;
            di.enabled = enabled;
            di.locked = locked;
            di.reason = refusal_text(why, t.research_node(id));
            d.items.push_back(di);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
    }

    // ── Summary: the design's stats and kit goods, previewing a hovered pick ──
    ImGui::SetCursorScreenPos({o.x, o.y + main_h + 6.0f});
    ImGui::Separator();
    {
        unit_design next = design;
        const bool has_preview = preview != k_no_kit_item && d.slot_open >= 0 && !d.confirm_close
                              && t.variant_of(preview) != design.fit[static_cast<std::size_t>(d.slot_open)];
        if (has_preview)
            next.fit[static_cast<std::size_t>(d.slot_open)] = t.variant_of(preview);
        const std::vector<stat> then = t.stats(next);

        const int   n     = static_cast<int>(now_stats.size());
        const float col_w = avail.x / static_cast<float>(std::max(1, n));
        const float x0    = ImGui::GetCursorPosX();
        for (int i = 0; i < n; ++i)
        {
            if (i) ImGui::SameLine(x0 + col_w * static_cast<float>(i));
            const stat& c  = now_stats[static_cast<std::size_t>(i)];
            const int   cb = then[static_cast<std::size_t>(i)].value;
            const int   a  = c.pct ? (c.value + 5) / 10 : c.value;
            const int   b  = c.pct ? (cb + 5) / 10 : cb;
            if (has_preview && c.value != cb)
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(cb > c.value ? palette::positive : palette::negative),
                                   "%s %d%s \xe2\x86\x92 %d%s", c.label, a, c.pct ? "%" : "", b, c.pct ? "%" : "");
            else
                ImGui::Text("%s %d%s", c.label, a, c.pct ? "%" : "");
        }
        const std::string goods_now = t.design_goods(design);
        if (has_preview)
            ImGui::TextDisabled("Kit goods per %s: %s  \xe2\x86\x92  %s", t.per(), goods_now.c_str(),
                                t.design_goods(next).c_str());
        else
            ImGui::TextDisabled("Kit goods per %s: %s", t.per(), goods_now.c_str());

        // The save model, said once: a pick drafts; Save makes it the live design.
        ImGui::TextDisabled("%s", d.stale_units > 0 ? t.stale_text(d.stale_units).c_str() : t.model_text().c_str());

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
            d.draft_cls     = key;
            d.draft_surface = surface;
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
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::pinned), "%d slot%s changed, not saved.",
                               d.changed_slots, d.changed_slots == 1 ? "" : "s");
            // A partly refused Save leaves its refused slots changed: say why beside them.
            if (!d.last_message.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", d.last_message.c_str());
            }
        }
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
                           t.class_label());
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
        // The close waits for the seam's answer (next draw): it goes on only if
        // every pick landed, else the designer stays open on the refused ones.
        save_draft(w, d, nation);
        d.confirm_close    = false;
        d.close_after_save = true;
        if (d.pending.empty())
            finish_close(s); // nothing to send (the draft matched the live design)
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
