// Headless harness (BL-202): Corp AI stage A — the scored utility layer over
// the corp-command seam. Covers:
//   R1 — commands act only through the player-grade seams; a rejected command
//        mutates nothing; the command/decision log is byte-identical across two
//        same-seed runs (determinism).
//   R2 — hysteresis (no build into an expected loss), cooldown (a touched
//        building holds for C evals), action budget (≤1 build + ≤3 dials per
//        eval), solvency gate (no spend through the reserve floor), and the
//        player corp is NEVER commanded.
//   R3 — the state export (corp_blackboard) is visibility-honest against
//        ground truth: own facts full; rival internals absent; unsurveyed tile
//        facts absent; deterministic ordering.
//   R4 — the hire gate + debit (BL-324/BL-352) read the LIVE (corp, body)
//        pools: goods in pools unlock a gated row and are drained by exactly
//        the hire cost (ascending body id); a corp without them is refused;
//        an ungated row hires with no resource debit.
//   R6 — the budget-claim producer (BL-537 / Sprint N3 T5): a survey foregone
//        at the solvency gate becomes ONE earmarked `public_exploration` claim
//        on the corp's home nation for the FULL survey cost of the top-scoring
//        gated body; no home nation or no gating means no claim.
// Hand-builds a minimal world (no Lua / SDL / ImGui); kept outside src/ so the
// CMake glob ignores it. Follows the corp_agency_harness.cpp pattern.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_budget.hpp" // budget_claim (R6, Sprint N3 T5)
#include "world/recipe_registry.hpp"
#include "world/survey_system.hpp"  // survey_cost (R6)
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}
std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// Scene: one surveyed body with a rich open tile, an ocean tile, and a poor
// tile; one hidden body with a rich tile (fog check); a market; an AI corp
// with cash + one working mine; a player corp with one identical mine.
// ---------------------------------------------------------------------------
struct scene
{
    world     w;
    entity_id body       = null_entity; ///< surveyed home body
    entity_id hidden     = null_entity; ///< unsurveyed body
    entity_id t_rich     = null_entity; ///< open, surveyed, rich iron tile
    entity_id t_ocean    = null_entity; ///< ocean tile (invalid build)
    entity_id t_hidden   = null_entity; ///< rich tile on the hidden body
    entity_id market     = null_entity;
    entity_id ai_corp    = null_entity;
    entity_id ai_bld     = null_entity;
    entity_id pl_corp    = null_entity;
    entity_id pl_bld     = null_entity;
};

entity_id make_tile(world& w, entity_id body, int gx, int gy,
                    terrain_substrate sub, float iron_richness)
{
    const entity_id t = w.create_entity();
    tile_component tc{};
    tc.body        = body;
    tc.grid_x      = gx;
    tc.grid_y      = gy;
    tc.substrate = sub;
    tc.landform    = terrain_landform::plains;
    if (iron_richness > 0.0f)
    {
        tc.resource_deposit[ri(resource_type::iron_ore)]   = iron_richness;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
    }
    w.tiles[t] = tc;
    return t;
}

