#include "presentation.hpp"

#include <string>

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
    // --- Ancient intermediates (BL-286/BL-429). STALE COMMENT REMOVED
    // (BL-591, 2026-08-24): these three were left as explicit nulls with a
    // "still unauthored" note that predates BL-429 (2026-08-15) giving all
    // three real producers and consumers — they have been live, produced
    // goods for over a week with no display name, the exact "(unnamed
    // resource)" defect this file's own comment on Ordnance already
    // documents happening once before. Found while authoring the depth
    // readout (BL-591), which needs to NAME the good that set a corp's
    // reached depth — charcoal is the ancient chain's very first output.
    { "Charcoal",           "Chr", IM_COL32( 90,  68,  56, 255) }, // dark burnt-wood brown
    { "Iron Blooms",        "FeBl",IM_COL32(172, 122,  96, 255) }, // rough smelted rust, between Iron Ore and Steel
    { "Trade Goods",        "Trd", IM_COL32(200, 172, 112, 255) }, // modest warm gold — a luxury, not a treasure
    // --- Propellant (BL-308). Authored, unlike the eight above: a corp holds it
    // in its (corp, body) pool and the Launchpad burns it, so it reaches the
    // stockpile strips the moment a Chemical Plant runs.
    { "Propellant",        "Prop",IM_COL32(180, 220, 235, 255) }, // cryogenic pale blue
    // --- Tier 2/3: processing chain roster (BL-340). Margin widens up the
    // tiers, so hue saturates and warms from silicon through spacecraft
    // components — the value gradient reads on the strip, not just in price.
    { "Silicon",            "Si+", IM_COL32(210, 200, 170, 255) }, // pale refined sand
    { "Refined Copper",     "Cu+", IM_COL32(224, 150,  80, 255) }, // brighter Okabe-Ito orange
    { "REE Alloy",          "REE+",IM_COL32(210, 180, 245, 255) }, // brighter violet
    { "Machinery",          "Mch", IM_COL32(170, 175, 190, 255) }, // cool machined steel
    { "Alloys",             "Aly", IM_COL32(190, 160, 200, 255) }, // steel-violet blend
    { "Electronics",        "Elc", IM_COL32(130, 200, 210, 255) }, // circuit teal
    { "Spacecraft Components","S/C",IM_COL32(235, 210, 100, 255) }, // high-value gold
    // --- Habitability tranche (BL-368, 2026-08-11) — authored 2026-08-17 by
    // BL-457. These three shipped in August with no presentation row: the array
    // is declared [resource_count] but only 34 of its 37 initialisers were
    // given, so C++ value-initialised the rest to nulls and presentation_of()
    // returned "(unnamed resource)" for all three. Not a crash — the fallback
    // did its job — but a population centre's own demand basket rendered
    // nameless. Found while appending ordnance, because a positional array
    // cannot gain index 37 without filling 34-36 first.
    //
    // Cooler and paler than the industrial tier: welfare goods, not
    // high-margin products, which is the same argument that priced them
    // modestly above their inputs in world_gen.lua.
    { "Clean Water",        "H2O+",IM_COL32(165, 215, 240, 255) }, // treated, paler than raw water
    { "Consumer Goods",     "Cns", IM_COL32(205, 172, 162, 255) }, // soft domestic rose-tan
    { "Medical Supplies",   "Med", IM_COL32(212, 232, 220, 255) }, // clinical pale green-white
    // --- The military terminal good (BL-457, 2026-08-17). Oxblood: the one
    // saturated red-crimson in the table, deliberately NOT reusing the
    // orange-browns the raws occupy (iron ore, spices, copper) so a strip
    // carrying ordnance reads as carrying something else entirely. It is a
    // terminal good like spacecraft components, and the pair should be
    // distinguishable at a glance — gold for the space road, oxblood for the
    // military one.
    { "Ordnance",           "Ord", IM_COL32(178,  66,  78, 255) }, // oxblood — terminal military good
    // --- Ancient roster, slice 1 (BL-585/BL-586, 2026-08-24) — authored WITH
    // the append this time, not left null: the depth readout (BL-591) needs
    // to name whichever of these a corp reaches next.
    { "Ceramics",           "Cer", IM_COL32(188, 128,  92, 255) }, // fired terracotta, warmer than raw Clay
    { "Dressed Stone",      "DStn",IM_COL32(180, 178, 170, 255) }, // paler, worked grey against raw Stone
    { "Planks",             "Plk", IM_COL32(196, 158, 104, 255) }, // milled wood tan, against Timber's foliage green
    { "Tools",              "Tool",IM_COL32(158, 148, 128, 255) }, // worked bronze-grey, cooler than Iron Blooms
    // --- Ancient roster, slice 2 (BL-586, 2026-08-24) — Tannery/Weaver/
    // Shipwright, authored WITH the append, same discipline as slice 1. Hides
    // and fibre sit with the endemic/ambient raws in hue register (pelt cream,
    // crop green); leather/cloth/rigging warm and saturate with depth, the
    // same value-gradient argument the Tier 2/3 processing roster above uses.
    { "Hides",              "Hid", IM_COL32(168, 118,  90, 255) }, // raw pelt hide, between Furs' cream and Clay's earth
    { "Fibre",              "Fib", IM_COL32(150, 196, 108, 255) }, // pale crop green, beside Agricultural Produce
    { "Leather",            "Lthr",IM_COL32(120,  78,  52, 255) }, // tanned, darker and warmer than raw Hides
    { "Cloth",              "Cth", IM_COL32(210, 200, 160, 255) }, // woven undyed fibre, paler than raw Fibre
    { "Rigging",            "Rig", IM_COL32(176, 140,  72, 255) }, // tarred cordage gold-brown, the roster's deepest good
    // The two goods that are SERVICES rather than substances, and the only rows
    // here whose colour is not a material's. Power is BL-708's; it was left
    // unauthored and so rendered as "(unnamed resource)" through the fallback
    // below — authored here with BL-709's own row rather than left for a good
    // the player now buys every tick to keep reading as a gap.
    { "Power",              "Pwr", IM_COL32(240, 208,  80, 255) }, // live-wire yellow, no material shares it
    { "Construction Capacity", "Cap", IM_COL32(232, 148,  40, 255) }, // hazard orange — crews and plant, the sector's throughput
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

