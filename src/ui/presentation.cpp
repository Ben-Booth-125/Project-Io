#include "presentation.hpp"

#include <cstddef>

namespace ui {

namespace {

// Indexed by static_cast<std::size_t>(resource_type); order matches the enum in
// components.hpp. Identity colours are chosen to read distinctly against the
// dark canvas and from one another.
//
// Re-picked 2026-07-08 (BL-016) against the Okabe-Ito colourblind-
// safe set. The prior Iron Ore / Copper Ore rust-browns sat too close in hue to
// Agricultural Produce / Timber's crop-greens under deuteranopia/protanopia
// simulation (the same red-on-green confusion the Resource lens's deposit fill
// inherits from this table) — the ore hues are shifted toward Okabe-Ito
// vermillion/orange and the crop/foliage hues toward its bluish-green, so the two
// families separate on hue, not just on the raw values a trichromat would read.
constexpr resource_presentation resource_table[resource_count] = {
    // --- Tier 1: raw materials (Earth-sourced) ---
    { "Iron Ore",          "Fe",  IM_COL32(196, 110,  60, 255) }, // vermillion rust
    { "Coal",              "Coal",IM_COL32( 70,  72,  78, 255) }, // near-black grey
    { "Petroleum",         "Oil", IM_COL32( 96,  82, 110, 255) }, // dark violet-brown
    { "Silica",            "Si",  IM_COL32(196, 188, 150, 255) }, // pale sand
    { "Copper Ore",        "Cu",  IM_COL32(214, 140,  60, 255) }, // Okabe-Ito orange
    { "Rare Earth Ore",    "REE", IM_COL32(200, 170, 240, 255) }, // violet
    { "Agricultural Produce","Agr",IM_COL32(110, 190, 130, 255) }, // bluish-green crop
    // --- Tier 1: raw materials (space-sourced) ---
    { "Water",             "H2O", IM_COL32(120, 180, 230, 255) }, // water blue
    { "Iron-Nickel Ore",   "FeNi",IM_COL32(150, 140, 130, 255) }, // dull steel
    { "Platinum Group",    "PGM", IM_COL32(220, 224, 232, 255) }, // bright platinum
    { "Regolith",          "Reg", IM_COL32(140, 132, 120, 255) }, // grey-tan dust
    // --- Tier 1: ambient ---
    { "Stone",             "Stn", IM_COL32(150, 150, 150, 255) }, // neutral grey
    { "Timber",            "Tmb", IM_COL32( 90, 170, 120, 255) }, // bluish-green foliage
    { "Sand",              "Snd", IM_COL32(214, 196, 140, 255) }, // light sand
    { "Clay",              "Cly", IM_COL32(178, 130, 100, 255) }, // earthy clay
    { "Peat",              "Pt",  IM_COL32(110,  86,  64, 255) }, // dark peat brown
    // --- Tier 1: endemic trade goods (mercantile track) ---
    // Warmer, more saturated hues than the industrial raws, so a market strip
    // reads at a glance as "this is the cargo people get rich on".
    { "Tobacco",           "Tob", IM_COL32(176, 138,  74, 255) }, // cured leaf gold-brown
    { "Spices",            "Spc", IM_COL32(206, 106,  52, 255) }, // paprika orange-red
    { "Coffee",            "Cof", IM_COL32(120,  74,  52, 255) }, // roasted bean brown
    { "Furs",              "Fur", IM_COL32(198, 178, 152, 255) }, // pale pelt cream
    // --- Tier 2: refined goods (prototype subset) ---
    { "Steel",             "Stl", IM_COL32(150, 165, 185, 255) }, // blue-grey alloy
    { "Refined Fuel",      "Fuel",IM_COL32(210, 160,  80, 255) }, // amber fuel
    { "Food Rations",      "Food",IM_COL32(220, 180, 120, 255) }, // warm ration tan
};

// Reserved corporation identity colours. Slot 0 is the player's corporation; the
// rest are rival slots. Re-picked 2026-07-08 (BL-016) from the Okabe-Ito
// colourblind-safe palette so player/rival and rival/rival pairs stay
// distinguishable under deuteranopia/protanopia — the prior red/green pair
// (slots 1/2) was exactly the confusion set the accessibility review flagged.
constexpr ImU32 corp_table[palette::corp_slot_count] = {
    IM_COL32(  0, 114, 178, 255), // player — Okabe-Ito blue
    IM_COL32(213,  94,   0, 255), // vermillion
    IM_COL32(  0, 158, 115, 255), // bluish green
    IM_COL32(240, 228,  66, 255), // yellow
    IM_COL32(204, 121, 167, 255), // reddish purple
    IM_COL32( 86, 180, 233, 255), // sky blue
};

} // namespace

const resource_presentation& presentation_of(resource_type r)
{
    // resource_table is declared [resource_count], so widening resource_type
    // silently appends ZERO-FILLED rows — a NULL name, not a missing row the
    // compiler would complain about. Every caller hands the result straight to
    // a "%s", so an unauthored resource is a null dereference rather than a
    // visible gap. BL-286 added eight logistics goods whose presentation is not
    // authored yet (BL-287-290 give them behaviour, and names/colours with it);
    // today they stay unreached only because nothing produces them and the
    // callers happen to guard on a positive quantity.
    static constexpr resource_presentation unauthored{
        "(unnamed resource)", "?", IM_COL32(140, 140, 140, 255) };
    const auto i = static_cast<std::size_t>(r);
    if (i >= resource_count || resource_table[i].name == nullptr)
        return unauthored;
    return resource_table[i];
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

const char* body_type_name(body_type t)
{
    switch (t)
    {
        case body_type::planet:   return "Planet";
        case body_type::moon:     return "Moon";
        case body_type::asteroid: return "Asteroid";
        case body_type::station:  return "Station";
        case body_type::star:     return "Star";
    }
    return "?";
}

const char* building_type_name(building_type t)
{
    switch (t)
    {
        case building_type::none:                 return "None";
        case building_type::extraction_site:      return "Extraction Site";
        case building_type::processing_facility:  return "Processing Facility";
        case building_type::port:                 return "Port";
        case building_type::launchpad:            return "Launchpad";
        case building_type::inland_logistics_hub: return "Inland Logistics Hub"; // BL-149
    }
    return "None";
}

namespace palette {

ImU32 corp_colour(int slot)
{
    const int s = ((slot % corp_slot_count) + corp_slot_count) % corp_slot_count;
    return corp_table[s];
}

int corp_emblem_shape(entity_id corp)
{
    return static_cast<int>(
        (static_cast<uint32_t>(corp) * 2654435761u) % corp_emblem_shape_count);
}

ImU32 corp_identity_colour(entity_id corp, entity_id player)
{
    if (corp == player)
        return corp_colour(0);
    int slot = static_cast<int>(
        (static_cast<uint32_t>(corp) * 2654435761u) % corp_slot_count);
    if (slot == 0)
        slot = 1;
    return corp_colour(slot);
}

ImU32 nation_colour(entity_id id)
{
    // Re-picked 2026-07-08 (BL-016): the prior 12-hue even wheel stepped straight
    // through the deutan/protan confusion band (red -> orange -> yellow -> lime
    // -> green reads as one indistinguishable smear under either deficiency —
    // the Country lens's worst-cited hazard). Twelve slots now extend the eight
    // Okabe-Ito colourblind-safe hues with four lightness-shifted variants of the
    // same safe hues (rather than new hues), since varying lightness within a
    // safe hue family is the standard way to extend a small CVD-safe palette.
    static constexpr ImU32 nation_table[nation_slot_count] = {
        IM_COL32(213,  94,   0, 255), // vermillion
        IM_COL32(230, 159,   0, 255), // orange
        IM_COL32(240, 228,  66, 255), // yellow
        IM_COL32(  0, 158, 115, 255), // bluish green
        IM_COL32( 86, 180, 233, 255), // sky blue
        IM_COL32(  0, 114, 178, 255), // blue
        IM_COL32(204, 121, 167, 255), // reddish purple
        IM_COL32(150, 150, 150, 255), // neutral grey (stands in for Okabe-Ito black — pure black would vanish on the dark canvas)
        IM_COL32(140,  60,   0, 255), // vermillion, shaded
        IM_COL32(150, 210, 180, 255), // bluish green, tinted
        IM_COL32(150, 180, 220, 255), // blue, tinted
        IM_COL32(150,  80, 110, 255), // reddish purple, shaded
    };
    // Knuth multiplicative hash so consecutive nation ids (the common case) land
    // on well-separated palette slots rather than adjacent hues.
    const uint32_t h = static_cast<uint32_t>(id) * 2654435761u;
    return nation_table[h % nation_slot_count];
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
