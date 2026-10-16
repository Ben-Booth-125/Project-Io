#include "nation_ai.hpp"

#include "components.hpp"
#include "economy_system.hpp" // BL-1335: economy_report (the tax scorer's budget read)
#include "nation_step.hpp"     // BL-1335: set_nation_tax_tier
#include "hex_neighbors.hpp"
#include "nation_tech.hpp"     // BL-1275: the research pick's verb and reads
#include "placement_rules.hpp" // BL-1275: is_depositless_site (the named-site scope)
#include "recipe_registry.hpp"
#include "stance.hpp"
#include "world.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <map>
#include <utility>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------------------
// The nation scorer (BL-542). Design and grant terms: nation_ai.hpp.
// ---------------------------------------------------------------------------
// Read the header first — it carries the objective, the three terms, and the
// determinism contract. This file carries the arithmetic and THE MAPPING.
//
// ============================ THE MAPPING ==================================
// Three terms, nine authored lines. Which term pushes which line, and why:
//
//   LINE                    PUSHED BY            WHY
//   ----------------------  -------------------  ---------------------------
//   logistics_maintenance   niche_fit            A niche you cannot DELIVER is
//                                                not a niche. The road is what
//                                                turns "I have it" into "they
//                                                can get it" (LOGISTICS.md:
//                                                logistics is the road).
//   schooling               calm                 The longest-horizon civil
//                                                line there is. A nation with
//                                                an army on its border defers
//                                                it; that deferral IS the
//                                                conflict term's cost.
//   military_research       threat               Lowers the cost of the war it
//                                                could not avoid. Term 2 is a
//                                                negative on EXPECTED cost, and
//                                                cost has two factors — this is
//                                                the one that is not deterrence.
//   academic_research       niche_gap + calm     Two pushes, deliberately. The
//                                                gap half is "make what I
//                                                cannot dig up"; the calm half
//                                                is the same civil-horizon
//                                                argument as schooling, at half
//                                                the weight because research
//                                                pays back sooner.
//   public_exploration      niche_gap            The OTHER answer to a niche it
//                                                cannot fill: look for ground it
//                                                does not know it has
//                                                (DISCOVERY.md's survey).
//   contracted_force        threat               The nation's force lever. It
//                                                BUYS force rather than raising
//                                                it (CONTRACTS.md, BL-377) —
//                                                which is also what gives the
//                                                player's mercenary company a
//                                                counterparty with money.
//   strategic_reserve       threat               Goods held against disruption.
//                                                A threatened nation stops
//                                                trusting the market to be there
//                                                next quarter.
//   public_works            niche_fit + calm     Build the capacity the niche
//                                                needs; and a calm nation builds
//                                                at all.
//   charters                niche_fit            The sharpest niche line: pay a
//                                                corporation to exist somewhere
//                                                it otherwise would not, which
//                                                is exactly how a nation turns a
//                                                complement it HOLDS into one it
//                                                SUPPLIES.
//   space_programme         calm                 The state's stake in the gate
//                                                into space (BL-644): satellite
//                                                launches are prestige spending
//                                                on the longest horizon of all,
//                                                and the first line a nation
//                                                with an army on its border
//                                                defers — the same argument as
//                                                schooling, further out.
//
// Term 3 (grudges) appears in NO row of that table, and that is the point. It
// enters one step earlier, as a per-neighbour multiplier inside terms 1 and 2,
// so it can never become a goal of its own. With terms 1 and 2 at zero, a grudge
// changes nothing at all — nation_scorer_harness R5c asserts exactly that.
// ===========================================================================

namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/// Ascending nation ids. THE deterministic walk order and the stable index the
/// cadence stagger keys on — `corp_ai.cpp`'s `sorted_corp_ids`, one grain up.
std::vector<entity_id> sorted_nation_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// Per-body raster: grid_y * grid_width + grid_x -> tile entity (or null).
///
/// THE ONE UNORDERED WALK IN THIS FILE, and it is safe by construction rather
/// than by convention: every tile writes exactly one cell, addressed by its own
/// (body, grid_x, grid_y), so no cell is written twice, nothing accumulates, and
/// the finished raster is identical whatever order `w.tiles` hands them over in.
/// `world::body_tile_grid` builds the same thing, but takes a non-const world
/// (it memoises into `world::body_tile_index`) and the scorer is pure.
using raster_map = std::unordered_map<entity_id, std::vector<entity_id>>;

