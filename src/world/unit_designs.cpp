#include "unit_designs.hpp"

#include "digitisation_tree_data.hpp" // io::dg::find_node — the kit's research nodes
#include "nation_tech.hpp"            // nation_holds
#include "world.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// The kit catalogue (BL-1434; MILITARY.md § The kit). Item names are from
// docs/research/MILITARY_KIT.md § 2, mapped onto the per-class slots Ben ruled
// (2026-10-28). Deltas are per man per round on k_land_stats' scale (infantry's
// soft attack is 60), ADDED to the class before the row's quality scale; hardness
// is per-mille. A first cut, tuned against campaign_battle_harness.
//
// GOODS. Kit is goods (Ben, 2026-10-27). The six kit goods by kind (small arms,
// armoured vehicles, artillery pieces, guided weapons, drones, with electronics)
// are BL-1422's kit half and not yet resources, so each item names the EXISTING
// goods they are made of: steel and machinery for small arms and guns, steel,
// machinery and alloys for vehicles, electronics and ordnance for guided weapons,
// electronics and alloys for drones. Later items need more, and add electronics.
//
// RESEARCH. Every later item names the age-tree node that opens it. The nodes are
// BL-1436's (kit in the age tree) to place; the ids below follow the tree's
// DG-<tree>-<branch> scheme under a KIT branch that does not exist yet, so none is
// found and every later item is locked until that item lands.
//
// APPEND-ONLY within a slot: a design stores an item as its place in its slot's
// list, so inserting before an item re-points every saved design that fits it.
// ---------------------------------------------------------------------------

