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

    // THE STOP YEAR SPLITS ON SHAPE (BL-906). A two-span epoch's industrial
    // arc still closes AT the epoch — that half is unchanged and untouched by
    // this item. A single-span (ancient-only) epoch used to close at the epoch
    // too, which coupled the Empires round's own end to a field that names the
    // CAMPAIGN's calendar start, not this round's close: the campaign opens at
    // `epoch_year == 0`, so the round ran 400 BCE -> 0 CE (400 years) against
    // the 400 BCE -> 1200 CE (1,600 years) `docs/generation/CIVILISATION.md`
    // § "The closure of the Empire era" specifies. `empires_stop_year`
    // (default 1200) is now that round's own close, set below in the branch
    // that actually applies.
    const bool two_span = era_minus_one_has_industrial_span(params);

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

    // BL-920: generation's own round opens on culture ground, growing city
    // states by ORGANISE and by the population-threshold rise check, rather
    // than one polity per founding culture holding everything (BL-826). See
    // the field comment on `city_states_by_population_threshold` for why
    // this is set HERE rather than on the struct default.
    hp.city_states_by_population_threshold = true;

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

    // BL-895: the network pays for war. BL-1021 (Ben, 2026-09-16, NR-827
    // option 2) re-based it: materials per year, per held region the network
    // reaches, per distinct KIND of unlike ground its realm reaches. The 40 is
    // the magnitude BL-895 shipped per roaded link, deliberately NOT moved by
    // the re-base -- raising it to make trade's share look material would be
    // fitting a figure to a target. A placeholder on the same footing as the
    // w_* weights.
    hp.trade_income_per_class = 40;

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

    // BL-929: a realm may DELIBERATELY buy a supply site rather than only
    // win it as the incidental yield of `build_work` or of enough campaigns
    // having walked a corridor. Priced ON THE SAME FOOTING as the road
    // promotion it sits beside -- the same 2000 a corridor already costs the
    // seat to cross a tier by use, since a purchase is buying the identical
    // outcome by choice. `supply_upgrade_reach_gain_q` at a quarter of
    // `work_reach_relief_cap_q` (800) means four purchases carry one region
    // from bare to the ceiling, so the verb is a real lever and not a single
    // one-shot fix. `supply_upgrade_threshold_q` at `work_threshold_q`'s own
    // 8 -- deliberately low, so a quiet round can clear it exactly as a
    // marginal Way Station can. Placeholder magnitudes on the same footing
    // as the w_* weights beside them -- the SHAPE is the ruling, the numbers
    // are history_sweep's to tune.
    hp.supply_upgrade_material_cost = 2000;
    hp.supply_upgrade_reach_gain_q  = 200;
    hp.supply_upgrade_threshold_q   = 8;

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
    // ground's own binding: 400 of reach, less an alien penalty where the
    // people is not its realm's own culture -- so near, kindred ground
    // converts and far, alien ground reasserts, which is the fault line
    // stated as two numbers.
    //
    // BL-944, RAISED (Ben, 2026-09-13: "raise the floor", not a second
    // binding term). Measured at delivery: reassertion fired ZERO times in
    // 16 worlds at a 200 penalty, because BL-896/BL-923's secession floor
    // (60) had already stripped the badly-reached ground before this test
    // ever ran on it -- held ground under an adopting realm sat at p10
    // 720-840 out of 1000, comfortably above a 400 (native) or 600 (alien)
    // bind floor. Sprint 39's other changes have since moved that floor:
    // swept fresh at 200 the alien floor already clears in 13/14
    // creed-worlds (90 reassertions across the sweep) rather than zero, so
    // the diagnosis this dial was set against is stale -- but the signal was
    // still thin (median 5 a world) and asymmetric only by accident. Raising
    // the ALIEN penalty specifically (not the shared convert floor) sharpens
    // exactly the axis a schism needs: alien ground is the residue-culture
    // ground CREEDS.md names as the fault line, so widening its bind gap is
    // what makes reassertion read as a consequence of KINSHIP, not of reach
    // alone. Swept at 450: 443 reassertions across the sweep (90 -> 443,
    // 4.9x), firing in all 14 creed-worlds, spread median 19 (range 7-91) --
    // a real per-world gradient rather than a uniform trickle, and the
    // shared convert floor is untouched so near, kindred ground still
    // converts exactly as before.
    hp.universal_creed_hold_years        = 200;
    hp.universal_creed_convert_supply_q  = 400;
    hp.universal_creed_alien_penalty_q   = 450;

    // BL-944 — THE SCHISM VERB ITSELF. 3 held regions of the same alien
    // residue culture reasserting together: enough that a realm's break can
    // read as a real people rather than one stray region's supply dipping
    // for a round, and low enough that it actually clears against the fresh
    // reassertion counts above (median 19 a creed-world, range 7-91).
    hp.schism_min_regions = 3;
    // BL-887: reach propagates through a chain of population centres, not out
    // of one capital (Ben, 2026-09-10). ON for generation's own round, which
    // is the round that has a real centre map to chain through -- BL-872
    // computes `network_supply_q` for every held region every decision round
    // and grows `region::centres` off it. The struct default stays OFF so no
    // synthetic fixture changes meaning; the field comments on
    // `history_sim_params::centre_chain_reach` carry the design and the four
    // ways its feedback loop is damped. Placeholder magnitudes, like the w_*
    // beside them -- the SHAPE is the ruling, history_sweep tunes the numbers.
    // OFF, AND THAT IS A MEASUREMENT RATHER THAN CAUTION. Built, wired and
    // swept both ways across 16 seeds at --epoch 0, the relay moves NOTHING
    // the round is judged on: secessions stay at a median 2 with 14 regions
    // walking, largest share stays at a median 10% (6-18%), hegemony stays
    // 0/16, and 15/16 worlds still show the arc. Raising the rebate more than
    // threefold (400 per centre, cap 900) reproduced the OLD figures exactly
    // -- so this is not a magnitude that wants tuning, it is a mechanism with
    // nothing to bite on here, for two diagnosable reasons:
    //
    //   THE REACH GATE ALREADY REFUSES NOTHING. BL-889's census reads
    //   "REFUSED reach gate median 0" on this round. Cheaper reach can only
    //   unlock ground a price was keeping shut, and no ground is shut.
    //
    //   THE GROUND THAT NEEDS A RELAY IS DENIED ONE. BL-872 FREEZES
    //   `region::centres` at or below `sustainable_settlement_floor_q`, so a
    //   province stranded under the secession floor is exactly a province with
    //   no town to relay through. A chain model helps where there is a chain;
    //   the pathological holdout is by construction the case with nothing
    //   behind it. `history_sim_harness`'s R3a2/R3a3 are the same shape and
    //   are unmoved, as they must be: `two_polity_world(34)` has no
    //   intervening ground at all.
    //
    // Left wired, left off, and one line from live. `history_sweep --set
    // centre_chain_reach=1` runs it without a rebuild, which is what the two
    // reasons above should be re-measured against once either one moves --
    // when the reach gate starts refusing campaigns, or when the freeze rule
    // stops denying a frontier province its town.
    // ON, ON THE MODEL'S MERITS (Ben, 2026-09-11), not on today's measurement.
    // The sweep says this changes almost nothing: tripling the rebate
    // reproduced the default figures exactly, because the reach gate refuses
    // NOTHING in this run (median 0) and BL-872 freezes centres below the
    // settlement floor, so the stranded province is by construction the one
    // with no town to relay through. Ben's ruling is that reach-through-centres
    // is the right MODEL regardless -- a world should be generated by the
    // better mechanism even where the better mechanism currently agrees with
    // the worse one. Re-measure when the reach gate starts refusing campaigns
    // or the freeze stops denying a frontier province its town.
    hp.centre_chain_reach        = true;
    hp.centre_reach_rebate_q     = 150;
    hp.centre_reach_rebate_cap_q = 500;
    hp.centre_reach_min_centres  = 1;

    // BL-868: creeds raise armies. A placeholder magnitude, like the w_* it
    // sits beside -- the SHAPE is the ruling, history_sweep tunes the number.
    hp.w_aggr_q = 300;

    // BL-838: fear of being next. Switched ON for generation's own round --
    // struct-default OFF, so every isolating fixture in the repo keeps its
    // meaning and only the real world runs it. The grant it stands on is
    // AI_OPPONENT.md sec 11 (Ben, 2026-09-11) and the whole of its scope is
    // third-party grudges: a polity is feared for what it has DONE, never for
    // how large it is. Placeholder magnitudes like the w_* beside them -- the
    // SHAPE is the ruling, history_sweep tunes the numbers.
    hp.w_fear_q       = 400;
    hp.fear_reference = 2000;

    // BL-839: THE HISTORICAL TURBULENCE LEAN, the player's one axis on this
    // pass (Ben, 2026-09-08). Resolved HERE, and nowhere else, because this is
    // the single function that turns a world descriptor into the params
    // generation's own round runs on -- so the lean reaches the sim by the same
    // route every other generation setting does, and `history_sim_params`
    // itself stays a struct of forces with no knowledge of a wizard.
    //
    // `lean::any` READS AS ORDINARY. An unset preference -- an old save, a
    // harness that built a bare `world_params` -- must run the world it always
    // ran, and `world_preferences`'s eight planetology axes default to `any`,
    // so a fixture that zero-initialises the block lands here too. See
    // `planetology.hpp` for why this axis has no sampled range to give `any`
    // its usual meaning.
    switch (params.preferences.history_turbulence)
    {
        case lean::low:  hp.turbulence_lean = -1; break; // calm
        case lean::high: hp.turbulence_lean =  1; break; // turbulent
        case lean::mid:
        case lean::any:
        default:         hp.turbulence_lean =  0; break; // ordinary
    }

    if (two_span)
    {
        // 1160 -> 1560 -> 1960 at the defaults. The ancient span is
        // `prehistory_years` long and capped at the medieval band; the
        // industrial span is `industrial_years` long with the ladder
        // unrestricted. Both bands tick at 4 years, as the single-span run
        // always has — the clock is not what changes between the spans, the
        // roster ceiling is.
        // The two-span industrial arc's own stop is `epoch_year`, unchanged by
        // BL-906 — see the header comment on `world_params::empires_stop_year`.
        hp.stop_year          = params.epoch_year;
        hp.boundary_year      = params.epoch_year - params.industrial_years;
        hp.start_year         = hp.boundary_year - params.prehistory_years;
        hp.span1_band_ceiling = roster_band::medieval;
        hp.tick_bands[0]      = {hp.boundary_year, 4};
        hp.tick_bands[1]      = {params.epoch_year, 4};
        hp.tick_band_count    = 2;
    }
    else
    {
        // BL-906: the single-span (ancient-only) run closes at its OWN stop
        // year, `empires_stop_year` (default 1200 CE), rather than at
        // `epoch_year` (the campaign's calendar start, 0 CE). `boundary_year`
        // and `span1_band_ceiling` stay at their struct defaults (INT64_MIN
        // and `industrial`), exactly as before this item: the ceiling is
        // inert twice over — no year is before the boundary, AND the clamp is
        // the identity — so nothing about the SHAPE of this branch changed,
        // only the year it now runs to.
        hp.stop_year       = params.empires_stop_year;
        hp.start_year      = params.epoch_year - params.prehistory_years;
        hp.tick_bands[0]   = {hp.stop_year, 4};
        hp.tick_band_count = 1;
    }

    // NO reach-cost override here, deliberately.
    //
    // An earlier cut of this block derived supply_decay_per_tile_q from the map
    // width (2000/gw), because the authored 28 gave a maximum reach of 36 tiles
    // and produced ZERO battles on a 312-wide map. BL-922 retired that term:
    // supply is priced from the CAPITAL over held ground in the
    // `terrain_reach_cost_q` currency alone, and its 4000 was measured on THIS
    // round (history_sweep --epoch 0, the BL-922 block), so the struct default
    // is the calibrated value and is not restated here.
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

