// ---------------------------------------------------------------------------
// Headless Planetology harness (BL-167; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises the ten-stage chain in src/world/planetology.cpp and the endowment
// it hands the tile pass. See docs/generation/PLANETOLOGY.md.
//
//   R1  DETERMINISM. The chain is a pure function of (seed, inputs, params):
//       the same inputs produce an identical result, and a different seed
//       produces a different one.
//
//   R2  ARCHETYPES. The four prototype bodies land in their stated archetypes
//       and derive the solar parameters the tile pipeline expects. Kepler must
//       come out a living Cradle with liquid water — the game opens on its
//       surface, so a derivation that dries it out breaks the campaign.
//
//   R3  THE B -> C JOINT. No life, no coal. A world whose biosphere never
//       reached land has ZERO coal, peat and timber; a sterile world has no
//       biogenic resource of any kind. This is the claim the whole feature
//       rests on, so it is asserted rather than assumed.
//
//   R4  THE IRON/COAL ANTAGONISM. The oxygenation dial trades iron against coal
//       in OPPOSITE directions, with every gate still passed. This is the
//       homeworld's main axis of variation; if it collapses, the corridor is
//       too narrow and the homeworld reads the same every campaign.
//
//   R5  HOMEWORLD CORRIDOR. Across the full knob space the homeworld always
//       reaches a land biosphere and always retains liquid water — the
//       guarantee is on the INPUTS, so no knob combination may break it.
//
// HONEST SCOPE NOTE: this validates the deterministic skeleton and the gate
// ordering. The model has more free parameters (~40 tuning constants) than it
// has real calibration bodies, so a green run means the constants have not
// MOVED, not that they are right. Do not read it as a physics validation.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/planetology.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <string>

namespace {

int g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

// Short display name, so the report reads without pulling in the UI layer.
const char* ui_name(resource_type r)
{
    switch (r) { case resource_type::tobacco: return "tobacco"; case resource_type::spices: return "spices";
                 case resource_type::coffee:  return "coffee";  case resource_type::furs:   return "furs";
                 default: return "?"; }
}

float dep(const planetology_state& s, resource_type r)
{
    return s.endowment[static_cast<std::size_t>(r)];
}

// The four prototype bodies, matching hard_coded_world.cpp's authored inputs.
body_inputs cinder() { return body_inputs{ "Cinder", 0.055f, 0.39f, 0.0f, 0.0f, 0.0f, false, false, 0.0f }; }
body_inputs kepler() { body_inputs b{}; b.name="Kepler"; b.mass_earths=1.00f; b.orbit_au=1.00f; b.is_homeworld=true; return b; }
body_inputs selene() { body_inputs b{}; b.name="Selene"; b.mass_earths=0.0123f; b.orbit_au=1.00f;
                       b.parent_mass_earths=1.00f; b.parent_orbit_au=0.00257f; b.eccentricity=0.055f; return b; }
body_inputs pallas() { body_inputs b{}; b.name="Pallas"; b.mass_earths=0.000034f; b.orbit_au=3.05f;
                       b.core_fragment=true; return b; }

bool same(const planetology_state& a, const planetology_state& b)
{
    if (a.archetype != b.archetype || a.stage != b.stage || a.peak != b.peak) return false;
    if (a.o2_fraction != b.o2_fraction || a.ferruginous_gyr != b.ferruginous_gyr) return false;
    if (a.profile.temperature != b.profile.temperature) return false;
    if (a.profile.water_fraction != b.profile.water_fraction) return false;
    if (a.history.size() != b.history.size()) return false;
    for (std::size_t i = 0; i < resource_count; ++i)
        if (a.endowment[i] != b.endowment[i]) return false;
    return true;
}

void dump(const char* label, const planetology_state& s)
{
    std::printf("\n  --- %s : %s (%s) ---\n", label, archetype_name(s.archetype),
                life_stage_name(s.stage));
    std::printf("      v_esc %.2f km/s | S %.3f | shore %.3f | T %.0f K | O2 %.1f%% | theta %.2f\n",
                static_cast<double>(s.v_esc_kms), static_cast<double>(s.instellation),
                static_cast<double>(s.shore), static_cast<double>(s.surface_temp_k),
                static_cast<double>(s.o2_fraction * 100.0f), static_cast<double>(s.theta));
    std::printf("      profile: temp=%d atm=%d hyd=%d geo=%d water=%.2f bias=%d\n",
                static_cast<int>(s.profile.temperature), static_cast<int>(s.profile.atmosphere),
                static_cast<int>(s.profile.hydrology),   static_cast<int>(s.profile.geology),
                static_cast<double>(s.profile.water_fraction), static_cast<int>(s.profile.bias));
    std::printf("      iron %.2f  coal %.2f  oil %.2f  copper %.2f  timber %.2f  water %.2f\n",
                static_cast<double>(dep(s, resource_type::iron_ore)),
                static_cast<double>(dep(s, resource_type::coal)),
                static_cast<double>(dep(s, resource_type::petroleum)),
                static_cast<double>(dep(s, resource_type::copper_ore)),
                static_cast<double>(dep(s, resource_type::timber)),
                static_cast<double>(dep(s, resource_type::water)));
    for (const history_event& h : s.history)
        std::printf("      %5.2f Gya  %-52s %s\n", static_cast<double>(h.gya),
                    h.event.c_str(), h.consequence.c_str());
}

} // namespace