const char* substrate_name(terrain_substrate s)
{
    switch (s)
    {
        case terrain_substrate::barren:      return "Barrens";
        case terrain_substrate::rocky:       return "Rock";
        case terrain_substrate::sedimentary: return "Soil";
        case terrain_substrate::volcanic:    return "Volcanic";
        case terrain_substrate::metallic:    return "Metallic";
        case terrain_substrate::regolith:    return "Regolith";
        case terrain_substrate::icy:         return "Ice";
        case terrain_substrate::ocean:       return "Ocean";
        case terrain_substrate::lake:        return "Lake";  // BL-516
        case terrain_substrate::coast:       return "Coast";
    }
    return "?";
}

const char* cover_name(terrain_cover c)
{
    switch (c)
    {
        case terrain_cover::none:   return "None";
        case terrain_cover::grass:  return "Grass";
        case terrain_cover::scrub:  return "Scrub";
        case terrain_cover::forest: return "Forest";
        case terrain_cover::marsh:  return "Marsh";
        case terrain_cover::snow:   return "Snow";
        case terrain_cover::dunes:  return "Dunes";
        case terrain_cover::ash:    return "Ash";
        case terrain_cover::salt:   return "Salt";
        case terrain_cover::urban:  return "Urban";
    }
    return "?";
}

const char* cover_density_word(std::uint8_t density)
{
    if (density == 0)  return nullptr;
    if (density < 100) return "Sparse";
    if (density < 190) return "Moderate";
    return "Dense";
}

namespace {

/// The cover as an ADJECTIVE, for composing with a substrate noun.
const char* cover_adjective(terrain_cover c)
{
    switch (c)
    {
        case terrain_cover::grass:  return "Grassy";
        case terrain_cover::scrub:  return "Scrubby";
        case terrain_cover::forest: return "Forested";
        case terrain_cover::marsh:  return "Marshy";
        case terrain_cover::snow:   return "Snowy";
        case terrain_cover::dunes:  return "Dune";
        case terrain_cover::ash:    return "Ashen";
        case terrain_cover::salt:   return "Salt-crusted";
        case terrain_cover::urban:  return "Urban";
        case terrain_cover::none:   break;
    }
    return nullptr;
}

} // namespace