// ---------------------------------------------------------------------------
// BL-931 — the Exploration span
// ---------------------------------------------------------------------------

bool exploration_sim_enabled(const world_params& params)
{
    return params.exploration_sim_enabled
        && era_minus_one_enabled(params)
        && !era_minus_one_has_industrial_span(params);
}

history_sim_params exploration_sim_params(const world_params& params)
{
    history_sim_params hp; // struct defaults throughout except the span/clock
                            // and the upkeep opt-in below.

    hp.start_year = params.empires_stop_year;      // 1200, wherever Empires closed.
    hp.stop_year  = params.exploration_stop_year;  // 1660 by default.
    hp.tick_bands[0]   = {hp.stop_year, 4};        // Same 4-year cadence as the
    hp.tick_band_count = 1;                        // Empires round's own close.

    // BL-931 — the round-level upkeep hook. Empty until BL-932, but this is
    // the one caller that wants it called at all.
    hp.exploration_upkeep_enabled = true;

    // BL-1036 — THE 1200 ANCHORS, EXPLICIT. Consolidation and the near-home
    // cutoff were read off `start_year`; this span opens at
    // `empires_stop_year`, so setting both to it is byte-identical to the
    // reads they replace. A later span (Digitisation, from 1660) copies these
    // params and keeps both at 1200 (DIGITISATION.md, the span paragraphs).
    hp.consolidation_year    = params.empires_stop_year;
    hp.near_home_cutoff_year = params.empires_stop_year;

    // BL-953 — a want points a campaign outward, and ranks subjection. THIS
    // span only; the struct default (0) keeps the Empires span byte-identical.
    // MEASURED, `exploration_sweep 3 --w_want_q=N`, 2026-09-14 (traced re-run,
    // w = 0/250/500/1000/2000): 0-500 move nothing beyond one-battle noise;
    // 1000 lets a maximal want double a prize (a typical 350-700 want: +35-70%,
    // the same order as the creed-appetite lean) and moves the displacement
    // median 0.04 -> 0.06 with the battle rate intact; 2000 starts swinging
    // whole seeds' neighbour-war counts (+45% on one, -60% on another), which
    // is the lean deciding rather than ranking.
    hp.w_want_q = 1000;

    // Carried opening behaviour from the Empires round, unchanged: a founding
    // that arrives inside this span (there are none scheduled — the migration
    // ends long before 1200 — but the flag is a property of the WORLD's rule
    // set, not of the span) still resolves as unorganised culture ground.
    hp.city_states_by_population_threshold = true;
    hp.settle_requires_razed_ground         = true;

    return hp;
}

