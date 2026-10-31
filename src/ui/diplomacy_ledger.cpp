#include "diplomacy_ledger.hpp"

#include "foldout_column.hpp"
#include "format.hpp"       // fmt::credits / fmt::percent
#include "nav_pane.hpp"     // close_all_panels — open_declaration takes the column
#include "player_side.hpp"  // owner_display_name
#include "presentation.hpp"

#include "world/embargo.hpp"         // embargoes — the Embargo / Lift toggle
#include "world/building_profit.hpp" // estimate_building_profit — the GDP read (seat_screen's)
#include "world/fronts.hpp"          // current_fronts — front strength
#include "world/province.hpp"        // province_holder_for
#include "world/sentiment.hpp"
#include "world/stance.hpp"
#include "world/supply_system.hpp"   // dispatch_market_price — a convoy's value
#include "world/unit_roster.hpp"     // unit_strength
#include "world/war.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ui {

namespace {

constexpr std::size_t k_trend_samples = 5; ///< Now and four ticks back (diplomacy.md, open question 1).
constexpr std::size_t k_trade_window  = 4; ///< Ticks of landings the exposure reads.

ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

const char* name_of(const world& w, entity_id n)
{
    const char* s = owner_display_name(w, n);
    return (s != nullptr && s[0] != '\0') ? s : "Unknown nation";
}

/// The player's nation: the seat once the world is folded (every campaign start),
/// else the actor `player_actor` resolves.
entity_id me_of(const world& w)
{
    return w.nations.count(w.player_entity) != 0 ? w.player_entity : player_actor(w);
}

/// The nation whose ground market @p market's centre stands on (the trade
/// emitter's own reading), or null.
entity_id market_nation_of(const world& w, entity_id market)
{
    const auto m = w.markets.find(market);
    if (m == w.markets.end() || m->second.centre_tile == null_entity)
        return null_entity;
    const auto n = w.tile_to_nation.find(m->second.centre_tile);
    return n == w.tile_to_nation.end() ? null_entity : n->second;
}

/// Every nation that still holds ground, ascending id.
std::vector<entity_id> live_nations(const world& w)
{
    std::vector<entity_id> out;
    for (const auto& [id, n] : w.nations)
        if (!n.tiles.empty())
            out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

// ── Derived caches ─────────────────────────────────────────────────────────

/// Province -> political owner and nation -> provinces owned, re-derived when the
/// territory moves. Also clears the province name cache.
std::map<uint32_t, std::string> g_province_names;

void refresh_owners(const world& w, diplomacy_ledger_state& d)
{
    if (d.owners_generation == w.territory_generation && d.owners_catchment == w.catchment_generation
        && !d.province_owners.empty())
        return;
    d.owners_generation = w.territory_generation;
    d.owners_catchment  = w.catchment_generation;
    d.province_owners.clear();
    d.provinces_owned.clear();
    g_province_names.clear();
    for (const province& pr : w.provinces.provinces)
    {
        const entity_id o = province_owner(w, pr.id);
        if (o == null_entity)
            continue;
        d.province_owners[pr.id] = o;
        ++d.provinces_owned[o];
    }
    d.candidates_generation = ~std::uint64_t{0}; // the goal candidates follow the territory
}

/// Where a province is, in the glossary's words: the population centre it holds
/// (a province has no name of its own — GLOSSARY.md § Province), "Province N"
/// where none stands in it. The Military ledger's reading.
const std::string& province_label(const world& w, uint32_t pid)
{
    const auto hit = g_province_names.find(pid);
    if (hit != g_province_names.end())
        return hit->second;
    entity_id best = null_entity;
    for (const auto& [cid, ctile] : w.population_centre_tile)
        if (w.provinces.province_of(ctile) == pid && (best == null_entity || cid < best))
            best = cid;
    std::string label;
    if (best != null_entity)
        if (const auto nm = w.population_centre_name.find(best);
            nm != w.population_centre_name.end() && !nm->second.empty())
            label = nm->second;
    if (label.empty())
        label = "Province " + std::to_string(pid);
    return g_province_names.emplace(pid, std::move(label)).first->second;
}

std::vector<uint32_t> provinces_of(const diplomacy_ledger_state& d, entity_id nation)
{
    std::vector<uint32_t> out;
    for (const auto& [pid, o] : d.province_owners)
        if (o == nation)
            out.push_back(pid);
    return out; // ascending: the map's order
}

/// GDP per nation per year — seat_screen's reading (NATIONS.md § GDP): the last
/// cleared quarter's output of every building on the nation's ground at its
/// market's price, times four. Once per econ tick, only while the ledger draws.
void refresh_gdp(const world& w, const recipe_registry& reg, const economy_report& rep,
                 diplomacy_ledger_state& d)
{
    if (d.gdp_tick == w.current_econ_tick)
        return;
    d.gdp_tick = w.current_econ_tick;
    d.gdp_year.clear();
    for (const auto& [bid, b] : w.buildings)
    {
        const auto n = w.tile_to_nation.find(b.tile);
        if (n == w.tile_to_nation.end())
            continue;
        const building_profit p = estimate_building_profit(w, reg, rep, bid);
        if (!p.has_data)
            continue;
        d.gdp_year[n->second] += static_cast<double>(p.revenue) * 4.0; // a tick is a quarter
    }
}

// ── The UI-side history (not saved; the world keeps none) ───────────────────

void sample_history(const world& w, diplomacy_ledger_state& d)
{
    const int tick = w.current_econ_tick;
    if (tick == d.sampled_tick)
        return;
    if (tick < d.sampled_tick) // a load or a new world: the window restarts
    {
        d.trend.clear();
        d.convoys_seen.clear();
        d.landed.clear();
    }
    const bool had_sample = d.sampled_tick >= 0 && tick > d.sampled_tick;
    const int  prev_tick  = d.sampled_tick;
    d.sampled_tick = tick;

    const entity_id me = me_of(w);
    for (const entity_id n : live_nations(w))
    {
        if (n == me)
            continue;
        for (const auto& pr : {std::make_pair(n, me), std::make_pair(me, n)})
        {
            std::vector<sentiment_value>& v = d.trend[pr];
            v.push_back(sentiment_toward(w.sentiment, pr.first, pr.second));
            if (v.size() > k_trend_samples)
                v.erase(v.begin());
        }
    }

    // Landings: a convoy seen in flight last sample and gone now has landed — or
    // was cut. The live tick surfaces no cut record to the UI, so a vanished
    // convoy counts as landed only if it was DUE (its progress would reach the end
    // within the ticks since the sample: a cut mid-route was not) and its
    // destination market still stands on the nation recorded (a border that moved
    // mid-flight makes it some other pair's trade, or none).
    const int elapsed = std::max(1, tick - prev_tick);
    std::map<std::uint32_t, diplomacy_ledger_state::convoy_seen> now;
    for (const convoy_component& c : w.convoys)
    {
        const entity_id from = owner_home_nation(w, c.corp);
        const entity_id to   = market_nation_of(w, c.dest_market);
        if (from == null_entity || to == null_entity || from == to)
            continue;
        const auto m = w.markets.find(c.dest_market);
        const float price = m == w.markets.end()
            ? 0.0f : dispatch_market_price(m->second, static_cast<std::size_t>(c.cargo_resource));
        const float v = c.cargo_qty * price;
        if (std::isfinite(v) && v > 0.0f)
            now[c.id] = {from, to, c.dest_market, v, c.progress + c.speed >= 1.0f - 1e-4f};
    }
    if (had_sample)
    {
        std::map<std::pair<entity_id, entity_id>, float> landed;
        for (const auto& [id, seen] : d.convoys_seen)
        {
            if (now.find(id) != now.end())
                continue;
            // `due` is judged one tick ahead; over a longer gap (ticks run faster
            // than frames) any convoy may have arrived, so it is not judged.
            if (!seen.due && elapsed <= 1)
                continue; // gone before it could arrive: cut, not landed
            if (market_nation_of(w, seen.dest) != seen.to)
                continue; // the destination changed nation in flight
            landed[{seen.from, seen.to}] += seen.value;
        }
        d.landed.push_back(std::move(landed));
        if (d.landed.size() > k_trade_window)
            d.landed.erase(d.landed.begin());
    }
    d.convoys_seen = std::move(now);
}

/// The change over the trend window, per dimension; false with no window yet.
bool trend_of(const diplomacy_ledger_state& d, entity_id obs, entity_id subj, sentiment_value& delta)
{
    const auto it = d.trend.find({obs, subj});
    if (it == d.trend.end() || it->second.size() < 2)
        return false;
    const sentiment_value& a = it->second.front();
    const sentiment_value& b = it->second.back();
    delta.access = b.access - a.access;
    delta.trust  = b.trust - a.trust;
    delta.alarm  = b.alarm - a.alarm;
    return true;
}

// ── Drawing primitives ─────────────────────────────────────────────────────

const char* band_word(float v, const sentiment_params& p)
{
    switch (band_of(v, p))
    {
    case sentiment_band::adverse:    return "adverse";
    case sentiment_band::wary:       return "wary";
    case sentiment_band::neutral:    return "neutral";
    case sentiment_band::favourable: return "favourable";
    case sentiment_band::trusted:    return "trusted";
    }
    return "neutral";
}

/// The war context a sentiment row can be read back to: the open war between the
/// pair, else the last one closed. Empty when the pair has none.
std::string war_context(const world& w, entity_id a, entity_id b)
{
    char line[200];
    if (const war_record* war = war_between(w, a, b))
    {
        std::snprintf(line, sizeof line, "At war: %s declared on %s, tick %d.",
                      name_of(w, war->declarer), name_of(w, war->defender), war->opened_tick);
        return line;
    }
    for (auto it = w.war_history.rbegin(); it != w.war_history.rend(); ++it)
        if ((it->declarer == a && it->defender == b) || (it->declarer == b && it->defender == a))
        {
            const char* how = it->end == war_end::white_peace     ? "a white peace"
                            : it->end == war_end::cession         ? "a cession"
                            : it->end == war_end::imposed_white   ? "an imposed white peace"
                                                                  : "an imposed cession";
            std::snprintf(line, sizeof line, "Last war: %s on %s, ticks %d-%d, ended in %s.",
                          name_of(w, it->declarer), name_of(w, it->defender), it->opened_tick,
                          it->closed_tick, how);
            return line;
        }
    return {};
}

/// One signed bar: a track over [-limit, limit] with a centre tick, the value's
/// fill from the centre, the figure after it and a trend arrow. @p bad_high says
/// positive is the bad side (Alarm). Hover names the value, band, change and war.
void sentiment_bar(const char* label, float v, bool has_trend, float delta, bool bad_high,
                   const sentiment_params& p, const std::string& context)
{
    // One label column wide enough for the longest label, from wherever the row
    // starts (the expanded block is indented), so the bar never runs into it.
    const float lx = ImGui::GetCursorPosX();
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine(lx + ImGui::CalcTextSize("Access").x + 10.0f);
    const float  h  = ImGui::GetTextLineHeight() * 0.62f;
    const float  bw = std::max(60.0f, ImGui::GetContentRegionAvail().x - 74.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float  y0 = p0.y + (ImGui::GetTextLineHeight() - h) * 0.5f;
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const float  cx = p0.x + bw * 0.5f;
    dl->AddRectFilled({p0.x, y0}, {p0.x + bw, y0 + h}, palette::sentiment_track, 2.0f);
    const float lim  = p.limit > 0.0f ? p.limit : 100.0f;
    const float frac = std::clamp(v / lim, -1.0f, 1.0f);
    const bool  good = bad_high ? v < 0.0f : v > 0.0f;
    const ImU32 fill = bad_high ? (v > 0.0f ? palette::sentiment_alarm : palette::positive)
                                : (good ? palette::positive : palette::negative);
    if (frac != 0.0f)
    {
        // A sliver stays visible for any non-zero value, so a small grudge reads.
        float x1 = cx + frac * bw * 0.5f;
        if (std::fabs(x1 - cx) < 2.0f) x1 = cx + (frac > 0.0f ? 2.0f : -2.0f);
        dl->AddRectFilled({std::min(cx, x1), y0}, {std::max(cx, x1), y0 + h}, fill, 1.0f);
    }
    dl->AddLine({cx, y0 - 2.0f}, {cx, y0 + h + 2.0f}, palette::text_secondary, 1.0f);
    ImGui::InvisibleButton(label, {bw, ImGui::GetTextLineHeight()});
    const bool hovered = ImGui::IsItemHovered();
    ImGui::SameLine();
    ImGui::Text("%+.1f", v);
    if (has_trend && std::fabs(delta) >= 0.05f)
    {
        ImGui::SameLine();
        const ImVec2 a  = ImGui::GetCursorScreenPos();
        const float  th = ImGui::GetTextLineHeight();
        const float  r  = th * 0.28f;
        const ImVec2 c{a.x + r + 1.0f, a.y + th * 0.5f};
        const bool   up = delta > 0.0f;
        const bool   better = bad_high ? !up : up;
        const ImU32  ac = better ? palette::positive : palette::negative;
        if (up) dl->AddTriangleFilled({c.x - r, c.y + r * 0.7f}, {c.x + r, c.y + r * 0.7f}, {c.x, c.y - r}, ac);
        else    dl->AddTriangleFilled({c.x - r, c.y - r * 0.7f}, {c.x + r, c.y - r * 0.7f}, {c.x, c.y + r}, ac);
        ImGui::Dummy({r * 2.0f + 2.0f, th});
    }
    if (hovered)
    {
        char head[96];
        std::snprintf(head, sizeof head, "%s %+.1f (%s)", label, v, band_word(v, p));
        std::string tip = head;
        if (has_trend)
        {
            char ch[64];
            std::snprintf(ch, sizeof ch, "\n%+.1f over the last four ticks", delta);
            tip += ch;
        }
        if (!context.empty())
            tip += "\n" + context;
        ImGui::SetTooltip("%s", tip.c_str());
    }
}

/// The three bars @p obs feels toward @p subj.
void sentiment_block(const world& w, const diplomacy_ledger_state& d, const sentiment_params& p,
                     entity_id obs, entity_id subj, const std::string& context)
{
    const sentiment_value v = sentiment_toward(w.sentiment, obs, subj);
    sentiment_value dt;
    const bool has = trend_of(d, obs, subj, dt);
    // Two pushes, not one product: an id product overflows a signed int.
    ImGui::PushID(static_cast<int>(obs));
    ImGui::PushID(static_cast<int>(subj));
    sentiment_bar("Access", v.access, has, dt.access, false, p, context);
    sentiment_bar("Trust",  v.trust,  has, dt.trust,  false, p, context);
    sentiment_bar("Alarm",  v.alarm,  has, dt.alarm,  true,  p, context);
    ImGui::PopID();
    ImGui::PopID();
}

/// A stance chip: a small framed word.
void chip(const char* text, ImU32 colour)
{
    const ImVec2 ts  = ImGui::CalcTextSize(text);
    const ImVec2 p0  = ImGui::GetCursorScreenPos();
    const float  pad = 3.0f;
    ImDrawList*  dl  = ImGui::GetWindowDrawList();
    dl->AddRect(p0, {p0.x + ts.x + pad * 2.0f, p0.y + ts.y}, colour, 3.0f);
    dl->AddText({p0.x + pad, p0.y}, colour, text); // fit-exempt: the chip is sized to its measured text
    ImGui::Dummy({ts.x + pad * 2.0f, ts.y});
}

/// The row expander (DRILL_THROUGH.md: ⌄ becomes ⌃ while open), right-aligned so
/// every row's control sits in one column. Returns true when pressed.
bool expander(bool open, float& cx, float& cy)
{
    const float h     = ImGui::GetFrameHeight();
    const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    ImGui::SameLine(right - h);
    const bool clicked = ImGui::InvisibleButton("##expand", {h, h});
    const bool hot     = ImGui::IsItemHovered();
    const ImVec2 mn    = ImGui::GetItemRectMin();
    cx = mn.x + h * 0.5f;
    cy = mn.y + h * 0.5f;
    const float r  = h * 0.28f;
    const ImU32 c  = hot ? palette::hover : palette::text_secondary;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (open) dl->AddTriangleFilled({cx - r, cy + r * 0.5f}, {cx + r, cy + r * 0.5f}, {cx, cy - r * 0.6f}, c);
    else      dl->AddTriangleFilled({cx - r, cy - r * 0.5f}, {cx + r, cy - r * 0.5f}, {cx, cy + r * 0.6f}, c);
    if (hot)
        ImGui::SetTooltip(open ? "Collapse" : "Expand here");
    return clicked;
}

/// A press that may be disabled; its reason is the tooltip (never ledger text —
/// the Military ledger's rule, Ben 2026-10-30). Publishes it for verify.
bool press(diplomacy_ledger_state& d, const char* label, const char* kind, entity_id nation,
           bool enabled, const std::string& reason, const char* tip = nullptr)
{
    if (!enabled) ImGui::BeginDisabled();
    const bool clicked = ImGui::Button(label);
    if (!enabled) ImGui::EndDisabled();
    const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
    d.presses.push_back({kind, nation, (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, enabled, reason});
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        if (!enabled && !reason.empty())     ImGui::SetTooltip("%s", reason.c_str());
        else if (tip != nullptr)             ImGui::SetTooltip("%s", tip);
    }
    return clicked && enabled;
}

corp_command command_for(const world& w, corp_verb verb, entity_id other)
{
    corp_command c;
    c.corp         = w.player_entity;
    c.verb         = verb;
    c.counterparty = other;
    c.air_provinces = no_air_provinces();
    return c;
}

void put_provinces(corp_command& c, const std::vector<uint32_t>& provs)
{
    c.air_provinces = no_air_provinces();
    for (std::size_t i = 0; i < provs.size() && i < c.air_provinces.size(); ++i)
        c.air_provinces[i] = provs[i];
}

/// "Kessar, Province 41 (held by you)" — a goal list with each holder.
std::string goals_line(const world& w, const std::vector<uint32_t>& goals, entity_id me)
{
    if (goals.empty())
        return "none named";
    std::string s;
    for (std::size_t i = 0; i < goals.size(); ++i)
    {
        if (i > 0) s += ", ";
        s += province_label(w, goals[i]);
        const entity_id h = province_holder_for(w, goals[i]);
        if (h != null_entity)
            s += std::string(" (") + (h == me ? "you" : name_of(w, h)) + ")";
    }
    return s;
}

// ── The declaration's preview cache ─────────────────────────────────────────

/// `declaration_price` and `war_justifications` walk every tile (the border test,
/// `nations_bordering`), so they run once per change, not every frame. The key is
/// everything they read that can move under an open declaration: the pair and
/// goals, the econ tick (sentiment folds once per tick), the territory and
/// catchment counters, and the sizes of the embargo, war, history and cession
/// tables (a press mid-tick moves those). Never saved; cleared by
/// `reset_diplomacy_ui`.
struct declaration_preview
{
    bool                  set = false;
    entity_id             me = null_entity, target = null_entity;
    std::vector<uint32_t> goals;
    int                   tick = 0;
    std::uint64_t         territory = 0, catchment = 0;
    std::size_t           embargoes = 0, wars = 0, history = 0, cessions = 0;
    war_price             price;
    std::array<bool, k_war_justification_count> just{};
};
declaration_preview g_preview;

/// The expanded row's front strengths, recomputed once a day (draw_expanded).
/// Cleared by `reset_diplomacy_ui`: its key can repeat across worlds.
struct front_read
{
    entity_id me = null_entity, them = null_entity;
    int tick = -1, day = -1;
    std::uint64_t territory = 0;
    std::size_t units = 0;
    bool border_me = false, border_them = false;
    int64_t f_me = 0, f_them = 0;
};
front_read g_fronts;

const declaration_preview& cached_preview(const world& w, const sentiment_params& sp, entity_id me,
                                          entity_id target, const std::vector<uint32_t>& goals)
{
    declaration_preview& c = g_preview;
    if (c.set && c.me == me && c.target == target && c.goals == goals && c.tick == w.current_econ_tick
        && c.territory == w.territory_generation && c.catchment == w.catchment_generation
        && c.embargoes == w.nation_embargoes.size() && c.wars == w.wars.size()
        && c.history == w.war_history.size() && c.cessions == w.province_cessions.size())
        return c;
    c.set       = true;
    c.me        = me;
    c.target    = target;
    c.goals     = goals;
    c.tick      = w.current_econ_tick;
    c.territory = w.territory_generation;
    c.catchment = w.catchment_generation;
    c.embargoes = w.nation_embargoes.size();
    c.wars      = w.wars.size();
    c.history   = w.war_history.size();
    c.cessions  = w.province_cessions.size();
    c.price     = declaration_price(w, sp, me, target, goals);
    c.just      = war_justifications(w, sp, me, target, goals);
    return c;
}

// ── The declaration ────────────────────────────────────────────────────────

void refresh_candidates(diplomacy_ledger_state& d)
{
    if (d.declare_target == null_entity)
    {
        d.goal_candidates.clear();
        return;
    }
    if (d.candidates_generation == d.owners_generation && !d.goal_candidates.empty())
        return;
    d.candidates_generation = d.owners_generation;
    d.goal_candidates = provinces_of(d, d.declare_target);
    // A goal whose province is no longer the target's drops out of the pick.
    d.goals.erase(std::remove_if(d.goals.begin(), d.goals.end(), [&](uint32_t g) {
                      return !std::binary_search(d.goal_candidates.begin(), d.goal_candidates.end(), g);
                  }),
                  d.goals.end());
}

void draw_declaration(const world& w, const recipe_registry& reg, ui_state& s, entity_id me)
{
    diplomacy_ledger_state& d = s.diplomacy;
    const entity_id target = d.declare_target;
    const bool answer = d.declare_answer;
    const sentiment_params& sp = reg.sentiment();

    ImGui::Separator();
    if (d.scroll_to_decl)
    {
        ImGui::SetScrollHereY(0.0f); // a just-opened declaration's head to the column top
        d.scroll_to_decl = false;
        d.scroll_to_row  = false;
    }
    ImGui::TextColored(col4(palette::war_goal_chosen), answer ? "Answer %s" : "Declare war on %s",
                       name_of(w, target));
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(answer ? "Name up to three of their provinces as your goals. Answering is free."
                                 : "Click their provinces on the map to choose up to three goals.");

    // Goals.
    ImGui::Text("Goals (%d of %d)", static_cast<int>(d.goals.size()), static_cast<int>(k_war_goals_max));
    if (d.goals.empty())
        ImGui::TextDisabled("  Click one of their provinces on the map.");
    for (std::size_t i = 0; i < d.goals.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Bullet();
        ImGui::SameLine();
        ImGui::TextUnformatted(province_label(w, d.goals[i]).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("x"))
        {
            d.goals.erase(d.goals.begin() + static_cast<long>(i));
            ImGui::PopID();
            break;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Remove this goal (or click the province again).");
        ImGui::PopID();
    }

    std::vector<uint32_t> goals = d.goals;
    std::sort(goals.begin(), goals.end());
    d.decl_goals      = static_cast<int>(goals.size());
    d.decl_candidates = static_cast<int>(d.goal_candidates.size());

    bool        can_send = !goals.empty();
    std::string reason   = goals.empty() ? "Choose at least one goal on the map." : std::string();

    // The decision row. Drawn directly under the summed price (so at the 1280x720
    // floor it is on screen without scrolling past the per-nation table), or at the
    // foot when there is no price to show. Returns true when the flow closed.
    bool decided = false;
    const auto decide = [&]() -> bool {
        decided = true;
        if (press(d, answer ? "Answer" : "Declare", answer ? "answer" : "declare", target, can_send, reason))
        {
            if (!d.attack_units.empty() && d.attack_tile != null_entity)
            {
                // A declare-and-attack: app's unit-order block checks every march first,
                // files the declaration (or the answer) with these goals, then marches.
                s.pending_unit_order_units   = d.attack_units;
                s.pending_unit_order_tile    = d.attack_tile;
                s.pending_unit_order_declare = target;
                s.pending_unit_order_attack  = true;
                s.pending_unit_order_goals   = goals;
            }
            else
            {
                corp_command c = command_for(w, answer ? corp_verb::answer_war : corp_verb::declare_war, target);
                put_provinces(c, goals);
                d.pending.push_back(c);
            }
            close_declaration(s);
            return true;
        }
        ImGui::SameLine();
        if (press(d, "Cancel", "cancel", target, true, {}, "Close the declaration. Nothing is sent (Escape too)."))
        {
            close_declaration(s);
            return true;
        }
        return false;
    };

    if (!answer)
    {
        // Justifications — every opening row, marked holding or not.
        const declaration_preview& pv = cached_preview(w, sp, me, target, goals);
        const auto& just = pv.just;
        ImGui::Text("Justifications");
        for (std::size_t i = 0; i < k_war_justification_count; ++i)
        {
            // The row's authored id ("lost_ground") in words: "Lost ground".
            std::string nm = war_justification_rows()[i].name;
            std::replace(nm.begin(), nm.end(), '_', ' ');
            if (!nm.empty()) nm[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(nm[0])));
            if (just[i]) ImGui::TextColored(col4(palette::positive), "  + %s", nm.c_str());
            else         ImGui::TextDisabled("  - %s", nm.c_str());
        }

        // The price — what declare_war will charge, summed and per neighbour.
        const war_price& price = pv.price;
        d.decl_valid     = price.valid;
        d.decl_justified = price.justified;
        d.decl_trust     = price.sum_trust;
        d.decl_alarm     = price.sum_alarm;
        d.decl_observers = 0;
        ImGui::Text("Price");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The Trust every other nation loses toward you and the Alarm it gains. A "
                              "justification that holds halves it; neighbours watch twice as hard.");
        if (!goals.empty() && !price.valid)
        {
            can_send = false;
            reason   = truce_between(w, me, target) ? "A truce stands."
                     : war_between(w, me, target) != nullptr ? "A war already stands between you."
                                                              : "The declaration would be refused.";
        }
        if (price.valid)
        {
            ImGui::TextColored(col4(palette::negative), "  Trust %+.1f   Alarm %+.1f  (all nations)",
                               price.sum_trust, price.sum_alarm);
            if (price.justified)
                ImGui::TextColored(col4(palette::positive), "  Justified: the price is halved.");
            if (!price.charged)
                ImGui::TextDisabled("  Nothing is charged while the world settles.");
            if (decide())
                return;
            // Per neighbour: the war_declared rows, bordering ones first, then the largest.
            std::vector<const war_price_row*> rows;
            for (const war_price_row& r : price.rows)
                if (r.kind == sentiment_factor_kind::war_declared)
                    rows.push_back(&r);
            d.decl_observers = static_cast<int>(rows.size());
            std::stable_sort(rows.begin(), rows.end(), [](const war_price_row* a, const war_price_row* b) {
                if (a->borders != b->borders) return a->borders;
                return std::fabs(a->trust) > std::fabs(b->trust);
            });
            // A table, so a long nation name never pushes its figures off the column:
            // the name column stretches and clips, the figures keep their width.
            const std::size_t shown = std::min<std::size_t>(rows.size(), 6);
            constexpr ImGuiTableFlags tf = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
            if (ImGui::BeginTable("##price", 3, tf, {ImGui::GetContentRegionAvail().x, 0.0f}))
            {
                ImGui::TableSetupColumn("Nation", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Trust", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("-00.0").x);
                ImGui::TableSetupColumn("Alarm", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("+000.0").x);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::TextDisabled("  Nation");
                ImGui::TableNextColumn(); ImGui::TextDisabled("Trust");
                ImGui::TableNextColumn(); ImGui::TextDisabled("Alarm");
                const auto line = [&](const char* nm, const char* tag, float t, float a) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextDisabled("  %s%s", nm, tag);
                    ImGui::TableNextColumn(); ImGui::Text("%+.1f", t);
                    ImGui::TableNextColumn(); ImGui::Text("%+.1f", a);
                };
                for (const war_price_row& r : price.rows)
                    if (r.kind == sentiment_factor_kind::hostility_declared)
                        line(name_of(w, r.observer), " (target)", r.trust, r.alarm);
                for (std::size_t i = 0; i < shown; ++i)
                    line(name_of(w, rows[i]->observer), rows[i]->borders ? " *" : "", rows[i]->trust, rows[i]->alarm);
                ImGui::EndTable();
            }
            if (rows.size() > shown)
                ImGui::TextDisabled("  and %d more", static_cast<int>(rows.size() - shown));
            if (ImGui::IsItemHovered() || ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("* a neighbour of you or of %s: it watches twice as hard.", name_of(w, target));
        }
    }
    else
    {
        d.decl_valid = true;
        ImGui::TextDisabled("Answering is free.");
    }

    if (!decided)
        decide();
}

// ── Nations ─────────────────────────────────────────────────────────────────

struct nation_row
{
    entity_id id = null_entity;
    bool hostile_out = false, hostile_in = false, friends = false, offer_out = false, offer_in = false;
    bool at_war = false, truce = false;
    sentiment_value toward_me;
};

/// Summed unit strength of @p nation's units standing on its fronts toward @p other;
/// false when the pair shares no front.
bool front_strength(const world& w, entity_id nation, entity_id other, int64_t& out)
{
    out = 0;
    std::set<entity_id> span;
    for (const front& f : current_fronts(w, nation))
        if (f.neighbour == other)
            span.insert(f.span.begin(), f.span.end());
    if (span.empty())
        return false;
    for (const auto& [uid, u] : w.units)
        if (u.owner == nation && span.count(u.position) != 0)
            out += unit_strength(w, u);
    return true;
}

void draw_expanded(const world& w, const recipe_registry& reg, const std::vector<corp_standing>& standings,
                   ui_state& s, const nation_row& r, entity_id me)
{
    diplomacy_ledger_state& d = s.diplomacy;
    const sentiment_params& sp = reg.sentiment();
    const entity_id them = r.id;
    ImGui::Indent(8.0f);

    // Your sentiment toward them.
    ImGui::TextDisabled("You toward them");
    sentiment_block(w, d, sp, me, them, war_context(w, me, them));

    // Power — public figures only (reach, market share, GDP); never a treasury.
    ImGui::TextDisabled("Power  (them / you)");
    const corp_standing* st_them = nullptr;
    const corp_standing* st_me   = nullptr;
    for (const corp_standing& c : standings)
    {
        if (c.corp == them) st_them = &c;
        if (c.corp == me)   st_me   = &c;
    }
    ImGui::Text("  Reach %d / %d bodies", st_them ? st_them->reach_bodies : 0, st_me ? st_me->reach_bodies : 0);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Bodies with at least one of the nation's buildings.");
    ImGui::Text("  Market share %s / %s", fmt::percent(st_them ? st_them->market_share : 0.0).c_str(),
                fmt::percent(st_me ? st_me->market_share : 0.0).c_str());
    const auto g_them = d.gdp_year.find(them), g_me = d.gdp_year.find(me);
    ImGui::Text("  GDP %s / %s a year",
                g_them == d.gdp_year.end() ? "-" : fmt::credits(g_them->second).c_str(),
                g_me == d.gdp_year.end() ? "-" : fmt::credits(g_me->second).c_str());
    // Fronts and unit positions move at most once a day: recompute per day, per
    // row, not per frame (`current_fronts` re-derives when its cache is stale).
    front_read& fr = g_fronts;
    if (fr.me != me || fr.them != them || fr.tick != w.current_econ_tick
        || fr.day != w.construction_quarter_day || fr.territory != w.territory_generation
        || fr.units != w.units.size())
    {
        fr.me = me; fr.them = them; fr.tick = w.current_econ_tick; fr.day = w.construction_quarter_day;
        fr.territory = w.territory_generation; fr.units = w.units.size();
        fr.border_me   = front_strength(w, me, them, fr.f_me);
        fr.border_them = front_strength(w, them, me, fr.f_them);
    }
    const int64_t f_me = fr.f_me, f_them = fr.f_them;
    const bool border_me = fr.border_me, border_them = fr.border_them;
    if (border_me || border_them)
        ImGui::Text("  Front strength %lld / %lld", static_cast<long long>(f_them), static_cast<long long>(f_me));
    else
        ImGui::Text("  Front strength: no border");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The summed strength of the units each side stands on the fronts you share.");

    // Trade exposure — what a war would cut (hostile cargo is intercepted).
    double imp_all = 0.0, imp_them = 0.0, exp_all = 0.0, exp_them = 0.0;
    for (const auto& tick : d.landed)
        for (const auto& [pr, v] : tick)
        {
            if (pr.second == me) { imp_all += v; if (pr.first == them)  imp_them += v; }
            if (pr.first == me)  { exp_all += v; if (pr.second == them) exp_them += v; }
        }
    ImGui::TextDisabled("Trade exposure");
    if (d.landed.empty())
        ImGui::Text("  No ticks watched yet.");
    else
    {
        ImGui::Text("  Imports from them %s", imp_all > 0.0 ? fmt::percent(imp_them / imp_all).c_str() : "-");
        ImGui::Text("  Exports to them %s", exp_all > 0.0 ? fmt::percent(exp_them / exp_all).c_str() : "-");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Shares of the cargo landed between nations over the last %d ticks this "
                          "session has watched. A war cuts it: hostile cargo is intercepted.",
                          static_cast<int>(k_trade_window));

    // Friendship — offer, withdraw, accept.
    ImGui::Spacing();
    if (r.offer_in)
    {
        if (press(d, "Accept friendship", "accept_friendship", them, true, {}, "They offered friendship."))
            d.pending.push_back(command_for(w, corp_verb::accept_friendship, them));
    }
    else if (r.friends || r.offer_out)
    {
        if (press(d, "Withdraw", "withdraw", them, !r.at_war, "At war: only peace ends a war.",
                  r.friends ? "End the friendship." : "Withdraw your friendship offer."))
            d.pending.push_back(command_for(w, corp_verb::return_to_neutral, them));
    }
    else
    {
        const std::string why = r.at_war ? "At war: only peace ends a war."
                              : r.hostile_out ? "You are hostile toward them."
                                              : std::string();
        if (press(d, "Offer friendship", "offer_friendship", them, why.empty(), why))
            d.pending.push_back(command_for(w, corp_verb::offer_friendship, them));
    }
    ImGui::SameLine();
    // Embargo / Lift — a TOGGLE (diplomacy.md § 5): its state is visible (the
    // press reads "Lift" in the active colour while the embargo stands), and
    // pressing it while active repeals. Both go through `set_embargo`.
    {
        const bool on = embargoes(w, me, them);
        if (on)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        const bool hit = press(d, on ? "Lift" : "Embargo", on ? "lift" : "embargo", them, true, {},
                               on ? "Your embargo stands: none of their cargo lands in your markets. Press to lift it."
                                  : "Embargo them: none of their cargo lands in your markets.");
        if (on)
            ImGui::PopStyleColor();
        if (hit)
        {
            corp_command c = command_for(w, corp_verb::set_embargo, them);
            c.order = on ? 0u : 1u;
            d.pending.push_back(c);
        }
    }

    // Declare war / the war's score and Offer peace.
    if (r.at_war)
    {
        const war_record* war = war_between(w, me, them);
        const int sc = war ? war_score(w, *war) : 0;
        const int mine = (war && war->declarer == me) ? sc : -sc;
        ImGui::Text("War score %+d", mine);
        ImGui::SameLine();
        if (press(d, "Offer peace", "peace_open", them, true, {}, "Open the war in the Wars tab."))
        {
            d.view       = 1;
            d.peace_with = them;
            d.peace_gain.clear();
            d.peace_give.clear();
        }
    }
    else if (d.declare_target != them)
    {
        int left = 0;
        if (const auto t = w.truces.find(canonical_pair(me, them)); t != w.truces.end())
            left = t->second - w.current_econ_tick;
        const std::string why = r.truce ? "A truce stands for " + std::to_string(left) + " more ticks." : std::string();
        if (press(d, "Declare war", "declare_open", them, why.empty(), why,
                  "Choose goals on the map; the price shows before anything is sent."))
            open_declaration(w, s, them);
    }

    if (d.declare_target == them && !d.declare_answer)
        draw_declaration(w, reg, s, me);
    ImGui::Unindent(8.0f);
}

void draw_nations(const world& w, const recipe_registry& reg, const std::vector<corp_standing>& standings,
                  ui_state& s, entity_id me)
{
    diplomacy_ledger_state& d = s.diplomacy;
    const sentiment_params& sp = reg.sentiment();

    std::vector<nation_row> rows;
    for (const entity_id n : live_nations(w))
    {
        if (n == me)
            continue;
        nation_row r;
        r.id          = n;
        r.hostile_out = is_hostile(w, me, n);
        r.hostile_in  = is_hostile(w, n, me);
        r.friends     = are_friends(w, me, n);
        r.offer_out   = has_pending_friendship_offer(w, me, n);
        r.offer_in    = has_pending_friendship_offer(w, n, me);
        r.at_war      = at_war(w, me, n);
        r.truce       = truce_between(w, me, n);
        r.toward_me   = sentiment_toward(w.sentiment, n, me);
        rows.push_back(r);
    }
    // Hostile first, then by Alarm toward you, highest first; id breaks ties.
    std::stable_sort(rows.begin(), rows.end(), [](const nation_row& a, const nation_row& b) {
        const bool ha = a.hostile_out || a.hostile_in || a.at_war;
        const bool hb = b.hostile_out || b.hostile_in || b.at_war;
        if (ha != hb) return ha;
        if (a.toward_me.alarm != b.toward_me.alarm) return a.toward_me.alarm > b.toward_me.alarm;
        return a.id < b.id;
    });

    if (rows.empty())
        ImGui::TextDisabled("No other nation holds ground.");

    for (const nation_row& r : rows)
    {
        ImGui::PushID(static_cast<int>(r.id));
        const bool open = d.expanded == r.id;
        if (open && d.scroll_to_row)
        {
            // A just-expanded row's head to the column top — unless it already
            // sits in the top third, where its content has room (the first row
            // would otherwise scroll the tab strip away for nothing).
            const float y = ImGui::GetCursorScreenPos().y, top = ImGui::GetWindowPos().y;
            if (y < top || y > top + ImGui::GetWindowHeight() * 0.35f)
                ImGui::SetScrollHereY(0.0f);
            d.scroll_to_row = false;
        }

        // Name, then the chips: you -> them, them -> you.
        const ImVec2 sw = ImGui::GetCursorScreenPos();
        const float  th = ImGui::GetTextLineHeight();
        ImGui::GetWindowDrawList()->AddRectFilled({sw.x, sw.y + 2.0f}, {sw.x + 4.0f, sw.y + th - 2.0f},
                                                  palette::nation_colour(r.id));
        ImGui::Dummy({6.0f, th});
        ImGui::SameLine();
        ImGui::TextUnformatted(name_of(w, r.id));
        float ex = -1.0f, ey = -1.0f;
        if (expander(open, ex, ey))
        {
            d.expanded = open ? null_entity : r.id;
            d.scroll_to_row = !open;
            if (open && d.declare_target == r.id && !d.declare_answer)
                close_declaration(s); // folding the row leaves no trace of its declaration
        }

        if (r.at_war)             { chip("At war", palette::front_hostile); ImGui::SameLine(); }
        else if (r.truce)         { chip("Truce", palette::front_neutral); ImGui::SameLine(); }
        ImGui::TextDisabled("you:");
        ImGui::SameLine();
        if (r.hostile_out)        chip("Hostile", palette::front_hostile);
        else if (r.friends)       chip("Friend", palette::front_friendly);
        else if (r.offer_out)     chip("Offer sent", palette::front_friendly);
        else                      chip("Neutral", palette::neutral);
        ImGui::SameLine();
        ImGui::TextDisabled("them:");
        ImGui::SameLine();
        if (r.hostile_in)         chip("Hostile", palette::front_hostile);
        else if (r.friends)       chip("Friend", palette::front_friendly);
        else if (r.offer_in)      chip("Offer pending", palette::front_friendly);
        else                      chip("Neutral", palette::neutral);

        // Their sentiment toward you.
        sentiment_block(w, d, sp, r.id, me, war_context(w, r.id, me));

        d.rows.push_back({r.id, ex, ey, open, r.hostile_out, r.hostile_in, r.friends, r.at_war, r.truce,
                          r.toward_me.access, r.toward_me.trust, r.toward_me.alarm});

        if (open)
            draw_expanded(w, reg, standings, s, r, me);
        ImGui::Separator();
        ImGui::PopID();
    }
}

// ── Wars ────────────────────────────────────────────────────────────────────

/// The war score as a centred bar from the declarer's side.
void score_bar(int score)
{
    const float  h  = ImGui::GetTextLineHeight() * 0.62f;
    const float  bw = std::max(60.0f, ImGui::GetContentRegionAvail().x - 50.0f);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float  y0 = p0.y + (ImGui::GetTextLineHeight() - h) * 0.5f;
    const float  cx = p0.x + bw * 0.5f;
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({p0.x, y0}, {p0.x + bw, y0 + h}, palette::sentiment_track, 2.0f);
    const float x1 = cx + std::clamp(score / 100.0f, -1.0f, 1.0f) * bw * 0.5f;
    if (score != 0)
        dl->AddRectFilled({std::min(cx, x1), y0}, {std::max(cx, x1), y0 + h},
                          score > 0 ? palette::positive : palette::negative, 1.0f);
    dl->AddLine({cx, y0 - 2.0f}, {cx, y0 + h + 2.0f}, palette::text_secondary, 1.0f);
    ImGui::Dummy({bw, ImGui::GetTextLineHeight()});
    ImGui::SameLine();
    ImGui::Text("%+d", score);
}

std::string terms_line(const world& w, const peace_terms& t)
{
    if (t.kind == peace_kind::white)
        return "white peace";
    return std::string("cession to ") + name_of(w, t.gainer) + ": " + goals_line(w, t.provinces, null_entity);
}

/// The peace composer on a war the player is party to.
void draw_peace(const world& w, ui_state& s, const war_record& war, entity_id me)
{
    diplomacy_ledger_state& d = s.diplomacy;
    const entity_id other = war.declarer == me ? war.defender : war.declarer;
    const int sc = war_score(w, war);
    const int mine = war.declarer == me ? sc : -sc;
    const bool impose = mine >= 100;

    // A pending offer on this war.
    if (const auto o = w.peace_offers.find({war.declarer, war.defender}); o != w.peace_offers.end())
    {
        if (o->second.offerer == other)
        {
            ImGui::TextWrapped("%s offers %s.", name_of(w, other), terms_line(w, o->second.terms).c_str());
            if (press(d, "Accept", "accept_peace", other, true, {}, "Conclude the peace on their terms."))
                d.pending.push_back(command_for(w, corp_verb::accept_peace, other));
            ImGui::SameLine();
            if (press(d, "Refuse", "refuse_peace", other, true, {}, "Refuse their offer; the war goes on."))
                d.pending.push_back(command_for(w, corp_verb::refuse_peace, other));
            return;
        }
        ImGui::TextDisabled("Your offer stands: %s.", terms_line(w, o->second.terms).c_str());
        return;
    }

    if (d.peace_with != other)
    {
        if (press(d, impose ? "Impose peace" : "Offer peace", "peace_open", other, true, {},
                  impose ? "The score stands at 100 in your favour: name the terms, no acceptance needed."
                         : "White peace, or a cession of goals the receiving side holds."))
        {
            d.peace_with = other;
            d.peace_gain.clear();
            d.peace_give.clear();
        }
        return;
    }

    // The composer. Goals they would cede to you (yours, held by you, still theirs),
    // or goals you would cede (theirs, held by them, still yours). One side at a time.
    const std::vector<uint32_t>& my_goals    = war.declarer == me ? war.declarer_goals : war.defender_goals;
    const std::vector<uint32_t>& their_goals = war.declarer == me ? war.defender_goals : war.declarer_goals;
    const auto owned = [&](entity_id n) {
        const auto it = d.provinces_owned.find(n);
        return it == d.provinces_owned.end() ? 0 : it->second;
    };
    const auto pick_list = [&](const char* head, const std::vector<uint32_t>& goals, entity_id gainer,
                               entity_id loser, std::vector<uint32_t>& chosen, std::vector<uint32_t>& other_side) {
        // A ticked province that no longer qualifies (retaken, or no longer the
        // loser's) drops out, so the offer is never stuck disabled on a box the
        // list no longer draws.
        chosen.erase(std::remove_if(chosen.begin(), chosen.end(), [&](uint32_t pid) {
                         return std::find(goals.begin(), goals.end(), pid) == goals.end()
                             || province_holder_for(w, pid) != gainer || province_owner(w, pid) != loser;
                     }),
                     chosen.end());
        bool any = false;
        for (const uint32_t pid : goals)
        {
            if (province_holder_for(w, pid) != gainer || province_owner(w, pid) != loser)
                continue;
            if (!any) { ImGui::TextDisabled("%s", head); any = true; }
            const bool on = std::binary_search(chosen.begin(), chosen.end(), pid);
            // Never the loser's last province (RELATIONS.md § 7, NR-1102).
            const bool last = !on && owned(loser) <= static_cast<int>(chosen.size()) + 1;
            const bool full = !on && chosen.size() >= k_war_goals_max;
            ImGui::PushID(static_cast<int>(pid));
            bool v = on;
            if (last || full) ImGui::BeginDisabled();
            if (ImGui::Checkbox(province_label(w, pid).c_str(), &v))
            {
                other_side.clear();
                if (v) chosen.push_back(pid);
                else   chosen.erase(std::find(chosen.begin(), chosen.end(), pid));
                std::sort(chosen.begin(), chosen.end());
            }
            if (last || full) ImGui::EndDisabled();
            if ((last || full) && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(last ? "It would be the loser's last province: a last province is never ceded."
                                       : "At most three provinces.");
            ImGui::PopID();
        }
        return any;
    };
    const bool any_gain = pick_list("They cede to you", my_goals, me, other, d.peace_gain, d.peace_give);
    const bool any_give = pick_list("You cede to them", their_goals, other, me, d.peace_give, d.peace_gain);
    if (!any_gain && !any_give)
        ImGui::TextDisabled("No goal is held: only a white peace.");

    const corp_verb verb = impose ? corp_verb::impose_peace : corp_verb::offer_peace;
    if (press(d, impose ? "Impose white peace" : "Offer white peace", impose ? "impose_white" : "offer_white",
              other, true, {}))
    {
        corp_command c = command_for(w, verb, other);
        c.order = 0;
        d.pending.push_back(c);
        d.peace_with = null_entity;
    }
    const bool gain = !d.peace_gain.empty();
    const std::vector<uint32_t>& provs = gain ? d.peace_gain : d.peace_give;
    std::string why;
    if (provs.empty())
        why = "Tick the provinces to be ceded.";
    else
    {
        peace_terms t;
        t.kind      = peace_kind::cession;
        t.gainer    = gain ? me : other;
        t.provinces = provs;
        if (check_peace_terms(w, war, t) != war_result::applied)
            why = "Those terms are not legal now.";
    }
    ImGui::SameLine();
    if (press(d, impose ? "Impose cession" : "Offer cession", impose ? "impose_cession" : "offer_cession", other,
              why.empty(), why))
    {
        corp_command c = command_for(w, verb, other);
        c.order = gain ? 1 : 2;
        put_provinces(c, provs);
        d.pending.push_back(c);
        d.peace_with = null_entity;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Close"))
        d.peace_with = null_entity;
}

void draw_wars(const world& w, const recipe_registry& reg, ui_state& s, entity_id me)
{
    diplomacy_ledger_state& d = s.diplomacy;
    std::vector<const war_record*> wars;
    for (const auto& [key, war] : w.wars)
        wars.push_back(&war);
    // The player's first; then the map's own (declarer, defender) order.
    std::stable_sort(wars.begin(), wars.end(), [me](const war_record* a, const war_record* b) {
        const bool pa = a->declarer == me || a->defender == me;
        const bool pb = b->declarer == me || b->defender == me;
        return pa && !pb;
    });
    if (wars.empty())
        ImGui::TextDisabled("No war is being fought.");

    for (const war_record* war : wars)
    {
        ImGui::PushID(static_cast<int>(war->declarer)); // two pushes: no signed-overflow product
        ImGui::PushID(static_cast<int>(war->defender));
        ++d.war_rows;
        const bool mine = war->declarer == me || war->defender == me;
        ImGui::TextColored(col4(mine ? palette::selection : palette::text_secondary), "%s  vs  %s",
                           name_of(w, war->declarer), name_of(w, war->defender));
        const int days = (w.current_econ_tick - war->opened_tick) * k_construction_quarter_days
                       + (w.construction_quarter_day - war->opened_day);
        ImGui::SameLine();
        ImGui::TextDisabled("  %d days", std::max(0, days));
        score_bar(war_score(w, *war));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("War score, from %s's side: half occupation of its goals, half battles.",
                              name_of(w, war->declarer));
        ImGui::TextWrapped("%s's goals: %s", name_of(w, war->declarer),
                           goals_line(w, war->declarer_goals, me).c_str());
        ImGui::TextWrapped("%s's goals: %s", name_of(w, war->defender),
                           war->answered ? goals_line(w, war->defender_goals, me).c_str() : "not yet answered");
        ImGui::Text("Exhaustion  %.0f / %.0f", war->declarer_exhaustion, war->defender_exhaustion);

        if (mine)
        {
            if (war->defender == me && !war->answered)
            {
                if (d.declare_target == war->declarer && d.declare_answer)
                    draw_declaration(w, reg, s, me);
                else if (press(d, "Answer", "answer_open", war->declarer, true, {},
                               "Name up to three of their provinces as your goals. Answering is free."))
                    open_declaration(w, s, war->declarer);
            }
            if (s.diplomacy.declare_target != war->declarer || !s.diplomacy.declare_answer)
                draw_peace(w, s, *war, me);
        }
        ImGui::Separator();
        ImGui::PopID();
        ImGui::PopID();
    }

    // Truces, with their time left.
    bool head = false;
    for (const auto& [pr, expiry] : w.truces)
    {
        if (w.current_econ_tick >= expiry)
            continue;
        if (!head) { ImGui::Spacing(); ImGui::TextDisabled("Truces"); head = true; }
        ++d.truce_rows;
        ImGui::Text("  %s and %s: %d ticks left", name_of(w, pr.first), name_of(w, pr.second),
                    expiry - w.current_econ_tick);
    }
}

/// A declaration the player can no longer SEE must not keep the canvas: while
/// `declare_target` is set the canvas suppresses right-click orders and turns
/// left-clicks into goal toggles. So it closes the moment its host goes — the
/// tab it lives on is left (a tab press, Offer peace's jump to Wars), another row
/// is expanded, or the war an answer was for is answered or gone.
void drop_orphan_declaration(const world& w, ui_state& s)
{
    diplomacy_ledger_state& d = s.diplomacy;
    if (d.declare_target == null_entity)
        return;
    bool orphan = d.view != (d.declare_answer ? 1 : 0);
    if (!d.declare_answer)
        orphan = orphan || d.expanded != d.declare_target;
    else
    {
        const war_record* war = war_between(w, me_of(w), d.declare_target);
        orphan = orphan || war == nullptr || war->answered || war->declarer != d.declare_target;
    }
    if (orphan)
        close_declaration(s);
}

} // namespace

void reset_diplomacy_ui(ui_state& s)
{
    s.diplomacy = diplomacy_ledger_state{};
    s.pending_unit_order_goals.clear();
    g_preview = declaration_preview{};
    g_fronts  = front_read{};
    g_province_names.clear();
}

void close_declaration(ui_state& s)
{
    diplomacy_ledger_state& d = s.diplomacy;
    d.declare_target = null_entity;
    d.declare_answer = false;
    d.goals.clear();
    d.goal_candidates.clear();
    d.candidates_generation = ~std::uint64_t{0};
    d.attack_units.clear();
    d.attack_tile = null_entity;
    // The declaration was scrolled to the column top; with it gone the row's own
    // presses would sit above the fold, so bring the row back into view.
    d.scroll_to_row = true;
}

void open_declaration(const world& w, ui_state& s, entity_id target, entity_id attack_tile,
                      const std::vector<entity_id>& units)
{
    if (!s.show_diplomacy_ledger)
    {
        close_all_panels(s);
        s.show_diplomacy_ledger = true;
    }
    diplomacy_ledger_state& d = s.diplomacy;
    close_declaration(s);
    const entity_id me = me_of(w);
    const war_record* war = war_between(w, me, target);
    d.declare_answer = war != nullptr && war->defender == me && war->declarer == target && !war->answered;
    d.declare_target = target;
    d.view           = d.declare_answer ? 1 : 0;
    d.expanded       = target;
    d.peace_with     = null_entity;
    d.attack_units   = units;
    d.attack_tile    = attack_tile;
    d.scroll_to_row  = false; // the declaration's own scroll wins (close_declaration set this)
    d.scroll_to_decl = true;
    if (attack_tile != null_entity)
        if (const uint32_t g = attack_goal_province(w, target, attack_tile); g != 0)
            d.goals.push_back(g);
}

void draw_diplomacy_ledger(const world& w, const recipe_registry& reg, const economy_report& rep,
                           const std::vector<corp_standing>& standings, ui_state& s, bool& open)
{
    diplomacy_ledger_state& d = s.diplomacy;
    sample_history(w, d); // every frame, open or not: the window fills before it is read
    d.tab_x[0] = d.tab_x[1] = d.tab_y[0] = d.tab_y[1] = -1.0f;
    d.rows.clear();
    d.presses.clear();
    d.decl_goals = d.decl_candidates = d.decl_observers = 0;
    d.decl_valid = d.decl_justified = false;
    d.decl_trust = d.decl_alarm = 0.0f;
    d.war_rows = d.truce_rows = 0;

    if (!open)
    {
        // Closing the ledger by any path leaves no trace of an open declaration.
        if (d.declare_target != null_entity)
            close_declaration(s);
        d.peace_with = null_entity;
        return;
    }

    drop_orphan_declaration(w, s);
    refresh_owners(w, d);
    refresh_candidates(d);

    if (!ui::foldout_begin("Diplomacy"))
    {
        ui::foldout_end();
        return;
    }

    // The tab strip; `nav_button` carries the toggle rule — re-clicking the active
    // tab closes the ledger, the other tab is an ordinary view change.
    ui::nav_button("Nations", 0, d.view, &open);
    d.tab_x[0] = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
    d.tab_y[0] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("How each nation stands with you, and what a war with it would cost.");
    ImGui::SameLine();
    ui::nav_button("Wars", 1, d.view, &open);
    d.tab_x[1] = (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
    d.tab_y[1] = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Who is fighting whom, and how each war is going.");
    if (d.view < 0 || d.view > 1)
        d.view = 0;
    ImGui::Separator();

    if (!d.message.empty())
        ImGui::TextColored(col4(palette::selection), "%s", d.message.c_str());

    if (open)
    {
        // The body scrolls in its own child so the tab strip and the result line
        // stay put: a tab scrolled out of reach is a toggle the player cannot press.
        // `verify.scroll_panel("diplomacy", f)` reaches it through this key.
        if (ImGui::BeginChild("##diplomacy_body", {0.0f, 0.0f}, false))
        {
            foldout_scroll_child("##diplomacy_body");
            const entity_id me = me_of(w);
            if (d.view == 0)
            {
                refresh_gdp(w, reg, rep, d);
                draw_nations(w, reg, standings, s, me);
            }
            else
                draw_wars(w, reg, s, me);
        }
        ImGui::EndChild();
        drop_orphan_declaration(w, s); // a tab or row press this frame left it unseen
    }
    if (!open)
    {
        close_declaration(s);
        d.peace_with = null_entity;
    }
    ui::foldout_end();
}

} // namespace ui
