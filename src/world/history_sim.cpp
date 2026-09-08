#include "history_sim.hpp"

#include "unit_roster.hpp"
#include "terrain_combat.hpp" // BL-384 trace: the defence term the scorer never sees

#include <algorithm>
#include <chrono>

// ---------------------------------------------------------------------------
// The Era -1 history sim (BL-277 + BL-271's first slice). See history_sim.hpp
// for the model; this file is the loop and the scorer.
//
// Everything below is integer arithmetic. There is no RNG: `salt` mixes the
// seed into a polity's weights so polities differ, and into tie-breaks so a
// tie resolves the same way on every replay. That is a hash, not a generator —
// nothing here consumes a stream, so inserting a decision does not shift the
// numbers a later decision sees.
// ---------------------------------------------------------------------------

namespace
{

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int64_t clampi64(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// Deterministic 32-bit mix (a finaliser, not a generator). Used for weight
/// perturbation and tie-breaks only; never as a probability.
uint32_t salt(uint32_t a, uint32_t b)
{
    uint32_t x = a * 0x9E3779B9u ^ (b + 0x85EBCA6Bu + (a << 6) + (a >> 2));
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/// Per-polity weight jitter, +/- `spread` per-mille, stable across replays.
///
/// `axis` is what makes polities differ in WHAT THEY VALUE rather than merely
/// in how strongly they value everything at once (BL-312). The first cut mixed
/// only the polity salt against a fixed constant, so a polity that liked
/// farmland 12% above baseline liked ore and ports 12% above baseline too — the
/// relative ordering of the three endowment terms was byte-identical for every
/// polity in every world, which is the opposite of the intended effect.
int jitter(int base, uint32_t s, uint32_t axis, int spread)
{
    if (spread <= 0) return base;
    const int d = static_cast<int>(salt(s, axis) % static_cast<uint32_t>(2 * spread + 1)) - spread;
    return base + (base * d) / 1000;
}

/// THE COMMON CURRENCY (BL-309). Every verb scores in ONE unit: expected
/// annual gain in ENDOWMENT VALUE HELD, where a region is worth the mean of
/// its three endowment windows (0-1000).
///
/// This replaces comparing four incommensurable numbers. The first cut compared
/// raw scores directly, and Invest — whose score was population/4000 clamped —
/// pinned at its ceiling and won forever. The second cut normalised each verb
/// by its own range, which structurally favours the NARROWEST range and simply
/// handed the win to Settle instead. Neither was fixable by tuning, because the
/// error was upstream of the constants: quantities that mean different things
/// cannot be ranked. So each verb now answers the same question — what is this
/// worth to me this year, in regions-worth-of-endowment — and the argmax is
/// an honest comparison rather than a coincidence of scales.
int region_value_q(const region& p)
{
    return (p.farm_q + p.ore_q + p.port_q) / 3;
}

/// What a candidate work is worth this year, IN THE SHARED CURRENCY (BL-321).
///
/// The split between the two terms is the honest part. A Granary feeds THIS
/// region and nowhere else, so it is valued against this region's own
/// endowment. A Way Station shortens marches across the whole polity, so reach
/// is valued against the polity's MEAN holding — mean, not total, because a
/// road through one region does not carry the empire's entire traffic, and
/// scoring it against the total made reach worth ~40x every local effect and
/// reduced the roster to its five road rows.
///
/// The row's authored `weight` then shapes the choice between rows that score
/// alike, as a 0.65x-1.3x multiplier over the table's 130-260 weight range. It
/// is a pull, not a cost — works.lua says so at the point of authoring.
int work_score_q(const work_row&           r,
                 const region&           p,
                 int                       mean_holding_value,
                 const history_sim_params& params)
{
    const work_effect& e = r.effect;

    const int local_q = (e.capacity_mod   * params.w_work_capacity
                       + e.manpower_mod   * params.w_work_manpower
                       + e.defence_mod    * params.w_work_defence
                       + e.industrial_mod * params.w_work_industrial) / 1000;
    const int empire_q = (e.reach_mod * params.w_work_reach) / 1000;

    const int64_t gain = (static_cast<int64_t>(region_value_q(p)) * local_q
                        + static_cast<int64_t>(mean_holding_value)  * empire_q) / 1000;

    const int years = params.work_amortise_years > 0 ? params.work_amortise_years : 1;
    int64_t s = gain / years;
    s = (s * clampi(r.weight, 0, 1000)) / 200;
    return static_cast<int>(clampi64(s, 0, 100000));
}

terrain_substrate sub_at(const sim_terrain_view& t, int idx)
{
    if (!t.substrate || idx < 0 || idx >= static_cast<int>(t.substrate->size()))
        return terrain_substrate::sedimentary; // the old grassland default, on its ground
    return (*t.substrate)[static_cast<std::size_t>(idx)];
}

terrain_cover cov_at(const sim_terrain_view& t, int idx)
{
    if (!t.cover || idx < 0 || idx >= static_cast<int>(t.cover->size()))
        return terrain_cover::grass; // the other half of the old grassland default
    return (*t.cover)[static_cast<std::size_t>(idx)];
}

std::uint8_t den_at(const sim_terrain_view& t, int idx)
{
    if (!t.density || idx < 0 || idx >= static_cast<int>(t.density->size()))
        return 150u; // the density decompose_biome grades a grassland to
    return (*t.density)[static_cast<std::size_t>(idx)];
}

terrain_landform lf_at(const sim_terrain_view& t, int idx)
{
    if (!t.landform || idx < 0 || idx >= static_cast<int>(t.landform->size()))
        return terrain_landform::plains;
    return (*t.landform)[static_cast<std::size_t>(idx)];
}

/// A polity's doctrine, from its culture's aggression (BL-277 Q5). An
/// aggressive creed leans frontal and brittle; a preserving one leans the
/// other way. This is `doctrine_row` as pure data exactly as combat.hpp
/// intends — no new resolution path.
doctrine_row doctrine_for(const polity& p)
{
    doctrine_row d;
    const int a = clampi(p.aggression_q, 0, 1000);
    d.frontal_bonus   = (a - 500) / 3;          // -166 .. +166 per-mille.
    d.flank_fragility = clampi(a / 4, 0, 250);  // Aggression buys exposure.
    d.mountain_penalty = clampi(a / 5, 0, 200);
    d.stance = siege_stance::field;
    return d;
}

/// Which span a year falls in (BL-760 (1)): 0 = the ancient span, 1 = industrial.
///
/// A SINGLE-SPAN RUN REPORTS EVERYTHING AS SPAN 0, which is the reading that
/// matches the design rather than the sentinel. `boundary_year` defaults to
/// INT64_MIN so that "no year is before it" leaves a one-span arc inert — but
/// read naively that puts an ancient arc's whole history in span 1, i.e. in the
/// industrial span it does not have. The explicit sentinel test is what keeps
/// the ancient-arc rows readable.
int span_index(const history_sim_params& params, int64_t y)
{
    if (params.boundary_year == INT64_MIN)
        return 0;
    return y >= params.boundary_year ? 1 : 0;
}

/// Count a fielded stack into the per-band and per-span rows.
void note_units_fielded(history_sim_state&        out,
                        const history_sim_params& params,
                        int64_t                   y,
                        roster_band               band,
                        const std::vector<army_stack_entry>& stack)
{
    int64_t n = 0;
    for (const army_stack_entry& e : stack)
        n += e.count;
    const int b = static_cast<int>(band);
    if (b < 0 || b >= roster_band_count)
        return;
    out.units_by_span_band[static_cast<std::size_t>(span_index(params, y))]
                          [static_cast<std::size_t>(b)] += n;
}

/// Turn raised manpower into a typed stack via the era-keyed roster (BL-274),
/// then scale the whole stack by the owner's COHESION (BL-308).
///
/// The roster answers "what can this ground field at this band"; cohesion
/// answers "how well does this polity fight right now". They are separate on
/// purpose: the first is a property of the map, the second of the polity's
/// recent history, and only the second can spiral.
///
/// `readiness_q` is the caller-side lever the winter-campaign candidate uses
/// against a defender (history_sim.hpp § season).
///
/// `ceiling` is the two-span band cap (BL-747). It carried a
/// `= roster_band::industrial` default — no restriction — which BOTH call sites
/// already override with `sim_band_ceiling(params, y)`. The default was therefore
/// dead, and dead in the dangerous direction: a new in-TU caller that omitted the
/// argument would silently un-apply the span cap, with no diagnostic and no
/// harness able to see it (BL-760 (3)). Required rather than defaulted, so
/// forgetting it is a compile error.
///
/// @p allow_naval carries NR-794 (Ben, 2026-09-07): ships are composed only into
/// a stack whose campaign actually crosses water. Required for the same reason
/// @p ceiling is — an omitted argument would silently re-admit fleets to land
/// battles, and nothing downstream could tell.
std::vector<army_stack_entry> build_stack(int64_t manpower,
                                          const region& home,
                                          const polity&   owner,
                                          int             readiness_q,
                                          roster_band     ceiling,
                                          bool            allow_naval,
                                          roster_band*    band_out)
{
    const int band_index = clampi(owner.capacity[static_cast<int>(sim_domain::military)], 1, 6);
    const roster_band band = min_band(roster_band_for_capacity(band_index), ceiling);
    // BL-760 (1): the band is computed HERE and nowhere else, so the counter
    // reads the value the stack was actually built from. Re-deriving it at the
    // call site would be a second copy of the min_band rule, free to drift from
    // this one — the divergence class this file's own header warns about.
    if (band_out != nullptr)
        *band_out = band;

    // Cohesion folds into readiness rather than into the counts: a shaken
    // polity fields the same men fighting worse, not fewer men fighting well.
    const int cohesion = clampi(owner.cohesion_q, 0, 1000);
    const int effective_readiness = (readiness_q * cohesion) / 1000;

    return roster_stack(manpower, home, band, effective_readiness, allow_naval);
}

/// True iff the straight line between two regions crosses SEA (open ocean or
/// coastal sea; lakes are not sea). BL-778's traversal legality is asked of an
/// EDGE, and this is the predicate that answers it.
///
/// DELIBERATELY THE SAME SAMPLER `tools/verify/sim_water_census.cpp` USES, step
/// for step, so the "43% of adjacency edges cross sea" figure the item is
/// scoped against and the legality the sim now enforces are the same
/// measurement. Sampled along the line rather than pathfound, because the sim
/// has no path either — its adjacency is a water-blind Chebyshev radius, which
/// is exactly the free reach this prices.
///
/// Integer-only, no wrap (the census does not wrap either), and a function of
/// the terrain raster alone: two runs at one seed ask it the same questions in
/// the same order and get the same answers.
bool line_crosses_sea(const region& a, const region& b,
                      const sim_terrain_view& terrain, int gw, int gh, int radius)
{
    const int steps = radius * 2;
    for (int t = 1; t < steps; ++t)
    {
        const int c = a.col + (b.col - a.col) * t / steps;
        const int r = a.row + (b.row - a.row) * t / steps;
        if (c < 0 || r < 0 || c >= gw || r >= gh) continue;
        if (is_sea(sub_at(terrain, c + r * gw))) return true;
    }
    return false;
}

/// Does @p home's ground let this polity field anything that may hold open
/// ocean? Asked of the ROW's traversal mask (BL-778), never of `unit_class`,
/// so an amphibious row added to the table later answers this without touching
/// the sim. `port_q` is what gates the three naval rows, so this is "does the
/// staging holding have a harbour good enough to carry an army".
bool can_field_naval(const region& home, roster_band band)
{
    for (const roster_row* r : available_rows(home, band))
        if (row_can_traverse(*r, traversal_domain::open_ocean, false))
            return true;
    return false;
}

/// Did either stack commit a naval entry? The BL-779 calibration reading.
bool stack_has_naval(const std::vector<army_stack_entry>& s)
{
    for (const army_stack_entry& e : s)
        if (e.cls == unit_class::naval && e.count > 0) return true;
    return false;
}

/// Total committed headcount in a stack — the denominator losses apply to.
int64_t stack_size(const std::vector<army_stack_entry>& s)
{
    int64_t n = 0;
    for (const army_stack_entry& e : s) n += e.count;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------

std::vector<uint16_t> owner_slice_at(const era_timelapse& t, int64_t year)
{
    std::vector<uint16_t> slice(static_cast<std::size_t>(t.region_stride), owner_none);
    for (const owner_change& c : t.changes)
    {
        if (static_cast<int64_t>(c.year) > year) break; // Appended in year order.
        if (c.region < slice.size()) slice[c.region] = c.owner;
    }
    return slice;
}

std::vector<uint16_t> owner_slice_at(const history_sim_state& s, int64_t year)
{
    return owner_slice_at(as_timelapse(s), year);
}

int region_distance(const region& a, const region& b, int gw)
{
    int dc = a.col - b.col;
    if (dc < 0) dc = -dc;
    if (gw > 0 && dc > gw / 2) dc = gw - dc; // Columns wrap: the map is a cylinder.
    int dr = a.row - b.row;
    if (dr < 0) dr = -dr;
    return dc > dr ? dc : dr; // Chebyshev — movement is eight-connected.
}

int step_for_year(const history_sim_params& p, int64_t y)
{
    const int n = clampi(p.tick_band_count, 0, sim_tick_band_max);
    if (n <= 0)
        return 1; // No table: fall back to the flat year tick.

    for (int i = 0; i < n; ++i)
        if (y < p.tick_bands[i].until_year)
            return p.tick_bands[i].step_years > 0 ? p.tick_bands[i].step_years : 1;

    // Past the last boundary — the finest band governs the tail. This is the
    // ordinary case for the last band, whose `until_year` IS the stop year.
    const int last = p.tick_bands[n - 1].step_years;
    return last > 0 ? last : 1;
}

// ---------------------------------------------------------------------------

// --- BL-825: the REPORT-ONLY cost split ------------------------------------
//
// Four wall-clock accumulators, written and never read by this file. They exist
// so a measurement harness can say where a 4000-year span's time goes without
// having to guess it from differential runs. The sim does not branch on them,
// they are not part of `history_sim_state`, and they must never reach a digest
// or a save — see the type comment in history_sim.hpp.
history_sim_profile& history_sim_last_profile()
{
    static history_sim_profile prof;
    return prof;
}

namespace
{
using prof_clock = std::chrono::steady_clock;

/// Adds its own lifetime to one accumulator. Construction and destruction only;
/// no control flow of the sim depends on it.
struct scoped_ns
{
    int64_t*               sink;
    prof_clock::time_point t0;
    explicit scoped_ns(int64_t& s_) : sink(&s_), t0(prof_clock::now()) {}
    ~scoped_ns()
    {
        *sink += std::chrono::duration_cast<std::chrono::nanoseconds>(
                     prof_clock::now() - t0).count();
    }
};
} // namespace

history_sim_state run_history_sim(settlement_state&         ss,
                                  const creed_state*        cs,
                                  const sim_terrain_view&   terrain,
                                  int                       gw,
                                  int                       gh,
                                  const history_sim_params& params,
                                  uint32_t                  seed,
                                  std::atomic<int>*         year_progress,
                                  const works_registry*     works)
{
    history_sim_profile& prof = history_sim_last_profile();
    prof = history_sim_profile{}; // this run's split, never the last one's.

    history_sim_state out;
    if (ss.regions.empty() || params.stop_year <= params.start_year)
        return out;

    // --- Seed polities from cultures --------------------------------------
    //
    // At the antiquity start `region::nation` is -1: the political pass has
    // not run, and a pre-national world's actors ARE its peoples (BL-221). The
    // sim writes `nation` as it goes, so the political map is this loop's
    // output rather than its input.
    {
        // BL-826 — PLURALITY. A polity is seeded per distinct people on the
        // map, and a people is present where it is the largest share. At the
        // seed the shares are pure (settlement writes `pure`), so this is
        // exactly the old set; it only differs on a state handed in mid-history.
        std::vector<int> cultures;
        for (const region& p : ss.regions)
        {
            const int pc = p.culture.plurality();
            if (pc >= 0 && std::find(cultures.begin(), cultures.end(), pc) == cultures.end())
                cultures.push_back(pc);
        }
        std::sort(cultures.begin(), cultures.end()); // Order must not depend on placement.

        for (int c : cultures)
        {
            polity q;
            q.id      = static_cast<int>(out.polities.size());
            q.culture = c;
            if (cs && c < static_cast<int>(cs->cultures.size()))
                q.aggression_q = cs->cultures[static_cast<std::size_t>(c)].aggression_q;
            else
                q.aggression_q = 500; // Neutral when creeds were not supplied.

            // Capital: the best-settled region of this culture. Ties break on
            // the lower index, which is placement order (best ground first).
            int best = -1, best_q = -1;
            for (std::size_t i = 0; i < ss.regions.size(); ++i)
            {
                const region& p = ss.regions[i];
                // BL-826 — PLURALITY. A seat sits where this people is the
                // largest, which is the same question the seed above asked.
                if (p.culture.plurality() != c) continue;
                if (p.settle_score_q > best_q) { best_q = p.settle_score_q; best = static_cast<int>(i); }
            }
            q.capital = best;
            out.polities.push_back(q);
        }
    }
    if (out.polities.empty()) return out;

    // --- Great-power seed (BL-299) ----------------------------------------
    //
    // Two majors with OPPOSED strategic creeds — one preserving, one on a
    // civilising mission — in a world whose periphery stays multipolar. The
    // periphery is not terrain: minors keep their own cultures, doctrines and
    // recorded history, exactly as the item's richness clause requires. All
    // this seed does is set two aggressions apart and mark the pair.
    if (params.seed_great_powers && out.polities.size() >= 2)
    {
        // Largest two by starting holdings, ties to the lower id so the choice
        // does not depend on iteration order.
        std::vector<std::pair<int, int>> by_size; // (count, id)
        for (const polity& q : out.polities)
        {
            // BL-826 — SHARE-WEIGHTED, and this is the one place a plurality
            // count would have been actively wrong. "Largest two by starting
            // holdings" is a MEASURE OF SIZE, and a half-held region is half a
            // holding; counting it as a whole one (or as none) makes the
            // great-power seed jump on a single per-mille crossing 500. The sum
            // of shares is the continuous reading and it is exactly the old
            // count on the pure shares the seed actually sees.
            int64_t share_sum = 0;
            for (const region& p : ss.regions) share_sum += p.culture.share_of(q.culture);
            const int n = static_cast<int>(share_sum / 1000);
            by_size.push_back({n, q.id});
        }
        std::sort(by_size.begin(), by_size.end(),
                  [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                      if (a.first != b.first) return a.first > b.first;
                      return a.second < b.second;
                  });

        polity& expansionist = out.polities[static_cast<std::size_t>(by_size[0].second)];
        polity& preserving   = out.polities[static_cast<std::size_t>(by_size[1].second)];
        expansionist.major = true;
        preserving.major   = true;
        expansionist.aggression_q = clampi(params.major_expansionist_aggression_q, 0, 1000);
        preserving.aggression_q   = clampi(params.major_preserving_aggression_q, 0, 1000);
    }

    // Region -> owning polity. Seeded from culture, then owned by conquest.
    std::vector<int> owner(ss.regions.size(), -1);
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
        for (const polity& q : out.polities)
            // BL-826 — PLURALITY, matching the seed: a polity exists per
            // distinct plurality culture, so this assigns each region to the
            // polity of the people who are largest on it.
            if (q.culture == ss.regions[i].culture.plurality()) { owner[i] = q.id; break; }

    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        region& p = ss.regions[i];
        p.nation = owner[i];
        // Seed a headcount so demography has something to grow from — the
        // graduation path settlement.hpp's demography note leaves to this item.
        if (p.population <= 0)
            p.population = region_seed_population(p.farm_q); // BL-766: ONE derivation,
                                                             // shared with `draw_urban_map`.
        p.last_demography_year = params.start_year;
        replenish_manpower(p);
    }

    // --- Per-year war pressure, reset each tick ---------------------------
    std::vector<int> war_pressure(ss.regions.size(), 0);

    // --- THE ANCIENT ROAD RECORD (BL-768) ---------------------------------
    //
    // Appended raw as events happen, then sorted and run-length-encoded into
    // `out.supply_corridors` at the end of the run. A raw list plus one sort is
    // deliberately preferred to a keyed map: the sort key is a pair of plain
    // integers, so the result cannot depend on a container's layout, and the
    // hot loop pays a push_back rather than a tree lookup.
    //
    // PURE OBSERVATION. Nothing below reads this back, so it cannot move a
    // decision — the same contract `battle_trace` holds, and the reason both
    // can be recorded unconditionally without a determinism argument.
    std::vector<std::pair<uint16_t, uint16_t>> corridor_uses;
    const auto note_corridor = [&](int a, int b) {
        if (a < 0 || b < 0 || a == b) return;
        if (a >= static_cast<int>(owner_index_limit)
         || b >= static_cast<int>(owner_index_limit)) return;
        const uint16_t lo = static_cast<uint16_t>(a < b ? a : b);
        const uint16_t hi = static_cast<uint16_t>(a < b ? b : a);
        corridor_uses.push_back({lo, hi});
    };

    // --- GRUDGES (BL-827) -------------------------------------------------
    //
    // Directed, sparse, decaying, and carrying its cause. The table is a sorted
    // vector searched by binary search — never a keyed map — so its order is a
    // property of the integers in it rather than of a container's layout.
    //
    // NOTHING BELOW READS A GRUDGE TO MAKE A DECISION. It is the same pure-
    // observation contract `battle_trace` holds, and it is what keeps this a
    // data-model change rather than an agent term: the scorer's inputs are
    // identical with this block deleted.
    const auto grudge_slot = [&](int from, int to) -> grudge* {
        if (from < 0 || to < 0 || from == to) return nullptr;
        if (from > 0xFFFE || to > 0xFFFE) return nullptr;
        const uint16_t f = static_cast<uint16_t>(from), t = static_cast<uint16_t>(to);
        const auto it = std::lower_bound(
            out.grudges.begin(), out.grudges.end(), std::pair<uint16_t, uint16_t>{f, t},
            [](const grudge& g, const std::pair<uint16_t, uint16_t>& k) {
                if (g.from != k.first) return g.from < k.first;
                return g.to < k.second;
            });
        if (it != out.grudges.end() && it->from == f && it->to == t) return &*it;
        grudge g;
        g.from = f;
        g.to   = t;
        return &*out.grudges.insert(it, g);
    };

    /// Raise one named event. Magnitude, place and date all recorded; the top
    /// few by magnitude are kept and the rest fall into the scalar, which is
    /// what makes the record bounded without making it a bare number.
    const auto raise_grudge = [&](int from, int to, grudge_kind kind,
                                  int region_idx, int64_t year, int magnitude) {
        if (magnitude <= 0) return;
        grudge* g = grudge_slot(from, to);
        if (!g) return;
        g->score = static_cast<int32_t>(
            clampi(g->score + magnitude, 0, std::max(1, params.grudge_cap)));
        if (g->score > g->peak) g->peak = g->score;
        ++g->event_count;

        grudge_event e;
        e.year      = static_cast<int32_t>(year);
        e.region    = (region_idx >= 0 && region_idx < static_cast<int>(owner_index_limit))
                        ? static_cast<uint16_t>(region_idx) : owner_none;
        e.kind      = kind;
        e.magnitude = static_cast<int32_t>(magnitude);

        // Keep the largest few, sorted descending by magnitude, ties on the
        // LATER year, then on the lower kind. A total order with an explicit
        // tie-break, so the kept set cannot depend on arrival order.
        const auto beats = [](const grudge_event& a, const grudge_event& b) {
            if (a.magnitude != b.magnitude) return a.magnitude > b.magnitude;
            if (a.year != b.year)           return a.year > b.year;
            return static_cast<int>(a.kind) < static_cast<int>(b.kind);
        };
        int at = static_cast<int>(g->events_kept);
        if (at < grudge_events_kept) { g->events[at] = e; g->events_kept = at + 1; }
        else if (beats(e, g->events[grudge_events_kept - 1]))
            g->events[grudge_events_kept - 1] = e;
        else return;
        for (int i = static_cast<int>(g->events_kept) - 1; i > 0; --i)
            if (beats(g->events[i], g->events[i - 1]))
                std::swap(g->events[i], g->events[i - 1]);
    };

    /// A polity died. ITS GRUDGES ARE LOST, IN BOTH DIRECTIONS — see the
    /// `history_sim_state::grudges` note for why that is the call rather than
    /// inheritance. What survives is `realm_ended`, raised from every surviving
    /// polity of the dead realm's people toward its killer, walked in polity-id
    /// order so the set and its order are both deterministic.
    const auto extinguish_polity = [&](int dead, int killer, int region_idx, int64_t year) {
        const int dead_culture = (dead >= 0 && dead < static_cast<int>(out.polities.size()))
                               ? out.polities[static_cast<std::size_t>(dead)].culture : -1;
        out.grudges.erase(
            std::remove_if(out.grudges.begin(), out.grudges.end(),
                           [&](const grudge& g) {
                               return g.from == static_cast<uint16_t>(dead)
                                   || g.to   == static_cast<uint16_t>(dead);
                           }),
            out.grudges.end());
        if (killer < 0 || dead_culture < 0) return;
        for (const polity& kin : out.polities)
        {
            if (!kin.alive || kin.id == dead || kin.id == killer) continue;
            if (kin.culture != dead_culture) continue;
            raise_grudge(kin.id, killer, grudge_kind::realm_ended,
                         region_idx, year, params.grudge_realm_ended);
        }
    };

    // --- Neighbour index --------------------------------------------------
    //
    // Campaign candidates are NEIGHBOURS ONLY, so the neighbourhood is built
    // once rather than rediscovered by scanning every region from every held
    // region every year. That scan is quadratic in region count and, with
    // the Settle verb growing the map past 400 regions, it dominated the
    // whole run (2.5s of a 2.5s run). Built once here, extended when a
    // region is founded, it is a lookup.
    std::vector<std::vector<int>> neighbours(ss.regions.size());
    const auto link_region = [&](std::size_t i) {
        for (std::size_t j = 0; j < ss.regions.size(); ++j)
        {
            if (i == j) continue;
            if (region_distance(ss.regions[i], ss.regions[j], gw) <= params.neighbour_radius)
            {
                neighbours[i].push_back(static_cast<int>(j));
                neighbours[j].push_back(static_cast<int>(i));
            }
        }
    };
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
        for (std::size_t j = i + 1; j < ss.regions.size(); ++j)
            if (region_distance(ss.regions[i], ss.regions[j], gw) <= params.neighbour_radius)
            {
                neighbours[i].push_back(static_cast<int>(j));
                neighbours[j].push_back(static_cast<int>(i));
            }

    // --- Terrain-weighted reach (BL-314 S2) -------------------------------
    //
    // Distance from the capital is a COST over the neighbour graph, not a
    // straight line: crossing a mountain range costs about twice what crossing
    // plains does, using the landform ratios logistics.cpp already defines for
    // the 1960 era. Computed by Dijkstra from the capital and cached until the
    // capital moves, so the per-year cost stays a lookup.
    std::vector<int> reach;            // Per-region cost from the current capital.
    int reach_capital = -2;            // Which capital `reach` was built for.

    const auto tile_cost = [&](const region& p) {
        // Landform ratios, x100: plains 100, highland 125, mountain 200, ...
        switch (lf_at(terrain, p.anchor))
        {
        case terrain_landform::mountain: return 200;
        case terrain_landform::rift:     return 160;
        case terrain_landform::canyon:   return 150;
        case terrain_landform::crater:   return 130;
        case terrain_landform::highland: return 125;
        case terrain_landform::valley:   return 110;
        default:                         return 100;
        }
    };

    const auto rebuild_reach = [&](int capital) {
        const scoped_ns prof_reach(prof.ns_reach); // BL-825, report-only
        ++prof.reach_rebuilds;
        reach.assign(ss.regions.size(), 1 << 28);
        if (capital < 0 || capital >= static_cast<int>(ss.regions.size())) return;
        reach[static_cast<std::size_t>(capital)] = 0;

        // Dijkstra without a heap: region counts are hundreds, and a simple
        // scan keeps the order deterministic without depending on a tie-break
        // inside a priority queue.
        std::vector<bool> done(ss.regions.size(), false);
        for (std::size_t iter = 0; iter < ss.regions.size(); ++iter)
        {
            int best = -1, best_c = 1 << 28;
            for (std::size_t i = 0; i < reach.size(); ++i)
                if (!done[i] && reach[i] < best_c) { best_c = reach[i]; best = static_cast<int>(i); }
            if (best < 0) break;
            done[static_cast<std::size_t>(best)] = true;

            const region& bp = ss.regions[static_cast<std::size_t>(best)];
            for (int nb : neighbours[static_cast<std::size_t>(best)])
            {
                const region& np2 = ss.regions[static_cast<std::size_t>(nb)];
                const int step = region_distance(bp, np2, gw)
                               * (tile_cost(bp) + tile_cost(np2)) / 200;
                const int cand = best_c + (step > 0 ? step : 1);
                if (cand < reach[static_cast<std::size_t>(nb)])
                    reach[static_cast<std::size_t>(nb)] = cand;
            }
        }
        reach_capital = capital;
    };

    // --- Time-lapse change list -------------------------------------------
    out.owner_changes.clear();
    for (std::size_t i = 0; i < owner.size(); ++i)
        if (owner[i] >= 0)
            out.owner_changes.push_back(owner_change{
                static_cast<int32_t>(params.start_year),
                static_cast<uint16_t>(i),
                static_cast<uint16_t>(owner[i])});

    const int64_t years = params.stop_year - params.start_year;
    out.battles_per_century.assign(static_cast<std::size_t>(years / 100 + 1), 0);

    // The stepped decision clock (Ben, 2026-08-12). `y` still advances one real
    // year at a time — demography must not skip — but the polities only ACT
    // when the year reaches `next_decision`, which walks forward by the current
    // band's step. See history_sim.hpp § The stepped decision clock.
    int64_t next_decision = params.start_year;
    int     step_years    = step_for_year(params, params.start_year);

    for (int64_t y = params.start_year; y < params.stop_year; ++y)
    {
        // Loading-screen sink only — never read back, so the sim stays pure.
        if (year_progress != nullptr)
            year_progress->store(static_cast<int>(y - params.start_year + 1),
                                 std::memory_order_relaxed);

        const std::size_t century =
            static_cast<std::size_t>((y - params.start_year) / 100);

        // ---- Demography -------------------------------------------------
        int64_t total_pop = 0;
        {
        const scoped_ns prof_demo(prof.ns_demography); // BL-825, report-only
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
        {
            advance_region_demography(ss.regions[i], 1, war_pressure[i]);
            // BL-766: the cities drawn before this loop started live through it
            // — they grow with the region and thin when it thins.
            advance_region_urban(ss.regions[i]);
            war_pressure[i] = 0;
            total_pop += ss.regions[i].population;
        }
        } // BL-825 demography timer
        if (total_pop > out.peak_population)
        {
            out.peak_population = total_pop;
            out.peak_year       = y;
        }

        // ---- The decision gate -------------------------------------------
        //
        // Below the band's step there is nothing for the polities to do this
        // year: demography above has already run, and the scorer — the
        // expensive half of this loop — is deliberately coarse in deep
        // prehistory. `step_years` is read here and used by the three RATE
        // applications further down (tech progress, cohesion recovery, contest
        // decay), which must cover the whole interval since the last round.
        if (y < next_decision)
            continue;
        step_years    = step_for_year(params, y);
        next_decision = y + step_years;

        // ---- GRUDGE DECAY (BL-827) ---------------------------------------
        //
        // A RATE, so it is multiplied by the step exactly like tech progress,
        // cohesion recovery and contest decay — see § The stepped decision
        // clock. Without it a 4000-year run reaches the epoch with every pair
        // maximally aggrieved, which carries no information at all.
        //
        // Entries that decay under the floor are DROPPED, which is what keeps
        // the table sparse across four millennia rather than merely starting
        // sparse. `peak` is not decayed: it is the record of how bad it once
        // got, and a decayed peak would be a second copy of `score`.
        if (params.grudge_decay_per_year_q > 0 && !out.grudges.empty())
        {
            const int shed = clampi(params.grudge_decay_per_year_q * step_years, 0, 1000);
            for (grudge& g : out.grudges)
                g.score -= (g.score * shed) / 1000;
            const int floor_q = std::max(0, params.grudge_floor);
            out.grudges.erase(
                std::remove_if(out.grudges.begin(), out.grudges.end(),
                               [&](const grudge& g) { return g.score <= floor_q; }),
                out.grudges.end());
        }

        // ---- Each polity acts, in id order (deterministic) ---------------
        const scoped_ns prof_dec(prof.ns_decisions); // BL-825, report-only
        ++prof.decision_rounds;
        for (polity& q : out.polities)
        {
            if (!q.alive) continue;

            // Holdings, and whether the capital still stands.
            std::vector<int> held;
            for (std::size_t i = 0; i < owner.size(); ++i)
                if (owner[i] == q.id) held.push_back(static_cast<int>(i));

            if (held.empty()) { q.alive = false; continue; }
            if (q.capital < 0 || owner[static_cast<std::size_t>(q.capital)] != q.id)
                // Capital fell. The successor is the polity's lowest-indexed
                // surviving region — placement order, which is best-ground
                // first, so it is a reasonable seat without being "the largest
                // holding" the first cut's comment claimed (BL-312).
                q.capital = held.front();

            // ---- ASSIMILATION (BL-826) --------------------------------
            //
            // HOLDING GROUND DIGESTS IT, SLOWLY. This is the whole anti-hegemony
            // half of the shares change: the conquest that used to flip a
            // region's culture at the instant the border moved now shifts it a
            // couple of per-mille of the foreign remainder a year, so a realm
            // that took a province four centuries ago is roughly half way and
            // one that took it last century has barely started. The score's
            // `w_cult` term reads that share, so wide recent conquest keeps
            // charging its owner and narrow old holdings stop.
            //
            // A RATE x THE STEP, like the three rates above it. Walked over
            // `held`, which is built in region-index order, so the order is a
            // property of the map and not of anything transient. Nothing is
            // appended to `ss.regions` here, so no reference taken below is
            // invalidated by it.
            if (params.assimilation_per_year_q > 0)
            {
                const int shift_q = clampi(params.assimilation_per_year_q * step_years, 0, 1000);
                for (int hi : held)
                    ss.regions[static_cast<std::size_t>(hi)].culture.shift_toward(q.culture, shift_q);
            }

            const region& cap = ss.regions[static_cast<std::size_t>(q.capital)];
            const uint32_t  qs  = salt(seed, static_cast<uint32_t>(q.id));

            if (reach_capital != q.capital || reach.size() != ss.regions.size())
                rebuild_reach(q.capital);

            // ---- What this polity's WORKS are worth it (BL-321) -----------
            //
            // One pass over `held`, producing the three aggregates the round
            // needs: the mean holding value (the denominator a work's
            // empire-wide benefit is scored against), the mean reach investment
            // (what relieves the burden of breadth), and the mean industrial
            // investment (what accelerates the tech ladder).
            //
            // MEANS, NOT TOTALS, throughout. A total would make every aggregate
            // grow with conquest alone, so a large empire would count itself as
            // well-roaded for having many unroaded regions — the opposite of
            // what the burden of breadth is measuring.
            int64_t holdings_value_sum = 0;
            int64_t works_reach_sum    = 0;
            int64_t works_ind_sum      = 0;
            for (int hi : held)
            {
                const region& hp = ss.regions[static_cast<std::size_t>(hi)];
                holdings_value_sum += region_value_q(hp);
                works_reach_sum    += hp.work_reach_mod;
                works_ind_sum      += hp.work_industrial_mod;
            }
            const int n_held = static_cast<int>(held.size()); // >= 1: empty held returned above.
            const int mean_holding_value = static_cast<int>(holdings_value_sum / n_held);
            const int mean_reach_q       = static_cast<int>(works_reach_sum / n_held);
            const int mean_industrial_q  = static_cast<int>(works_ind_sum / n_held);

            // ---- THE MATERIALS BAND, DERIVED ONCE (BL-748) ----------------
            //
            // Keyed off MATERIALS, not military. The unit roster reads the
            // military column because that is the column whose rows turn over
            // at a roster boundary; a Blast Works turns over with metallurgy
            // instead. Same band enum, different column — which is the point of
            // the two tables sharing `roster_band` rather than one deriving
            // from the other.
            //
            // ONE derivation, TWO readers: the `build_work` candidate below,
            // and the furnace at the end of this round. They must agree, or a
            // polity would light furnaces at a band it could not build at —
            // and re-deriving it at each site is exactly the drift this file's
            // header warns about (see `build_stack`'s `band_out`).
            // The MILITARY band, derived exactly as `build_stack` derives it,
            // and read by the BL-778 legality gate to ask whether this polity
            // can field anything that may hold open ocean. Same one-derivation
            // discipline as `mat_band` immediately below.
            const roster_band mil_band = min_band(
                roster_band_for_capacity(
                    clampi(q.capacity[static_cast<int>(sim_domain::military)], 1, 6)),
                sim_band_ceiling(params, y));

            const roster_band mat_band = min_band(
                roster_band_for_capacity(
                    clampi(q.capacity[static_cast<int>(sim_domain::materials)], 1, 6)),
                sim_band_ceiling(params, y));

            // THE BURDEN OF BREADTH (BL-314 S3). Every region held past
            // `free_holdings` costs supply on every campaign this polity runs.
            const int over = static_cast<int>(held.size()) - params.free_holdings;
            int burden = over > 0 ? over * params.holdings_burden_q : 0;

            // AND THE COUNTER-MOVE (BL-321). A polity that spent its rounds on
            // roads and wharves administers its breadth more cheaply. Relief is
            // proportional and capped below 1000, so building buys a discount
            // and never an exemption: the stall still arrives, just later and
            // by the polity's own choice rather than by arithmetic it could do
            // nothing about. That difference — ceiling to decision — is the
            // whole reason this item exists.
            {
                const int relief = clampi(mean_reach_q, 0, params.work_reach_relief_cap_q);
                burden = burden - (burden * relief) / 1000;
            }

            // ONE PRICE FOR SUPPLY, PAID BY BOTH THE SCORER AND THE BATTLE
            // (BL-318). These were two separate expressions, and they disagreed:
            // the scorer measured the staging hub's distance (bounded by
            // neighbour_radius, so at most ~250 per-mille of decay) while the
            // executed battle re-derived it from the CAPITAL (unbounded, and on
            // a 312-wide map routinely past the 1000 that clamps supply to
            // zero). So the scorer chose campaigns it believed were nearly fully
            // supplied and then fought them at nothing, which is why
            // `stalled_campaigns` counted almost every launch.
            //
            // That is this item's whole thesis in miniature — a cost authored on
            // one scale and spent on another — so the fix is not to pick the
            // better of the two lines but to delete one of them. Priced here,
            // once, the estimate and the outcome cannot drift again.
            //
            // The three terms are the three costs the design names: LOCAL
            // staging distance, STRATEGIC terrain-weighted reach from the
            // capital (BL-316 S2), and the BURDEN OF BREADTH (BL-316 S3).
            // `hub` is the region the campaign is STAGED FROM, and it is a
            // parameter rather than a capture because its works discount the
            // terrain cost (BL-321): reach_mod is authored as a discount on the
            // supply cost through/from a region, so it is the staging
            // holding's roads and wharves doing the carrying, not the capital's.
            //
            // The discount applies to the TERRAIN term alone. A span bridge
            // makes a mountain cheaper to cross; it does not shorten the march,
            // which is what supply_decay_per_tile_q charges for.
            const auto campaign_supply = [&](int hub_dist, std::size_t ti, int hub) {
                const int reach_here = (ti < reach.size() && reach[ti] < (1 << 27))
                                     ? static_cast<int>(reach[ti]) : hub_dist;
                const int hub_reach_q = (hub >= 0)
                    ? clampi(ss.regions[static_cast<std::size_t>(hub)].work_reach_mod,
                             0, params.work_reach_relief_cap_q)
                    : 0;
                const int terrain_cost = reach_here * params.terrain_reach_cost_q / 100;
                const int terrain_paid = terrain_cost - (terrain_cost * hub_reach_q) / 1000;
                return clampi(1000
                            - hub_dist * params.supply_decay_per_tile_q
                            - terrain_paid
                            - clampi(burden, 0, 1000 - params.holdings_burden_floor_q),
                            0, 1000);
            };

            // ---- BL-778 / BL-779: the water gate on a campaign edge -------
            //
            // TWO QUESTIONS, ASKED OF ONE EDGE, and they are deliberately
            // different questions with different answers.
            //
            // LEGALITY (BL-778, general — docs/military/MILITARY.md § Domains
            // and traversal): a land force may not cross sea it does not own.
            // The sim's adjacency is a water-blind Chebyshev radius, so until
            // now a polity campaigned across up to nine tiles of open water for
            // free, on 43% of its edges (BL-755's census). Priced as a LEGALITY
            // TEST rather than a supply penalty, because a legality test cannot
            // be tuned into meaninglessness.
            //
            // FORAGE (ancient-sim SIMPLIFICATION — docs/generation/
            // MILITARY_HISTORY.md § Forage): a force feeds where it is adjacent
            // to land or water its own polity owns, and starves where it is
            // not. It lives HERE, in the sim's caller, and NOT in
            // `terrain_combat`'s table: both resolvers read that table, and the
            // general claim is the opposite one — overseas supply works. It is
            // a stand-in for a logistics model this sim cannot afford, not a
            // statement about how supply works in Io.
            //
            // The bridging case is the polity's OWN SHORE. A q-owned coastal
            // region touching the target is the shallow, causewayed water a
            // land force may wade, so it makes the crossing both legal and
            // fed — which is what makes littoral empires cohere and stops
            // nothing but the free ocean hop.
            const auto owns_shore_at = [&](std::size_t ti) {
                for (int n : neighbours[ti])
                {
                    const std::size_t ni = static_cast<std::size_t>(n);
                    if (owner[ni] == q.id
                     && ss.regions[ni].domain == region_domain::coastal_water)
                        return true;
                }
                return false;
            };
            // The staging holding reaches the target overland (or over its own
            // lakes) — no sea in the line at all.
            const auto dry_contact = [&](int hub, std::size_t ti) {
                return !line_crosses_sea(ss.regions[static_cast<std::size_t>(hub)],
                                         ss.regions[ti], terrain, gw, gh,
                                         params.neighbour_radius);
            };

            // ---- Build the bounded candidate set -------------------------
            //
            // Four verbs. The set is bounded by construction: neighbours only,
            // one settle site, one invest domain, one consolidate.
            sim_verb best_verb  = sim_verb::none;
            int      best_score = 0;
            int      best_target = -1;
            bool     best_winter = false;
            // Which works row `build_work` chose. A second field rather than an
            // overload of `best_target`, because build_work needs BOTH a
            // region and a row and the other verbs' single target already
            // means three different things (region, parent, domain).
            int      best_work_row = -1;
            // BL-384 instrumentation, READ ONLY BY THE TRACE. The scorer's own
            // estimate of the odds for the candidate it chose, and the staging
            // hub it scored those odds against. Carried because the trace has to
            // compare the ESTIMATE against the REALISED outcome, and by the time
            // the battle resolves the scoring locals are long out of scope.
            //
            // Bookkeeping only: nothing reads these to make a decision, so the
            // run is byte-identical with tracing on or off.
            int      best_p_win_q = 0;
            int      best_hub     = -1;
            // Sprint 28 lane A instrumentation, ALSO READ ONLY BY THE TRACE.
            // The best Campaign score that cleared `campaign_threshold_q` this
            // round, kept separately from `best_score` because `best_score` is
            // overwritten by whichever verb wins — by the time the round ends
            // there is no way to recover what Campaign offered, which is the
            // one number the fork needs. Both are written only under
            // `trace_battles` and neither is ever compared against, so the run
            // is byte-identical with tracing on or off.
            int      best_campaign_score  = 0;
            bool     campaign_cleared_now = false;

            // -- Campaign --------------------------------------------------
            for (int hi : held)
            {
                for (int tn : neighbours[static_cast<std::size_t>(hi)])
                {
                    const std::size_t ti = static_cast<std::size_t>(tn);
                    const int to = owner[ti];
                    if (to == q.id || to < 0) continue;
                    if (params.trace_battles) ++out.campaign_contacts;

                    // THE WATER GATE, applied before anything is scored — an
                    // illegal campaign is not a bad candidate, it is not a
                    // candidate. See the two lambdas above for the rule.
                    const bool dry   = dry_contact(hi, ti);
                    const bool shore = dry ? true : owns_shore_at(ti);
                    if (!dry && !shore
                     && !can_field_naval(ss.regions[static_cast<std::size_t>(hi)], mil_band))
                    {
                        ++out.illegal_campaigns;
                        continue;
                    }
                    // Forage is the SAME reading: fed on one's own ground or
                    // one's own shore, starving on a sea leg carried by ships.
                    // Ships get the force there; they do not feed it.
                    const bool forages = dry || shore;

                    const region& tgt = ss.regions[ti];
                    const int cap_dist = region_distance(cap, tgt, gw);

                    // SUPPLY PROJECTS FROM THE STAGING HOLDING, NOT THE CAPITAL
                    // (Ben, 2026-08-12: "perhaps we can try instructing supply
                    // hubs?").
                    //
                    // `hi` is already the polity's own region adjacent to the
                    // target — it IS a supply hub, and the loop was standing in
                    // it while measuring supply all the way back to the capital.
                    // That made reach a property of empire SHAPE rather than of
                    // frontier presence: a large polity could not attack its own
                    // border because its capital was far away.
                    //
                    // Measuring from the staging region instead is both the
                    // better model (armies victual at the frontier) and what
                    // makes reach scale-free — hub_dist is bounded by
                    // neighbour_radius, so it cannot blow up when the map does.
                    // The capital still matters, as the preference term below.
                    const int hub_dist =
                        region_distance(ss.regions[static_cast<std::size_t>(hi)], tgt, gw);

                    // Defender's fielded power, as a headcount proxy, raised by
                    // whatever the target has BUILT (BL-321). A Wall Circuit
                    // has to be visible to the scorer, not only to
                    // resolve_battle: a polity that walked into a bastion it
                    // could not see would be making the decision on stale
                    // information every time, and the work would read as bad
                    // luck rather than as the defender's choice it is.
                    const int64_t def_men = (tgt.manpower_stock * params.levy_fraction_q) / 1000;
                    const int def_works   = clampi(tgt.work_defence_mod, 0, 1000);
                    const int def_scaled  = static_cast<int>(clampi64(
                        (def_men / 64) * (1000 + def_works) / 1000, 0, 1000));

                    // Three DIFFERENT axes, so a polity can prize farmland and
                    // shrug at ore rather than merely valuing everything alike.
                    int value = (jitter(params.w_farm, qs, 0xA1u, 160) * tgt.farm_q
                              +  jitter(params.w_ore,  qs, 0xB2u, 160) * tgt.ore_q
                              +  jitter(params.w_port, qs, 0xC3u, 160) * tgt.port_q) / 1000;

                    // RING CLOSURE — the mechanical answer to "what makes a
                    // Rome" (BL-277 Q1). A coastal target next to coast this
                    // polity already holds advances the ring, so littoral
                    // hegemony emerges from a scored term, never a script.
                    if (tgt.port_q > 400)
                    {
                        int coastal_held = 0;
                        for (int h2 : held)
                            if (ss.regions[static_cast<std::size_t>(h2)].port_q > 400
                             && region_distance(ss.regions[static_cast<std::size_t>(h2)], tgt, gw)
                                    <= params.neighbour_radius)
                                ++coastal_held;
                        const int ring_closure_q = clampi(coastal_held * 250, 0, 1000);
                        value += (params.w_ring * ring_closure_q) / 1000;
                    }

                    // In the currency: expected value TAKEN, discounted by the
                    // odds of taking it, less what the attempt costs.
                    value = (value * params.campaign_gain_q) / 1000;

                    // Odds from the power ratio the sim can actually estimate:
                    // levy x supply x cohesion against the defender's levy.
                    // A force that cannot forage arrives at NOTHING. The
                    // starvation is expressed through the supply channel the
                    // resolver already reads rather than through a term of its
                    // own — zero supply takes the full attrition hit, which is
                    // the pressure the rule is for (MILITARY_HISTORY.md
                    // § Forage). No new constant: it is the same 0..1000.
                    const int supply_here =
                        forages ? campaign_supply(hub_dist, ti, hi) : 0;

                    int64_t atk_men = 0;
                    for (int hi2 : held)
                        if (ss.regions[static_cast<std::size_t>(hi2)].manpower_stock > atk_men)
                            atk_men = ss.regions[static_cast<std::size_t>(hi2)].manpower_stock;
                    const int64_t atk_est = (atk_men * supply_here / 1000)
                                          * clampi(q.cohesion_q, 1, 1000) / 1000;
                    const int64_t def_est = tgt.manpower_stock > 0 ? tgt.manpower_stock : 1;
                    const int p_win_q = static_cast<int>(
                        clampi64((atk_est * 1000) / (atk_est + def_est), 0, 1000));

                    value = (value * p_win_q) / 1000;

                    // Costs, in the same unit: distance is a real logistics
                    // cost now (BL-314), not just a preference.
                    // DISTANCE IS A PREFERENCE HERE, NOT THE COST — and it must
                    // be proportional for the same reason w_cult had to be
                    // (2026-08-12).
                    //
                    // This was `value -= w_dist * cap_dist / 10`, flat. It was
                    // survivable on a 180-wide map and fatal on a 312-wide one:
                    // capital-to-frontier distances scale with the map, region
                    // values do not, so tripling the grid tripled this penalty
                    // against an unchanged prize and drove every campaign score
                    // below threshold. Measured on the real world: 0 battles,
                    // 1195 foundings — a completely peaceful 400 years.
                    //
                    // The REAL cost of distance is already modelled, twice, and
                    // properly: supply_decay_per_tile_q and terrain_reach_cost_q
                    // both feed `supply_here` below, which feeds both the odds
                    // and an explicit supply cost. This term only ever expressed
                    // "nearer is nicer", so as a per-mille discount it says that
                    // at any map size instead of vetoing war on large ones.
                    value = (value * clampi(1000 - params.w_dist * cap_dist / 100, 200, 1000)) / 1000;
                    value -= (1000 - supply_here) * params.campaign_supply_cost_q / 1000;
                    // FOREIGN GROUND IS WORTH LESS, PROPORTIONALLY — not a flat
                    // toll (2026-08-12).
                    //
                    // This was `value -= w_cult`, a flat 150 subtracted from a
                    // region value that measures only ~200-300. It therefore
                    // ate half to three quarters of the entire prize on every
                    // cross-cultural target, which is nearly all of them, and it
                    // alone suppressed EVERY war: measured across a 400-year run,
                    // w_cult 150 -> 0 battles, and 0 -> 266 battles / 150
                    // conquests, with w_dist and w_def making no difference at
                    // all. A term meant to express a preference was acting as a
                    // veto.
                    //
                    // That is BL-318's incommensurability in miniature: a cost
                    // authored on one scale subtracted from a value on another.
                    // As a fraction it means the same thing at any region
                    // value — foreign ground is worth (1000 - w_cult)/1000 of
                    // what home ground is — so the signal survives and the veto
                    // does not.
                    //
                    // BL-826 — AND NOW IT IS A QUESTION ABOUT THE DISTRIBUTION
                    // RATHER THAN AN EQUALITY TEST, which is the second half of
                    // making assimilation a real lever. `tgt.culture != q.culture`
                    // was a step function: ground was foreign or it was not, so
                    // a conquest flipped the whole discount off the instant the
                    // border moved and a conqueror was charged for ground only
                    // once. Charged against the FOREIGN SHARE instead, the
                    // discount fades exactly as fast as the ground stops being
                    // foreign — which is centuries, at
                    // `assimilation_per_year_q`. A realm that has just taken a
                    // province still pays nearly the full discount to take the
                    // next one beside it, and that is the anti-hegemony force:
                    // conquest is digested, not annexed.
                    //
                    // Recovers the old behaviour exactly at the two ends: pure
                    // foreign ground pays 1000/1000 of w_cult, pure own ground
                    // pays none.
                    {
                        const int foreign_q = 1000 - tgt.culture.share_of(q.culture);
                        const int cult_q    =
                            (clampi(params.w_cult, 0, 1000) * clampi(foreign_q, 0, 1000)) / 1000;
                        value = (value * (1000 - cult_q)) / 1000;
                    }

                    // Season as an action axis: summer and winter are two
                    // candidates over the same objective, not two ticks.
                    //
                    // Winter's BENEFIT must appear in the score, not only at
                    // execution. Scoring winter with the same defender power as
                    // summer and then charging it a premium makes it strictly
                    // worse than summer, so it is never chosen and the whole
                    // axis is dead — the first cut had exactly that bug. The
                    // defender's readiness penalty is what the scorer is
                    // trading the premium and the extra attrition against.
                    for (int w = 0; w < 2; ++w)
                    {
                        const bool winter = (w == 1);
                        const int def_ready = winter ? (1000 - params.winter_readiness_penalty_q) : 1000;
                        const int def_eff   = (def_scaled * def_ready) / 1000;

                        int s = value - (params.w_def * def_eff) / 2000;
                        if (winter) s -= params.winter_score_premium_q;
                        if (params.trace_battles) ++out.campaign_scored;
                        s += static_cast<int>(salt(qs, static_cast<uint32_t>(ti)) % 16u); // Stable tie-break.

                        // THE THRESHOLD TEST, OBSERVED SEPARATELY FROM THE
                        // ARGMAX. The line below fuses two questions with `&&`
                        // — "is this worth doing" and "is it the best thing to
                        // do" — and a candidate that fails the first is
                        // indistinguishable from one that fails the second in
                        // any counter taken after it. Read here, on the same
                        // `s` and the same constant the decision uses, so the
                        // measurement cannot drift from the thing measured.
                        if (params.trace_battles && s >= params.campaign_threshold_q)
                        {
                            ++out.campaign_cleared;
                            campaign_cleared_now = true;
                            if (s > best_campaign_score) best_campaign_score = s;
                        }

                        if (s > best_score && s >= params.campaign_threshold_q)
                        {
                            best_score = s; best_verb = sim_verb::campaign;
                            best_target = static_cast<int>(ti); best_winter = winter;
                            best_p_win_q = p_win_q; best_hub = hi; // trace only
                        }
                    }
                }
            }

            // -- Settle ----------------------------------------------------
            //
            // The growth-without-war axis. `run_settlement` founds every
            // region before the stop year and leaves none after it, so
            // without this verb a 2000-year run has a frozen region count.
            {
                int pressure_best = -1, pressure_src = -1;
                for (int hi : held)
                {
                    const region& p = ss.regions[static_cast<std::size_t>(hi)];
                    // The works-aware ceiling (BL-321), matching what
                    // `advance_region_demography` actually grows toward. The
                    // plain overload would have read a Granary region as far
                    // more crowded than it is and sent it out to settle land it
                    // did not need — the settle pressure and the growth model
                    // must divide by the same K or the verb fires on a fiction.
                    const int64_t K = region_carrying_capacity(p.farm_q, p.work_capacity_mod);
                    if (K <= 0) continue;
                    const int pressure = static_cast<int>(clampi64((p.population * 1000) / K, 0, 1000));
                    if (pressure > pressure_best) { pressure_best = pressure; pressure_src = hi; }
                }
                // A polity fighting for its life does not colonise (BL-308).
                // Letting it was the main reason losers regrew faster than they
                // were conquered, and why elimination never happened.
                const bool may_settle = q.cohesion_q >= params.settle_cohesion_gate_q;
                if (may_settle && pressure_src >= 0 && pressure_best >= params.settle_pressure_q)
                {
                    const region& sp = ss.regions[static_cast<std::size_t>(pressure_src)];
                    const int daughter_value = (region_value_q(sp) * 800) / 1000;
                    const int s = (daughter_value * pressure_best) / 1000
                                - (static_cast<int>(held.size()) > params.free_holdings
                                   ? params.holdings_burden_q * 4 : 0);
                    if (s > best_score && s >= params.settle_threshold_q)
                    {
                        best_score = s; best_verb = sim_verb::settle; best_target = pressure_src;
                        best_winter = false;
                    }
                }
            }

            // -- Invest ----------------------------------------------------
            {
                // ONE DOMAIN, CHOSEN BY ARREARS AND BY GROUND (BL-767).
                //
                // The old rule was "whichever domain sits at the lowest band",
                // which levels all seven in lockstep and makes `capacity[]` a
                // flat line rather than the profile the ladder asks for. Two
                // weighted pulls now decide it: how far a domain is behind, and
                // what the polity's ground argues for. See
                // `invest_ground_pull_q` for the measurement that motivated it.
                //
                // Integer throughout, and the tie-break is the lower enum
                // value — the same rule the old argmin used, so a world with no
                // ground signal at all decides exactly as it did.
                int dom = 0;
                {
                    // The polity's ground, as the mean of what it holds. Means,
                    // not totals, for the reason the works aggregates above
                    // give: a total makes conquest alone look like endowment.
                    int64_t farm_sum = 0, ore_sum = 0, energy_sum = 0, port_sum = 0;
                    for (int hi : held)
                    {
                        const region& hp = ss.regions[static_cast<std::size_t>(hi)];
                        farm_sum   += hp.farm_q;
                        ore_sum    += hp.ore_q;
                        energy_sum += hp.energy_q;
                        port_sum   += hp.port_q;
                    }
                    // Four windows, seven domains. Institutions, military and
                    // medicine have no ground behind them and take 0 — they
                    // advance on arrears alone, which is the honest reading of
                    // "no endowment window measures this".
                    int ground_q[sim_domain_count] = {0, 0, 0, 0, 0, 0, 0};
                    ground_q[static_cast<int>(sim_domain::agriculture)] =
                        static_cast<int>(farm_sum / n_held);
                    ground_q[static_cast<int>(sim_domain::materials)] =
                        static_cast<int>(ore_sum / n_held);
                    ground_q[static_cast<int>(sim_domain::energy)] =
                        static_cast<int>(energy_sum / n_held);
                    ground_q[static_cast<int>(sim_domain::transport)] =
                        static_cast<int>(port_sum / n_held);

                    // A CAPPED DOMAIN IS NOT A CANDIDATE, and leaving it as one
                    // was a real defect rather than a tuning choice. A domain at
                    // the top band has `arrears == 0` but KEEPS its ground pull,
                    // so on good ground it outbids every under-levelled domain
                    // forever — and the investment it wins does nothing, because
                    // the rung below is gated on `capacity[d] < 6` while
                    // `progress_q[d]` goes on accumulating. The polity spends
                    // every remaining round buying a level it already has.
                    //
                    // Measured: a polity with mean farm_q ~900 caps agriculture,
                    // and materials then stalls one band under the Industrial
                    // rung with every later Invest round producing literally
                    // nothing. The old argmin could not do this — it could only
                    // select a capped domain when all seven were capped.
                    int best_pull = INT32_MIN;
                    bool any_open = false;
                    for (int d = 0; d < sim_domain_count; ++d)
                    {
                        if (q.capacity[d] >= 6)
                            continue;               // nothing to buy here
                        any_open = true;
                        const int arrears = 6 - clampi(q.capacity[d], 1, 6);
                        const int pull = arrears * params.invest_level_pull_q
                                       + (ground_q[d] * params.invest_ground_pull_q) / 1000;
                        if (pull > best_pull) { best_pull = pull; dom = d; }
                    }
                    // Every domain capped: the verb has nothing to do. `dom`
                    // stays 0 and the rung gate below makes the round inert,
                    // which is the same outcome the old rule reached.
                    (void)any_open;
                }

                int64_t pop = 0;
                for (int hi : held) pop += ss.regions[static_cast<std::size_t>(hi)].population;

                // Divided by the band already held: each further band costs more
                // to want, so investment does not pin at its ceiling the moment
                // a polity has a few mature regions (BL-309). Marginal value,
                // not accumulated size.
                int64_t holdings_value = 0;
                for (int hi : held)
                    holdings_value += region_value_q(ss.regions[static_cast<std::size_t>(hi)]);
                const int band_now = clampi(q.capacity[dom], 1, 6);
                const int s = static_cast<int>(clampi64(
                    (holdings_value * params.invest_yield_q)
                        / (1000LL * params.invest_amortise_years * band_now),
                    0, 1000));
                (void)pop;
                if (s > best_score && s >= params.invest_threshold_q)
                {
                    best_score = s; best_verb = sim_verb::invest; best_target = dom;
                    best_winter = false;
                }
            }

            // -- Consolidate -----------------------------------------------
            //
            // A scored candidate, not merely the fallback. It is worth more to a
            // polity whose cohesion has been dented, which is what makes the
            // BL-308 death-spiral escape reachable at all: under raw-score
            // comparison Consolidate was never chosen after ~year 176, so the
            // recovery the header promised could not happen.
            {
                int64_t hv = 0;
                for (int hi : held)
                    hv += region_value_q(ss.regions[static_cast<std::size_t>(hi)]);
                const int shortfall = 1000 - clampi(q.cohesion_q, 0, 1000);
                const int s = static_cast<int>(clampi64(
                    (hv * shortfall) / (1000LL * params.consolidate_divisor), 0, 1000));
                if (s > best_score && s >= params.consolidate_threshold_q)
                {
                    best_score = s; best_verb = sim_verb::consolidate;
                    best_target = -1; best_winter = false;
                }
            }
            // -- Build a work (BL-321) -------------------------------------
            //
            // Scored LAST, and that ordering carries a small contract: because
            // every earlier verb uses `>` against `best_score`, a work must
            // strictly beat them, and `best_work_row` is only ever read on the
            // one path that just wrote it.
            //
            // BOUNDED BY CONSTRUCTION, like the other four. Two candidate
            // regions — the capital, and one rotated through the holdings by
            // the year — rather than every holding. Scoring all of them would be
            // O(held x rows) per polity per round inside a pass already costing
            // ~23 s of a ~25 s world; the rotation still reaches every region
            // over a run, because the round count (136 on the default ladder) is
            // large against any one polity's holdings.
            if (works != nullptr && works->size() > 0)
            {
                // The materials band, derived ONCE above this round's verbs
                // (BL-748) so the works table and the furnace read the same
                // value. It used to be re-derived here.
                const roster_band band = mat_band;

                for (int slot = 0; slot < clampi(params.work_candidate_regions, 0, 8); ++slot)
                {
                    int pi = -1;
                    if (slot == 0)
                    {
                        pi = q.capital;
                    }
                    else
                    {
                        // Rotation is a hash of (polity, year, slot), not a
                        // counter: nothing is carried between rounds, so
                        // inserting or removing a decision cannot shift which
                        // region a later round looks at. Same reason the rest
                        // of this file uses `salt` rather than a generator.
                        const uint32_t h = salt(qs, static_cast<uint32_t>(y) * 977u
                                                    + static_cast<uint32_t>(slot));
                        pi = held[static_cast<std::size_t>(h % static_cast<uint32_t>(n_held))];
                    }
                    if (pi < 0 || pi >= static_cast<int>(ss.regions.size())) continue;

                    const region& bp = ss.regions[static_cast<std::size_t>(pi)];
                    const std::vector<const work_row*> avail = works->available(bp, band);

                    for (const work_row* r : avail)
                    {
                        const int id = works->index_of(r);
                        if (id < 0 || static_cast<std::size_t>(id) >= works_mask_bits) continue;
                        // Already standing here: a work is a finite, saturating
                        // investment, not a dial a polity can keep turning.
                        if ((bp.works_built & (uint32_t{1} << id)) != 0) continue;

                        int s = work_score_q(*r, bp, mean_holding_value, params);
                        s += static_cast<int>(salt(qs, static_cast<uint32_t>(id) * 31u
                                                      + static_cast<uint32_t>(pi))
                                              % 8u); // Stable tie-break, as elsewhere.

                        if (s > best_score && s >= params.work_threshold_q)
                        {
                            best_score = s; best_verb = sim_verb::build_work;
                            best_target = pi; best_work_row = id; best_winter = false;
                        }
                    }
                }
            }

            // THE ROUND-GRAIN HALF OF THE FORK (Sprint 28 lane A). Taken here,
            // after every verb has been offered and BEFORE the `none` default
            // below, because the question is which verb WON THE COMPARISON —
            // a round that fell through to Consolidate by default chose
            // nothing, and folding it in with rounds Consolidate actually beat
            // Campaign on would invent a competitor that never competed.
            //
            // (A cleared Campaign always beats the initial `best_score` of 0,
            // so `best_verb` cannot be `none` on any round recorded here. The
            // ordering is still deliberate: it makes that a property of the
            // placement rather than of an argument about the initial value.)
            if (params.trace_battles && campaign_cleared_now)
            {
                ++out.campaign_cleared_rounds;
                if (best_verb != sim_verb::campaign) ++out.campaign_cleared_lost;
                out.verb_contests.push_back(verb_contest_trace{
                    static_cast<int32_t>(y),
                    static_cast<uint16_t>(q.id),
                    best_campaign_score,
                    best_score,
                    best_verb});
            }

            if (best_verb == sim_verb::none) best_verb = sim_verb::consolidate;

            // ---- Execute -------------------------------------------------
            switch (best_verb)
            {
            case sim_verb::campaign:
            {
                if (params.trace_battles) ++out.campaign_chosen;
                const std::size_t ti = static_cast<std::size_t>(best_target);
                region& tgt = ss.regions[ti];

                // Nearest LEGAL holding is the staging region (BL-778). The
                // scorer refused this target unless some hub could reach it,
                // and execute picks its own hub — so it filters by the same
                // rule, or it could stage a campaign the scorer would never
                // have offered. `tgt_shore` and `tgt_naval` are hoisted because
                // neither depends on which hub is chosen.
                const bool tgt_shore = owns_shore_at(ti);
                int src = -1, src_d = 1 << 30;
                for (int hi : held)
                {
                    const std::size_t hs = static_cast<std::size_t>(hi);
                    if (!dry_contact(hi, ti) && !tgt_shore
                     && !can_field_naval(ss.regions[hs], mil_band))
                        continue;
                    const int d = region_distance(ss.regions[hs], tgt, gw);
                    if (d < src_d) { src_d = d; src = hi; }
                }
                if (src < 0) break; // no legal staging holding — nothing marches
                region& home = ss.regions[static_cast<std::size_t>(src)];

                const int64_t want   = (home.manpower_stock * params.levy_fraction_q) / 1000;
                const int64_t raised = raise_manpower(home, want);
                if (raised <= 0) break;

                // FORCE COMMITMENT (BL-277 Q2), priced by the SAME lambda the
                // scorer used. `src_d` is the staging hub's distance — the army
                // victuals at the frontier region it marched from, and the
                // capital still bears on the result through the terrain-weighted
                // reach term inside `campaign_supply`.
                //
                // Execute picks the NEAREST holding while the scorer scored one
                // (hub, target) pair, so the fought supply is never worse than
                // the scored estimate. Optimistic by a bounded amount, and in
                // the right direction: the sim does not launch campaigns it then
                // silently under-supplies.
                //
                // FORAGE, priced by the same rule the scorer used (§ Forage).
                // The file's own thesis applies: a cost authored on one scale
                // and spent on another is the bug, so the estimate and the
                // outcome ask the identical question.
                const bool exec_dry     = dry_contact(src, ti);
                const bool exec_forages = exec_dry || tgt_shore;
                const int atk_supply = exec_forages ? campaign_supply(src_d, ti, src) : 0;
                const int def_supply = 1000;
                if (!exec_forages) ++out.starved_campaigns;

                // BL-768 — THE SUPPLY CORRIDOR, recorded where it is priced.
                // `src` is the staging holding the army victualled from and
                // `ti` the objective it marched on, so this pair is literally
                // the line supply moved along, taken from the two indices
                // `campaign_supply` was just handed rather than reconstructed.
                // Recorded on LAUNCH, not on victory: the road was walked
                // whether or not the battle was won, and a network that only
                // remembered the winners would be a map of conquests rather
                // than a map of routes.
                note_corridor(src, static_cast<int>(ti));

                // The stall, counted where it actually happens (BL-312). The
                // first cut incremented this AFTER resolve_battle and only at
                // exactly zero supply, so it counted launched battles rather
                // than stalls and measured a constant zero in every run.
                if (atk_supply < params.stalled_supply_q) ++out.stalled_campaigns;

                // THE DEFENCE WORKS LAND HERE (BL-321), as readiness on the
                // defender's stack — which `roster_stack` turns into an
                // additive per-mille offset on each unit's `type_power_mod`.
                // That is deliberately the same channel cohesion uses, and for
                // the same reason combat.hpp gives: the engine scores whatever
                // stack it is handed and knows nothing about walls. A Bastion
                // Fort at +640 is worth about +64 against row power values of
                // 90..380 — it tilts a fight rather than deciding one, so a
                // fortress buys the defender an edge and never immunity.
                const int def_works_q = clampi(tgt.work_defence_mod, 0, 1000);
                const int def_ready = (best_winter ? (1000 - params.winter_readiness_penalty_q) : 1000)
                                    + def_works_q;

                const polity* dq = nullptr;
                for (const polity& o : out.polities)
                    if (o.id == owner[ti]) { dq = &o; break; }

                roster_band atk_band = roster_band::classical;
                std::vector<army_stack_entry> atk =
                    build_stack(raised, home, q, 1000, sim_band_ceiling(params, y),
                                !exec_dry, &atk_band);
                note_units_fielded(out, params, y, atk_band, atk);
                const int64_t def_want = (tgt.manpower_stock * params.levy_fraction_q) / 1000;
                const int64_t def_men  = raise_manpower(tgt, def_want);
                roster_band def_band = roster_band::classical;
                std::vector<army_stack_entry> def =
                    build_stack(def_men, tgt, dq ? *dq : q, def_ready, sim_band_ceiling(params, y),
                                !exec_dry, &def_band);
                note_units_fielded(out, params, y, def_band, def);

                const prof_clock::time_point prof_bat0 = prof_clock::now(); // BL-825
                const battle_outcome bo = resolve_battle(
                    atk, doctrine_for(q),
                    def, dq ? doctrine_for(*dq) : doctrine_for(q),
                    sub_at(terrain, tgt.anchor), cov_at(terrain, tgt.anchor),
                    den_at(terrain, tgt.anchor), lf_at(terrain, tgt.anchor),
                    best_winter ? season::winter : season::summer,
                    atk_supply, def_supply);
                prof.ns_battles += std::chrono::duration_cast<std::chrono::nanoseconds>(
                    prof_clock::now() - prof_bat0).count(); // BL-825, report-only

                ++out.battles;
                // BL-779's calibration reading: how often naval combat actually
                // occurs. Counted, never asserted upward — rare is the design
                // (MILITARY_HISTORY.md § Naval).
                if (stack_has_naval(atk) || stack_has_naval(def)) ++out.naval_battles;
                if (!exec_dry) ++out.sea_leg_battles;
                if (century < out.battles_per_century.size())
                    ++out.battles_per_century[century];
                if (best_winter) ++out.winter_campaigns;

                // Losses are per-mille of each side's OWN committed count —
                // resolve_battle never mutates a stack, so spending them here
                // is this caller's job (combat.hpp § battle_outcome).
                const int64_t atk_lost = (stack_size(atk) * bo.attacker_losses_permille) / 1000;
                const int64_t def_lost = (stack_size(def) * bo.defender_losses_permille) / 1000;
                home.population = clampi64(home.population - atk_lost / 4, 0, 1LL << 40);
                tgt.population  = clampi64(tgt.population  - def_lost / 4, 0, 1LL << 40);

                war_pressure[static_cast<std::size_t>(src)] = clampi(bo.decisiveness / 2, 0, 1000);
                war_pressure[ti] = clampi(bo.decisiveness, 0, 1000);
                tgt.contest_q = clampi(tgt.contest_q + bo.decisiveness / 4, 0, 1000);

                // TERRITORY MOVES AT PROVINCE GRANULARITY, NEVER TILE.
                //
                // A ground-down frontier eventually gives (BL-308): the bar a
                // victory must clear falls as the region's accumulated
                // contest rises. A flat threshold meant centuries of fighting
                // over the same ground moved nothing — battles outnumbered
                // conquests by up to 1000:1 in the first sweep.
                const int relief = (tgt.contest_q * params.contest_transfer_relief_q) / 1000;
                const int needed = clampi(params.transfer_decisiveness_q - relief, 40, 1000);

                const bool won_it = (bo.result == battle_result::attacker_victory);
                const bool takes_it = won_it && bo.decisiveness >= needed;

                // BL-384's observation point, and it is HERE rather than beside
                // resolve_battle for one reason: the conquest bar is computed
                // here, and "did it win" and "did that win move a border" are
                // different questions with different answers. 267 battles and
                // zero conquests could be either, and a trace taken before this
                // line could not tell them apart.
                if (params.trace_battles)
                {
                    battle_trace bt;
                    bt.year     = static_cast<int32_t>(y);
                    bt.attacker = static_cast<uint16_t>(q.id);
                    bt.defender = static_cast<uint16_t>(dq ? dq->id : 0);
                    bt.region   = static_cast<uint16_t>(ti);
                    bt.p_win_q    = best_p_win_q;
                    bt.scored_hub = best_hub;
                    bt.winter     = best_winter;
                    bt.exec_hub          = src;
                    bt.attacker_men      = stack_size(atk);
                    bt.defender_men      = stack_size(def);
                    bt.attacker_supply_q = atk_supply;
                    bt.defender_ready_q  = def_ready;
                    bt.works_defence_q   = def_works_q;
                    // The term the scorer never sees — recomputed from the same
                    // inputs the resolver was handed, not from a second source.
                    bt.terrain_defence_q = terrain_defence(
                        sub_at(terrain, tgt.anchor), cov_at(terrain, tgt.anchor),
                        den_at(terrain, tgt.anchor), lf_at(terrain, tgt.anchor));
                    bt.attacker_won    = won_it;
                    bt.decisiveness    = bo.decisiveness;
                    bt.transfer_needed = needed;
                    bt.conquered       = takes_it;
                    out.battle_traces.push_back(bt);
                }

                if (takes_it)
                {
                    // The loser's cohesion falls: this is where defeat starts
                    // to compound rather than merely accumulate.
                    if (dq)
                    {
                        polity& loser = out.polities[static_cast<std::size_t>(dq->id)];
                        loser.cohesion_q = clampi(loser.cohesion_q - params.cohesion_loss_on_defeat_q,
                                                  params.cohesion_floor_q, 1000);
                    }

                    // The sack — the collapse path the first sweep had none of.
                    tgt.population = clampi64(
                        tgt.population - (tgt.population * params.sack_population_loss_q) / 1000,
                        0, 1LL << 40);
                    // BL-766: and it falls hardest on the walls. This is the
                    // one place history DESTROYS a centre rather than thinning
                    // it, so a sacked city reads as a smaller or absent centre
                    // on the epoch map.
                    sack_region_urban(tgt, params.sack_population_loss_q);

                    const int loser_id = dq ? dq->id : -1;
                    const bool was_seat =
                        dq && dq->capital == static_cast<int>(ti);

                    owner[ti]  = q.id;
                    tgt.nation = q.id;
                    // BL-826 — THE CONQUEROR'S GODS NO LONGER ARRIVE THE SAME
                    // AFTERNOON. This was `tgt.culture = q.culture`, an instant
                    // replacement, and it is the single line that made conquest
                    // free: the ground stopped being foreign the moment it was
                    // taken, so `w_cult` charged a conqueror once and never
                    // again. The shares are left exactly as the battle found
                    // them and the ASSIMILATION block at the top of the round
                    // moves them, a couple of per-mille of the foreign
                    // remainder a year, for as long as the conqueror holds on.
                    //
                    // `creed_conquered` still fires here, because the fact
                    // being recorded is that a conqueror's pantheon was imposed
                    // — an event, at a date — and `founding_culture` is still
                    // never overwritten. What changed is how long the people
                    // take to follow the flag.
                    tgt.creed_conquered = true;
                    ++out.conquests;

                    // BL-827 — THE NAMED EVENT, with its place and its date.
                    // Directed from the LOSER: the realm that lost the province
                    // resents the one that took it, and not the reverse.
                    // Magnitude carries the seat distinction, which is the
                    // difference between losing a frontier march and losing the
                    // capital.
                    if (loser_id >= 0)
                        raise_grudge(loser_id, q.id,
                                     was_seat ? grudge_kind::seat_sacked
                                              : grudge_kind::ground_taken,
                                     static_cast<int>(ti), y,
                                     was_seat ? params.grudge_seat_sacked
                                              : params.grudge_ground_taken);
                    out.owner_changes.push_back(owner_change{
                        static_cast<int32_t>(y), static_cast<uint16_t>(ti),
                        static_cast<uint16_t>(q.id)});
                    out.history.push_back(history_event{
                        years_from_calendar_year(y), chain_stage::legacy,
                        tgt.name + " changes hands", std::string{}});

                    // BL-827 — DID THAT END A REALM? Detected here, at the
                    // conquest, rather than at the top of the next round where
                    // `alive` is refreshed: the killer is only knowable at the
                    // moment of the killing, and reconstructing it afterwards
                    // from ids is exactly the accident the successor call
                    // rejects.
                    if (loser_id >= 0)
                    {
                        bool any_left = false;
                        for (int o : owner) if (o == loser_id) { any_left = true; break; }
                        if (!any_left)
                        {
                            out.polities[static_cast<std::size_t>(loser_id)].alive = false;
                            extinguish_polity(loser_id, q.id, static_cast<int>(ti), y);
                        }
                    }
                }
                else if (dq)
                {
                    // BL-827 — A BATTLE THAT MOVED NOTHING is still a raid on
                    // somebody's ground, and it is the event that makes a
                    // centuries-long frontier read as a feud rather than as
                    // silence. Small, because it is: the grinding is in the
                    // repetition, and `contest_q` above is what the frontier
                    // itself accumulates.
                    raise_grudge(dq->id, q.id, grudge_kind::border_raided,
                                 static_cast<int>(ti), y, params.grudge_border_raided);
                }
                break;
            }
            case sim_verb::settle:
            {
                const region& src = ss.regions[static_cast<std::size_t>(best_target)];

                // BL-310: find an UNOCCUPIED cell, widening the ring as the
                // neighbourhood fills. The first cut offered nine candidate
                // cells with no occupancy test and re-picked the same mature
                // parent every year, so regions piled up — 132 regions on
                // 33 distinct cells in one measured run, worst stack nine deep.
                // Co-located regions then had region_distance 0 and the
                // Ages map drew a whole stack as one dot.
                int nc = -1, nr = -1;
                for (int ring = 1; ring <= 6 && nc < 0; ++ring)
                {
                    const int span = 2 * ring + 1;
                    for (int probe = 0; probe < span * span; ++probe)
                    {
                        const uint32_t h = salt(qs, static_cast<uint32_t>(y) * 131u
                                                    + static_cast<uint32_t>(probe));
                        const int dc = static_cast<int>(h % static_cast<uint32_t>(span)) - ring;
                        const int dr = static_cast<int>((h >> 8) % static_cast<uint32_t>(span)) - ring;
                        if (dc == 0 && dr == 0) continue;

                        int cc = src.col + dc;
                        if (gw > 0) cc = ((cc % gw) + gw) % gw;
                        const int rr = clampi(src.row + dr, 0, gh > 0 ? gh - 1 : 0);

                        // BL-777 — NOBODY FOUNDS ON OPEN OCEAN.
                        //
                        // This probe applied no terrain test at all, so 447 of
                        // 1754 regions across three seeds were anchored on
                        // water and 183 of those on open ocean (measured by
                        // sim_water_census, 2026-09-06). `terrain_combat`
                        // returns 0 defence and 0 forage for every water kind,
                        // so those regions were silently undefendable.
                        //
                        // IT IS NOT A "NO WATER" TEST, and the distinction is
                        // the design. Under the ownership ruling (BL-776,
                        // PROVINCES.md § Who owns water) coastal water belongs
                        // to whoever owns the shore, so founding on the
                        // shoreline ring or a lake is LEGITIMATE — there is an
                        // owner to found under. Open ocean has no owner at all,
                        // structurally, so it is the only domain refused. That
                        // is ~183 sites rather than the ~447 a blanket ban
                        // would have deleted.
                        //
                        // A caller with no terrain is unaffected: `sub_at`
                        // hands out the neutral dry default, so every synthetic
                        // harness case probes exactly the cells it always did.
                        const int cand = (gw > 0) ? rr * gw + cc : -1;
                        if (region_domain_of(sub_at(terrain, cand)) == region_domain::open_ocean)
                            continue;

                        bool taken = false;
                        for (const region& e : ss.regions)
                            if (e.col == cc && e.row == rr) { taken = true; break; }
                        if (!taken) { nc = cc; nr = rr; break; }
                    }
                }
                if (nc < 0) break; // Neighbourhood full — no room to expand here.

                region np;
                np.col = nc;
                np.row = nr;
                np.anchor = (gw > 0) ? np.row * gw + np.col : -1;
                // BL-777: the domain of the ground actually chosen. The probe
                // above has already refused open ocean, so this records `land`
                // or `coastal_water` — but it is DERIVED rather than assumed,
                // so the field stays a fact about the tile and the census can
                // check the two against each other.
                np.domain = region_domain_of(sub_at(terrain, np.anchor));

                // Daughter ground is a decayed inheritance of the parent's —
                // good land begets good land, but never better than its parent.
                np.farm_q = (src.farm_q * 850) / 1000;
                np.ore_q  = (src.ore_q  * 700) / 1000;
                np.port_q = (src.port_q * 700) / 1000;
                np.energy_q = (src.energy_q * 700) / 1000;
                np.settle_score_q = (src.settle_score_q * 800) / 1000;
                // BL-826: a daughter is founded WHOLLY by its founders. A
                // settling party carries one people, so the shares start pure
                // and only conquest can mix them.
                np.culture = culture_shares::pure(q.culture);
                np.founding_culture = q.culture;
                np.name = src.name + " Reach";
                np.founded_year = y;
                np.last_demography_year = y;
                np.nation = q.id;
                np.population = clampi64(region_carrying_capacity(np.farm_q) / 16, 1, 1 << 30);
                replenish_manpower(np);
                // BL-766: a region founded HERE gets its settlement on the same
                // terms as one drawn before the sim ran — one rule, not two.
                draw_region_urban(np);

                // THE DAUGHTER'S OWN GROUND DECIDES ITS FURNACE (BL-748).
                //
                // Without this every region the run founds would carry the
                // struct default of -1 — "this ground never could" — and on a
                // 1960 arc that is two regions in three, so Stage 4 could only
                // ever reach the settlement pass's original ground. That is
                // not a bounded simplification, it is the endowment gate
                // silently answering "no" for a majority of the map.
                //
                // The gate is recomputed from the daughter's OWN fuel, on the
                // same expression settlement.cpp § Stage 4 uses — inheriting
                // the flag would let poor ground industrialise because its
                // parent could, which is the opposite of endowment-not-virtue.
                // The date's ground terms are recomputed the same way.
                //
                // THE FOUNDING TERM IS CHARGED, NOT INHERITED, and getting that
                // wrong was a real defect. Stage 4's formula carries
                // `founded_year / 8`, and the first cut folded it into the
                // parent's residual on the grounds that "the rest of that
                // formula is not reachable inside this loop". `founded_year` IS
                // reachable — `np.founded_year = y` is set a few lines above.
                // So a daughter settled in 1900 by a parent founded in year 0
                // was charged 0 extra years where the endowment rule charges
                // ~237, and the late frontier lit its furnaces centuries early.
                // That term exists precisely to punish a late frontier, so it is
                // taken off the parent's residual and recomputed from the
                // daughter's own founding year. Everything genuinely out of
                // reach — the world's arable share, the creed bonuses, the
                // parent's own draw — stays inherited.
                {
                    const auto ground_lag = [](const region& r) {
                        return 90 - r.energy_q / 12 - r.ore_q / 22;
                    };
                    const auto founding_lag = [](const region& r) {
                        return static_cast<int>(r.founded_year / 8);
                    };
                    const int daughter_fuel = np.energy_q + np.ore_q / 2;
                    if (daughter_fuel >= 900 && src.industrial_lag_years >= 0)
                    {
                        const int residual = src.industrial_lag_years
                                           - ground_lag(src) - founding_lag(src);
                        np.industrial_lag_years =
                            clampi(ground_lag(np) + founding_lag(np) + residual, 0, 235);
                    }
                }

                // The change list indexes regions as uint16_t, so refuse to
                // create one the time-lapse could not address (BL-312). Past
                // 65,535 the cast wrapped silently and owner_slice_at's bounds
                // check could not catch it — the wrapped index is small and in
                // range, so replay produced a plausible but WRONG map.
                if (ss.regions.size() >= owner_index_limit) break;

                ss.regions.push_back(np);
                owner.push_back(q.id);
                war_pressure.push_back(0);
                neighbours.emplace_back();
                link_region(ss.regions.size() - 1); // Keep the index complete.

                // BL-768 — the road a founding party walked. The daughter is
                // reached FROM its parent and supplied from there until it can
                // feed itself, so (parent, daughter) is the second and by far
                // the commoner of the two corridor sources: a polity settles far
                // more often than it campaigns, which is what gives a peaceful
                // history a road network at all.
                note_corridor(best_target, static_cast<int>(ss.regions.size()) - 1);
                out.owner_changes.push_back(owner_change{
                    static_cast<int32_t>(y),
                    static_cast<uint16_t>(ss.regions.size() - 1),
                    static_cast<uint16_t>(q.id)});
                ++out.foundings;
                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    np.name + " is settled", std::string{}});
                break;
            }
            case sim_verb::invest:
            {
                const int d = clampi(best_target, 0, sim_domain_count - 1);
                // A RATE: investment accrues per year, so a coarse band must
                // credit the whole interval. Without this a 100-year band would
                // advance tech exactly as far as a 1-year one and the ladder
                // would never leave band 1 (history_sim.hpp § stepped clock).
                //
                // THE INDUSTRIAL WORKS LAND HERE (BL-321), AND THE DIVERGENCE
                // THAT USED TO BE RECORDED HERE IS CLOSED (BL-748).
                //
                // The old note said: the design calls `industrial_mod` a
                // "pull-forward on the Stage 4 furnace date", but Stage 4 ran
                // inside `run_settlement` and had already finished before this
                // loop started, so there was no furnace date left to pull — and
                // the boost was applied to whatever domain Invest happened to
                // pick, which is the tech ladder generally rather than the
                // industrial clock specifically.
                //
                // The furnace date now lives INSIDE the run: a polity lights
                // when its MATERIALS capacity crosses the Industrial rung (see
                // the furnace block at the end of this round). So the
                // pull-forward is expressed against that crossing, which means
                // it applies to the materials ladder and to nothing else. A
                // Blast Works shortens the road to a furnace; it does not make
                // a polity better at medicine.
                //
                // INERT TODAY, and honestly so: `mean_industrial_q` is the mean
                // of `region::work_industrial_mod` over the polity's holdings,
                // and BL-757 measured ZERO works raised across sixteen seeds
                // because `build_work` never wins the scored contest. The
                // mechanism is wired and the boost is zero until that is fixed
                // — which is a finding about BL-757, not a reason to route this
                // through a domain it does not belong to.
                const bool  ind_domain = (d == static_cast<int>(sim_domain::materials));
                const int   ind_boost  = ind_domain ? clampi(mean_industrial_q, 0, 1000) : 0;
                const int   progress   = clampi(best_score, 0, 1000) * step_years;
                q.progress_q[d] += progress + (progress * ind_boost) / 1000;
                // A band costs more the higher it sits — capacity follows the
                // map, and it never runs away (ANCIENT_TECH_LADDER § diffusion).
                // The rate is a parameter since BL-767; the value is unchanged.
                const int cost = params.capacity_band_cost * q.capacity[d];
                if (q.progress_q[d] >= cost && q.capacity[d] < 6)
                {
                    q.progress_q[d] -= cost;
                    ++q.capacity[d];
                }
                break;
            }
            case sim_verb::build_work:
            {
                if (works == nullptr || best_target < 0
                 || best_target >= static_cast<int>(ss.regions.size())) break;

                region& bp = ss.regions[static_cast<std::size_t>(best_target)];
                const work_row* r = works->row_at(static_cast<std::size_t>(best_work_row));
                if (r == nullptr) break;
                if (!apply_work_to_region(bp, *works, best_work_row)) break;

                // The manpower ceiling just moved, so the stock's headroom did
                // too. Without this the Arsenal a polity built this round would
                // not be worth anything until the next demography step happened
                // to top the stock up — a work with a visible delay nobody
                // asked for.
                replenish_manpower(bp);

                ++out.works_raised;
                {
                    const int b = static_cast<int>(r->band);
                    if (b >= 0 && b < roster_band_count)
                        ++out.works_by_span_band[static_cast<std::size_t>(span_index(params, y))]
                                                [static_cast<std::size_t>(b)];
                }
                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    bp.name + " raises a " + r->name, std::string{}});
                break;
            }
            case sim_verb::consolidate:
            default:
                for (int hi : held)
                {
                    region& p = ss.regions[static_cast<std::size_t>(hi)];
                    // A RATE: a frontier cools by the year, not by the round.
                    p.contest_q = clampi(p.contest_q - 8 * step_years, 0, 1000);
                }
                // Cohesion recovers only here, and slower than it is lost — so
                // the spiral is escapable, but only by a polity that stops
                // fighting for several years running (BL-308).
                //
                // A RATE, for the same reason: `cohesion_recovery_q` is
                // documented per year of Consolidate, so a coarse band credits
                // the interval it actually covers. The clamp still bounds it,
                // so a 100-year band recovers fully rather than overshooting.
                q.cohesion_q = clampi(q.cohesion_q + params.cohesion_recovery_q * step_years,
                                      params.cohesion_floor_q, 1000);
                break;
            }

            // ---- THE FURNACE (BL-748) ------------------------------------
            //
            // Stage 4's date used to be `run_settlement`'s, fixed before this
            // loop started. Under an industrial epoch that is backwards — the
            // SECOND SPAN is where industrialisation happens — so the date is
            // now the year a polity's materials capacity crosses the Industrial
            // rung INSIDE the run, plus the lag its ground imposes.
            //
            // TWO HALVES, EACH OWNED WHERE IT BELONGS. The endowment gate and
            // the per-region lag are `run_settlement`'s (settlement.cpp § Stage
            // 4) — that arithmetic is unchanged, coefficient for coefficient,
            // only re-anchored. WHEN is this loop's, and it is reached by
            // playing: a polity that spends its rounds fighting never climbs
            // the ladder and never lights, which is a legitimate outcome and
            // not a gap for anything downstream to fill in.
            //
            // Read through `mat_band`, so the rung is the same derivation the
            // works table uses and a polity cannot light a furnace at a band it
            // could not build at. On a two-span run that means no furnace
            // before the boundary year, because `sim_band_ceiling` caps span 0
            // at medieval. On a single-span ancient arc the ceiling is inert —
            // and no region there carries a lag at all (Stage 4 does not run
            // below 1700), so the 0 CE world lights nothing, exactly as before.
            //
            // Placed AFTER the verb executes so a crossing bought by this
            // round's Invest is visible this round rather than next.
            {
                if (q.industrial_year == k_never_industrialised
                    && mat_band == roster_band::industrial)
                {
                    q.industrial_year = y;
                    ++out.polities_industrialised;
                }

                if (q.industrial_year != k_never_industrialised)
                {
                    for (int hi : held)
                    {
                        region& p = ss.regions[static_cast<std::size_t>(hi)];
                        // A negative lag is the gate saying "this ground never
                        // could" — below-average fuel, or an arc whose Stage 4
                        // never ran at all.
                        if (p.industrialised || p.industrial_lag_years < 0) continue;
                        if (y < q.industrial_year + p.industrial_lag_years) continue;
                        p.industrialised  = true;
                        p.industrial_year = y;
                        ++out.regions_industrialised;
                    }
                }
            }
        }

