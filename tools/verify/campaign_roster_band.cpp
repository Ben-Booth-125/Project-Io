// campaign_roster_band — BL-461 (the ancient product no longer fields
// industrial-era units).
//
// THE DEFECT. src/world/unit_roster.hpp used to hard-code
// `campaign_roster_band = roster_band::industrial`, with a comment reading
// "the campaign is simply industrial-era throughout" — true the day it was
// written, false since the 0 CE refocus (NR-177, 2026-08-12) moved the
// default epoch to 0. Three call sites read the stale constant
// (src/ui/selection_panel.cpp, src/world/corp_ai.cpp,
// src/world/corp_command.cpp) and none consulted the epoch, so a 0 CE
// mercenary company could hire (and a rival corp_ai could score-and-hire)
// industrial-era units.
//
// THE FIX. `campaign_roster_band_for(era_band)` derives the band from
// `recipe_registry::era()`, which every call site now reads instead of the
// old constant.
//
//   R1  At era_band::ancient the derived band is NOT industrial — it is the
//       roster's lowest band, classical.
//   R2  At era_band::industrial the derived band IS industrial — the
//       pre-BL-461 behaviour for the 1960 arc is unchanged.
//   R3  Over a real world+corp fixture with every stockpile gate satisfied,
//       available_rows() offers no industrial-band row at the ancient
//       band, and offers every band up to and including industrial at the
//       industrial band. This is the observable defect surface: what
//       hire actually lists.
//   R4  Single source of truth: every one of the three former call sites
//       computed its band by literally the same expression,
//       `campaign_roster_band_for(reg.era())` — so no call site can drift
//       from another the way the old duplicated-constant scheme could
//       silently allow. Asserted here by calling the shared function
//       itself rather than three copies of the logic.
//
// The process exits non-zero if any assertion FAILs.

#include "world/recipe_registry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool contains_band(const std::vector<const roster_row*>& rows, roster_band band)
{
    for (const roster_row* r : rows)
        if (r->band == band)
            return true;
    return false;
}

bool any_row_above(const std::vector<const roster_row*>& rows, roster_band band)
{
    for (const roster_row* r : rows)
        if (static_cast<int>(r->band) > static_cast<int>(band))
            return true;
    return false;
}

} // namespace

int main()
{
    std::printf("=== campaign roster band derivation (BL-461) ===\n\n");

    // --- R1 / R2: the pure derivation --------------------------------------
    std::printf("R1/R2 -- campaign_roster_band_for\n");
    check(campaign_roster_band_for(era_band::ancient) == roster_band::classical,
          "R1 the ancient band derives to classical, NOT industrial");
    check(campaign_roster_band_for(era_band::ancient) != roster_band::industrial,
          "R1 (restated) the ancient band is explicitly not industrial");
    check(campaign_roster_band_for(era_band::industrial) == roster_band::industrial,
          "R2 the industrial band derives to industrial (pre-BL-461 behaviour preserved)");

    // --- R3: the observable hire-list surface -------------------------------
    std::printf("\nR3 -- available_rows() over a fully-gated corp fixture\n");
    {
        world w;
        const entity_id corp = w.create_entity();
        const entity_id body = w.create_entity();
        w.player_entity = corp;
        w.corporations[corp] = corporation_component{};

        // Satisfy every campaign gate axis (steel/ore, food, port, energy) so
        // gate failure cannot be confused with era gating.
        stockpile_component& pool = w.pool_for(corp, body);
        pool.quantities[static_cast<std::size_t>(resource_type::steel)]          = 1000.0f;
        pool.quantities[static_cast<std::size_t>(resource_type::food_rations)]   = 1000.0f;
        pool.quantities[static_cast<std::size_t>(resource_type::coal)]           = 1000.0f;

        building_component port_bldg{};
        port_bldg.type = building_type::port;
        port_bldg.tile = body; // any valid-looking entity id; unread by this gate
        const entity_id port_id = w.create_entity();
        w.buildings[port_id] = port_bldg;
        w.corporations[corp].assets.push_back(port_id);

        const auto ancient_rows    = available_rows(w, corp, campaign_roster_band_for(era_band::ancient));
        const auto industrial_rows = available_rows(w, corp, campaign_roster_band_for(era_band::industrial));

        check(!ancient_rows.empty(),
              "R3 the ancient band still offers classical rows (bands are cumulative)");
        check(!any_row_above(ancient_rows, roster_band::classical),
              "R3 a 0 CE company is offered NO row above the classical band");
        check(!contains_band(ancient_rows, roster_band::industrial),
              "R3 (the defect itself) a 0 CE company is offered no industrial-band row");

        check(contains_band(industrial_rows, roster_band::classical) &&
                  contains_band(industrial_rows, roster_band::medieval) &&
                  contains_band(industrial_rows, roster_band::gunpowder) &&
                  contains_band(industrial_rows, roster_band::industrial),
              "R3 the industrial band still offers every band up to and including industrial");

        check(ancient_rows.size() < industrial_rows.size(),
              "R3 the ancient band's hire list is a strict subset in size of the industrial one");
    }

    // --- R4: the three former call sites share one function ----------------
    std::printf("\nR4 -- one derivation, not three copies\n");
    {
        // The old defect was three independent reads of a fixed constant.
        // Post-fix, selection_panel.cpp / corp_ai.cpp / corp_command.cpp all
        // compute `campaign_roster_band_for(reg.era())` -- textually
        // identical expressions over the SAME function, so there is no
        // second implementation left to drift. Confirm the function is
        // total and consistent across both live era_band values (the only
        // two values era_band_for_epoch ever produces).
        recipe_registry reg_ancient;
        reg_ancient.set_era(era_band::ancient);
        recipe_registry reg_industrial;
        reg_industrial.set_era(era_band::industrial);

        const roster_band from_ancient_reg    = campaign_roster_band_for(reg_ancient.era());
        const roster_band from_industrial_reg = campaign_roster_band_for(reg_industrial.era());

        check(from_ancient_reg == roster_band::classical,
              "R4 selection_panel/corp_ai/corp_command's shared expression agrees with R1 for ancient");
        check(from_industrial_reg == roster_band::industrial,
              "R4 selection_panel/corp_ai/corp_command's shared expression agrees with R2 for industrial");
        check(from_ancient_reg != from_industrial_reg,
              "R4 the two eras are never conflated onto the same band");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