int main()
{
    const planetology_params def{};

    std::printf("=== Planetology harness (BL-167) ===\n");

    const planetology_state c = run_planetology(cinder(), def, 0xC1D0001u);
    const planetology_state k = run_planetology(kepler(), def, 0xE471001u);
    const planetology_state s = run_planetology(selene(), def, 0x5E1E001u);
    const planetology_state p = run_planetology(pallas(), def, 0x9A11A5u);

    dump("Cinder", c);
    dump("Kepler", k);
    dump("Selene", s);
    dump("Pallas", p);
    std::printf("\n");

    // --- R1 determinism ---
    check(same(k, run_planetology(kepler(), def, 0xE471001u)), "R1 same seed -> identical result");
    check(!same(k, run_planetology(kepler(), def, 0xE471002u)), "R1 different seed -> different result");

    // --- R2 archetypes + derived profile ---
    check(k.archetype == body_archetype::cradle,        "R2 Kepler is a Cradle");
    check(k.profile.hydrology == hydrological_state::liquid, "R2 Kepler keeps liquid water (campaign depends on it)");
    check(k.profile.temperature == temperature_class::temperate, "R2 Kepler derives temperate");
    check(k.profile.atmosphere >= atmosphere_class::moderate,    "R2 Kepler derives a real atmosphere");
    check(k.stage == life_stage::civilised,             "R2 Kepler reaches civilisation");

    check(c.archetype == body_archetype::dead_rock,     "R2 Cinder is a Dead Rock");
    check(c.profile.temperature == temperature_class::scorching, "R2 Cinder derives scorching");
    check(c.profile.atmosphere == atmosphere_class::none,        "R2 Cinder derives airless");

    check(s.archetype == body_archetype::dead_rock,     "R2 Selene is a Dead Rock");
    check(s.profile.atmosphere == atmosphere_class::none,        "R2 Selene derives airless");
    check(s.profile.temperature == temperature_class::cold,      "R2 Selene derives cold");

    check(p.archetype == body_archetype::core_fragment, "R2 Pallas is a Core Fragment");
    check(p.profile.bias == composition_bias::metallic, "R2 Pallas derives a metallic bias");

    // --- R3 the B -> C joint: no life, no coal ---
    check(dep(c, resource_type::coal)      == 0.0f, "R3 sterile Cinder has zero coal");
    check(dep(c, resource_type::petroleum) == 0.0f, "R3 sterile Cinder has zero petroleum");
    check(dep(c, resource_type::timber)    == 0.0f, "R3 sterile Cinder has zero timber");
    check(dep(s, resource_type::clay)      == 0.0f, "R3 airless dry Selene has zero clay (aqueous alteration)");
    check(dep(s, resource_type::water)      > 0.0f, "R3 Selene still has polar cold-trap water");
    check(dep(k, resource_type::coal)       > 0.0f, "R3 living Kepler has coal");
    check(dep(k, resource_type::iron_ore)   > 0.0f, "R3 living Kepler has iron");

    // A wet world too small for a mobile lid: it oxygenates, never gets the
    // tectonic nutrient shock the second oxygenation needs, and stalls below the
    // ozone threshold. Marine life, no land life — so petroleum is possible and
    // coal is categorically impossible. This is the B -> C joint's sharpest edge.
    {
        body_inputs small{};
        small.name        = "Stunted";
        small.mass_earths = 0.35f;
        small.orbit_au    = 1.00f;
        const planetology_state m = run_planetology(small, def, 0x1234u);
        dump("Stunted (small wet world)", m);
        check(m.peak < life_stage::land,              "R3 a small wet world never reaches land");
        check(dep(m, resource_type::coal)     == 0.0f, "R3 no-land world has zero coal");
        check(dep(m, resource_type::timber)   == 0.0f, "R3 no-land world has zero timber");
        check(dep(m, resource_type::iron_ore)  > 0.0f, "R3 no-land world still has banded iron");
    }

    // --- R4 the iron/coal antagonism ---
    {
        planetology_params early = def; early.oxygenation = 0.05f;
        planetology_params late  = def; late.oxygenation  = 0.95f;
        const planetology_state e = run_planetology(kepler(), early, 0xE471001u);
        const planetology_state l = run_planetology(kepler(), late,  0xE471001u);

        std::printf("\n  oxygenation 0.05 -> iron %.2f coal %.2f  (window %.2f Gyr)\n",
                    static_cast<double>(dep(e, resource_type::iron_ore)),
                    static_cast<double>(dep(e, resource_type::coal)),
                    static_cast<double>(e.ferruginous_gyr));
        std::printf("  oxygenation 0.95 -> iron %.2f coal %.2f  (window %.2f Gyr)\n\n",
                    static_cast<double>(dep(l, resource_type::iron_ore)),
                    static_cast<double>(dep(l, resource_type::coal)),
                    static_cast<double>(l.ferruginous_gyr));

        // The claim is that the endowment swings WITHIN the passing corridor, so
        // both ends must still be Cradles. Comparing a Cradle against a world
        // that died at a gate would prove nothing.
        check(e.archetype == body_archetype::cradle && l.archetype == body_archetype::cradle,
              "R4 both ends of the dial still pass every gate");
        check(l.ferruginous_gyr > e.ferruginous_gyr,
              "R4 oxygenating late gives a longer banded-iron window");
        check(dep(l, resource_type::iron_ore) > dep(e, resource_type::iron_ore),
              "R4 oxygenating late gives MORE iron");
        check(dep(l, resource_type::coal) < dep(e, resource_type::coal),
              "R4 oxygenating late gives LESS coal (the antagonism holds)");
    }

    // --- R5 the homeworld corridor holds across the whole knob space ---
    {
        int breaks = 0, tried = 0;
        for (int a = 0; a <= 4; ++a)
        for (int o = 0; o <= 4; ++o)
        for (int w = 0; w <= 3; ++w)
        {
            planetology_params q{};
            q.system_age_gyr = 3.5f + static_cast<float>(a) * 1.2f;
            q.oxygenation    = static_cast<float>(o) * 0.25f;
            q.home_ocean     = 0.35f + static_cast<float>(w) * 0.15f;
            const planetology_state h = run_planetology(kepler(), q, 0xE471001u ^ static_cast<uint32_t>(a * 31 + o * 7 + w));
            ++tried;
            if (h.profile.hydrology != hydrological_state::liquid || h.peak < life_stage::land)
            {
                ++breaks;
                std::printf("      corridor break: age %.1f oxy %.2f ocean %.2f -> %s (%s)\n",
                            static_cast<double>(q.system_age_gyr), static_cast<double>(q.oxygenation),
                            static_cast<double>(q.home_ocean), archetype_name(h.archetype),
                            life_stage_name(h.peak));
            }
        }
        std::printf("      corridor: %d/%d knob combinations reached a land biosphere\n",
                    tried - breaks, tried);
        check(breaks == 0, "R5 homeworld reaches a land biosphere across the whole knob space");
    }

    // --- R6 wizard stability -------------------------------------------------
    // The New World wizard walks the stages in order and takes one decision at
    // each. That only works if a decision made at stage N never rewrites the
    // history of a stage before N — otherwise walking Back would show the player
    // a different past than the one they just decided against.
    //
    // Each knob is asserted against the stage it is decided at.
    {
        struct knob
        {
            const char* name;
            chain_stage decided_at;
            float planetology_params::*field;
            float alt;
        };
        const knob knobs[] = {
            { "home_mass",        chain_stage::accretion, &planetology_params::home_mass,        1.40f },
            { "radiogenic",       chain_stage::engine,    &planetology_params::radiogenic,       0.55f },
            { "home_ocean",       chain_stage::water,     &planetology_params::home_ocean,       0.42f },
            { "abiogenesis_ease", chain_stage::spark,     &planetology_params::abiogenesis_ease, 0.30f },
            { "oxygenation",      chain_stage::breath,    &planetology_params::oxygenation,      0.90f },
            { "coal_climate",     chain_stage::green,     &planetology_params::coal_climate,     0.05f },
            { "drawdown",         chain_stage::spend,     &planetology_params::drawdown,         0.10f },
        };

        for (const knob& kn : knobs)
        {
            planetology_params alt = def;
            alt.*(kn.field) = kn.alt;

            const planetology_state base = run_planetology(kepler(), def, 0xE471001u);
            const planetology_state mod  = run_planetology(kepler(), alt, 0xE471001u);

            bool stable = true;
            for (const history_event& h : base.history)
            {
                if (h.stage >= kn.decided_at)
                    continue;
                // Find the same earlier-stage line in the modified run.
                bool found = false;
                for (const history_event& m : mod.history)
                    if (m.stage == h.stage && m.event == h.event &&
                        m.consequence == h.consequence && m.gya == h.gya)
                    { found = true; break; }
                if (!found) { stable = false; break; }
            }
            char what[128];
            std::snprintf(what, sizeof what,
                          "R6 '%s' leaves every stage before %s untouched",
                          kn.name, chain_stage_name(kn.decided_at));
            check(stable, what);
        }
    }

    // --- R7 the new decision points actually do something --------------------
    // A knob that changes nothing is worse than no knob: it teaches the player
    // the screen is decorative. Each is asserted to move its own outcome.
    {
        planetology_params heavy = def; heavy.home_mass = 1.45f;
        planetology_params cold  = def; cold.radiogenic = 0.42f;
        planetology_params dead  = def; dead.abiogenesis_ease = 0.0f;
        planetology_params dry   = def; dry.coal_climate = 0.0f;
        planetology_params wet   = def; wet.coal_climate = 1.0f;

        check(run_planetology(kepler(), heavy, 0xE471001u).v_esc_kms >
              run_planetology(kepler(), def, 0xE471001u).v_esc_kms,
              "R7 a heavier homeworld has a higher escape velocity");
        check(run_planetology(kepler(), cold, 0xE471001u).theta <
              run_planetology(kepler(), def, 0xE471001u).theta,
              "R7 a radiogenically poor system runs a colder interior");
        check(run_planetology(kepler(), dead, 0xE471001u).peak == life_stage::prebiotic,
              "R7 abiogenesis_ease 0 leaves the homeworld lifeless");
        check(dep(run_planetology(kepler(), wet, 0xE471001u), resource_type::coal) >
              dep(run_planetology(kepler(), dry, 0xE471001u), resource_type::coal),
              "R7 everwet basins yield more coal than seasonal ones");
    }

    // --- R8 preferences, reject-and-reroll ------------------------------------
    // The wizard expresses preferences and the seed picks within them; misses are
    // rerolled rather than clamped. Three properties have to hold or that is not
    // a generator, it is a slot machine:
    //   * resolution is a pure function of (preferences, seed);
    //   * bumping a round's reroll counter really does produce a different world;
    //   * whatever comes back has ALREADY passed the strict floor, so the player
    //     never sees a homeworld the rules say cannot exist.
    {
        world_preferences pref;                      // every axis `any`
        const resolved_world a = resolve_preferences(pref, 0x1234ABCDu);
        const resolved_world b = resolve_preferences(pref, 0x1234ABCDu);
        check(a.params.star_mass == b.params.star_mass &&
              a.params.home_mass == b.params.home_mass &&
              a.home_orbit_au    == b.home_orbit_au &&
              a.attempts         == b.attempts,
              "R8 resolve_preferences is a pure function of (preferences, seed)");

        const resolved_world c = resolve_preferences(pref, 0x1234ABCEu);
        check(a.params.star_mass != c.params.star_mass ||
              a.home_orbit_au    != c.home_orbit_au,
              "R8 a different seed resolves to a different world");

        world_preferences rolled = pref;
        rolled.roll[0] = 1;
        const resolved_world d = resolve_preferences(rolled, 0x1234ABCDu);
        check(a.params.star_mass != d.params.star_mass ||
              a.home_orbit_au    != d.home_orbit_au,
              "R8 rerolling round A redraws the world");

        world_preferences rolled_c = pref;
        rolled_c.roll[2] = 1;
        const resolved_world e = resolve_preferences(rolled_c, 0x1234ABCDu);
        check(e.params.star_mass == a.params.star_mass,
              "R8 rerolling round C leaves round A's draw alone");

        // Every resolved homeworld must clear the floor, across many seeds and
        // across every preference the UI can express.
        int bad = 0, worst = 0, gave_up = 0;
        for (uint32_t s = 0; s < 400; ++s)
        {
            world_preferences p2;
            p2.star         = static_cast<lean>(s % 4);
            p2.interior     = static_cast<lean>((s / 4) % 4);
            p2.oxygen_story = static_cast<lean>((s / 16) % 4);
            p2.ocean        = static_cast<lean>((s / 64) % 4);
            const resolved_world rw = resolve_preferences(p2, 0xA5A50000u + s);
            body_inputs home = prototype_body(1);
            home.orbit_au = rw.home_orbit_au;
            const planetology_state st = run_planetology(home, rw.params, (0xA5A50000u + s) ^ 0xE471001u);
            if (!homeworld_viability(st).ok) ++bad;
            if (rw.gave_up) ++gave_up;
            worst = std::max(worst, static_cast<int>(rw.attempts));
        }
        std::printf("      400 preference combinations: %d unviable, %d gave up, worst %d attempts\n",
                    bad, gave_up, worst);
        check(bad == 0,     "R8 every resolved homeworld clears the strict Earth-like floor");
        check(gave_up == 0, "R8 no preference combination exhausts the reroll cap");

        // The homeworld's orbit must track its star, or the wide-sweep failure the
        // whole reshaping was built to fix comes straight back.
        world_preferences dim = pref;  dim.star  = lean::low;
        world_preferences bright = pref; bright.star = lean::high;
        const resolved_world rd = resolve_preferences(dim,    0x777u);
        const resolved_world rb = resolve_preferences(bright, 0x777u);
        check(rd.home_orbit_au < rb.home_orbit_au,
              "R8 a dimmer star pulls the homeworld's orbit inward");
    }

    // --- R9 the C -> D joint: endemic trade goods ------------------------------
    // The B -> C joint says what a world can BUILD with. This one says what it is
    // worth CARRYING FROM. The claim is that value comes from geography rather
    // than utility, so the properties to assert are about WHERE things are, not
    // how much there is.
    {
        // A living world evolves some; a dead one evolves none. The C -> D stage
        // is downstream of B -> C, so no land biosphere means no cash crops.
        check(!k.endemics.empty(),  "R9 a land biosphere evolves endemic trade goods");
        check(c.endemics.empty(),   "R9 sterile Cinder evolves none");
        check(s.endemics.empty(),   "R9 airless Selene evolves none");
        check(p.endemics.empty(),   "R9 a core fragment evolves none");

        // Endemic means SOME worlds, not every world. If every biosphere produced
        // the full set there would be nothing to trade.
        int seen[4] = {}, worlds = 0, total = 0;
        for (uint32_t i = 0; i < 200; ++i)
        {
            const planetology_state e = run_planetology(kepler(), def, 0xE4710000u + i);
            if (e.endemics.empty()) continue;
            ++worlds;
            total += static_cast<int>(e.endemics.size());
            for (const endemic_good& g : e.endemics)
                switch (g.good)
                {
                    case resource_type::tobacco: seen[0]++; break;
                    case resource_type::spices:  seen[1]++; break;
                    case resource_type::coffee:  seen[2]++; break;
                    case resource_type::furs:    seen[3]++; break;
                    default: break;
                }
        }
        std::printf("      200 worlds: tobacco %d, spices %d, coffee %d, furs %d"
                    " (mean %.2f goods per world)\n",
                    seen[0], seen[1], seen[2], seen[3],
                    worlds ? static_cast<double>(total) / worlds : 0.0);
        for (int i = 0; i < 4; ++i)
        {
            check(seen[i] > 0,       "R9 every endemic good occurs on some world");
            check(seen[i] < worlds,  "R9 no endemic good occurs on EVERY world (scarcity is real)");
        }

        // Each one is bound to a region, not smeared over the globe — that
        // boundedness is precisely what makes distance worth paying for.
        bool bounded = true;
        for (const endemic_good& g : k.endemics)
            if (g.sector_width > 0.35f || g.lat_hi - g.lat_lo > 0.60f)
                bounded = false;
        check(bounded, "R9 each endemic good is confined to one band and one sector");
    }

    // --- R10 the trade margin actually exists in a built world -----------------
    // Generating an endemic good is only half of C -> D. The other half is that a
    // market far from where it grows pays more for it. Assert that against a real
    // generated world rather than against the model in isolation.
    {
        world_params wp;
        const world wr = make_hard_coded_world(wp);

        int priced_goods = 0;
        float best_ratio = 1.0f;
        const char* best_name = "";

        for (resource_type g : { resource_type::tobacco, resource_type::spices,
                                 resource_type::coffee,  resource_type::furs })
        {
            const std::size_t ri = static_cast<std::size_t>(g);

            float lo = 1e9f, hi = 0.0f;
            int   markets = 0, source_tiles = 0;
            for (const auto& [tid, tc] : wr.tiles)
                if (tc.resource_deposit[ri] > 0.0f) ++source_tiles;
            for (const auto& [mid, mc] : wr.markets)
            {
                if (mc.base_price[ri] <= 0.0f) continue;
                lo = std::min(lo, mc.base_price[ri]);
                hi = std::max(hi, mc.base_price[ri]);
                ++markets;
            }
            if (markets == 0 || source_tiles == 0) continue;

            ++priced_goods;
            const float ratio = hi / std::max(lo, 0.01f);
            std::printf("      %-8s %5d source tiles, %d markets, price %.2f -> %.2f  (x%.2f)\n",
                        ui_name(g), source_tiles, markets,
                        static_cast<double>(lo), static_cast<double>(hi),
                        static_cast<double>(ratio));
            if (ratio > best_ratio) { best_ratio = ratio; best_name = ui_name(g); }
        }

        check(priced_goods > 0, "R10 the built world carries at least one endemic good");
        std::printf("      widest margin: %s at x%.2f\n", best_name, static_cast<double>(best_ratio));
        check(best_ratio > 1.5f,
              "R10 a market far from the source pays meaningfully more than one at it");

        // The goods must be genuinely LOCAL. If a crop covered most of the planet
        // there would be no distance to trade across.
        for (resource_type g : { resource_type::tobacco, resource_type::spices,
                                 resource_type::coffee,  resource_type::furs })
        {
            const std::size_t ri = static_cast<std::size_t>(g);
            int on = 0, land = 0;
            for (const auto& [tid, tc] : wr.tiles)
            {
                if (tc.composition == terrain_composition::ocean) continue;
                ++land;
                if (tc.resource_deposit[ri] > 0.0f) ++on;
            }
            if (on == 0) continue;
            const float share = static_cast<float>(on) / static_cast<float>(std::max(land, 1));
            check(share < 0.15f, "R10 an endemic good covers only a small share of the land");
        }
    }

    // --- R11 the player starts with something to manage ------------------------
    // scripts/verify/recipe_workforce.lua realises US-007 ("steer a building's
    // recipe and workforce") and fails loudly if the generated world seeds the
    // player no processing facility — correctly, since the flow would have nothing
    // to act on. That is a GENERATION property, so it is measured here across
    // seeds rather than discovered one world at a time.
    {
        int      with = 0, n_ok = 0;
        uint32_t seeds_ok[16] = {};
        constexpr int n = 12;
        for (int i = 0; i < n; ++i)
        {
            world_params wp;
            wp.seed = static_cast<uint32_t>(i) * 0x9E3779B1u;
            const world wr = make_hard_coded_world(wp);

            const auto cit = wr.corporations.find(wr.player_entity);
            if (cit == wr.corporations.end()) continue;
            for (const entity_id bid : cit->second.assets)
            {
                const auto bit = wr.buildings.find(bid);
                if (bit != wr.buildings.end() &&
                    bit->second.type == building_type::processing_facility)
                { ++with; seeds_ok[n_ok++] = wp.seed; break; }
            }
        }
        std::printf("      player opens with a processing facility on %d of %d seeds:", with, n);
        for (int i = 0; i < n_ok; ++i)
            std::printf(" 0x%08X", seeds_ok[i]);
        std::printf("\n");
        check(with > 0, "R11 the player can open with a processing facility to manage");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
