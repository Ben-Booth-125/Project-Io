// Headless round-trip harness for the SAVE ENVELOPE (BL-685; NR-708). No SDL,
// no Lua, no renderer -- but it does link ImGui, and that is the whole reason it
// is hand-declared in CMakeLists.txt rather than reached by the generic
// tools/verify glob.
//
// WHY IT LINKS IMGUI, on the font_glyph_harness precedent. The envelope lives in
// src/core/save_game.{hpp,cpp}, which includes ui/ui_state.hpp and so <imgui.h>.
// The headless harness tier links io_world_obj and excludes ImGui BY
// CONSTRUCTION -- which is exactly why the envelope has never been testable from
// it, and why NR-708 records that NO envelope field had round-trip coverage. It
// opens no window and creates no context: it writes a file and reads it back.
//
// `save_roundtrip.cpp` covers the WORLD half (world_save.{hpp,cpp}) and is
// unaffected; this is its missing counterpart on the outer IOSG stream.
//
//   S1  The whole file round-trips: write_save_game -> read_save_game returns a
//       world and an envelope equal to the ones written.
//   S2  The CLOCK survives -- every field distinct, so a field read into its
//       neighbour shows.
//   S3  `world_params` and the generation report survive -- including the
//       per-body settlement record and its BL-766 urban fields.
//   S4  The app-owned histories survive, values and order.
//   S5  The ui_state slice survives -- the two enums and the nine floats, each a
//       distinct non-dyadic value, so a swapped pair cannot pass.
//   S6  BL-685: `world::exchanges` survives THROUGH THE OUTER FILE -- rows, the
//       wrap cursor and the lifetime counter -- including a WRAPPED ring, whose
//       stored order is not its chronological order.
//   S7  A version mismatch is refused WHOLE, and both destinations are left
//       exactly as they were (the atomicity `read_save_game` documents).
//
// Kept outside src/ so the CMake game glob does not pull it into the build.

#include "core/save_game.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* what)
{
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond)
        ++g_failures;
}

std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

/// Where the harness writes. A plain relative path in the working directory:
/// %TEMP% is banned as a harness target, and the file is removed on the way out.
const char* k_path = "save_envelope_roundtrip.iosave";

/// A small hand-built world. NOT a generated one -- the world half already has
/// its own round-trip harness, and generating here would spend twenty seconds
/// proving something `save_roundtrip.cpp` proves better.
world make_world()
{
    world w;

    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};

    const entity_id market = w.create_entity();
    {
        market_component mc;
        mc.body = body;
        mc.base_price[ri(resource_type::iron_ore)] = 4.25f;
        mc.price = mc.base_price;
        w.markets[market] = mc;
    }

    const entity_id corp = w.create_entity();
    {
        corporation_component cc;
        cc.balance   = 12345.5f;
        cc.is_player = true;
        w.corporations[corp] = cc;
    }
    w.pool_at(corp, pool_key_for_body(w, body)).quantities[ri(resource_type::iron_ore)] = 99.5f;
    w.current_econ_tick = 3;

    return w;
}

/// Fill the exchange ring with @p n distinctive rows. Every field differs from
/// every other field of the same row, and from the same field of every other
/// row: a value of 0 round-trips even when it is read into the wrong member,
/// which is the defect a fixture like this exists to catch.
void seed_exchanges(world& w, std::size_t n)
{
    const entity_id market = w.markets.begin()->first;
    for (std::size_t i = 0; i < n; ++i)
    {
        exchange_record e;
        e.tick       = static_cast<int>(1000 + i);
        e.market     = market;
        e.resource   = static_cast<resource_type>(i % resource_count);
        e.quantity   = 3.5f + static_cast<float>(i);
        e.unit_price = 0.25f + static_cast<float>(i) * 2.0f;
        e.seller     = static_cast<entity_id>(500 + i);
        e.buyer      = (i % 3 == 0) ? null_entity : static_cast<entity_id>(900 + i);
        w.exchanges.push(e);
    }
}

bool same_row(const exchange_record& a, const exchange_record& b)
{
    return a.tick == b.tick && a.market == b.market && a.resource == b.resource
        && a.quantity == b.quantity && a.unit_price == b.unit_price && a.seller == b.seller
        && a.buyer == b.buyer;
}

/// Field-wise, and STORED-ORDER-wise. Comparing `oldest_first` alone would pass
/// a reader that dropped the wrap cursor and rotated the ring, because the
/// chronological read would still line up if the rows were re-sorted -- so the
/// raw vector and the cursors are compared too.
bool same_ring(const exchange_record_ring& a, const exchange_record_ring& b)
{
    if (a.entries.size() != b.entries.size() || a.next != b.next || a.total != b.total)
        return false;
    for (std::size_t i = 0; i < a.entries.size(); ++i)
        if (!same_row(a.entries[i], b.entries[i]))
            return false;
    return true;
}

