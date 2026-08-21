// ---------------------------------------------------------------------------
// Tile axis split (BL-519) — no SDL / Lua
// ---------------------------------------------------------------------------
// BL-519 split `terrain_composition` into `terrain_substrate` x `terrain_cover`
// plus a graded `cover_density`, leaving `terrain_landform` untouched. The item
// is a REFACTOR WITH ONE NEW AXIS ON TOP, and every assertion here is shaped
// around that claim rather than around the code that implements it — because the
// way a split like this fails is not a crash, it is a silent re-tuning that
// nobody notices until a world looks wrong.
//
//   A1  THE DENSITY INVARIANT. `cover_density` is zero if and only if `cover` is
//       `none`. components.hpp states this as an invariant every producer and
//       consumer may rely on, so it is asserted over real generated worlds
//       rather than left as a comment.
//
//   A2  THE SHIPPED BUG IS FIXED. Paving a tile no longer destroys its geology.
//       Before the split, `maybe_transform_to_urban` overwrote the composition,
//       so a metallic tile that built up to a city STOPPED BEING METALLIC. Urban
//       is a cover now (Ben, 2026-08-21), so the substrate must survive the
//       transform. This is the one assertion that would have failed before the
//       item and passes after it.
//
//   A3  COMBAT IS REPRODUCED, NOT RE-TUNED. Every pre-split composition maps to
//       one (substrate, cover, density) triple, and terrain_defence /
//       terrain_attrition on that triple must return the pre-split number
//       EXACTLY. Eleven rows, hard-coded from the pre-BL-519 tables. If a later
//       change re-tunes cover, this is what says so.
//
//   A4  PLACEMENT IS REPRODUCED, NOT RE-TUNED. The same claim for BL-366's
//       non-extraction stack cap.
//
//   A5  THE NEW EXPRESSIVENESS IS REAL — the assertion that says the item did
//       something, not merely that it broke nothing. Ben's brief was "a mountain
//       might have a forest or not", and the pre-split model could not say
//       "rocky AND forested" because the one slot had to choose. So: a forested
//       rocky tile must carry BOTH timber (from the cover) and iron (from the
//       substrate), and must read differently from bare rock in combat.
//
//   A6  DETERMINISM. Two generations at one seed produce byte-identical axes.
//       The cover pass consumes no RNG stream and is a stateless fold; if that
//       ever stops being true, this is where it shows.
//
//   D   REPORTED, not asserted: the cover census over a seed sweep — how much of
//       the map carries each cover, and how many tiles are COMPOSITE (a cover on
//       ground that is not sedimentary, i.e. exactly the case the pre-split model
//       could not express). A number, not a threshold: nobody has chosen how much
//       of a world should be composite, and asserting an unchosen threshold would
//       be inventing a rule.
//
// Run: tile_axes_harness

#include "world/continents.hpp"
#include "world/placement_rules.hpp"
#include "world/planetology.hpp"
#include "world/terrain_combat.hpp"
#include "world/tile_generation.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr int gw = 180, gh = 84; // the shipped homeworld grid

int failures = 0;
int checks   = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

struct generated
{
    world                  w;
    std::vector<entity_id> ids;
};

/// One homeworld-shaped body: planetology -> continents -> tiles. Mirrors
/// tile_height_retention's minimal path for the same reason it does — every
/// assertion here is about one generation's internal consistency.
void generate_one(generated& g, uint32_t campaign_seed)
{
    const resolved_world rw = resolve_preferences(world_preferences{}, campaign_seed);
    body_inputs home = prototype_body(1);
    home.orbit_au = rw.home_orbit_au;

    const uint32_t body_seed = campaign_seed ^ prototype_body_seed(1);
    const planetology_state st = run_planetology(home, rw.params, body_seed);
    const continent_state   cs = run_continents(st, gw, gh, body_seed ^ 0xC0117E57u);

    const entity_id body = g.w.create_entity();
    g.ids = generate_body_tiles(g.w, body, gw, gh, st.profile,
                                campaign_seed ^ 0xE471001u, 1.0f, &st, nullptr,
                                &cs.height_bias, &cs.convergent);
}

