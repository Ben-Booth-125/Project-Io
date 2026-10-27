// ---------------------------------------------------------------------------
// BL-1437 — obsolete unit types. No SDL/Lua.
// ---------------------------------------------------------------------------
// MILITARY.md § Land — the classes (Ben, 2026-10-27): a nation marks a roster row
// obsolete and Recruit hides it; every row below the modern band starts obsolete;
// a rival nation's scorer never raises an obsolete row.
//
//   O1  THE DEFAULTS: every pre-modern row reads obsolete, every modern row live.
//   O2  THE VERB flips one row for the seat's nation, and back; the flips stay sorted.
//   O3  THE VERB VALIDATES: an unknown row, an order other than 0/1, a no-op and a
//       non-seat nation are each refused and change nothing.
//   O4  THE FLIPS SURVIVE A SAVE and hash: an untouched world hashes as before.
//
// Run: obsolete_units
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cstdio>
#include <sstream>
#include <string>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

corp_command_result mark(world& w, const recipe_registry& reg, entity_id nation, uint16_t row,
                         uint32_t order)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = nation; cmd.verb = corp_verb::set_unit_obsolete;
    cmd.unit_type = row; cmd.order = order;
    return apply_corp_command(w, reg, cmd);
}

} // namespace

int main()
{
    std::printf("=== obsolete unit types (BL-1437) ===\n");
    const recipe_registry reg{};
    const std::vector<roster_row>& table = unit_roster_table();

    world w;
    const entity_id seat  = w.create_entity();
    const entity_id rival = w.create_entity();
    w.nations[seat]  = nation_component{};
    w.nations[rival] = nation_component{};
    w.player_entity  = seat;

    std::printf("\nO1 — the defaults\n");
    {
        bool pre = true, modern = true;
        int modern_rows = 0;
        for (std::size_t i = 0; i < table.size(); ++i)
        {
            const bool o = roster_row_obsolete(w, seat, static_cast<uint16_t>(i));
            if (table[i].band == roster_band::modern) { ++modern_rows; if (o) modern = false; }
            else if (!o) pre = false;
        }
        check(pre, "O1a every row below the modern band starts obsolete");
        check(modern && modern_rows == 7, "O1b the seven modern rows start live");
    }

    const uint16_t levy = 0;
    uint16_t rifle = 0;
    for (std::size_t i = 0; i < table.size(); ++i)
        if (table[i].band == roster_band::modern) { rifle = static_cast<uint16_t>(i); break; }

    std::printf("\nO2 — the verb flips, and back\n");
    {
        check(mark(w, reg, seat, levy, 0) == corp_command_result::applied && !roster_row_obsolete(w, seat, levy),
              "O2a the seat marks a pre-modern row live");
        check(mark(w, reg, seat, rifle, 1) == corp_command_result::applied && roster_row_obsolete(w, seat, rifle),
              "O2b and a modern row obsolete");
        const auto& f = w.nations.at(seat).roster_obsolete_flips;
        check(f.size() == 2 && f[0] < f[1], "O2c the nation holds two flips, ascending");
        check(roster_row_obsolete(w, rival, levy) && !roster_row_obsolete(w, rival, rifle),
              "O2d another nation still reads the defaults");
        check(mark(w, reg, seat, levy, 1) == corp_command_result::applied && roster_row_obsolete(w, seat, levy)
                  && w.nations.at(seat).roster_obsolete_flips.size() == 1,
              "O2e marking it back drops the flip");
    }

    std::printf("\nO3 — the verb validates\n");
    {
        const uint64_t h = w.state_hash(1);
        check(mark(w, reg, seat, static_cast<uint16_t>(table.size()), 1) == corp_command_result::rejected_invalid
                  && w.state_hash(1) == h, "O3a an unknown row is refused, nothing changes");
        check(mark(w, reg, seat, rifle, 2) == corp_command_result::rejected_invalid && w.state_hash(1) == h,
              "O3b an order other than 0 or 1 is refused");
        check(mark(w, reg, seat, rifle, 1) == corp_command_result::rejected_state && w.state_hash(1) == h,
              "O3c a no-op is refused");
        check(mark(w, reg, rival, levy, 0) == corp_command_result::rejected_not_owner && w.state_hash(1) == h,
              "O3d a nation that is not the seat is refused");
    }

    std::printf("\nO4 — save and hash\n");
    {
        const std::string bytes = to_bytes(w);
        world back;
        std::istringstream in(bytes, std::ios::binary);
        const bool ok = read_world_snapshot(back, in);
        check(ok && back.nations.at(seat).roster_obsolete_flips == w.nations.at(seat).roster_obsolete_flips
                  && back.state_hash(2) == w.state_hash(2) && to_bytes(back) == bytes,
              "O4a the flips round-trip by value (hash and bytes equal)");
        world clean;
        clean.nations[clean.create_entity()] = nation_component{};
        world clean2 = clean;
        mark(clean2, reg, clean2.nations.begin()->first, levy, 0); // not the seat: refused
        check(clean.state_hash(0) == clean2.state_hash(0),
              "O4b a world nobody flipped hashes alike");
        world touched = w;
        check(touched.state_hash(3) != [&] { world u = w; u.nations.at(seat).roster_obsolete_flips.clear(); return u.state_hash(3); }(),
              "O4c a flip moves the hash");
    }

    std::printf("\n%d checks, %d failures\n", checks, failures);
    std::printf("%s\n", failures == 0 ? "ALL PASS (0 failures)" : "FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