/// An envelope with a DISTINCT value in every field. Nothing here is 0, 1 or
/// equal to its neighbour.
save_envelope make_envelope()
{
    save_envelope e;

    e.sim_tick     = 8123456789ull;
    e.day_tick     = 4242ull;
    e.econ_tick    = 77ull;
    e.elapsed_days = 1234.5;
    e.speed        = 3;

    e.params.seed        = 0xC0FFEEu;
    e.params.abundance   = abundance_level::sparse;
    e.params.epoch_year  = -350;
    // BL-760 (2): a year field must differ from the authored default, or a run
    // that left it there would round-trip clean even if the writer swapped its
    // order — an assertion that cannot fail is not an assertion. 137 is
    // non-default. (Its old twin, the two-span arc's year field, left the
    // record at v20.)
    e.params.prehistory_years = 137;
    e.params.body_count       = 7;
    // BL-1047 (save_game_version 20): the fields that choose the history. Each
    // is non-default and distinct from its same-typed neighbours, so a
    // transposition among the four int64 years or the three bools shows.
    e.params.era_seed                       = 0x5EEDu;
    // BL-1083 (save_game_version 22): the four per-span reroll counters, each
    // distinct from its neighbours AND from era_seed, so a transposition among
    // the five consecutive u32s shows.
    e.params.span_seed[0]                   = 0x5A0Du;
    e.params.span_seed[1]                   = 0x5A1Du;
    e.params.span_seed[2]                   = 0x5A2Du;
    e.params.span_seed[3]                   = 0x5A3Du;
    e.params.empires_start_year             = -777;
    e.params.empires_stop_year              = 1111;
    e.params.exploration_sim_enabled        = false;
    e.params.exploration_stop_year          = 1555;
    e.params.industrialisation_span_enabled = true;
    e.params.industrialisation_stop_year    = 1888;
    e.params.resume_seeds_corridor_tier     = false;
    // preferences is the SEVENTH field and the one most exposed to the defect
    // this row exists for: save_game writes EIGHT consecutive same-typed `lean`
    // enums plus roll[3], all read back under one bound, so any two of them
    // could be transposed and every default-valued round-trip would stay green.
    // Each is therefore given a DISTINCT value, so a swap of any pair shows.
    e.params.preferences.star         = lean::low;
    e.params.preferences.world_size   = lean::mid;
    e.params.preferences.interior     = lean::high;
    e.params.preferences.metal        = lean::low;
    e.params.preferences.ocean        = lean::high;
    e.params.preferences.oxygen_story = lean::mid;
    e.params.preferences.coal_basins  = lean::high;
    e.params.preferences.drawdown     = lean::low;
    // BL-839 (save_game_version 12): the turbulence lean is the NINTH lean and
    // the newest, so it is the one most exposed to a writer/reader drift. Given
    // `high` -- distinct from its neighbour `drawdown` above and from the field
    // default (`mid`), so neither a transposition nor a silently-unread field
    // round-trips clean.
    e.params.preferences.history_turbulence = lean::high;
    e.params.preferences.roll[0]      = 11;
    e.params.preferences.roll[1]      = 22;
    e.params.preferences.roll[2]      = 33;

    generation_report::body_entry be;
    be.name         = "Vhessari Prime";
    be.id           = 41;
    be.is_homeworld = true;

    // BL-766, the urban record on `region`, and the S3 precedent this file set
    // for BL-747: two ints and an int64 appended to a record whose other ints
    // are also small — so every one of them gets a DISTINCT, non-default value
    // and the population is distinct from the urban population, or a writer
    // that emitted `centres_razed` where `centres` belongs would round-trip
    // clean and this row would assert nothing.
    //
    // Two regions, and the SECOND is the one carrying the razed count, so a
    // reader that dropped a field would desynchronise the vector rather than
    // merely mis-set one member.
    region r0;
    r0.name                 = "Ashen Quarter";
    r0.anchor               = 913;
    r0.farm_q               = 641;
    r0.population           = 84213;
    // BL-748's field, added in the MERGE rather than by its own agent: it could
    // not build this harness from a worktree, so the merged v6 stream carried a
    // field with no round-trip assertion at all. 47 and 213 are distinct from
    // each other and from every other int in the record.
    r0.industrial_lag_years = 47;
    r0.centres              = 3;
    r0.centres_razed        = 0;
    r0.urban_population     = 31775;
    // BL-777's field, at a DISTINCT NON-DEFAULT value on both regions, which is
    // the whole point of setting it here: `region_domain::land` is the struct
    // default and a default round-trips clean even through a writer that
    // transposed the field or dropped it. r0 takes `coastal_water` and r1 takes
    // `open_ocean` — the extreme of the range, so the `r_enum` bound
    // (`max_region_dom`) is exercised at its edge rather than in its middle.
    //
    // That `open_ocean` is a value GENERATION no longer produces is not a
    // contradiction: this harness asserts the SERIALISER carries every value of
    // the type, and the generation-side claim (no region anchors on open ocean)
    // is sim_water_census's to make.
    r0.domain               = region_domain::coastal_water;
    // BL-835's field, and the SECOND time this harness has met the same shape
    // of gap: the comment above records BL-748 arriving through a merge with no
    // assertion because its agent could not build this harness from a worktree.
    // `army_stock` arrived exactly that way. It is written directly beside
    // `manpower_stock`, which is the whole hazard - two adjacent i64s that both
    // default to zero transpose CLEANLY, so a writer that swapped them would
    // pass every other check in this file. Four distinct values, none equal to
    // any other int in the record, is what makes the swap detectable.
    r0.manpower_stock       = 5104;
    r0.army_stock           = 1662;
    region r1;
    r1.name                 = "Torrend Reach";
    r1.anchor               = 274;
    r1.farm_q               = 388;
    r1.population           = 19507;
    r1.industrial_lag_years = 213;
    r1.centres              = 1;
    r1.centres_razed        = 5;
    r1.urban_population     = 12099;
    r1.domain               = region_domain::open_ocean;
    r1.manpower_stock       = 2873;
    r1.army_stock           = 941;
    be.settlement.regions.push_back(r0);
    be.settlement.regions.push_back(r1);
    be.settlement.median_industrial_year = 1843;
    be.settlement.urban_map_drawn        = true;

    // THE PLAYBACK RECORD, save_game_version 11 (BL-817). Distinct values in
    // every field of every entry, and DIFFERENT BETWEEN the two entries of each
    // array, for the reason the army-pool check above states: a dropped field
    // desynchronises the whole stream and fails everything after it, but a
    // TRANSPOSED pair reads back clean unless the two sides differ.
    be.prehistory_timelapse.region_stride = 2;
    be.prehistory_timelapse.start_year    = -4000;
    be.prehistory_timelapse.years         = 4000;
    be.prehistory_timelapse.changes.push_back(owner_change{-4000, 0, 3});
    be.prehistory_timelapse.changes.push_back(owner_change{-1200, 1, 7});

    be.prehistory_timelapse.steps.push_back(timelapse_step{-4000, 0, 2});
    be.prehistory_timelapse.steps.push_back(timelapse_step{-1200, 2, 1});

    polity_sample ps0; ps0.population = 84213; ps0.polity = 3; ps0.regions = 11;
    ps0.cap_military = 2; ps0.cap_materials = 5;
    polity_sample ps1; ps1.population = 19507; ps1.polity = 7; ps1.regions = 4;
    ps1.cap_military = 6; ps1.cap_materials = 1;
    polity_sample ps2; ps2.population = 550021; ps2.polity = 3; ps2.regions = 29;
    ps2.cap_military = 4; ps2.cap_materials = 3;
    // BL-1080, save_game_version 21 -- the industry points at the sample's
    // tail. Distinct per sample, one of them past 2^32 so a narrowing on either
    // side of the wire is visible, and one zero (the pre-span case).
    ps0.industry_points = 0;
    ps1.industry_points = 7340031;
    ps2.industry_points = 9876543210123LL;
    be.prehistory_timelapse.samples.push_back(ps0);
    be.prehistory_timelapse.samples.push_back(ps1);
    be.prehistory_timelapse.samples.push_back(ps2);

    culture_change cc0;
    cc0.year = -4000; cc0.region = 0;
    cc0.id[0] = 2; cc0.id[1] = 5; cc0.id[2] = -1;
    cc0.weight_q[0] = 610; cc0.weight_q[1] = 300; cc0.weight_q[2] = 0;
    cc0.other_q = 90;
    culture_change cc1;
    cc1.year = -1200; cc1.region = 1;
    cc1.id[0] = 9; cc1.id[1] = -1; cc1.id[2] = -1;
    cc1.weight_q[0] = 1000; cc1.weight_q[1] = 0; cc1.weight_q[2] = 0;
    cc1.other_q = 0;
    be.prehistory_timelapse.culture_changes.push_back(cc0);
    be.prehistory_timelapse.culture_changes.push_back(cc1);
    // BL-916, save_game_version 13 -- the event layer. Every field distinct
    // from its neighbours so a transposition between the three u16 slots is
    // visible; one entry carries the none sentinel so it survives the range
    // check as the sentinel and not as an out-of-stride index.
    be.prehistory_timelapse.events.push_back(lapse_event{-3900, 0, 0, 3, lapse_event_none});
    be.prehistory_timelapse.events.push_back(lapse_event{-1200, 2, 1, 7, 3});
    be.prehistory_timelapse.events.push_back(lapse_event{-1150, 5, 0, 2, 1});
    // BL-1106, save_game_version 22 -- the name table. Two names per list so
    // an off-by-one between the lists is visible, an empty string in the
    // middle so a zero-length prefix survives, and a polity_creed row with the
    // -1 sentinel beside real indices (the reader's floor is -1).
    be.prehistory_timelapse.civilisation_name = {"Vharenu", "", "Kesh-Tolm"};
    be.prehistory_timelapse.creed_name        = {"Solun", "Irathe"};
    be.prehistory_timelapse.polity_creed      = {-1, 1, -1, 0, 1, -1, -1, 0};

    // BL-1068, save_game_version 18 -- the Industrialisation span's own
    // record, written AFTER exploration_timelapse. Distinct from the
    // prehistory record in every field a misplaced read would land on.
    be.industrialisation_timelapse.region_stride = 2;
    be.industrialisation_timelapse.start_year    = 1660;
    be.industrialisation_timelapse.years         = 300;
    be.industrialisation_timelapse.changes.push_back(owner_change{1661, 1, 4});
    be.industrialisation_timelapse.changes.push_back(owner_change{1873, 0, 9});
    be.industrialisation_timelapse.events.push_back(lapse_event{1914, 1, 1, 9, 6});
    // BL-1080, save_game_version 21 -- a furnace crossing (kind 17, the
    // highest kind: the reader's range check must admit it) and an industry
    // sample on the round the industry belongs to.
    be.industrialisation_timelapse.events.push_back(
        lapse_event{1931, static_cast<uint8_t>(lapse_event_kind::furnace_lit), 0, 4, lapse_event_none});
    be.industrialisation_timelapse.steps.push_back(timelapse_step{1931, 0, 1});
    {
        polity_sample ip; ip.population = 3100000; ip.polity = 4; ip.regions = 2;
        ip.cap_military = 3; ip.cap_materials = 6; ip.industry_points = 41250000;
        be.industrialisation_timelapse.samples.push_back(ip);
    }

    e.report.bodies.push_back(be);
    e.report.industrialisation_years     = 300;
    e.report.industrialisation_battles   = 417;
    e.report.industrialisation_conquests = 63;
    e.report.industrialisation_foundings = 29;

    // BL-768's three report counters, at DISTINCT non-default values.
    //
    // ADDED ON MERGE, and that is now a pattern rather than an accident: this is
    // the THIRD save-format change this sprint to land with its fields written
    // and read but never ASSERTED, because no worktree agent can build this
    // harness (it links imgui, so neither headless builder compiles it) and the
    // authoring agent therefore cannot extend it. The field goes in, the version
    // bumps, and the only thing standing between a transposition and a silent
    // corrupt load is that somebody downstream noticed. Whoever owns the builder
    // gap should treat this as its cost.
    e.report.prehistory_corridors = 3187;
    e.report.prehistory_junctions = 124;
    e.report.markets_from_trade   = 41;

    e.balance_history     = { 1.5f, -2.25f, 3.125f };
    e.income_history      = { 10.5f, 11.75f };
    e.expenditure_history = { -4.5f };

    e.market_history[7][ri(resource_type::coal)].price  = { 2.5f, 2.75f };
    e.market_history[7][ri(resource_type::coal)].supply = { 40.5f };
    e.market_history[7][ri(resource_type::coal)].demand = { 12.25f, 13.5f, 14.75f };

    e.building_rank_hist.push_back({ { 5, 2 }, { 6, 1 } });
    e.building_rank_hist.push_back({ { 5, 3 } });

    e.primary_level     = canvas_level::planetary;
    e.overlay           = overlay_mode::none;
    e.active_body       = 17;
    e.selected_entity   = 23;
    e.selected_province = 91;

    // Nine distinct non-dyadic-ish floats: a reader that swapped pan_x and pan_y,
    // or read a zoom into a pan, cannot pass.
    e.solar_zoom      = 1.3f;  e.solar_pan_x     = -2.7f;  e.solar_pan_y     = 3.1f;
    e.circum_zoom     = 4.9f;  e.circum_pan_x    = -5.3f;  e.circum_pan_y    = 6.7f;
    e.planetary_zoom  = 7.1f;  e.planetary_pan_x = -8.9f;  e.planetary_pan_y = 9.3f;

    return e;
}

