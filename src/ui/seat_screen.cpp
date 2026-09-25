/// @file seat_screen.cpp
/// The corporation selection canvas (BL-1076, STARTUP.md § The seat).
///
/// ITS QUESTION: which corporation am I? Drawn at Begin, over the settled world
/// the wizard handed forward, once the validation run is over and before play
/// opens. Ben, 2026-09-24 (the select-company design session):
///
///   * a ranked list BESIDE a map — every specialist, in static-score order,
///     the ones below the viability floor visibly MARKED and still pickable;
///   * the map is the home body, and it HIGHLIGHTS THE HOVERED ROW'S FIRM —
///     its HQ, its works and its home market's catchment;
///   * a card with FOUR things and no more: industry and goods; HQ and home
///     market with that market's prices for the firm's goods and inputs; cash,
///     debt and assets; its nation and that nation's stance;
///   * picking opens a BRIEFING (the opening position in prose; for a marked
///     firm, why the floor marked it), then Confirm seats the player and opens
///     play, Back returns with nothing changed. Two presses, because choosing
///     who you are is a deliberate act.
///
/// READ-ONLY UNTIL CONFIRM. Every figure is read off `m_world` and the
/// ranking; the one mutation is Confirm's `take_seat_pick`, which goes through
/// the command seam as `corp_verb::take_seat` — the same act an agent makes.
/// Immediate-mode: nothing here is retained but the baked map runs, which are
/// a draw cache for one body and are rebuilt if the body changes.

#include "core/app.hpp"

#include <imgui.h>

#include "ui/history_lapse.hpp"   // lapse_year_label, for the briefing's origin sentence (BL-1099)
#include "ui/icons.hpp"
#include "ui/market_ledger.hpp"   // market_city_name
#include "ui/presentation.hpp"
#include "world/budget_system.hpp" // k_debt_interest_per_quarter, for the briefing's debt line
#include "world/era_timelapse.hpp" // owner_change / owner_none -- the origin sentence's realm (BL-1099)
#include "world/logistics.hpp"     // body_tile_grid
#include "world/market_clearing.hpp"
#include "world/sentiment.hpp"
#include "world/settlement.hpp"    // settlement_state::regions -- the origin region's name (BL-1099)

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const char* focus_word(industrial_focus f)
{
    switch (f)
    {
    case industrial_focus::extraction: return "Extraction";
    case industrial_focus::processing: return "Processing";
    case industrial_focus::trade:      return "Trade";
    }
    return "Unknown";
}

const char* politics_word(ideology v)
{
    switch (v)
    {
    case ideology::authoritarian: return "authoritarian";
    case ideology::technocratic:  return "technocratic";
    case ideology::mercantile:    return "mercantile";
    case ideology::isolationist:  return "isolationist";
    }
    return "unknown";
}

const char* posture_word(expansionism v)
{
    switch (v)
    {
    case expansionism::passive:    return "passive";
    case expansionism::moderate:   return "moderate";
    case expansionism::aggressive: return "aggressive";
    }
    return "unknown";
}

/// What one firm makes and draws, read off its holdings: an extraction site's
/// target, and a processor's recipe outputs / inputs. Nothing inferred.
struct firm_goods
{
    std::array<bool, resource_count> makes{};
    std::array<bool, resource_count> draws{};
    int works = 0;
};

