#include "colonisation.hpp"

#include "hex_neighbors.hpp"

#include <algorithm>
#include <queue>

// ---------------------------------------------------------------------------
// colonisation — the implementation. `colonisation.hpp` carries the argument;
// this file carries the arithmetic, and comments here explain only what the
// header could not say without the numbers in front of it.
// ---------------------------------------------------------------------------

namespace
{

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/// Integer floor(log2(n)) for n >= 1, and 0 below it. The whole of the
/// logarithm this file needs: predation decays per DOUBLING, so the only
/// question ever asked of a headcount is how many times it has doubled.
///
/// INTEGER, NOT `std::log2`. A transcendental in a path the generation reads
/// is exactly the non-determinism `.claude/rules/io-standing-rules.md` forbids
/// — the same libm call can differ in its last bit between compilers, and a
/// world would then generate differently on two machines.
int doublings(int64_t n)
{
    int d = 0;
    while (n > 1) { n >>= 1; ++d; }
    return d;
}

} // namespace

// ---------------------------------------------------------------------------
// Farm classes
// ---------------------------------------------------------------------------

farm_class classify_farm_class(terrain_substrate s, terrain_cover c,
                               terrain_landform lf, bool shoreline)
{
    // MOST-SPECIFIC FIRST. See the header: a marsh on volcanic rock should read
    // as the thing that makes it unusual, not as the thing it shares with half
    // the map. Each test below is therefore a claim about which reading a
    // package coined on that ground would actually have learned.

    // Cold ground first of all: a snowfield is a snowfield whatever is under it,
    // and no package coined on temperate soil has anything to say about it.
    if (s == terrain_substrate::icy || c == terrain_cover::snow)
        return farm_class::boreal;

    // Volcanic ash is the one substrate that changes what farming there MEANS —
    // extremely fertile, and dangerous — so it outranks the cover on top of it.
    if (s == terrain_substrate::volcanic || c == terrain_cover::ash)
        return farm_class::volcanic;

    // Standing water on soft ground: the first granaries, and the class the
    // design names first.
    if (c == terrain_cover::marsh)
        return farm_class::floodplain;

    // Barrier landforms next, because ground you cannot walk across easily is
    // not ground you farm the same way whatever grew on it.
    if (lf == terrain_landform::mountain || lf == terrain_landform::canyon
        || lf == terrain_landform::rift)
        return farm_class::montane;

    // The shoreline ring. AFTER the barriers deliberately: a cliff coast is a
    // mountain to a farmer and a coast only to a sailor, and this span has no
    // sailors in it.
    if (shoreline)
        return farm_class::coastal;

    if (c == terrain_cover::forest)
        return farm_class::woodland;

    // Dead ground. Dunes and salt crust are farmed by almost nothing, and
    // saying so is what makes a desert a real barrier rather than slow grass.
    if (c == terrain_cover::dunes || c == terrain_cover::salt)
        return farm_class::arid;

    if (lf == terrain_landform::valley)
        return farm_class::valley;
    if (lf == terrain_landform::highland || lf == terrain_landform::crater)
        return farm_class::highland;

    if (c == terrain_cover::grass)
        return farm_class::grassland;

    // Bare hard ground, once nothing above has claimed it.
    if (s == terrain_substrate::rocky || s == terrain_substrate::metallic
        || s == terrain_substrate::regolith)
        return farm_class::stone;

    if (s == terrain_substrate::barren)
        return farm_class::arid;

    // Open ground the other eleven tests did not recognise — soil under scrub or
    // nothing. The honest default, not a sentinel.
    return farm_class::steppe;
}

// ---------------------------------------------------------------------------
// The package
// ---------------------------------------------------------------------------

namespace
{

/// Whether the tile at (col, row) touches water — the shoreline test the farm
/// classes and the year cost both need. East-west wraps (the body is a
/// cylinder); the poles do not.
bool touches_water(const std::vector<terrain_substrate>& sub, int gw, int gh,
                   int col, int row)
{
    for (int side = 0; side < 6; ++side)
    {
        const auto n = hex_neighbors::neighbour(col, row, side);
        if (n.gy < 0 || n.gy >= gh) continue;
        int nx = n.gx % gw;
        if (nx < 0) nx += gw;
        const std::size_t ni = static_cast<std::size_t>(n.gy) * static_cast<std::size_t>(gw)
                             + static_cast<std::size_t>(nx);
        if (ni < sub.size() && is_water(sub[ni])) return true;
    }
    return false;
}

} // namespace

domestication_package coin_package(const std::vector<terrain_substrate>& substrate,
                                   const std::vector<terrain_cover>&     cover,
                                   const std::vector<terrain_landform>&  landform,
                                   int gw, int gh, int col, int row,
                                   int window_radius)
{
    domestication_package pkg;
    if (gw <= 0 || gh <= 0 || substrate.empty()) return pkg;

    // COUNT THE GROUND THE CRADLE ACTUALLY SAW. Nothing else: affinity is
    // derived from the cradle's own window, and a package carries nothing about
    // a steppe it never looked at.
    std::array<int, farm_class_count> seen{};
    int land = 0;

    for (int dr = -window_radius; dr <= window_radius; ++dr)
    {
        const int r = row + dr;
        if (r < 0 || r >= gh) continue;
        for (int dc = -window_radius; dc <= window_radius; ++dc)
        {
            int c = (col + dc) % gw;
            if (c < 0) c += gw;
            const std::size_t i = static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                + static_cast<std::size_t>(c);
            if (i >= substrate.size() || is_water(substrate[i])) continue;

            const terrain_cover    cv = i < cover.size()    ? cover[i]    : terrain_cover::none;
            const terrain_landform lf = i < landform.size() ? landform[i] : terrain_landform::plains;
            const farm_class f = classify_farm_class(substrate[i], cv, lf,
                                                     touches_water(substrate, gw, gh, c, r));
            ++seen[static_cast<std::size_t>(f)];
            ++land;
        }
    }

    if (land == 0) return pkg; // A cradle with no land in its window coins nothing.

    // AFFINITY IS THE SHARE OF THE WINDOW THAT GROUND OCCUPIED, scaled so a
    // window made entirely of one class yields 1000 on it. A class the window
    // barely held falls under `package_affinity_floor` and is dropped to zero
    // outright — the difference between "farms it badly" and "does not farm it"
    // is the whole of what makes a frontier stall, so a small number here would
    // quietly delete every frontier in the world.
    //
    // THE SQUARE ROOT THAT IS NOT HERE. The obvious shaping — soften the share
    // so a 5% class still enters — was rejected for exactly that reason: it
    // makes every package broad, and breadth is the asymmetry generator. A
    // cradle that saw a gradient should be broad BECAUSE it saw a gradient.
    int breadth = 0;
    for (int f = 0; f < farm_class_count; ++f)
    {
        const int share = static_cast<int>((static_cast<int64_t>(seen[static_cast<std::size_t>(f)])
                                            * 1000) / land);
        if (share < package_affinity_floor) continue;
        // The cradle's home ground is farmed well, not merely at its own share:
        // a people that lives on floodplains is GOOD at floodplains. Scaled so a
        // half-window class reaches the ceiling.
        pkg.affinity[static_cast<std::size_t>(f)] =
            static_cast<uint16_t>(clampi(share * 2, 0, 1000));
        ++breadth;
    }
    pkg.breadth = static_cast<uint8_t>(clampi(breadth, 0, farm_class_count));
    return pkg;
}