namespace {

using lc = land_class;
using rt = resource_type;

constexpr kit_good S(float q) { return {rt::steel, q}; }
constexpr kit_good M(float q) { return {rt::machinery, q}; }
constexpr kit_good A(float q) { return {rt::alloys, q}; }
constexpr kit_good E(float q) { return {rt::electronics, q}; }
constexpr kit_good O(float q) { return {rt::ordnance, q}; }

/// A base item: no delta, no goods, no node.
constexpr kit_item base(const char* id, const char* name, lc c, uint8_t slot)
{
    return {id, name, c, slot, {}, {}, 0, nullptr};
}

/// A later item. Delta order is land_combat_stats': soft, hard, hardness, defence, breakthrough,
/// recon, stealth (BL-1453: the last two are per unit; MILITARY.md § Recon).
constexpr kit_item later(const char* id, const char* name, lc c, uint8_t slot,
                         land_combat_stats d, const char* node,
                         kit_good g0, kit_good g1 = {}, kit_good g2 = {})
{
    const uint8_t n = static_cast<uint8_t>(1 + (g1.qty > 0.0f ? 1 : 0) + (g2.qty > 0.0f ? 1 : 0));
    return {id, name, c, slot, d, {g0, g1, g2}, n, node};
}

constexpr kit_item k_items[] = {
    // --- Infantry: Weapon (0) · Armour (1) ---------------------------------
    base ("inf.weapon.battle_rifle",          "Battle rifle",                      lc::infantry, 0),
    later("inf.weapon.assault_rifle",         "Assault rifle",                     lc::infantry, 0, {10, 0, 0, 0, 0}, "DG-KIT-INF-WEAPON-1", S(2), M(1)),
    later("inf.weapon.light_mortar",          "Light mortar",                      lc::infantry, 0, {20, 0, 0, 0, 0}, "DG-KIT-INF-WEAPON-2", S(2), O(2)),
    later("inf.weapon.wire_guided_atgm",      "Wire-guided anti-tank missile",     lc::infantry, 0, {0, 25, 0, 0, 0}, "DG-KIT-INF-WEAPON-3", M(1), E(1), O(2)),
    later("inf.weapon.fire_and_forget_atgm",  "Fire-and-forget anti-tank missile", lc::infantry, 0, {0, 40, 0, 0, 0}, "DG-KIT-INF-WEAPON-4", E(2), O(2)),
    base ("inf.armour.steel_helmet",          "Steel helmet",                      lc::infantry, 1),
    later("inf.armour.flak_vest",             "Flak vest",                         lc::infantry, 1, {0, 0, 0, 10, 0, 0, 5}, "DG-KIT-INF-ARMOUR-1", S(1), A(1)),
    later("inf.armour.ceramic_plates",        "Ceramic plate carrier",             lc::infantry, 1, {0, 0, 0, 20, 0, 0, 10}, "DG-KIT-INF-ARMOUR-2", A(2)),
    later("inf.armour.networked_kit",         "Networked soldier kit",             lc::infantry, 1, {10, 0, 0, 15, 10, 10, 15}, "DG-KIT-INF-ARMOUR-3", A(1), E(2)),

    // --- Motorised infantry: Weapon (0) · Armour (1) · Vehicle (2) ---------
    base ("mot.weapon.battle_rifle",          "Battle rifle",                      lc::motorised, 0),
    later("mot.weapon.assault_rifle",         "Assault rifle",                     lc::motorised, 0, {10, 0, 0, 0, 0}, "DG-KIT-MOT-WEAPON-1", S(2), M(1)),
    later("mot.weapon.wire_guided_atgm",      "Wire-guided anti-tank missile",     lc::motorised, 0, {0, 25, 0, 0, 0}, "DG-KIT-MOT-WEAPON-2", M(1), E(1), O(2)),
    base ("mot.armour.steel_helmet",          "Steel helmet",                      lc::motorised, 1),
    later("mot.armour.flak_vest",             "Flak vest",                         lc::motorised, 1, {0, 0, 0, 10, 0, 0, 5}, "DG-KIT-MOT-ARMOUR-1", S(1), A(1)),
    later("mot.armour.ceramic_plates",        "Ceramic plate carrier",             lc::motorised, 1, {0, 0, 0, 20, 0, 0, 10}, "DG-KIT-MOT-ARMOUR-2", A(2)),
    base ("mot.vehicle.trucks",               "Trucks",                            lc::motorised, 2),
    later("mot.vehicle.protected_trucks",     "Protected trucks",                  lc::motorised, 2, {0, 0, 150, 10, 0}, "DG-KIT-MOT-VEHICLE-1", S(3), M(1)),
    later("mot.vehicle.wheeled_carriers",     "Wheeled carriers",                  lc::motorised, 2, {0, 0, 250, 0, 15}, "DG-KIT-MOT-VEHICLE-2", S(4), M(2), A(1)),

    // --- Mechanised infantry: Weapon (0) · Armour (1) · Vehicle (2) --------
    base ("mech.weapon.battle_rifle",         "Battle rifle",                      lc::mechanised, 0),
    later("mech.weapon.assault_rifle",        "Assault rifle",                     lc::mechanised, 0, {10, 0, 0, 0, 0}, "DG-KIT-MECH-WEAPON-1", S(2), M(1)),
    later("mech.weapon.fire_and_forget_atgm", "Fire-and-forget anti-tank missile", lc::mechanised, 0, {0, 40, 0, 0, 0}, "DG-KIT-MECH-WEAPON-2", E(2), O(2)),
    base ("mech.armour.steel_helmet",         "Steel helmet",                      lc::mechanised, 1),
    later("mech.armour.flak_vest",            "Flak vest",                         lc::mechanised, 1, {0, 0, 0, 10, 0, 0, 5}, "DG-KIT-MECH-ARMOUR-1", S(1), A(1)),
    later("mech.armour.ceramic_plates",       "Ceramic plate carrier",             lc::mechanised, 1, {0, 0, 0, 20, 0, 0, 10}, "DG-KIT-MECH-ARMOUR-2", A(2)),
    base ("mech.vehicle.apc",                 "Armoured personnel carrier",        lc::mechanised, 2),
    later("mech.vehicle.ifv_autocannon",      "Infantry fighting vehicle (autocannon)", lc::mechanised, 2, {20, 15, 0, 0, 0}, "DG-KIT-MECH-VEHICLE-1", S(4), M(2)),
    later("mech.vehicle.ifv_missiles",        "Fighting vehicle with missiles",    lc::mechanised, 2, {20, 30, 0, 0, 0}, "DG-KIT-MECH-VEHICLE-2", S(4), M(2), E(1)),
    later("mech.vehicle.heavy_ifv",           "Heavy fighting vehicle, active protection", lc::mechanised, 2, {20, 30, 150, 20, 0, 5, 0}, "DG-KIT-MECH-VEHICLE-3", S(5), A(2), E(2)),

    // --- Medium armour (the armour class): Armament (0) · Armour (1) · Tracks (2)
    base ("arm.armament.rifled_gun",          "Rifled gun",                        lc::armour, 0),
    later("arm.armament.smoothbore_gun",      "Smoothbore gun",                    lc::armour, 0, {0, 30, 0, 0, 0}, "DG-KIT-ARM-ARMAMENT-1", S(3), M(2)),
    later("arm.armament.penetrators_thermal", "Improved penetrators, thermal sights", lc::armour, 0, {15, 55, 0, 0, 0, 5, 0}, "DG-KIT-ARM-ARMAMENT-2", S(3), M(2), E(1)),
    later("arm.armament.gun_missiles",        "Gun-launched missiles, networked fire control", lc::armour, 0, {30, 80, 0, 0, 0, 10, 0}, "DG-KIT-ARM-ARMAMENT-3", M(2), E(3), O(2)),
    base ("arm.armour.rolled_steel",          "Rolled steel",                      lc::armour, 1),
    later("arm.armour.composite",             "Composite armour",                  lc::armour, 1, {0, 0, 0, 15, 15}, "DG-KIT-ARM-ARMOUR-1", S(2), A(3)),
    later("arm.armour.reactive",              "Reactive armour",                   lc::armour, 1, {0, 0, 0, 30, 15}, "DG-KIT-ARM-ARMOUR-2", A(3), O(2)),
    later("arm.armour.active_protection",     "Active protection",                 lc::armour, 1, {0, 0, 0, 50, 35}, "DG-KIT-ARM-ARMOUR-3", A(3), E(3)),
    base ("arm.tracks.torsion_bar",           "Torsion-bar tracks",                lc::armour, 2),
    later("arm.tracks.hydropneumatic",        "Hydropneumatic suspension",         lc::armour, 2, {0, 0, 0, 0, 10}, "DG-KIT-ARM-TRACKS-1", S(2), M(2)),
    later("arm.tracks.composite_tracks",      "Lightweight composite tracks",      lc::armour, 2, {0, 0, 0, 5, 15}, "DG-KIT-ARM-TRACKS-2", A(2), M(2)),

    // --- Light armour: Armament (0) · Armour (1) · Wheels (2) --------------
    base ("lt.armament.heavy_mg",             "Heavy machine gun",                 lc::light_armour, 0),
    later("lt.armament.autocannon",           "Autocannon",                        lc::light_armour, 0, {15, 10, 0, 0, 0}, "DG-KIT-LT-ARMAMENT-1", S(2), M(1)),
    later("lt.armament.thermal_autocannon",   "Autocannon with thermal imaging",   lc::light_armour, 0, {30, 10, 0, 0, 0, 20, 0}, "DG-KIT-LT-ARMAMENT-2", S(2), M(1), E(1)),
    later("lt.armament.atgm_mount",           "Anti-tank missile mount",           lc::light_armour, 0, {0, 30, 0, 0, 0}, "DG-KIT-LT-ARMAMENT-3", M(1), E(1), O(2)),
    base ("lt.armour.light_plate",            "Light steel plate",                 lc::light_armour, 1),
    later("lt.armour.spaced",                 "Spaced armour",                     lc::light_armour, 1, {0, 0, 0, 10, 0}, "DG-KIT-LT-ARMOUR-1", S(2)),
    later("lt.armour.composite_applique",     "Composite applique",                lc::light_armour, 1, {0, 0, 100, 15, 0}, "DG-KIT-LT-ARMOUR-2", A(2)),
    base ("lt.wheels.scout_car",              "Scout car",                         lc::light_armour, 2),
    later("lt.wheels.six_wheeler",            "Run-flat six-wheeler",              lc::light_armour, 2, {0, 0, 0, 0, 10, 0, 5}, "DG-KIT-LT-WHEELS-1", S(2), M(1)),
    later("lt.wheels.tracked_scout",          "Tracked scout",                     lc::light_armour, 2, {0, 0, 100, 0, 15}, "DG-KIT-LT-WHEELS-2", S(3), M(2)),

    // --- Artillery: Gun (0) · Munitions (1) · Tow (2) ----------------------
    base ("art.gun.towed_howitzer",           "Towed howitzer",                    lc::artillery, 0),
    later("art.gun.long_barrel",              "Long-barrel howitzer",              lc::artillery, 0, {20, 0, 0, 0, 0}, "DG-KIT-ART-GUN-1", S(3), M(1)),
    later("art.gun.rocket_launcher",          "Multiple rocket launcher",          lc::artillery, 0, {60, 0, 0, 0, 0}, "DG-KIT-ART-GUN-2", S(3), M(2), O(2)),
    later("art.gun.guided_rockets",           "Guided long-range rockets",         lc::artillery, 0, {40, 20, 0, 0, 0}, "DG-KIT-ART-GUN-3", M(2), E(2), O(2)),
    base ("art.munitions.he_shells",          "High-explosive shells",             lc::artillery, 1),
    later("art.munitions.guided_shells",      "Guided shells",                     lc::artillery, 1, {20, 20, 0, 0, 0}, "DG-KIT-ART-MUNITIONS-1", O(2), E(1)),
    later("art.munitions.submunitions",       "Anti-armour submunitions",          lc::artillery, 1, {0, 40, 0, 0, 0}, "DG-KIT-ART-MUNITIONS-2", O(3), E(1)),
    later("art.munitions.loitering_drones",   "Loitering drones",                  lc::artillery, 1, {20, 40, 0, 0, 0, 25, 0}, "DG-KIT-ART-MUNITIONS-3", E(2), A(2)),
    base ("art.tow.gun_tractor",              "Gun tractor",                       lc::artillery, 2),
    later("art.tow.self_propelled",           "Self-propelled chassis",            lc::artillery, 2, {0, 0, 300, 0, 15}, "DG-KIT-ART-TOW-1", S(4), M(2)),
    later("art.tow.armoured_sp",              "Armoured self-propelled chassis",   lc::artillery, 2, {0, 0, 400, 10, 15}, "DG-KIT-ART-TOW-2", S(5), M(2), A(1)),

    // --- Air defence: Launcher (0) · Fire control (1) · Vehicle (2) --------
    // Its kit is mostly for the air theatre (BL-1429); the ground effects stay small.
    base ("ad.launcher.radar_guns",           "Radar-directed guns",               lc::air_defence, 0),
    later("ad.launcher.short_range",          "Short-range missiles",              lc::air_defence, 0, {0, 5, 0, 0, 0}, "DG-KIT-AD-LAUNCHER-1", E(1), O(2)),
    later("ad.launcher.medium_range",         "Medium-range missiles",             lc::air_defence, 0, {0, 10, 0, 0, 0}, "DG-KIT-AD-LAUNCHER-2", E(2), O(2)),
    later("ad.launcher.layered",              "Layered defence with counter-drone", lc::air_defence, 0, {5, 10, 0, 0, 0}, "DG-KIT-AD-LAUNCHER-3", E(3), O(2)),
    base ("ad.fire_control.optical",          "Optical",                           lc::air_defence, 1),
    later("ad.fire_control.search_radar",     "Search radar",                      lc::air_defence, 1, {0, 0, 0, 5, 0, 5, 0}, "DG-KIT-AD-FIRE-1", E(2)),
    later("ad.fire_control.integrated",       "Integrated network",                lc::air_defence, 1, {0, 0, 0, 10, 0, 10, 0}, "DG-KIT-AD-FIRE-2", E(3)),
    later("ad.fire_control.electronic_warfare", "Electronic warfare",              lc::air_defence, 1, {5, 0, 0, 10, 0}, "DG-KIT-AD-FIRE-3", E(3), A(1)),
    base ("ad.vehicle.towed_mount",           "Towed mount",                       lc::air_defence, 2),
    later("ad.vehicle.truck_mounted",         "Truck-mounted",                     lc::air_defence, 2, {0, 0, 0, 0, 5}, "DG-KIT-AD-VEHICLE-1", S(2), M(1)),
    later("ad.vehicle.tracked_carrier",       "Tracked carrier",                   lc::air_defence, 2, {0, 0, 200, 5, 0}, "DG-KIT-AD-VEHICLE-2", S(3), M(2)),
};

constexpr std::size_t k_item_count = sizeof(k_items) / sizeof(k_items[0]);

/// The slot names, per land class (MILITARY.md § The kit table).
constexpr const char* k_slot_names[land_class_count][k_kit_slot_max] = {
    {"Weapon",   "Armour",       ""},        // infantry
    {"Weapon",   "Armour",       "Vehicle"}, // motorised
    {"Weapon",   "Armour",       "Vehicle"}, // mechanised
    {"Armament", "Armour",       "Tracks"},  // armour (medium armour)
    {"Armament", "Armour",       "Wheels"},  // light armour
    {"Gun",      "Munitions",    "Tow"},     // artillery
    {"Launcher", "Fire control", "Vehicle"}, // air defence
    {"",         "",             ""},        // none
};

constexpr int slot_count(lc c)
{
    switch (c)
    {
        case lc::infantry:     return 2;
        case lc::motorised:
        case lc::mechanised:
        case lc::armour:
        case lc::light_armour:
        case lc::artillery:
        case lc::air_defence:  return 3;
        case lc::none:         return 0;
    }
    return 0;
}

constexpr bool str_eq(const char* a, const char* b)
{
    if (a == nullptr || b == nullptr)
        return a == b;
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return *a == *b;
}

constexpr bool is_zero(const land_combat_stats& d)
{
    return d.soft == 0 && d.hard == 0 && d.hardness == 0 && d.defence == 0 && d.breakthrough == 0
        && d.recon == 0 && d.stealth == 0;
}

/// The class hardness the validator keeps a delta inside (k_land_stats' column;
/// checked against land_class_stats at run time by the harness).
constexpr int class_hardness(lc c)
{
    switch (c)
    {
        case lc::infantry:     return 0;
        case lc::motorised:    return 100;
        case lc::mechanised:   return 600;
        case lc::armour:       return 900;
        case lc::light_armour: return 500;
        case lc::artillery:    return 100;
        case lc::air_defence:  return 200;
        case lc::none:         return 0;
    }
    return 0;
}

constexpr const char* first_error()
{
    if (k_item_count >= k_no_kit_item)
        return "catalogue too large for a u16 id";
    for (std::size_t i = 0; i < k_item_count; ++i)
    {
        const kit_item& it = k_items[i];
        if (it.id == nullptr || it.id[0] == '\0' || it.name == nullptr || it.name[0] == '\0')
            return "an item has no id or name";
        if (static_cast<int>(it.slot) >= slot_count(it.cls))
            return "an item names a slot its class lacks";
        for (std::size_t j = 0; j < i; ++j)
        {
            if (str_eq(k_items[j].id, it.id))
                return "two items share an id";
            if (it.research_node != nullptr && str_eq(k_items[j].research_node, it.research_node))
                return "two items share a research node";
        }
        const int hard = class_hardness(it.cls) + it.delta.hardness;
        if (hard < 0 || hard > 1000)
            return "a hardness delta takes its class outside 0..1000";
        if (it.goods_count > k_kit_goods_max)
            return "an item names too many goods";
        // BL-1453: kit only ADDS recon and stealth (MILITARY.md § Recon: light armour,
        // drones and optics carry recon; better infantry armour raises stealth).
        if (it.delta.recon < 0 || it.delta.stealth < 0)
            return "a recon or stealth delta is negative";
        if (it.research_node == nullptr)
        {
            if (!is_zero(it.delta) || it.goods_count != 0)
                return "a base item carries a delta or goods";
        }
        else
        {
            if (it.research_node[0] == '\0')
                return "a later item names an empty research node";
            if (it.goods_count == 0)
                return "a later item names no goods";
            for (std::size_t g = 0; g < it.goods_count; ++g)
                if (!(it.goods[g].qty > 0.0f))
                    return "a later item names a good of no quantity";
        }
    }
    for (int c = 0; c < land_class_count; ++c)
    {
        const lc cls = static_cast<lc>(c);
        for (int s = 0; s < slot_count(cls); ++s)
        {
            int count = 0, bases = 0;
            bool base_first = false;
            for (std::size_t i = 0; i < k_item_count; ++i)
            {
                const kit_item& it = k_items[i];
                if (it.cls != cls || static_cast<int>(it.slot) != s)
                    continue;
                if (it.research_node == nullptr)
                {
                    ++bases;
                    base_first = (count == 0);
                }
                ++count;
            }
            if (bases != 1 || !base_first)
                return "a slot has no base item listed first, or more than one";
            if (count - 1 < 2 || count - 1 > 4)
                return "a slot has fewer than two or more than four later items";
        }
    }
    return nullptr;
}

static_assert(first_error() == nullptr, "the kit catalogue is malformed: see first_error()");

/// Per (class, slot), the catalogue ids that fit it, in table order (base first).
struct slot_index
{
    std::vector<uint16_t> items[land_class_count][k_kit_slot_max];
    std::vector<uint8_t>  variant; ///< Per catalogue id.
};

const slot_index& index()
{
    static const slot_index idx = [] {
        slot_index s;
        s.variant.assign(k_item_count, 0);
        for (std::size_t i = 0; i < k_item_count; ++i)
        {
            const kit_item& it = k_items[i];
            std::vector<uint16_t>& v = s.items[static_cast<std::size_t>(it.cls)][it.slot];
            s.variant[i] = static_cast<uint8_t>(v.size());
            v.push_back(static_cast<uint16_t>(i));
        }
        return s;
    }();
    return idx;
}

const std::vector<uint16_t> k_no_items;

bool g_waived = false;

} // namespace

