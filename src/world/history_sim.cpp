#include "history_sim.hpp"

#include "unit_roster.hpp"
#include "terrain_combat.hpp" // BL-384 trace: the defence term the scorer never sees

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <queue>
#include <unordered_map>

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

/// BL-869 — wears `tongue_word`'s `pick(int)` contract off `salt`, so coining
/// a civilisation's name costs no RNG stream of its own — consistent with
/// this file's own rule (top of file): "there is no RNG... nothing here
/// consumes a stream."
struct hash_picker
{
    uint32_t s;
    int pick(int n)
    {
        s = salt(s, 0xC1F1u);
        return n <= 1 ? 0 : static_cast<int>(s % static_cast<uint32_t>(n));
    }
};

/// Coin a civilisation's name FROM THE TONGUES THAT MIXED (CIVILISATION.md:
/// "never from a bank of its own"), drawing each syllable's onset/vowel/coda
/// from one or the other founding culture's OWN phonology in turn — so the
/// result is built entirely out of sounds its members already speak, never a
/// third invented accent, exactly the substrate `world/tongue.hpp` exists for.
///
/// PURE in (@p cultures, @p a, @p b, @p seed) alone — not in region, year or
/// call order — so the same founding pair coins the same name wherever and
/// whenever its mix first crosses the bar.
std::string coin_civilisation_name(const std::vector<culture>& cultures, int a, int b,
                                   uint32_t seed)
{
    const tongue& ta = cultures[static_cast<std::size_t>(a)].speech;
    const tongue& tb = cultures[static_cast<std::size_t>(b)].speech;
    hash_picker r{salt(seed, salt(static_cast<uint32_t>(a), static_cast<uint32_t>(b)))};

    const int syllables = 2 + r.pick(2);
    std::string name;
    for (int i = 0; i < syllables; ++i)
    {
        const tongue& t = (r.pick(2) == 0 && ta.usable()) ? ta : (tb.usable() ? tb : ta);
        if (!t.usable()) continue;
        name += t.onsets[static_cast<std::size_t>(r.pick(static_cast<int>(t.onsets.size())))];
        name += t.vowels[static_cast<std::size_t>(r.pick(static_cast<int>(t.vowels.size())))];
        if (!t.codas.empty() && r.pick(3) == 0)
            name += t.codas[static_cast<std::size_t>(r.pick(static_cast<int>(t.codas.size())))];
    }
    if (name.empty()) name = "mix"; // Both tongues unusable — should not occur in practice.
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
    return name;
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
                q.aggression_q = leaned_aggression_q(
                    params, cs->cultures[static_cast<std::size_t>(c)].aggression_q);
            else
                q.aggression_q = 500; // Neutral when creeds were not supplied.
            // BL-839: the turbulence lean's SPREAD is applied HERE, at the one
            // place a culture's temperament enters a polity, rather than in
            // `build_creeds` -- the pantheon is a fact about the people and is
            // not the player's to lean; how hard that temperament pushes on the
            // sim's scorer is.

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
        // BL-835 — THE WORLD OPENS WITH ARMIES ALREADY STANDING. Seeded at the
        // target rather than at zero: a run that began with every region bare
        // would spend its first century as a free-for-all of undefended ground,
        // which is an artefact of the start and not a fact about the world.
        p.army_stock = garrison_target(p, params.garrison_fraction_q);
        p.manpower_stock = clampi64(p.manpower_stock - p.army_stock, 0, p.manpower_stock);
    }

    // --- SETTLEMENT SEATS, SPARSE FROM THE OPENING MAP (BL-866) -----------
    //
    // CIVILISATION.md § The unit is the city state: "a settlement is a SEAT
    // FLAG on a region, and every region points at the seat it feeds." Each
    // polity's `capital` above is already derived as "the best-settled region
    // of this culture" — exactly what a founding city state's seat is — so
    // this reuses that choice rather than running a second placement pass.
    // One seat per polity is the sparse first cut: with dozens to hundreds of
    // regions per surviving culture, seats land at a small fraction of the
    // map, and an empire's later seat count (several, once it has conquered
    // other polities' capitals — see the conquest block below) is a
    // consequence of war, not of this seeding.
    //
    // EVERY REGION RESOLVES TO A SEAT HERE: `owner[i]` was just derived from
    // `out.polities`, and every polity's `capital` is a valid index (the loop
    // above never leaves one at -1 when regions exist), so no region opens
    // the run already outside anyone's reach.
    for (const polity& q : out.polities)
    {
        if (q.capital < 0 || static_cast<std::size_t>(q.capital) >= ss.regions.size())
            continue;
        region& seat = ss.regions[static_cast<std::size_t>(q.capital)];
        seat.is_seat     = true;
        seat.seat_region = q.capital;
    }
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        region& p = ss.regions[i];
        if (p.is_seat) continue; // Already points at itself, above.
        if (p.nation < 0 || p.nation >= static_cast<int>(out.polities.size())) continue;
        p.seat_region = out.polities[static_cast<std::size_t>(p.nation)].capital;
    }

    // --- THE ANCIENT ROAD RECORD (BL-768) ---------------------------------
    //
    // Appended raw as events happen, then sorted and run-length-encoded into
    // `out.supply_corridors` at the end of the run. A raw list plus one sort is
    // deliberately preferred to a keyed map: the sort key is a pair of plain
    // integers, so the result cannot depend on a container's layout, and the
    // hot loop pays a push_back rather than a tree lookup.
    //
    // NO LONGER PURE OBSERVATION (BL-837). Until this item nothing below read
    // this back — the record existed only for `road_generation.cpp` to stamp
    // onto the FINISHED world's tile grid, one pass removed from the sim that
    // produced it. `road_uses_live` is the same count, kept live, and
    // `rebuild_reach` below reads it: a corridor this run has actually walked
    // repeatedly is cheaper for the REST OF THIS RUN, which is what makes a
    // road something a polity BUILDS rather than a label applied afterwards
    // (CIVILISATION.md's outcome brief: "a sparse road network which connects
    // city-states, forms empires"). `battle_trace` still holds the
    // pure-observation contract; this record no longer does.
    std::vector<std::pair<uint16_t, uint16_t>> corridor_uses;

    // Canonical (lo, hi) edge key -> live use count, read by `rebuild_reach`
    // and gated by `road_tier1_uses`/`road_tier2_uses` below. A plain
    // `unordered_map` is safe here (unlike the ownership/political state)
    // because nothing iterates it — every read is a point lookup on a key
    // computed from region indices, never a walk in map order.
    std::unordered_map<uint64_t, int> road_uses_live;
    // Bumped every time an edge's tier (0/1/2, from `road_tier_for_uses`)
    // actually changes, so `reach_cache` can invalidate on "a road changed"
    // without invalidating on every single use — most uses do not cross a
    // tier boundary, and re-running a polity's Dijkstra on every one of them
    // would revive the per-round-rebuild cost BL-834 just eliminated.
    int roads_version = 0;

    // BL-887 — THE SAME TRICK FOR THE CENTRE MAP. With `centre_chain_reach`
    // on, a region's `centres` is an input to `rebuild_reach`, and towns turn
    // over constantly (promoted in the demography loop, razed by a sack). This
    // counter moves only when a count ACTUALLY CHANGES, so a quiet century
    // costs no rebuilds at all and the BL-834 cache survives. With the model
    // off nothing ever bumps it and the cache behaves exactly as it did.
    int centres_version = 0;

    const auto edge_key = [](int a, int b) -> uint64_t {
        const uint32_t lo = static_cast<uint32_t>(a < b ? a : b);
        const uint32_t hi = static_cast<uint32_t>(a < b ? b : a);
        return (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);
    };
    const auto road_tier_for_uses = [&](int uses) {
        if (uses >= params.road_tier2_uses) return 2;
        if (uses >= params.road_tier1_uses) return 1;
        return 0;
    };
    // Mirrors logistics.hpp's `road_traversal_multiplier` exactly (1 / (1 +
    // 0.5 x tier): Track ~0.67, Road 0.50) — the SAME discount shape the
    // campaign-era A* already applies for the same reason, restated here
    // rather than called across the ECS boundary this file is deliberately
    // free of (settlement.hpp: "no `world&`, no tile ids, no allocator").
    const auto road_discount = [](int tier) {
        return 1.0f / (1.0f + 0.5f * static_cast<float>(tier));
    };
    const auto road_tier_between = [&](int a, int b) {
        const auto it = road_uses_live.find(edge_key(a, b));
        return it != road_uses_live.end() ? road_tier_for_uses(it->second) : 0;
    };

    // BL-895 -- WHAT A PLACE IS BEST AT, as a CLASS rather than a quantity.
    // The phase has no price, so trade cannot read "how much"; it can only read
    // "unlike". Three classes because three are what a region carries: farming
    // ground, ore ground, and a port. Ties resolve in a fixed order so the
    // classification cannot depend on anything but the region's own numbers.
    const auto region_trade_class = [](const region& p) {
        if (p.port_q >= p.farm_q && p.port_q >= p.ore_q) return 2;
        if (p.ore_q  >= p.farm_q)                        return 1;
        return 0;
    };

    // BL-895 sink 2 -- A ROAD COSTS MATERIALS TO BUILD. `payer_seat` is the
    // acting polity's capital, or -1 for a walk nobody is paying for (which is
    // also the disabled path: `road_build_material_cost == 0`). The WALK is
    // always recorded -- `corridor_uses` is the pure-observation record
    // `road_generation.cpp` stamps onto the finished world, and a party that
    // walked a line walked it whether or not anyone widened it. What money buys
    // is the TIER, which is the thing that does something to reach.
    //
    // A REFUSED PROMOTION HOLDS THE COUNT ONE SHORT rather than discarding the
    // walk, so the corridor is promoted the next time it is walked with the
    // materials standing. Poverty DELAYS a road; it does not forbid one.
    const auto note_corridor = [&](int a, int b, int payer_seat) {
        if (a < 0 || b < 0 || a == b) return;
        if (a >= static_cast<int>(owner_index_limit)
         || b >= static_cast<int>(owner_index_limit)) return;
        const uint16_t lo = static_cast<uint16_t>(a < b ? a : b);
        const uint16_t hi = static_cast<uint16_t>(a < b ? b : a);
        corridor_uses.push_back({lo, hi});

        int& uses = road_uses_live[edge_key(a, b)];
        const int before = road_tier_for_uses(uses);
        ++uses;
        const int after = road_tier_for_uses(uses);
        if (after == before) return;

        if (params.road_build_material_cost > 0)
        {
            region* seat = (payer_seat >= 0
                         && static_cast<std::size_t>(payer_seat) < ss.regions.size())
                         ? &ss.regions[static_cast<std::size_t>(payer_seat)] : nullptr;
            const int64_t cost = params.road_build_material_cost;
            if (seat == nullptr || seat->material_stock < cost)
            {
                --uses;                       // held one short, not thrown away
                ++out.road_builds_refused;
                return;
            }
            seat->material_stock -= cost;
            out.materials_spent_on_roads += cost;
        }
        ++roads_version;
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

    // --- Neighbour index (BL-855: degree-capped) ---------------------------
    //
    // Campaign candidates are NEIGHBOURS ONLY, so the neighbourhood is built
    // once rather than rediscovered by scanning every region from every held
    // region every year. That scan is quadratic in region count and, with
    // the Settle verb growing the map past 400 regions, it dominated the
    // whole run (2.5s of a 2.5s run). Built once here, extended when a
    // region is founded, it is a lookup.
    //
    // BL-855: `neighbour_radius` alone is not a bound on degree. The map does
    // not grow but the regions filling it do, so a fixed-radius disc holds
    // more and more of them as a run goes on — the graph densifies and
    // BL-844's heap could not fix an O(E log V) that was growing because E
    // was. `max_neighbour_degree` caps it: each region links only to its
    // nearest candidates (by `region_distance`, ties on the lower index — no
    // container-order dependency), and only while both ends still have a
    // free slot.
    //
    // WHY THIS IS DETERMINISTIC EVEN THOUGH IT IS ORDER-SENSITIVE. Capping
    // degree while filling slots on a first-come basis means a region's final
    // neighbour set can depend on which of its candidates were linked
    // FIRST — but "first" here is region-INDEX order, walked 0..N-1 below and
    // by founding order thereafter (`link_region` is only ever called for the
    // newest region against the ones that already exist). Region index is
    // itself a deterministic function of the seed and the simulation trace,
    // so the result is reproducible byte-for-byte; it is simply no longer a
    // pure function of the region SET alone, the way the uncapped radius was.
    std::vector<std::vector<int>> neighbours(ss.regions.size());
    std::vector<int> degree(ss.regions.size(), 0);
    const auto link_region = [&](std::size_t i) {
        std::vector<std::pair<int, int>> candidates; // (distance, region index)
        for (std::size_t j = 0; j < i; ++j)
        {
            if (degree[j] >= params.max_neighbour_degree) continue; // No free slot.
            const int d = region_distance(ss.regions[i], ss.regions[j], gw);
            if (d <= params.neighbour_radius)
                candidates.emplace_back(d, static_cast<int>(j));
        }
        std::sort(candidates.begin(), candidates.end()); // Nearest first, index tie-break.
        for (const std::pair<int, int>& c : candidates)
        {
            if (degree[i] >= params.max_neighbour_degree) break;
            const std::size_t j = static_cast<std::size_t>(c.second);
            neighbours[i].push_back(static_cast<int>(j));
            neighbours[j].push_back(static_cast<int>(i));
            ++degree[i];
            ++degree[j];
        }
    };
    for (std::size_t i = 0; i < ss.regions.size(); ++i) link_region(i);

    // --- Terrain-weighted reach (BL-314 S2) -------------------------------
    //
    // Distance from the capital is a COST over the neighbour graph, not a
    // straight line: crossing a mountain range costs about twice what crossing
    // plains does, using the landform ratios logistics.cpp already defines for
    // the 1960 era. Computed by Dijkstra from the capital and cached until the
    // capital moves, so the per-year cost stays a lookup.
    //
    // ONE SLOT PER POLITY, NOT ONE SHARED SLOT (BL-834). The paragraph above is
    // true of a single polity and false of twelve. A shared slot holds the
    // PREVIOUS polity's capital when the next one asks for reach, so the cache
    // never survived one iteration of the round and every polity paid a full
    // Dijkstra every round: 12,000 rebuilds over a 1,000-round run, and 66-86%
    // of the whole span. That is measured rather than suspected (BL-825).
    //
    // The key is `polity::id`, which is that polity's index in `out.polities`
    // and is assigned once at seeding, so the container is a plain vector and
    // there is no iteration order for a result to depend on.
    //
    // WHY THIS IS OUTPUT-IDENTICAL, which is the only property that matters in
    // `world/*`: the cost vector is a function of the capital, the neighbour
    // graph and the terrain under each region's anchor. Terrain is fixed for the
    // run and an anchor is written once, at founding. The graph is mutated in
    // exactly one place — `link_region`, called only when a region is founded —
    // and that founding also grows `ss.regions`. So the region-count check below
    // catches every graph mutation, and the capital check catches the only two
    // places a capital is assigned. There is no third input to go stale.
    struct reach_cache
    {
        std::vector<int> cost;    ///< Per-region cost from `capital`.
        int              capital = -2; ///< Which capital `cost` was built for.
        /// BL-837: `roads_version` this cost vector was built against. A road
        /// crossing a tier changes an edge weight without moving the capital
        /// or growing the region count, so it is a THIRD invalidation input
        /// alongside the two the header comment above names.
        int              roads_version = -1;
        /// BL-887: `centres_version` this cost vector was built against. With
        /// `centre_chain_reach` on, a town standing up or being sacked changes
        /// a relay without moving the capital, growing the region count or
        /// crossing a road tier -- so it is a FOURTH invalidation input, and
        /// the header comment above is exhaustive only with it listed.
        int              centres_version = -1;
    };
    std::vector<reach_cache> reach_by_polity;

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

    const auto rebuild_reach = [&](reach_cache& rc, int capital) {
        const scoped_ns prof_reach(prof.ns_reach); // BL-825, report-only
        ++prof.reach_rebuilds;
        std::vector<int>& reach = rc.cost;
        reach.assign(ss.regions.size(), 1 << 28);
        if (capital < 0 || capital >= static_cast<int>(ss.regions.size())) return;
        reach[static_cast<std::size_t>(capital)] = 0;

        // DIJKSTRA WITH A HEAP (BL-844), REPLACING A LINEAR SCAN.
        //
        // The scan this replaces was a deliberate choice, and half of its
        // reasoning was right: a tie-break inside a priority queue is exactly
        // the container-order dependency `world/*` forbids. The other half —
        // "region counts are hundreds" — stopped being true. A run founds
        // constantly, from ~150 regions to ~1,190 over 4,000 years, so the scan
        // was O(N^2) against a graph whose edges are only O(N) and the cost per
        // rebuild rose with the span (BL-834's re-measurement).
        //
        // THE TIE-BREAK IS ELIMINATED RATHER THAN TRUSTED. The comparator
        // orders on the PAIR (cost, region index), and region indices are
        // unique, so no two queue entries ever compare equal and there is no
        // tie for the heap's layout to break. That is also precisely what the
        // scan did: `reach[i] < best_c` is strict, so it took the lowest cost
        // and, among equals, the lowest index. Same order, same result.
        //
        // Lazy deletion, not decrease-key: a relaxed region is pushed again and
        // the stale entry is skipped when it surfaces. `done` is what makes
        // that safe, and it is the same `done` the scan used.
        //
        // UNREACHABLE REGIONS ARE NEVER ENQUEUED, which preserves the scan's
        // `if (best < 0) break`. A region enters the queue only when something
        // relaxes it below its 1 << 28 sentinel, so the queue empties exactly
        // when the scan would have found nothing left under the sentinel.
        //
        // BL-887 — THE CENTRE RELAY, AND WHY `done` GOES AWAY WHEN IT IS ON.
        // A relaying region refunds part of the cost that reached it before it
        // relaxes onward, which is a NODE DISCOUNT, and a node discount breaks
        // the one property plain Dijkstra rests on: that a settled region can
        // never be improved later. A path arriving at a town by a longer route
        // can leave it cheaper than a shorter route left a bare region, so a
        // region already settled genuinely may need relaxing again.
        //
        // The fix is to stop asserting the property rather than to trust it:
        // with the relay on, a popped entry is skipped only when it is STALE
        // (`best_c > reach[best]`), and an improved region is simply pushed
        // again. That is the same lazy-deletion queue run to a FIXPOINT
        // instead of to a single settle per region.
        //
        // IT TERMINATES, and the reason is damping point 2 on the param: every
        // edge step is clamped to at least 1 and the rebate is capped strictly
        // below the whole accrued cost, so a relaxation strictly LOWERS an
        // integer that is bounded below by zero. There is no negative cycle to
        // ride down, because there is no negative edge.
        //
        // IT IS STILL DETERMINISTIC, and for a stronger reason than the
        // comparator's tie-break: the result is the unique least-cost fixpoint
        // of a fixed graph, so the ORDER entries come off the heap cannot
        // change the answer at all -- only how many pops it takes to get
        // there. The comparator's (cost, index) pair is kept anyway so the pop
        // order itself stays reproducible for profiling.
        using node = std::pair<int, int>; // (cost, region index) — never equal.
        std::priority_queue<node, std::vector<node>, std::greater<node>> frontier;
        std::vector<bool> done(ss.regions.size(), false);
        const bool relay = params.centre_chain_reach;
        const int  relay_min = std::max(1, params.centre_reach_min_centres);
        const int  relay_per = clampi(params.centre_reach_rebate_q, 0, 999);
        const int  relay_cap = clampi(params.centre_reach_rebate_cap_q, 0, 999);
        frontier.push({0, capital});
        while (!frontier.empty())
        {
            const node top = frontier.top();
            frontier.pop();
            const int best_c = top.first, best = top.second;
            if (relay)
            {
                // Stale-entry test, replacing the settle test. `reach[best]`
                // is the best cost known for this region right now; anything
                // worse is a superseded push.
                if (best_c > reach[static_cast<std::size_t>(best)]) continue;
            }
            else
            {
                if (done[static_cast<std::size_t>(best)]) continue; // A stale entry.
                done[static_cast<std::size_t>(best)] = true;
            }

            const region& bp = ss.regions[static_cast<std::size_t>(best)];
            // THE REBATE. A fraction of what it COST to get here, never a
            // credit against nothing -- the capital sits at `best_c == 0` and
            // refunds zero, which is what stops a centre bootstrapping its own
            // reach. The fraction rises with the town count (LOGISTICS.md's
            // "cities generate it", a bigger node moving more) and is capped
            // strictly below the whole cost.
            int out_c = best_c;
            if (relay && best != capital && bp.centres >= relay_min)
            {
                const int64_t frac = std::min<int64_t>(
                    static_cast<int64_t>(bp.centres) * relay_per, relay_cap);
                out_c = best_c - static_cast<int>((static_cast<int64_t>(best_c) * frac) / 1000);
                if (out_c < 0) out_c = 0; // Unreachable given frac < 1000; belt and braces.
            }
            for (int nb : neighbours[static_cast<std::size_t>(best)])
            {
                const region& np2 = ss.regions[static_cast<std::size_t>(nb)];
                const int raw_step = region_distance(bp, np2, gw)
                                    * (tile_cost(bp) + tile_cost(np2)) / 200;
                // BL-837 — THE ROAD DISCOUNT, applied to the SAME edge this
                // history's own corridors have actually walked. A tier-2 edge
                // costs half what an unroaded one does, mirroring
                // logistics.cpp's `road_traversal_multiplier` exactly (see
                // `road_discount` above). This is what makes "the only way to
                // reach further is to BUILD further" literally true of the
                // Dijkstra: the capital's effective reach grows along lines
                // the polity has actually used, never along a straight-line
                // radius.
                const float discount = road_discount(road_tier_between(best, nb));
                const int step = raw_step > 0
                                ? std::max(1, static_cast<int>(raw_step * discount + 0.5f))
                                : 1;
                const int cand = out_c + step; // BL-887: `out_c == best_c` with the relay off.
                if (cand < reach[static_cast<std::size_t>(nb)])
                {
                    reach[static_cast<std::size_t>(nb)] = cand;
                    frontier.push({cand, nb});
                }
            }
        }
        rc.capital         = capital;
        rc.roads_version   = roads_version;
        rc.centres_version = centres_version;
    };

    // --- Time-lapse change list -------------------------------------------
    out.owner_changes.clear();
    for (std::size_t i = 0; i < owner.size(); ++i)
        if (owner[i] >= 0)
            out.owner_changes.push_back(owner_change{
                static_cast<int32_t>(params.start_year),
                static_cast<uint16_t>(i),
                static_cast<uint16_t>(owner[i])});


    // --- THE PLAYBACK RECORD (BL-817) -------------------------------------
    //
    // Ownership above is one third of a time-lapse. These two closures record
    // the other two: the per-polity SERIES the scoreboard re-ranks off, and the
    // culture mix as it drifts. era_timelapse.hpp § The playback record carries
    // the encoding argument; what matters here is the direction of the arrows.
    //
    // THE RECORDER ONLY READS THE SIM. It touches `owner`, `ss.regions` and
    // `out.polities` as const, and writes into `out.steps` / `out.samples` /
    // `out.culture_changes` and its own two locals. It draws no random number,
    // it advances no clock, and nothing below reads a field it wrote — so a run
    // with `record_playback` off differs from a recorded run in the three
    // vectors and in nothing else. `history_sim_harness` asserts that on the
    // same seed rather than trusting this paragraph.
    const int record_interval = params.record_interval_years > 0
                              ? params.record_interval_years : 1;
    // Last shares WRITTEN for each region, so a change is a change since the
    // last RECORDED step rather than since the last year. Grows with the region
    // vector, which only ever appends (Settle), so an index is stable for life.
    std::vector<culture_shares> last_shares;
    std::vector<char>           shares_seen;
    int64_t                     last_record_year = INT64_MIN;

    // Per-polity accumulators, hoisted so a 200-step run does not allocate 200
    // times.
    //
    // THE SIM DOES GROW THE POLITY TABLE NOW, and this comment used to say the
    // opposite. It was true when it was written: polities were seeded once and
    // the table was fixed for the run. BL-846's founding schedule meeting
    // BL-856's coined cultures broke it — a region founded mid-span carrying a
    // culture the migration invented needs a seat, so one is created on the
    // spot (see the schedule block in the year loop).
    //
    // These two vectors were sized ONCE against the old invariant while the
    // sample loop below walks `out.polities.size()` as it stands NOW, so every
    // polity born mid-run indexed past the end. In Release that is a silent
    // out-of-bounds write and the run appears to work; in a Debug build the
    // bounds check aborts the process, which is how it was found — the wizard
    // died on "Begin" with exit 3 and no message, while both harness builds
    // passed.
    //
    // Grown to fit at the top of the record instead. A stale invariant in a
    // comment is worth more than no comment only while it is true.
    std::vector<int64_t> step_pop(out.polities.size(), 0);
    std::vector<int32_t> step_regions(out.polities.size(), 0);

    const auto record_step = [&](int64_t y_now) {
        if (!params.record_playback) return;

        if (step_pop.size() < out.polities.size())
        {
            step_pop.resize(out.polities.size(), 0);
            step_regions.resize(out.polities.size(), 0);
        }

        std::fill(step_pop.begin(), step_pop.end(), 0);
        std::fill(step_regions.begin(), step_regions.end(), 0);

        const std::size_t n_reg = std::min(ss.regions.size(), owner.size());
        if (last_shares.size() < ss.regions.size())
        {
            last_shares.resize(ss.regions.size());
            shares_seen.resize(ss.regions.size(), 0);
        }

        timelapse_step st;
        st.year         = static_cast<int32_t>(y_now);
        st.first_sample = static_cast<int32_t>(out.samples.size());

        // ONE walk in ascending region index — the raster order the rest of this
        // file walks in — folding both halves of the record at once. The culture
        // entries therefore come out ascending by region within a year without
        // a sort, which is the ordering era_timelapse.hpp promises.
        for (std::size_t i = 0; i < n_reg; ++i)
        {
            const region& r = ss.regions[i];
            const int     o = owner[i];
            if (o >= 0 && o < static_cast<int>(step_pop.size()))
            {
                step_pop[static_cast<std::size_t>(o)] += r.population;
                ++step_regions[static_cast<std::size_t>(o)];
            }

            if (!shares_seen[i] || last_shares[i] != r.culture)
            {
                last_shares[i] = r.culture;
                shares_seen[i] = 1;
                culture_change c;
                c.year    = static_cast<int32_t>(y_now);
                c.region  = static_cast<uint16_t>(i);
                for (int k = 0; k < timelapse_culture_slots && k < culture_share_slots; ++k)
                {
                    c.id[k]       = r.culture.id[k];
                    c.weight_q[k] = r.culture.weight_q[k];
                }
                c.other_q = r.culture.other_q;
                out.culture_changes.push_back(c);
            }
        }

        // Ascending polity id, LIVING ONLY — a realm holding nothing is absent
        // from the step rather than present with zeros, the same rule
        // `make_pass_one_output` applies to holdings.
        for (std::size_t pi = 0; pi < out.polities.size(); ++pi)
        {
            if (step_regions[pi] <= 0) continue;
            polity_sample smp;
            smp.polity        = static_cast<uint16_t>(pi);
            smp.regions       = static_cast<uint16_t>(step_regions[pi]);
            smp.population    = step_pop[pi];
            smp.cap_military  = static_cast<uint8_t>(clampi(
                out.polities[pi].capacity[static_cast<int>(sim_domain::military)], 0, 255));
            smp.cap_materials = static_cast<uint8_t>(clampi(
                out.polities[pi].capacity[static_cast<int>(sim_domain::materials)], 0, 255));
            out.samples.push_back(smp);
        }

        st.sample_count = static_cast<int32_t>(out.samples.size()) - st.first_sample;
        out.steps.push_back(st);
        last_record_year = y_now;
    };

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

        // ---- The founding schedule (BL-846) ------------------------------
        //
        // COLONISATION HAPPENS HERE, INSIDE THE SPAN, and that is the whole
        // point of the block. `run_settlement` used to hand this loop a
        // finished map — every region it would ever have, placed before the
        // first tick — so the only thing the sim could show was borders moving
        // and a time-lapse of it opened on a world already full. Now a region
        // whose stream arrives during the span waits in `pending_foundings`
        // and is founded when its year comes round.
        //
        // NO ACTOR, exactly as COLONISATION.md requires: nothing scores this,
        // nothing chooses it, and no polity decides. The year arrived, so the
        // people arrived. It is the diffusion's schedule being played back, not
        // a sixth verb.
        //
        // The list is sorted ascending by (year, anchor), so this drains a
        // prefix and the walk order is a total order on stable values.
        while (!ss.pending_foundings.empty()
               && ss.pending_foundings.front().founded_year <= y)
        {
            // The change list indexes regions as uint16_t, so refuse to create
            // one the time-lapse could not address — the same guard the Settle
            // verb carries, and for the same reason (BL-312): past 65,535 the
            // cast wrapped silently to a small in-range index and replay drew a
            // plausible but WRONG map.
            if (ss.regions.size() >= owner_index_limit) break;

            region np = std::move(ss.pending_foundings.front());
            ss.pending_foundings.erase(ss.pending_foundings.begin());

            // WHOSE IT IS: the polity of the people whose stream arrived. The
            // same plurality rule the world-opening seed uses, so a region
            // founded in year -3000 is owned on identical terms to one that was
            // there at tick zero.
            const int np_culture = np.culture.plurality();
            int np_owner = -1;
            for (const polity& q : out.polities)
                if (q.culture == np_culture) { np_owner = q.id; break; }

            // A PEOPLE THAT COMES INTO BEING AND SETTLES GROUND IS A POWER
            // (Ben, 2026-09-09, choosing this over adopting daughters into their
            // parent's polity).
            //
            // THE DEFECT THIS CLOSES, and it was measured rather than guessed:
            // polities are seeded ONCE, at the top of this function, from the
            // cultures present in the opening region set — the cradle cultures.
            // A region founded mid-span carrying a culture the MIGRATION coined
            // (BL-856) therefore matched no polity, took `nation = -1`, and
            // never entered the ownership record at all. On seed 0 that was 604
            // foundings against 523 ownership changes: EIGHTY-ONE regions
            // founded and owned by nobody, whose ground the wizard's map drew as
            // permanent grey wilderness. Half a continent of "unsettled" land
            // was in fact settled by peoples the sim had no seat for.
            //
            // Seeding on demand is the honest reading: a people exists, it holds
            // ground, so it is a power — however small, and it starts with
            // exactly one region because it has only just arrived.
            //
            // DETERMINISTIC: the schedule is drained in (year, anchor) order, so
            // ids are handed out in that order on every machine. `reach_by_polity`
            // is keyed on `polity::id` and resizes on demand, so a polity
            // appearing mid-run costs it nothing. No iterator over `out.polities`
            // is live here — this block runs before the decision round opens.
            if (np_owner < 0 && np_culture >= 0)
            {
                polity q;
                q.id      = static_cast<int>(out.polities.size());
                q.culture = np_culture;
                q.aggression_q =
                    (cs && np_culture < static_cast<int>(cs->cultures.size()))
                        ? leaned_aggression_q(
                              params, cs->cultures[static_cast<std::size_t>(np_culture)].aggression_q)
                        : 500; // BL-839: same lean as the founding read above.
                // Its seat is the region it just founded — the only one it has.
                q.capital = static_cast<int>(ss.regions.size());
                np_owner  = q.id;
                out.polities.push_back(q);
            }
            np.nation = np_owner;

            // BL-867 — EVERY FOUNDING GETS A SEAT POINTER, on the same rule
            // the opening map uses (above): a brand-new polity's first
            // region IS its seat (its `capital` was just set to this very
            // index, above), an existing polity's new region is ordinary
            // hinterland pointing at the capital it already has. Without
            // this a region founded here defaults to `seat_region == -1` —
            // "falls outside anyone's reach", the honest reading for ground
            // no seat touches, but the wrong one for ground a seat's own
            // people just walked onto.
            if (np_owner >= 0)
            {
                const int cap = out.polities[static_cast<std::size_t>(np_owner)].capital;
                np.seat_region = cap;
                if (cap == static_cast<int>(ss.regions.size())) np.is_seat = true;
            }

            ss.regions.push_back(std::move(np));
            owner.push_back(np_owner);
            neighbours.emplace_back();
            degree.push_back(0);
            link_region(ss.regions.size() - 1); // Keep the index complete.

            if (np_owner >= 0)
                out.owner_changes.push_back(owner_change{
                    static_cast<int32_t>(y),
                    static_cast<uint16_t>(ss.regions.size() - 1),
                    static_cast<uint16_t>(np_owner)});
            ++out.foundings;
            out.history.push_back(history_event{
                years_from_calendar_year(y), chain_stage::legacy,
                ss.regions.back().name + " is settled", std::string{}});
        }

        // ---- Demography -------------------------------------------------
        int64_t total_pop = 0;
        {
        const scoped_ns prof_demo(prof.ns_demography); // BL-825, report-only
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
        {
            // BL-835 — WAR PRESSURE IS NO LONGER A DEMOGRAPHIC INPUT, and the
            // literal 0 is the change rather than an omission.
            //
            // `advance_region_demography`'s third argument is a battle-intensity
            // drawdown: up to ~4% of a region's people a year, killed by the
            // fighting standing on them. That is total warfare, and Ben's ruling
            // is that stage 4 does not simulate it. Population moves on
            // demography, habitability, famine and plague; the war is fought by
            // `army_stock`, one field over, and it is spent there.
            //
            // The settlement-level API keeps the parameter — it is a correct
            // model of something, `demography_harness` asserts it, and the
            // campaign era may yet want it. This sim just stops feeding it.
            advance_region_demography(ss.regions[i], 1, /*war_pressure_q=*/0);
            // BL-766: the cities drawn before this loop started live through it
            // — they grow with the region and thin when it thins.
            //
            // BL-872 — GATED ON LAST DECISION ROUND'S NETWORK READING. This
            // demography loop runs every year; `region::network_supply_q` is
            // only refreshed once per DECISION round, inside the per-polity
            // loop further down (the same place BL-837 prices a garrison's
            // upkeep). So this reads a value up to one decision-round stale —
            // deliberate, deterministic, and the same lag `mean_reach_q` and
            // every other per-round aggregate in this file already carries.
            // On year one, before any decision round has run at all, the
            // field's own default (1000, full supply) is what is read.
            // BL-887: a town standing up here is a new relay for
            // `rebuild_reach`, so the centre count is watched across the call
            // and `centres_version` bumped only when it actually moved. Most
            // years it does not, which is what keeps the BL-834 cache alive.
            const int centres_before = ss.regions[i].centres;
            advance_region_urban(ss.regions[i],
                ss.regions[i].network_supply_q > params.sustainable_settlement_floor_q);
            if (ss.regions[i].centres != centres_before) ++centres_version;
            // BL-835 — ONE YEAR OF THE MUSTER, for every region whether or not
            // anyone is fighting over it. This is what makes an undefended
            // region a TEMPORARY state: a region stripped by a march away, or
            // by a garrison broken in battle, is rebuilding from the year after
            // it happened, at the pace its own people can pay for.
            muster_garrison(ss.regions[i], params.garrison_fraction_q,
                            params.garrison_muster_q, params.garrison_disband_q);
            total_pop += ss.regions[i].population;

            // BL-867 — INDUSTRY FLOWS TO THE SEAT, EVERY REGION, EVERY YEAR.
            // Computed AFTER this year's muster, so a garrison mustered up
            // this round already shows in the labour left for industry —
            // CIVILISATION.md's "starves or stops producing" as arithmetic on
            // `manpower_ceiling` and `army_stock`, both already advanced
            // above. `seat_region` is never -1 for a region the owning loop
            // has reached (the opening seed and both founding sites below all
            // set it), so this is unconditional rather than a defensive
            // check on a case that should not occur.
            // BL-895 -- TRADE INCOME FROM THE NETWORK, not from a market.
            //
            // A roaded link between two held regions that hold UNLIKE ground
            // yields materials. Only DIFFERENCE is read: this phase has no
            // order book, no firm and no price (CIVILISATION.md sec Materials
            // are spent when something happens), so "what a place holds" is a
            // CLASS -- what it is best at -- and never a quantity.
            //
            // Counted from the lower-indexed region only, so a pair is not paid
            // twice, and gated on a walked corridor rather than mere adjacency:
            // trade follows the road, which is what makes BL-837's network
            // worth building for a second reason and what makes BL-896's
            // collapse-by-network-failure cost a realm its income before it
            // costs it ground.
            if (params.trade_income_per_link > 0)
            {
                const int oi = owner[i] == owner_none ? -1 : static_cast<int>(owner[i]);
                if (oi >= 0)
                {
                    for (int nb : neighbours[i])
                    {
                        if (nb <= static_cast<int>(i)) continue;          // pay the pair once
                        if (owner[static_cast<std::size_t>(nb)] != owner[i]) continue;
                        // ANY WALKED CORRIDOR CARRIES TRADE, not only a promoted
                        // Track. Measured 2026-09-11: of 1,607 distinct corridors,
                        // 1,347 are walked ONCE and only 155 reach the 4 uses
                        // `road_tier1_uses` needs -- about 52 roaded edges per
                        // world. Gating trade on a Track therefore paid almost
                        // nothing (0.013% of materials) for a reason that had
                        // nothing to do with trade. A route people have walked is
                        // a trade route whether or not it has been widened; the
                        // TIER is about what a line does to REACH, which is
                        // BL-837's subject, not this one's.
                        if (road_uses_live.find(edge_key(static_cast<int>(i), nb))
                            == road_uses_live.end()) continue;
                        if (region_trade_class(ss.regions[i])
                         == region_trade_class(ss.regions[static_cast<std::size_t>(nb)])) continue;
                        const int seat_t = ss.regions[i].seat_region;
                        if (seat_t >= 0 && static_cast<std::size_t>(seat_t) < ss.regions.size())
                        {
                            ss.regions[static_cast<std::size_t>(seat_t)].material_stock +=
                                params.trade_income_per_link;
                            out.materials_produced  += params.trade_income_per_link;
                            out.materials_from_trade += params.trade_income_per_link;
                        }
                    }
                }
            }

            const int64_t produced = region_industry_output(ss.regions[i]);
            if (produced > 0)
            {
                const int seat_idx = ss.regions[i].seat_region;
                if (seat_idx >= 0 && static_cast<std::size_t>(seat_idx) < ss.regions.size())
                {
                    ss.regions[static_cast<std::size_t>(seat_idx)].material_stock += produced;
                    out.materials_produced += produced;
                }
            }
        }

        // ---- BL-895 sink 1: THE STANDING ARMY EATS, EVERY YEAR -----------
        //
        // A SECOND PASS, deliberately. The loop above credits industry and
        // trade to seats as it walks the regions in index order, so a charge
        // levied inside it would read a seat's stock half-accumulated and the
        // answer would depend on where the paying region sat relative to its
        // own capital in the index. Charging afterwards asks every garrison
        // the same question about a settled year, which is the determinism
        // requirement stated plainly rather than a tidiness preference.
        //
        // UNPAID TROOPS GO HOME, they do not serve for free and they do not
        // put the seat into deficit. The heads return to `manpower_stock`
        // exactly as `muster_garrison`'s over-target disband sends them --
        // a discharged soldier was never subtracted from `population`, so
        // returning him to the recruitable pool is the only consistent
        // destination. This is the strangling channel BL-896 needs: a realm
        // cut off from its income loses its army without losing a battle.
        if (params.army_upkeep_per_1000_heads > 0)
        {
            for (std::size_t i = 0; i < ss.regions.size(); ++i)
            {
                region& p = ss.regions[i];
                if (p.army_stock <= 0) continue;
                const int64_t due =
                    (p.army_stock * params.army_upkeep_per_1000_heads) / 1000;
                if (due <= 0) continue;

                const int st = p.seat_region;
                int64_t paid = 0;
                if (st >= 0 && static_cast<std::size_t>(st) < ss.regions.size())
                {
                    region& seat = ss.regions[static_cast<std::size_t>(st)];
                    paid = std::min(due, seat.material_stock);
                    seat.material_stock -= paid;
                    out.materials_spent_on_upkeep += paid;
                }

                if (paid >= due || params.unpaid_army_disband_q <= 0) continue;

                // The unpaid SHARE of the garrison, then the per-mille of that
                // share which walks off this year. Both bounded by the pool
                // itself, so a region can never disband more than it holds.
                const int64_t unpaid_heads =
                    (p.army_stock * (due - paid)) / due;
                const int64_t gone = std::min(
                    p.army_stock,
                    (unpaid_heads * params.unpaid_army_disband_q) / 1000);
                if (gone <= 0) continue;
                p.army_stock    -= gone;
                p.manpower_stock += gone;
                out.army_heads_unpaid_disbanded += gone;
            }
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
            {
                // Capital fell. The successor is the polity's lowest-indexed
                // surviving region — placement order, which is best-ground
                // first, so it is a reasonable seat without being "the largest
                // holding" the first cut's comment claimed (BL-312).
                const int old_capital = q.capital;
                q.capital = held.front();

                // BL-866 — THE SURVIVING HINTERLAND FOLLOWS ITS REALM'S NEW
                // SEAT. The old capital is no longer this polity's seat (it
                // now belongs to whoever just took it, and stays a seat in
                // its own right there — see the conquest block), so the
                // regions this polity still holds that pointed at it would
                // otherwise be feeding ground they no longer own.
                region& new_seat = ss.regions[static_cast<std::size_t>(q.capital)];
                new_seat.is_seat     = true;
                new_seat.seat_region = q.capital;
                for (int h : held)
                    if (h != q.capital
                     && ss.regions[static_cast<std::size_t>(h)].seat_region == old_capital)
                        ss.regions[static_cast<std::size_t>(h)].seat_region = q.capital;
            }

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

            // ---- CIVILISATIONS FROM MIXING (BL-869) -----------------------
            //
            // CIVILISATION.md § A civilisation is what mixing makes: "a region
            // carrying two peoples in quantity, for a long time" is the
            // trigger. ASSIMILATION just above is what MOVES the shares; this
            // is what reads how long they have sat mixed, using the same
            // `held` this round already built and the same `step_years`
            // clock the rate above it uses. NO ACTOR: nothing here is a
            // polity choice, exactly like the founding schedule earlier in
            // this loop — a mix either has settled into an answer or it has
            // not, and the sim only reports which.
            if (cs != nullptr)
            {
                for (int hi : held)
                {
                    region& rg = ss.regions[static_cast<std::size_t>(hi)];
                    if (rg.civilisation >= 0) continue; // Formed once, never revisited.

                    const int second = rg.culture.id[1];
                    const bool mixed_now =
                        second >= 0 && rg.culture.weight_q[1] >= civilisation_mix_threshold_q;
                    // UNBROKEN, not cumulative (settlement.hpp: "a mix that
                    // digests away and later returns starts the clock over").
                    rg.mix_years = mixed_now ? rg.mix_years + step_years : 0;
                    if (!mixed_now || rg.mix_years < civilisation_mix_years_bar) continue;

                    const int a = std::min(static_cast<int>(rg.culture.id[0]), second);
                    const int b = std::max(static_cast<int>(rg.culture.id[0]), second);
                    if (a < 0 || b >= static_cast<int>(cs->cultures.size())) continue;
                    const int op_q = culture_opposition_q(cs->cultures, a, b);

                    // NR-817 — FRACTURE IS THE FORMATION RULE. Too opposed to
                    // settle a shared answer about how to live, so no
                    // civilisation is coined here at all. The region tries
                    // again next round: assimilation runs every round too, so
                    // the mix itself — and therefore the pair being read —
                    // can still change.
                    if (op_q > civilisation_opposition_bar_q) continue;

                    // Find the record for exactly this pair, or found one.
                    // MANY regions sharing the same two peoples grow ONE
                    // civilisation, not one each — the record is a fact about
                    // the pair, not about any single region's ground.
                    int idx = -1;
                    for (std::size_t i = 0; i < out.civilisations.size(); ++i)
                    {
                        const civilisation& cv = out.civilisations[i];
                        if (cv.members.size() == 2 && cv.members[0] == a && cv.members[1] == b)
                        { idx = static_cast<int>(i); break; }
                    }
                    if (idx < 0)
                    {
                        civilisation cv;
                        cv.members = { a, b };
                        cv.name    = coin_civilisation_name(cs->cultures, a, b, seed);

                        // THE ETHIC — the settled answer to the same two
                        // questions a war god's temperament asks
                        // (CIVILISATION.md: "derived from what the member
                        // cultures disagreed about and settled"). The mean of
                        // the founding pair's own war gods (`pantheon[1]`,
                        // the same god `culture_opposition_q`'s temperament
                        // axis reads), integer-rounded down on a tie so the
                        // result needs no stream of its own.
                        const culture& ca = cs->cultures[static_cast<std::size_t>(a)];
                        const culture& cb = cs->cultures[static_cast<std::size_t>(b)];
                        if (ca.pantheon.size() > 1 && cb.pantheon.size() > 1)
                        {
                            cv.ethic.zeal     = (ca.pantheon[1].zeal     + cb.pantheon[1].zeal)     / 2;
                            cv.ethic.dominion = (ca.pantheon[1].dominion + cb.pantheon[1].dominion) / 2;
                        }
                        cv.strain_q    = op_q; // INHERITED, not resolved (NR-817).
                        cv.formed_year = y;

                        idx = static_cast<int>(out.civilisations.size());
                        out.civilisations.push_back(std::move(cv));
                        ++out.civilisations_formed;
                        out.history.push_back(history_event{
                            years_from_calendar_year(y), chain_stage::legacy,
                            out.civilisations.back().name
                                + " is settled as a shared way of life",
                            std::string{}});
                    }
                    rg.civilisation = idx;
                }
            }

            const region& cap = ss.regions[static_cast<std::size_t>(q.capital)];
            const uint32_t  qs  = salt(seed, static_cast<uint32_t>(q.id));

            // This polity's own slot, grown on demand: `id` is an index into
            // `out.polities`, and nothing appends a polity mid-run, but sizing
            // here rather than up front keeps the two facts independent.
            if (reach_by_polity.size() <= static_cast<std::size_t>(q.id))
                reach_by_polity.resize(static_cast<std::size_t>(q.id) + 1);
            reach_cache& rc = reach_by_polity[static_cast<std::size_t>(q.id)];

            if (rc.capital != q.capital || rc.cost.size() != ss.regions.size()
             || rc.roads_version != roads_version // BL-837: a road crossed a tier
             || (params.centre_chain_reach
              && rc.centres_version != centres_version)) // BL-887: a relay moved
                rebuild_reach(rc, q.capital);
            const std::vector<int>& reach = rc.cost;

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

            // ---- BL-837 — AN ARMY BEYOND SUSTAINABLE REACH CANNOT BE
            // MAINTAINED. The campaign gate below stops a polity PROJECTING
            // force past its roads; this is that rule's standing-army twin,
            // and it is what makes holding distant ground cost something
            // even when nobody ever contests it.
            //
            // Read the SAME terrain-and-road reach `campaign_supply` prices a
            // campaign at (`reach[hi]`, already road-discounted by
            // `rebuild_reach` above), but WITHOUT the staging-hub relief or
            // the burden-of-breadth term: a garrison standing at home pays
            // neither a march's cost nor the empire's own overextension
            // penalty, only the ground's terrain price back to the capital.
            // `sustainable_garrison_floor_q` is deliberately below the
            // campaign floor for exactly that reason (see the field comment).
            //
            // A RATE x THE STEP, like every other per-year accumulator in
            // this loop. `held` is already region-index order, so the walk
            // order is a property of the map, not of anything transient.
            //
            // BL-872 — `region::network_supply_q` IS COMPUTED HERE FOR EVERY
            // HELD REGION, UNCONDITIONALLY, not only ones carrying an army.
            // CIVILISATION.md's open question ("one reach quantity or two")
            // is answered ONE: governance ("can the seat rule this ground")
            // and supply ("can materials reach it") are the SAME terrain-
            // and-road Dijkstra reach from a polity's own seat that this
            // block already priced a garrison's upkeep with, before this
            // item. There is one network and one seat per region in this
            // sim, so a second number here would only restate the first.
            // `advance_region_urban`'s caller (the demography loop, above,
            // one decision round behind) is what reads it to gate
            // `region::centres` growth.
            for (int hi : held)
            {
                region& hp = ss.regions[static_cast<std::size_t>(hi)];
                const std::size_t hidx = static_cast<std::size_t>(hi);
                const int reach_here = (hidx < reach.size() && reach[hidx] < (1 << 27))
                                      ? reach[hidx] : (1 << 27);
                const int hub_reach_q = clampi(hp.work_reach_mod, 0, params.work_reach_relief_cap_q);
                const int terrain_cost = reach_here * leaned_terrain_reach_cost_q(params) / 100;
                const int terrain_paid = terrain_cost - (terrain_cost * hub_reach_q) / 1000;
                const int supply_at_q  = clampi(1000 - terrain_paid, 0, 1000);
                hp.network_supply_q = supply_at_q;

                if (params.unsustained_army_attrition_q > 0 && hp.army_stock > 0
                 && supply_at_q <= params.sustainable_garrison_floor_q)
                {
                    ++out.unsustained_attrition_events;
                    const int64_t lost = (hp.army_stock
                                         * clampi(params.unsustained_army_attrition_q, 0, 1000)
                                         * step_years) / 1000;
                    hp.army_stock = std::max<int64_t>(0, hp.army_stock - lost);
                }
            }

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
                const int terrain_cost = reach_here * leaned_terrain_reach_cost_q(params) / 100;
                const int terrain_paid = terrain_cost - (terrain_cost * hub_reach_q) / 1000;
                return clampi(1000
                            - hub_dist * params.supply_decay_per_tile_q
                            - terrain_paid
                            - clampi(burden, 0, 1000 - params.holdings_burden_floor_q),
                            0, 1000);
            };

            // ---- BL-899: sea legs — the ration a crossing lands on ---------
            //
            // A REDUCED RATION, SCALED BY HOW SEAFARING THE STAGING REGION'S
            // PEOPLE ARE (docs/lore/CREEDS.md § Sea legs). Returns per-mille of
            // the ordinary foraged supply, or 0 where the crossing starves as
            // it does today. Read at the SUPPLY calculation by BOTH the scorer
            // and execute — the file's own thesis is that the estimate and the
            // outcome must ask the identical question.
            //
            // SHARE-WEIGHTED OVER THE REGION'S PEOPLE, not read off the
            // plurality: the ground is mixed and the tradition is carried by
            // whoever actually lives there, so a region half-settled by a
            // seafaring people lands on half the ration. The slots are a
            // fixed-order array, so the sum is order-independent by
            // construction.
            //
            // THREE CONDITIONS, ALL INDEPENDENT: the people carry sea legs
            // above the floor, the staging ground touches water, and the realm
            // can field a naval row at its band. The last is NOT merged into
            // the others — sea legs feed a force that has landed; they never
            // put it on the water.
            const auto sea_legs_ration = [&](int hub) -> int {
                if (params.sea_legs_ration_q <= 0 || hub < 0 || cs == nullptr)
                    return 0;
                const region& h = ss.regions[static_cast<std::size_t>(hub)];
                if (h.port_q < params.sea_legs_port_q) return 0;
                if (!can_field_naval(h, mil_band)) return 0;
                int weighted = 0;
                for (int si = 0; si < culture_share_slots; ++si)
                {
                    const int cid = static_cast<int>(h.culture.id[si]);
                    if (cid < 0 || cid >= static_cast<int>(cs->cultures.size())) continue;
                    weighted += static_cast<int>(h.culture.weight_q[si])
                              * clampi(cs->cultures[static_cast<std::size_t>(cid)].sea_legs_q,
                                       0, 1000);
                }
                const int legs = clampi(weighted / 1000, 0, 1000);
                if (legs < params.sea_legs_floor_q) return 0;
                return clampi((params.sea_legs_ration_q * legs) / 1000, 0, 1000);
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
            // BL-835 — THE FIELD ARMY A CAMPAIGN CAN CONCENTRATE AT ONE HUB.
            //
            // The hub's own garrison, plus what the polity's holdings ADJACENT
            // to the hub can spare, and no more than double the hub's own.
            //
            // WHY THE POOL CANNOT BE ONE REGION'S GARRISON ALONE. Measured:
            // with the attacker fielding a single region's army against a
            // single region's army, the defender's terrain multiplier and works
            // made every campaign a losing proposition, `p_win_q` collapsed to
            // roughly a fifth of what it had been, and the seed-0 fixture went
            // from 258 battles to 0. A model in which no polity can ever
            // profitably attack is not a better model than one in which the
            // same region is taken 258 times; it is the same failure mirrored.
            //
            // WHY IT IS CAPPED AT 2x THE HUB'S OWN. `neighbours` is a radius,
            // not a border, so an uncapped sum would let a large realm mass
            // twenty garrisons on one frontier and roll the map. The cap says
            // a march can roughly double itself from what it picks up on the
            // way and no more, which keeps the contest inside the range the
            // resolver was calibrated on.
            //
            // THE SUPPORTING HOLDINGS SPARE HALF, because they are still
            // covering their own ground — and what they spare is genuinely
            // GONE from them while the campaign runs, which is what makes a
            // wide offensive an uncovered frontier rather than a free action.
            //
            // `commit` is the only difference between the estimate and the
            // execution: the scorer asks the question, the campaign takes the
            // men. One rule, read twice, so a polity cannot be offered a force
            // it then fails to raise (this file's standing thesis — a cost
            // authored on one scale and spent on another is the bug).
            const auto gather_army = [&](int hub, bool commit) -> int64_t {
                const std::size_t hs = static_cast<std::size_t>(hub);
                int64_t total = ss.regions[hs].army_stock;
                if (total <= 0) return 0;
                if (commit) ss.regions[hs].army_stock = 0;

                int64_t budget = total; // The cap: support may match the hub, not exceed it.
                for (int n : neighbours[hs])
                {
                    if (budget <= 0) break;
                    const std::size_t ni = static_cast<std::size_t>(n);
                    if (owner[ni] != q.id) continue;
                    const int64_t spare = ss.regions[ni].army_stock / 2;
                    if (spare <= 0) continue;
                    const int64_t take = std::min(spare, budget);
                    total  += take;
                    budget -= take;
                    if (commit) ss.regions[ni].army_stock -= take;
                }
                return total;
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

            // -- BL-838: FEAR OF BEING NEXT, the decider's side of it -------
            //
            // AI_OPPONENT.md sec 11, the grant "The Era -1 scorer may read the
            // GRUDGE LEDGER, scoped to fear of annihilation" (Ben, 2026-09-11).
            // Until that grant the rule written at the grudge field itself was
            // absolute -- nothing in this sim reads a grudge to make a
            // decision -- so this lambda is the ONE place the ledger is read
            // for a decision, and everything it refuses to read is as much the
            // specification as what it reads.
            //
            // WHAT IT ANSWERS: how much have people LIKE ME suffered at this
            // polity's hands. Third-party grudges only, kin-filtered.
            //
            //   - `g.from == q.id` IS SKIPPED. D reading its own ledger to pick
            //     a target is the revenge term BL-827 declined and the grant
            //     explicitly excludes; it is a term inside the actor rather
            //     than a fact about the world. Without this line the mechanism
            //     would still "work" and would be the wrong mechanism.
            //   - NOTHING HERE TOUCHES SIZE, RANK, holdings, population or
            //     army. The whole input is the ledger, which is a record of
            //     what the target DID, with a place and a date attached to
            //     every contributing event -- which is exactly why a coalition
            //     forming here is explicable on the map and a rank term is not.
            //   - The kin filter is `culture`, the sim's only notion of "a
            //     people like me", and the same one `extinguish_polity` walks
            //     for `realm_ended`. It is not an alliance: nobody agrees to
            //     anything, nobody is told, and each polity reaches the same
            //     conclusion separately from public facts.
            //
            // CACHED PER ROUND, per polity, because the table is scanned
            // linearly and one polity may score many targets in a year. The
            // cache is declared HERE, inside the round, so it cannot outlive a
            // year in which the ledger moves. The scan order is the sorted
            // (from, to) order of `out.grudges`, so the sum is a property of
            // the integers in the table and not of any container's layout.
            std::vector<int> fear_cache;
            const auto fear_here = [&](int target) -> int
            {
                // BL-839: the LEANED weight, so a turbulence setting that
                // scales fear to zero short-circuits exactly as w_fear_q == 0 does.
                if (leaned_w_fear_q(params) == 0) return 0;
                if (fear_cache.empty())
                    fear_cache.assign(out.polities.size(), -1);
                if (target < 0 || target >= static_cast<int>(fear_cache.size())) return 0;
                int& slot = fear_cache[static_cast<std::size_t>(target)];
                if (slot < 0) slot = fear_of_next_q(out, params, q.id, target);
                return slot;
            };

            // -- Campaign --------------------------------------------------
            for (int hi : held)
            {
                // BL-835 — A HUB WITH NO ARMY STAGES NOTHING. Hoisted above the
                // target loop rather than repeated inside it: whether this
                // holding has a force to march is a property of the holding,
                // and the execute path filters staging holdings by the same
                // test, so the scorer cannot offer a campaign execute would
                // then refuse to launch.
                const int64_t hub_army = gather_army(hi, /*commit=*/false);
                if (hub_army <= 0) continue;

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
                    // BL-893: THE FOURTH ESCAPE -- weight of the land behind
                    // the attacker. A wet contact is legal if the staging
                    // region carries strictly more population than the target.
                    // The force still does not forage (`forages` below is
                    // `dry || shore`), so it crosses hungry exactly as a
                    // ship-carried one does.
                    const bool heavier =
                        params.amphibious_weight_crossing
                     && ss.regions[static_cast<std::size_t>(hi)].population
                            > ss.regions[ti].population;
                    if (!dry && !shore && !heavier
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
                    //
                    // BL-835 — IT READS THE ARMY STANDING ON THE REGION. This
                    // was `tgt.manpower_stock * levy_fraction_q`, i.e. a share
                    // of the target's recruitable CIVILIANS, which is how a
                    // region emptied of people became permanently undefendable.
                    // The question the scorer asks is now "what is standing
                    // there", and its answer changes from year to year.
                    // ...AND IT INCLUDES THE LEVY THE TARGET WILL CALL UP. The
                    // execute path musters the defender to its garrison target
                    // out of its manpower pool before the fight, so a scorer
                    // reading the bare `army_stock` would be pricing a province
                    // it can never actually meet — this file's standing thesis
                    // again, a cost authored on one scale and paid on another.
                    // The two lines below are `muster_garrison`'s arithmetic
                    // asked as a question instead of applied as a mutation.
                    const int64_t def_target = garrison_target(tgt, params.garrison_fraction_q);
                    int64_t def_men = tgt.army_stock;
                    if (def_men < def_target)
                    {
                        const int64_t gap  = def_target - def_men;
                        const int64_t want = (gap * clampi(params.defence_levy_q, 0, 1000)) / 1000;
                        def_men += std::min(want, tgt.manpower_stock);
                    }
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
                    // BL-899 — and a crossing that cannot forage may still land
                    // on the reduced ration its people's creed earned. Same
                    // `campaign_supply`, scaled; never a bonus added on top.
                    const int sl_ration = forages ? 0 : sea_legs_ration(hi);
                    const int supply_here =
                        forages ? campaign_supply(hub_dist, ti, hi)
                                : (sl_ration > 0
                                       ? (campaign_supply(hub_dist, ti, hi) * sl_ration) / 1000
                                       : 0);

                    // BL-837 — REACH GATES A CAMPAIGN; IT DOES NOT MERELY
                    // PRICE IT (Ben, 2026-09-09 elicitation). Everything
                    // above already prices distance and terrain into
                    // `supply_here`; what was missing is that pricing alone
                    // never actually STOPS a rich-enough polity, it only
                    // discourages it. Applied only when the force forages at
                    // all — a `!forages` crossing is the water/forage rule's
                    // business (MILITARY_HISTORY.md § Forage) and stays
                    // priced at zero rather than gated a second time by an
                    // unrelated rule.
                    //
                    // NOT A CANDIDATE, exactly like the water-legality gate
                    // above: `continue` before anything scores, counted
                    // separately from `illegal_campaigns` so the two refusals
                    // — no legal line at all, versus a legal line too far to
                    // supply — stay distinguishable in the trace.
                    if (forages && supply_here <= params.sustainable_campaign_floor_q)
                    {
                        ++out.reach_denied_campaigns;
                        continue;
                    }

                    // BL-835 — THE ARMY AT THE HUB IT WOULD ACTUALLY MARCH FROM.
                    //
                    // This was the LARGEST manpower stock anywhere in the realm,
                    // which was wrong twice over and is now wrong zero times.
                    // It read a civilian pool rather than an army, and it read
                    // it in a region the campaign would never stage from — the
                    // execute path marches from the nearest legal holding, and
                    // `hi` IS that holding for this candidate. A polity now
                    // scores the force it can actually put on the objective,
                    // which is the sharpened question the item asks for: not
                    // "can I take this" but "can I get an army there".
                    const int64_t atk_men = hub_army;
                    const int64_t atk_est = (atk_men * supply_here / 1000)
                                          * clampi(q.cohesion_q, 1, 1000) / 1000;
                    const int64_t def_est = def_men > 0 ? def_men : 1;
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

                        // OPPOSITION SCALES THE WEIGHT, PER PAIR (BL-870;
                        // CIVILISATION.md § Culture relations). `w_cult` stays
                        // exactly the kind of number it always was — a
                        // per-mille discount on foreign ground — this only
                        // asks a queryable opposition value
                        // (`culture_opposition_q`) to set it per attacker/
                        // target pair instead of once for the whole world.
                        // OPPOSITION PERMITS CONQUEST, IT DOES NOT SCORE IT
                        // (Ben, 2026-09-09): one scorer term, `w_cult` itself,
                        // reads a richer input rather than a second term
                        // being added beside it.
                        //
                        // OPPOSITION *PERMITS* THE DISCOUNT TO BE WAIVED, it
                        // does not add to it (fixed 2026-09-10 — the first
                        // cut of this line had the sense of `op_q` backwards,
                        // making an OPPOSED pair pay MORE of w_cult and a KIN
                        // pair pay LESS, which is the opposite of "opposition
                        // permits conquest": a fresh, amicable split should
                        // still read as close to home soil, while genuine
                        // enemies should find that foreignness alone no
                        // longer holds them back). So the weight is anchored
                        // at op_q == 500 exactly as before, but runs the
                        // OTHER way: kin/like-minded pairs (op_q toward 0)
                        // pay up to five-thirds of the flat w_cult discount,
                        // opposed strangers (op_q toward 1000) nearly waive
                        // it. Falls back to the flat weight with no creeds
                        // supplied (every harness fixture in this repo that
                        // isolates one mechanism passes `cs == nullptr` on
                        // purpose; only the real generation path and BL-870's
                        // own case feed a `creed_state` in here).
                        int cult_w = params.w_cult;
                        if (cs != nullptr)
                        {
                            const int op_q = culture_opposition_q(
                                cs->cultures, q.culture, tgt.culture.plurality());
                            cult_w = clampi((params.w_cult * (1250 - op_q)) / 750, 0, 1000);
                        }
                        const int cult_q =
                            (clampi(cult_w, 0, 1000) * clampi(foreign_q, 0, 1000)) / 1000;
                        value = (value * (1000 - cult_q)) / 1000;
                    }

                    // BL-838 -- FEAR OF BEING NEXT, leaning the prize upward
                    // in proportion to what this target has done to peoples
                    // like the one deciding. Same idiom as `w_cult`, `w_dist`
                    // and `w_aggr_q`: one term reads a richer input instead of
                    // a second term being added beside the score.
                    //
                    // ONE-SIDED AND APPLIED ONCE. One-sided because a grudge
                    // ledger's neutral is the ABSENCE of a record and there is
                    // no "fewer than no wrongs done" to lean the other way --
                    // so a blameless target scores exactly what it scored
                    // before this term existed, which is what "a large but
                    // peaceful polity attracts no coalition" has to mean.
                    // Applied once, HERE rather than inside the season loop,
                    // because both seasons score the same objective against the
                    // same ledger and the fear is a property of the target, not
                    // of the campaigning weather.
                    // BL-839: the turbulence lean SCALES this weight; it never
                    // replaces or introduces it, so a run with w_fear_q at 0 --
                    // the struct default and every isolating fixture -- is
                    // untouched at every setting.
                    const int w_fear_leaned = leaned_w_fear_q(params);
                    if (w_fear_leaned != 0)
                    {
                        const int fear_q = fear_here(to);
                        if (fear_q > 0)
                        {
                            const int lean = (w_fear_leaned * fear_q) / 1000;
                            value = value + (value * lean) / 1000;
                            if (value < 0) value = 0;
                            ++out.fear_leaned_campaigns;
                            const uint16_t tid = static_cast<uint16_t>(to);
                            const auto fit = std::lower_bound(
                                out.fear_targets_seen.begin(),
                                out.fear_targets_seen.end(), tid);
                            if (fit == out.fear_targets_seen.end() || *fit != tid)
                                out.fear_targets_seen.insert(fit, tid);
                            out.fear_targets_distinct =
                                static_cast<int64_t>(out.fear_targets_seen.size());
                        }
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

                        // BL-868 -- THE CREED'S APPETITE, leaning the value the
                        // score is built from. Symmetric around a neutral 500 so
                        // a peaceable people is discouraged exactly as much as a
                        // warlike one is encouraged, and proportional so it
                        // scales with the prize rather than swamping it. Applied
                        // to `value` for the same reason w_cult is: one term
                        // reads a richer input instead of a second term being
                        // added beside the score.
                        if (params.w_aggr_q != 0)
                        {
                            const int lean =
                                (params.w_aggr_q * (clampi(q.aggression_q, 0, 1000) - 500)) / 500;
                            value = value + (value * lean) / 1000;
                            if (value < 0) value = 0;
                        }
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
                    // RE-SETTLEMENT ONLY (BL-894). In the Empires round Settle
                    // refounds ground somebody emptied; it does not manufacture
                    // new ground on population pressure alone. `centres_razed`
                    // is the record of that harm and it PERSISTS -- a razed city
                    // later rebuilt still says it was razed (settlement.cpp
                    // sack_region_urban) -- so this reads "has this ground ever
                    // been sacked", not "is it empty right now".
                    if (params.settle_requires_razed_ground && p.centres_razed <= 0) continue;
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
                    // BL-835 — and it must have an ARMY, by the same test the
                    // scorer used. Nearest-with-a-force, not merely nearest:
                    // without this, execute could stage from a bare region the
                    // scorer had already skipped and the round would fall
                    // through having chosen a verb it could not perform.
                    if (ss.regions[hs].army_stock <= 0) continue;
                    if (!dry_contact(hi, ti) && !tgt_shore
                     && !can_field_naval(ss.regions[hs], mil_band))
                        continue;
                    const int d = region_distance(ss.regions[hs], tgt, gw);
                    if (d < src_d) { src_d = d; src = hi; }
                }
                if (src < 0) break; // no legal staging holding — nothing marches
                region& home = ss.regions[static_cast<std::size_t>(src)];

                // BL-835 — THE FIELD ARMY MARCHES, AND IT LEAVES. The staging
                // holding's whole garrison goes; `home.army_stock` is zero
                // until the survivors come back, which is a real and visible
                // cost rather than a bookkeeping one — a polity that campaigns
                // out of a frontier march has just uncovered that march, and
                // its neighbours' scorers can see it.
                //
                // This is the line that used to read
                // `raise_manpower(home, manpower_stock * levy_fraction_q)`:
                // an army conjured out of civilians at the moment of battle
                // and never seen again, which is precisely the accounting that
                // let war eat the population.
                const int64_t raised = gather_army(src, /*commit=*/true);
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
                // BL-899 — SEA LEGS, asked of the staging holding execute
                // actually chose, by the identical lambda the scorer used.
                const int exec_ration = exec_forages ? 0 : sea_legs_ration(src);
                const int atk_supply =
                    exec_forages ? campaign_supply(src_d, ti, src)
                                 : (exec_ration > 0
                                        ? (campaign_supply(src_d, ti, src) * exec_ration) / 1000
                                        : 0);
                const int def_supply = 1000;
                if (!exec_forages)
                {
                    // One or the other, never both: a crossing is fed on the
                    // reduced ration or it starves as it did before.
                    if (exec_ration > 0) ++out.sea_legs_fed_campaigns;
                    else                 ++out.starved_campaigns;
                }

                // BL-768 — THE SUPPLY CORRIDOR, recorded where it is priced.
                // `src` is the staging holding the army victualled from and
                // `ti` the objective it marched on, so this pair is literally
                // the line supply moved along, taken from the two indices
                // `campaign_supply` was just handed rather than reconstructed.
                // Recorded on LAUNCH, not on victory: the road was walked
                // whether or not the battle was won, and a network that only
                // remembered the winners would be a map of conquests rather
                // than a map of routes.
                note_corridor(src, static_cast<int>(ti), q.capital);

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
                // BL-867 — MATERIALS ARE SPENT WHEN THE ACTION HAPPENS
                // (CIVILISATION.md § Materials are spent when something
                // happens). Drawn from the acting polity's OWN seat — never
                // the staging holding, which may be a bare frontier march
                // with no stock of its own, because the design puts the
                // stores at the realm's capital and nowhere else. SPENT, not
                // borrowed: a seat with nothing standing in it pays what it
                // can and the campaign marches anyway, understocked.
                int64_t material_cost = 0, material_spent = 0;
                int material_penalty_q = 0;
                if (q.capital >= 0 && static_cast<std::size_t>(q.capital) < ss.regions.size())
                {
                    region& seat = ss.regions[static_cast<std::size_t>(q.capital)];
                    // FLOORED AT 1, NOT TRUNCATED TO 0 (found by M2 against a
                    // real generated world, 2026-09-10): integer division
                    // rounds `raised * campaign_material_cost_per_head_q /
                    // 1000` down to zero for any force under 250 heads at the
                    // default coefficient, which is most real campaigns —
                    // the cost silently vanished while materials still
                    // accumulated, so nothing was ever visibly spent.
                    material_cost  = raised > 0
                                    ? std::max<int64_t>(
                                          1, (raised * params.campaign_material_cost_per_head_q) / 1000)
                                    : 0;
                    material_spent = std::min(material_cost, seat.material_stock);
                    seat.material_stock -= material_spent;
                    out.materials_spent_on_campaigns += material_spent;

                    // THE SHORTFALL IS WHAT MAKES OVER-MUSTER MEASURABLY
                    // WORSE OFF. A polity whose garrisons have been eating its
                    // own industry (`region_industry_capacity`, spent every
                    // year above) arrives here with an empty seat, and the
                    // army that marches anyway is visibly weaker for it —
                    // through the SAME readiness channel works and winter
                    // already share, so it tilts a fight rather than deciding
                    // one, exactly as those two do.
                    if (material_cost > 0)
                    {
                        const int covered_q = clampi(
                            static_cast<int>((material_spent * 1000) / material_cost), 0, 1000);
                        material_penalty_q =
                            ((1000 - covered_q) * params.material_shortfall_penalty_q) / 1000;
                    }
                }
                const int atk_ready = clampi(1000 - material_penalty_q, 0, 1000);

                const int def_works_q = clampi(tgt.work_defence_mod, 0, 1000);
                const int def_ready = (best_winter ? (1000 - params.winter_readiness_penalty_q) : 1000)
                                    + def_works_q;

                const polity* dq = nullptr;
                for (const polity& o : out.polities)
                    if (o.id == owner[ti]) { dq = &o; break; }

                roster_band atk_band = roster_band::classical;
                std::vector<army_stack_entry> atk =
                    build_stack(raised, home, q, atk_ready, sim_band_ceiling(params, y),
                                !exec_dry, &atk_band);
                note_units_fielded(out, params, y, atk_band, atk);
                // BL-835 — THE EMERGENCY LEVY. A province being invaded calls
                // up its own people, now, rather than at the 25%-a-year pace of
                // the peacetime muster: `muster_rate_q` of 1000 fills the
                // garrison to its target in one act, BOUNDED BY THE REGION'S
                // OWN MANPOWER POOL, which is what a levy actually is.
                //
                // IT IS THE MECHANISM THAT MAKES GROUND EXPENSIVE TO TAKE
                // TWICE, and it was measured into existence. Without it the
                // attacker's survivors marched home intact while the defender
                // rebuilt at a quarter of a quarter, so the second battle on any
                // frontier was a walkover: the synthetic fixture fell in two
                // battles where it used to grind for sixty-six, and four of
                // eight real seeds showed every single battle ending in a
                // conquest.
                //
                // AND THE POOL IS FINITE, which is the other half. A frontier
                // attacked year after year empties its manpower and then really
                // is defenceless — honestly, temporarily, and with a cause you
                // can point at. That is BL-308's "a ground-down frontier
                // eventually gives", arriving through exhaustion rather than
                // through the transfer bar alone.
                muster_garrison(tgt, params.garrison_fraction_q,
                                params.defence_levy_q, params.garrison_disband_q);

                // THE DEFENDER FIELDS WHAT IS STANDING THERE, all of
                // it. There is no sense in a garrison holding half itself back
                // from the fight on the ground it is garrisoning, and reading
                // the whole pool keeps the two sides symmetric: attacker and
                // defender each commit one region's field army, which is the
                // same 1:1 shape the old `levy_fraction_q` of `manpower_stock`
                // gave both of them.
                const int64_t def_men = tgt.army_stock;
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

                // BL-835 — THE BATTLE IS SPENT ON THE ARMIES, AND ONLY ON THEM.
                //
                // These two lines were `home.population -= atk_lost / 4` and
                // `tgt.population -= def_lost / 4`: a battle killed civilians in
                // both regions, on both sides, whoever won, and the arbitrary
                // /4 is the tell that nobody could say what the quantity meant.
                // The soldiers now come out of the pool the soldiers are in.
                //
                // `atk_survivors` is carried rather than spent here because
                // where it ends up depends on the outcome — see the conquest
                // block below. The defender's remnant is settled immediately:
                // it is standing where it fought.
                const int64_t atk_survivors = clampi64(raised - atk_lost, 0, raised);
                tgt.army_stock = clampi64(def_men - def_lost, 0, def_men);

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

                    // BL-835 — THE COUNTRYSIDE KEEPS ITS PEOPLE. The line that
                    // stood here took `sack_population_loss_q` (22%) off
                    // `tgt.population` on every conquest, and 258 conquests of
                    // one region is how a province ends a 4000-year run with
                    // nobody living on it. Ben's ruling is that population is
                    // civilian and war does not consume it; a conquest is now a
                    // change of flag over the same people, which is also what
                    // gives BL-826's culture shares a subject to assimilate.
                    //
                    // BL-766: and the sack still falls on the walls. This is the
                    // one place history DESTROYS a centre rather than thinning
                    // it, so a sacked city reads as a smaller or absent centre
                    // on the epoch map.
                    // BL-887: a razed city is a relay REMOVED from the
                    // network, the mirror of the demography loop's bump.
                    const int sacked_centres_before = tgt.centres;
                    sack_region_urban(tgt, params.sack_population_loss_q);
                    if (tgt.centres != sacked_centres_before) ++centres_version;

                    // BL-835 — THE ARMY THAT TOOK IT IS THE ARMY THAT HOLDS IT,
                    // and this is the line that breaks the ping-pong.
                    //
                    // The defeated garrison is dispersed with the province it
                    // was defending, and the victors garrison the ground they
                    // just walked onto. So a region that has just changed hands
                    // is the BEST-defended region on that frontier, not the
                    // worst — where before it was left bare, and therefore the
                    // top-scoring objective for the polity that had just lost
                    // it, on the very next round, for the rest of the run.
                    //
                    // The cost is paid at the other end: `home` is now empty
                    // and re-mustering from its own people, so a conqueror is
                    // holding a forward province with an uncovered rear. That
                    // is question A in the shape the item asks for — not "can I
                    // take this" but "can I keep an army there".
                    tgt.army_stock = atk_survivors;

                    const int loser_id = dq ? dq->id : -1;
                    const bool was_seat =
                        dq && dq->capital == static_cast<int>(ti);

                    owner[ti]  = q.id;
                    tgt.nation = q.id;

                    // BL-866 — GROUND IS NOT CONQUERED REGION BY REGION.
                    // Taking a SEAT takes every region that points at it, in
                    // this same event (CIVILISATION.md § The unit is the city
                    // state). `seat_region` is left unchanged on every one of
                    // them: `ti` is still a seat, only its flag moved, the
                    // same way `founding_culture` survives a conquest below.
                    //
                    // A region taken on ITS OWN — decoupled from a seat it no
                    // longer shares an owner with — re-points at the
                    // conqueror's own seat, or falls outside anyone's reach
                    // if the conqueror somehow holds none (never observed:
                    // every living polity's `capital` is a valid seat by
                    // construction, above and at the reassignment site).
                    if (tgt.is_seat)
                    {
                        for (std::size_t hi = 0; hi < ss.regions.size(); ++hi)
                        {
                            if (hi == ti) continue;
                            region& h = ss.regions[hi];
                            if (h.seat_region != static_cast<int>(ti)) continue;
                            owner[hi] = q.id;
                            h.nation  = q.id;
                        }
                    }
                    else
                    {
                        tgt.seat_region = (q.capital >= 0) ? q.capital : -1;
                    }

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

                // BL-835 — AND IF IT DID NOT TAKE THE GROUND, THE ARMY MARCHES
                // HOME. Placed after the whole if/else rather than inside the
                // `else if (dq)` arm, because a repulsed army comes back
                // whether or not the defender had a polity to be resented by.
                //
                // THIS IS WHY AN ARMY IS A POOL AND NOT AN EXPENDITURE. Under
                // the old accounting every campaign consumed its levy outright,
                // so a polity's capacity to fight was set entirely by how fast
                // civilians regrew — war was a demographic process wearing a
                // military costume. Survivors returning make a standing army a
                // thing a polity HAS, which can be worn down over a campaign
                // season and rebuilt over a generation, and which can be
                // somewhere other than where it is needed.
                // The whole column comes back to the STAGING hub rather than
                // dispersing to the holdings it was drawn from. Deliberate: an
                // army that fought together is standing together, and the next
                // round's `gather_army` will redistribute it through the same
                // half-share rule if it marches again.
                if (!takes_it)
                    home.army_stock = clampi64(home.army_stock + atk_survivors,
                                               0, 1LL << 40);
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
                // BL-867 — ORDINARY HINTERLAND, POINTING AT THE FOUNDER'S
                // SEAT. `q` already exists and already has a capital (it is
                // the polity performing this verb), so a region it settles
                // is never itself a new seat — the sparse first cut names
                // that "one seat per polity", and a Settle never mints one.
                np.seat_region = q.capital;
                np.population = clampi64(region_carrying_capacity(np.farm_q) / 16, 1, 1 << 30);
                replenish_manpower(np);
                // BL-835 — A NEW REGION HAS NO ARMY YET, and that is the
                // correct opening state rather than an oversight: a frontier
                // settlement musters its garrison over its first decades like
                // every other region, out of the people who walked there. The
                // world-opening seed above is the exception, and it is one
                // because a run cannot begin mid-muster.
                np.army_stock = 0;
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
                neighbours.emplace_back();
                degree.push_back(0);
                link_region(ss.regions.size() - 1); // Keep the index complete.

                // BL-768 — the road a founding party walked. The daughter is
                // reached FROM its parent and supplied from there until it can
                // feed itself, so (parent, daughter) is the second and by far
                // the commoner of the two corridor sources: a polity settles far
                // more often than it campaigns, which is what gives a peaceful
                // history a road network at all.
                note_corridor(best_target, static_cast<int>(ss.regions.size()) - 1, q.capital);
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

        // ---- BL-896 - GROUND THE REALM CANNOT REACH SECEDES ---------------
        //
        // Ben's ruling, 2026-09-11: collapse is NETWORK FAILURE. Succession,
        // exhaustion and external shock were all offered as causes and none was
        // chosen, so this is the settled cause rather than the likely one, and
        // CIVILISATION.md's incumbent reading ("reach-gating is what makes the
        // collapse mechanical rather than scripted... never a collapse event
        // fired at a date") is promoted from a likelihood to a rule.
        //
        // THE NETWORK IS REACH, NOT THE ROAD GRAPH (ruling 1, same session).
        // `region::network_supply_q` was computed above for every held region
        // this round, off the same terrain-and-road Dijkstra `rebuild_reach`
        // already discounts by the corridors this history has actually walked.
        // So the instrument is dense (every held region, every round) where the
        // road graph is sparse (about fifty promoted edges per world), and no
        // road-density fix is needed first.
        //
        // AFTER THE DECISION LOOP, NEVER INSIDE IT. A successor is a
        // `push_back` onto `out.polities`, which invalidates the `polity&` that
        // loop holds. Everything below therefore indexes rather than
        // references.
        //
        // DETERMINISM IS THE REAL CONSTRAINT HERE and every choice below is
        // made for it: polities are walked in id order, regions in index order,
        // blocks grow by a breadth-first walk seeded from the lowest index in
        // the block, and successors are allocated in the order those blocks are
        // found. Nothing below reads a container whose order is undefined.
        if (params.secession_supply_floor_q > 0)
        {
            // Snapshot the count: a successor born this round does not itself
            // secede in its own birth round. It has had no round to be measured
            // and the reading on its ground is still its parent's.
            const std::size_t pol_count = out.polities.size();
            for (std::size_t pi = 0; pi < pol_count; ++pi)
            {
                if (!out.polities[pi].alive) continue;
                const int qid  = out.polities[pi].id;
                const int qcap = out.polities[pi].capital;

                // The cut-off set, in region-index order. The capital is never
                // in it: a seat is by construction at zero distance from itself
                // and cannot secede from the realm it IS.
                std::vector<int> cut;
                for (std::size_t i = 0; i < owner.size(); ++i)
                {
                    if (owner[i] != qid) continue;
                    if (static_cast<int>(i) == qcap) continue;
                    if (ss.regions[i].network_supply_q
                        <= params.secession_supply_floor_q)
                        cut.push_back(static_cast<int>(i));
                }
                if (static_cast<int>(cut.size()) < params.secession_min_regions) continue;

                std::vector<char> taken(cut.size(), 0);
                for (std::size_t ci = 0; ci < cut.size(); ++ci)
                {
                    if (taken[ci]) continue;

                    // A CONTIGUOUS BLOCK LEAVES TOGETHER. One region leaving
                    // alone shatters a realm into specks; a cut-off block
                    // leaving as one SPLITS it, and a split is what produces
                    // the nations of unequal strength the dark age is asked to
                    // hand forward.
                    std::vector<int> block;
                    std::vector<int> queue{cut[ci]};
                    taken[ci] = 1;
                    for (std::size_t qi = 0; qi < queue.size(); ++qi)
                    {
                        const int r = queue[qi];
                        block.push_back(r);
                        for (int nb : neighbours[static_cast<std::size_t>(r)])
                            for (std::size_t cj = 0; cj < cut.size(); ++cj)
                                if (!taken[cj] && cut[cj] == nb)
                                { taken[cj] = 1; queue.push_back(nb); }
                    }
                    if (static_cast<int>(block.size()) < params.secession_min_regions)
                        continue;
                    std::sort(block.begin(), block.end());

                    // The successor's seat is the block's lowest index -
                    // placement order, which is best-ground first: the same
                    // rule the capital-fell branch above already uses.
                    const int seat = block.front();
                    if (out.polities.size() >= owner_index_limit) break;

                    polity np;
                    np.id = static_cast<int>(out.polities.size());
                    // THE GROUND'S OWN PEOPLE, not the parent's. A province
                    // that walks away is its own plurality's realm, which is
                    // what makes secession carry culture forward rather than
                    // clone the empire that lost it.
                    np.culture  = ss.regions[static_cast<std::size_t>(seat)].culture.plurality();
                    np.capital  = seat;
                    np.aggression_q =
                        (cs && np.culture >= 0
                         && np.culture < static_cast<int>(cs->cultures.size()))
                            ? leaned_aggression_q(
                                  params, cs->cultures[static_cast<std::size_t>(np.culture)].aggression_q)
                            : 500; // BL-839: same lean as the founding read above.
                    // INSTITUTIONS ARE INHERITED, not reset. A province of an
                    // empire knows what the empire knew, and that is the whole
                    // reason a dark age leaves unequal nations rather than a
                    // blank map. Cohesion is inherited for the same reason and
                    // so that no new dial is invented for this event.
                    for (int d = 0; d < sim_domain_count; ++d)
                    {
                        np.capacity[d]   = out.polities[pi].capacity[d];
                        np.progress_q[d] = out.polities[pi].progress_q[d];
                    }
                    np.cohesion_q      = out.polities[pi].cohesion_q;
                    np.industrial_year = out.polities[pi].industrial_year;
                    out.polities.push_back(np);

                    for (int r : block)
                    {
                        const std::size_t ri = static_cast<std::size_t>(r);
                        owner[ri]                  = np.id;
                        ss.regions[ri].nation      = np.id;
                        ss.regions[ri].seat_region = seat;
                        ss.regions[ri].is_seat     = (r == seat);
                        out.owner_changes.push_back(owner_change{
                            static_cast<int32_t>(y),
                            static_cast<uint16_t>(r),
                            static_cast<uint16_t>(np.id)});
                    }

                    // A SECESSION IS A DEFEAT FOR THE PARENT, through the same
                    // cohesion channel every other loss uses - never a term
                    // invented for this event.
                    out.polities[pi].cohesion_q = clampi(
                        out.polities[pi].cohesion_q - params.cohesion_loss_on_defeat_q,
                        params.cohesion_floor_q, 1000);

                    // AND IT LEAVES A GRUDGE, carried today and inert today
                    // (BL-898 owns making it bite). `ground_taken` rather than
                    // a fifth kind: ground did change hands, and that is the
                    // honest cause to tell a player.
                    raise_grudge(qid, np.id, grudge_kind::ground_taken,
                                 seat, y, params.grudge_ground_taken);

                    ++out.secessions;
                    out.regions_seceded += static_cast<int64_t>(block.size());
                    out.history.push_back(history_event{
                        years_from_calendar_year(y), chain_stage::legacy,
                        ss.regions[static_cast<std::size_t>(seat)].name
                            + " breaks away, beyond its seat's reach",
                        std::string{}});
                }
            }
        }

        // ---- BL-897 - A CREED THAT SPANS CULTURES -------------------------
        //
        // ../../docs/lore/CREEDS.md sec The four calls, settled (Ben,
        // 2026-09-11). Every creed in this codebase before this block is LOCAL
        // BY CONSTRUCTION: a pantheon belongs to a cradle culture and travels
        // only as that people travels. This block builds the one that does not
        // -- it is coined new, it belongs to nobody, and it spreads along
        // CONTACT rather than ancestry.
        //
        // SITED AFTER THE SECESSION BLOCK, DELIBERATELY. Humiliation is the
        // first of the two causes, and BL-896's fragmentation is the largest
        // producer of it: a realm that has just lost a block of ground took its
        // cohesion hit a few lines above. Reading cohesion before that would
        // read last round's realm.
        //
        // NOTHING HERE ROLLS. The only stream touched is `salt`, used to coin a
        // phonology -- the same hash-as-picker idiom `coin_civilisation_name`
        // uses, and for the same stated reason (this file consumes no RNG).
        // Every gate below is a comparison against a scalar the round already
        // computed, and every walk below is in index order.
        if (params.universal_creed_humbled_cohesion_q > 0)
        {
            // How busy and how reachable one realm's network is, counted the
            // same way BL-895 pays for it: a walked corridor between two held
            // regions that hold UNLIKE ground. Recomputed on demand rather than
            // accumulated in the demography pass, so this item adds nothing to
            // the hot loop and touches none of BL-895's code.
            const auto realm_network = [&](int qid, int& links, int& mean_supply_q) {
                links = 0;
                int64_t sum = 0, held = 0;
                const std::size_t n = std::min(owner.size(), ss.regions.size());
                for (std::size_t i = 0; i < n; ++i)
                {
                    if (owner[i] != qid) continue;
                    ++held;
                    sum += ss.regions[i].network_supply_q;
                    for (int nb : neighbours[i])
                    {
                        if (nb <= static_cast<int>(i)) continue;   // count the pair once
                        const std::size_t ni = static_cast<std::size_t>(nb);
                        if (ni >= n || owner[ni] != owner[i]) continue;
                        if (road_uses_live.find(edge_key(static_cast<int>(i), nb))
                            == road_uses_live.end()) continue;
                        if (region_trade_class(ss.regions[i])
                         == region_trade_class(ss.regions[ni])) continue;
                        ++links;
                    }
                }
                mean_supply_q = held > 0 ? static_cast<int>(sum / held) : 0;
            };

            // ---- (a) IT ARISES, from humiliation and from density ---------
            //
            // Both are required and neither is sufficient. A shattered realm in
            // an empty world has nobody to offer an answer for everyone TO; a
            // rich, dense, intact realm has not been asked the question.
            const std::size_t pol_count_arise = out.polities.size();
            for (std::size_t pi = 0; pi < pol_count_arise; ++pi)
            {
                if (!out.polities[pi].alive) continue;
                if (out.polities[pi].universal_creed >= 0) continue;
                if (out.polities[pi].cohesion_q
                    > params.universal_creed_humbled_cohesion_q) continue;

                int links = 0, mean_q = 0;
                realm_network(out.polities[pi].id, links, mean_q);
                if (links < params.universal_creed_min_trade_links) continue;
                if (mean_q < params.universal_creed_network_floor_q) continue;

                // IT IS COINED NEW AND BELONGS TO NOBODY. A phonology rolled
                // for this creed alone -- NOT drawn from the founding realm's
                // culture, which would hand it a cradle by the back door and
                // make every other people read its spread as that people's
                // spread. The stream is the run seed crossed with the realm and
                // the year, so the same world coins the same creed on replay
                // and two realms never coin the same name.
                hash_picker r{salt(salt(seed, static_cast<uint32_t>(out.polities[pi].id)),
                                   static_cast<uint32_t>(y & 0xFFFF))};
                universal_creed uc;
                uc.speech = roll_tongue(r);
                const int syllables = 2 + r.pick(2);
                uc.name = tongue_word(r, uc.speech, syllables);
                if (uc.name.empty()) continue; // Unusable inventory; should not occur.
                uc.founded_year  = y;
                uc.origin_polity = out.polities[pi].id;
                uc.origin_region = out.polities[pi].capital;

                const int ci = static_cast<int>(out.universal_creeds.size());
                const std::string creed_name = uc.name;
                out.universal_creeds.push_back(std::move(uc));
                ++out.universal_creeds_arisen;

                // The realm that coined it adopts it in the same breath: the
                // institution IS where it arose. Its peoples still convert
                // unevenly afterwards, exactly like anyone else's.
                out.polities[pi].universal_creed    = ci;
                out.polities[pi].creed_adopted_year = y;
                ++out.polities_adopted_creed;

                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    creed_name + " is preached in a broken realm, and claims every people",
                    std::string{}});
            }

            // ---- (b) THE POLITY ADOPTS IT, ALONG CONTACT ------------------
            //
            // Not along ancestry, which is the whole departure. A realm adopts
            // where its ground touches ground that already holds the creed
            // ACROSS A WALKED CORRIDOR -- the same contact test trade uses,
            // because a route people walk is how anything crosses between two
            // peoples in this phase. Kinship is not consulted: binding two
            // peoples who are NOT kin is the job CREEDS.md gives this
            // mechanism, and a kinship gate would quietly undo it.
            //
            // FIRST MATCH IN REGION-INDEX ORDER WINS, so a realm touching two
            // creeds takes one deterministically.
            const std::size_t pol_count_adopt = out.polities.size();
            const std::size_t n_reg_creed     = std::min(owner.size(), ss.regions.size());
            for (std::size_t pi = 0; pi < pol_count_adopt; ++pi)
            {
                if (!out.polities[pi].alive) continue;
                if (out.polities[pi].universal_creed >= 0) continue;
                const int qid = out.polities[pi].id;

                int found = -1;
                for (std::size_t i = 0; i < n_reg_creed && found < 0; ++i)
                {
                    if (owner[i] != qid) continue;
                    for (int nb : neighbours[i])
                    {
                        const std::size_t ni = static_cast<std::size_t>(nb);
                        if (ni >= n_reg_creed) continue;
                        if (owner[ni] == owner_none || owner[ni] == owner[i]) continue;
                        const int oc = ss.regions[ni].universal_creed;
                        const int8_t h = ss.regions[ni].creed_hold;
                        if (oc < 0 || (h != 1 && h != 2)) continue;
                        if (road_uses_live.find(edge_key(static_cast<int>(i), nb))
                            == road_uses_live.end()) continue;
                        found = oc;
                        break;
                    }
                }
                if (found < 0) continue;
                out.polities[pi].universal_creed    = found;
                out.polities[pi].creed_adopted_year = y;
                ++out.polities_adopted_creed;
            }

            // ---- (c) ITS PEOPLES CONVERT UNEVENLY AFTER -------------------
            //
            // The second grain, and it is EXPECTED TO DISAGREE with the first.
            // Ground under a realm that has adopted enters DUAL-HOLD, carrying
            // its pantheon and the creed at once; after
            // `universal_creed_hold_years` the pair settles, and which way it
            // settles is read off how bound that ground is to the realm --
            // distance through `network_supply_q`, difference through the alien
            // penalty. Neither is a roll and neither is a date.
            //
            // A REASSERTED PEOPLE IS NOT RE-OFFERED. It answered. Leaving it
            // answered is what makes the fault line a STABLE feature of the map
            // rather than a shimmer, which is what a later schism slice needs
            // to cut along.
            for (std::size_t i = 0; i < n_reg_creed; ++i)
            {
                if (owner[i] == owner_none) continue;
                const std::size_t oi = static_cast<std::size_t>(owner[i]);
                if (oi >= out.polities.size()) continue;
                const int pc = out.polities[oi].universal_creed;
                if (pc < 0) continue;

                region& rg = ss.regions[i];
                if (rg.creed_hold == 0)
                {
                    // THE PANTHEON UNDERNEATH is recorded the moment the creed
                    // arrives, not when it is subsumed: the residue must be the
                    // creed this ground actually held at contact, and
                    // assimilation moves `culture` afterwards.
                    rg.creed_hold            = 1;
                    rg.universal_creed       = pc;
                    rg.creed_residue_culture = rg.culture.plurality();
                    rg.creed_hold_years      = 0;
                    continue;
                }
                if (rg.creed_hold != 1) continue;

                rg.creed_hold_years += step_years;
                if (rg.creed_hold_years < params.universal_creed_hold_years) continue;

                const int  own_culture = out.polities[oi].culture;
                const bool alien       = own_culture >= 0
                                      && rg.culture.plurality() != own_culture;
                const int  bind_q = rg.network_supply_q
                                  - (alien ? params.universal_creed_alien_penalty_q : 0);
                if (bind_q >= params.universal_creed_convert_supply_q)
                {
                    rg.creed_hold = 2; // The pantheon fades to residue.
                    ++out.peoples_converted;
                }
                else
                {
                    // IT REASSERTS. The people falls back out of the creed and
                    // the residue is KEPT -- the record of a creed that reached
                    // this ground and lost it is exactly what a schism is made
                    // of, and it outlives the institution above it.
                    rg.creed_hold      = 3;
                    rg.universal_creed = -1;
                    ++out.peoples_reasserted;
                }
            }
        }

        // Ownership changes are appended where they happen (conquest, founding),
        // so there is nothing to snapshot at the end of a year.

        // ---- THE RECORDED STEP (BL-817) ----------------------------------
        //
        // Reached only on a decision round — the gate above `continue`s past it
        // on every other year — and then only once the interval has elapsed, so
        // the cadence is max(decision band, record_interval_years). Sampling
        // between two rounds would record the same numbers twice.
        //
        // AFTER the verbs execute, so a step shows the world the round left
        // behind rather than the one it started from.
        if (params.record_playback
            && (last_record_year == INT64_MIN || y - last_record_year >= record_interval))
            record_step(y);
    }

    // THE CLOSING STEP, always taken, at the stop year. Without it the record's
    // last step is wherever the interval happened to land — up to one interval
    // short of the epoch — and a scoreboard would show a standing that is not
    // the standing the world was handed on with.
    if (params.record_playback
        && (out.steps.empty() || out.steps.back().year != static_cast<int32_t>(params.stop_year)))
        record_step(params.stop_year);

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

// ---------------------------------------------------------------------------
// The turbulence lean, resolved (BL-839)
// ---------------------------------------------------------------------------
//
// All three clamp the lean to -1..+1 at the point of use rather than trusting
// the field, because `--set turbulence_lean=7` is one keystroke away and a
// scale of x3.8 arriving silently is exactly the "table labelled as a trial of
// a force that was never changed" failure `apply_override`'s whitelist exists
// to stop, one layer further in.

static int turbulence_scale_q(const history_sim_params& p, int magnitude_q)
{
    const int lean = clampi(p.turbulence_lean, -1, 1);
    if (lean == 0) return 1000;                       // ORDINARY IS THE IDENTITY.
    const int m = magnitude_q < 0 ? 0 : magnitude_q;
    // Floored at 0: a calm magnitude above 1000 would otherwise flip the sign
    // of the force it scales, which is a different force, not a calmer one.
    return clampi(1000 + lean * m, 0, 100000);
}

int leaned_aggression_q(const history_sim_params& p, int culture_aggression_q)
{
    const int a = clampi(culture_aggression_q, 0, 1000);
    const int s = turbulence_scale_q(p, p.turbulence_aggression_spread_q);
    if (s == 1000) return a;
    // ABOUT THE 500 NEUTRAL, which is what makes this a spread. Integer
    // division truncates toward zero on both signs, so a widening and a
    // narrowing round the same way and the neutral is an exact fixed point.
    return clampi(500 + ((a - 500) * s) / 1000, 0, 1000);
}

int leaned_w_fear_q(const history_sim_params& p)
{
    const int s = turbulence_scale_q(p, p.turbulence_fear_q);
    if (s == 1000) return p.w_fear_q;
    return static_cast<int>((static_cast<int64_t>(p.w_fear_q) * s) / 1000);
}

int leaned_terrain_reach_cost_q(const history_sim_params& p)
{
    const int s = turbulence_scale_q(p, p.turbulence_reach_cost_q);
    if (s == 1000) return p.terrain_reach_cost_q;
    const int64_t v = (static_cast<int64_t>(p.terrain_reach_cost_q) * s) / 1000;
    return v < 0 ? 0 : static_cast<int>(v);
}

int fear_of_next_q(const history_sim_state& s, const history_sim_params& p,
                   int decider, int target)
{
    if (decider < 0 || target < 0 || decider == target) return 0;
    if (decider >= static_cast<int>(s.polities.size())) return 0;
    if (target  >= static_cast<int>(s.polities.size())) return 0;
    const int kin_culture = s.polities[static_cast<std::size_t>(decider)].culture;
    if (kin_culture < 0) return 0;
    const uint16_t tid  = static_cast<uint16_t>(target);
    const uint16_t self = static_cast<uint16_t>(decider);
    int64_t total = 0;
    // The scan order is the SORTED (from, to) order of the table, so the sum is
    // a property of the integers in it and not of any container layout.
    for (const grudge& g : s.grudges)
    {
        if (g.to != tid) continue;
        if (g.from == self) continue; // THE REVENGE EXCLUSION -- the grant's, not an optimisation.
        const std::size_t fi = static_cast<std::size_t>(g.from);
        if (fi >= s.polities.size()) continue;
        const polity& kin = s.polities[fi];
        if (!kin.alive) continue;
        if (kin.culture != kin_culture) continue;
        total += g.score;
    }
    return static_cast<int>(clampi64(
        (total * 1000) / std::max(1, p.fear_reference), 0, 1000));
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