uint32_t exploration_sim_seed(const world_params& params)
{
    // Own constant, own additive fold — neither the Empires round's seed
    // (`0x415C1E17u`) nor a bare re-use of the world seed.
    return (params.seed ^ 0x3720A7E1u) + params.era_seed * 0x9E3779B9u;
}

// ---------------------------------------------------------------------------
// BL-1040 — the Digitisation span
// ---------------------------------------------------------------------------

history_sim_params digitisation_sim_params(const world_params& params)
{
    // THE BASE IS EXPLORATION'S, and the one line below is the whole of that
    // decision: struct defaults plus Exploration's overrides (the upkeep step,
    // the want weight, the carried opening behaviour, and both 1200 anchors).
    // Never `era_minus_one_sim_params` -- that is the Empires round's verb set.
    history_sim_params hp = exploration_sim_params(params);

    // THE SPAN. It opens where Exploration closed and runs to the epoch, on
    // Exploration's own 4-year band (Ben, 2026-09-17, NR-888): at the defaults
    // 1660 -> 1960, 75 decision rounds (1660, 1664, ... 1956).
    hp.start_year      = params.exploration_stop_year;
    hp.stop_year       = params.digitisation_stop_year;
    hp.tick_bands[0]   = {hp.stop_year, 4};
    hp.tick_band_count = 1;

    // THE 1200 ANCHORS ARE NOT TOUCHED. `exploration_sim_params` set both to
    // `empires_stop_year`, and they stay there: the sweep of seat stores into
    // the capital happens once, and a pair met after 1200 stays far however
    // late a span opens (DIGITISATION.md, PROPOSED 2026-09-18, not
    // overturned). Re-anchoring them at 1660 would consolidate a second time
    // and call every pair met during Exploration near home.

    // THE INDUSTRY TREE, IN THIS SPAN ONLY (BL-1038; TREES.md sec Milestones:
    // it opens at its root to every living polity at 1660). Opened at the
    // span's own start, which is 1660 at the defaults -- the tree belongs to
    // this span, so a harness that moves the span's open moves the tree's
    // with it rather than leaving a gap or an overlap.
    hp.industry_tree_enabled = true;
    hp.industry_open_year    = hp.start_year;

    // `resume_seeds_corridor_tier` (BL-1037) keeps its struct default, OFF:
    // it moves every resumed span, and BL-1044 turns it on with the re-bless.
    return hp;
}

uint32_t digitisation_sim_seed(const world_params& params)
{
    // Own constant, own additive fold, on the shape of the two seeds above --
    // neither the Empires round's (`0x415C1E17u`) nor Exploration's
    // (`0x3720A7E1u`). The constant is the one BL-1036's fidelity harness
    // stood in with before this span existed, adopted so its reported
    // "own seed" source means the same run before and after.
    return (params.seed ^ 0x5D1C7A11u) + params.era_seed * 0x9E3779B9u;
}
