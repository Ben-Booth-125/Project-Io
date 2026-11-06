#pragma once

// ---------------------------------------------------------------------------
// BL-1430 — the sea theatre's authored numbers (MILITARY.md § Sea)
// ---------------------------------------------------------------------------
// Authored in scripts/economy.lua under `economy.military.sea` and read by
// recipe_registry.cpp into `military_capability_params::sea`. The C++ defaults
// below EQUAL the Lua values, so a hand-built harness registry sails exactly what
// the game sails. Its own header so recipe_registry.hpp can hold it without
// pulling sea_combat.hpp (and world.hpp) into every TU.
//
// UNITS. Attack is milli-HP a DAY at full strength. Detection and stealth compare
// as a share: a viewer's detection D finds a ship of stealth S at D / (D + S) a
// day. Range is steps over water from an own port. Pace is per-mille of a day per
// tile. A FIRST CUT; the sprint's closing reading tunes it.
// ---------------------------------------------------------------------------

#include "components.hpp" // ship_class, sea_mission, resource_count

#include <array>
#include <cstdint>

constexpr uint8_t sea_mission_bit(sea_mission m)
{
    return static_cast<uint8_t>(1u << static_cast<uint8_t>(m));
}

constexpr uint8_t ship_class_bit(ship_class c)
{
    return static_cast<uint8_t>(1u << static_cast<uint8_t>(c));
}

/// One ship class's numbers.
struct ship_class_stats
{
    int hp        = 100;  ///< HP at full strength.
    int attack    = 0;    ///< Milli-HP a day dealt across the hostile ships it has detected.
    int detection = 0;    ///< What it adds to its side's detection where it is present.
    int stealth   = 0;    ///< What a hostile side's detection is compared with.
    int range     = 20;   ///< Steps over water from an own port to a zone's nearest tile.
    int days_per_tile_permille = 500; ///< Pace: per-mille of a day to sail one tile.
    int break_hp_permille      = 300; ///< Below this HP share it breaks off home.
    int break_morale_permille  = 200; ///< Below this morale it breaks off home.
    int muster_days = 180; ///< Days to raise it.
    int crew        = 30;  ///< Sea recruits a hire spends from the nation's sea pool.
    float hire_cost = 0.0f; ///< Credits, from the treasury's uncommitted part.
    std::array<float, resource_count> hire_goods{}; ///< Bought off the shelf at hire.
    uint8_t missions = 0; ///< Bit (1 << sea_mission) per mission it may work.
    uint8_t targets  = 0; ///< Bit (1 << ship_class) per class it can hit: the target mask.
};

struct sea_params
{
    std::array<ship_class_stats, ship_class_count> cls{};

    /// A patrol's detection counts at this per-mille of its class's value.
    int patrol_detection_permille = 1500;
    /// Morale lost per per-mille of full HP lost, per-mille: 1000 loses them equally.
    int morale_per_hp_permille = 1000;
    /// Morale a ship regains a day lying at its home port.
    int dock_morale_per_day = 10;
    /// A broken ship sails again once it is back at or above both of these.
    int resume_hp_permille     = 700;
    int resume_morale_permille = 600;
    /// HP a ship at its home port regains a quarter, ppm of full, fully supplied.
    int refit_ppm_per_quarter = 200000;
    /// Goods drawn at the home port for a full quarter's refit.
    std::array<float, resource_count> refit_goods{};
    /// Goods one ship draws per economy tick at its home port.
    std::array<float, resource_count> ship_upkeep_goods{};
    /// Milli-HP one milli-hit of a tactical wing's soft + hard lands on a ship, per-mille.
    int bomb_hp_per_hit_permille = 10000;
};

/// The shipped defaults — the same numbers scripts/economy.lua authors.
inline sea_params default_sea_params()
{
    using rt = resource_type;
    const auto g = [](std::array<float, resource_count>& a, rt r, float q) {
        a[static_cast<std::size_t>(r)] = q;
    };
    sea_params p;
    const uint8_t pat = sea_mission_bit(sea_mission::patrol);
    const uint8_t esc = sea_mission_bit(sea_mission::escort);
    const uint8_t rai = sea_mission_bit(sea_mission::raid);
    const uint8_t blo = sea_mission_bit(sea_mission::blockade);
    const uint8_t t_sub = ship_class_bit(ship_class::submarine);
    const uint8_t t_des = ship_class_bit(ship_class::destroyer);
    const uint8_t t_cap = ship_class_bit(ship_class::capital);
    const uint8_t t_tra = ship_class_bit(ship_class::transport);

    ship_class_stats& s = p.cls[static_cast<std::size_t>(ship_class::submarine)];
    s.hp = 60; s.attack = 3000; s.detection = 20; s.stealth = 600; s.range = 40;
    s.days_per_tile_permille = 700; s.muster_days = 180; s.crew = 20; s.hire_cost = 1200.0f;
    g(s.hire_goods, rt::steel, 4.0f); g(s.hire_goods, rt::machinery, 3.0f); g(s.hire_goods, rt::electronics, 2.0f);
    s.missions = rai;
    s.targets  = static_cast<uint8_t>(t_cap | t_tra);

    ship_class_stats& d = p.cls[static_cast<std::size_t>(ship_class::destroyer)];
    d.hp = 80; d.attack = 2000; d.detection = 200; d.stealth = 50; d.range = 25;
    d.days_per_tile_permille = 400; d.muster_days = 180; d.crew = 30; d.hire_cost = 900.0f;
    g(d.hire_goods, rt::steel, 5.0f); g(d.hire_goods, rt::machinery, 3.0f); g(d.hire_goods, rt::electronics, 1.0f);
    d.missions = static_cast<uint8_t>(pat | esc | rai | blo);
    d.targets  = static_cast<uint8_t>(t_sub | t_des | t_cap | t_tra);

    ship_class_stats& c = p.cls[static_cast<std::size_t>(ship_class::capital)];
    c.hp = 200; c.attack = 6000; c.detection = 50; c.stealth = 30; c.range = 30;
    c.days_per_tile_permille = 500; c.muster_days = 360; c.crew = 100; c.hire_cost = 3500.0f;
    g(c.hire_goods, rt::steel, 14.0f); g(c.hire_goods, rt::alloys, 4.0f); g(c.hire_goods, rt::machinery, 6.0f);
    g(c.hire_goods, rt::electronics, 2.0f);
    c.missions = static_cast<uint8_t>(pat | esc | rai | blo);
    c.targets  = static_cast<uint8_t>(t_des | t_cap | t_tra);

    ship_class_stats& t = p.cls[static_cast<std::size_t>(ship_class::transport)];
    t.hp = 80; t.attack = 0; t.detection = 10; t.stealth = 30; t.range = 30;
    t.days_per_tile_permille = 600; t.muster_days = 135; t.crew = 20; t.hire_cost = 700.0f;
    g(t.hire_goods, rt::steel, 6.0f); g(t.hire_goods, rt::machinery, 2.0f);
    t.missions = 0;
    t.targets  = 0;

    g(p.refit_goods, rt::steel, 3.0f);
    g(p.refit_goods, rt::machinery, 1.0f);
    g(p.ship_upkeep_goods, rt::refined_fuel, 2.0f);
    return p;
}
