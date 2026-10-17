/// @file nation_history.cpp
/// A nation's history, read off the generation report (BL-1295). See the header.

#include "ui/nation_history.hpp"

#include "ui/history_lapse.hpp"                 // lapse_year_label
#include "world/history_pattern_detectors.hpp" // history_pattern, history_pattern_bit

#include <algorithm>

namespace ui {

namespace {

/// The on-screen word for each pattern's tag (history_pattern_detectors.hpp
/// names them beside the enumerators; GLOSSARY.md § History pattern).
const char* pattern_tag_word(history_pattern p)
{
    switch (p)
    {
    case history_pattern::rise_and_fall: return "rose and fell";
    case history_pattern::late_culture:  return "colonised";
    case history_pattern::world_war:     return "world war";
    case history_pattern::civil_war:     return "survived civil war";
    case history_pattern::revolution:    return "revolution";
    case history_pattern::count:         break;
    }
    return "";
}

bool in_set(const std::vector<uint16_t>& set, uint16_t id)
{
    return std::binary_search(set.begin(), set.end(), id);
}

/// The realm's name: the record's own table first (a revolution renames, and
/// the record an event sits in carries the name it bore then), then any other
/// record's, else a plain stand-in -- never an invented name.
std::string realm_name(const generation_report::body_entry& be, const era_timelapse& rec, uint16_t id)
{
    if (id < rec.polity_name.size() && !rec.polity_name[id].empty())
        return rec.polity_name[id];
    for (const era_timelapse* t : {&be.industrialisation_timelapse, &be.exploration_timelapse,
                                   &be.prehistory_timelapse})
        if (id < t->polity_name.size() && !t->polity_name[id].empty())
            return t->polity_name[id];
    return "an unnamed realm";
}

std::string region_name(const generation_report::body_entry& be, uint16_t region)
{
    if (region != lapse_event_none && region < be.settlement.regions.size()
        && !be.settlement.regions[region].name.empty())
        return be.settlement.regions[region].name;
    return {};
}

} // namespace

const generation_report::body_entry* homeworld_entry(const generation_report& rep)
{
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld && !b.nation_ids.empty())
            return &b;
    return nullptr;
}

int nation_fold_slot(const generation_report::body_entry& be, entity_id nation)
{
    for (std::size_t n = 0; n < be.nation_ids.size(); ++n)
        if (be.nation_ids[n] == nation)
            return static_cast<int>(n);
    return -1;
}

std::vector<uint16_t> nation_lineage(const generation_report::body_entry& be, entity_id nation)
{
    std::vector<uint16_t> set;
    const int n = nation_fold_slot(be, nation);
    if (n < 0 || static_cast<std::size_t>(n) >= be.nation_polity.size())
        return set;
    const std::size_t sn = static_cast<std::size_t>(n);
    if (be.nation_polity[sn] >= 0)
        set.push_back(static_cast<uint16_t>(be.nation_polity[sn]));
    const int32_t first = sn < be.nation_absorbed_first.size() ? be.nation_absorbed_first[sn] : 0;
    const int32_t count = sn < be.nation_absorbed_count.size() ? be.nation_absorbed_count[sn] : 0;
    for (int32_t k = 0; k < count; ++k)
    {
        const std::size_t at = static_cast<std::size_t>(first + k);
        if (at < be.nation_absorbed.size() && be.nation_absorbed[at] >= 0)
            set.push_back(static_cast<uint16_t>(be.nation_absorbed[at]));
    }
    std::sort(set.begin(), set.end());
    set.erase(std::unique(set.begin(), set.end()), set.end());

    // The parent line: a successor's parent, walked to a fixed point. Bounded
    // -- each pass either adds an id or stops, and ids are finite.
    const era_timelapse* recs[3] = {&be.prehistory_timelapse, &be.exploration_timelapse,
                                    &be.industrialisation_timelapse};
    for (bool grew = !set.empty(); grew;)
    {
        grew = false;
        for (const era_timelapse* t : recs)
            for (const lapse_event& e : t->events)
            {
                const auto k = static_cast<lapse_event_kind>(e.kind);
                if (k != lapse_event_kind::broke_away && k != lapse_event_kind::schism
                    && k != lapse_event_kind::seceded_at_close)
                    continue;
                if (e.other == lapse_event_none || !in_set(set, e.polity) || in_set(set, e.other))
                    continue;
                set.insert(std::lower_bound(set.begin(), set.end(), e.other), e.other);
                grew = true;
            }
    }
    return set;
}

std::string nation_pattern_tags_text(const generation_report::body_entry& be, entity_id nation)
{
    const int n = nation_fold_slot(be, nation);
    if (n < 0 || static_cast<std::size_t>(n) >= be.nation_pattern_tags.size())
        return {};
    const uint8_t mask = be.nation_pattern_tags[static_cast<std::size_t>(n)];
    std::string s;
    for (unsigned p = 0; p < static_cast<unsigned>(history_pattern::count); ++p)
    {
        const history_pattern hp = static_cast<history_pattern>(p);
        if ((mask & history_pattern_bit(hp)) == 0) continue;
        if (!s.empty()) s += ", ";
        s += pattern_tag_word(hp);
    }
    return s;
}

