#pragma once

#include <string_view>

// ---------------------------------------------------------------------------
// Space mechanic keys (BL-1592 space unlocks, BL-1607 early space tree)
// ---------------------------------------------------------------------------
// Every space thing research unlocks is a `mechanic` effect on one tree node,
// keyed by one of these strings. A system that launches, fits, builds or
// designs the thing asks `nation_has_mechanic(w, nation, key)` (nation_tech.hpp)
// with the constant, never a literal.
//
// NEAR SPACE is the Digitisation tree's (docs/economy/tech/DIGITISATION_TREE.md
// § Space unlocks); FAR SPACE is the Early Space tree's
// (docs/economy/tech/EARLY_SPACE_TREE.md § Effects). tools/session/tech_tree_store.js
// --check holds this file to the two stores both ways: each list below is
// exactly its tree's mechanic keys, and every key sits on exactly one node.
// Keep one constant per line in the `= "key";` form: the check parses it.
// ---------------------------------------------------------------------------

namespace space_keys {

// ---- near space: the Digitisation tree ------------------------------------
inline constexpr std::string_view dispatch                   = "space_dispatch";             // Rocketry
inline constexpr std::string_view role_comms                 = "space_role_comms";           // Communication Satellites
inline constexpr std::string_view role_navigation            = "space_role_navigation";      // Satellite Navigation
inline constexpr std::string_view role_observation           = "space_role_observation";     // Thermal Imaging
inline constexpr std::string_view role_survey                = "space_role_survey";          // Seismic Survey
inline constexpr std::string_view astronaut_training         = "astronaut_training";         // Mass Higher Education
inline constexpr std::string_view station_core               = "space_station_core";         // Rocketry
inline constexpr std::string_view module_research            = "space_module_research";      // Research Culture
inline constexpr std::string_view module_defence             = "space_module_defence";       // Integrated Air Defence
inline constexpr std::string_view module_observatory         = "space_module_observatory";   // Scanned-Array Radar
inline constexpr std::string_view module_power               = "space_module_power";         // Photolithography
inline constexpr std::string_view module_habitation          = "space_module_habitation";    // Polymer Chemistry
inline constexpr std::string_view module_life_support        = "space_module_life_support";  // Desalination
inline constexpr std::string_view module_fuel_depot          = "space_module_fuel_depot";    // Cryogenic Storage
inline constexpr std::string_view module_dock                = "space_module_dock";          // Gantry Cranes
inline constexpr std::string_view module_cargo_hold          = "space_module_cargo_hold";    // Container Shipping
inline constexpr std::string_view module_microgravity_factory = "space_module_microgravity_factory"; // Industrial Robotics
inline constexpr std::string_view module_comms_relay         = "space_module_comms_relay";   // Packet Networks
inline constexpr std::string_view module_market              = "space_module_market";        // Electronic Settlement
inline constexpr std::string_view grid_7x5                   = "space_grid_7x5";             // Numerical Control

// ---- far space: the Early Space tree --------------------------------------
inline constexpr std::string_view prospecting                = "prospecting";                // Remote Prospecting
inline constexpr std::string_view base_founding              = "base_founding";              // Off-World Bases
inline constexpr std::string_view ice_extractor              = "ice_extractor";              // Ice Extraction
inline constexpr std::string_view electrolysis_plant         = "electrolysis_plant";         // Electrolysis Plants
inline constexpr std::string_view tanks_standard             = "space_tanks_standard";       // Storable Propellants
inline constexpr std::string_view drive_chemical             = "space_drive_chemical";       // Restartable Engines
inline constexpr std::string_view hull_hauler                = "space_hull_hauler";          // Space Haulers
inline constexpr std::string_view tanks_extended             = "space_tanks_extended";       // Extended Tankage
inline constexpr std::string_view hull_lander                = "space_hull_lander";          // Landers
inline constexpr std::string_view debris_arm                 = "space_debris_arm";           // Rendezvous and Capture
inline constexpr std::string_view drive_nuclear_thermal      = "space_drive_nuclear_thermal"; // Nuclear Thermal Rockets
inline constexpr std::string_view hull_tug                   = "space_hull_tug";             // Orbital Tugs
inline constexpr std::string_view armament_kinetic           = "space_armament_kinetic";     // Kinetic Interceptors
inline constexpr std::string_view armour_whipple             = "space_armour_whipple";       // Whipple Shielding
inline constexpr std::string_view hull_frigate               = "space_hull_frigate";         // Frigates
inline constexpr std::string_view armament_missile           = "space_armament_missile";     // Orbital Missiles
inline constexpr std::string_view armour_composite           = "space_armour_composite";     // Composite Hull Armour
inline constexpr std::string_view orbital_bombardment        = "orbital_bombardment";        // Orbital Bombardment
inline constexpr std::string_view orbital_defence_battery    = "orbital_defence_battery";    // Orbital Defence Battery
inline constexpr std::string_view module_shipyard            = "space_module_shipyard";      // Orbital Shipyards
inline constexpr std::string_view grid_9x7                   = "space_grid_9x7";             // Heavy Truss
inline constexpr std::string_view hull_cruiser               = "space_hull_cruiser";         // Cruisers

/// The Digitisation tree's space keys, in this file's order.
inline constexpr std::string_view near_space[] = {
    dispatch, role_comms, role_navigation, role_observation, role_survey, astronaut_training,
    station_core, module_research, module_defence, module_observatory, module_power,
    module_habitation, module_life_support, module_fuel_depot, module_dock, module_cargo_hold,
    module_microgravity_factory, module_comms_relay, module_market, grid_7x5,
};

/// The Early Space tree's keys, in this file's order.
inline constexpr std::string_view far_space[] = {
    prospecting, base_founding, ice_extractor, electrolysis_plant, tanks_standard, drive_chemical,
    hull_hauler, tanks_extended, hull_lander, debris_arm, drive_nuclear_thermal, hull_tug,
    armament_kinetic, armour_whipple, hull_frigate, armament_missile, armour_composite,
    orbital_bombardment, orbital_defence_battery, module_shipyard, grid_9x7, hull_cruiser,
};

} // namespace space_keys
