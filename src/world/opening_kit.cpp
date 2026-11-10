#include "world/opening_kit.hpp"

#include "world/corporation_generation_detail.hpp" // the Pass 3 helpers, shared not copied
#include "world/input_reach.hpp"
#include "world/logistic_points.hpp" // lp_coverage_at, lp_consumer -- the logistics cap's cover read
#include "world/logistics.hpp"       // invalidate_logistics_caches
#include "world/economy_system.hpp"  // k_loss_streak_to_idle -- the kit input stock (BL-1487)
#include "world/market_clearing.hpp" // market_for_tile
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/road_generation.hpp" // lay_port_lanes -- a port the cap lays lanes as the seeding's does
#include "world/sea_combat.hpp"      // port_mouth_tile -- the cap keeps a sea berth (BL-1532)
#include "world/state_industry.hpp"  // state_handover_report -- the flat treasury's foundings
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// BL-1473 — A MAJOR'S OPENING KIT (CORPORATION_GENERATION.md § A major's opening
// kit, Ben 2026-11-02). See opening_kit.hpp for the contract. The order:
//
//   1. THE TAKE-UP, every major first (so no major's kit is laid against
//      another major's walk): every extraction site, processor, research
//      institute, school, university and Planetary Marketplace on a major's
//      homeworld ground goes, whoever placed it. Ports, hubs, stations,
//      launchpads, bases, bunkers, roads and track stay.
//   2. Then, ascending major id, THE KIT, owned by the nation:
//      a. the fixed EXTRACTION slots, ranked richness x cover over the whole
//         ground (the Pass 3 extractor rank); a Well or Wharf by habitability,
//         then tile id — ranked, not drawn;
//      b. the FLEX slots: the raw the kit's processors are shortest of at
//         t_idle, decided after the fixed extraction so the processors can be
//         sized against it;
//      c. the PROCESSORS, anchored at the capital, then the market centres by
//         chain distance, each placed only where chain-feasible, sized to
//         spare input and wanted (the charter walk's own tests);
//      d. a produce slot no Farm or Wharf could hold tries a Hydroponics Bay
//         now that the processors stand, else becomes a flex slot;
//      e. the CIVIC slots, nearest the capital, under the placement gates;
//      f. the OPENING STOCK, one Pass 4b stock per category on the shelves of
//         the markets the category's kit buildings stand in.
// ---------------------------------------------------------------------------

