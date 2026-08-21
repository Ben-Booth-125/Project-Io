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
//   R13 THE CHECKPOINT MODEL (BL-217, GENERATION_CHECKPOINT_BRANCH_MODEL).
//       Asserts (a) same seed -> identical checkpoints vector, different seed
//       -> different; (b) the S5-S8 checkpoints correctly mirror what
//       died_at/archetype already encode; (c) resolve_checkpoint's
//       eligibility filter forces a reroll when every candidate is excluded,
//       never a silent fallback to an ineligible branch — exercised against a
//       small synthetic checkpoint class, since neither of the two real
//       classes has an easy all-excluded scenario to hand.
//       NOTE: this reuses the R13 number this file's own comment previously
//       reserved for "BL-217 rarity" — that description was written ahead of
//       BL-217's actual settled scope (checkpoint/branch/lean, not rarity
//       bands) and is corrected here rather than left stale.
//
//   R14 THE DATED TIMESTAMP (BL-220). history_event carries an integer year
//       before the campaign epoch, so ONE ordered biography spans 4.5 Gya and
//       1687 CE. Asserts exact construction, the magnitude-picking display
//       format, total oldest-first ordering, and that a historical line
//       interleaves into the deep-time list — the foundation the HISTORY.md
//       ladder (BL-221/BL-222) is built on. R12 is reserved by BL-209.
//
// HONEST SCOPE NOTE: this validates the deterministic skeleton and the gate
// ordering. The model has more free parameters (~40 tuning constants) than it
// has real calibration bodies, so a green run means the constants have not
// MOVED, not that they are right. Do not read it as a physics validation.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/planetology.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

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
    // Timestamps are compared exactly — they are integers now, so R1 catches a
    // nondeterministic date rather than tolerating it as float noise (BL-220).
    for (std::size_t i = 0; i < a.history.size(); ++i)
        if (a.history[i].years_before_epoch != b.history[i].years_before_epoch) return false;
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
        std::printf("      %14s  %-52s %s\n",
                    format_history_date(h.years_before_epoch).c_str(),
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
                        m.consequence == h.consequence &&
                        m.years_before_epoch == h.years_before_epoch)
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
        const world wr = make_hard_coded_world(no_prehistory(wp));

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
                if (is_water(tc.substrate)) continue;
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
            const world wr = make_hard_coded_world(no_prehistory(wp));

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

    // --- R13 the checkpoint model (BL-217) ------------------------------------
    {
        auto checkpoints_equal = [](const std::vector<checkpoint_record>& a,
                                    const std::vector<checkpoint_record>& b) {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i)
            {
                if (a[i].stage_id != b[i].stage_id) return false;
                if (a[i].branch_taken != b[i].branch_taken) return false;
                if (a[i].seed_used != b[i].seed_used) return false;
                if (a[i].viability_result != b[i].viability_result) return false;
            }
            return true;
        };

        // (a) determinism: same seed -> identical checkpoints, different seed
        // -> different (Kepler always resolves at least the Spark/Breath/Green
        // checkpoints, so an all-empty vector could not hide a regression).
        const planetology_state k2 = run_planetology(kepler(), def, 0xE471001u);
        const planetology_state k3 = run_planetology(kepler(), def, 0xE471002u);
        check(!k.checkpoints.empty(), "R13 Kepler resolves at least one checkpoint");
        check(checkpoints_equal(k.checkpoints, k2.checkpoints),
              "R13 same seed -> identical checkpoints vector");
        check(!checkpoints_equal(k.checkpoints, k3.checkpoints),
              "R13 different seed -> different checkpoints vector");

        // (b) the S5-S8 checkpoints mirror what died_at/archetype already
        // encode. Cinder dies at Air (S2) but the Spark code path still runs
        // for an already-terminated body (a subsurface ocean is the one
        // branch that can still carry life past an airless verdict), so
        // Cinder records exactly ONE checkpoint — a failing Spark, since
        // Cinder has no ice_shell_hint to make it a subsurface-ocean
        // candidate — and it dies there, so it never reaches Breath or
        // Green. Pallas returns before the Spark section even runs (the
        // core_fragment early-return at S1), so it records none at all.
        check(p.checkpoints.empty(),
              "R13 a core fragment (returns at S1, before Spark) records no checkpoint");
        check(c.checkpoints.size() == 1
                  && c.checkpoints.front().stage_id == chain_stage::spark
                  && !c.checkpoints.front().viability_result,
              "R13 Cinder (dies at Air) still records one failing Spark checkpoint");
        bool kepler_all_ok = !k.checkpoints.empty();
        bool kepler_all_biological = !k.checkpoints.empty();
        for (const checkpoint_record& r : k.checkpoints)
        {
            if (!r.viability_result) kepler_all_ok = false;
            if (r.stage_id != chain_stage::spark && r.stage_id != chain_stage::breath
                && r.stage_id != chain_stage::green)
                kepler_all_biological = false;
        }
        check(kepler_all_ok,
              "R13 a Cradle's checkpoints all report viability_result == true");
        check(kepler_all_biological,
              "R13 every Kepler checkpoint sits at Spark, Breath or Green");

        // A world that fails partway (Mat World: dies at Breath's GOE branch)
        // should record a Spark success followed by a Breath failure, and
        // stop there — no Green checkpoint, since the chain already died.
        //
        // GOE fails (goe_fires == false) when goe_at <= 0.15 Gya, theta is
        // too hot (>= 2.4), or water_fraction is too low. Starving the time
        // BUDGET (a young system_age_gyr) turns out NOT to isolate this:
        // Spark itself requires age >= 1.5 Gyr to fire at all (its own
        // time_budget gate), and at exactly that floor goe_at still clears
        // 0.15 for every dial value — the budget floor and the GOE floor
        // are too close together to split with age alone. theta is the
        // reliable lever instead: max out `radiogenic` so the interior runs
        // hot enough (theta >= 2.4) that GOE fails on ITS OWN condition
        // regardless of the timing arithmetic, while age stays >= 1.5 so
        // Spark still succeeds first.
        bool found_mat_world = false;
        for (float age_gyr = 1.55f; age_gyr < 2.10f && !found_mat_world; age_gyr += 0.1f)
        {
            planetology_params mat = def;
            mat.system_age_gyr = age_gyr;
            mat.radiogenic = 2.2f; // clamp ceiling — pushes theta well past 2.4
            for (uint32_t seed = 1; seed < 64 && !found_mat_world; ++seed)
            {
                const planetology_state trial = run_planetology(kepler(), mat, seed);
                if (trial.archetype != body_archetype::mat_world) continue;
                found_mat_world = true;
                bool saw_spark_ok = false, saw_breath_fail = false, saw_green = false;
                for (const checkpoint_record& r : trial.checkpoints)
                {
                    if (r.stage_id == chain_stage::spark && r.viability_result) saw_spark_ok = true;
                    if (r.stage_id == chain_stage::breath && !r.viability_result) saw_breath_fail = true;
                    if (r.stage_id == chain_stage::green) saw_green = true;
                }
                check(saw_spark_ok,
                      "R13 a Mat World still records a successful Spark checkpoint");
                check(saw_breath_fail,
                      "R13 a Mat World records a failing Breath checkpoint");
                check(!saw_green,
                      "R13 a Mat World records no Green checkpoint (the chain already died)");
            }
        }
        check(found_mat_world, "R13 a Mat World is reachable within the trial age/seed sweep");

        // (c) resolve_checkpoint's eligibility filter: an all-ineligible
        // candidate set is itself a viability failure and forces a reroll,
        // never a silent fallback to an ineligible branch. Neither of the two
        // real checkpoint classes has an easy all-excluded scenario to hand
        // (biology's own candidates are never lean-filtered today), so this
        // exercises a small synthetic checkpoint class built only for this
        // test: three candidates, the middle one is excluded on attempt 0 but
        // present from attempt 1 onward, and floor_ok is deliberately always
        // true so only the eligibility corollary is under test.
        {
            std::vector<checkpoint_record> synth;
            const char* chosen_label = nullptr;
            const bool resolved = resolve_checkpoint(
                chain_stage::spark, 0xABCDu, 0x5A5Au, /*max_attempts=*/8,
                [&](checkpoint_rng&, uint32_t attempt) {
                    std::vector<checkpoint_branch> cs;
                    cs.push_back({ "alpha", false });
                    cs.push_back({ "beta", false }); // both ineligible on attempt 0
                    cs.push_back({ "gamma", false });
                    if (attempt >= 1)
                        cs[1].eligible = true; // "beta" becomes eligible from attempt 1
                    return cs;
                },
                [&](checkpoint_rng&, const checkpoint_branch& b) { chosen_label = b.label; },
                [&]() { return true; },
                synth);

            check(resolved, "R13 resolve_checkpoint eventually resolves once a branch is eligible");
            check(chosen_label != nullptr && std::string(chosen_label) == "beta",
                  "R13 resolve_checkpoint only ever picks an ELIGIBLE branch");
            check(synth.size() >= 2,
                  "R13 the ineligible attempt(s) are recorded, not silently skipped");
            check(!synth.empty() && !synth.front().viability_result,
                  "R13 an all-ineligible attempt records viability_result == false");
            check(!synth.empty() && synth.front().branch_taken.find("no eligible branch") != std::string::npos,
                  "R13 an all-ineligible attempt's record says so, not a chosen branch's label");
            check(synth.back().viability_result,
                  "R13 the attempt that finally resolves records viability_result == true");

            // And the never-eligible degenerate case: if NOTHING is ever
            // eligible, the checkpoint exhausts max_attempts and reports
            // failure rather than fabricating a winner.
            std::vector<checkpoint_record> never;
            const bool never_resolved = resolve_checkpoint(
                chain_stage::spark, 0xDEADu, 0x5A5Au, /*max_attempts=*/4,
                [&](checkpoint_rng&, uint32_t) {
                    return std::vector<checkpoint_branch>{ { "x", false }, { "y", false } };
                },
                [&](checkpoint_rng&, const checkpoint_branch&) {},
                [&]() { return true; },
                never);
            check(!never_resolved,
                  "R13 a checkpoint with no eligible branch EVER exhausts its reroll cap and fails");
            check(never.size() == 4,
                  "R13 it records exactly one attempt per reroll, not a partial or doubled count");
        }
    }

    // --- R14 the dated timestamp (BL-220) -------------------------------------
    // history_event stores an integer year before the campaign epoch, so one
    // ordered biography can carry both 4.5 Gya and 1687 CE. R12 is reserved by
    // BL-209 (the molecular trace) and R13 by BL-217 (rarity), hence R14.
    {
        // --- construction ---
        // 4.5e9 is exactly representable, so it does NOT exercise the rounding.
        // Assert a value that does, in both directions, or the `+ 0.5` in
        // years_from_gya could be deleted with the suite still green.
        check(years_from_gya(4.50f) == 4500000000LL,
              "R14 an exactly-representable Gya value converts without drift");
        check(years_from_gya(1.0e-6f) == 1000LL,
              "R14 a value needing round-half-up converts to the nearer year");
        check(years_from_gya(-1.0e-6f) == -1000LL,
              "R14 the negative branch of the rounding is symmetric");
        check(years_from_calendar_year(campaign_epoch_year) == 0,
              "R14 the campaign epoch is year zero");
        check(years_from_calendar_year(1687) == 273,
              "R14 a historical year converts to years-before-epoch");
        check(years_from_calendar_year(-3200) == 5160,
              "R14 a BCE year converts across the zero crossing");

        // NOT ASSERTED, deliberately: years_from_gya is not exact in general.
        // float32 carries ~7 significant digits at any exponent, so 2.4f lands
        // ~95 years off a round 2.4 Gya. That is harmless for deep time (the
        // display rounds to 0.01 Gyr) and it is DETERMINISTIC, which is what
        // the model actually requires — but it is not exactness, and claiming
        // it would be a lie the harness then has to defend.
        std::printf("      years_from_gya(2.4f) = %lld (a round 2.4 Gya would be 2400000000)\n",
                    static_cast<long long>(years_from_gya(2.4f)));

        // The defect the item exists to fix, rendered through the REAL
        // function. The old float path collapsed every historical date onto
        // "0.00 Gya"; these two must now differ, and differ as calendar years.
        // Asserting against a hard-coded "%.2f Gya" in the harness instead
        // would exercise no production code at all.
        check(format_history_date(years_from_calendar_year(1687)) == "1687" &&
              format_history_date(years_from_calendar_year(1450)) == "1450",
              "R14 two historical dates that both read '0.00 Gya' before now read as themselves");

        // Each magnitude band picks its own unit.
        check(format_history_date(0) == "now",
              "R14 the epoch itself reads as 'now'");
        check(format_history_date(years_from_gya(4.50f)) == "4.50 Gya",
              "R14 deep time reads in Gya");
        check(format_history_date(252000000LL) == "252 Mya",
              "R14 the hundred-million-year band reads in Mya");
        check(format_history_date(11650LL) == "11,650 years ago",
              "R14 the late-glacial band reads as an interval, with separators");
        check(format_history_date(273LL) == "1687",
              "R14 recorded history reads as a calendar year");
        check(format_history_date(5160LL) == "-3200",
              "R14 a BCE date reads as a negative calendar year");

        // BOUNDARIES, both sides. Asserting the middle of each band is what let
        // a real defect through: 999,999,999 rounded UP into "1000 Mya", a unit
        // the table never promises, because the unit was chosen before the
        // rounding. Every band edge is now pinned from both directions.
        check(format_history_date(999999999LL) == "1.00 Gya",
              "R14 a value just under a Gyr rounds INTO Gya, never to '1000 Mya'");
        check(format_history_date(1000000000LL) == "1.00 Gya",
              "R14 exactly one Gyr reads in Gya");
        check(format_history_date(999499999LL) == "999 Mya",
              "R14 the last value that genuinely rounds to 999 Myr stays in Mya");
        check(format_history_date(1000000LL) == "1 Mya",
              "R14 exactly one Myr reads in Mya");
        check(format_history_date(999999LL) == "999,999 years ago",
              "R14 one year under a Myr reads as an interval");
        check(format_history_date(10000LL) == "10,000 years ago",
              "R14 exactly ten kyr is an interval");
        check(format_history_date(9999LL) == "-8039",
              "R14 one year under ten kyr is a calendar year");
        check(format_history_date(1LL) == "1959",
              "R14 the year before the epoch reads as 1959");

        // The Mya rounding, actually exercised. 252000000 is already an exact
        // multiple of 1e6, so it leaves the +500000 offset dead weight.
        check(format_history_date(251900000LL) == "252 Mya",
              "R14 a fractional Myr rounds to the nearer whole, not truncated");
        check(format_history_date(251400000LL) == "251 Mya",
              "R14 and rounds down when it should");
        // A grouping length that is an exact multiple of three, which is where
        // a separator loop goes wrong (a leading comma).
        check(format_history_date(100000LL) == "100,000 years ago",
              "R14 a six-digit interval groups without a leading comma");

        // Future dates are degenerate but must not render as garbage: nothing
        // generates one today, but the water gate emits `age - 1.2f` against an
        // age clamped only at 1.0 Gyr, so a caller setting system_age_gyr low
        // already reaches this branch. A bare "-200 Mya" is a worse answer than
        // saying the date is in the future.
        check(format_history_date(-1LL) == "1961",
              "R14 a future year reads as a calendar year, not a negative");
        check(format_history_date(-20000LL) == "20,000 years hence",
              "R14 a far-future interval reads 'hence', with no minus sign");
        check(format_history_date(-252000000LL) == "252 Myr hence",
              "R14 a future deep-time date never renders as a negative 'Mya'");
        check(format_history_date(-4500000000LL) == "4.50 Gyr hence",
              "R14 nor as a negative 'Gya'");

        // Ordering is total and oldest-first over a real body's biography —
        // the property the single-list presentation model depends on, and the
        // one that has to survive the historical stages interleaving into it.
        bool ordered = true, in_range = true;
        for (std::size_t i = 0; i < k.history.size(); ++i)
        {
            if (i > 0 && k.history[i - 1].years_before_epoch < k.history[i].years_before_epoch)
                ordered = false;
            const int64_t y = k.history[i].years_before_epoch;
            if (y < 0 || y > 14000000000LL) in_range = false;
        }
        check(ordered,  "R14 the biography is ordered oldest-first");
        check(in_range, "R14 every line is dated within the age of the universe");

        // A historical line sorts into the SAME list as the deep-time ones.
        // This is the interleave BL-221/BL-222 depend on, asserted now so the
        // foundation is proven before a stage relies on it.
        //
        // It does NOT land last: Kepler's S9 drawdown line is dated AT the
        // epoch ("now"), and a 1602 charter is properly older than that. The
        // claim worth asserting is that it lands strictly between the two
        // regimes — after every deep-time line, before the epoch.
        const int64_t charter = years_from_calendar_year(1602);
        std::vector<history_event> mixed = k.history;
        mixed.push_back(history_event{ charter, chain_stage::legacy,
                                       "A chartered company is registered.", "" });
        std::stable_sort(mixed.begin(), mixed.end(),
                         [](const history_event& a, const history_event& b)
                         { return a.years_before_epoch > b.years_before_epoch; });

        std::size_t at = mixed.size();
        bool still_ordered = true;
        for (std::size_t i = 0; i < mixed.size(); ++i)
        {
            if (mixed[i].years_before_epoch == charter) at = i;
            if (i > 0 && mixed[i - 1].years_before_epoch < mixed[i].years_before_epoch)
                still_ordered = false;
        }
        check(still_ordered, "R14 the biography stays ordered once a historical line joins it");
        const bool interleaved = (at > 0 && at + 1 < mixed.size());
        check(interleaved,
              "R14 a historical line interleaves between deep time and the epoch");
        // `check` records a FAIL and returns — it does not abort — so this must
        // guard on `interleaved` rather than re-deriving the bound. Testing
        // only `at < mixed.size()` leaves `at == 0` live, and `mixed[at - 1]`
        // then indexes with (size_t)-1 in exactly the regression this group
        // exists to catch.
        check(interleaved && mixed[at - 1].years_before_epoch >= 1000000LL,
              "R14 every deep-time line still sorts ahead of the historical one");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
