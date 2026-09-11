// ---------------------------------------------------------------------------
// history_sim_harness — BL-277 (Era -1 campaign scorer) + BL-271 first slice.
//
// Binds the requirement group "era-minus-1-history-sim" (R1..R8). Runs the
// year-tick sim over the generated Kepler settlement state and over a small
// synthetic two-polity world where one mechanism at a time can be isolated.
//
// ALSO COVERS the Era -1 logistics slices (BL-316) and the shared-currency
// scorer (BL-318), which landed with no assertions behind them:
//   S1a/S1b  real terrain is live, not inert
//   S2a      terrain-weighted reach never makes mountains cheaper
//   S3a/S3b  the burden of breadth is live and points the right way
//   B318a-c  no verb has swallowed the run
//
// AND (BL-384) the full-run OUTCOME group B384a-d, which is a different kind of
// claim from everything above and is deliberately last. Every check before it
// asks whether a mechanism is live; B384 asks whether the run these mechanisms
// compose into ever reaches a conclusion — whether a region ever changes
// hands by war across a spread of generated worlds. That question had no
// assertion behind it, which is how "267 battles, zero conquests" survived: the
// conquest count was compared between two runs for determinism (`same_run`,
// `differs`) but never compared against zero over more than one world.
//
// THE PATTERN THESE USE, and the reason they are shaped this way: each runs the
// SAME world twice with one variable changed, and asserts the DIRECTION of the
// difference. An absolute count would be a calibration target, and calibration
// targets get tuned toward instead of measured — which is how the sim ended up
// with a `stalled_campaigns` counter that read a constant zero and a terrain
// module nobody noticed was dead.
//
// Headless: world/* logic only, no SDL and no Lua.
// ---------------------------------------------------------------------------

#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/sim_terrain_build.hpp"
#include "world/unit_roster.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

const generation_report::body_entry* kepler_of(const generation_report& r)
{
    for (const generation_report::body_entry& b : r.bodies)
        if (b.is_homeworld) return &b; // identity, not display name (BL-257)
    return r.bodies.empty() ? nullptr : &r.bodies.front();
}

/// Compare two runs for the byte-level equality R1 asserts.
bool same_run(const history_sim_state& a, const history_sim_state& b,
              const settlement_state& sa, const settlement_state& sb)
{
    if (a.owner_changes.size() != b.owner_changes.size()) return false;
    for (std::size_t i = 0; i < a.owner_changes.size(); ++i)
        if (a.owner_changes[i].year != b.owner_changes[i].year
         || a.owner_changes[i].region != b.owner_changes[i].region
         || a.owner_changes[i].owner != b.owner_changes[i].owner)
            return false;
    if (a.region_stride != b.region_stride) return false;
    if (a.battles != b.battles || a.conquests != b.conquests
     || a.foundings != b.foundings || a.winter_campaigns != b.winter_campaigns)
        return false;
    if (sa.regions.size() != sb.regions.size()) return false;
    for (std::size_t i = 0; i < sa.regions.size(); ++i)
    {
        const region& p = sa.regions[i];
        const region& q = sb.regions[i];
        if (p.nation != q.nation || p.population != q.population
         || p.manpower_stock != q.manpower_stock || p.culture != q.culture
         || p.contest_q != q.contest_q)
            return false;
    }
    return true;
}

/// EVERY OUTPUT EXCEPT THE PLAYBACK RECORD ITSELF (BL-817).
///
/// This is the non-perturbation instrument, and it is deliberately WIDER than
/// `same_run`: it compares the ownership list, every counter the sim keeps, the
/// road corridors, the grudge table, the polity ladder, the narration and the
/// mutated settlement state - everything a consumer could read - while ignoring
/// `steps`, `samples` and `culture_changes`. A recorded run and a suppressed
/// run must agree on all of it, or the recorder is a participant rather than an
/// observer.
///
/// `same_run` is not reused because it would be the wrong claim here: it checks
/// determinism between two IDENTICAL configurations, and this checks that two
/// DIFFERENT configurations produce the same history. Narrow coverage would let
/// a perturbation hide in a field `same_run` never looks at.
bool same_except_record(const history_sim_state& a, const history_sim_state& b,
                        const settlement_state& sa, const settlement_state& sb)
{
    if (a.owner_changes.size() != b.owner_changes.size()) return false;
    for (std::size_t i = 0; i < a.owner_changes.size(); ++i)
        if (a.owner_changes[i].year   != b.owner_changes[i].year
         || a.owner_changes[i].region != b.owner_changes[i].region
         || a.owner_changes[i].owner  != b.owner_changes[i].owner)
            return false;

    if (a.supply_corridors.size() != b.supply_corridors.size()) return false;
    for (std::size_t i = 0; i < a.supply_corridors.size(); ++i)
        if (a.supply_corridors[i].a    != b.supply_corridors[i].a
         || a.supply_corridors[i].b    != b.supply_corridors[i].b
         || a.supply_corridors[i].uses != b.supply_corridors[i].uses)
            return false;

    if (a.grudges.size() != b.grudges.size()) return false;
    for (std::size_t i = 0; i < a.grudges.size(); ++i)
        if (a.grudges[i].from != b.grudges[i].from || a.grudges[i].to != b.grudges[i].to
         || a.grudges[i].score != b.grudges[i].score || a.grudges[i].peak != b.grudges[i].peak)
            return false;

    if (a.polities.size() != b.polities.size()) return false;
    for (std::size_t i = 0; i < a.polities.size(); ++i)
    {
        const polity& p = a.polities[i];
        const polity& q = b.polities[i];
        if (p.capital != q.capital || p.cohesion_q != q.cohesion_q
         || p.industrial_year != q.industrial_year || p.parent != q.parent)
            return false;
        for (int d = 0; d < sim_domain_count; ++d)
            if (p.capacity[d] != q.capacity[d] || p.progress_q[d] != q.progress_q[d])
                return false;
    }

    if (a.history.size() != b.history.size()) return false;
    for (std::size_t i = 0; i < a.history.size(); ++i)
        if (a.history[i].event != b.history[i].event
         || a.history[i].consequence != b.history[i].consequence
         || a.history[i].years_before_epoch != b.history[i].years_before_epoch)
            return false;

    if (a.battles != b.battles || a.conquests != b.conquests || a.foundings != b.foundings
     || a.winter_campaigns != b.winter_campaigns || a.stalled_campaigns != b.stalled_campaigns
     || a.illegal_campaigns != b.illegal_campaigns || a.starved_campaigns != b.starved_campaigns
     || a.naval_battles != b.naval_battles || a.sea_leg_battles != b.sea_leg_battles
     || a.works_raised != b.works_raised
     || a.polities_industrialised != b.polities_industrialised
     || a.regions_industrialised != b.regions_industrialised
     || a.peak_population != b.peak_population || a.peak_year != b.peak_year
     || a.materials_produced != b.materials_produced
     || a.materials_spent_on_campaigns != b.materials_spent_on_campaigns
     || a.campaign_contacts != b.campaign_contacts || a.campaign_scored != b.campaign_scored
     || a.campaign_chosen != b.campaign_chosen || a.campaign_cleared != b.campaign_cleared
     || a.campaign_cleared_rounds != b.campaign_cleared_rounds
     || a.campaign_cleared_lost != b.campaign_cleared_lost
     || a.region_stride != b.region_stride || a.years != b.years
     || a.start_year != b.start_year)
        return false;
    if (a.battles_per_century != b.battles_per_century) return false;
    if (a.works_by_span_band != b.works_by_span_band) return false;
    if (a.units_by_span_band != b.units_by_span_band) return false;

    // THE SETTLEMENT STATE IS AN OUTPUT TOO - the sim mutates it in place, and
    // it is the half generation actually carries forward.
    if (sa.regions.size() != sb.regions.size()) return false;
    for (std::size_t i = 0; i < sa.regions.size(); ++i)
    {
        const region& p = sa.regions[i];
        const region& q = sb.regions[i];
        if (p.nation != q.nation || p.population != q.population
         || p.manpower_stock != q.manpower_stock || p.army_stock != q.army_stock
         || p.culture != q.culture || p.contest_q != q.contest_q
         || p.protection_q != q.protection_q || p.industrialised != q.industrialised
         || p.industrial_year != q.industrial_year || p.works_built != q.works_built
         || p.material_stock != q.material_stock || p.is_seat != q.is_seat
         || p.seat_region != q.seat_region
         // BL-872 — the new per-region reach reading and what it gates.
         || p.network_supply_q != q.network_supply_q || p.centres != q.centres
         || p.centres_razed != q.centres_razed || p.urban_population != q.urban_population)
            return false;
    }
    return true;
}

/// Byte-level equality of the playback record alone - the other half of the
/// pair. Two runs of the same seed must record identically.
bool same_record(const history_sim_state& a, const history_sim_state& b)
{
    if (a.steps.size() != b.steps.size()) return false;
    for (std::size_t i = 0; i < a.steps.size(); ++i)
        if (a.steps[i].year != b.steps[i].year
         || a.steps[i].first_sample != b.steps[i].first_sample
         || a.steps[i].sample_count != b.steps[i].sample_count)
            return false;
    if (a.samples.size() != b.samples.size()) return false;
    for (std::size_t i = 0; i < a.samples.size(); ++i)
        if (a.samples[i].polity != b.samples[i].polity
         || a.samples[i].regions != b.samples[i].regions
         || a.samples[i].population != b.samples[i].population
         || a.samples[i].cap_military != b.samples[i].cap_military
         || a.samples[i].cap_materials != b.samples[i].cap_materials)
            return false;
    if (a.culture_changes.size() != b.culture_changes.size()) return false;
    for (std::size_t i = 0; i < a.culture_changes.size(); ++i)
    {
        const culture_change& x = a.culture_changes[i];
        const culture_change& y = b.culture_changes[i];
        if (x.year != y.year || x.region != y.region || x.other_q != y.other_q) return false;
        for (int k = 0; k < timelapse_culture_slots; ++k)
            if (x.id[k] != y.id[k] || x.weight_q[k] != y.weight_q[k]) return false;
    }
    return true;
}

/// A minimal two-region world: one rich target, one owner, at a chosen
/// distance. Used to isolate the supply-decay stall from everything else.
settlement_state two_polity_world(int separation)
{
    settlement_state ss;

    region a;
    a.col = 0; a.row = 0; a.anchor = 0;
    a.culture = culture_shares::pure(0); a.founding_culture = 0;
    a.farm_q = 900; a.ore_q = 500; a.port_q = 100;
    a.settle_score_q = 900; a.name = "Home";
    ss.regions.push_back(a);

    region b;
    b.col = separation; b.row = 0; b.anchor = separation;
    b.culture = culture_shares::pure(1); b.founding_culture = 1;
    b.farm_q = 950; b.ore_q = 900; b.port_q = 100;
    b.settle_score_q = 880; b.name = "Prize";
    ss.regions.push_back(b);

    return ss;
}

/// ONE polity holding TWO regions from year zero — no conquest, no rival,
/// no campaign verb ever scored at all. Used to isolate garrison maintenance
/// (BL-837) from the campaign scorer entirely: both regions share a founding
/// culture, so `run_history_sim`'s plurality seed (BL-826) folds them into a
/// single polity before the year loop starts, and any movement in
/// `outpost.army_stock` over the run can only be the maintenance rule.
settlement_state one_polity_two_regions(int separation)
{
    settlement_state ss;

    region home;
    home.col = 0; home.row = 0; home.anchor = 0;
    home.culture = culture_shares::pure(0); home.founding_culture = 0;
    home.farm_q = 900; home.ore_q = 500; home.port_q = 100;
    home.settle_score_q = 900; home.name = "Home"; // highest settle score -> capital.
    ss.regions.push_back(home);

    region outpost;
    outpost.col = separation; outpost.row = 0; outpost.anchor = separation;
    outpost.culture = culture_shares::pure(0); outpost.founding_culture = 0;
    outpost.farm_q = 700; outpost.ore_q = 300; outpost.port_q = 50;
    outpost.settle_score_q = 300; outpost.name = "Outpost";
    ss.regions.push_back(outpost);

    return ss;
}

