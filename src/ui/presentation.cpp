#include "presentation.hpp"

#include <cstddef>

namespace ui {

namespace {

// Indexed by static_cast<std::size_t>(resource_type); order matches the enum in
// components.hpp. Identity colours are chosen to read distinctly against the
// dark canvas and from one another.
constexpr resource_presentation resource_table[resource_count] = {
    // --- Tier 1: raw materials (Earth-sourced) ---
    { "Iron Ore",          "Fe",  IM_COL32(176, 120,  92, 255) }, // rust
    { "Coal",              "Coal",IM_COL32( 70,  72,  78, 255) }, // near-black grey
    { "Petroleum",         "Oil", IM_COL32( 96,  82, 110, 255) }, // dark violet-brown
    { "Silica",            "Si",  IM_COL32(196, 188, 150, 255) }, // pale sand
    { "Copper Ore",        "Cu",  IM_COL32(196, 118,  78, 255) }, // copper
    { "Rare Earth Ore",    "REE", IM_COL32(200, 170, 240, 255) }, // violet
    { "Agricultural Produce","Agr",IM_COL32(150, 196,  92, 255) }, // crop green
    // --- Tier 1: raw materials (space-sourced) ---
    { "Water",             "H2O", IM_COL32(120, 180, 230, 255) }, // water blue
    { "Iron-Nickel Ore",   "FeNi",IM_COL32(150, 140, 130, 255) }, // dull steel
    { "Platinum Group",    "PGM", IM_COL32(220, 224, 232, 255) }, // bright platinum
    { "Regolith",          "Reg", IM_COL32(140, 132, 120, 255) }, // grey-tan dust
    // --- Tier 1: ambient ---
    { "Stone",             "Stn", IM_COL32(150, 150, 150, 255) }, // neutral grey
    { "Timber",            "Tmb", IM_COL32(120, 160,  90, 255) }, // foliage green
    { "Sand",              "Snd", IM_COL32(214, 196, 140, 255) }, // light sand
    { "Clay",              "Cly", IM_COL32(178, 130, 100, 255) }, // earthy clay
    { "Peat",              "Pt",  IM_COL32(110,  86,  64, 255) }, // dark peat brown
    // --- Tier 2: refined goods (prototype subset) ---
    { "Steel",             "Stl", IM_COL32(150, 165, 185, 255) }, // blue-grey alloy
    { "Refined Fuel",      "Fuel",IM_COL32(210, 160,  80, 255) }, // amber fuel
    { "Food Rations",      "Food",IM_COL32(220, 180, 120, 255) }, // warm ration tan
};

// Reserved faction identity colours. Slot 0 is the player's corporation; the
// rest are placeholders for the rival factions the data model already permits.
constexpr ImU32 faction_table[palette::faction_slot_count] = {
    IM_COL32( 80, 150, 230, 255), // player — corporate blue
    IM_COL32(220, 110,  90, 255), // red
    IM_COL32(110, 200, 130, 255), // green
    IM_COL32(210, 180,  80, 255), // amber
    IM_COL32(170, 120, 210, 255), // violet
    IM_COL32( 90, 200, 205, 255), // teal
};

} // namespace

const resource_presentation& presentation_of(resource_type r)
{
    return resource_table[static_cast<std::size_t>(r)];
}

const char* resource_name(resource_type r)
{
    return presentation_of(r).name;
}

const char* composition_name(terrain_composition c)
{
    switch (c)
    {
        case terrain_composition::barren:    return "Barren";
        case terrain_composition::rocky:     return "Rocky";
        case terrain_composition::volcanic:  return "Volcanic";
        case terrain_composition::icy:       return "Icy";
        case terrain_composition::tundra:    return "Tundra";
        case terrain_composition::grassland: return "Grassland";
        case terrain_composition::forest:    return "Forest";
        case terrain_composition::wetland:   return "Wetland";
        case terrain_composition::ocean:     return "Ocean";
        case terrain_composition::regolith:  return "Regolith";
        case terrain_composition::metallic:  return "Metallic";
    }
    return "?";
}

const char* landform_name(terrain_landform l)
{
    switch (l)
    {
        case terrain_landform::plains:   return "Plains";
        case terrain_landform::highland: return "Highland";
        case terrain_landform::mountain: return "Mountain";
        case terrain_landform::canyon:   return "Canyon";
        case terrain_landform::valley:   return "Valley";
        case terrain_landform::crater:   return "Crater";
        case terrain_landform::rift:     return "Rift";
    }
    return "?";
}

namespace palette {

ImU32 faction_colour(int slot)
{
    const int s = ((slot % faction_slot_count) + faction_slot_count) % faction_slot_count;
    return faction_table[s];
}

} // namespace palette

ImU32 value_colour(fmt::sign s)
{
    switch (s)
    {
        case fmt::sign::positive: return palette::positive;
        case fmt::sign::negative: return palette::negative;
        case fmt::sign::zero:     return palette::neutral;
    }
    return palette::neutral;
}

ImU32 value_colour(double v)
{
    return value_colour(fmt::sign_of(v));
}

} // namespace ui