domestication_package cross_packages(const domestication_package& a,
                                     const domestication_package& b)
{
    domestication_package out;
    int breadth = 0;
    for (int f = 0; f < farm_class_count; ++f)
    {
        const std::size_t i = static_cast<std::size_t>(f);
        // MAXIMUM, never a sum. A daughter is never better than the better
        // parent — the floor the design names, expressed as the only operation
        // that cannot manufacture skill neither parent had.
        out.affinity[i] = std::max(a.affinity[i], b.affinity[i]);
        if (out.affinity[i] > 0) ++breadth;
    }
    out.breadth = static_cast<uint8_t>(clampi(breadth, 0, farm_class_count));
    return out;
}

// ---------------------------------------------------------------------------
// Predation
// ---------------------------------------------------------------------------

int predation_base_q(int body_biosphere_q, terrain_cover c, terrain_landform lf)
{
    // THE BODY TERM GATES THE WHOLE QUANTITY, and multiplicatively rather than
    // additively. A world that never reached land animals carries NONE of this
    // at all — not "a little" — so the ground term must be unable to
    // manufacture danger on a sterile rock.
    const int body = clampi(body_biosphere_q, 0, 1000);
    if (body == 0) return 0;

    // The region term, from cover. Dense forest and wetland are dangerous; open
    // lowland and cold country much less so.
    int ground = 200; // Open ground: something is always out there.
    switch (c)
    {
        case terrain_cover::forest: ground = 900; break;
        case terrain_cover::marsh:  ground = 800; break;
        case terrain_cover::scrub:  ground = 450; break;
        case terrain_cover::grass:  ground = 300; break;
        case terrain_cover::snow:   ground = 120; break;
        case terrain_cover::dunes:
        case terrain_cover::salt:   ground = 100; break;
        case terrain_cover::ash:    ground = 150; break;
        case terrain_cover::urban:  ground =  60; break;
        case terrain_cover::none:   ground = 200; break;
    }

    // Broken ground shelters what lives in it. A modest addend, not a term of
    // its own — the design names cover, and this keeps a mountain forest worse
    // than a plains forest without inventing a second axis.
    if (lf == terrain_landform::mountain || lf == terrain_landform::canyon
        || lf == terrain_landform::rift)
        ground += 100;

    return clampi(static_cast<int>((static_cast<int64_t>(ground) * body) / 1000), 0, 1000);
}

int predation_now_q(int base_q, int64_t population, int per_doubling_q)
{
    const int base = clampi(base_q, 0, 1000);
    if (base == 0 || population <= 1) return base;

    // LOGARITHMIC IN POPULATION: each doubling buys the same fixed reduction, so
    // the returns diminish forever. Counted from ONE head rather than from the
    // founding band, so the curve does not depend on how large a founding party
    // happened to be.
    //
    // THE COEFFICIENT MUST BE SIZED TO THE RANGE THAT ACTUALLY OCCURS, and the
    // first cut was not — which made the whole mechanism inert at game scale
    // rather than merely mistuned. A region is founded with ~2,000 heads
    // (settlement.cpp § Antiquity stop) and grows toward a carrying capacity in
    // the high hundreds of thousands, so the live range is roughly 11 to 20
    // doublings. At 90 per doubling the floor below is reached at TEN — 1,024
    // heads — so every region the game ever has sits on the floor and predation
    // is a constant wearing a decay's name. The harness caught it as three
    // identical readings at 1k / 2k / 4k heads.
    //
    // The default is therefore set so the curve is still moving across the whole
    // of that range and only saturates well past it (see the header).
    const int64_t bought = static_cast<int64_t>(doublings(population))
                         * static_cast<int64_t>(clampi(per_doubling_q, 0, 1000));

    // NEVER FULLY BOUGHT OFF. The floor is what makes predation a transient
    // frontier cost rather than a wall AND stops it becoming a wall's opposite:
    // a decay reaching zero would make every frontier temporary, which the
    // design calls the same defect wearing the opposite sign. A tenth of the
    // wild stands whatever lives on it.
    const int64_t remaining = 1000 - std::min<int64_t>(bought, 900);
    return clampi(static_cast<int>((static_cast<int64_t>(base) * remaining) / 1000), 0, 1000);
}

int predation_capacity_mult_q(int predation_q)
{
    // Sustainable population sits BELOW `region_carrying_capacity(farm_q)` by
    // this fraction. Half the capacity at maximum danger: enough that lethal
    // ground cannot hold the density Stage 0 needs for surplus (the predation
    // floor, which `agrarian_score` is meant to keep a cradle away from in the
    // first place), and not so much that dangerous ground is simply unusable.
    return clampi(1000 - clampi(predation_q, 0, 1000) / 2, 500, 1000);
}

// ---------------------------------------------------------------------------
// The year cost
// ---------------------------------------------------------------------------

int32_t tile_year_cost(terrain_substrate s, terrain_cover c, terrain_landform lf,
                       bool shoreline, bool river)
{
    // OPEN OCEAN IS IMPASSABLE, gated on the substrate that already IS the real
    // water test. Lakes and the coastal shelf are not ocean and are not walked
    // either — a stream moves over land, along the water, never on it.
    if (is_water(s)) return colonisation_impassable;

    int32_t cost = colonisation_base_centiyears;

    // THE CHEAP ROUTES — the ones people actually followed, and the discount is
    // deliberately STEEP (BL-857; Ben, 2026-09-09).
    //
    // THE COAST IS NOT MERELY CHEAP, IT IS THE ROAD. At the first cut's 55% a
    // coastal route was preferable and not decisive, so streams pushed inland
    // about as readily as along the shore and the map's first cultures did not
    // string out the way real ones did. Early migration follows shorelines
    // because the shore FEEDS you while you walk it, so the discount has to be
    // strong enough that a coastal route beats an inland one over any
    // comparable distance. An inland-first map is the tell that this is
    // mispriced.
    //
    // A RIVER COURSE IS THE SAME ROAD (BL-967): one corridor price for the
    // shore and the river bank, because COLONISATION.md names them as one tier
    // of cheap ground and a second number would be a second claim. The first
    // cut priced a river at 22% against the coast's 30% — a preference the
    // design never stated, retired with the constant.
    if (river || shoreline) cost = (cost * colonisation_corridor_pct) / 100;

    // Barrier terrain is DEAR, and this is where three centuries of mountain
    // come from. Multiplicative on the base so a forested mountain is worse
    // than either alone — the design's "priced twice" applies to crossing as
    // much as to living.
    switch (lf)
    {
        case terrain_landform::mountain: cost = (cost * 600) / 100; break;
        case terrain_landform::rift:     cost = (cost * 400) / 100; break;
        case terrain_landform::canyon:   cost = (cost * 350) / 100; break;
        case terrain_landform::highland: cost = (cost * 175) / 100; break;
        case terrain_landform::crater:   cost = (cost * 150) / 100; break;
        case terrain_landform::valley:   cost = (cost *  90) / 100; break;
        case terrain_landform::plains:   break;
    }

    switch (c)
    {
        case terrain_cover::forest: cost = (cost * 250) / 100; break;
        case terrain_cover::marsh:  cost = (cost * 200) / 100; break;
        case terrain_cover::dunes:  cost = (cost * 400) / 100; break;
        case terrain_cover::salt:   cost = (cost * 300) / 100; break;
        case terrain_cover::snow:   cost = (cost * 300) / 100; break;
        case terrain_cover::scrub:  cost = (cost * 120) / 100; break;
        case terrain_cover::ash:    cost = (cost * 150) / 100; break;
        case terrain_cover::grass:
        case terrain_cover::urban:
        case terrain_cover::none:   break;
    }

    if (s == terrain_substrate::icy) cost = (cost * 250) / 100;

    // A floor of one year: no ground is free to cross, and a zero would let the
    // frontier expand without the clock moving at all.
    return cost < 100 ? 100 : cost;
}