scene make_scene(float ai_cash)
{
    scene s;
    world& w = s.w;

    s.body = w.create_entity();
    {
        body_component b{};
        b.name        = "Home";
        b.type        = body_type::planet;
        b.grid_width  = 4;
        b.grid_height = 4;
        b.orbital_radius_au = 1.0f;
        b.survey.phase = survey_phase::surveyed;
        w.bodies[s.body] = b;
    }
    s.hidden = w.create_entity();
    {
        body_component b{};
        b.name        = "Veil";
        b.type        = body_type::planet;
        b.grid_width  = 4;
        b.grid_height = 4;
        b.orbital_radius_au = 2.0f;
        b.survey.phase = survey_phase::hidden;
        w.bodies[s.hidden] = b;
    }

    s.t_rich   = make_tile(w, s.body, 0, 0, terrain_substrate::rocky, 1.0f);
    s.t_ocean  = make_tile(w, s.body, 1, 0, terrain_substrate::ocean, 0.0f);
    make_tile(w, s.body, 2, 0, terrain_substrate::rocky, 0.0f); // poor filler
    const entity_id t_ai = make_tile(w, s.body, 3, 0, terrain_substrate::rocky, 0.5f);
    const entity_id t_pl = make_tile(w, s.body, 0, 1, terrain_substrate::rocky, 0.5f);
    s.t_hidden = make_tile(w, s.hidden, 0, 0, terrain_substrate::rocky, 1.0f);

    s.market = w.create_entity();
    {
        market_component mc{};
        mc.body = s.body;
        mc.base_price[ri(resource_type::iron_ore)] = 4.0f;
        mc.price = mc.base_price;
        w.markets[s.market] = mc;
    }

    auto make_corp = [&](bool is_player, const char* name, float cash,
                         entity_id tile, entity_id& out_bld) -> entity_id {
        const entity_id bld = w.create_entity();
        building_component b{};
        b.tile               = tile;
        b.type               = building_type::extraction_site;
        b.workforce_assigned = 0.5f;
        b.target_resource    = resource_type::iron_ore;
        b.workforce_auto     = false; // pin the player's dial (BL-181 is out of scope here)
        w.buildings[bld] = b;

        const entity_id corp = w.create_entity();
        corporation_component cc;
        cc.name             = name;
        cc.is_player        = is_player;
        cc.focus            = industrial_focus::extraction;
        cc.starting_capital = cash;
        cc.balance          = cash;
        cc.assets.push_back(bld);
        w.corporations[corp] = cc;
        out_bld = bld;
        return corp;
    };

    s.ai_corp = make_corp(false, "Meridian Extraction", ai_cash, t_ai, s.ai_bld);
    s.pl_corp = make_corp(true,  "Player Co",           500.0f,  t_pl, s.pl_bld);
    s.w.player_entity = s.pl_corp;
    return s;
}

recipe_registry make_registry()
{
    recipe_registry reg;
    building_economics ex;
    ex.base_rate            = 10.0f;
    ex.maintenance          = 2.0f;
    ex.base_wage            = 4.0f;
    ex.build_cost           = 100.0f;
    ex.build_duration_ticks = 2.0f;
    reg.set_economics(building_type::extraction_site, ex);
    return reg;
}

/// One full sim tick: economy step (incl. tier-0 agency + BL-202 strategic
/// tier) → market clearing → budget.
void run_tick(scene& s, const recipe_registry& reg, int tick)
{
    s.w.current_day_tick  = tick;
    s.w.current_econ_tick = tick;
    const economy_report rep = run_economy_step(s.w, reg);
    const auto flows = clear_markets(s.w, reg, rep);
    apply_budget(s.w, reg, flows, rep.workforce_contention, nullptr);
}

/// Serialise the decision ring to a canonical string (the byte-identity probe).
std::string serialise_log(const world& w)
{
    std::string out;
    char line[256];
    for (const corp_decision& d : w.ai_decisions.entries)
    {
        std::snprintf(line, sizeof line, "t%d c%u v%d s%u tl%u wf%d rc%u sc%.9g ru%.9g r%d\n",
                      d.tick, d.corp, static_cast<int>(d.command.verb), d.command.subject,
                      d.command.tile, d.command.workforce, d.command.recipe,
                      d.winning_score, d.runner_up, static_cast<int>(d.reason));
        out += line;
    }
    return out;
}

std::string run_campaign(float ai_cash, int ticks, world* out_final = nullptr)
{
    scene s = make_scene(ai_cash);
    const recipe_registry reg = make_registry();
    for (int t = 1; t <= ticks; ++t)
        run_tick(s, reg, t);
    const std::string log = serialise_log(s.w);
    if (out_final)
        *out_final = s.w;
    return log;
}

bool blackboard_has(const corp_blackboard& bb, entity_id subject, const char* pred)
{
    for (const corp_fact& f : bb.facts)
        if (f.subject == subject && f.predicate == pred)
            return true;
    return false;
}

std::string serialise_bb(const corp_blackboard& bb)
{
    std::string out;
    char line[256];
    for (const corp_fact& f : bb.facts)
    {
        std::snprintf(line, sizeof line, "%d|%u|%s|%.9g|%.3f|%d\n", f.tick, f.subject,
                      f.predicate.c_str(), f.value, f.confidence,
                      static_cast<int>(f.provenance));
        out += line;
    }
    return out;
}

} // namespace

