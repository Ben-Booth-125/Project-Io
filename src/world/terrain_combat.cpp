#include "terrain_combat.hpp"

#include <algorithm>

namespace {

int clamp1000(int v) { return std::clamp(v, 0, 1000); }

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

// The material. Cover and going, not elevation — wetland is the standout,
// because marsh has stopped more armies than most mountains: it is impassable
// to weight without being high at all.
int defence_composition(terrain_composition c)
{
    switch (c)
    {
        case terrain_composition::wetland:   return 250;
        case terrain_composition::forest:    return 200;
        case terrain_composition::volcanic:  return 150;
        case terrain_composition::rocky:     return 100;
        case terrain_composition::icy:       return 100;
        case terrain_composition::tundra:    return  50;
        case terrain_composition::grassland: return   0; // open ground
        case terrain_composition::barren:    return   0; // costly, but not defensible
        case terrain_composition::regolith:  return   0;
        case terrain_composition::metallic:  return   0;
        case terrain_composition::ocean:     return   0; // mode change, not a value
    }
    return 0;
}

// What the ground denies an army. Composition dominates: this is forage, water
// and shelter, which is a question about material rather than shape.
int attrition_composition(terrain_composition c)
{
    switch (c)
    {
        case terrain_composition::regolith:  return 900;
        case terrain_composition::icy:       return 850;
        case terrain_composition::barren:    return 800;
        case terrain_composition::metallic:  return 800;
        case terrain_composition::volcanic:  return 700;
        case terrain_composition::tundra:    return 500;
        case terrain_composition::rocky:     return 400;
        case terrain_composition::wetland:   return 350; // disease and mire
        case terrain_composition::forest:    return 250;
        case terrain_composition::grassland: return 100; // forage and water
        case terrain_composition::ocean:     return   0;
    }
    return 0;
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

int terrain_defence(terrain_composition comp, terrain_landform lf)
{
    if (comp == terrain_composition::ocean)
        return 0;
    return clamp1000(defence_landform(lf) + defence_composition(comp));
}

int terrain_attrition(terrain_composition comp, terrain_landform lf)
{
    if (comp == terrain_composition::ocean)
        return 0;
    return clamp1000(attrition_composition(comp) + attrition_landform(lf));
}

int terrain_resistance(terrain_composition comp, terrain_landform lf)
{
    if (comp == terrain_composition::ocean)
        return 0;
    // 60/40 toward defence. Defensible ground shapes a border more strongly than
    // merely expensive ground — a frontier sits on a ridge, not on a moor — but
    // attrition alone must still be able to stop an army crossing an ice cap that
    // nobody is defending, which is why it is a substantial minority rather than
    // a garnish.
    return clamp1000((terrain_defence(comp, lf) * 6 + terrain_attrition(comp, lf) * 4) / 10);
}
