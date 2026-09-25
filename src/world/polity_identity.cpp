#include "polity_identity.hpp"

#include <algorithm>
#include <climits>
#include <utility>

// ---------------------------------------------------------------------------
// The lineage tree's wedges
// ---------------------------------------------------------------------------

lineage_wedges lineage_wedges_of(const std::vector<int32_t>& parent,
                                 const std::vector<int32_t>* folded_into)
{
    lineage_wedges out;
    const std::size_t n = parent.size();
    out.wedge.assign(n, -1);
    const auto folded_to = [&](std::size_t i) -> int32_t {
        if (folded_into == nullptr || i >= folded_into->size()) return -1;
        const int32_t f = (*folded_into)[i];
        return (f >= 0 && static_cast<std::size_t>(f) < i) ? f : -1;
    };
    for (std::size_t i = 0; i < n; ++i)
    {
        if (const int32_t f = folded_to(i); f >= 0)
        {
            out.wedge[i] = out.wedge[static_cast<std::size_t>(f)];
            continue;
        }
        const int32_t p = parent[i];
        if (p < 0 || static_cast<std::size_t>(p) >= i)
            out.wedge[i] = out.count++;               // a root: the next wedge
        else
            out.wedge[i] = out.wedge[static_cast<std::size_t>(p)];
    }
    return out;
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

std::vector<int32_t> nearest_region_raster(const std::vector<uint8_t>& water, int gw, int gh,
                                           const std::vector<int32_t>& region_col,
                                           const std::vector<int32_t>& region_row)
{
    std::vector<int32_t> tile_region;
    if (gw <= 0 || gh <= 0) return tile_region;
    const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    if (water.size() != n) return tile_region;
    tile_region.assign(n, -1);

    // Multi-source BFS: every anchor seeded at distance 0, in region order, so
    // a tie between two anchors resolves to the lower index — the same rule
    // finish_history_lapse always applied.
    std::vector<int> frontier;
    frontier.reserve(n);
    const std::size_t nreg = std::min(region_col.size(), region_row.size());
    for (std::size_t r = 0; r < nreg; ++r)
    {
        const int c = region_col[r], rr = region_row[r];
        if (c < 0 || c >= gw || rr < 0 || rr >= gh) continue;
        const std::size_t i = static_cast<std::size_t>(rr) * static_cast<std::size_t>(gw)
                            + static_cast<std::size_t>(c);
        if (water[i] != 0) continue;
        if (tile_region[i] >= 0) continue;
        tile_region[i] = static_cast<int32_t>(r);
        frontier.push_back(static_cast<int>(i));
    }
    for (std::size_t head = 0; head < frontier.size(); ++head)
    {
        const int i = frontier[head];
        const int32_t owner = tile_region[static_cast<std::size_t>(i)];
        const int c = i % gw, r = i / gw;
        const int steps[4][2] = { {1, 0}, {-1, 0}, {0, 1}, {0, -1} };
        for (const auto& s : steps)
        {
            int nc = c + s[0], nr = r + s[1];
            if (nr < 0 || nr >= gh) continue;
            if (nc < 0)   nc += gw;
            if (nc >= gw) nc -= gw;
            const std::size_t ni = static_cast<std::size_t>(nr) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(nc);
            if (tile_region[ni] >= 0) continue;
            if (water[ni] != 0) continue;
            tile_region[ni] = owner;
            frontier.push_back(static_cast<int>(ni));
        }
    }
    return tile_region;
}

std::vector<std::vector<int32_t>> region_adjacency(const std::vector<int32_t>& tile_region,
                                                   int gw, int gh, std::size_t region_count)
{
    std::vector<std::pair<int32_t, int32_t>> edges;
    if (gw > 0 && gh > 0
        && tile_region.size() == static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
    {
        for (int r = 0; r < gh; ++r)
            for (int c = 0; c < gw; ++c)
            {
                const int32_t a = tile_region[static_cast<std::size_t>(r * gw + c)];
                if (a < 0) continue;
                const int steps[2][2] = { {1, 0}, {0, 1} }; // east and south: each pair once
                for (const auto& s : steps)
                {
                    int nc = c + s[0], nr = r + s[1];
                    if (nr >= gh) continue;
                    if (nc >= gw) nc -= gw;
                    const int32_t b = tile_region[static_cast<std::size_t>(nr * gw + nc)];
                    if (b < 0 || b == a) continue;
                    edges.emplace_back(std::min(a, b), std::max(a, b));
                }
            }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

    std::vector<std::vector<int32_t>> nbrs(region_count);
    for (const auto& e : edges)
    {
        if (static_cast<std::size_t>(e.first) >= region_count
         || static_cast<std::size_t>(e.second) >= region_count) continue;
        nbrs[static_cast<std::size_t>(e.first)].push_back(e.second);
        nbrs[static_cast<std::size_t>(e.second)].push_back(e.first);
    }
    return nbrs;
}

// ---------------------------------------------------------------------------
// Reads off the record
// ---------------------------------------------------------------------------

namespace {

std::size_t polity_table_size(const era_timelapse& rec)
{
    std::size_t n = rec.polity_name.size();
    for (const owner_change& c : rec.changes)
        if (c.owner != owner_none && static_cast<std::size_t>(c.owner) + 1 > n)
            n = static_cast<std::size_t>(c.owner) + 1;
    for (const polity_sample& s : rec.samples)
        if (static_cast<std::size_t>(s.polity) + 1 > n) n = static_cast<std::size_t>(s.polity) + 1;
    return n;
}

} // namespace

std::vector<int32_t> polity_first_region(const era_timelapse& rec)
{
    std::vector<int32_t> first(polity_table_size(rec), -1);
    for (const owner_change& c : rec.changes)
    {
        if (c.owner == owner_none) continue;
        int32_t& f = first[static_cast<std::size_t>(c.owner)];
        if (f < 0) f = static_cast<int32_t>(c.region);
    }
    return first;
}

std::vector<int32_t> polity_seat_region(const era_timelapse& rec,
                                        const std::vector<int32_t>& first_region)
{
    std::vector<int32_t> seat = first_region;
    for (const lapse_event& e : rec.events)
    {
        if (e.kind != static_cast<uint8_t>(lapse_event_kind::founded)
         && e.kind != static_cast<uint8_t>(lapse_event_kind::inherited)) continue;
        if (e.polity == lapse_event_none || e.region == lapse_event_none) continue;
        if (static_cast<std::size_t>(e.polity) >= seat.size()) continue;
        // The first statement wins: a founding is stated once, and a resume's
        // restatement is the first event of its record.
        int32_t& s = seat[static_cast<std::size_t>(e.polity)];
        if (s < 0 || s == first_region[static_cast<std::size_t>(e.polity)])
            s = static_cast<int32_t>(e.region);
    }
    return seat;
}

std::vector<int32_t> culture_plurality_at(const era_timelapse& rec, int year)
{
    std::vector<int32_t> plural(static_cast<std::size_t>(std::max<int32_t>(0, rec.region_stride)), -1);
    for (const culture_change& c : rec.culture_changes)
    {
        if (c.year > year) break; // ascending by year
        if (static_cast<std::size_t>(c.region) >= plural.size())
            plural.resize(static_cast<std::size_t>(c.region) + 1, -1);
        plural[static_cast<std::size_t>(c.region)] = c.id[0];
    }
    return plural;
}

std::vector<int32_t> polity_founding_culture(const era_timelapse& rec,
                                             const std::vector<int32_t>& seat_region,
                                             const std::vector<int32_t>& first_region)
{
    const std::size_t npol = std::max(seat_region.size(), first_region.size());
    std::vector<int32_t> culture(npol, -1);
    if (npol == 0) return culture;

    // Founding year per polity: the year of its first change.
    std::vector<int32_t> founded(npol, INT32_MAX);
    for (const owner_change& c : rec.changes)
    {
        if (c.owner == owner_none || static_cast<std::size_t>(c.owner) >= npol) continue;
        int32_t& y = founded[static_cast<std::size_t>(c.owner)];
        if (c.year < y) y = c.year;
    }

    // One forward walk over the culture list against the polities sorted by
    // founding year: the plurality of the seat at the founding is the last
    // change for that region at or before that year.
    std::vector<std::size_t> order;
    order.reserve(npol);
    for (std::size_t p = 0; p < npol; ++p)
        if (founded[p] != INT32_MAX) order.push_back(p);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (founded[a] != founded[b]) return founded[a] < founded[b];
        return a < b;
    });

    std::vector<int32_t> plural(static_cast<std::size_t>(std::max<int32_t>(0, rec.region_stride)), -1);
    std::size_t ci = 0;
    for (const std::size_t p : order)
    {
        const int32_t y = founded[p];
        while (ci < rec.culture_changes.size() && rec.culture_changes[ci].year <= y)
        {
            const culture_change& c = rec.culture_changes[ci++];
            if (static_cast<std::size_t>(c.region) >= plural.size())
                plural.resize(static_cast<std::size_t>(c.region) + 1, -1);
            plural[static_cast<std::size_t>(c.region)] = c.id[0];
        }
        const int32_t seat = (p < seat_region.size() && seat_region[p] >= 0)
                                 ? seat_region[p]
                                 : (p < first_region.size() ? first_region[p] : -1);
        if (seat >= 0 && static_cast<std::size_t>(seat) < plural.size())
            culture[p] = plural[static_cast<std::size_t>(seat)];
    }
    return culture;
}

// ---------------------------------------------------------------------------
// The assignment
// ---------------------------------------------------------------------------

namespace {

/// Polity adjacency across the WHOLE record: two polities are adjacent if they
/// ever held neighbouring regions at the same time. A neighbouring pair can
/// only come into being when one side changes hands, so folding the change
/// list and looking around each changed region sees every pair that ever
/// existed. Linear in changes times region degree.
std::vector<std::vector<int32_t>> polity_adjacency(const era_timelapse& rec,
                                                   const std::vector<std::vector<int32_t>>& region_nbrs,
                                                   std::size_t npol)
{
    const std::size_t nreg = region_nbrs.size();
    std::vector<uint16_t> owner(nreg, owner_none);
    std::vector<std::pair<int32_t, int32_t>> edges;
    for (const owner_change& ch : rec.changes)
    {
        if (ch.region >= nreg) continue;
        owner[ch.region] = ch.owner;
        if (ch.owner == owner_none || static_cast<std::size_t>(ch.owner) >= npol) continue;
        for (const int32_t nb : region_nbrs[ch.region])
        {
            const uint16_t o = owner[static_cast<std::size_t>(nb)];
            if (o == owner_none || o == ch.owner || static_cast<std::size_t>(o) >= npol) continue;
            edges.emplace_back(std::min<int32_t>(ch.owner, o), std::max<int32_t>(ch.owner, o));
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    std::vector<std::vector<int32_t>> nbrs(npol);
    for (const auto& e : edges)
    {
        nbrs[static_cast<std::size_t>(e.first)].push_back(e.second);
        nbrs[static_cast<std::size_t>(e.second)].push_back(e.first);
    }
    return nbrs;
}

/// Per polity: its LAST-HELD GROUND — the regions it held at the start of
/// the year it lost its last one, plus any it gained inside that year before
/// the loss — and an empty list for a realm alive at the record's end. One
/// pass: each polity's current set is snapshotted lazily the first time a
/// change of a new year touches it, gains inside the year join the snapshot,
/// and the snapshot is what the death records. O(changes x regions held).
std::vector<std::vector<int32_t>> last_held_ground(const era_timelapse& rec, std::size_t npol)
{
    const std::size_t nreg = static_cast<std::size_t>(std::max<int32_t>(0, rec.region_stride));
    std::vector<uint16_t> owner(nreg, owner_none);
    std::vector<std::vector<int32_t>> cur(npol), snap(npol), last(npol);
    std::vector<int32_t> stamp(npol, INT32_MIN);
    const auto touch = [&](std::size_t p, int32_t year) {
        if (stamp[p] != year) { snap[p] = cur[p]; stamp[p] = year; }
    };
    for (const owner_change& ch : rec.changes)
    {
        if (ch.region >= nreg) continue;
        const uint16_t was = owner[ch.region];
        if (was == ch.owner) continue;
        owner[ch.region] = ch.owner;
        if (was != owner_none && static_cast<std::size_t>(was) < npol)
        {
            touch(was, ch.year);
            std::vector<int32_t>& c = cur[was];
            c.erase(std::remove(c.begin(), c.end(), static_cast<int32_t>(ch.region)), c.end());
            if (c.empty()) last[was] = snap[was];
        }
        if (ch.owner != owner_none && static_cast<std::size_t>(ch.owner) < npol)
        {
            touch(ch.owner, ch.year);
            cur[ch.owner].push_back(static_cast<int32_t>(ch.region));
            snap[ch.owner].push_back(static_cast<int32_t>(ch.region));
            last[ch.owner].clear(); // holding ground again: not dead
        }
    }
    for (std::size_t p = 0; p < npol; ++p)
        if (!cur[p].empty()) last[p].clear();
    return last;
}

/// People share at the record's opening step, per polity (per-mille), 0 for a
/// polity absent from that step. What the clash rule compares.
std::vector<int32_t> opening_share(const era_timelapse& rec, std::size_t npol)
{
    std::vector<int32_t> share(npol, 0);
    if (rec.steps.empty()) return share;
    const timelapse_step& st = rec.steps.front();
    int64_t total = 0;
    for (int32_t i = st.first_sample; i < st.first_sample + st.sample_count
                                     && static_cast<std::size_t>(i) < rec.samples.size(); ++i)
        total += std::max<int64_t>(0, rec.samples[static_cast<std::size_t>(i)].population);
    if (total <= 0) return share;
    for (int32_t i = st.first_sample; i < st.first_sample + st.sample_count
                                     && static_cast<std::size_t>(i) < rec.samples.size(); ++i)
    {
        const polity_sample& s = rec.samples[static_cast<std::size_t>(i)];
        if (static_cast<std::size_t>(s.polity) < npol)
            share[s.polity] = static_cast<int32_t>(
                (std::max<int64_t>(0, s.population) * 1000) / total);
    }
    return share;
}

} // namespace

polity_identity assign_polity_identity(const polity_identity_input& in)
{
    polity_identity id;
    if (in.rec == nullptr || in.region_nbrs == nullptr) return id;
    const era_timelapse& rec = *in.rec;

    const std::size_t npol = polity_table_size(rec);
    id.first_region = polity_first_region(rec);
    id.first_region.resize(npol, -1);
    id.seat_region  = polity_seat_region(rec, id.first_region);
    id.seat_region.resize(npol, -1);
    id.slot.assign(npol, -1);
    id.rung_carry.assign(npol, 0);
    id.ratchet_year.assign(npol * 2, INT32_MAX);
    if (npol == 0) return id;

    const bool wedges = in.family != nullptr && in.family_count > 0;
    const int  k_slots = wedges ? in.family_count * polity_slot_offsets
                                : polity_fallback_slot_count;

    const std::vector<std::vector<int32_t>> nbrs = polity_adjacency(rec, *in.region_nbrs, npol);
    const std::vector<std::vector<int32_t>> dead_ground = last_held_ground(rec, npol);

    // Which polities hold ground in THIS record at all.
    std::vector<char> present(npol, 0);
    for (const owner_change& c : rec.changes)
        if (c.owner != owner_none && static_cast<std::size_t>(c.owner) < npol) present[c.owner] = 1;

    // --- 1. Pins: kept as they are, then the clash rule. -------------------
    std::vector<char> pinned(npol, 0);
    if (in.pins != nullptr)
    {
        for (std::size_t p = 0; p < npol && p < in.pins->slot.size(); ++p)
        {
            const int32_t s = in.pins->slot[p];
            if (s >= 0 && s < k_slots) { id.slot[p] = s; pinned[p] = 1; }
        }
        for (std::size_t p = 0; p < npol && p < in.pins->rung.size(); ++p)
            id.rung_carry[p] = std::max<int32_t>(0, in.pins->rung[p]);

        // A CLASH is two pinned realms this record makes neighbours while
        // they share a slot — they were never neighbours in the record that
        // slotted them, or the greedy walk would have kept them apart. Rule:
        // the smaller people share at the round's opening step is re-slotted
        // (unpinned, coloured with the rest below). Each pair counted once.
        const std::vector<int32_t> share = opening_share(rec, npol);
        for (std::size_t a = 0; a < npol; ++a)
        {
            if (!pinned[a] || !present[a]) continue;
            for (const int32_t b : nbrs[a])
            {
                if (static_cast<std::size_t>(b) <= a) continue; // each pair once
                if (!pinned[static_cast<std::size_t>(b)] || !present[static_cast<std::size_t>(b)]) continue;
                if (id.slot[a] != id.slot[static_cast<std::size_t>(b)]) continue;
                ++id.pinned_clashes;
                const std::size_t loser =
                    (share[static_cast<std::size_t>(b)] < share[a]
                     || (share[static_cast<std::size_t>(b)] == share[a] && static_cast<std::size_t>(b) > a))
                        ? static_cast<std::size_t>(b) : a;
                if (pinned[loser]) { pinned[loser] = 0; id.slot[loser] = -1; ++id.reslotted; }
            }
        }
    }

    // --- 2. Retired slots: a dead realm's slot is reserved. ---------------
    // Dead from the predecessor (pinned but never present here) and dead
    // inside this record both count; the ground each last held decides who
    // may take the slot.
    std::vector<int32_t> region_dead_owner;
    if (in.pins != nullptr) region_dead_owner = in.pins->region_dead_owner;
    region_dead_owner.resize(static_cast<std::size_t>(std::max<int32_t>(0, rec.region_stride)), -1);
    // Note: predecessor-dead pins never present here keep `region_dead_owner`
    // from the pins; this record's own dead overwrite it in death order.
    std::vector<char> reserved(static_cast<std::size_t>(k_slots), 0);
    for (std::size_t p = 0; p < npol; ++p)
    {
        if (pinned[p] && !present[p] && id.slot[p] >= 0)
            reserved[static_cast<std::size_t>(id.slot[p])] = 1;
        if (!dead_ground[p].empty())
            for (const int32_t r : dead_ground[p])
                if (r >= 0 && static_cast<std::size_t>(r) < region_dead_owner.size())
                    region_dead_owner[static_cast<std::size_t>(r)] = static_cast<int32_t>(p);
    }

    // --- 3. Greedy, in founding (id) order, for every unpinned present realm.
    // A dead realm precedes any newcomer on its ground, so its slot is known
    // when the newcomer asks. Candidates: the realm's own wedge first, then
    // the other wedges in order, wrapping; the first pass skips reserved
    // slots, the second allows them, and only then the least-used neighbour
    // slot is taken — a bound met honestly rather than a palette widened.
    std::vector<int> used(static_cast<std::size_t>(k_slots), 0);
    for (std::size_t p = 0; p < npol; ++p)
    {
        if (pinned[p] || !present[p]) continue;
        std::fill(used.begin(), used.end(), 0);
        for (const int32_t nb : nbrs[p])
        {
            const int32_t s = id.slot[static_cast<std::size_t>(nb)];
            if (s >= 0 && s < k_slots) ++used[static_cast<std::size_t>(s)];
        }

        int chosen = -1;

        // The dead-ground rule first: seated inside a dead realm's last-held
        // ground, the newcomer takes that realm's slot where no neighbour
        // holds it — the retirement the design names.
        const int32_t seat = id.seat_region[p] >= 0 ? id.seat_region[p] : id.first_region[p];
        if (seat >= 0 && static_cast<std::size_t>(seat) < region_dead_owner.size())
        {
            const int32_t dead = region_dead_owner[static_cast<std::size_t>(seat)];
            if (dead >= 0 && static_cast<std::size_t>(dead) < npol && dead != static_cast<int32_t>(p))
            {
                const int32_t s = id.slot[static_cast<std::size_t>(dead)];
                if (s >= 0 && s < k_slots && used[static_cast<std::size_t>(s)] == 0)
                {
                    chosen = s;
                    ++id.inherited_dead_slot;
                    // The slot passes: the ground is the newcomer's now.
                    reserved[static_cast<std::size_t>(s)] = 0;
                }
            }
        }

        const int fam = (wedges && p < in.family->size() && (*in.family)[p] >= 0
                         && (*in.family)[p] < in.family_count)
                            ? (*in.family)[p] : 0;
        const int start = wedges ? fam * polity_slot_offsets : 0;
        for (int pass = 0; pass < 2 && chosen < 0; ++pass)
            for (int k = 0; k < k_slots && chosen < 0; ++k)
            {
                const int s = (start + k) % k_slots;
                if (used[static_cast<std::size_t>(s)] != 0) continue;
                if (pass == 0 && reserved[static_cast<std::size_t>(s)]) continue;
                chosen = s;
            }
        if (chosen < 0)
        {
            chosen = start % k_slots;
            for (int k = 0; k < k_slots; ++k)
            {
                const int s = (start + k) % k_slots;
                if (used[static_cast<std::size_t>(s)] < used[static_cast<std::size_t>(chosen)]) chosen = s;
            }
            ++id.spills;
        }
        id.slot[p] = chosen;
    }

    // --- 4. The ratchet's named moments (BL-1087 R4). ----------------------
    // `civilisation_formed` for the realm, and the first step at which it
    // crosses the sweep's ROSE rule against its own first sample in this
    // record. Each at most once; ascending per polity.
    {
        std::vector<int32_t> civ(npol, INT32_MAX);
        for (const lapse_event& e : rec.events)
        {
            if (e.kind != static_cast<uint8_t>(lapse_event_kind::civilisation_formed)) continue;
            if (e.polity == lapse_event_none || static_cast<std::size_t>(e.polity) >= npol) continue;
            int32_t& y = civ[e.polity];
            if (e.year < y) y = e.year;
        }
        std::vector<int32_t> start(npol, -1), rose(npol, INT32_MAX);
        for (const timelapse_step& st : rec.steps)
        {
            for (int32_t i = st.first_sample; i < st.first_sample + st.sample_count
                                             && static_cast<std::size_t>(i) < rec.samples.size(); ++i)
            {
                const polity_sample& s = rec.samples[static_cast<std::size_t>(i)];
                if (static_cast<std::size_t>(s.polity) >= npol) continue;
                if (start[s.polity] < 0) { start[s.polity] = s.regions; continue; }
                if (rose[s.polity] != INT32_MAX) continue;
                if (s.regions >= 2 * start[s.polity] && s.regions >= start[s.polity] + 3)
                    rose[s.polity] = st.year;
            }
        }
        for (std::size_t p = 0; p < npol; ++p)
        {
            int32_t a = civ[p], b = rose[p];
            if (b < a) std::swap(a, b);
            id.ratchet_year[2 * p]     = a;
            id.ratchet_year[2 * p + 1] = b;
        }
    }
    return id;
}

int polity_rung_at(const polity_identity& id, std::size_t polity, int year)
{
    if (polity >= id.slot.size()) return 0;
    int rung = polity < id.rung_carry.size() ? id.rung_carry[polity] : 0;
    if (2 * polity + 1 < id.ratchet_year.size())
    {
        if (id.ratchet_year[2 * polity]     <= year) ++rung;
        if (id.ratchet_year[2 * polity + 1] <= year) ++rung;
    }
    return rung;
}

polity_pins pins_from(const polity_identity& id, const era_timelapse& rec,
                      const polity_pins* predecessor)
{
    polity_pins pins;
    pins.slot = id.slot;
    pins.rung.resize(id.slot.size(), 0);
    const int end_year = rec.start_year + rec.years;
    for (std::size_t p = 0; p < id.slot.size(); ++p)
        pins.rung[p] = polity_rung_at(id, p, end_year);
    // Keep the predecessor's slots for ids this record never saw (they stay
    // retired), so a realm dead two rounds ago does not free its colour.
    if (predecessor != nullptr)
    {
        if (pins.slot.size() < predecessor->slot.size()) pins.slot.resize(predecessor->slot.size(), -1);
        if (pins.rung.size() < predecessor->rung.size()) pins.rung.resize(predecessor->rung.size(), 0);
        for (std::size_t p = 0; p < predecessor->slot.size(); ++p)
            if (pins.slot[p] < 0) pins.slot[p] = predecessor->slot[p];
        for (std::size_t p = 0; p < predecessor->rung.size(); ++p)
            if (pins.rung[p] == 0) pins.rung[p] = predecessor->rung[p];
        pins.region_dead_owner = predecessor->region_dead_owner;
    }
    pins.region_dead_owner.resize(static_cast<std::size_t>(std::max<int32_t>(0, rec.region_stride)), -1);
    const std::vector<std::vector<int32_t>> dead = last_held_ground(rec, id.slot.size());
    for (std::size_t p = 0; p < dead.size(); ++p)
        for (const int32_t r : dead[p])
            if (r >= 0 && static_cast<std::size_t>(r) < pins.region_dead_owner.size())
                pins.region_dead_owner[static_cast<std::size_t>(r)] = static_cast<int32_t>(p);
    return pins;
}

// ---------------------------------------------------------------------------
// The capital fold (BL-1088)
// ---------------------------------------------------------------------------

std::vector<polity_capital_move> polity_capital_moves(const era_timelapse& rec)
{
    std::vector<polity_capital_move> moves;
    for (const lapse_event& e : rec.events)
    {
        const bool seat = e.kind == static_cast<uint8_t>(lapse_event_kind::founded)
                       || e.kind == static_cast<uint8_t>(lapse_event_kind::inherited)
                       || e.kind == static_cast<uint8_t>(lapse_event_kind::capital_moved);
        if (!seat || e.polity == lapse_event_none || e.region == lapse_event_none) continue;
        moves.push_back(polity_capital_move{e.year, e.polity, e.region});
    }
    return moves;
}

int32_t polity_capital_at(const std::vector<polity_capital_move>& moves, uint16_t polity,
                          int year, int32_t fallback)
{
    int32_t at = fallback;
    for (const polity_capital_move& m : moves)
    {
        if (m.year > year) break;
        if (m.polity == polity) at = static_cast<int32_t>(m.region);
    }
    return at;
}