const std::vector<kit_item>& kit_catalogue()
{
    static const std::vector<kit_item> v(std::begin(k_items), std::end(k_items));
    return v;
}

const char* kit_catalogue_first_error()
{
    return first_error();
}

int kit_slot_count(land_class c)
{
    return slot_count(c);
}

const char* kit_slot_name(land_class c, int slot)
{
    const auto i = static_cast<std::size_t>(c);
    if (i >= static_cast<std::size_t>(land_class_count) || slot < 0 || slot >= slot_count(c))
        return "";
    return k_slot_names[i][slot];
}

const std::vector<uint16_t>& kit_items_for(land_class c, int slot)
{
    const auto i = static_cast<std::size_t>(c);
    if (i >= static_cast<std::size_t>(land_class_count) || slot < 0 || slot >= slot_count(c))
        return k_no_items;
    return index().items[i][slot];
}

uint16_t kit_item_at(land_class c, int slot, uint8_t variant)
{
    const std::vector<uint16_t>& v = kit_items_for(c, slot);
    return variant < v.size() ? v[variant] : k_no_kit_item;
}

uint16_t kit_item_by_id(std::string_view id)
{
    for (std::size_t i = 0; i < k_item_count; ++i)
        if (id == k_items[i].id)
            return static_cast<uint16_t>(i);
    return k_no_kit_item;
}

