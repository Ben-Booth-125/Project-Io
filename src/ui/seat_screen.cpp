/// @file seat_screen.cpp
/// The NATION SELECTION screen (BL-1281, BL-1295; STARTUP.md § The seat).
///
/// ITS QUESTION: which nation am I? Drawn at Begin, over the settled world the
/// wizard handed forward, once the validation run is over and before play
/// opens. Ben, 2026-10-10 (overturning the corporation canvas at the grain of
/// the pick; CONCEPT.md § Player identity):
///
///   * a list of the MAJOR nations only -- nations holding a champion; majors
///     and seats are one-to-one, so the rows are the major target's count --
///     in their champions' rank order, a champion below the viability floor
///     visibly MARKED and its nation still pickable;
///   * the map is the home body, and it HIGHLIGHTS THE SHOWN NATION'S
///     TERRITORY, with its champion's HQ, works and home market on it;
///   * a NATION CARD of seven parts: territory; population and GDP; treasury
///     and budget weights; the champion (the firm card, unchanged in content);
///     resource profile and deposits; neighbours with their reading and
///     grudges; and HISTORY (BL-1295) -- the pattern tags, a short timeline,
///     and a link into the History ledger filtered to the nation;
///   * picking opens a BRIEFING, then Confirm seats the player and opens play,
///     Back returns with nothing changed. Two presses, because choosing who you
///     are is a deliberate act. The screen is shaped for up to six humans and
///     seats one (NR-1025): nothing here assumes there is only one row taken.
///
/// READ-ONLY UNTIL CONFIRM. Every figure is read off `m_world`, the ranking
/// and the generation report; the one mutation is Confirm's `take_seat_pick`,
/// which names the MAJOR and goes through the command seam as
/// `corp_verb::take_seat` on its champion -- the same act an agent makes.
/// Immediate-mode: nothing here is retained but the baked map runs, which are
/// a draw cache for one body and are rebuilt if the body changes.

#include "core/app.hpp"

#include <imgui.h>
#include <imgui_internal.h> // BringWindowToDisplayFront -- the history link's ledger over the screen

#include "ui/history_lapse.hpp"   // lapse_year_label, for the briefing's origin sentence (BL-1099)
#include "ui/icons.hpp"
#include "ui/market_ledger.hpp"   // market_city_name
#include "ui/foldout_column.hpp" // foldout_scroll_child (verify.scroll_panel "seat_card")
#include "ui/nation_history.hpp"  // the card's history part (BL-1295)
#include "ui/presentation.hpp"
#include "ui/tile_inspector.hpp"  // the History ledger the card's link opens (BL-1295)
#include "world/budget_system.hpp" // k_debt_interest_per_quarter, for the briefing's debt line
#include "world/building_profit.hpp" // estimate_building_profit -- the card's GDP read (BL-1281)
#include "world/era_timelapse.hpp" // owner_slice_at / owner_none -- the origin sentence's realm (BL-1099)
#include "world/logistics.hpp"     // body_tile_grid
#include "world/nation_budget.hpp" // budget_priority -- the card's budget weights (BL-1281)
#include "world/polity_identity.hpp" // polity_seat_region -- the origin sentence's seat fallback (BL-1088's rule)
#include "world/market_clearing.hpp"
#include "world/sentiment.hpp"
#include "world/settlement.hpp"    // settlement_state::regions -- the origin region's name (BL-1099)