std::string terrain_name(terrain_substrate sub, terrain_cover cov, std::uint8_t density)
{
    (void)density; // the density WORD is a separate line; see cover_density_word

    // Water and paving are answers on their own — neither wants a qualifier, and
    // urban deliberately says nothing about the ground it stands on even though
    // (since BL-519) the ground is still there underneath.
    // BL-516: each water kind names itself. They are three answers, not one with
    // a qualifier — a lake is not a kind of ocean.
    if (sub == terrain_substrate::ocean) return "Ocean";
    if (sub == terrain_substrate::coast) return "Coast";
    if (sub == terrain_substrate::lake)  return "Lake";
    if (cov == terrain_cover::urban)     return "Urban";

    // THE FAMILIAR NAMES. Every one of these is the exact word the pre-split
    // model showed for the composition this pair replaces, so the split is
    // invisible to a player looking at ground they already knew.
    if (sub == terrain_substrate::sedimentary)
    {
        switch (cov)
        {
            case terrain_cover::grass:  return "Grassland";
            case terrain_cover::forest: return "Forest";
            case terrain_cover::marsh:  return "Wetland";
            case terrain_cover::scrub:  return "Scrubland"; // was "Tundra"
            case terrain_cover::none:   return "Bare Soil";
            default: break;
        }
    }

    if (cov == terrain_cover::none)
        return substrate_name(sub);

    const char* adj = cover_adjective(cov);
    if (adj == nullptr)
        return substrate_name(sub);
    return std::string(adj) + " " + substrate_name(sub);
}

std::string terrain_name(const tile_component& t)
{
    return terrain_name(t.substrate, t.cover, t.cover_density);
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
        case building_type::military_base:        return "Military Base";        // BL-325 S1
        case building_type::research_institute:   return "Research Institute";   // BL-332
        case building_type::schooling:            return "Schooling";            // BL-615
        case building_type::university:           return "University";           // BL-615
    }
    return "None";
}

const char* extraction_building_name(resource_type r)
{
    switch (r)
    {
        case resource_type::iron_ore:       return "Iron Mine";
        case resource_type::coal:           return "Coal Mine";
        case resource_type::petroleum:      return "Oil Field";
        case resource_type::silica:         return "Silica Quarry";
        case resource_type::copper_ore:     return "Copper Mine";
        case resource_type::rare_earth_ore: return "Rare-Earth Mine";
        case resource_type::water:          return "Water Extractor";
        case resource_type::stone:          return "Quarry";
        case resource_type::timber:         return "Woodcutter's Camp";
        case resource_type::sand:           return "Sand Pit";
        case resource_type::clay:           return "Clay Pit";
        case resource_type::peat:           return "Peat Cutting";
        default:                            return resource_name(r);
    }
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

ImU32 building_kind_colour(building_type type)
{
    // Hue-separated by hand, not hashed: building_type is a closed enumeration of
    // eight, and the one place these colours are read (a stacked tile's ring) is
    // exactly where two adjacent hues would be indistinguishable. Each hue also
    // carries a mnemonic rather than being arbitrary — ochre for what comes out of
    // the ground, steel blue for the works that refines it, teal for the water-side
    // port, violet for the launch to orbit, lime for the road node, crimson for the
    // garrison, magenta for the institute.
    switch (type)
    {
        case building_type::extraction_site:      return IM_COL32(222, 168,  82, 255); // ochre
        case building_type::processing_facility:  return IM_COL32(108, 160, 220, 255); // steel blue
        case building_type::port:                 return IM_COL32( 96, 206, 198, 255); // teal
        case building_type::launchpad:            return IM_COL32(168, 132, 236, 255); // violet
        case building_type::inland_logistics_hub: return IM_COL32(176, 206, 104, 255); // lime
        case building_type::military_base:        return IM_COL32(222,  96,  92, 255); // crimson
        case building_type::research_institute:   return IM_COL32(232, 124, 196, 255); // magenta
        case building_type::schooling:            return IM_COL32(240, 200, 110, 255); // amber — BL-615
        case building_type::university:           return IM_COL32(150, 170, 244, 255); // periwinkle — BL-615
        case building_type::none:                 break;
    }
    return neutral;
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