raster_map build_raster(const world& w)
{
    raster_map r;
    r.reserve(w.bodies.size());
    for (const auto& [body_id, bc] : w.bodies)
    {
        if (bc.grid_width <= 0 || bc.grid_height <= 0)
            continue;
        r.emplace(body_id, std::vector<entity_id>(
                               static_cast<std::size_t>(bc.grid_width) *
                                   static_cast<std::size_t>(bc.grid_height),
                               null_entity));
    }
    for (const auto& [tile_id, tc] : w.tiles)
    {
        const auto rit = r.find(tc.body);
        if (rit == r.end())
            continue;
        const auto bit = w.bodies.find(tc.body);
        if (bit == w.bodies.end())
            continue;
        const int gw = bit->second.grid_width;
        const int gh = bit->second.grid_height;
        if (tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        rit->second[static_cast<std::size_t>(tc.grid_y) * static_cast<std::size_t>(gw) +
                    static_cast<std::size_t>(tc.grid_x)] = tile_id;
    }
    return r;
}

/// The odd-r hex neighbour of `tile` on `side` (0..5), or null_entity when the
/// step leaves the grid. Columns WRAP (the body is a cylinder), rows do not —
/// the same rule `province.cpp` and `logistics.cpp` walk.
entity_id neighbour_tile(const world& w, const raster_map& r, entity_id tile, int side)
{
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return null_entity;
    const auto bit = w.bodies.find(tit->second.body);
    if (bit == w.bodies.end())
        return null_entity;
    const auto rit = r.find(tit->second.body);
    if (rit == r.end())
        return null_entity;

    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;
    const hex_neighbors::coord c =
        hex_neighbors::neighbour(tit->second.grid_x, tit->second.grid_y, side);
    if (c.gy < 0 || c.gy >= gh || gw <= 0)
        return null_entity;
    int nx = c.gx % gw;
    if (nx < 0)
        nx += gw;
    return rit->second[static_cast<std::size_t>(c.gy) * static_cast<std::size_t>(gw) +
                       static_cast<std::size_t>(nx)];
}

/// Nation of the tile, or null_entity for unclaimed ground (ocean, a body with
/// no nation pass).
entity_id tile_nation(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it == w.tile_to_nation.end() ? null_entity : it->second;
}

// ---------------------------------------------------------------------------
// The horizon: who borders whom, and by how much
// ---------------------------------------------------------------------------

/// nation -> (neighbour -> shared tile-edge count). std::map on both levels, so
/// every later walk over a nation's neighbours is in ascending id order without
/// a sort at each use. The count is an INTEGER accumulation, so it is
/// associative and the tile-walk order cannot reach it; the walk is over each
/// nation's own `tiles` vector regardless, which is stored order, not hashed.
using border_map = std::map<entity_id, std::map<entity_id, int>>;

border_map build_borders(const world& w, const raster_map& r)
{
    border_map borders;
    for (const entity_id nid : sorted_nation_ids(w))
    {
        const nation_component& nc = w.nations.at(nid);
        auto&                   row = borders[nid];
        for (const entity_id tile : nc.tiles)
        {
            for (int side = 0; side < 6; ++side)
            {
                const entity_id nb = neighbour_tile(w, r, tile, side);
                if (nb == null_entity)
                    continue;
                const entity_id other = tile_nation(w, nb);
                if (other == null_entity || other == nid)
                    continue;
                ++row[other];
            }
        }
    }
    return borders;
}

// ---------------------------------------------------------------------------
// Term 3 — the grudge
// ---------------------------------------------------------------------------
// WHAT THIS IS STANDING IN FOR. NATIONS.md § Settled 4 puts the Era -1 pair
// outcomes — who invaded whom, who never touched whom — behind BL-541
// (directional tariffs), which derives them at campaign setup. They do not exist
// in `world` today: there is no campaign-era nation-pair table of any kind. So
// the grudge here is derived from the residue that DOES survive generation, and
// it is the residue the pre-history actually wrote:
//
//   - the CONTESTED BORDER. NATION_GENERATION.md derives `expansionism` from a
//     "border-contest integral" precisely because a long shared border is where
//     the sim's pressure went. A nation that grew into empty land has short
//     borders; one that grew by pressing against a neighbour has long ones.
//   - the PAIR'S POSTURE. Two aggressive neighbours are a grudge; two passive
//     ones are not, however long the border between them.
//
// It is ONE FUNCTION on purpose. When BL-541 lands the real pair outcomes, this
// body is what changes and nothing else in the scorer moves.
//
// DELIBERATELY NOT USING `ideology`. Its enumerator comments already read as
// pair sentiment ("hostile to mercantile/technocratic neighbours"), which is
// tempting and wrong to spend here: NATIONS.md § Settled makes SENTIMENT the
// substrate (BL-545) and this scorer must not fork a second, incompatible
// sentiment model in the week before it lands.

float posture_level(expansionism e)
{
    switch (e)
    {
    case expansionism::passive: return 0.0f;
    case expansionism::moderate: return 1.0f;
    case expansionism::aggressive: return 2.0f;
    }
    return 0.0f;
}

float grudge_from_border(const world& w, entity_id a, entity_id b, int border_len,
                         const nation_ai_params& p)
{
    if (a == b || border_len <= 0)
        return 0.0f;
    const auto ait = w.nations.find(a);
    const auto bit = w.nations.find(b);
    if (ait == w.nations.end() || bit == w.nations.end())
        return 0.0f;

    const float sat = p.grudge_border_saturation > 0.0f ? p.grudge_border_saturation : 1.0f;
    const float contested = std::min(1.0f, static_cast<float>(border_len) / sat);
    const float posture =
        (posture_level(ait->second.posture) + posture_level(bit->second.posture)) / 4.0f;

    return clamp01(p.grudge_border_weight * contested + p.grudge_posture_weight * posture);
}

// ---------------------------------------------------------------------------
// Term 1 inputs — profile shares and the price of a niche
// ---------------------------------------------------------------------------

/// A nation's output profile as SHARES that sum to 1, never as magnitudes.
/// This is the line "a nation scores well by producing what others need and
/// cannot make, never by producing the most" expressed as arithmetic: scale a
/// nation's whole abundance array by any positive factor and every share — and
/// therefore `niche_fit` — is unchanged. Asserted by R3b.
std::array<float, resource_count> abundance_share(const nation_component& nc)
{
    std::array<float, resource_count> out{};
    float                             total = 0.0f;
    for (std::size_t i = 0; i < resource_count; ++i)
        total += std::max(0.0f, nc.resource_abundance[i]);
    if (total <= 0.0f)
        return out;
    for (std::size_t i = 0; i < resource_count; ++i)
        out[i] = std::max(0.0f, nc.resource_abundance[i]) / total;
    return out;
}

/// The price weight of each resource ON THIS NATION'S OWN GROUND — the mean
/// resolved price across the markets of the bodies it holds tiles on,
/// renormalised so the mean weight is ~1 and floored by `price_floor`.
///
/// TERRESTRIAL: it never reads a market on a body this nation has no tile on.
/// A niche in something nobody near it values is a weak niche, which is what the
/// floor keeps it as, rather than nothing.
std::array<float, resource_count> price_weights(const world& w, entity_id nation,
                                                const nation_ai_params& p)
{
    std::array<float, resource_count> out{};
    out.fill(1.0f);

    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return out;

    // Bodies this nation stands on, ascending and unique.
    std::vector<entity_id> bodies;
    for (const entity_id tile : nit->second.tiles)
    {
        const auto tit = w.tiles.find(tile);
        if (tit != w.tiles.end())
            bodies.push_back(tit->second.body);
    }
    std::sort(bodies.begin(), bodies.end());
    bodies.erase(std::unique(bodies.begin(), bodies.end()), bodies.end());
    if (bodies.empty())
        return out;

    // Markets on those bodies, ascending — the accumulation order.
    std::vector<entity_id> mids;
    for (const auto& kv : w.markets)
        if (std::binary_search(bodies.begin(), bodies.end(), kv.second.body))
            mids.push_back(kv.first);
    std::sort(mids.begin(), mids.end());
    if (mids.empty())
        return out;

    std::array<float, resource_count> sum{};
    for (const entity_id mid : mids)
    {
        const market_component& mc = w.markets.at(mid);
        for (std::size_t i = 0; i < resource_count; ++i)
            sum[i] += std::max(0.0f, mc.price[i]);
    }

    float mean = 0.0f;
    for (std::size_t i = 0; i < resource_count; ++i)
        mean += sum[i];
    mean /= static_cast<float>(resource_count);
    if (mean <= 0.0f)
        return out;

    for (std::size_t i = 0; i < resource_count; ++i)
        out[i] = std::max(p.price_floor, sum[i] / mean);
    return out;
}

// ---------------------------------------------------------------------------
// Term 2 inputs — force standing near a border
// ---------------------------------------------------------------------------

/// Corps registered in each nation, ascending. `corporation_component::
/// home_nation` is the registration, so this needs no building scan.
std::map<entity_id, std::vector<entity_id>> corps_by_nation(const world& w)
{
    std::map<entity_id, std::vector<entity_id>> out;
    std::vector<entity_id>                      cids;
    cids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        cids.push_back(kv.first);
    std::sort(cids.begin(), cids.end());
    for (const entity_id cid : cids)
    {
        const entity_id home = w.corporations.at(cid).home_nation;
        if (home != null_entity)
            out[home].push_back(cid);
    }
    return out;
}

struct force_tables
{
    /// nation -> force standing on its OWN ground. Deterrence.
    std::map<entity_id, float> own;
    /// threatened nation -> (nation the force stands in -> threatening force).
    /// Keyed by the SOURCE nation because the grudge discount is per-pair; a
    /// force on unclaimed ground keys on null_entity and gets no discount, which
    /// is the honest reading — an unattributable army on your border is fully
    /// threatening.
    std::map<entity_id, std::map<entity_id, float>> border;
};

force_tables build_force(const world& w, const raster_map& r, const nation_ai_params& p)
{
    force_tables t;

    const std::map<entity_id, std::vector<entity_id>> registered = corps_by_nation(w);

    std::vector<entity_id> uids;
    uids.reserve(w.units.size());
    for (const auto& kv : w.units)
        uids.push_back(kv.first);
    std::sort(uids.begin(), uids.end()); // pins the float accumulation below

    for (const entity_id uid : uids)
    {
        const unit_component& uc = w.units.at(uid);
        if (uc.count <= 0)
            continue;
        const float supply = static_cast<float>(uc.supply_factor_permille) / 1000.0f;
        const float raw    = p.force_scale * static_cast<float>(uc.count) * supply;
        if (raw <= 0.0f)
            continue;

        const entity_id home = tile_nation(w, uc.position);
        if (home != null_entity)
            t.own[home] += raw;

        // Which nations does this unit stand on the border OF? At most six, and
        // deduplicated: a unit in a salient touches one neighbour on three sides
        // and must count once, not three times.
        std::vector<entity_id> touched;
        for (int side = 0; side < 6; ++side)
        {
            const entity_id nb = neighbour_tile(w, r, uc.position, side);
            if (nb == null_entity)
                continue;
            const entity_id other = tile_nation(w, nb);
            if (other == null_entity || other == home)
                continue;
            touched.push_back(other);
        }
        std::sort(touched.begin(), touched.end());
        touched.erase(std::unique(touched.begin(), touched.end()), touched.end());

        for (const entity_id threatened : touched) // ascending: pinned
        {
            // PAIR-STATE (stance.hpp). A declared hostility toward a corp
            // registered in the threatened nation AMPLIFIES the force; it never
            // substitutes for it, so a hostile corp with no army near the border
            // threatens nothing and an undeclared army still threatens something.
            float       hostile_share = 0.0f;
            const auto  rit           = registered.find(threatened);
            if (rit != registered.end() && !rit->second.empty())
            {
                int hostile = 0;
                for (const entity_id target : rit->second) // ascending: pinned
                    if (is_hostile(w, uc.owner, target))
                        ++hostile;
                hostile_share =
                    static_cast<float>(hostile) / static_cast<float>(rit->second.size());
            }
            t.border[threatened][home] += raw * (1.0f + p.hostility_amplifier * hostile_share);
        }
    }
    return t;
}

} // namespace

