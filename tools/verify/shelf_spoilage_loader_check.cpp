// shelf_spoilage_loader_check — BL-1179 (shelf spoilage): the economy.shelf_spoilage
// LOADER, on the real scripts/economy.lua.
//
// MARKETS.md § The shelf spoils: every good on a market shelf loses a fixed
// per-tick share, authored per good in data, durables slowest "but never zero".
// recipe_registry::load_from_lua validates each rate as the value that lands —
// a finite number in (0, 1] — and REJECTS otherwise, never clamps.
// market_inventory_harness (Lua-free) holds the drain arithmetic; this holds
// the loader, the way works_loader_check does for the works table.
//
//   S1  the shipped table loads, and EVERY resource carries a rate in (0, 1]
//       (the startup check unspoiled_priced_goods could then never fire)
//   S2  the shipped classes hold: perishable > consumable > durable > 0
//   S3  a zero is refused ("never zero")
//   S4  a negative, a rate above 1, NaN, an infinity and a string are refused
//   S5  an unknown good is refused
//   S6  an absent table loads with no good spoiling (the inert default)
//
// Build (needs a live Lua state):  bash tools/verify/build_lua_harness.sh shelf_spoilage_loader_check
// Run from the repo root (it reads scripts/recipes.lua and scripts/economy.lua).

#include "scripting/lua_state.hpp"
#include "world/recipe_registry.hpp"

#include <sol/sol.hpp>

#include <cstdio>
#include <exception>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

std::string load_with(const char* corrupt, recipe_registry& out, bool& threw)
{
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    if (corrupt != nullptr && corrupt[0] != '\0')
        lua.state().safe_script(corrupt);
    threw = false;
    try { out.load_from_lua(lua); }
    catch (const std::exception& e) { threw = true; return e.what(); }
    return {};
}

void refused(const char* corrupt, const char* needle, const std::string& what)
{
    recipe_registry r;
    bool threw = false;
    const std::string msg = load_with(corrupt, r, threw);
    check(threw && msg.find(needle) != std::string::npos,
          what + (threw ? "  (\"" + msg + "\")" : "  (did NOT throw)"));
}

float rate(const recipe_registry& r, resource_type t)
{
    return r.shelf_spoilage()[static_cast<std::size_t>(t)];
}

} // namespace

int main()
{
    std::printf("shelf_spoilage_loader_check — BL-1179 (shelf spoilage)\n");

    {
        recipe_registry r;
        bool threw = false;
        const std::string msg = load_with(nullptr, r, threw);
        check(!threw, "S1 the shipped economy.lua loads" + (threw ? "  (" + msg + ")" : std::string()));
        int missing = 0;
        for (std::size_t i = 0; i < resource_count; ++i)
            if (!(r.shelf_spoilage()[i] > 0.0f && r.shelf_spoilage()[i] <= 1.0f))
            {
                std::printf("    no rate: resource %zu\n", i);
                ++missing;
            }
        check(missing == 0, "S1 every resource carries a shelf spoilage rate in (0, 1]");

        const float per = rate(r, resource_type::food_rations);
        const float con = rate(r, resource_type::refined_fuel);
        const float dur = rate(r, resource_type::steel);
        std::printf("    perishable %.3f  consumable %.3f  durable %.3f\n", per, con, dur);
        check(per > con && con > dur && dur > 0.0f,
              "S2 perishable > consumable > durable > 0 (food_rations, refined_fuel, steel)");
        check(rate(r, resource_type::water) == per && rate(r, resource_type::clean_water) == per
                  && rate(r, resource_type::medical_supplies) == per
                  && rate(r, resource_type::agricultural_produce) == per,
              "S2 the five perishables named by the ruling share the perishable rate");
        check(rate(r, resource_type::iron_ore) == dur && rate(r, resource_type::alloys) == dur
                  && rate(r, resource_type::electronics) == dur,
              "S2 ores, alloys and components sit at the durable rate");
    }

    refused("economy.shelf_spoilage.steel = 0", "never spoils at zero",
            "S3 a zero rate is refused");
    refused("economy.shelf_spoilage.steel = -0.1", "is not a finite rate",
            "S4 a negative rate is refused");
    refused("economy.shelf_spoilage.steel = 1.5", "is not a finite rate",
            "S4 a rate above 1 is refused");
    refused("economy.shelf_spoilage.steel = 0/0", "is not a finite rate",
            "S4 NaN is refused");
    refused("economy.shelf_spoilage.steel = math.huge", "is not a finite rate",
            "S4 an infinity is refused");
    refused("economy.shelf_spoilage.steel = '0.02'", "is not a number",
            "S4 a string is refused");
    refused("economy.shelf_spoilage.steal = 0.02", "Unknown resource 'steal'",
            "S5 an unknown good is refused");

    {
        recipe_registry r;
        bool threw = false;
        load_with("economy.shelf_spoilage = nil", r, threw);
        bool all_zero = true;
        for (std::size_t i = 0; i < resource_count; ++i)
            if (r.shelf_spoilage()[i] != 0.0f) all_zero = false;
        check(!threw && all_zero, "S6 an absent table loads and no good spoils");
    }

    std::printf(failures == 0 ? "\nALL PASS\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
