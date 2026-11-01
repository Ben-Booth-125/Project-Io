#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// BL-1463 — recruits: the authored numbers (MILITARY.md § Recruits, Ben 2026-10-30)
// ---------------------------------------------------------------------------
// Authored in scripts/economy.lua under `economy.military.recruits` and read by
// recipe_registry.cpp into `military_capability_params::recruits`. The C++ defaults
// below EQUAL the Lua values, so a hand-built harness registry recruits what the game
// recruits. Its own header so recipe_registry.hpp can hold it without pulling
// recruits.hpp (and world.hpp) into every TU. The mechanism is recruits.hpp.
// ---------------------------------------------------------------------------

/// The cap is FIFTEEN QUARTERS of output (MILITARY.md § Recruits): a design constant,
/// not a tuning knob, so it is not authored.
inline constexpr int k_recruit_cap_quarters = 15;

/// The most a pool can ever hold: the save reader's ceiling (2^30, world_save.cpp), and
/// what every cap is clamped to, so a pool always reloads and never nears int32.
inline constexpr int32_t k_recruit_pool_max = 1 << 30;

/// The most recruits the loader admits per base per quarter (economy.military.recruits).
/// 20x the authored land rate; at it a nation reaches the pool ceiling only past 71,582
/// bases of one theatre (15 x 1000 x 71,583 > 2^30), where the cap is clamped anyway.
inline constexpr int k_recruit_rate_max = 1000;

struct recruit_params
{
    /// Land recruits one completed, running `military_base` makes a quarter. 50 is one
    /// hire's men (`hire_batch_manpower`): a base sustains one new unit a quarter, the
    /// muster queue's own cadence.
    int land_per_base = 50;
    /// Air recruits one completed, running `airbase` makes a quarter. 20 is a fighter
    /// wing's crew (`air_class_stats::crew`): an airbase sustains about one wing a quarter.
    int air_per_base = 20;
    /// Logistic Points one recruit costs when it refills a unit, paid at the unit's
    /// paying road as its supply is (LOGISTICS.md: one point admits one unit of cargo; a
    /// recruit is priced as one unit of it).
    float lp_per_recruit = 1.0f;
};