// ---------------------------------------------------------------------------
// The flood
// ---------------------------------------------------------------------------

namespace
{

/// One entry on the frontier. Ordered by (arrival, tile, source) so the pop
/// order is a TOTAL order on stable indices — never on heap layout.
struct front_entry
{
    int64_t arrival_cy = 0;  ///< Centi-years since the run's zero point.
    int32_t tile       = 0;
    int32_t source     = -1; ///< Index into the caller's `sources`.
    /// The culture this stream is CARRYING — the source's, or a daughter it
    /// coined on the way (BL-856). Inherited from the tile it expanded from,
    /// which is what makes divergence follow the route rather than the source.
    int32_t culture    = -1;
    /// Centi-years since this stream last diverged. Crossing
    /// `colonisation_split_centiyears` coins a daughter and resets it.
    int64_t since_split_cy = 0;
    /// The farm class this stream's culture was coined on (BL-864). Settling
    /// ground of a DIFFERENT class is what makes a daughter: a people that
    /// comes down from the highlands onto a floodplain farms differently, eats
    /// differently, and within a few centuries is different.
    farm_class origin_class = farm_class::steppe;
};

/// Greater-than, because std::priority_queue is a MAX heap and we want the
/// earliest arrival first.
struct front_worse
{
    bool operator()(const front_entry& a, const front_entry& b) const
    {
        if (a.arrival_cy != b.arrival_cy) return a.arrival_cy > b.arrival_cy;
        if (a.tile       != b.tile)       return a.tile       > b.tile;
        return a.source > b.source;
    }
};

} // namespace