// ---------------------------------------------------------------------------
// Public surface
// ---------------------------------------------------------------------------

bool nation_is_due(const world& w, entity_id n, int econ_tick, const nation_ai_params& p)
{
    if (w.nations.find(n) == w.nations.end())
        return false;
    const std::vector<entity_id> ids = sorted_nation_ids(w);
    const auto                   it  = std::lower_bound(ids.begin(), ids.end(), n);
    if (it == ids.end() || *it != n)
        return false;
    const int k     = std::max(1, p.cadence_k);
    const int index = static_cast<int>(it - ids.begin());
    // Index over the SORTED set, exactly as corp_ai.cpp keys its stagger. A
    // nation admitted with the highest id therefore lands in the last slot and
    // shifts nobody (R2b). A nation admitted with a LOWER id shifts everyone
    // above it by one — inherent to an index over a sorted set, true of
    // corp_ai's stagger too, and not something this file can fix without a
    // stored slot, which would be state the scorer is not allowed to hold.
    return (econ_tick % k) == (index % k);
}

float nation_grudge(const world& w, entity_id n, entity_id other, const nation_ai_params& p)
{
    if (n == other)
        return 0.0f;
    const auto nit = w.nations.find(n);
    if (nit == w.nations.end() || w.nations.find(other) == w.nations.end())
        return 0.0f;

    // Standalone accessor: builds its own raster and counts only THIS pair's
    // border. O(this nation's tiles); the sweep uses the shared `build_borders`
    // instead and never calls through here.
    const raster_map r = build_raster(w);
    int              border_len = 0;
    for (const entity_id tile : nit->second.tiles)
        for (int side = 0; side < 6; ++side)
        {
            const entity_id nb = neighbour_tile(w, r, tile, side);
            if (nb != null_entity && tile_nation(w, nb) == other)
                ++border_len;
        }
    return grudge_from_border(w, n, other, border_len, p);
}