bool same_market_history(const ui::market_plot_history& a, const ui::market_plot_history& b)
{
    if (a.size() != b.size())
        return false;
    for (const auto& [mid, series] : a)
    {
        const auto it = b.find(mid);
        if (it == b.end())
            return false;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (series[r].price != it->second[r].price || series[r].supply != it->second[r].supply
                || series[r].demand != it->second[r].demand)
                return false;
    }
    return true;
}

} // namespace

int main()
{
    std::printf("Save ENVELOPE round-trip (BL-685 / NR-708) harness\n");

    // -----------------------------------------------------------------------
    // S1-S6 -- the whole file, with a ring that has NOT wrapped
    // -----------------------------------------------------------------------
    {
        world w = make_world();
        seed_exchanges(w, 12);
        const save_envelope env = make_envelope();

        check(write_save_game(k_path, w, env), "S1 write_save_game wrote the file");

        world         lw;
        save_envelope le;
        check(read_save_game(k_path, lw, le), "S1 read_save_game read it back whole");

        check(le.sim_tick == env.sim_tick && le.day_tick == env.day_tick
                  && le.econ_tick == env.econ_tick && le.elapsed_days == env.elapsed_days
                  && le.speed == env.speed,
              "S2 the clock survives (five distinct fields)");

        // EVERY field, not three of six. prehistory_years and (until v20) the
        // two-span arc's year field were both unasserted on a record whose version was just bumped to 4,
        // and both default to 400 — so an order swap in the writer round-tripped
        // clean and the comment guarding it was the only check (BL-760 (2)).
        check(le.params.seed == env.params.seed && le.params.abundance == env.params.abundance
                  && le.params.epoch_year == env.params.epoch_year
                  && le.params.prehistory_years == env.params.prehistory_years
                  && le.params.body_count == env.params.body_count,
              "S3 world_params survives (the five original scalar fields)");
        const world_preferences& lp = le.params.preferences;
        const world_preferences& ep = env.params.preferences;
        check(lp.star == ep.star && lp.world_size == ep.world_size
                  && lp.interior == ep.interior && lp.metal == ep.metal
                  && lp.ocean == ep.ocean && lp.oxygen_story == ep.oxygen_story
                  && lp.coal_basins == ep.coal_basins && lp.drawdown == ep.drawdown
                  && lp.history_turbulence == ep.history_turbulence
                  && lp.roll[0] == ep.roll[0] && lp.roll[1] == ep.roll[1]
                  && lp.roll[2] == ep.roll[2],
              "S3 world_params.preferences survives (9 leans + roll[3], each distinct)");
        // Pinned to a LITERAL, not compared to the original: a writer and
        // reader that both omitted the field would compare equal at the default
        // and say nothing. BL-839's field default is `mid`, so `high` here can
        // only have arrived through the seam.
        check(lp.history_turbulence == lean::high,
              "S3 the BL-839 turbulence lean survives (high, not the mid default)");
        // The differential the requirement actually asks for: the two year
        // fields must come back DISTINCT and in the right slots. Comparing
        // round-tripped-to-original cannot catch a swap if the writer and reader
        // swap symmetrically, so this pins them to their literal values.
        check(le.params.prehistory_years == 137 && le.params.epoch_year == -350,
              "S3 the epoch and prehistory fields land in the RIGHT slots (-350/137)");
        // BL-1047, pinned to literals for the same reason.
        check(le.params.era_seed == 0x5EEDu
                  && le.params.empires_start_year == -777
                  && le.params.empires_stop_year == 1111
                  && le.params.exploration_stop_year == 1555
                  && le.params.industrialisation_stop_year == 1888,
              "S3 era_seed and the four span years survive in their slots (BL-1047, v20)");
        // BL-1083, pinned to literals: five consecutive u32s (era_seed then the
        // four span seeds) are exactly the shape a symmetric transposition survives.
        check(le.params.span_seed[0] == 0x5A0Du && le.params.span_seed[1] == 0x5A1Du
                  && le.params.span_seed[2] == 0x5A2Du && le.params.span_seed[3] == 0x5A3Du,
              "S3 the four per-span seeds survive in their slots (BL-1083, v22)");
        check(!le.params.exploration_sim_enabled
                  && le.params.industrialisation_span_enabled
                  && !le.params.resume_seeds_corridor_tier,
              "S3 the three span switches survive in their slots (false/true/false, BL-1047)");
        // Pinned to literals, not compared to the original: three consecutive
        // int64s of the same type are exactly the shape a symmetric writer/reader
        // transposition survives.
        check(le.report.prehistory_corridors == 3187
                  && le.report.prehistory_junctions == 124
                  && le.report.markets_from_trade == 41,
              "S3 the BL-768 report counters survive (corridors / junctions / markets, each distinct)");
        check(le.report.bodies.size() == 1 && le.report.bodies[0].name == "Vhessari Prime"
                  && le.report.bodies[0].id == 41 && le.report.bodies[0].is_homeworld,
              "S3 the generation report's body entry survives (name, id, homeworld flag)");

        // BL-766: the urban record. Pinned to LITERALS rather than compared
        // field-to-field, for the reason the year-slot row above gives — a
        // writer and reader that swap symmetrically compare equal to each other
        // and are still wrong.
        const bool settlement_ok =
            le.report.bodies.size() == 1
            && le.report.bodies[0].settlement.regions.size() == 2
            && le.report.bodies[0].settlement.regions[0].name == "Ashen Quarter"
            && le.report.bodies[0].settlement.regions[0].population == 84213
            && le.report.bodies[0].settlement.regions[0].industrial_lag_years == 47
            && le.report.bodies[0].settlement.regions[1].industrial_lag_years == 213
            && le.report.bodies[0].settlement.regions[0].centres == 3
            && le.report.bodies[0].settlement.regions[0].centres_razed == 0
            && le.report.bodies[0].settlement.regions[0].urban_population == 31775
            && le.report.bodies[0].settlement.regions[1].name == "Torrend Reach"
            && le.report.bodies[0].settlement.regions[1].population == 19507
            && le.report.bodies[0].settlement.regions[1].centres == 1
            && le.report.bodies[0].settlement.regions[1].centres_razed == 5
            && le.report.bodies[0].settlement.regions[1].urban_population == 12099
            // BL-777, save_game_version 7: the appended domain byte, distinct
            // and non-default on BOTH regions and different BETWEEN them.
            && le.report.bodies[0].settlement.regions[0].domain
                   == region_domain::coastal_water
            && le.report.bodies[0].settlement.regions[1].domain
                   == region_domain::open_ocean;
        check(settlement_ok,
              "S3 the region urban record and domain survive (centres / razed / urban heads "
              "/ domain, "
              "each distinct, both regions)");
        // BL-835, save_game_version 10. Asserted as a PAIR against its
        // neighbour: the failure this catches is not a dropped field (which
        // would desynchronise the whole stream and break every check below it)
        // but a TRANSPOSED one, and only distinct values on both sides can
        // tell those apart.
        check(le.report.bodies.size() == 1
                  && le.report.bodies[0].settlement.regions.size() == 2
                  && le.report.bodies[0].settlement.regions[0].manpower_stock == 5104
                  && le.report.bodies[0].settlement.regions[0].army_stock == 1662
                  && le.report.bodies[0].settlement.regions[1].manpower_stock == 2873
                  && le.report.bodies[0].settlement.regions[1].army_stock == 941,
              "S3 the army pool survives BESIDE the manpower pool it is raised "
              "from, unswapped (BL-835, both regions)");
        // BL-817, save_game_version 11 -- the playback record. Written as one
        // predicate over every field of every entry, because the failure worth
        // catching here is a transposition inside a fixed-size array (the three
        // culture slots) and only distinct values on both sides can see it.
        {
            const era_timelapse& t = le.report.bodies.empty()
                                   ? env.report.bodies[0].prehistory_timelapse
                                   : le.report.bodies[0].prehistory_timelapse;
            const era_timelapse& o = env.report.bodies[0].prehistory_timelapse;
            bool play_ok = le.report.bodies.size() == 1
                        && t.steps.size() == o.steps.size()
                        && t.samples.size() == o.samples.size()
                        && t.culture_changes.size() == o.culture_changes.size();
            for (std::size_t i = 0; play_ok && i < o.steps.size(); ++i)
                play_ok = t.steps[i].year == o.steps[i].year
                       && t.steps[i].first_sample == o.steps[i].first_sample
                       && t.steps[i].sample_count == o.steps[i].sample_count;
            for (std::size_t i = 0; play_ok && i < o.samples.size(); ++i)
                play_ok = t.samples[i].population == o.samples[i].population
                       && t.samples[i].polity == o.samples[i].polity
                       && t.samples[i].regions == o.samples[i].regions
                       && t.samples[i].cap_military == o.samples[i].cap_military
                       && t.samples[i].cap_materials == o.samples[i].cap_materials
                       && t.samples[i].industry_points == o.samples[i].industry_points;
            for (std::size_t i = 0; play_ok && i < o.culture_changes.size(); ++i)
            {
                play_ok = t.culture_changes[i].year == o.culture_changes[i].year
                       && t.culture_changes[i].region == o.culture_changes[i].region
                       && t.culture_changes[i].other_q == o.culture_changes[i].other_q;
                for (int k = 0; play_ok && k < timelapse_culture_slots; ++k)
                    play_ok = t.culture_changes[i].id[k] == o.culture_changes[i].id[k]
                           && t.culture_changes[i].weight_q[k]
                                  == o.culture_changes[i].weight_q[k];
            }
            check(play_ok,
                  "S3 the playback record survives whole -- steps, per-polity samples and "
                  "the culture-share change list, slot for slot (BL-817)");
            bool ev_ok = t.events.size() == o.events.size();
            for (std::size_t i = 0; ev_ok && i < o.events.size(); ++i)
                ev_ok = t.events[i].year   == o.events[i].year
                     && t.events[i].kind   == o.events[i].kind
                     && t.events[i].region == o.events[i].region
                     && t.events[i].polity == o.events[i].polity
                     && t.events[i].other  == o.events[i].other;
            check(ev_ok && o.events.size() == 3,
                  "S3 the event layer survives whole, field for field, sentinel included (BL-916)");
            // BL-1106, save_game_version 22 -- the name table, list for list
            // and slot for slot, the empty name and the -1 sentinel included.
            check(t.civilisation_name == o.civilisation_name
                      && t.creed_name == o.creed_name
                      && t.polity_creed == o.polity_creed
                      && o.civilisation_name.size() == 3 && o.creed_name.size() == 2
                      && o.polity_creed.size() == 8,
                  "S3 the name table survives whole -- civilisation and creed names in "
                  "index order, the empty name, and polity_creed with its -1 sentinel "
                  "(BL-1106)");
        }
        // BL-1068, save_game_version 18 -- the Industrialisation span's own
        // record and counters, field for field, and NOT the prehistory
        // record read into the wrong slot.
        {
            bool ind_ok = le.report.bodies.size() == 1;
            if (ind_ok)
            {
                const era_timelapse& t = le.report.bodies[0].industrialisation_timelapse;
                const era_timelapse& o = env.report.bodies[0].industrialisation_timelapse;
                ind_ok = t.region_stride == 2 && t.start_year == 1660 && t.years == 300
                      && t.changes.size() == o.changes.size()
                      && t.events.size() == 2 && t.events[0].year == 1914
                      && t.events[0].region == 1 && t.events[0].polity == 9
                      && t.events[0].other == 6
                      // BL-1080: the furnace crossing and the industry sample.
                      && t.events[1].year == 1931
                      && t.events[1].kind == static_cast<uint8_t>(lapse_event_kind::furnace_lit)
                      && t.events[1].region == 0 && t.events[1].polity == 4
                      && t.events[1].other == lapse_event_none
                      && t.samples.size() == 1 && t.samples[0].industry_points == 41250000
                      && t.samples[0].cap_materials == 6;
                for (std::size_t i = 0; ind_ok && i < o.changes.size(); ++i)
                    ind_ok = t.changes[i].year == o.changes[i].year
                          && t.changes[i].region == o.changes[i].region
                          && t.changes[i].owner == o.changes[i].owner;
            }
            check(ind_ok && le.report.industrialisation_years == 300
                      && le.report.industrialisation_battles == 417
                      && le.report.industrialisation_conquests == 63
                      && le.report.industrialisation_foundings == 29,
                  "S3 the Industrialisation span's record and its four counters survive "
                  "whole (BL-1068)");
        }
        check(le.report.bodies.size() == 1
                  && le.report.bodies[0].settlement.median_industrial_year == 1843
                  && le.report.bodies[0].settlement.urban_map_drawn,
              "S3 settlement_state's own scalars survive, urban_map_drawn included");

        check(le.balance_history == env.balance_history
                  && le.income_history == env.income_history
                  && le.expenditure_history == env.expenditure_history,
              "S4 the three app histories survive, values and order");
        check(same_market_history(le.market_history, env.market_history),
              "S4 the per-market price/supply/demand series survive");
        check(le.building_rank_hist == env.building_rank_hist,
              "S4 the building rank window survives");

        check(le.primary_level == env.primary_level && le.overlay == env.overlay
                  && le.active_body == env.active_body
                  && le.selected_entity == env.selected_entity
                  && le.selected_province == env.selected_province,
              "S5 the ui_state slice's rung, lens and selection survive");
        check(le.solar_zoom == env.solar_zoom && le.solar_pan_x == env.solar_pan_x
                  && le.solar_pan_y == env.solar_pan_y && le.circum_zoom == env.circum_zoom
                  && le.circum_pan_x == env.circum_pan_x && le.circum_pan_y == env.circum_pan_y
                  && le.planetary_zoom == env.planetary_zoom
                  && le.planetary_pan_x == env.planetary_pan_x
                  && le.planetary_pan_y == env.planetary_pan_y,
              "S5 all nine camera floats survive, unswapped");

        check(same_ring(lw.exchanges, w.exchanges),
              "S6 the exchange record survives the whole file (12 rows, cursors included)");
        check(lw.exchanges.size() == 12 && lw.exchanges.total == 12,
              "S6 and it is not vacuously empty on either side");
    }

    // -----------------------------------------------------------------------
    // S6 -- a WRAPPED ring, whose stored order is not its chronological order
    // -----------------------------------------------------------------------
    // The case the cursors exist for. A reader that dropped `next` would still
    // pass the check above (an unwrapped ring has next == 0) and would hand every
    // consumer the history rotated at the wrap point.
    {
        world w = make_world();
        seed_exchanges(w, exchange_record_ring::capacity + 5);
        const save_envelope env = make_envelope();

        check(w.exchanges.next == 5, "S6 the fixture ring really has wrapped");

        world         lw;
        save_envelope le;
        check(write_save_game(k_path, w, env) && read_save_game(k_path, lw, le),
              "S6 a wrapped ring writes and reads");
        check(same_ring(lw.exchanges, w.exchanges),
              "S6 a WRAPPED ring survives with its wrap cursor and lifetime count");

        bool chronological = true;
        for (std::size_t i = 1; i < lw.exchanges.size(); ++i)
            chronological = chronological
                && lw.exchanges.oldest_first(i).tick
                       == lw.exchanges.oldest_first(i - 1).tick + 1;
        check(chronological,
              "S6 the loaded ring still reads chronologically across the wrap");
    }

    // -----------------------------------------------------------------------
    // S7 -- a version mismatch is refused whole, destinations untouched
    // -----------------------------------------------------------------------
    {
        world w = make_world();
        seed_exchanges(w, 4);
        write_save_game(k_path, w, make_envelope());

        // The envelope version is the second uint32 of the file, immediately
        // after the IOSG magic. Bump it past what this build accepts.
        {
            std::fstream f(k_path, std::ios::binary | std::ios::in | std::ios::out);
            f.seekp(4, std::ios::beg);
            const uint32_t bad = save_game_version + 1;
            f.write(reinterpret_cast<const char*>(&bad), sizeof(bad));
        }

        world         lw = make_world();
        save_envelope le;
        le.speed        = 99;
        le.selected_entity = 4242;
        seed_exchanges(lw, 2);
        const std::size_t rows_before  = lw.exchanges.size();
        const uint64_t    total_before = static_cast<uint64_t>(lw.exchanges.total);

        check(!read_save_game(k_path, lw, le), "S7 a bumped envelope version is REFUSED");
        check(le.speed == 99 && le.selected_entity == 4242,
              "S7 the destination envelope is untouched by the refusal");
        check(lw.exchanges.size() == rows_before
                  && static_cast<uint64_t>(lw.exchanges.total) == total_before,
              "S7 the destination world is untouched by the refusal");

        // And a bad magic, one field earlier.
        {
            std::fstream f(k_path, std::ios::binary | std::ios::in | std::ios::out);
            f.seekp(0, std::ios::beg);
            const uint32_t bad = 0xDEADBEEFu;
            f.write(reinterpret_cast<const char*>(&bad), sizeof(bad));
        }
        check(!read_save_game(k_path, lw, le), "S7 a bad magic is refused too");

        // BL-916, save_game_version 13: the IMMEDIATE previous format, PINNED
        // TO A LITERAL. A v12 stream has no event-list length where v13 expects
        // one, so a reader that accepted it would take the next body entry's
        // name length as an event count. Refused whole. The literal is a PAST
        // format, which is the only kind this file pins (save_roundtrip.cpp's
        // rule): it can never be broken by a later bump, only made older.
        static_assert(save_game_version >= 13,
                      "BL-916 released layout 13; S8 names v12 as a refused predecessor");
        {
            write_save_game(k_path, w, make_envelope());
            std::fstream f(k_path, std::ios::binary | std::ios::in | std::ios::out);
            f.seekp(4, std::ios::beg);
            const uint32_t v12 = 12;
            f.write(reinterpret_cast<const char*>(&v12), sizeof(v12));
        }
        le.speed = 99;
        check(!read_save_game(k_path, lw, le) && le.speed == 99,
              "S8 a v12-versioned stream (pre event layer) is refused with the destination untouched");
    }

    std::remove(k_path);

    std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
