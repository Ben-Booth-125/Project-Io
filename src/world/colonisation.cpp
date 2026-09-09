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

    // THE CHEAP ROUTES — the ones people actually followed. A river course
    // first where the caller can see one, the shoreline otherwise.
    if (river)          cost = (cost * 35) / 100;
    else if (shoreline) cost = (cost * 55) / 100;

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
                               s.tile, static_cast<int32_t>(si)});
    }

    const int64_t boundary_cy = static_cast<int64_t>(in.boundary_year) * 100;

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
        f.source_region[i] = src.region;
        f.culture[i]       = src.culture;
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
            front.push(front_entry{at, static_cast<int32_t>(ni), e.source});
        }
    }

    return f;
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