entity_id nation_highest_grudge_neighbour(const world& w, entity_id n, const nation_ai_params& p)
{
    const auto nit = w.nations.find(n);
    if (nit == w.nations.end())
        return null_entity;

    // Standalone accessor, same shape as nation_grudge above: builds its own
    // raster and its own candidate set rather than reusing score_national_
    // budgets' shared build_borders, so a caller can ask this question for one
    // nation without a whole-world sweep.
    const raster_map r = build_raster(w);

    // std::map, so the argmax below walks ascending id with no separate sort —
    // seed_nation_garrisons' (nation_generation.cpp) own idiom for the same pick.
    std::map<entity_id, int> candidates;
    for (const entity_id tile : nit->second.tiles)
        for (int side = 0; side < 6; ++side)
        {
            const entity_id nb = neighbour_tile(w, r, tile, side);
            if (nb == null_entity)
                continue;
            const entity_id other = tile_nation(w, nb);
            if (other == null_entity || other == n)
                continue;
            ++candidates[other];
        }

    entity_id best        = null_entity;
    float     best_grudge = -1.0f;
    for (const auto& [other, len] : candidates) // ascending: strict '>' keeps the lowest tie
    {
        const float g = grudge_from_border(w, n, other, len, p);
        if (g > best_grudge)
        {
            best_grudge = g;
            best        = other;
        }
    }
    return best;
}

std::map<entity_id, nation_budget> score_national_budgets(const world& w,
                                                          const nation_ai_params& p,
                                                          int                     econ_tick,
                                                          nation_scorer_report*   report)
{
    std::map<entity_id, nation_budget> out;
    if (report)
        *report = nation_scorer_report{};

    const std::vector<entity_id> ids = sorted_nation_ids(w);
    if (ids.empty())
        return out;

    const raster_map   raster  = build_raster(w);
    const border_map   borders = build_borders(w, raster);
    const force_tables force   = build_force(w, raster, p);
    const int          k       = std::max(1, p.cadence_k);

    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        const entity_id         nid = ids[index];
        const nation_component& nc  = w.nations.at(nid);

        // A nation with no ground has no neighbours, no niche and no border, so
        // it has no opinion. Counted, not silently dropped.
        if (nc.tiles.empty())
        {
            if (report)
                ++report->skipped_landless;
            continue;
        }
        if (report)
            ++report->eligible;

        // Staggered cadence over the sorted set — see nation_is_due.
        if ((econ_tick % k) != (static_cast<int>(index) % k))
            continue;

        nation_score_terms terms;
        terms.nation = nid;

        // ---- The horizon ---------------------------------------------------
        const auto bit = borders.find(nid);
        if (bit != borders.end())
            for (const auto& [other, len] : bit->second) // std::map: ascending
                if (len > 0)
                    terms.neighbours.push_back(other);

        // ---- Term 1: niche fit --------------------------------------------
        // The neighbourhood profile is the mean over THIS NATION AND ITS
        // NEIGHBOURS and nothing wider — the terrestrial horizon, in the one
        // place where widening it would be easiest and most wrong. A neighbour
        // LACKS a resource when its share is below that local mean; this nation
        // can SUPPLY one when its own share is above it.
        const std::array<float, resource_count> a_share = abundance_share(nc);
        const std::array<float, resource_count> pw      = price_weights(w, nid, p);

        std::array<float, resource_count> mean_share{};
        for (std::size_t i = 0; i < resource_count; ++i)
            mean_share[i] = a_share[i];
        for (const entity_id other : terms.neighbours) // ascending: pinned
        {
            const std::array<float, resource_count> o_share =
                abundance_share(w.nations.at(other));
            for (std::size_t i = 0; i < resource_count; ++i)
                mean_share[i] += o_share[i];
        }
        {
            const float denom = static_cast<float>(terms.neighbours.size() + 1);
            for (std::size_t i = 0; i < resource_count; ++i)
                mean_share[i] /= denom;
        }

        float fit_raw = 0.0f;
        float gap_raw = 0.0f;
        for (const entity_id other : terms.neighbours) // ascending: pinned
        {
            const int   len    = bit->second.at(other);
            const float grudge = grudge_from_border(w, nid, other, len, p);
            // TERM 3, HALF ONE: a grudge makes this neighbour's niche less
            // attractive to complement. Multiplicative and per-neighbour, so it
            // scales a term rather than adding one.
            const float bias = std::max(0.0f, 1.0f - p.niche_grudge_bias * grudge);

            const std::array<float, resource_count> o_share =
                abundance_share(w.nations.at(other));

            float fit_n = 0.0f;
            float gap_n = 0.0f;
            for (std::size_t i = 0; i < resource_count; ++i) // authored enum order
            {
                const float lack = std::max(0.0f, (mean_share[i] - o_share[i]) * p.lack_scale);
                if (lack <= 0.0f)
                    continue;
                const float supply_edge = std::max(0.0f, a_share[i] - mean_share[i]);
                const float own_deficit = std::max(0.0f, mean_share[i] - a_share[i]);
                fit_n += supply_edge * lack * pw[i];
                gap_n += own_deficit * lack * pw[i];
            }
            fit_raw += bias * fit_n;
            gap_raw += bias * gap_n;
        }
        if (!terms.neighbours.empty())
        {
            const float n = static_cast<float>(terms.neighbours.size());
            const float ns = p.niche_scale > 0.0f ? p.niche_scale : 1.0f;
            const float gs = p.gap_scale > 0.0f ? p.gap_scale : 1.0f;
            terms.niche_fit = clamp01(fit_raw / (n * ns));
            terms.niche_gap = clamp01(gap_raw / (n * gs));
        }

        // ---- Term 2: conflict avoidance ------------------------------------
        float threat_raw = 0.0f;
        const auto fit_b = force.border.find(nid);
        if (fit_b != force.border.end())
            for (const auto& [source, amount] : fit_b->second) // std::map: ascending
            {
                terms.border_force += amount;
                float grudge = 0.0f;
                if (source != null_entity && bit != borders.end())
                {
                    const auto lit = bit->second.find(source);
                    if (lit != bit->second.end())
                        grudge = grudge_from_border(w, nid, source, lit->second, p);
                }
                // TERM 3, HALF TWO: a conflict with an old enemy is less costly
                // to ACCEPT, so its threat buys less avoidance. Same shape as
                // half one — a multiplier on an existing term, never a line.
                const float bias = std::max(0.0f, 1.0f - p.conflict_grudge_bias * grudge);
                threat_raw += bias * amount;
            }

        const auto own_it = force.own.find(nid);
        terms.own_force   = own_it == force.own.end() ? 0.0f : own_it->second;

        {
            const float damped =
                threat_raw / (1.0f + std::max(0.0f, p.deterrence_scale) * terms.own_force);
            const float ts = p.threat_scale > 0.0f ? p.threat_scale : 1.0f;
            terms.threat   = clamp01(damped / ts);
            terms.calm     = 1.0f - terms.threat;
        }

        // ---- Term 3, reported ----------------------------------------------
        if (!terms.neighbours.empty())
        {
            float g = 0.0f;
            for (const entity_id other : terms.neighbours) // ascending: pinned
                g += grudge_from_border(w, nid, other, bit->second.at(other), p);
            terms.mean_grudge = g / static_cast<float>(terms.neighbours.size());
        }

        // ---- The mapping ----------------------------------------------------
        // See § THE MAPPING at the top of this file for which term pushes which
        // line and why. Written out line by line, in the authored enum order,
        // rather than folded into a loop over a coefficient table: this is the
        // part a designer reads, and a table would hide it.
        nation_budget nb;
        auto&         wv = nb.weights;
        const float   b  = p.base_weight;

        wv[static_cast<std::size_t>(budget_priority::logistics_maintenance)] =
            b + p.niche_logistics * terms.niche_fit;
        wv[static_cast<std::size_t>(budget_priority::schooling)] =
            b + p.calm_schooling * terms.calm;
        wv[static_cast<std::size_t>(budget_priority::military_research)] =
            b + p.threat_milres * terms.threat;
        wv[static_cast<std::size_t>(budget_priority::academic_research)] =
            b + p.gap_academic * terms.niche_gap + p.calm_academic * terms.calm;
        wv[static_cast<std::size_t>(budget_priority::public_exploration)] =
            b + p.gap_exploration * terms.niche_gap;
        wv[static_cast<std::size_t>(budget_priority::contracted_force)] =
            b + p.threat_contracted * terms.threat;
        wv[static_cast<std::size_t>(budget_priority::strategic_reserve)] =
            b + p.threat_reserve * terms.threat;
        wv[static_cast<std::size_t>(budget_priority::public_works)] =
            b + p.niche_works * terms.niche_fit + p.calm_works * terms.calm;
        wv[static_cast<std::size_t>(budget_priority::charters)] =
            b + p.niche_charters * terms.niche_fit;
        wv[static_cast<std::size_t>(budget_priority::space_programme)] =
            b + p.calm_space * terms.calm;

        // Normalise in the authored enum order. `nation_budget` normalises on
        // read anyway; doing it here means the vector has ONE meaning rather
        // than two, and a surface can print a weight as the percentage it is.
        // A vector that sums to zero (base_weight 0, every term 0) is left at
        // zero — the header's own inertness condition, not an error.
        float total = 0.0f;
        for (std::size_t i = 0; i < priority_count; ++i)
        {
            wv[i] = std::max(0.0f, wv[i]);
            total += wv[i];
        }
        if (total > 0.0f)
            for (std::size_t i = 0; i < priority_count; ++i)
                wv[i] /= total;

        // The savings dial: a nation with nothing to fear withholds more.
        nb.reserve_fraction = clamp01(p.base_reserve + p.calm_reserve_bonus * terms.calm);

        out.emplace(nid, nb);
        if (report)
            report->scored.push_back(std::move(terms));
    }

    return out;
}

