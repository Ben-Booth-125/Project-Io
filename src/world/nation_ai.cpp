#include "nation_ai.hpp"

#include "components.hpp"
#include "hex_neighbors.hpp"
#include "stance.hpp"
#include "world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