const char* cover_name(terrain_cover c)
{
    switch (c)
    {
        case terrain_cover::none:   return "none";
        case terrain_cover::grass:  return "grass";
        case terrain_cover::scrub:  return "scrub";
        case terrain_cover::forest: return "forest";
        case terrain_cover::marsh:  return "marsh";
        case terrain_cover::snow:   return "snow";
        case terrain_cover::dunes:  return "dunes";
        case terrain_cover::ash:    return "ash";
        case terrain_cover::salt:   return "salt";
        case terrain_cover::urban:  return "urban";
    }
    return "?";
}

const char* substrate_name(terrain_substrate s)
{
    switch (s)
    {
        case terrain_substrate::barren:      return "barren";
        case terrain_substrate::rocky:       return "rocky";
        case terrain_substrate::sedimentary: return "sedimentary";
        case terrain_substrate::volcanic:    return "volcanic";
        case terrain_substrate::metallic:    return "metallic";
        case terrain_substrate::regolith:    return "regolith";
        case terrain_substrate::icy:         return "icy";
        case terrain_substrate::ocean:       return "ocean";
        case terrain_substrate::lake:        return "lake";  // BL-516
        case terrain_substrate::coast:       return "coast";
    }
    return "?";
}

/// The pre-split table, transcribed. Each row is one old `terrain_composition`,
/// the (substrate, cover, density) triple BL-519 decomposes it into, and the two
/// combat numbers the pre-split code returned for it on plains. These constants
/// are the CONTRACT: they came from the pre-BL-519 terrain_combat.cpp, and they
/// are what makes A3 a regression check rather than a restatement.
struct legacy_row
{
    const char*       name;
    terrain_substrate sub;
    terrain_cover     cov;
    std::uint8_t      den;
    int               defence;   // terrain_defence(.., plains)
    int               attrition; // terrain_attrition(.., plains)
    int               stack_cap; // non_extraction_stack_cap
};

constexpr legacy_row k_legacy[] = {
    { "grassland", terrain_substrate::sedimentary, terrain_cover::grass,  150,   0, 100,  6 },
    { "forest",    terrain_substrate::sedimentary, terrain_cover::forest, 205, 200, 250,  6 },
    { "wetland",   terrain_substrate::sedimentary, terrain_cover::marsh,  170, 250, 350,  6 },
    { "tundra",    terrain_substrate::sedimentary, terrain_cover::scrub,   75,  50, 500,  3 },
    { "barren",    terrain_substrate::barren,      terrain_cover::none,     0,   0, 800,  4 },
    { "rocky",     terrain_substrate::rocky,       terrain_cover::none,     0, 100, 400,  4 },
    { "volcanic",  terrain_substrate::volcanic,    terrain_cover::none,     0, 150, 700,  2 },
    { "icy",       terrain_substrate::icy,         terrain_cover::none,     0, 100, 850,  2 },
    { "regolith",  terrain_substrate::regolith,    terrain_cover::none,     0,   0, 900,  4 },
    { "metallic",  terrain_substrate::metallic,    terrain_cover::none,     0,   0, 800,  4 },
    { "ocean",     terrain_substrate::ocean,       terrain_cover::none,     0,   0,   0,  0 },
};

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

} // namespace

