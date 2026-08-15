// era_roster — the BL-433 era gate over the REAL authored economy data.
//
// WHAT THIS EXISTS TO SETTLE. The live product is the ancient arc at 0 CE
// (NR-177), but scripts/economy.lua shipped a `launchpad` building type and
// scripts/recipes.lua shipped petroleum, propellant and spacecraft chains — the
// space arc's roster, parked at the backlog level but never parked in the DATA
// the ancient product loads. BL-433 gates that data on an era band.
//
// It loads the real scripts rather than hand-building a registry, for the same
// reason interbody_pull_harness does: the question is what the AUTHORED tags do.
// A hand-built fixture would assert only that the mechanism works on data the
// harness itself wrote, which is exactly how a mis-tagged entry would slip past.
//
// THE FOUR ROWS (requirements.json § era-gated-roster):
//
//   R1  The ancient band excludes every `industrial` entry — no Launchpad, none
//       of the petroleum/propellant/spacecraft recipes — and the industrial band
//       includes them all.
//   R2  LOAD-BEARING. Recipe ids are STABLE across bands. A recipe's id is its
//       index in m_recipes and that id is stored in building_component.recipe, so
//       the filter masks rather than removes; if it ever compacted instead, every
//       building whose recipe sat after a filtered one would silently repoint.
//       This row is the one that would catch that.
//   R3  DEFAULT-OFF. A registry whose band is never set sees the full authored
//       roster, so every pre-BL-433 harness is unaffected.
//   R4  An unknown era string is a load-time error naming the entry, not a silent
//       fallback to `any` — a typo must not quietly re-admit a space-era building.

#include "scripting/lua_state.hpp"
#include "world/recipe_registry.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++failures;
}

recipe_registry load_registry(lua_state& lua)
{
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    return reg;
}

/// Names browsable on processing_facility in the registry's current band.
std::vector<std::string> browsable(const recipe_registry& reg)
{
    std::vector<std::string> out;
    for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
        out.push_back(reg.recipe_at(building_type::processing_facility, i).name);
    return out;
}

bool contains(const std::vector<std::string>& v, const std::string& s)
{
    for (const std::string& x : v)
        if (x == s)
            return true;
    return false;
}

/// Recipes the ancient arc must not be offered. Named explicitly rather than
/// derived from the tags: deriving them from the same field under test would make
/// the assertion vacuous — it would pass whatever the data said.
const char* k_industrial_recipes[] = {
    "refined_fuel", "hydroponics_bay", "steel_from_iron_nickel",
    "propellant_atmospheric", "propellant_electrolysis",
    "silicon", "ree_alloy", "machinery", "alloys", "electronics",
    "spacecraft_components", "clean_water", "consumer_goods", "medical_supplies",
};

/// Chains that must survive into the ancient arc. Thin by design at this point —
/// filling the ancient roster out is BL-429, not this item.
const char* k_shared_recipes[] = { "steel", "food_rations", "refined_copper" };

} // namespace