uint8_t kit_variant_of(uint16_t item)
{
    return item < k_item_count ? index().variant[item] : uint8_t{0};
}

bool kit_item_researched(const world& w, entity_id nation, uint16_t item)
{
    if (item >= k_item_count)
        return false;
    const kit_item& it = k_items[item];
    if (it.research_node == nullptr)
        return true; // base kit: open to every nation
    if (g_waived)
        return true;
    const int node = io::dg::find_node(it.research_node);
    return node >= 0 && nation_holds(w, nation, node);
}

void set_kit_research_waived_for_harness(bool waived)
{
    g_waived = waived;
}

unit_design design_of(const world& w, entity_id nation, land_class c)
{
    unit_design d;
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end() || slot_count(c) == 0)
        return d;
    for (const uint16_t item : nit->second.unit_design_kit)
    {
        if (item >= k_item_count || k_items[item].cls != c)
            continue;
        d.fit[k_items[item].slot] = kit_variant_of(item);
    }
    return d;
}

uint16_t design_fitted(const world& w, entity_id nation, land_class c, int slot)
{
    if (slot < 0 || slot >= slot_count(c))
        return k_no_kit_item;
    return kit_item_at(c, slot, design_of(w, nation, c).fit[static_cast<std::size_t>(slot)]);
}