#include <algorithm>
#include <array>
#include <cstdio>
#include <cmath>     // std::floor -- the map's pixel-snapped runs
#include <cstdlib>   // std::abs — the card's nearest-region read (BL-1089)
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
/// that year (`owner_slice_at`) -- never from the charter report. The region
/// is the settlement record's; the realm is the polity holding that region at
/// the founding year on whichever of the cradle's records spans it (the
/// Industrialisation span first, the two before it if the year falls
/// earlier), NAMED AS THE BOARD AND THE TICKER NAME IT (BL-1088,
/// CIVILISATION.md § A realm's name): by the record's coined `polity_name`,
/// carried by id, and only where the tongue could not coin, by its seat
/// (`polity_seat_region`) -- the rule `polity_name_of` applies on the round,
/// so one realm reads as one name on the board, the ticker and this line.
/// "since" is the realm's tenure of the region, walked back through the
/// earlier records while the same polity id holds it (NR-939), and the realm
/// clause reads "its own realm" when it names the nation's own (NR-941). A firm
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

    // The record whose span holds the year: the cradle's, latest first. The
    // cradle's three records are kept in that order so the tenure walk below
    // can step to the one before.
    const era_timelapse* recs[3] = {nullptr, nullptr, nullptr};
    int                  at      = -1;
    for (const generation_report::body_entry& be : rep.bodies)
    {
        const era_timelapse* order[3] = {&be.industrialisation_timelapse,
                                         &be.exploration_timelapse, &be.prehistory_timelapse};
        for (int j = 0; j < 3; ++j)
        {
            const era_timelapse* t = order[j];
            if (t->changes.empty()) continue;
            if (cc.founded_year >= t->start_year && cc.founded_year <= t->start_year + t->years)
            {
                at = j;
                break;
            }
        }
        if (at >= 0)
        {
            for (int j = 0; j < 3; ++j) recs[j] = order[j];
            break;
        }
    }
    const era_timelapse* rec = at >= 0 ? recs[at] : nullptr;

    // The holder of the origin region at the year -- the record's own slice,
    // the fold every reader of a record uses -- and the year it took it: the
    // last change of that region's owner at or before the year.
    const std::size_t origin      = static_cast<std::size_t>(cc.origin_region);
    const auto        taken_since = [origin](const era_timelapse& t, int32_t year) {
        int32_t  since = t.start_year;
        uint16_t prev  = owner_none;
        for (const owner_change& ch : t.changes)
        {
            if (ch.year > year) break; // ascending by year
            if (ch.region != origin) continue;
            if (ch.owner != prev) since = ch.year;
            prev = ch.owner;
        }
        return since;
    };
    std::string realm;
    int32_t     since = 0;
    if (rec != nullptr)
    {
        const std::vector<uint16_t> slice  = owner_slice_at(*rec, cc.founded_year);
        const uint16_t              owner  = origin < slice.size() ? slice[origin] : owner_none;
        if (owner != owner_none)
        {
            since = taken_since(*rec, cc.founded_year);
            // "SINCE" IS TENURE, NOT THE RECORD'S OPENING (Ben, 2026-09-25,
            // NR-939; STARTUP.md § The seat). A region held from before a
            // record opens changes AT its opening, so while `since` sits on the
            // opening and the SAME polity id holds the region at the close of
            // the record before, the walk reads that record too. A polity id
            // is one table across the three records (a resumed span inherits
            // its realms by id), so the step is well-defined; a different
            // holder at the earlier close means the realm took it at the seam.
            for (int j = at + 1; j < 3; ++j)
            {
                const era_timelapse* earlier = recs[j];
                if (earlier == nullptr || earlier->changes.empty()) break;
                if (since != recs[j - 1]->start_year) break;
                const int32_t               close = earlier->start_year + earlier->years;
                const std::vector<uint16_t> held  = owner_slice_at(*earlier, close);
                if (origin >= held.size() || held[origin] != owner) break;
                since = taken_since(*earlier, close);
            }
            if (owner < rec->polity_name.size() && !rec->polity_name[owner].empty())
                realm = rec->polity_name[owner];
            else
            {
                const std::vector<int32_t> seat = polity_seat_region(*rec, polity_first_region(*rec));
                if (owner < seat.size() && seat[owner] >= 0
                    && static_cast<std::size_t>(seat[owner]) < ss->regions.size())
                    realm = ss->regions[static_cast<std::size_t>(seat[owner])].name;
            }
        }
    }

    std::string s = "Chartered from " + city + "'s industry, in " + region;
    if (nat != nullptr) s += ", under " + nat->name;
    // An UNDATED firm (`founded_year` 0: an origin the walk stamped, a year no
    // finish dated -- a fixture, or a save from before the pairing) keeps the
    // sentence short of any year: year 0 falls inside the Empires record's
    // span, and reading a realm off it would name a realm the firm never knew.
    //
    // ONE NAME WHEN THE TWO AGREE (Ben, 2026-09-25, NR-941). A nation carries
    // its founding realm's coined name (BL-1089), so on ground the nation's own
    // realm held the full form names it twice; the sentence then says "its own
    // realm". The full form stays for ground some other realm held.
    if (cc.founded_year != 0 && !realm.empty() && nat != nullptr && realm == nat->name)
        s += ", its own realm since " + ui::lapse_year_label(since);
    else if (cc.founded_year != 0 && !realm.empty())
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


/// BL-1281: the on-screen name of a national budget line.
const char* budget_line_word(std::size_t i)
{
    switch (static_cast<budget_priority>(i))
    {
    case budget_priority::logistics_maintenance: return "Logistics maintenance";
    case budget_priority::schooling:             return "Schooling";
    case budget_priority::military_research:     return "Military research";
    case budget_priority::academic_research:     return "Academic research";
    case budget_priority::public_exploration:    return "Public exploration";
    case budget_priority::contracted_force:      return "Contracted force";
    case budget_priority::strategic_reserve:     return "Strategic reserve";
    case budget_priority::public_works:          return "Public works";
    case budget_priority::charters:              return "Charters";
    case budget_priority::space_programme:       return "Space programme";
    default:                                     break;
    }
    return "Other";
}

/// BL-1281 -- THE NATION'S FIGURES, read off the world as it stands at the
/// seat: no new state, nothing stored. GDP is NATIONS.md § GDP's reading,
/// "the value of production on its tiles at local market prices": the last
/// cleared quarter's output of every building on the nation's ground, valued
/// at its market's price (`estimate_building_profit`'s revenue), times the
/// four quarters of a year. A world no quarter has cleared on reads none.
struct nation_figures
{
    int    tiles      = 0;
    double population = 0.0; ///< thousands
    int    centres    = 0;
    double gdp_year   = 0.0;
    bool   gdp_read   = false;
};