namespace {

using namespace corpgen;

/// The target non-extraction placement reads (ignored by can_place for them).
constexpr resource_type k_no_target = resource_type::iron_ore;

bool taken_up_type(building_type t)
{
    switch (t)
    {
    case building_type::extraction_site:
    case building_type::processing_facility:
    case building_type::research_institute:
    case building_type::schooling:
    case building_type::university:
    case building_type::planetary_marketplace:
        return true;
    default:
        return false;
    }
}

const char* civic_name(building_type t)
{
    switch (t)
    {
    case building_type::planetary_marketplace: return "planetary_marketplace";
    case building_type::research_institute:    return "research_institute";
    case building_type::schooling:             return "schooling";
    case building_type::university:            return "university";
    default:                                   return "building";
    }
}

/// A major's homeworld ground: its tiles on its capital's body, ascending.
struct major_ground
{
    entity_id              nation  = null_entity;
    entity_id              capital = null_entity;
    entity_id              body    = null_entity;
    int                    gw      = 0;
    std::vector<entity_id> tiles;
    std::set<entity_id>    tile_set;
};

major_ground ground_of(const world& w, entity_id nid, const nation_component& nc)
{
    major_ground g;
    g.nation  = nid;
    g.capital = nc.capital_tile;
    const auto ct = w.tiles.find(nc.capital_tile);
    if (ct == w.tiles.end())
        return g;
    g.body = ct->second.body;
    if (const auto b = w.bodies.find(g.body); b != w.bodies.end())
        g.gw = b->second.grid_width;
    for (const entity_id t : nc.tiles)
    {
        const auto it = w.tiles.find(t);
        const auto tn = w.tile_to_nation.find(t);
        if (it == w.tiles.end() || it->second.body != g.body || tn == w.tile_to_nation.end()
            || tn->second != nid)
            continue;
        g.tiles.push_back(t);
    }
    std::sort(g.tiles.begin(), g.tiles.end());
    g.tiles.erase(std::unique(g.tiles.begin(), g.tiles.end()), g.tiles.end());
    g.tile_set.insert(g.tiles.begin(), g.tiles.end());
    return g;
}

/// The major's land tiles ordered by column-wrapped squared grid distance to
/// @p from, ties to the lower tile id.
std::vector<entity_id> tiles_nearest(const world& w, const major_ground& g, entity_id from)
{
    std::vector<std::pair<long long, entity_id>> ring;
    const auto ft = w.tiles.find(from);
    if (ft == w.tiles.end())
        return {};
    for (const entity_id t : g.tiles)
    {
        const tile_component& tc = w.tiles.at(t);
        if (placement_rules::is_water_tile(tc.substrate))
            continue;
        long long dx = std::llabs(static_cast<long long>(tc.grid_x) - ft->second.grid_x);
        if (g.gw > 0 && dx > g.gw / 2)
            dx = g.gw - dx;
        const long long dy = static_cast<long long>(tc.grid_y) - ft->second.grid_y;
        ring.push_back({ dx * dx + dy * dy, t });
    }
    std::sort(ring.begin(), ring.end());
    std::vector<entity_id> out;
    out.reserve(ring.size());
    for (const auto& r : ring)
        out.push_back(r.second);
    return out;
}

/// The processors' anchor places (Pass 3, "Industry gathers where logistics is
/// cheap"): the capital, then the major's market centres on its body by chain
/// distance from the capital (the road walk the charter ladder reads), a centre
/// the roads do not reach after every reached one, ties to the lower tile id.
std::vector<entity_id> anchor_places(world& w, const major_ground& g)
{
    std::vector<entity_id> places{ g.capital };
    std::set<entity_id> centres;
    for (const auto& [mid, mc] : w.markets)
    {
        (void)mid;
        if (mc.centre_tile == g.capital || g.tile_set.count(mc.centre_tile) == 0)
            continue;
        centres.insert(mc.centre_tile);
    }
    if (centres.empty())
        return places;
    const auto chain = gen_road_walk(w, g.body, { g.capital }, null_entity, 0.0f);
    std::vector<std::pair<float, entity_id>> ranked;
    for (const entity_id t : centres)
    {
        const auto d = chain.find(t);
        ranked.push_back({ d != chain.end() ? d->second : std::numeric_limits<float>::infinity(), t });
    }
    std::sort(ranked.begin(), ranked.end());
    for (const auto& [d, t] : ranked)
    {
        (void)d;
        places.push_back(t);
    }
    return places;
}

/// The browse index of the in-band processing recipe @p name; -1 when the band
/// masks it. Throws when no recipe of that name is authored at all.
int browse_index_of(const recipe_registry& reg, const std::string& name, const std::string& kit)
{
    if (reg.recipe_id(name) == no_recipe)
        throw std::runtime_error("world_gen.opening_kit." + kit + ": unknown recipe '" + name + "'");
    const int n = reg.recipe_count(building_type::processing_facility);
    for (int i = 0; i < n; ++i)
        if (reg.recipe_at(building_type::processing_facility, i).name == name)
            return i;
    return -1;
}

/// In-band recipes making @p good: most output of the good first, ties to
/// browse order (the charter walk's `chain_recipes_for_good` preference).
std::vector<int> recipes_for_good(const recipe_registry& reg, resource_type good)
{
    const std::size_t gi = static_cast<std::size_t>(good);
    std::vector<int> out;
    const int n = reg.recipe_count(building_type::processing_facility);
    for (int i = 0; i < n; ++i)
        if (reg.recipe_at(building_type::processing_facility, i).outputs[gi] > 0.0f)
            out.push_back(i);
    std::stable_sort(out.begin(), out.end(), [&](int a, int b) {
        return reg.recipe_at(building_type::processing_facility, a).outputs[gi]
             > reg.recipe_at(building_type::processing_facility, b).outputs[gi];
    });
    return out;
}

/// One rung of an extraction slot's ladder: dig @p good on @p tier ground.
struct dig_rung
{
    resource_type good;
    dig_site      tier;
};

/// The slot's rungs, as named first, then § "A slot the ground cannot hold"
/// rung 2 (the same slot class, another good).
std::vector<dig_rung> extraction_ladder(resource_type g)
{
    switch (g)
    {
    case resource_type::agricultural_produce: // a Farm, then a Fishing Wharf (a Bay is laid later)
        return { { g, dig_site::deposit }, { g, dig_site::wharf } };
    case resource_type::water: // a Well, then an ice deposit
        return { { g, dig_site::well }, { g, dig_site::deposit } };
    case resource_type::coal:
        return { { g, dig_site::deposit }, { resource_type::petroleum, dig_site::deposit } };
    case resource_type::petroleum:
        return { { g, dig_site::deposit }, { resource_type::coal, dig_site::deposit } };
    case resource_type::iron_ore:
        return { { g, dig_site::deposit }, { resource_type::copper_ore, dig_site::deposit } };
    case resource_type::copper_ore:
        return { { g, dig_site::deposit }, { resource_type::iron_ore, dig_site::deposit } };
    default:
        return { { g, dig_site::deposit } };
    }
}

/// The own good's tiers alone (a flex slot digs one good, on its dig ladder).
std::vector<dig_rung> own_good_tiers(resource_type g)
{
    if (g == resource_type::water || g == resource_type::agricultural_produce)
    {
        std::vector<dig_rung> out;
        for (const dig_site t : dig_ladder(g))
            out.push_back({ g, t });
        return out;
    }
    return { { g, dig_site::deposit } };
}

std::string rung_name(const dig_rung& r)
{
    std::string n = resource_names::name_of(r.good);
    if (r.tier == dig_site::well)  n += " (Well)";
    if (r.tier == dig_site::wharf) n += " (Fishing Wharf)";
    if (r.tier == dig_site::deposit && r.good == resource_type::water) n += " (ice)";
    return n;
}

/// Everything one lay shares.
struct kit_layer
{
    world&                 w;
    const recipe_registry& reg;
    const opening_kit_config& kit;
    gen_occupancy          occ;
    gen_lp_cover           cover;
    input_reach            cr;
};

/// The kit's own buildings on one major.
struct major_kit
{
    major_ground           g;
    std::vector<entity_id> own;        ///< every kit building (the chain tier's "own")
    std::vector<entity_id> extractors;
    std::vector<entity_id> processors;
    std::vector<entity_id> civic;
    std::vector<entity_id> places;     ///< anchor_places, read once
    std::map<entity_id, std::vector<entity_id>> nearest; ///< place -> tiles_nearest, read lazily
    output_want            want;
    bool                   want_ready = false;
    /// The kit's prospective draw (BL-1473 round 2): every processing slot's planned
    /// inputs and every not-yet-laid kit building's upkeep, folded into `want` when
    /// it is first measured.
    std::array<float, resource_count> prospective{};
};

/// One processing slot's planned recipe inputs at the nominal rate.
std::array<float, resource_count> planned_inputs(const recipe_registry& reg, const recipe* rc)
{
    std::array<float, resource_count> out{};
    if (rc == nullptr)
        return out;
    building_component fresh{}; // value-initialised: workforce_assigned has no default (judged at the authored staffing)
    fresh.type = building_type::processing_facility;
    const float batches = judged_batches(reg, fresh);
    for (std::size_t r = 0; r < resource_count; ++r)
        if (rc->inputs[r] > 0.0f)
            out[r] = batches * rc->inputs[r];
    return out;
}

void own_it(world& w, major_kit& mk, entity_id bid)
{
    w.nations.at(mk.g.nation).state_assets.push_back(bid);
    mk.own.push_back(bid);
}

/// Place one extractor of @p r on the major's ground: candidates on that
/// tier's ground for that good with stack room; spread first (a tile no site of
/// the good works yet), then richness x cover on a deposit, habitability on a
/// Well or Wharf site, then the richer, then the lower tile id. Null if none.
entity_id place_extractor(kit_layer& L, major_kit& mk, const dig_rung& r)
{
    // RUNG 1 IS COVERED GROUND (cold review): a tile the cover at placement reaches
    // ranks ahead of every tile it does not, so spreading never sends a site onto
    // uncovered ground while covered ground with room holds the good. Within each,
    // spread before stack, then richness x cover (a deposit) or habitability (a Well
    // or Wharf site), then the richer, then the lower tile id.
    struct cand { int uncovered; int stacked; float key; float rich; entity_id tid; };
    std::vector<cand> cs;
    for (const entity_id tid : mk.g.tiles)
    {
        const tile_component& tc = L.w.tiles.at(tid);
        if (dig_site_of(L.w, tid, tc, r.good) != r.tier
            || !L.occ.has_room(tc, tid, building_type::extraction_site, r.good))
            continue;
        const float rich  = tc.resource_deposit[static_cast<std::size_t>(r.good)];
        const float cover = L.cover.cover(L.w, tid);
        const float key   = r.tier == dig_site::deposit ? rich * cover : tc.habitability;
        cs.push_back({ cover > 0.0f ? 0 : 1, L.occ.extraction_on(tid, r.good) > 0 ? 1 : 0, key, rich,
                       tid });
    }
    if (cs.empty())
        return null_entity;
    std::sort(cs.begin(), cs.end(), [](const cand& a, const cand& b) {
        if (a.uncovered != b.uncovered) return a.uncovered < b.uncovered;
        if (a.stacked != b.stacked) return a.stacked < b.stacked;
        if (a.key != b.key)         return a.key > b.key;
        if (a.rich != b.rich)       return a.rich > b.rich;
        return a.tid < b.tid;
    });
    const resource_type good = r.good;
    const entity_id bid = author_building(L.w, cs.front().tid, building_type::extraction_site, L.occ, &good);
    own_it(L.w, mk, bid);
    mk.extractors.push_back(bid);
    return bid;
}

/// The recipe each processing slot would run first (its first in-band named
/// recipe, else the first in-band recipe for its good) — the flex choice's
/// reading of the kit's processors before they stand.
std::vector<const recipe*> planned_recipes(const recipe_registry& reg, const opening_kit_config& kit)
{
    std::vector<const recipe*> out;
    for (const opening_kit_slot& s : kit.slots)
    {
        if (s.category != kit_slot_category::processing)
            continue;
        int idx = -1;
        for (const std::string& n : s.recipes)
            if ((idx = browse_index_of(reg, n, kit.name)) >= 0)
                break;
        if (idx < 0)
        {
            const std::vector<int> any = recipes_for_good(reg, s.good);
            if (!any.empty())
                idx = any.front();
        }
        // One entry per processing slot, in slot order; null where none is in band.
        out.push_back(idx >= 0 ? &reg.recipe_at(building_type::processing_facility, idx) : nullptr);
    }
    return out;
}

/// THE FLEX ORDER: the raws the kit's processors draw, the one they are
/// SHORTEST of first — the lowest ratio of the kit's own extraction of it to the
/// processors' draw at t_idle (the sized rule's reading), ties to the larger
/// shortfall then the lower resource index. After them every other extractable
/// raw, by its best deposit on the ground (richness x cover), then index — the
/// fallback where no drawn raw can be placed.
std::vector<resource_type> flex_order(kit_layer& L, major_kit& mk, const std::vector<const recipe*>& procs)
{
    building_component fresh{}; // value-initialised: workforce_assigned has no default (judged at the authored staffing)
    fresh.type = building_type::processing_facility;
    const float batches = judged_batches(L.reg, fresh) * L.reg.t_idle();
    std::array<float, resource_count> draw{}, supply{};
    for (const recipe* rc : procs)
        for (std::size_t r = 0; rc != nullptr && r < resource_count; ++r)
            if (rc->inputs[r] > 0.0f)
                draw[r] += batches * rc->inputs[r];
    for (const entity_id bid : mk.extractors)
        if (const auto bit = L.w.buildings.find(bid); bit != L.w.buildings.end())
            for (std::size_t r = 0; r < resource_count; ++r)
                supply[r] += building_output(L.w, L.reg, bid, bit->second, r, nullptr);

    struct drawn { float ratio; float shortfall; std::size_t r; };
    std::vector<drawn> ds;
    std::set<std::size_t> seen;
    for (const resource_type e : placement_rules::k_extractable)
    {
        const std::size_t r = static_cast<std::size_t>(e);
        if (!(draw[r] > 0.0f) || !seen.insert(r).second)
            continue;
        ds.push_back({ supply[r] / draw[r], draw[r] - supply[r], r });
    }
    std::sort(ds.begin(), ds.end(), [](const drawn& a, const drawn& b) {
        if (a.ratio != b.ratio)         return a.ratio < b.ratio;
        if (a.shortfall != b.shortfall) return a.shortfall > b.shortfall;
        return a.r < b.r;
    });
    std::vector<resource_type> out;
    for (const drawn& d : ds)
        out.push_back(static_cast<resource_type>(d.r));

    std::vector<std::pair<float, std::size_t>> rest;
    for (const resource_type e : placement_rules::k_extractable)
    {
        const std::size_t r = static_cast<std::size_t>(e);
        if (!seen.insert(r).second)
            continue;
        float best = -1.0f;
        for (const entity_id tid : mk.g.tiles)
        {
            const tile_component& tc = L.w.tiles.at(tid);
            const float rich = tc.resource_deposit[r];
            if (!(rich > 0.0f) || !placement_rules::can_place(tc, building_type::extraction_site, e))
                continue;
            best = std::max(best, rich * L.cover.cover(L.w, tid));
        }
        if (best >= 0.0f)
            rest.push_back({ best, r });
    }
    std::sort(rest.begin(), rest.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second < b.second;
    });
    for (const auto& [k, r] : rest)
    {
        (void)k;
        out.push_back(static_cast<resource_type>(r));
    }
    return out;
}