land_combat_stats kit_stats(land_class c, const unit_design& d)
{
    land_combat_stats s = land_class_stats(c);
    for (int slot = 0; slot < slot_count(c); ++slot)
    {
        const uint16_t item = kit_item_at(c, slot, d.fit[static_cast<std::size_t>(slot)]);
        if (item == k_no_kit_item)
            continue; // out of range reads as base, which adds nothing
        const land_combat_stats& k = k_items[item].delta;
        s.soft += k.soft;
        s.hard += k.hard;
        s.hardness += k.hardness;
        s.defence += k.defence;
        s.breakthrough += k.breakthrough;
        s.recon += k.recon;
        s.stealth += k.stealth;
    }
    s.soft         = std::max(0, s.soft);
    s.hard         = std::max(0, s.hard);
    s.defence      = std::max(0, s.defence);
    s.breakthrough = std::max(0, s.breakthrough);
    s.hardness     = std::clamp(s.hardness, 0, 1000);
    s.recon        = std::max(0, s.recon);
    s.stealth      = std::max(0, s.stealth);
    return s;
}

land_combat_stats design_stats(const world& w, entity_id nation, land_class c)
{
    return kit_stats(c, design_of(w, nation, c));
}

namespace {

void add_goods(std::array<float, resource_count>& out, uint16_t item)
{
    if (item >= k_item_count)
        return;
    const kit_item& it = k_items[item];
    for (std::size_t g = 0; g < it.goods_count; ++g)
        out[static_cast<std::size_t>(it.goods[g].res)] += it.goods[g].qty;
}

} // namespace

std::array<float, resource_count> kit_goods(land_class c, const unit_design& d)
{
    std::array<float, resource_count> out{};
    for (int slot = 0; slot < slot_count(c); ++slot)
        add_goods(out, kit_item_at(c, slot, d.fit[static_cast<std::size_t>(slot)]));
    return out;
}

std::array<float, resource_count> kit_refit_goods(land_class c, const unit_design& from,
                                                  const unit_design& to)
{
    std::array<float, resource_count> out{};
    for (int slot = 0; slot < slot_count(c); ++slot)
    {
        const auto s = static_cast<std::size_t>(slot);
        if (from.fit[s] != to.fit[s])
            add_goods(out, kit_item_at(c, slot, to.fit[s]));
    }
    return out;
}

bool design_valid(land_class c, const unit_design& d)
{
    for (std::size_t s = 0; s < k_kit_slot_max; ++s)
    {
        if (static_cast<int>(s) >= slot_count(c))
        {
            if (d.fit[s] != 0) return false;
        }
        else if (d.fit[s] >= kit_items_for(c, static_cast<int>(s)).size())
            return false;
    }
    return true;
}

land_class unit_land_class(const unit_component& u)
{
    const std::vector<roster_row>& t = unit_roster_table();
    return u.type < t.size() ? land_class_for(t[u.type]) : land_class::infantry;
}

bool unit_design_stale(const world& w, const unit_component& u)
{
    return u.kit != design_of(w, u.owner, unit_land_class(u));
}

// ===========================================================================
// BL-1449 — the AIR kit catalogue (MILITARY.md § Air). Same rules as the land
// catalogue above: per (air class, slot) a base item first, then two later items,
// each naming a research node under a KIT-AIR branch the age tree does not hold
// yet (so every later item reads locked until the kit nodes are placed) and the
// existing goods it is made of. Deltas are on air_class_stats' scales: hp in HP,
// air in milli-HP a day, soft/hard in milli-hits a day, strategic in condition
// per-mille a day, recon in per-mille, range in tiles. APPEND-ONLY within a slot.
// ===========================================================================