colonisation_field run_colonisation(const colonisation_input& in,
                                    const std::vector<colonisation_source>& sources)
{
    colonisation_field f;
    if (in.substrate == nullptr || in.gw <= 0 || in.gh <= 0) return f;

    const std::vector<terrain_substrate>& sub = *in.substrate;
    const int gw = in.gw, gh = in.gh;
    const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    if (sub.size() < n) return f;

    f.gw = gw;
    f.gh = gh;
    f.arrival_year.assign(n, colonisation_never_reached);
    f.source_region.assign(n, -1);
    f.culture.assign(n, -1);
    f.ground.assign(n, farm_class::steppe);
    f.farmable.assign(n, 0u);
    f.last_arrival_year = in.backstop_year; // Overwritten by the first landing.
    bool any_arrival = false;

    const auto cover_at = [&](std::size_t i) {
        return (in.cover && i < in.cover->size()) ? (*in.cover)[i] : terrain_cover::none;
    };
    const auto landform_at = [&](std::size_t i) {
        return (in.landform && i < in.landform->size()) ? (*in.landform)[i]
                                                        : terrain_landform::plains;
    };
    const auto river_at = [&](std::size_t i) {
        return (in.river && i < in.river->size()) && (*in.river)[i] != 0;
    };

    // --- Classify the ground once ------------------------------------------
    //
    // Both the flood and the founding gate read the class, and the shoreline
    // test walks six neighbours, so doing it per pop would pay for it as many
    // times as a tile is considered. One raster pass instead.
    std::vector<uint8_t> shore(n, 0u);
    for (int row = 0; row < gh; ++row)
        for (int col = 0; col < gw; ++col)
        {
            const std::size_t i = static_cast<std::size_t>(row) * static_cast<std::size_t>(gw)
                                + static_cast<std::size_t>(col);
            if (is_water(sub[i])) continue;
            shore[i] = touches_water(sub, gw, gh, col, row) ? 1u : 0u;
            f.ground[i] = classify_farm_class(sub[i], cover_at(i), landform_at(i), shore[i] != 0u);
        }

    // --- Seed the frontier from every source -------------------------------
    //
    // A source arrives at its OWN anchor in its own ready year at zero extra
    // cost: it is already there. Centi-years are counted from the run's zero
    // point (calendar year 0) so a negative BCE year needs no special case.
    std::priority_queue<front_entry, std::vector<front_entry>, front_worse> front;
    for (std::size_t si = 0; si < sources.size(); ++si)
    {
        const colonisation_source& s = sources[si];
        if (s.tile < 0 || static_cast<std::size_t>(s.tile) >= n) continue;
        if (is_water(sub[static_cast<std::size_t>(s.tile)])) continue;
        front.push(front_entry{static_cast<int64_t>(s.ready_year) * 100,
                               s.tile, static_cast<int32_t>(si), s.culture, 0,
                               f.ground[static_cast<std::size_t>(s.tile)]});
    }

    const int64_t boundary_cy = static_cast<int64_t>(in.backstop_year) * 100;

    // THE DAUGHTER-CULTURE ALLOCATOR (BL-856). Ids run upward from one past the
    // last cradle culture, handed out in ARRIVAL order because the heap pops in
    // arrival order — so the numbering is a record of when each people came to
    // be, and is identical on every machine. A negative `first_spawn_culture`
    // disables spawning entirely, which is what the synthetic route cases want.
    int32_t next_culture = in.first_spawn_culture;

    // A PEOPLE DIVERGES OCCASIONALLY, NOT ONCE PER TILE — the grain fix.
    //
    // The first cut split whenever ANY advancing tile had been walking longer
    // than the threshold. Every tile on a frontier carries its own clock, so a
    // broad lobe crossed the threshold across its whole width at once and coined
    // a separate culture on each tile: 1,739 / 2,474 / 1,231 "peoples" per world
    // against 13-17 that actually held ground. The mechanism was right and the
    // GRAIN was wrong — it was measuring tiles, not streams.
    //
    // So a lineage may only diverge once per `colonisation_split_centiyears`.
    // The whole lobe that crosses together produces ONE daughter, which is what
    // a people splitting actually looks like, and the count becomes a function
    // of the span rather than of the map's width.
    //
    // Keyed by PARENT culture and compared against arrival year, both of which
    // are stable integers, so this adds no order dependence: two tiles crossing
    // in the same year still resolve on the heap's (year, tile, source) order.
    std::vector<int64_t> last_split_cy;

    // PER-CULTURE BOOKKEEPING FOR THE TWO NEW TRIGGERS (BL-864). Both are
    // consequences of the walk rather than decisions, so the span keeps its
    // no-actor rule: nothing here chooses to divide, it simply has.
    //
    //  * `culture_tiles` -- how much ground this people holds, counted as tiles
    //    are claimed. Reset on a split, so the SIZE trigger is bounded by
    //    (land tiles / colonisation_culture_max_tiles).
    //  * `last_biome_split_cy` -- when this lineage last coined a daughter by
    //    BIOME (BL-918). The trigger now fires on EVERY farm-class transition
    //    rather than once per (culture, class) pair as BL-864 bounded it; what
    //    bounds it instead is time in the country on two clocks -- see
    //    `colonisation_biome_split_centiyears` for both and for why the old
    //    bound was against the runaway rather than against the design.
    std::vector<int32_t> culture_tiles;
    std::vector<int64_t> last_biome_split_cy;
    const auto grow_culture_state = [&](std::size_t k) {
        if (k >= culture_tiles.size())
        {
            culture_tiles.resize(k + 1, 0);
            last_biome_split_cy.resize(k + 1, INT64_MIN);
        }
    };
    // ONE RATE LIMITER, TWO CLOCKS: the distance trigger's and the biome
    // trigger's are kept apart so a lineage that just diverged by walking may
    // still diverge by crossing into new country, and vice versa -- the two
    // are different reasons and the design wants both counted.
    const auto rate_limited = [](std::vector<int64_t>& last, int32_t parent, int64_t at,
                                 int64_t interval) {
        if (parent < 0) return false;
        const std::size_t k = static_cast<std::size_t>(parent);
        if (k >= last.size()) last.resize(k + 1, INT64_MIN);
        if (last[k] != INT64_MIN && at - last[k] < interval) return false;
        last[k] = at;
        return true;
    };
    const auto may_split = [&](int32_t parent, int64_t at) {
        return rate_limited(last_split_cy, parent, at, colonisation_split_centiyears);
    };
    const auto may_split_biome = [&](int32_t parent, int64_t at) {
        return rate_limited(last_biome_split_cy, parent, at,
                            colonisation_biome_lineage_centiyears);
    };
    const auto tally = [&](split_trigger t) {
        ++f.split_census[static_cast<std::size_t>(t)];
    };

    // --- Walk ---------------------------------------------------------------
    while (!front.empty())
    {
        const front_entry e = front.top();
        front.pop();

        const std::size_t i = static_cast<std::size_t>(e.tile);
        // Already claimed by an earlier arrival. THIS IS THE WHOLE OF
        // "whoever's stream arrived, arrived": the first pop of a tile wins it,
        // and every later one is discarded without a comparison.
        if (f.arrival_year[i] != colonisation_never_reached) continue;
        // NOTHING ARRIVES AFTER THE BOUNDARY. The span ends; streams still in
        // flight never land, and the ground they were crossing toward stays
        // empty. That is the structural half a harness may assert — no
        // diffusion runs after the boundary.
        if (e.arrival_cy > boundary_cy) continue;

        const colonisation_source& src = sources[static_cast<std::size_t>(e.source)];

        f.arrival_year[i]  = e.arrival_cy / 100;
        // THE MIGRATION'S END (BL-858). Pops come out in ascending arrival
        // order, so the last tile claimed carries the year the last stream
        // landed -- the moment nothing more is going to happen.
        if (!any_arrival) { f.last_arrival_year = f.arrival_year[i]; any_arrival = true; }
        else if (f.arrival_year[i] > f.last_arrival_year)
            f.last_arrival_year = f.arrival_year[i];
        if (e.culture >= 0)
        {
            grow_culture_state(static_cast<std::size_t>(e.culture));
            ++culture_tiles[static_cast<std::size_t>(e.culture)];
        }
        f.source_region[i] = src.region;
        f.culture[i]       = e.culture; // The stream's, which may be a daughter.
        // GROUND NO PACKAGE SUITS IS NOT SETTLED. The stream still CROSSES it —
        // a people walks over a mountain it cannot farm — so the tile is
        // claimed and passed through, but it is not a founding candidate.
        // Emptiness is a real outcome here, not a failure to fill.
        f.farmable[i] = src.package.can_farm(f.ground[i]) ? 1u : 0u;

        const int col = static_cast<int>(i) % gw;
        const int row = static_cast<int>(i) / gw;

        for (int side = 0; side < 6; ++side)
        {
            const auto nb = hex_neighbors::neighbour(col, row, side);
            if (nb.gy < 0 || nb.gy >= gh) continue;
            int nx = nb.gx % gw;         // The body is a cylinder: east-west wraps.
            if (nx < 0) nx += gw;
            const std::size_t ni = static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(nx);
            if (f.arrival_year[ni] != colonisation_never_reached) continue;

            const int32_t step = tile_year_cost(sub[ni], cover_at(ni), landform_at(ni),
                                                shore[ni] != 0u, river_at(ni));
            if (step == colonisation_impassable) continue;

            const int64_t at = e.arrival_cy + step;
            if (at > boundary_cy) continue; // Cannot land in time; do not queue it.

            // MIGRATION SPAWNS CULTURES (BL-856). A stream carries its people
            // until it has been walking long enough that the ones who arrive
            // are no longer the ones who set out; then it coins a daughter and
            // the clock restarts. No actor: distance and time do this, not a
            // decision, and the split is a consequence of the walk in exactly
            // the sense this file's header means.
            int32_t    child_culture = e.culture;
            int64_t    child_since   = e.since_split_cy + step;
            farm_class child_origin  = e.origin_class;
            const farm_class g       = f.ground[ni];

            // THREE WAYS A PEOPLE BECOMES TWO, and none of them is a decision.
            //
            //  COUNTRY -- it has settled ground of a class it was not coined on
            //    (BL-864). The truest of the three: the package IS coined from a
            //    class, so "this is different country" is a question the model
            //    can already answer about itself.
            //  SIZE -- it holds more ground than one people holds (BL-864).
            //  DRIFT -- it has simply been walking long enough (BL-856).
            //
            // COUNTRY is tested first: where a stream crosses into new country
            // AND has been walking a long time, the country is the better
            // explanation of why its children differ -- and the daughter becomes
            // a people OF that country, inheriting the class as its own origin.
            if (next_culture >= 0 && e.culture >= 0)
            {
                const std::size_t k = static_cast<std::size_t>(e.culture);
                grow_culture_state(k);

                bool          split = false;
                split_trigger why   = split_trigger::distance;
                // COUNTRY, ON EVERY TRANSITION (BL-918), gated by time in the
                // country on both clocks -- the stream's own and its
                // lineage's. A stream that has just been coined cannot be
                // re-coined on the next tile of a jagged boundary, and a
                // lineage's whole lobe crossing at once coins one daughter.
                if (g != e.origin_class
                    && child_since >= colonisation_biome_split_centiyears
                    && may_split_biome(e.culture, at))
                {
                    child_origin = g;
                    split        = true;
                    why          = split_trigger::biome;
                }
                else if (culture_tiles[k] >= colonisation_culture_max_tiles)
                {
                    culture_tiles[k] = 0;
                    split            = true;
                    why              = split_trigger::size;
                }
                else if (child_since >= colonisation_split_centiyears
                         && may_split(e.culture, at))
                {
                    split = true;
                    why   = split_trigger::distance;
                }

                if (split)
                {
                    child_culture = next_culture++;
                    child_since   = 0;
                    f.spawns.push_back(culture_spawn{child_culture, e.culture,
                                                     static_cast<int32_t>(ni),
                                                     child_origin, at / 100,
                                                     /*crossed_water=*/false, why});
                    tally(why);
                }
            }
            front.push(front_entry{at, static_cast<int32_t>(ni), e.source,
                                   child_culture, child_since, child_origin});
        }

        // --- The crude overseas hop (BL-857) ------------------------------
        //
        // Open water is impassable to the WALK above and was never impassable
        // to people. A short crossing — to an island already visible from the
        // shore, or over a strait — is how the awkward corners of a world get
        // peopled, and without it they simply never do.
        //
        // ONLY FROM THE SHORE, and that falls out rather than being tested: a
        // tile with no water neighbour enqueues nothing below.
        //
        // A BOUNDED FLOOD OVER WATER, land to land. Breadth-first over water
        // tiles to a depth of `colonisation_max_hop_tiles`, landing on the
        // first land it reaches. The bound is the whole of what keeps this
        // CRUDE: three tiles crosses a strait and cannot cross an ocean, so
        // "people got everywhere" never becomes "people sailed". A harness
        // asserts it.
        //
        // DETERMINISM: the water frontier is walked in raster order and every
        // landing is pushed onto the same totally-ordered heap as a land step,
        // so a hop and a walk arriving in the same year break their tie on
        // (tile, source) exactly as two walks do.
        if (shore[i] != 0u)
        {
            std::vector<std::size_t> wave{i};
            std::vector<uint8_t>     seen_water;
            for (int depth = 1; depth <= colonisation_max_hop_tiles && !wave.empty(); ++depth)
            {
                std::vector<std::size_t> next;
                for (const std::size_t wi : wave)
                {
                    const int wc = static_cast<int>(wi) % gw;
                    const int wr = static_cast<int>(wi) / gw;
                    for (int side = 0; side < 6; ++side)
                    {
                        const auto nb = hex_neighbors::neighbour(wc, wr, side);
                        if (nb.gy < 0 || nb.gy >= gh) continue;
                        int nx = nb.gx % gw;
                        if (nx < 0) nx += gw;
                        const std::size_t ni =
                            static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                            + static_cast<std::size_t>(nx);

                        if (!is_water(sub[ni]))
                        {
                            // LANDFALL. Already-claimed ground is skipped by the
                            // pop guard anyway, but not queueing it keeps the
                            // heap small.
                            if (f.arrival_year[ni] != colonisation_never_reached) continue;
                            const int64_t at = e.arrival_cy
                                             + static_cast<int64_t>(depth)
                                                   * colonisation_hop_centiyears;
                            if (at > boundary_cy) continue;
                            // A CROSSING AGES A STREAM like any other travel,
                            // so a hop can coin a daughter exactly as a walk
                            // can — which is how an island ends up with its own
                            // people rather than a copy of the mainland's.
                            const int64_t hop_cost = static_cast<int64_t>(depth)
                                                   * colonisation_hop_centiyears;
                            int32_t hop_culture = e.culture;
                            int64_t hop_since   = e.since_split_cy + hop_cost;
                            if (hop_since >= colonisation_split_centiyears
                                && next_culture >= 0 && may_split(e.culture, at))
                            {
                                hop_culture = next_culture++;
                                hop_since   = 0;
                                f.spawns.push_back(culture_spawn{hop_culture, e.culture,
                                                                 static_cast<int32_t>(ni),
                                                                 e.origin_class, at / 100,
                                                                 /*crossed_water=*/true,
                                                                 split_trigger::distance});
                                tally(split_trigger::distance);
                            }
                            front.push(front_entry{at, static_cast<int32_t>(ni), e.source,
                                                   hop_culture, hop_since, e.origin_class});
                            continue;
                        }

                        if (depth == colonisation_max_hop_tiles) continue; // No further.
                        if (seen_water.empty()) seen_water.assign(n, 0u);
                        if (seen_water[ni]) continue;
                        seen_water[ni] = 1u;
                        next.push_back(ni);
                    }
                }
                wave.swap(next);
            }
        }
    }

    return f;
}