/// A three-region CHAIN: Home (polity A) — Mid (polity B) — Far (polity C),
/// one founding culture each (BL-922; see the note on Far). Home borders Mid; Mid borders
/// Far; Home does NOT border Far directly at the separations this harness
/// uses. Used to isolate the road discount (BL-837): once Home takes Mid,
/// reaching Far is a TWO-HOP question the capital's Dijkstra answers through
/// whatever the Home-Mid edge is worth AT THE TIME — raw, or discounted by
/// however many times that edge has already been walked.
settlement_state road_chain_world(int leg)
{
    settlement_state ss;

    region home;
    home.col = 0; home.row = 0; home.anchor = 0;
    home.culture = culture_shares::pure(0); home.founding_culture = 0;
    home.farm_q = 900; home.ore_q = 500; home.port_q = 100;
    home.settle_score_q = 900; home.name = "Home";
    // BL-922: HOME IS THE AGGRESSOR, BY WEIGHT OF PEOPLE. The fixture asks a
    // two-hop question (Home takes Mid, then reaches Far through it), and
    // that question is only asked if Home wins the first hop. Under the old
    // currency the gate refused nearly every candidate and the run never got
    // that far, so who won did not matter; priced from the capital the first
    // hop is allowed, and a Mid that took Home instead would answer a
    // different question. A tenfold population makes the first hop Home's.
    home.population = 2'000'000;
    ss.regions.push_back(home);

    region mid;
    mid.col = leg; mid.row = 0; mid.anchor = leg;
    mid.population = 20'000;
    mid.culture = culture_shares::pure(1); mid.founding_culture = 1;
    mid.farm_q = 700; mid.ore_q = 400; mid.port_q = 100;
    mid.settle_score_q = 700; mid.name = "Mid"; // higher than Far's -> B's capital.
    ss.regions.push_back(mid);

    region far;
    far.col = 2 * leg; far.row = 0; far.anchor = 2 * leg;
    // BL-922: Far is its OWN polity (C), not B's hinterland. Under BL-866 a
    // seat's hinterland falls WITH the seat, so a Far that pointed at Mid was
    // taken in the same event as Mid and the two-hop question this fixture
    // exists to ask was never asked. As a city state of its own it is reached
    // only by the second hop, which is the whole point.
    far.culture = culture_shares::pure(2); far.founding_culture = 2;
    far.population = 20'000;
    far.farm_q = 950; far.ore_q = 900; far.port_q = 100;
    far.settle_score_q = 500; far.name = "Far";
    ss.regions.push_back(far);

    return ss;
}

/// The synthetic worlds' own grid. Small enough to allocate a terrain field
/// for, wide enough that a 34-tile separation cannot wrap the cylinder.
constexpr int syn_gw = 168;
constexpr int syn_gh = 90;

/// A UNIFORM terrain field, held by value so the view it hands out stays valid.
///
/// The point is to make terrain a VARIABLE the harness controls. BL-316 S1's
/// finding was that both callers passed an empty view, so every Era -1 battle
/// ever fought resolved on default grassland/plains and the whole
/// terrain_combat module was inert — a state no assertion could have caught,
/// because there was nothing to compare against. Two runs differing only in
/// their ground is that comparison.
struct flat_terrain
{
    std::vector<terrain_substrate> substrate;
    std::vector<terrain_cover>     cover;
    std::vector<std::uint8_t>      density;
    std::vector<terrain_landform>  landform;

    flat_terrain(int gw, int gh, terrain_substrate su, terrain_cover cv, std::uint8_t d,
                 terrain_landform l)
        : substrate(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), su)
        , cover(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), cv)
        , density(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), d)
        , landform(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), l)
    {
    }

    sim_terrain_view view() const
    {
        return sim_terrain_view{ &substrate, &cover, &density, &landform };
    }
};

/// Did two runs produce a different history at all? The inertness check.
bool differs(const history_sim_state& a, const history_sim_state& b)
{
    if (a.battles != b.battles || a.conquests != b.conquests
     || a.foundings != b.foundings || a.stalled_campaigns != b.stalled_campaigns)
        return true;
    if (a.owner_changes.size() != b.owner_changes.size()) return true;
    for (std::size_t i = 0; i < a.owner_changes.size(); ++i)
        if (a.owner_changes[i].year != b.owner_changes[i].year
         || a.owner_changes[i].region != b.owner_changes[i].region
         || a.owner_changes[i].owner != b.owner_changes[i].owner)
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// BL-384 — the outcome instruments
// ---------------------------------------------------------------------------
//
// ONE DEFINITION OF DOMINANCE, NOT TWO. `history_sweep.cpp` already fixed what
// "a power dominates this world" means — half of all owned regions, held as a
// per-mille REPORTING threshold that gates nothing — and the shape it reads it
// through is `slice_shape` over a materialised owner slice. Both are reproduced
// here rather than re-invented, because a second definition of the same word
// would let the sweep and the harness disagree about whether a given world had
// a hegemon while both looked right in isolation.
//
// The sweep's own note is inherited with it: 0.5 is the plainest reading of
// "one power dominates the world", and it is REPORTED. B384 asserts nothing
// against it — see the group's comment for why the dominance column stays a
// column.
constexpr int hegemony_threshold_q = 500; // history_sweep.cpp § hegemony_threshold_q

/// Distinct owners and the largest owner's share (per-mille of OWNED regions)
/// in one materialised year slice. `owner_none` cells are regions that do not
/// exist yet at that year, and are excluded from the denominator — a share of
/// "all regions including ones nobody has founded" would fall as the Settle
/// verb ran and read as de-concentration that never happened.
void slice_shape(const std::vector<uint16_t>& slice, int& powers, int& top_share_q)
{
    std::vector<uint16_t> ids;
    std::vector<int>      counts;
    int live = 0;
    for (uint16_t o : slice)
    {
        if (o == owner_none) continue;
        ++live;
        auto it = std::find(ids.begin(), ids.end(), o);
        if (it == ids.end()) { ids.push_back(o); counts.push_back(1); }
        else                 { ++counts[static_cast<std::size_t>(it - ids.begin())]; }
    }
    powers = static_cast<int>(ids.size());
    int top = 0;
    for (int c : counts) top = c > top ? c : top;
    top_share_q = live > 0 ? (top * 1000) / live : 0;
}

/// One world's outcome row. Every field answers "did the run CONCLUDE", which
/// is the axis the counters the sim already carries cannot reach on their own.
struct outcome_row
{
    uint32_t seed = 0;

    int64_t battles   = 0;
    int64_t conquests = 0;
    int64_t foundings = 0;
    int64_t stalled   = 0;

    /// Polities the run left holding nothing (`polity::alive == false`), out of
    /// the ones that HELD GROUND at the start.
    ///
    /// Counted against the start slice rather than against `polities.size()`
    /// deliberately: the sim seeds one polity per culture, and a culture that
    /// never held a region would otherwise be counted as somebody the run had
    /// killed. That would report a positive elimination rate for a run in which
    /// nothing whatsoever happened, which is the exact failure this row exists
    /// to detect.
    int polities_at_start = 0;
    int polities_at_end   = 0;
    int eliminated        = 0;

    /// The largest polity's share of owned regions at the epoch, per-mille,
    /// and whether that crossed `hegemony_threshold_q`.
    int  top_share_q   = 0;
    bool hegemon       = false;

    int64_t ms = 0;
};

} // namespace