namespace {

using ac = air_class;

constexpr kit_good AL(float q) { return {resource_type::alloys, q}; }
constexpr kit_good EL(float q) { return {resource_type::electronics, q}; }
constexpr kit_good MA(float q) { return {resource_type::machinery, q}; }
constexpr kit_good OR(float q) { return {resource_type::ordnance, q}; }

constexpr air_kit_item abase(const char* id, const char* name, ac c, uint8_t slot)
{
    return {id, name, c, slot, {}, {}, 0, nullptr};
}

/// Delta order is air_combat_stats': hp, air, soft, hard, strategic, recon, range.
constexpr air_kit_item alater(const char* id, const char* name, ac c, uint8_t slot,
                              air_combat_stats d, const char* node,
                              kit_good g0, kit_good g1 = {}, kit_good g2 = {})
{
    const uint8_t n = static_cast<uint8_t>(1 + (g1.qty > 0.0f ? 1 : 0) + (g2.qty > 0.0f ? 1 : 0));
    return {id, name, c, slot, d, {g0, g1, g2}, n, node};
}

constexpr air_kit_item k_air_items[] = {
    // --- Fighter: Airframe (0) · Armament (1) · Avionics (2) ------------------
    abase ("ftr.airframe.swept_wing",      "Swept-wing interceptor",               ac::fighter, 0),
    alater("ftr.airframe.twin_engine",     "Twin-engine air superiority fighter",  ac::fighter, 0, {20, 300, 0, 0, 0, 0, 2}, "DG-KIT-AIR-FTR-AIRFRAME-1", AL(3), MA(2)),
    alater("ftr.airframe.low_observable",  "Low-observable fighter",               ac::fighter, 0, {10, 600, 0, 0, 0, 0, 3}, "DG-KIT-AIR-FTR-AIRFRAME-2", AL(4), EL(2)),
    abase ("ftr.armament.cannon_heat",     "Cannon and heat-seeking missiles",     ac::fighter, 1),
    alater("ftr.armament.radar_guided",    "Radar-guided missiles",                ac::fighter, 1, {0, 400, 0, 0, 0, 0, 0}, "DG-KIT-AIR-FTR-ARMAMENT-1", EL(1), OR(2)),
    alater("ftr.armament.beyond_visual",   "Beyond-visual-range missiles",         ac::fighter, 1, {0, 800, 0, 0, 0, 0, 0}, "DG-KIT-AIR-FTR-ARMAMENT-2", EL(2), OR(2)),
    abase ("ftr.avionics.ranging_radar",   "Ranging radar",                        ac::fighter, 2),
    alater("ftr.avionics.pulse_doppler",   "Pulse-doppler radar",                  ac::fighter, 2, {0, 200, 0, 0, 0, 0, 2}, "DG-KIT-AIR-FTR-AVIONICS-1", EL(2)),
    alater("ftr.avionics.scanned_array",   "Electronically scanned radar",         ac::fighter, 2, {0, 400, 0, 0, 0, 0, 4}, "DG-KIT-AIR-FTR-AVIONICS-2", EL(3)),

    // --- Strike / multirole ---------------------------------------------------
    abase ("mr.airframe.fighter_bomber",   "Fighter-bomber",                       ac::multirole, 0),
    alater("mr.airframe.multirole",        "Multirole fighter",                    ac::multirole, 0, {15, 200, 0, 0, 0, 0, 2}, "DG-KIT-AIR-MR-AIRFRAME-1", AL(3), MA(2)),
    alater("mr.airframe.stealth_strike",   "Low-observable strike airframe",       ac::multirole, 0, {10, 200, 10, 10, 0, 0, 4}, "DG-KIT-AIR-MR-AIRFRAME-2", AL(4), EL(2)),
    abase ("mr.armament.unguided",         "Unguided bombs and rockets",           ac::multirole, 1),
    alater("mr.armament.laser_guided",     "Laser-guided bombs",                   ac::multirole, 1, {0, 0, 20, 30, 0, 0, 0}, "DG-KIT-AIR-MR-ARMAMENT-1", EL(1), OR(2)),
    alater("mr.armament.standoff",         "Precision stand-off weapons",          ac::multirole, 1, {0, 0, 30, 50, 0, 0, 2}, "DG-KIT-AIR-MR-ARMAMENT-2", EL(2), OR(3)),
    abase ("mr.avionics.bombsight",        "Bombsight radar",                      ac::multirole, 2),
    alater("mr.avionics.targeting_pod",    "Targeting pod",                        ac::multirole, 2, {0, 0, 10, 10, 0, 0, 0}, "DG-KIT-AIR-MR-AVIONICS-1", EL(2)),
    alater("mr.avionics.sensor_fusion",    "Networked sensor fusion",              ac::multirole, 2, {0, 200, 15, 15, 0, 0, 0}, "DG-KIT-AIR-MR-AVIONICS-2", EL(3)),

    // --- Strategic bomber -----------------------------------------------------
    abase ("bmr.airframe.subsonic",        "Subsonic heavy bomber",                ac::bomber, 0),
    alater("bmr.airframe.supersonic",      "Supersonic bomber",                    ac::bomber, 0, {20, 0, 0, 0, 0, 0, 5}, "DG-KIT-AIR-BMR-AIRFRAME-1", AL(5), MA(3)),
    alater("bmr.airframe.low_observable",  "Low-observable bomber",                ac::bomber, 0, {30, 0, 0, 0, 0, 0, 5}, "DG-KIT-AIR-BMR-AIRFRAME-2", AL(6), EL(3)),
    abase ("bmr.armament.free_fall",       "Free-fall bombs",                      ac::bomber, 1),
    alater("bmr.armament.guided_bombs",    "Guided bombs",                         ac::bomber, 1, {0, 0, 0, 0, 3, 0, 0}, "DG-KIT-AIR-BMR-ARMAMENT-1", EL(1), OR(3)),
    alater("bmr.armament.cruise_missiles", "Air-launched cruise missiles",         ac::bomber, 1, {0, 0, 0, 0, 6, 0, 10}, "DG-KIT-AIR-BMR-ARMAMENT-2", EL(2), OR(4)),
    abase ("bmr.avionics.navigation",      "Navigation radar",                     ac::bomber, 2),
    alater("bmr.avionics.terrain_follow",  "Terrain-following radar",              ac::bomber, 2, {10, 0, 0, 0, 0, 0, 0}, "DG-KIT-AIR-BMR-AVIONICS-1", EL(2)),
    alater("bmr.avionics.countermeasures", "Electronic countermeasures suite",     ac::bomber, 2, {25, 0, 0, 0, 0, 0, 0}, "DG-KIT-AIR-BMR-AVIONICS-2", EL(3)),

    // --- Attack helicopter ----------------------------------------------------
    abase ("hel.airframe.utility_gunship", "Utility gunship",                      ac::helicopter, 0),
    alater("hel.airframe.attack",          "Dedicated attack helicopter",          ac::helicopter, 0, {15, 0, 0, 20, 0, 0, 1}, "DG-KIT-AIR-HEL-AIRFRAME-1", AL(2), MA(2)),
    alater("hel.airframe.armoured",        "Armoured tandem-seat helicopter",      ac::helicopter, 0, {25, 0, 0, 30, 0, 0, 1}, "DG-KIT-AIR-HEL-AIRFRAME-2", AL(3), MA(2)),
    abase ("hel.armament.rockets",         "Rockets and door guns",                ac::helicopter, 1),
    alater("hel.armament.wire_guided",     "Wire-guided anti-tank missiles",       ac::helicopter, 1, {0, 0, 0, 40, 0, 0, 0}, "DG-KIT-AIR-HEL-ARMAMENT-1", EL(1), OR(2)),
    alater("hel.armament.fire_and_forget", "Fire-and-forget anti-tank missiles",   ac::helicopter, 1, {0, 0, 10, 70, 0, 0, 0}, "DG-KIT-AIR-HEL-ARMAMENT-2", EL(2), OR(2)),
    abase ("hel.avionics.day_optics",      "Day optics",                           ac::helicopter, 2),
    alater("hel.avionics.night_vision",    "Night-vision sights",                  ac::helicopter, 2, {0, 0, 10, 10, 0, 0, 0}, "DG-KIT-AIR-HEL-AVIONICS-1", EL(2)),
    alater("hel.avionics.mast_radar",      "Mast-mounted radar",                   ac::helicopter, 2, {0, 0, 15, 20, 0, 50, 0}, "DG-KIT-AIR-HEL-AVIONICS-2", EL(3)),

    // --- Strike drone ---------------------------------------------------------
    abase ("drn.airframe.target_drone",    "Converted target drone",               ac::drone, 0),
    alater("drn.airframe.long_endurance",  "Medium-altitude long-endurance drone", ac::drone, 0, {15, 0, 0, 0, 0, 0, 4}, "DG-KIT-AIR-DRN-AIRFRAME-1", AL(2), EL(1)),
    alater("drn.airframe.jet_combat",      "Jet-powered combat drone",             ac::drone, 0, {20, 0, 0, 0, 0, 0, 6}, "DG-KIT-AIR-DRN-AIRFRAME-2", AL(3), EL(2)),
    abase ("drn.armament.light_rockets",   "Light guided rockets",                 ac::drone, 1),
    alater("drn.armament.guided_missiles", "Guided missiles",                      ac::drone, 1, {0, 0, 20, 25, 0, 0, 0}, "DG-KIT-AIR-DRN-ARMAMENT-1", EL(1), OR(2)),
    alater("drn.armament.loitering",       "Loitering munitions",                  ac::drone, 1, {0, 0, 30, 40, 0, 0, 0}, "DG-KIT-AIR-DRN-ARMAMENT-2", EL(2), AL(1)),
    abase ("drn.avionics.line_of_sight",   "Line-of-sight datalink",               ac::drone, 2),
    alater("drn.avionics.satellite_link",  "Satellite datalink",                   ac::drone, 2, {0, 0, 0, 0, 0, 50, 6}, "DG-KIT-AIR-DRN-AVIONICS-1", EL(2)),
    alater("drn.avionics.autonomous",      "Autonomous targeting",                 ac::drone, 2, {0, 0, 10, 10, 0, 50, 0}, "DG-KIT-AIR-DRN-AVIONICS-2", EL(3)),

    // --- Recon / AWACS --------------------------------------------------------
    abase ("rcn.airframe.recon_jet",       "Reconnaissance jet",                   ac::recon, 0),
    alater("rcn.airframe.high_altitude",   "High-altitude reconnaissance aircraft", ac::recon, 0, {20, 0, 0, 0, 0, 0, 5}, "DG-KIT-AIR-RCN-AIRFRAME-1", AL(3), EL(1)),
    alater("rcn.airframe.early_warning",   "Airborne early-warning aircraft",      ac::recon, 0, {15, 0, 0, 0, 0, 100, 0}, "DG-KIT-AIR-RCN-AIRFRAME-2", AL(3), EL(3)),
    abase ("rcn.armament.defensive_guns",  "Defensive guns",                       ac::recon, 1),
    alater("rcn.armament.self_defence",    "Self-defence missiles",                ac::recon, 1, {10, 0, 0, 0, 0, 0, 0}, "DG-KIT-AIR-RCN-ARMAMENT-1", EL(1), OR(1)),
    alater("rcn.armament.towed_decoys",    "Towed decoys",                         ac::recon, 1, {20, 0, 0, 0, 0, 0, 0}, "DG-KIT-AIR-RCN-ARMAMENT-2", EL(2)),
    abase ("rcn.avionics.cameras",         "Cameras",                              ac::recon, 2),
    alater("rcn.avionics.side_looking",    "Side-looking radar",                   ac::recon, 2, {0, 0, 0, 0, 0, 80, 0}, "DG-KIT-AIR-RCN-AVIONICS-1", EL(2)),
    alater("rcn.avionics.signals_intel",   "Signals-intelligence suite",           ac::recon, 2, {0, 0, 0, 0, 0, 150, 0}, "DG-KIT-AIR-RCN-AVIONICS-2", EL(3)),
};

constexpr std::size_t k_air_item_count = sizeof(k_air_items) / sizeof(k_air_items[0]);

constexpr bool air_zero(const air_combat_stats& d)
{
    return d.hp == 0 && d.air == 0 && d.soft == 0 && d.hard == 0 && d.strategic == 0
        && d.recon == 0 && d.range == 0;
}

constexpr const char* air_first_error()
{
    if (k_air_item_count >= k_no_kit_item)
        return "air catalogue too large for a u16 id";
    for (std::size_t i = 0; i < k_air_item_count; ++i)
    {
        const air_kit_item& it = k_air_items[i];
        if (it.id == nullptr || it.id[0] == '\0' || it.name == nullptr || it.name[0] == '\0')
            return "an air item has no id or name";
        if (static_cast<int>(it.cls) >= air_class_count || it.slot >= k_air_kit_slot_count)
            return "an air item names a class or slot that does not exist";
        for (std::size_t j = 0; j < i; ++j)
        {
            if (str_eq(k_air_items[j].id, it.id))
                return "two air items share an id";
            if (it.research_node != nullptr && str_eq(k_air_items[j].research_node, it.research_node))
                return "two air items share a research node";
        }
        for (std::size_t j = 0; j < k_item_count; ++j)
            if (it.research_node != nullptr && str_eq(k_items[j].research_node, it.research_node))
                return "an air item shares a research node with a land item";
        if (it.goods_count > k_kit_goods_max)
            return "an air item names too many goods";
        if (it.research_node == nullptr)
        {
            if (!air_zero(it.delta) || it.goods_count != 0)
                return "a base air item carries a delta or goods";
        }
        else
        {
            if (it.research_node[0] == '\0' || it.goods_count == 0)
                return "a later air item names no research node or no goods";
            for (std::size_t g = 0; g < it.goods_count; ++g)
                if (!(it.goods[g].qty > 0.0f))
                    return "a later air item names a good of no quantity";
        }
    }
    for (int c = 0; c < air_class_count; ++c)
        for (int s = 0; s < k_air_kit_slot_count; ++s)
        {
            int count = 0, bases = 0;
            bool base_first = false;
            for (std::size_t i = 0; i < k_air_item_count; ++i)
            {
                const air_kit_item& it = k_air_items[i];
                if (static_cast<int>(it.cls) != c || static_cast<int>(it.slot) != s)
                    continue;
                if (it.research_node == nullptr)
                {
                    ++bases;
                    base_first = (count == 0);
                }
                ++count;
            }
            if (bases != 1 || !base_first)
                return "an air slot has no base item listed first, or more than one";
            if (count - 1 < 2 || count - 1 > 4)
                return "an air slot has fewer than two or more than four later items";
        }
    return nullptr;
}

static_assert(air_first_error() == nullptr, "the air kit catalogue is malformed: see air_first_error()");

struct air_slot_index
{
    std::vector<uint16_t> items[air_class_count][k_air_kit_slot_count];
    std::vector<uint8_t>  variant;
};

const air_slot_index& air_index()
{
    static const air_slot_index idx = [] {
        air_slot_index s;
        s.variant.assign(k_air_item_count, 0);
        for (std::size_t i = 0; i < k_air_item_count; ++i)
        {
            const air_kit_item& it = k_air_items[i];
            std::vector<uint16_t>& v = s.items[static_cast<std::size_t>(it.cls)][it.slot];
            s.variant[i] = static_cast<uint8_t>(v.size());
            v.push_back(static_cast<uint16_t>(i));
        }
        return s;
    }();
    return idx;
}

bool air_class_ok(air_class c, int slot)
{
    return static_cast<int>(c) < air_class_count && slot >= 0 && slot < k_air_kit_slot_count;
}

} // namespace