// ---------------------------------------------------------------------------
// Why a cradle stopped (BL-851)
// ---------------------------------------------------------------------------

const char* cradle_outcome_name(cradle_outcome o)
{
    switch (o)
    {
        case cradle_outcome::spread:          return "spread";
        case cradle_outcome::sterility:       return "sterility";
        case cradle_outcome::encirclement:    return "encirclement";
        case cradle_outcome::dilution:        return "dilution";
        case cradle_outcome::predation_floor: return "predation floor";
    }
    return "?";
}

std::vector<cradle_outcome> classify_cradle_outcomes(
    const colonisation_field&               f,
    const std::vector<colonisation_source>& sources,
    const std::vector<cradle_vitals>&       vitals)
{
    std::vector<cradle_outcome> out(sources.size(), cradle_outcome::spread);
    if (f.empty()) return out;

    const int gw = f.gw, gh = f.gh;

    // --- Per-source tallies, in ONE raster pass ---------------------------
    //
    // One pass rather than one per source: the map is walked once and every
    // source's counters are updated from the tile it claimed. That keeps this a
    // read over the field rather than a second search, which is the whole
    // reason the field exists.
    std::vector<int64_t> claimed(sources.size(), 0);
    std::vector<int64_t> farmable_held(sources.size(), 0);

    // REGION INDEX -> SOURCE INDEX, built once.
    //
    // `source_region` carries the CALLER'S region index, not our source index,
    // so the two must be matched rather than assumed equal — a caller may pass
    // a subset of its regions as sources. The obvious way to match them is a
    // scan over `sources` per tile, and that is O(tiles x sources): 1.8M
    // comparisons at today's 150 regions, and 30M at the 2,500 the region-count
    // question (NR-809) is about. A classifier that scales worse than the walk
    // it classifies would be an odd thing to ship in the same commit as a
    // measurement saying so.
    //
    // A DENSE VECTOR RATHER THAN A MAP, because region indices are small,
    // contiguous and already bounded by `owner_index_limit` — and because an
    // unordered container keyed here would be one more place for a walk order
    // to depend on a hash.
    int32_t max_region = -1;
    for (const colonisation_source& s : sources)
        if (s.region > max_region) max_region = s.region;
    std::vector<int32_t> source_of(static_cast<std::size_t>(max_region + 1), -1);
    for (std::size_t s = 0; s < sources.size(); ++s)
        if (sources[s].region >= 0)
        {
            // FIRST WINS, so a duplicated region index is resolved by source
            // order rather than by whichever happened to be written last.
            int32_t& slot = source_of[static_cast<std::size_t>(sources[s].region)];
            if (slot < 0) slot = static_cast<int32_t>(s);
        }

    const auto source_index = [&](int32_t region) -> int32_t {
        if (region < 0 || region > max_region) return -1;
        return source_of[static_cast<std::size_t>(region)];
    };

    for (std::size_t i = 0; i < f.source_region.size(); ++i)
    {
        const int32_t s = source_index(f.source_region[i]);
        if (s < 0) continue;
        ++claimed[static_cast<std::size_t>(s)];
        if (f.farmable[i]) ++farmable_held[static_cast<std::size_t>(s)];
    }

    // --- WHY did each stopped source stop? Read its own frontier. ----------
    //
    // THE TEST THAT SEPARATES DILUTION FROM ENCIRCLEMENT, and getting it wrong
    // is what the first cut did. That version asked only "did this source get
    // much ground", and reported everything that did not as ENCIRCLEMENT. On
    // real worlds that read 317 encircled against 186 spread and ZERO sterile,
    // which is not a measurement of anything: with 149 sources flooding one map
    // simultaneously, most of them are simply born into ground their neighbours
    // already hold. Being outnumbered is not being penned, and a sweep told
    // otherwise would tune the barrier costs to fix a crowding effect.
    //
    // So the question is asked of the FRONTIER instead. Walk the tiles adjacent
    // to what a source holds but not held by it, and count two kinds:
    //   WALL  — water, or ground no stream ever reached. Terrain stopped it.
    //   RIVAL — ground another stream got to first. Somebody else stopped it.
    // Whichever is larger is why it stopped. That is a claim about cause rather
    // than about size, which is the whole of what BL-851 asked for.
    //
    // Testing the whole map instead of the frontier would report every cradle
    // in the world as diluted, since somewhere on a continent there is always
    // suitable ground in somebody else's hands.
    std::vector<int64_t> wall_edge(sources.size(), 0);
    std::vector<int64_t> rival_edge(sources.size(), 0);
    for (std::size_t i = 0; i < f.source_region.size(); ++i)
    {
        const int32_t sr = f.source_region[i];
        const int32_t si = source_index(sr);
        if (si < 0) continue;
        const std::size_t s = static_cast<std::size_t>(si);

        const int col = static_cast<int>(i) % gw;
        const int row = static_cast<int>(i) / gw;
        for (int side = 0; side < 6; ++side)
        {
            const auto nb = hex_neighbors::neighbour(col, row, side);
            if (nb.gy < 0 || nb.gy >= gh) { ++wall_edge[s]; continue; }
            int nx = nb.gx % gw;
            if (nx < 0) nx += gw;
            const std::size_t ni = static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(nx);
            if (ni >= f.source_region.size()) continue;

            const int32_t nsr = f.source_region[ni];
            if (nsr == sr) continue;               // Its own ground; not a frontier at all.
            if (nsr < 0) { ++wall_edge[s]; continue; } // Water, or ground nobody reached.
            ++rival_edge[s];                       // Somebody else got there first.
        }
    }

    // --- Classify, most-fatal first ----------------------------------------
    //
    // A cradle that is BOTH penned and dying is reported as dying: the reading
    // that matters is the one that ends the people, and the sweep should never
    // have to guess which of two labels it was given.
    for (std::size_t s = 0; s < sources.size(); ++s)
    {
        const bool has_vitals = s < vitals.size();
        if (has_vitals && vitals[s].surplus_threshold > 0
            && vitals[s].sustainable < vitals[s].surplus_threshold)
        {
            out[s] = cradle_outcome::predation_floor;
            continue;
        }

        // It holds farmable ground beyond its own anchor tile. The anchor is
        // subtracted so a cradle sitting alone on one farmable tile is not
        // counted as having spread onto it.
        if (farmable_held[s] > 1) { out[s] = cradle_outcome::spread; continue; }

        // PRE-EMPTED OUTRIGHT is the purest dilution there is, and it does not
        // reach the `outbid` test — that test walks the tiles a source HOLDS,
        // and this source holds none. A rival's stream reached its anchor
        // before it was ready to send, so it never claimed even the ground it
        // was standing on.
        //
        // Without this clause it read as ENCIRCLEMENT, because "claimed almost
        // nothing" is also what a penned cradle looks like. The two are
        // opposite findings — one says the ground was taken, the other says
        // there was nowhere to go — and a sweep that confused them would be
        // exactly the no-information reading BL-851 exists to remove.
        if (claimed[s] == 0) { out[s] = cradle_outcome::dilution; continue; }

        // A RIVAL-DOMINATED FRONTIER IS DILUTION. Most of what this stream
        // could have walked into is already somebody else's.
        if (rival_edge[s] > wall_edge[s]) { out[s] = cradle_outcome::dilution; continue; }

        // ENCIRCLEMENT AGAINST STERILITY, and this is the only place the two
        // are separated. Both stopped against terrain rather than against a
        // rival; the difference is whether the stream MOVED. A stream that
        // crossed real ground and found nothing it could farm is STERILE —
        // narrow affinity, no matching class anywhere it reached. A stream that
        // barely left its anchor never got the chance to find out: every exit
        // was barrier terrain whose year-cost the span did not pay.
        //
        // The threshold is deliberately small and deliberately stated. Six is
        // one hex ring: a stream that claimed no more than its own immediate
        // neighbourhood did not walk.
        out[s] = claimed[s] > 6 ? cradle_outcome::sterility
                                : cradle_outcome::encirclement;
    }

    return out;
}