/// BL-1099 / R22 -- THE ORIGIN SENTENCE (STARTUP.md § The seat; Ben,
/// 2026-09-24): "Chartered from <city>'s industry, in <region>, under
/// <nation>, the realm of <X> since <year>". Read from the firm's own
/// `founded_year` and `origin_region` and from the record's owner slice at
/// that year -- never from the charter report. The region is the settlement
/// record's; the realm is the polity holding that region at the founding
/// year on whichever of the cradle's records spans it (the Industrialisation
/// span first, the two before it if the year falls earlier), named by its
/// seat exactly as the round's board names it (`polity_seat`: the first
/// region it held); "since" is the year that realm took the region. A firm
/// with no origin (none the search chartered) gets no sentence; a firm whose
/// year no record spans, or whose ground no realm held, keeps the sentence
/// short of the realm clause rather than inventing one.
std::string seat_origin_sentence(const world& w, const generation_report& rep,
                                 const corporation_component& cc, const std::string& city,
                                 const nation_component* nat)
{
    const settlement_state* ss = w.gen_settlement.get();
    if (ss == nullptr || cc.origin_region < 0
        || static_cast<std::size_t>(cc.origin_region) >= ss->regions.size())
        return {};
    const std::string& region = ss->regions[static_cast<std::size_t>(cc.origin_region)].name;

    // The record whose span holds the year: the cradle's, latest first.
    const era_timelapse* rec = nullptr;
    for (const generation_report::body_entry& be : rep.bodies)
    {
        for (const era_timelapse* t : {&be.industrialisation_timelapse, &be.exploration_timelapse,
                                       &be.prehistory_timelapse})
        {
            if (t->changes.empty()) continue;
            if (cc.founded_year >= t->start_year && cc.founded_year <= t->start_year + t->years)
            {
                rec = t;
                break;
            }
        }
        if (rec != nullptr) break;
    }

    // The holder of the origin region at the year, and the year it took it;
    // each polity's seat is the first region it held in the record.
    std::string realm;
    int32_t     since = 0;
    if (rec != nullptr)
    {
        uint16_t             owner = owner_none;
        std::vector<int32_t> seat;
        for (const owner_change& ch : rec->changes)
        {
            if (ch.year > cc.founded_year) break; // ascending by year
            if (ch.owner != owner_none)
            {
                if (seat.size() <= ch.owner) seat.resize(static_cast<std::size_t>(ch.owner) + 1, -1);
                if (seat[ch.owner] < 0) seat[ch.owner] = ch.region;
            }
            if (static_cast<int>(ch.region) == cc.origin_region && ch.owner != owner)
            {
                owner = ch.owner;
                since = ch.year;
            }
        }
        if (owner != owner_none && owner < seat.size() && seat[owner] >= 0
            && static_cast<std::size_t>(seat[owner]) < ss->regions.size())
            realm = ss->regions[static_cast<std::size_t>(seat[owner])].name;
    }

    std::string s = "Chartered from " + city + "'s industry, in " + region;
    if (nat != nullptr) s += ", under " + nat->name;
    // An UNDATED firm (`founded_year` 0: an origin the walk stamped, a year no
    // finish dated -- a fixture, or a save from before the pairing) keeps the
    // sentence short of any year: year 0 falls inside the Empires record's
    // span, and reading a realm off it would name a realm the firm never knew.
    if (cc.founded_year != 0 && !realm.empty())
        s += ", the realm of " + realm + " since " + ui::lapse_year_label(since);
    else if (cc.founded_year != 0)
        s += ", in " + ui::lapse_year_label(cc.founded_year);
    s += ".";
    return s;
}

firm_goods goods_of(const world& w, const recipe_registry& reg, const corporation_component& cc)
{
    firm_goods g;
    for (const entity_id bid : cc.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        ++g.works;
        if (b.type == building_type::extraction_site)
            g.makes[static_cast<std::size_t>(b.target_resource)] = true;
        else if (b.type == building_type::processing_facility)
        {
            if (const recipe* r = reg.get_recipe(b.recipe))
                for (std::size_t i = 0; i < resource_count; ++i)
                {
                    if (r->outputs[i] > 0.0f) g.makes[i] = true;
                    if (r->inputs[i]  > 0.0f) g.draws[i] = true;
                }
        }
    }
    return g;
}

std::string goods_list(const std::array<bool, resource_count>& set, std::size_t cap = 4)
{
    std::string s;
    std::size_t n = 0, more = 0;
    for (std::size_t i = 0; i < resource_count; ++i)
    {
        if (!set[i]) continue;
        if (n < cap)
        {
            if (!s.empty()) s += ", ";
            s += ui::resource_name(static_cast<resource_type>(i));
            ++n;
        }
        else
            ++more;
    }
    if (more > 0)
        s += " +" + std::to_string(more) + " more";
    return s.empty() ? std::string("nothing yet") : s;
}