const std::vector<air_kit_item>& air_kit_catalogue()
{
    static const std::vector<air_kit_item> v(std::begin(k_air_items), std::end(k_air_items));
    return v;
}

const char* air_kit_catalogue_first_error()
{
    return air_first_error();
}

const char* air_kit_slot_name(int slot)
{
    static const char* const names[k_air_kit_slot_count] = {"Airframe", "Armament", "Avionics"};
    return (slot >= 0 && slot < k_air_kit_slot_count) ? names[slot] : "";
}

const std::vector<uint16_t>& air_kit_items_for(air_class c, int slot)
{
    if (!air_class_ok(c, slot))
        return k_no_items;
    return air_index().items[static_cast<std::size_t>(c)][slot];
}

uint16_t air_kit_item_at(air_class c, int slot, uint8_t variant)
{
    const std::vector<uint16_t>& v = air_kit_items_for(c, slot);
    return variant < v.size() ? v[variant] : k_no_kit_item;
}

uint16_t air_kit_item_by_id(std::string_view id)
{
    for (std::size_t i = 0; i < k_air_item_count; ++i)
        if (id == k_air_items[i].id)
            return static_cast<uint16_t>(i);
    return k_no_kit_item;
}

uint8_t air_kit_variant_of(uint16_t item)
{
    return item < k_air_item_count ? air_index().variant[item] : uint8_t{0};
}

