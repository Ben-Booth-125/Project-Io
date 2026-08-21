#include "terrain_combat.hpp"

#include <algorithm>
#include <cmath>

namespace {

int clamp1000(int v) { return std::clamp(v, 0, 1000); }

/// Scale an authored cover value by its density: 0.55x at the sparsest to 1.0x at
/// full, so a cover barely there barely counts and the authored number is the
/// value at full canopy. ROUNDED, not truncated — the calibration table in
/// terrain_defence reproduces the pre-split numbers exactly, and truncation lost
/// three of them to an off-by-one.
int scale_by_density(int base, std::uint8_t density)
{
    if (base == 0)
        return 0;
    return static_cast<int>(std::lround(
        static_cast<double>(base) * (0.55 + 0.45 * static_cast<double>(cover_fraction(density)))));
}

// The shape of the ground. Ordered as topography and defensibility together:
// a peak is the strongest position, a rift or gorge is a line an attacker must
// force, a plateau grants the high ground, and flat land grants nothing.
int defence_landform(terrain_landform lf)
{
    switch (lf)
    {
        case terrain_landform::mountain: return 1000;
        case terrain_landform::rift:     return  700;
        case terrain_landform::canyon:   return  650;
        case terrain_landform::highland: return  400;
        case terrain_landform::crater:   return  300; // a rim helps; a basin does not
        case terrain_landform::valley:   return  150;
        case terrain_landform::plains:   return    0;
    }
    return 0;
}

// COVER AND GOING — and this function is the clearest case BL-519 made for the
// split. Its own pre-split comment said "Cover and going, not elevation", which
// is a description of the COVER axis; it was reading the composition slot only
// because cover had nowhere else to live. Marsh is the standout, because a bog
// has stopped more armies than most mountains: it is impassable to weight
// without being high at all.
//
// Graded by density (Ben, 2026-08-21), so a thin wood is worth less than a
// closed one — which the binary composition could not say.
int defence_cover(terrain_cover c, std::uint8_t density)
{
    // CALIBRATED, NOT INVENTED. Each biotic row is set so that a cover at the
    // density Pass 4c gives it reproduces the pre-split number exactly — see the
    // table in terrain_defence. That is what makes this a refactor with a new
    // axis on top rather than a silent re-tuning of combat.
    int base = 0;
    switch (c)
    {
        case terrain_cover::marsh:  base = 294; break; // -> 250 at density 170 (old wetland)
        case terrain_cover::forest: base = 219; break; // -> 200 at density 205 (old forest)
        case terrain_cover::scrub:  base =  73; break; // ->  50 at density  75 (old tundra)
        case terrain_cover::grass:  base =   0; break; // ->   0 (old grassland: open ground)
        case terrain_cover::urban:  base = 220; break; // NEW: built ground is fought for street by street
        case terrain_cover::dunes:  base =  60; break; // NEW: soft going, no sightline
        case terrain_cover::snow:   base =  40; break; // NEW
        case terrain_cover::ash:    base =  20; break; // NEW
        case terrain_cover::salt:   base =   0; break;
        case terrain_cover::none:   base =   0; break;
    }
    return scale_by_density(base, density);
}

// The ground itself. Small next to cover and landform, and that is the honest
// reading: rock and ice are awkward to cross, but what a defender actually uses
// is the shape of the ground and what is growing on it.
int defence_substrate(terrain_substrate s)
{
    switch (s)
    {
        case terrain_substrate::volcanic:    return 150; // broken lava field
        case terrain_substrate::rocky:       return 100;
        case terrain_substrate::icy:         return 100;
        case terrain_substrate::barren:      return   0; // costly, but not defensible
        case terrain_substrate::sedimentary: return   0;
        case terrain_substrate::regolith:    return   0;
        case terrain_substrate::metallic:    return   0;
        case terrain_substrate::ocean:       return   0; // mode change, not a value
    }
    return 0;
}

// What the GROUND denies an army: forage, water and shelter. This is the half of
// the old attrition table that really was about material — regolith, ice, bare
// rock and metal have nothing to give, and no cover changes that fact about the
// ground.
int attrition_substrate(terrain_substrate s)
{
    switch (s)
    {
        case terrain_substrate::regolith:    return 900;
        case terrain_substrate::icy:         return 850;
        case terrain_substrate::barren:      return 800;
        case terrain_substrate::metallic:    return 800;
        case terrain_substrate::volcanic:    return 700;
        // BARE SOIL, and it is meant to read as harsh. Sedimentary ground with
        // nothing growing on it is a dust bowl, not a meadow — the meadow is the
        // GRASS, and it is the cover's relief below that turns this 600 into the
        // old grassland's 100. Pass 4c never produces a bare sedimentary tile
        // (the biome table always dresses it), so this is the value a harness
        // fixture sees rather than one the map does.
        case terrain_substrate::sedimentary: return 600;
        case terrain_substrate::rocky:       return 400;
        case terrain_substrate::ocean:       return   0;
    }
    return 0;
}

// What the COVER gives BACK — relief subtracted from the substrate's cost,
// because vegetation is forage, fuel and shelter. Splitting the old single table
// this way is what lets a forested rocky upland read as easier than a bare one,
// which the pre-split model could not express at all: `forest` and `rocky` were
// alternatives, not a pair.
//
// Calibrated against a bare-sedimentary base of 600, so each old biotic
// composition lands on its old number at its own density (table in
// terrain_attrition). Grass reliefs hardest, and that ordering is deliberate: the
// old table put grassland at 100 and forest at 250, i.e. open grazing country is
// the best marching ground there is, and woodland — for all its fuel and shelter
// — is slower.
//
// A NEGATIVE relief is a cover that COSTS. Snow, dunes, ash and salt make the
// ground underneath worse rather than better, which is the other thing a single
// composition slot could never say.
int attrition_cover_relief(terrain_cover c, std::uint8_t density)
{
    int base = 0;
    switch (c)
    {
        case terrain_cover::grass:  base =  614; break; // -> 100 on sedimentary at d150 (old grassland)
        case terrain_cover::forest: base =  384; break; // -> 250 on sedimentary at d205 (old forest)
        case terrain_cover::marsh:  base =  294; break; // -> 350 on sedimentary at d170 (old wetland)
        case terrain_cover::scrub:  base =  147; break; // -> 500 on sedimentary at d 75 (old tundra)
        case terrain_cover::urban:  base =  400; break; // NEW: an army in a town is a supplied army
        case terrain_cover::snow:   base =  -80; break; // NEW: a cover that costs
        case terrain_cover::dunes:  base =  -60; break; // NEW
        case terrain_cover::ash:    base =  -60; break; // NEW
        case terrain_cover::salt:   base =  -40; break; // NEW
        case terrain_cover::none:   base =    0; break;
    }
    return scale_by_density(base, density);
}

// The climb, added on top of the material cost.
int attrition_landform(terrain_landform lf)
{
    switch (lf)
    {
        case terrain_landform::mountain: return 200;
        case terrain_landform::rift:     return 120;
        case terrain_landform::canyon:   return 100;
        case terrain_landform::crater:   return  80;
        case terrain_landform::highland: return  60;
        case terrain_landform::valley:   return   0;
        case terrain_landform::plains:   return   0;
    }
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
// The public three, on the split axes (BL-519)
// ---------------------------------------------------------------------------
// EVERY PRE-SPLIT VALUE IS REPRODUCED. Each old composition maps to one
// (substrate, cover) pair at a known density, and the tables above are
// calibrated so the sum lands on the old number:
//
//   old composition   substrate     cover   density   defence   attrition
//   ---------------   -----------   -----   -------   -------   ---------
//   grassland         sedimentary   grass     150         0         100
//   forest            sedimentary   forest    205       200         250
//   wetland           sedimentary   marsh     170       250         350
//   tundra            sedimentary   scrub      75        50         500
//   rocky             rocky         none        0       100         400
//   barren            barren        none        0         0         800
//   volcanic          volcanic      none        0       150         700
//   icy               icy           none        0       100         850
//   regolith          regolith      none        0         0         900
//   metallic          metallic      none        0         0         800
//   ocean             ocean         none        0         0           0
//
// What is NEW is every pair the old model could not name: a forested rocky
// mountain now defends like woodland on rock (100 + 200) and forages like
// woodland on rock (400 - 250), where before it had to be one or the other.

int terrain_defence(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                    terrain_landform lf)
{
    if (sub == terrain_substrate::ocean)
        return 0;
    return clamp1000(defence_landform(lf) + defence_substrate(sub)
                     + defence_cover(cov, density));
}

int terrain_attrition(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                      terrain_landform lf)
{
    if (sub == terrain_substrate::ocean)
        return 0;
    return clamp1000(attrition_substrate(sub) - attrition_cover_relief(cov, density)
                     + attrition_landform(lf));
}

int terrain_resistance(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                       terrain_landform lf)
{
    if (sub == terrain_substrate::ocean)
        return 0;
    // 60/40 toward defence. Defensible ground shapes a border more strongly than
    // merely expensive ground — a frontier sits on a ridge, not on a moor — but
    // attrition alone must still be able to stop an army crossing an ice cap that
    // nobody is defending, which is why it is a substantial minority rather than
    // a garnish.
    return clamp1000((terrain_defence(sub, cov, density, lf) * 6
                      + terrain_attrition(sub, cov, density, lf) * 4) / 10);
}