/// "Iron Ore 12.4, Steel 40.1" — the home market's resolved prices for @p set.
std::string price_list(const market_component* m, const std::array<bool, resource_count>& set,
                       std::size_t cap = 4)
{
    if (!m) return "no market";
    std::string s;
    std::size_t n = 0;
    for (std::size_t i = 0; i < resource_count && n < cap; ++i)
    {
        if (!set[i]) continue;
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s%s %.1f", s.empty() ? "" : ", ",
                      ui::resource_name(static_cast<resource_type>(i)),
                      static_cast<double>(m->price[i]));
        s += buf;
        ++n;
    }
    return s.empty() ? std::string("none") : s;
}

entity_id hq_tile_of(const world& w, const corporation_component& cc)
{
    if (const auto hb = w.buildings.find(cc.hq_building); hb != w.buildings.end())
        return hb->second.tile;
    for (const entity_id bid : cc.assets) // no HQ recorded: the first sited holding
        if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
            return bit->second.tile;
    return null_entity;
}

ImU32 mix(ImU32 a, ImU32 b, float t)
{
    const auto ch = [&](int shift) {
        const float x = static_cast<float>((a >> shift) & 0xFF);
        const float y = static_cast<float>((b >> shift) & 0xFF);
        return static_cast<ImU32>(x + (y - x) * t + 0.5f) << shift;
    };
    return ch(IM_COL32_R_SHIFT) | ch(IM_COL32_G_SHIFT) | ch(IM_COL32_B_SHIFT)
         | (0xFFu << IM_COL32_A_SHIFT);
}

} // namespace