/// Lay one flex slot: the flex order's first raw that places. The good placed,
/// or false.
bool place_flex(kit_layer& L, major_kit& mk, const std::vector<const recipe*>& procs,
                std::string& placed_as, entity_id& bid)
{
    for (const resource_type r : flex_order(L, mk, procs))
        for (const dig_rung& rung : own_good_tiers(r))
            if ((bid = place_extractor(L, mk, rung)) != null_entity)
            {
                placed_as = rung_name(rung);
                return true;
            }
    return false;
}

/// The recipes of the kit's processors standing now.
std::vector<const recipe*> standing_recipes(const kit_layer& L, const major_kit& mk)
{
    std::vector<const recipe*> out;
    for (const entity_id bid : mk.processors)
        if (const auto bit = L.w.buildings.find(bid); bit != L.w.buildings.end())
            if (const recipe* rc = L.reg.get_recipe(bit->second.recipe))
                out.push_back(rc);
    return out;
}

/// Place one processor running one of @p cands (browse indices, in preference
/// order) at the first anchor place that takes it: at each place the nearest own
/// land tile with stack room on which some candidate's placement gate passes;
/// there, the candidate of the best chain tier (own, market, reach), ties to
/// the candidates' order, among those that are chain-feasible, sized to spare
/// input (`chain_recipe_placeable`) and wanted (`output_want::short_for`). A
/// place where none qualifies is unplaced and the next is tried. Returns the
/// building and the chosen browse index, or null.
/// Why a processor slot found no site, counted over every (place, candidate)
/// the walk read: the first test each candidate failed there.
struct proc_refusals
{
    int no_site    = 0; ///< places with no own land tile with room the gate admits
    int not_wanted = 0; ///< the output is not short (BL-1217 D6)
    int no_feed    = 0; ///< an input has no producer within reach (chain-feasible)
    int not_sized  = 0; ///< a producer within reach, but not the spare (BL-1233)
    /// For a sized refusal: per input, the smallest deficit over the places read —
    /// need x t_idle less the reachable spare (the sized rule's own terms).
    std::map<std::size_t, float> short_by;

    std::string text() const
    {
        char buf[200];
        std::snprintf(buf, sizeof buf, "refused: %d place(s) without a site, %d not wanted, "
                      "%d no feed in reach, %d short of spare input",
                      no_site, not_wanted, no_feed, not_sized);
        std::string out = buf;
        for (const auto& [r, d] : short_by)
        {
            char b2[96];
            std::snprintf(b2, sizeof b2, " [short %s by %.2f]",
                          resource_names::name_of(static_cast<resource_type>(r)).c_str(),
                          static_cast<double>(d));
            out += b2;
        }
        return out;
    }
};

entity_id place_processor(kit_layer& L, major_kit& mk, const std::vector<int>& cands, int& chosen,
                          proc_refusals* why = nullptr)
{
    chosen = -1;
    if (cands.empty())
        return null_entity;
    if (!mk.want_ready)
    {
        // WANT AT THE MAJOR'S OWN MARKETS (Ben, 2026-11-03): the markets centred on its
        // ground, and the capital's market whatever its centre. Reach and spare still
        // read every neighbour (chain_recipe_placeable, unchanged).
        std::set<entity_id> own_markets;
        for (const auto& [mid, mc] : L.w.markets)
            if (mk.g.tile_set.count(mc.centre_tile) != 0)
                own_markets.insert(mid);
        if (const entity_id cm = market_for_tile(L.w, mk.g.capital); cm != null_entity)
            own_markets.insert(cm);
        mk.want       = measure_output_want_at(L.w, L.reg, mk.g.body, own_markets);
        mk.want_ready = true;
        // ITS OWN PROCESSORS WANT IT (the ruling's own words): the kit is one curated
        // chain, so what the kit's not-yet-laid processors and civic buildings will
        // draw is wanted at its markets before they stand -- steel by the machinery
        // slot, fuel by the Marketplace's upkeep. Each processing slot's inputs are
        // taken out again as that slot is decided (`book` then adds what it runs);
        // the upkeep stays, as the building it is for will draw it.
        for (std::size_t r = 0; r < resource_count; ++r)
            mk.want.want[r] += mk.prospective[r];
    }
    for (const entity_id place : mk.places)
    {
        auto nit = mk.nearest.find(place);
        if (nit == mk.nearest.end())
            nit = mk.nearest.emplace(place, tiles_nearest(L.w, mk.g, place)).first;
        entity_id site = null_entity;
        std::vector<int> gated;
        for (const entity_id tid : nit->second)
        {
            const tile_component& tc = L.w.tiles.at(tid);
            if (!placement_rules::can_place(tc, building_type::processing_facility, k_no_target)
                || !L.occ.has_room(tc, tid, building_type::processing_facility, k_no_target))
                continue;
            gated.clear();
            for (const int i : cands)
            {
                const recipe& rc = L.reg.recipe_at(building_type::processing_facility, i);
                const placement_gate gate =
                    L.reg.placement_gate_for(building_type::processing_facility, L.reg.recipe_id(rc.name));
                if (placement_rules::can_place_in_world(L.w, tid, building_type::processing_facility,
                                                        k_no_target, -1.0f, null_entity, gate))
                    gated.push_back(i);
            }
            if (!gated.empty())
            {
                site = tid;
                break;
            }
        }
        if (site == null_entity)
        {
            if (why) ++why->no_site;
            continue;
        }
        const entity_id bid    = author_building(L.w, site, building_type::processing_facility, L.occ);
        const entity_id market = market_for_tile(L.w, site);
        chain_begin_decision(L.w, L.reg, L.cr);
        int best = -1, best_tier = chain_tier_none;
        for (const int i : gated)
        {
            const recipe& rc = L.reg.recipe_at(building_type::processing_facility, i);
            // BL-1551 (Ben, 2026-11-08): no market asks for arms before the first hire, so the
            // want gate cannot speak for them. The kit's arms slots are the ruling's own want.
            const bool arms = rc.outputs[static_cast<std::size_t>(resource_type::small_arms)] > 0.0f
                || rc.outputs[static_cast<std::size_t>(resource_type::armoured_vehicles)] > 0.0f
                || rc.outputs[static_cast<std::size_t>(resource_type::artillery_pieces)] > 0.0f;
            if (!arms && !mk.want.short_for(L.w, rc, site))
            {
                if (why) ++why->not_wanted;
                continue;
            }
            int t = chain_recipe_placeable(L.w, L.reg, L.cr, bid, market, rc, &mk.own);
            // The same ruling, for the feed: a works whose steel or machinery is not yet in
            // reach still opens, idle, and runs when they arrive. Last among the feasible.
            if (t == chain_tier_none && arms)
                t = chain_tier_none - 1;
            if (t == chain_tier_none && why)
            {
                if (chain_recipe_tier(L.w, L.reg, L.cr, bid, market, rc, &mk.own) == chain_tier_none)
                    ++why->no_feed;
                else
                {
                    ++why->not_sized;
                    const float need = judged_batches(L.reg, L.w.buildings.at(bid)) * L.reg.t_idle();
                    for (std::size_t r = 0; r < resource_count; ++r)
                    {
                        if (!(rc.inputs[r] > 0.0f))
                            continue;
                        const reachable_spare sp = reachable_supply(L.w, L.reg, L.cr, market, r, bid);
                        const float deficit = need * rc.inputs[r] - sp.spare;
                        if (deficit > 0.0f || sp.landed < 0.0f)
                        {
                            const float d = std::max(deficit, 0.0f);
                            const auto it = why->short_by.find(r);
                            if (it == why->short_by.end() || d < it->second)
                                why->short_by[r] = d;
                        }
                    }
                }
            }
            if (t < best_tier)
            {
                best_tier = t;
                best      = i;
            }
        }
        if (best < 0)
        {
            chain_unplace(L.w, bid, L.occ);
            continue;
        }
        const recipe& took = L.reg.recipe_at(building_type::processing_facility, best);
        L.w.buildings.at(bid).recipe = L.reg.recipe_id(took.name);
        mk.want.book(L.w, L.reg, took);
        own_it(L.w, mk, bid);
        mk.processors.push_back(bid);
        chosen = best;
        return bid;
    }
    return null_entity;
}

