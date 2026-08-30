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
//   S3  `world_params` and the generation report survive.
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
    w.pool_for(corp, body).quantities[ri(resource_type::iron_ore)] = 99.5f;
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

    generation_report::body_entry be;
    be.name         = "Vhessari Prime";
    be.id           = 41;
    be.is_homeworld = true;
    e.report.bodies.push_back(be);

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

        check(le.params.seed == env.params.seed && le.params.abundance == env.params.abundance
                  && le.params.epoch_year == env.params.epoch_year,
              "S3 world_params survives");
        check(le.report.bodies.size() == 1 && le.report.bodies[0].name == "Vhessari Prime"
                  && le.report.bodies[0].id == 41 && le.report.bodies[0].is_homeworld,
              "S3 the generation report's body entry survives (name, id, homeworld flag)");

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
    }

    std::remove(k_path);

    std::printf("\n%s (%d failure(s))\n", g_failures == 0 ? "PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