bool air_kit_item_researched(const world& w, entity_id nation, uint16_t item)
{
    if (item >= k_air_item_count)
        return false;
    const air_kit_item& it = k_air_items[item];
    if (it.research_node == nullptr)
        return true;
    if (g_waived)
        return true;
    const int node = io::dg::find_node(it.research_node);
    return node >= 0 && nation_holds(w, nation, node);
}

unit_design air_design_of(const world& w, entity_id nation, air_class c)
{
    unit_design d;
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end() || static_cast<int>(c) >= air_class_count)
        return d;
    for (const uint16_t item : nit->second.air_design_kit)
    {
        if (item >= k_air_item_count || k_air_items[item].cls != c)
            continue;
        d.fit[k_air_items[item].slot] = air_kit_variant_of(item);
    }
    return d;
}

uint16_t air_design_fitted(const world& w, entity_id nation, air_class c, int slot)
{
    if (!air_class_ok(c, slot))
        return k_no_kit_item;
    return air_kit_item_at(c, slot, air_design_of(w, nation, c).fit[static_cast<std::size_t>(slot)]);
}

air_combat_stats air_kit_delta(air_class c, const unit_design& d)
{
    air_combat_stats s;
    for (int slot = 0; slot < k_air_kit_slot_count; ++slot)
    {
        const uint16_t item = air_kit_item_at(c, slot, d.fit[static_cast<std::size_t>(slot)]);
        if (item == k_no_kit_item)
            continue;
        const air_combat_stats& k = k_air_items[item].delta;
        s.hp += k.hp; s.air += k.air; s.soft += k.soft; s.hard += k.hard;
        s.strategic += k.strategic; s.recon += k.recon; s.range += k.range;
    }
    return s;
}

std::array<float, resource_count> air_kit_goods(air_class c, const unit_design& d)
{
    std::array<float, resource_count> out{};
    for (int slot = 0; slot < k_air_kit_slot_count; ++slot)
    {
        const uint16_t item = air_kit_item_at(c, slot, d.fit[static_cast<std::size_t>(slot)]);
        if (item >= k_air_item_count)
            continue;
        const air_kit_item& it = k_air_items[item];
        for (std::size_t g = 0; g < it.goods_count; ++g)
            out[static_cast<std::size_t>(it.goods[g].res)] += it.goods[g].qty;
    }
    return out;
}

bool air_design_valid(air_class c, const unit_design& d)
{
    if (static_cast<int>(c) >= air_class_count)
        return false;
    for (int s = 0; s < k_air_kit_slot_count; ++s)
        if (d.fit[static_cast<std::size_t>(s)] >= air_kit_items_for(c, s).size())
            return false;
    return true;
}