/// Place one civic building on the tile nearest the capital that admits it.
entity_id place_civic(kit_layer& L, major_kit& mk, building_type type, std::string& why)
{
    if (!L.reg.building_available(type))
    {
        why = "not available in the campaign band";
        return null_entity;
    }
    const placement_gate gate = L.reg.placement_gate_for(type, no_recipe);
    auto nit = mk.nearest.find(mk.g.capital);
    if (nit == mk.nearest.end())
        nit = mk.nearest.emplace(mk.g.capital, tiles_nearest(L.w, mk.g, mk.g.capital)).first;
    bool first = true;
    for (const entity_id tid : nit->second)
    {
        const tile_component& tc = L.w.tiles.at(tid);
        if (!L.occ.has_room(tc, tid, type, k_no_target))
            continue;
        const placement_rules::placement_result pr =
            placement_rules::can_place_in_world(L.w, tid, type, k_no_target, -1.0f, null_entity, gate);
        if (!pr)
        {
            if (first)
                why = std::string("no own tile admits it (nearest: ")
                    + placement_rules::placement_reason_text(pr.reason) + ")";
            first = false;
            continue;
        }
        const entity_id bid = author_building(L.w, tid, type, L.occ);
        own_it(L.w, mk, bid);
        mk.civic.push_back(bid);
        return bid;
    }
    if (first)
        why = "no own tile with room";
    return null_entity;
}

/// One Pass 4b stock per category on the shelves of the markets the category's
/// kit buildings stand in, split by how many stand in each (place_opening_stock's
/// split). Nothing is minted: generation's opening stock, as every charter's.
void shelve_stock(world& w, const std::vector<entity_id>& bids, industrial_focus focus)
{
    std::map<entity_id, int> weight;
    for (const entity_id bid : bids)
        if (const auto bit = w.buildings.find(bid); bit != w.buildings.end())
            if (const entity_id mid = market_for_tile(w, bit->second.tile); mid != null_entity)
                ++weight[mid];
    int total = 0;
    for (const auto& [mid, n] : weight)
        total += n;
    if (total <= 0)
        return;
    const std::array<float, resource_count> stock = kit_opening_stock(focus);
    for (const auto& [mid, n] : weight)
    {
        market_component& mc = w.markets.at(mid);
        const float share = static_cast<float>(n) / static_cast<float>(total);
        for (std::size_t r = 0; r < resource_count; ++r)
            if (stock[r] > 0.0f)
                mc.inventory[r] += stock[r] * share;
    }
}

/// THE KIT STOCKS ITS INPUTS (Ben, 2026-11-04, BL-1487). Each kit processor's recipe
/// inputs, at its judged staffing, for `k_loss_streak_to_idle` ticks, on the shelf of
/// the market it stands in: a fresh chain is fed through the idle reflex's window
/// before its own feeders deliver, so neither end idles for an empty shelf. Ascending
/// building id; nothing is minted beyond generation's opening stock, as § Pass 4b's.
void shelve_kit_inputs(world& w, const recipe_registry& reg, std::vector<entity_id> bids)
{
    std::sort(bids.begin(), bids.end());
    for (const entity_id bid : bids)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const recipe* rc = reg.get_recipe(bit->second.recipe);
        const entity_id mid = market_for_tile(w, bit->second.tile);
        if (rc == nullptr || mid == null_entity)
            continue;
        const float span = judged_batches(reg, bit->second) * static_cast<float>(k_loss_streak_to_idle);
        market_component& mc = w.markets.at(mid);
        for (std::size_t r = 0; r < resource_count; ++r)
            if (rc->inputs[r] > 0.0f)
                mc.inventory[r] += span * rc->inputs[r];
    }
}

using rec_outcome = opening_kit_slot_record::outcome;

void set_record(opening_kit_slot_record& rec, rec_outcome o, std::string as, std::string note,
                entity_id bid)
{
    rec.result    = o;
    rec.placed_as = std::move(as);
    rec.note      = std::move(note);
    rec.building  = bid;
}

} // namespace