int main()
{
    std::printf("=== tile axis split (BL-519) ===\n");

    constexpr uint32_t seed = 20260821u;
    generated g;
    generate_one(g, seed);
    std::printf("  generated %zu tiles on a %dx%d homeworld at seed %u\n\n",
                g.ids.size(), gw, gh, seed);

    // -----------------------------------------------------------------------
    std::printf("A1 — the density invariant\n");
    {
        int violations = 0, worst_idx = -1;
        for (const entity_id id : g.ids)
        {
            if (id == null_entity) continue;
            const tile_component& t = g.w.tiles.at(id);
            const bool bare = (t.cover == terrain_cover::none);
            if (bare != (t.cover_density == 0))
            {
                ++violations;
                if (worst_idx < 0) worst_idx = static_cast<int>(id);
            }
        }
        check(violations == 0,
              "A1  cover_density == 0 if and only if cover == none, on every tile");
        if (violations)
            std::printf("        %d violations, first on tile %d\n", violations, worst_idx);
    }

    // -----------------------------------------------------------------------
    std::printf("\nA2 — the transform no longer destroys the geology\n");
    {
        // The exact case that was broken before BL-519: a METALLIC tile built up
        // to its cap. Pre-split, `maybe_transform_to_urban` wrote
        // composition = urban and the tile stopped being metallic — a slot
        // conflict that had already shipped.
        world w;
        const entity_id body = w.create_entity();
        const entity_id tile = w.create_entity();
        tile_component tc{};
        tc.body      = body;
        tc.substrate = terrain_substrate::metallic;
        tc.cover     = terrain_cover::none;
        tc.landform  = terrain_landform::plains;
        w.tiles[tile] = tc;

        const int cap = placement_rules::non_extraction_stack_cap(terrain_substrate::metallic,
                                                                 terrain_cover::none);
        for (int i = 0; i < cap; ++i)
        {
            const entity_id b = w.create_entity();
            building_component bc{};
            bc.tile = tile;
            bc.type = building_type::processing_facility;
            w.buildings[b] = bc;
        }

        const bool fired = placement_rules::maybe_transform_to_urban(w, tile);
        const tile_component& after = w.tiles.at(tile);
        check(fired, "A2a the transform fires once the stack fills its cap");
        check(after.cover == terrain_cover::urban, "A2b the tile is now urban (as a COVER)");
        check(after.substrate == terrain_substrate::metallic,
              "A2c THE SUBSTRATE SURVIVES — a city on metallic ground is still on metallic ground");
        check(after.cover_density == k_cover_density_max,
              "A2d paved ground is at full density, never sparsely paved");
        check(placement_rules::non_extraction_stack_cap(after.substrate, after.cover) == 12,
              "A2e the urban cap applies whatever ground the city stands on");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA3 — every pre-split combat value is reproduced exactly\n");
    {
        int bad = 0;
        for (const legacy_row& r : k_legacy)
        {
            const int d = terrain_defence(r.sub, r.cov, r.den, terrain_landform::plains);
            const int a = terrain_attrition(r.sub, r.cov, r.den, terrain_landform::plains);
            if (d != r.defence || a != r.attrition)
            {
                ++bad;
                std::printf("        %-10s defence %4d (want %4d)  attrition %4d (want %4d)\n",
                            r.name, d, r.defence, a, r.attrition);
            }
        }
        check(bad == 0,
              "A3  all 11 pre-split compositions reproduce their defence and attrition");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA4 — every pre-split stack cap is reproduced exactly\n");
    {
        int bad = 0;
        for (const legacy_row& r : k_legacy)
        {
            const int c = placement_rules::non_extraction_stack_cap(r.sub, r.cov);
            if (c != r.stack_cap)
            {
                ++bad;
                std::printf("        %-10s cap %d (want %d)\n", r.name, c, r.stack_cap);
            }
        }
        check(bad == 0, "A4  all 11 pre-split compositions reproduce their stack cap");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA5 — the pair the pre-split model could not name\n");
    {
        // Ben's brief, made testable: a ROCKY mountain that happens to be
        // forested. Pre-split this tile was not expressible at all — naming the
        // forest cost you the rock, and naming the rock cost you the forest.
        const int def_bare = terrain_defence(terrain_substrate::rocky, terrain_cover::none, 0,
                                             terrain_landform::mountain);
        const int def_wood = terrain_defence(terrain_substrate::rocky, terrain_cover::forest, 205,
                                             terrain_landform::mountain);
        const int att_bare = terrain_attrition(terrain_substrate::rocky, terrain_cover::none, 0,
                                               terrain_landform::mountain);
        const int att_wood = terrain_attrition(terrain_substrate::rocky, terrain_cover::forest, 205,
                                               terrain_landform::mountain);
        std::printf("        rocky mountain  bare: defence %4d attrition %4d\n", def_bare, att_bare);
        std::printf("        rocky mountain wood: defence %4d attrition %4d\n", def_wood, att_wood);
        check(att_wood < att_bare,
              "A5a a forested crag costs an army less than a bare one (the cover forages)");
        check(def_wood >= def_bare,
              "A5b ...and defends at least as well (the cover is cover)");

        // The deposit half, which is the economic payoff the item names: ore
        // follows the substrate, timber follows the cover, so a forested rocky
        // tile carries BOTH. Scanned over the real generated world rather than a
        // fixture, so it is a claim about worlds that exist.
        int composite = 0, both_goods = 0;
        for (const entity_id id : g.ids)
        {
            if (id == null_entity) continue;
            const tile_component& t = g.w.tiles.at(id);
            if (t.substrate == terrain_substrate::sedimentary
                || is_water(t.substrate)) // BL-516: every water kind, not just open sea
                continue;
            if (!is_biotic_cover(t.cover)) continue;
            ++composite;
            if (t.resource_deposit[ri(resource_type::timber)] > 0.0f
                && t.resource_deposit[ri(resource_type::iron_ore)] > 0.0f)
                ++both_goods;
        }
        std::printf("        %d composite tiles (biotic cover on non-sedimentary ground);"
                    " %d carry timber AND iron\n", composite, both_goods);
        check(composite > 0,
              "A5c the world actually contains composite tiles (the axis is not inert)");
        check(both_goods > 0,
              "A5d at least one carries BOTH timber and iron — impossible before the split");
    }

    // -----------------------------------------------------------------------
    std::printf("\nA6 — determinism\n");
    {
        generated h;
        generate_one(h, seed);
        int mismatched = 0;
        const std::size_t n = std::min(g.ids.size(), h.ids.size());
        for (std::size_t i = 0; i < n; ++i)
        {
            if (g.ids[i] == null_entity || h.ids[i] == null_entity) continue;
            const tile_component& a = g.w.tiles.at(g.ids[i]);
            const tile_component& b = h.w.tiles.at(h.ids[i]);
            if (a.substrate != b.substrate || a.cover != b.cover
                || a.cover_density != b.cover_density || a.landform != b.landform)
                ++mismatched;
        }
        check(g.ids.size() == h.ids.size() && mismatched == 0,
              "A6  two generations at one seed produce byte-identical axes");
    }

    // -----------------------------------------------------------------------
    // D — REPORTED, never asserted. Nobody has chosen how much of a world should
    // carry each cover, and a threshold invented here would be a rule this item
    // did not earn. The number is the point: it is what Ben reads to decide
    // whether Pass 4d is doing enough, and what a later change is compared to.
    // -----------------------------------------------------------------------
    std::printf("\nD — cover census (REPORTED, not asserted)\n");
    {
        constexpr int k_seeds = 6;
        std::map<int, int> cover_hist;   // cover -> tiles
        std::map<int, int> composite_by; // substrate -> composite tiles
        int land = 0, composite = 0, dressed = 0;

        for (int s = 0; s < k_seeds; ++s)
        {
            generated gs;
            generate_one(gs, seed + static_cast<uint32_t>(s) * 7919u);
            for (const entity_id id : gs.ids)
            {
                if (id == null_entity) continue;
                const tile_component& t = gs.w.tiles.at(id);
                if (is_water(t.substrate)) continue; // BL-516
                ++land;
                ++cover_hist[static_cast<int>(t.cover)];
                if (t.cover != terrain_cover::none) ++dressed;
                if (t.cover != terrain_cover::none
                    && t.substrate != terrain_substrate::sedimentary)
                {
                    ++composite;
                    ++composite_by[static_cast<int>(t.substrate)];
                }
            }
        }

        std::printf("        %d land tiles over %d seeds; %d dressed (%.2f%%), %d BARE (%.2f%%)\n",
                    land, k_seeds, dressed, land ? 100.0 * dressed / land : 0.0,
                    land - dressed, land ? 100.0 * (land - dressed) / land : 0.0);
        std::printf("        by cover:");
        for (const auto& [c, n] : cover_hist)
            std::printf(" %s=%d(%.2f%%)", cover_name(static_cast<terrain_cover>(c)), n,
                        land ? 100.0 * n / land : 0.0);
        std::printf("\n");
        std::printf("        COMPOSITE (cover on non-sedimentary ground) %d of %d land tiles"
                    " (%.2f%%) — the case the pre-split model could not express:",
                    composite, land, land ? 100.0 * composite / land : 0.0);
        for (const auto& [su, n] : composite_by)
            std::printf(" %s=%d", substrate_name(static_cast<terrain_substrate>(su)), n);
        std::printf("\n");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf("%s\n", failures == 0 ? "ALL PASS (0 failures)" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