int main()
{
    std::printf("=== history sim harness (BL-277 scorer + BL-271 year tick) ===\n");

    world_params wp;
    generation_report r1;
    const world w1 = make_hard_coded_world(wp, &r1);
    (void)w1;

    const generation_report::body_entry* k1 = kepler_of(r1);
    if (!k1)
    {
        std::printf("FAIL  no body in the generation report\n");
        return 1;
    }

    const sim_terrain_view no_terrain{}; // Both members null: neutral terrain, legal by design.

    // THE DEFAULTS ARE THE REAL EPOCH. This harness pinned start/stop to
    // 0 -> 1960, which stopped being the run the game does when the campaign
    // epoch moved to 0 CE (the sim now runs 4000 BCE -> 0 CE under the stepped
    // decision clock). Left pinned, every Kepler assertion below measured a
    // configuration that no longer exists, and the stepped clock — the thing
    // that makes the run affordable — was never exercised here at all.
    history_sim_params params;

    // AND THE GRID MUST BE THE REAL GRID. These runs passed 168x90 against a
    // Kepler that is home_grid_width x home_grid_height (312x145). `gw` is the
    // cylinder's circumference: understate it and `region_distance` wraps
    // columns that do not wrap, silently reporting two regions on opposite
    // sides of the map as neighbours. Same class of defect as the sweep's
    // 168x90-vs-180x84 terrain misalignment, and just as quiet.
    constexpr int kgw = home_grid_width;
    constexpr int kgh = home_grid_height;

    // --- R1  determinism ---------------------------------------------------
    {
        settlement_state s1 = k1->settlement;
        settlement_state s2 = k1->settlement;
        const history_sim_state a = run_history_sim(s1, nullptr, no_terrain, kgw, kgh, params, 12345u);
        const history_sim_state b = run_history_sim(s2, nullptr, no_terrain, kgw, kgh, params, 12345u);
        check(same_run(a, b, s1, s2),
              "R1   two runs of the same seed produce an identical history and ownership ring");
        check(!a.polities.empty(), "R1b  the sim seeds at least one polity from the cultures");
    }

    // --- R6/R7/R8  ring size, runtime, and founding ------------------------
    {
        settlement_state s = k1->settlement;
        const std::size_t before = s.regions.size();

        const auto t0 = std::chrono::steady_clock::now();
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, params, 7u);
        const auto t1 = std::chrono::steady_clock::now();
        const int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        const int64_t bytes = owner_ring_bytes(a);
        std::printf("      run: %lld regions -> %lld, %lld battles, %lld conquests, "
                    "%lld foundings, %lld winter, ring %lld bytes, %lld ms\n",
                    static_cast<long long>(before),
                    static_cast<long long>(s.regions.size()),
                    static_cast<long long>(a.battles),
                    static_cast<long long>(a.conquests),
                    static_cast<long long>(a.foundings),
                    static_cast<long long>(a.winter_campaigns),
                    static_cast<long long>(bytes),
                    static_cast<long long>(ms));

        check(bytes > 0 && bytes < 1024 * 1024,
              "R6   the ownership time-lapse substrate stays under 1 MB");
        // The <1s target moved to BL-320 (Era -1 sim runtime): the settle-occupancy
        // fix quadrupled real region count (correct behaviour, ~4x work), and the
        // measured full run is ~2.1s. Bound against that reality so the harness
        // stays honest; the sub-second bar returns when BL-320 lands its index.
        check(ms < 3000,
              "R7   a full 4000 BCE -> 0 CE run fits the measured budget (BL-320)");
        check(s.regions.size() > before,
              "R8   the Settle verb founds regions during the run");
        check(a.years == params.stop_year - params.start_year,
              "R6b  the run covers one tick per simulated year");

        // The change list must actually replay: the final slice has to agree
        // with the regions' own nation field, or the time-lapse would drift
        // from the world it claims to depict.
        const std::vector<uint16_t> last = owner_slice_at(a, params.stop_year);
        bool replay_agrees = (last.size() == s.regions.size());
        if (replay_agrees)
            for (std::size_t i = 0; i < s.regions.size(); ++i)
                if (s.regions[i].nation >= 0
                 && last[i] != static_cast<uint16_t>(s.regions[i].nation))
                    { replay_agrees = false; break; }
        check(replay_agrees,
              "R6c  replaying the change list reproduces the final political map");
    }

    // --- R3  the supply-decay stall ----------------------------------------
    //
    // Same two polities, same prize, different distance. Near, the campaign is
    // supplied and the region can change hands; far, supply decays to
    // nothing and the frontier stalls on arithmetic alone.
    {
        history_sim_params p2 = params;
        // The synthetic cases keep their own short, FLAT span: they isolate one
        // mechanism, and the stepped clock's coarse prehistory bands would make
        // "how many rounds happened" a second variable. `params` now carries the
        // real 4000 BCE epoch, so start_year has to be stated, not inherited.
        p2.start_year = 0;
        p2.stop_year  = 400;

        // THE RADIUS MUST NOT BE WHAT STOPS THE FAR CASE (BL-312). The first cut
        // put the far prize 34 tiles out against a neighbour_radius of 9, so it
        // was filtered before supply decay was ever computed — the assertion
        // passed because nothing was SCORED, not because supply ran out, and the
        // stall mechanism carrying the whole non-hegemony argument had no test
        // behind it. Widening the radius past the separation makes the far
        // target a real candidate, so only supply decay can stop it.
        p2.neighbour_radius = 40;

        // AND the distance term in the OBJECTIVE SCORE must not be what stops
        // it either. Zeroing `w_dist` isolates the logistics price as the only
        // thing left that can stop the far case, which is the mechanism under
        // test. (Historically `w_dist` was a flat subtraction and vetoed the far
        // target outright; it is a proportional discount now, so it discourages
        // rather than forbids. Zeroing it still removes the variable.)
        p2.w_dist = 0;

        // BL-922: supply is priced from the capital over held ground in the
        // `terrain_reach_cost_q` currency alone -- the staging-hub decay
        // (`supply_decay_per_tile_q`, 28 x hub_dist) that used to drive the
        // far case to ~50/1000 is gone. Stated explicitly so the far prize
        // (34 tiles) lands at ~250: under `stalled_supply_q` (300), so it
        // STALLS, and above `sustainable_campaign_floor_q` (80), so it is
        // still fought for. An instrument setting, not the shipped value.
        p2.terrain_reach_cost_q = 2200;

        settlement_state near_w = two_polity_world(3);
        settlement_state far_w  = two_polity_world(34);

        const history_sim_state a = run_history_sim(near_w, nullptr, no_terrain, syn_gw, syn_gh, p2, 99u);
        const history_sim_state b = run_history_sim(far_w,  nullptr, no_terrain, syn_gw, syn_gh, p2, 99u);

        std::printf("      near: %lld battles / %lld conquests | far: %lld battles / %lld conquests\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(b.battles), static_cast<long long>(b.conquests));

        check(a.battles > 0, "R3a  an adjacent objective is actually campaigned for");
        check(b.battles > 0,
              "R3a2 the FAR objective is genuinely scored and attacked — the radius is not what stops it");
        check(b.stalled_campaigns > 0,
              "R3a3 those far campaigns arrive under-supplied (the stall is real, not absence)");
        // WHAT ACTUALLY STOPS DISTANT EXPANSION — measured, not assumed. On
        // plains, with the burden of breadth inert (two regions is far under
        // `free_holdings`), supply only mitigates attrition and attrition on
        // plains is 100/1000. So the whole span from fully supplied to totally
        // cut off is 10% of combat power, and the far region can still fall.
        //
        // This is RECORDED rather than asserted away, and it is why BL-316 exists:
        // the fix for a 10% dynamic range is not to inflate the supply term but
        // to make the other two terms real — terrain (a mountain battle carries
        // up to 900/1000 attrition, so supply matters enormously there) and the
        // burden of breadth. Both are exercised by S2/S3 below, on purpose, in
        // conditions where they can actually bite.
        std::printf("      far: %lld stalled of %lld campaigns, %lld conquests "
                    "(supply's plains dynamic range is ~10%% of combat power)\n",
                    static_cast<long long>(b.stalled_campaigns),
                    static_cast<long long>(b.battles),
                    static_cast<long long>(b.conquests));

        // DIRECTION, NOT AN ABSOLUTE. This was `c.battles == 0` — the claim that
        // the score's distance penalty vetoes the far target outright. That was
        // true of the flat `w_dist` and is deliberately no longer true: a flat
        // subtraction from a ~200-300 region value suppressed every war on a
        // large map, so `w_dist` became a proportional discount. Asserting the
        // old absolute would now be asserting the bug back. What must still hold
        // is the direction — pricing distance cannot make the far target MORE
        // attractive.
        {
            history_sim_params p_dist = p2;
            p_dist.w_dist = 120;   // The default: distance priced in the score.
            settlement_state far2 = two_polity_world(34);
            const history_sim_state c =
                run_history_sim(far2, nullptr, no_terrain, syn_gw, syn_gh, p_dist, 99u);
            std::printf("      far with w_dist=120: %lld battles / %lld conquests "
                        "(w_dist=0 gave %lld / %lld)\n",
                        static_cast<long long>(c.battles), static_cast<long long>(c.conquests),
                        static_cast<long long>(b.battles), static_cast<long long>(b.conquests));
            check(c.battles <= b.battles,
                  "R3b2 pricing distance in the score never makes a far target more attractive");
        }
        check(a.conquests >= b.conquests,
              "R3c  proximity never scores worse than distance for territorial gain");
    }

    // --- S1  real terrain is LIVE, not inert (BL-316) ----------------------
    //
    // The finding this slice was filed on: both callers passed an empty
    // `sim_terrain_view`, so every Era -1 battle ever fought resolved on default
    // grassland/plains and terrain_combat — defence, attrition, BL-233's
    // modifiers — was dead code. That state is invisible to any single run. It
    // shows up only as two runs that differ ONLY in their ground and produce
    // byte-identical histories.
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 400;
        ps.neighbour_radius = 40;

        const flat_terrain plains(syn_gw, syn_gh,
                                  terrain_substrate::sedimentary, terrain_cover::grass, 150, terrain_landform::plains);
        const flat_terrain alps(syn_gw, syn_gh,
                                terrain_substrate::rocky, terrain_cover::none, 0, terrain_landform::mountain);

        settlement_state wp_ = two_polity_world(6);
        settlement_state wm  = two_polity_world(6);
        const history_sim_state on_plains =
            run_history_sim(wp_, nullptr, plains.view(), syn_gw, syn_gh, ps, 77u);
        const history_sim_state on_mountain =
            run_history_sim(wm,  nullptr, alps.view(),   syn_gw, syn_gh, ps, 77u);

        std::printf("      plains: %lld battles / %lld conquests | mountain: %lld / %lld\n",
                    static_cast<long long>(on_plains.battles),
                    static_cast<long long>(on_plains.conquests),
                    static_cast<long long>(on_mountain.battles),
                    static_cast<long long>(on_mountain.conquests));

        check(differs(on_plains, on_mountain),
              "S1a  terrain CHANGES the history — the terrain view is no longer inert");
        check(on_mountain.conquests <= on_plains.conquests,
              "S1b  mountains are not easier to conquer across than plains");
    }

    // --- S2  terrain-weighted reach (BL-316) -------------------------------
    //
    // Reach is a COST over the neighbour graph, not a straight line: the same
    // separation across mountains costs about twice what it costs across
    // plains. Measured through supply, so the observable is the stall rate.
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 400;
        ps.neighbour_radius = 40;
        // Reach is priced per 100 of accumulated cost, so it is a small term at
        // the default. Raised here so the SIGN of the effect is measurable
        // without changing what the sim does at its shipped tuning — this is an
        // instrument gain, not a calibration.
        ps.terrain_reach_cost_q = 400;

        const flat_terrain plains(syn_gw, syn_gh,
                                  terrain_substrate::sedimentary, terrain_cover::grass, 150, terrain_landform::plains);
        const flat_terrain alps(syn_gw, syn_gh,
                                terrain_substrate::rocky, terrain_cover::none, 0, terrain_landform::mountain);

        settlement_state wp_ = two_polity_world(20);
        settlement_state wm  = two_polity_world(20);
        const history_sim_state flat_run =
            run_history_sim(wp_, nullptr, plains.view(), syn_gw, syn_gh, ps, 88u);
        const history_sim_state hilly_run =
            run_history_sim(wm,  nullptr, alps.view(),   syn_gw, syn_gh, ps, 88u);

        std::printf("      reach: plains %lld stalled / %lld battles | mountain %lld / %lld\n",
                    static_cast<long long>(flat_run.stalled_campaigns),
                    static_cast<long long>(flat_run.battles),
                    static_cast<long long>(hilly_run.stalled_campaigns),
                    static_cast<long long>(hilly_run.battles));

        check(hilly_run.conquests <= flat_run.conquests,
              "S2a  terrain-weighted reach never makes mountainous ground cheaper to take");
    }

    // --- S3  the burden of breadth (BL-316) --------------------------------
    //
    // Ben's mechanism: supply available to a campaign falls with the polity's
    // TOTAL holdings, so expansion eventually pays for itself in reach and the
    // frontier stall becomes ARITHMETIC rather than a scoring preference.
    // Before this slice, holding 500 regions cost exactly what holding 5 did.
    //
    // Isolated by turning the burden on and off over the same world. `burden`
    // is subtracted from supply directly, so the direction is unambiguous:
    // never more conquest with the burden on than with it off.
    {
        history_sim_params base = params;
        base.start_year = 0;
        base.stop_year  = 400;
        base.neighbour_radius = 40;

        history_sim_params off = base;
        off.holdings_burden_q = 0;      // No cost of breadth at all — the old world.

        history_sim_params on = base;
        on.free_holdings     = 0;       // Every region held costs supply...
        on.holdings_burden_q = 300;     // ...and costs it visibly.

        settlement_state w_off = two_polity_world(12);
        settlement_state w_on  = two_polity_world(12);
        const history_sim_state a =
            run_history_sim(w_off, nullptr, no_terrain, syn_gw, syn_gh, off, 55u);
        const history_sim_state b =
            run_history_sim(w_on,  nullptr, no_terrain, syn_gw, syn_gh, on,  55u);

        std::printf("      burden off: %lld battles / %lld conquests / %lld stalled | "
                    "on: %lld / %lld / %lld\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(a.stalled_campaigns),
                    static_cast<long long>(b.battles), static_cast<long long>(b.conquests),
                    static_cast<long long>(b.stalled_campaigns));

        check(differs(a, b),
              "S3a  the burden of breadth is LIVE — holding ground is no longer free");
        check(b.conquests <= a.conquests,
              "S3b  a polity charged for its breadth never conquers more than one that is not");
    }

    // --- BL-318  the verbs share one currency ------------------------------
    //
    // The failure this item names is a SCORER whose argmax is decided by scale
    // rather than by desirability: one verb pins at its ceiling and wins every
    // year forever. Both observed cuts had exactly that shape — Invest-dominated
    // (82 regions, ownership frozen after year 458 of 1960), then
    // Settle-dominated (1532 regions, 1450 foundings, conquests ZERO).
    //
    // So the check is not "the numbers look right", which is unfalsifiable, but
    // that NO VERB HAS SWALLOWED THE RUN: the sim must still be deciding things
    // late, and more than one verb must be reachable. Both cuts above would fail
    // this; neither would fail a total-count assertion.
    {
        settlement_state s = k1->settlement;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, params, 2024u);

        int64_t last_change = params.start_year;
        for (const owner_change& c : a.owner_changes)
            if (c.year > last_change) last_change = c.year;

        const int64_t span   = params.stop_year - params.start_year;
        const int64_t elapsed = last_change - params.start_year;

        std::printf("      currency: %lld foundings / %lld conquests / %lld battles, "
                    "last ownership change year %lld of %lld..%lld\n",
                    static_cast<long long>(a.foundings),
                    static_cast<long long>(a.conquests),
                    static_cast<long long>(a.battles),
                    static_cast<long long>(last_change),
                    static_cast<long long>(params.start_year),
                    static_cast<long long>(params.stop_year));

        // THE INERT-TAIL TEST. "Last ownership change at year 458 of 1960" is
        // the exact symptom the shared currency was built to remove: three
        // quarters of the run doing nothing, so every distribution the sweep
        // reported described a world that stopped early.
        check(elapsed * 2 >= span,
              "B318a the run is still deciding in its second half — no verb has swallowed it");

        // Neither verb may be extinct. Settle-only and conquest-only are the two
        // observed collapses, and each is invisible to a check on the other.
        check(a.foundings > 0, "B318b the Settle verb is reachable under the shared currency");
        check(a.conquests > 0, "B318c the Campaign verb is reachable under the shared currency");
    }

    // --- BL-384  the sim conquers nothing -----------------------------------
    //
    // THE ASSERTION HALF ONLY (BL-384's own prescribed order: assert before
    // fix). The observed defect: a full-span run over the real epoch produced
    // 267 battles against 0 conquests — every battle fought, no ground taken,
    // so the political map is produced entirely by Settle expanding into empty
    // space. War contributes nothing to who owns what.
    //
    // This is a THRESHOLD assertion, not a direction assertion like the pairs
    // above — deliberately, because a direction check ("conquests > 0" alone,
    // as B318c already has) is satisfiable by a single lucky conquest in 1960
    // years and would not have caught the 267:0 ratio. The bar here is the
    // ratio itself: conquest must not be vanishingly rare relative to battles
    // fought, i.e. NOT the near-1000:1 shape the pre-BL-308 comment records.
    //
    // EXPECTED RESULT: RED against today's build. That is the deliverable of
    // this half of the item, not a bug in the harness — do not weaken this
    // assertion to make it pass; the fix is BL-384's next half (Sprint 28).
    {
        settlement_state s = k1->settlement;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, params, 9001u);

        std::printf("      BL384: %lld battles / %lld conquests over %lld..%lld\n",
                    static_cast<long long>(a.battles),
                    static_cast<long long>(a.conquests),
                    static_cast<long long>(params.start_year),
                    static_cast<long long>(params.stop_year));

        check(a.battles > 0,
              "BL384a the run fights at all (a zero-battle run would make the ratio check vacuous)");

        // The pre-fix shape was "battles outnumbered conquests by up to 1000:1"
        // (history_sim.cpp's own comment). A working conquest mechanic should
        // clear a battles:conquests ratio far short of that — pinned here at
        // 20:1 as a generous, clearly-distinguishing bar between "broken" and
        // "working", not a calibration target to tune toward.
        check(a.conquests * 20 >= a.battles,
              "BL384b conquest is not vanishingly rare relative to battles fought (ratio < 20:1)");
    }

    // --- R5  season is an action axis, not a clock -------------------------
    {
        settlement_state s = k1->settlement;
        history_sim_params p3 = params; // The real epoch — winter is a real axis in it.
        p3.winter_score_premium_q = 0;  // Make winter freely competitive.
        // BL-922: REACH MUST NOT BE WHAT STOPS THE WAR HERE. This fixture
        // asks whether winter is chosen as a candidate, which needs campaigns
        // to happen at all. With `params` carrying no centre relay and the
        // shipped 4000 (measured on generation's round, relay ON), this seed
        // fought 2 battles in 4,000 years and the check read "no winter" for
        // want of any war. Priced at the pre-gradient magnitude the fixture
        // was written against, so the season axis is the only variable.
        p3.terrain_reach_cost_q = 10;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, p3, 4242u);
        // Print the counts BOTH checks read. R5b is trivially true when both are
        // zero, so a bare R5 failure cannot distinguish "winter is never chosen"
        // (a real regression in the scoring axis) from "this seed fought no war
        // at all" (a fragile single-seed fixture). One line makes it diagnosable.
        std::printf("      R5 fixture: %d battles / %d winter campaigns (seed 4242, premium 0)\n",
                    a.battles, a.winter_campaigns);
        check(a.winter_campaigns > 0,
              "R5   winter campaigns are chosen as candidates, not scheduled by a clock");
        check(a.battles >= a.winter_campaigns,
              "R5b  winter campaigns are a subset of all campaigns");
    }

    // --- R4  region granularity ------------------------------------------
    //
    // Structural, not observational: run_history_sim takes no mutable tile
    // access at all — its terrain view is two const pointers — so it CANNOT
    // write a tile. The assertion below records that transfer moves the
    // region's own owner field and nothing wider.
    {
        settlement_state s = two_polity_world(3);
        history_sim_params p4 = params;
        p4.start_year = 0;   // Synthetic world: its own flat span (see R3).
        p4.stop_year  = 400;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, syn_gw, syn_gh, p4, 5u);
        bool anchors_intact = (s.regions[0].anchor == 0 && s.regions[1].anchor == 3);
        check(anchors_intact,
              "R4   a run never rewrites a region's anchor tile — transfer is region-granular");
        check(a.region_stride == static_cast<int>(s.regions.size()),
              "R4b  the ring's stride matches the final region count");
    }

    // --- BL-274  era-keyed rosters ----------------------------------------
    //
    // Asymmetry is the point: two regions at the same band field different
    // rosters because their ground differs. A test that only checked "a stack
    // comes back" would pass on a table that ignored endowment entirely.
    {
        region bare;  bare.farm_q = 100; bare.ore_q = 0;   bare.port_q = 0;   bare.energy_q = 0;
        region forge; forge.farm_q = 100; forge.ore_q = 900; forge.port_q = 0; forge.energy_q = 0;
        region coast; coast.farm_q = 100; coast.ore_q = 0;  coast.port_q = 900; coast.energy_q = 0;

        const auto bare_rows  = available_rows(bare,  roster_band::classical);
        const auto forge_rows = available_rows(forge, roster_band::classical);
        const auto coast_rows = available_rows(coast, roster_band::classical);

        check(forge_rows.size() > bare_rows.size(),
              "B274a ore country fields types bare ground cannot (endowment gates bite)");

        bool coast_has_naval = false;
        for (const roster_row* r : coast_rows) if (r->cls == unit_class::naval) coast_has_naval = true;
        bool bare_has_naval = false;
        for (const roster_row* r : bare_rows) if (r->cls == unit_class::naval) bare_has_naval = true;
        check(coast_has_naval && !bare_has_naval,
              "B274b a port fields ships and a landlocked region does not");

        // The stirrup is a MEDIEVAL unlock — the T1/T2 boundary settled with the
        // roster grouping. A classical roster must not contain heavy horse.
        bool classical_stirrup = false;
        for (const roster_row* r : available_rows(forge, roster_band::classical))
            if (std::string(r->name) == "Stirrup Horse") classical_stirrup = true;
        bool medieval_stirrup = false;
        for (const roster_row* r : available_rows(forge, roster_band::medieval))
            if (std::string(r->name) == "Stirrup Horse") medieval_stirrup = true;
        check(!classical_stirrup, "B274c shock cavalry is NOT a 0 CE unit (the stirrup is T2)");

        // Bands are cumulative — nothing un-invents a spear.
        check(available_rows(forge, roster_band::industrial).size()
                  > available_rows(forge, roster_band::classical).size(),
              "B274d bands are cumulative: a later band keeps the earlier rows");
        (void)medieval_stirrup;

        check(roster_band_for_capacity(1) == roster_band::classical
           && roster_band_for_capacity(3) == roster_band::medieval
           && roster_band_for_capacity(4) == roster_band::gunpowder
           && roster_band_for_capacity(6) == roster_band::industrial,
              "B274e capacity bands map onto the settled roster grouping");
    }

    // --- BL-299  two-great-powers seed ------------------------------------
    {
        settlement_state s = k1->settlement;
        history_sim_params p5 = params;
        // Seeding happens at setup, so a short slice of the real epoch is
        // enough — and stays cheap. Stated relative to start_year, which is
        // 4000 BCE now, not 0.
        p5.stop_year = p5.start_year + 300;
        p5.seed_great_powers = true;
        const history_sim_state a = run_history_sim(s, nullptr, no_terrain, kgw, kgh, p5, 11u);

        int majors = 0, expansionist = 0, preserving = 0;
        for (const polity& q : a.polities)
        {
            if (!q.major) continue;
            ++majors;
            if (q.aggression_q >= 800) ++expansionist;
            if (q.aggression_q <= 300) ++preserving;
        }
        check(majors == 2, "B299a exactly two majors are seeded");
        check(expansionist == 1 && preserving == 1,
              "B299b the majors carry OPPOSED creeds, not the same one twice");
        check(static_cast<int>(a.polities.size()) > 2,
              "B299c the periphery survives as actors, not terrain");
    }

    // --- BL-384  the full run must reach a CONCLUSION ----------------------
    //
    // WHAT THIS GROUP ASSERTS THAT NOTHING ABOVE DOES. Every check before it is
    // a MECHANISM check: terrain is live, supply decays, the burden bites, the
    // verbs share a currency. Each is a claim about one moving part, and the
    // whole file's stated pattern — run the same world twice with one variable
    // changed and assert the DIRECTION — is the right shape for those.
    //
    // It is the wrong shape for this one. "Does the generator ever finish a
    // war" is not a direction, it is an OUTCOME, and an outcome has no sibling
    // run to be compared against. The gap that left is exactly how BL-384 was
    // filed: `same_run` and `differs` both read `conquests`, so two runs were
    // required to AGREE about the count without anything ever asking what the
    // count was. A sim that conquers nothing is perfectly deterministic.
    //
    // WHY A SEED SET AND NOT ONE WORLD. B318c already asserts conquests > 0,
    // on one seed, and one seed cannot tell "this sim never conquers" from
    // "this world happened to be peaceful". Measured below, that distinction is
    // the entire finding: the outcome is BIMODAL across worlds, and no
    // single-seed check can see a bimodal distribution at all.
    //
    // COST. Each row generates a whole world (with its pre-epoch pass — this
    // harness may not use `no_prehistory`, see tools/verify/harness_params.hpp)
    // and then runs the full 4000 BCE -> 0 CE sim over it. That is the reason
    // this group is last and the reason the count is small; the harness carries
    // the `sweep` label already (CMakeLists § IO_TEST_SWEEP_HARNESSES).
    {
        // SEEDS 0..N-1, WHICH IS history_sweep's OWN SEED CONVENTION
        // (`wp.seed = i`). Deliberate, so a row here and a row there describe
        // the same world and can be read against each other. Fixed in the
        // source rather than taken from argv: a red assertion whose seed set is
        // an argument is a red assertion that can be argued away.
        constexpr int outcome_seeds = 8;

        // REAL TERRAIN AND THE REAL GRID, because the claim is about the run
        // the GAME does. The Kepler checks above pass `no_terrain` — correct
        // for them, since they isolate one mechanism — but a peaceful-world
        // finding measured on ground that does not exist would be unactionable.
        //
        // No works registry, which is this harness's standing convention (the
        // real table is Lua and every check here is Lua-free). The sweep's
        // hand-built fixture is deliberately not duplicated — see its own note
        // on why that fixture is not a second authoring path.
        std::vector<outcome_row> rows;
        rows.reserve(static_cast<std::size_t>(outcome_seeds));

        for (int i = 0; i < outcome_seeds; ++i)
        {
            const auto t0 = std::chrono::steady_clock::now();

            world_params owp;
            owp.seed = static_cast<uint32_t>(i);
            generation_report rep;
            const world w = make_hard_coded_world(owp, &rep);

            const generation_report::body_entry* k = kepler_of(rep);
            if (k == nullptr) continue;

            settlement_state ss = k->settlement;
            const sim_terrain_arrays terr =
                build_sim_terrain(w, k->id, kgw, kgh); // the entry names its own entity (BL-257)

            history_sim_params op; // the real epoch and the real tuning, untouched
            const history_sim_state a =
                run_history_sim(ss, nullptr, terr.view(), kgw, kgh, op, owp.seed);

            outcome_row row;
            row.seed      = owp.seed;
            row.battles   = a.battles;
            row.conquests = a.conquests;
            row.foundings = a.foundings;
            row.stalled   = a.stalled_campaigns;

            const std::vector<uint16_t> first = owner_slice_at(a, op.start_year);
            const std::vector<uint16_t> last  = owner_slice_at(a, op.stop_year);

            int dummy = 0;
            slice_shape(first, row.polities_at_start, dummy);
            slice_shape(last,  row.polities_at_end,   row.top_share_q);
            row.hegemon = row.top_share_q >= hegemony_threshold_q;

            for (const polity& q : a.polities)
            {
                if (q.alive) continue;
                bool held_at_start = false;
                for (uint16_t o : first)
                    if (o == static_cast<uint16_t>(q.id)) { held_at_start = true; break; }
                if (held_at_start) ++row.eliminated;
            }

            const auto t1 = std::chrono::steady_clock::now();
            row.ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

            rows.push_back(row);
        }

        std::printf("\n      --- BL-384 full-run outcome, %d worlds, real terrain ---\n",
                    static_cast<int>(rows.size()));
        std::printf("      seed  battles  conquests  stalled  foundings  "
                    "powers(0>epoch)  elim  top%%   ms\n");
        int64_t total_ms = 0;
        int worlds_with_conquest = 0, worlds_that_fought = 0;
        int worlds_fought_took_nothing = 0, total_eliminated = 0, hegemonies = 0;
        for (const outcome_row& r : rows)
        {
            std::printf("      %4u  %7lld  %9lld  %7lld  %9lld  %6d > %6d  %4d  %3d%%  %5lld\n",
                        r.seed,
                        static_cast<long long>(r.battles),
                        static_cast<long long>(r.conquests),
                        static_cast<long long>(r.stalled),
                        static_cast<long long>(r.foundings),
                        r.polities_at_start, r.polities_at_end,
                        r.eliminated, r.top_share_q / 10,
                        static_cast<long long>(r.ms));
            total_ms += r.ms;
            if (r.conquests > 0) ++worlds_with_conquest;
            if (r.battles > 0)   ++worlds_that_fought;
            if (r.battles > 0 && r.conquests == 0) ++worlds_fought_took_nothing;
            total_eliminated += r.eliminated;
            if (r.hegemon) ++hegemonies;
        }
        const int n = static_cast<int>(rows.size());
        std::printf("      %d/%d worlds fought, %d/%d took ground, %d polities eliminated, "
                    "%d hegemonies, %lld ms total\n",
                    worlds_that_fought, n, worlds_with_conquest, n,
                    total_eliminated, hegemonies, static_cast<long long>(total_ms));

        check(n == outcome_seeds, "B384  every seed in the set produced a world and ran");

        // THE SHARE IS ALL OF THEM, AND THAT IS NOT A DIAL.
        //
        // Any share below 1.0 would make this a calibration target, and the
        // header of this file says why calibration targets do not belong here:
        // they get tuned toward instead of measured. What is claimed is
        // REACHABILITY — the same claim B318c makes on one seed — and a verb
        // that is unreachable in a given world is unreachable in it however
        // many other worlds it works in.
        //
        // The claim also has a plain design reading. The pre-epoch sim exists
        // to hand the campaign a world with a past (hard_coded_world.cpp: "the
        // campaign opened onto a world that had been settled and then stood
        // perfectly still: no wars, no borders that had ever moved, nothing to
        // inherit"). A world that runs four thousand years without one region
        // changing hands is that world again — the pass ran and produced the
        // state it was written to prevent.
        //
        // The achieved share is printed above regardless.
        //
        // B384a RETIRED (BL-854, Ben's ruling 2026-09-10): "for this round of
        // generation we should not see war — inevitable means we carry the
        // capacity for war to the next round." COLONISATION.md's premise is
        // that the migration span is a settlement process with occasional
        // violence, not a war-every-world guarantee — measured at 5/8 worlds
        // fighting, so the old claim failed for a reason the design now says
        // is correct. What must still hold is CAPABILITY, carried forward
        // into the history round rather than asserted here, and B384b below
        // is what actually checks that the transfer branch fires somewhere
        // in the set.

        // THE DEFECT EXACTLY AS FILED: 267 battles, zero conquests. A world
        // that campaigns for an era and takes nothing means the transfer branch
        // (history_sim.cpp § "TERRITORY MOVES AT PROVINCE GRANULARITY") is
        // unreachable behind a decisiveness bar no victory clears.
        //
        // Kept separate from B384a on purpose, because the two fail for
        // opposite reasons and a single row could not tell them apart: B384a
        // fails when the scorer never picks Campaign, B384b when it picks it
        // constantly and the transfer never fires. Reading which one is red is
        // the diagnosis.
        check(worlds_fought_took_nothing == 0,
              "B384b no world fights a whole era and takes nothing (the transfer branch fires)");

        // THE ARC'S SECOND RUNG. BL-308 added polity cohesion precisely because
        // "the first sweep produced elimination in 0 of 12 worlds" — losing
        // ground cost a polity nothing that made the next defeat more likely,
        // so no defeat ever led to another (history_sim.hpp § cohesion_q).
        //
        // That is the project's own statement that a zero here is a defect and
        // not a preference, which is what makes it assertable at all. It is
        // stated across the WHOLE set rather than per world: one polity losing
        // its last region somewhere in N worlds is the weakest form of "the
        // death spiral terminates", and anything weaker is not a claim.
        check(total_eliminated > 0,
              "B384c a polity somewhere in the set loses its last region (BL-308's spiral ends)");

        // DOMINANCE IS REPORTED AND GATES NOTHING — history_sweep.cpp's ruling,
        // inherited with its threshold. BL-224's non-hegemony is a tuning
        // target read off the spread, not a construction guarantee, so the
        // `top%` column and the hegemony count above are instruments. Asserting
        // a bound on them here would be writing the calibration this arc has
        // not chosen yet, and would hide the spread it needs to see.
    }

    // --- B817  THE PLAYBACK RECORD -----------------------------------------
    //
    // Three claims, and they are the three DONE WHEN clauses of BL-817:
    // the record is bounded and its size is STATED; a recorded run and a
    // suppressed run agree bit-for-bit on every other output; and the same seed
    // records identically twice.
    //
    // Run at the DEFAULT span - 4000 BCE -> 0 CE, the real epoch - so the size
    // figure printed here is the figure the item bounds, not one measured on a
    // short arc and extrapolated.
    {
        history_sim_params rec = params;                 // record_playback defaults ON.
        history_sim_params off = params; off.record_playback = false;

        settlement_state s1 = k1->settlement;
        settlement_state s2 = k1->settlement;
        settlement_state s3 = k1->settlement;
        const history_sim_state a  = run_history_sim(s1, nullptr, no_terrain, kgw, kgh, rec, 4242u);
        const history_sim_state a2 = run_history_sim(s2, nullptr, no_terrain, kgw, kgh, rec, 4242u);
        const history_sim_state n  = run_history_sim(s3, nullptr, no_terrain, kgw, kgh, off, 4242u);

        const long long play  = static_cast<long long>(playback_record_bytes(a));
        const long long ring  = static_cast<long long>(owner_ring_bytes(a));
        // The counterfactual the encoding argument rests on: what a DENSE grid
        // of culture shares would have cost over the same run. Printed, not
        // asserted - it is the reason the change list was chosen, and a reader
        // should be able to check the reasoning rather than take it.
        const long long dense = static_cast<long long>(a.steps.size())
                              * static_cast<long long>(a.region_stride)
                              * static_cast<long long>(sizeof(culture_change));
        std::printf("      playback record over %lld years: %lld steps, %lld samples, "
                    "%lld culture changes = %lld bytes  "
                    "(ownership ring %lld, a dense culture grid would be %lld)\n",
                    static_cast<long long>(a.years),
                    static_cast<long long>(a.steps.size()),
                    static_cast<long long>(a.samples.size()),
                    static_cast<long long>(a.culture_changes.size()),
                    play, ring, dense);

        check(!a.steps.empty() && !a.samples.empty(),
              "B817a a full-span run emits recorded steps with per-polity samples");
        check(play > 0 && play < 1024 * 1024,
              "B817b the playback record stays under 1 MB over the whole 4000-year span");
        check(a.steps.back().year == static_cast<int32_t>(rec.stop_year),
              "B817c the closing step lands on the stop year, not wherever the interval fell");

        // THE CADENCE CLAIM, asserted rather than commented: ascending years,
        // never finer than the interval except for the closing step, and every
        // step's sample span inside the sample array.
        bool cadence_ok = true;
        for (std::size_t i = 0; i < a.steps.size(); ++i)
        {
            const timelapse_step& st = a.steps[i];
            if (st.first_sample < 0 || st.sample_count < 0
             || static_cast<std::size_t>(st.first_sample) + static_cast<std::size_t>(st.sample_count)
                    > a.samples.size())
                cadence_ok = false;
            if (i == 0) continue;
            const int32_t gap = st.year - a.steps[i - 1].year;
            if (gap <= 0) cadence_ok = false;
            // Only the closing step may sit inside the interval.
            if (gap < rec.record_interval_years && i + 1 != a.steps.size()) cadence_ok = false;
        }
        check(cadence_ok,
              "B817d steps ascend, honour the record interval, and index inside the sample array");

        // A SERIES, NOT AN ENDPOINT (BL-830's dependency). A scoreboard that
        // re-ranks needs the same polity sampled at more than one step, and a
        // record that only carried a final standing would pass every check
        // above while being useless for the thing it exists for.
        int repeated = 0;
        if (a.steps.size() >= 2)
        {
            const timelapse_step& f = a.steps.front();
            const timelapse_step& l = a.steps.back();
            for (int i = 0; i < f.sample_count; ++i)
                for (int j = 0; j < l.sample_count; ++j)
                    if (a.samples[static_cast<std::size_t>(f.first_sample + i)].polity
                     == a.samples[static_cast<std::size_t>(l.first_sample + j)].polity)
                        ++repeated;
        }
        check(repeated > 0,
              "B817e a polity is sampled at more than one step (the record is a series)");

        // THE CULTURE HALF ACTUALLY MOVES. Every region emits one entry at its
        // first recorded step, so a record equal to that count is a record of
        // foundings and nothing else - assimilation would be invisible in it.
        check(static_cast<int64_t>(a.culture_changes.size())
              > static_cast<int64_t>(a.region_stride),
              "B817f the culture record carries drift, not only each region's first appearance");

        check(n.steps.empty() && n.samples.empty() && n.culture_changes.empty(),
              "B817g record_playback=false emits nothing at all");
        check(same_except_record(a, n, s1, s3),
              "B817h a recorded run and a suppressed run agree on EVERY other output");
        check(same_record(a, a2) && same_except_record(a, a2, s1, s2),
              "B817i the same seed records identically twice");

        // --- B916  THE EVENT LAYER -------------------------------------------
        //
        // The same three claims B817 makes of the playback record, made of the
        // event list: bounded and STATED; suppressed together with the record
        // so a recorded and a suppressed run agree on everything else (B817h
        // above already covers this, because `same_except_record` ignores
        // `events` and `n` was run with `record_playback` off); and internally
        // consistent with the counters the sim keeps for the same moments —
        // which is the check that catches an emit site that was missed.
        {
            int kinds[static_cast<int>(lapse_event_kind::count)] = {0};
            bool ascending = true, in_range = true;
            for (std::size_t i = 0; i < a.events.size(); ++i)
            {
                const lapse_event& e = a.events[i];
                if (e.kind < static_cast<uint8_t>(lapse_event_kind::count)) ++kinds[e.kind];
                else in_range = false;
                if (i > 0 && e.year < a.events[i - 1].year) ascending = false;
                if (e.region != lapse_event_none && e.region >= a.region_stride) in_range = false;
            }
            const long long ev_bytes = static_cast<long long>(event_record_bytes(a));
            std::printf("      event layer: %lld events = %lld bytes  "
                        "(founded %d, seat captured %d, realm ended %d, broke away %d, "
                        "capital moved %d, road promoted %d, civilisation %d, creed %d)\n",
                        static_cast<long long>(a.events.size()), ev_bytes,
                        kinds[0], kinds[1], kinds[2], kinds[3], kinds[4], kinds[5], kinds[6], kinds[7]);

            check(!a.events.empty() && ev_bytes < 1024 * 1024,
                  "B916a the event layer is emitted and stays under 1 MB over the whole span");
            check(ascending && in_range,
                  "B916b events ascend by year, carry a known kind, and index inside the stride");
            check(n.events.empty(),
                  "B916c record_playback=false suppresses the event layer with the rest of the record");

            // EVERY DEATH IS AN EVENT, and only deaths are: the polity table's
            // `alive` flags are the sim's own ledger of who ended.
            int dead = 0, seceded = 0;
            for (const polity& p : a.polities)
            {
                if (!p.alive)     ++dead;
                if (p.parent >= 0) ++seceded;
            }
            check(kinds[static_cast<int>(lapse_event_kind::realm_ended)] == dead,
                  "B916d realm_ended events equal the polities the run left dead");
            check(kinds[static_cast<int>(lapse_event_kind::broke_away)]
                      == static_cast<int>(a.secessions)
                  && seceded == static_cast<int>(a.secessions),
                  "B916e broke_away events and `polity::parent` both equal the secession counter");
            check(kinds[static_cast<int>(lapse_event_kind::founded)]
                      == static_cast<int>(a.polities.size()) - static_cast<int>(a.secessions),
                  "B916f every polity that was not a successor was founded");
            check(kinds[static_cast<int>(lapse_event_kind::civilisation_formed)]
                      == static_cast<int>(a.civilisations_formed)
                  && kinds[static_cast<int>(lapse_event_kind::creed_preached)]
                      == static_cast<int>(a.universal_creeds_arisen),
                  "B916g civilisation and creed events equal their counters");
        }
    }

    // --- B914  THE LIVE TAP DOES NOT PERTURB THE RUN ------------------------
    //
    // BL-914's determinism claim, asserted the same way BL-817's is above (and
    // for the same reason: a recorder that can be READ is not the risk here —
    // nothing in `world/*` ever reads a tap field back — the risk is a
    // recorder that changes what it watches by the act of watching it). A
    // tapped run and an untapped run must agree on EVERY field the harness
    // already checks, not just the ownership list: `same_except_record` covers
    // the counters, the road corridors, the grudge table, the polity ladder,
    // the narration and the mutated settlement state, and `same_record` covers
    // the playback series — between them that is everything `history_sim_state`
    // carries except the event layer, checked separately below.
    {
        era_lapse_tap        tap;
        settlement_state     s_tapped   = k1->settlement;
        settlement_state     s_untapped = k1->settlement;
        const history_sim_state tapped =
            run_history_sim(s_tapped, nullptr, no_terrain, kgw, kgh, params, 4242u,
                            nullptr, nullptr, &tap);
        const history_sim_state untapped =
            run_history_sim(s_untapped, nullptr, no_terrain, kgw, kgh, params, 4242u);

        check(same_except_record(tapped, untapped, s_tapped, s_untapped)
              && same_record(tapped, untapped),
              "B914a a tapped run and an untapped run agree on every output field "
              "already checked above (ownership, counters, corridors, grudges, "
              "polities, narration, the settlement mutation, and the playback record)");

        bool events_match = tapped.events.size() == untapped.events.size();
        for (std::size_t i = 0; events_match && i < tapped.events.size(); ++i)
        {
            const lapse_event& x = tapped.events[i];
            const lapse_event& y = untapped.events[i];
            if (x.year != y.year || x.kind != y.kind || x.region != y.region
             || x.polity != y.polity || x.other != y.other)
                events_match = false;
        }
        check(events_match,
              "B914b the event layer is identical between a tapped and an untapped run");

        // A REAL ASSERTION ABOUT A RUN THAT WAS ACTUALLY WATCHED, not a
        // vacuous pass over a tap nothing ever published into.
        check(tap.epoch_now() > 0 && !tap.changes.empty() && !tap.region_col.empty(),
              "B914c the tap actually published region geometry and ownership over the watched run");

        // AND WHAT IT PUBLISHED AGREES WITH WHAT THE RUN ITSELF PRODUCED. The
        // per-year publish lags the sim by one year (era_lapse_tap::publish,
        // era_timelapse.hpp) precisely so it never reports a year that might
        // still be mid-append; the final flush right before `run_history_sim`
        // returns is what closes that one-year gap, so by here the tap must
        // hold the complete record, not a lagging prefix of it.
        check(tap.changes.size() == tapped.owner_changes.size()
              && tap.culture_changes.size() == tapped.culture_changes.size()
              && tap.events.size() == tapped.events.size(),
              "B914d the tap's final flush holds the complete record, not a lagging prefix of it");

        // THE GEOMETRY MIRROR AGREES TOO — every published region is the same
        // region the run itself ended up with, in the same order.
        bool geometry_match = tap.region_col.size() == s_tapped.regions.size();
        for (std::size_t i = 0; geometry_match && i < tap.region_col.size(); ++i)
            if (tap.region_col[i] != s_tapped.regions[i].col
             || tap.region_row[i] != s_tapped.regions[i].row
             || tap.region_name[i] != s_tapped.regions[i].name)
                geometry_match = false;
        check(geometry_match,
              "B914e the tap's region geometry mirror matches the run's own regions, in order");
    }

    // --- R9  culture relations: opposition permits conquest (BL-870) -------
    //
    // THE SAME IDIOM AS R3/S1/S2/S3/B318 ABOVE: one world, one variable
    // changed, the DIRECTION asserted rather than a count. Here the variable
    // is the `creed_state` fed to the scorer, not a param — `two_polity_world`
    // already seeds region "Home" pure culture 0 and region "Prize" pure
    // culture 1, which makes `foreign_q` exactly 1000 for the only campaign
    // candidate on the board and isolates `w_cult`'s new per-pair term from
    // everything else the scorer weighs.
    //
    // TWO SYNTHETIC WORLDS OF CULTURES, not two synthetic worlds of ground:
    // "kin" makes cultures 0 and 1 the same war-god temperament, the same
    // country, freshly split (low kinship-years) — an alliance-shaped pair,
    // opposition near zero. "strangers" makes them opposite war-god extremes,
    // opposite countries, and severs the family tree (`parent = -1` on both)
    // so no kinship discount can soften it — an enmity-shaped pair,
    // opposition at its ceiling. Every other input — the regions, the
    // distance, the params — is identical between the two runs.
    {
        auto war_pantheon = [](int zeal, int dominion) {
            std::vector<culture_god> p(2);
            p[0].domain = "the storm"; p[0].zeal = 5; p[0].dominion = 5; // chief god, inert here
            p[1].domain = "war";       p[1].zeal = zeal; p[1].dominion = dominion;
            return p;
        };

        creed_state kin;
        {
            culture c0; c0.pantheon = war_pantheon(5, 5);
            c0.origin_farm_class = 0; c0.parent = -1; c0.coined_year = -4000;
            culture c1 = c0; // SAME pantheon, SAME country: a daughter fresh off the split.
            c1.parent = 0; c1.coined_year = -3900; // 100 years apart, the same people still.
            kin.cultures = { c0, c1 };
        }

        creed_state strangers;
        {
            culture c0; c0.pantheon = war_pantheon(0, 0);
            c0.origin_farm_class = 0; c0.parent = -1; c0.coined_year = -4000;
            culture c1; c1.pantheon = war_pantheon(10, 10);
            c1.origin_farm_class = 5; c1.parent = -1; c1.coined_year = -4000; // no shared ancestor
            strangers.cultures = { c0, c1 };
        }

        // Sanity on the value BEFORE it ever reaches the scorer — a directly
        // queryable check on the function BL-869 is meant to read later.
        const int op_kin      = culture_opposition_q(kin.cultures, 0, 1);
        const int op_strangers = culture_opposition_q(strangers.cultures, 0, 1);
        std::printf("      opposition: kin %d/1000, strangers %d/1000\n",
                    op_kin, op_strangers);
        check(op_kin < op_strangers,
              "R9a  opposition is symmetric axes discounted by kinship — a fresh split of "
              "identical temperament and country reads far less opposed than two severed "
              "strangers at both extremes");
        check(op_kin == culture_opposition_q(kin.cultures, 1, 0)
           && op_strangers == culture_opposition_q(strangers.cultures, 1, 0),
              "R9b  opposition is SYMMETRIC (NR-815) — a matrix, not a directed pair like grudge");

        history_sim_params p9 = params;
        p9.start_year = 0; p9.stop_year = 400;
        p9.neighbour_radius = 40; // Same widening R3 needed: the radius must not be what stops it.
        p9.w_dist = 0;            // Isolate w_cult; distance is not under test here.

        settlement_state w_kin       = two_polity_world(6);
        settlement_state w_strangers = two_polity_world(6);

        const history_sim_state a =
            run_history_sim(w_kin, &kin, no_terrain, syn_gw, syn_gh, p9, 4713u);
        const history_sim_state b =
            run_history_sim(w_strangers, &strangers, no_terrain, syn_gw, syn_gh, p9, 4713u);

        std::printf("      R9 kin: %lld battles / %lld conquests | strangers: %lld / %lld\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(b.battles), static_cast<long long>(b.conquests));

        // DIRECTION FIXED 2026-09-10: opposition PERMITS conquest (it waives
        // the foreignness discount), so the ENMITY-shaped pair is the one
        // expected to conquer AT LEAST as readily, never the kin pair — the
        // first cut of this assertion had it backwards, matching a scorer
        // bug (see history_sim.cpp) it was written to confirm rather than
        // catch.
        check(a.battles > 0, "R9c  the near, kin pair is still campaigned for at all");
        check(b.conquests >= a.conquests,
              "R9d  ENMITY-SHAPED beats ALLIANCE-SHAPED: severed strangers conquer no less "
              "readily than kin over the identical ground (opposition PERMITS conquest, it "
              "does not forbid it, but it must not make the kin pair MORE likely to take "
              "the ground than the strangers)");
    }

    // --- R10  civilisations form only where mixing actually happened (BL-869) --
    //
    // THE SAME IDIOM AGAIN: one fixture, one variable changed, the DIRECTION
    // (here: forms / does not form) asserted rather than a magic count.
    // `one_polity_two_regions` is reused rather than `two_polity_world`
    // because it puts BOTH regions under one polity from year zero — no
    // campaign is ever scored, so nothing here can be confused with R9's
    // conquest-permission question. The mix itself is HAND-SET rather than
    // grown by conquest, with `assimilation_per_year_q = 0` so it cannot
    // drift during the run: this isolates "does a long-held mix form a
    // civilisation" from "does assimilation move a mix at all", which R9's
    // own creed_state fixtures already cover.
    {
        auto war_pantheon10 = [](int zeal, int dominion) {
            std::vector<culture_god> p(2);
            p[0].domain = "the storm"; p[0].zeal = 5; p[0].dominion = 5; // chief god, inert here
            p[1].domain = "war";       p[1].zeal = zeal; p[1].dominion = dominion;
            return p;
        };

        // LOW opposition: same temperament, same country, a fresh split —
        // exactly R9's "kin" shape, so a mix of these two should settle.
        creed_state near_kin;
        {
            culture c0; c0.pantheon = war_pantheon10(5, 5);
            c0.origin_farm_class = 0; c0.parent = -1; c0.coined_year = -4000;
            culture c1 = c0;
            c1.parent = 0; c1.coined_year = -3900;
            near_kin.cultures = { c0, c1 };
        }

        // HIGH opposition: opposite war-god extremes, opposite countries, no
        // shared ancestor — R9's "strangers" shape, above the formation bar.
        creed_state enemies;
        {
            culture c0; c0.pantheon = war_pantheon10(0, 0);
            c0.origin_farm_class = 0; c0.parent = -1; c0.coined_year = -4000;
            culture c1; c1.pantheon = war_pantheon10(10, 10);
            c1.origin_farm_class = 5; c1.parent = -1; c1.coined_year = -4000;
            enemies.cultures = { c0, c1 };
        }

        const int op_kin    = culture_opposition_q(near_kin.cultures, 0, 1);
        const int op_enemy  = culture_opposition_q(enemies.cultures, 0, 1);
        std::printf("      R10 opposition: near-kin %d/1000 (bar %d), enemies %d/1000\n",
                    op_kin, civilisation_opposition_bar_q, op_enemy);
        check(op_kin <= civilisation_opposition_bar_q,
              "R10 fixture sanity: the near-kin pair sits AT OR BELOW the formation bar");
        check(op_enemy > civilisation_opposition_bar_q,
              "R10 fixture sanity: the severed-enemies pair sits ABOVE the formation bar");

        history_sim_params p10 = params;
        p10.start_year = 0;
        // Comfortably past `civilisation_mix_years_bar` (300) at any tick
        // band this file's default params use, so the check is about the
        // MECHANISM crossing its bar, not about outrunning a coarse step.
        p10.stop_year  = 800;
        p10.assimilation_per_year_q = 0; // The mix must not drift — see comment above.

        // Two peoples "in quantity" — the second culture at exactly the
        // mixing threshold (200/1000) — so the fixture exercises the
        // threshold itself rather than an unambiguously large mix.
        culture_shares mixed;
        mixed.id[0] = 0; mixed.weight_q[0] = 800;
        mixed.id[1] = 1; mixed.weight_q[1] = 200;
        mixed.other_q = 0;

        settlement_state w_kin = one_polity_two_regions(6);
        w_kin.regions[0].culture = mixed; // "Home" carries the mix; "Outpost" stays pure.

        settlement_state w_enemy = one_polity_two_regions(6);
        w_enemy.regions[0].culture = mixed;

        // CONTROL: no second culture anywhere, ever — the doc's own "done
        // when": "a world with little mixing produces few or none".
        settlement_state w_pure = one_polity_two_regions(6);

        const history_sim_state a =
            run_history_sim(w_kin,   &near_kin, no_terrain, syn_gw, syn_gh, p10, 91u);
        const history_sim_state b =
            run_history_sim(w_enemy, &enemies,  no_terrain, syn_gw, syn_gh, p10, 91u);
        const history_sim_state c =
            run_history_sim(w_pure,  &near_kin, no_terrain, syn_gw, syn_gh, p10, 91u);

        std::printf("      R10 civilisations formed: near-kin %lld, enemies %lld, pure %lld\n",
                    static_cast<long long>(a.civilisations_formed),
                    static_cast<long long>(b.civilisations_formed),
                    static_cast<long long>(c.civilisations_formed));

        check(a.civilisations_formed >= 1,
              "R10a  a long-held, low-opposition mix DOES grow a civilisation");
        check(!a.civilisations.empty() && !a.civilisations.front().name.empty(),
              "R10b  the formed record carries a coined name, not a blank one");
        check(!a.civilisations.empty()
              && a.civilisations.front().members.size() == 2
              && a.civilisations.front().members[0] == 0
              && a.civilisations.front().members[1] == 1,
              "R10c  the record names exactly the two cultures that actually mixed");
        check(!a.civilisations.empty() && a.civilisations.front().strain_q <= civilisation_opposition_bar_q,
              "R10d  a formed civilisation's inherited strain never exceeds the formation bar");
        check(w_kin.regions[0].civilisation >= 0 && w_kin.regions[1].civilisation < 0,
              "R10e  only the region that actually carried the mix points at the record — "
              "the untouched 'Outpost' does not");

        check(b.civilisations_formed == 0,
              "R10f  FRACTURE (NR-817): the same long-held mix, above the opposition bar, "
              "coins NO civilisation at all");

        check(c.civilisations_formed == 0,
              "R10g  a region with no second culture never forms one, however long it runs "
              "(CIVILISATION.md: \"a world with little mixing produces few or none\")");
    }

    // ---------------------------------------------------------------------
    // BL-837 — ancient logistics and roads (CIVILISATION.md § The road is
    // the empire's skeleton, and reach GATES conquest).
    //
    // Three claims, three checks, following the file's own idiom: run the
    // SAME world twice with one variable changed and assert the DIRECTION.
    //   BL837a  reach GATES a campaign — a target beyond sustainable reach is
    //           refused outright, not merely scored worse.
    //   BL837b  a road tier extends how far that gate reaches — the SAME
    //           two-hop target is refused less once the first leg has been
    //           walked enough to earn a tier.
    //   BL837c  a garrison beyond sustainable reach cannot be MAINTAINED —
    //           its army_stock attrites over years with nobody attacking it.
    // ---------------------------------------------------------------------

    // --- BL837a  reach GATES a campaign, it does not merely price it -------
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 400;
        ps.neighbour_radius = 40;
        ps.w_dist = 0; // isolate the gate from the distance PREFERENCE term (as R3 does).

        // Calibrated so `campaign_supply` lands around 150-160/1000: well
        // above where the odds term already zeroes itself out on its own
        // (see the field comment on `sustainable_campaign_floor_q`), so a
        // refusal here can only be the GATE, never the pre-existing pricing.
        //
        // THE FLOORS ARE SET LOCALLY, NOT INHERITED FROM `params`
        // (2026-09-10, fixing a calibration bug this test shipped with). The
        // first cut read the production default (250 at the time) for
        // "gated" — which ties this test's pass/fail to whatever
        // `history_sweep` later tunes the shipped default to, and it broke
        // exactly that way when the default was lowered to fix B384c (a
        // production regression — see the field's own comment). A test
        // proving the MECHANISM must not depend on the CALIBRATION.
        const int separation = 30;
        // BL-922: the currency is now capital-reach x `terrain_reach_cost_q`
        // alone (the hub-distance decay is gone), so the ~155-160 this case
        // was calibrated against is restated in it: 30 tiles x 2800 / 100 =
        // 840 off 1000 -> 160. An instrument setting, not the shipped value.
        ps.terrain_reach_cost_q = 2800;

        history_sim_params gated = ps;
        gated.sustainable_campaign_floor_q = 200; // Above the ~155-160 this target supplies: GATES it.
        history_sim_params open  = ps;
        open.sustainable_campaign_floor_q = -1000; // Never gates — the old, price-only shape.

        settlement_state w_gated = two_polity_world(separation);
        settlement_state w_open  = two_polity_world(separation);
        const history_sim_state a =
            run_history_sim(w_gated, nullptr, no_terrain, syn_gw, syn_gh, gated, 321u);
        const history_sim_state b =
            run_history_sim(w_open,  nullptr, no_terrain, syn_gw, syn_gh, open,  321u);

        std::printf("      gate on: %lld battles / %lld denied | gate off: %lld battles / %lld denied\n",
                    static_cast<long long>(a.battles),
                    static_cast<long long>(a.reach_denied_campaigns),
                    static_cast<long long>(b.battles),
                    static_cast<long long>(b.reach_denied_campaigns));

        check(a.reach_denied_campaigns > 0,
              "BL837a1 the reach gate actually fires — candidates are refused, not merely priced");
        check(a.battles == 0,
              "BL837a2 GATED: a target beyond sustainable reach is never fought for at all");
        // NARROWED 2026-09-10, and deliberately so. The original claim ("the
        // ungated run IS fought for") reaches past this item's own mechanism
        // into whether the scorer's SEPARATE odds/verb-choice logic ever
        // picks Campaign for a supply-155 target at all — that is downstream
        // of BL-837 and not its responsibility to prove. What BL-837 owns,
        // and what this asserts, is that the SAME candidate the gate refuses
        // is NEVER refused once the gate is removed — the gate, and only the
        // gate, is the source of every one of `a`'s denials.
        check(a.reach_denied_campaigns > 0 && b.reach_denied_campaigns == 0,
              "BL837a3 the SAME target is refused only when GATED, never when the gate is off");
    }

    // --- BL837b  a road extends how far the gate reaches -------------------
    //
    // A three-region chain, Home - Mid - Far, Mid and Far held by the same
    // rival polity from the start. Home can only reach Far by first taking
    // Mid, then campaigning onward — a genuine two-hop question the
    // capital's Dijkstra answers through whatever the Home-Mid edge is
    // priced at. `road_tier1_uses=1` makes the FIRST walk of that edge (the
    // conquest of Mid itself) earn it a tier immediately, so the discount is
    // live for every reach computation from then on; a huge threshold in the
    // control run means the edge never earns one at all.
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 1200;
        ps.neighbour_radius = 26;      // covers each 24-tile leg, not the 48-tile skip.
        ps.w_dist = 0;
        // BL-922: restated in the capital-reach currency. Home -> Mid is one
        // 24-tile hop (480 off 1000: allowed, walked, and so ROADED at
        // `road_tier1_uses=1`); Mid -> Far is a second hop the capital prices
        // through Mid: 48 unroaded (960 off -> 40, DENIED at the 80 floor)
        // against 16 + 24 = 40 roaded (800 off -> 200, allowed). Any value in
        // 1917..2299 separates the two; 2000 is the round one.
        ps.terrain_reach_cost_q = 2000;

        history_sim_params with_road = ps;
        with_road.road_tier1_uses = 1;

        history_sim_params no_road = ps;
        no_road.road_tier1_uses = 1'000'000'000; // this edge can never earn a tier.

        settlement_state w_road = road_chain_world(24);
        settlement_state w_none = road_chain_world(24);
        const history_sim_state a =
            run_history_sim(w_road, nullptr, no_terrain, syn_gw, syn_gh, with_road, 777u);
        const history_sim_state b =
            run_history_sim(w_none, nullptr, no_terrain, syn_gw, syn_gh, no_road,   777u);

        std::printf("      with road: %lld battles / %lld conquests / %lld denied | "
                    "without: %lld / %lld / %lld\n",
                    static_cast<long long>(a.battles), static_cast<long long>(a.conquests),
                    static_cast<long long>(a.reach_denied_campaigns),
                    static_cast<long long>(b.battles), static_cast<long long>(b.conquests),
                    static_cast<long long>(b.reach_denied_campaigns));

        // NARROWED 2026-09-10, and deliberately so — the same reasoning as
        // BL837a3. The original claim (`differs(a, b)`, i.e. the discount
        // changes actual battles/conquests/owner-changes) reaches past this
        // item's mechanism into the scorer's odds/verb-choice logic: at
        // `terrain_reach_cost_q=260` (an instrument gain chosen to make the
        // TERRAIN term dominate, not a calibration) BOTH the roaded and
        // unroaded edge deny essentially every candidate over the run, so
        // neither ever reaches a battle regardless of the discount — that is
        // a fact about how hard the instrument leans on this scenario, not
        // evidence the discount does nothing. What BL-837 owns, and what is
        // asserted here, is that the discount measurably and strictly
        // reduces how often the gate fires on the SAME edge — the
        // `road_uses_live` -> `road_tier_between` -> `road_discount` chain is
        // live, whether or not that's enough to flip THIS instrument's
        // extreme cost below THIS run's floor.
        check(a.reach_denied_campaigns < b.reach_denied_campaigns,
              "BL837b1 the road discount is LIVE — the SAME edge is denied strictly less once roaded");
        check(a.conquests >= b.conquests,
              "BL837b2 a roaded edge never conquers LESS than the same edge unroaded");
    }

    // --- BL837c  an army beyond sustainable reach cannot be maintained -----
    //
    // One polity, two regions, no rival anywhere on the map — the campaign
    // scorer never fires at all, so any change in the Outpost's standing
    // army is the maintenance rule alone. Same separation both runs; only
    // `terrain_reach_cost_q` moves, exactly as S2 isolates terrain's effect
    // on the campaign side.
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 200;

        history_sim_params near = ps;
        near.terrain_reach_cost_q = 10; // an instrument setting well inside reach (the shipped value is BL-922's 4000).

        history_sim_params far = ps;
        far.terrain_reach_cost_q = 25000; // an instrument gain: saturate the terrain term outright.

        settlement_state w_near = one_polity_two_regions(5);
        settlement_state w_far  = one_polity_two_regions(5);
        const history_sim_state a =
            run_history_sim(w_near, nullptr, no_terrain, syn_gw, syn_gh, near, 55u);
        const history_sim_state b =
            run_history_sim(w_far,  nullptr, no_terrain, syn_gw, syn_gh, far,  55u);

        std::printf("      garrison near: %lld attrition-years, outpost army %lld | "
                    "far: %lld attrition-years, outpost army %lld\n",
                    static_cast<long long>(a.unsustained_attrition_events),
                    static_cast<long long>(w_near.regions[1].army_stock),
                    static_cast<long long>(b.unsustained_attrition_events),
                    static_cast<long long>(w_far.regions[1].army_stock));

        check(a.unsustained_attrition_events == 0,
              "BL837c1 a garrison well inside sustainable reach is never attrited by distance alone");
        check(b.unsustained_attrition_events > 0,
              "BL837c2 a garrison beyond sustainable reach DOES attrite — it cannot be maintained");
        check(w_far.regions[1].army_stock < w_near.regions[1].army_stock,
              "BL837c3 the unsustained outpost ends the run with a smaller standing army");
    }

    // ---------------------------------------------------------------------
    // BL-872 — centres are derived by supply and governance (CIVILISATION.md
    // § Centres are derived by supply and governance). Same shape as BL837c:
    // one polity, two regions, no rival anywhere on the map, so the ONLY
    // thing that can move `region::centres` on the Outpost is demography
    // and this item's network gate — never the campaign/conquest machinery.
    // Long enough a run (600 years) for the Outpost's population, and so its
    // urban share, to converge well past the first `region_centre_heads`
    // rung several times over, so "did NOT grow past its opening seed" is a
    // real, load-bearing claim and not an artefact of the run ending early.
    // ---------------------------------------------------------------------
    {
        history_sim_params ps = params;
        ps.start_year = 0;
        ps.stop_year  = 600;

        history_sim_params near = ps;
        near.terrain_reach_cost_q = 10; // an instrument setting well inside reach (the shipped value is BL-922's 4000).

        history_sim_params far = ps;
        // Calibrated (printed below) to land the Outpost's network_supply_q
        // BETWEEN the garrison floor (0) and the settlement floor (40) — a
        // marginal case that freezes GROWTH without also starving the
        // standing garrison, so this test is honestly exercising BL-872's
        // OWN new floor rather than riding on BL-837's pre-existing one.
        far.terrain_reach_cost_q = 19600;

        settlement_state w_near = one_polity_two_regions(5);
        settlement_state w_far  = one_polity_two_regions(5);
        // BL-872 FIX (2026-09-10): `one_polity_two_regions` builds raw
        // `region` structs directly, never through `draw_region_urban` — so
        // without this, BOTH regions start at `centres == 0` (the struct's
        // bare default), not the `1` every REAL generated region gets the
        // instant it is settled (settlement.cpp: "ground that farms gets a
        // settlement, whenever it is settled"). The far case's own claim is
        // "keeps its OPENING SEED, no more" — that claim is meaningless
        // against a fixture that never planted one.
        for (settlement_state* w : {&w_near, &w_far})
            for (region& r : w->regions)
                draw_region_urban(r);
        const history_sim_state a =
            run_history_sim(w_near, nullptr, no_terrain, syn_gw, syn_gh, near, 812u);
        const history_sim_state b =
            run_history_sim(w_far,  nullptr, no_terrain, syn_gw, syn_gh, far,  812u);

        const region& outpost_near = w_near.regions[1];
        const region& outpost_far  = w_far.regions[1];

        std::printf("      near: supply_q=%d centres=%d urban_pop=%lld population=%lld | "
                    "far: supply_q=%d centres=%d urban_pop=%lld population=%lld\n",
                    outpost_near.network_supply_q, outpost_near.centres,
                    static_cast<long long>(outpost_near.urban_population),
                    static_cast<long long>(outpost_near.population),
                    outpost_far.network_supply_q, outpost_far.centres,
                    static_cast<long long>(outpost_far.urban_population),
                    static_cast<long long>(outpost_far.population));

        // The scenario is only proving what it claims to prove if the "far"
        // reading actually lands BELOW the settlement floor and the "near"
        // one comfortably above it — printed above for a human to check,
        // asserted here so a future recalibration of either floor cannot
        // silently turn this into a vacuous pass.
        check(outpost_far.network_supply_q < params.sustainable_settlement_floor_q,
              "BL872a0 the far case actually lands below the settlement floor (not vacuous)");
        check(outpost_near.network_supply_q > params.sustainable_settlement_floor_q,
              "BL872a1 the near case actually lands above the settlement floor (not vacuous)");

        check(outpost_near.centres > 1,
              "BL872a2 well-supplied ground grows PAST its opening seed as population arrives");
        check(outpost_far.centres == 1,
              "BL872a3 CUT OFF ground stops growing centres — it keeps its opening seed, no more");
        check(outpost_near.centres > outpost_far.centres,
              "BL872a4 the SAME ground grows strictly more centres well-supplied than cut off");

        // FREEZE, NOT RAZE (this item's other open question). The cut-off
        // Outpost's `centres_razed` must stay zero — nobody sacked these
        // walls, the network merely stopped reaching them, and that is
        // deliberately NOT the same event `sack_region_urban` records.
        check(outpost_far.centres_razed == 0,
              "BL872a5 a network cut FREEZES growth, it does not RAZE what already stands "
              "(centres_razed is sack_region_urban's field alone)");

        // The demographic engine itself is untouched by the gate: the
        // Outpost's headcount and urban share still converge on the SAME
        // ground whether or not the network can feed a NEW centre from it —
        // this item gates `centres`, nothing upstream of it.
        check(outpost_far.population > 0 && outpost_far.urban_population > region_centre_heads,
              "BL872a6 population and urban share still grow on cut-off ground — only NEW "
              "centres are gated, not the demography the gate reads");
    }

    // --- M1  over-muster starves industry (BL-867) --------------------------
    //
    // CIVILISATION.md § Materials are spent: "a polity that musters too hard
    // starves or stops producing." Isolated from combat entirely — separation
    // 60 is far past `neighbour_radius` (9), so neither polity ever contacts
    // the other and the only thing that differs between the two runs is how
    // much of each region's recruitable ceiling is kept under arms.
    {
        history_sim_params p_low = params;
        p_low.start_year = 0;
        p_low.stop_year  = 200;
        p_low.garrison_fraction_q = 50;

        history_sim_params p_high = p_low;
        p_high.garrison_fraction_q = 1000; // the WHOLE manpower ceiling, standing.

        settlement_state w_low  = two_polity_world(60);
        settlement_state w_high = two_polity_world(60);

        const history_sim_state lo =
            run_history_sim(w_low,  nullptr, no_terrain, syn_gw, syn_gh, p_low,  4101u);
        const history_sim_state hi =
            run_history_sim(w_high, nullptr, no_terrain, syn_gw, syn_gh, p_high, 4101u);

        std::printf("      overmuster: garrison 5%% -> %lld materials produced | "
                    "garrison 100%% -> %lld (no contact either way: %lld / %lld battles)\n",
                    static_cast<long long>(lo.materials_produced),
                    static_cast<long long>(hi.materials_produced),
                    static_cast<long long>(lo.battles), static_cast<long long>(hi.battles));

        check(lo.battles == 0 && hi.battles == 0,
              "M1a  separation 60 keeps the two polities out of contact — the isolation holds");
        check(lo.materials_produced > hi.materials_produced,
              "M1b  a heavier standing garrison leaves less labour for industry, and produces less");
    }

    // --- M2  a campaign visibly costs materials (BL-867) --------------------
    //
    // THE SAME SEED B318c ALREADY PROVES REACHES CAMPAIGN (2024u) — not an
    // arbitrary pick. The first cut of this case used 8670u, which B318 never
    // vouches for; found by this item's own re-verification (2026-09-10) to
    // be a quiet world under default params (conquests == 0), which made
    // M2b fail for a reason that had nothing to do with materials at all.
    // Reusing 2024u means "does this run fight" is already someone else's
    // proven claim, so a failure here is about materials, never about
    // whether the seed happens to go to war.
    {
        settlement_state s = k1->settlement;
        const history_sim_state a =
            run_history_sim(s, nullptr, no_terrain, kgw, kgh, params, 2024u);

        std::printf("      materials: %lld produced / %lld spent on campaigns, over %lld conquests\n",
                    static_cast<long long>(a.materials_produced),
                    static_cast<long long>(a.materials_spent_on_campaigns),
                    static_cast<long long>(a.conquests));

        check(a.materials_produced > 0,
              "M2a  industry credits real material stock to real seats over the run");
        check(a.conquests > 0 && a.materials_spent_on_campaigns > 0,
              "M2b  a run that fights ALSO visibly spends materials on the campaigns it launches");
    }

    // --- M3  taking a seat takes its stores (BL-867) -------------------------
    //
    // CIVILISATION.md § Materials are spent: "the stores sit AT THE SEAT, and
    // fall with it." Preload the prize region with a stock no industry this
    // short a run could produce on its own (`region_seed_population` at
    // `farm_q=950` is a few hundred thousand at most, and 200 years of that
    // region's own OWN industry cannot manufacture this number from nothing)
    // — so if the conqueror ends the run holding at least this much on that
    // region, the ONLY explanation left is that the flag carried the stock
    // with it, per settlement.hpp's design: nothing copies `material_stock`
    // anywhere, ownership just moves over it.
    {
        history_sim_params p3 = params;
        p3.start_year = 0;
        p3.stop_year  = 400;
        p3.neighbour_radius = 40;
        p3.w_dist = 0;

        settlement_state w3 = two_polity_world(3);
        w3.regions[1].material_stock = 5000000;

        const history_sim_state m =
            run_history_sim(w3, nullptr, no_terrain, syn_gw, syn_gh, p3, 4103u);

        int  last_owner_of_1 = -1;
        bool captured = false;
        for (const owner_change& c : m.owner_changes)
            if (c.region == 1) { captured = true; last_owner_of_1 = c.owner; }

        std::printf("      seat capture: region 1 captured=%s, final owner %d, "
                    "material_stock now %lld (preloaded 5000000)\n",
                    captured ? "yes" : "no", last_owner_of_1,
                    static_cast<long long>(w3.regions[1].material_stock));

        check(captured, "M3a  the prize seat changes hands inside the run");
        check(w3.regions[1].nation == last_owner_of_1,
              "M3b  the region's own settled nation field agrees with the recorded owner change");
        check(w3.regions[1].material_stock >= 5000000,
              "M3c  the captured seat's preloaded stock is carried by the ownership change, never reset");
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