opening_kit_report lay_major_opening_kits(world& w, const recipe_registry& reg,
                                          const opening_kit_config& kit)
{
    opening_kit_report rep;
    rep.kit = kit.name;
    // Every recipe name resolves BEFORE anything is taken up (cold review): an unknown
    // name throws here, with the world as it was handed in.
    for (const opening_kit_slot& s : kit.slots)
        for (const std::string& n : s.recipes)
            (void)browse_index_of(reg, n, kit.name);

    // The majors, ascending id, each with homeworld ground and a capital on it.
    std::vector<entity_id> majors;
    for (const auto& [nid, nc] : w.nations)
        if (nc.is_major && w.tiles.count(nc.capital_tile) != 0)
            majors.push_back(nid);
    std::sort(majors.begin(), majors.end());
    rep.majors = static_cast<int>(majors.size());
    if (majors.empty())
        return rep;

    std::vector<major_kit> kits;
    kits.reserve(majors.size());
    for (const entity_id nid : majors)
    {
        major_kit mk;
        mk.g = ground_of(w, nid, w.nations.at(nid));
        kits.push_back(std::move(mk));
        opening_kit_major m;
        m.nation = nid;
        rep.per_major.push_back(std::move(m));
    }

    // 1. THE TAKE-UP, every major's ground before any kit is laid.
    {
        std::map<entity_id, std::size_t> ground_major; // tile -> index
        for (std::size_t k = 0; k < kits.size(); ++k)
            for (const entity_id t : kits[k].g.tiles)
                ground_major.emplace(t, k);
        std::vector<std::pair<entity_id, std::size_t>> gone;
        for (const auto& [bid, b] : w.buildings)
            if (taken_up_type(b.type))
                if (const auto gm = ground_major.find(b.tile); gm != ground_major.end())
                    gone.push_back({ bid, gm->second });
        std::sort(gone.begin(), gone.end());
        std::set<entity_id> gone_ids;
        for (const auto& [bid, k] : gone)
            gone_ids.insert(bid);
        // A charter's index loses them (it owns nothing; BL-1407). Keyed edits.
        std::vector<entity_id> emptied;
        for (auto& [cid, ch] : w.gen_charters)
        {
            const std::size_t before = ch.assets.size();
            ch.assets.erase(std::remove_if(ch.assets.begin(), ch.assets.end(),
                                           [&](entity_id b) { return gone_ids.count(b) != 0; }),
                            ch.assets.end());
            if (gone_ids.count(ch.hq_building) != 0)
                ch.hq_building = null_entity;
            if (before > 0 && ch.assets.empty())
                emptied.push_back(cid);
        }
        std::sort(emptied.begin(), emptied.end());
        for (const entity_id cid : emptied)
        {
            // Its opening stock goes with its buildings, as a removed roster's does.
            unplace_opening_stock(w, cid);
            w.gen_opening_stock.erase(cid);
        }
        rep.charters_emptied = static_cast<int>(emptied.size());
        for (const auto& [bid, k] : gone)
        {
            release_from_owner(w, bid);
            w.buildings.erase(bid);
            w.stockpiles.erase(bid);
            ++rep.per_major[k].taken_up;
            ++rep.taken_up;
        }
        invalidate_logistics_caches(w);
    }

    // 2. THE KIT, ascending major id.
    kit_layer L{ w, reg, kit, gen_occupancy{}, gen_lp_cover{}, make_input_reach(w, reg) };
    for (const auto& [bid, b] : w.buildings) // additive counts: order-free
        L.occ.add(b);
    // BL-1500 (PRODUCTION.md, Ben 2026-11-05: generation obeys the cap with no exemptions):
    // THE CAPITAL'S SOURCE HOLDS ITS SLOT. The seeding that follows must stand a source on
    // the capital and the cap now binds it, while the kit's processors anchor at the capital
    // first and would fill the tile (seeds 0, 12, 28, 46: every major's capital, full when
    // the seeding reached it). So the kit lays around one held slot, and goes next door for
    // what it cannot fit: the occupancy counts the source before it stands. Local to the
    // kit's own count; nothing is added to the world.
    L.occ.hold_capital_slots(w);
    const std::vector<const recipe*> planned = planned_recipes(reg, kit);

    for (std::size_t k = 0; k < kits.size(); ++k)
    {
        major_kit&         mk = kits[k];
        opening_kit_major& rm = rep.per_major[k];
        mk.places = anchor_places(w, mk.g);
        rm.slots.resize(kit.slots.size());
        for (std::size_t i = 0; i < kit.slots.size(); ++i)
        {
            rm.slots[i].category = kit.slots[i].category;
            rm.slots[i].slot     = kit.slots[i].label;
        }

        // a. The fixed extraction slots. A slot nothing on its ladder places
        //    becomes a flex slot (rung 3); a produce slot first tries a Bay (d).
        std::vector<std::size_t> flex, hydro;
        for (std::size_t i = 0; i < kit.slots.size(); ++i)
        {
            const opening_kit_slot& s = kit.slots[i];
            if (s.category != kit_slot_category::extraction)
                continue;
            if (s.flex)
            {
                flex.push_back(i);
                continue;
            }
            const std::vector<dig_rung> ladder = extraction_ladder(s.good);
            bool done = false;
            for (std::size_t j = 0; j < ladder.size() && !done; ++j)
                if (const entity_id bid = place_extractor(L, mk, ladder[j]); bid != null_entity)
                {
                    set_record(rm.slots[i], j == 0 ? rec_outcome::placed : rec_outcome::substituted,
                               rung_name(ladder[j]), j == 0 ? "" : "rung 2: the same class, another good",
                               bid);
                    done = true;
                }
            if (!done)
                (s.good == resource_type::agricultural_produce ? hydro : flex).push_back(i);
        }

        // b. The flex slots, against the kit's planned processors.
        for (const std::size_t i : flex)
        {
            std::string as;
            entity_id   bid = null_entity;
            const bool  authored = kit.slots[i].flex;
            if (place_flex(L, mk, planned, as, bid))
                set_record(rm.slots[i], authored ? rec_outcome::placed : rec_outcome::substituted,
                           as, authored ? "flex: the raw the processors are shortest of"
                                        : "rung 3: became a flex slot", bid);
            else
                set_record(rm.slots[i], rec_outcome::unbuilt, "",
                           "no deposit, Well or Wharf ground for any raw", null_entity);
        }

        // c. The processors. First the kit's prospective draw (see place_processor):
        //    each processing slot's planned inputs and every processor and civic
        //    building's upkeep basket, at the major's markets.
        {
            const building_upkeep_params& up = reg.building_upkeep();
            std::size_t pj = 0;
            for (const opening_kit_slot& s : kit.slots)
            {
                building_type bt = building_type::none;
                if (s.category == kit_slot_category::processing)
                {
                    const std::array<float, resource_count> in = planned_inputs(reg, planned[pj++]);
                    for (std::size_t r = 0; r < resource_count; ++r)
                        mk.prospective[r] += in[r];
                    bt = building_type::processing_facility;
                }
                else if (s.category == kit_slot_category::civic)
                    bt = s.building;
                if (bt == building_type::none)
                    continue;
                const std::array<float, resource_count> u = building_upkeep_goods(up, bt, reg.era());
                for (std::size_t r = 0; r < resource_count; ++r)
                    mk.prospective[r] += u[r];
            }
        }
        std::size_t proc_j = 0;
        for (std::size_t i = 0; i < kit.slots.size(); ++i)
        {
            const opening_kit_slot& s = kit.slots[i];
            if (s.category != kit_slot_category::processing)
                continue;
            {
                // This slot's planned inputs leave the prospective draw now it is decided.
                const std::array<float, resource_count> in = planned_inputs(reg, planned[proc_j++]);
                auto& tgt = mk.want_ready ? mk.want.want : mk.prospective;
                for (std::size_t r = 0; r < resource_count; ++r)
                    tgt[r] -= in[r];
            }
            std::vector<int> named;
            for (const std::string& n : s.recipes)
                if (const int idx = browse_index_of(reg, n, kit.name); idx >= 0)
                    named.push_back(idx);
            const std::vector<int> for_good = recipes_for_good(reg, s.good);
            if (s.recipes.empty())
                named = for_good;
            std::vector<int> other;
            for (const int idx : for_good)
                if (std::find(named.begin(), named.end(), idx) == named.end())
                    other.push_back(idx);
            std::vector<int> nearest_feed; // rung 3: any wanted, feasible processor
            for (int idx = 0; idx < reg.recipe_count(building_type::processing_facility); ++idx)
                nearest_feed.push_back(idx);

            int chosen = -1;
            proc_refusals as_named;
            entity_id bid = place_processor(L, mk, named, chosen, &as_named);
            rm.refused_no_site    += as_named.no_site;
            rm.refused_not_wanted += as_named.not_wanted;
            rm.refused_no_feed    += as_named.no_feed;
            rm.refused_not_sized  += as_named.not_sized;
            if (bid != null_entity)
            {
                set_record(rm.slots[i], rec_outcome::placed,
                           reg.recipe_at(building_type::processing_facility, chosen).name, "", bid);
                continue;
            }
            if ((bid = place_processor(L, mk, other, chosen)) != null_entity)
            {
                set_record(rm.slots[i], rec_outcome::substituted,
                           reg.recipe_at(building_type::processing_facility, chosen).name,
                           "rung 2: another recipe for the good; as named " + as_named.text(), bid);
                continue;
            }
            if ((bid = place_processor(L, mk, nearest_feed, chosen)) != null_entity)
            {
                set_record(rm.slots[i], rec_outcome::substituted,
                           reg.recipe_at(building_type::processing_facility, chosen).name,
                           "rung 3: the feasible processor nearest its feed; as named "
                               + as_named.text(), bid);
                continue;
            }
            set_record(rm.slots[i], rec_outcome::unbuilt, "",
                       "no chain-feasible, sized, wanted site at the capital or any market centre; "
                       "as named " + as_named.text(),
                       null_entity);
        }

        // d. A produce slot no Farm or Wharf held: a Hydroponics Bay, else a flex slot.
        for (const std::size_t i : hydro)
        {
            int chosen = -1;
            std::vector<int> bay;
            if (reg.recipe_id("hydroponics_bay") != no_recipe)
                if (const int idx = browse_index_of(reg, "hydroponics_bay", kit.name); idx >= 0)
                    bay.push_back(idx);
            if (const entity_id bid = place_processor(L, mk, bay, chosen); bid != null_entity)
            {
                set_record(rm.slots[i], rec_outcome::substituted, "hydroponics_bay",
                           "rung 2: no Farm or Wharf ground", bid);
                continue;
            }
            std::string as;
            entity_id   bid = null_entity;
            if (place_flex(L, mk, standing_recipes(L, mk), as, bid))
                set_record(rm.slots[i], rec_outcome::substituted, as, "rung 3: became a flex slot", bid);
            else
                set_record(rm.slots[i], rec_outcome::unbuilt, "",
                           "no Farm, Wharf or Bay site and no ground for any raw", null_entity);
        }

        // e. The civic slots.
        for (std::size_t i = 0; i < kit.slots.size(); ++i)
        {
            const opening_kit_slot& s = kit.slots[i];
            if (s.category != kit_slot_category::civic)
                continue;
            std::string why;
            if (const entity_id bid = place_civic(L, mk, s.building, why); bid != null_entity)
                set_record(rm.slots[i], rec_outcome::placed, civic_name(s.building), "", bid);
            else
                set_record(rm.slots[i], rec_outcome::unbuilt, "", why, null_entity);
        }

        // e2. A MARKETPLACE ON THE MILL'S MARKET (Ben, 2026-11-07, BL-1545;
        // CORPORATION_GENERATION.md § A major's opening kit). Every steel slot that
        // stood a building names its market; a market none of the kit's Marketplaces
        // already stands in gets one, on the free own tile in that market nearest the
        // slot's building that admits it. One rule for every major; an unbuilt steel
        // slot names no market. Trade points belong to the market a trade building
        // stands in (TRADE.md), so this is what lets the mill's steel leave it.
        {
            std::set<entity_id> held; // markets a kit Marketplace already stands in
            for (const entity_id c : mk.civic)
                if (const auto ci = w.buildings.find(c);
                    ci != w.buildings.end() && ci->second.type == building_type::planetary_marketplace)
                    if (const entity_id cm = market_for_tile(w, ci->second.tile); cm != null_entity)
                        held.insert(cm);
            for (std::size_t i = 0; i < kit.slots.size(); ++i)
            {
                const opening_kit_slot& s = kit.slots[i];
                if (s.category != kit_slot_category::processing || s.good != resource_type::steel)
                    continue;
                const auto bi = w.buildings.find(rm.slots[i].building);
                if (bi == w.buildings.end())
                {
                    ++rm.mill_marketplace_no_slot;
                    continue;
                }
                const entity_id m = market_for_tile(w, bi->second.tile);
                if (m == null_entity)
                {
                    ++rm.mill_marketplace_no_site;
                    continue;
                }
                if (held.count(m) != 0)
                {
                    ++rm.mill_marketplace_held;
                    continue;
                }
                if (!L.reg.building_available(building_type::planetary_marketplace))
                {
                    ++rm.mill_marketplace_no_site;
                    continue;
                }
                const placement_gate gate = L.reg.placement_gate_for(building_type::planetary_marketplace, no_recipe);
                entity_id placed = null_entity;
                for (const entity_id tid : tiles_nearest(L.w, mk.g, bi->second.tile))
                {
                    if (market_for_tile(w, tid) != m)
                        continue;
                    const tile_component& tc = L.w.tiles.at(tid);
                    if (!L.occ.has_room(tc, tid, building_type::planetary_marketplace, k_no_target))
                        continue;
                    if (!placement_rules::can_place_in_world(L.w, tid, building_type::planetary_marketplace,
                                                             k_no_target, -1.0f, null_entity, gate))
                        continue;
                    placed = author_building(L.w, tid, building_type::planetary_marketplace, L.occ);
                    break;
                }
                if (placed == null_entity)
                {
                    ++rm.mill_marketplace_no_site;
                    continue;
                }
                own_it(L.w, mk, placed);
                mk.civic.push_back(placed);
                held.insert(m);
                ++rm.mill_marketplace_placed;
            }
        }

        // f. The opening stock.
        shelve_stock(w, mk.extractors, industrial_focus::extraction);
        shelve_stock(w, mk.processors, industrial_focus::processing);
        shelve_kit_inputs(w, reg, mk.processors);

        rm.proc_built = static_cast<int>(mk.processors.size());
        for (const opening_kit_slot_record& r : rm.slots)
        {
            rm.placed      += r.result == rec_outcome::placed;
            rm.substituted += r.result == rec_outcome::substituted;
            rm.unbuilt     += r.result == rec_outcome::unbuilt;
        }
        rep.placed      += rm.placed;
        rep.substituted += rm.substituted;
        rep.unbuilt     += rm.unbuilt;
    }
    // The reach legs warmed the logistics caches; the caller may route next.
    invalidate_logistics_caches(w);
    return rep;
}