nation_figures figures_of(const world& w, const recipe_registry& reg, const economy_report& rep,
                          entity_id nation, const nation_component& nat)
{
    nation_figures f;
    f.tiles = static_cast<int>(nat.tiles.size());
    for (const auto& [pc, comp] : w.population_centres)
    {
        if (comp.razed) continue;
        const auto t = w.population_centre_tile.find(pc);
        if (t == w.population_centre_tile.end()) continue;
        const auto n = w.tile_to_nation.find(t->second);
        if (n == w.tile_to_nation.end() || n->second != nation) continue;
        f.population += comp.population;
        ++f.centres;
    }
    for (const auto& [bid, b] : w.buildings)
    {
        const auto n = w.tile_to_nation.find(b.tile);
        if (n == w.tile_to_nation.end() || n->second != nation) continue;
        const building_profit p = estimate_building_profit(w, reg, rep, bid);
        if (!p.has_data) continue;
        f.gdp_read = true;
        f.gdp_year += static_cast<double>(p.revenue) * 4.0; // one economy tick is one quarter (budget_system.hpp)
    }
    return f;
}

/// "Iron Ore 412, Coal 220, ..." -- the nation's richest deposits by its summed
/// abundance (`nation_component::resource_abundance`, Pass 3), and how many of
/// the goods its ground carries at all.
std::string deposits_line(const nation_component& nat, int& present)
{
    std::array<std::size_t, resource_count> order{};
    present = 0;
    for (std::size_t i = 0; i < resource_count; ++i)
    {
        order[i] = i;
        if (nat.resource_abundance[i] > 0.0f) ++present;
    }
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return nat.resource_abundance[a] > nat.resource_abundance[b];
    });
    std::string s;
    for (std::size_t k = 0; k < 5 && k < resource_count; ++k)
    {
        const std::size_t i = order[k];
        if (nat.resource_abundance[i] <= 0.0f) break;
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s%s %.0f", s.empty() ? "" : ", ",
                      ui::resource_name(static_cast<resource_type>(i)),
                      static_cast<double>(nat.resource_abundance[i]));
        s += buf;
    }
    return s.empty() ? std::string("none on its ground") : s;
}

void card_heading(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(170, 190, 220, 255));
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

} // namespace

