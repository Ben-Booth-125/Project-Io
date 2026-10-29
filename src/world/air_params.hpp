#pragma once

// ---------------------------------------------------------------------------
// BL-1449 — the air theatre's authored numbers (MILITARY.md § Air)
// ---------------------------------------------------------------------------
// Authored in scripts/economy.lua under `economy.military.air` and read by
// recipe_registry.cpp into `military_capability_params::air`. The C++ defaults
// below EQUAL the Lua values, so a hand-built harness registry flies exactly what
// the game flies. Its own header so recipe_registry.hpp can hold it without
// pulling air_combat.hpp (and world.hpp) into every TU.
//
// UNITS. Air-to-air and air-defence damage are milli-HP a DAY at full strength;
// ground attack (soft / hard) is milli-hits a day on the land round's scale
// (campaign_battle_params: hp_share, hp_cost, readiness_cost convert it to men
// and readiness); strategic attack is building condition per-mille a day; range
// is straight-line tiles. A FIRST CUT, logged in BL-1449's report.
// ---------------------------------------------------------------------------

#include "components.hpp" // air_class, air_mission, resource_count

#include <array>
#include <cstdint>

/// One air class's numbers.
struct air_class_stats
{
    int hp        = 100;  ///< HP at full strength.
    int air       = 0;    ///< Air attack: milli-HP a day dealt to hostile wings.
    int superiority_permille = 1000; ///< The share of `air` that counts flying superiority.
    int soft      = 0;    ///< Milli-hits a day on a target's soft share.
    int hard      = 0;    ///< Milli-hits a day on its hard share.
    int strategic = 0;    ///< Condition per-mille a day off a hostile building.
    int recon     = 0;    ///< Recon power: per-mille added to its side's hits, before the cap.
    int range     = 10;   ///< Straight-line tiles from the airbase to a province's nearest tile.
    int ad_vulnerability_permille = 1000; ///< Scales the air-defence fire it takes.
    bool has_morale = true; ///< A drone has no pilot, so no morale.
    int muster_days = 90; ///< Days to raise it.
    float hire_cost = 0.0f; ///< Credits, from the treasury's uncommitted part.
    std::array<float, resource_count> hire_goods{}; ///< Bought off the shelf at hire.
    uint8_t missions = 0; ///< Bit (1 << air_mission) per mission it may fly.
};

constexpr uint8_t air_mission_bit(air_mission m)
{
    return static_cast<uint8_t>(1u << static_cast<uint8_t>(m));
}

struct air_params
{
    std::array<air_class_stats, air_class_count> cls{};

    /// At share 0 a side's bombing lands at (1000 - this) per-mille.
    int bombing_penalty_permille = 700;
    /// At share 0 a march step costs (1000 + this) per-mille of its price.
    int march_slow_permille = 500;
    /// A nation reveals a hostile nation's units in a province when its share
    /// against it is ABOVE this.
    int reveal_share_permille = 500;
    /// The recon bonus to a side's hits in a province never exceeds this.
    int recon_cap_permille = 500;
    /// Milli-HP a day one air defence unit at full strength deals each hostile
    /// wing over its province (before the wing's vulnerability and its share of
    /// the day spent there).
    int ad_attack = 1000;
    /// Condition per-mille a damaged building regains a day, fully supplied.
    int repair_permille_per_day = 4;
    /// Goods drawn per per-mille of condition repaired.
    std::array<float, resource_count> repair_goods_per_permille{};
    /// Goods one wing draws per economy tick at its airbase's tile.
    std::array<float, resource_count> wing_upkeep_goods{};
};

/// The shipped defaults — the same numbers scripts/economy.lua authors.
inline air_params default_air_params()
{
    using rt = resource_type;
    const auto g = [](std::array<float, resource_count>& a, rt r, float q) {
        a[static_cast<std::size_t>(r)] = q;
    };
    air_params p;
    const uint8_t sup = air_mission_bit(air_mission::superiority);
    const uint8_t tac = air_mission_bit(air_mission::tactical);
    const uint8_t str = air_mission_bit(air_mission::strategic);
    const uint8_t rec = air_mission_bit(air_mission::recon);

    air_class_stats& f = p.cls[static_cast<std::size_t>(air_class::fighter)];
    f.hp = 100; f.air = 2000; f.superiority_permille = 1000; f.range = 15;
    f.ad_vulnerability_permille = 700; f.muster_days = 135; f.hire_cost = 900.0f;
    g(f.hire_goods, rt::alloys, 4.0f); g(f.hire_goods, rt::electronics, 3.0f);
    g(f.hire_goods, rt::machinery, 2.0f); g(f.hire_goods, rt::refined_fuel, 2.0f);
    f.missions = sup;

    air_class_stats& m = p.cls[static_cast<std::size_t>(air_class::multirole)];
    m.hp = 100; m.air = 1500; m.superiority_permille = 600; m.soft = 80; m.hard = 60;
    m.range = 12; m.ad_vulnerability_permille = 1000; m.muster_days = 135; m.hire_cost = 1100.0f;
    g(m.hire_goods, rt::alloys, 4.0f); g(m.hire_goods, rt::electronics, 3.0f);
    g(m.hire_goods, rt::machinery, 2.0f); g(m.hire_goods, rt::ordnance, 2.0f);
    m.missions = static_cast<uint8_t>(tac | sup);

    air_class_stats& b = p.cls[static_cast<std::size_t>(air_class::bomber)];
    b.hp = 140; b.strategic = 12; b.superiority_permille = 0; b.range = 40;
    b.ad_vulnerability_permille = 1600; b.muster_days = 180; b.hire_cost = 1600.0f;
    g(b.hire_goods, rt::alloys, 6.0f); g(b.hire_goods, rt::electronics, 3.0f);
    g(b.hire_goods, rt::machinery, 3.0f); g(b.hire_goods, rt::ordnance, 3.0f);
    b.missions = str;

    air_class_stats& h = p.cls[static_cast<std::size_t>(air_class::helicopter)];
    h.hp = 60; h.soft = 50; h.hard = 120; h.superiority_permille = 0; h.range = 6;
    h.ad_vulnerability_permille = 1300; h.muster_days = 90; h.hire_cost = 500.0f;
    g(h.hire_goods, rt::alloys, 2.0f); g(h.hire_goods, rt::machinery, 2.0f);
    g(h.hire_goods, rt::electronics, 1.0f);
    h.missions = tac;

    air_class_stats& d = p.cls[static_cast<std::size_t>(air_class::drone)];
    d.hp = 40; d.soft = 40; d.hard = 40; d.recon = 150; d.superiority_permille = 0; d.range = 10;
    d.ad_vulnerability_permille = 1200; d.has_morale = false; d.muster_days = 90; d.hire_cost = 400.0f;
    g(d.hire_goods, rt::electronics, 2.0f); g(d.hire_goods, rt::alloys, 1.0f);
    d.missions = static_cast<uint8_t>(tac | rec);

    air_class_stats& r = p.cls[static_cast<std::size_t>(air_class::recon)];
    r.hp = 80; r.recon = 300; r.superiority_permille = 0; r.range = 20;
    r.ad_vulnerability_permille = 800; r.muster_days = 135; r.hire_cost = 800.0f;
    g(r.hire_goods, rt::alloys, 3.0f); g(r.hire_goods, rt::electronics, 4.0f);
    r.missions = rec;

    g(p.repair_goods_per_permille, rt::steel, 0.05f);
    g(p.repair_goods_per_permille, rt::machinery, 0.02f);
    g(p.wing_upkeep_goods, rt::refined_fuel, 1.0f);
    return p;
}