void print_opening_kit(const opening_kit_report& r)
{
    std::printf("[opening_kit] kit '%s': %d major(s), %d building(s) taken up (%d charter(s) "
                "emptied); slots %d placed, %d substituted, %d unbuilt\n",
                r.kit.c_str(), r.majors, r.taken_up, r.charters_emptied, r.placed, r.substituted,
                r.unbuilt);
    for (const opening_kit_major& m : r.per_major)
    {
        std::printf("[opening_kit]   major %llu: took up %d; placed %d, substituted %d, unbuilt %d; "
                    "%d processor(s) standing; as-named refusals: no site %d, not wanted %d, "
                    "no feed %d, short of spare %d",
                    static_cast<unsigned long long>(m.nation), m.taken_up, m.placed, m.substituted,
                    m.unbuilt, m.proc_built, m.refused_no_site, m.refused_not_wanted,
                    m.refused_no_feed, m.refused_not_sized);
        std::printf("; mill Marketplace: laid %d, market held %d, slot unbuilt %d, no site %d",
                    m.mill_marketplace_placed, m.mill_marketplace_held, m.mill_marketplace_no_slot,
                    m.mill_marketplace_no_site);
        for (const opening_kit_slot_record& s : m.slots)
        {
            if (s.result == opening_kit_slot_record::outcome::substituted)
                std::printf("; %s -> %s (%s)", s.slot.c_str(), s.placed_as.c_str(), s.note.c_str());
            else if (s.result == opening_kit_slot_record::outcome::unbuilt)
                std::printf("; %s UNBUILT (%s)", s.slot.c_str(), s.note.c_str());
        }
        std::printf("\n");
    }
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------
// THE LOGISTICS CAP (Ben, 2026-11-03; see opening_kit.hpp)
// ---------------------------------------------------------------------------

namespace {

/// Remove one building generation stood: its owner's list, every charter's
/// index (and anchor), the building and its stockpile.
void take_up_node(world& w, entity_id bid)
{
    for (auto& [cid, ch] : w.gen_charters) // keyed edits: order-free
    {
        (void)cid;
        ch.assets.erase(std::remove(ch.assets.begin(), ch.assets.end(), bid), ch.assets.end());
        if (ch.hq_building == bid)
            ch.hq_building = null_entity;
    }
    release_from_owner(w, bid);
    w.buildings.erase(bid);
    w.stockpiles.erase(bid);
}

int node_slot(building_type t)
{
    switch (t)
    {
    case building_type::port:                 return 0;
    case building_type::railway_station:      return 1;
    case building_type::inland_logistics_hub: return 2;
    default:                                  return -1;
    }
}

const char* node_name(int slot)
{
    static const char* const names[3] = { "port", "railway_station", "inland_logistics_hub" };
    return names[slot];
}

constexpr building_type k_node_type[3] = { building_type::port, building_type::railway_station,
                                           building_type::inland_logistics_hub };

/// How many covered candidate tiles a processor's re-site tries before it gives up:
/// each try authors the plant and runs the Pass 3 chain test there.
constexpr std::size_t k_resite_processor_tries = 48;

/// KIT WITHIN REACH (Ben, 2026-11-04: "Place within reach too"). The kit is laid
/// against the network the history left; the cap then takes most of it up. Each kit
/// extractor or processor the capped network leaves without cover is taken up and
/// laid again on covered ground of the major's: an extractor digging the same good on
/// the same tier (spread before stack, then richness x cover or habitability, then the
/// richer, then the lower tile id, `place_extractor`'s ranking); a processor running
/// the same recipe on the covered tile nearest its old site that admits it and where
/// it passes Pass 3 (`chain_recipe_placeable`: chain-feasible and sized to spare
/// input). Extractors first, so a moved processor reads its feed where it now stands.
/// A building no covered tile takes stays where it is, counted.
void resite_uncovered_kit(world& w, const recipe_registry& reg, opening_kit_report& rep)
{
    gen_occupancy occ;
    for (const auto& [bid, b] : w.buildings) // additive counts: order-free
        occ.add(b);
    occ.hold_capital_slots(w);
    lp_reach_memo memo; // the sources do not change below: one memo for the pass
    const auto covered_at = [&](entity_id tid) { return lp_coverage_at(w, memo, tid); };

    for (opening_kit_major& m : rep.per_major)
    {
        const auto nit = w.nations.find(m.nation);
        if (nit == w.nations.end())
            continue;
        const major_ground g = ground_of(w, m.nation, nit->second);
        if (g.body == null_entity)
            continue;
        std::vector<entity_id> own;
        for (const opening_kit_slot_record& r : m.slots)
            if (r.building != null_entity)
                own.push_back(r.building);

        const auto rebind = [&](opening_kit_slot_record& r, entity_id fresh) {
            std::replace(own.begin(), own.end(), r.building, fresh);
            r.building = fresh;
            r.note += r.note.empty() ? "re-sited within reach" : "; re-sited within reach";
            ++m.resited;
        };

        for (int pass = 0; pass < 2; ++pass)
            for (opening_kit_slot_record& r : m.slots)
            {
                const auto bit = w.buildings.find(r.building);
                if (bit == w.buildings.end() || !lp_consumer(bit->second)
                    || covered_at(bit->second.tile) > 0.0f)
                    continue;
                const building_component old = bit->second;
                if (pass == 0 && old.type == building_type::extraction_site)
                {
                    const resource_type good = old.target_resource;
                    const dig_site tier = dig_site_of(w, old.tile, w.tiles.at(old.tile), good);
                    struct cand { int stacked; float key; float rich; entity_id tid; };
                    std::vector<cand> cs;
                    for (const entity_id tid : g.tiles)
                    {
                        const tile_component& tc = w.tiles.at(tid);
                        if (dig_site_of(w, tid, tc, good) != tier
                            || !occ.has_room(tc, tid, building_type::extraction_site, good))
                            continue;
                        const float cover = covered_at(tid);
                        if (!(cover > 0.0f))
                            continue;
                        const float rich = tc.resource_deposit[static_cast<std::size_t>(good)];
                        cs.push_back({ occ.extraction_on(tid, good) > 0 ? 1 : 0,
                                       tier == dig_site::deposit ? rich * cover : tc.habitability,
                                       rich, tid });
                    }
                    if (cs.empty())
                    {
                        ++m.resite_failed;
                        continue;
                    }
                    std::sort(cs.begin(), cs.end(), [](const cand& a, const cand& b) {
                        if (a.stacked != b.stacked) return a.stacked < b.stacked;
                        if (a.key != b.key)         return a.key > b.key;
                        if (a.rich != b.rich)       return a.rich > b.rich;
                        return a.tid < b.tid;
                    });
                    chain_unplace(w, r.building, occ);
                    const entity_id fresh =
                        author_building(w, cs.front().tid, building_type::extraction_site, occ, &good);
                    w.nations.at(m.nation).state_assets.push_back(fresh);
                    rebind(r, fresh);
                }
                else if (pass == 1 && old.type == building_type::processing_facility)
                {
                    const recipe* rc = reg.get_recipe(old.recipe);
                    if (rc == nullptr)
                        continue;
                    const placement_gate gate =
                        reg.placement_gate_for(building_type::processing_facility, old.recipe);
                    // Take the old plant up first, so its own stack slot and its draw on
                    // its feed are free for the test; it is laid back if nothing takes it.
                    const entity_id old_tile = old.tile;
                    chain_unplace(w, r.building, occ);
                    input_reach cr = make_input_reach(w, reg);
                    entity_id fresh = null_entity;
                    std::size_t tries = 0;
                    for (const entity_id tid : tiles_nearest(w, g, old_tile))
                    {
                        if (tries >= k_resite_processor_tries)
                            break;
                        const tile_component& tc = w.tiles.at(tid);
                        if (!placement_rules::can_place(tc, building_type::processing_facility, k_no_target)
                            || !occ.has_room(tc, tid, building_type::processing_facility, k_no_target)
                            || !(covered_at(tid) > 0.0f)
                            || !placement_rules::can_place_in_world(w, tid, building_type::processing_facility,
                                                                    k_no_target, -1.0f, null_entity, gate))
                            continue;
                        ++tries;
                        const entity_id bid = author_building(w, tid, building_type::processing_facility, occ);
                        w.buildings.at(bid).recipe = old.recipe;
                        chain_begin_decision(w, reg, cr);
                        if (chain_recipe_placeable(w, reg, cr, bid, market_for_tile(w, tid), *rc, &own)
                            != chain_tier_none)
                        {
                            fresh = bid;
                            break;
                        }
                        chain_unplace(w, bid, occ);
                    }
                    if (fresh == null_entity)
                    {
                        // Nothing covered takes it: the plant stands again where it stood.
                        fresh = author_building(w, old_tile, building_type::processing_facility, occ);
                        w.buildings.at(fresh).recipe = old.recipe;
                        w.nations.at(m.nation).state_assets.push_back(fresh);
                        std::replace(own.begin(), own.end(), r.building, fresh);
                        r.building = fresh;
                        ++m.resite_failed;
                        continue;
                    }
                    w.nations.at(m.nation).state_assets.push_back(fresh);
                    rebind(r, fresh);
                }
            }
    }
    invalidate_logistics_caches(w);
}

} // namespace

void release_kit_foundings(world& w, state_handover_report& handover)
{
    const auto release = [&](entity_id nation, entity_id& building, float& paid) {
        if (building != null_entity && w.buildings.count(building) == 0)
        {
            building = null_entity; // taken up by the kit or the cap
            paid     = 0.0f;
            return;
        }
        if (const auto nit = w.nations.find(nation); nit != w.nations.end() && nit->second.is_major)
            paid = 0.0f; // paid from the chest the flat treasury replaced
    };
    for (state_founding& f : handover.founded)
        release(f.nation, f.building, f.paid);
    for (state_port_founding& p : handover.ports)
        release(p.nation, p.building, p.paid);
}

int count_uncovered_buildings(world& w, entity_id nation, bool include_idled)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0;
    std::vector<entity_id> bids = nit->second.state_assets;
    std::sort(bids.begin(), bids.end());
    lp_reach_memo memo;
    int n = 0;
    for (const entity_id bid : bids)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        const bool idled_consumer = include_idled && b.ticks_remaining <= 0
            && (b.type == building_type::extraction_site
                || b.type == building_type::processing_facility);
        if (!lp_consumer(b) && !idled_consumer)
            continue;
        if (!(lp_coverage_at(w, memo, bit->second.tile) > 0.0f))
            ++n;
    }
    return n;
}