void app::spectate_from_seat()
{
    // BL-1334: the same session --spectate opens. The flag is set and the
    // seat is left empty: take_seat_pick is NOT called, so no take_seat
    // command is issued and nobody becomes the player. Play opens clean, as
    // Confirm's does: the history link's ledger and filter close.
    m_ui.spectating = true;
    if (m_seat_ui.history_open) m_ui.show_tile_ledger = false;
    m_seat_ui.history_open     = false;
    m_seat_ui.briefing         = null_entity;
    m_ui.history_nation_filter = null_entity;
    std::printf("[seat] Spectate: the world opens with nobody seated (player_entity %lld kept as the viewpoint anchor only)\n",
                static_cast<long long>(m_world.player_entity));
    std::fflush(stdout);
    if (m_seat_ui.from_verify) m_screen = app_screen::in_game;
    else                       finish_new_game();
}

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

    const auto&                   cands  = m_seat_result.candidates;
    const std::vector<seat_major> majors = seat_majors();
    if (majors.empty())
    {
        // No major at all: nothing to offer. Say so, and let the player go on
        // with whatever the generator flagged rather than strand them here.
        ImGui::TextUnformatted("This world holds no major nation to seat.");
        if (ImGui::Button("Open play"))
        {
            if (m_seat_ui.from_verify) m_screen = app_screen::in_game;
            else                       finish_new_game();
        }
        ImGui::End();
        return;
    }
    m_seat_ui.hovered = std::clamp(m_seat_ui.hovered, 0, static_cast<int>(majors.size()) - 1);
    const generation_report::body_entry* home = ui::homeworld_entry(m_generation_report);

    // --- the header ------------------------------------------------------
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
    ImGui::TextUnformatted("WHICH NATION ARE YOU?");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::text_secondary);
    int majors_clear = 0;
    for (const seat_major& m : majors)
        if (cands[static_cast<std::size_t>(m.candidate)].shortlisted) ++majors_clear;
    // BL-1406 (STARTUP.md § The seat): the list is of NATIONS. Picking one is
    // becoming it - governing it and building every building on its ground. The
    // champion it still names under the hood (take_seat seats it until BL-1407
    // deletes the corporation type) is not shown.
    ImGui::Text("%d major nations. Taking a nation is governing it: you build every building on its "
                "ground. Ranked by its ground: %d clear the viability floor; the marked ones do not, "
                "and you may still take one.",
                static_cast<int>(majors.size()), majors_clear);
    // BL-1089: THE NATION COUNT EXPLAINS THE MERGES (NATION_GENERATION.md
    // § Pass 2d), read off the saved report's fold record.
    if (home != nullptr)
    {
        int realms = 0, absorbed = 0, ownerless = 0;
        for (std::size_t n = 0; n < home->nation_polity.size(); ++n)
        {
            if (home->nation_polity[n] >= 0) ++realms; else ++ownerless;
            if (n < home->nation_absorbed_count.size()) absorbed += std::max<int32_t>(0, home->nation_absorbed_count[n]);
        }
        ImGui::Text("%zu nations: %d realms of the close, %d realms absorbed under the floor, %d of ground no realm held. "
                    "Only the majors can be played.",
                    home->nation_ids.size(), realms, absorbed, ownerless);
    }
    ImGui::PopStyleColor();
    // BL-1334: the third answer to the screen's question -- no nation. Always
    // shown, so watching needs no row pressed first.
    if (ImGui::Button("Spectate"))
    {
        spectate_from_seat();
        ImGui::End();
        return;
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::text_secondary);
    ImGui::TextUnformatted("or take no nation: open the world with nobody seated and watch every "
                           "nation play itself.");
    ImGui::PopStyleColor();
    ImGui::Dummy({1.0f, 6.0f});

    const float pad     = ImGui::GetStyle().ItemSpacing.x;
    const float avail   = ImGui::GetContentRegionAvail().x;
    const float list_w  = std::max(340.0f, avail * 0.30f);
    const float right_w = std::max(200.0f, avail - list_w - pad);
    const float body_h  = ImGui::GetContentRegionAvail().y;

    // The nation the map and the card show: the open briefing's, else the hovered row.
    int shown = m_seat_ui.hovered;
    if (m_seat_ui.briefing != null_entity)
        for (int i = 0; i < static_cast<int>(majors.size()); ++i)
            if (majors[static_cast<std::size_t>(i)].nation == m_seat_ui.briefing)
                shown = i;

    // --- the list of majors ----------------------------------------------
    ImGui::BeginChild("##seat_list", {list_w, body_h}, ImGuiChildFlags_Borders);
    if (ImGui::BeginTable("##seat_rows", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 22.0f);
        ImGui::TableSetupColumn("Nation", ImGuiTableColumnFlags_WidthStretch, 2.6f);
        ImGui::TableSetupColumn("Treasury", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(majors.size()); ++i)
        {
            const seat_major& m = majors[static_cast<std::size_t>(i)];
            const spawn_seat_candidate& c = cands[static_cast<std::size_t>(m.candidate)];
            const auto cit = m_world.corporations.find(m.champion);
            const auto nit = m_world.nations.find(m.nation);
            if (cit == m_world.corporations.end() || nit == m_world.nations.end())
                continue;
            const bool marked = !c.shortlisted;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushID(i);
            // The row is one Selectable spanning every column: hover points the
            // map at the nation; a press opens its briefing, and pressing the
            // row whose briefing is open closes it (the toggle rule).
            const bool open = (m_seat_ui.briefing == m.nation);
            char label[16];
            std::snprintf(label, sizeof label, "%d", i + 1);
            if (ImGui::Selectable(label, open || i == shown, ImGuiSelectableFlags_SpanAllColumns))
                m_seat_ui.briefing = open ? null_entity : m.nation;
            if (ImGui::IsItemHovered())
                m_seat_ui.hovered = i; // the last hovered row stays shown when the pointer leaves
            ImGui::PopID();

            ImGui::TableSetColumnIndex(1);
            const ImVec2 sw = ImGui::GetCursorScreenPos();
            const float  lh = ImGui::GetTextLineHeight();
            ImGui::GetWindowDrawList()->AddRectFilled({sw.x, sw.y + 2.0f}, {sw.x + lh - 4.0f, sw.y + lh - 2.0f},
                                                      ui::palette::nation_colour(m.nation));
            ImGui::Dummy({lh - 2.0f, lh});
            ImGui::SameLine();
            if (marked) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 158, 255));
            ImGui::TextUnformatted(nit->second.name.c_str());
            if (marked)
            {
                ImGui::PopStyleColor();
                ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::pinned);
                ImGui::TextUnformatted("below the floor");
                ImGui::PopStyleColor();
            }
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.0f cr", static_cast<double>(nit->second.treasury));
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // --- the map and the card / briefing ---------------------------------
    ImGui::BeginChild("##seat_right", {right_w, body_h});
    const seat_major&           sm  = majors[static_cast<std::size_t>(shown)];
    const spawn_seat_candidate& sc  = cands[static_cast<std::size_t>(sm.candidate)];
    const auto                  scit = m_world.corporations.find(sm.champion);
    const corporation_component* scc =
        scit != m_world.corporations.end() ? &scit->second : nullptr;
    const nation_component* nat = nullptr;
    if (const auto nit = m_world.nations.find(sm.nation); nit != m_world.nations.end())
        nat = &nit->second;
    const entity_id hq_tile = scc ? hq_tile_of(m_world, *scc) : null_entity;
    entity_id body = null_entity;
    if (const auto t = m_world.tiles.find(hq_tile); t != m_world.tiles.end())
        body = t->second.body;
    if (body == null_entity && nat != nullptr)
        if (const auto t = m_world.tiles.find(nat->capital_tile); t != m_world.tiles.end())
            body = t->second.body;
    if (body == null_entity)
        body = m_launch_body;

    // Bake the body's political base once: water, and land tinted by its
    // nation; and, in the same pass, which nations share a land border.
    if (body != null_entity && body != m_seat_ui.body)
    {
        m_seat_ui.body = body;
        m_seat_ui.base.clear();
        m_seat_ui.borders.clear();
        m_seat_ui.territory.clear();
        m_seat_ui.territory_of = null_entity;
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
            std::vector<entity_id> owner(grid.size(), null_entity);
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
                            {
                                colour   = mix(colour, ui::palette::nation_colour(nit->second), 0.35f);
                                owner[k] = nit->second;
                            }
                        }
                    }
                    if (col == 0) { cur = colour; continue; }
                    if (col < gw && colour == cur) continue;
                    m_seat_ui.base.push_back({row, c0, col, cur});
                    c0  = col;
                    cur = colour;
                }
            }
            // A land border: two land tiles side by side (east, column-wrapped,
            // and south) held by different nations.
            for (int row = 0; row < gh; ++row)
                for (int col = 0; col < gw; ++col)
                {
                    const entity_id a = owner[static_cast<std::size_t>(row) * gw + col];
                    if (a == null_entity) continue;
                    const entity_id e = owner[static_cast<std::size_t>(row) * gw + (col + 1) % gw];
                    const entity_id s = row + 1 < gh ? owner[static_cast<std::size_t>(row + 1) * gw + col]
                                                     : null_entity;
                    for (const entity_id b : {e, s})
                        if (b != null_entity && b != a)
                            m_seat_ui.borders.push_back({std::min(a, b), std::max(a, b)});
                }
            std::sort(m_seat_ui.borders.begin(), m_seat_ui.borders.end());
            m_seat_ui.borders.erase(std::unique(m_seat_ui.borders.begin(), m_seat_ui.borders.end()),
                                    m_seat_ui.borders.end());
        }
    }

    // The shown nation's territory, run-merged (re-merged when it changes).
    if (sm.nation != m_seat_ui.territory_of && m_seat_ui.gw > 0)
    {
        m_seat_ui.territory_of = sm.nation;
        m_seat_ui.territory.clear();
        const std::vector<entity_id>& grid = body_tile_grid(m_world, m_seat_ui.body);
        const int gw = m_seat_ui.gw;
        if (grid.size() == static_cast<std::size_t>(gw) * m_seat_ui.gh)
            for (int row = 0; row < m_seat_ui.gh; ++row)
            {
                int start = -1;
                for (int col = 0; col <= gw; ++col)
                {
                    bool in = false;
                    if (col < gw)
                        if (const auto nit = m_world.tile_to_nation.find(grid[static_cast<std::size_t>(row) * gw + col]);
                            nit != m_world.tile_to_nation.end())
                            in = nit->second == sm.nation;
                    if (in && start < 0) start = col;
                    if (!in && start >= 0)
                    {
                        m_seat_ui.territory.push_back({row, start, col, 0});
                        start = -1;
                    }
                }
            }
    }

    const entity_id home_market = hq_tile != null_entity ? market_for_tile(m_world, hq_tile)
                                                         : null_entity;

    // The map takes the top of the pane, capped so the card keeps the rest.
    const float map_wmax = ImGui::GetContentRegionAvail().x;
    const float map_hmax = std::max(80.0f, ImGui::GetContentRegionAvail().y * 0.40f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (m_seat_ui.gw > 0 && m_seat_ui.gh > 0)
    {
        const float cell  = std::min(map_wmax / static_cast<float>(m_seat_ui.gw),
                                     map_hmax / static_cast<float>(m_seat_ui.gh));
        const float map_w = cell * static_cast<float>(m_seat_ui.gw);
        const float map_h = cell * static_cast<float>(m_seat_ui.gh);
        const ImVec2 tl   = ImGui::GetCursorScreenPos();
        ImGui::Dummy({map_w, map_h}); // the map takes no input: pointing is done at the list

        // Run edges snap to whole pixels: at a fractional cell an unsnapped
        // row edge leaves a hairline seam of background between rows.
        const auto run_rect = [&](const seat_screen_state::run& r, ImU32 colour) {
            dl->AddRectFilled({std::floor(tl.x + r.c0 * cell), std::floor(tl.y + r.row * cell)},
                              {std::floor(tl.x + r.c1 * cell), std::floor(tl.y + (r.row + 1) * cell)},
                              colour);
        };
        for (const auto& r : m_seat_ui.base)
            run_rect(r, r.colour);
        // THE CARD'S PART 1: the nation's territory, lit in its own colour.
        const ImU32 lit = mix(ui::palette::nation_colour(sm.nation), IM_COL32(255, 255, 255, 255), 0.30f);
        for (const auto& r : m_seat_ui.territory)
            run_rect(r, lit);

        const auto at = [&](entity_id tile, ImVec2& out) {
            const auto t = m_world.tiles.find(tile);
            if (t == m_world.tiles.end() || t->second.body != body) return false;
            out = {tl.x + (t->second.grid_x + 0.5f) * cell, tl.y + (t->second.grid_y + 0.5f) * cell};
            return true;
        };
        // Every other major's champion HQ, faint: the shown nation reads as a
        // choice among rivals rather than a lone patch.
        for (const seat_major& m : majors)
        {
            if (m.nation == sm.nation) continue;
            const auto cit = m_world.corporations.find(m.champion);
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
    if (!scc || !nat)
    {
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    // --- facts both the card and the briefing read ---------------------
    const std::string city = home_market != null_entity ? ui::market_city_name(m_world, home_market)
                                                        : std::string("no market");

    // --- BL-1089: WHICH REALM THE NATION IS, and the region under the city --
    // Read off the saved report's fold record (NATION_GENERATION.md § Pass 2d).
    // The revolution line (BL-1294) is the timeline's now, one row of it.
    std::string realm_line;
    std::string city_region; // "(in <region>)" on the HQ line
    if (home != nullptr)
    {
        const std::vector<std::string>* names =
            !home->industrialisation_timelapse.polity_name.empty() ? &home->industrialisation_timelapse.polity_name
          : !home->exploration_timelapse.polity_name.empty()       ? &home->exploration_timelapse.polity_name
          : !home->prehistory_timelapse.polity_name.empty()        ? &home->prehistory_timelapse.polity_name
                                                                    : nullptr;
        const auto realm_name = [&](int32_t pol) -> std::string {
            if (names != nullptr && pol >= 0 && static_cast<std::size_t>(pol) < names->size()
                && !(*names)[static_cast<std::size_t>(pol)].empty())
                return (*names)[static_cast<std::size_t>(pol)];
            return "an unnamed realm";
        };
        const int n = ui::nation_fold_slot(*home, sm.nation);
        if (n >= 0 && static_cast<std::size_t>(n) < home->nation_polity.size())
        {
            const std::size_t sn  = static_cast<std::size_t>(n);
            const int32_t     pol = home->nation_polity[sn];
            if (pol < 0)
                realm_line = "Ground no realm held at the close; named at the fold.";
            else
            {
                const int32_t first = sn < home->nation_absorbed_first.size() ? home->nation_absorbed_first[sn] : 0;
                const int32_t count = sn < home->nation_absorbed_count.size() ? home->nation_absorbed_count[sn] : 0;
                if (count <= 0)
                    realm_line = "The realm as it stood at the close.";
                else
                {
                    realm_line = "Absorbed under the floor: ";
                    for (int32_t k = 0; k < count; ++k)
                    {
                        const std::size_t ab = static_cast<std::size_t>(first + k);
                        if (ab >= home->nation_absorbed.size()) break;
                        if (k > 0) realm_line += k + 1 == count ? " and " : ", ";
                        realm_line += realm_name(home->nation_absorbed[ab]);
                    }
                    realm_line += ".";
                }
            }
        }
        if (hq_tile != null_entity && !home->settlement.regions.empty())
        {
            if (const auto t = m_world.tiles.find(hq_tile); t != m_world.tiles.end())
            {
                // The homeworld's own grid width for the column wrap — the
                // grid `region::col` and `tile_component::grid_x` index.
                const int gw = home_grid_width;
                int best = -1; long best_d = 0;
                for (std::size_t r = 0; r < home->settlement.regions.size(); ++r)
                {
                    const region& rg = home->settlement.regions[r];
                    int dc = std::abs(rg.col - t->second.grid_x);
                    if (gw > 0 && dc > gw / 2) dc = gw - dc;
                    const int  dr = std::abs(rg.row - t->second.grid_y);
                    const long d  = static_cast<long>(dc) * dc + static_cast<long>(dr) * dr;
                    if (best < 0 || d < best_d) { best = static_cast<int>(r); best_d = d; }
                }
                if (best >= 0 && !home->settlement.regions[static_cast<std::size_t>(best)].name.empty())
                    city_region = " (in " + home->settlement.regions[static_cast<std::size_t>(best)].name + ")";
            }
        }
    }

    // The card / briefing scrolls inside the rest of the pane.
    ImGui::BeginChild("##seat_card", {0.0f, 0.0f}, ImGuiChildFlags_None);
    ui::foldout_scroll_child("##seat_card"); // verify.scroll_panel("seat_card", ...)
    // Confirm and Back head the pane when a row is selected, so the pick is
    // reachable without scrolling past the card (Ben, 2026-10-15).
    const auto confirm_row = [&]() {
        if (ImGui::Button("Confirm"))
        {
            // THE PICK, as a game act: the major, through the seam as
            // corp_verb::take_seat on its champion.
            const corp_command_result r = take_seat_pick(sm.nation);
            if (r == corp_command_result::applied)
            {
                // Play opens clean: the history link's ledger and filter close.
                if (m_seat_ui.history_open) m_ui.show_tile_ledger = false;
                m_seat_ui.history_open     = false;
                m_ui.history_nation_filter = null_entity;
                if (m_seat_ui.from_verify) m_screen = app_screen::in_game;
                else                       finish_new_game();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Back"))
            m_seat_ui.briefing = null_entity; // nothing changed: the world was only read
    };
    // THE CARD IS ALWAYS SHOWN for the shown nation (Ben, 2026-10-15, live
    // click): it used to give way to the briefing the moment a row was
    // pressed, so the selected nation was the one nation with no card. The
    // briefing prose sits BELOW the card of the selected row; Confirm heads it.
    if (m_seat_ui.briefing != null_entity)
    {
        confirm_row();
        ImGui::Separator();
    }
    {
        // --- THE NATION CARD: seven parts (STARTUP.md § The seat) ------------
        // Its question: which nation am I? Two columns: the nation's own facts
        // on the left, its history on the right (the champion line retired with
        // the champion, Ben 2026-10-23 - BL-1406).
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
        ImGui::Text("%s, a major nation: %s %s state, %s on its borders.", nat->name.c_str(),
                    nat->politics == ideology::isolationist ? "an" : "a",
                    politics_word(nat->politics), posture_word(nat->posture));
        ImGui::PopStyleColor();

        const nation_figures fig = figures_of(m_world, m_registry, m_last_econ_report, sm.nation, *nat);
        if (ImGui::BeginTable("##nation_card", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushTextWrapPos(0.0f);

            // (1) territory
            card_heading("Territory");
            ImGui::Text("%d tiles, lit on the map.", fig.tiles);

            // (2) population and GDP
            card_heading("Population and GDP");
            ImGui::Text("Population %.2f million in %d centres.", fig.population / 1000.0, fig.centres);
            if (fig.gdp_read)
            {
                const double per_head = fig.population > 0.0 ? fig.gdp_year / (fig.population * 1000.0) : 0.0;
                ImGui::Text("GDP %.0f cr a year (%.2f cr a head): its ground's last quarter of output, "
                            "at its markets' prices.", fig.gdp_year, per_head);
            }
            else
                ImGui::TextUnformatted("GDP: no quarter has cleared on this world yet.");

            // (3) treasury and budget weights
            card_heading("Treasury and budget");
            ImGui::Text("Treasury %.0f cr.", static_cast<double>(nat->treasury));
            if (const auto bit = m_world.nation_budgets.find(sm.nation); bit != m_world.nation_budgets.end())
            {
                const nation_budget& nb = bit->second;
                std::array<std::size_t, priority_count> order{};
                for (std::size_t i = 0; i < priority_count; ++i) order[i] = i;
                std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                    return nb.weights[a] > nb.weights[b];
                });
                std::string lines;
                for (const std::size_t i : order)
                {
                    if (nb.weights[i] <= 0.0f) break;
                    char buf[80];
                    std::snprintf(buf, sizeof buf, "%s%s %.0f%%", lines.empty() ? "" : ", ",
                                  budget_line_word(i), static_cast<double>(nb.weights[i] * 100.0f));
                    lines += buf;
                }
                ImGui::Text("Budget: %s; %.0f%% held in reserve.",
                            lines.empty() ? "no line weighted" : lines.c_str(),
                            static_cast<double>(std::clamp(nb.reserve_fraction, 0.0f, 1.0f) * 100.0f));
            }
            else
                ImGui::TextUnformatted("Budget: no weights set yet.");

            // (5) resource profile and deposits
            card_heading("Resources");
            int present = 0;
            const std::string deps = deposits_line(*nat, present);
            ImGui::Text("Richest deposits: %s. %d of %d goods lie on its ground.", deps.c_str(), present,
                        static_cast<int>(resource_count));

            // (6) neighbours, with their reading and any grudge
            card_heading("Neighbours");
            // No nation-to-nation stance is declared at the seat, so the reading
            // shown is the derived one (RELATIONS.md: sentiment), both ways. A
            // negative Trust before play is the grudge record's seeding
            // (grudge_sentiment.hpp, BL-898): the old quarrel, continued.
            int shown_n = 0, more = 0;
            for (const auto& [a, b] : m_seat_ui.borders)
            {
                if (a != sm.nation && b != sm.nation) continue;
                const entity_id nb = a == sm.nation ? b : a;
                const auto nit = m_world.nations.find(nb);
                if (nit == m_world.nations.end()) continue;
                if (shown_n >= 6) { ++more; continue; }
                ++shown_n;
                bool is_major = false;
                for (const seat_major& m : majors) if (m.nation == nb) is_major = true;
                const sentiment_value theirs = sentiment_toward(m_world.sentiment, nb, sm.nation);
                const sentiment_value ours   = sentiment_toward(m_world.sentiment, sm.nation, nb);
                std::string grudge;
                if (theirs.trust < 0.0f && ours.trust < 0.0f) grudge = "; a grudge both ways";
                else if (theirs.trust < 0.0f)                grudge = "; it holds a grudge";
                else if (ours.trust < 0.0f)                  grudge = "; you hold a grudge";
                ImGui::Text("%s (%s): reads you Access %+.2f, Trust %+.2f; you read it Access %+.2f, Trust %+.2f%s.",
                            nit->second.name.c_str(), is_major ? "major" : "minor",
                            static_cast<double>(theirs.access), static_cast<double>(theirs.trust),
                            static_cast<double>(ours.access), static_cast<double>(ours.trust),
                            grudge.c_str());
            }
            if (shown_n == 0)
                ImGui::TextUnformatted("No nation shares a land border with it.");
            else if (more > 0)
                ImGui::Text("+%d more.", more);
            ImGui::PopTextWrapPos();

            ImGui::TableSetColumnIndex(1);
            ImGui::PushTextWrapPos(0.0f);


            // (7) history (BL-1295)
            card_heading("History");
            if (home != nullptr)
            {
                const std::string tags = ui::nation_pattern_tags_text(*home, sm.nation);
                ImGui::Text("Patterns: %s.", tags.empty() ? "none of the five" : tags.c_str());
                if (!realm_line.empty())
                    ImGui::TextUnformatted(realm_line.c_str());
                const std::vector<ui::nation_history_row> rows = ui::nation_timeline(*home, sm.nation, 8);
                for (const ui::nation_history_row& r : rows)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::text_secondary);
                    ImGui::Text("%-9s", ui::lapse_year_label(r.year).c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                    ImGui::TextUnformatted(r.text.c_str());
                }
                if (rows.empty())
                    ImGui::TextUnformatted("No recorded event names its line.");
                // The link into the History ledger, filtered to this nation. A
                // toggle: pressed while open, it closes the ledger.
                const bool open_here = m_seat_ui.history_open && m_ui.history_nation_filter == sm.nation;
                if (ImGui::SmallButton(open_here ? "Close its history" : "Open its history in the History ledger"))
                {
                    if (open_here)
                    {
                        m_seat_ui.history_open = false;
                        m_ui.show_tile_ledger  = false;
                    }
                    else
                    {
                        m_seat_ui.history_open   = true;
                        m_ui.show_tile_ledger    = true;
                        m_ui.history_view        = ui::history_view_ages;
                        m_ui.history_nation_filter = sm.nation;
                    }
                }
            }
            else
                ImGui::TextUnformatted("This world carries no generation record.");
            ImGui::PopTextWrapPos();
            ImGui::EndTable();
        }
        if (m_seat_ui.briefing == null_entity)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::text_secondary);
            ImGui::TextUnformatted("Press a row to read its briefing.");
            ImGui::PopStyleColor();
        }
    }
    if (m_seat_ui.briefing != null_entity)
    {
        ImGui::Separator();
        // --- THE BRIEFING: the opening position, in prose -------------------
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(225, 230, 240, 255));
        ImGui::Text("Briefing: %s", nat->name.c_str());
        ImGui::PopStyleColor();
        ImGui::PushTextWrapPos(0.0f);
        // The nation's grain first: whose realm this is, and whose law.
        ImGui::Text("You are %s, %s %s state, %s on its borders, with %.0f cr in its treasury. You "
                    "govern it, and you build every building on its ground.", nat->name.c_str(),
                    nat->politics == ideology::isolationist ? "an" : "a",
                    politics_word(nat->politics), posture_word(nat->posture),
                    static_cast<double>(nat->treasury));
        // BL-1099 / R22: one sentence of origin, at the champion's grain where
        // it names the charter and the nation's where it names the realm.
        if (const std::string origin = seat_origin_sentence(m_world, m_generation_report, *scc, city, nat);
            !origin.empty())
            ImGui::TextUnformatted(origin.c_str());
        if (!sc.shortlisted)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ui::palette::pinned);
            if (sc.holdings_scored == 0)
                ImGui::Text("The floor marked this nation: none of the %d holdings on its ground stands in a market "
                            "the landscape scored, so none stands where a chain closes and a "
                            "resource sits inside the pin band. You may still take it.",
                            sc.holdings);
            else
                ImGui::Text("The floor marked this nation: %d of the %d holdings on its ground stand in scored "
                            "markets, but in none of them does a chain close with a resource "
                            "inside the pin band. You may still take it.",
                            sc.holdings_scored, sc.holdings);
            ImGui::PopStyleColor();
        }
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();

    ImGui::EndChild();
    ImGui::End();

    // BL-1295: THE HISTORY LINK'S LEDGER -- the real History ledger, drawn over
    // the screen in its own column, on its Ages view, filtered to the nation.
    // Its own close (re-pressing its active tab) ends the link as well.
    if (m_seat_ui.history_open && m_screen == app_screen::choosing_seat)
    {
        if (!m_ui.show_tile_ledger)
            m_seat_ui.history_open = false;
        else
        {
            ui::draw_tile_inspector(m_world, m_ui, m_generation_report, m_active_world_params,
                                    &m_ui.show_tile_ledger);
            // The ledger's window may predate this screen's (an in-game frame
            // drew it first), and both refuse bring-to-front-on-focus, so the
            // screen would paint over it: lift it above the screen explicitly.
            if (ImGuiWindow* lw = ImGui::FindWindowByName("Tile Ledger"))
                ImGui::BringWindowToDisplayFront(lw);
        }
    }
}
