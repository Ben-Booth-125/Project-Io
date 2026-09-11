#include "era_minus_one.hpp"

// ---------------------------------------------------------------------------
// The Era -1 invocation's parameter derivations (BL-462). See the header for
// why they live here rather than at `make_hard_coded_world`'s call site.
// ---------------------------------------------------------------------------

bool era_minus_one_enabled(const world_params& params)
{
    // THE EPOCH CLAUSE IS GONE (BL-747). It read
    // `params.epoch_year < 1700 && params.prehistory_years > 0`, on the
    // reasoning that above 1700 the settlement pass had already pre-computed
    // the history and the era had nothing left to simulate. The two-span
    // design says otherwise: the same engine plays an ancient span and then an
    // industrial one, so a 1960 arc runs the sim too — the epoch decides only
    // whether there IS a second span, which is
    // `era_minus_one_has_industrial_span`'s question, not this one's.
    //
    // `prehistory_years == 0` is a SCOPE KNOB, not a tuning dial: it skips the
    // pass outright, which is how the harnesses that do not test the era avoid
    // paying its cost (world_params::prehistory_years).
    return params.prehistory_years > 0;
}

bool era_minus_one_has_industrial_span(const world_params& params)
{
    // The same 1700 the gate used to turn on, now asking a different question.
    //
    // `industrial_years > 0` is the second clause for the same reason
    // `prehistory_years > 0` is one on the gate above: the field is a SCOPE
    // KNOB, and its own doc-comment promises that zero means no industrial
    // span. Without this clause a zero would instead put the boundary AT the
    // epoch, which leaves the ancient span running the whole way with the
    // medieval ceiling on — the opposite of what the knob says it does.
    return params.epoch_year >= 1700 && params.industrial_years > 0;
}