        // Ownership changes are appended where they happen (conquest, founding),
        // so there is nothing to snapshot at the end of a year.
    }

    out.region_stride = static_cast<int>(ss.regions.size());
    out.years           = years;
    out.start_year      = params.start_year;

    // --- The ancient road record, folded (BL-768) -------------------------
    //
    // Sort by (a, b) — a total order over two plain integers — then run-length
    // encode. The corridor a region walked forty times and the one it walked
    // once are the same edge with different traffic, and traffic is what the
    // ancient tier rule reads, so the count has to survive the fold.
    {
        std::sort(corridor_uses.begin(), corridor_uses.end());
        out.supply_corridors.reserve(corridor_uses.size());
        for (const auto& e : corridor_uses)
        {
            if (!out.supply_corridors.empty()
                && out.supply_corridors.back().a == e.first
                && out.supply_corridors.back().b == e.second)
            {
                ++out.supply_corridors.back().uses;
                continue;
            }
            out.supply_corridors.push_back(history_corridor{e.first, e.second, 1});
        }
    }

    // --- The world median furnace year (BL-748) ---------------------------
    //
    // `run_settlement` used to own this, and could not any more: nothing has
    // industrialised while that pass is running. THIS is the first moment the
    // answer exists, so the sim writes it back into the same field on the same
    // state it was handed — one owner, one field, no second derivation for
    // BL-219's early/late corporate pivot to disagree with.
    //
    // Left at whatever it already held when nobody industrialised, which for
    // every path in the project is 0 — nobody — and 0 is what the
    // never-industrialised rung in corporation_generation.cpp reads.
    {
        std::vector<int64_t> years_lit;
        for (const region& p : ss.regions)
            if (p.industrialised) years_lit.push_back(p.industrial_year);
        if (!years_lit.empty())
        {
            std::sort(years_lit.begin(), years_lit.end());
            ss.median_industrial_year = years_lit[years_lit.size() / 2];
        }
    }

    for (polity& q : out.polities)
    {
        bool any = false;
        for (int o : owner) if (o == q.id) { any = true; break; }
        q.alive = any;
    }

    // --- THE TARIFF POSTURE (BL-750) --------------------------------------
    //
    // A DERIVED OUTPUT READ AT HANDOFF, NEVER A SCORED VERB (Ben, 2026-09-06;
    // NATIONS.md sec 4 Tariffs). Nothing in the decision loop above reads
    // `protection_q`, and nothing here can move the run: this block executes
    // once, after the last round, and writes a field no verb, no score and no
    // RNG draw ever touches. A run with this block deleted would take every
    // decision it takes with it.
    //
    // The formula and the reason for its shape are on `polity::protection_q`.
    // Integer throughout, walked in polity-id order over a vector, so it is
    // byte-identical from a seed like everything else in this file.
    {
        std::vector<int> alive_ids;
        for (const polity& q : out.polities)
            if (q.alive) alive_ids.push_back(q.id);

        // The world's FIRST furnace, among the polities that survived to be
        // handed over. A polity that lit and was then eliminated is not part of
        // the field the campaign inherits.
        int64_t lead = k_never_industrialised;
        for (int qi : alive_ids)
        {
            const int64_t yr = out.polities[static_cast<std::size_t>(qi)].industrial_year;
            if (yr == k_never_industrialised) continue;
            if (lead == k_never_industrialised || yr < lead) lead = yr;
        }

        if (lead != k_never_industrialised && alive_ids.size() > 1)
        {
            const int64_t span = std::max<int64_t>(1, params.stop_year - lead);
            for (int qi : alive_ids)
            {
                polity& q = out.polities[static_cast<std::size_t>(qi)];

                // Strictly before: a tie is not "ahead", so two polities that
                // lit the same year neither protect against each other.
                int ahead = 0;
                for (int pi : alive_ids)
                {
                    if (pi == qi) continue;
                    const int64_t py = out.polities[static_cast<std::size_t>(pi)].industrial_year;
                    if (py == k_never_industrialised) continue;
                    if (q.industrial_year == k_never_industrialised
                        || py < q.industrial_year)
                        ++ahead;
                }
                const int share_ahead_q =
                    (ahead * 1000) / (static_cast<int>(alive_ids.size()) - 1);

                // A polity that never lit is behind by the WHOLE remaining
                // span, which is the continuous reading of the sentinel rather
                // than a second branch downstream.
                const int64_t my_year = q.industrial_year == k_never_industrialised
                                      ? params.stop_year : q.industrial_year;
                const int lag_q = clampi(
                    static_cast<int>(((my_year - lead) * 1000) / span), 0, 1000);

                q.protection_q = clampi((share_ahead_q * lag_q) / 1000, 0, 1000);
            }
        }

        // Broadcast onto the ground, the way `contest_q` is broadcast in
        // `derive_national_character`: the settlement state is the handoff
        // object, and the political pass reads regions, not polities.
        for (std::size_t i = 0; i < ss.regions.size() && i < owner.size(); ++i)
        {
            const int o = owner[i];
            ss.regions[i].protection_q =
                (o >= 0 && o < static_cast<int>(out.polities.size()))
                    ? out.polities[static_cast<std::size_t>(o)].protection_q : 0;
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Grudge reads (BL-827)
// ---------------------------------------------------------------------------

int grudge_between(const history_sim_state& s, int from, int to)
{
    if (from < 0 || to < 0 || from > 0xFFFE || to > 0xFFFE) return 0;
    const uint16_t f = static_cast<uint16_t>(from), t = static_cast<uint16_t>(to);
    const auto it = std::lower_bound(
        s.grudges.begin(), s.grudges.end(), std::pair<uint16_t, uint16_t>{f, t},
        [](const grudge& g, const std::pair<uint16_t, uint16_t>& k) {
            if (g.from != k.first) return g.from < k.first;
            return g.to < k.second;
        });
    if (it != s.grudges.end() && it->from == f && it->to == t) return it->score;
    return 0;
}

std::vector<grudge> top_grudges(const history_sim_state& s, int n)
{
    std::vector<grudge> v = s.grudges;
    // A TOTAL order with an explicit tie-break, so the listing is identical on
    // every machine — the discipline GENERATION_STRATEGY.md § What keeps it
    // deterministic requires of every argmax in the generation layer.
    std::sort(v.begin(), v.end(), [](const grudge& a, const grudge& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.peak  != b.peak)  return a.peak  > b.peak;
        if (a.from  != b.from)  return a.from  < b.from;
        return a.to < b.to;
    });
    if (n >= 0 && static_cast<std::size_t>(n) < v.size())
        v.resize(static_cast<std::size_t>(n));
    return v;
}

std::string grudge_event_line(const grudge_event& e, const settlement_state& ss)
{
    const char* what = "a wrong";
    switch (e.kind)
    {
    case grudge_kind::ground_taken:  what = "ground taken";      break;
    case grudge_kind::seat_sacked:   what = "the seat sacked";   break;
    case grudge_kind::border_raided: what = "the border raided"; break;
    case grudge_kind::realm_ended:   what = "a realm ended";     break;
    }

    std::string where = "somewhere";
    if (e.region != owner_none
        && static_cast<std::size_t>(e.region) < ss.regions.size()
        && !ss.regions[static_cast<std::size_t>(e.region)].name.empty())
        where = ss.regions[static_cast<std::size_t>(e.region)].name;

    // Year first, because the date is what makes it a cause rather than a
    // modifier. Signed calendar year, the same convention the sim runs on.
    return std::to_string(static_cast<long long>(e.year)) + ": " + what
         + " at " + where + " (+" + std::to_string(static_cast<long long>(e.magnitude)) + ")";
}

// ---------------------------------------------------------------------------
// The pass 1 -> pass 2 handoff (BL-828)
// ---------------------------------------------------------------------------

pass_one_output make_pass_one_output(const settlement_state&  ss,
                                     const history_sim_state& hs,
                                     int                      culture_count)
{
    pass_one_output o;
    o.regions             = ss.regions;
    o.polities            = hs.polities;
    o.culture_count       = culture_count;
    o.works_by_span_band  = hs.works_by_span_band;
    o.grudges             = hs.grudges;
    o.timelapse           = as_timelapse(hs);
    o.start_year          = hs.start_year;
    o.stop_year           = hs.start_year + hs.years;

    // THE PROVINCES EACH POLITY HOLDS, derived from the region table's own
    // `nation` field — which the sim writes as it goes — rather than from a
    // second copy of ownership kept beside it. One source, so the holdings and
    // the map cannot disagree.
    //
    // Walked in region-index order into a vector indexed by polity id, so both
    // the outer order (ascending polity id) and the inner order (ascending
    // region index) are properties of the data. A map keyed on polity id would
    // have been the shorter spelling and the wrong one.
    std::vector<std::vector<int>> by_polity(o.polities.size());
    for (std::size_t i = 0; i < o.regions.size(); ++i)
    {
        const int n = o.regions[i].nation;
        if (n >= 0 && n < static_cast<int>(by_polity.size()))
            by_polity[static_cast<std::size_t>(n)].push_back(static_cast<int>(i));
    }
    for (std::size_t pi = 0; pi < o.polities.size(); ++pi)
    {
        if (by_polity[pi].empty()) continue; // A realm holding nothing crosses as nothing.
        polity_holdings h;
        h.polity  = static_cast<int>(pi);
        h.regions = by_polity[pi];
        o.holdings.push_back(std::move(h));
    }
    return o;
}

bool pass_one_output_valid(const pass_one_output& o, std::string* why)
{
    const auto fail = [&](const std::string& msg) {
        if (why) *why = msg;
        return false;
    };

    // 1. The culture shares. THE INVARIANT THE WHOLE OF BL-826 RESTS ON: the
    //    per-mille weights sum to exactly 1000, and every named slot is in
    //    range, sorted descending, with no culture named twice.
    for (std::size_t i = 0; i < o.regions.size(); ++i)
    {
        const culture_shares& c = o.regions[i].culture;
        if (c.total_q() != 1000)
            return fail("region " + std::to_string(i) + " culture shares sum to "
                        + std::to_string(c.total_q()) + ", not 1000");
        if (c.other_q < 0)
            return fail("region " + std::to_string(i) + " has a negative culture tail");
        for (int k = 0; k < culture_share_slots; ++k)
        {
            if (c.id[k] < 0)
            {
                if (c.weight_q[k] != 0)
                    return fail("region " + std::to_string(i) + " slot "
                                + std::to_string(k) + " is empty but weighted");
                continue;
            }
            if (c.weight_q[k] <= 0)
                return fail("region " + std::to_string(i) + " slot "
                            + std::to_string(k) + " is named but unweighted");
            if (o.culture_count > 0 && c.id[k] >= o.culture_count)
                return fail("region " + std::to_string(i) + " names culture "
                            + std::to_string(c.id[k]) + " out of range");
            if (k > 0 && c.id[k - 1] >= 0 && c.weight_q[k - 1] < c.weight_q[k])
                return fail("region " + std::to_string(i) + " culture shares are not sorted");
            for (int j = 0; j < k; ++j)
                if (c.id[j] == c.id[k])
                    return fail("region " + std::to_string(i) + " names culture "
                                + std::to_string(c.id[k]) + " twice");
        }
    }

    // 2. The holdings. Ascending polity ids, ascending region indices, every
    //    region held at most once, and every holder a living polity.
    std::vector<char> claimed(o.regions.size(), 0);
    int last_polity = -1;
    for (const polity_holdings& h : o.holdings)
    {
        if (h.polity <= last_polity)
            return fail("holdings are not in ascending polity order");
        last_polity = h.polity;
        if (h.polity < 0 || h.polity >= static_cast<int>(o.polities.size()))
            return fail("holdings name polity " + std::to_string(h.polity) + " out of range");
        if (!o.polities[static_cast<std::size_t>(h.polity)].alive)
            return fail("polity " + std::to_string(h.polity) + " holds ground but is not alive");
        int last_region = -1;
        for (int r : h.regions)
        {
            if (r <= last_region) return fail("holdings are not in ascending region order");
            last_region = r;
            if (r < 0 || r >= static_cast<int>(o.regions.size()))
                return fail("holdings name region " + std::to_string(r) + " out of range");
            if (claimed[static_cast<std::size_t>(r)])
                return fail("region " + std::to_string(r) + " is held twice");
            claimed[static_cast<std::size_t>(r)] = 1;
            if (o.regions[static_cast<std::size_t>(r)].nation != h.polity)
                return fail("region " + std::to_string(r) + " holdings disagree with its nation");
        }
    }

    // 3. The grudges. Sorted, directed, in range, and CARRYING THEIR CAUSE — an
    //    entry with a score and no event is the modifier this design refuses.
    std::pair<int, int> last_key{-1, -1};
    for (const grudge& g : o.grudges)
    {
        const std::pair<int, int> key{g.from, g.to};
        if (!(last_key < key)) return fail("grudges are not sorted by (from, to)");
        last_key = key;
        if (g.from == g.to) return fail("a polity holds a grudge against itself");
        if (g.from >= o.polities.size() || g.to >= o.polities.size())
            return fail("a grudge names a polity out of range");
        if (g.score < 0 || g.peak < g.score)
            return fail("a grudge's peak is below its standing score");
        if (g.event_count <= 0 || g.events_kept <= 0)
            return fail("a grudge carries a score with no cause");
        if (g.events_kept > grudge_events_kept || g.events_kept > g.event_count)
            return fail("a grudge keeps more events than it has");
        for (int k = 1; k < g.events_kept; ++k)
            if (g.events[k - 1].magnitude < g.events[k].magnitude)
                return fail("a grudge's kept events are not sorted by magnitude");
    }

    if (why) why->clear();
    return true;
}