std::vector<nation_history_row> nation_timeline(const generation_report::body_entry& be,
                                                entity_id nation, std::size_t max_rows)
{
    const std::vector<uint16_t> line = nation_lineage(be, nation);
    if (line.empty() || max_rows == 0)
        return {};

    struct cand { int32_t year; int order; int rank; std::string text; };
    std::vector<cand> cands;
    int order = 0;
    const era_timelapse* recs[3] = {&be.prehistory_timelapse, &be.exploration_timelapse,
                                    &be.industrialisation_timelapse};
    for (const era_timelapse* t : recs)
        for (const lapse_event& e : t->events)
        {
            ++order;
            const auto  k     = static_cast<lapse_event_kind>(e.kind);
            const bool  mine  = e.polity != lapse_event_none && in_set(line, e.polity);
            const bool  other = e.other != lapse_event_none && in_set(line, e.other);
            const std::string at = region_name(be, e.region);
            const std::string where = at.empty() ? std::string() : " at " + at;
            std::string text;
            int rank = 1; // 1: always key; 2: a conquest, fills what is left
            switch (k)
            {
            case lapse_event_kind::founded:
                if (mine) text = realm_name(be, *t, e.polity) + " was founded" + where + ".";
                break;
            case lapse_event_kind::broke_away:
                if (mine) text = realm_name(be, *t, e.polity) + " broke away from "
                               + realm_name(be, *t, e.other) + ".";
                break;
            case lapse_event_kind::schism:
                if (mine) text = realm_name(be, *t, e.polity) + " split from "
                               + realm_name(be, *t, e.other) + " over creed.";
                break;
            case lapse_event_kind::seceded_at_close:
                if (mine) text = realm_name(be, *t, e.polity) + " seceded from "
                               + realm_name(be, *t, e.other) + ".";
                break;
            case lapse_event_kind::subject_bound:
                if (mine)       text = realm_name(be, *t, e.polity) + " was bound as a subject of "
                                     + realm_name(be, *t, e.other) + ".";
                else if (other) text = realm_name(be, *t, e.other) + " took "
                                     + realm_name(be, *t, e.polity) + " as a subject.";
                break;
            case lapse_event_kind::subject_freed:
                if (mine) text = realm_name(be, *t, e.polity) + " freed itself from "
                               + realm_name(be, *t, e.other) + ".";
                break;
            case lapse_event_kind::exclave_ceded:
                // BL-1310 (NATION_GENERATION.md sec Pass 2e): ground cut off
                // from the realm's core, ceded to the nation around it. A
                // realm can cede several: they fill what is left, as conquests do.
                rank = 2;
                if (mine && e.other != lapse_event_none)
                    text = realm_name(be, *t, e.polity) + " ceded ground it could not reach to "
                         + realm_name(be, *t, e.other) + ".";
                else if (mine)
                    text = realm_name(be, *t, e.polity) + " gave up ground it could not reach.";
                else if (other && e.polity != lapse_event_none)
                    text = realm_name(be, *t, e.other) + " received ground cut off from "
                         + realm_name(be, *t, e.polity) + ".";
                break;
            case lapse_event_kind::absorbed_at_close:
                if (mine)
                    text = realm_name(be, *t, e.polity) + " was absorbed by "
                         + realm_name(be, *t, e.other) + ".";
                else if (other && e.polity != lapse_event_none)
                    text = realm_name(be, *t, e.other) + " absorbed "
                         + realm_name(be, *t, e.polity) + ".";
                break;
            case lapse_event_kind::revolution:
                // The card's revolution line (BL-1294), restated as a row: the
                // old name is the Exploration record's entry for the same id.
                if (mine)
                {
                    const std::vector<std::string>& before = be.exploration_timelapse.polity_name;
                    const std::string old_name = e.polity < before.size() ? before[e.polity] : std::string{};
                    text = "Taken by revolution" + (at.empty() ? std::string() : " from " + at);
                    text += old_name.empty() ? std::string(".") : "; formerly " + old_name + ".";
                }
                break;
            case lapse_event_kind::champion_chartered:
                if (mine) text = realm_name(be, *t, e.polity) + " chartered its national champion" + where + ".";
                break;
            case lapse_event_kind::state_charter:
                if (mine) text = realm_name(be, *t, e.polity)
                               + " chartered its national champion, its treasury paying the gap" + where + ".";
                break;
            case lapse_event_kind::champion_bought_out:
                if (mine)       text = realm_name(be, *t, e.polity) + "'s specialist was bought out by "
                                     + realm_name(be, *t, e.other) + "'s champion.";
                else if (other) text = realm_name(be, *t, e.other) + "'s champion bought out "
                                     + realm_name(be, *t, e.polity) + "'s specialist.";
                break;
            case lapse_event_kind::realm_ended:
                rank = 2;
                if (other && !mine)  text = realm_name(be, *t, e.other) + " ended "
                                          + realm_name(be, *t, e.polity) + ".";
                else if (mine && e.other != lapse_event_none)
                    text = realm_name(be, *t, e.polity) + " fell to " + realm_name(be, *t, e.other) + ".";
                break;
            default:
                break;
            }
            if (!text.empty())
                cands.push_back({e.year, order, rank, std::move(text)});
        }

    // Rank 1 first, oldest first; if rank 1 alone overflows, keep its first row
    // (the founding) and its latest. Conquests fill what is left.
    std::vector<cand> keep;
    std::vector<cand> key, conquest;
    for (cand& c : cands) (c.rank == 1 ? key : conquest).push_back(std::move(c));
    if (key.size() > max_rows)
    {
        keep.push_back(std::move(key.front()));
        for (std::size_t i = key.size() - (max_rows - 1); i < key.size(); ++i)
            keep.push_back(std::move(key[i]));
    }
    else
    {
        keep = std::move(key);
        for (std::size_t i = 0; i < conquest.size() && keep.size() < max_rows; ++i)
            keep.push_back(std::move(conquest[i]));
    }
    std::sort(keep.begin(), keep.end(), [](const cand& a, const cand& b) {
        return a.year != b.year ? a.year < b.year : a.order < b.order;
    });
    std::vector<nation_history_row> out;
    out.reserve(keep.size());
    for (cand& c : keep)
        out.push_back({c.year, std::move(c.text)});
    return out;
}

} // namespace ui
