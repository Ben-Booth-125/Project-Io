#include "presentation.hpp"

#include <cstddef>

namespace ui {

namespace {

// Indexed by static_cast<std::size_t>(resource_type); order matches the enum in
// components.hpp. Identity colours are chosen to read distinctly against the
// dark canvas and from one another.
constexpr resource_presentation resource_table[resource_count] = {
    { "Iron Ore",    "Fe",  IM_COL32(176, 120,  92, 255) }, // rust
    { "Ice",         "Ice", IM_COL32(170, 214, 230, 255) }, // pale cyan
    { "Silicates",   "Si",  IM_COL32(196, 188, 150, 255) }, // sand
    { "Rare Metals", "RM",  IM_COL32(200, 170, 240, 255) }, // violet
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