int main()
{
    std::printf("=== era-gated economy roster (BL-433) ===\n\n");

    lua_state lua;
    recipe_registry reg = load_registry(lua);

    const std::size_t authored = reg.recipe_count();
    std::printf("authored recipes: %zu\n\n", authored);

    // --- R3: the default band is `any` and sees everything ---------------------
    std::printf("R3 — default band is band-agnostic\n");
    check(reg.era() == era_band::any, "a freshly loaded registry has band `any`");
    check(browsable(reg).size() == authored,
          "the default band browses every authored recipe (" +
              std::to_string(browsable(reg).size()) + " of " + std::to_string(authored) + ")");
    check(reg.building_available(building_type::launchpad),
          "the default band still offers the Launchpad (nothing is gated until a band is set)");

    // Absolute ids captured under the default band, to compare against every other.
    std::vector<uint16_t> ids_any;
    for (const char* n : k_shared_recipes)
        ids_any.push_back(reg.recipe_id(n));
    for (const char* n : k_industrial_recipes)
        ids_any.push_back(reg.recipe_id(n));

    // --- R1: the ancient band drops the industrial roster ----------------------
    std::printf("\nR1 — ancient band\n");
    reg.set_era(era_band::ancient);
    const std::vector<std::string> anc = browsable(reg);
    std::printf("  browsable in ancient: %zu of %zu authored\n", anc.size(), authored);

    check(!reg.building_available(building_type::launchpad),
          "the Launchpad is NOT available at 0 CE");
    check(reg.building_available(building_type::extraction_site) &&
              reg.building_available(building_type::processing_facility) &&
              reg.building_available(building_type::port),
          "the shared building types survive into the ancient band");

    bool any_industrial_offered = false;
    for (const char* n : k_industrial_recipes)
        if (contains(anc, n))
        {
            std::printf("      leaked into ancient: %s\n", n);
            any_industrial_offered = true;
        }
    check(!any_industrial_offered, "no industrial recipe is browsable in the ancient band");

    bool all_shared_offered = true;
    for (const char* n : k_shared_recipes)
        if (!contains(anc, n))
        {
            std::printf("      missing from ancient: %s\n", n);
            all_shared_offered = false;
        }
    check(all_shared_offered, "every shared recipe survives into the ancient band");

    // ANTI-VACUITY: if the tags were all `any`, or all `industrial`, the two checks
    // above could both pass on a degenerate roster. Pin the shape.
    check(anc.size() > 0 && anc.size() < authored,
          "the ancient roster is a PROPER subset — neither empty nor the whole file");

    // --- R1 (other half): the industrial band keeps everything -----------------
    std::printf("\nR1 — industrial band\n");
    reg.set_era(era_band::industrial);
    const std::vector<std::string> ind = browsable(reg);
    check(ind.size() == authored,
          "the industrial band browses every authored recipe (" + std::to_string(ind.size()) +
              " of " + std::to_string(authored) + ")");
    check(reg.building_available(building_type::launchpad),
          "the Launchpad is available in the industrial band");

    // --- R2: ids are absolute, and stable across every band --------------------
    // The load-bearing row. recipe_at() indexes the era's list; recipe_id()/
    // get_recipe() index the authored one. If the filter ever compacted m_recipes
    // instead of masking, these would diverge and every stored recipe id in a save
    // would repoint.
    std::printf("\nR2 — recipe ids are stable across bands\n");
    for (const era_band band : { era_band::ancient, era_band::industrial, era_band::any })
    {
        reg.set_era(band);
        std::size_t k = 0;
        bool stable = true;
        for (const char* n : k_shared_recipes)
            if (reg.recipe_id(n) != ids_any[k++])
                stable = false;
        for (const char* n : k_industrial_recipes)
            if (reg.recipe_id(n) != ids_any[k++])
                stable = false;
        check(stable, "every authored recipe keeps its absolute id under band " +
                          std::to_string(static_cast<int>(band)));
    }

    // A filtered-out recipe is still RESOLVABLE by its stored id — the case a save
    // written in one band and opened in another would hit.
    reg.set_era(era_band::ancient);
    const uint16_t fuel_id = reg.recipe_id("refined_fuel");
    const recipe*  fuel    = reg.get_recipe(fuel_id);
    check(fuel != nullptr && fuel->name == "refined_fuel",
          "a recipe filtered OUT of the ancient band still resolves by its stored id");
    check(!contains(browsable(reg), "refined_fuel"),
          "...while remaining absent from what the ancient band offers");

    // --- R4: an unknown era string is a load-time error ------------------------
    std::printf("\nR4 — an unknown era string throws\n");
    {
        lua_state bad;
        bad.load("scripts/recipes.lua");
        bad.load("scripts/economy.lua");
        // Corrupt one entry in place: a plausible typo, which is the real failure
        // mode — not an obviously absurd value.
        bad.state().safe_script("recipes[2].era = 'industial'");
        recipe_registry r2;
        bool threw = false;
        std::string msg;
        try { r2.load_from_lua(bad); }
        catch (const std::exception& e) { threw = true; msg = e.what(); }
        check(threw, "a misspelled era is rejected rather than silently treated as `any`");
        check(threw && msg.find("industial") != std::string::npos,
              "...and the error names the offending value (\"" + msg + "\")");
    }

    std::printf("\n=== %s (%d failure%s) ===\n",
                failures == 0 ? "ALL PASS" : "FAILURES",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
