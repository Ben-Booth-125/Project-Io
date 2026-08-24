#include "resource_names.hpp"

#include <unordered_map>

namespace resource_names {

resource_type resource_from_name(const std::string& name, bool& ok)
{
    static const std::unordered_map<std::string, resource_type> table = {
        { "iron_ore",              resource_type::iron_ore },
        { "coal",                  resource_type::coal },
        { "petroleum",             resource_type::petroleum },
        { "silica",                resource_type::silica },
        { "copper_ore",            resource_type::copper_ore },
        { "rare_earth_ore",        resource_type::rare_earth_ore },
        { "agricultural_produce",  resource_type::agricultural_produce },
        { "water",                 resource_type::water },
        { "iron_nickel_ore",       resource_type::iron_nickel_ore },
        { "platinum_group_metals", resource_type::platinum_group_metals },
        { "regolith",              resource_type::regolith },
        { "stone",                 resource_type::stone },
        { "timber",                resource_type::timber },
        { "sand",                  resource_type::sand },
        { "clay",                  resource_type::clay },
        { "peat",                  resource_type::peat },
        { "tobacco",               resource_type::tobacco },
        { "spices",                resource_type::spices },
        { "coffee",                resource_type::coffee },
        { "furs",                  resource_type::furs },
        // BL-286 logistics/endemic goods.
        { "charcoal",              resource_type::charcoal },
        { "iron_blooms",           resource_type::iron_blooms },
        { "trade_goods_misc",      resource_type::trade_goods_misc },
        { "steel",                 resource_type::steel },
        { "refined_fuel",          resource_type::refined_fuel },
        { "food_rations",          resource_type::food_rations },
        { "propellant",            resource_type::propellant },
        { "silicon",               resource_type::silicon },
        { "refined_copper",        resource_type::refined_copper },
        { "ree_alloy",             resource_type::ree_alloy },
        { "machinery",             resource_type::machinery },
        { "alloys",                resource_type::alloys },
        { "electronics",           resource_type::electronics },
        { "spacecraft_components", resource_type::spacecraft_components },
        { "clean_water",           resource_type::clean_water },
        { "consumer_goods",        resource_type::consumer_goods },
        { "medical_supplies",      resource_type::medical_supplies },
        { "ordnance",              resource_type::ordnance }, // BL-457
        // BL-585/BL-586 (2026-08-24) — ancient roster slice 1. NR-237's lesson
        // applied on purpose: this table is the ONE mapping recipes.lua AND
        // world_gen.lua's base_price both load through (recipe_registry.cpp,
        // world_gen_config.cpp), so a name missing here throws at load in
        // both files, not silently — add every new resource_type HERE the
        // same change it is appended to the enum, never after.
        { "ceramics",              resource_type::ceramics },
        { "dressed_stone",         resource_type::dressed_stone },
        { "planks",                resource_type::planks },
        { "tools",                 resource_type::tools },
        // BL-586 slice 2 (2026-08-24) — Tannery/Weaver/Shipwright. Same rule
        // as the block above: every new resource_type lands HERE the same
        // change it is appended to the enum.
        { "hides",                 resource_type::hides },
        { "fibre",                 resource_type::fibre },
        { "leather",               resource_type::leather },
        { "cloth",                 resource_type::cloth },
        { "rigging",               resource_type::rigging },
    };

    const auto it = table.find(name);
    ok = (it != table.end());
    return ok ? it->second : resource_type::iron_ore;
}

} // namespace resource_names
