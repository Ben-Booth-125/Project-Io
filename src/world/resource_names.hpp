#pragma once

#include "components.hpp"

#include <string>

// Headless-safe: only includes world/components.hpp, so every SDL- and
// Lua-free world/* consumer can include it and stay headlessly buildable.

/// The single canonical Lua-name <-> resource_type lookup (BL-414).
///
/// Before this, `recipe_registry.cpp` and `world_gen_config.cpp` each held an
/// independent, hand-authored `unordered_map<std::string, resource_type>`
/// literal that had to separately list and agree on every `resource_type`
/// enum value. They drifted: `world_gen_config.cpp`'s copy was missing the
/// BL-368 habitability tranche for one day (2026-08-11 to 2026-08-12),
/// which crashed a fresh session at startup with "Unknown resource
/// 'medical_supplies' in world_gen.kepler_market.base_price" — fixed at the
/// time as a one-off entry addition, not structurally. This header is the
/// structural fix: one table, one place to add a new `resource_type` value.
///
/// Covers the full enum, deliberately — see the two former call sites'
/// comments (now folded in here) for why a "prototype subset" table is
/// unsafe: nothing enforces that Lua content stays inside a subset.
namespace resource_names {

/// Look up the `resource_type` matching a Lua resource identifier (e.g.
/// "iron_ore" -> `resource_type::iron_ore`). Sets `ok` false and returns
/// `resource_type::iron_ore` (an arbitrary but harmless placeholder — callers
/// must check `ok` before trusting the value) for an unrecognised name.
///
/// Callers throw on `!ok` rather than silently falling back — an unknown
/// resource name in authored Lua content is a content bug that must be loud,
/// not a case to guess through.
resource_type resource_from_name(const std::string& name, bool& ok);

} // namespace resource_names
