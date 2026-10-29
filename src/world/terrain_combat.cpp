#include "terrain_combat.hpp"
#include "terrain_class.hpp"   // BL-1455: the class rows' lookup lives here, beside the shared table
#include "recipe_registry.hpp" // military_capability_params: the class rows and the march days
#include "unit_roster.hpp"

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
        case terrain_substrate::ocean:       // BL-516: mode change, not a value — and a
        case terrain_substrate::coast:       // lake or a shoreline is no more a place to
        case terrain_substrate::lake:        return   0; // stand and fight than open sea is.
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
        case terrain_substrate::ocean:       // BL-516: no army forages on water, of any kind.
        case terrain_substrate::coast:
        case terrain_substrate::lake:        return   0;
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
    if (is_water(sub)) // BL-516: every water kind, not just the open sea
        return 0;
    return clamp1000(defence_landform(lf) + defence_substrate(sub)
                     + defence_cover(cov, density));
}

int terrain_attrition(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                      terrain_landform lf)
{
    if (is_water(sub)) // BL-516: every water kind, not just the open sea
        return 0;
    return clamp1000(attrition_substrate(sub) - attrition_cover_relief(cov, density)
                     + attrition_landform(lf));
}

int terrain_resistance(terrain_substrate sub, terrain_cover cov, std::uint8_t density,
                       terrain_landform lf)
{
    if (is_water(sub)) // BL-516: every water kind, not just the open sea
        return 0;
    // 60/40 toward defence. Defensible ground shapes a border more strongly than
    // merely expensive ground — a frontier sits on a ridge, not on a moor — but
    // attrition alone must still be able to stop an army crossing an ice cap that
    // nobody is defending, which is why it is a substantial minority rather than
    // a garnish.
    return clamp1000((terrain_defence(sub, cov, density, lf) * 6
                      + terrain_attrition(sub, cov, density, lf) * 4) / 10);
}

// ---------------------------------------------------------------------------
// Terrain and the classes (BL-1455; MILITARY.md § Terrain and the classes)
// ---------------------------------------------------------------------------

int class_terrains_of(const tile_component& t, class_terrain out[2])
{
    if (is_water(t.substrate))
        return 0; // water is a mode, not a terrain row (§ Terrain defence and attrition)
    int n = 0;
    switch (t.landform)
    {
        case terrain_landform::mountain: out[n++] = class_terrain::mountain; break;
        case terrain_landform::canyon:   out[n++] = class_terrain::canyon;   break;
        case terrain_landform::rift:     out[n++] = class_terrain::rift;     break;
        default: break;
    }
    switch (t.cover)
    {
        case terrain_cover::marsh:  out[n++] = class_terrain::marsh;  break;
        case terrain_cover::dunes:  out[n++] = class_terrain::desert; break;
        case terrain_cover::forest: out[n++] = class_terrain::forest; break;
        case terrain_cover::urban:  out[n++] = class_terrain::urban;  break;
        case terrain_cover::snow:   out[n++] = class_terrain::snow;   break;
        default: break;
    }
    return n;
}

bool is_open_ground(const tile_component& t)
{
    if (is_water(t.substrate))
        return false;
    if (t.landform != terrain_landform::plains && t.landform != terrain_landform::highland)
        return false;
    switch (t.cover)
    {
        case terrain_cover::none:
        case terrain_cover::grass:
        case terrain_cover::salt:
        case terrain_cover::snow:
        case terrain_cover::dunes:
            return true;
        default:
            return false; // scrub, forest, marsh, ash and urban conceal
    }
}

terrain_class_effect terrain_class_effects(const military_capability_params& mil, land_class c,
                                           const tile_component& t)
{
    terrain_class_effect e;
    const int ci = static_cast<int>(c);
    if (c == k_land_class_from_cls || c == land_class::none || ci < 0 || ci >= land_class_count)
        return e; // a pre-modern row, or not a land fighter: no class rows

    const terrain_class_params& p = mil.terrain;
    const auto& class_rows = p.rows[static_cast<std::size_t>(ci)];

    // THE DISMOUNT, as a move multiplier: foot infantry's days over the class's own,
    // so a carrier on high ground marches at foot pace and no slower.
    const float own  = mil.march_days_per_land_class[static_cast<std::size_t>(ci)];
    const float foot = mil.march_days_per_land_class[static_cast<std::size_t>(land_class::infantry)];
    const float dismount_move = (own > 0.0f && foot > 0.0f) ? std::max(1.0f, foot / own) : 1.0f;

    bool  any_stealth = false;
    float stealth     = 1.0f;
    class_terrain keys[2];
    const int n = class_terrains_of(t, keys);
    for (int i = 0; i < n; ++i) // THE WORST ROW, per effect - never a product of rows
    {
        const terrain_class_row& r = class_rows[static_cast<std::size_t>(keys[i])];
        e.move = std::max(e.move, r.move);
        if (r.dismount)
        {
            e.dismounted = true;
            e.move       = std::max(e.move, dismount_move);
        }
        e.blocked_unless_roaded = e.blocked_unless_roaded || r.blocked_unless_roaded;
        e.fight = std::min(e.fight, r.fight);
        e.wear  = std::max(e.wear, r.wear);
        if (r.has_stealth) // only a row that authors stealth votes on it
        {
            stealth     = any_stealth ? std::min(stealth, r.stealth) : r.stealth;
            any_stealth = true;
        }
    }
    // OPEN GROUND EXPOSES EVERYONE (Ben, 2026-10-29): one more stealth vote, every class.
    if (is_open_ground(t))
    {
        stealth     = any_stealth ? std::min(stealth, p.open_ground_stealth) : p.open_ground_stealth;
        any_stealth = true;
    }
    e.stealth = any_stealth ? stealth : 1.0f;

    const bool road = tile_has_road(t);
    e.walled = e.blocked_unless_roaded && !road;
    // A ROAD ON THE DEFENDED TILE HALVES THE FIGHT DEBUFF: scale' = 1 - (1 - scale) / 2.
    e.battle_fight = road ? 1.0f - (1.0f - e.fight) * 0.5f : e.fight;
    return e;
}

terrain_class_effect terrain_class_effects_for_type(const military_capability_params& mil,
                                                    std::uint16_t type, const tile_component& t)
{
    const auto& table = unit_roster_table();
    if (type >= table.size())
        return terrain_class_effect{};
    const roster_row& r = table[type];
    if (r.land == k_land_class_from_cls)
        return terrain_class_effect{}; // pre-modern: no land class, so no class rows
    return terrain_class_effects(mil, r.land, t);
}

int terrain_fight_permille(const terrain_class_effect& e)
{
    return std::clamp(static_cast<int>(std::lround(static_cast<double>(e.battle_fight) * 1000.0)),
                      0, 1000);
}