// ---------------------------------------------------------------------------
// The research pick (BL-1275; AI_OPPONENT.md § 11, Ben 2026-10-10 and 2026-10-15)
// ---------------------------------------------------------------------------
//
// THE SCORING RULE. score(node) = relief(node) - cost(node), both in milli-RP,
// relief = research_relief_rp x (direct(node) + reach(node)), in RELIEF UNITS
// (one home building-equivalent moved by 100%).
//
//   cost    node_cost_milli at TODAY's price index — what the queue will charge.
//
//   direct  what holding the node does for THIS nation:
//     extraction_rate   each home extraction site the scope matches x (factor - 1) x E(ext)
//     processing_yield  each home processing facility, its output goods' (factor - 1)
//                       averaged over the outputs (one building counts once) x E(proc)
//     logistics_cost    each home producer of a matched good, (1 - factor) averaged the
//                       same way (one building counts once) x E(log)
//     national_research the added share x max(1, home buildings) x E(res); the
//                       University-scoped share x the staffed Universities on its ground
//     unlocks           research_unlock_relief per thing the node unlocks — a recipe
//                       (x E(proc)), a building or a placement (x E(works)), the space
//                       gate (x E(space)) — read from the node's effects, read or
//                       declared unread (an unlock is worth its promise to the scorer)
//     the next cluster  (a major) research_cluster_relief x E(res) x the cost units of
//                       the nodes it brings within reach (a node needing two groups
//                       counts half per group it completes), plus
//                       research_major_relief x E(res) x its own cost units: a major is
//                       the cluster it closes, so its worth scales with its ring
//
//   reach   the share of each major the node buys on the way: for every major M not
//           held or queued whose route (the enqueue planner's own preview — the
//           cheapest route, the one the queue would take) contains the node,
//           direct(M) x units(node) / units(route). The route shrinks as a nation
//           progresses, so each next node of a route it has begun is worth more
//           per unit: depth follows from the arithmetic, not from a goal. Nothing
//           is queued beyond the one node; no route is remembered. The two sides
//           of a pick-one pair carry the same share (either satisfies the route).
//
//   E(line) — THE BINDING TERMS — is the nation's own scored budget weight
//   (`w.nation_budgets`, from its niche fit, gap and threat), as
//   0.5 + 0.5 x weight x priority_count: an average line weighs 1, an unfunded
//   line 0.5, so a line shades the choice and never zeroes a lever. Unscored: 1.
//   ext = mean(charters, public_works); proc = mean(academic, public_works);
//   log = logistics_maintenance; res = academic_research; works = public_works;
//   space = space_programme.
//
// TIES. Score ties go to the lower node index — except between the two sides of
// a pick-one pair, which break by the nation's own seed (an FNV hash of its
// generated name and id), so nations diverge.
//
// An UNREAD modifier relieves nothing. A quest whose deed is unmet is never a
// candidate and never a route. No look beyond one ring of the tree, no memory.
// ---------------------------------------------------------------------------