void cap_major_logistics(world& w, const recipe_registry& reg, const opening_kit_config& kit,
                         opening_kit_report& rep)
{
    if (!kit.logistics_capped())
        return;
    const int count[3] = { kit.ports, kit.stations, kit.hubs };
    for (opening_kit_major& m : rep.per_major)
    {
        const auto nit = w.nations.find(m.nation);
        if (nit == w.nations.end())
            continue;
        const major_ground g = ground_of(w, m.nation, nit->second);
        if (g.body == null_entity)
            continue;

        // The standing nodes on the major's ground, in the seeding's order from the capital.
        const auto chain = gen_road_walk(w, g.body, { g.capital }, null_entity, 0.0f);
        struct node { float d; entity_id tile; entity_id bid; int slot; };
        std::vector<node> nodes;
        for (const auto& [bid, b] : w.buildings)
        {
            const int s = node_slot(b.type);
            if (s < 0 || g.tile_set.count(b.tile) == 0)
                continue;
            const auto d = chain.find(b.tile);
            nodes.push_back({ d != chain.end() ? d->second : std::numeric_limits<float>::infinity(),
                              b.tile, bid, s });
        }
        std::sort(nodes.begin(), nodes.end(), [](const node& a, const node& b) {
            if (a.d != b.d)       return a.d < b.d;
            if (a.tile != b.tile) return a.tile < b.tile;
            return a.bid < b.bid;
        });
        // FILL THE SLOTS FROM THE STANDING NODES, DOWN EACH SLOT'S LADDER, BEFORE ANY IS
        // TAKEN UP (cold review): first each node into its own type's slot, in the
        // seeding's order; then a slot still open takes a standing node from further
        // down its ladder (a port slot a station, then a hub; a station slot a hub), so
        // a standing station is never taken up only for the port slot to lay a fresh one.
        int filled[3] = { 0, 0, 0 };
        std::vector<char> used(nodes.size(), 0);
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].tile == g.capital)
            {
                ++m.nodes_source; // the capital's source: kept, outside the slots
                used[i] = 1;
            }

        // THE LAST PORT SLOT IS A SEA BERTH (BL-1532; MILITARY.md § Sea, Ben 2026-11-07:
        // "prefer the sea"). The kit's fleet lies at a port with a sea mouth, and a lake's
        // shore is none. While a sea-mouthed port stands on the major's ground and none is
        // kept, the LAST open port slot admits only a sea-mouthed port: a lake port, or a
        // station or hub climbing the ladder, may take any port slot but that one. Every pass
        // below asks `admits`, so kit cover still chooses WHICH sea port is kept.
        std::vector<char> sea(nodes.size(), 0);
        bool sea_standing = false;
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (nodes[i].slot == 0 && port_mouth_tile(w, nodes[i].bid) != null_entity)
            {
                sea[i] = 1;
                sea_standing = true;
            }
        const auto admits = [&](std::size_t i, int s) {
            if (filled[s] >= count[s])
                return false;
            if (s != 0 || sea[i] || !sea_standing || filled[0] + 1 < count[0])
                return true;
            for (std::size_t k = 0; k < nodes.size(); ++k)
                if (used[k] && sea[k])
                    return true; // a sea berth is kept already
            return false;
        };

        // KIT COVER FIRST (Ben, 2026-11-04: "Keep covering nodes"): before the seeding's
        // order fills the slots, the standing node that brings the most of the kit's
        // buildings under cover is kept, again and again while one adds any, each into
        // its own type's slot or, that full, the nearest slot up its ladder. Cover is read
        // as `lp_route_to` reads it - a kept node's own reach touches the building and
        // the node is on the chain to the capital - over the kept set alone, so the
        // nodes about to be taken up count for nothing. Ties go to the seeding's order.
        {
            std::vector<entity_id> kit_tiles;
            for (const opening_kit_slot_record& r : m.slots)
                if (const auto bit = w.buildings.find(r.building);
                    bit != w.buildings.end() && lp_consumer(bit->second))
                    kit_tiles.push_back(bit->second.tile);
            std::sort(kit_tiles.begin(), kit_tiles.end());
            kit_tiles.erase(std::unique(kit_tiles.begin(), kit_tiles.end()), kit_tiles.end());

            std::vector<lp_network> net(nodes.size(), lp_network::road);
            std::vector<std::unordered_map<entity_id, float>> field(nodes.size());
            for (std::size_t i = 0; i < nodes.size(); ++i)
            {
                (void)lp_source_network(w.buildings.at(nodes[i].bid).type, net[i]);
                field[i] = lp_distance_field(w, g.body, net[i], { nodes[i].tile });
            }
            const auto reaches = [&](std::size_t i, entity_id tile) {
                const auto d = field[i].find(tile);
                return d != field[i].end() && lp_reach(net[i], d->second) > 0.0f;
            };
            // The kit tiles the set @p kept covers: the chain closed outward from the
            // capital (a link counts either way, as `memo_chain`'s), then each tile any
            // chained node reaches.
            const auto covered = [&](const std::vector<std::size_t>& kept) {
                std::vector<std::size_t> chain;
                std::vector<char> in(kept.size(), 0);
                for (bool grew = true; grew;)
                {
                    grew = false;
                    for (std::size_t k = 0; k < kept.size(); ++k)
                    {
                        if (in[k])
                            continue;
                        const std::size_t i = kept[k];
                        bool joins = reaches(i, g.capital);
                        for (std::size_t c = 0; c < chain.size() && !joins; ++c)
                            joins = reaches(i, nodes[chain[c]].tile) || reaches(chain[c], nodes[i].tile);
                        if (!joins)
                            continue;
                        in[k] = 1;
                        chain.push_back(i);
                        grew = true;
                    }
                }
                int n = 0;
                for (const entity_id t : kit_tiles)
                    for (const std::size_t i : chain)
                        if (reaches(i, t)) { ++n; break; }
                return n;
            };
            const auto slot_for = [&](std::size_t i) {
                for (int s = nodes[i].slot; s >= 0; --s)
                    if (admits(i, s))
                        return s;
                return -1;
            };
            std::vector<std::size_t> kept;
            for (std::size_t i = 0; i < nodes.size(); ++i)
                if (used[i])
                    kept.push_back(i);
            int base = covered(kept);
            for (;;)
            {
                std::size_t best = nodes.size();
                int best_gain = 0;
                for (std::size_t i = 0; i < nodes.size(); ++i)
                {
                    if (used[i] || slot_for(i) < 0)
                        continue;
                    kept.push_back(i);
                    const int gain = covered(kept) - base;
                    kept.pop_back();
                    if (gain > best_gain)
                    {
                        best_gain = gain;
                        best      = i;
                    }
                }
                // No one node adds cover: a PAIR may, a bridge onto the chain and the node
                // beyond it that reaches the kit (a bridge alone adds nothing). The pair
                // adding the most is kept, both nodes, when the slots hold both.
                std::size_t second = nodes.size();
                if (best == nodes.size())
                    for (std::size_t i = 0; i < nodes.size(); ++i)
                    {
                        if (used[i] || slot_for(i) < 0)
                            continue;
                        const int si = slot_for(i);
                        ++filled[si];
                        used[i] = 1; // held for the pair's second pick, so `admits` reads it
                        kept.push_back(i);
                        for (std::size_t j = i + 1; j < nodes.size(); ++j)
                        {
                            if (used[j] || slot_for(j) < 0)
                                continue;
                            kept.push_back(j);
                            const int gain = covered(kept) - base;
                            kept.pop_back();
                            if (gain > best_gain)
                            {
                                best_gain = gain;
                                best      = i;
                                second    = j;
                            }
                        }
                        kept.pop_back();
                        used[i] = 0;
                        --filled[si];
                    }
                if (best == nodes.size())
                    break;
                for (const std::size_t pick : { best, second })
                {
                    if (pick == nodes.size())
                        continue;
                    const int s = slot_for(pick);
                    ++filled[s];
                    ++m.nodes_kept;
                    ++m.nodes_for_kit;
                    used[pick] = 1;
                    kept.push_back(pick);
                    if (s != nodes[pick].slot)
                        m.logistics_note += std::string(" ") + node_name(s) + "<-standing "
                                          + node_name(nodes[pick].slot);
                }
                base += best_gain;
            }
        }

        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            const node& n = nodes[i];
            if (used[i])
                continue;
            if (admits(i, n.slot))
            {
                ++filled[n.slot];
                ++m.nodes_kept;
                used[i] = 1;
            }
        }
        for (int s = 0; s < 3; ++s)
            for (int rung = s + 1; rung < 3 && filled[s] < count[s]; ++rung)
                for (std::size_t i = 0; i < nodes.size() && filled[s] < count[s]; ++i)
                    if (!used[i] && nodes[i].slot == rung && admits(i, s))
                    {
                        used[i] = 1;
                        ++filled[s];
                        ++m.nodes_kept;
                        m.logistics_note += std::string(" ") + node_name(s) + "<-standing "
                                          + node_name(rung);
                    }
        std::vector<entity_id> gone;
        for (std::size_t i = 0; i < nodes.size(); ++i)
            if (!used[i])
                gone.push_back(nodes[i].bid);
        std::sort(gone.begin(), gone.end());
        for (const entity_id bid : gone)
            take_up_node(w, bid);
        m.nodes_taken = static_cast<int>(gone.size());
        if (!gone.empty())
            invalidate_logistics_caches(w);

        // A slot still open: laid nearest the capital where placement allows, down the
        // ladder (port -> station -> hub; station -> hub), and built as the Logistic
        // Point seeding builds a node it lands (`seed_logistics_sources`' place_source):
        // zero staff, standing complete, one network node to a hub's tile, and a port
        // laying its lanes as it stands.
        const std::vector<entity_id> near = tiles_nearest(w, g, g.capital);
        const auto lay = [&](building_type type) -> bool {
            if (!reg.building_available(type))
                return false;
            const placement_gate gate = reg.placement_gate_for(type, no_recipe);
            for (const entity_id tid : near)
            {
                if (tid == g.capital)
                    continue; // the capital's source is not a slot
                const tile_component& tc = w.tiles.at(tid);
                if (placement_rules::non_extraction_buildings_on_tile(w, tid)
                    >= placement_rules::non_extraction_stack_cap(tc.substrate, tc.cover))
                    continue;
                if (!placement_rules::can_place_in_world(w, tid, type, k_no_target, -1.0f,
                                                         null_entity, gate))
                    continue;
                bool node_here = false, station_here = false;
                for (const auto& [obid, ob] : w.buildings)
                {
                    (void)obid;
                    if (ob.tile != tid)
                        continue;
                    node_here    = node_here || node_slot(ob.type) >= 0;
                    station_here = station_here || ob.type == building_type::railway_station;
                }
                if (type == building_type::inland_logistics_hub && node_here)
                    continue; // the seeding's one network node to a hub's tile
                if (type == building_type::railway_station)
                {
                    // The build door's station rule (construction.cpp, BL-1255): a city
                    // tile touching a rail line, one station to a tile.
                    bool city = false;
                    for (const auto& [centre, ctile] : w.population_centre_tile)
                        if (ctile == tid) { (void)centre; city = true; break; }
                    if (!city || station_here || !tile_touches_track(w, tid))
                        continue;
                }
                const entity_id bid = w.create_entity();
                building_component bc{};
                bc.tile               = tid;
                bc.type               = type;
                bc.workforce_assigned = 0.0f; // zero staff, as every network node
                bc.workforce_target   = default_workforce_target(type);
                bc.ticks_remaining    = 0;    // stands complete, as every generated holding
                w.buildings[bid]  = bc;
                w.stockpiles[bid] = stockpile_component{};
                w.nations.at(m.nation).state_assets.push_back(bid);
                if (type == building_type::port)
                    (void)lay_port_lanes(w, bid); // as a port the seeding lands
                else
                    invalidate_logistics_caches(w);
                return true;
            }
            return false;
        };
        for (int s = 0; s < 3; ++s)
            for (int k = filled[s]; k < count[s]; ++k)
            {
                bool laid = false;
                for (int rung = s; rung < 3 && !laid; ++rung)
                    if (lay(k_node_type[rung]))
                    {
                        laid = true;
                        ++m.nodes_laid;
                        if (rung != s)
                            m.logistics_note += std::string(" ") + node_name(s) + "->" + node_name(rung);
                    }
                if (!laid)
                {
                    ++m.nodes_open;
                    m.logistics_note += std::string(" ") + node_name(s) + " OPEN";
                }
            }
    }
    invalidate_logistics_caches(w);
    resite_uncovered_kit(w, reg, rep);
    for (opening_kit_major& m : rep.per_major)
        m.uncovered = count_uncovered_buildings(w, m.nation);
    invalidate_logistics_caches(w);
}

void print_opening_kit_logistics(const opening_kit_report& r)
{
    for (const opening_kit_major& m : r.per_major)
        std::printf("[opening_kit] logistics major %llu: capital source %d node(s); kept %d, "
                    "(%d for the kit), taken up %d, laid %d, open %d;%s kit re-sited %d (%d stayed); "
                    "%d building(s) without cover after the cap\n",
                    static_cast<unsigned long long>(m.nation), m.nodes_source, m.nodes_kept,
                    m.nodes_for_kit,
                    m.nodes_taken, m.nodes_laid, m.nodes_open,
                    m.logistics_note.empty() ? "" : m.logistics_note.c_str(), m.resited,
                    m.resite_failed, m.uncovered);
    std::fflush(stdout);
}