void app::draw_seat_screen()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0.0f, 0.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(disp, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!ImGui::Begin("##seat_screen", nullptr, flags))
    {
        ImGui::End();
        return;
    }

    const auto& cands = m_seat_result.candidates;
    if (cands.empty())
    {
        // No specialist at all: nothing to offer. Say so, and let the player go
        // on with whatever the generator flagged rather than strand them here.
        ImGui::TextUnformatted("This world holds no corporation to seat.");
        if (ImGui::Button("Open play"))
        {
            if (m_seat_ui.from_verify) m_screen = app_screen::in_game;
            else                       finish_new_game();
        }
        ImGui::End();
        return;
    }
    m_seat_ui.hovered = std::clamp(m_seat_ui.hovered, 0, static_cast<int>(cands.size()) - 1);

    // --- the header ------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
    ImGui::TextUnformatted("WHICH CORPORATION ARE YOU?");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::text_secondary);
    ImGui::Text("%d corporations, ranked by the ground they stand on. %d clear the viability "
                "floor; the marked ones do not, and you may still take one.",
                static_cast<int>(cands.size()), m_seat_result.shortlist_size);
    ImGui::PopStyleColor();
    ImGui::Dummy({1.0f, 6.0f});

    const float pad    = ImGui::GetStyle().ItemSpacing.x;
    const float avail  = ImGui::GetContentRegionAvail().x;
    const float list_w = std::max(360.0f, avail * 0.40f);
    const float right_w = std::max(200.0f, avail - list_w - pad);
    const float body_h = ImGui::GetContentRegionAvail().y;

    // The firm the map and the card show: the open briefing's, else the hovered row.
    int shown = m_seat_ui.hovered;
    if (m_seat_ui.briefing != null_entity)
        for (int i = 0; i < static_cast<int>(cands.size()); ++i)
            if (cands[static_cast<std::size_t>(i)].corp == m_seat_ui.briefing)
                shown = i;

    // --- the ranked list -------------------------------------------------
    ImGui::BeginChild("##seat_list", {list_w, body_h}, ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("##seat_rows", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableSetupColumn("Corporation", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn("Industry", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Holdings", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableHeadersRow();

        bool any_hovered = false;
        for (int i = 0; i < static_cast<int>(cands.size()); ++i)
        {
            const spawn_seat_candidate& c = cands[static_cast<std::size_t>(i)];
            const auto cit = m_world.corporations.find(c.corp);
            if (cit == m_world.corporations.end())
                continue;
            const corporation_component& cc = cit->second;
            const bool marked = !c.shortlisted;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(i);
            // The row is one Selectable spanning every column: hover points the
            // map at the firm; a press opens its briefing, and pressing the row
            // whose briefing is open closes it (the toggle rule).
            const bool open = (m_seat_ui.briefing == c.corp);
            char label[16];
            std::snprintf(label, sizeof label, "%d", i + 1);
            if (ImGui::Selectable(label, open || i == shown,
                                  ImGuiSelectableFlags_SpanAllColumns))
                m_seat_ui.briefing = open ? null_entity : c.corp;
            if (ImGui::IsItemHovered())
            {
                m_seat_ui.hovered = i;
                any_hovered       = true;
            }
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            if (marked) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 158, 255));
            ImGui::TextUnformatted(cc.name.c_str());
            if (marked)
            {
                ImGui::PopStyleColor();
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::pinned);
                ImGui::TextUnformatted("  below the floor");
                ImGui::PopStyleColor();
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(focus_word(cc.focus));
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", c.holdings);
        }
        (void)any_hovered; // the last hovered row stays shown when the pointer leaves
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // --- the map and the card / briefing ---------------------------------
    ImGui::BeginChild("##seat_right", {right_w, body_h});
    const spawn_seat_candidate& sc = cands[static_cast<std::size_t>(shown)];
    const auto scit = m_world.corporations.find(sc.corp);
    const corporation_component* scc =
        scit != m_world.corporations.end() ? &scit->second : nullptr;
    const entity_id hq_tile = scc ? hq_tile_of(m_world, *scc) : null_entity;
    entity_id body = null_entity;
    if (const auto t = m_world.tiles.find(hq_tile); t != m_world.tiles.end())
        body = t->second.body;
    if (body == null_entity)
        body = m_launch_body;

    // Bake the body's political base once: water, and land tinted by its nation.
    if (body != null_entity && body != m_seat_ui.body)
    {
        m_seat_ui.body = body;
        m_seat_ui.base.clear();
        m_seat_ui.catchment.clear();
        m_seat_ui.catchment_of = null_entity;
        m_seat_ui.gw = m_seat_ui.gh = 0;
        if (const auto bit = m_world.bodies.find(body); bit != m_world.bodies.end())
        {
            m_seat_ui.gw = bit->second.grid_width;
            m_seat_ui.gh = bit->second.grid_height;
        }
        const std::vector<entity_id>& grid = body_tile_grid(m_world, body);
        const int gw = m_seat_ui.gw, gh = m_seat_ui.gh;
        if (gw > 0 && gh > 0 && grid.size() == static_cast<std::size_t>(gw) * gh)
        {
            m_seat_ui.tile_market.assign(grid.size(), null_entity);
            for (int row = 0; row < gh; ++row)
            {
                int    c0  = 0;
                ImU32  cur = 0;
                for (int col = 0; col <= gw; ++col)
                {
                    ImU32 colour = 0;
                    if (col < gw)
                    {
                        const std::size_t k = static_cast<std::size_t>(row) * gw + col;
                        const entity_id tid = grid[k];
                        const auto tit = m_world.tiles.find(tid);
                        if (tit == m_world.tiles.end() || is_water(tit->second.substrate))
                            colour = IM_COL32(22, 30, 44, 255);
                        else
                        {
                            colour = IM_COL32(64, 70, 62, 255);
                            if (const auto nit = m_world.tile_to_nation.find(tid);
                                nit != m_world.tile_to_nation.end())
                                colour = mix(colour, ui::palette::nation_colour(nit->second), 0.35f);
                            m_seat_ui.tile_market[k] = market_for_tile(m_world, tid);
                        }
                    }
                    if (col == 0) { cur = colour; continue; }
                    if (col < gw && colour == cur) continue;
                    m_seat_ui.base.push_back({row, c0, col, cur});
                    c0  = col;
                    cur = colour;
                }
            }
        }
    }

    const entity_id home_market = hq_tile != null_entity ? market_for_tile(m_world, hq_tile)
                                                         : null_entity;
    if (home_market != m_seat_ui.catchment_of && m_seat_ui.gw > 0)
    {
        m_seat_ui.catchment_of = home_market;
        m_seat_ui.catchment.clear();
        const int gw = m_seat_ui.gw;
        for (int row = 0; row < m_seat_ui.gh; ++row)
        {
            int start = -1;
            for (int col = 0; col <= gw; ++col)
            {
                const bool in = col < gw && home_market != null_entity
                    && m_seat_ui.tile_market[static_cast<std::size_t>(row) * gw + col] == home_market;
                if (in && start < 0) start = col;
                if (!in && start >= 0)
                {
                    m_seat_ui.catchment.push_back({row, start, col, 0});
                    start = -1;
                }
            }
        }
    }

    // The card or briefing takes a fixed band under the map; the map fills the rest.
    const float line_h   = ImGui::GetTextLineHeightWithSpacing();
    const float card_h   = line_h * 9.0f;
    const float map_wmax = ImGui::GetContentRegionAvail().x;
    const float map_hmax = std::max(80.0f, ImGui::GetContentRegionAvail().y - card_h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (m_seat_ui.gw > 0 && m_seat_ui.gh > 0)
    {
        const float cell  = std::min(map_wmax / static_cast<float>(m_seat_ui.gw),
                                     map_hmax / static_cast<float>(m_seat_ui.gh));
        const float map_w = cell * static_cast<float>(m_seat_ui.gw);
        const float map_h = cell * static_cast<float>(m_seat_ui.gh);
        const ImVec2 tl   = ImGui::GetCursorScreenPos();
        ImGui::Dummy({map_w, map_h}); // the map takes no input: pointing is done at the list

        for (const auto& r : m_seat_ui.base)
            dl->AddRectFilled({tl.x + r.c0 * cell, tl.y + r.row * cell},
                              {tl.x + r.c1 * cell, tl.y + (r.row + 1) * cell}, r.colour);
        // The home market's catchment: a light wash over the tiles it serves.
        for (const auto& r : m_seat_ui.catchment)
            dl->AddRectFilled({tl.x + r.c0 * cell, tl.y + r.row * cell},
                              {tl.x + r.c1 * cell, tl.y + (r.row + 1) * cell},
                              IM_COL32(120, 190, 255, 60));

        const auto at = [&](entity_id tile, ImVec2& out) {
            const auto t = m_world.tiles.find(tile);
            if (t == m_world.tiles.end() || t->second.body != body) return false;
            out = {tl.x + (t->second.grid_x + 0.5f) * cell, tl.y + (t->second.grid_y + 0.5f) * cell};
            return true;
        };
        // Every other ranked firm's HQ, faint, so the highlighted one reads as a
        // choice among neighbours rather than a lone dot.
        for (const spawn_seat_candidate& c : cands)
        {
            if (c.corp == sc.corp) continue;
            const auto cit = m_world.corporations.find(c.corp);
            if (cit == m_world.corporations.end()) continue;
            ImVec2 p;
            if (at(hq_tile_of(m_world, cit->second), p))
                dl->AddCircleFilled(p, std::max(2.0f, cell * 0.9f), IM_COL32(200, 204, 214, 110), 10);
        }
        if (home_market != null_entity)
            if (const auto mit = m_world.markets.find(home_market); mit != m_world.markets.end())
            {
                ImVec2 p;
                if (at(mit->second.centre_tile, p))
                    ui::icons::market(dl, p, std::max(4.0f, cell * 2.2f), ui::palette::settlement);
            }
        if (scc)
        {
            // The HQ first, then the works OVER it: a firm's holdings cluster
            // within a few tiles of its HQ, and a glyph drawn last hid them.
            ImVec2 hp;
            if (at(hq_tile, hp))
                ui::icons::hq(dl, hp, std::max(5.0f, cell * 2.0f), ui::palette::hover);
            for (const entity_id bid : scc->assets)
            {
                const auto bit = m_world.buildings.find(bid);
                if (bit == m_world.buildings.end() || bid == scc->hq_building) continue;
                ImVec2 p;
                if (!at(bit->second.tile, p)) continue;
                const float h = std::max(2.5f, cell * 0.8f);
                dl->AddRectFilled({p.x - h, p.y - h}, {p.x + h, p.y + h}, IM_COL32(10, 12, 18, 255));
                dl->AddRectFilled({p.x - h + 1.0f, p.y - h + 1.0f}, {p.x + h - 1.0f, p.y + h - 1.0f},
                                  ui::palette::hover);
            }
        }
        dl->AddRect(tl, {tl.x + map_w, tl.y + map_h}, IM_COL32(52, 60, 76, 255));
    }

    ImGui::Dummy({1.0f, 4.0f});
    if (!scc)
    {
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    // --- facts both the card and the briefing read ---------------------
    const firm_goods goods = goods_of(m_world, m_registry, *scc);
    const market_component* hm = nullptr;
    if (const auto mit = m_world.markets.find(home_market); mit != m_world.markets.end())
        hm = &mit->second;
    const std::string city = home_market != null_entity ? ui::market_city_name(m_world, home_market)
                                                        : std::string("no market");
    const float cash = std::max(0.0f, scc->balance);
    const float debt = std::max(0.0f, -scc->balance);
    const float book = scc->returns.empty() ? -1.0f : scc->returns.back().book_value;
    const nation_component* nat = nullptr;
    if (const auto nit = m_world.nations.find(scc->home_nation); nit != m_world.nations.end())
        nat = &nit->second;
    const sentiment_value read = sentiment_toward(m_world.sentiment, scc->home_nation, sc.corp);

    if (m_seat_ui.briefing == null_entity)
    {
        // --- THE CARD: four lines, and no more -----------------------------
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
        ImGui::Text("%s%s", scc->name.c_str(), sc.shortlisted ? "" : "   (below the viability floor)");
        ImGui::PopStyleColor();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text("Industry: %s. Makes %s%s%s.", focus_word(scc->focus),
                    goods_list(goods.makes).c_str(),
                    std::any_of(goods.draws.begin(), goods.draws.end(), [](bool b) { return b; })
                        ? "; draws " : "",
                    std::any_of(goods.draws.begin(), goods.draws.end(), [](bool b) { return b; })
                        ? goods_list(goods.draws).c_str() : "");
        ImGui::Text("HQ and home market: %s. Prices there: %s%s%s.", city.c_str(),
                    price_list(hm, goods.makes).c_str(),
                    std::any_of(goods.draws.begin(), goods.draws.end(), [](bool b) { return b; })
                        ? "; inputs " : "",
                    std::any_of(goods.draws.begin(), goods.draws.end(), [](bool b) { return b; })
                        ? price_list(hm, goods.draws).c_str() : "");
        if (book >= 0.0f)
            ImGui::Text("Cash %.0f cr, debt %.0f cr, assets %d holdings (book value %.0f cr).",
                        static_cast<double>(cash), static_cast<double>(debt), sc.holdings,
                        static_cast<double>(book));
        else
            ImGui::Text("Cash %.0f cr, debt %.0f cr, assets %d holdings.",
                        static_cast<double>(cash), static_cast<double>(debt), sc.holdings);
        if (nat)
            ImGui::Text("Nation: %s, %s and %s. Its stance toward the firm: Access %+.2f, Trust %+.2f.",
                        nat->name.c_str(), politics_word(nat->politics), posture_word(nat->posture),
                        static_cast<double>(read.access), static_cast<double>(read.trust));
        else
            ImGui::TextUnformatted("Nation: none. The firm answers to no national law.");
        ImGui::PopTextWrapPos();
        ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::text_secondary);
        ImGui::TextUnformatted("Press a row to read its briefing.");
        ImGui::PopStyleColor();
    }
    else
    {
        // --- THE BRIEFING: the opening position, in prose -------------------
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
        ImGui::Text("Briefing: %s", scc->name.c_str());
        ImGui::PopStyleColor();
        ImGui::PushTextWrapPos(0.0f);
        const std::string makes = goods_list(goods.makes, 6);
        const bool draws_any =
            std::any_of(goods.draws.begin(), goods.draws.end(), [](bool b) { return b; });
        ImGui::Text("You are %s, %s corporation with %d works. You make %s.",
                    scc->name.c_str(), scc->focus == industrial_focus::extraction
                        ? "an extraction" : scc->focus == industrial_focus::processing
                        ? "a processing" : "a trade",
                    goods.works, makes.c_str());
        if (draws_any)
            ImGui::Text("You sell at %s's market, where your goods fetch %s, and you buy %s "
                        "there at %s.", city.c_str(), price_list(hm, goods.makes).c_str(),
                        goods_list(goods.draws).c_str(), price_list(hm, goods.draws).c_str());
        else
            ImGui::Text("You sell at %s's market, where your goods fetch %s.", city.c_str(),
                        price_list(hm, goods.makes).c_str());
        if (debt > 0.0f)
            ImGui::Text("You owe %.0f cr. While the balance stays below zero the debt compounds "
                        "at %.1f%% a quarter.", static_cast<double>(debt),
                        static_cast<double>(k_debt_interest_per_quarter * 100.0f));
        else
            ImGui::Text("You owe nothing, and hold %.0f cr in cash.", static_cast<double>(cash));
        if (nat)
            ImGui::Text("You trade under %s's law: %s %s state, %s on its borders. It reads you "
                        "at Access %+.2f, Trust %+.2f.", nat->name.c_str(),
                        nat->politics == ideology::isolationist ? "an" : "a",
                        politics_word(nat->politics), posture_word(nat->posture),
                        static_cast<double>(read.access), static_cast<double>(read.trust));
        else
            ImGui::TextUnformatted("You trade under no nation's law.");
        // BL-1099 / R22: one sentence of origin, from the firm's own fields and
        // the record's owner slice -- the whole of the history the seat carries.
        if (const std::string origin = seat_origin_sentence(m_world, m_generation_report, *scc, city, nat);
            !origin.empty())
            ImGui::TextUnformatted(origin.c_str());
        if (!sc.shortlisted)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::pinned);
            if (sc.holdings_scored == 0)
                ImGui::Text("The floor marked this firm: none of your %d holdings stands in a market "
                            "the landscape scored, so none stands where a chain closes and a "
                            "resource sits inside the pin band. You may still take it.",
                            sc.holdings);
            else
                ImGui::Text("The floor marked this firm: %d of your %d holdings stand in scored "
                            "markets, but in none of them does a chain close with a resource "
                            "inside the pin band. You may still take it.",
                            sc.holdings_scored, sc.holdings);
            ImGui::PopStyleColor();
        }
        ImGui::PopTextWrapPos();
        ImGui::Dummy({1.0f, 4.0f});
        if (ImGui::Button("Confirm"))
        {
            // THE PICK, as a game act: through the seam as corp_verb::take_seat.
            const corp_command_result r = take_seat_pick(sc.corp);
            if (r == corp_command_result::applied)
            {
                if (m_seat_ui.from_verify) m_screen = app_screen::in_game;
                else                       finish_new_game();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Back"))
            m_seat_ui.briefing = null_entity; // nothing changed: the world was only read
    }

    ImGui::EndChild();
    ImGui::End();
}
