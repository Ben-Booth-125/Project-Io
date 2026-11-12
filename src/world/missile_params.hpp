#pragma once

// ---------------------------------------------------------------------------
// BL-1431 — missiles' authored numbers (MILITARY.md § Missiles)
// ---------------------------------------------------------------------------
// A FIRST CUT (calibrated 2026-11-10). Held on `military_capability_params::missile`.
// These C++ defaults are the numbers; scripts/economy.lua does not author them yet
// (owed: a `economy.military.missiles` table read by recipe_registry.cpp). Its own
// header so recipe_registry.hpp can hold it without pulling missiles.hpp (and
// world.hpp) into every TU.
//
// UNITS. Damage is per-mille of what a missile that LANDS does: of one target unit's
// men and readiness, or of a building's condition. Interception is per-mille of a
// missile one interceptor stops. Range is straight-line tiles. Goods are what one
// missile (or interceptor) is assembled from, bought off the nation's shelves at
// posted prices when it is stocked.
// ---------------------------------------------------------------------------

#include "components.hpp" // resource_type, resource_count

#include <array>
#include <cstdint>

/// The missile kinds. APPEND-ONLY: carried in `corp_command::order` and saved as the
/// magazine's column order. `strategic_nuclear` is a NAME ONLY: no stock holds one and
/// no verb fires one (MILITARY.md § Missiles); `fire_missile` refuses it whole.
enum class missile_kind : uint8_t
{
    short_range       = 0,
    long_range        = 1,
    tactical_nuclear  = 2,
    strategic_nuclear = 3, ///< Never stocked, never fired.
};
/// The kinds a magazine holds and a verb may fire: the first three.
inline constexpr int missile_fireable_count = 3;

/// The most a nation's magazine holds of one kind, and an air defence unit of
/// interceptors. Bounds the save reader as well as the verbs.
inline constexpr int32_t k_missile_magazine_cap = 64;
inline constexpr int32_t k_interceptor_cap      = 24;

struct missile_kind_stats
{
    int range = 12;                    ///< Straight-line tiles from the launcher.
    int unit_men_permille = 0;         ///< Of one target unit's men, per missile landed.
    int unit_readiness_permille = 0;   ///< Of its readiness, per missile landed.
    int building_condition_permille = 0; ///< Off a building's condition, per missile landed.
    int intercept_permille = 0;        ///< What one interceptor stops of one missile.
    bool area = false;                 ///< Hits EVERY hostile unit and building in the province.
    std::array<float, resource_count> goods{}; ///< One missile's goods.
};

struct missile_params
{
    std::array<missile_kind_stats, missile_fireable_count> kind{};
    std::array<float, resource_count> interceptor_goods{}; ///< One interceptor's goods.
    int interceptors_per_missile = 2; ///< Doctrine: each incoming missile draws this many.
    int salvo_max      = 8;           ///< Missiles one fire verb may launch.
    int stock_per_verb = 16;          ///< Missiles or interceptors one stock verb may buy.
    int sight_tiles    = 4;           ///< A unit or ship sees provinces this near.
    /// BL-1643 (Ben, 2026-11-11): a SILO fires long-range and tactical nuclear missiles at
    /// this reach, whatever the kind's own range — twice a ship or bomber's long reach —
    /// and only while its condition stands ABOVE `silo_min_condition_permille` (half).
    int silo_range                  = 90;
    int silo_min_condition_permille = 500;
    /// The struck nation's Alarm counts each tactical warhead this many times; every
    /// other nation once (RELATIONS.md § 6: severe, above a declaration of war).
    float nuclear_struck_magnitude = 2.0f;
    /// What a rival scorer charges itself per point of Alarm a strike raises, credits.
    float rival_alarm_price = 100.0f;
    /// A rival fires only when the strike's value beats its missiles' cost by this.
    float rival_value_margin = 1.25f;
    /// A rival stocks toward these, per kind, while mobilised; and its air defence
    /// units toward `rival_interceptor_target` each.
    std::array<int, missile_fireable_count> rival_magazine_target{ { 4, 2, 1 } };
    int rival_interceptor_target = 6;
};

/// The shipped first cut.
inline missile_params default_missile_params()
{
    using rt = resource_type;
    const auto g = [](std::array<float, resource_count>& a, rt r, float q) {
        a[static_cast<std::size_t>(r)] = q;
    };
    missile_params p;

    missile_kind_stats& s = p.kind[static_cast<std::size_t>(missile_kind::short_range)];
    s.range = 12; s.unit_men_permille = 60; s.unit_readiness_permille = 120;
    s.building_condition_permille = 150; s.intercept_permille = 700;
    g(s.goods, rt::guided_weapons, 1.0f); g(s.goods, rt::ordnance, 1.0f);

    missile_kind_stats& l = p.kind[static_cast<std::size_t>(missile_kind::long_range)];
    l.range = 45; l.unit_men_permille = 80; l.unit_readiness_permille = 120;
    l.building_condition_permille = 300; l.intercept_permille = 250;
    g(l.goods, rt::guided_weapons, 3.0f); g(l.goods, rt::electronics, 2.0f);
    g(l.goods, rt::refined_fuel, 4.0f);

    missile_kind_stats& n = p.kind[static_cast<std::size_t>(missile_kind::tactical_nuclear)];
    n.range = 12; n.unit_men_permille = 600; n.unit_readiness_permille = 1000;
    n.building_condition_permille = 700; n.intercept_permille = 500; n.area = true;
    g(n.goods, rt::guided_weapons, 4.0f); g(n.goods, rt::electronics, 6.0f);
    g(n.goods, rt::machinery, 6.0f); g(n.goods, rt::alloys, 4.0f);

    g(p.interceptor_goods, rt::guided_weapons, 2.0f);
    g(p.interceptor_goods, rt::electronics, 3.0f);
    return p;
}