namespace {

/// Relief is clamped to +/- this before it is rounded (the nation_tech cap).
constexpr std::int64_t k_relief_cap_milli = 4'000'000'000'000'000'000LL;

/// relief - cost, saturating: relief is clamped to +/- k_relief_cap_milli and a
/// cost is non-negative and itself capped, so the result is clamped to the band.
std::int64_t sat_sub(std::int64_t a, std::int64_t b)
{
    const std::int64_t lo = -k_relief_cap_milli;
    if (b > 0 && a < lo + b)
        return lo;
    if (b < 0 && a > k_relief_cap_milli + b)
        return k_relief_cap_milli;
    return a - b;
}

std::int64_t relief_to_milli(double relief_units, double rp_per_unit)
{
    const double m = relief_units * rp_per_unit * static_cast<double>(k_milli_per_rp);
    if (!std::isfinite(m))
        return 0;
    const double cap = static_cast<double>(k_relief_cap_milli);
    return static_cast<std::int64_t>(std::llround(std::clamp(m, -cap, cap)));
}

const io::dg::node& dg(int n) { return io::dg::nodes[static_cast<std::size_t>(n)]; }

struct home_profile
{
    std::map<std::pair<int, bool>, int> ext;   ///< (resource, depositless) -> sites
    std::map<std::uint16_t, int>        proc;  ///< recipe id -> facilities
    int universities = 0; ///< Staffed Universities on its ground (advance_research's count).
    int total        = 0;
};

home_profile build_home_profile(const world& w, entity_id nation)
{
    home_profile hp;
    // Integer counts only, under ordered keys: the walk order cannot reach the result.
    for (const entity_id owner : economic_owner_ids(w))
    {
        const std::vector<entity_id>* assets = assets_of(w, owner);
        if (assets == nullptr)
            continue;
        const bool home = owner_home_nation(w, owner) == nation;
        for (const entity_id bid : *assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            const building_component& b = bit->second;
            if (b.decommissioned || b.ticks_remaining > 0)
                continue;
            if (b.type == building_type::university && b.workforce_assigned > 0.0f)
                if (const auto tn = w.tile_to_nation.find(b.tile); tn != w.tile_to_nation.end() && tn->second == nation)
                    ++hp.universities;
            if (!home)
                continue;
            ++hp.total;
            if (b.type == building_type::extraction_site)
                ++hp.ext[{ static_cast<int>(b.target_resource),
                           placement_rules::is_depositless_site(w, b.tile, b.target_resource) }];
            else if (b.type == building_type::processing_facility && b.recipe != no_recipe)
                ++hp.proc[b.recipe];
        }
    }
    return hp;
}

struct line_emphasis
{
    double ext = 1.0, proc = 1.0, log = 1.0, res = 1.0, works = 1.0, space = 1.0;
};

line_emphasis emphasis_for(const world& w, entity_id nation)
{
    line_emphasis e;
    const auto it = w.nation_budgets.find(nation);
    if (it == w.nation_budgets.end())
        return e;
    double total = 0.0;
    for (const float x : it->second.weights)
        total += std::max(0.0f, x);
    if (!(total > 0.0))
        return e;
    const auto W = [&](budget_priority l) {
        const double share = static_cast<double>(std::max(0.0f, it->second.weights[static_cast<std::size_t>(l)]))
                           / total * static_cast<double>(priority_count);
        return 0.5 + 0.5 * share;
    };
    e.ext   = 0.5 * (W(budget_priority::charters) + W(budget_priority::public_works));
    e.proc  = 0.5 * (W(budget_priority::academic_research) + W(budget_priority::public_works));
    e.log   = W(budget_priority::logistics_maintenance);
    e.res   = W(budget_priority::academic_research);
    e.works = W(budget_priority::public_works);
    e.space = W(budget_priority::space_programme);
    return e;
}

/// What an effect row unlocks, for the scorer: 0 nothing, 1 a recipe, 2 a
/// building or placement, 3 the space gate. Read from the store's own kinds;
/// an unread row unlocks when it waits on a recipe and says "unlock", or waits
/// on a building or a placement the campaign does not yet carry.
int unlock_class(const io::dg::effect& ef)
{
    using io::dg::effect_kind;
    const std::string_view waits = ef.waits_on;
    const std::string_view what  = ef.what;
    switch (ef.kind)
    {
        case effect_kind::unlock_recipe:    return 1;
        case effect_kind::unlock_structure: return 2;
        case effect_kind::mechanic:         return std::string_view(ef.target) == "space_dispatch" ? 3 : 0;
        case effect_kind::unread:
        {
            static constexpr std::string_view k_recipes[]   = { "fertiliser recipe", "plastics recipe", "microchip recipe" };
            static constexpr std::string_view k_buildings[] = { "Nuclear Plant", "Container Port", "offshore placement",
                                                                "coastal water placement" };
            for (const std::string_view r : k_recipes)
                if (waits == r && what.find("unlock") != std::string_view::npos)
                    return 1;
            for (const std::string_view b : k_buildings)
                if (waits == b)
                    return 2;
            return 0;
        }
        default: return 0;
    }
}

struct scorer_ctx
{
    const world&                           w;
    const recipe_registry&                 reg;
    const nation_ai_params&                p;
    entity_id                              nation;
    const std::bitset<io::dg::node_count>& held;
    home_profile                           hp;
    line_emphasis                          e;
};

bool open_target(const scorer_ctx& c, int m)
{
    // A node that may still become a candidate: unheld, not refused by a held
    // pair side, and not a quest whose deed is unmet.
    if (c.held.test(static_cast<std::size_t>(m)) || !deed_met(c.w, c.nation, m))
        return false;
    const int ex = dg(m).excludes;
    return ex < 0 || !c.held.test(static_cast<std::size_t>(ex));
}

/// direct(n), in relief units.
double direct_relief(const scorer_ctx& c, int n)
{
    const line_emphasis& e = c.e;
    double relief = 0.0;
    for (const auto& [key, count] : c.hp.ext)
    {
        effect_target at;
        at.resource = key.first;
        at.site     = extraction_site_name(static_cast<resource_type>(key.first), key.second);
        relief += count * (static_cast<double>(node_scalar_at(n, modifier_subject::extraction_rate, at)) - 1.0) * e.ext;
        effect_target lt;
        lt.resource = key.first;
        relief += count * (1.0 - static_cast<double>(node_scalar_at(n, modifier_subject::logistics_cost, lt))) * e.log;
    }
    for (const auto& [rid, count] : c.hp.proc)
    {
        const recipe* rc = c.reg.get_recipe(rid);
        if (rc == nullptr)
            continue;
        double yield = 0.0, haul = 0.0;
        int    nout  = 0;
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            if (!(rc->outputs[r] > 0.0f))
                continue;
            effect_target at;
            at.group    = rc->group;
            at.recipe   = rc->name;
            at.resource = static_cast<int>(r);
            yield += static_cast<double>(node_scalar_at(n, modifier_subject::processing_yield, at)) - 1.0;
            effect_target lt;
            lt.resource = static_cast<int>(r);
            haul += 1.0 - static_cast<double>(node_scalar_at(n, modifier_subject::logistics_cost, lt));
            ++nout;
        }
        if (nout > 0)
            relief += count * (yield * e.proc + haul * e.log) / nout;
    }
    relief += (static_cast<double>(node_scalar_at(n, modifier_subject::national_research, effect_target{})) - 1.0)
            * std::max(1, c.hp.total) * e.res;
    if (c.hp.universities > 0)
    {
        effect_target ut;
        ut.building    = building_type::university;
        ut.scoped_only = true;
        relief += (static_cast<double>(node_scalar_at(n, modifier_subject::national_research, ut)) - 1.0)
                * c.hp.universities * e.res;
    }