int main()
{
    std::printf("BL-202 corp-AI scored-utility harness\n");

    // =====================================================================
    // R1 — the command seam
    // =====================================================================
    {
        scene s = make_scene(1000.0f);
        const recipe_registry reg = make_registry();

        // Applied build goes through construct_building: asset appended, no
        // up-front debit (pay-as-you-build), building authored.
        corp_command build{};
        build.tick = 1; build.corp = s.ai_corp; build.verb = corp_verb::build;
        build.tile = s.t_rich; build.type = building_type::extraction_site;
        build.target = resource_type::iron_ore;
        entity_id built = null_entity;
        const auto r1 = apply_corp_command(s.w, reg, build, &built);
        check(r1 == corp_command_result::applied && built != null_entity,
              "BL-202 R1: a valid build command is applied through construct_building");
        check(s.w.buildings.count(built) == 1 &&
              s.w.buildings.at(built).ticks_remaining == 2,
              "BL-202 R1: the built building is authored via the seam (build_duration honoured)");

        // Rejected command mutates NOTHING: snapshot, fire at the ocean, compare.
        const std::size_t n_bld  = s.w.buildings.size();
        const float       ai_bal = s.w.corporations.at(s.ai_corp).balance;
        const float       pl_bal = s.w.corporations.at(s.pl_corp).balance;
        corp_command bad = build;
        bad.tile = s.t_ocean;
        const auto r2 = apply_corp_command(s.w, reg, bad);
        check(r2 == corp_command_result::rejected_placement,
              "BL-202 R1: an ocean-tile build is rejected by the placement seam");
        check(s.w.buildings.size() == n_bld &&
              s.w.corporations.at(s.ai_corp).balance == ai_bal &&
              s.w.corporations.at(s.pl_corp).balance == pl_bal,
              "BL-202 R1: the rejected command mutates nothing");

        // Ownership guard: dialling a building the corp does not own is refused.
        corp_command steal{};
        steal.tick = 1; steal.corp = s.ai_corp; steal.verb = corp_verb::set_workforce;
        steal.subject = s.pl_bld; steal.workforce = 0;
        const int pl_wt = s.w.buildings.at(s.pl_bld).workforce_target;
        check(apply_corp_command(s.w, reg, steal) == corp_command_result::rejected_not_owner &&
              s.w.buildings.at(s.pl_bld).workforce_target == pl_wt,
              "BL-202 R1: a command on an unowned building is refused and mutates nothing");
    }

    // Determinism: two identical campaigns produce byte-identical decision logs.
    {
        const std::string log_a = run_campaign(1000.0f, 40);
        const std::string log_b = run_campaign(1000.0f, 40);
        check(!log_a.empty(),
              "BL-202 R1: the AI acts at all over 40 ticks (log is non-empty)");
        check(log_a == log_b,
              "BL-202 R1: decision logs are byte-identical across two same-seed runs");
    }

    // =====================================================================
    // R2 — hysteresis, cooldown, budget, solvency, player untouched
    // =====================================================================
    {
        world w_final;
        const std::string log = run_campaign(1000.0f, 40, &w_final);

        // Player untouched: no logged decision names the player corp, and the
        // player's building keeps its authored dials.
        bool player_commanded = false;
        for (const corp_decision& d : w_final.ai_decisions.entries)
            if (d.corp == w_final.player_entity)
                player_commanded = true;
        check(!player_commanded,
              "BL-202 R2: the player corp is NEVER strategically commanded");

        // Recover scene entity ids (same construction order as make_scene).
        scene probe = make_scene(1000.0f);
        const building_component& plb = w_final.buildings.at(probe.pl_bld);
        check(plb.workforce_target == 100 && !plb.decommissioned && plb.ai_cooldown == 0,
              "BL-202 R2: the player's building is untouched (dials, state, cooldown)");

        // Budget: per (corp, tick) at most 1 build and 3 dials.
        bool budget_ok = true;
        for (const corp_decision& a : w_final.ai_decisions.entries)
        {
            int builds = 0, dials = 0;
            for (const corp_decision& b : w_final.ai_decisions.entries)
            {
                if (b.tick != a.tick || b.corp != a.corp)
                    continue;
                if (b.command.verb == corp_verb::build) ++builds;
                else if (b.command.verb != corp_verb::survey && b.command.verb != corp_verb::hire_unit)
                    ++dials;
            }
            if (builds > 1 || dials > 3)
                budget_ok = false;
        }
        check(budget_ok, "BL-202 R2: action budget holds (<=1 build, <=3 dials per eval)");

        // Cooldown: two dial commands on the same building are at least
        // cooldown_evals * cadence_k ticks apart (4 * 4 = 16 by default).
        bool cooldown_ok = true;
        const auto& es = w_final.ai_decisions.entries;
        for (std::size_t i = 0; i < es.size(); ++i)
            for (std::size_t j = i + 1; j < es.size(); ++j)
            {
                const corp_decision& a = es[i];
                const corp_decision& b = es[j];
                // hire_unit is neither a build nor a per-building dial — it never
                // sets cmd.subject (BL-324), so two hires in different evals would
                // otherwise misread as "the same building dialled twice".
                const bool a_dial = a.command.verb != corp_verb::build &&
                                   a.command.verb != corp_verb::survey &&
                                   a.command.verb != corp_verb::hire_unit;
                const bool b_dial = b.command.verb != corp_verb::build &&
                                   b.command.verb != corp_verb::survey &&
                                   b.command.verb != corp_verb::hire_unit;
                if (!a_dial || !b_dial) continue;
                if (a.command.subject != b.command.subject) continue;
                const int gap = (b.tick > a.tick) ? b.tick - a.tick : a.tick - b.tick;
                if (gap < 16) // includes gap 0: two dials, one building, one eval
                    cooldown_ok = false;
            }
        check(cooldown_ok, "BL-202 R2: a dialled building holds for the cooldown window");

        // Solvency gate — surgical: one strategic evaluation on a due tick.
        // capex 100, reserve floor max(50, 2 x wage bill 2) = 50. A corp at
        // cash 120 could afford the build but would breach the floor (20 <= 50)
        // — vetoed; at cash 300 the same candidate passes (200 > 50).
        {
            const recipe_registry reg = make_registry();
            auto builds_at = [&](float cash) {
                scene s = make_scene(cash);
                economy_report rep; // strategic step needs no production report for builds
                run_corp_strategic_step(s.w, reg, rep, /*tick=*/4); // AI corp index 0 -> due at tick%4==0
                for (const corp_decision& d : s.w.ai_decisions.entries)
                    if (d.command.verb == corp_verb::build)
                        return true;
                return false;
            };
            check(!builds_at(120.0f),
                  "BL-202 R2: the solvency gate blocks a build that breaches the reserve floor");
            check(builds_at(300.0f),
                  "BL-202 R2: the same candidate passes once cash clears the floor (gate, not ban)");
        }

        // Hysteresis / expected-loss veto: with a worthless market price the
        // build's expected net is negative — no construction, ever.
        {
            scene s = make_scene(1000.0f);
            const recipe_registry reg = make_registry();
            s.w.markets.at(s.market).base_price[ri(resource_type::iron_ore)] = 0.1f;
            s.w.markets.at(s.market).price = s.w.markets.at(s.market).base_price;
            for (int t = 1; t <= 40; ++t)
                run_tick(s, reg, t);
            bool built_at_loss = false;
            for (const corp_decision& d : s.w.ai_decisions.entries)
                if (d.command.verb == corp_verb::build)
                    built_at_loss = true;
            check(!built_at_loss,
                  "BL-202 R2: hysteresis/do-nothing bias — no build into an expected loss");
        }
    }

    // =====================================================================
    // R3 — visibility-honest state export
    // =====================================================================
    {
        scene s = make_scene(1000.0f);
        s.w.buildings.at(s.pl_bld).workforce_target = 77; // rival internal state
        const corp_blackboard bb = export_corp_blackboard(s.w, s.ai_corp, 5);

        check(bb._v == 1 && bb.corp == s.ai_corp && bb.tick == 5,
              "BL-202 R3: export carries schema version, corp, and tick");
        check(blackboard_has(bb, s.ai_corp, "cash"),
              "BL-202 R3: own cash is exported in full (own_asset)");
        check(blackboard_has(bb, s.ai_bld, "building_recipe") &&
              blackboard_has(bb, s.ai_bld, "building_workforce_target"),
              "BL-202 R3: own building internals are exported in full");
        check(blackboard_has(bb, s.pl_bld, "rival_building_type") &&
              blackboard_has(bb, s.pl_bld, "rival_building_tile"),
              "BL-202 R3: rival building existence/type/tile are visible (BL-068)");

        // Ground-truth honesty: nothing about rival internals leaks.
        bool leak = false;
        for (const corp_fact& f : bb.facts)
        {
            if (f.subject == s.pl_corp && f.predicate == "cash") leak = true;
            if (f.subject == s.pl_bld &&
                (f.predicate == "building_recipe" ||
                 f.predicate == "building_workforce_target" ||
                 f.predicate.rfind("pool", 0) == 0))
                leak = true;
        }
        check(!leak, "BL-202 R3: rival cash / recipe / workforce / pools NEVER leak");

        // Geographic fog: the surveyed rich tile exports a deposit fact; the
        // identical tile on the hidden body exports nothing.
        check(blackboard_has(bb, s.t_rich, "tile_deposit:0"),
              "BL-202 R3: surveyed tile deposits are exported (provenance survey)");
        bool hidden_leak = false;
        for (const corp_fact& f : bb.facts)
            if (f.subject == s.t_hidden)
                hidden_leak = true;
        check(!hidden_leak,
              "BL-202 R3: tiles on an unsurveyed body export NO facts");

        // Deterministic ordering: two exports serialise byte-identically.
        const corp_blackboard bb2 = export_corp_blackboard(s.w, s.ai_corp, 5);
        check(serialise_bb(bb) == serialise_bb(bb2),
              "BL-202 R3: the export is deterministically ordered (byte-identical)");
    }

    // =====================================================================
    // R4 — hire gate + debit read the live (corp, body) pools (BL-324/BL-352)
    // =====================================================================
    {
        const recipe_registry reg = make_registry();

        // Row indices by name — cmd.unit_type indexes unit_roster_table().
        const auto& table = unit_roster_table();
        std::size_t levy = table.size(), iron_foot = table.size();
        for (std::size_t i = 0; i < table.size(); ++i)
        {
            if (std::strcmp(table[i].name, "Levy Spear") == 0) levy = i;
            if (std::strcmp(table[i].name, "Iron Foot") == 0)  iron_foot = i;
        }
        check(levy < table.size() && iron_foot < table.size(),
              "BL-352 R4: the probe rows (ungated Levy Spear, ore-gated Iron Foot) exist");

        auto hire = [&](scene& s, std::size_t row) {
            corp_command cmd{};
            cmd.tick = 1; cmd.corp = s.ai_corp; cmd.verb = corp_verb::hire_unit;
            cmd.tile = s.t_rich; cmd.unit_type = static_cast<uint16_t>(row);
            return apply_corp_command(s.w, reg, cmd);
        };

        // BL-325 S2: a hire must land on the corp's own COMPLETED military
        // base, so every applied-path scenario plants one on t_rich first.
        auto add_muster_base = [&](scene& s) {
            const entity_id b = s.w.create_entity();
            building_component bc{};
            bc.tile           = s.t_rich;
            bc.type           = building_type::military_base;
            bc.ticks_remaining = 0;
            s.w.buildings[b] = bc;
            s.w.corporations[s.ai_corp].assets.push_back(b);
        };

        // Goods in the live pools unlock the gated row, and the hire drains
        // them across bodies in ascending id order (home before hidden) by
        // exactly the flat axis cost (5, corp_command.cpp's hire_axis_cost).
        {
            scene s = make_scene(1000.0f);
            add_muster_base(s);
            s.w.pool_for(s.ai_corp, s.body).quantities[ri(resource_type::steel)]   = 3.0f;
            s.w.pool_for(s.ai_corp, s.hidden).quantities[ri(resource_type::steel)] = 4.0f;
            const auto r = hire(s, iron_foot);
            check(r == corp_command_result::applied && s.w.units.size() == 1,
                  "BL-352 R4: pooled goods make the gated row hireable through the seam");
            check(s.w.pool_for(s.ai_corp, s.body).quantities[ri(resource_type::steel)] == 0.0f &&
                  s.w.pool_for(s.ai_corp, s.hidden).quantities[ri(resource_type::steel)] == 2.0f,
                  "BL-352 R4: the debit drains pools in ascending body order by exactly the cost");
        }

        // No goods anywhere: the gate refuses the row (availability re-check),
        // and nothing is created or drained.
        {
            scene s = make_scene(1000.0f);
            const auto r = hire(s, iron_foot);
            check(r == corp_command_result::rejected_invalid && s.w.units.empty(),
                  "BL-352 R4: a corp with empty pools is refused the gated row");
        }

        // Gate open (any steel > 0) but under the axis cost: the two-phase
        // debit refuses as unaffordable and leaves the pool untouched.
        {
            scene s = make_scene(1000.0f);
            add_muster_base(s);
            s.w.pool_for(s.ai_corp, s.body).quantities[ri(resource_type::steel)] = 3.0f;
            const auto r = hire(s, iron_foot);
            check(r == corp_command_result::rejected_funds && s.w.units.empty() &&
                  s.w.pool_for(s.ai_corp, s.body).quantities[ri(resource_type::steel)] == 3.0f,
                  "BL-352 R4: an unaffordable hire is refused whole — no partial debit");
        }

        // An ungated row hires with no resource debit at all.
        {
            scene s = make_scene(1000.0f);
            add_muster_base(s);
            s.w.pool_for(s.ai_corp, s.body).quantities[ri(resource_type::steel)] = 7.0f;
            const auto r = hire(s, levy);
            check(r == corp_command_result::applied && s.w.units.size() == 1 &&
                  s.w.pool_for(s.ai_corp, s.body).quantities[ri(resource_type::steel)] == 7.0f,
                  "BL-352 R4: an ungated row hires without touching the pools");
        }
    }


    // =====================================================================
    // R5 — BL-498: the hire GATE and the hire DEBIT read ONE shared table
    // =====================================================================
    //
    // THE BUG CLASS. Until 2026-08-21 `debit_hire_cost` (corp_command.cpp)
    // hand-mirrored `campaign_gate_input`'s (unit_roster.cpp) three axis
    // preference lists, guarded by nothing but a comment saying they must
    // match. The failure mode is asymmetric and nasty in BOTH directions:
    // a resource on the GATE's list but not the DEBIT's is a FREE HIRE
    // (BL-394's own defect); one on the DEBIT's but not the GATE's is a
    // silently unhireable row. Both lists now come from
    // `unit_roster.hpp § hire_axis_table`.
    //
    // The assertion the item asks for, stated behaviourally: for EVERY axis
    // and EVERY candidate resource on it, a corp holding exactly that one
    // resource at exactly the axis cost gates the row open AND pays for it
    // in exactly that resource. If the two lists ever drift, some candidate
    // fails one half of that sentence.
    {
        const recipe_registry reg  = make_registry();
        const roster_band     band = campaign_roster_band_for(reg.era());

        auto add_bldg = [&](scene& s, building_type bt) {
            const entity_id b = s.w.create_entity();
            building_component bc{};
            bc.tile            = s.t_rich;
            bc.type            = bt;
            bc.ticks_remaining = 0;
            s.w.buildings[b] = bc;
            s.w.corporations[s.ai_corp].assets.push_back(b);
        };

        // The axes are disjoint by construction; assert it, because the
        // per-axis probes below rely on endowing one axis without
        // accidentally satisfying another.
        {
            bool disjoint = true;
            for (std::size_t a = 0; a < hire_axis_count; ++a)
                for (std::size_t b = a + 1; b < hire_axis_count; ++b)
                    for (const resource_type x : hire_axis_resources(static_cast<hire_axis>(a)))
                        for (const resource_type y : hire_axis_resources(static_cast<hire_axis>(b)))
                            if (x == y) disjoint = false;
            check(disjoint, "BL-498 R5: the three hire axes share no resource");
        }

        // Every axis names at least one resource — an empty axis would make
        // its row unhireable while still reading as gated.
        {
            bool populated = true;
            for (std::size_t a = 0; a < hire_axis_count; ++a)
                if (hire_axis_resources(static_cast<hire_axis>(a)).count == 0) populated = false;
            check(populated, "BL-498 R5: no axis on the shared table is empty");
        }

        // A resource on NO axis opens NO axis — the gate reads the table and
        // nothing else, so it cannot acquire a candidate the debit lacks.
        {
            scene s = make_scene(1000.0f);
            s.w.pool_for(s.ai_corp, s.body).quantities[ri(resource_type::machinery)] = 1000.0f;
            const campaign_roster_gate_input g = campaign_gate_input(s.w, s.ai_corp);
            check(g.ore_q == 0 && g.farm_q == 0 && g.energy_q == 0,
                  "BL-498 R5: a resource absent from the table opens no axis");
        }

        // The per-axis, per-candidate probe.
        for (std::size_t ax = 0; ax < hire_axis_count; ++ax)
        {
            const hire_axis axis = static_cast<hire_axis>(ax);

            // Pick a row this campaign band actually offers that gates on
            // THIS axis, using a fully-endowed corp to enumerate.
            std::size_t probe_row = SIZE_MAX;
            {
                scene s = make_scene(1000.0f);
                for (std::size_t a = 0; a < hire_axis_count; ++a)
                    for (const resource_type r : hire_axis_resources(static_cast<hire_axis>(a)))
                        s.w.pool_for(s.ai_corp, s.body).quantities[ri(r)] = 1000.0f;
                add_bldg(s, building_type::port);
                const auto  rows  = available_rows(s.w, s.ai_corp, band);
                const auto& table = unit_roster_table();
                for (const roster_row* r : rows)
                {
                    if (gate_axis_q(r->gate, axis) <= 0) continue;
                    for (std::size_t i = 0; i < table.size(); ++i)
                        if (&table[i] == r) { probe_row = i; break; }
                    if (probe_row != SIZE_MAX) break;
                }
            }
            check(probe_row != SIZE_MAX,
                  ("BL-498 R5: axis " + std::to_string(ax) +
                   " has a gated row this band offers (the probe is not vacuous)").c_str());
            if (probe_row == SIZE_MAX) continue;

            const roster_row& row = unit_roster_table()[probe_row];

            for (const resource_type cand : hire_axis_resources(axis))
            {
                scene s = make_scene(1000.0f);
                add_bldg(s, building_type::military_base);
                if (row.gate.port_q > 0) add_bldg(s, building_type::port);

                // Every OTHER gated axis endowed generously; THIS axis holds
                // exactly one candidate at exactly the axis cost.
                for (std::size_t a = 0; a < hire_axis_count; ++a)
                {
                    if (a == ax) continue;
                    const resource_type other =
                        hire_axis_resources(static_cast<hire_axis>(a)).candidates[0];
                    s.w.pool_for(s.ai_corp, s.body).quantities[ri(other)] = 1000.0f;
                }
                s.w.pool_for(s.ai_corp, s.body).quantities[ri(cand)] = hire_axis_cost;

                corp_command cmd{};
                cmd.tick = 1; cmd.corp = s.ai_corp; cmd.verb = corp_verb::hire_unit;
                cmd.tile = s.t_rich; cmd.unit_type = static_cast<uint16_t>(probe_row);
                const auto r = apply_corp_command(s.w, reg, cmd);

                const float left = s.w.pool_for(s.ai_corp, s.body).quantities[ri(cand)];
                const std::string label =
                    "BL-498 R5: axis " + std::to_string(ax) + " candidate " +
                    std::to_string(static_cast<int>(cand)) +
                    " gates row '" + row.name + "' open AND pays for it in that resource";
                check(r == corp_command_result::applied && s.w.units.size() == 1 && left == 0.0f,
                      label.c_str());
            }
        }
    }

    // =====================================================================
    // R6 — the budget-claim producer (BL-537 / Sprint N3 T5)
    // =====================================================================
    // A survey the rival WANTED and could not afford becomes a claim on its
    // home nation's `public_exploration` line: the FULL survey cost, earmarked
    // to the body (NR-568). One per corp per evaluation — the top-scoring gated
    // survey — none from a corp with no home nation, none when the survey was
    // not gated. The scene's hidden body sits 2 AU out (cost 4508); a second
    // hidden body at 1 AU (cost 2508) scores higher (same area, lower cost), so
    // the claim must name THAT one and carry 2508, not 4508.
    {
        const recipe_registry reg = make_registry();

        auto add_nation = [](scene& s) {
            const entity_id n = s.w.create_entity();
            nation_component nc{};
            nc.name     = "Homeland";
            nc.treasury = 0.0f; // the pass is not run here; the claim is the subject
            s.w.nations[n] = nc;
            return n;
        };
        auto add_near_hidden = [](scene& s) {
            const entity_id b = s.w.create_entity();
            body_component bc{};
            bc.name         = "Near Veil";
            bc.type         = body_type::planet;
            bc.grid_width   = 4;
            bc.grid_height  = 4;
            bc.orbital_radius_au = 1.0f;
            bc.survey.phase = survey_phase::hidden;
            s.w.bodies[b] = bc;
            return b;
        };

        // Cash-poor, with a home nation: exactly one claim, the right fields.
        {
            scene s = make_scene(1000.0f);
            const entity_id nation = add_nation(s);
            const entity_id near   = add_near_hidden(s);
            s.w.corporations.at(s.ai_corp).home_nation = nation;
            economy_report rep;
            run_corp_strategic_step(s.w, reg, rep, /*tick=*/4);

            const float near_cost = survey_cost(s.w, near);
            const float far_cost  = survey_cost(s.w, s.hidden);
            check(near_cost < far_cost && near_cost > 1000.0f,
                  "BL-537 R6: the probe is not vacuous - both surveys cost more than "
                  "the corp holds, and the near body is the cheaper (higher-scoring) one");
            check(rep.budget_claims.size() == 1,
                  "BL-537 R6: a cash-gated rival with a home nation files exactly ONE "
                  "claim per evaluation, though two hidden bodies were gated");
            if (rep.budget_claims.size() == 1)
            {
                const budget_claim& c = rep.budget_claims[0];
                check(c.nation == nation && c.corp == s.ai_corp &&
                      c.line == budget_priority::public_exploration,
                      "BL-537 R6: the claim names the home nation, the corp, and the "
                      "public_exploration line");
                check(c.subject == near && c.amount == near_cost,
                      "BL-537 R6: the claim is earmarked to the TOP-SCORING gated survey "
                      "and carries that body's FULL survey cost (NR-568)");
            }
            check(s.w.bodies.at(near).survey.phase == survey_phase::hidden &&
                  s.w.bodies.at(s.hidden).survey.phase == survey_phase::hidden,
                  "BL-537 R6: filing a claim dispatches nothing - both bodies stay hidden");
        }

        // No home nation: nobody to ask; no claim.
        {
            scene s = make_scene(1000.0f);
            add_near_hidden(s);
            economy_report rep;
            run_corp_strategic_step(s.w, reg, rep, /*tick=*/4);
            check(rep.budget_claims.empty() &&
                  s.w.corporations.at(s.ai_corp).home_nation == null_entity,
                  "BL-537 R6: a cash-gated rival with NO home nation files no claim");
        }

        // Rich: the survey passes the gate and is dispatched; no claim.
        {
            scene s = make_scene(20000.0f);
            const entity_id nation = add_nation(s);
            const entity_id near   = add_near_hidden(s);
            s.w.corporations.at(s.ai_corp).home_nation = nation;
            economy_report rep;
            run_corp_strategic_step(s.w, reg, rep, /*tick=*/4);
            check(rep.budget_claims.empty() &&
                  s.w.bodies.at(near).survey.phase == survey_phase::in_transit,
                  "BL-537 R6: a rival that could afford its survey dispatches it and "
                  "files no claim - the gate, not the wish, is the trigger");
        }
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
