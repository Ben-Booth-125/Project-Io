#include "stat_grid.hpp"

#include "player_side.hpp" // is_player_side
#include "presentation.hpp"

#include "world/air_combat.hpp" // wing_stats, wing_class

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace ui {

namespace {

using icons::stat_glyph;

constexpr const char* k_dash = "\xe2\x80\x94";

/// How a stat's raw number prints.
enum class fmt_kind
{
    whole,   ///< An integer as is.
    permille, ///< Per-mille, printed as a whole percentage.
    days,    ///< Days, one decimal where it has one.
};

/// What a stat is: its glyph, its tooltip, how it prints, its bar colour (0 = none).
struct spec
{
    stat_glyph  glyph;
    const char* name;
    const char* what;
    fmt_kind    fmt;
    ImU32       bar;
};

// The land stats (MILITARY.md § Land - the unit's stats; § Carried supplies; § Recon;
// § Marching). One line each on what the number DOES, in the player's words.
const spec k_hp        {stat_glyph::hp, "HP", "Men fit to fight, against the count the unit is raised to. At none the unit is gone.", fmt_kind::whole, palette::unit_hp};
const spec k_readiness {stat_glyph::readiness, "Readiness", "Cohesion in contact. Takes most of the damage landed; at none the unit leaves the fight.", fmt_kind::permille, palette::unit_readiness};
const spec k_morale    {stat_glyph::morale, "Morale", "Will to fight. Scales the unit's attack and caps its readiness.", fmt_kind::permille, palette::unit_morale};
const spec k_food      {stat_glyph::food, "Food", "Carried food. Below full the unit is hungry; at none it starves and loses men.", fmt_kind::permille, palette::unit_food};
const spec k_ammo      {stat_glyph::ammunition, "Ammunition", "Carried ammunition. Scales every combat stat, never below a fifth.", fmt_kind::permille, palette::unit_ammo};
const spec k_equipment {stat_glyph::equipment, "Equipment", "Carried kit fit to use. Scales every combat stat.", fmt_kind::permille, palette::unit_equipment};
const spec k_soft      {stat_glyph::soft_attack, "Soft attack", "Hits on the enemy's unarmoured share, per man.", fmt_kind::whole, 0};
const spec k_hard      {stat_glyph::hard_attack, "Hard attack", "Hits on the enemy's armoured share, per man.", fmt_kind::whole, 0};
const spec k_defence   {stat_glyph::defence, "Defence", "Hits absorbed when defending, per man.", fmt_kind::whole, 0};
const spec k_break     {stat_glyph::breakthrough, "Breakthrough", "Hits absorbed when attacking, per man.", fmt_kind::whole, 0};
const spec k_hardness  {stat_glyph::hardness, "Hardness", "The share of the unit that is armoured: hard attack lands on it, soft attack on the rest.", fmt_kind::permille, 0};
const spec k_recon     {stat_glyph::recon, "Recon", "Added to its side's recon each day it attacks: what lets the attack see a stealthy defender.", fmt_kind::whole, 0};
const spec k_stealth   {stat_glyph::stealth, "Stealth", "The recon an attacker needs to land its full hits on this unit.", fmt_kind::whole, 0};
const spec k_speed     {stat_glyph::speed, "Speed", "Days to cross one plains tile. Terrain multiplies them; a road divides them.", fmt_kind::days, 0};

// The air stats (MILITARY.md § Air).
const spec k_air_hp      {stat_glyph::hp, "HP", "The wing's airframes fit to fly. At none the wing is lost.", fmt_kind::whole, palette::unit_hp};
const spec k_air_morale  {stat_glyph::morale, "Morale", "The crews' will to fight. Scales the wing's attack.", fmt_kind::permille, palette::unit_morale};
const spec k_air_supply  {stat_glyph::supply, "Supply", "How well its upkeep is met. Scales the wing's strength.", fmt_kind::permille, palette::unit_supply};
const spec k_air_air     {stat_glyph::air_attack, "Air attack", "Damage a day to hostile wings: what air superiority is fought with.", fmt_kind::whole, 0};
const spec k_air_soft    {stat_glyph::soft_attack, "Soft attack", "Bombing hits a day on a target's unarmoured share.", fmt_kind::whole, 0};
const spec k_air_hard    {stat_glyph::hard_attack, "Hard attack", "Bombing hits a day on a target's armoured share.", fmt_kind::whole, 0};
const spec k_air_strat   {stat_glyph::strategic, "Strategic", "Condition a day off a hostile building whose output its condition scales.", fmt_kind::permille, 0};
const spec k_air_recon   {stat_glyph::recon, "Recon", "Added to its side's hits over the provinces it is assigned.", fmt_kind::whole, 0};
const spec k_air_range   {stat_glyph::range, "Range", "Tiles from its airbase to the farthest province it can be assigned.", fmt_kind::whole, 0};

// The sea stats (MILITARY.md § Sea).
const spec k_sea_hp     {stat_glyph::hp, "HP", "The hull. It mends at the home port, drawing goods. At none the ship is sunk.", fmt_kind::whole, palette::unit_hp};
const spec k_sea_morale {stat_glyph::morale, "Morale", "The crew's will to fight. Scales its attack; below its floor the ship breaks off.", fmt_kind::permille, palette::unit_morale};
const spec k_sea_supply {stat_glyph::supply, "Supply", "How well its upkeep is met at its home port. Scales its strength.", fmt_kind::permille, palette::unit_supply};
const spec k_sea_attack {stat_glyph::soft_attack, "Attack", "Damage a day across the hostile ships its side has found and its class can hit.", fmt_kind::whole, 0};
const spec k_sea_detect {stat_glyph::recon, "Detection", "Added to its side's detection where it is. A ship is engaged only once found.", fmt_kind::whole, 0};
const spec k_sea_stealth{stat_glyph::stealth, "Stealth", "What a hostile side's detection is rolled against to find this ship.", fmt_kind::whole, 0};
const spec k_sea_speed  {stat_glyph::speed, "Speed", "Days to sail one tile.", fmt_kind::days, 0};
const spec k_sea_range  {stat_glyph::range, "Range", "Tiles over water from an own port to the farthest zone it can be given.", fmt_kind::whole, 0};
const spec k_sea_cap    {stat_glyph::capacity, "Capacity", "Land units it carries.", fmt_kind::whole, 0};

std::string fmt_value(fmt_kind f, double v)
{
    char buf[32];
    switch (f)
    {
        case fmt_kind::permille: std::snprintf(buf, sizeof buf, "%d%%", static_cast<int>(std::lround(v / 10.0))); break;
        case fmt_kind::days:     std::snprintf(buf, sizeof buf, "%.3g d", v); break;
        case fmt_kind::whole:
        default:                 std::snprintf(buf, sizeof buf, "%d", static_cast<int>(std::lround(v))); break;
    }
    return buf;
}

/// A cell of @p s showing @p v; a stored stat's bar at @p share.
stat_cell cell(const spec& s, double v, float share = -1.0f)
{
    stat_cell c;
    c.glyph = s.glyph;
    c.name  = s.name;
    c.what  = s.what;
    c.value = fmt_value(s.fmt, v);
    if (s.bar != 0 && share >= 0.0f)
    {
        c.bar     = std::clamp(share, 0.0f, 1.0f);
        c.bar_col = s.bar;
    }
    return c;
}

/// A hidden cell: the glyph, its tooltip, a dash.
stat_cell hidden(const spec& s, const char* why)
{
    stat_cell c;
    c.glyph  = s.glyph;
    c.name   = s.name;
    c.what   = s.what;
    c.value  = k_dash;
    c.detail = why;
    return c;
}

/// A design cell: @p v with its signed delta against @p live (display units).
stat_cell design_cell(const spec& s, int v, int live)
{
    stat_cell c = cell(s, v);
    const int a = s.fmt == fmt_kind::permille ? static_cast<int>(std::lround(v / 10.0)) : v;
    const int b = s.fmt == fmt_kind::permille ? static_cast<int>(std::lround(live / 10.0)) : live;
    const int d = a - b;
    if (d != 0)
    {
        char buf[24];
        // U+2212 MINUS SIGN for a loss, so the sign reads at the value's weight.
        if (d > 0) std::snprintf(buf, sizeof buf, "+%d%s", d, s.fmt == fmt_kind::permille ? "%" : "");
        else       std::snprintf(buf, sizeof buf, "\xe2\x88\x92%d%s", -d, s.fmt == fmt_kind::permille ? "%" : "");
        c.delta      = buf;
        c.delta_good = d > 0;
    }
    return c;
}

/// Mean / lowest / highest of a group's values for one stat.
struct acc
{
    double sum = 0.0, lo = 0.0, hi = 0.0, share = 0.0;
    int    n   = 0;
    void add(double v, double s = 0.0)
    {
        lo = n == 0 ? v : std::min(lo, v);
        hi = n == 0 ? v : std::max(hi, v);
        sum += v;
        share += s;
        ++n;
    }
};

stat_cell group_cell(const spec& s, const acc& a, const char* why_none)
{
    if (a.n == 0)
        return hidden(s, why_none);
    stat_cell c = cell(s, a.sum / a.n, s.bar != 0 ? static_cast<float>(a.share / a.n) : -1.0f);
    // The range in smaller type beneath, bare of its unit ("82-100", not
    // "82%-100%"): the value above names the unit, and the cell is narrow.
    auto bare = [&](double v) {
        char b[24];
        if (s.fmt == fmt_kind::permille) std::snprintf(b, sizeof b, "%d", static_cast<int>(std::lround(v / 10.0)));
        else if (s.fmt == fmt_kind::days) std::snprintf(b, sizeof b, "%.3g", v);
        else std::snprintf(b, sizeof b, "%d", static_cast<int>(std::lround(v)));
        return std::string(b);
    };
    c.range  = bare(a.lo) + "\xe2\x80\x93" + bare(a.hi);
    char buf[64];
    std::snprintf(buf, sizeof buf, "Mean of %d unit%s; lowest to highest beneath.", a.n, a.n == 1 ? "" : "s");
    c.detail = buf;
    return c;
}

float march_days(const recipe_registry& reg, land_class c)
{
    const auto& t = reg.military().march_days_per_land_class;
    const auto  i = static_cast<std::size_t>(c);
    return i < t.size() ? t[i] : 0.0f;
}

float share_of(int permille) { return static_cast<float>(permille) / 1000.0f; }

const char* k_private = "A rival's condition and stocks are not seen.";
const char* k_unseen  = "Not seen: shown once met in a battle or revealed by recon.";

std::vector<drawn_stat_grid>& record()
{
    static std::vector<drawn_stat_grid> r;
    return r;
}

/// Lay @p cells into @p out as rows of at most three.
int rows_of(const stat_band& b) { return std::max(1, static_cast<int>((b.cells.size() + 2) / 3)); }

bool band_has_bar(const stat_band& b)
{
    for (const stat_cell& c : b.cells)
        if (c.bar >= 0.0f)
            return true;
    return false;
}

bool band_has_range(const stat_band& b)
{
    for (const stat_cell& c : b.cells)
        if (!c.range.empty())
            return true;
    return false;
}

constexpr float k_small   = 0.80f; // a range's type, against the value's
constexpr float k_bar_h   = 3.0f;
constexpr float k_row_gap = 3.0f;
constexpr float k_pad     = 4.0f;

float row_h(const stat_band& b)
{
    const float line = ImGui::GetTextLineHeight();
    float h = line + 2.0f * k_pad;
    if (band_has_bar(b))   h += k_bar_h + 2.0f;
    if (band_has_range(b)) h += line * k_small;
    return h;
}

} // namespace

// ---- The draw ----------------------------------------------------------------

float stat_grid_height(const stat_bands& bands)
{
    float h = 0.0f;
    for (const stat_band& b : bands)
        h += static_cast<float>(rows_of(b)) * (row_h(b) + k_row_gap);
    return std::max(0.0f, h - k_row_gap);
}

void draw_stat_grid(const char* id, const stat_bands& bands, float width)
{
    drawn_stat_grid rec;
    rec.id    = id;
    rec.frame = ImGui::GetFrameCount();

    ImDrawList*  dl    = ImGui::GetWindowDrawList();
    ImFont*      font  = ImGui::GetFont();
    const float  fs    = ImGui::GetFontSize();
    const float  line  = ImGui::GetTextLineHeight();
    const ImVec2 o     = ImGui::GetCursorScreenPos();
    const float  gap   = 4.0f;

    float label_w = 0.0f;
    for (const stat_band& b : bands)
        label_w = std::max(label_w, ImGui::CalcTextSize(b.name).x);
    label_w += 8.0f;
    const float cell_w = std::max(30.0f, (width - label_w - 2.0f * gap) / 3.0f);

    ImGui::PushID(id);
    float y = o.y;
    int   idx = 0;
    for (const stat_band& b : bands)
    {
        const float rh = row_h(b);
        // The band's name at its left, level with its first row.
        dl->AddText({o.x, y + k_pad}, palette::text_secondary, b.name);
        for (std::size_t i = 0; i < b.cells.size(); ++i)
        {
            const stat_cell& c   = b.cells[i];
            const int        row = static_cast<int>(i / 3), col = static_cast<int>(i % 3);
            const ImVec2 p0 = {o.x + label_w + static_cast<float>(col) * (cell_w + gap),
                               y + static_cast<float>(row) * (rh + k_row_gap)};
            const ImVec2 p1 = {p0.x + cell_w, p0.y + rh};

            ImGui::SetCursorScreenPos(p0);
            ImGui::PushID(idx++);
            ImGui::InvisibleButton("##cell", {cell_w, rh});
            const bool hot = ImGui::IsItemHovered();
            ImGui::PopID();

            dl->AddRectFilled(p0, p1, hot ? IM_COL32(40, 44, 56, 255) : IM_COL32(26, 28, 36, 255), 3.0f);
            dl->PushClipRect(p0, p1, true); // a value never bleeds into its neighbour
            const bool  dash = c.value == k_dash;
            const ImU32 ink  = dash ? palette::neutral : palette::selection;
            const float gr   = line * 0.42f;
            icons::stat(dl, {p0.x + k_pad + gr, p0.y + k_pad + line * 0.5f}, gr, c.glyph,
                        dash ? palette::neutral : palette::text_secondary);

            // The value right-aligned; a delta right of it, in its direction's colour.
            float right = p1.x - k_pad;
            const float icon_right = p0.x + k_pad + 2.0f * gr + 2.0f; // the value stops at the glyph
            if (!c.delta.empty())
            {
                const float dw = ImGui::CalcTextSize(c.delta.c_str()).x;
                dl->AddText({right - dw, p0.y + k_pad}, c.delta_good ? palette::positive : palette::negative,
                            c.delta.c_str());
                right -= dw + 4.0f;
            }
            const float vw = ImGui::CalcTextSize(c.value.c_str()).x;
            dl->AddText({std::max(right - vw, icon_right), p0.y + k_pad}, ink, c.value.c_str());

            float ty = p0.y + k_pad + line;
            if (c.bar >= 0.0f)
            {
                ty += 1.0f;
                const float x0 = p0.x + k_pad, x1 = p1.x - k_pad;
                dl->AddRectFilled({x0, ty}, {x1, ty + k_bar_h}, IM_COL32(60, 64, 74, 255), 1.0f);
                if (c.bar > 0.0f)
                    dl->AddRectFilled({x0, ty}, {x0 + (x1 - x0) * c.bar, ty + k_bar_h}, c.bar_col, 1.0f);
                ty += k_bar_h + 1.0f;
            }
            else if (band_has_bar(b))
                ty += k_bar_h + 2.0f;
            if (!c.range.empty())
            {
                const float  sz = fs * k_small;
                const ImVec2 rs = font->CalcTextSizeA(sz, FLT_MAX, 0.0f, c.range.c_str());
                dl->AddText(font, sz, {p1.x - k_pad - rs.x, ty}, palette::text_secondary, c.range.c_str());
            }
            dl->PopClipRect();

            if (hot)
            {
                ImGui::BeginTooltip();
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "%s", c.name);
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 22.0f);
                ImGui::TextUnformatted(c.what);
                if (!c.detail.empty())
                    ImGui::TextDisabled("%s", c.detail.c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            drawn_stat_cell dc;
            dc.band  = b.name;
            dc.stat  = c.name;
            dc.value = c.value;
            dc.range = c.range;
            dc.delta = c.delta;
            dc.bar   = c.bar >= 0.0f;
            dc.x     = (p0.x + p1.x) * 0.5f;
            dc.y     = (p0.y + p1.y) * 0.5f;
            rec.cells.push_back(std::move(dc));
        }
        y += static_cast<float>(rows_of(b)) * (rh + k_row_gap);
    }
    ImGui::PopID();

    // Advance the layout past the whole grid.
    ImGui::SetCursorScreenPos(o);
    ImGui::Dummy({width, std::max(1.0f, y - o.y - k_row_gap)});

    auto& r = record();
    for (drawn_stat_grid& g : r)
        if (g.id == rec.id)
        {
            g = std::move(rec);
            return;
        }
    r.push_back(std::move(rec));
}

const std::vector<drawn_stat_grid>& stat_grids_drawn() { return record(); }

// ---- The builders ------------------------------------------------------------

bool unit_seen_by_player(const world& w, entity_id unit)
{
    const auto it = w.units.find(unit);
    if (it == w.units.end())
        return false;
    if (is_player_side(w, it->second.owner))
        return true;
    // Revealed by recon: the last air day's reveal set (world::air_revealed, BL-1449) -
    // the player's air superiority or a recon wing over the unit's province.
    if (const entity_id me = player_nation(w); me != null_entity && w.air_revealed.count({me, unit}) != 0)
        return true;
    for (const active_battle& b : w.battles)
    {
        const bool att = std::binary_search(b.attacker_units.begin(), b.attacker_units.end(), unit);
        const bool def = std::binary_search(b.defender_units.begin(), b.defender_units.end(), unit);
        if ((att && is_player_side(w, b.defender)) || (def && is_player_side(w, b.attacker)))
            return true;
    }
    return false;
}

stat_bands unit_stat_bands(const world& w, const recipe_registry& reg, entity_id unit)
{
    return unit_group_stat_bands(w, reg, {unit});
}

stat_bands unit_group_stat_bands(const world& w, const recipe_registry& reg,
                                 const std::vector<entity_id>& group)
{
    acc hp, ready, morale, food, ammo, equip, soft, hard, def, brk, hardness, recon, stealth, speed;
    int est_sum = 0, men_sum = 0;
    bool own = false;
    for (const entity_id uid : group)
    {
        const auto it = w.units.find(uid);
        if (it == w.units.end())
            continue;
        const unit_component& u = it->second;
        if (is_player_side(w, u.owner))
        {
            own = true;
            const int est = unit_establishment(u);
            hp.add(u.count, est > 0 ? static_cast<double>(u.count) / est : 0.0);
            men_sum += u.count;
            est_sum += est;
            ready.add(u.readiness_permille, share_of(u.readiness_permille));
            morale.add(u.morale_permille, share_of(u.morale_permille));
            food.add(u.food_permille, share_of(u.food_permille));
            ammo.add(u.ammo_permille, share_of(u.ammo_permille));
            equip.add(u.equipment_permille, share_of(u.equipment_permille));
        }
        if (!unit_seen_by_player(w, uid))
            continue;
        const land_combat_stats st = unit_land_stats(u);
        soft.add(st.soft);
        hard.add(st.hard);
        def.add(st.defence);
        brk.add(st.breakthrough);
        hardness.add(st.hardness);
        recon.add(st.recon);
        stealth.add(st.stealth);
        speed.add(march_days(reg, unit_land_class(u)));
    }
    const bool  single = group.size() == 1;
    const char* none   = own ? "" : k_private;
    auto pick = [&](const spec& s, const acc& a, const char* why) {
        if (a.n == 0)
            return hidden(s, why);
        if (single)
        {
            stat_cell c = cell(s, a.sum, s.bar != 0 ? static_cast<float>(a.share) : -1.0f);
            return c;
        }
        return group_cell(s, a, why);
    };
    const char* unseen = k_unseen;

    stat_bands out;
    stat_band cond{"Condition", {pick(k_hp, hp, none), pick(k_readiness, ready, none), pick(k_morale, morale, none)}};
    if (own)
    {
        char buf[48];
        std::snprintf(buf, sizeof buf, single ? "%d of %d men" : "%d of %d men in all", men_sum, est_sum);
        cond.cells[0].detail = buf + (single ? std::string() : std::string(". ") + cond.cells[0].detail);
    }
    out.push_back(std::move(cond));
    out.push_back({"Supply", {pick(k_food, food, none), pick(k_ammo, ammo, none), pick(k_equipment, equip, none)}});
    out.push_back({"Combat", {pick(k_soft, soft, unseen), pick(k_hard, hard, unseen), pick(k_defence, def, unseen),
                              pick(k_break, brk, unseen), pick(k_hardness, hardness, unseen)}});
    out.push_back({"Field", {pick(k_recon, recon, unseen), pick(k_stealth, stealth, unseen), pick(k_speed, speed, unseen)}});
    return out;
}

stat_bands land_design_stat_bands(const recipe_registry& reg, land_class c,
                                  const land_combat_stats& shown, const land_combat_stats& live)
{
    stat_bands out;
    out.push_back({"Combat", {design_cell(k_soft, shown.soft, live.soft), design_cell(k_hard, shown.hard, live.hard),
                              design_cell(k_defence, shown.defence, live.defence),
                              design_cell(k_break, shown.breakthrough, live.breakthrough),
                              design_cell(k_hardness, shown.hardness, live.hardness)}});
    stat_cell spd = cell(k_speed, march_days(reg, c));
    spd.detail = "The class's own: kit does not change it.";
    out.push_back({"Field", {design_cell(k_recon, shown.recon, live.recon),
                             design_cell(k_stealth, shown.stealth, live.stealth), std::move(spd)}});
    return out;
}

stat_bands wing_stat_bands(const world& w, const recipe_registry& reg, entity_id wing)
{
    stat_bands out;
    const auto it = w.wings.find(wing);
    const bool mine = it != w.wings.end() && is_player_side(w, it->second.owner);
    if (!mine)
    {
        // A rival's wing (BL-068): nothing of its condition; no mechanism sees its
        // design yet, so every cell reads as unseen.
        out.push_back({"Condition", {hidden(k_air_hp, k_private), hidden(k_air_morale, k_private),
                                     hidden(k_air_supply, k_private)}});
        out.push_back({"Combat", {hidden(k_air_air, k_unseen), hidden(k_air_soft, k_unseen),
                                  hidden(k_air_hard, k_unseen), hidden(k_air_strat, k_unseen)}});
        out.push_back({"Field", {hidden(k_air_recon, k_unseen), hidden(k_air_range, k_unseen)}});
        return out;
    }
    const air_wing&      wg = it->second;
    const air_wing_stats st = wing_stats(reg, wg);
    const float hp_share = static_cast<float>(wg.hp_ppm) / static_cast<float>(k_wing_hp_full);
    stat_cell hp = cell(k_air_hp, static_cast<double>(st.hp) * hp_share, hp_share);
    {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%d of %d at full", static_cast<int>(std::lround(st.hp * hp_share)), st.hp);
        hp.detail = buf;
    }
    stat_cell morale = st.has_morale ? cell(k_air_morale, wg.morale_permille, share_of(wg.morale_permille))
                                     : hidden(k_air_morale, "No pilot: a drone has no morale.");
    out.push_back({"Condition", {std::move(hp), std::move(morale),
                                 cell(k_air_supply, wg.supply_factor_permille, share_of(wg.supply_factor_permille))}});
    out.push_back({"Combat", {cell(k_air_air, st.air), cell(k_air_soft, st.soft), cell(k_air_hard, st.hard),
                              cell(k_air_strat, st.strategic)}});
    out.push_back({"Field", {cell(k_air_recon, st.recon), cell(k_air_range, st.range)}});
    return out;
}

stat_bands ship_stat_bands(const world& w, const recipe_registry& reg, entity_id ship)
{
    stat_bands out;
    const auto it = w.ships.find(ship);
    const bool mine = it != w.ships.end() && is_player_side(w, it->second.owner);
    if (!mine)
    {
        // A rival's ship (BL-068): nothing of its condition, and no mechanism reads its design.
        out.push_back({"Condition", {hidden(k_sea_hp, k_private), hidden(k_sea_morale, k_private),
                                     hidden(k_sea_supply, k_private)}});
        out.push_back({"Combat", {hidden(k_sea_attack, k_unseen), hidden(k_sea_detect, k_unseen),
                                  hidden(k_sea_stealth, k_unseen)}});
        out.push_back({"Field", {hidden(k_sea_speed, k_unseen), hidden(k_sea_range, k_unseen)}});
        return out;
    }
    const sea_ship&       s  = it->second;
    const ship_live_stats st = ship_stats(reg, s);
    const float hp_share = static_cast<float>(s.hp_ppm) / static_cast<float>(k_ship_hp_full);
    stat_cell hp = cell(k_sea_hp, static_cast<double>(st.hp) * hp_share, hp_share);
    {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%d of %d at full", static_cast<int>(std::lround(st.hp * hp_share)), st.hp);
        hp.detail = buf;
    }
    out.push_back({"Condition", {std::move(hp), cell(k_sea_morale, s.morale_permille, share_of(s.morale_permille)),
                                 cell(k_sea_supply, s.supply_factor_permille, share_of(s.supply_factor_permille))}});
    out.push_back({"Combat", {cell(k_sea_attack, st.attack), cell(k_sea_detect, st.detection), cell(k_sea_stealth, st.stealth)}});
    const ship_class cls = ship_class_of(s);
    std::vector<stat_cell> field{cell(k_sea_speed, st.days_per_tile_permille / 1000.0),
                                 cell(k_sea_range, reg.military().sea.cls[static_cast<std::size_t>(cls)].range)};
    if (cls == ship_class::transport)
        field.push_back(cell(k_sea_cap, st.capacity));
    out.push_back({"Field", std::move(field)});
    return out;
}

stat_bands ship_design_stat_bands(const ship_live_stats& shown, const ship_live_stats& live, int range)
{
    stat_bands out;
    stat_cell hp = design_cell(k_sea_hp, shown.hp, live.hp);
    hp.detail = "At full strength.";
    out.push_back({"Condition", {std::move(hp)}});
    out.push_back({"Combat", {design_cell(k_sea_attack, shown.attack, live.attack),
                              design_cell(k_sea_detect, shown.detection, live.detection),
                              design_cell(k_sea_stealth, shown.stealth, live.stealth)}});
    stat_cell spd = cell(k_sea_speed, shown.days_per_tile_permille / 1000.0);
    std::vector<stat_cell> field{std::move(spd), cell(k_sea_range, range)};
    if (shown.capacity > 0 || live.capacity > 0)
        field.push_back(design_cell(k_sea_cap, shown.capacity, live.capacity));
    out.push_back({"Field", std::move(field)});
    return out;
}

stat_bands air_design_stat_bands(const air_combat_stats& shown, const air_combat_stats& live)
{
    stat_bands out;
    stat_cell hp = design_cell(k_air_hp, shown.hp, live.hp);
    hp.detail = "At full strength.";
    out.push_back({"Condition", {std::move(hp)}});
    out.push_back({"Combat", {design_cell(k_air_air, shown.air, live.air), design_cell(k_air_soft, shown.soft, live.soft),
                              design_cell(k_air_hard, shown.hard, live.hard),
                              design_cell(k_air_strat, shown.strategic, live.strategic)}});
    out.push_back({"Field", {design_cell(k_air_recon, shown.recon, live.recon),
                             design_cell(k_air_range, shown.range, live.range)}});
    return out;
}

} // namespace ui