    const io::dg::node& nd = dg(n);
    for (std::uint16_t i = nd.effect_begin; i < nd.effect_end; ++i)
        switch (unlock_class(io::dg::effects[i]))
        {
            case 1: relief += c.p.research_unlock_relief * e.proc;  break;
            case 2: relief += c.p.research_unlock_relief * e.works; break;
            case 3: relief += c.p.research_unlock_relief * e.space; break;
            default: break;
        }

    if (nd.t == io::dg::tier::major)
    {
        // The next cluster: the nodes this major brings within reach.
        double opened = 0.0;
        for (int m = 0; m < io::dg::node_count; ++m)
        {
            const io::dg::node& md = dg(m);
            if (m == n || !open_target(c, m))
                continue;
            int unmet = 0;
            bool completes = false;
            for (int g = 0; g < md.group_count; ++g)
            {
                bool met = false, has_n = false;
                for (const std::int16_t k : md.groups[static_cast<std::size_t>(g)])
                {
                    if (k < 0) continue;
                    met   |= c.held.test(static_cast<std::size_t>(k));
                    has_n |= k == n;
                }
                if (!met) { ++unmet; completes |= has_n; }
            }
            if (completes && unmet > 0)
                opened += static_cast<double>(md.cost_units) / unmet;
        }
        relief += (c.p.research_cluster_relief * opened + c.p.research_major_relief * nd.cost_units) * e.res;
    }
    return std::isfinite(relief) ? relief : 0.0;
}

/// Deterministic per-nation bit for a pair tie: FNV-1a over the nation's
/// generated name and id.
bool pair_tie_prefers_higher(const world& w, entity_id nation)
{
    std::uint64_t h = 1469598103934665603ull;
    const auto mix = [&](std::uint8_t b) { h = (h ^ b) * 1099511628211ull; };
    if (const auto it = w.nations.find(nation); it != w.nations.end())
        for (const char ch : it->second.name)
            mix(static_cast<std::uint8_t>(ch));
    for (int s = 0; s < 32; s += 8)
        mix(static_cast<std::uint8_t>((static_cast<std::uint64_t>(nation) >> s) & 0xFFu));
    return ((h >> 17) & 1u) != 0;
}

} // namespace

bool research_queue_idle(const world& w, entity_id nation)
{
    const auto it = w.nation_tech.find(nation);
    if (it == w.nation_tech.end())
        return true;
    for (const std::uint16_t q : it->second.queue)
        if (deed_met(w, nation, q))
            return false;
    return true;
}

research_ai_pick choose_research_node(const world& w, const recipe_registry& reg,
                                      entity_id nation, const nation_ai_params& p)
{
    research_ai_pick pick;
    pick.nation = nation;
    if (w.nations.find(nation) == w.nations.end())
        return pick;

    static const std::bitset<io::dg::node_count> k_none{};
    const auto tit = w.nation_tech.find(nation);
    const std::bitset<io::dg::node_count>& held = tit != w.nation_tech.end() ? tit->second.held : k_none;

    scorer_ctx c{ w, reg, p, nation, held, build_home_profile(w, nation), emphasis_for(w, nation) };
    const research_params& rp    = reg.research();
    const std::int64_t     price = research_price_ppm(w, rp);

    // direct() is memoised per pick: a node may be a candidate and a major both.
    std::array<double, io::dg::node_count> direct{};
    std::array<bool, io::dg::node_count>   have{};
    const auto direct_of = [&](int n) {
        if (!have[static_cast<std::size_t>(n)])
        {
            direct[static_cast<std::size_t>(n)] = direct_relief(c, n);
            have[static_cast<std::size_t>(n)]   = true;
        }
        return direct[static_cast<std::size_t>(n)];
    };

    // Reach: each open major's worth, shared over its route by cost units.
    std::array<double, io::dg::node_count> reach{};
    std::vector<std::uint16_t> plan;
    for (int m = 0; m < io::dg::node_count; ++m)
    {
        if (dg(m).t != io::dg::tier::major || !open_target(c, m))
            continue;
        plan.clear();
        if (preview_enqueue(w, nation, m, &plan) != enqueue_result::queued || plan.empty())
            continue;
        double units = 0.0;
        for (const std::uint16_t k : plan)
            units += dg(k).cost_units;
        const double v = direct_of(m);
        if (!(units > 0.0) || !(v > 0.0))
            continue;
        for (const std::uint16_t k : plan)
            if (k != m) // the major itself is worth its direct value, not a share of it
                reach[k] += v * dg(k).cost_units / units;
    }
    // The planner's route takes the EARLIER side of a pick-one pair on a cost
    // tie; either side satisfies the requirement equally, so both sides carry
    // the larger share — the side is the nation's choice (its direct value,
    // then its seed), never the planner's tie-break.
    for (int a = 0; a < io::dg::node_count; ++a)
    {
        const int b = dg(a).excludes;
        if (b > a)
            reach[static_cast<std::size_t>(a)] = reach[static_cast<std::size_t>(b)] =
                std::max(reach[static_cast<std::size_t>(a)], reach[static_cast<std::size_t>(b)]);
    }

    const bool higher = pair_tie_prefers_higher(w, nation);
    for (int n = 0; n < io::dg::node_count; ++n)
    {
        if (held.test(static_cast<std::size_t>(n)) || !node_requirements_met(w, nation, n) || !deed_met(w, nation, n))
            continue;
        plan.clear();
        if (preview_enqueue(w, nation, n, &plan) != enqueue_result::queued || plan.size() != 1)
            continue; // queued already, or a pick-one pair refuses it
        ++pick.candidates;
        const std::int64_t relief_milli =
            relief_to_milli(direct_of(n) + reach[static_cast<std::size_t>(n)], p.research_relief_rp);
        const std::int64_t cost_milli = node_cost_milli(n, rp, price);
        const std::int64_t score      = sat_sub(relief_milli, cost_milli);
        bool take = pick.node < 0 || score > pick.score_milli;
        // A tie with the pair sibling breaks by the nation's seed; any other tie
        // keeps the lower index (the ascending walk).
        if (!take && score == pick.score_milli && dg(n).excludes == pick.node)
            take = higher;
        if (take)
        {
            pick.node         = n;
            pick.relief_milli = relief_milli;
            pick.cost_milli   = cost_milli;
            pick.score_milli  = score;
        }
    }
    return pick;
}

