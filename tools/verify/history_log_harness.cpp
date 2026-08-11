// Headless world-history-log harness (BL-208; no SDL / Lua / ImGui).
//
// Mirrors tools/verify/history_ladder_harness.cpp's style: builds the REAL
// generated world via make_hard_coded_world (not hand-fabricated log entries),
// since the gap history_ladder_harness itself never closed — it reads
// generation_report's per-body history directly, never through world state —
// is exactly what BL-208 fixes. This harness is the first to exercise
// seed_genesis_history and confirm the bridge actually lands entries in
// world::history_log, not just in the presentation-only report.
//
//   R1  GENESIS CHAPTER. After seed_genesis_history, world::history_log holds
//       one genesis-topic entry per body per generation_report history line,
//       correctly tagged by that body's entity id, with the SAME timestamp/
//       event/consequence.
//
//   R2  CHECKPOINT MIGRATION. planetology_state::checkpoints entries appear as
//       topic=checkpoint, tagged by body, each with a resolved (non-fabricated)
//       timestamp — see history_log.cpp's resolve_checkpoint_timestamp for the
//       documented resolution rule (a deliberate simplification, not an exact
//       per-line pairing; flagged in NEEDS_REVIEW.json).
//
//   R3  LIVE SOURCES. Running a short bot-vs-bot economy loop (mirrors
//       ai_skill_harness.cpp's run_tick/make_registry) produces decision- and
//       agency-topic entries. A synthetic convoy pair (mirrors
//       trade_routes_harness.cpp) proves a trade_route entry PAIR appears on
//       FIRST establishment — one entry tagged per endpoint, same narration,
//       source-then-destination order (BL-282) — and does NOT duplicate on a
//       repeat completion of the same lane (which only bumps the existing
//       trade_route).
//
//   R4  ROUND-TRIP. write_history_log then read_history_log reproduces a
//       populated log field-for-field.
//
//   R5  MAGIC/VERSION REJECTION. A wrong-magic or wrong-version stream is
//       refused (returns false) and leaves the destination world's
//       history_log untouched — never misread as garbage entries.
//
//   R6  NO REGRESSION. export_corp_blackboard and body_activity_visibility —
//       neither touched by this item — still run and return sane output after
//       the log wiring. The deeper behavioural coverage stays owned by
//       blackboard_harness.cpp / commercial_fog_harness.cpp (unchanged).
//
// The process exits non-zero if any assertion FAILs.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_log.hpp"
#include "world/market_clearing.hpp"
#include "world/planetology.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

// ---------------------------------------------------------------------------
// Hand-built registry — mirrors ai_skill_harness.cpp's make_registry() (kept
// minimal here: only what's needed to get the reflex + strategic tiers firing
// over a handful of ticks, not a full economics authoring pass).
// ---------------------------------------------------------------------------
recipe_registry make_registry()
{
    recipe_registry reg;

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    processing.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 25.0f;
    reg.set_economics(building_type::processing_facility, processing);

    building_economics port;
    port.maintenance          = 8.0f;
    port.base_wage            = 6.0f;
    port.build_cost           = 150.0f;
    port.build_duration_ticks = 2.0f;
    port.resource_build_cost[static_cast<std::size_t>(resource_type::steel)] = 20.0f;
    reg.set_economics(building_type::port, port);

    recipe steel;
    steel.name = "steel";
    steel.inputs[static_cast<std::size_t>(resource_type::iron_ore)] = 2.0f;
    steel.outputs[static_cast<std::size_t>(resource_type::steel)]   = 1.0f;
    reg.add_recipe(steel);

    recipe fuel;
    fuel.name = "refined_fuel";
    fuel.inputs[static_cast<std::size_t>(resource_type::petroleum)]     = 2.0f;
    fuel.outputs[static_cast<std::size_t>(resource_type::refined_fuel)] = 1.0f;
    reg.add_recipe(fuel);

    return reg;
}

/// One full sim tick, mirroring ai_skill_harness.cpp's run_tick: economy step
/// (incl. the BL-202 strategic tier) -> market clearing -> budget.
void run_tick(world& w, const recipe_registry& reg, int tick)
{
    w.current_day_tick = tick;
    const economy_report rep = run_economy_step(w, reg);
    const auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention, nullptr);
}

// Build a synthetic body with a market centred on it (mirrors
// trade_routes_harness.cpp's make_body_market).
struct body_market { entity_id body; entity_id market; };

body_market make_body_market(world& w, const char* name)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.name = name;
    bc.type = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.grid_width = bc.grid_height = 4;
    w.bodies[body] = bc;

    const entity_id market = w.create_entity();
    market_component mc{};
    mc.body = body;
    w.markets[market] = mc;

    return { body, market };
}

