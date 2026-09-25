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

/// BL-924 -- A CITY IS A PRIZE, A SEAT IS THE PRIZE. What a region carries
/// BEYOND bare endowment, worth reading only for CAMPAIGN's own target
/// valuation -- never folded into `region_value_q` above.
///
/// `region_value_q` alone made every region worth the same regardless of
/// what stood on it, so a capital scored no higher than an empty hamlet and
/// campaign targeting had no gradient toward a decisive objective -- the
/// 4-seed sweep showed the consequence as border churn (28% of contested
/// regions taken 3+ times). Centres and urban population are the settlement
/// itself, the accumulated work effects are the infrastructure built on top
/// of it, and `is_seat` marks the one region per polity worth the most of
/// all.
///
/// KEPT OUT OF `region_value_q` DELIBERATELY (first cut folded it in and
/// cost history_sim_harness six checks: B318c, BL384a, R5, B384c, BL837b1,
/// M2b). `region_value_q` is THE shared currency Settle, Consolidate,
/// build_work's reach gain and a polity's own holdings-value sum all read —
/// none of those verbs are about what a region is worth to ATTACK, and
/// inflating their common currency crowded Campaign out of the argmax in
/// every fixture built assuming pure endowment. Campaign's own `value` at
/// its scoring site never went through `region_value_q` in the first place
/// (it reads `w_farm`/`w_ore`/`w_port` against the target directly), so this
/// term is added there, and there only.
/// BL-973: a modifier term read for a region's OWNER (defined with the tree
/// effect surface, below the run loop, in this same unnamed namespace;
/// declared here because the campaign pricing and resolution sites inside
/// `run_history_sim` read it).
int nation_tree_mod_q(const std::vector<polity>& ps, int nation, io::tree_modifier_term t);

int campaign_prize_q(const region& p)
{
    const int centre_q = clampi(p.centres * 200, 0, 1000);
    const int urban_q  = clampi(static_cast<int>(p.urban_population / 100), 0, 1000);
    const int works_q  = clampi((p.work_capacity_mod + p.work_manpower_mod
                                + p.work_industrial_mod) / 3, 0, 1000);
    int prize_q = (centre_q + urban_q + works_q) / 3;
    // The seat premium: a capital is worth half again what a bare city of
    // the same size would be, so sacking one (BL-895 already razes its
    // centres) is worth far more than trading an ordinary border region,
    // and retaking a sacked one is worth far less -- the churn this item
    // exists to cool.
    if (p.is_seat) prize_q = clampi(prize_q + prize_q / 2, 0, 1000);
    return prize_q;
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

/// Count a fielded stack into the per-band rows.
void note_units_fielded(history_sim_state&        out,
                        roster_band               band,
                        const std::vector<army_stack_entry>& stack)
{
    int64_t n = 0;
    for (const army_stack_entry& e : stack)
        n += e.count;
    const int b = static_cast<int>(band);
    if (b < 0 || b >= roster_band_count)
        return;
    out.units_by_band[static_cast<std::size_t>(b)] += n;
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
/// There is no band ceiling (BL-1075): the superseded two-span arc capped its
/// first span at medieval, and that arc is deleted, so the band is the owner's
/// military capacity rung and nothing else.
///
/// @p allow_naval carries NR-794 (Ben, 2026-09-07): ships are composed only into
/// a stack whose campaign actually crosses water. Required rather than
/// defaulted — an omitted argument would silently re-admit fleets to land
/// battles, and nothing downstream could tell.
std::vector<army_stack_entry> build_stack(int64_t manpower,
                                          const region& home,
                                          const polity&   owner,
                                          int             readiness_q,
                                          bool            allow_naval,
                                          roster_band*    band_out)
{
    const int band_index = clampi(owner.capacity[static_cast<int>(sim_domain::military)], 1, 6);
    const roster_band band = roster_band_for_capacity(band_index);
    // BL-760 (1): the band is computed HERE and nowhere else, so the counter
    // reads the value the stack was actually built from. Re-deriving it at the
    // call site would be a second copy of the rule, free to drift from this
    // one — the divergence class this file's own header warns about.
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

// ---------------------------------------------------------------------------
// BL-931 — objects with a term, and the (still empty) upkeep step
// ---------------------------------------------------------------------------

void expire_dated_objects(std::vector<dated_object>& objects, int64_t year)
{
    objects.erase(
        std::remove_if(objects.begin(), objects.end(),
                        [year](const dated_object& o) { return o.expires_year <= year; }),
        objects.end());
}

// ---------------------------------------------------------------------------
// BL-939 — the scarcity signal
// ---------------------------------------------------------------------------

int scarcity_good_index(region_class good)
{
    switch (good)
    {
    case region_class::farm:   return 0;
    case region_class::ore:    return 1;
    case region_class::energy: return 2;
    case region_class::port:   return 3;
    case region_class::none:   return -1;
    }
    return -1;
}

void refresh_market_scarcity(std::vector<region>& regions, const std::vector<polity>& polities)
{
    constexpr int             good_count = 4;
    const region_class goods[good_count] =
        { region_class::farm, region_class::ore, region_class::energy, region_class::port };

    // Per-polity, per-good: does it hold AT LEAST ONE region dominant in the
    // good, anywhere on its ground? Independently derived here rather than
    // read off `derive_wants`'s own private table -- that function runs once,
    // at the pass 1 -> pass 2 handoff; this one runs every decision round of
    // the Exploration span, on ground that is still changing hands.
    std::vector<std::array<bool, good_count>> holds(polities.size());
    for (auto& row : holds) row.fill(false);
    std::vector<int64_t> polity_population(polities.size(), 0);
    for (const region& r : regions)
    {
        if (r.nation < 0 || static_cast<std::size_t>(r.nation) >= polities.size()) continue;
        const std::size_t n = static_cast<std::size_t>(r.nation);
        polity_population[n] += r.population;
        for (int g = 0; g < good_count; ++g)
            if (r.dominant == goods[static_cast<std::size_t>(g)])
                holds[n][static_cast<std::size_t>(g)] = true;
    }

    for (region& r : regions)
    {
        if (!r.has_market
         || r.nation < 0 || static_cast<std::size_t>(r.nation) >= polities.size())
        {
            for (int g = 0; g < good_count; ++g) { r.scarcity_q[g] = 0; r.scarcity_raw_q[g] = 0; }
            continue;
        }
        const std::size_t n = static_cast<std::size_t>(r.nation);

        // A DEMAND-PRESSURE TERM FROM THE POLITY'S OWN POPULATION. A
        // placeholder scale (same footing as the treasury income weights,
        // BL-932) -- a measurement owed, not a guess dressed up as one.
        const int pop_term_q = clampi(static_cast<int>(polity_population[n] / 5000), 0, 1000);

        for (int g = 0; g < good_count; ++g)
        {
            // The market's OWN ground already carries this good -- nothing
            // to want locally, regardless of what the rest of the realm
            // holds.
            if (r.dominant == goods[static_cast<std::size_t>(g)])
            {
                r.scarcity_raw_q[g] = 0;
                r.scarcity_q[g]     = 0;
                continue;
            }

            // A good the polity holds NOWHERE is a sharper want than one it
            // merely lacks at this particular market.
            const int base_q = holds[n][static_cast<std::size_t>(g)] ? 300 : 700;
            r.scarcity_raw_q[g] = clampi(base_q + pop_term_q / 4, 0, 1000);
            // BL-954: the unmet signal starts equal to the raw want; the
            // upkeep step relieves it by inbound flow once flows are known.
            r.scarcity_q[g] = r.scarcity_raw_q[g];
        }
    }
}

int market_scarcity_q(const std::vector<region>& regions, const history_sim_state& s,
                       int viewer_polity, int market_region, region_class good)
{
    if (market_region < 0 || static_cast<std::size_t>(market_region) >= regions.size()) return 0;
    const region& r = regions[static_cast<std::size_t>(market_region)];
    if (!r.has_market) return 0;
    const int gi = scarcity_good_index(good);
    if (gi < 0) return 0;
    if (r.nation == viewer_polity) return r.scarcity_q[gi]; // a polity always knows its own market
    // BL-933 -- A SECOND LEGIBILITY PATH: a bound `trade_access` clause makes
    // the holder's market legible even to a viewer with no contact record at
    // all (EXPLORATION.md sec What a treaty is: "one party's market is
    // legible and reachable to the other"). Checked before the omniscience
    // guard, deliberately -- a treaty is a stronger fact than mere contact.
    if (has_treaty_clause(s, viewer_polity, r.nation, treaty_clause::trade_access))
        return r.scarcity_q[gi];
    if (!has_contact(s, viewer_polity, r.nation)) return 0; // the omniscience guard
    return r.scarcity_q[gi];
}

// ---------------------------------------------------------------------------
// BL-953 -- a want points a campaign outward
// ---------------------------------------------------------------------------

int polity_good_want_q(const std::vector<region>& regions, const std::vector<polity>& polities,
                       const std::vector<culture_good_preference>& prefs,
                       int polity_id, region_class good)
{
    if (polity_id < 0 || static_cast<std::size_t>(polity_id) >= polities.size()) return 0;
    const polity& q = polities[static_cast<std::size_t>(polity_id)];
    if (q.capital < 0 || static_cast<std::size_t>(q.capital) >= regions.size()) return 0;
    const region& seat = regions[static_cast<std::size_t>(q.capital)];
    if (!seat.has_market) return 0;
    const int gi = scarcity_good_index(good);
    if (gi < 0) return 0;
    const int scarcity = clampi(seat.scarcity_q[gi], 0, 1000);
    if (scarcity == 0) return 0;

    // THE PEOPLE AT THE SEAT, at the grain the preference table is keyed on
    // (`region::culture`'s plurality) -- a preference is a fact about who
    // lives there, not who rules (EXPLORATION.md sec A good acquires a
    // cultural preference). The founding culture only where the seat carries
    // no share at all.
    int culture = seat.culture.plurality();
    if (culture < 0) culture = q.culture;

    int weight_q = 0;
    if (culture >= 0)
    {
        // `prefs` is ascending by culture (then by the fixed good order), so
        // a culture's own entries are one contiguous run of at most four.
        auto it = std::lower_bound(prefs.begin(), prefs.end(), culture,
            [](const culture_good_preference& p, int c) { return p.culture < c; });
        for (; it != prefs.end() && it->culture == culture; ++it)
            if (it->good == good) { weight_q = clampi(it->weight_q, 0, 1000); break; }
    }
    return clampi((scarcity * (500 + weight_q / 2)) / 1000, 0, 1000);
}

int want_leaned_campaign_value(int value, int w_want_q, int want_q)
{
    if (value <= 0 || w_want_q == 0 || want_q <= 0) return value;
    const int64_t lean = static_cast<int64_t>(w_want_q) * clampi(want_q, 0, 1000);
    const int64_t leaned = static_cast<int64_t>(value) + (static_cast<int64_t>(value) * lean) / 1000000;
    return static_cast<int>(clampi64(leaned, 0, INT32_MAX));
}

int choose_subjection_native(const std::vector<std::pair<int, int>>& candidates)
{
    int best_id = -1, best_want = 0;
    for (const auto& c : candidates)
    {
        if (best_id < 0 || c.second > best_want || (c.second == best_want && c.first < best_id))
        {
            best_id   = c.first;
            best_want = c.second;
        }
    }
    return best_id;
}

// ---------------------------------------------------------------------------
// BL-1096 -- the purchase fork inside the subjection (EXPLORATION.md sec Two
// ways to claim ground across water). Three pure functions so the harness can
// put the three cases to the verb directly and the loop cannot decide twice.
// ---------------------------------------------------------------------------

bool subjection_purchase_params_valid(const history_sim_params& p)
{
    return p.subjection_purchase_rate_q >= 0 && p.subjection_purchase_rate_q <= 10000
        && p.subjection_purchase_floor  >= 0 && p.subjection_purchase_floor  <= (1LL << 48);
}

int64_t purchase_price_q(const region& native_seat, const history_sim_params& p)
{
    // stock <= 2^48 (the treasury's own clamp) and rate <= 10^4 < 2^14, so
    // the product stays under 2^62; the caller validated both constants.
    const int64_t stock = std::max<int64_t>(native_seat.treasury, 0);
    const int64_t at_rate = (stock * p.subjection_purchase_rate_q) / 1000;
    return std::max<int64_t>(p.subjection_purchase_floor, at_rate);
}

int subjection_verb(const region& native_seat, int64_t buyer_treasury,
                    const history_sim_params& p)
{
    // OFF (both zero) or REJECTED (out of domain): every binding is a TAKE.
    // Both are "the fork is not on", and neither prices anything -- a zero
    // price would otherwise buy every coast for nothing.
    if (!subjection_purchase_params_valid(p)) return 1;
    if (p.subjection_purchase_rate_q == 0 && p.subjection_purchase_floor == 0) return 1;
    if (native_seat.port_q <= 0) return 1;                     // no coast to land a party on
    return buyer_treasury >= purchase_price_q(native_seat, p) ? 0 : 1;
}

// ---------------------------------------------------------------------------
// BL-955 -- spend is ALLOCATED, not bought whenever affordable
// ---------------------------------------------------------------------------

void exploration_lean_ranks(const std::vector<polity>& polities, const creed_state* cs,
                            std::vector<int>& expansion_rank_q,
                            std::vector<int>& consolidator_rank_q)
{
    expansion_rank_q.assign(polities.size(), 0);
    consolidator_rank_q.assign(polities.size(), 0);
    if (cs == nullptr) return;

    // The ranked set: living polities with a culture, in ascending index.
    std::vector<std::size_t> members;
    std::vector<int> expn, cons;
    for (std::size_t i = 0; i < polities.size(); ++i)
    {
        const polity& q = polities[i];
        if (!q.alive || q.culture < 0 || static_cast<std::size_t>(q.culture) >= cs->cultures.size())
            continue;
        const culture& c = cs->cultures[static_cast<std::size_t>(q.culture)];
        members.push_back(i);
        expn.push_back(expansion_lean_q(c));
        cons.push_back(consolidator_lean_q(c));
    }
    if (members.empty()) return;

    // Rank = how many members lean STRICTLY lower, read off a sorted copy by
    // lower_bound -- a property of the integers alone, so ties share a rank.
    std::vector<int> expn_sorted = expn, cons_sorted = cons;
    std::sort(expn_sorted.begin(), expn_sorted.end());
    std::sort(cons_sorted.begin(), cons_sorted.end());
    const int64_t denom = std::max<int64_t>(1, static_cast<int64_t>(members.size()) - 1);
    for (std::size_t k = 0; k < members.size(); ++k)
    {
        const int64_t below_e = std::lower_bound(expn_sorted.begin(), expn_sorted.end(), expn[k])
                              - expn_sorted.begin();
        const int64_t below_c = std::lower_bound(cons_sorted.begin(), cons_sorted.end(), cons[k])
                              - cons_sorted.begin();
        expansion_rank_q[members[k]]    = static_cast<int>(clampi64(below_e * 1000 / denom, 0, 1000));
        consolidator_rank_q[members[k]] = static_cast<int>(clampi64(below_c * 1000 / denom, 0, 1000));
    }
}

exploration_spend_scores score_exploration_spend(const history_sim_params&     p,
                                                 const exploration_spend_facts& f)
{
    const int expn  = clampi(f.expansion_rank_q, 0, 1000);
    const int cons  = clampi(f.consolidator_rank_q, 0, 1000);
    const int want  = clampi(f.water_want_q, 0, 1000);
    const int alarm = clampi(f.alarm_q, 0, 1000);
    const int stock = clampi(f.port_stock_q, 0, 1000);

    exploration_spend_scores s;

    // HOLD -- the value of keeping the purse, leaning up with consolidation
    // (a realm that grows inward has less to buy abroad), weighted below the
    // army's own consolidator term.
    s.hold_q = p.spend_hold_base_q + (cons * p.spend_w_hold_consolidator_q) / 1000;

    // STANDING ARMY -- consolidation and the Alarm of long-known neighbours.
    // BL-972: no saturation term. What stops a realm buying an army every
    // round is the bill it has already paid this round (the treasury the
    // eligibility reads is the treasury AFTER upkeep) and the levy bound: a
    // step is a whole step of heads the seat's pool can actually lend.
    s.army_eligible = p.standing_army_build_cost_q > 0 && f.treasury >= p.standing_army_build_cost_q
                   && f.levy_room >= std::max<int64_t>(p.standing_army_build_step_q, 1);
    s.army_q = (cons * p.spend_w_consolidator_q + alarm * p.spend_w_alarm_q) / 1000;

    // PORT and NAVY share the OUTWARD pull: expansion, and wants only water reaches.
    const int outward = (expn * p.spend_w_expansion_q + want * p.spend_w_water_want_q) / 1000;

    s.port_eligible = p.port_build_cost_q > 0 && f.treasury >= p.port_build_cost_q
                   && f.port_window_q > 0 && f.port_stock_q < 1000;
    s.port_q = (outward * (1000 - stock)) / 1000; // a nearly full port is worth little more

    s.navy_eligible = p.navy_build_cost_q > 0 && f.treasury >= p.navy_build_cost_q
                   && f.port_stock_q >= p.navy_min_port_stock_q;
    s.navy_q = outward; // BL-972: a fleet no longer saturates; its bill is what bounds it
    return s;
}

exploration_spend_option choose_exploration_spend(const exploration_spend_scores& s)
{
    // TOTAL ORDER: walked hold -> army -> port -> navy, and a later option
    // replaces the incumbent only on a STRICTLY higher score, so an exact tie
    // goes to hold, then army, then port, then navy. Ineligible never wins.
    exploration_spend_option best   = exploration_spend_option::hold;
    int                      best_q = s.hold_q;
    if (s.army_eligible && s.army_q > best_q) { best = exploration_spend_option::army_step; best_q = s.army_q; }
    if (s.port_eligible && s.port_q > best_q) { best = exploration_spend_option::port_step; best_q = s.port_q; }
    if (s.navy_eligible && s.navy_q > best_q) { best = exploration_spend_option::navy_step; best_q = s.navy_q; }
    return best;
}

int64_t defender_levy_estimate(const region& tgt, const history_sim_params& params)
{
    // `muster_garrison`'s LEVY arithmetic, asked as a question: the target
    // and the shortfall are read on the ORDINARY men, never the paid
    // standing heads (BL-955; 0 throughout the Empire span, where this is
    // exactly the pre-BL-955 estimate).
    const int64_t def_target = garrison_target(tgt, params.garrison_fraction_q);
    const int64_t ordinary   = tgt.army_stock - standing_army_heads(tgt);
    int64_t def_men = tgt.army_stock;
    if (ordinary < def_target)
    {
        const int64_t gap  = def_target - ordinary;
        const int64_t want = (gap * clampi(params.defence_levy_q, 0, 1000)) / 1000;
        if (want > 0 && tgt.manpower_stock > 0) def_men += std::min(want, tgt.manpower_stock);
    }
    return def_men;
}

/// The best land line joining @p a and @p b in @p ctx (0 where none). The
/// same lookup `trade_flow_volume_q` makes.
static int trade_land_line_q(const trade_context& ctx, int a, int b)
{
    const uint16_t lo = static_cast<uint16_t>(std::min(a, b));
    const uint16_t hi = static_cast<uint16_t>(std::max(a, b));
    const auto it = std::lower_bound(
        ctx.land_lines.begin(), ctx.land_lines.end(), std::pair<uint16_t, uint16_t>{lo, hi},
        [](const trade_context::land_line& x, const std::pair<uint16_t, uint16_t>& k) {
            if (x.lo != k.first) return x.lo < k.first;
            return x.hi < k.second;
        });
    return (it != ctx.land_lines.end() && it->lo == lo && it->hi == hi) ? it->line_q : 0;
}

void run_exploration_upkeep(std::vector<region>&                 regions,
                            std::vector<polity>&                 polities,
                            const std::vector<history_corridor>& corridors,
                            const history_sim_params&             params,
                            int64_t                                year,
                            int                                    step_years,
                            exploration_upkeep_spend*              spend,
                            const std::vector<dated_object>*       treaties,
                            std::vector<trade_flow>*               flows_out,
                            const exploration_spend_context*       spend_ctx)
{
    // BL-939 -- the demand half, refreshed on the same round-level cadence
    // the treasury's own earn runs on: a market's signal is a fact about
    // ground that is still changing hands, exactly like the treasury's own
    // endowment/network terms below. BL-954: this fills the RAW want; the
    // unmet signal is relieved below once the round's flows are known.
    refresh_market_scarcity(regions, polities);

    // ---- BL-954: TRADE IS A WANT MET BY THROUGHPUT. ------------------------
    // Raw want -> flows -> relieve the signal -> earn (below) -> pay/invest.
    // Flows are sized off the RAW signal only, so relief never feeds back
    // into the volume that produced it.
    std::vector<trade_flow> flows;
    trade_context           trade_ctx; // BL-955 reads its land lines below
    if (treaties != nullptr && !treaties->empty())
    {
        trade_ctx = build_trade_context(regions, polities, corridors);
        flows = compute_trade_flows(trade_ctx, regions, polities, *treaties);
    }
    std::vector<int64_t> trade_volume(polities.size(), 0); // both ends, per polity
    for (const trade_flow& f : flows)
    {
        const polity& buyer = polities[f.buyer]; // in range: compute_trade_flows checked
        region& market = regions[static_cast<std::size_t>(buyer.capital)];
        market.scarcity_q[f.good] = std::max(0, market.scarcity_q[f.good] - f.volume_q);
        trade_volume[f.seller] += f.volume_q;
        trade_volume[f.buyer]  += f.volume_q;
    }

    // ---- BL-955: THE ALLOCATION'S INPUTS, READ ONCE, BEFORE ANY STOCK MOVES.
    // Ranks, across-water want and near-home Alarm are all read off the
    // round's opening stocks (after the signal is relieved, before any polity
    // pays or invests), so no polity's choice depends on the index order in
    // which the others bought.
    const std::size_t np = polities.size();
    // BL-972: per polity, the per-mille of its paid standing heads that this
    // round's bill did NOT cover (0 = met in full). Read by the decay pass
    // at the end, which is why it is per polity and not per seat: the paid
    // heads a campaign marched onto other held ground are on the same bill.
    std::vector<int> army_unpaid_per_mille(np, 0);
    std::vector<int> expn_rank, cons_rank;
    exploration_lean_ranks(polities, spend_ctx ? spend_ctx->creeds : nullptr, expn_rank, cons_rank);
    std::vector<int>     water_want(np, 0), near_alarm(np, 0);
    // The REALM's paid standing heads, wherever they stand -- a campaign that
    // marches them onto conquered ground does not reset the saturation read.
    std::vector<int64_t> realm_paid(np, 0);
    {
        constexpr int good_count = 4;
        const region_class goods[good_count] =
            { region_class::farm, region_class::ore, region_class::energy, region_class::port };
        // A good is "across water" for a polity when it holds NO ground
        // dominant in it AND no inbound flow of it arrives over a pair joined
        // by a land line. The read is deliberately that simple: a want a
        // realm's own ground or a road already answers is not one a hull
        // would, and every other unmet want is.
        std::vector<std::array<bool, good_count>> reachable(np);
        for (auto& row : reachable) row.fill(false);
        for (const region& r : regions)
        {
            if (r.nation < 0 || static_cast<std::size_t>(r.nation) >= np) continue;
            const std::size_t n = static_cast<std::size_t>(r.nation);
            realm_paid[n] += standing_army_heads(r); // 0 unless paid for by this holder
            const int gi = scarcity_good_index(r.dominant);
            if (gi >= 0) reachable[n][static_cast<std::size_t>(gi)] = true;
        }
        for (const trade_flow& f : flows)
            if (f.good < good_count && trade_land_line_q(trade_ctx, f.seller, f.buyer) > 0)
                reachable[f.buyer][f.good] = true;

        // The preference weight is only filled when `w_want_q` != 0 (generation
        // runs 1000); at 0 the table is empty and every good weighs 0 here.
        static const std::vector<culture_good_preference> no_prefs;
        const std::vector<culture_good_preference>& prefs =
            (spend_ctx && spend_ctx->prefs) ? *spend_ctx->prefs : no_prefs;
        const history_sim_state* st = spend_ctx ? spend_ctx->state : nullptr;

        for (std::size_t i = 0; i < np; ++i)
        {
            if (!polities[i].alive) continue;
            int want = 0;
            for (int g = 0; g < good_count; ++g)
                if (!reachable[i][static_cast<std::size_t>(g)])
                    want = std::max(want, polity_good_want_q(regions, polities, prefs,
                                                             static_cast<int>(i), goods[g]));
            water_want[i] = want;

            // NEAR-HOME ALARM: the max over every contact this polity made
            // before the near-home cutoff (BL-941's near-home read; BL-1036
            // anchors it at 1200 explicitly rather than at the span's own
            // open), walked in the table's own sorted order.
            if (st != nullptr && i <= 0xFFFE)
            {
                const uint16_t self = static_cast<uint16_t>(i);
                auto it = std::lower_bound(st->contacts.begin(), st->contacts.end(), self,
                    [](const contact& c, uint16_t k) { return c.from < k; });
                int alarm = 0;
                for (; it != st->contacts.end() && it->from == self; ++it)
                {
                    if (it->first.year >= params.near_home_cutoff_year) continue;
                    alarm = std::max(alarm, deterrence_alarm_q(regions, *st, params,
                                                               static_cast<int>(i), it->to));
                }
                near_alarm[i] = alarm;
            }
        }
    }

    // BL-932 -- EARN, the first of "earn, then pay stocks, then invest"
    // (EXPLORATION.md sec The engine is shared). PAY and INVEST beyond the
    // ordinary verb are still owed to a later item (BL-933's stocks).
    for (polity& q : polities)
    {
        if (!q.alive) continue;
        if (q.capital < 0 || static_cast<std::size_t>(q.capital) >= regions.size()) continue;
        region& seat = regions[static_cast<std::size_t>(q.capital)];

        // ---- CONSOLIDATION: THE PHASE'S OPENING ACT, ONCE (EXPLORATION.md
        // sec Capital arrives: "At 1200 CE every seat's stores flow to the
        // capital, once"). Fires exactly on the round at
        // `params.consolidation_year` -- the Exploration span's own start
        // year, which it sets explicitly (BL-1036). A span that opens later
        // (Industrialisation, at 1660) keeps the 1200 anchor and so never reaches
        // it: the sweep happened once, and a second one would be a second
        // windfall. Unreachable on the Empire span too, because
        // `exploration_upkeep_enabled` is false throughout it.
        //
        // BL-998 (Ben, 2026-09-15, NR-871): EVERY seat the polity holds, not
        // the capital alone. A polity's hinterland seats each carry their own
        // `material_stock` (CIVILISATION.md: stores sit AT THE SEAT); before
        // this ruling only the capital's own stock became treasury and every
        // other held seat kept its hoard on the ground, unseen by the spend
        // scorer. "Once" is literal: the non-capital seats are zeroed here and
        // accumulate again as the round runs -- nothing sweeps them later.
        if (year == params.consolidation_year)
        {
            int64_t folded = 0;
            for (region& r : regions)
            {
                if (!r.is_seat || r.nation != q.id) continue;
                folded += r.material_stock;
                r.material_stock = 0;
            }
            // The capital itself may not carry `is_seat` in a synthetic
            // fixture; its own stock folds regardless, exactly as before.
            folded += seat.material_stock;
            seat.material_stock = 0;
            seat.treasury = clampi64(seat.treasury + folded, 0, 1LL << 48);
        }

        // BL-1041: the purse as the round's earn finds it -- the base the
        // round's SURPLUS (earn less the army and navy bills) is read against.
        const int64_t purse_before_earn = seat.treasury;

        // ---- ONGOING EARN: endowment, the inherited network, a market. ----
        // "Fed by what the polity already holds and already reaches" — read
        // fresh every round rather than cached, so ground lost or gained this
        // round is reflected the very next time the treasury earns.
        int64_t endowment_sum_q = 0;
        int64_t held_count      = 0;
        for (const region& r : regions)
        {
            if (r.nation != q.id) continue;
            endowment_sum_q += (r.farm_q + r.ore_q + r.energy_q + r.port_q) / 4;
            ++held_count;
        }
        const int64_t endowment_mean_q = held_count > 0 ? endowment_sum_q / held_count : 0;

        int64_t corridor_touch = 0;
        for (const history_corridor& c : corridors)
        {
            const bool a_held = static_cast<std::size_t>(c.a) < regions.size()
                              && regions[c.a].nation == q.id;
            const bool b_held = static_cast<std::size_t>(c.b) < regions.size()
                              && regions[c.b].nation == q.id;
            if (a_held || b_held) ++corridor_touch;
        }

        const int64_t endowment_income =
            (endowment_mean_q * params.treasury_endowment_income_q / 1000)
                * std::max(step_years, 1);
        const int64_t corridor_income =
            corridor_touch * params.treasury_corridor_income_q * std::max(step_years, 1);
        // BL-954 -- A MARKET EARNS BY WHAT FLOWS THROUGH IT, NOT BY STANDING.
        // No flat per-market term: every flow credits both of its capitals.
        const int64_t own_trade_volume =
            (q.id >= 0 && static_cast<std::size_t>(q.id) < trade_volume.size())
                ? trade_volume[static_cast<std::size_t>(q.id)] : 0;
        const int64_t trade_income =
            (own_trade_volume * params.treasury_trade_income_q * std::max(step_years, 1)) / 1000;
        // Subject tribute (EXPLORATION.md sec Capital arrives): a fourth
        // source named by the design, left at 0 -- subjects do not exist yet
        // (BL-933/934). A hook, not a guess: the design says four sources
        // and this is the fourth's place, ready for that item to fill.
        const int64_t tribute_income = 0;

        seat.treasury = clampi64(
            seat.treasury + endowment_income + corridor_income + trade_income + tribute_income,
            0, 1LL << 48);

        // ---- BL-972 -- THE BILL, before any purchase. ----------------------
        // EXPLORATION.md sec Force persists now: each stock is one "the
        // treasury maintains", and "a polity that over-builds is poorer every
        // round afterwards". The army is billed first (the garrison at home
        // before the hulls), then the fleet. Each bill is met in full where
        // the purse allows, else for as many heads/units as it covers, and
        // the UNPAID share decays at BL-955's rates -- a treasury of 0
        // reproduces BL-955's decay exactly. The treasury the scored choice
        // below reads is the treasury AFTER this, which is what makes a
        // standing force a cost in the world rather than a cap in the scorer.
        const int64_t years_q = std::max(step_years, 1);
        const std::size_t pi = static_cast<std::size_t>(&q - polities.data());
        const bool had_navy = q.navy_stock > 0; // the round's OPENING fleet, for the lapse record
        {
            const int64_t heads = realm_paid[pi];
            const int64_t rate  =
                std::max<int64_t>(params.standing_army_upkeep_per_1000_heads_year_q, 0) * years_q;
            if (heads > 0 && rate > 0)
            {
                const int64_t due = (heads * rate) / 1000;
                int64_t heads_paid = heads;
                int64_t paid_sum   = due;
                if (seat.treasury < due)
                {
                    heads_paid = std::min(heads, (seat.treasury * 1000) / rate);
                    paid_sum   = (heads_paid * rate) / 1000; // floor: never above the purse
                    // Every unpaid head counts, even where the per-mille would round to 0.
                    army_unpaid_per_mille[pi] = static_cast<int>(
                        clampi64(((heads - heads_paid) * 1000 + heads - 1) / heads, 1, 1000));
                    if (spend) ++spend->army_unpaid;
                }
                seat.treasury -= std::min(paid_sum, seat.treasury);
                if (spend) spend->army_upkeep += paid_sum;
            }
        }
        {
            const int64_t units = q.navy_stock;
            const int64_t rate  =
                std::max<int64_t>(params.navy_upkeep_per_1000_units_year_q, 0) * years_q;
            if (units > 0 && rate > 0)
            {
                const int64_t due = (units * rate) / 1000;
                int64_t units_paid = units;
                int64_t paid_sum   = due;
                if (seat.treasury < due)
                {
                    units_paid = std::min(units, (seat.treasury * 1000) / rate);
                    paid_sum   = (units_paid * rate) / 1000;
                    // The hulls the purse could not keep: BL-955's decay, on
                    // the unpaid share only, at least one unit while any is.
                    const int64_t unpaid = units - units_paid;
                    const int64_t decay  = std::max<int64_t>(
                        (unpaid * params.navy_decay_per_mille_year_q * years_q) / 1000, 1);
                    q.navy_stock = clampi64(units - std::min(decay, units), 0, 1LL << 48);
                    if (spend) ++spend->navy_unpaid;
                }
                seat.treasury -= std::min(paid_sum, seat.treasury);
                if (spend) spend->navy_upkeep += paid_sum;
            }
        }

        // ---- BL-1041 -- CAPITAL PAID IN, AS A CONSEQUENCE. -----------------
        // INDUSTRIALISATION.md sec Beat 1 (RULED, Ben 2026-09-18): "each round a
        // fixed share of the capital treasury's surplus after the round's bills
        // converts to industry points." No polity scores it, so it is no verb.
        // THE SURPLUS is the purse's net rise across this round's earn and its
        // army and navy bills -- the round's income left over once the force
        // standing has been paid for -- floored at 0: a round whose bills eat
        // its earn pays nothing in, and the stock the purse already held is
        // never taxed a second time. The share LEAVES the treasury (a
        // conversion, not a copy); the scored purchase below reads what is left.
        // BL-1056 (RULED, Ben 2026-09-19, NR-897): the points SPREAD over the
        // polity's held regions that stand centres, in proportion to their
        // urban scale, by integer largest-remainder apportionment -- a treasury
        // builds its realm's works where its people are. Landing them all on
        // the capital's region let one region hold up to 65% of a world's
        // points. The capital still leads where it is the largest centre.
        // NR-901 (RULED, Ben 2026-09-19, option A): A POLITY THAT HOLDS NO TOWN
        // CONVERTS NOTHING -- no held region stands a centre carrying heads, so
        // there is nowhere for its works to stand: no debit, no credit, and the
        // treasury keeps the round's share. (It used to land the whole credit on
        // the capital's region, townless ground no campaign centre can receive.)
        // A treasury unit is `industry_points_per_treasury_unit` points (a
        // unit choice, history_sim.hpp says why). Only inside the span (the switch,
        // from its open year) and only on in-domain constants; a conversion
        // that would carry ANY receiving region past `industry_points_ceiling`,
        // or whose heads leave the apportionment's domain, is refused whole and
        // counted -- nothing moves.
        if (params.industry_points_enabled && year >= params.industry_open_year
            && industry_points_params_valid(params))
        {
            const int64_t surplus = std::max<int64_t>(seat.treasury - purse_before_earn, 0);
            // surplus <= 2^48 (the treasury's own clamp) and share <= 1000, so
            // the product stays under 2^58; `paid_in` <= 2^48, and times the
            // exchange rate (<= 10^4 < 2^14) the credit stays under 2^62.
            const int64_t paid_in = (surplus * params.industry_points_treasury_share_q) / 1000;
            if (paid_in > 0)
            {
                const int64_t credit = paid_in * params.industry_points_per_treasury_unit;
                std::vector<std::pair<int, int64_t>> spread;
                bool refuse = !industry_points_apportion_by_scale(regions, q.id, credit, spread);
                // NR-901: no town, no conversion. Not a refusal (nothing was out
                // of domain) and not a debit: the purse keeps the share.
                const bool no_town = !refuse && spread.empty();
                // Refused whole: past the ceiling on any receiving region, the
                // apportionment's domain, or (never on a sane purse, whose base
                // is >= 0) more than the purse holds.
                for (const auto& part : spread)
                    if (regions[static_cast<std::size_t>(part.first)].industry_points
                            > industry_points_ceiling - part.second)
                        refuse = true;
                if (no_town)
                {
                    // Nothing moves: the treasury keeps the round's share.
                }
                else if (refuse || paid_in > seat.treasury)
                {
                    if (spend) ++spend->industry_points_refused;
                }
                else
                {
                    seat.treasury -= paid_in;
                    for (const auto& part : spread)
                    {
                        region& rr = regions[static_cast<std::size_t>(part.first)];
                        rr.industry_points               += part.second;
                        rr.industry_points_from_treasury += part.second; // report-only tally
                    }
                    if (spend)
                    {
                        spend->industry_points_paid_in   += credit;
                        spend->industry_treasury_debited += paid_in;
                    }
                }
            }
        }

        // ---- BL-935 -- PAY, then INVEST: ports, navies, standing armies. --
        //
        // Same all-or-nothing SHAPE `try_build_post_road` already uses: one
        // step per funded round, or a decay when the round could not be
        // funded, never a fractional trickle. All three spend the CAPITAL'S
        // treasury, never `material_stock` (EXPLORATION.md sec Force
        // persists now: "spending capital on ports").

        // ---- BL-955 -- ONE SCORED CHOICE, then the unchanged decays. ------
        // EXPLORATION.md sec Force persists now ("Spend is ALLOCATED"): a
        // polity that can afford all three stocks still builds the one its
        // situation asks for. The gates read the round's OPENING stocks, so a
        // navy step can land in a round whose unbuilt port also silts (accepted).
        exploration_spend_facts facts;
        facts.expansion_rank_q    = expn_rank[pi];
        facts.consolidator_rank_q = cons_rank[pi];
        facts.water_want_q        = water_want[pi];
        facts.alarm_q             = near_alarm[pi];
        facts.treasury            = seat.treasury;
        facts.port_window_q       = seat.port_q;
        facts.port_stock_q        = seat.port_stock_q;
        facts.navy_stock          = q.navy_stock;
        facts.standing_army       = realm_paid[pi]; // the realm's PAID heads, persistent
        // BL-972: what the seat's pool can lend this round, in whole heads.
        facts.levy_room           =
            (std::max<int64_t>(seat.manpower_stock, 0)
             * clampi(params.standing_army_levy_per_mille_q, 0, 1000)) / 1000;
        const exploration_spend_option pick =
            choose_exploration_spend(score_exploration_spend(params, facts));
        // PORT -- only ground carrying the endowment WINDOW can host one at
        // all; `port_q` is that window and is never itself spent.
        if (seat.port_q > 0)
        {
            if (pick == exploration_spend_option::port_step)
            {
                seat.treasury -= params.port_build_cost_q;
                seat.port_stock_q = clampi(
                    seat.port_stock_q + params.port_build_step_q, 0, 1000);
                if (spend) { spend->ports += params.port_build_cost_q; ++spend->port_steps; }
            }
            else if (seat.port_stock_q > 0)
            {
                const int decay = static_cast<int>(
                    (static_cast<int64_t>(seat.port_stock_q)
                     * params.port_decay_per_mille_year_q * years_q) / 1000);
                seat.port_stock_q = clampi(seat.port_stock_q - std::max(decay, 1), 0, 1000);
            }
        }
        else
        {
            seat.port_stock_q = 0; // no window, no port, ever.
        }

        // NAVY -- BL-972: the running cost is the bill above, and the unpaid
        // share already decayed there. Here only growth, if the round chose
        // it and the capital's own port is built up enough to stage it from.
        if (pick == exploration_spend_option::navy_step)
        {
            seat.treasury -= params.navy_build_cost_q;
            q.navy_stock = clampi64(q.navy_stock + params.navy_build_step_q, 0, 1LL << 48);
            if (spend) { spend->navies += params.navy_build_cost_q; ++spend->navy_steps; }
        }
        // BL-955 reading 7: a lapse is a fleet that stood at the round's
        // opening and stands at nothing after its step -- recorded AFTER the
        // step, so a fleet rebuilt the round it hit zero is not counted.
        if (had_navy && q.navy_stock == 0 && spend && pi <= 0xFFFE)
            spend->navies_lapsed.push_back(static_cast<uint16_t>(pi));

        // STANDING ARMY -- adds to `region::army_stock` directly (the same
        // pool `gather_army`/`resolve_battle` already read, so a funded
        // standing army fights exactly like any other garrison), and records
        // the heads as PAID (`region::standing_army`), which the yearly muster
        // never disbands. Not funded this round, the paid status decays below.
        if (pick == exploration_spend_option::army_step)
        {
            const int64_t paid   = standing_army_heads(seat);
            // BL-972: a LEVY, not a conjuring. The step's heads leave the
            // seat's pool of eligible civilians -- the same pool the yearly
            // muster draws its garrison from, so a paid army and a mustered
            // one now contend for the same people. Eligibility already
            // proved the pool lends a whole step, so `raised` is the step.
            const int64_t raised = raise_manpower(seat, params.standing_army_build_step_q);
            seat.treasury -= params.standing_army_build_cost_q;
            seat.army_stock = clampi64(seat.army_stock + raised, 0, 1LL << 48);
            seat.standing_army       = std::min(paid + raised, seat.army_stock);
            seat.standing_army_owner = q.id;
            if (spend)
            {
                spend->standing_armies += params.standing_army_build_cost_q;
                ++spend->army_steps;
                spend->levy_raised += raised;
            }
        }
    }

    // ---- BL-972: THE UNPAID SHARE OF A STANDING ARMY GOES HOME. -----------
    // Every region carrying paid heads whose payer's bill went short this
    // round loses, of those heads, the unpaid share times
    // `standing_army_decay_per_mille_year_q` per year -- never below zero,
    // at least one head while any is unpaid. The men are not killed, and
    // they are not left standing as ordinary garrison either: a levy that
    // is no longer paid returns to the `manpower_stock` of the ground it
    // stands on, capped at that ground's ceiling exactly as
    // `muster_garrison`'s own disband is. A realm whose bill was met in full
    // loses none; ground whose payer is not a tracked polity reads as wholly
    // unpaid (the pre-BL-972 "not funded this round" reading). A polity
    // whose bill went short cannot have bought a step this round (the
    // remainder after a short bill is under one head's rate), so this pass
    // never touches heads raised above.
    {
        const int64_t years_q = std::max(step_years, 1);
        for (std::size_t ri = 0; ri < regions.size(); ++ri)
        {
            region& r = regions[ri];
            const int64_t paid = standing_army_heads(r);
            if (paid <= 0) { r.standing_army = 0; continue; }
            r.standing_army = paid;
            const bool tracked = r.nation >= 0 && static_cast<std::size_t>(r.nation) < np;
            const int unpaid_q =
                tracked ? army_unpaid_per_mille[static_cast<std::size_t>(r.nation)] : 1000;
            if (unpaid_q <= 0) continue;
            int64_t gone = (paid * unpaid_q * params.standing_army_decay_per_mille_year_q * years_q)
                         / 1000000;
            gone = std::min(std::max<int64_t>(gone, 1), paid);
            r.standing_army = paid - gone;
            r.army_stock    = std::max<int64_t>(r.army_stock - gone, 0);
            const int64_t ceiling = manpower_ceiling(r.population, r.work_manpower_mod);
            r.manpower_stock = clampi64(r.manpower_stock + gone, 0, ceiling);
            if (spend) spend->levy_returned += gone;
        }
    }

    if (flows_out != nullptr) *flows_out = std::move(flows);
}

// ---------------------------------------------------------------------------
// BL-954 — trade flows
// ---------------------------------------------------------------------------

trade_context build_trade_context(const std::vector<region>&           regions,
                                  const std::vector<polity>&           polities,
                                  const std::vector<history_corridor>& corridors)
{
    constexpr int good_count = 4;
    const region_class goods[good_count] =
        { region_class::farm, region_class::ore, region_class::energy, region_class::port };

    trade_context ctx;
    ctx.holding_q.assign(polities.size(), std::array<int32_t, 4>{0, 0, 0, 0});

    std::vector<int64_t>                  held(polities.size(), 0);
    std::vector<std::array<int64_t, 4>>   dominant(polities.size(), std::array<int64_t, 4>{0, 0, 0, 0});
    for (const region& r : regions)
    {
        if (r.nation < 0 || static_cast<std::size_t>(r.nation) >= polities.size()) continue;
        const std::size_t n = static_cast<std::size_t>(r.nation);
        ++held[n];
        for (int g = 0; g < good_count; ++g)
            if (r.dominant == goods[g]) ++dominant[n][static_cast<std::size_t>(g)];
    }
    for (std::size_t n = 0; n < polities.size(); ++n)
        if (held[n] > 0)
            for (int g = 0; g < good_count; ++g)
                ctx.holding_q[n][static_cast<std::size_t>(g)] = static_cast<int32_t>(
                    (dominant[n][static_cast<std::size_t>(g)] * 1000) / held[n]);

    // LAND LINES. A corridor whose two endpoints are held by two different
    // polities is a place their networks touch; the line it offers is the
    // weaker of the two sides' own reach to it. Walked in the corridor
    // vector's own sorted (a, b) order, then sorted by (lo, hi) and folded to
    // the best line per pair -- the result is a property of the integers.
    for (const history_corridor& c : corridors)
    {
        if (static_cast<std::size_t>(c.a) >= regions.size()
         || static_cast<std::size_t>(c.b) >= regions.size()) continue;
        const region& ra = regions[c.a];
        const region& rb = regions[c.b];
        if (ra.nation < 0 || rb.nation < 0 || ra.nation == rb.nation) continue;
        if (static_cast<std::size_t>(ra.nation) >= polities.size()
         || static_cast<std::size_t>(rb.nation) >= polities.size()) continue;
        trade_context::land_line ln;
        ln.lo     = static_cast<uint16_t>(std::min(ra.nation, rb.nation));
        ln.hi     = static_cast<uint16_t>(std::max(ra.nation, rb.nation));
        ln.line_q = clampi(std::min(ra.network_supply_q, rb.network_supply_q), 0, 1000);
        ctx.land_lines.push_back(ln);
    }
    std::sort(ctx.land_lines.begin(), ctx.land_lines.end(),
              [](const trade_context::land_line& x, const trade_context::land_line& y) {
                  if (x.lo != y.lo) return x.lo < y.lo;
                  if (x.hi != y.hi) return x.hi < y.hi;
                  return x.line_q > y.line_q; // best line first within a pair
              });
    ctx.land_lines.erase(
        std::unique(ctx.land_lines.begin(), ctx.land_lines.end(),
                    [](const trade_context::land_line& x, const trade_context::land_line& y) {
                        return x.lo == y.lo && x.hi == y.hi;
                    }),
        ctx.land_lines.end());
    return ctx;
}

int trade_flow_volume_q(const trade_context& ctx, const std::vector<region>& regions,
                        const std::vector<polity>& polities,
                        int seller, int buyer, int good)
{
    if (good < 0 || good >= 4) return 0;
    if (seller < 0 || buyer < 0 || seller == buyer) return 0;
    if (static_cast<std::size_t>(seller) >= polities.size()
     || static_cast<std::size_t>(buyer) >= polities.size()) return 0;
    const polity& ps = polities[static_cast<std::size_t>(seller)];
    const polity& pb = polities[static_cast<std::size_t>(buyer)];
    if (!ps.alive || !pb.alive) return 0;
    if (ps.capital < 0 || static_cast<std::size_t>(ps.capital) >= regions.size()) return 0;
    if (pb.capital < 0 || static_cast<std::size_t>(pb.capital) >= regions.size()) return 0;
    const region& seller_seat = regions[static_cast<std::size_t>(ps.capital)];
    const region& buyer_seat  = regions[static_cast<std::size_t>(pb.capital)];

    // WANT: the buyer capital market's RAW signal (0 where it stands no market).
    const int want_q = buyer_seat.scarcity_raw_q[good];
    if (want_q <= 0) return 0;

    // HOLDER: the seller's share of held ground dominant in the good.
    const int holding_q = static_cast<std::size_t>(seller) < ctx.holding_q.size()
                        ? ctx.holding_q[static_cast<std::size_t>(seller)][static_cast<std::size_t>(good)]
                        : 0;
    if (holding_q <= 0) return 0;

    // LINE: the better of land (a corridor joining the two realms) and sea
    // (both seats' built ports, carried by the SELLER's navy).
    const int land_q = trade_land_line_q(ctx, seller, buyer);
    const int sea_q = ps.navy_stock > 0
                    ? clampi(std::min(seller_seat.port_stock_q, buyer_seat.port_stock_q), 0, 1000)
                    : 0;
    const int line_q = std::max(land_q, sea_q);

    return std::max(0, std::min({want_q, holding_q, line_q}));
}

int pair_trade_value_q(const trade_context& ctx, const std::vector<region>& regions,
                       const std::vector<polity>& polities, int a, int b,
                       const std::vector<trade_flow>& flows)
{
    // THE MARGINAL TRADE THE PAIR'S CLAUSE OPENS. A buyer's want and a
    // seller's holding are each ONE quantity shared across every partner
    // (`compute_trade_flows`), so what binding THIS pair is worth is only
    // what the other partners have not already taken: the unshared volume,
    // capped by the buyer's raw want less what OTHER sellers already bring it,
    // and by the seller's holding less what it already sends OTHER buyers.
    // Flows between a and b themselves are excluded from both remainders, so
    // re-scoring a bound pair reads the same quantity as scoring it unbound.
    int64_t in_other[2][4]  = { {0, 0, 0, 0}, {0, 0, 0, 0} }; // [side][good]
    int64_t out_other[2][4] = { {0, 0, 0, 0}, {0, 0, 0, 0} };
    for (const trade_flow& f : flows)
    {
        if (f.good >= 4 || f.volume_q <= 0) continue;
        for (int side = 0; side < 2; ++side)
        {
            const int me = side == 0 ? a : b, other = side == 0 ? b : a;
            if (f.buyer == me && f.seller != other)  in_other[side][f.good]  += f.volume_q;
            if (f.seller == me && f.buyer != other)  out_other[side][f.good] += f.volume_q;
        }
    }

    int total = 0;
    for (int g = 0; g < 4; ++g)
        for (int dir = 0; dir < 2; ++dir)
        {
            const int seller_side = dir, buyer_side = 1 - dir;
            const int seller = seller_side == 0 ? a : b;
            const int buyer  = buyer_side == 0 ? a : b;
            const int v = trade_flow_volume_q(ctx, regions, polities, seller, buyer, g);
            if (v <= 0) continue; // also guarantees every id and capital read below is in range
            const int want_q = regions[static_cast<std::size_t>(
                polities[static_cast<std::size_t>(buyer)].capital)].scarcity_raw_q[g];
            const int holding_q = ctx.holding_q[static_cast<std::size_t>(seller)][static_cast<std::size_t>(g)];
            const int64_t want_room = std::max<int64_t>(0, want_q - in_other[buyer_side][g]);
            const int64_t hold_room = std::max<int64_t>(0, holding_q - out_other[seller_side][g]);
            total += static_cast<int>(std::min<int64_t>({static_cast<int64_t>(v), want_room, hold_room}));
        }
    return total; // bounded by 8 * 1000
}

/// Drop every flow whose (seller, buyer) pair holds no `trade_access` clause
/// in @p objects. Order-preserving, so a sorted vector stays sorted.
static void prune_flows_without_trade_access(std::vector<trade_flow>&         flows,
                                             const std::vector<dated_object>& objects)
{
    std::vector<std::pair<int32_t, int32_t>> bound;
    for (const dated_object& o : objects)
    {
        if (o.kind != static_cast<int32_t>(treaty_clause::trade_access)) continue;
        bound.push_back({std::min(o.a, o.b), std::max(o.a, o.b)});
    }
    std::sort(bound.begin(), bound.end());
    flows.erase(std::remove_if(flows.begin(), flows.end(),
                    [&](const trade_flow& f) {
                        const int32_t s = f.seller, u = f.buyer;
                        const std::pair<int32_t, int32_t> key{std::min(s, u), std::max(s, u)};
                        return !std::binary_search(bound.begin(), bound.end(), key);
                    }),
                flows.end());
}

std::vector<trade_flow> compute_trade_flows(const trade_context&             ctx,
                                            const std::vector<region>&       regions,
                                            const std::vector<polity>&       polities,
                                            const std::vector<dated_object>& treaties)
{
    // The bound pairs, canonical (lo, hi), deduplicated -- a pair holding two
    // overlapping trade_access clauses trades once, not twice.
    std::vector<std::pair<uint16_t, uint16_t>> pairs;
    for (const dated_object& o : treaties)
    {
        if (o.kind != static_cast<int32_t>(treaty_clause::trade_access)) continue;
        if (o.a < 0 || o.b < 0 || o.a == o.b || o.a > 0xFFFE || o.b > 0xFFFE) continue;
        pairs.push_back({static_cast<uint16_t>(std::min(o.a, o.b)),
                         static_cast<uint16_t>(std::max(o.a, o.b))});
    }
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

    std::vector<trade_flow> flows;
    for (const auto& pr : pairs)
    {
        const int ends[2][2] = { { pr.first, pr.second }, { pr.second, pr.first } };
        for (const auto& e : ends)
            for (int g = 0; g < 4; ++g)
            {
                const int v = trade_flow_volume_q(ctx, regions, polities, e[0], e[1], g);
                if (v <= 0) continue;
                flows.push_back(trade_flow{static_cast<uint16_t>(e[0]), static_cast<uint16_t>(e[1]),
                                           static_cast<uint8_t>(g), static_cast<int32_t>(v)});
            }
    }

    // ONE WANT, SHARED ACROSS SELLERS. Each flow above is bounded by the
    // buyer's whole raw want, so a buyer bound to three holders would import
    // three wants' worth. The want is the bound on what ARRIVES
    // (EXPLORATION.md sec Trade is a want met by throughput), so it is spent
    // down per (buyer, good): the fattest line first, ties to the lower seller.
    std::sort(flows.begin(), flows.end(), [](const trade_flow& x, const trade_flow& y) {
        if (x.buyer != y.buyer) return x.buyer < y.buyer;
        if (x.good != y.good) return x.good < y.good;
        if (x.volume_q != y.volume_q) return x.volume_q > y.volume_q;
        return x.seller < y.seller;
    });
    {
        std::size_t i = 0;
        while (i < flows.size())
        {
            const uint16_t buyer = flows[i].buyer;
            const uint8_t  good  = flows[i].good;
            const region& seat = regions[static_cast<std::size_t>(
                polities[static_cast<std::size_t>(buyer)].capital)];
            int remaining = std::max(0, seat.scarcity_raw_q[good]);
            for (; i < flows.size() && flows[i].buyer == buyer && flows[i].good == good; ++i)
            {
                const int take = std::min(flows[i].volume_q, remaining);
                flows[i].volume_q = take;
                remaining -= take;
            }
        }
    }
    flows.erase(std::remove_if(flows.begin(), flows.end(),
                               [](const trade_flow& f) { return f.volume_q <= 0; }),
                flows.end());

    // ONE HOLDING, SHARED ACROSS BUYERS -- the mirror of the want above. A
    // seller's `holding_q` bounds each flow it sends, so a seller bound to two
    // buyers would otherwise export its holding twice. Spent down per
    // (seller, good) over what the want spend-down left: the fattest flow
    // first, ties to the lower buyer.
    std::sort(flows.begin(), flows.end(), [](const trade_flow& x, const trade_flow& y) {
        if (x.seller != y.seller) return x.seller < y.seller;
        if (x.good != y.good) return x.good < y.good;
        if (x.volume_q != y.volume_q) return x.volume_q > y.volume_q;
        return x.buyer < y.buyer;
    });
    {
        std::size_t i = 0;
        while (i < flows.size())
        {
            const uint16_t seller = flows[i].seller;
            const uint8_t  good   = flows[i].good;
            int remaining = static_cast<std::size_t>(seller) < ctx.holding_q.size()
                          ? std::max(0, ctx.holding_q[seller][good]) : 0;
            for (; i < flows.size() && flows[i].seller == seller && flows[i].good == good; ++i)
            {
                const int take = std::min(flows[i].volume_q, remaining);
                flows[i].volume_q = take;
                remaining -= take;
            }
        }
    }
    flows.erase(std::remove_if(flows.begin(), flows.end(),
                               [](const trade_flow& f) { return f.volume_q <= 0; }),
                flows.end());

    std::sort(flows.begin(), flows.end(), [](const trade_flow& x, const trade_flow& y) {
        if (x.seller != y.seller) return x.seller < y.seller;
        if (x.buyer != y.buyer) return x.buyer < y.buyer;
        return x.good < y.good;
    });
    return flows;
}

history_sim_state run_history_sim(settlement_state&         ss,
                                  const creed_state*        cs,
                                  const sim_terrain_view&   terrain,
                                  int                       gw,
                                  int                       gh,
                                  const history_sim_params& params,
                                  uint32_t                  seed,
                                  std::atomic<int>*         year_progress,
                                  const works_registry*     works,
                                  era_lapse_tap*            tap)
{
    history_sim_profile& prof = history_sim_last_profile();
    prof = history_sim_profile{}; // this run's split, never the last one's.

    history_sim_state out;
    if (ss.regions.empty() || params.stop_year <= params.start_year)
        return out;

    // BL-1096: the purchase fork's constants are judged ONCE, at the open, and
    // a run whose pair leaves its domain buys nothing for the whole run and
    // says so -- rejected, never clamped. `subjection_verb` also refuses on
    // its own; this flag is the report.
    out.subjection_purchase_params_rejected = !subjection_purchase_params_valid(params);

    // BL-914: seed the tap's geometry mirror with the regions this call
    // already opens on (round 4 always does — settlement/migration ran
    // first), so the very first renderer poll already has something to draw
    // a map from. `tap_region_col/row/name` is this run's own running,
    // append-only flattening of `ss.regions`, kept local because
    // `era_timelapse.hpp` cannot depend on `settlement.hpp`'s `region` type.
    std::vector<int32_t>     tap_region_col;
    std::vector<int32_t>     tap_region_row;
    std::vector<std::string> tap_region_name;
    if (tap != nullptr)
    {
        tap_region_col.reserve(ss.regions.size());
        tap_region_row.reserve(ss.regions.size());
        tap_region_name.reserve(ss.regions.size());
        for (const region& r : ss.regions)
        {
            tap_region_col.push_back(r.col);
            tap_region_row.push_back(r.row);
            tap_region_name.push_back(r.name);
        }
        tap->publish_regions(tap_region_col, tap_region_row, tap_region_name);
    }

    // BL-1088 -- A REALM IS NAMED ONCE, AT FOUNDING, IN ITS FOUNDING CULTURE'S
    // TONGUE (CIVILISATION.md sec A realm's name; the register in
    // NATION_GENERATION.md sec Pass 5). Called at every site that pushes a new
    // polity onto the table, and nowhere else: a resumed span inherits its
    // realms' names with the rest of the table and coins nothing for them.
    // `coin_realm_name` is a pure function of the tongue and the realm -- its
    // stream is the tongue's own signature crossed with (id, seat), never this
    // run's RNG -- so naming draws nothing from the decision loop and moves no
    // digest. The name is read by nothing below; it rides the table onto the
    // record (`as_timelapse`) and into nation generation.
    const auto coin_name_for = [&](polity& q) {
        if (cs == nullptr || q.culture < 0 || q.culture >= static_cast<int>(cs->cultures.size()))
            return;
        const uint64_t salt = (static_cast<uint64_t>(static_cast<uint32_t>(q.id)) << 20)
                            ^ static_cast<uint64_t>(static_cast<uint32_t>(q.capital < 0 ? 0 : q.capital));
        q.name = coin_realm_name(cs->cultures[static_cast<std::size_t>(q.culture)].speech, salt);
    };
    // The tap's append-only mirror of the realm name table (same contract as
    // the region geometry above); refreshed beside each `publish`. Distinct
    // from the tap's own `publish_names` (BL-1106's civilisation and creed
    // tables), which rides after the publish.
    std::vector<std::string> tap_polity_name;
    const auto publish_polity_names = [&]() {
        if (tap == nullptr) return;
        for (std::size_t i = tap_polity_name.size(); i < out.polities.size(); ++i)
            tap_polity_name.push_back(out.polities[i].name);
        tap->publish_polity_names(tap_polity_name);
    };

    // Region -> owning polity, -1 for unorganised or unowned ground.
    std::vector<int> owner(ss.regions.size(), -1);

    // -----------------------------------------------------------------------
    // BL-931 — RESUMING A PRIOR SPAN'S CLOSE, rather than seeding a fresh
    // opening. See `history_sim_params::resume_polities`: null is every
    // caller before this item, unconditionally, so the whole `else` branch
    // below reproduces the pre-BL-931 opening line for line.
    // -----------------------------------------------------------------------
    if (params.resume_polities != nullptr)
    {
        // THE CALLER ALREADY SET `ss.regions` TO THE CLOSING STATE (typically
        // `pass_one_output::regions`): population, `army_stock`,
        // `is_seat`/`seat_region` and `nation` are all live, so none of the
        // seeding, polity-construction, great-power or seat-placement work
        // below runs again. `nation` is read straight into `owner`, which is
        // the one working copy the rest of this function actually consults —
        // BL-769's own comment on the field is exactly this reuse.
        out.polities = *params.resume_polities;
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
            owner[i] = ss.regions[i].nation;
        if (params.resume_grudges  != nullptr) out.grudges  = *params.resume_grudges;
        if (params.resume_contacts != nullptr) out.contacts = *params.resume_contacts;
        if (params.resume_corridors != nullptr)
            out.supply_corridors = *params.resume_corridors;
        // BL-1097: the prior span's sea-leg record, on the same footing as the
        // road record above -- inherited whole, and the close's fold SUMS this
        // span's crossings onto it.
        if (params.resume_sea_legs != nullptr)
            out.sea_legs = *params.resume_sea_legs;

        // BL-1036: the three tables BL-931's four left behind. Copied as
        // handed -- the handoff sorts its dated objects and the sim appends in
        // formation order, and every read of the table is order-free (a
        // clause lookup, a sorted pair set, an expiry filter). Round 1's own
        // `expire_dated_objects` still runs before any read.
        if (params.resume_dated_objects != nullptr)
            out.dated_objects = *params.resume_dated_objects;
        if (params.resume_civilisations != nullptr)
            out.civilisations = *params.resume_civilisations;
        if (params.resume_universal_creeds != nullptr)
            out.universal_creeds = *params.resume_universal_creeds;
    }
    else
    {
    // Population and manpower are seeded for EVERY region either way — the
    // city-state test (BL-920) and the old plurality seed (BL-826) both need
    // a headcount to read or to grow from.
    for (region& p : ss.regions)
    {
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

    // BL-920 -- FALSE BY DEFAULT (struct default), TRUE ON GENERATION'S OWN
    // ROUND (`era_minus_one_sim_params`). See the field comment on
    // `city_states_by_population_threshold`: dozens of `history_sim_harness`
    // fixtures build a synthetic world whose whole premise is BL-826's old
    // seed, isolating a DIFFERENT mechanism on top of it, and this keeps
    // every one of them meaning exactly what it always meant.
    if (params.city_states_by_population_threshold)
    {
        // --- BL-920: THE OPENING SEEDS CULTURE GROUND, NOT POLITIES -------
        //
        // CIVILISATION.md sec "A city state spawns where a region's
        // population is above a threshold" (Ben, 2026-09-11, ruling on
        // NR-835): "City states spawn in provinces above a threshold
        // population... 300,000 heads, provisional." The rule is per
        // REGION, never per culture, and never a quota: a region whose
        // population already stands above `city_state_population_threshold`
        // IS a city state; everything else opens as UNORGANISED ground of
        // its culture (`region::nation == -1`), grown later by the ORGANISE
        // verb, taken by conquest, or promoted in its own right once ITS
        // population crosses the line (the per-year rise check further down
        // this function).
        //
        // This SUPERSEDES BL-826's one-polity-per-plurality-culture seed,
        // which put one polity per distinct people on the map and handed it
        // EVERY region of that culture — 24 to 60 culture-blobs, never a
        // field of city states, and every scheduled founding thereafter
        // free hinterland for the incumbent.
        //
        // ONE POLITY PER QUALIFYING REGION, walked in ascending region
        // index — the order the rest of this file uses — so id assignment
        // is a property of the map, never of a culture list's own order. A
        // culture that clears the threshold on several regions at once opens
        // with several separate city states, not one polity holding several
        // seats; growing into an empire is what the rest of this phase is
        // for.
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
        {
            region& p = ss.regions[i];
            const int pc = p.culture.plurality();
            if (pc < 0) continue;
            if (p.population < params.city_state_population_threshold) continue;

            polity q;
            q.id      = static_cast<int>(out.polities.size());
            q.culture = pc;
            if (cs && pc < static_cast<int>(cs->cultures.size()))
                q.aggression_q = leaned_aggression_q(
                    params, cs->cultures[static_cast<std::size_t>(pc)].aggression_q);
            else
                q.aggression_q = 500; // Neutral when creeds were not supplied.
            q.capital = static_cast<int>(i);

            p.nation      = q.id;
            p.is_seat     = true;
            p.seat_region = static_cast<int>(i);
            owner[i]      = q.id;
            coin_name_for(q); // BL-1088
            out.polities.push_back(q);
        }
        // NO EARLY RETURN ON AN EMPTY OPENING (BL-920, superseding the old
        // guard below). Under the one-per-culture seed a run with no
        // polities meant a region set with no cultures at all -- a fact
        // about the map that made the rest of this function meaningless.
        // Under the threshold seed, ZERO regions clearing
        // `city_state_population_threshold` at the opening is a LEGITIMATE,
        // even likely, outcome for a sparsely-farmed world -- the ruling's
        // own "a minority of regions... never a quota" language admits a
        // minority of zero. The founding schedule and the per-year rise
        // check below can still seed the first city state from nothing, so
        // returning here would silence a world the mechanism is built to
        // grow.
    }
    else
    {
        // --- BL-826: ONE POLITY PER PLURALITY CULTURE (the old seed) -----
        //
        // At the antiquity start `region::nation` is -1: the political pass
        // has not run, and a pre-national world's actors ARE its peoples
        // (BL-221). A polity is seeded per distinct people on the map, and a
        // people is present where it is the largest share. At the seed the
        // shares are pure (settlement writes `pure`), so this is exactly the
        // old set; it only differs on a state handed in mid-history.
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

            // Capital: the best-settled region of this culture. Ties break on
            // the lower index, which is placement order (best ground first).
            int best = -1, best_q = -1;
            for (std::size_t i = 0; i < ss.regions.size(); ++i)
            {
                const region& p = ss.regions[i];
                if (p.culture.plurality() != c) continue;
                if (p.settle_score_q > best_q) { best_q = p.settle_score_q; best = static_cast<int>(i); }
            }
            q.capital = best;
            coin_name_for(q); // BL-1088
            out.polities.push_back(q);
        }
        if (out.polities.empty()) return out;

        for (std::size_t i = 0; i < ss.regions.size(); ++i)
            for (const polity& q : out.polities)
                if (q.culture == ss.regions[i].culture.plurality()) { owner[i] = q.id; break; }
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
            ss.regions[i].nation = owner[i];
    }

    // --- Great-power seed (BL-299) ----------------------------------------
    //
    // Two majors with OPPOSED strategic creeds — one preserving, one on a
    // civilising mission — in a world whose periphery stays multipolar. The
    // periphery is not terrain: minors keep their own cultures, doctrines and
    // recorded history, exactly as the item's richness clause requires. All
    // this seed does is set two aggressions apart and mark the pair.
    if (params.seed_great_powers && out.polities.size() >= 2)
    {
        // Largest two, ties to the lower id so the choice does not depend on
        // iteration order. BL-920: a polity opens as ONE seat rather than a
        // whole culture's ground under the new seed, so SIZE is read as the
        // seat's own population; under the old seed a polity IS a whole
        // culture's ground, so size stays the share-weighted region count
        // BL-826 measured it as (a half-held region is half a holding).
        std::vector<std::pair<int64_t, int>> by_size; // (size, id)
        for (const polity& q : out.polities)
        {
            int64_t size = 0;
            if (params.city_states_by_population_threshold)
                size = (q.capital >= 0 && static_cast<std::size_t>(q.capital) < ss.regions.size())
                     ? ss.regions[static_cast<std::size_t>(q.capital)].population : 0;
            else
                for (const region& p : ss.regions) size += p.culture.share_of(q.culture) / 1000;
            by_size.push_back({size, q.id});
        }
        std::sort(by_size.begin(), by_size.end(),
                  [](const std::pair<int64_t, int>& a, const std::pair<int64_t, int>& b) {
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

    if (params.city_states_by_population_threshold)
    {
        // --- BL-920: NEAREST-SEAT POINTER FOR UNORGANISED GROUND ---------
        //
        // The region adjacency graph (`neighbours`) is grown below from the
        // FINAL region set and does not exist yet, so the opening's own
        // pointer is geometric — the same Chebyshev `region_distance` a
        // campaign's own target choice reads. It is a POINTER ONLY:
        // unorganised ground carries no `nation` and is nobody's holding,
        // but a hinterland region has always pointed at the seat it feeds
        // (§ The unit is the city state), and unorganised ground needs the
        // same fact to answer "nearest to whom" for the render and for
        // ORGANISE's own reach test below.
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
        {
            region& p = ss.regions[i];
            if (p.is_seat) continue; // Already points at itself, above.
            int nearest = -1, nearest_d = 1 << 30;
            for (const polity& q : out.polities)
            {
                if (q.capital < 0) continue;
                const int d = region_distance(p, ss.regions[static_cast<std::size_t>(q.capital)], gw);
                if (d < nearest_d) { nearest_d = d; nearest = q.capital; }
            }
            p.seat_region = nearest;
        }
    }
    else
    {
        // --- SETTLEMENT SEATS, SPARSE FROM THE OPENING MAP (BL-866) ------
        //
        // CIVILISATION.md § The unit is the city state: "a settlement is a
        // SEAT FLAG on a region, and every region points at the seat it
        // feeds." Each polity's `capital` above is already derived as "the
        // best-settled region of this culture" — exactly what a founding
        // city state's seat is — so this reuses that choice rather than
        // running a second placement pass.
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
    }
    } // BL-931: end of the non-resumed opening (`params.resume_polities == nullptr`).

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

    // BL-1097 -- THE SEA-LEG RECORD, on the road record's exact footing: a
    // raw (lo, hi) list appended as legs are crossed and folded into
    // `out.sea_legs` at the close, plus a live count keyed like
    // `road_uses_live` (point lookups only, never iterated) that
    // `note_sea_leg` reads to notice the one tier crossing a lane has. The
    // sim reads neither back: reach and supply still price water as they
    // did, and the stamp is a later pass's (EXPLORATION.md sec The colonial
    // tie is a sea lane). Capture copies the live map SORTED (see
    // `capture_state`), so nothing about the map's layout can reach an
    // output.
    std::vector<std::pair<uint16_t, uint16_t>> sea_leg_uses;
    std::unordered_map<uint64_t, int>          sea_uses_live;

    // BL-925 -- which cross-border corridors are CURRENTLY open to amicable
    // trade, keyed the same way as `road_uses_live` above. Point lookups and
    // point writes only, so the plain `unordered_map` note above applies here
    // unchanged. Holds `true` only while the pair is presently open; an
    // absent key and a `false` value both read as closed, but the sim keeps
    // the entry once opened so a re-open never re-notes the same corridor as
    // new (the event layer wants ONE transition per state change, not one per
    // year the state holds).
    std::unordered_map<uint64_t, bool> trade_link_open_live;

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
        // BL-940: the third rung. ORDINARY TRAFFIC is not meant to reach
        // `road_tier3_uses` (see that field's own comment) — it is bought,
        // by `try_build_post_road` below, which sets the live count straight
        // to the threshold rather than incrementing toward it.
        if (uses >= params.road_tier3_uses) return 3;
        if (uses >= params.road_tier2_uses) return 2;
        if (uses >= params.road_tier1_uses) return 1;
        return 0;
    };
    // Mirrors logistics.hpp's `road_traversal_multiplier` exactly (1 / (1 +
    // 0.5 x tier): Track ~0.67, Road 0.50) — the SAME discount shape the
    // campaign-era A* already applies for the same reason, restated here
    // rather than called across the ECS boundary this file is deliberately
    // free of (settlement.hpp: "no `world&`, no tile ids, no allocator").
    //
    // BL-922: INTEGER, not float. 1 / (1 + 0.5 x tier) == 2 / (2 + tier), so
    // the discounted step is `raw x 2 / (2 + tier)` rounded half-up in
    // integer arithmetic. The float form it replaces was deterministic on
    // one machine and one compiler; this one is deterministic by construction.
    const auto road_discounted = [](int raw, int tier) {
        const int den = 2 + tier;
        return (raw * 2 + den / 2) / den;
    };
    const auto road_tier_between = [&](int a, int b) {
        const auto it = road_uses_live.find(edge_key(a, b));
        return it != road_uses_live.end() ? road_tier_for_uses(it->second) : 0;
    };

    // BL-949 (b) -- A RESUMED SPAN STARTS ON THE NETWORK IT INHERITED. The
    // prior span's record is copied into `out.supply_corridors` at the top of
    // this function, and the fold at the close SUMS this span's walks onto it
    // -- so without this seed the record would say a corridor is a Road while
    // the live count `rebuild_reach` reads starts it at tier 0, and a line the
    // Empires round paved would cost the Exploration span as if nobody had
    // ever walked it. Seeded from `uses` (the record's own traffic), walked in
    // the record's own sorted order; the map is point-looked-up only, so the
    // order cannot reach an output anyway. BL-1037: `uses` under-reads a
    // BOUGHT rung and over-reads a REFUSED walk, so with
    // `resume_seeds_corridor_tier` on the seed is clamped to the record's
    // `tier` instead (see that param and the branch below).
    if (params.resume_polities != nullptr && params.resume_live_roads != nullptr)
    {
        // BL-1036 FIDELITY ORACLE: a continued run's own live network, handed
        // over verbatim (see `history_sim_params::resume_live_roads`). A row
        // of zero uses still creates its edge -- presence is read.
        for (const history_corridor& c : *params.resume_live_roads)
        {
            if (c.a == c.b || c.uses < 0) continue;
            if (c.a >= owner_index_limit || c.b >= owner_index_limit) continue;
            road_uses_live[edge_key(c.a, c.b)] += c.uses;
        }
    }
    else if (params.resume_polities != nullptr && params.resume_corridors != nullptr)
    {
        // BL-1037 -- THE RUNG, NOT THE WALKS (`resume_seeds_corridor_tier`).
        // With the switch on, the live count is the record's `uses` clamped
        // into the band of the rung the record carries, so the corridor
        // reopens at exactly that rung: a bought rung is not demoted to what
        // its walks earn, and a refused walk is not promoted for free.
        const auto tier_threshold = [&](int t) -> int {
            return t <= 0 ? 0
                 : t == 1 ? params.road_tier1_uses
                 : t == 2 ? params.road_tier2_uses
                          : params.road_tier3_uses;
        };
        for (const history_corridor& c : *params.resume_corridors)
        {
            if (c.a == c.b || c.uses <= 0) continue;
            if (c.a >= owner_index_limit || c.b >= owner_index_limit) continue;
            if (!params.resume_seeds_corridor_tier)
            {
                road_uses_live[edge_key(c.a, c.b)] += c.uses;
                continue;
            }
            const int tier = std::min<int>(c.tier, 3);
            int live = std::max(c.uses, tier_threshold(tier));
            if (tier < 3)
                live = std::max(tier_threshold(tier), std::min(live, tier_threshold(tier + 1) - 1));
            // The record holds one row per edge; `max` rather than `+=` so a
            // duplicate row could never carry an edge past its rung.
            int& slot = road_uses_live[edge_key(c.a, c.b)];
            slot = std::max(slot, live);
        }
    }

    // BL-1097 -- A RESUMED SPAN OPENS ON THE LEGS IT INHERITED, seeded from
    // the record's own `uses` (a lane is earned by traffic only, so there is
    // no bought rung to clamp to, and the walks ARE the count). Walked in the
    // record's sorted order; point-inserted only, so the order cannot reach
    // an output. A leg already over the tier opens as a lane: no
    // `sea_lane_opened` is re-noted for it, since the event is the CROSSING
    // and the crossing happened in the span that recorded it.
    if (params.resume_polities != nullptr && params.resume_sea_legs != nullptr)
        for (const sea_leg& l : *params.resume_sea_legs)
        {
            if (l.a == l.b || l.uses <= 0) continue;
            if (l.a >= owner_index_limit || l.b >= owner_index_limit) continue;
            sea_uses_live[edge_key(l.a, l.b)] += l.uses;
        }

    // BL-922 -- SUPPLY IS PRICED FROM THE CAPITAL OVER HELD GROUND ONLY, so
    // reach depends on WHO HOLDS WHAT, and a cache built against one
    // ownership map is stale against the next. One counter per polity,
    // bumped for the loser and the gainer whenever a region changes hands
    // (conquest, founding, secession), so a conquest between two other realms
    // costs this one no rebuild. Indexed by `polity::id`, grown on demand.
    std::vector<int> owner_version_by_polity;
    const auto touch_owner = [&](int pid) {
        if (pid < 0) return;
        if (owner_version_by_polity.size() <= static_cast<std::size_t>(pid))
            owner_version_by_polity.resize(static_cast<std::size_t>(pid) + 1, 0);
        ++owner_version_by_polity[static_cast<std::size_t>(pid)];
    };
    const auto owner_version_of = [&](int pid) -> int {
        if (pid < 0 || owner_version_by_polity.size() <= static_cast<std::size_t>(pid)) return 0;
        return owner_version_by_polity[static_cast<std::size_t>(pid)];
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

    // BL-1021 -- GROUND THE NETWORK REACHES FOR TRADE. The same reading and the
    // same comparison that gates a town's growth (`advance_region_urban`'s
    // caller below): `sustainable_settlement_floor_q` is the floor whose own
    // comment defines it as "a network that can carry ordinary trade". One
    // decision round stale, exactly as that gate is.
    const auto reached_for_trade = [&](std::size_t i) -> bool {
        return ss.regions[i].network_supply_q > params.sustainable_settlement_floor_q;
    };

    // BL-1021 -- WHAT KINDS OF GROUND EACH REALM REACHES, one bit per
    // `region_trade_class`, indexed by polity id. Rebuilt once a year before
    // the demography pass pays anyone, so every region is paid against the
    // same settled reading whatever its index.
    std::vector<uint8_t> realm_kinds_reached;
    std::vector<int32_t> realm_regions_held; // same year, every held region, reached or not
    // The BL-895 reading's bucket: SIZE BAND x 4 + KINDS REACHED (0..3), where
    // the size band is regions held 1 / 2-3 / 4-7 / 8+ (0..3). Size is in the
    // key because a connected realm is usually a larger one, and a reading that
    // did not hold size fixed would credit trade with what industry did.
    const auto trade_bucket_of = [&](int qid) -> std::size_t {
        if (qid < 0 || static_cast<std::size_t>(qid) >= realm_kinds_reached.size()) return 0;
        const unsigned m = realm_kinds_reached[static_cast<std::size_t>(qid)];
        const std::size_t kinds = (m & 1u) + ((m >> 1) & 1u) + ((m >> 2) & 1u);
        const int32_t held = realm_regions_held[static_cast<std::size_t>(qid)];
        const std::size_t band = held >= 8 ? 3u : held >= 4 ? 2u : held >= 2 ? 1u : 0u;
        return band * 4u + kinds;
    };

    // BL-925 -- WHEN TWO NEIGHBOURING POLITIES ARE AMICABLE. Kin cultures
    // (opposition below `organise_opposition_bar_q`, the SAME bar Organise
    // already reads -- CIVILISATION.md's kinship ladder is one bar, not two)
    // AND neither holds a biting grudge against the other, read BOTH
    // directions of `grudge_between` because a one-sided wrong is enough to
    // sour a border. `cs == nullptr` (a fixture isolating a different
    // mechanism, same convention as every other `cs` read in this file)
    // refuses rather than defaults open -- no culture data means no kinship
    // claim can be made.
    const auto polities_amicable = [&](int qa, int qb) -> bool {
        if (qa < 0 || qb < 0 || qa == qb) return false;
        if (static_cast<std::size_t>(qa) >= out.polities.size()
         || static_cast<std::size_t>(qb) >= out.polities.size()) return false;
        if (cs == nullptr) return false;
        const int ca = out.polities[static_cast<std::size_t>(qa)].culture;
        const int cb = out.polities[static_cast<std::size_t>(qb)].culture;
        if (ca < 0 || cb < 0) return false;
        if (ca >= static_cast<int>(cs->cultures.size())
         || cb >= static_cast<int>(cs->cultures.size())) return false;
        if (culture_opposition_q(cs->cultures, ca, cb) >= params.organise_opposition_bar_q)
            return false;
        if (params.trade_grudge_bar_q > 0)
        {
            if (grudge_between(out, qa, qb) >= params.trade_grudge_bar_q) return false;
            if (grudge_between(out, qb, qa) >= params.trade_grudge_bar_q) return false;
        }
        return true;
    };

    // --- THE EVENT LAYER (BL-916) -----------------------------------------
    //
    // A pure record, appended at the sites that already push a prose line into
    // `out.history`, and read by nothing below. `event_year` is the sim's
    // current year as the recorder sees it: assigned at the top of every year
    // of the loop and read ONLY here, so a closure defined before the loop
    // (`note_corridor`) can date what it records without a clock of its own.
    // Gated on `record_playback` with the rest of the record, so the harness
    // holds a recorded and a suppressed run bit-identical on everything else.
    int64_t event_year = params.start_year;
    const auto note_event = [&](lapse_event_kind kind, int region_idx, int polity_id,
                                int other_id) {
        if (!params.record_playback) return;
        const auto slot = [](int v) {
            return (v < 0 || v >= static_cast<int>(lapse_event_none))
                       ? lapse_event_none : static_cast<uint16_t>(v);
        };
        lapse_event e;
        e.year   = static_cast<int32_t>(event_year);
        e.kind   = static_cast<uint8_t>(kind);
        e.region = slot(region_idx);
        e.polity = slot(polity_id);
        e.other  = slot(other_id);
        out.events.push_back(e);
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
    // materials standing. Poverty DELAYS a road; it does not forbid one. The
    // RECORD still counts the refused walk, so its `uses` can stand on a rung
    // the live count never paid for; a resumed span that seeds from `tier`
    // (BL-1037, `resume_seeds_corridor_tier`) reopens it one short, as here.
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
        // BL-916: the promotion is the event, not the walk — a walk is the
        // corridor record's business, and a tier crossing is what a watcher
        // can see on the map.
        note_event(lapse_event_kind::road_promoted, lo, after, hi);
    };

    // BL-1097 -- ONE SEA LEG CROSSED, recorded where it is crossed. The
    // water sibling of `note_corridor` with the money taken out: a lane is
    // earned by traffic alone (EXPLORATION.md sec The colonial tie is a sea
    // lane: "traffic earns the tier, and a crossing made once is no lane"),
    // so there is no promotion to refuse and no seat to debit. The walk is
    // always recorded; the ONE tier crossing (`sea_lane_tier1_uses`) notes a
    // `sea_lane_opened` event, and that event is the only thing outside the
    // fold that ever hears of a leg. Nothing below reads the count back.
    const auto note_sea_leg = [&](int a, int b) {
        if (a < 0 || b < 0 || a == b) return;
        if (a >= static_cast<int>(owner_index_limit)
         || b >= static_cast<int>(owner_index_limit)) return;
        const uint16_t lo = static_cast<uint16_t>(a < b ? a : b);
        const uint16_t hi = static_cast<uint16_t>(a < b ? b : a);
        sea_leg_uses.push_back({lo, hi});

        int& uses = sea_uses_live[edge_key(a, b)];
        const bool before = uses >= params.sea_lane_tier1_uses;
        ++uses;
        const bool after = uses >= params.sea_lane_tier1_uses;
        if (after && !before)
        {
            ++out.sea_lanes_opened;
            note_event(lapse_event_kind::sea_lane_opened, lo, 1, hi);
        }
    };

    // BL-929 -- A CORRIDOR BOUGHT OUTRIGHT, never merely walked into
    // existence. Sets the edge's live use count straight to the threshold
    // its NEXT tier needs, so a purchase always crosses a boundary — there
    // is no "purchase that changed nothing" the way one more incidental walk
    // can be. `note_corridor`'s refusal model is deliberately NOT reused: a
    // refused walk is held one short because the corridor will be walked
    // again on its own; a refused PURCHASE simply did not happen, so the
    // uses count is left untouched rather than nudged.
    //
    // THE RECORD UNDER-READS A BOUGHT RUNG (was BL-959): the promotion appends
    // ONE row to the corridor record, so the record's `uses` under-reads the
    // tier it grants. The carried `history_corridor::tier` is right, and a
    // reader that wants the rung reads `tier`, never infers it from `uses`.
    // WHAT NOW HOLDS (BL-1037): the resume is that reader. With
    // `history_sim_params::resume_seeds_corridor_tier` on, a resumed span seeds
    // this edge's live count from `tier`, so the rung bought here survives the
    // span boundary (on by default since BL-1044); off -- the legacy arc -- the
    // next span reopens it at the rung its walks earn.
    // `uses` itself stays traffic either way: it counts walks, never rungs.
    const auto try_upgrade_corridor = [&](int a, int b, int payer_seat) -> bool {
        if (a < 0 || b < 0 || a == b) return false;
        if (a >= static_cast<int>(owner_index_limit)
         || b >= static_cast<int>(owner_index_limit)) return false;
        if (params.supply_upgrade_material_cost <= 0) return false;
        const int cur_tier = road_tier_between(a, b);
        if (cur_tier >= 2) return false; // already at the top tier — nothing left to buy
        if (payer_seat < 0 || static_cast<std::size_t>(payer_seat) >= ss.regions.size())
            return false;
        region& seat = ss.regions[static_cast<std::size_t>(payer_seat)];
        if (seat.material_stock < params.supply_upgrade_material_cost) return false;

        seat.material_stock -= params.supply_upgrade_material_cost;
        out.materials_spent_on_supply_sites += params.supply_upgrade_material_cost;
        const int need = cur_tier == 0 ? params.road_tier1_uses : params.road_tier2_uses;
        road_uses_live[edge_key(a, b)] = need;
        ++roads_version;

        const uint16_t lo = static_cast<uint16_t>(a < b ? a : b);
        const uint16_t hi = static_cast<uint16_t>(a < b ? b : a);
        // The finished world's road stamp (`road_generation.cpp`) reads
        // `out.supply_corridors`, itself built from `corridor_uses` at the
        // end of the run (line ~4350 below) — a bought corridor has to
        // appear there or it renders as if it had never been walked at all.
        corridor_uses.push_back({lo, hi});
        note_event(lapse_event_kind::road_promoted, lo, road_tier_for_uses(need), hi);
        return true;
    };

    // BL-940 -- THE ROAD LADDER'S THIRD RUNG, BOUGHT WITH CAPITAL
    // (EXPLORATION.md sec Goods move as throughput: EX-WY-1a, Post Roads).
    // SAME SHAPE AS `try_upgrade_corridor` ABOVE, deliberately not folded
    // into it: that function spends `material_stock` and stops at tier 2 by
    // construction (`cur_tier >= 2` refuses); this one spends the TREASURY
    // (BL-932, `region::treasury`) and is the only path that can ever reach
    // `road_tier3_uses`. Gated on `params.exploration_upkeep_enabled` at the
    // call site, not here, exactly like `try_upgrade_corridor`'s own
    // material-cost gate — a zero cost disables it unconditionally.
    const auto try_build_post_road = [&](int a, int b, int payer_capital) -> bool {
        if (a < 0 || b < 0 || a == b) return false;
        if (a >= static_cast<int>(owner_index_limit)
         || b >= static_cast<int>(owner_index_limit)) return false;
        if (params.post_road_treasury_cost <= 0) return false;
        if (road_tier_between(a, b) != 2) return false; // only a Road may become a Post Road
        if (payer_capital < 0 || static_cast<std::size_t>(payer_capital) >= ss.regions.size())
            return false;
        region& seat = ss.regions[static_cast<std::size_t>(payer_capital)];
        if (seat.treasury < params.post_road_treasury_cost) return false;

        seat.treasury -= params.post_road_treasury_cost;
        road_uses_live[edge_key(a, b)] = params.road_tier3_uses;
        ++roads_version;
        ++out.post_roads_built;
        out.treasury_spent_on_roads += params.post_road_treasury_cost;

        const uint16_t lo = static_cast<uint16_t>(a < b ? a : b);
        const uint16_t hi = static_cast<uint16_t>(a < b ? b : a);
        corridor_uses.push_back({lo, hi}); // same reasoning as try_upgrade_corridor's own push.
        note_event(lapse_event_kind::road_promoted, lo, 3, hi);
        return true;
    };

    // BL-929 -- A REGION'S OWN RELIEF, BOUGHT DIRECTLY, rather than only as
    // the incidental yield of `apply_work_to_region` winning `work_score_q`'s
    // contest (BL-757 measured that ZERO times across sixteen real
    // generated worlds). Raises `region::work_reach_mod` by a fixed step,
    // clamped at the SAME `work_reach_relief_cap_q` every other reader of
    // that field already clamps against.
    const auto try_upgrade_region_reach = [&](int hi, int payer_seat) -> bool {
        if (hi < 0 || static_cast<std::size_t>(hi) >= ss.regions.size()) return false;
        if (params.supply_upgrade_material_cost <= 0) return false;
        region& hp = ss.regions[static_cast<std::size_t>(hi)];
        if (hp.work_reach_mod >= params.work_reach_relief_cap_q) return false;
        if (payer_seat < 0 || static_cast<std::size_t>(payer_seat) >= ss.regions.size())
            return false;
        region& seat = ss.regions[static_cast<std::size_t>(payer_seat)];
        if (seat.material_stock < params.supply_upgrade_material_cost) return false;

        seat.material_stock -= params.supply_upgrade_material_cost;
        out.materials_spent_on_supply_sites += params.supply_upgrade_material_cost;
        hp.work_reach_mod = clampi(hp.work_reach_mod + params.supply_upgrade_reach_gain_q,
                                   0, params.work_reach_relief_cap_q);
        return true;
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

    // --- CONTACT (BL-908) --------------------------------------------------
    //
    // Directed, sparse, event-carrying — the grudge table's own shape and
    // discipline (sorted vector, binary search, never a keyed map). The
    // difference from a grudge is what it records: a FACT of meeting, never
    // a magnitude, so there is nothing here to decay.
    const auto contact_slot = [&](int from, int to) -> contact* {
        if (from < 0 || to < 0 || from == to) return nullptr;
        if (from > 0xFFFE || to > 0xFFFE) return nullptr;
        const uint16_t f = static_cast<uint16_t>(from), t = static_cast<uint16_t>(to);
        const auto it = std::lower_bound(
            out.contacts.begin(), out.contacts.end(), std::pair<uint16_t, uint16_t>{f, t},
            [](const contact& c, const std::pair<uint16_t, uint16_t>& k) {
                if (c.from != k.first) return c.from < k.first;
                return c.to < k.second;
            });
        if (it != out.contacts.end() && it->from == f && it->to == t) return &*it;
        contact c;
        c.from = f;
        c.to   = t;
        return &*out.contacts.insert(it, c);
    };

    /// True where the pair already has an entry EITHER way — checked before
    /// inserting rather than after, since `contact_slot` inserts on lookup.
    const auto contact_exists = [&](int from, int to) -> bool {
        if (from < 0 || to < 0 || from > 0xFFFE || to > 0xFFFE) return false;
        const uint16_t f = static_cast<uint16_t>(from), t = static_cast<uint16_t>(to);
        const auto it = std::lower_bound(
            out.contacts.begin(), out.contacts.end(), std::pair<uint16_t, uint16_t>{f, t},
            [](const contact& c, const std::pair<uint16_t, uint16_t>& k) {
                if (c.from != k.first) return c.from < k.first;
                return c.to < k.second;
            });
        return it != out.contacts.end() && it->from == f && it->to == t;
    };

    /// Establish contact in BOTH directions if the pair has never met. A
    /// no-op on a pair that already has an entry — the record keeps the
    /// FIRST meeting, never the latest, because "who met whom first" is the
    /// fact and a later re-contact adds nothing to it.
    const auto raise_contact = [&](int a, int b, contact_kind kind,
                                   int region_idx, int64_t year) {
        if (a < 0 || b < 0 || a == b) return;
        if (contact_exists(a, b)) return; // symmetric: (a,b) implies (b,a)
        if (params.trace_battles) ++out.contacts_raised_trace[static_cast<int>(kind) & 1]; // BL-1019, trace only
        contact_event e;
        e.year   = static_cast<int32_t>(year);
        e.region = (region_idx >= 0 && region_idx < static_cast<int>(owner_index_limit))
                     ? static_cast<uint16_t>(region_idx) : owner_none;
        e.kind   = kind;
        if (contact* c = contact_slot(a, b)) c->first = e;
        if (contact* c = contact_slot(b, a)) c->first = e;
    };

    /// A polity died. ITS GRUDGES ARE LOST, IN BOTH DIRECTIONS — see the
    /// `history_sim_state::grudges` note for why that is the call rather than
    /// inheritance. What survives is `realm_ended`, raised from every surviving
    /// polity of the dead realm's people toward its killer, walked in polity-id
    /// order so the set and its order are both deterministic.
    const auto extinguish_polity = [&](int dead, int killer, int region_idx, int64_t year) {
        const int dead_culture = (dead >= 0 && dead < static_cast<int>(out.polities.size()))
                               ? out.polities[static_cast<std::size_t>(dead)].culture : -1;
        // BL-916: THE DEATH IS RECORDED HERE, at the one site that knows the
        // killer. The step record samples living polities only, so a realm's
        // end used to be an absence a reader had to infer — and inferred wrong.
        note_event(lapse_event_kind::realm_ended, region_idx, dead, killer);
        out.grudges.erase(
            std::remove_if(out.grudges.begin(), out.grudges.end(),
                           [&](const grudge& g) {
                               return g.from == static_cast<uint16_t>(dead)
                                   || g.to   == static_cast<uint16_t>(dead);
                           }),
            out.grudges.end());

        // BL-908 — A CONQUEROR INHERITS WHAT ITS VICTIM KNEW, because the
        // knowledge of who else exists was in the SEAT, not in the dead
        // ruler — the opposite call from the grudge erase just above, on
        // purpose (see the `contacts` field note for why the two differ).
        // Every polity `dead` had met, the killer now has met too, unless
        // the killer already had (`raise_contact` is a no-op there). Walked
        // in ascending polity-id order so the transfer is deterministic
        // regardless of the dead polity's own contact-table order.
        if (killer >= 0)
        {
            for (const polity& other : out.polities)
            {
                if (other.id == dead || other.id == killer) continue;
                if (!contact_exists(dead, other.id)) continue;
                raise_contact(killer, other.id, contact_kind::inherited,
                              region_idx, year);
            }
        }
        out.contacts.erase(
            std::remove_if(out.contacts.begin(), out.contacts.end(),
                           [&](const contact& c) {
                               return c.from == static_cast<uint16_t>(dead)
                                   || c.to   == static_cast<uint16_t>(dead);
                           }),
            out.contacts.end());

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
    //
    // BL-922 -- A SECOND, UNCAPPED INDEX FOR SUPPLY. The degree cap above is
    // a bound on CAMPAIGN candidates, and it is first-come: a region founded
    // into a full neighbourhood may carry no edge to its own parent, or to
    // any region of its own realm, while standing a few tiles from them.
    // While reach was ownership-blind that was harmless -- some path existed.
    // Priced over HELD ground only, it is not: measured on this build tree,
    // 70-80% of the ground that seceded under the capped index had a FED
    // region of its own realm within `neighbour_radius` (history_sweep's
    // BL-922 lines). So supply walks `supply_neighbours` -- every region
    // within `neighbour_radius`, no cap -- and the campaign scorer keeps
    // `neighbours`. Affordable precisely because the walk is held-only: a
    // rebuild touches a realm's own regions' lists, never the whole map's.
    std::vector<std::vector<int>> neighbours(ss.regions.size());
    std::vector<std::vector<int>> supply_neighbours(ss.regions.size());
    std::vector<int> degree(ss.regions.size(), 0);
    const auto link_region = [&](std::size_t i) {
        std::vector<std::pair<int, int>> candidates; // (distance, region index)
        for (std::size_t j = 0; j < i; ++j)
        {
            const int d = region_distance(ss.regions[i], ss.regions[j], gw);
            if (d > params.neighbour_radius) continue;
            supply_neighbours[i].push_back(static_cast<int>(j)); // BL-922: uncapped
            supply_neighbours[j].push_back(static_cast<int>(i));
            if (degree[j] >= params.max_neighbour_degree) continue; // No free slot.
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
    // graph, the terrain under each region's anchor, the road tiers, the
    // centre map (relay on) and -- since BL-922 -- WHICH REGIONS THIS POLITY
    // HOLDS. Terrain is fixed for the run and an anchor is written once, at
    // founding. The graph is mutated in exactly one place — `link_region`,
    // called only when a region is founded — and that founding also grows
    // `ss.regions`. A region founded by ANOTHER polity is ground this polity
    // does not hold, which the held-only walk below never expands, so a grown
    // region count alone only extends the vector with the sentinel; a region
    // founded by THIS polity bumps its `owner_version` and rebuilds. The
    // capital check catches the only two places a capital is assigned.
    struct reach_cache
    {
        std::vector<int> cost;    ///< Per-region cost from `capital`, over HELD ground.
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
        /// BL-922: this polity's `owner_version_by_polity` entry the vector
        /// was built against. Supply crosses HELD ground only, so a region
        /// gained or lost changes what the capital can reach -- the FIFTH
        /// input, and the one a conquest moves.
        int              owner_version = -1;
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

    // THE COST OF ONE STEP between two regions: Chebyshev distance weighted by
    // the mean landform ratio of the two ends, then discounted by whatever
    // road tier this history has walked onto that line (BL-837, mirroring
    // logistics.cpp's `road_traversal_multiplier`: Track ~0.67, Road 0.50).
    // Never below 1, so no edge is free. ONE definition, read by the Dijkstra
    // below and by `campaign_supply`'s last hop onto a target (BL-922) -- the
    // file's standing thesis: a cost authored on one scale and spent on
    // another is the bug, so both ask the identical question.
    const auto edge_step = [&](int a, int b) {
        const region& ra = ss.regions[static_cast<std::size_t>(a)];
        const region& rb = ss.regions[static_cast<std::size_t>(b)];
        const int raw_step = region_distance(ra, rb, gw) * (tile_cost(ra) + tile_cost(rb)) / 200;
        return raw_step > 0 ? std::max(1, road_discounted(raw_step, road_tier_between(a, b))) : 1;
    };

    // BL-929's own reading of the same step, AT A TIER THE EDGE DOES NOT YET
    // CARRY -- what would this edge cost if the corridor were promoted right
    // now. `edge_step` cannot answer this: it always reads the edge's LIVE
    // tier. Scoring "should I buy the next tier" needs the cost the purchase
    // would actually produce, asked without touching `road_uses_live` --
    // the scorer must be free to ask this of an edge it does not buy.
    const auto edge_step_at_tier = [&](int a, int b, int tier) {
        const region& ra = ss.regions[static_cast<std::size_t>(a)];
        const region& rb = ss.regions[static_cast<std::size_t>(b)];
        const int raw_step = region_distance(ra, rb, gw) * (tile_cost(ra) + tile_cost(rb)) / 200;
        return raw_step > 0 ? std::max(1, road_discounted(raw_step, tier)) : 1;
    };

    // BL-922 -- THE CAPITAL IS THE STRATEGIC HEADQUARTERS (CIVILISATION.md
    // § How an empire actually falls, Ben 2026-09-11). Reach is a Dijkstra
    // from the polity's capital over the ground the polity HOLDS: a region
    // held by anyone else, or by nobody, is IMPASSABLE to supply. It is
    // never relaxed and never expanded, so its cost stays at the sentinel.
    // The consequence is the one the design names: a war that takes the
    // ground between a capital and its far block cuts that block off, and
    // the block's `network_supply_q` reads as unreachable on the next round
    // rather than as if the foreign ground between were a road.
    //
    // Campaign TARGETS are foreign by definition and so never carry a cost
    // here; `campaign_supply` prices one as the staging hub's cost plus ONE
    // `edge_step` onto the target (the last hop of a march is over ground
    // the polity does not yet hold, and that is priced, not forbidden).
    const auto rebuild_reach = [&](reach_cache& rc, int capital, int qid) {
        const scoped_ns prof_reach(prof.ns_reach); // BL-825, report-only
        ++prof.reach_rebuilds;
        std::vector<int>& reach = rc.cost;
        reach.assign(ss.regions.size(), 1 << 28);
        rc.capital         = capital;
        rc.roads_version   = roads_version;
        rc.centres_version = centres_version;
        rc.owner_version   = owner_version_of(qid);
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
        // BL-922 -- ONLY HELD GROUND IS RELAXED. The neighbour loop skips any
        // region whose owner is not this polity, so a foreign or empty region
        // is never enqueued at all, which is the held-only domain stated
        // above expressed as the cheapest possible test. The walk is therefore
        // over a SUBGRAPH of the region graph -- smaller than the whole-map
        // Dijkstra it replaces, by exactly the share of the map this polity
        // does not hold.
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
            for (int nb : supply_neighbours[static_cast<std::size_t>(best)])
            {
                // BL-922: foreign or empty ground is impassable to supply.
                if (owner[static_cast<std::size_t>(nb)] != qid) continue;
                // BL-837 — THE ROAD DISCOUNT is inside `edge_step`, applied
                // to the SAME edge this history's own corridors have actually
                // walked. This is what makes "the only way to reach further
                // is to BUILD further" literally true of the Dijkstra: the
                // capital's effective reach grows along lines the polity has
                // actually used, never along a straight-line radius.
                const int cand = out_c + edge_step(best, nb); // BL-887: `out_c == best_c` with the relay off.
                if (cand < reach[static_cast<std::size_t>(nb)])
                {
                    reach[static_cast<std::size_t>(nb)] = cand;
                    frontier.push({cand, nb});
                }
            }
        }
    };

    // --- Time-lapse change list -------------------------------------------
    out.owner_changes.clear();
    for (std::size_t i = 0; i < owner.size(); ++i)
        if (owner[i] >= 0)
            out.owner_changes.push_back(owner_change{
                static_cast<int32_t>(params.start_year),
                static_cast<uint16_t>(i),
                static_cast<uint16_t>(owner[i])});

    // BL-916: every opening polity is FOUNDED at the start year, at its seat.
    // Ascending id, which is the order the seed above allocated them in.
    //
    // BL-1036: A RESUMED SPAN NOTES ONLY THE LIVING. It inherits the prior
    // span's whole polity table, the dead included (their ids stay valid for
    // every index that names them), and noting a realm that fell centuries
    // ago as founded at this span's open put a ghost on the replay. The event
    // layer only -- `note_event` is read by nothing in the sim and is off
    // under `record_playback == false`, so no decision and no digest moves.
    // BL-1080 -- THE FURNACES THIS SPAN INHERITS. A region that crossed the
    // furnace in an earlier span is industrial from this span's first frame,
    // and the record would otherwise not know it: crossings are noted where
    // they happen (the rung block in the decision loop), and those happened
    // in a record this span does not carry. So each is noted here, AT ITS OWN
    // YEAR -- earlier than this span's start, and ascending (year, then region
    // index), so the list stays ascending ahead of the founded notes below.
    // The event layer only: read by nothing in the sim, off under
    // `record_playback == false`, so no decision and no digest moves.
    if (params.record_playback)
    {
        std::vector<std::pair<int64_t, int>> lit;
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
        {
            const region& r = ss.regions[i];
            if (r.industrialised && r.industrial_year < params.start_year)
                lit.emplace_back(r.industrial_year, static_cast<int>(i));
        }
        std::sort(lit.begin(), lit.end());
        const int64_t year_before = event_year;
        for (const auto& [yr, ri] : lit)
        {
            event_year = yr;
            note_event(lapse_event_kind::furnace_lit, ri,
                       ri < static_cast<int>(owner.size()) ? owner[static_cast<std::size_t>(ri)] : -1,
                       -1);
        }
        event_year = year_before;
    }

    // BL-1088 -- A RESUMED SPAN INHERITS, IT DOES NOT RE-FOUND. A living realm
    // this span takes over from the span before is noted as `inherited` at its
    // capital, dated at this span's open: the record still states where every
    // living realm sits (the only place a resumed record does), the UI's
    // capital fold reads it exactly as a founding, and the ticker stays silent
    // on it because nothing rose (CIVILISATION.md sec A realm's name). The
    // fresh opening keeps `founded`. The event layer only; no digest moves.
    for (const polity& q : out.polities)
    {
        if (params.resume_polities != nullptr && !q.alive) continue;
        note_event(params.resume_polities != nullptr ? lapse_event_kind::inherited
                                                     : lapse_event_kind::founded,
                   q.capital, q.id, -1);
    }


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
    // BL-1080: industry points standing on each polity's ground at the step.
    std::vector<int64_t> step_industry(out.polities.size(), 0);

    const auto record_step = [&](int64_t y_now) {
        if (!params.record_playback) return;

        if (step_pop.size() < out.polities.size())
        {
            step_pop.resize(out.polities.size(), 0);
            step_regions.resize(out.polities.size(), 0);
            step_industry.resize(out.polities.size(), 0);
        }

        std::fill(step_pop.begin(), step_pop.end(), 0);
        std::fill(step_regions.begin(), step_regions.end(), 0);
        std::fill(step_industry.begin(), step_industry.end(), 0);

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
                // Saturating: a region's stock is bounded by
                // industry_points_ceiling (2^60), so a sum over a realm's
                // regions is held below INT64_MAX rather than trusted to fit.
                int64_t& ind = step_industry[static_cast<std::size_t>(o)];
                ind = (r.industry_points > INT64_MAX - ind) ? INT64_MAX
                                                            : ind + std::max<int64_t>(0, r.industry_points);
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
            smp.industry_points = step_industry[pi];
            // BL-1095 -- THE FLEET AND THE HARBOUR, read as they stand: the
            // realm's standing navy and its capital's built port (0-1000).
            // Pure reads of `out.polities` / `ss.regions` into the record; the
            // sim never reads a sample back, so the digest is untouched. A
            // capital off the region table (never, on a living realm) reads 0.
            {
                const polity& pq = out.polities[pi];
                smp.navy_stock = std::max<int64_t>(0, pq.navy_stock);
                if (pq.capital >= 0 && static_cast<std::size_t>(pq.capital) < ss.regions.size())
                    smp.port_stock_q = static_cast<int16_t>(clampi(
                        ss.regions[static_cast<std::size_t>(pq.capital)].port_stock_q, 0, 1000));
            }
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

    // BL-953 -- THE ROUND'S LIVE CULTURAL PREFERENCE (EXPLORATION.md sec
    // Preference is read LIVE). Derived AT MOST ONCE per decision round, from
    // that round's own regions/contacts/polities, and read by every campaign
    // candidate and subjection pick in the round -- never re-derived per
    // candidate. Empty whenever the want lean is off (the Empires span, or
    // `w_want_q` 0), which is what keeps those runs byte-identical.
    const bool want_lean_on = params.exploration_upkeep_enabled && params.w_want_q != 0;
    std::vector<culture_good_preference> round_prefs;
    int want_culture_count = cs ? static_cast<int>(cs->cultures.size()) : 0;
    if (want_lean_on && cs == nullptr)
        for (const region& r : ss.regions)
            want_culture_count = std::max(want_culture_count, r.culture.plurality() + 1);

    // BL-1036 -- THE FIDELITY CAPTURE (`history_sim_params::capture_year`).
    // Pure reads into `out.capture`, which nothing below ever reads back. The
    // live road map is unordered, so its entries are collected and SORTED by
    // (a, b) before they land -- the copy's order is a property of the
    // integers, never of the map's layout.
    const auto capture_state = [&](int64_t at_year) {
        history_sim_capture& cap = out.capture;
        cap.captured         = true;
        cap.year             = at_year;
        cap.regions          = ss.regions;
        cap.owner            = owner;
        cap.polities         = out.polities;
        cap.grudges          = out.grudges;
        cap.contacts         = out.contacts;
        cap.supply_corridors = out.supply_corridors;
        cap.dated_objects    = out.dated_objects;
        cap.trade_flows      = out.trade_flows;
        cap.civilisations    = out.civilisations;
        cap.universal_creeds = out.universal_creeds;
        cap.live_roads.clear();
        cap.live_roads.reserve(road_uses_live.size());
        for (const auto& kv : road_uses_live)
        {
            history_corridor c;
            c.a    = static_cast<uint16_t>(kv.first >> 32);
            c.b    = static_cast<uint16_t>(kv.first & 0xFFFFFFFFull);
            c.uses = kv.second;
            c.tier = static_cast<uint8_t>(road_tier_for_uses(kv.second));
            cap.live_roads.push_back(c);
        }
        std::sort(cap.live_roads.begin(), cap.live_roads.end(),
                  [](const history_corridor& x, const history_corridor& y2) {
                      return x.a != y2.a ? x.a < y2.a : x.b < y2.b;
                  });
        // BL-1097: the live sea-leg table, collected and sorted the same way.
        cap.sea_legs.clear();
        cap.sea_legs.reserve(sea_uses_live.size());
        for (const auto& kv : sea_uses_live)
        {
            if (kv.second <= 0) continue;
            sea_leg l;
            l.a    = static_cast<uint16_t>(kv.first >> 32);
            l.b    = static_cast<uint16_t>(kv.first & 0xFFFFFFFFull);
            l.uses = kv.second;
            cap.sea_legs.push_back(l);
        }
        std::sort(cap.sea_legs.begin(), cap.sea_legs.end(),
                  [](const sea_leg& x, const sea_leg& y2) {
                      return x.a != y2.a ? x.a < y2.a : x.b < y2.b;
                  });
    };

    // BL-1056: did the span-open survey run on the table this call opened on?
    // Read ONCE, here, so "the span did not run" (the Fuel Doctrine's coal pull
    // keeps the seam) is never confused with "this polity holds no surveyed
    // ground" (it reads 0). A pure function of the opening table.
    const bool span_surveyed = industry_span_survey_ran(ss.regions);
    // BL-1059 (Ben, 2026-09-19, NR-899): the two top-third bars, fixed ONCE
    // here from the table the run opened on and never moved: every pull below
    // reads these. Only regions carrying both span-open scores are read, and
    // no score is ever rewritten after the open, so a run resumed mid-span
    // re-derives the same bars. Off the span nothing is surveyed and both
    // read `industry_ground_bar_none` (and nothing reads them).
    out.ground_bars = industry_ground_bars_at_open(ss.regions);
    const industry_ground_bars ground_bars = out.ground_bars;

    for (int64_t y = params.start_year; y < params.stop_year; ++y)
    {
        event_year = y; // BL-916: the recorder's clock, read by `note_event` alone.
        if (y == params.capture_year) capture_state(y); // BL-1036: read-only.

        // Loading-screen sink only — never read back, so the sim stays pure.
        if (year_progress != nullptr)
            year_progress->store(static_cast<int>(y - params.start_year + 1),
                                 std::memory_order_relaxed);

        // BL-914: publish last year's finished record before this year's own
        // work begins. One year behind is deliberate, not a rounding slip —
        // it guarantees the tap never reports a year whose events might still
        // be mid-append, so a renderer reading it can never run ahead of what
        // is actually settled. Write-only: nothing below ever reads `tap` back.
        if (tap != nullptr)
        {
            publish_polity_names(); // BL-1088: the realm name table's new tail, first
            tap->publish(out.owner_changes, out.culture_changes, out.events,
                        static_cast<int32_t>(y - 1));
            // BL-1106: the name table rides the same publish, so a live ticker
            // names a civilisation or creed in the year it is coined. A pure
            // read of `out`; the tap copies only the tail.
            std::vector<std::string> tap_civ, tap_creed;
            std::vector<int32_t>     tap_polity_creed;
            fill_lapse_name_table(out, tap_civ, tap_creed, tap_polity_creed);
            tap->publish_names(tap_civ, tap_creed, tap_polity_creed);
        }

        const std::size_t century =
            static_cast<std::size_t>((y - params.start_year) / 100);

        // ---- BL-973: fold every polity's held nodes into its effect surface
        // before anything this round reads a term or a key. Re-run the
        // instant a node is bought (the Invest verb below), so a purchase is
        // visible to every later reader in the same round. Polity order is
        // id order; the fold is pure in the masks, so the order is moot.
        for (polity& q : out.polities)
            apply_tree_effects(q);

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

            int np_owner = -1;

            if (params.city_states_by_population_threshold)
            {
                // BL-920 — A SCHEDULED FOUNDING ARRIVES AS UNORGANISED
                // GROUND OF ITS OWN CULTURE, NOT FREE HINTERLAND FOR AN
                // INCUMBENT. This supersedes the BL-856/BL-867 reading that
                // matched the founding culture to an existing polity (or
                // minted one on the spot) and handed it the new region
                // outright — under that rule every scheduled founding after
                // the opening grew SOME polity for free, so a realm's size
                // at 1200 CE tracked how far its people walked rather than
                // how it acted (CIVILISATION.md sec "WHAT THE OPENING IS
                // TODAY"). A founding now sits on the map exactly as the
                // opening's own unorganised ground does: peopled, carrying
                // its culture, owned by nobody, organised later by
                // ORGANISE, by conquest, or promoted in its own right once
                // its population crosses `city_state_population_threshold`
                // (the per-year rise check below).
                np.nation  = -1;
                np.is_seat = false;

                // NEAREST-SEAT POINTER, geometric like the opening's own
                // (above): `neighbours` exists by this point in the run but
                // a fresh founding is not linked into it until
                // `link_region` below, so this reads the same Chebyshev
                // distance the opening used rather than a graph walk. No
                // seat anywhere yet (a frontier the migration reached
                // before any city state did) leaves `seat_region == -1` —
                // "falls outside anyone's reach", the honest reading for
                // ground no seat touches yet.
                int nearest = -1, nearest_d = 1 << 30;
                for (const polity& q : out.polities)
                {
                    if (q.capital < 0
                     || static_cast<std::size_t>(q.capital) >= ss.regions.size())
                        continue;
                    const int d = region_distance(np, ss.regions[static_cast<std::size_t>(q.capital)], gw);
                    if (d < nearest_d) { nearest_d = d; nearest = q.capital; }
                }
                np.seat_region = nearest;
            }
            else
            {
                // --- BL-846/BL-856/BL-867 (the old reading) --------------
                //
                // WHOSE IT IS: the polity of the people whose stream
                // arrived. The same plurality rule the world-opening seed
                // uses, so a region founded in year -3000 is owned on
                // identical terms to one that was there at tick zero. A
                // people that comes into being and settles ground with no
                // existing polity of its culture mints one on the spot,
                // seated on the region it just founded.
                const int np_culture = np.culture.plurality();
                for (const polity& q : out.polities)
                    if (q.culture == np_culture) { np_owner = q.id; break; }

                if (np_owner < 0 && np_culture >= 0)
                {
                    polity q;
                    q.id      = static_cast<int>(out.polities.size());
                    q.culture = np_culture;
                    q.aggression_q =
                        (cs && np_culture < static_cast<int>(cs->cultures.size()))
                            ? leaned_aggression_q(
                                  params, cs->cultures[static_cast<std::size_t>(np_culture)].aggression_q)
                            : 500;
                    q.capital = static_cast<int>(ss.regions.size());
                    np_owner  = q.id;
                    coin_name_for(q); // BL-1088
                    out.polities.push_back(q);
                    note_event(lapse_event_kind::founded, q.capital, q.id, -1);
                }
                np.nation = np_owner;

                if (np_owner >= 0)
                {
                    const int cap = out.polities[static_cast<std::size_t>(np_owner)].capital;
                    np.seat_region = cap;
                    if (cap == static_cast<int>(ss.regions.size())) np.is_seat = true;
                }
            }

            ss.regions.push_back(std::move(np));
            owner.push_back(np_owner);
            touch_owner(np_owner); // BL-922: new held ground changes reach (a no-op at -1).
            neighbours.emplace_back();
            supply_neighbours.emplace_back();
            degree.push_back(0);
            link_region(ss.regions.size() - 1); // Keep the index complete.

            // BL-914: this region's geometry joins the tap's mirror the
            // instant it exists, well ahead of the per-year ownership publish
            // below — a founding this rare (hundreds, not thousands, across a
            // run) costs nothing extra locked.
            if (tap != nullptr)
            {
                tap_region_col.push_back(ss.regions.back().col);
                tap_region_row.push_back(ss.regions.back().row);
                tap_region_name.push_back(ss.regions.back().name);
                tap->publish_regions(tap_region_col, tap_region_row, tap_region_name);
            }

            // NO OWNER_CHANGE for unorganised ground (BL-920): it is
            // `owner_none` in the timelapse until something organises it,
            // which is what lets the render show it in the dull culture
            // tint rather than a polity's colour. The old path always
            // carries an owner and records the change as it always did.
            if (np_owner >= 0)
                out.owner_changes.push_back(owner_change{
                    static_cast<int32_t>(y),
                    static_cast<uint16_t>(ss.regions.size() - 1),
                    static_cast<uint16_t>(np_owner)});
            ++out.foundings;
            ++out.foundings_scheduled; // BL-926: the schedule played back.
            // BL-916: a people arriving on new ground is still an event,
            // even though (per COLONISATION.md) nothing here is an actor's
            // choice — `-1` is "no polity", the same sentinel `note_event`
            // already uses for "no place".
            if (params.city_states_by_population_threshold)
                note_event(lapse_event_kind::founded,
                           static_cast<int>(ss.regions.size() - 1), -1, -1);
            out.history.push_back(history_event{
                years_from_calendar_year(y), chain_stage::legacy,
                ss.regions.back().name + " is settled", std::string{}});
        }

        // ---- Demography -------------------------------------------------
        int64_t total_pop = 0;
        {
        const scoped_ns prof_demo(prof.ns_demography); // BL-825, report-only

        // ---- BL-1021: THE KINDS OF GROUND EACH REALM REACHES, THIS YEAR ----
        //
        // Computed whether or not trade pays, so a zero-income control run
        // classifies realms for the BL-895 reading exactly as a live one does.
        // Two passes in region-index order, OR-only writes, so the result is a
        // property of the map and not of walk order.
        realm_kinds_reached.assign(out.polities.size(), 0);
        realm_regions_held.assign(out.polities.size(), 0);
        {
            const std::size_t n = std::min(owner.size(), ss.regions.size());
            for (std::size_t i = 0; i < n; ++i)
            {
                const int oi = owner[i];
                if (oi < 0 || static_cast<std::size_t>(oi) >= realm_kinds_reached.size()) continue;
                ++realm_regions_held[static_cast<std::size_t>(oi)];
                if (!reached_for_trade(i)) continue;
                realm_kinds_reached[static_cast<std::size_t>(oi)] |=
                    static_cast<uint8_t>(1u << region_trade_class(ss.regions[i]));
            }
            // ACROSS AN AMICABLE BORDER (BL-925): a walked corridor joining two
            // realms' REACHED ground makes each side's kind one the other
            // reaches. Only the ground the corridor touches is added, never the
            // neighbour's whole realm. Like ground adds nothing by construction,
            // since the near region's own kind is already set above.
            for (std::size_t i = 0; i < n; ++i)
            {
                const int oi = owner[i];
                if (oi < 0 || static_cast<std::size_t>(oi) >= realm_kinds_reached.size()) continue;
                if (!reached_for_trade(i)) continue;
                for (int nb : neighbours[i])
                {
                    if (nb <= static_cast<int>(i)) continue;
                    const std::size_t ni = static_cast<std::size_t>(nb);
                    if (ni >= n) continue;
                    const int oj = owner[ni];
                    if (oj < 0 || oj == oi
                     || static_cast<std::size_t>(oj) >= realm_kinds_reached.size()) continue;
                    if (!reached_for_trade(ni)) continue;
                    if (road_uses_live.find(edge_key(static_cast<int>(i), nb))
                        == road_uses_live.end()) continue;
                    if (!polities_amicable(oi, oj)) continue;
                    realm_kinds_reached[static_cast<std::size_t>(oi)] |=
                        static_cast<uint8_t>(1u << region_trade_class(ss.regions[ni]));
                    realm_kinds_reached[static_cast<std::size_t>(oj)] |=
                        static_cast<uint8_t>(1u << region_trade_class(ss.regions[i]));
                }
            }
            for (const polity& q : out.polities)
                if (q.alive)
                    ++out.realm_years_by_trade_bucket[trade_bucket_of(q.id)];
        }

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
            // Only DIFFERENCE is read: this phase has no order book, no firm
            // and no price (CIVILISATION.md sec Materials are spent when
            // something happens), so "what a place holds" is a CLASS -- what it
            // is best at -- and never a quantity.
            //
            // BL-1021 -- PAID ON THE KINDS OF GROUND THE REALM REACHES, NOT ON
            // ROADED PAIRS (Ben, 2026-09-16, NR-827 option 2). The per-link
            // income this replaces was bounded by a region's adjacency, so it
            // could never grow with a realm the way industry does; measured at
            // 0.25% of production. Now a held region the network reaches trades
            // with every KIND of ground its realm reaches that is unlike its
            // own (`realm_kinds_reached`, built above this loop), and is paid
            // that count times the constant, at its own seat. A realm of one
            // kind earns nothing however large; ground the network cannot reach
            // earns nothing and lends its kind to nobody. Roads still pay, but
            // through REACH -- `rebuild_reach` discounts walked corridors -- and
            // a realm whose network fails loses every kind that ground held.
            if (params.trade_income_per_class > 0)
            {
                const int oi = owner[i] == owner_none ? -1 : static_cast<int>(owner[i]);
                if (oi >= 0)
                {
                    if (reached_for_trade(i)
                     && static_cast<std::size_t>(oi) < realm_kinds_reached.size())
                    {
                        const unsigned unlike_mask =
                            static_cast<unsigned>(realm_kinds_reached[static_cast<std::size_t>(oi)])
                            & ~(1u << region_trade_class(ss.regions[i]));
                        const int64_t unlike = static_cast<int64_t>(
                            (unlike_mask & 1u) + ((unlike_mask >> 1) & 1u) + ((unlike_mask >> 2) & 1u));
                        const int seat_t = ss.regions[i].seat_region;
                        if (unlike > 0 && seat_t >= 0
                         && static_cast<std::size_t>(seat_t) < ss.regions.size())
                        {
                            const int64_t paid = unlike * params.trade_income_per_class;
                            ss.regions[static_cast<std::size_t>(seat_t)].material_stock += paid;
                            out.materials_produced   += paid;
                            out.materials_from_trade += paid;
                        }
                    }

                    // BL-925 -- THE LAPSE DRAWS THE LINK. The cross-border link is
                    // still an EVENT the time-lapse draws, gated exactly as it was
                    // (a walked corridor joining unlike ground across an amicable
                    // border); since BL-1021 it pays nothing per edge -- what it
                    // buys is the far kind in `realm_kinds_reached`. One
                    // transition event per state change, not one per year the
                    // link holds open.
                    for (int nb : neighbours[i])
                    {
                        if (nb <= static_cast<int>(i)) continue;          // note the pair once
                        const int oj = owner[static_cast<std::size_t>(nb)] == owner_none
                                       ? -1 : static_cast<int>(owner[static_cast<std::size_t>(nb)]);
                        if (oj < 0 || oj == oi || !polities_amicable(oi, oj)) continue;
                        // ANY WALKED CORRIDOR CARRIES TRADE, not only a promoted
                        // Track. Measured 2026-09-11: of 1,607 distinct corridors,
                        // 1,347 are walked ONCE and only 155 reach the 4 uses
                        // `road_tier1_uses` needs.
                        const uint64_t ekey = edge_key(static_cast<int>(i), nb);
                        if (road_uses_live.find(ekey) == road_uses_live.end()) continue;
                        if (region_trade_class(ss.regions[i])
                         == region_trade_class(ss.regions[static_cast<std::size_t>(nb)])) continue;

                        bool& open_now = trade_link_open_live[ekey];
                        if (!open_now)
                        {
                            open_now = true;
                            note_event(lapse_event_kind::trade_link_opened,
                                       static_cast<int>(i), -1, nb);
                        }
                    }

                    // BL-925 -- A GRUDGE CLOSES THE LINK, OR CONQUEST DOES.
                    // Swept as its own pass rather than inline above: the loop
                    // above only visits a pair while it STILL QUALIFIES, so a
                    // pair that stopped clearing amicability needs its own
                    // check or the "open" entry would stay stale and the lapse
                    // would draw it forever.
                    if (!trade_link_open_live.empty())
                    {
                        for (int nb : neighbours[i])
                        {
                            if (nb <= static_cast<int>(i)) continue;
                            const uint64_t ekey = edge_key(static_cast<int>(i), nb);
                            auto it = trade_link_open_live.find(ekey);
                            if (it == trade_link_open_live.end() || !it->second) continue;
                            const int oj = owner[static_cast<std::size_t>(nb)] == owner_none
                                           ? -1 : static_cast<int>(owner[static_cast<std::size_t>(nb)]);
                            const bool still_amicable = oj >= 0 && oj != oi
                                                       && polities_amicable(oi, oj);
                            if (still_amicable) continue;
                            it->second = false;
                            note_event(lapse_event_kind::trade_link_closed,
                                       static_cast<int>(i), -1, nb);
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
                // BL-1021 -- the BL-895 reading: upkeep owed and met, by how
                // many kinds of ground the owning realm reached this year.
                if (i < owner.size() && owner[i] >= 0)
                {
                    const std::size_t kb = trade_bucket_of(owner[i]);
                    out.upkeep_due_by_trade_bucket[kb]  += due;
                    out.upkeep_paid_by_trade_bucket[kb] += paid;
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
                const int64_t stock_before = p.army_stock;
                p.army_stock    -= gone;
                scale_standing_army(p, stock_before); // BL-955: paid men walk off alike
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

        // ---- BL-920: A CENTRE THAT CROSSES THE THRESHOLD RISES ------------
        //
        // CIVILISATION.md sec "A CITY STATE SPAWNS WHERE A REGION'S
        // POPULATION IS ABOVE A THRESHOLD": city states keep RISING through
        // the span as later-founded ground grows toward its ceiling, not
        // only at the opening. Checked every year, at demography's own
        // grain, rather than the coarse decision clock below — a threshold
        // crossing is a fact about population, not a decision anybody makes,
        // exactly like the founding schedule it sits beside. Walked in
        // ascending region index, so two regions crossing in the same year
        // hand out ids in an order the map determines, not iteration order.
        //
        // GATED ON THE SAME FLAG AS THE OPENING (BL-920). Under the old
        // BL-826 seed every region is already somebody's from year zero
        // (`owner[i] >= 0` always), so this loop is a guaranteed no-op there
        // — gated anyway so it costs the old path nothing, not even the scan.
        for (std::size_t i = 0; params.city_states_by_population_threshold
                              && i < ss.regions.size(); ++i)
        {
            region& r = ss.regions[i];
            if (owner[i] >= 0) continue; // Already organised: not this mechanism's ground.
            if (r.population < params.city_state_population_threshold) continue;
            const int culture = r.culture.plurality();
            if (culture < 0) continue;

            polity q;
            q.id      = static_cast<int>(out.polities.size());
            q.culture = culture;
            q.aggression_q =
                (cs && culture < static_cast<int>(cs->cultures.size()))
                    ? leaned_aggression_q(
                          params, cs->cultures[static_cast<std::size_t>(culture)].aggression_q)
                    : 500; // BL-839: same lean as every other seat above.
            q.capital = static_cast<int>(i);

            r.nation      = q.id;
            void_stale_standing_army(r); // BL-955
            r.is_seat     = true;
            r.seat_region = static_cast<int>(i);
            owner[i]      = q.id;
            coin_name_for(q); // BL-1088
            out.polities.push_back(q);

            out.owner_changes.push_back(owner_change{
                static_cast<int32_t>(y), static_cast<uint16_t>(i),
                static_cast<uint16_t>(q.id)});
            // BL-916: a city state rising is an event with a place and a date,
            // the same `founded` kind the opening and the schedule both use.
            note_event(lapse_event_kind::founded, static_cast<int>(i), q.id, -1);
            ++out.city_states_risen;
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

        // ---- BL-931: OBJECTS WITH A TERM, AND THE UPKEEP STEP -------------
        //
        // Both run once per decision round, ahead of grudge decay and every
        // polity's own turn — "earn, then pay stocks, then invest"
        // (EXPLORATION.md sec The engine is shared) has to happen before a
        // polity spends this round's Invest choice, and a treaty due to
        // expire this round should already be gone before anything reads it.
        // `expire_dated_objects` runs UNCONDITIONALLY (an empty vector costs
        // nothing and nothing populates it outside this item's scope yet);
        // the upkeep call is gated on `exploration_upkeep_enabled` so the
        // Empire span — every caller before this item — never takes it.
        expire_dated_objects(out.dated_objects, y);

        // ---- BL-1041: INDUSTRY POINTS, THE SCALE ACCRUAL ------------------
        //
        // INDUSTRIALISATION.md sec Beat 1. Every region with centres is credited on
        // itself, once a round, off the round's OPENING state (ahead of the
        // upkeep and every polity's turn, so no polity's act this round moves
        // another region's credit). Behind its switch and from the span's own
        // open year, so every other span never enters; a run whose constants
        // left their domain credits nothing and says so. The treasury half is
        // inside `run_exploration_upkeep`, after the bills it is a share of.
        if (params.industry_points_enabled && y >= params.industry_open_year)
        {
            if (!industry_points_params_valid(params))
            {
                out.industry_points_params_rejected = true;
            }
            else
            {
                const scoped_ns prof_points(prof.ns_industry_points); // report-only
                const industry_points_round pr =
                    accrue_industry_points(ss.regions, out.polities, params, step_years);
                out.industry_points_from_scale += pr.credited;
                out.industry_points_refused    += pr.refused;
            }
        }

        if (params.exploration_upkeep_enabled)
        {
            // BL-955: the paid standing army's raw invariant, checked every
            // decision round over everything the last round did. Read-only.
            for (const region& r : ss.regions)
                if (!standing_army_invariant_holds(r)) ++out.standing_army_invariant_violations;

            // BL-953 -- the round's preference, once. Derived AHEAD of the
            // upkeep since BL-955, whose allocation weights the across-water
            // want by it: `derive_culture_preference` reads ownership, culture
            // shares, dominance, contacts and liveness -- nothing the upkeep
            // writes -- so the table is identical either side of the call.
            if (want_lean_on)
                round_prefs = derive_culture_preference(ss.regions, out.contacts,
                                                        out.polities, want_culture_count);

            exploration_upkeep_spend upkeep_spend;
            // BL-955: the allocation reads the state's contacts (near-home
            // Alarm), the creeds (lean ranks) and the round's preference.
            exploration_spend_context spend_ctx;
            spend_ctx.state  = &out;
            spend_ctx.creeds = cs;
            spend_ctx.prefs  = &round_prefs;
            // BL-954: the state's treaties open this round's flows, rebuilt
            // into `out.trade_flows` (never accumulated).
            run_exploration_upkeep(ss.regions, out.polities, out.supply_corridors,
                                   params, y, step_years, &upkeep_spend,
                                   &out.dated_objects, &out.trade_flows, &spend_ctx);
            out.treasury_spent_on_ports           += upkeep_spend.ports;
            out.treasury_spent_on_navies          += upkeep_spend.navies;
            out.treasury_spent_on_standing_armies += upkeep_spend.standing_armies;
            // BL-972: the bill and the levy, summed over the span.
            out.treasury_spent_on_army_upkeep += upkeep_spend.army_upkeep;
            out.treasury_spent_on_navy_upkeep += upkeep_spend.navy_upkeep;
            out.army_upkeep_unpaid_rounds     += upkeep_spend.army_unpaid;
            out.navy_upkeep_unpaid_rounds     += upkeep_spend.navy_unpaid;
            out.levy_heads_raised             += upkeep_spend.levy_raised;
            out.levy_heads_returned           += upkeep_spend.levy_returned;
            // BL-1041: the treasury paid into industry points on capitals.
            out.industry_points_from_treasury += upkeep_spend.industry_points_paid_in;
            out.treasury_spent_on_industry    += upkeep_spend.industry_treasury_debited;
            out.industry_points_refused       += upkeep_spend.industry_points_refused;
            out.port_steps_bought += upkeep_spend.port_steps;
            out.navy_steps_bought += upkeep_spend.navy_steps;
            out.army_steps_bought += upkeep_spend.army_steps;
            for (uint16_t lapsed : upkeep_spend.navies_lapsed)
            {
                if (out.navy_lapsed.size() <= lapsed) out.navy_lapsed.resize(lapsed + 1u, 0);
                out.navy_lapsed[lapsed] = 1;
            }

            // ---- BL-940: THE ROAD LADDER'S THIRD RUNG, BOUGHT WITH CAPITAL.
            //
            // A polity holding EX-WY-1a (Post Roads) may promote AT MOST ONE
            // of its own Road-tier (2) corridors to a Post Road (3) per
            // round, if its treasury covers `post_road_treasury_cost` — the
            // treasury decides which frontier earns it, not traffic
            // (`try_build_post_road` above refuses anything not already at
            // tier 2). Bounded to one purchase per polity per round so this
            // addition's cost cannot scale with corridor count.
            //
            // THE PICK IS DETERMINISTIC: the polity's own held regions,
            // walked in ascending region-index order (owner[] order, not a
            // hash), each one's neighbour list read in its own stored
            // (insertion) order — the same discipline `rebuild_reach`'s own
            // walks already hold to.
            // BL-973: the capability is the store effect keyed `post_roads`
            // (EX-WY-1a's `upgrade`, today), read off the polity's folded
            // effect surface — no node id is compared here any more.
            if (params.post_road_treasury_cost > 0)
            {
                for (polity& q : out.polities)
                {
                    if (!q.alive) continue;
                    if (!polity_holds_tree_key(q, io::tree_effect_key::post_roads)) continue;
                    if (q.capital < 0 || static_cast<std::size_t>(q.capital) >= ss.regions.size())
                        continue;
                    if (ss.regions[static_cast<std::size_t>(q.capital)].treasury
                        < params.post_road_treasury_cost)
                        continue;

                    int found_a = -1, found_b = -1;
                    for (std::size_t hi = 0; hi < ss.regions.size() && found_a < 0; ++hi)
                    {
                        if (ss.regions[hi].nation != q.id) continue;
                        if (hi >= neighbours.size()) continue;
                        for (int nb : neighbours[hi])
                        {
                            if (nb < 0 || static_cast<std::size_t>(nb) >= ss.regions.size()) continue;
                            if (ss.regions[static_cast<std::size_t>(nb)].nation != q.id) continue;
                            if (road_tier_between(static_cast<int>(hi), nb) != 2) continue;
                            found_a = static_cast<int>(hi);
                            found_b = nb;
                            break;
                        }
                    }
                    if (found_a >= 0 && try_build_post_road(found_a, found_b, q.capital))
                    {
                        if (out.post_roads_by_polity.size() <= static_cast<std::size_t>(q.id))
                            out.post_roads_by_polity.resize(static_cast<std::size_t>(q.id) + 1, 0);
                        ++out.post_roads_by_polity[static_cast<std::size_t>(q.id)];
                    }
                }
            }

            // ---- BL-933: TREATY FORMATION, "nobody negotiates" ------------
            //
            // Walk `out.contacts` in its own sorted (from, to) order, taking
            // the canonical `from < to` row of each mutual pair once (contact
            // is recorded in both directions, EXPLORATION.md sec What a
            // treaty is table). Both parties score the binding INDEPENDENTLY,
            // against the SAME grudge/aggression/distrust facts this round
            // already holds -- no bargaining loop, no mutation between the two
            // reads. The walk order is the table's own order, so the result
            // does not depend on anything but the integers already in it.
            //
            // BL-954: what a binding would OPEN in trade is part of what it
            // is worth. One context for this round's formation AND break
            // re-score: nothing between here and the end of the break walk
            // changes who holds what, and the raw signal was refreshed by
            // the upkeep step above.
            const trade_context treaty_trade_ctx =
                build_trade_context(ss.regions, out.polities, out.supply_corridors);

            // BL-1018 DIAGNOSTIC (trace only): the raw capability each side
            // of every living NEAR-HOME pair reads of the other this round,
            // bound or not -- the round's opening stocks, exactly what the
            // formation walk and the break re-score below both read. One pass
            // of its own so a pair formed this round is not read twice. Pure
            // reads into a trace vector no decision consults.
            if (params.trace_battles)
                for (const contact& c : out.contacts)
                {
                    if (c.from >= c.to || c.first.year >= params.near_home_cutoff_year) continue;
                    if (c.to >= out.polities.size()
                     || !out.polities[c.from].alive || !out.polities[c.to].alive) continue;
                    out.near_capability_trace.push_back(visible_capability_raw(ss.regions, out, c.to));
                    out.near_capability_trace.push_back(visible_capability_raw(ss.regions, out, c.from));
                }

            for (const contact& c : out.contacts)
            {
                if (c.from >= c.to) continue; // the pair's OTHER row; skip
                const int a = c.from, b = c.to;
                if (a >= static_cast<int>(out.polities.size())
                 || b >= static_cast<int>(out.polities.size())) continue;
                if (!out.polities[static_cast<std::size_t>(a)].alive
                 || !out.polities[static_cast<std::size_t>(b)].alive) continue;
                // Already bound: non-aggression is the anchor clause every
                // formed treaty carries, so its presence stands for the pair.
                if (has_treaty_clause(out, a, b, treaty_clause::non_aggression)) continue;

                const int ga = grudge_between(out, a, b), gb = grudge_between(out, b, a);
                const polity& pa = out.polities[static_cast<std::size_t>(a)];
                const polity& pb = out.polities[static_cast<std::size_t>(b)];
                // BL-941 -- NEAR HOME is whether this pair's own contact
                // predates the near-home cutoff (a long-known neighbour)
                // rather than coming after it (a frontier pair). BL-1036: the
                // cutoff is 1200 explicitly, never this span's own open, so a
                // pair met after 1200 stays far however late a span opens.
                // `c` is already this pair's canonical contact row, so its own
                // `first.year` is the fact, not a second lookup.
                const bool near_home = c.first.year < params.near_home_cutoff_year;
                const int alarm_a = deterrence_alarm_q(ss.regions, out, params, a, b);
                const int alarm_b = deterrence_alarm_q(ss.regions, out, params, b, a);
                const int trade_ab = pair_trade_value_q(treaty_trade_ctx, ss.regions, out.polities, a, b,
                                                             out.trade_flows);
                const int value_a = treaty_value_q(params, ga, gb, pb.treaties_broken, pa.aggression_q,
                                                    alarm_a, near_home, trade_ab);
                const int value_b = treaty_value_q(params, gb, ga, pa.treaties_broken, pb.aggression_q,
                                                    alarm_b, near_home, trade_ab);
                if (value_a < params.treaty_formation_threshold_q
                 || value_b < params.treaty_formation_threshold_q) continue;

                // FORMED: the four MUTUAL clauses bind together this cut --
                // tribute is directed and is instead added by the subjection
                // block below, at the moment an overlord link actually exists
                // to pay one (§ A colony is a subject). One `dated_object`
                // per clause, sharing (a, b, expires_year) -- the seam
                // `history_sim_state::dated_objects` was built for.
                const int64_t expires = y + std::max<int64_t>(params.treaty_term_years, 1);
                const uint16_t lo = static_cast<uint16_t>(a), hi_ = static_cast<uint16_t>(b);
                const treaty_clause mutual[4] = {
                    treaty_clause::non_aggression, treaty_clause::trade_access,
                    treaty_clause::sphere_of_claim, treaty_clause::mutual_defence,
                };
                for (treaty_clause tc : mutual)
                    out.dated_objects.push_back(dated_object{
                        expires, static_cast<int32_t>(tc), lo, hi_});
                note_event(lapse_event_kind::treaty_formed, pa.capital, a, b);
                ++out.treaties_formed;
            }

            // ---- BL-933: BREAKING A TREATY, "legal and costly" ------------
            //
            // Re-scored every round against the SAME `treaty_value_q`
            // formation used. A pair whose value has fallen under HALF the
            // formation bar for one side is the hysteresis that keeps a
            // treaty sitting exactly on the line from flickering on and off
            // every round. Deduped pairs, sorted, so a tie between two
            // simultaneously-failing sides breaks on the LOWER id.
            {
                std::vector<std::pair<uint16_t, uint16_t>> active;
                for (const dated_object& o : out.dated_objects)
                    if (o.kind == static_cast<int32_t>(treaty_clause::non_aggression))
                        active.push_back({o.a, o.b});
                std::sort(active.begin(), active.end());
                active.erase(std::unique(active.begin(), active.end()), active.end());

                const int break_bar = params.treaty_formation_threshold_q / 2;
                for (const auto& pr : active)
                {
                    const int a = pr.first, b = pr.second;
                    if (a >= static_cast<int>(out.polities.size())
                     || b >= static_cast<int>(out.polities.size())) continue;
                    polity& pa = out.polities[static_cast<std::size_t>(a)];
                    polity& pb = out.polities[static_cast<std::size_t>(b)];
                    if (!pa.alive || !pb.alive) continue;

                    const int ga = grudge_between(out, a, b), gb = grudge_between(out, b, a);
                    // BL-941 -- same near-home read as formation, off the
                    // pair's own recorded first contact (not `c`, since this
                    // loop walks bound pairs rather than the contact table).
                    const bool near_home =
                        contact_first_year(out, a, b) < params.near_home_cutoff_year; // BL-1036
                    const int alarm_a = deterrence_alarm_q(ss.regions, out, params, a, b);
                    const int alarm_b = deterrence_alarm_q(ss.regions, out, params, b, a);
                    const int trade_ab = pair_trade_value_q(treaty_trade_ctx, ss.regions, out.polities, a, b,
                                                             out.trade_flows);
                    const int value_a = treaty_value_q(params, ga, gb, pb.treaties_broken, pa.aggression_q,
                                                        alarm_a, near_home, trade_ab);
                    const int value_b = treaty_value_q(params, gb, ga, pa.treaties_broken, pb.aggression_q,
                                                        alarm_b, near_home, trade_ab);

                    int defector = -1, wronged = -1;
                    if (value_a < break_bar && value_a <= value_b) { defector = a; wronged = b; }
                    else if (value_b < break_bar)                  { defector = b; wronged = a; }
                    if (defector < 0) continue;

                    const uint16_t du = static_cast<uint16_t>(defector);
                    const uint16_t wu = static_cast<uint16_t>(wronged);
                    out.dated_objects.erase(
                        std::remove_if(out.dated_objects.begin(), out.dated_objects.end(),
                            [&](const dated_object& o) {
                                return (o.a == du && o.b == wu) || (o.a == wu && o.b == du);
                            }),
                        out.dated_objects.end());

                    polity& pdef = out.polities[static_cast<std::size_t>(defector)];
                    ++pdef.treaties_broken;
                    // WRITES INTO THE EXISTING GRUDGE TABLE, never a second
                    // ledger (EXPLORATION.md sec What a treaty is).
                    raise_grudge(wronged, defector, grudge_kind::treaty_broken,
                                 out.polities[static_cast<std::size_t>(wronged)].capital,
                                 y, params.grudge_treaty_broken_q);
                    note_event(lapse_event_kind::treaty_broken,
                               out.polities[static_cast<std::size_t>(defector)].capital,
                               defector, wronged);
                    ++out.treaties_broken;
                }
            }

            // A FLOW NEVER OUTLIVES ITS CLAUSE. The round's flows were opened
            // by the clauses standing at the upkeep step (after this round's
            // expiry); a pair that broke above no longer holds trade_access,
            // so its flows leave the record the round leaves behind. The
            // pruning is order-preserving: the vector stays sorted.
            prune_flows_without_trade_access(out.trade_flows, out.dated_objects);

            // ---- BL-934: SUBJECTION -- trade provinces and subjected polities
            //
            // Snapshot the count: a polity subjected THIS round is not itself
            // an arriving power or a native target again in the same round
            // (no chained subjection within one round, and none across
            // rounds either -- § open question, declined for this wave).
            {
                const std::size_t pc = out.polities.size();
                for (std::size_t ai = 0; ai < pc; ++ai)
                {
                    polity& arriving = out.polities[ai];
                    if (!arriving.alive || arriving.overlord >= 0) continue;
                    if (!polity_holds_exploration_sea_legs(arriving)) continue; // THE NODE, never a rank
                    if (arriving.capital < 0
                     || static_cast<std::size_t>(arriving.capital) >= ss.regions.size()) continue;

                    // BL-953 -- COLLECT, THEN RANK. Every native passing the
                    // same eligibility tests as before is gathered with the
                    // arriving power's want for the native seat's dominant
                    // good; the highest want is bound, ties to the lower id
                    // (EXPLORATION.md sec A want points a campaign outward:
                    // "subjection ranks the natives a power could bind by the
                    // same want"). With the lean off every want is 0 and the
                    // pick is the lowest eligible id -- the old id-order walk.
                    std::vector<std::pair<int, int>> eligible_natives;
                    for (std::size_t ni = 0; ni < pc; ++ni)
                    {
                        if (ni == ai) continue;
                        polity& native = out.polities[ni];
                        if (!native.alive || native.overlord >= 0) continue;
                        if (native.capital < 0
                         || static_cast<std::size_t>(native.capital) >= ss.regions.size()) continue;
                        if (!has_contact(out, arriving.id, native.id)) continue;

                        // The two cheap scalar tests run BEFORE the O(P)
                        // sphere-of-claim scan below. Every test here is a
                        // side-effect-free `continue`, so their order changes
                        // nothing but how much an ineligible native costs.
                        const int dist = region_distance(
                            ss.regions[static_cast<std::size_t>(arriving.capital)],
                            ss.regions[static_cast<std::size_t>(native.capital)], gw);
                        if (dist > params.subjection_reach_q) continue;

                        const int64_t arriving_treasury =
                            ss.regions[static_cast<std::size_t>(arriving.capital)].treasury;
                        const int64_t native_treasury =
                            ss.regions[static_cast<std::size_t>(native.capital)].treasury;
                        if (arriving_treasury < native_treasury + params.subjection_treasury_margin_q)
                            continue;

                        // SPHERE OF CLAIM: non-interference over a native
                        // polity's ground BETWEEN THE TWO TREATY PARTIES --
                        // never actually-empty land (EXPLORATION.md sec What
                        // a treaty is / no terra nullius). A treaty partner of
                        // `arriving` who has already met `native` holds
                        // priority; `arriving` stands off.
                        bool sphere_blocked = false;
                        for (std::size_t oi = 0; oi < pc && !sphere_blocked; ++oi)
                        {
                            if (oi == ai) continue;
                            if (!out.polities[oi].alive) continue;
                            if (!has_treaty_clause(out, arriving.id, static_cast<int>(oi),
                                                    treaty_clause::sphere_of_claim)) continue;
                            if (has_contact(out, static_cast<int>(oi), native.id))
                                sphere_blocked = true;
                        }
                        if (sphere_blocked) continue;

                        const int want_q = want_lean_on
                            ? polity_good_want_q(ss.regions, out.polities, round_prefs, arriving.id,
                                                 ss.regions[static_cast<std::size_t>(native.capital)].dominant)
                            : 0;
                        eligible_natives.push_back({static_cast<int>(ni), want_q});
                    }

                    const int chosen = choose_subjection_native(eligible_natives);
                    if (chosen >= 0)
                    {
                        polity& native = out.polities[static_cast<std::size_t>(chosen)];
                        region& nseat = ss.regions[static_cast<std::size_t>(native.capital)];
                        region& bseat = ss.regions[static_cast<std::size_t>(arriving.capital)];

                        // BL-1096 -- BOUGHT OR TAKEN, decided on the ground's
                        // terms and nothing else (EXPLORATION.md sec Two ways
                        // to claim ground across water, Ben 2026-09-24): a
                        // coastal seat whose price the arriver's seat covers is
                        // BOUGHT -- the price moves, no grudge is written, the
                        // culture shares stand; anything else is TAKEN as the
                        // block always did. `subject_kind` is the VERB, recorded
                        // here once and never a proxy read off the coast. The
                        // seller never decides. With the fork off or its
                        // constants rejected (`subjection_purchase_params_
                        // rejected`) `subjection_verb` returns TAKEN for every
                        // native, so the control run is what this block was.
                        const int verb = out.subjection_purchase_params_rejected
                            ? 1 : subjection_verb(nseat, bseat.treasury, params);
                        native.overlord     = arriving.id;
                        native.subject_kind = static_cast<int8_t>(verb);

                        // Either way the tribute clause and its term bind
                        // (EXPLORATION.md: "what differs is what the natives
                        // keep and what they remember").
                        const int64_t tribute_expires =
                            y + std::max<int64_t>(params.treaty_term_years, 1);
                        out.dated_objects.push_back(dated_object{
                            tribute_expires, static_cast<int32_t>(treaty_clause::tribute),
                            static_cast<uint16_t>(native.id), static_cast<uint16_t>(arriving.id)});

                        if (verb == 0)
                        {
                            // THE PRICE CHANGES HANDS rather than vanishing:
                            // debited from the buyer's seat (covered, by the
                            // verb's own test) and credited to the native's,
                            // clamped exactly as tribute is. Nothing is
                            // digested and nothing is resented, because
                            // nothing was conquered.
                            const int64_t price = purchase_price_q(nseat, params);
                            bseat.treasury -= price;
                            nseat.treasury  = clampi64(nseat.treasury + price, 0, 1LL << 48);
                            ++out.provinces_bought;
                            out.treasury_spent_on_purchases += price;
                            // The purchase is a moment of its own, noted
                            // INSTEAD of `subject_bound` so the ticker can say
                            // "buys" rather than "falls under" for this binding.
                            note_event(lapse_event_kind::province_bought,
                                       native.capital, native.id, arriving.id);
                            // BL-1097: the purchase party's crossing, buyer's
                            // seat to the seat it buys, is a sea leg.
                            note_sea_leg(arriving.capital, native.capital);
                            ++out.sea_legs_noted_purchase;
                        }
                        else
                        {
                            raise_grudge(native.id, arriving.id, grudge_kind::ground_taken,
                                         native.capital, y, params.grudge_ground_taken / 2);
                            note_event(lapse_event_kind::subject_bound,
                                       native.capital, native.id, arriving.id);
                        }
                        ++out.subjections_formed; // one native per arriving power per round
                    }
                }
            }

            // ---- BL-934: TRIBUTE REMITTANCE AND REFUSED RENEWAL -----------
            //
            // Reads the SAME quantities the Empire phase's own secession
            // check reads -- cohesion, distance, and whether the overlord is
            // reachable at all (`secession_supply_floor_q`'s land-empire
            // shape, restated for an overseas link).
            for (polity& subj : out.polities)
            {
                if (!subj.alive || subj.overlord < 0) continue;
                if (static_cast<std::size_t>(subj.overlord) >= out.polities.size()
                 || !out.polities[static_cast<std::size_t>(subj.overlord)].alive)
                {
                    // The overlord is gone; the link dissolves with it --
                    // never a rank, never a decision, just a fact that ceased
                    // to have a holder.
                    subj.overlord = -1;
                    subj.subject_kind = -1;
                    continue;
                }
                polity& lord = out.polities[static_cast<std::size_t>(subj.overlord)];
                const bool reachable = has_contact(out, subj.id, lord.id);
                int dist = INT32_MAX;
                if (subj.capital >= 0 && lord.capital >= 0
                 && static_cast<std::size_t>(subj.capital) < ss.regions.size()
                 && static_cast<std::size_t>(lord.capital) < ss.regions.size())
                    dist = region_distance(ss.regions[static_cast<std::size_t>(subj.capital)],
                                           ss.regions[static_cast<std::size_t>(lord.capital)], gw);

                const bool outrun = !reachable
                                  || dist > params.subject_secession_distance_q
                                  || subj.cohesion_q <= params.subject_secession_cohesion_q;
                if (outrun)
                {
                    out.dated_objects.erase(
                        std::remove_if(out.dated_objects.begin(), out.dated_objects.end(),
                            [&](const dated_object& o) {
                                return o.kind == static_cast<int32_t>(treaty_clause::tribute)
                                    && o.a == static_cast<uint16_t>(subj.id)
                                    && o.b == static_cast<uint16_t>(lord.id);
                            }),
                        out.dated_objects.end());
                    note_event(lapse_event_kind::subject_freed, subj.capital, subj.id, lord.id);
                    raise_grudge(subj.id, lord.id, grudge_kind::border_raided,
                                 subj.capital, y, params.grudge_border_raided);
                    subj.overlord = -1;
                    subj.subject_kind = -1;
                    ++out.subjections_freed;
                    continue;
                }

                // RENEWAL: the tribute clause's term may have lapsed this
                // round (`expire_dated_objects`, above the whole decision
                // gate). The subject just cleared every quantity secession
                // would have read, so the clause renews for another term
                // rather than silently going quiet -- "refusing renewal IS
                // the secession mechanism" implies a subject that does NOT
                // refuse renews.
                if (!has_treaty_clause(out, subj.id, lord.id, treaty_clause::tribute))
                    out.dated_objects.push_back(dated_object{
                        y + std::max<int64_t>(params.treaty_term_years, 1),
                        static_cast<int32_t>(treaty_clause::tribute),
                        static_cast<uint16_t>(subj.id), static_cast<uint16_t>(lord.id)});

                // BL-1097 -- THE STANDING TRAFFIC between a metropole and what
                // it holds: one sea leg, overlord capital to subject seat, per
                // decision round for as long as the tribute clause stands
                // (EXPLORATION.md sec The colonial tie is a sea lane). The
                // clause stands here by construction -- the subject just
                // renewed rather than refused -- and the leg is noted whether
                // or not the remittance below finds anything to move, since
                // the traffic is the link, not the amount.
                if (subj.capital >= 0 && lord.capital >= 0 && subj.capital != lord.capital)
                {
                    note_sea_leg(lord.capital, subj.capital);
                    ++out.sea_legs_noted_tribute;
                }

                if (params.treaty_tribute_rate_q > 0 && subj.capital >= 0 && lord.capital >= 0
                 && static_cast<std::size_t>(subj.capital) < ss.regions.size()
                 && static_cast<std::size_t>(lord.capital) < ss.regions.size())
                {
                    region& sseat = ss.regions[static_cast<std::size_t>(subj.capital)];
                    region& lseat = ss.regions[static_cast<std::size_t>(lord.capital)];
                    const int64_t amount = (sseat.treasury * params.treaty_tribute_rate_q) / 1000;
                    if (amount > 0)
                    {
                        sseat.treasury -= amount;
                        lseat.treasury = clampi64(lseat.treasury + amount, 0, 1LL << 48);
                        out.tribute_remitted += amount;
                    }
                }
            }
        }

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
        //
        // One function, `decay_grudges`, so the harness asserts the arithmetic
        // the round loop actually runs (BL-842).
        decay_grudges(out.grudges, params, step_years);

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

            if (held.empty())
            {
                // BL-916: the fallback death, with no killer knowable here.
                // The conquest path records its own at `extinguish_polity`
                // and marks `alive` false first, so this cannot double-count.
                note_event(lapse_event_kind::realm_ended, q.capital, q.id, -1);
                q.alive = false;
                continue;
            }
            if (q.capital < 0 || owner[static_cast<std::size_t>(q.capital)] != q.id)
            {
                // Capital fell. The successor is the polity's lowest-indexed
                // surviving region — placement order, which is best-ground
                // first, so it is a reasonable seat without being "the largest
                // holding" the first cut's comment claimed (BL-312).
                const int old_capital = q.capital;
                q.capital = held.front();
                note_event(lapse_event_kind::capital_moved, q.capital, q.id, old_capital); // BL-916

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
                        note_event(lapse_event_kind::civilisation_formed, hi, q.id, idx); // BL-916
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

            if (rc.capital != q.capital
             || rc.roads_version != roads_version // BL-837: a road crossed a tier
             || rc.owner_version != owner_version_of(q.id) // BL-922: ground changed hands
             || (params.centre_chain_reach
              && rc.centres_version != centres_version)) // BL-887: a relay moved
                rebuild_reach(rc, q.capital, q.id);
            else if (rc.cost.size() != ss.regions.size())
                // BL-922: a region founded by SOMEONE ELSE since the last
                // build. Not held here, so never expanded here: the vector
                // grows with the sentinel and the rest of it stands. A region
                // founded by THIS polity bumped `owner_version` and took the
                // branch above.
                rc.cost.resize(ss.regions.size(), 1 << 28);
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
            // Read the SAME capital-over-held-ground reach `campaign_supply`
            // prices a campaign at (`reach[hi]`, already road-discounted by
            // `rebuild_reach` above; BL-922 -- foreign ground impassable), but
            // WITHOUT the burden-of-breadth term: a garrison standing at home
            // pays neither a march's cost nor the empire's own overextension
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
                // BL-973 FINDING, NOT WIRED: the `reach` term is folded into
                // `polity::tree_mod_q` but deliberately NOT added here. At
                // the authored magnitudes (empire reach nodes sum to 540‰,
                // exploration's to 1380‰, against `work_reach_relief_cap_q`
                // = 800) technology alone discounts most of the terrain
                // price, and the BL-872 fixtures — cut-off ground stops
                // growing, a far campaign starves — stop holding. Whether
                // the store's magnitudes or the sim's cap moves is a
                // design call; until it is made, `reach` is on the stated
                // unread list (`tree_effect_declared_unread`).
                const int hub_reach_q = clampi(hp.work_reach_mod, 0, params.work_reach_relief_cap_q);
                // 64-BIT ON PURPOSE (BL-922): the unreachable sentinel is
                // 1 << 27, and sentinel x cost overflowed `int` at any cost
                // above ~15 -- cut-off ground then read as FULLY supplied
                // instead of as nothing. Latent under the old 10; live now.
                const int64_t terrain_cost = static_cast<int64_t>(reach_here)
                                           * leaned_terrain_reach_cost_q(params) / 100;
                const int64_t terrain_paid = terrain_cost - (terrain_cost * hub_reach_q) / 1000;
                const int supply_at_q  = static_cast<int>(clampi64(1000 - terrain_paid, 0, 1000));
                hp.network_supply_q = supply_at_q;

                if (params.unsustained_army_attrition_q > 0 && hp.army_stock > 0
                 && supply_at_q <= params.sustainable_garrison_floor_q)
                {
                    ++out.unsustained_attrition_events;
                    const int64_t lost = (hp.army_stock
                                         * clampi(params.unsustained_army_attrition_q, 0, 1000)
                                         * step_years) / 1000;
                    const int64_t stock_before = hp.army_stock;
                    hp.army_stock = std::max<int64_t>(0, hp.army_stock - lost);
                    scale_standing_army(hp, stock_before); // BL-955
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
            const roster_band mil_band = roster_band_for_capacity(
                clampi(q.capacity[static_cast<int>(sim_domain::military)], 1, 6));

            const roster_band mat_band = roster_band_for_capacity(
                clampi(q.capacity[static_cast<int>(sim_domain::materials)], 1, 6));

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
            // BL-922 -- ONE BASIS: THE CAPITAL, OVER HELD GROUND. The march is
            // priced as the staging hub's reach from the capital (`reach[hub]`,
            // the held-only Dijkstra above) plus ONE `edge_step` from the hub
            // onto the target, which is foreign ground by definition and so
            // carries no reach of its own. The separate staging-hub distance
            // term (`supply_decay_per_tile_q x hub_dist`) is GONE: it priced
            // "can I move at all" off a frontier region and barely decayed for
            // an ordinary neighbour-adjacent march, which is exactly why the
            // reach gate refused nothing (BL-905). What is left is the design's
            // question -- has this empire outrun its network -- asked of the
            // same `network_supply_q` the holding, growth and secession
            // readings use, plus the last hop.
            //
            // A hub the capital cannot reach over held ground (cut off by a
            // war, or across water) supplies nothing, and the gate refuses the
            // campaign: a block the capital cannot feed cannot project force.
            //
            // The two remaining terms are the STRATEGIC terrain-weighted reach
            // from the capital (BL-316 S2) and the BURDEN OF BREADTH (BL-316
            // S3). `hub` is the region the campaign is STAGED FROM, and it is
            // a parameter rather than a capture because its works discount the
            // terrain cost (BL-321): reach_mod is authored as a discount on the
            // supply cost through/from a region, so it is the staging
            // holding's roads and wharves doing the carrying, not the capital's.
            const auto campaign_supply = [&](std::size_t ti, int hub) {
                const std::size_t hs = static_cast<std::size_t>(hub);
                const bool hub_reached = hub >= 0 && hs < reach.size() && reach[hs] < (1 << 27);
                const int reach_here = hub_reached
                                     ? reach[hs] + edge_step(hub, static_cast<int>(ti))
                                     : (1 << 27);
                // (BL-973: the tree's `reach` term is NOT added here either —
                // see the holdings-supply site above for the finding.)
                const int hub_reach_q = (hub >= 0)
                    ? clampi(ss.regions[hs].work_reach_mod, 0, params.work_reach_relief_cap_q)
                    : 0;
                // 64-bit for the same reason as the holdings loop above: the
                // sentinel times the cost does not fit an `int`.
                const int64_t terrain_cost = static_cast<int64_t>(reach_here)
                                           * leaned_terrain_reach_cost_q(params) / 100;
                const int64_t terrain_paid = terrain_cost - (terrain_cost * hub_reach_q) / 1000;
                return static_cast<int>(clampi64(1000
                            - terrain_paid
                            - clampi(burden, 0, 1000 - params.holdings_burden_floor_q),
                            0, 1000));
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
                const int base_ration = clampi((params.sea_legs_ration_q * legs) / 1000, 0, 1000);

                // BL-935 -- A BUILT PORT (region::port_stock_q, an ASSET the
                // treasury paid for -- never the bare endowment window
                // `port_q` above) and this polity's standing navy lower the
                // cost of a crossing staged from it (EXPLORATION.md sec Force
                // persists now: "enables cheaper overseas skirmishes").
                // Multiplicative on the ration a seafaring people already
                // earned -- a people with no tradition at all (`legs` under
                // the floor, refused above) gets nothing from the best port
                // in the world. Widened past the bare ration's [0, 1000]
                // ceiling: this is the price gap reading 1 (displacement)
                // depends on.
                const int port_bonus_q = port_crossing_ration_bonus_q(h.port_stock_q, q.navy_stock);
                return clampi((base_ration * port_bonus_q) / 1000, 0, 2000);
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
            // BL-921 — FORCE FOLLOWS THE NETWORK, not a fixed radius.
            //
            // THE STRUCTURAL BUG THIS REPLACES (BL-835's model). A hub's own
            // garrison plus at most as much again from HALF of its immediate
            // neighbours meant a 150-region empire and a 3-region polity
            // fielded the SAME-sized stack at any border: success (holding
            // more ground) wrote nothing into force, and every brake tried on
            // a riser measured null (BL-838, BL-839, BL-887, BL-905) because
            // there was no riser to brake.
            //
            // THE SHAPE (Ben, 2026-09-11 form, ruling A on NR-836; NR-838):
            // A POOLED LEVY along the supplied network, priced from the
            // capital — no standing field army this sprint (NR-836's call is
            // parked). Every region this polity HOLDS contributes a share of
            // its garrison in proportion to `region::network_supply_q`, the
            // SAME capital-over-held-ground, road-discounted reach term
            // BL-872/BL-922 already compute once per round for every held
            // region (loop above, this same polity, this same round) — so a
            // realm with roads to its holdings brings more of itself to a
            // border than one without, and reach is finally the lever the
            // design says it is. Reading from the CAPITAL rather than from
            // the hub is deliberate, not a shortcut: it is the one network
            // this sim already prices everything else against, so a second,
            // hub-relative Dijkstra here would only restate the first at a
            // different root.
            //
            // THE HUB ITSELF IS UNCONDITIONAL, unchanged from BL-835: a hub
            // with no army of its own stages nothing (comment at the campaign
            // loop below), regardless of how well-supplied the rest of the
            // realm is — the garrison standing at the border does not need to
            // march to itself.
            //
            // NO SEPARATE CAP. The old 2x-the-hub ceiling existed because an
            // unbounded RADIUS sum could mass twenty garrisons on one
            // frontier; the network sum is bounded instead by
            // `network_supply_q` itself, which decays with terrain and
            // distance from the capital and reads 0 past sustainable reach —
            // ground the capital cannot supply contributes nothing, which is
            // the moderator, not an arbitrary multiple.
            //
            // WHAT'S TAKEN IS GONE, same as before: a contributing region's
            // `army_stock` is reduced by exactly its committed share while the
            // campaign runs, so a wide offensive still uncovers the regions it
            // draws from.
            //
            // `commit` is the only difference between the estimate and the
            // execution: the scorer asks the question, the campaign takes the
            // men. One rule, read twice, so a polity cannot be offered a force
            // it then fails to raise (this file's standing thesis — a cost
            // authored on one scale and spent on another is the bug).
            // BL-955: the paid standing heads a COMMITTED gather carried out,
            // so the survivors keep their paid status in proportion wherever
            // they end the campaign. Always 0 in the Empire span.
            int64_t committed_standing = 0;
            const auto gather_army = [&](int hub, bool commit) -> int64_t {
                const std::size_t hs = static_cast<std::size_t>(hub);
                int64_t total = ss.regions[hs].army_stock;
                if (total <= 0) return 0; // BL-835: a hub with no army stages nothing.
                if (commit)
                    committed_standing = draw_army_with_standing(ss.regions[hs], total); // the whole hub

                for (int hi : held)
                {
                    if (hi == hub) continue;
                    const std::size_t hii = static_cast<std::size_t>(hi);
                    const int64_t stock = ss.regions[hii].army_stock;
                    if (stock <= 0) continue;
                    const int supply_q = clampi(ss.regions[hii].network_supply_q, 0, 1000);
                    if (supply_q <= 0) continue;
                    const int64_t take = (stock * supply_q) / 1000;
                    if (take <= 0) continue;
                    total += take;
                    if (commit)
                        committed_standing += draw_army_with_standing(ss.regions[hii], take);
                }
                // Paid heads marched can never exceed the men marched.
                if (commit && committed_standing > total) committed_standing = total;
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
            // BL-929: the corridor's OTHER endpoint when `upgrade_supply`
            // chose a corridor site (`best_target` is the near endpoint, the
            // one whose `network_supply_q` the purchase was scored against);
            // -1 when the chosen site is a region's own relief instead.
            int      best_supply_partner = -1;
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
            int      best_dclass  = 0; // BL-950 diagnostic, trace only.
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
            // BL-1019, trace only: the best score an UNMET-owner candidate
            // (the first crossing) cleared the threshold with this round, so
            // the round's end can say what beat it. Never compared by a decision.
            int      best_unmet_score     = 0;
            bool     unmet_cleared_now    = false;

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
                    // BL-950 DIAGNOSTIC (trace only): is the target's owner a
                    // pre-span neighbour (0), met during the span (1) or unmet
                    // (2)? Read by nothing in the sim.
                    const int dclass = params.trace_battles
                        ? [&]() {
                              const int64_t fy = contact_first_year(out, q.id, to);
                              return fy == INT64_MAX ? 2
                                   : (fy < params.near_home_cutoff_year ? 0 : 1); // BL-1036
                          }()
                        : 0;
                    if (params.trace_battles) ++out.campaign_class_trace[dclass][0];
                    // BL-933 -- A BOUND NON-AGGRESSION CLAUSE MAKES A
                    // CAMPAIGN ILLEGAL, not merely costly, exactly like the
                    // water gate just below (EXPLORATION.md sec What a
                    // treaty is: "neither party campaigns against the other
                    // while the term runs"). This is the direct, countable
                    // cause of the displacement reading's neighbour-war half.
                    if (has_treaty_clause(out, q.id, to, treaty_clause::non_aggression))
                    {
                        ++out.treaty_blocked_campaigns;
                        if (params.trace_battles) ++out.campaign_class_trace[dclass][1];
                        continue;
                    }
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
                        if (params.trace_battles) ++out.campaign_class_trace[dclass][2];
                        continue;
                    }
                    // Forage is the SAME reading: fed on one's own ground or
                    // one's own shore, starving on a sea leg carried by ships.
                    // Ships get the force there; they do not feed it.
                    const bool forages = dry || shore;

                    const region& tgt = ss.regions[ti];
                    const int cap_dist = region_distance(cap, tgt, gw);

                    // SUPPLY IS PRICED FROM THE CAPITAL OVER HELD GROUND
                    // (BL-922, superseding the 2026-08-12 staging-hub model).
                    // `hi` is the polity's own region adjacent to the target;
                    // `campaign_supply` reads the capital's reach TO that hub
                    // plus one step onto the target, so a large polity CAN
                    // attack its own border -- provided its network actually
                    // reaches that border. Reach is a property of empire shape
                    // on purpose: that is the question the gate asks.
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
                    const int64_t def_men = defender_levy_estimate(tgt, params);
                    // BL-973: the DEFENDER's held `defence` nodes stack with
                    // its ground's works, in the same clamp.
                    const int def_works   = clampi(tgt.work_defence_mod
                                                 + nation_tree_mod_q(out.polities, owner[ti],
                                                                     io::tree_modifier_term::defence),
                                                   0, 1000);
                    const int def_scaled  = static_cast<int>(clampi64(
                        (def_men / 64) * (1000 + def_works) / 1000, 0, 1000));

                    // Three DIFFERENT axes, so a polity can prize farmland and
                    // shrug at ore rather than merely valuing everything alike.
                    int value = (jitter(params.w_farm, qs, 0xA1u, 160) * tgt.farm_q
                              +  jitter(params.w_ore,  qs, 0xB2u, 160) * tgt.ore_q
                              +  jitter(params.w_port, qs, 0xC3u, 160) * tgt.port_q) / 1000;

                    // BL-924 -- A CITY IS A PRIZE, A SEAT IS THE PRIZE. Read
                    // directly off the target, not through the shared
                    // `region_value_q` every other verb's score is built
                    // from — see `campaign_prize_q`'s own comment for why.
                    value += (jitter(params.w_prize, qs, 0xD4u, 160)
                              * campaign_prize_q(tgt)) / 1000;

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

                    // BL-933 -- MUTUAL DEFENCE: "makes a small polity
                    // expensive to eat" (EXPLORATION.md sec What a treaty
                    // is), read as a value discount rather than a second
                    // legality gate -- attacking a defended target stays
                    // legal, it just prices worse. Read ANY living ally bound
                    // to the target by the clause; the discount scales with
                    // the strongest ally's own cohesion, which is the only
                    // "how seriously would they actually fight" reading this
                    // file has cheaply available (the same term the odds
                    // calculation below reads for the primary combatants).
                    int ally_discount_trace = 0; // BL-1019 trace only
                    {
                        int ally_deterrence_q = 0;
                        for (const polity& ally : out.polities)
                        {
                            if (!ally.alive || ally.id == to || ally.id == q.id) continue;
                            if (!has_treaty_clause(out, to, ally.id, treaty_clause::mutual_defence))
                                continue;
                            ally_deterrence_q = std::max(ally_deterrence_q,
                                                          clampi(ally.cohesion_q, 0, 1000));
                        }
                        if (ally_deterrence_q > 0)
                            value -= (value * ally_deterrence_q) / 2000; // up to -50% at full cohesion
                        ally_discount_trace = ally_deterrence_q;
                    }

                    // In the currency: expected value TAKEN, discounted by the
                    // odds of taking it, less what the attempt costs.
                    value = (value * params.campaign_gain_q) / 1000;
                    const int value_prize_trace = value; // BL-1019 trace: the ground's worth before odds and costs

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
                        forages ? campaign_supply(ti, hi)
                                : (sl_ration > 0
                                       ? (campaign_supply(ti, hi) * sl_ration) / 1000
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
                        if (params.trace_battles) ++out.campaign_class_trace[dclass][3];
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
                    const int value_costed_trace = value; // BL-1019 trace: after odds, distance and supply cost
                    int cult_keep_trace = 1000;           // BL-1019 trace: the share foreignness leaves
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
                        cult_keep_trace = 1000 - cult_q;
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

                    // BL-868 -- THE CREED'S APPETITE, leaning the value the
                    // score is built from. Symmetric around a neutral 500 so
                    // a peaceable people is discouraged exactly as much as a
                    // warlike one is encouraged, and proportional so it
                    // scales with the prize rather than swamping it. Applied
                    // to `value` for the same reason w_cult is: one term
                    // reads a richer input instead of a second term being
                    // added beside the score. `q.aggression_q` already carries
                    // BL-839's turbulence lean (`leaned_aggression_q`), so this
                    // site reads it as-is.
                    //
                    // APPLIED ONCE, OUTSIDE THE SEASON LOOP (BL-927, NR-831).
                    // The creed's appetite is a property of the DECIDER, not of
                    // the campaigning weather, so summer and winter must read
                    // the same leaned value -- the same rule the fear lean above
                    // follows. Sited inside the loop it re-leaned the already-
                    // leaned summer value for winter (x1.30 became x1.69 at
                    // w_aggr_q = 300), a squaring nobody wrote down.
                    if (params.w_aggr_q != 0)
                    {
                        const int lean =
                            (params.w_aggr_q * (clampi(q.aggression_q, 0, 1000) - 500)) / 500;
                        value = value + (value * lean) / 1000;
                        if (value < 0) value = 0;
                    }

                    // BL-953 -- A WANT POINTS THE CAMPAIGN OUTWARD
                    // (EXPLORATION.md sec A want points a campaign outward).
                    // The decider's own want for the good this ground holds,
                    // weighted by its people's LIVE preference (the round's
                    // `round_prefs`, derived once above), leans the prize up.
                    // Same idiom as the fear and appetite leans: one term
                    // reads a richer input, applied ONCE, outside the season
                    // loop, because the want is a property of the decider and
                    // the ground, not of the weather. Only a positive prize
                    // leans -- a want ranks winnable campaigns, it never
                    // rescues an unwinnable one. Off (and byte-identical) in
                    // the Empires span and at `w_want_q` 0.
                    if (want_lean_on)
                        value = want_leaned_campaign_value(
                            value, params.w_want_q,
                            polity_good_want_q(ss.regions, out.polities, round_prefs, q.id,
                                               tgt.dominant));

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
                    // BL-1019 DIAGNOSTIC (trace only): what an in-reach
                    // candidate of each contact class is scored ON, summed so
                    // the sweep can set a first crossing's terms beside a
                    // known neighbour's. Pure reads of locals already computed
                    // (and one pure `campaign_prize_q`); no jitter, no RNG.
                    // Read by nothing in the sim.
                    if (params.trace_battles)
                    {
                        int64_t* ct = out.class_score_trace[dclass];
                        ++ct[0];
                        ct[1] += value_prize_trace;
                        ct[2] += p_win_q;
                        ct[3] += supply_here;
                        ct[4] += value;
                        ct[5] += (params.w_def * def_scaled) / 2000;
                        ct[6] += cap_dist;
                        if (!forages) ++ct[7];
                        ct[8]  += campaign_prize_q(tgt);
                        ct[9]  += (params.w_farm * tgt.farm_q + params.w_ore * tgt.ore_q
                                 + params.w_port * tgt.port_q) / 1000;
                        ct[10] += value_costed_trace;
                        ct[11] += cult_keep_trace;
                        if (ally_discount_trace > 0) ++ct[12];
                    }

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
                            ++out.campaign_class_trace[dclass][4];
                            campaign_cleared_now = true;
                            if (s > best_campaign_score) best_campaign_score = s;
                            if (dclass == 2 && (!unmet_cleared_now || s > best_unmet_score)) // BL-1019
                            {
                                unmet_cleared_now = true;
                                best_unmet_score  = s;
                            }
                        }

                        if (s > best_score && s >= params.campaign_threshold_q)
                        {
                            best_score = s; best_verb = sim_verb::campaign;
                            best_target = static_cast<int>(ti); best_winter = winter;
                            best_p_win_q = p_win_q; best_hub = hi; // trace only
                            best_dclass = dclass;                  // trace only
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
            // -- Organise (BL-920) ------------------------------------------
            //
            // Growth over UNORGANISED culture ground the polity's network
            // can reach (CIVILISATION.md sec "The unit is the city state").
            // Bounded by construction like the other four: the polity's own
            // frontier, one neighbour deep, one candidate chosen. Priced by
            // KINSHIP (`culture_opposition_q`, 0 own people, 1000 fully
            // opposed): own culture cheapest, kin dearer, and a people at or
            // above `organise_opposition_bar_q` is refused here — it can
            // only be taken by Campaign, above.
            {
                for (int hi : held)
                {
                    for (int tn : neighbours[static_cast<std::size_t>(hi)])
                    {
                        const std::size_t ti = static_cast<std::size_t>(tn);
                        if (owner[ti] >= 0) continue; // Somebody's already -- Campaign's ground, not this verb's.
                        const region& tgt = ss.regions[ti];
                        const int tgt_culture = tgt.culture.plurality();
                        if (tgt_culture < 0) continue;

                        const int op_q = (cs != nullptr)
                            ? culture_opposition_q(cs->cultures, q.culture, tgt_culture)
                            : 0;
                        if (op_q >= params.organise_opposition_bar_q) continue;

                        // BL-922 -- THE NETWORK MUST SUPPLY THE GROUND, read
                        // exactly as `campaign_supply` reads a march's last
                        // hop: the hub's own reach from the capital, plus ONE
                        // `edge_step` onto ground the polity does not yet
                        // hold.
                        const std::size_t hidx = static_cast<std::size_t>(hi);
                        const int reach_here = (hidx < reach.size() && reach[hidx] < (1 << 27))
                                              ? reach[hidx] : (1 << 27);
                        const int64_t terrain_cost =
                            static_cast<int64_t>(reach_here + edge_step(hi, tn))
                                * leaned_terrain_reach_cost_q(params) / 100;
                        const int supply_here_q =
                            static_cast<int>(clampi64(1000 - terrain_cost, 0, 1000));
                        if (supply_here_q <= params.organise_reach_floor_q) continue;

                        const int s = (region_value_q(tgt) * (1000 - op_q)) / 1000;
                        if (s > best_score && s >= params.organise_threshold_q)
                        {
                            best_score = s; best_verb = sim_verb::organise;
                            best_target = static_cast<int>(ti); best_winter = false;
                        }
                    }
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

            // -- Upgrade supply site (BL-929) -------------------------------
            //
            // SCORED LAST, after `build_work`, for the same reason build_work
            // is scored after everything above it: `>` against `best_score`
            // means this must strictly beat every other candidate, and it is
            // the newest verb in the contest.
            //
            // OFFERED ONLY WHERE SUPPLY IS ALREADY LOW: every held region at
            // or below `sustainable_settlement_floor_q` — the SAME floor
            // growth (BL-872) and secession (BL-896) already read, so "low"
            // means what it already means everywhere else in this file.
            // "Falling" is not separately tracked (this file keeps no
            // history of last round's own reading to compare against, the
            // same one-round-at-a-time contract `network_supply_q` is read
            // under everywhere else), so a region already at or under the
            // floor stands in for both readings the design names.
            //
            // TWO SITE KINDS, ONE PRICE, scored against the SAME identity
            // `network_supply_q` is computed from just above — never a
            // second Dijkstra. Candidate A is the region's OWN relief;
            // candidate B is ONE incoming edge from a held neighbour, asked
            // "what would this region's reach be if THIS edge alone were
            // promoted" via `edge_step_at_tier`. Neither candidate re-walks
            // the graph, so a purchase can only ever be scored against the
            // one region it was asked about — a corridor's benefit to
            // regions FARTHER OUT than the one scored here is real (the
            // Dijkstra would carry a cheaper edge onward) but uncounted,
            // which understates a deep corridor's value rather than
            // overstating it. Honest and bounded, not exact.
            if (params.supply_upgrade_material_cost > 0
             && q.capital >= 0 && static_cast<std::size_t>(q.capital) < ss.regions.size()
             && ss.regions[static_cast<std::size_t>(q.capital)].material_stock
                    >= params.supply_upgrade_material_cost)
            {
                for (int hi : held)
                {
                    const std::size_t hidx = static_cast<std::size_t>(hi);
                    region& hp = ss.regions[hidx];
                    const int old_supply_q = hp.network_supply_q;
                    if (old_supply_q > params.sustainable_settlement_floor_q) continue;

                    const int reach_here = (hidx < reach.size() && reach[hidx] < (1 << 27))
                                          ? reach[hidx] : (1 << 27);
                    if (reach_here >= (1 << 27)) continue; // unreachable: no purchase fixes this

                    // -- Candidate A: this region's own relief -------------
                    if (hp.work_reach_mod < params.work_reach_relief_cap_q)
                    {
                        const int new_hub_reach_q = clampi(
                            hp.work_reach_mod + params.supply_upgrade_reach_gain_q,
                            0, params.work_reach_relief_cap_q);
                        const int64_t terrain_cost = static_cast<int64_t>(reach_here)
                                                   * leaned_terrain_reach_cost_q(params) / 100;
                        const int64_t terrain_paid =
                            terrain_cost - (terrain_cost * new_hub_reach_q) / 1000;
                        const int new_supply_q =
                            static_cast<int>(clampi64(1000 - terrain_paid, 0, 1000));
                        const int gain_q = clampi(new_supply_q - old_supply_q, 0, 1000);
                        if (gain_q > 0)
                        {
                            const int s = static_cast<int>(
                                (static_cast<int64_t>(region_value_q(hp)) * gain_q) / 1000);
                            if (s > best_score && s >= params.supply_upgrade_threshold_q)
                            {
                                best_score = s; best_verb = sim_verb::upgrade_supply;
                                best_target = hi; best_supply_partner = -1;
                                best_winter = false;
                            }
                        }
                    }

                    // -- Candidate B: one incoming edge, held neighbour only
                    for (int nb : neighbours[hidx])
                    {
                        const std::size_t nidx = static_cast<std::size_t>(nb);
                        if (owner[nidx] != q.id) continue;
                        if (nidx >= reach.size() || reach[nidx] >= (1 << 27)) continue;
                        const int cur_tier = road_tier_between(nb, hi);
                        if (cur_tier >= 2) continue;
                        const int64_t candidate_reach = static_cast<int64_t>(reach[nidx])
                                                       + edge_step_at_tier(nb, hi, cur_tier + 1);
                        if (candidate_reach >= reach_here) continue; // not this region's best path

                        const int64_t terrain_cost = candidate_reach
                                                   * leaned_terrain_reach_cost_q(params) / 100;
                        const int hub_reach_q =
                            clampi(hp.work_reach_mod, 0, params.work_reach_relief_cap_q);
                        const int64_t terrain_paid =
                            terrain_cost - (terrain_cost * hub_reach_q) / 1000;
                        const int new_supply_q =
                            static_cast<int>(clampi64(1000 - terrain_paid, 0, 1000));
                        const int gain_q = clampi(new_supply_q - old_supply_q, 0, 1000);
                        if (gain_q <= 0) continue;

                        const int s = static_cast<int>(
                            (static_cast<int64_t>(region_value_q(hp)) * gain_q) / 1000);
                        if (s > best_score && s >= params.supply_upgrade_threshold_q)
                        {
                            best_score = s; best_verb = sim_verb::upgrade_supply;
                            best_target = hi; best_supply_partner = nb;
                            best_winter = false;
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
            // BL-1019: the same fork for the FIRST CROSSING alone. A round in
            // which an unmet owner's ground cleared the bar -- what won it?
            // Trace only: reads the round's settled argmax, writes counters.
            if (params.trace_battles && unmet_cleared_now)
            {
                ++out.unmet_contest_trace[0];
                if (best_verb == sim_verb::campaign && best_dclass == 2)
                    ++out.unmet_contest_trace[1];
                else
                {
                    if (best_verb == sim_verb::campaign)
                        ++out.unmet_contest_trace[best_dclass == 0 ? 2 : 3];
                    else
                    {
                        ++out.unmet_contest_trace[4];
                        ++out.unmet_lost_to_verb_trace[static_cast<int>(best_verb) & 7];
                    }
                    out.unmet_contest_margin_sum += best_score - best_unmet_score;
                }
            }

            if (best_verb == sim_verb::none) best_verb = sim_verb::consolidate;

            // ---- Execute -------------------------------------------------
            switch (best_verb)
            {
            case sim_verb::campaign:
            {
                if (params.trace_battles) ++out.campaign_chosen;
                if (params.trace_battles) ++out.campaign_class_trace[best_dclass][5];
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
                // scorer used. `src` is the staging hub; the army is fed from
                // the capital over held ground to that hub and one step on
                // (BL-922), through the terrain-weighted reach term inside
                // `campaign_supply`.
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
                // Read once and named (BL-1022) so the battle trace can record
                // what this march would have drawn foraging. `campaign_supply`
                // is a pure read of this round's reach and burden, so asking it
                // here rather than inside each arm below changes no value.
                const int exec_forage_supply = campaign_supply(ti, src);
                const int atk_supply =
                    exec_forages ? exec_forage_supply
                                 : (exec_ration > 0
                                        ? (exec_forage_supply * exec_ration) / 1000
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
                // BL-1097 -- A WET CAMPAIGN'S CROSSING is a sea leg too, noted
                // BESIDE the land note above (never instead of it, so the road
                // record is byte for byte what it was): staging hub to target,
                // at launch, whenever the march is not over dry contact -- and only
                // in the spans whose record the lane belongs to (Exploration and
                // Industrialisation, the ones that run the upkeep; EXPLORATION.md
                // § The colonial tie is a sea lane): the Empires span walks wet
                // campaigns too and notes none, so round 4 draws no lane (the
                // review's fix round on BL-1097).
                if (!exec_dry && params.exploration_upkeep_enabled)
                {
                    note_sea_leg(src, static_cast<int>(ti));
                    ++out.sea_legs_noted_campaign;
                    // BL-1095 -- THE WET CAMPAIGN IS A MOMENT OF ITS OWN on the
                    // record (EXPLORATION.md § Force persists now): its target,
                    // its attacker and the staging hub it was victualled from,
                    // so the map draws the crossing as a sail. Same gate as
                    // the leg, so the Empires record carries none; a pure
                    // record write, read by nothing below.
                    note_event(lapse_event_kind::sea_leg_campaign,
                               static_cast<int>(ti), q.id, src);
                }

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

                    // BL-1021 -- the BL-895 reading: what this campaign cost
                    // against what the seat could pay, by how many kinds of
                    // ground the acting realm reached this year.
                    {
                        const std::size_t kb = trade_bucket_of(q.id);
                        ++out.campaigns_by_trade_bucket[kb];
                        out.campaign_cost_by_trade_bucket[kb]  += material_cost;
                        out.campaign_spent_by_trade_bucket[kb] += material_spent;
                    }

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

                const int def_works_q = clampi(tgt.work_defence_mod
                                             + nation_tree_mod_q(out.polities, owner[ti],
                                                                 io::tree_modifier_term::defence), // BL-973
                                               0, 1000);
                const int def_ready = (best_winter ? (1000 - params.winter_readiness_penalty_q) : 1000)
                                    + def_works_q;

                const polity* dq = nullptr;
                for (const polity& o : out.polities)
                    if (o.id == owner[ti]) { dq = &o; break; }

                roster_band atk_band = roster_band::classical;
                std::vector<army_stack_entry> atk =
                    build_stack(raised, home, q, atk_ready, !exec_dry, &atk_band);
                note_units_fielded(out, atk_band, atk);
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
                    build_stack(def_men, tgt, dq ? *dq : q, def_ready, !exec_dry, &def_band);
                note_units_fielded(out, def_band, def);

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
                scale_standing_army(tgt, def_men); // BL-955: paid defenders die alike
                // BL-955: the attacker's paid heads that came through, in proportion.
                const int64_t atk_standing_survivors = std::min(atk_survivors,
                    raised > 0 ? (std::min(committed_standing, raised) * atk_survivors) / raised : 0);

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
                    bt.exec_dry        = exec_dry;          // BL-1022
                    bt.exec_forages    = exec_forages;
                    bt.ration_q        = exec_ration;
                    bt.forage_supply_q = exec_forage_supply;
                    bt.attacker_lost   = atk_lost;
                    bt.defender_lost   = def_lost;
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
                    // BL-955: the defender's paid men are gone with the ground;
                    // the conqueror's paid survivors garrison it as its own.
                    tgt.standing_army       = std::min(atk_standing_survivors, tgt.army_stock);
                    tgt.standing_army_owner = q.id;

                    const int loser_id = dq ? dq->id : -1;
                    const bool was_seat =
                        dq && dq->capital == static_cast<int>(ti);

                    owner[ti]  = q.id;
                    tgt.nation = q.id;
                    void_stale_standing_army(tgt); // BL-955 (the conqueror's paid survivors set above)
                    touch_owner(loser_id); // BL-922: ground changed hands, both
                    touch_owner(q.id);     // realms' held-only reach is stale

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
                            touch_owner(owner[hi]); // BL-922: whoever held it
                            owner[hi] = q.id;
                            h.nation  = q.id;
                            void_stale_standing_army(h); // BL-955
                            // BL-922: RECORDED, like every other transfer.
                            // The seat's own change is pushed below; its
                            // hinterland's were not, so a replay of the
                            // change list (`owner_slice_at`) drifted from the
                            // map by every region a seat capture carried.
                            out.owner_changes.push_back(owner_change{
                                static_cast<int32_t>(y), static_cast<uint16_t>(hi),
                                static_cast<uint16_t>(q.id)});
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

                    // BL-908 — A CAMPAIGN THAT TOOK GROUND IS A MEETING, first
                    // and regardless of the grudge magnitude: q now knows the
                    // loser exists (and the reverse), which a grudge alone
                    // cannot say if `grudge_ground_taken` were ever tuned to 0.
                    if (loser_id >= 0)
                        raise_contact(q.id, loser_id, contact_kind::campaign,
                                     static_cast<int>(ti), y);
                    out.owner_changes.push_back(owner_change{
                        static_cast<int32_t>(y), static_cast<uint16_t>(ti),
                        static_cast<uint16_t>(q.id)});
                    out.history.push_back(history_event{
                        years_from_calendar_year(y), chain_stage::legacy,
                        tgt.name + " changes hands", std::string{}});
                    // BL-916: a SEAT falling is the named moment; a march
                    // changing hands stays an ownership change on the map.
                    if (was_seat)
                        note_event(lapse_event_kind::seat_captured,
                                   static_cast<int>(ti), q.id, loser_id);

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

                    // BL-908 — A RAID THAT MOVED NOTHING IS STILL A MEETING.
                    raise_contact(q.id, dq->id, contact_kind::campaign,
                                 static_cast<int>(ti), y);
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
                // round's `gather_army` will redraw it from the realm's network
                // (BL-921) if it marches again.
                if (!takes_it)
                {
                    home.army_stock = clampi64(home.army_stock + atk_survivors,
                                               0, 1LL << 40);
                    // BL-955: paid survivors come home paid.
                    if (atk_standing_survivors > 0)
                    {
                        const int64_t home_paid = standing_army_heads(home); // 0 if not q's
                        home.standing_army       = std::min(home_paid + atk_standing_survivors,
                                                            home.army_stock);
                        home.standing_army_owner = q.id;
                    }
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
                // The seam's keep: one constant, so the fuel survey's inheritance
                // (BL-1041 DEFAULT A, below) cannot drift from energy_q's.
                constexpr int daughter_seam_keep_q = 700;
                np.farm_q = (src.farm_q * 850) / 1000;
                np.ore_q  = (src.ore_q  * 700) / 1000;
                np.port_q = (src.port_q * 700) / 1000;
                np.energy_q = (src.energy_q * daughter_seam_keep_q) / 1000;
                // BL-1041 DEFAULT A (RULED, Ben 2026-09-18, wave 1 form): ground
                // founded after the span-open survey has no tiles to survey, so
                // it inherits its parent's SURVEYED fuel at the same discount,
                // exactly as `energy_q` inherits. Only where the parent was
                // surveyed (>= 0): off the span nothing is, and this is inert.
                // The forest survey is not inherited (a daughter's woods are
                // not its parent's); it stays -1, "not surveyed".
                if (params.industry_survey_inherits_at_founding && src.survey_fuel_q >= 0)
                    np.survey_fuel_q = (std::min(src.survey_fuel_q, 1000) * daughter_seam_keep_q) / 1000;
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
                touch_owner(q.id); // BL-922: new held ground changes reach
                neighbours.emplace_back();
                supply_neighbours.emplace_back();
                degree.push_back(0);
                link_region(ss.regions.size() - 1); // Keep the index complete.

                // BL-914: the Settle verb's own founding, the far commoner of
                // this function's two region-creating sites (see the founding
                // schedule above, near the top of the year loop) — most of a
                // run's regions are minted HERE, not there.
                if (tap != nullptr)
                {
                    tap_region_col.push_back(ss.regions.back().col);
                    tap_region_row.push_back(ss.regions.back().row);
                    tap_region_name.push_back(ss.regions.back().name);
                    tap->publish_regions(tap_region_col, tap_region_row, tap_region_name);
                }

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
                ++out.foundings_settled; // BL-926: the Settle verb chose it.
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
                // BL-973: the polity's held `industrial` nodes pull the
                // same crossing forward, beside its works' mean.
                const bool  ind_domain = (d == static_cast<int>(sim_domain::materials));
                const int   ind_boost  = ind_domain
                    ? clampi(mean_industrial_q + tree_mod_q(q, io::tree_modifier_term::industrial), 0, 1000)
                    : 0;
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

                // ---- BL-912: THE EMPIRE TREE, INVESTED ALONGSIDE THE BAND -
                //
                // The domain band above is the DERIVED reading TREES.md keeps
                // for the works gate and the unit roster boundary; this is
                // the real state -- "the sim reads nodes, not bands"
                // (TREES.md sec The sim reads nodes). Same Invest verb, same
                // round: choosing a node is an argument to the verb the AI
                // grant register already covers (AI_OPPONENT.md sec 11), not
                // a new one.
                {
                    // Ground means, recomputed here: BL-767's own `ground_q`
                    // locals above went out of scope with their block, and
                    // this is cheap over one polity's own holdings.
                    int64_t farm_sum = 0, ore_sum = 0, energy_sum = 0, port_sum = 0;
                    for (int hi : held)
                    {
                        const region& hp = ss.regions[static_cast<std::size_t>(hi)];
                        farm_sum   += hp.farm_q;
                        ore_sum    += hp.ore_q;
                        energy_sum += hp.energy_q;
                        port_sum   += hp.port_q;
                    }
                    const int ground_farm_q = static_cast<int>(farm_sum   / n_held);
                    const int ground_ore_q  = static_cast<int>(ore_sum    / n_held);
                    const int ground_fuel_q = static_cast<int>(energy_sum / n_held);
                    const int ground_port_q = static_cast<int>(port_sum   / n_held);

                    // "Bound" terms reuse this round's own domain arrears —
                    // the same shape BL-767's `pull` already reads — for the
                    // three the empire tree also names.
                    const int reach_bound_q    = clampi(
                        (6 - q.capacity[static_cast<int>(sim_domain::transport)]) * 180, 0, 1000);
                    const int manpower_bound_q = clampi(
                        (6 - q.capacity[static_cast<int>(sim_domain::military)]) * 180, 0, 1000);
                    const int food_bound_q     = clampi(
                        (6 - q.capacity[static_cast<int>(sim_domain::agriculture)]) * 180, 0, 1000);

                    // Stores: how empty the seat sits. A placeholder scale
                    // (TREES.md sec Effects: "magnitudes are authored by
                    // judgement"), read off the seat's own stock rather than
                    // invented from nothing.
                    const region& seat = ss.regions[static_cast<std::size_t>(q.capital)];
                    const int stores_low_q = clampi(
                        1000 - static_cast<int>(clampi64(seat.material_stock / 4, 0, 1000)), 0, 1000);
                    const int surplus_q = 1000 - std::max(
                        { reach_bound_q, manpower_bound_q, food_bound_q, stores_low_q });

                    // Re-pick only when nothing is targeted, or the target
                    // went dark under it (a rival fork side closed, or the
                    // ring it needs is no longer the frontier — cannot
                    // happen without diffusion, kept as a real check anyway).
                    if (q.empire_investing < 0
                     || !empire_node_available(q.empire_mask, q.empire_investing))
                    {
                        q.empire_investing = static_cast<int16_t>(choose_empire_node(
                            q.empire_mask, q.cohesion_q, stores_low_q, reach_bound_q,
                            manpower_bound_q, food_bound_q, ground_ore_q, ground_farm_q,
                            ground_fuel_q, ground_port_q, surplus_q));
                        q.empire_progress_q = 0;
                    }

                    if (q.empire_investing >= 0)
                    {
                        // RESEARCH IS A FLOW (TREES.md sec State): a fraction
                        // of the industry slice, scaled by the spire ring
                        // held and by contact degree. Never a stockpile —
                        // earned and spent the same round, on the node
                        // currently targeted.
                        int64_t industry_sum = 0;
                        for (int hi : held)
                            industry_sum += region_industry_output(
                                ss.regions[static_cast<std::size_t>(hi)]);

                        int spire_ring = 0;
                        for (int i = 0; i < io::empire_tree::node_count; ++i)
                        {
                            const io::empire_tree::node& nd = io::empire_tree::nodes[i];
                            if (nd.kind == io::empire_tree::node_kind::milestone
                             && (q.empire_mask & (1ULL << i)))
                                spire_ring = std::max(spire_ring, static_cast<int>(nd.ring));
                        }

                        // BL-908's directed contact table ("who has met
                        // whom") is what TREES.md sec State names as the
                        // second research scale. The table EXISTS
                        // (`history_sim_state::contacts`, read through
                        // `has_contact`; the Industry scorer's `known` term
                        // reads it, BL-1038), but THIS rate does not:
                        // contact_degree stays 0 (an inert multiplier,
                        // `(1000+0*40)/1000 == 1`) because wiring it moves
                        // every shipped world's empire tree, which is a
                        // re-bless decision, not a comment fix. (The note
                        // this replaces said BL-908 had not landed; it had.)
                        const int contact_degree = 0;

                        // BL-973: the `research` modifier term is the
                        // tree's own reader of itself — every held
                        // research node scales the flow (TREES.md sec
                        // Effects: "the research rate itself").
                        const int64_t research_q =
                            (industry_sum * params.empire_research_fraction_q) / 1000
                                * (1000 + spire_ring * 150) / 1000
                                * (1000 + clampi(contact_degree, 0, 10) * 40) / 1000
                                * (1000 + clampi(tree_mod_q(q, io::tree_modifier_term::research), 0, 4000)) / 1000;
                        q.empire_progress_q += static_cast<int32_t>(
                            clampi64(research_q * step_years, 0, INT32_MAX));

                        const io::empire_tree::node& tn =
                            io::empire_tree::nodes[q.empire_investing];
                        const int tn_kind_base =
                            tn.kind == io::empire_tree::node_kind::minor ? 1
                          : tn.kind == io::empire_tree::node_kind::major ? 3 : 6;
                        const int tn_cost =
                            params.capacity_band_cost * tn_kind_base * static_cast<int>(tn.ring);
                        if (q.empire_progress_q >= tn_cost)
                        {
                            q.empire_mask |= (1ULL << q.empire_investing);
                            q.empire_progress_q = 0;
                            q.empire_investing  = -1;
                            apply_tree_effects(q); // BL-973: the surface follows the mask at once
                        }
                    }

                    // ---- BL-930: THE EXPLORATION TREE, invested alongside -
                    // the empire tree, in the SAME round, on the SAME held-
                    // ground means computed above. Gated on the empire rim
                    // (`polity_holds_empire_rim`): EX-SP-1a's own gate is the
                    // Empire tree's rim milestone (`EXPLORATION_TREE.md` sec
                    // The spire; CIVILISATION.md sec "Some peoples gain the
                    // capacity to explore"), so a polity that never reaches
                    // that rim never invests here at all -- exactly the
                    // per-polity threshold that doc names, expressed as an
                    // ordinary availability gate rather than a special case.
                    if (polity_holds_empire_rim(q))
                    {
                        if (q.exploration_investing < 0
                         || !exploration_node_available(q.exploration_mask, q.exploration_investing))
                        {
                            // BL-932 -- PURSE_LOW, same shape as `stores_low_q`
                            // above (a placeholder scale, TREES.md sec Effects),
                            // read off the capital's own `treasury` (`seat`,
                            // declared above in this same block) rather than
                            // its `material_stock`: how empty the PURSE sits,
                            // as distinct from how empty the granary does.
                            const int purse_low_q = clampi(
                                1000 - static_cast<int>(clampi64(seat.treasury / 4, 0, 1000)),
                                0, 1000);
                            // BL-939 -- WANTS_UNMET, read off the capital's
                            // OWN market (a polity always knows its own
                            // want, so no contact gate applies here -- that
                            // gate is `market_scarcity_q`'s, for reading a
                            // FOREIGN market). Mean over the four goods;
                            // zero where the capital never stood a market.
                            int wants_unmet_q = 0;
                            if (seat.has_market)
                            {
                                int sum_q = 0;
                                for (int g = 0; g < 4; ++g) sum_q += seat.scarcity_q[g];
                                wants_unmet_q = clampi(sum_q / 4, 0, 1000);
                            }
                            // BL-940 -- THROUGHPUT_BOUND: how constrained the
                            // polity's own network currently runs, read off
                            // `region::network_supply_q` (already 0-1000,
                            // terrain-and-road-priced reach from the capital
                            // over held ground -- EXPLORATION.md sec Goods
                            // move as throughput: "throughput in everything
                            // but name") over the SAME `held` set the ground
                            // means above already walk. Mean supply, inverted:
                            // a realm whose own roads run thin scores high
                            // here, which is what makes Post Roads (EX-WY-1a)
                            // and its ring-2 siblings worth investing in.
                            int64_t supply_sum_q = 0;
                            for (int hi : held)
                                supply_sum_q += ss.regions[static_cast<std::size_t>(hi)].network_supply_q;
                            const int throughput_bound_q = clampi(
                                1000 - static_cast<int>(supply_sum_q / n_held), 0, 1000);
                            // BL-934 -- SUBJECT_HELD, now real: does this
                            // polity hold at least one live subject? Read off
                            // ANOTHER polity's `overlord` field, never this
                            // one's -- a boolean-as-1000, same shape as
                            // `coastal_holdings`/`ground_port` above.
                            int subject_held_q = 0;
                            for (const polity& other : out.polities)
                                if (other.alive && other.overlord == q.id) { subject_held_q = 1000; break; }
                            // BL-942 -- TWO WAYS TO BE STRONG. `cs == nullptr`
                            // (a fixture isolating a different mechanism, same
                            // guard every other creed read in this file uses)
                            // reads as no lean either way, which is exactly
                            // `creed_lean_weight_q`'s own no-op case.
                            int cons_lean_q = 0, expn_lean_q = 0;
                            if (cs != nullptr && q.culture >= 0
                             && static_cast<std::size_t>(q.culture) < cs->cultures.size())
                            {
                                const culture& own = cs->cultures[static_cast<std::size_t>(q.culture)];
                                cons_lean_q = consolidator_lean_q(own);
                                expn_lean_q = expansion_lean_q(own);
                            }
                            q.exploration_investing = static_cast<int16_t>(choose_exploration_node(
                                q.exploration_mask, stores_low_q, reach_bound_q,
                                ground_port_q, ground_farm_q, surplus_q,
                                purse_low_q, wants_unmet_q, throughput_bound_q,
                                subject_held_q, cons_lean_q, expn_lean_q,
                                params.exploration_creed_lean_weight_q));
                            q.exploration_progress_q = 0;
                        }

                        if (q.exploration_investing >= 0)
                        {
                            // Same "research is a flow" shape as the empire
                            // tree's (TREES.md sec State): a fraction of the
                            // held ground's industry output, spent the same
                            // round it is earned.
                            int64_t industry_sum = 0;
                            for (int hi : held)
                                industry_sum += region_industry_output(
                                    ss.regions[static_cast<std::size_t>(hi)]);

                            const int64_t research_q =
                                (industry_sum * params.empire_research_fraction_q) / 1000
                                    * (1000 + clampi(tree_mod_q(q, io::tree_modifier_term::research), 0, 4000)) / 1000; // BL-973
                            q.exploration_progress_q += static_cast<int32_t>(
                                clampi64(research_q * step_years, 0, INT32_MAX));

                            const io::exploration_tree::node& tn =
                                io::exploration_tree::nodes[q.exploration_investing];
                            const int tn_kind_base =
                                tn.kind == io::exploration_tree::node_kind::minor ? 1
                              : tn.kind == io::exploration_tree::node_kind::major ? 3 : 6;
                            const int tn_cost =
                                params.capacity_band_cost * tn_kind_base * static_cast<int>(tn.ring);
                            if (q.exploration_progress_q >= tn_cost)
                            {
                                q.exploration_mask |= (1ULL << q.exploration_investing);
                                q.exploration_progress_q = 0;
                                q.exploration_investing  = -1;
                                apply_tree_effects(q); // BL-973
                            }
                        }
                    }

                    // ---- BL-1038: THE INDUSTRY TREE, behind its switch -----
                    //
                    // The third tree on the same Invest verb, same round, same
                    // held-ground readings. EVERY LIVING POLITY ENTERS AT THE
                    // ROOT from `industry_open_year` (TREES.md sec Milestones,
                    // the Industry exception): no rim gate, so a landlocked
                    // realm Exploration's coast-bound rim would exclude for good
                    // is only ever BEHIND here. With the switch off (every
                    // span but the Industrialisation span's own) this block is
                    // skipped whole and no Industry field is read or written.
                    //
                    // The readings below are the Industry scorer's ONLY: the
                    // empire and exploration scorers above keep their own
                    // gates (the empire's fuel mean, the exploration's open
                    // fuel gate) and their own rate (the industry slice).
                    if (params.industry_tree_enabled && y >= params.industry_open_year)
                    {
                        industry_scorer_reading ir;
                        ir.reach_bound_q    = reach_bound_q;
                        ir.manpower_bound_q = manpower_bound_q;
                        ir.food_bound_q     = food_bound_q;
                        ir.stores_low_q     = stores_low_q;
                        ir.cohesion_q       = q.cohesion_q;
                        ir.surplus_q        = surplus_q;
                        ir.ground_ore_q     = ground_ore_q;
                        ir.ground_farm_q    = ground_farm_q;
                        ir.ground_port_q    = ground_port_q;

                        // THE SEAM (INDUSTRY_TREE.md sec Aims): the richest
                        // held region's fuel -- the span-open survey under
                        // DEFAULT B below, energy_q with it off or where
                        // nothing was surveyed -- so a realm holding one
                        // coalfield passes the GATE however much else it
                        // holds. The seam is the gate's reading, the re-check's
                        // and `fuel_seen`'s; the `ground_fuel` TERM is not the
                        // seam inside the span (BL-1056, NR-896: a share, set
                        // at the pick below). And
                        // LABOUR (`labour_bound`): of the held
                        // non-subsistence surplus (`manpower_ceiling` — the
                        // budget muster and industry share, settlement.hpp sec
                        // Materials and labour), the share standing under arms,
                        // i.e. the hands the works cannot have. A realm with no
                        // surplus at all reads 0: nothing is being withheld.
                        // BL-1041 DEFAULT B (RULED, Ben 2026-09-18, wave 1 form):
                        // the seam reads each held region's FUEL READING -- the
                        // span-open survey where it has one, energy_q where it
                        // has none -- so the gate, its re-check and `fuel_seen`
                        // (all of which read this one field) agree with the
                        // points' fuel factor. The `ground_fuel` TERM is a
                        // share over the span-open survey (set at the pick).
                        // With nothing surveyed (every path but the span) the
                        // seam IS the energy_q max, and `ground_fuel` reads it.
                        const bool seam_reads_survey = params.industry_fuel_gate_reads_survey;
                        int64_t ceiling_sum = 0, under_arms = 0;
                        int foreign_held = 0;
                        for (int hi : held)
                        {
                            const region& hp = ss.regions[static_cast<std::size_t>(hi)];
                            ir.fuel_seam_q = std::max(ir.fuel_seam_q,
                                seam_reads_survey ? industry_fuel_reading_q(hp) : clampi(hp.energy_q, 0, 1000));
                            const int64_t ceiling = manpower_ceiling(hp.population, hp.work_manpower_mod);
                            ceiling_sum += ceiling;
                            under_arms  += std::min(std::max<int64_t>(hp.army_stock, 0), ceiling);
                            if (q.culture >= 0 && hp.culture.plurality() != q.culture) ++foreign_held;
                        }
                        ir.labour_bound_q = ceiling_sum > 0
                            ? static_cast<int>(clampi64((under_arms * 1000) / ceiling_sum, 0, 1000)) : 0;
                        // `many_peoples`: the share of held ground whose
                        // plurality people is not the realm's own — "held
                        // ground whose shares still charge the holder". A share,
                        // never a count, so it does not grow with the realm.
                        ir.many_peoples_q = clampi((foreign_held * 1000) / n_held, 0, 1000);
                        if (ir.fuel_seam_q >= industry_fuel_seam_bar_q) q.industry_fuel_seen = true;

                        // RE-PICK when nothing is targeted, when the target
                        // went dark under a fork, OR (Industry only) when its
                        // gate closed since the pick — a seam region lost to
                        // conquest or secession closes a `fuel` node that is
                        // mid-purchase, so "no seam, no Railway" holds on
                        // every round the target is funded, not only at the
                        // pick (INDUSTRY_TREE.md sec Aims). The gate reads the
                        // same `ir` means and seam filled above.
                        //
                        // ACCUMULATED PROGRESS IS LOST (reset to 0 below), the
                        // same thing that happens when a fork closes under the
                        // target. TREES.md sec State gives a polity ONE progress
                        // integer, for the node it is investing in: carrying it
                        // to the re-picked node would hand that node research
                        // earned toward a different one, and there is no
                        // per-node store to park it in until the seam returns.
                        // The loss is the visible price of losing the seam.
                        if (!industry_target_stands(q.industry_mask, q.industry_investing, ir))
                        {
                            // The readings only a re-pick needs, each a walk
                            // over the polity table or the held set.
                            //
                            // `threatened`: the heaviest grudge any LIVING
                            // polity holds against this one (grudge scale:
                            // ground taken 300, a seat sacked 700 — clamped at
                            // 1000). A neighbour's grudge is the visible cause.
                            // `known`: the Industry masks of every living
                            // polity this one has met (BL-908's contact table,
                            // `has_contact(from = q)`), per node in the scorer.
                            int threat = 0;
                            for (const polity& other : out.polities)
                            {
                                if (!other.alive || other.id == q.id) continue;
                                threat = std::max(threat, grudge_between(out, other.id, q.id));
                                if (other.industry_mask != 0 && has_contact(out, q.id, other.id))
                                    ir.known_mask |= other.industry_mask;
                            }
                            ir.threatened_q = clampi(threat, 0, 1000);

                            // THE FUEL DOCTRINE'S TWO PULLS (BL-1056; Ben
                            // 2026-09-19, NR-896): `ground_forest` and
                            // `ground_fuel` are each the per-mille SHARE of held
                            // regions whose span-open share clears its own
                            // resource's top-third bar (BL-1059, NR-899,
                            // NR-900), over
                            // the SAME held regions (those carrying the
                            // span-open survey) -- like with like, and neither grows with the realm,
                            // where the best-of-held they replace could only
                            // rise as it did. No gate reads either, so only a
                            // pick needs them. Off the Industrialisation span nothing
                            // was surveyed (`span_surveyed` false): forest reads
                            // 0 (the old pin) and fuel -1, which leaves the term
                            // on the seam. Inside it a realm holding no surveyed
                            // region reads 0 on both.
                            ir.ground_forest_q = industry_ground_forest_q(ss.regions, held, ground_bars);
                            ir.ground_fuel_q   = industry_ground_fuel_q(ss.regions, held, ground_bars, span_surveyed);

                            // `colonial_reach`: a held region the seat reaches
                            // only across water — `line_crosses_sea`, the SAME
                            // test that makes a campaign a sea leg.
                            for (int hi : held)
                            {
                                if (hi == q.capital) continue;
                                if (line_crosses_sea(seat, ss.regions[static_cast<std::size_t>(hi)],
                                                     terrain, gw, gh, params.neighbour_radius))
                                { ir.colonial_reach_q = 1000; break; }
                            }

                            // `fuel_bound`: the seat market's UNMET energy want
                            // (BL-939/954's signal, after inbound trade) — fuel
                            // demanded beyond what landed. 0 where the seat
                            // stands no market.
                            if (seat.has_market)
                                ir.fuel_bound_q = clampi(
                                    seat.scarcity_q[scarcity_good_index(region_class::energy)], 0, 1000);

                            // `credit_bound`: how far the seat's purse falls
                            // short of the one venture the sim prices in capital
                            // — a post road (`post_road_treasury_cost`), the
                            // rail head's own predecessor. 1000 with an empty
                            // purse, 0 once the treasury could pay for it
                            // outright. A placeholder scale, like `purse_low`'s.
                            if (params.post_road_treasury_cost > 0)
                                ir.credit_bound_q = static_cast<int>(clampi64(
                                    1000 - (std::max<int64_t>(0, seat.treasury) * 1000)
                                               / params.post_road_treasury_cost, 0, 1000));

                            q.industry_investing = static_cast<int16_t>(
                                choose_industry_node(q.industry_mask, ir));
                            q.industry_progress_q = 0;
                        }

                        if (q.industry_investing >= 0)
                        {
                            // THE RATE READS URBAN MASS (TREES.md sec State):
                            // the k largest held centres, clamped, then the
                            // integer superlinear transform — never the
                            // industry slice the other two trees earn from —
                            // scaled by the Industry spire ring held and the
                            // research modifier exactly as the empire rate is.
                            const int64_t research_q = industry_research_per_year_q(
                                industry_urban_mass(ss.regions, held),
                                industry_spire_ring(q.industry_mask),
                                tree_mod_q(q, io::tree_modifier_term::research), params);
                            q.industry_progress_q = static_cast<int32_t>(clampi64(
                                static_cast<int64_t>(q.industry_progress_q) + research_q * step_years,
                                0, INT32_MAX));

                            const io::industry_tree::node& tn =
                                io::industry_tree::nodes[q.industry_investing];
                            const int tn_kind_base =
                                tn.kind == io::industry_tree::node_kind::minor ? 1
                              : tn.kind == io::industry_tree::node_kind::major ? 3 : 6;
                            const int tn_cost =
                                params.capacity_band_cost * tn_kind_base * static_cast<int>(tn.ring);
                            if (q.industry_progress_q >= tn_cost)
                            {
                                q.industry_mask |= (1ULL << q.industry_investing);
                                q.industry_progress_q = 0;
                                q.industry_investing  = -1;
                                apply_tree_effects(q); // the surface follows the mask at once
                            }
                        }
                    }
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
                        ++out.works_by_band[static_cast<std::size_t>(b)];
                }
                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    bp.name + " raises a " + r->name, std::string{}});
                break;
            }
            case sim_verb::upgrade_supply:
            {
                if (best_target < 0 || best_target >= static_cast<int>(ss.regions.size())) break;
                const int payer = q.capital;
                bool bought = false;
                bool corridor_kind = false;
                if (best_supply_partner >= 0)
                {
                    corridor_kind = try_upgrade_corridor(best_supply_partner, best_target, payer);
                    bought = corridor_kind;
                }
                else
                {
                    bought = try_upgrade_region_reach(best_target, payer);
                }
                if (!bought) break; // the scorer's own read of the seat went stale — refused, not forced

                ++out.supply_sites_upgraded;
                if (corridor_kind) ++out.supply_sites_upgraded_corridors;
                else                ++out.supply_sites_upgraded_regions;
                // BL-916 lapse marker — the corridor case already gets one
                // from `road_promoted` inside `try_upgrade_corridor`; a
                // region buying its own relief needs its OWN event, or a
                // waystation purchase would draw nothing on the lapse at all.
                if (!corridor_kind)
                    note_event(lapse_event_kind::supply_site_upgraded, best_target, q.id, -1);

                const region& tp = ss.regions[static_cast<std::size_t>(best_target)];
                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    corridor_kind
                        ? (tp.name + " buys a road to widen its supply")
                        : (tp.name + " buys a waystation to widen its own reach"),
                    std::string{}});
                break;
            }
            case sim_verb::organise:
            {
                if (best_target < 0 || best_target >= static_cast<int>(ss.regions.size())) break;
                const std::size_t ti = static_cast<std::size_t>(best_target);
                if (owner[ti] >= 0) break; // Overtaken this round -- nothing left to organise.
                region& tgt = ss.regions[ti];
                const int tgt_culture = tgt.culture.plurality();
                if (tgt_culture < 0) break;

                const int op_q = (cs != nullptr)
                    ? culture_opposition_q(cs->cultures, q.culture, tgt_culture) : 0;
                if (op_q >= params.organise_opposition_bar_q) break; // Opposed -- conquest only.

                // PRICED BY KINSHIP, FROM THE CAPITAL'S OWN STOCKPILE
                // (CIVILISATION.md sec "What materials are FOR"): own people
                // cheapest, kin dearer, up to double the base cost at the
                // opposition bar itself. A REFUSAL DELAYS, IT DOES NOT
                // FORBID -- the same shape a refused corridor promotion
                // takes above: next round tries again with materials stood.
                region* seat = (q.capital >= 0
                             && static_cast<std::size_t>(q.capital) < ss.regions.size())
                             ? &ss.regions[static_cast<std::size_t>(q.capital)] : nullptr;
                const int64_t cost = (params.organise_material_cost * (1000 + op_q)) / 1000;
                if (seat == nullptr || seat->material_stock < cost) break;
                seat->material_stock -= cost;

                owner[ti]  = q.id;
                tgt.nation = q.id;
                void_stale_standing_army(tgt); // BL-955
                touch_owner(q.id); // BL-922: this polity's held-only reach is now stale.

                // A CENTRE ORGANISED ABOVE THE CITY-STATE THRESHOLD IS ITS
                // OWN SEAT within the same empire; below it, ordinary
                // hinterland pointing at the capital that organised it
                // (CIVILISATION.md sec "A city state spawns where a
                // region's population is above a threshold").
                if (tgt.population >= params.city_state_population_threshold)
                {
                    tgt.is_seat     = true;
                    tgt.seat_region = static_cast<int>(ti);
                }
                else
                {
                    tgt.is_seat     = false;
                    tgt.seat_region = q.capital;
                }

                out.owner_changes.push_back(owner_change{
                    static_cast<int32_t>(y), static_cast<uint16_t>(ti),
                    static_cast<uint16_t>(q.id)});
                ++out.organised;
                out.history.push_back(history_event{
                    years_from_calendar_year(y), chain_stage::legacy,
                    tgt.name + " is organised", std::string{}});
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
                // BL-973: held `cohesion` nodes scale the recovery rate
                // (TREES.md sec Effects: "recovery on Consolidate").
                q.cohesion_q = clampi(q.cohesion_q
                                    + params.cohesion_recovery_q * step_years
                                        * (1000 + clampi(tree_mod_q(q, io::tree_modifier_term::cohesion), 0, 4000)) / 1000,
                                      params.cohesion_floor_q, 1000);
                break;
            }

            // ---- THE FURNACE (BL-748) ------------------------------------
            //
            // Stage 4's date used to be `run_settlement`'s, fixed before this
            // loop started. That is backwards — the run is where
            // industrialisation happens — so the date is now the year a
            // polity's materials capacity crosses the Industrial rung INSIDE
            // the run, plus the lag its ground imposes.
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
            // could not build at. No generated region carries a lag at all
            // (the settlement pass stops at 0 CE, below Stage 4's 1700), so the
            // generated world lights nothing here, exactly as before BL-1075.
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
                        // BL-1080: the crossing, on the record the round
                        // plays. Event layer only -- read by nothing here.
                        note_event(lapse_event_kind::furnace_lit, hi, q.id, -1);
                    }
                }
            }
        }

        // ---- BL-896/BL-923 - GROUND THE REALM CANNOT REACH SECEDES --------
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
        // BL-923, SAME DAY: THE UNIT IS THE CITY STATE, NOT A CONTIGUOUS BLOCK
        // OF TWO OR MORE. Each seat the capital can no longer supply becomes
        // its own polity, taking the hinterland that already points at it
        // (`region::seat_region`) whether or not every one of those hinterland
        // regions independently reads as cut off -- a hinterland shares its
        // seat's fate, it is not measured seat-by-seat. A cut-off region that
        // is not itself a seat, and whose OWN seat did not also fall (so it is
        // not already carried along above), joins the nearest cut-off seat by
        // `region_distance`; where this realm has no cut-off seat at all this
        // round, such a region stands alone as a city state of its own -- it
        // is never handed to a neighbour and never released to nobody. This
        // REPLACES BL-896's block-of-two-or-more policy (NR-826 call 2,
        // reversed by NR-837); the seat's plurality culture rule (NR-826 call
        // 3) is unchanged.
        //
        // AFTER THE DECISION LOOP, NEVER INSIDE IT. A successor is a
        // `push_back` onto `out.polities`, which invalidates the `polity&` that
        // loop holds. Everything below therefore indexes rather than
        // references.
        //
        // DETERMINISM IS THE REAL CONSTRAINT HERE and every choice below is
        // made for it: polities are walked in id order, regions in index
        // order, cut-off seats are walked in index order, leftover cut-off
        // ground attaches by index order too, and successors are allocated in
        // the order those blocks are found. Nothing below reads a container
        // whose order is undefined.
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
                if (cut.empty()) continue;

                // CUT-OFF SEATS, INDEX ORDER. Each becomes its own city state,
                // carrying the whole hinterland that already points at it.
                std::vector<int> cut_seats;
                for (int r : cut)
                    if (ss.regions[static_cast<std::size_t>(r)].is_seat)
                        cut_seats.push_back(r);

                // BLOCK PER CUT-OFF SEAT: every region this realm still holds
                // whose `seat_region` already points at that seat, cut off or
                // not -- the hinterland leaves with its seat, not piecemeal.
                std::vector<std::vector<int>> blocks(cut_seats.size());
                std::vector<char> covered(cut.size(), 0); // which `cut` entries a block already claimed
                for (std::size_t si = 0; si < cut_seats.size(); ++si)
                {
                    const int seat = cut_seats[si];
                    for (std::size_t i = 0; i < owner.size(); ++i)
                    {
                        if (owner[i] != qid) continue;
                        if (ss.regions[i].seat_region != seat) continue;
                        blocks[si].push_back(static_cast<int>(i));
                    }
                    for (std::size_t ci = 0; ci < cut.size(); ++ci)
                        if (ss.regions[static_cast<std::size_t>(cut[ci])].seat_region == seat)
                            covered[ci] = 1;
                }

                // LEFTOVER CUT-OFF GROUND: no seat of its own, and its actual
                // seat was not itself cut off this round. It joins the nearest
                // cut-off seat's block; with no cut-off seat in this realm at
                // all, it becomes a singleton city state (block of one, self-
                // seated) in cut order, which is index order.
                std::vector<int> singleton_seats; // seats with no cut-off seat to join
                for (std::size_t ci = 0; ci < cut.size(); ++ci)
                {
                    if (covered[ci]) continue;
                    const int r = cut[ci];
                    if (cut_seats.empty())
                    {
                        singleton_seats.push_back(r);
                        continue;
                    }
                    int nearest = -1, nearest_d = 1 << 30;
                    for (int seat : cut_seats)
                    {
                        const int d = region_distance(
                            ss.regions[static_cast<std::size_t>(r)],
                            ss.regions[static_cast<std::size_t>(seat)], gw);
                        if (d < nearest_d) { nearest_d = d; nearest = seat; }
                    }
                    for (std::size_t si = 0; si < cut_seats.size(); ++si)
                        if (cut_seats[si] == nearest) { blocks[si].push_back(r); break; }
                }

                // ONE POLITY PER SEAT-BLOCK, THEN ONE PER SINGLETON, both in
                // the deterministic orders built above.
                std::vector<std::pair<int, std::vector<int>>> pieces; // (seat, block)
                for (std::size_t si = 0; si < cut_seats.size(); ++si)
                {
                    std::sort(blocks[si].begin(), blocks[si].end());
                    pieces.push_back({cut_seats[si], blocks[si]});
                }
                for (int r : singleton_seats)
                    pieces.push_back({r, std::vector<int>{r}});

                bool any_piece_left = false; // BL-926: one breakdown per parent per round.
                for (auto& piece : pieces)
                {
                    const int seat = piece.first;
                    std::vector<int>& block = piece.second;
                    if (block.empty()) continue;
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
                    np.parent          = qid; // BL-926/BL-916: the lineage hook, written here only.
                    coin_name_for(np); // BL-1088
                    out.polities.push_back(np);
                    note_event(lapse_event_kind::broke_away, seat, np.id, qid); // BL-916

                    // BL-922 -- REPORT-ONLY: was this block cut off by the
                    // MAP or by the GRAPH? A seceding region with a FED region
                    // of its own realm standing within `neighbour_radius` was
                    // not out of reach geographically; the degree-capped
                    // neighbour index (BL-855) simply carried no edge to it.
                    // Counted before ownership moves, read by history_sweep.
                    // Touches no decision.
                    for (int r : block)
                    {
                        const region& br = ss.regions[static_cast<std::size_t>(r)];
                        bool fed_near = false;
                        for (std::size_t j = 0; j < owner.size() && !fed_near; ++j)
                        {
                            if (owner[j] != qid || ss.regions[j].network_supply_q <= 0) continue;
                            if (region_distance(br, ss.regions[j], gw) <= params.neighbour_radius)
                                fed_near = true;
                        }
                        if (fed_near) ++out.regions_seceded_graph_cut;
                    }

                    for (int r : block)
                    {
                        const std::size_t ri = static_cast<std::size_t>(r);
                        owner[ri]                  = np.id;
                        ss.regions[ri].nation      = np.id;
                        void_stale_standing_army(ss.regions[ri]); // BL-955
                        touch_owner(out.polities[pi].id); // BL-922: the parent lost ground
                        touch_owner(np.id);              // and the successor gained it
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
                    out.secession_piece_sizes.push_back(static_cast<int32_t>(block.size())); // BL-926
                    if (!any_piece_left) { any_piece_left = true; ++out.breakdowns; }   // BL-926
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
            // way BL-895 FIRST paid for it: a walked corridor between two held
            // regions that hold UNLIKE ground. (BL-1021 moved the income to
            // kinds of ground reached; this reads CONTACT, so it still counts
            // corridors.) Recomputed on demand rather than
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
                note_event(lapse_event_kind::creed_preached,
                           out.polities[pi].capital, out.polities[pi].id, ci); // BL-916

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

            // ---- (d) THE SCHISM -- A FAULT LINE, NOT A NETWORK FAILURE ----
            //
            // BL-944. CREEDS.md's fault line, made mechanical: an institution
            // the realm adopted (`polity::universal_creed`) standing over
            // ground whose people ANSWERED WITH THEIR OWN PANTHEON instead
            // (`region::creed_hold == 3`, REASSERTED) and who are not the
            // realm's own kin (`creed_residue_culture != culture`). Enough
            // such ground of the SAME residue culture is what it takes for
            // that people to walk away as its own polity.
            //
            // GROUPED BY CULTURE, NOT BY SEAT OR REACH -- the one choice that
            // keeps this event from ever blending into BL-896/BL-923's
            // network-failure secession above. That block groups a realm's
            // CUT-OFF ground by its seat, a geographic, reach-driven test;
            // this one groups a realm's REASSERTED ground by its RESIDUE
            // CULTURE regardless of where it sits on the map, because a
            // schism is a people answering together, not a hinterland losing
            // its road. It is counted into `out.schisms`/`regions_sundered`
            // and raises a `grudge_kind::faith_sundered` grudge -- never the
            // `secessions`/`ground_taken` pair BL-896 uses -- so the two
            // causes can never be read as one in `history_sweep`.
            //
            // AFTER THE LOOP, NEVER INSIDE IT, for the same reason BL-896
            // gives above: a schism is a `push_back` onto `out.polities`,
            // which would invalidate a `polity&` held across it. Everything
            // below indexes rather than references.
            //
            // DETERMINISM: polities walked in id order, a realm's reasserted-
            // and-alien ground collected in region-index order, then sorted
            // by (residue culture, region index) so every culture's group is
            // both contiguous and internally ordered with no container whose
            // order is undefined; a group's seat is its LOWEST region index.
            if (params.schism_min_regions > 0)
            {
                const std::size_t pol_count_schism = out.polities.size();
                for (std::size_t pi = 0; pi < pol_count_schism; ++pi)
                {
                    if (!out.polities[pi].alive) continue;
                    if (out.polities[pi].universal_creed < 0) continue;
                    const int qid          = out.polities[pi].id;
                    const int qcap         = out.polities[pi].capital;
                    const int own_culture  = out.polities[pi].culture;

                    // The reasserted, alien ground this realm still holds,
                    // as (residue culture, region index) pairs -- sorting
                    // this list is what turns it into contiguous groups.
                    std::vector<std::pair<int, int>> alien_reasserted;
                    for (std::size_t i = 0; i < n_reg_creed; ++i)
                    {
                        if (owner[i] != qid) continue;
                        if (static_cast<int>(i) == qcap) continue; // a seat cannot secede from itself
                        const region& rg2 = ss.regions[i];
                        if (rg2.creed_hold != 3) continue;
                        const int rc = rg2.creed_residue_culture;
                        if (rc < 0 || rc == own_culture) continue;
                        alien_reasserted.push_back({rc, static_cast<int>(i)});
                    }
                    if (alien_reasserted.empty()) continue;
                    std::sort(alien_reasserted.begin(), alien_reasserted.end());

                    for (std::size_t k = 0; k < alien_reasserted.size(); )
                    {
                        std::size_t j = k;
                        while (j < alien_reasserted.size()
                            && alien_reasserted[j].first == alien_reasserted[k].first)
                            ++j;

                        const int group_culture = alien_reasserted[k].first;
                        std::vector<int> block;
                        for (std::size_t m = k; m < j; ++m)
                            block.push_back(alien_reasserted[m].second); // already index-ascending
                        k = j;

                        if (static_cast<int>(block.size()) < params.schism_min_regions) continue;
                        if (out.polities.size() >= owner_index_limit) break;

                        const int this_culture = group_culture;
                        const int seat          = block.front(); // already the lowest index

                        polity np;
                        np.id      = static_cast<int>(out.polities.size());
                        np.culture = this_culture;
                        np.capital = seat;
                        np.aggression_q =
                            (cs && this_culture >= 0
                             && this_culture < static_cast<int>(cs->cultures.size()))
                                ? leaned_aggression_q(
                                      params, cs->cultures[static_cast<std::size_t>(this_culture)].aggression_q)
                                : 500; // BL-839: same lean the founding/secession reads above use.
                        // INSTITUTIONS AND STANDING ARE INHERITED, exactly as
                        // BL-896's secession inherits them above -- a people
                        // that walks away over faith knows what its former
                        // realm knew, and this invents no new dial for it.
                        for (int d = 0; d < sim_domain_count; ++d)
                        {
                            np.capacity[d]   = out.polities[pi].capacity[d];
                            np.progress_q[d] = out.polities[pi].progress_q[d];
                        }
                        np.cohesion_q      = out.polities[pi].cohesion_q;
                        np.industrial_year = out.polities[pi].industrial_year;
                        np.parent          = qid; // the lineage hook, same field BL-896/926 write.
                        // `np.universal_creed` stays -1 (the struct default):
                        // the whole point of a schism is a people that left
                        // the institution its former realm still holds.
                        coin_name_for(np); // BL-1088
                        out.polities.push_back(np);
                        note_event(lapse_event_kind::schism, seat, np.id, qid);

                        // BL-981 -- A SCHISM MOVES SEATS, IT DOES NOT RAZE
                        // THEM. The block is grouped by residue culture, so
                        // it can carry a region that is already a seat of the
                        // parent realm (organised above the city-state
                        // threshold, or a seat taken in an earlier war). The
                        // first cut wrote `is_seat = (r == seat)` over every
                        // block member, demoting any such seat while ground
                        // OUTSIDE the block -- the parent's own, non-reasserted
                        // hinterland -- kept pointing at it: one dangling
                        // `seat_region` per demoted seat (colonisation_harness
                        // D1, seed 2, one region). Three rules now hold the
                        // pointer invariant (CIVILISATION.md sec The unit is
                        // the city state) through the event:
                        //   1. `seat` becomes the new polity's capital seat;
                        //      every other seat in the block STAYS a seat --
                        //      a realm born of a schism holds as many seats
                        //      as walked out with it, exactly as an empire
                        //      holds every seat it organised.
                        //   2. A non-seat block member keeps its pointer when
                        //      its seat came along in the block; otherwise
                        //      its seat stayed with the parent, so it
                        //      re-points at `seat` -- the same "re-pointed at
                        //      the conqueror's seat" rule the campaign verb
                        //      applies to ground taken on its own.
                        //   3. Parent-held ground outside the block whose seat
                        //      just left re-points at the parent's capital --
                        //      the "surviving hinterland follows its realm's
                        //      seat" rule the capital-fell block applies.
                        //      Unorganised ground (`nation < 0`, BL-920's
                        //      pure distance pointer) is untouched: the seat
                        //      it points at is still a seat.
                        // Block members are walked index-ascending and `seat`
                        // is the lowest index, so its flag is set before any
                        // member reads it.
                        std::vector<char> in_block(ss.regions.size(), 0);
                        for (int r : block) in_block[static_cast<std::size_t>(r)] = 1;

                        for (int r : block)
                        {
                            const std::size_t ri = static_cast<std::size_t>(r);
                            owner[ri]             = np.id;
                            ss.regions[ri].nation = np.id;
                            void_stale_standing_army(ss.regions[ri]); // BL-955
                            touch_owner(qid);
                            touch_owner(np.id);
                            if (r == seat)
                            {
                                ss.regions[ri].is_seat     = true;
                                ss.regions[ri].seat_region = seat;
                            }
                            else if (!ss.regions[ri].is_seat)
                            {
                                const int sr = ss.regions[ri].seat_region;
                                const bool seat_came_along =
                                    sr >= 0 && static_cast<std::size_t>(sr) < ss.regions.size()
                                    && in_block[static_cast<std::size_t>(sr)]
                                    && ss.regions[static_cast<std::size_t>(sr)].is_seat;
                                if (!seat_came_along) ss.regions[ri].seat_region = seat;
                            }
                            // else: an existing seat walks out as a seat --
                            // `is_seat` and its self-pointer are already right.
                            out.owner_changes.push_back(owner_change{
                                static_cast<int32_t>(y),
                                static_cast<uint16_t>(r),
                                static_cast<uint16_t>(np.id)});
                        }

                        // Rule 3: the parent's orphaned hinterland.
                        for (std::size_t hi = 0; hi < ss.regions.size(); ++hi)
                        {
                            if (in_block[hi]) continue;
                            region& h = ss.regions[hi];
                            if (h.nation != qid) continue;
                            const int sr = h.seat_region;
                            if (sr < 0 || static_cast<std::size_t>(sr) >= ss.regions.size()) continue;
                            if (!in_block[static_cast<std::size_t>(sr)]) continue;
                            h.seat_region = (qcap >= 0) ? qcap : -1;
                        }

                        // A SCHISM IS A DEFEAT FOR THE PARENT TOO, through the
                        // same cohesion channel every other loss uses.
                        out.polities[pi].cohesion_q = clampi(
                            out.polities[pi].cohesion_q - params.cohesion_loss_on_defeat_q,
                            params.cohesion_floor_q, 1000);

                        // THE DISTINCT GRUDGE: `faith_sundered`, never
                        // `ground_taken` -- the readable cause is that a
                        // people answered its own pantheon, not that a road
                        // failed to reach them.
                        raise_grudge(qid, np.id, grudge_kind::faith_sundered,
                                     seat, y, params.grudge_ground_taken);

                        ++out.schisms;
                        out.regions_sundered += static_cast<int64_t>(block.size());
                        out.history.push_back(history_event{
                            years_from_calendar_year(y), chain_stage::legacy,
                            ss.regions[static_cast<std::size_t>(seat)].name
                                + " breaks away, its own pantheon reasserted against the creed",
                            std::string{}});
                    }
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

    // BL-955: the paid standing army's raw invariant, once more at the close.
    if (params.exploration_upkeep_enabled)
        for (const region& r : ss.regions)
            if (!standing_army_invariant_holds(r)) ++out.standing_army_invariant_violations;

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
        std::vector<history_corridor> fresh;
        fresh.reserve(corridor_uses.size());
        for (const auto& e : corridor_uses)
        {
            if (!fresh.empty() && fresh.back().a == e.first && fresh.back().b == e.second)
            {
                ++fresh.back().uses;
                continue;
            }
            fresh.push_back(history_corridor{e.first, e.second, 1});
        }

        // BL-956: A RESUMED RUN ALREADY HOLDS A RECORD (`resume_corridors`),
        // and appending this span's fold after it broke the (a, b) order and
        // split one edge's traffic across two rows — the Exploration handoff's
        // validator caught it. Merge instead: both lists are sorted by (a, b),
        // so one linear merge keeps the order and sums `uses` on a shared
        // edge. A non-resumed run starts empty, so this is the plain fold.
        if (out.supply_corridors.empty())
        {
            out.supply_corridors = std::move(fresh);
        }
        else
        {
            std::vector<history_corridor> inherited = std::move(out.supply_corridors);
            std::sort(inherited.begin(), inherited.end(),
                      [](const history_corridor& x, const history_corridor& y) {
                          return x.a != y.a ? x.a < y.a : x.b < y.b;
                      });
            out.supply_corridors.clear();
            out.supply_corridors.reserve(inherited.size() + fresh.size());
            const auto push = [&](const history_corridor& c) {
                if (!out.supply_corridors.empty()
                    && out.supply_corridors.back().a == c.a
                    && out.supply_corridors.back().b == c.b)
                    out.supply_corridors.back().uses += c.uses;
                else
                    out.supply_corridors.push_back(c);
            };
            std::size_t i = 0, j = 0;
            while (i < inherited.size() || j < fresh.size())
            {
                const bool take_inherited =
                    j >= fresh.size()
                    || (i < inherited.size()
                        && (inherited[i].a != fresh[j].a ? inherited[i].a < fresh[j].a
                                                         : inherited[i].b <= fresh[j].b));
                push(take_inherited ? inherited[i++] : fresh[j++]);
            }
        }

        // BL-949 (a) -- THE RUNG TRAVELS WITH THE RECORD. Every row takes the
        // tier its LIVE count stands at now, which is the tier `rebuild_reach`
        // last read -- a bought post road reads 3 here though it added one walk
        // to `uses`, and an inherited Road the span never touched reads the
        // rung its seeded count gives it. A row whose edge has no live entry
        // (a record out of the owner index range) keeps tier 0.
        for (history_corridor& c : out.supply_corridors)
            c.tier = static_cast<uint8_t>(road_tier_between(c.a, c.b));
    }

    // --- The sea-leg record, folded (BL-1097) -----------------------------
    //
    // The road fold above, restated for water: sort the raw (lo, hi) list,
    // run-length encode it, and merge it onto whatever `resume_sea_legs`
    // handed in -- both sorted by (a, b), one linear merge summing `uses` on
    // a shared leg. No tier travels with the row: a lane is a pure function
    // of `uses` against `sea_lane_tier1_uses`, earned by traffic and never
    // bought, so there is no second count for the record to disagree with.
    {
        std::sort(sea_leg_uses.begin(), sea_leg_uses.end());
        std::vector<sea_leg> fresh;
        fresh.reserve(sea_leg_uses.size());
        for (const auto& e : sea_leg_uses)
        {
            if (!fresh.empty() && fresh.back().a == e.first && fresh.back().b == e.second)
            {
                ++fresh.back().uses;
                continue;
            }
            fresh.push_back(sea_leg{e.first, e.second, 1});
        }
        if (out.sea_legs.empty())
        {
            out.sea_legs = std::move(fresh);
        }
        else
        {
            std::vector<sea_leg> inherited = std::move(out.sea_legs);
            std::sort(inherited.begin(), inherited.end(),
                      [](const sea_leg& x, const sea_leg& y2) {
                          return x.a != y2.a ? x.a < y2.a : x.b < y2.b;
                      });
            out.sea_legs.clear();
            out.sea_legs.reserve(inherited.size() + fresh.size());
            const auto push = [&](const sea_leg& l) {
                if (!out.sea_legs.empty()
                    && out.sea_legs.back().a == l.a && out.sea_legs.back().b == l.b)
                    out.sea_legs.back().uses += l.uses;
                else
                    out.sea_legs.push_back(l);
            };
            std::size_t i = 0, j = 0;
            while (i < inherited.size() || j < fresh.size())
            {
                const bool take_inherited =
                    j >= fresh.size()
                    || (i < inherited.size()
                        && (inherited[i].a != fresh[j].a ? inherited[i].a < fresh[j].a
                                                         : inherited[i].b <= fresh[j].b));
                push(take_inherited ? inherited[i++] : fresh[j++]);
            }
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

    // --- BL-910: CAPITALS AND MARKETS STAND AT THE CLOSE --------------------
    // CIVILISATION.md sec Capitals exist at the close. NOT A NEW PLACEMENT
    // PASS: every living polity already tracks its capital as a region index
    // (`polity::capital`), kept pointed at whichever of its regions `is_seat`
    // as ownership moves (see the "capital fell" block above). This is a pure
    // READ of that fact, at the one moment pass 1 hands its output forward —
    // it marks the region a market stands on, never invents a capital.
    //
    // NO QUOTA. A polity with no living holdings never reaches this loop
    // (`q.alive` is false), so it and every region it once held carry no
    // market — the same permission a city state gets, not a floor.
    for (const polity& q : out.polities)
    {
        if (!q.alive) continue;
        if (q.capital < 0 || q.capital >= static_cast<int>(ss.regions.size()))
            continue; // Defensive: an alive polity always names a capital above.
        region& cap = ss.regions[static_cast<std::size_t>(q.capital)];
        if (!cap.is_seat) continue; // Defensive; the capital IS the seat by construction.
        cap.has_market = true;
    }

    // --- THE TARIFF POSTURE (BL-750; the derivation is BL-1102) -----------
    //
    // A DERIVED OUTPUT READ AT HANDOFF, NEVER A SCORED VERB (Ben, 2026-09-06;
    // NATIONS.md sec 4 Tariffs). Nothing in the decision loop above reads
    // `protection_q`, and nothing here can move the run: this block executes
    // once, after the last round, and writes a field no verb, no score and no
    // RNG draw ever touches. A run with this block deleted would take every
    // decision it takes with it.
    //
    // THE DERIVATION (Ben, 2026-09-24, sprint 47 ruling R18; INDUSTRIALISATION.md
    // sec The boundary and sec What crosses into play). The close derives each
    // living polity's posture from the three inputs that already cross -- the
    // scarcity signal, the trade flows and the cultural preference -- as they
    // stand at the close, and from nothing else. Per living polity P with a
    // capital market, over the four goods g (farm, ore, energy, port):
    //
    //   short_g = polity_good_want_q(P, g)
    //             the capital market's UNMET `scarcity_q[g]`, weighted by its
    //             people's preference: scarcity * (500 + weight_q / 2) / 1000
    //   in_g    = sum of `volume_q` over `trade_flows` with buyer  == P, good == g
    //   out_g   = sum of `volume_q` over `trade_flows` with seller == P, good == g
    //
    //   protection_q = clamp( sum_g (short_g + in_g - out_g) / 4, 0, 1000 )
    //
    // Read it as: what its people run short of (sharpened by what they
    // prefer), plus what it takes in, less what it sends out, averaged over
    // the four goods. A polity that imports what it wants arrives protective;
    // one that sells arrives open; one short of nothing and trading nothing
    // arrives at 0. Every term is on the signal's own 0-1000 scale --
    // `short_g + in_g` never exceeds the raw want (the upkeep floors the unmet
    // signal at raw less inbound, and the flow spend-down caps inbound at raw)
    // and `out_g` never exceeds a holding -- so the average is 0-1000 by
    // construction and the clamp only ever lifts a net exporter's negative to 0.
    //
    // INDEPENDENT OF `boundary_year` (the two-span arc's field, which no span
    // sets). Gated instead on `exploration_upkeep_enabled`, the switch under
    // which `refresh_market_scarcity` and `compute_trade_flows` run at all: the
    // Empires span carries neither signal nor flow, so it writes 0 there
    // exactly as the arithmetic would, and stays byte-identical.
    // `derive_national_protection` -> `seed_national_tariffs` (law.cpp) then
    // bands the broadcast value into an `import_tariff` law; the Era -1 sim
    // carries no other tariff derivation.
    //
    // Integer throughout, walked in polity-id order over vectors (the flow
    // table is sorted (seller, buyer, good); the preference table ascending
    // (culture, good)), so it is byte-identical from a seed like everything
    // else in this file.
    {
        if (params.exploration_upkeep_enabled)
        {
            // The preference AS IT STANDS AT THE CLOSE -- re-derived over the
            // final ground rather than read off `round_prefs`, which the last
            // round derived before its own conquests moved ownership. The same
            // fold, on the same inputs, that `make_industrialisation_output`
            // hands forward as `culture_preference`.
            const std::vector<culture_good_preference> close_prefs =
                derive_culture_preference(ss.regions, out.contacts, out.polities, want_culture_count);

            constexpr int good_count = 4;
            const region_class goods[good_count] =
                { region_class::farm, region_class::ore, region_class::energy, region_class::port };

            // in_g / out_g per polity, from the sim's own flow table -- the one
            // the last decision round rebuilt (`out.trade_flows`).
            std::vector<std::array<int64_t, good_count>> in_q(out.polities.size());
            std::vector<std::array<int64_t, good_count>> out_q(out.polities.size());
            for (auto& row : in_q)  row.fill(0);
            for (auto& row : out_q) row.fill(0);
            for (const trade_flow& f : out.trade_flows)
            {
                if (f.good >= good_count || f.volume_q <= 0) continue;
                if (f.buyer  < out.polities.size()) in_q[f.buyer][f.good]   += f.volume_q;
                if (f.seller < out.polities.size()) out_q[f.seller][f.good] += f.volume_q;
            }

            for (std::size_t pi = 0; pi < out.polities.size(); ++pi)
            {
                polity& q = out.polities[pi];
                q.protection_q = 0; // this close's value, never a resumed one's
                if (!q.alive) continue;
                int64_t net = 0;
                for (int g = 0; g < good_count; ++g)
                {
                    const std::size_t gi = static_cast<std::size_t>(g);
                    const int short_q = polity_good_want_q(ss.regions, out.polities, close_prefs,
                                                           static_cast<int>(pi), goods[gi]);
                    net += static_cast<int64_t>(short_q) + in_q[pi][gi] - out_q[pi][gi];
                }
                q.protection_q = static_cast<int>(clampi64(net / good_count, 0, 1000));
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

    // BL-914: the final flush. The per-year publish above always lags by one
    // year, so the last year simulated is never folded in inside the loop —
    // this is the one place that year's record reaches the tap.
    if (tap != nullptr)
    {
        publish_polity_names(); // BL-1088
        tap->publish(out.owner_changes, out.culture_changes, out.events,
                    static_cast<int32_t>(params.stop_year - 1));
        // BL-1106: the closing name table, same as the per-year publish.
        std::vector<std::string> tap_civ, tap_creed;
        std::vector<int32_t>     tap_polity_creed;
        fill_lapse_name_table(out, tap_civ, tap_creed, tap_polity_creed);
        tap->publish_names(tap_civ, tap_creed, tap_polity_creed);
    }

    return out;
}

// ---------------------------------------------------------------------------
// The tree effect surface (BL-973) — one fold for every tree
// ---------------------------------------------------------------------------
//
// The generated tables (`io::empire_tree`, `io::exploration_tree`, and since
// BL-1038 `io::industry_tree`) have distinct `node` types with one layout, so the walkers are templates over
// the node type rather than a per-tree twin — the same reason the generator
// itself was generalised (BL-930). Walk order is the table's fixed authored
// order; nothing here depends on anything transient.

namespace {

/// Fold one tree's held effects into the polity's surface.
template <typename NodeT, int N, int E>
void fold_tree_effects(uint64_t mask, const NodeT (&nodes)[N],
                       const io::tree_effect (&effects)[E], polity& q)
{
    for (int i = 0; i < N; ++i)
    {
        if (!(mask & (1ULL << i))) continue;
        const NodeT& n = nodes[i];
        for (int r = 0; r < static_cast<int>(n.effects_n); ++r)
        {
            const io::tree_effect& e = effects[n.effects_begin + r];
            if (e.kind == io::tree_effect_kind::modifier
             && e.term != io::tree_modifier_term::none)
                q.tree_mod_q[static_cast<int>(e.term)] += e.per_mille;
            if (e.key != io::tree_effect_key::none)
                q.tree_keys |= (1u << static_cast<unsigned>(e.key));
        }
    }
}

/// Rule 1's ring lock, read off the store: is there a held node carrying
/// `open "ring <ring>"`?
template <typename NodeT, int N, int E>
bool tree_ring_open(uint64_t mask, int ring, const NodeT (&nodes)[N],
                    const io::tree_effect (&effects)[E])
{
    for (int i = 0; i < N; ++i)
    {
        if (!(mask & (1ULL << i))) continue;
        const NodeT& n = nodes[i];
        for (int r = 0; r < static_cast<int>(n.effects_n); ++r)
        {
            const io::tree_effect& e = effects[n.effects_begin + r];
            if (e.kind == io::tree_effect_kind::open && static_cast<int>(e.open_ring) == ring)
                return true;
        }
    }
    return false;
}

/// A term read for a region's OWNER. Every creation site assigns
/// `id = out.polities.size()`, so the id is the index; that is checked
/// rather than assumed, with the id walk (`dq`'s idiom) as the fallback.
/// 0 for unowned ground or an unknown id. Called per candidate target in
/// campaign pricing, hence O(1) on the common path.
int nation_tree_mod_q(const std::vector<polity>& ps, int nation, io::tree_modifier_term t)
{
    if (nation < 0) return 0;
    const std::size_t ni = static_cast<std::size_t>(nation);
    if (ni < ps.size() && ps[ni].id == nation) return tree_mod_q(ps[ni], t);
    for (const polity& o : ps)
        if (o.id == nation) return tree_mod_q(o, t);
    return 0;
}

} // namespace

void apply_tree_effects(polity& q)
{
    for (int i = 0; i < io::tree_modifier_term_count; ++i) q.tree_mod_q[i] = 0;
    q.tree_keys = 0;
    fold_tree_effects(q.empire_mask,      io::empire_tree::nodes,      io::empire_tree::effects,      q);
    fold_tree_effects(q.exploration_mask, io::exploration_tree::nodes, io::exploration_tree::effects, q);
    // BL-1038: the third tree. `industry_mask` is written only behind
    // `history_sim_params::industry_tree_enabled`, so on every shipped path
    // this walks a zero mask and sums nothing into the shared surface.
    fold_tree_effects(q.industry_mask,    io::industry_tree::nodes,    io::industry_tree::effects,    q);
}

tree_effect_reader tree_effect_reader_of(const io::tree_effect& e)
{
    using K = io::tree_effect_kind;
    using T = io::tree_modifier_term;
    using Y = io::tree_effect_key;

    // Identity reads first: a keyed effect is read by its key whatever its
    // kind (the key is the contract; the kind is the doc's classification).
    switch (e.key)
    {
        case Y::sea_legs:   return tree_effect_reader::sea_legs_gate;
        case Y::post_roads: return tree_effect_reader::post_roads_gate;
        case Y::none: break;
    }
    if (e.kind == K::open)
    {
        if (e.open_tree)      return tree_effect_reader::tree_gate;
        if (e.open_ring > 0)  return tree_effect_reader::ring_gate;
        return tree_effect_reader::unread;
    }
    if (e.kind == K::modifier)
    {
        switch (e.term)
        {
            // `reach` is folded but unread — see the holdings-supply site in
            // run_history_sim for the BL-872 finding that keeps it so.
            case T::defence:    return tree_effect_reader::modifier_defence;
            case T::industrial: return tree_effect_reader::modifier_industrial;
            case T::cohesion:   return tree_effect_reader::modifier_cohesion;
            case T::research:   return tree_effect_reader::modifier_research;
            default:            return tree_effect_reader::unread;
        }
    }
    return tree_effect_reader::unread;
}

bool tree_effect_declared_unread(const io::tree_effect& e)
{
    using K = io::tree_effect_kind;
    using T = io::tree_modifier_term;
    if (e.key != io::tree_effect_key::none) return false; // keyed effects are read by key
    switch (e.kind)
    {
        case K::open:
            return false; // every open is a gate
        case K::modifier:
            switch (e.term)
            {
                // Consumers read region fields (`work_capacity_mod`,
                // `work_manpower_mod`) inside settlement.cpp with no polity
                // in scope, or the sim carries no such term at all (stores
                // decay, plague resistance, forage, a labour split). `reach`
                // HAS a surface (the hub's `work_reach_mod`) and is withheld
                // on a measured finding — the BL-872 fixtures fail at the
                // authored magnitudes; see the holdings-supply site.
                case T::reach:
                case T::carrying_capacity: case T::manpower: case T::stores:
                case T::assimilation:      case T::plague:   case T::forage:
                case T::muster_cost:
                    return true;
                default:
                    return false;
            }
        // Works and unit rows still gate on the derived band (TREES.md's open
        // question); the rest are prose the sim has no term for.
        case K::unlock: case K::upgrade: case K::retire: case K::access: case K::reach:
        case K::intel:  case K::institution: case K::doctrine: case K::resource:
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// The empire tree (BL-912) — availability, the scorer, the rim
// ---------------------------------------------------------------------------

bool empire_node_available(uint64_t mask, int node_idx)
{
    if (node_idx < 0 || node_idx >= io::empire_tree::node_count) return false;
    const uint64_t bit = 1ULL << node_idx;
    if (mask & bit) return false; // already held

    const io::empire_tree::node& n = io::empire_tree::nodes[node_idx];

    // A closed fork side goes dark PERMANENTLY (TREES.md sec Forks): taking
    // one closes the other's availability window for good, and nothing ever
    // un-completes, so this is a one-way gate on the mask alone.
    if (n.excludes >= 0 && (mask & (1ULL << n.excludes))) return false;

    // Rule 1/the spire: ring r is locked until a held node carries the
    // store's `open "ring r"` effect — the milestone one ring down, by the
    // spire's shape (TREES.md sec Milestones). Ring 1 has no gate. BL-973:
    // read off the effect, not off "a milestone at ring r-1".
    if (n.ring > 1
     && !tree_ring_open(mask, static_cast<int>(n.ring), io::empire_tree::nodes, io::empire_tree::effects))
        return false;

    // Rule 2: travel is OR. Available if it is the tree's true root (its OWN
    // declared `links` was empty — see `node::is_root`'s comment: every
    // other ring-1 major names EM-SP-1a as ITS prerequisite, which makes
    // EM-SP-1a non-isolated in the undirected `neighbours_mask` even though
    // it has none of its own) or at least one linked neighbour is held.
    if (!n.is_root && n.neighbours_mask != 0 && (mask & n.neighbours_mask) == 0) return false;

    // Rule 4, the AND half: a milestone's `requires` (majors from >=2
    // branches at its own ring) must ALL be held, plus, if present, one side
    // of its fork pair (TREES.md sec Milestones: requires_fork, EITHER side).
    if (n.kind == io::empire_tree::node_kind::milestone)
    {
        if ((mask & n.requires_mask) != n.requires_mask) return false;
        if (n.requires_fork_a >= 0)
        {
            const bool a = (mask & (1ULL << n.requires_fork_a)) != 0;
            const bool b = (mask & (1ULL << n.requires_fork_b)) != 0;
            if (!a && !b) return false;
        }
    }

    return true;
}

int choose_empire_node(uint64_t mask, int cohesion_q, int stores_low_q,
                        int reach_bound_q, int manpower_bound_q, int food_bound_q,
                        int ground_ore_q, int ground_farm_q, int ground_fuel_q,
                        int ground_port_q, int surplus_q)
{
    using namespace io::empire_tree;

    // THE SCORER (TREES.md sec "The scorer — one shape, three trees"): a
    // frontier node is scored on the term its `pursued_when` names, plus a
    // constant per-kind pull that keeps the spire climbing (a milestone opens
    // the next ring for every branch at once, so it outweighs a lone major),
    // minus its cost. Every term below reads a quantity the round already
    // computed (ground means, cohesion, arrears-style binding); terms this
    // slice cannot cheaply derive (`threatened`, `plague_struck`,
    // `many_peoples`, `known`) are left at 0 rather than guessed — TREES.md
    // sec Effects allows placeholder magnitudes, and a 0 term simply never
    // wins the argmax on its own account, which is honest rather than wrong.
    //
    // INTEGER THROUGHOUT, and the tie-break is the LOWER NODE INDEX — the
    // tree's own fixed authored order (io::empire_tree::nodes), so two
    // polities scoring a tie always resolve it identically.
    const int term_value[term_count] = {
        /* spire             */ 1000,
        /* stores_low        */ stores_low_q,
        /* surplus           */ surplus_q,
        /* reach_bound       */ reach_bound_q,
        /* coastal_holdings  */ ground_port_q,
        /* threatened        */ 0,
        /* ground_ore        */ ground_ore_q,
        /* manpower_bound    */ manpower_bound_q,
        /* ground_grass      */ ground_farm_q, // farm_q is the pasture proxy
                                                // elsewhere in this codebase
                                                // (unit_roster.hpp), reused here
        /* cohesion_low      */ 1000 - clampi(cohesion_q, 0, 1000),
        /* ground_fuel       */ ground_fuel_q,
        /* food_bound        */ food_bound_q,
        /* ground_farm       */ ground_farm_q,
        /* plague_struck     */ 0,
        /* many_peoples      */ 0,
    };

    // Endowment gates (TREES.md sec The five rules, rule 2: "A major's
    // endowment gate... [is an] additional AND condition on top of
    // availability"). `grassland` has no ground signal of its own in this
    // codebase and reuses the farm mean, exactly as `unit_roster.hpp` does
    // for pasture ("PROXY FOR PASTURE — horses have no signal of their own").
    // A bar of 250/1000 is a placeholder, per TREES.md sec Effects.
    const auto gate_open = [&](gate_atom g) {
        switch (g)
        {
        case gate_atom::none:       return true;
        case gate_atom::ore_q:      return ground_ore_q  >= 250;
        case gate_atom::fuel:       return ground_fuel_q >= 250;
        case gate_atom::arable:     return ground_farm_q >= 250;
        case gate_atom::coastal:    return ground_port_q > 0;
        case gate_atom::grassland:  return ground_farm_q >= 250;
        }
        return true;
    };

    int best_idx = -1;
    int best_score = INT32_MIN;
    for (int i = 0; i < node_count; ++i)
    {
        if (!empire_node_available(mask, i)) continue;
        const node& n = nodes[i];
        if (!gate_open(n.gate)) continue;
        const int kind_bonus = n.kind == node_kind::milestone ? 600
                              : n.kind == node_kind::major     ? 0
                                                                : -100; // minors trail their major
        const int term = clampi(term_value[static_cast<int>(node_term[i])], 0, 1000);
        const int cost_q = (n.kind == node_kind::minor ? 1 : n.kind == node_kind::major ? 3 : 6)
                          * static_cast<int>(n.ring);
        const int score = term + kind_bonus - cost_q;
        if (score > best_score) { best_score = score; best_idx = i; }
    }
    return best_idx;
}

// ---------------------------------------------------------------------------
// The exploration tree (BL-930) — availability, the scorer, the rim
// ---------------------------------------------------------------------------

bool exploration_node_available(uint64_t mask, int node_idx)
{
    if (node_idx < 0 || node_idx >= io::exploration_tree::node_count) return false;
    const uint64_t bit = 1ULL << node_idx;
    if (mask & bit) return false; // already held

    const io::exploration_tree::node& n = io::exploration_tree::nodes[node_idx];

    // A closed fork side goes dark PERMANENTLY (TREES.md sec Forks) — same
    // one-way gate `empire_node_available` runs.
    if (n.excludes >= 0 && (mask & (1ULL << n.excludes))) return false;

    // Rule 1/the spire: ring r is locked until a held node carries the
    // store's `open "ring r"` effect (BL-973 — the effect, not a counted
    // milestone). Ring 1 has no gate.
    if (n.ring > 1
     && !tree_ring_open(mask, static_cast<int>(n.ring), io::exploration_tree::nodes, io::exploration_tree::effects))
        return false;

    // Rule 2: travel is OR. Available if it is the tree's true root, or at
    // least one linked neighbour is held.
    if (!n.is_root && n.neighbours_mask != 0 && (mask & n.neighbours_mask) == 0) return false;

    // Rule 4, the AND half: a milestone's `requires` set must ALL be held,
    // plus, if present, one side of its fork pair.
    if (n.kind == io::exploration_tree::node_kind::milestone)
    {
        if ((mask & n.requires_mask) != n.requires_mask) return false;
        if (n.requires_fork_a >= 0)
        {
            const bool a = (mask & (1ULL << n.requires_fork_a)) != 0;
            const bool b = (mask & (1ULL << n.requires_fork_b)) != 0;
            if (!a && !b) return false;
        }
    }

    return true;
}

int consolidator_lean_q(const culture& c)
{
    if (c.pantheon.size() < 2) return 0; // no war-god slot -> no lean either way.
    const int dominion_q = clampi(c.pantheon[1].dominion, 0, 10) * 100; // 0-1000
    const int sea_q      = clampi(c.sea_legs_q, 0, 1000);
    // "High dominion WITH NO SEA LEGS" -- the product form so both facts must
    // hold at once: high dominion alone (a proud but coastal people) is not
    // yet a consolidator, and no sea legs alone (a landlocked but timid one)
    // is not either.
    return clampi((dominion_q * (1000 - sea_q)) / 1000, 0, 1000);
}

int expansion_lean_q(const culture& c)
{
    const int sea_q = clampi(c.sea_legs_q, 0, 1000);
    if (c.pantheon.size() < 2) return sea_q; // sea legs alone still says something.
    const int zeal_q = clampi(c.pantheon[1].zeal, 0, 10) * 100; // 0-1000
    // "Deep sea legs WITH a universalising creed" -- zeal stands in for the
    // creed's own missionary push, the same axis `culture_opposition_q`
    // reads for temperament. Averaged rather than multiplied: unlike the
    // consolidator case, EXPLORATION.md does not require BOTH facts at their
    // extreme for the reading to matter, and a product would zero out a
    // strong-sea-legs, middling-zeal culture that should still read as an
    // expansionist by the doc's own "deep sea legs" framing.
    return clampi((sea_q + zeal_q) / 2, 0, 1000);
}

int choose_exploration_node(uint64_t mask, int stores_low_q, int reach_bound_q,
                             int ground_port_q, int ground_farm_q, int surplus_q,
                             int purse_low_q, int wants_unmet_q, int throughput_bound_q,
                             int subject_held_q, int consolidator_lean_q, int expansion_lean_q,
                             int creed_lean_weight_q)
{
    using namespace io::exploration_tree;

    // THE SCORER (TREES.md sec "The scorer — one shape, four trees"), read
    // against this tree's own term set (exploration_tree_data.hpp's
    // `scorer_term`, 12 distinct terms across the 31 nodes).
    //
    // `purse_low`/`wants_unmet`/`throughput_bound` are now real, threaded-
    // through arguments (BL-932/BL-939/BL-940). `subject_held` is now real
    // too (BL-934): the caller passes 1000 where the polity holds at least
    // one subject, 0 otherwise (`polity::overlord >= 0` read on ANOTHER
    // polity, never on this one) — a bare boolean rather than a count,
    // exactly as `coastal_holdings`/`ground_port` are booleans-as-1000
    // already. `threatened` and `known` are STILL 0 here for the same reason
    // `choose_empire_node` leaves `threatened` at 0: this slice has no cheap
    // "visible capability" or "foreign agent" signal to read yet. A 0 term
    // never wins the argmax on its own account (it can still lose to one),
    // which is honest rather than wrong.
    //
    // EVERY HARNESS CALL SITE FROM BEFORE THIS ITEM PASSES 0 FOR
    // `subject_held_q` (the parameter's own default), so BL-934 landing is
    // visible as a diff only where a caller actually threads a live value
    // through, never as a silent change to an existing fixture.
    //
    // BL-942 -- TWO WAYS TO BE STRONG. `consolidator_lean_q`/
    // `expansion_lean_q` (0-1000, `consolidator_lean_q`/`expansion_lean_q`
    // free functions below, derived from the polity's own culture) WEIGHT
    // the terms EXPLORATION.md sec Two ways to be strong names for each
    // strategy -- `throughput_bound` for the consolidator (the road ladder,
    // dense internal throughput: WY branch's own term already, per
    // exploration_tree_data.hpp), `coastal_holdings`/`ground_port`/
    // `subject_held` for the expansionist (ports, hulls, subjects: the HL/PT
    // branches' own terms). This is a WEIGHTING of terms the scorer already
    // has, never a flag or a branch on which one a polity "is" -- a lean of
    // 0 on both axes reduces exactly to the pre-BL-942 scorer, and
    // `creed_lean_weight_q == 0` does the same for every polity at once.
    const int cons_bonus = clampi(consolidator_lean_q, 0, 1000)
                          * clampi(creed_lean_weight_q, 0, 1000) / 1000;
    const int expn_bonus = clampi(expansion_lean_q, 0, 1000)
                          * clampi(creed_lean_weight_q, 0, 1000) / 1000;
    const int term_value[term_count] = {
        /* stores_low       */ stores_low_q,
        /* spire            */ 1000,
        /* purse_low        */ purse_low_q,       // BL-932
        /* surplus          */ surplus_q,
        /* coastal_holdings */ clampi(ground_port_q + expn_bonus, 0, 1000),      // BL-942
        /* reach_bound      */ reach_bound_q,
        /* wants_unmet      */ wants_unmet_q,     // BL-939
        /* threatened       */ 0,
        /* ground_port      */ clampi(ground_port_q + expn_bonus, 0, 1000),      // BL-942
        /* subject_held     */ clampi(clampi(subject_held_q, 0, 1000) + expn_bonus, 0, 1000), // BL-934/942
        /* throughput_bound */ clampi(throughput_bound_q + cons_bonus, 0, 1000), // BL-940/942
        /* known            */ 0,
    };

    // Endowment gates, same shape as `choose_empire_node`'s: `coastal` reads
    // whether the polity's held ground carries a port endowment, `arable`
    // whether its farm mean clears the same placeholder 250/1000 bar.
    const auto gate_open = [&](gate_atom g) {
        switch (g)
        {
        case gate_atom::none:       return true;
        case gate_atom::ore_q:      return true; // unused by this tree; open rather than blocking
        case gate_atom::fuel:       return true; // unused by this tree
        case gate_atom::arable:     return ground_farm_q >= 250;
        case gate_atom::coastal:    return ground_port_q > 0;
        case gate_atom::grassland:  return ground_farm_q >= 250;
        }
        return true;
    };

    int best_idx = -1;
    int best_score = INT32_MIN;
    for (int i = 0; i < node_count; ++i)
    {
        if (!exploration_node_available(mask, i)) continue;
        const node& n = nodes[i];
        if (!gate_open(n.gate)) continue;
        const int kind_bonus = n.kind == node_kind::milestone ? 600
                              : n.kind == node_kind::major     ? 0
                                                                : -100; // minors trail their major
        const int term = clampi(term_value[static_cast<int>(node_term[i])], 0, 1000);
        const int cost_q = (n.kind == node_kind::minor ? 1 : n.kind == node_kind::major ? 3 : 6)
                          * static_cast<int>(n.ring);
        const int score = term + kind_bonus - cost_q;
        if (score > best_score) { best_score = score; best_idx = i; }
    }
    return best_idx;
}

// ---------------------------------------------------------------------------
// The industry tree (BL-1038) — availability, the scorer, the rate
// ---------------------------------------------------------------------------

int64_t isqrt64(int64_t v)
{
    if (v <= 0) return 0;
    const uint64_t n = static_cast<uint64_t>(v);
    // Start at 2^ceil(bits/2), which is >= sqrt(n) because n < 2^bits. From an
    // overestimate Newton's step floor((x + n/x) / 2) falls monotonically to
    // floor(sqrt(n)) and then stops falling — the loop's exit. x + n/x stays
    // below 2^33 for any n < 2^63, so nothing overflows.
    int bits = 0;
    for (uint64_t t = n; t != 0; t >>= 1) ++bits;
    uint64_t x = 1ULL << ((bits + 1) / 2);
    for (;;)
    {
        const uint64_t y = (x + n / x) >> 1;
        if (y >= x) break;
        x = y;
    }
    return static_cast<int64_t>(x);
}

int64_t industry_urban_mass(const std::vector<region>& regions, const std::vector<int>& held)
{
    // The k largest, kept descending by bubbling each value down the array.
    // Only the SUM is returned, and the sum of the k largest values does not
    // depend on which of several equal values landed where.
    int64_t top[industry_research_top_k] = {};
    for (int hi : held)
    {
        if (hi < 0 || static_cast<std::size_t>(hi) >= regions.size()) continue;
        int64_t v = std::max<int64_t>(0, regions[static_cast<std::size_t>(hi)].urban_population);
        for (int k = 0; k < industry_research_top_k; ++k)
            if (v > top[k]) std::swap(v, top[k]);
    }
    int64_t sum = 0;
    for (int k = 0; k < industry_research_top_k; ++k) sum += top[k];
    return sum;
}

int industry_spire_ring(uint64_t mask)
{
    // The empire rate's own loop (the Invest block, BL-912), over this table.
    int ring = 0;
    for (int i = 0; i < io::industry_tree::node_count; ++i)
    {
        const io::industry_tree::node& nd = io::industry_tree::nodes[i];
        if (nd.kind == io::industry_tree::node_kind::milestone && (mask & (1ULL << i)))
            ring = std::max(ring, static_cast<int>(nd.ring));
    }
    return ring;
}

int64_t industry_research_per_year_q(int64_t urban_mass, int spire_ring, int research_mod_q,
                                     const history_sim_params& params)
{
    // CLAMP FIRST, then transform. The cap is the design's own bound (a
    // param); 2^32 heads is the arithmetic's, so that whatever the param says
    // Mc * isqrt(Mc) stays under 2^48, and with the spire ring clamped to 6
    // and the research modifier to 4000 no product below passes 2^62.
    const int64_t cap = clampi64(params.industry_urban_mass_cap, 0, 1LL << 32);
    const int64_t mc  = clampi64(urban_mass, 0, cap);
    const int64_t ref = std::max<int64_t>(1, isqrt64(std::max<int64_t>(1, params.industry_urban_mass_reference)));
    const int64_t superlinear = (mc * isqrt64(mc)) / ref;      // Mc x sqrt(Mc / reference)
    const int64_t fraction    = clampi(params.industry_research_fraction_q, 0, 1000);
    // FROM HERE ON, THE EMPIRE RATE'S OWN SCALING, factor for factor and in
    // its order (TREES.md sec State: a research rate is scaled by the spire
    // ring held and by contact degree; the Industry tree swaps only the base,
    // the industry slice, for urban mass). Contact degree is 0 here as it is
    // in every tree — the inert `(1000 + 0 * 40) / 1000` is not written out.
    // The `research` modifier is every tree's reader of itself (BL-973).
    const int64_t rate = (superlinear * fraction) / 1000
                             * (1000 + clampi(spire_ring, 0, 6) * 150) / 1000
                             * (1000 + clampi(research_mod_q, 0, 4000)) / 1000;
    return rate;
}

bool industry_node_available(uint64_t mask, int node_idx)
{
    if (node_idx < 0 || node_idx >= io::industry_tree::node_count) return false;
    const uint64_t bit = 1ULL << node_idx;
    if (mask & bit) return false; // already held

    const io::industry_tree::node& n = io::industry_tree::nodes[node_idx];

    // A closed fork side goes dark permanently (TREES.md sec Forks).
    if (n.excludes >= 0 && (mask & (1ULL << n.excludes))) return false;

    // Rule 1/the spire: ring r is locked until a held node carries the
    // store's `open "ring r"` effect. Ring 1 has no gate.
    if (n.ring > 1
     && !tree_ring_open(mask, static_cast<int>(n.ring), io::industry_tree::nodes, io::industry_tree::effects))
        return false;

    // Rule 2: travel is OR — the root is exempt, anything else needs a held
    // neighbour.
    if (!n.is_root && n.neighbours_mask != 0 && (mask & n.neighbours_mask) == 0) return false;

    // Rule 4, the AND half: a milestone's `requires` set, plus one side of
    // its fork pair where it names one (IN-SP-1m: the Fuel Doctrine; IN-SP-2m:
    // the Works Doctrine).
    if (n.kind == io::industry_tree::node_kind::milestone)
    {
        if ((mask & n.requires_mask) != n.requires_mask) return false;
        if (n.requires_fork_a >= 0)
        {
            const bool a = (mask & (1ULL << n.requires_fork_a)) != 0;
            const bool b = (mask & (1ULL << n.requires_fork_b)) != 0;
            if (!a && !b) return false;
        }
    }

    return true;
}

namespace {

/// BL-1056: the Fuel Doctrine's two pulls, taken over ONE region set. A held
/// region counts only if it carries the span-open survey of BOTH scores -- the
/// survey writes the two together. Each span-open SHARE is tested against ITS
/// OWN resource's top-third bar (BL-1059, NR-899, NR-900). A region the span founded has no
/// forest reading (DEFAULT A inherits fuel only), so it is out of BOTH pulls,
/// never in one and out of the other. Writes the two clear-counts and the
/// count read; order-free.
void industry_ground_pull_counts(const std::vector<region>& regions, const std::vector<int>& held,
                                 const industry_ground_bars& bars,
                                 int64_t& read, int64_t& fuel_clear, int64_t& forest_clear)
{
    read = fuel_clear = forest_clear = 0;
    for (int hi : held)
    {
        if (hi < 0 || static_cast<std::size_t>(hi) >= regions.size()) continue;
        const region& r = regions[static_cast<std::size_t>(hi)];
        if (r.survey_fuel_q < 0 || r.survey_forest_q < 0) continue;
        ++read;
        if (industry_ground_clears(r.survey_fuel_raw,   bars.fuel_raw))   ++fuel_clear;
        if (industry_ground_clears(r.survey_forest_raw, bars.forest_raw)) ++forest_clear;
    }
}

/// BL-1059: one resource's top-third bar over @p shares (every read region's
/// span-open share, zeros included; sorted in place). k = floor(n / 3), the
/// bar is the value at ascending position n - k, and a tie band at that value
/// clears WHOLE (NR-904) -- see `industry_ground_bars`. A 0 at the cut becomes
/// a bar of 1: zeros never clear, and every carrier does.
int industry_top_third_bar(std::vector<int>& shares)
{
    const std::size_t n = shares.size();
    const std::size_t k = n / 3;
    if (k == 0) return industry_ground_bar_none;
    std::sort(shares.begin(), shares.end());
    return std::max(shares[n - k], 1);
}

} // namespace

industry_ground_bars industry_ground_bars_at_open(const std::vector<region>& regions)
{
    // THE TOP-THIRD BARS (BL-1059; Ben, 2026-09-19, NR-899, NR-900;
    // INDUSTRY_TREE.md sec The scorer). Read over EVERY region carrying both
    // span-open scores -- the set the pulls read -- zeros included, so both
    // resources clear at one rate of the world; ranked on the UNCLAMPED share,
    // so a pile at the score's cap is not one tied value. A region with no
    // share (a hand-built table only) ranks as 0. The sort makes each bar
    // independent of region order.
    std::vector<int> fuel, forest;
    fuel.reserve(regions.size());
    forest.reserve(regions.size());
    for (const region& r : regions)
    {
        if (r.survey_fuel_q < 0 || r.survey_forest_q < 0) continue;
        fuel.push_back(std::max(r.survey_fuel_raw, 0));
        forest.push_back(std::max(r.survey_forest_raw, 0));
    }
    industry_ground_bars b;
    b.fuel_raw   = industry_top_third_bar(fuel);
    b.forest_raw = industry_top_third_bar(forest);
    return b;
}

bool industry_span_survey_ran(const std::vector<region>& regions)
{
    for (const region& r : regions)
        if (r.survey_forest_q >= 0) return true;
    return false;
}

int industry_ground_forest_q(const std::vector<region>& regions, const std::vector<int>& held,
                             const industry_ground_bars& bars)
{
    // THE SHARE OF HELD GROUND IN THE WORLD'S TOP THIRD FOR FOREST (BL-1056,
    // NR-896; the bar BL-1059, NR-899; INDUSTRY_TREE.md sec The scorer). It
    // was the BEST held score, which can only rise as a realm grows: breadth,
    // not ground, decided the Fuel Doctrine. A share of held regions cannot
    // grow with the realm's size -- a region not clearing the bar only ever lowers it,
    // and one with no span-open reading leaves it alone. With no read region
    // held the reading is 0: off the span that is the old pin, and inside it
    // a realm on ground it has no reading of pulls toward neither side.
    int64_t read = 0, fuel_clear = 0, forest_clear = 0;
    industry_ground_pull_counts(regions, held, bars, read, fuel_clear, forest_clear);
    return read > 0 ? static_cast<int>((forest_clear * 1000) / read) : 0;
}

int industry_ground_fuel_q(const std::vector<region>& regions, const std::vector<int>& held,
                           const industry_ground_bars& bars, bool survey_ran)
{
    // THE SHARE OF HELD GROUND IN THE WORLD'S TOP THIRD FOR FUEL (BL-1056,
    // NR-896; the bar BL-1059, NR-899), over EXACTLY the regions the forest
    // share reads, each against its own resource's bar, so the Fuel Doctrine
    // compares like with like. -1 only when
    // the span-open survey never happened (@p survey_ran false: every path but
    // the Industrialisation span), and the term then keeps the seam, the old
    // reading. Inside the span a realm with no read region reads 0, never the
    // seam: a best-of-held fallback there would grow with the realm again.
    // The GATE is not this -- it still reads any held seam.
    if (!survey_ran) return -1;
    int64_t read = 0, fuel_clear = 0, forest_clear = 0;
    industry_ground_pull_counts(regions, held, bars, read, fuel_clear, forest_clear);
    return read > 0 ? static_cast<int>((fuel_clear * 1000) / read) : 0;
}

// ---------------------------------------------------------------------------
// BL-1041 — industry points
// ---------------------------------------------------------------------------

bool industry_points_params_valid(const history_sim_params& p)
{
    return p.industry_points_per_million_urban_heads_year >= 0
        && p.industry_points_per_million_urban_heads_year <= 1000000
        && p.industry_points_per_treasury_unit >= 1 && p.industry_points_per_treasury_unit <= 10000
        && p.industry_points_fuel_floor_q     >= 0 && p.industry_points_fuel_floor_q     <= 1000
        && p.industry_points_treasury_share_q >= 0 && p.industry_points_treasury_share_q <= 1000;
}

int industry_fuel_reading_q(const region& r)
{
    // -1 is "not surveyed" (settlement.hpp), never "no fuel": such ground reads
    // the energy_q it was founded with, which is the best survey there is.
    if (r.survey_fuel_q >= 0) return std::min(r.survey_fuel_q, 1000);
    return clampi(r.energy_q, 0, 1000);
}

int industry_points_fuel_factor_q(int fuel_reading_q, const history_sim_params& p)
{
    const int floor_q = p.industry_points_fuel_floor_q; // in domain: the caller validated
    const int f       = clampi(fuel_reading_q, 0, 1000); // the reading's own 0-1000 scale
    return floor_q + ((1000 - floor_q) * f) / 1000;
}

int industry_tree_industrial_q(uint64_t industry_mask)
{
    using namespace io::industry_tree;
    // KNOWN DRIFT, OPEN (BL-1041 cold review): INDUSTRIALISATION.md sec Beat 1 reads
    // "Industry tree CAPACITY nodes held", and the store tags every node's
    // diffusion kind (capacity / practice / artifact; TREES.md sec Diffusion
    // follows kind). The GENERATED table (`industry_tree_data.hpp`) does not
    // carry that kind, so this fold cannot filter on it and sums the
    // `industrial` modifier of every held node -- practice nodes included
    // (IN-MT-1e +30, IN-LD-1b +80, IN-LD-1c -40, IN-CH-2b +40, IN-CH-2c +20).
    // Filtering needs the generator to emit the kind first.
    int sum = 0;
    for (int i = 0; i < node_count; ++i)
    {
        if (!(industry_mask & (1ULL << i))) continue;
        const node& n = nodes[i];
        for (int r = 0; r < static_cast<int>(n.effects_n); ++r)
        {
            const io::tree_effect& e = effects[n.effects_begin + r];
            if (e.kind == io::tree_effect_kind::modifier
             && e.term == io::tree_modifier_term::industrial)
                sum += e.per_mille;
        }
    }
    return sum;
}

int64_t industry_points_scale_credit(const region& r, int industrial_q,
                                     const history_sim_params& p, int step_years)
{
    if (r.centres <= 0) return 0; // only a region with centres builds
    const int64_t heads = r.urban_population;
    if (heads < 0 || heads > industry_points_urban_heads_max) return -1;
    if (step_years < 1 || step_years > industry_points_step_years_max) return -1;
    // The tree multiplier's DOMAIN (see the header): outside it is a table
    // defect, refused rather than clamped into a plausible number.
    const int64_t tree_mult_q = 1000 + static_cast<int64_t>(industrial_q);
    if (tree_mult_q < 100 || tree_mult_q > 5000) return -1;

    // Staged so no product leaves int64: heads (<= 2^31) x rate (<= 10^6)
    // x years (<= 10^3) < 2.2e18, then divided before each further factor.
    int64_t pts = (heads * p.industry_points_per_million_urban_heads_year
                   * static_cast<int64_t>(step_years)) / 1000000;
    pts = (pts * industry_points_fuel_factor_q(industry_fuel_reading_q(r), p)) / 1000;
    pts = (pts * tree_mult_q) / 1000;
    return pts;
}

bool industry_points_apportion_by_scale(const std::vector<region>& regions, int holder, int64_t credit,
                                        std::vector<std::pair<int, int64_t>>& out)
{
    // BL-1056 (Ben, 2026-09-19, NR-897). LARGEST REMAINDER, EXACT: region i's
    // share is credit * w_i / W; it takes the floor, and the credit the floors
    // leave (fewer points than regions weighed) goes one each to the largest
    // remainders, ties to the lower region index. Staged as
    // (credit / W) * w_i + ((credit % W) * w_i) / W so nothing leaves int64:
    // credit % W < W <= 2^32 and w_i <= 2^31.
    out.clear();
    if (credit <= 0) return true;
    int64_t total = 0;
    for (std::size_t i = 0; i < regions.size(); ++i)
    {
        const region& r = regions[i];
        if (r.nation != holder || r.centres <= 0 || r.urban_population <= 0) continue;
        if (r.urban_population > industry_points_urban_heads_max) { out.clear(); return false; }
        total += r.urban_population;
        if (total > industry_points_apportion_heads_max) { out.clear(); return false; }
        out.emplace_back(static_cast<int>(i), r.urban_population); // the weight, for now
    }
    if (out.empty()) return true;

    const int64_t whole = credit / total, part = credit % total;
    std::vector<int64_t> rem(out.size(), 0);
    int64_t given = 0;
    for (std::size_t k = 0; k < out.size(); ++k)
    {
        const int64_t w   = out[k].second;
        const int64_t num = part * w;
        out[k].second = whole * w + num / total;
        rem[k]        = num % total;
        given        += out[k].second;
    }
    int64_t left = credit - given; // 0 <= left < out.size()
    if (left > 0)
    {
        std::vector<std::size_t> order(out.size());
        for (std::size_t k = 0; k < order.size(); ++k) order[k] = k;
        // `out` is in ascending region index, so a stable sort on the
        // remainder alone breaks ties to the lower region index.
        std::stable_sort(order.begin(), order.end(),
                         [&rem](std::size_t a, std::size_t b) { return rem[a] > rem[b]; });
        for (std::size_t k = 0; k < order.size() && left > 0; ++k, --left) ++out[order[k]].second;
    }
    return true;
}

industry_points_round accrue_industry_points(std::vector<region>&       regions,
                                             const std::vector<polity>& polities,
                                             const history_sim_params&  p,
                                             int                        step_years)
{
    // The holder's capacity, once per polity per round, indexed like the
    // table (every creation site assigns id = index; checked, and a region
    // whose holder does not resolve reads 0, like unheld ground).
    std::vector<int> industrial(polities.size(), 0);
    for (std::size_t i = 0; i < polities.size(); ++i)
        if (polities[i].id == static_cast<int>(i))
            industrial[i] = industry_tree_industrial_q(polities[i].industry_mask);

    industry_points_round out;
    for (region& r : regions)
    {
        if (r.centres <= 0) continue;
        const int ind = (r.nation >= 0 && static_cast<std::size_t>(r.nation) < industrial.size())
                            ? industrial[static_cast<std::size_t>(r.nation)] : 0;
        const int64_t credit = industry_points_scale_credit(r, ind, p, step_years);
        if (credit < 0 || r.industry_points > industry_points_ceiling - credit)
        {
            ++out.refused; // nothing moves on this region
            continue;
        }
        r.industry_points += credit;
        out.credited      += credit;
    }
    return out;
}

namespace {

/// Compile-time string equality over the generated table's ids.
constexpr bool industry_id_is(const char* a, const char* b)
{
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return *a == *b;
}

/// A node's index in `io::industry_tree::nodes`, found BY ID at compile time,
/// or -1. Never a magic index: a store edit that renumbers the table moves
/// this with it, and one that drops the id fails the static_assert below.
constexpr int industry_node_by_id(const char* id)
{
    for (int i = 0; i < io::industry_tree::node_count; ++i)
        if (industry_id_is(io::industry_tree::nodes[i].id, id)) return i;
    return -1;
}

/// IN-MT-1a, Coke Smelting: the node `furnace_lit` reads.
constexpr int industry_coke_smelting_index = industry_node_by_id("IN-MT-1a");
static_assert(industry_coke_smelting_index >= 0 && industry_coke_smelting_index < 64,
              "IN-MT-1a (Coke Smelting) is not in the Industry store: `furnace_lit` reads it BY ID "
              "(INDUSTRY_TREE.md sec The scorer) -- re-read the scorer before renaming it");

} // namespace

bool industry_coke_smelting_held(uint64_t mask)
{
    // Ben, 2026-09-18 (wave 1 form): `furnace_lit` is COKE SMELTING HELD -- a
    // coal-fired furnace. It read "a Fuel Doctrine side held" (NR-892), and
    // that was 1000 at every pick that reads the term: all three readers
    // (Electrification, Synthetic Chemistry, Plant Registry) sit behind The
    // Cheap Ton, which requires a side. exploration_sim_harness T8.7.3 pins it.
    return (mask & (1ULL << industry_coke_smelting_index)) != 0;
}

void industry_term_values(uint64_t mask, const industry_scorer_reading& r,
                          int (&out)[io::industry_tree::term_count])
{
    using namespace io::industry_tree;

    // THE ORDER GUARD. `out` is written BY NAME through the generated enum
    // (`at(scorer_term::x)`), never positionally, so a store edit that
    // reorders the terms cannot put a reading under the wrong term. These
    // asserts make any such edit LOUD anyway: a new term would otherwise read
    // a silent 0 here, a removed one fails to compile below, and a reorder
    // means the store moved under the scorer and somebody should look. If one
    // fires after `gen_empire_tree_table.js industry`, re-read
    // INDUSTRY_TREE.md sec The scorer, give the new term a reading (or pin it
    // at 0 by name, as two are below), then update these lines.
    static_assert(term_count == 22, "industry scorer terms changed: every term needs a reading here");
    static_assert(static_cast<int>(scorer_term::spire)           ==  0
               && static_cast<int>(scorer_term::reach_bound)     ==  1
               && static_cast<int>(scorer_term::furnace_lit)     ==  2
               && static_cast<int>(scorer_term::ground_ore)      ==  3
               && static_cast<int>(scorer_term::ground_fuel)     ==  4
               && static_cast<int>(scorer_term::ground_forest)   ==  5
               && static_cast<int>(scorer_term::tariff_pressure) ==  6
               && static_cast<int>(scorer_term::threatened)      ==  7
               && static_cast<int>(scorer_term::food_bound)      ==  8
               && static_cast<int>(scorer_term::plague_struck)   ==  9
               && static_cast<int>(scorer_term::coastal_holdings)== 10
               && static_cast<int>(scorer_term::colonial_reach)  == 11
               && static_cast<int>(scorer_term::cohesion_low)    == 12
               && static_cast<int>(scorer_term::fuel_bound)      == 13
               && static_cast<int>(scorer_term::labour_bound)    == 14
               && static_cast<int>(scorer_term::manpower_bound)  == 15
               && static_cast<int>(scorer_term::ground_farm)     == 16
               && static_cast<int>(scorer_term::ground_port)     == 17
               && static_cast<int>(scorer_term::surplus)         == 18
               && static_cast<int>(scorer_term::known)           == 19
               && static_cast<int>(scorer_term::credit_bound)    == 20
               && static_cast<int>(scorer_term::many_peoples)    == 21,
                  "industry scorer_term order moved: the store changed under the scorer");

    const auto at = [&out](scorer_term t) -> int& { return out[static_cast<int>(t)]; };
    for (int& v : out) v = 0;

    // `furnace_lit` — the polity holds Coke Smelting (Ben, 2026-09-18, wave 1
    // form; before it, a Fuel Doctrine side, which every reader held by
    // construction). Found by id at compile time, never by a magic index.
    const bool furnace_lit = industry_coke_smelting_held(mask);

    at(scorer_term::spire)            = 1000;
    at(scorer_term::reach_bound)      = r.reach_bound_q;
    at(scorer_term::furnace_lit)      = furnace_lit ? 1000 : 0;
    at(scorer_term::ground_ore)       = r.ground_ore_q;
    // BL-1056 (NR-896): both Fuel Doctrine pulls read a SHARE of held ground
    // clearing that resource's top-third bar (BL-1059, NR-899, NR-900); off
    // the span no share is taken (-1) and the coal pull keeps the seam, the old
    // reading. The GATE still reads the seam.
    at(scorer_term::ground_fuel)      = r.ground_fuel_q >= 0 ? r.ground_fuel_q : r.fuel_seam_q;
    at(scorer_term::ground_forest)    = r.ground_forest_q;   // the span-open survey, share of held clearing the forest bar
    at(scorer_term::tariff_pressure)  = 0; // PINNED: no landed price at a market before the campaign
    at(scorer_term::threatened)       = r.threatened_q;
    at(scorer_term::food_bound)       = r.food_bound_q;
    at(scorer_term::plague_struck)    = 0; // PINNED: the history sim runs no plague (the empire scorer pins it too)
    at(scorer_term::coastal_holdings) = r.ground_port_q;
    at(scorer_term::colonial_reach)   = r.colonial_reach_q;
    at(scorer_term::cohesion_low)     = 1000 - clampi(r.cohesion_q, 0, 1000);
    at(scorer_term::fuel_bound)       = r.fuel_bound_q;
    at(scorer_term::labour_bound)     = r.labour_bound_q;
    at(scorer_term::manpower_bound)   = r.manpower_bound_q;
    at(scorer_term::ground_farm)      = r.ground_farm_q;
    at(scorer_term::ground_port)      = r.ground_port_q;
    at(scorer_term::surplus)          = r.surplus_q;
    at(scorer_term::known)            = 0; // per node: `choose_industry_node` reads `known_mask`
    at(scorer_term::credit_bound)     = r.credit_bound_q;
    at(scorer_term::many_peoples)     = r.many_peoples_q;

    for (int& v : out) v = clampi(v, 0, 1000);
}

bool industry_gate_open(io::industry_tree::gate_atom g, const industry_scorer_reading& r)
{
    using io::industry_tree::gate_atom;
    // The EMPIRE scorer's gate_open, atom for atom, except `fuel`: the seam
    // (the max over held ground) against the same bar, where the empire reads
    // the mean. `grassland` reuses the farm mean as the empire's does.
    switch (g)
    {
    case gate_atom::none:       return true;
    case gate_atom::ore_q:      return r.ground_ore_q  >= 250;
    case gate_atom::fuel:       return r.fuel_seam_q   >= industry_fuel_seam_bar_q;
    case gate_atom::arable:     return r.ground_farm_q >= 250;
    case gate_atom::coastal:    return r.ground_port_q > 0;
    case gate_atom::grassland:  return r.ground_farm_q >= 250;
    }
    return true;
}

int choose_industry_node(uint64_t mask, const industry_scorer_reading& r)
{
    using namespace io::industry_tree;

    int term_value[term_count];
    industry_term_values(mask, r, term_value);

    // Same shape as the other two scorers: term + kind bonus - cost, integer,
    // ties to the lower node index.
    int best_idx = -1;
    int best_score = INT32_MIN;
    for (int i = 0; i < node_count; ++i)
    {
        if (!industry_node_available(mask, i)) continue;
        const node& n = nodes[i];
        if (!industry_gate_open(n.gate, r)) continue;
        const int kind_bonus = n.kind == node_kind::milestone ? 600
                              : n.kind == node_kind::major     ? 0
                                                                : -100; // minors trail their major
        const int term = node_term[i] == scorer_term::known
            ? (((r.known_mask >> i) & 1ULL) != 0 ? 1000 : 0)   // a met polity holds THIS node
            : term_value[static_cast<int>(node_term[i])];
        const int cost_q = (n.kind == node_kind::minor ? 1 : n.kind == node_kind::major ? 3 : 6)
                          * static_cast<int>(n.ring);
        const int score = term + kind_bonus - cost_q;
        if (score > best_score) { best_score = score; best_idx = i; }
    }
    return best_idx;
}

bool industry_target_stands(uint64_t mask, int investing, const industry_scorer_reading& r)
{
    if (investing < 0 || investing >= io::industry_tree::node_count) return false;
    if (!industry_node_available(mask, investing)) return false; // a fork closed under it
    // The gate, on THIS round's reading: a seam lost since the pick closes a
    // `fuel` node exactly as it would have refused it at the pick.
    return industry_gate_open(io::industry_tree::nodes[investing].gate, r);
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

void decay_grudges(std::vector<grudge>& grudges, const history_sim_params& params, int step_years)
{
    if (params.grudge_decay_per_year_q <= 0 || grudges.empty()) return;
    const int shed    = clampi(params.grudge_decay_per_year_q * std::max(step_years, 1), 0, 1000);
    const int floor_q = std::max(0, params.grudge_floor);
    for (grudge& g : grudges)
    {
        // THE PROPORTIONAL RATE, WITH A FLOOR OF ONE (BL-842). The integer
        // product truncates to a ZERO decrement once score < 1000/shed -- below
        // 84 at generation's 4-year step, below 334 at a 1-year one -- and a
        // score that never moves never reaches `grudge_floor`, so every pair
        // that ever skirmished crossed the handoff as a permanent row. One unit
        // a round is the smallest decrement that is not zero; above the
        // truncation line it never binds, so the large-score half-life (~230
        // years) is unchanged. BELOW the line it is FASTER than the exponential
        // would be, and step-dependent (a round is a round, not a year):
        // history_sim_harness BL842 prints the measured half-lives.
        int dec = (g.score * shed) / 1000;
        if (dec < 1 && shed > 0 && g.score > floor_q) dec = 1;
        g.score -= dec;
    }
    grudges.erase(
        std::remove_if(grudges.begin(), grudges.end(),
                       [&](const grudge& g) { return g.score <= floor_q; }),
        grudges.end());
}

// ---------------------------------------------------------------------------
// Contact reads (BL-908)
// ---------------------------------------------------------------------------

bool has_contact(const history_sim_state& s, int from, int to)
{
    if (from < 0 || to < 0 || from > 0xFFFE || to > 0xFFFE) return false;
    const uint16_t f = static_cast<uint16_t>(from), t = static_cast<uint16_t>(to);
    const auto it = std::lower_bound(
        s.contacts.begin(), s.contacts.end(), std::pair<uint16_t, uint16_t>{f, t},
        [](const contact& c, const std::pair<uint16_t, uint16_t>& k) {
            if (c.from != k.first) return c.from < k.first;
            return c.to < k.second;
        });
    return it != s.contacts.end() && it->from == f && it->to == t;
}

int64_t contact_first_year(const history_sim_state& s, int from, int to)
{
    if (from < 0 || to < 0 || from > 0xFFFE || to > 0xFFFE) return INT64_MAX;
    const uint16_t f = static_cast<uint16_t>(from), t = static_cast<uint16_t>(to);
    const auto it = std::lower_bound(
        s.contacts.begin(), s.contacts.end(), std::pair<uint16_t, uint16_t>{f, t},
        [](const contact& c, const std::pair<uint16_t, uint16_t>& k) {
            if (c.from != k.first) return c.from < k.first;
            return c.to < k.second;
        });
    if (it != s.contacts.end() && it->from == f && it->to == t) return it->first.year;
    return INT64_MAX;
}

// ---------------------------------------------------------------------------
// Treaty reads and scoring (BL-933)
// ---------------------------------------------------------------------------

bool has_treaty_clause(const history_sim_state& s, int x, int y, treaty_clause clause)
{
    if (x < 0 || y < 0) return false;
    const uint8_t k = static_cast<uint8_t>(clause);
    const uint16_t xu = static_cast<uint16_t>(x), yu = static_cast<uint16_t>(y);
    if (clause == treaty_clause::tribute)
    {
        // Directed: a == subject, b == overlord. The caller passes the
        // subject as x for "does x owe y tribute".
        for (const dated_object& o : s.dated_objects)
            if (o.kind == k && o.a == xu && o.b == yu) return true;
        return false;
    }
    // The four mutual clauses are stored canonically (a < b) but a reader
    // asks in either order — same convention `contact` borrows from `grudge`
    // for a mutual fact stored as a directed pair, restated here for a pair
    // stored in canonical order instead of two independent rows.
    for (const dated_object& o : s.dated_objects)
        if (o.kind == k
         && ((o.a == xu && o.b == yu) || (o.a == yu && o.b == xu)))
            return true;
    return false;
}

int treaty_value_q(const history_sim_params& p,
                    int grudge_against_other_q, int grudge_from_other_q,
                    int counterpart_treaties_broken, int decider_aggression_q,
                    int alarm_from_other_q, bool near_home,
                    int trade_value_q)
{
    // BASE: a treaty is worth more the less either side already resents the
    // other -- a biting mutual history makes a promise of peace both less
    // needed as a distinct commitment (an active war reads as a war, not a
    // broken treaty) and less credible from a partner who has raised the
    // grudge in the first place. Both directions of `grudge_between` count,
    // read both ways per EXPLORATION.md sec What a treaty is.
    int value = 1000 - clampi((grudge_against_other_q + grudge_from_other_q) / 4, 0, 900);

    // COST (CEILING): freedom given up, priced by the decider's OWN doctrine.
    // A highly aggressive culture prices a non-aggression clause's lost
    // freedom higher, so the same peace-value clears a lower net score for it
    // -- the doctrine is read, never a term inside the actor deciding FOR it
    // (the aggression lean is a recorded fact about the culture, same as
    // everywhere else this file reads `aggression_q`).
    value -= clampi(decider_aggression_q, 0, 1000) / 4;

    // DISTRUST: "the cost lands on every OTHER party's willingness to bind
    // with the defector" -- read the COUNTERPART's own broken-treaty count,
    // never the decider's.
    value -= counterpart_treaties_broken * clampi(p.treaty_defector_distrust_q, 0, 1000) / 100;

    // BL-941 -- ALARM, GATED ON PROXIMITY. Near home, the counterpart's own
    // visible capability raises the value of a promise of peace with it --
    // "a neighbour that reads high visible capability should be MORE likely
    // to form/maintain a non-aggression treaty." Far from home the same
    // fleet meets no deterrent (EXPLORATION.md sec The arms race...), so
    // Alarm earns nothing there and a flat penalty applies instead: a pair
    // that has only just met should not bind as readily as a long-known
    // neighbour, which is the direct fix for NR-851's "every contacted pair
    // scored identically" finding.
    if (near_home)
        value += clampi(alarm_from_other_q, 0, 1000)
               * clampi(p.deterrence_alarm_weight_q, 0, 1000) / 1000;
    else
        value -= clampi(p.treaty_far_penalty_q, 0, 1000);

    // BL-954 -- TRADE, NEAR AND FAR ALIKE. The flow the pair's trade-access
    // clause would open (both directions, every good) is part of what the
    // binding is worth: "only trade can make a stranger worth a promise."
    value += static_cast<int>(
        (static_cast<int64_t>(clampi(trade_value_q, 0, 8000))
         * clampi(p.treaty_trade_weight_q, 0, 1000)) / 1000);

    return clampi(value, 0, 1000);
}

int visible_capability_q(const std::vector<region>& regions, const history_sim_state& s,
                          const history_sim_params& p, int polity_id)
{
    if (polity_id < 0 || polity_id >= static_cast<int>(s.polities.size())) return 0;
    const polity& q = s.polities[static_cast<std::size_t>(polity_id)];
    if (!q.alive || q.capital < 0 || static_cast<std::size_t>(q.capital) >= regions.size())
        return 0;
    const int64_t capability = regions[static_cast<std::size_t>(q.capital)].army_stock + q.navy_stock;
    return static_cast<int>(clampi64((capability * 1000) / std::max<int64_t>(1, p.visible_capability_reference),
                                      0, 1000));
}

// BL-1018: trace-only. Mirrors `visible_capability_q`'s numerator exactly and
// is called by no decision -- the function above is deliberately left as it
// was rather than routed through this one.
int64_t visible_capability_raw(const std::vector<region>& regions, const history_sim_state& s,
                               int polity_id)
{
    if (polity_id < 0 || polity_id >= static_cast<int>(s.polities.size())) return 0;
    const polity& q = s.polities[static_cast<std::size_t>(polity_id)];
    if (!q.alive || q.capital < 0 || static_cast<std::size_t>(q.capital) >= regions.size())
        return 0;
    return regions[static_cast<std::size_t>(q.capital)].army_stock + q.navy_stock;
}

int deterrence_alarm_q(const std::vector<region>& regions, const history_sim_state& s,
                        const history_sim_params& p, int self, int other)
{
    if (self < 0 || other < 0 || self == other) return 0;
    if (!has_contact(s, self, other)) return 0; // the omniscience guard: unmet is unseen.
    return visible_capability_q(regions, s, p, other);
}

// ---------------------------------------------------------------------------
// The directed want table (BL-909)
// ---------------------------------------------------------------------------

namespace
{
    /// Same binary search as `has_contact`, but over a bare table rather than
    /// `history_sim_state`, since `derive_wants` runs at handoff time against
    /// the already-copied `pass_one_output::contacts`.
    bool contact_between(const std::vector<contact>& contacts, uint16_t from, uint16_t to)
    {
        const auto it = std::lower_bound(
            contacts.begin(), contacts.end(), std::pair<uint16_t, uint16_t>{from, to},
            [](const contact& c, const std::pair<uint16_t, uint16_t>& k) {
                if (c.from != k.first) return c.from < k.first;
                return c.to < k.second;
            });
        return it != contacts.end() && it->from == from && it->to == to;
    }
} // namespace

std::vector<want> derive_wants(const std::vector<region>& regions,
                                const std::vector<contact>& contacts,
                                const std::vector<polity>&  polities)
{
    // THE FOUR WINDOWS, in `region_class` order — the same world-relative
    // dominance floor `classify` already applies to `region::dominant`
    // (settlement.cpp), so this table invents no new threshold of its own.
    constexpr int             good_count = 4;
    const region_class goods[good_count] =
        { region_class::farm, region_class::ore, region_class::energy, region_class::port };

    // Per-polity, per-good: does the polity hold at least one region DOMINANT
    // in that good, and does at least one of those regions also carry a
    // market (BL-910)? Both are booleans over already-computed per-region
    // facts — no magnitude, no price.
    std::vector<std::array<bool, good_count>> holds(polities.size());
    std::vector<std::array<bool, good_count>> shows_market(polities.size());
    for (auto& row : holds)        row.fill(false);
    for (auto& row : shows_market) row.fill(false);

    for (const region& r : regions)
    {
        if (r.nation < 0 || static_cast<std::size_t>(r.nation) >= polities.size()) continue;
        const std::size_t n = static_cast<std::size_t>(r.nation);
        for (int g = 0; g < good_count; ++g)
        {
            if (r.dominant != goods[static_cast<std::size_t>(g)]) continue;
            holds.at(n).at(static_cast<std::size_t>(g)) = true;
            if (r.has_market) shows_market.at(n).at(static_cast<std::size_t>(g)) = true;
        }
    }

    // A WANT REQUIRES CONTACT (CIVILISATION.md sec The directed want): walked
    // only over living polities, gated on `contact_between`, so a pair that
    // never met contributes nothing — the omniscience guard the design is
    // explicit about.
    //
    // Produced already in ascending (from, to, good) order: the outer two
    // walks are ascending polity id, the inner walk is the fixed `goods`
    // array order, so no separate sort is needed.
    std::vector<want> out;
    for (std::size_t a = 0; a < polities.size(); ++a)
    {
        if (!polities[a].alive) continue;
        for (std::size_t b = 0; b < polities.size(); ++b)
        {
            if (a == b || !polities[b].alive) continue;
            if (!contact_between(contacts, static_cast<uint16_t>(a), static_cast<uint16_t>(b)))
                continue;
            for (int g = 0; g < good_count; ++g)
            {
                const std::size_t gi = static_cast<std::size_t>(g);
                if (!holds.at(b).at(gi) || holds.at(a).at(gi)) continue; // B lacks it, or A already has it.
                want w;
                w.from       = static_cast<uint16_t>(a);
                w.to         = static_cast<uint16_t>(b);
                w.good       = goods[gi];
                w.via_market = shows_market.at(b).at(gi);
                out.push_back(w);
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Cultural good preference (BL-936)
// ---------------------------------------------------------------------------

std::vector<culture_good_preference> derive_culture_preference(
    const std::vector<region>& regions, const std::vector<contact>& contacts,
    const std::vector<polity>& polities, int culture_count)
{
    // THE SAME FOUR WINDOWS `derive_wants` reads, in the same order — no new
    // threshold invented for a culture-grain reading of the same facts.
    constexpr int good_count = 4;
    const region_class goods[good_count] =
        { region_class::farm, region_class::ore, region_class::energy, region_class::port };

    if (culture_count <= 0) return {};

    // CULTURE-GRAIN HOLDS: does at least one region where this culture is the
    // PLURALITY (`region::culture.id[0]`) sit dominant in this good? The same
    // "ground never held" reading `derive_wants` takes at polity grain,
    // regrouped by the culture-share table CIVILISATION.md's mixing already
    // maintains rather than by ownership — a preference is a fact about who
    // LIVES there, not who rules it (EXPLORATION.md sec A good acquires a
    // cultural preference).
    std::vector<std::array<bool, good_count>> holds(static_cast<std::size_t>(culture_count));
    for (auto& row : holds) row.fill(false);

    // POLITY-GRAIN HOLDS: reused for the exposure walk below, same shape
    // `derive_wants` builds for itself (kept separate rather than shared,
    // since the two functions are pure and independent by design).
    std::vector<std::array<bool, good_count>> polity_holds(polities.size());
    for (auto& row : polity_holds) row.fill(false);

    // WHICH POLITIES A CULTURE HAS A PLURALITY STAKE IN — a culture
    // straddling several realms counts each once. Built as a sorted set so
    // the exposure walk below is a property of the integers in it, not of
    // region iteration order.
    std::vector<std::vector<int>> culture_polities(static_cast<std::size_t>(culture_count));

    for (const region& r : regions)
    {
        const int pc = r.culture.id[0];
        const bool has_pc = pc >= 0 && static_cast<std::size_t>(pc) < holds.size();
        if (r.nation >= 0 && static_cast<std::size_t>(r.nation) < polity_holds.size())
            for (int g = 0; g < good_count; ++g)
                if (r.dominant == goods[g])
                    polity_holds[static_cast<std::size_t>(r.nation)][static_cast<std::size_t>(g)] = true;
        if (!has_pc) continue;
        for (int g = 0; g < good_count; ++g)
            if (r.dominant == goods[g])
                holds[static_cast<std::size_t>(pc)][static_cast<std::size_t>(g)] = true;
        if (r.nation >= 0)
        {
            auto& v = culture_polities[static_cast<std::size_t>(pc)];
            if (std::find(v.begin(), v.end(), r.nation) == v.end()) v.push_back(r.nation);
        }
    }
    for (auto& v : culture_polities) std::sort(v.begin(), v.end());

    // ROUTE EXPOSURE: for a good this culture lacks, how many DISTINCT
    // foreign polities has any of its own plurality-holding realms met that
    // themselves hold the good? "What its route exposed it to" — the same
    // contact-gated read `derive_wants` takes, tallied rather than stopped at
    // one bit, so the weight below can RANK rather than merely flag.
    //
    // Produced in ascending (culture, good) order: the outer walk is
    // ascending culture id, the inner walk the fixed `goods` array order, so
    // no separate sort is needed — same discipline `derive_wants` holds.
    std::vector<culture_good_preference> out;
    for (int c = 0; c < culture_count; ++c)
    {
        const auto& stake = culture_polities[static_cast<std::size_t>(c)];
        for (int g = 0; g < good_count; ++g)
        {
            const std::size_t gi = static_cast<std::size_t>(g);
            if (holds[static_cast<std::size_t>(c)][gi]) continue; // already holds it -- nothing to want.

            int exposure = 0;
            for (int from_polity : stake)
            {
                if (from_polity < 0 || !polities[static_cast<std::size_t>(from_polity)].alive) continue;
                for (std::size_t to = 0; to < polities.size(); ++to)
                {
                    if (static_cast<int>(to) == from_polity || !polities[to].alive) continue;
                    if (!polity_holds[to][gi]) continue;
                    if (!contact_between(contacts, static_cast<uint16_t>(from_polity),
                                          static_cast<uint16_t>(to))) continue;
                    ++exposure;
                }
            }
            if (exposure == 0) continue; // a known absence with no route yet -- not a preference.

            culture_good_preference p;
            p.culture  = static_cast<int16_t>(c);
            p.good     = goods[gi];
            // MAGNITUDE, NOT JUST A BIT: breadth of exposure is what "ranks
            // where a fleet goes first" (EXPLORATION.md sec A good acquires
            // a cultural preference). Capped at 4 contacted holders so one
            // culture that has met everyone does not swamp the scale a
            // sparser world reads on.
            p.weight_q = static_cast<int16_t>(clampi(exposure * 250, 0, 1000));
            out.push_back(p);
        }
    }
    return out;
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
    case grudge_kind::treaty_broken: what = "a treaty broken";   break;
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

namespace
{
    // THE PROVINCES EACH POLITY HOLDS — shared by both handoffs (BL-828,
    // BL-956) so the two cannot derive the political map two ways.
    std::vector<polity_holdings> derive_holdings(const std::vector<region>& regions,
                                                 const std::vector<polity>& polities)
    {
        std::vector<std::vector<int>> by_polity(polities.size());
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            const int n = regions[i].nation;
            if (n >= 0 && n < static_cast<int>(by_polity.size()))
                by_polity[static_cast<std::size_t>(n)].push_back(static_cast<int>(i));
        }
        std::vector<polity_holdings> out;
        for (std::size_t pi = 0; pi < polities.size(); ++pi)
        {
            if (by_polity[pi].empty()) continue; // A realm holding nothing crosses as nothing.
            polity_holdings h;
            h.polity  = static_cast<int>(pi);
            h.regions = by_polity[pi];
            out.push_back(std::move(h));
        }
        return out;
    }

    // THE SURVIVING-NETWORK RULE (BL-911), shared by both handoffs so the
    // Exploration filter is "exactly as the Empire handoff filters its own"
    // by construction rather than by a second copy of the test.
    bool corridor_region_survives(const std::vector<region>& regions,
                                  const std::vector<polity>& polities, uint16_t region_idx)
    {
        if (region_idx >= regions.size()) return false;
        const int n = regions[static_cast<std::size_t>(region_idx)].nation;
        if (n < 0 || n >= static_cast<int>(polities.size())) return false;
        return polities[static_cast<std::size_t>(n)].alive;
    }

    std::vector<history_corridor> filter_surviving_corridors(
        const std::vector<history_corridor>& corridors,
        const std::vector<region>& regions, const std::vector<polity>& polities)
    {
        std::vector<history_corridor> out;
        out.reserve(corridors.size());
        for (const history_corridor& c : corridors)
            if (corridor_region_survives(regions, polities, c.a)
             || corridor_region_survives(regions, polities, c.b))
                out.push_back(c);
        return out;
    }

    // --- The culture table across the handoff (BL-969) ---------------------
    //
    // `culture` carries no operator==, and adding one to creeds.hpp for a
    // check that lives here would widen a header with hundreds of includers.
    // Field-wise instead, EVERY member listed, so a field added to `culture`
    // that this omits is a silent hole in the equality proof -- keep this in
    // step with creeds.hpp's `culture` and `culture_god`.
    bool culture_god_equal(const culture_god& a, const culture_god& b)
    {
        return a.name == b.name && a.domain == b.domain && a.epithet == b.epithet
            && a.zeal == b.zeal && a.dominion == b.dominion;
    }

    bool culture_row_equal(const culture& a, const culture& b)
    {
        if (a.cradle != b.cradle || a.name != b.name) return false;
        if (a.speech.onsets != b.speech.onsets || a.speech.vowels != b.speech.vowels
            || a.speech.codas != b.speech.codas)
            return false;
        if (a.pantheon.size() != b.pantheon.size()) return false;
        for (std::size_t g = 0; g < a.pantheon.size(); ++g)
            if (!culture_god_equal(a.pantheon[g], b.pantheon[g])) return false;
        return a.aggression_q == b.aggression_q && a.sea_legs_q == b.sea_legs_q
            && a.parent == b.parent && a.origin_farm_class == b.origin_farm_class
            && a.coined_year == b.coined_year
            && a.coined_from == b.coined_from && a.folded_into == b.folded_into;
    }

    /// THE BOUNDARY FOLD'S SHAPE (BL-1017), on the table alone. Every culture
    /// the Colonisation round left without ground folded into an ancestor; this
    /// is the proof that the tree the Empires round walks is the one the ruling
    /// describes rather than one a reader must trust:
    ///
    ///  - A FOLDED CULTURE RECORDS ITS LINK: it names the lineage it was coined
    ///    from, its `parent` is the name that absorbed it, that name is itself
    ///    living, and it is an ancestor on the lineage — never a stranger.
    ///  - NO ORPHAN: a living culture's `parent` never names a folded culture,
    ///    and re-parenting only ever moves UP the lineage.
    ///
    /// A hand-built table with no lineage recorded (`coined_from` -1, nothing
    /// folded) passes trivially, as every fixture predating the fold must.
    bool culture_fold_valid(const std::vector<culture>& table, std::string* why)
    {
        const auto fail = [&](const std::string& msg) {
            if (why) *why = msg;
            return false;
        };
        const int n = static_cast<int>(table.size());
        // Is @p anc on @p from's lineage (inclusive)? Bounded: `coined_from` is
        // range-checked below for every row before any later row walks it.
        const auto on_lineage = [&](int from, int anc) {
            for (int at = from, guard = 0; at >= 0 && guard <= n; ++guard)
            {
                if (at == anc) return true;
                at = table[static_cast<std::size_t>(at)].coined_from;
            }
            return false;
        };
        for (int i = 0; i < n; ++i)
        {
            const culture& c = table[static_cast<std::size_t>(i)];
            const std::string id = "culture " + std::to_string(i);
            if (c.coined_from < -1 || c.coined_from >= i)
                return fail(id + " names lineage " + std::to_string(c.coined_from)
                            + " out of range (must be -1 or below " + std::to_string(i) + ")");
            if (c.folded_into < -1 || c.folded_into >= i)
                return fail(id + " folded into " + std::to_string(c.folded_into)
                            + ", out of range (must be -1 or below " + std::to_string(i) + ")");
            if (c.folded_into >= 0)
            {
                if (c.coined_from < 0)
                    return fail(id + " folded but records no lineage it was coined from");
                if (c.parent != c.folded_into)
                    return fail(id + " folded into " + std::to_string(c.folded_into)
                                + " but its parent names " + std::to_string(c.parent));
                if (table[static_cast<std::size_t>(c.folded_into)].folded_into >= 0)
                    return fail(id + " folded into " + std::to_string(c.folded_into)
                                + ", which folded too");
                if (!on_lineage(c.coined_from, c.folded_into))
                    return fail(id + " folded into " + std::to_string(c.folded_into)
                                + ", which is not on its lineage");
                continue;
            }
            if (c.parent >= 0 && c.parent < i
                && table[static_cast<std::size_t>(c.parent)].folded_into >= 0)
                return fail(id + " is ORPHANED: its parent " + std::to_string(c.parent)
                            + " folded at the boundary and it was not re-parented");
            if (c.parent >= 0 && c.coined_from >= 0 && !on_lineage(c.coined_from, c.parent))
                return fail(id + " was re-parented onto " + std::to_string(c.parent)
                            + ", which is not on its lineage");
        }
        return true;
    }

    /// NOTHING THE HANDOFF CARRIES NAMES A FOLDED CULTURE (BL-1017). The fold
    /// keeps ids stable on the strength of this: a culture folds only where no
    /// record names it, and the rounds after it only ever write a culture that
    /// some region already carried. A region share, a founding culture or a
    /// polity naming a folded culture would be a reader walking an empty name.
    bool folded_cultures_unnamed(const std::vector<culture>& table,
                                 const std::vector<region>&  regions,
                                 const std::vector<polity>&  polities, std::string* why)
    {
        const auto fail = [&](const std::string& msg) {
            if (why) *why = msg;
            return false;
        };
        const auto folded = [&](int c) {
            return c >= 0 && static_cast<std::size_t>(c) < table.size()
                && table[static_cast<std::size_t>(c)].folded_into >= 0;
        };
        for (std::size_t i = 0; i < regions.size(); ++i)
        {
            const region& r = regions[i];
            for (int k = 0; k < culture_share_slots; ++k)
                if (r.culture.weight_q[k] > 0 && folded(r.culture.id[k]))
                    return fail("region " + std::to_string(i) + " carries a share of folded culture "
                                + std::to_string(r.culture.id[k]));
            if (folded(r.founding_culture))
                return fail("region " + std::to_string(i) + " was founded by folded culture "
                            + std::to_string(r.founding_culture));
        }
        for (std::size_t p = 0; p < polities.size(); ++p)
            if (folded(polities[p].culture))
                return fail("polity " + std::to_string(p) + " is a people of folded culture "
                            + std::to_string(polities[p].culture));
        return true;
    }

    /// The culture-table half of both validators, shared so the two handoffs
    /// hold the table to one rule. @p live is the `creed_state` the fold read
    /// from, or null when the caller has none (a harness re-validating a
    /// captured value); the equality check runs only when it is given, and
    /// that check is the one that makes the doc's claim true.
    bool culture_table_valid(const std::vector<culture>& table, int culture_count,
                             int64_t stop_year, const creed_state* live, std::string* why)
    {
        const auto fail = [&](const std::string& msg) {
            if (why) *why = msg;
            return false;
        };
        if (static_cast<int>(table.size()) != culture_count)
            return fail("the culture table holds " + std::to_string(table.size())
                        + " rows but culture_count says " + std::to_string(culture_count));
        for (std::size_t i = 0; i < table.size(); ++i)
        {
            const culture& c = table[i];
            // A daughter's parent is ALWAYS at a lower index (creeds.hpp,
            // BL-865) -- ids are handed out in arrival order -- so the walk
            // toward the root strictly decreases. -1 is a cradle culture.
            if (c.parent < -1 || c.parent >= static_cast<int>(i))
                return fail("culture " + std::to_string(i) + " names parent "
                            + std::to_string(c.parent) + " out of range (must be -1 or below "
                            + std::to_string(i) + ")");
            // Coined within the span or earlier. INT64_MIN is "unknown" and
            // passes trivially, as it must -- a fixture culture carries no
            // year and is not thereby malformed.
            if (c.coined_year > stop_year)
                return fail("culture " + std::to_string(i) + " was coined in "
                            + std::to_string(c.coined_year) + ", after the close at "
                            + std::to_string(stop_year));
        }
        if (live != nullptr)
        {
            if (live->cultures.size() != table.size())
                return fail("the culture table copy holds " + std::to_string(table.size())
                            + " rows but the live creed_state holds "
                            + std::to_string(live->cultures.size()));
            for (std::size_t i = 0; i < table.size(); ++i)
                if (!culture_row_equal(table[i], live->cultures[i]))
                    return fail("culture " + std::to_string(i)
                                + " in the handoff copy differs from the live creed_state row");
        }
        return true;
    }
} // namespace

pass_one_output make_pass_one_output(const settlement_state&  ss,
                                     const history_sim_state& hs,
                                     const creed_state*       cs)
{
    pass_one_output o;
    o.regions             = ss.regions;
    o.polities            = hs.polities;
    // BL-969: the count and the table from ONE source, so they cannot
    // disagree; the validator still checks that they do not, because a
    // hand-built value can.
    o.culture_count       = cs != nullptr ? static_cast<int>(cs->cultures.size()) : 0;
    if (cs != nullptr) o.cultures = cs->cultures;
    o.works_by_band       = hs.works_by_band;
    o.grudges             = hs.grudges;
    o.contacts            = hs.contacts;

    // THE DIRECTED WANT TABLE (BL-909) — a pure derivation over the region
    // table and the contact table just copied above, computed here (rather
    // than kept as running sim state) because it needs nothing the sim
    // accumulates tick by tick.
    o.wants               = derive_wants(o.regions, o.contacts, o.polities);

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
    o.holdings = derive_holdings(o.regions, o.polities);

    // THE SURVIVING NETWORK CROSSES THE HANDOFF, UNEVENLY (BL-911). A corridor
    // (already sorted ascending by (a, b) at the source, BL-768) survives when
    // at least one endpoint region is held, at the epoch, by a polity `alive`
    // in `o.polities` — the same ownership read `by_polity` above just took,
    // so a segment on ground held by nobody living is dropped rather than
    // carried on the strength of the OTHER end alone... unless that other end
    // is itself held by a survivor, which is exactly the "at least one" test.
    o.surviving_corridors = filter_surviving_corridors(hs.supply_corridors, o.regions, o.polities);
    return o;
}

bool pass_one_output_valid(const pass_one_output& o, std::string* why,
                           const creed_state* live)
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

    // 4. Capitals and markets at the close (BL-910). A market never stands
    //    without a seat under it, and a market only ever stands ON a seat —
    //    never a hinterland region a polity happens to hold.
    for (std::size_t i = 0; i < o.regions.size(); ++i)
    {
        if (o.regions[i].has_market && !o.regions[i].is_seat)
            return fail("region " + std::to_string(i)
                        + " carries a market without a seat");
    }
    //    And NO QUOTA the other way either: every living polity that holds
    //    ground names a capital, that capital IS a seat, and that seat
    //    carries the market — nothing here manufactures one; it only checks
    //    that the closing block above did not skip one it owed.
    for (const polity_holdings& h : o.holdings)
    {
        const polity& q = o.polities[static_cast<std::size_t>(h.polity)];
        if (q.capital < 0 || q.capital >= static_cast<int>(o.regions.size()))
            return fail("polity " + std::to_string(h.polity)
                        + " holds ground but names no capital");
        const region& cap = o.regions[static_cast<std::size_t>(q.capital)];
        if (!cap.is_seat)
            return fail("polity " + std::to_string(h.polity)
                        + "'s capital is not a seat");
        if (!cap.has_market)
            return fail("polity " + std::to_string(h.polity)
                        + "'s capital carries no market at the close");
    }

    // 5. Contact (BL-908). Sorted, directed, in range, symmetric, and CARRYING
    //    THE EVENT that joined the pair — a fact with no cause is exactly the
    //    unsourced modifier the grudge table already refuses.
    std::pair<int, int> last_contact_key{-1, -1};
    for (const contact& c : o.contacts)
    {
        const std::pair<int, int> key{c.from, c.to};
        if (!(last_contact_key < key)) return fail("contacts are not sorted by (from, to)");
        last_contact_key = key;
        if (c.from == c.to) return fail("a polity is recorded in contact with itself");
        if (c.from >= o.polities.size() || c.to >= o.polities.size())
            return fail("a contact names a polity out of range");
        if (static_cast<int>(c.first.kind) < 0
            || static_cast<int>(c.first.kind) >= contact_kind_count)
            return fail("a contact names an out-of-range contact_kind");

        // SYMMETRIC BY CONSTRUCTION: meeting is mutual, so the reverse pair
        // must exist too, with the same first-contact year (the fact of
        // meeting has one date, not one per direction).
        bool found_reverse = false;
        for (const contact& r : o.contacts)
        {
            if (r.from != c.to || r.to != c.from) continue;
            found_reverse = true;
            if (r.first.year != c.first.year)
                return fail("a contact pair disagrees on its first-contact year");
            break;
        }
        if (!found_reverse) return fail("a contact is missing its reverse direction");
    }

    // 6. The directed want table (BL-909). Sorted, directed, in range, never
    //    a self-want, gated on contact, and only naming a `to` that actually
    //    holds the good — the checks the DONE WHEN clause asks for.
    std::vector<std::array<bool, 4>> holds(o.polities.size());
    for (auto& row : holds) row.fill(false);
    const region_class want_goods[4] =
        { region_class::farm, region_class::ore, region_class::energy, region_class::port };
    for (const region& r : o.regions)
    {
        if (r.nation < 0 || static_cast<std::size_t>(r.nation) >= o.polities.size()) continue;
        for (int g = 0; g < 4; ++g)
            if (r.dominant == want_goods[static_cast<std::size_t>(g)])
                holds.at(static_cast<std::size_t>(r.nation)).at(static_cast<std::size_t>(g)) = true;
    }
    std::pair<int, int> last_want_pair{-1, -1};
    for (const want& w : o.wants)
    {
        if (w.from == w.to) return fail("a polity wants a good from itself");
        if (w.from >= o.polities.size() || w.to >= o.polities.size())
            return fail("a want names a polity out of range");
        if (w.good == region_class::none)
            return fail("a want names no good");

        const std::pair<int, int> pair_key{w.from, w.to};
        if (pair_key < last_want_pair)
            return fail("wants are not sorted by (from, to)");
        last_want_pair = pair_key;

        if (!contact_between(o.contacts, w.from, w.to))
            return fail("a want crosses a pair with no contact");

        int gi = -1;
        for (int g = 0; g < 4; ++g)
            if (want_goods[static_cast<std::size_t>(g)] == w.good) gi = g;
        if (gi < 0 || !holds.at(w.to).at(static_cast<std::size_t>(gi)))
            return fail("a want names a holder that does not hold the good");
        if (holds.at(w.from).at(static_cast<std::size_t>(gi)))
            return fail("a want names a good the wanting polity already holds");
    }

    // 7. The culture table (BL-969): sized to `culture_count`, parents in
    //    range and below their child, coined at or before the close, and --
    //    when the live table is given -- equal to it row for row.
    if (!culture_table_valid(o.cultures, o.culture_count, o.stop_year, live, why))
        return false;

    // 8. The boundary fold (BL-1017): folded cultures record their link, no
    //    living culture is orphaned, and nothing carried names a folded one.
    if (!culture_fold_valid(o.cultures, why)) return false;
    if (!folded_cultures_unnamed(o.cultures, o.regions, o.polities, why)) return false;

    if (why) why->clear();
    return true;
}

// ---------------------------------------------------------------------------
// The Exploration -> Industrialisation handoff (BL-956)
// ---------------------------------------------------------------------------

exploration_output make_exploration_output(const settlement_state&  ss,
                                           const history_sim_state& hs,
                                           const creed_state*       cs)
{
    exploration_output o;
    o.regions       = ss.regions;
    o.polities      = hs.polities;
    // BL-969: count and table from one source, as `make_pass_one_output`.
    const int culture_count = cs != nullptr ? static_cast<int>(cs->cultures.size()) : 0;
    o.culture_count = culture_count;
    if (cs != nullptr) o.cultures = cs->cultures;
    o.contacts      = hs.contacts;
    o.grudges       = hs.grudges;
    o.start_year    = hs.start_year;
    o.stop_year     = hs.start_year + hs.years;

    // STANDING TREATIES AT THE CLOSE. The sim expires objects at the top of
    // each decision round, so a term that ran out between the last round and
    // `stop_year` can still sit in `hs.dated_objects`; the same expiry rule,
    // applied at `stop_year`, is what "standing at 1660" means. Then sorted,
    // because the sim's own vector is in insertion order and a handoff's
    // order must be a property of its integers.
    o.dated_objects = hs.dated_objects;
    expire_dated_objects(o.dated_objects, o.stop_year);
    std::sort(o.dated_objects.begin(), o.dated_objects.end(),
              [](const dated_object& x, const dated_object& y) {
                  if (x.a != y.a) return x.a < y.a;
                  if (x.b != y.b) return x.b < y.b;
                  if (x.kind != y.kind) return x.kind < y.kind;
                  return x.expires_year < y.expires_year;
              });

    // TRADE FLOWS AT THE CLOSE: the span's final decision round's flows, kept
    // only where they still stand at `stop_year` -- the pair still holds a
    // trade_access clause among the standing objects just folded (a clause
    // that expired between the last round and the close takes its flows with
    // it), and both parties are still alive (a polity conquered after the
    // last round's upkeep cannot be trading at the close). Order-preserving:
    // the sim's vector is already sorted by (seller, buyer, good).
    o.trade_flows = hs.trade_flows;
    prune_flows_without_trade_access(o.trade_flows, o.dated_objects);
    o.trade_flows.erase(
        std::remove_if(o.trade_flows.begin(), o.trade_flows.end(),
            [&](const trade_flow& f) {
                return f.seller >= o.polities.size() || f.buyer >= o.polities.size()
                    || !o.polities[f.seller].alive || !o.polities[f.buyer].alive;
            }),
        o.trade_flows.end());

    // Both derived tables are pure folds over the 1660 state just copied —
    // the same derivations the Empire handoff and the sim itself use.
    o.wants              = derive_wants(o.regions, o.contacts, o.polities);
    o.culture_preference = derive_culture_preference(o.regions, o.contacts, o.polities,
                                                     culture_count);

    o.holdings            = derive_holdings(o.regions, o.polities);
    o.surviving_corridors = filter_surviving_corridors(hs.supply_corridors, o.regions, o.polities);

    // BL-1097: the sea-leg record crosses WHOLE -- no dead filter, because a
    // lane outlives the polity that made it (see the field). Already sorted
    // and summed by the run's close fold.
    o.sea_legs = hs.sea_legs;

    // BL-1096: the purchase fork's two counters, copied not re-derived.
    o.provinces_bought            = hs.provinces_bought;
    o.treasury_spent_on_purchases = hs.treasury_spent_on_purchases;

    // BL-1036: the records the carried civilisation and creed indices point
    // into, copied whole and in index order so a resumed span continues the
    // numbering rather than restarting it (see the field comment for the gap).
    o.civilisations    = hs.civilisations;
    o.universal_creeds = hs.universal_creeds;
    return o;
}

bool exploration_output_valid(const exploration_output& o, std::string* why,
                              const creed_state* live)
{
    const auto fail = [&](const std::string& msg) {
        if (why) *why = msg;
        return false;
    };
    const int np = static_cast<int>(o.polities.size());
    const int nr = static_cast<int>(o.regions.size());

    // 1. The region table: shares sum to 1000 and name cultures in range, and
    //    every owner id is a real polity or none.
    for (int i = 0; i < nr; ++i)
    {
        const region& r = o.regions[static_cast<std::size_t>(i)];
        if (r.culture.total_q() != 1000)
            return fail("region " + std::to_string(i) + " culture shares sum to "
                        + std::to_string(r.culture.total_q()) + ", not 1000");
        for (int k = 0; k < culture_share_slots; ++k)
            if (o.culture_count > 0 && r.culture.id[k] >= o.culture_count)
                return fail("region " + std::to_string(i) + " names culture "
                            + std::to_string(r.culture.id[k]) + " out of range");
        if (r.nation < -1 || r.nation >= np)
            return fail("region " + std::to_string(i) + " is owned by polity "
                        + std::to_string(r.nation) + " out of range");
        if (r.treasury < 0)
            return fail("region " + std::to_string(i) + " carries a negative treasury");
        if (r.port_stock_q < 0 || r.port_stock_q > 1000)
            return fail("region " + std::to_string(i) + " carries a port_stock_q off the 0-1000 scale");
        if (r.army_stock < 0)
            return fail("region " + std::to_string(i) + " carries a negative army_stock");
    }

    // 2. The overlord graph. An overlord is a real polity and never the
    //    subject itself; `subject_kind` is set exactly when an overlord is.
    for (int i = 0; i < np; ++i)
    {
        const polity& q = o.polities[static_cast<std::size_t>(i)];
        if (q.overlord < -1 || q.overlord >= np)
            return fail("polity " + std::to_string(i) + " names overlord "
                        + std::to_string(q.overlord) + " out of range");
        if (q.overlord == i)
            return fail("polity " + std::to_string(i) + " is its own overlord");
        if (q.subject_kind < -1 || q.subject_kind > 1)
            return fail("polity " + std::to_string(i) + " carries an out-of-range subject_kind");
        if ((q.overlord < 0) != (q.subject_kind < 0))
            return fail("polity " + std::to_string(i)
                        + " has an overlord and a subject_kind that disagree");
        if (q.navy_stock < 0)
            return fail("polity " + std::to_string(i) + " carries a negative navy_stock");
    }

    // 3. The holdings. Ascending, in range, held by the living, each region
    //    held once and matching its own `nation` — and every owned region
    //    held, so the set and the map are the same map.
    std::vector<char> claimed(o.regions.size(), 0);
    int last_polity = -1;
    for (const polity_holdings& h : o.holdings)
    {
        if (h.polity <= last_polity)
            return fail("holdings are not in ascending polity order");
        last_polity = h.polity;
        if (h.polity < 0 || h.polity >= np)
            return fail("holdings name polity " + std::to_string(h.polity) + " out of range");
        if (!o.polities[static_cast<std::size_t>(h.polity)].alive)
            return fail("polity " + std::to_string(h.polity) + " holds ground but is not alive");
        int last_region = -1;
        for (int r : h.regions)
        {
            if (r <= last_region) return fail("holdings are not in ascending region order");
            last_region = r;
            if (r < 0 || r >= nr)
                return fail("holdings name region " + std::to_string(r) + " out of range");
            if (claimed[static_cast<std::size_t>(r)])
                return fail("region " + std::to_string(r) + " is held twice");
            claimed[static_cast<std::size_t>(r)] = 1;
            if (o.regions[static_cast<std::size_t>(r)].nation != h.polity)
                return fail("region " + std::to_string(r) + " holdings disagree with its nation");
        }
    }
    for (int i = 0; i < nr; ++i)
        if (o.regions[static_cast<std::size_t>(i)].nation >= 0 && !claimed[static_cast<std::size_t>(i)])
            return fail("region " + std::to_string(i) + " is owned but appears in no holding");

    // 4. The grudges. Sorted, directed, in range, carrying their cause.
    std::pair<int, int> last_grudge{-1, -1};
    for (const grudge& g : o.grudges)
    {
        const std::pair<int, int> key{g.from, g.to};
        if (!(last_grudge < key)) return fail("grudges are not sorted by (from, to)");
        last_grudge = key;
        if (g.from == g.to) return fail("a polity holds a grudge against itself");
        if (g.from >= o.polities.size() || g.to >= o.polities.size())
            return fail("a grudge names a polity out of range");
        if (g.score < 0 || g.peak < g.score)
            return fail("a grudge's peak is below its standing score");
        if (g.event_count <= 0 || g.events_kept <= 0)
            return fail("a grudge carries a score with no cause");
    }

    // 5. The contacts. Sorted, directed, in range, never a self-pair.
    std::pair<int, int> last_contact{-1, -1};
    for (const contact& c : o.contacts)
    {
        const std::pair<int, int> key{c.from, c.to};
        if (!(last_contact < key)) return fail("contacts are not sorted by (from, to)");
        last_contact = key;
        if (c.from == c.to) return fail("a polity is recorded in contact with itself");
        if (c.from >= o.polities.size() || c.to >= o.polities.size())
            return fail("a contact names a polity out of range");
    }

    // 6. The wants. Sorted by pair, in range, never a self-want, gated on
    //    contact.
    std::pair<int, int> last_want{-1, -1};
    for (const want& w : o.wants)
    {
        if (w.from == w.to) return fail("a polity wants a good from itself");
        if (w.from >= o.polities.size() || w.to >= o.polities.size())
            return fail("a want names a polity out of range");
        if (w.good == region_class::none) return fail("a want names no good");
        const std::pair<int, int> key{w.from, w.to};
        if (key < last_want) return fail("wants are not sorted by (from, to)");
        last_want = key;
        if (!contact_between(o.contacts, w.from, w.to))
            return fail("a want crosses a pair with no contact");
    }

    // 7. Cultural good preference. Strictly ascending (culture, good index),
    //    in range, weight on the 0-1000 scale.
    std::pair<int, int> last_pref{-1, -1};
    for (const culture_good_preference& p : o.culture_preference)
    {
        const int gi = scarcity_good_index(p.good);
        if (gi < 0) return fail("a culture preference names no good");
        if (p.culture < 0 || (o.culture_count > 0 && p.culture >= o.culture_count))
            return fail("a culture preference names a culture out of range");
        const std::pair<int, int> key{p.culture, gi};
        if (!(last_pref < key)) return fail("culture preferences are not sorted by (culture, good)");
        last_pref = key;
        if (p.weight_q < 0 || p.weight_q > 1000)
            return fail("a culture preference weight is off the 0-1000 scale");
    }

    // 8. Standing treaties and tribute. Sorted, in range, never a self-pair,
    //    a known clause, mutual clauses in canonical (a < b) order, and still
    //    inside their term at the close — a remaining term is never <= 0.
    for (std::size_t i = 0; i < o.dated_objects.size(); ++i)
    {
        const dated_object& d = o.dated_objects[i];
        if (i > 0)
        {
            const dated_object& p = o.dated_objects[i - 1];
            const bool out_of_order =
                  d.a != p.a ? d.a < p.a
                : d.b != p.b ? d.b < p.b
                : d.kind != p.kind ? d.kind < p.kind
                : d.expires_year < p.expires_year;
            if (out_of_order)
                return fail("dated objects are not sorted by (a, b, kind, expires_year)");
        }
        if (d.a < 0 || d.a >= np || d.b < 0 || d.b >= np)
            return fail("a dated object names a polity out of range");
        if (d.a == d.b) return fail("a dated object binds a polity to itself");
        if (d.kind < 0 || d.kind >= treaty_clause_count)
            return fail("a dated object names an out-of-range treaty clause");
        if (d.kind != static_cast<int32_t>(treaty_clause::tribute) && d.a > d.b)
            return fail("a mutual treaty clause is not in canonical (a < b) order");
        if (d.expires_year <= o.stop_year)
            return fail("a dated object's term has already ended at the close");
    }

    // 9. The surviving network. Sorted, endpoints in range and distinct, and
    //    every corridor with a living holder at one end.
    std::pair<int, int> last_corridor{-1, -1};
    for (const history_corridor& c : o.surviving_corridors)
    {
        const std::pair<int, int> key{c.a, c.b};
        if (!(last_corridor < key)) return fail("surviving corridors are not sorted by (a, b)");
        last_corridor = key;
        if (c.a == c.b) return fail("a surviving corridor joins a region to itself");
        if (c.a >= o.regions.size() || c.b >= o.regions.size())
            return fail("a surviving corridor names a region out of range");
        if (!corridor_region_survives(o.regions, o.polities, c.a)
         && !corridor_region_survives(o.regions, o.polities, c.b))
            return fail("a surviving corridor has no living holder at either end");
    }

    // 9b. The sea-leg record (BL-1097). Sorted, a < b, both ends in range,
    //     uses positive. No living-holder test: a lane outlives its maker.
    std::pair<int, int> last_leg{-1, -1};
    for (const sea_leg& l : o.sea_legs)
    {
        const std::pair<int, int> key{l.a, l.b};
        if (!(last_leg < key)) return fail("sea legs are not sorted by (a, b)");
        last_leg = key;
        if (l.a >= l.b) return fail("a sea leg is not in canonical (a < b) order");
        if (l.b >= o.regions.size()) return fail("a sea leg names a region out of range");
        if (l.uses <= 0) return fail("a sea leg carries no uses");
    }

    // 10. Trade flows. Strictly ascending (seller, buyer, good), both parties
    //     real, alive and distinct, a known good, a positive volume -- and
    //     every flow's pair holding a trade_access clause among THIS value's
    //     own standing objects, so no flow outlives the clause that opened it.
    {
        std::vector<std::pair<int32_t, int32_t>> bound;
        for (const dated_object& d : o.dated_objects)
            if (d.kind == static_cast<int32_t>(treaty_clause::trade_access))
                bound.push_back({std::min(d.a, d.b), std::max(d.a, d.b)});
        std::sort(bound.begin(), bound.end());

        for (std::size_t i = 0; i < o.trade_flows.size(); ++i)
        {
            const trade_flow& f = o.trade_flows[i];
            if (i > 0)
            {
                const trade_flow& p = o.trade_flows[i - 1];
                const bool ascending =
                      f.seller != p.seller ? f.seller > p.seller
                    : f.buyer != p.buyer   ? f.buyer > p.buyer
                    : f.good > p.good;
                if (!ascending)
                    return fail("trade flows are not strictly sorted by (seller, buyer, good)");
            }
            if (f.seller >= np || f.buyer >= np)
                return fail("a trade flow names a polity out of range");
            if (f.seller == f.buyer) return fail("a polity trades with itself");
            if (!o.polities[f.seller].alive || !o.polities[f.buyer].alive)
                return fail("a trade flow names a polity that is not alive");
            if (f.good >= 4) return fail("a trade flow names an out-of-range good");
            if (f.volume_q <= 0) return fail("a trade flow carries no volume");
            const int32_t s = f.seller, u = f.buyer;
            if (!std::binary_search(bound.begin(), bound.end(),
                                    std::pair<int32_t, int32_t>{std::min(s, u), std::max(s, u)}))
                return fail("a trade flow's pair holds no standing trade_access clause");
        }
    }

    if (o.start_year > o.stop_year) return fail("the span closes before it opens");

    // 11. The culture table (BL-969), held to the same rule as pass 1's.
    if (!culture_table_valid(o.cultures, o.culture_count, o.stop_year, live, why))
        return false;

    // 12. The boundary fold (BL-1017), held to the same rule as pass 1's.
    if (!culture_fold_valid(o.cultures, why)) return false;
    if (!folded_cultures_unnamed(o.cultures, o.regions, o.polities, why)) return false;

    if (why) why->clear();
    return true;
}

// ---------------------------------------------------------------------------
// The Industrialisation span's close (BL-1040)
// ---------------------------------------------------------------------------

industrialisation_output make_industrialisation_output(const settlement_state&  ss,
                                             const history_sim_state& hs,
                                             const creed_state*       cs)
{
    // ONE FOLD RULE FOR BOTH CLOSES. The standing-treaty expiry, the flow
    // prune, the dead-filter on corridors and the derived tables are what
    // "standing at the close" means for any span of this engine; restating
    // them here would be a second construction of the same value.
    industrialisation_output o;
    static_cast<exploration_output&>(o) = make_exploration_output(ss, hs, cs);
    return o;
}

campaign_band_reading derive_campaign_band(const std::vector<polity>& polities)
{
    campaign_band_reading r;
    for (const polity& q : polities)
    {
        if (!q.alive) continue;
        ++r.alive;
        // The SAME clamp and the SAME function the round's `mat_band` reads
        // (the works table and the furnace), so this cannot name a rung the
        // run could not build at.
        const int m = std::clamp(q.capacity[static_cast<int>(sim_domain::materials)], 1, 6);
        if (m > r.mat_cap_max) r.mat_cap_max = m;
        if (roster_band_for_capacity(m) == roster_band::industrial) ++r.at_rung;
        if (q.industrial_year != k_never_industrialised) ++r.ever_crossed;
    }
    r.band = r.at_rung > 0 ? era_band::industrial : era_band::ancient;
    return r;
}

bool industrialisation_output_valid(const industrialisation_output& o, std::string* why,
                               const creed_state*        live,
                               const exploration_output* from,
                               int64_t                   stop_year)
{
    // 1. Every rule the Exploration close is held to.
    if (!exploration_output_valid(o, why, live)) return false;

    const auto fail = [&](const std::string& msg) {
        if (why) *why = msg;
        return false;
    };

    // 2. The span ran: a close that opens and shuts on one year is a span
    //    that never played, and nothing downstream should read it as 1960.
    if (!(o.start_year < o.stop_year)) return fail("the span closes on the year it opens");

    // 2b. BL-1053: and it ran to the year it was asked for. `stop_year` on
    //     the fold is `start_year + years` -- the years the sim actually
    //     played -- so a span that ended early reads as a close short of the
    //     epoch here, rather than as a 1960 world world setup builds on.
    if (stop_year != INT64_MIN && o.stop_year != stop_year)
        return fail("the span closes in " + std::to_string(o.stop_year) + ", not on its stop year "
                    + std::to_string(stop_year));

    if (from != nullptr)
    {
        // 3. IT RESUMED FROM `from`, and continued it. The span opens on the
        //    year the value it resumed from closed on.
        if (o.start_year != from->stop_year)
            return fail("the span opens in " + std::to_string(o.start_year)
                        + ", not on its handoff's close " + std::to_string(from->stop_year));

        // 4. Nothing the resume carried is dropped or renumbered. Each table
        //    is append-only across a span -- a polity that dies keeps its row,
        //    a region the span founds is appended, a record coined in the span
        //    takes the next free index -- so the resumed prefix must still be
        //    there, in place.
        if (o.regions.size() < from->regions.size())
            return fail("the region table shrank across the span");
        for (std::size_t i = 0; i < from->regions.size(); ++i)
            if (o.regions[i].col != from->regions[i].col || o.regions[i].row != from->regions[i].row)
                return fail("region " + std::to_string(i) + " moved across the span");

        if (o.polities.size() < from->polities.size())
            return fail("the polity table shrank across the span");
        for (std::size_t i = 0; i < from->polities.size(); ++i)
            if (o.polities[i].id != from->polities[i].id)
                return fail("polity " + std::to_string(i) + " changed its id across the span");

        if (o.culture_count < from->culture_count)
            return fail("the culture table shrank across the span");

        // The records BL-1036 carries so a resumed span continues the
        // numbering: a span that restarted them would coin record 0 twice.
        if (o.civilisations.size() < from->civilisations.size())
            return fail("the civilisation table shrank across the span -- the resume restarted it");
        for (std::size_t i = 0; i < from->civilisations.size(); ++i)
            if (o.civilisations[i].name != from->civilisations[i].name
             || o.civilisations[i].formed_year != from->civilisations[i].formed_year)
                return fail("civilisation " + std::to_string(i) + " is not the record the span resumed");
        if (o.universal_creeds.size() < from->universal_creeds.size())
            return fail("the universal-creed table shrank across the span -- the resume restarted it");
        for (std::size_t i = 0; i < from->universal_creeds.size(); ++i)
            if (o.universal_creeds[i].name != from->universal_creeds[i].name
             || o.universal_creeds[i].founded_year != from->universal_creeds[i].founded_year)
                return fail("universal creed " + std::to_string(i) + " is not the record the span resumed");

        // 5. BL-1053: THE CONTACTS CONTINUE. A contact is dropped in one place
        //    only -- when one of its pair is extinguished -- and the event that
        //    joined a pair is never rewritten (`raise_contact` is a no-op on a
        //    pair that exists). So every contact the resume carried between
        //    two polities still alive at the close is still here, joined by
        //    the same event. `o.contacts` is strictly sorted by (from, to):
        //    rule 5 of `exploration_output_valid`, already passed above.
        const std::size_t npol = o.polities.size();
        for (const contact& c : from->contacts)
        {
            if (c.from >= npol || c.to >= npol) continue; // out of range on `from`: its own validator's call
            if (!o.polities[c.from].alive || !o.polities[c.to].alive) continue;
            const auto it = std::lower_bound(
                o.contacts.begin(), o.contacts.end(), c,
                [](const contact& x, const contact& y) {
                    return x.from != y.from ? x.from < y.from : x.to < y.to;
                });
            if (it == o.contacts.end() || it->from != c.from || it->to != c.to)
                return fail("the contact " + std::to_string(c.from) + " -> " + std::to_string(c.to)
                            + " the span resumed is gone, with both polities alive at the close");
            if (it->first.year != c.first.year || it->first.region != c.first.region
             || it->first.kind != c.first.kind)
                return fail("the contact " + std::to_string(c.from) + " -> " + std::to_string(c.to)
                            + " changed the event that first joined it");
        }

        // 6. BL-1053: THE STANDING OBJECTS CONTINUE. An object the resume
        //    carried whose term runs past the close leaves only by a cause
        //    the sim records: a treaty break between its pair (which erases
        //    every object the pair holds, and raises the defector's
        //    `treaties_broken`), or -- for tribute -- the subject freed from
        //    that overlord. Anything else missing was lost by the resume or
        //    the fold. `o.dated_objects` is sorted by (a, b, kind,
        //    expires_year): rule 8 of `exploration_output_valid`.
        const auto obj_less = [](const dated_object& x, const dated_object& y) {
            if (x.a != y.a) return x.a < y.a;
            if (x.b != y.b) return x.b < y.b;
            if (x.kind != y.kind) return x.kind < y.kind;
            return x.expires_year < y.expires_year;
        };
        const auto broke_during_span = [&](int32_t p) {
            return p >= 0 && static_cast<std::size_t>(p) < npol
                && static_cast<std::size_t>(p) < from->polities.size()
                && o.polities[static_cast<std::size_t>(p)].treaties_broken
                       > from->polities[static_cast<std::size_t>(p)].treaties_broken;
        };
        for (const dated_object& d : from->dated_objects)
        {
            if (d.expires_year <= o.stop_year) continue; // its term ran out inside the span
            if (std::binary_search(o.dated_objects.begin(), o.dated_objects.end(), d, obj_less))
                continue;
            if (broke_during_span(d.a) || broke_during_span(d.b)) continue;
            if (d.kind == static_cast<int32_t>(treaty_clause::tribute))
            {
                // Freed: the subject no longer answers to this overlord. Or
                // freed and taken again by the same one, which pays under a
                // NEW term -- the same pair's tribute with another expiry.
                if (d.a >= 0 && static_cast<std::size_t>(d.a) < npol
                 && o.polities[static_cast<std::size_t>(d.a)].overlord != d.b)
                    continue;
                bool retaken = false;
                for (const dated_object& e : o.dated_objects)
                    if (e.a == d.a && e.b == d.b && e.kind == d.kind) { retaken = true; break; }
                if (retaken) continue;
            }
            return fail("a dated object (kind " + std::to_string(d.kind) + ", " + std::to_string(d.a)
                        + " / " + std::to_string(d.b) + ", to " + std::to_string(d.expires_year)
                        + ") the span resumed is gone before its term, with no break or release recorded");
        }
    }

    if (why) why->clear();
    return true;
}