history_sim_params era_minus_one_sim_params(const world_params& params)
{
    history_sim_params hp;

    // The stop year is the epoch on BOTH shapes; only the start and the
    // interior differ.
    const bool two_span = era_minus_one_has_industrial_span(params);
    hp.stop_year = params.epoch_year;

    // SETTLE IS RE-SETTLEMENT IN GENERATION'S OWN ROUND (BL-894; Ben,
    // 2026-09-11). This is the Empires round -- the land is already settled
    // when it opens, and the migration that filled it ran before this sim
    // starts (`run_settlement`, plus BL-846's founding schedule for anything
    // dated after the start year). A verb that manufactures fresh ground on
    // population pressure alone belongs to that earlier diffusion, not here.
    //
    // Set on the DERIVED params rather than on the struct default, so every
    // harness measuring the old 4000 BCE -> 0 CE arc -- which spans the
    // migration era and therefore still needs pressure-driven settling --
    // keeps the rule it was written against.
    hp.settle_requires_razed_ground = true;

    // BL-893: the water gate's weight escape, on for the same reason -- this is
    // the round the rule was designed for.
    hp.amphibious_weight_crossing = true;

    // BL-899: SEA LEGS -- the reduced ration a crossing lands on
    // (docs/lore/CREEDS.md, Sea legs). BL-893 above opened LEGALITY and changed
    // no outcome, because a wet crossing forages on nothing and so loses every
    // verb contest on supply. These three switch on the ration that makes some
    // crossings survivable without making any of them free.
    //
    // 900: a creed at FULL sea legs lands at nine tenths of what it would have
    // foraged -- nearly fed, never better fed than it would be on dry ground.
    // 300: the floor a people's creed must clear before it is fed at all, so a
    // landlocked people with a token storm god still starves. 300: the port_q a
    // staging region must reach, which is a real port rather than a puddle.
    // Placeholder magnitudes on the same footing as the w_* weights -- the
    // SHAPE is Ben's ruling, the numbers are history_sweep's to argue.
    hp.sea_legs_ration_q = 900;
    hp.sea_legs_floor_q  = 300;
    hp.sea_legs_port_q   = 300;

    // BL-895: the network pays for war. Materials per year per roaded link
    // between held regions that hold unlike ground. A placeholder magnitude on
    // the same footing as the w_* weights -- the SHAPE is the ruling, the
    // number is for history_sweep to tune.
    hp.trade_income_per_link = 40;

    // BL-895's TWO SINKS (Ben, 2026-09-11). Without them the income above
    // could not gate anything: campaigns were the ONLY thing materials were
    // ever spent on, hundreds against hundreds of millions produced. A
    // standing army is now a permanent claim on production, and a road is
    // something a realm BUYS rather than a free side effect of walking.
    // Placeholder magnitudes on the same footing as the w_* weights -- the
    // SHAPE is the ruling, history_sweep tunes the numbers.
    // 20, NOT A GUESS: measured across 16 seeds at --epoch 0 against a stated
    // target -- the sinks should claim a VISIBLE MINORITY of production, so
    // stock is a constraint a realm manages, while road-building stays a
    // CHOICE rather than something poverty forbids. At 200 the sinks claimed
    // 63% of production and 324 corridor promotions per world were refused
    // for want of materials, which is poverty governing the network. At 20
    // they claim 8% with 43 refusals, and 2,732,809 heads a world are still
    // sent home unpaid -- the strangling channel bites without swallowing the
    // economy.
    hp.army_upkeep_per_1000_heads = 20;
    hp.unpaid_army_disband_q      = 250;
    hp.road_build_material_cost   = 2000;

    // BL-896: collapse is NETWORK FAILURE (Ben, 2026-09-11). Ground whose
    // reach from its own seat has fallen this low secedes rather than falling
    // to a neighbour -- a successor realm, which is what the dark age has to
    // hand forward. Sited just above `sustainable_settlement_floor_q` (40, the
    // struct default this round takes): ground too far out for its towns to
    // grow is the same ground too far out to be ruled, which is one reach
    // reading doing both jobs rather than two floors disagreeing. NOT ordered
    // against `sustainable_garrison_floor_q`, which is ZERO on this round --
    // garrison attrition is effectively off here, so there is no ordering to
    // claim. Placeholder magnitudes; history_sweep tunes them.
    hp.secession_supply_floor_q = 60;
    hp.secession_min_regions    = 2;

    // BL-897: a creed that spans cultures (Ben, 2026-09-11). Coined new by a
    // realm that has been HUMBLED and sits in a DENSE network, then spreading
    // along contact. Placeholder magnitudes on the same footing as the w_* and
    // the BL-895/896 floors -- the SHAPE is the ruling, history_sweep tunes the
    // numbers.
    //
    // 300 for humiliation, against a cohesion floor of 180 and a start of 1000:
    // a realm at 300 has taken roughly six defeats' worth of the
    // `cohesion_loss_on_defeat_q` channel and is near the bottom of the spiral,
    // which is what "shattered or humbled" has to mean if a victor is not to
    // qualify. 8 links for density, against the ~52 roaded edges a whole world
    // carries -- so it asks for a realm holding a real share of the network,
    // not a realm holding two roads. 200 for mean reach, well above dead ground
    // and well below a realm that still supplies itself: a creed spreads along
    // contact, so the realm coining it must still be one place.
    hp.universal_creed_humbled_cohesion_q = 300;
    hp.universal_creed_min_trade_links    = 8;
    hp.universal_creed_network_floor_q    = 200;

    // A PEOPLE HOLDS BOTH, THEN IT RESOLVES. 200 years is a span a world of
    // 4000 years can show several times over, and long enough that conversion
    // reads as a generation rather than a flip. The pair then settles on the
    // ground's own binding: 400 of reach, less 200 where the people is not its
    // realm's own culture -- so near, kindred ground converts and far, alien
    // ground reasserts, which is the fault line stated as two numbers.
    hp.universal_creed_hold_years        = 200;
    hp.universal_creed_convert_supply_q  = 400;
    hp.universal_creed_alien_penalty_q   = 200;

    // BL-868: creeds raise armies. A placeholder magnitude, like the w_* it
    // sits beside -- the SHAPE is the ruling, history_sweep tunes the number.
    hp.w_aggr_q = 300;

    if (two_span)
    {
        // 1160 -> 1560 -> 1960 at the defaults. The ancient span is
        // `prehistory_years` long and capped at the medieval band; the
        // industrial span is `industrial_years` long with the ladder
        // unrestricted. Both bands tick at 4 years, as the single-span run
        // always has — the clock is not what changes between the spans, the
        // roster ceiling is.
        hp.boundary_year      = params.epoch_year - params.industrial_years;
        hp.start_year         = hp.boundary_year - params.prehistory_years;
        hp.span1_band_ceiling = roster_band::medieval;
        hp.tick_bands[0]      = {hp.boundary_year, 4};
        hp.tick_bands[1]      = {params.epoch_year, 4};
        hp.tick_band_count    = 2;
    }
    else
    {
        // EXACTLY what this function did before BL-747, byte for byte, and
        // that is the point: `boundary_year` and `span1_band_ceiling` are left
        // at their struct defaults (INT64_MIN and `industrial`), so the
        // ceiling is inert twice over — no year is before the boundary, AND
        // the clamp is the identity. An ancient epoch therefore executes the
        // same values through the same code as it did, which is what keeps the
        // 0 CE arc's `state_hash` byte-identical across this change. Adding a
        // candidate to the scorer would move the argmax even where it never
        // wins; adding an inert clamp cannot.
        hp.start_year      = params.epoch_year - params.prehistory_years;
        hp.tick_bands[0]   = {params.epoch_year, 4};
        hp.tick_band_count = 1;
    }

    // NO supply-decay override here, deliberately.
    //
    // An earlier cut of this block derived supply_decay_per_tile_q from the map
    // width (2000/gw), because the authored 28 gave a maximum reach of 36 tiles
    // and produced ZERO battles on a 312-wide map. That was a workaround for
    // measuring supply from the CAPITAL. The sim now supplies a campaign from
    // the staging holding next to the target (history_sim.cpp, `hub_dist`), so
    // reach is bounded by neighbour_radius rather than by empire shape and does
    // not need rescaling when the map does. The authored constant stands.
    return hp;
}

uint32_t era_minus_one_sim_seed(const world_params& params)
{
    // `era_seed` lets the SAME ground be played through twice and come out
    // differently — the wizard's history round rerolls it and nothing else, so
    // the planetology rounds above it are untouched (world_params::era_seed).
    //
    // ADDED, NOT XORed, and multiplied by an odd constant first. XOR would let
    // an era_seed that happened to equal a low bit pattern of the fold collapse
    // two different rolls onto one history; the odd multiplier scatters
    // consecutive roll counters (1, 2, 3...) across the whole word, which is
    // what a reroll counter actually produces.
    //
    // AT era_seed 0 THIS IS THE ORIGINAL EXPRESSION, digit for digit, so every
    // existing world, golden and fixture is unmoved.
    return (params.seed ^ 0x415C1E17u) + params.era_seed * 0x9E3779B9u;
}