namespace {

std::vector<entity_id> research_ai_nations(const world& w, bool spectating)
{
    std::vector<entity_id> ids;
    // The age opens at the handover: everything researched in the settle is
    // erased (clear_research_at_handover), so a queue filled then would only
    // have the research line spend the treasury on nothing.
    if (!research_age_open(w))
        return ids;
    ids.reserve(w.nations.size());
    // The player's nation is never queued for in a seated session; under
    // spectate nobody is seated and the seat's nation is queued for too (Ben,
    // 2026-10-15, AI_OPPONENT.md § 11).
    const entity_id seated = spectating ? null_entity : player_nation(w);
    for (const auto& kv : w.nations)
        if (kv.first != seated)
            ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

int run_nation_research_ai(world& w, const recipe_registry& reg, bool spectating,
                           std::vector<research_ai_pick>* out)
{
    int queued = 0;
    for (const entity_id n : research_ai_nations(w, spectating))
    {
        if (!research_queue_idle(w, n))
            continue;
        research_ai_pick pick = choose_research_node(w, reg, n, reg.nation_ai());
        if (pick.node >= 0)
        {
            pick.queued = enqueue_node(w, n, pick.node) == enqueue_result::queued;
            if (pick.queued)
                ++queued;
        }
        if (out)
            out->push_back(pick);
    }
    return queued;
}

int run_nation_research_repick(world& w, const recipe_registry& reg, bool spectating,
                               std::vector<research_ai_pick>* out)
{
    const nation_ai_params& p = reg.nation_ai();
    int queued = 0;
    for (const entity_id n : research_ai_nations(w, spectating))
    {
        // One node queued at a time: re-pick only while the queue is idle and
        // the pool already holds the next pick's whole cost, so it completes
        // now from RP in hand. No treasury is spent here (the line paid only
        // what the queue owed in advance_research).
        for (int k = 0; k < p.research_repicks_per_quarter && research_queue_idle(w, n); ++k)
        {
            const auto tit = w.nation_tech.find(n);
            if (tit == w.nation_tech.end())
                break;
            research_ai_pick pick = choose_research_node(w, reg, n, p);
            if (pick.node < 0 || tit->second.pool_milli < pick.cost_milli)
                break;
            pick.queued = enqueue_node(w, n, pick.node) == enqueue_result::queued;
            if (out)
                out->push_back(pick);
            if (!pick.queued)
                break;
            ++queued;
            if (flow_research_queue(w, n, reg.research()) == 0)
                break; // not completed (cannot happen with the pool covering it); stays queued
        }
    }
    return queued;
}

// ---------------------------------------------------------------------------
// BL-1335 — the tax tier scorer. Contract in nation_ai.hpp.
// ---------------------------------------------------------------------------

int choose_tax_tier(int tier, float discontent, bool budget_short, const tax_params& p,
                    int cadence_k)
{
    const int   t     = tax_tier_valid(tier) ? tier : k_default_tax_tier;
    const float k     = static_cast<float>(std::max(1, cadence_k));
    const float limit = p.bite_threshold - p.ai_headroom;
    const auto  proj  = [&](int at) {
        return discontent + p.pressure[tax_tier_index(at)] * k;
    };

    if (proj(t) > limit && t > k_tax_tier_min)
        return t - 1;
    if (budget_short)
        return (t < k_tax_tier_max && proj(t + 1) <= limit) ? t + 1 : t;
    if (t > k_default_tax_tier)
        return t - 1;
    if (t < k_default_tax_tier && proj(t + 1) <= limit)
        return t + 1;
    return t;
}

bool nation_budget_short(const economy_report& report, entity_id nation)
{
    for (const nation_budget_result& r : report.national_budget.nations)
    {
        if (r.nation != nation)
            continue;
        for (const budget_line_result& l : r.lines)
            if (l.rationed && l.demand > 0.0f)
                return true;
    }
    for (const research_accrual& a : report.research)
        if (a.nation == nation && a.line_share > 0.0f && a.line_spend >= a.line_share
            && a.owed_milli > 0)
            return true;
    return false;
}

int run_nation_tax_ai(world& w, const recipe_registry& reg, const economy_report& report,
                      int econ_tick, bool spectating, std::vector<tax_ai_pick>* out)
{
    const std::vector<entity_id> ids = sorted_nation_ids(w);
    const int k = std::max(1, reg.nation_ai().cadence_k);
    // The player's nation is never scored in a seated session; under spectate
    // nobody is seated (AI_OPPONENT.md § 10i) and it is scored like any nation.
    const entity_id seated = spectating ? null_entity : player_nation(w);
    int moved = 0;
    for (std::size_t index = 0; index < ids.size(); ++index)
    {
        const entity_id n = ids[index];
        if (n == seated)
            continue;
        if ((econ_tick % k) != (static_cast<int>(index) % k)) // nation_is_due's stagger
            continue;
        const nation_component& nc = w.nations.at(n);
        tax_ai_pick pick;
        pick.nation     = n;
        pick.tier_from  = nc.tax_tier;
        pick.discontent = nc.discontent;
        pick.short_     = nation_budget_short(report, n);
        pick.tier_to    = choose_tax_tier(nc.tax_tier, nc.discontent, pick.short_, reg.tax(), k);
        if (pick.tier_to != pick.tier_from && set_nation_tax_tier(w, n, pick.tier_to))
            ++moved;
        if (out)
            out->push_back(pick);
    }
    return moved;
}