void arrive_convoy(world& w, entity_id corp, entity_id src_market, entity_id dest_market)
{
    convoy_component cv{};
    cv.source_market  = src_market;
    cv.dest_market    = dest_market;
    cv.cargo_resource = resource_type::iron_ore;
    cv.cargo_qty      = 10.0f;
    cv.progress       = 1.0f;
    cv.arrived        = true;
    cv.corp           = corp;
    w.convoys.push_back(cv);
}

std::size_t count_topic(const std::vector<world_history_entry>& log, history_topic t)
{
    std::size_t n = 0;
    for (const world_history_entry& e : log) if (e.topic == t) ++n;
    return n;
}

} // namespace

int main()
{
    std::printf("=== world history log (BL-208) ===\n\n");

    // =========================================================================
    // R1/R2 — genesis + checkpoint chapters, over the real generated world.
    // =========================================================================
    world_params wp;
    wp.seed = 0xB208u;
    generation_report report;
    world w = make_hard_coded_world(wp, &report);
    seed_genesis_history(w, report);

    const generation_report::body_entry* kepler = nullptr;
    for (const auto& b : report.bodies) if (b.is_homeworld) kepler = &b; // BL-257: identity, not name
    check(kepler != nullptr, "R1 Kepler appears in the generation report");
    if (!kepler) { std::printf("\n=== FAILURES PRESENT ===\n"); return 1; }

    const entity_id kepler_id = w.home_body; // the world already knows which body is home
    check(kepler_id != null_entity, "R1 Kepler resolves to a world body entity");

    {
        // Every genesis-topic entry tagged Kepler must correspond to one of
        // Kepler's generation_report history lines (same timestamp/event/
        // consequence) — a straight content check, order-independent (the
        // per-body merge with checkpoint entries may interleave them).
        std::vector<const world_history_entry*> kepler_genesis;
        for (const world_history_entry& e : w.history_log)
            if (e.topic == history_topic::genesis && e.body == kepler_id)
                kepler_genesis.push_back(&e);

        check(kepler_genesis.size() == kepler->state.history.size(),
              "R1 genesis-topic entry count matches Kepler's generation_report history count");

        bool all_matched = true;
        for (const history_event& h : kepler->state.history)
        {
            bool found = false;
            for (const world_history_entry* e : kepler_genesis)
                if (e->timestamp == h.years_before_epoch && e->event == h.event &&
                    e->consequence == h.consequence)
                {
                    found = true;
                    break;
                }
            if (!found) { all_matched = false; break; }
        }
        check(all_matched, "R1 every Kepler history line reappears in the log, field-identical");

        // A body's genesis entries must NOT bleed onto another body's tag.
        std::size_t non_kepler_genesis = 0;
        for (const world_history_entry& e : w.history_log)
            if (e.topic == history_topic::genesis && e.body != kepler_id && e.body != null_entity)
                ++non_kepler_genesis;
        check(non_kepler_genesis > 0,
              "R1 other bodies also carry their own tagged genesis entries (not just Kepler)");
    }

    {
        std::vector<const world_history_entry*> kepler_checkpoints;
        for (const world_history_entry& e : w.history_log)
            if (e.topic == history_topic::checkpoint && e.body == kepler_id)
                kepler_checkpoints.push_back(&e);

        check(kepler_checkpoints.size() == kepler->state.checkpoints.size(),
              "R2 checkpoint-topic entry count matches Kepler's checkpoints count");
        check(!kepler_checkpoints.empty(),
              "R2 Kepler (a Cradle) resolves at least one checkpoint");

        bool all_dated = true;
        for (const world_history_entry* e : kepler_checkpoints)
            if (e->timestamp == 0) all_dated = false; // 0 would mean the resolver's empty-fallback fired
        check(all_dated,
              "R2 every checkpoint entry carries a resolved (non-fallback-zero) timestamp");
    }

    // =========================================================================
    // R1 (determinism cross-check) — two identical-seed generations produce
    // identical genesis-chapter entries. (The authoritative check for this
    // lives in determinism_harness.cpp per BL-208's brief; this is a cheap
    // local corroboration.)
    // =========================================================================
    {
        generation_report report2;
        world w2 = make_hard_coded_world(wp, &report2);
        seed_genesis_history(w2, report2);

        auto genesis_of = [](const world& ww, entity_id body) {
            std::vector<const world_history_entry*> v;
            for (const world_history_entry& e : ww.history_log)
                if ((e.topic == history_topic::genesis || e.topic == history_topic::checkpoint) &&
                    e.body == body)
                    v.push_back(&e);
            return v;
        };
        const entity_id kepler_id2 = w2.home_body;

        const auto ga = genesis_of(w, kepler_id);
        const auto gb = genesis_of(w2, kepler_id2);
        bool same = ga.size() == gb.size();
        for (std::size_t i = 0; same && i < ga.size(); ++i)
            same = ga[i]->timestamp == gb[i]->timestamp && ga[i]->topic == gb[i]->topic &&
                   ga[i]->event == gb[i]->event && ga[i]->consequence == gb[i]->consequence;
        check(same, "R1 two identical-seed generations produce an identical genesis+checkpoint chapter");
    }

    // =========================================================================
    // R3 — live sources: decision/agency over a short bot-vs-bot run, and a
    // trade-route entry that fires once (not on every completion).
    // =========================================================================
    {
        const recipe_registry reg = make_registry();
        for (int t = 1; t <= 40; ++t)
            run_tick(w, reg, t);

        check(count_topic(w.history_log, history_topic::decision) > 0,
              "R3 at least one decision-topic entry appears over 40 ticks of bot-vs-bot play");
        check(count_topic(w.history_log, history_topic::agency) > 0,
              "R3 at least one agency-topic entry appears over 40 ticks of bot-vs-bot play");
    }

    {
        world tw; // a fresh, minimal world — isolates the trade-route dedup check
        const entity_id corp = tw.create_entity();
        const body_market a = make_body_market(tw, "A");
        const body_market b = make_body_market(tw, "B");

        arrive_convoy(tw, corp, a.market, b.market);
        credit_arrived_convoys(tw, 10);
        check(tw.trade_routes.size() == 1, "R3 the underlying trade_route is created (sanity)");

        // BL-282: a new route is a TWO-body event, so it pushes TWO entries —
        // one tagged per endpoint — carrying the SAME narration. A body-scoped
        // filter then finds the route from either side.
        check(count_topic(tw.history_log, history_topic::trade_route) == 2,
              "R3 exactly two trade_route-topic log entries on FIRST establishment (BL-282)");
        {
            std::vector<const world_history_entry*> routes;
            for (const world_history_entry& e : tw.history_log)
                if (e.topic == history_topic::trade_route)
                    routes.push_back(&e);
            const bool paired = routes.size() == 2;
            check(paired && routes[0]->body != routes[1]->body,
                  "R3 the two entries carry DISTINCT body tags");
            check(paired &&
                      ((routes[0]->body == a.body && routes[1]->body == b.body) ||
                       (routes[0]->body == b.body && routes[1]->body == a.body)),
                  "R3 the two body tags are exactly the route's two endpoints");
            check(paired && routes[0]->event == routes[1]->event,
                  "R3 both entries carry the same narration naming both endpoints");
            check(paired && routes[0]->timestamp == routes[1]->timestamp &&
                      routes[0]->timestamp == 10,
                  "R3 both entries share the establishing tick");
            // Order within the tick is fixed source-then-destination, not
            // iteration-order dependent — the determinism contract.
            check(paired && routes[0]->body == a.body,
                  "R3 entry order within the tick is source-then-destination (deterministic)");
            // The point of the change: filtering by EITHER endpoint finds it.
            const auto by_body = [&](entity_id id) {
                int n = 0;
                for (const world_history_entry& e : tw.history_log)
                    if (e.topic == history_topic::trade_route && e.body == id)
                        ++n;
                return n;
            };
            check(by_body(a.body) == 1, "R3 a body-scoped filter on the SOURCE finds the route");
            check(by_body(b.body) == 1, "R3 a body-scoped filter on the DESTINATION finds the route");
        }

        // A second completion on the SAME (unordered) lane bumps the existing
        // trade_route (trade_routes_harness.cpp's own R1 covers that byte-for-
        // byte) but must NOT add further log entries.
        arrive_convoy(tw, corp, b.market, a.market);
        credit_arrived_convoys(tw, 25);
        check(tw.trade_routes.size() == 1, "R3 the repeat completion still bumps the SAME trade_route");
        check(count_topic(tw.history_log, history_topic::trade_route) == 2,
              "R3 the repeat completion does NOT duplicate the trade_route log entries");

        // A distinct lane (different body pair) is a genuinely new route and
        // DOES get its own pair of entries.
        const body_market c = make_body_market(tw, "C");
        arrive_convoy(tw, corp, a.market, c.market);
        credit_arrived_convoys(tw, 30);
        check(count_topic(tw.history_log, history_topic::trade_route) == 4,
              "R3 a genuinely distinct lane adds its own PAIR of trade_route log entries");
    }

    // =========================================================================
    // R4/R5 — serialisation round-trip + magic/version rejection.
    // =========================================================================
    {
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_history_log(w, ss);

        world w_read;
        const bool ok = read_history_log(w_read, ss);
        check(ok, "R4 read_history_log succeeds on a stream it wrote itself");
        check(w_read.history_log.size() == w.history_log.size(),
              "R4 round-trip preserves entry count");

        bool identical = w_read.history_log.size() == w.history_log.size();
        for (std::size_t i = 0; identical && i < w.history_log.size(); ++i)
        {
            const world_history_entry& a = w.history_log[i];
            const world_history_entry& b = w_read.history_log[i];
            identical = a.timestamp == b.timestamp && a.topic == b.topic &&
                        a.body == b.body && a.corp == b.corp &&
                        a.event == b.event && a.consequence == b.consequence;
        }
        check(identical, "R4 round-trip is field-identical, entry for entry, in order");
    }

    {
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_history_log(w, ss);
        std::string bytes = ss.str();
        check(bytes.size() >= 4, "R5 sanity: the stream has at least a magic's worth of bytes");
        bytes[0] = static_cast<char>(bytes[0] ^ 0xFF); // corrupt the magic's first byte

        std::stringstream bad(bytes, std::ios::in | std::ios::out | std::ios::binary);
        world w_bad;
        w_bad.history_log.push_back(world_history_entry{}); // sentinel: must survive untouched
        const bool ok = read_history_log(w_bad, bad);
        check(!ok, "R5 a wrong-magic stream is rejected");
        check(w_bad.history_log.size() == 1,
              "R5 a rejected read leaves the destination world's history_log untouched");
    }

    {
        // Right magic, wrong version.
        std::stringstream ss(std::ios::in | std::ios::out | std::ios::binary);
        write_history_log(w, ss);
        std::string bytes = ss.str();
        // Version is the second uint32_t, right after the magic.
        bytes[4] = static_cast<char>(bytes[4] + 1);

        std::stringstream bad(bytes, std::ios::in | std::ios::out | std::ios::binary);
        world w_bad;
        const bool ok = read_history_log(w_bad, bad);
        check(!ok, "R5 a wrong-version stream is rejected");
    }

    {
        // Truncated mid-header: cut off inside the count field itself (bytes
        // 8-11), so read_u32(count) fails outright.
        std::stringstream trunc(std::ios::in | std::ios::out | std::ios::binary);
        write_history_log(w, trunc);
        std::string bytes = trunc.str();
        std::string short_bytes = bytes.substr(0, 10); // magic(4) + version(4) + 2 of count's 4 bytes
        std::stringstream bad(short_bytes, std::ios::in | std::ios::out | std::ios::binary);
        world w_bad;
        const bool ok = read_history_log(w_bad, bad);
        check(!ok, "R5 a stream truncated mid-count-field is rejected");
    }

    {
        // Truncated mid-entry: valid header + a genuine (non-zero) entry
        // count, but the stream runs out partway through reading the FIRST
        // entry's fields — the scenario the header-only truncation above does
        // NOT exercise (that one fails inside the count field; this one fails
        // inside the per-entry read loop, after count was accepted).
        std::stringstream trunc(std::ios::in | std::ios::out | std::ios::binary);
        write_history_log(w, trunc);
        std::string bytes = trunc.str();
        check(bytes.size() > 12 + 8, "R5 sanity: the real log has at least one whole entry to truncate into");
        std::string short_bytes = bytes.substr(0, 12 + 8); // header(12) + only the first entry's timestamp
        std::stringstream bad(short_bytes, std::ios::in | std::ios::out | std::ios::binary);
        world w_bad;
        w_bad.history_log.push_back(world_history_entry{}); // sentinel: must survive untouched
        const bool ok = read_history_log(w_bad, bad);
        check(!ok, "R5 a stream truncated mid-entry (valid count, incomplete first entry) is rejected");
        check(w_bad.history_log.size() == 1,
              "R5 a mid-entry-truncated read also leaves the destination world's history_log untouched");
    }

    // =========================================================================
    // R6 — no regression: export_corp_blackboard / body_activity_visibility
    // (neither touched by this item) still run and return sane output.
    // =========================================================================
    {
        check(w.player_entity != null_entity, "R6 sanity: the generated world has a player corp");
        const corp_blackboard bb = export_corp_blackboard(w, w.player_entity, w.current_day_tick);
        check(bb.corp == w.player_entity, "R6 export_corp_blackboard still resolves the requested corp");
        check(!bb.facts.empty(), "R6 export_corp_blackboard still returns facts (own cash at least)");

        const activity_vis v = body_activity_visibility(w, kepler_id, w.current_day_tick);
        check(v == activity_vis::visible,
              "R6 body_activity_visibility still reports the home body visible (player presence)");
    }

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