int64_t colonisation_field_bytes(const colonisation_field& f)
{
    const int64_t n = static_cast<int64_t>(f.arrival_year.size());
    return n * static_cast<int64_t>(sizeof(int64_t)      // arrival_year
                                    + sizeof(int32_t)    // source_region
                                    + sizeof(int32_t)    // culture
                                    + sizeof(farm_class) // ground
                                    + sizeof(uint8_t));  // farmable
}

const char* split_trigger_name(split_trigger t)
{
    switch (t)
    {
        case split_trigger::distance:  return "distance";
        case split_trigger::biome:     return "biome";
        case split_trigger::size:      return "size";
        case split_trigger::isolation: return "isolation";
        case split_trigger::count:     break;
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Isolation (BL-918): a range breaks into insular groups
// ---------------------------------------------------------------------------

bool isolation_barrier(terrain_substrate s, terrain_landform lf)
{
    if (is_water(s)) return true;
    switch (lf)
    {
        case terrain_landform::mountain:
        case terrain_landform::canyon:
        case terrain_landform::rift:     return true;
        case terrain_landform::plains:
        case terrain_landform::highland:
        case terrain_landform::valley:
        case terrain_landform::crater:   break;
    }
    return false;
}

namespace
{

/// Signed column difference the SHORT way round the cylinder.
int wrapped_dc(int from, int to, int gw)
{
    int dc = to - from;
    if (dc >  gw / 2) dc -= gw;
    if (dc < -gw / 2) dc += gw;
    return dc;
}

/// floor(a / b) for b > 0, exact for negative a — `/` truncates toward zero.
int floor_div(int a, int b)
{
    const int q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

/// round(a / b) to nearest, halves up, for b > 0. Integer throughout.
int round_div(int a, int b) { return floor_div(2 * a + b, 2 * b); }

} // namespace

bool isolation_adjacent(const std::vector<uint8_t>& barrier, int gw, int gh,
                        int32_t tile_a, int32_t tile_b)
{
    if (gw <= 0 || gh <= 0 || tile_a < 0 || tile_b < 0) return false;
    const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    if (static_cast<std::size_t>(tile_a) >= n || static_cast<std::size_t>(tile_b) >= n)
        return false;
    if (tile_a == tile_b) return true;

    const int ca = tile_a % gw, ra = tile_a / gw;
    const int cb = tile_b % gw, rb = tile_b / gw;
    const int dc = wrapped_dc(ca, cb, gw);
    const int dr = rb - ra;
    const int adc = dc < 0 ? -dc : dc;
    const int adr = dr < 0 ? -dr : dr;
    // CHEBYSHEV OVER THE WRAPPED GRID, as `settlement.cpp`'s `grid_dist` and
    // the sim's region graph both measure -- the same yardstick, so "cut off
    // by sheer distance" here means what "not a neighbour" means there.
    const int dist = adc > adr ? adc : adr;
    if (dist > colonisation_isolation_radius) return false;

    // THE LINE BETWEEN THEM, sampled once per step of the longer axis, both
    // ends excluded (the anchors themselves are settled ground, whatever they
    // sit on). Integer rounding, so the same pair samples the same tiles on
    // every machine; symmetric in (a, b) because round-half-up of t/N and of
    // (N-t)/N land on the same cells when N is walked from both ends -- and
    // where they would not, the pass only ever asks in one order (lower
    // region index first), so the answer is stable regardless.
    for (int t = 1; t < dist; ++t)
    {
        int col = ca + round_div(dc * t, dist);
        col %= gw;
        if (col < 0) col += gw;
        const int row = ra + round_div(dr * t, dist);
        if (row < 0 || row >= gh) return false; // Off the open edge: no route.
        const std::size_t i = static_cast<std::size_t>(row) * static_cast<std::size_t>(gw)
                            + static_cast<std::size_t>(col);
        if (barrier[i] != 0u) return false;
    }
    return true;
}

isolation_result run_isolation_splits(const colonisation_input&      in,
                                      const colonisation_field&      f,
                                      std::vector<isolation_region>& regions,
                                      int32_t&                       next_culture,
                                      int64_t                        start_year,
                                      int64_t                        end_year)
{
    isolation_result out;
    if (in.substrate == nullptr || in.gw <= 0 || in.gh <= 0 || next_culture < 0) return out;
    if (regions.empty() || end_year <= start_year) return out;

    const int gw = in.gw, gh = in.gh;
    const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    if (in.substrate->size() < n) return out;
    const std::size_t R = regions.size();

    // --- The barrier raster, once ---------------------------------------
    std::vector<uint8_t> barrier(n, 0u);
    for (std::size_t i = 0; i < n; ++i)
    {
        const terrain_landform lf = (in.landform && i < in.landform->size())
                                        ? (*in.landform)[i] : terrain_landform::plains;
        barrier[i] = isolation_barrier((*in.substrate)[i], lf) ? 1u : 0u;
    }

    // --- The adjacency, once ----------------------------------------------
    //
    // TERRAIN DOES NOT MOVE, so which pairs of anchors are cheaply adjacent is
    // a fact of the map and is read once; what changes step to step is only
    // which regions exist and whose they are. Bucketed by a cell the size of
    // the radius so each region looks at its 3x3 neighbourhood of cells
    // rather than at every other region: linear in regions times a local
    // count, and the 2,500-region worlds NR-809 is about stay cheap.
    //
    // EDGES ARE (lower index, higher index), gathered in ascending order of
    // the lower index and then of the higher, so the list is the same on
    // every machine whatever the bucket layout.
    const int cell   = colonisation_isolation_radius;
    const int cells_w = (gw + cell - 1) / cell;
    const int cells_h = (gh + cell - 1) / cell;
    std::vector<std::vector<int32_t>> bucket(
        static_cast<std::size_t>(cells_w) * static_cast<std::size_t>(cells_h));
    for (std::size_t r = 0; r < R; ++r)
    {
        const int32_t t = regions[r].tile;
        if (t < 0 || static_cast<std::size_t>(t) >= n) continue;
        const int cx = (t % gw) / cell, cy = (t / gw) / cell;
        bucket[static_cast<std::size_t>(cy) * static_cast<std::size_t>(cells_w)
               + static_cast<std::size_t>(cx)].push_back(static_cast<int32_t>(r));
    }
    std::vector<std::pair<int32_t, int32_t>> edges;
    for (std::size_t r = 0; r < R; ++r)
    {
        const int32_t t = regions[r].tile;
        if (t < 0 || static_cast<std::size_t>(t) >= n) continue;
        const int cx = (t % gw) / cell, cy = (t / gw) / cell;
        std::vector<int32_t> cand;
        for (int dy = -1; dy <= 1; ++dy)
        {
            const int y = cy + dy;
            if (y < 0 || y >= cells_h) continue;
            for (int dx = -1; dx <= 1; ++dx)
            {
                int x = (cx + dx) % cells_w;
                if (x < 0) x += cells_w;
                for (const int32_t o : bucket[static_cast<std::size_t>(y)
                                              * static_cast<std::size_t>(cells_w)
                                              + static_cast<std::size_t>(x)])
                    if (o > static_cast<int32_t>(r)) cand.push_back(o);
            }
        }
        // A wrapped grid with fewer than three cells across can list a
        // neighbour twice; sort and dedupe so the edge list is canonical.
        std::sort(cand.begin(), cand.end());
        cand.erase(std::unique(cand.begin(), cand.end()), cand.end());
        for (const int32_t o : cand)
            if (isolation_adjacent(barrier, gw, gh, t, regions[static_cast<std::size_t>(o)].tile))
                edges.emplace_back(static_cast<int32_t>(r), o);
    }

    // --- The record steps ---------------------------------------------------
    //
    // `isolated_since[r]` is the year region r was first found outside its
    // people's origin component, or NONE while it is inside one. A component's
    // age is its OLDEST member's -- a group that has been cut off for the span
    // has diverged whether or not a new settlement joined it last decade, and
    // reading the youngest member instead would let a growing lobe postpone
    // its own divergence forever. Erring toward more splits is the design's
    // instruction.
    constexpr int64_t NONE = INT64_MIN;
    std::vector<int64_t> isolated_since(R, NONE);
    std::vector<int32_t> parent_of(R);   // Union-find, rebuilt per step.
    std::vector<int32_t> comp_key(R);    // Lowest region index in each component.
    std::vector<int32_t> comp_seat(R);   // Earliest-founded region, ties on index.
    std::vector<int64_t> comp_oldest(R); // min isolated_since over the component.

    const auto find = [&](int32_t x) {
        while (parent_of[static_cast<std::size_t>(x)] != x)
        {
            parent_of[static_cast<std::size_t>(x)] =
                parent_of[static_cast<std::size_t>(parent_of[static_cast<std::size_t>(x)])];
            x = parent_of[static_cast<std::size_t>(x)];
        }
        return x;
    };
    const auto unite = [&](int32_t a, int32_t b) {
        a = find(a);
        b = find(b);
        if (a == b) return;
        // THE LOWER INDEX IS THE ROOT, always: the representative is then a
        // property of the set, not of the order the edges arrived in.
        if (a < b) parent_of[static_cast<std::size_t>(b)] = a;
        else       parent_of[static_cast<std::size_t>(a)] = b;
    };
    const auto earlier = [&](int32_t a, int32_t b) { // Is region a the earlier seat?
        const isolation_region& ra = regions[static_cast<std::size_t>(a)];
        const isolation_region& rb = regions[static_cast<std::size_t>(b)];
        return ra.founded_year != rb.founded_year ? ra.founded_year < rb.founded_year : a < b;
    };

    for (int64_t y = start_year + colonisation_isolation_step_years;; y += colonisation_isolation_step_years)
    {
        if (y > end_year) y = end_year;
        ++out.steps;

        // Partition: a region is present once founded; an edge joins two
        // present regions of the same people.
        for (std::size_t r = 0; r < R; ++r) parent_of[r] = static_cast<int32_t>(r);
        const auto present = [&](int32_t r) {
            const isolation_region& x = regions[static_cast<std::size_t>(r)];
            return x.culture >= 0 && x.founded_year <= y;
        };
        for (const auto& [a, b] : edges)
            if (present(a) && present(b)
                && regions[static_cast<std::size_t>(a)].culture
                       == regions[static_cast<std::size_t>(b)].culture)
                unite(a, b);

        // Per culture, the ORIGIN component is the one holding the earliest
        // settlement. One pass in region order finds each culture's seat and
        // each component's seat; cultures are keyed by id in a dense table.
        int32_t max_culture = -1;
        for (std::size_t r = 0; r < R; ++r)
            if (regions[r].culture > max_culture) max_culture = regions[r].culture;
        std::vector<int32_t> culture_seat(static_cast<std::size_t>(max_culture + 1), -1);
        for (std::size_t r = 0; r < R; ++r)
        {
            const int32_t ri = static_cast<int32_t>(r);
            if (!present(ri)) continue;
            int32_t& seat = culture_seat[static_cast<std::size_t>(regions[r].culture)];
            if (seat < 0 || earlier(ri, seat)) seat = ri;
            const int32_t root = find(ri);
            if (root == ri) { comp_seat[r] = ri; comp_oldest[r] = NONE; }
            else if (earlier(ri, comp_seat[static_cast<std::size_t>(root)]))
                comp_seat[static_cast<std::size_t>(root)] = ri;
        }
        // A region's isolation clock: started when first found outside the
        // origin component, cleared when it is back inside one.
        for (std::size_t r = 0; r < R; ++r)
        {
            const int32_t ri = static_cast<int32_t>(r);
            if (!present(ri)) { isolated_since[r] = NONE; continue; }
            const int32_t root   = find(ri);
            const int32_t origin = find(culture_seat[static_cast<std::size_t>(regions[r].culture)]);
            if (root == origin) { isolated_since[r] = NONE; continue; }
            if (isolated_since[r] == NONE) isolated_since[r] = y;
            int64_t& oldest = comp_oldest[static_cast<std::size_t>(root)];
            if (oldest == NONE || isolated_since[r] < oldest) oldest = isolated_since[r];
        }
        // Coin. Roots are visited in ascending index, so daughters are
        // allocated in a fixed order within the step; each coining rewrites
        // its component and no other, so the order cannot change the outcome.
        for (std::size_t r = 0; r < R; ++r)
        {
            const int32_t ri = static_cast<int32_t>(r);
            if (!present(ri) || find(ri) != ri) continue;           // Not a root.
            if (comp_oldest[r] == NONE) continue;                   // In the origin.
            if (y - comp_oldest[r] < colonisation_isolation_span_years) continue;

            const int32_t parent = regions[r].culture;
            const int32_t seat   = comp_seat[r];
            const int32_t id     = next_culture++;
            const int32_t seat_tile = regions[static_cast<std::size_t>(seat)].tile;
            const farm_class origin_class =
                (seat_tile >= 0 && static_cast<std::size_t>(seat_tile) < f.ground.size())
                    ? f.ground[static_cast<std::size_t>(seat_tile)] : farm_class::steppe;
            out.spawns.push_back(culture_spawn{id, parent, seat_tile, origin_class, y,
                                               /*crossed_water=*/false,
                                               split_trigger::isolation});
            for (std::size_t o = r; o < R; ++o)
            {
                const int32_t oi = static_cast<int32_t>(o);
                if (!present(oi) || find(oi) != ri) continue;
                regions[o].culture = id;
                isolated_since[o]  = NONE; // Its own people now; the clock is theirs.
                out.recultured.push_back(region_reculture{oi, id, parent, y});
            }
        }

        if (y >= end_year) break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// The boundary fold (BL-1017)
// ---------------------------------------------------------------------------

culture_fold fold_empty_cultures(const std::vector<int32_t>& coined_parent,
                                 const std::vector<uint8_t>& holds_ground)
{
    const std::size_t n = coined_parent.size();
    culture_fold out;
    out.parent.assign(n, -1);
    out.folded_into.assign(n, -1);

    const auto linked = [&](std::size_t i) {
        const int32_t p = coined_parent[i];
        return p >= 0 && static_cast<std::size_t>(p) < i;
    };

    // Interior-ness is read off the COINED tree: a folded name that had a
    // daughter is the case that needs re-parenting, whatever became of her.
    std::vector<uint8_t> has_child(n, 0u);
    for (std::size_t i = 0; i < n; ++i)
        if (linked(i)) has_child[static_cast<std::size_t>(coined_parent[i])] = 1u;

    // living[i]: i itself where it keeps its name, else the living ancestor
    // that absorbed it. Filled in ascending order, so living[parent] is always
    // settled by the time a daughter reads it — which is the whole of why no
    // daughter can be orphaned.
    std::vector<int32_t> living(n, -1);
    for (std::size_t i = 0; i < n; ++i)
    {
        const int32_t id = static_cast<int32_t>(i);
        if (!linked(i))
        {
            living[i]     = id;
            out.parent[i] = coined_parent[i];
            continue;
        }
        const int32_t up    = living[static_cast<std::size_t>(coined_parent[i])];
        const bool    holds = i < holds_ground.size() && holds_ground[i] != 0u;
        out.parent[i] = up;
        if (holds)
        {
            living[i] = id;
            if (up != coined_parent[i]) ++out.reparented;
        }
        else
        {
            living[i]          = up;
            out.folded_into[i] = up;
            ++out.folded;
            if (has_child[i]) ++out.folded_interior;
        }
    }
    return out;
}
