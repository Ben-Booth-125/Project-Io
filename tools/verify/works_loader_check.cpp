// works_loader_check — the works table's LOADER refusals, on the real scripts/works.lua.
//
// BL-1149 (scale credit from the works) gave every row an `employs` column, the
// heads the work employs, and `works_registry::load_from_lua` validates it:
// required on every row, a whole number (a whole-valued float such as 20000.0 or
// 2e4 reads as the number it spells), in [0, work_employs_max], and at least one
// row employing anyone. works_roster_harness is Lua-free by design and cannot see
// the loader, so its refusals were asserted in prose only (the BL-1149 cold
// review). This harness loads the REAL table, corrupts one field in place the way
// era_roster.cpp's R4/R5 do, and asserts each refusal throws and names its cause.
//
//   W1  the shipped table loads, and every row carries the heads it was authored with
//   W2  a row with no `employs` is refused
//   W3  a negative is refused
//   W4  a fraction is refused, and so are a string, NaN and an infinity
//   W5  a value over 10^7 is refused; 10^7 itself and 0 load
//   W6  a whole-valued float (20000.0, 2e4) loads as the whole number
//   W7  a table in which nobody is employed is refused
//
// Build (needs a live Lua state):  bash tools/verify/build_lua_harness.sh works_loader_check
// Run from the repo root (it reads scripts/works.lua).

#include "scripting/lua_state.hpp"
#include "world/works_roster.hpp"

#include <sol/sol.hpp>

#include <cstdio>
#include <exception>
#include <string>

namespace {

int failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok) ++failures;
}

/// Load scripts/works.lua, run @p corrupt against it, then the loader. Returns
/// the loader's message, or "" when it did not throw; @p out gets the table.
std::string load_with(const char* corrupt, works_registry& out, bool& threw)
{
    lua_state lua;
    lua.load("scripts/works.lua");
    if (corrupt != nullptr && corrupt[0] != '\0')
        lua.state().safe_script(corrupt);
    threw = false;
    try { out.load_from_lua(lua); }
    catch (const std::exception& e) { threw = true; return e.what(); }
    return {};
}

/// A refusal row: the corruption throws and the message carries @p needle.
void refused(const char* corrupt, const char* needle, const std::string& what)
{
    works_registry r;
    bool threw = false;
    const std::string msg = load_with(corrupt, r, threw);
    check(threw && msg.find(needle) != std::string::npos,
          what + " (\"" + (threw ? msg : std::string("no throw")) + "\")");
}

} // namespace

int main()
{
    std::printf("=== works_loader_check (BL-1149: the loader's refusals on the real works table) ===\n");

    // --- W1: the shipped table ---------------------------------------------
    std::printf("\nW1 -- the shipped table loads\n");
    {
        works_registry r;
        bool threw = false;
        const std::string msg = load_with(nullptr, r, threw);
        check(!threw && r.size() > 0, "scripts/works.lua loads" + (threw ? " -- " + msg : std::string()));
        const int granary = r.id_of("Granary"), blast = r.id_of("Blast Works"), wall = r.id_of("Wall Circuit");
        check(granary >= 0 && r.row_at(static_cast<std::size_t>(granary))->employs == 2000
                  && blast >= 0 && r.row_at(static_cast<std::size_t>(blast))->employs == 120000
                  && wall >= 0 && r.row_at(static_cast<std::size_t>(wall))->employs == 0,
              "rows carry the heads they were authored with (Granary 2,000, Blast Works 120,000, Wall Circuit 0)");
    }

    // --- W2-W5, W7: the refusals -------------------------------------------
    std::printf("\nW2 -- a missing 'employs'\n");
    refused("works[1].employs = nil", "has no 'employs'", "a row with no 'employs' is refused, never read as 0");

    std::printf("\nW3 -- a negative\n");
    refused("works[1].employs = -1", "is outside [0, 10000000]", "a negative 'employs' is refused");

    std::printf("\nW4 -- not a whole number\n");
    refused("works[1].employs = 2000.5", "not a whole number", "a fraction is refused, never rounded");
    refused("works[1].employs = '2000'", "not a whole number", "a string is refused");
    refused("works[1].employs = 0/0", "not a whole number", "NaN is refused");
    refused("works[1].employs = math.huge", "not a whole number", "an infinity is refused");

    std::printf("\nW5 -- the ceiling\n");
    refused("works[1].employs = 10000001", "is outside [0, 10000000]", "a value over 10^7 is refused");
    {
        works_registry r;
        bool threw = false;
        const std::string msg = load_with("works[1].employs = 10000000; works[2].employs = 0", r, threw);
        check(!threw && r.row_at(0)->employs == 10000000 && r.row_at(1)->employs == 0,
              "10^7 itself and 0 load" + (threw ? " -- " + msg : std::string()));
    }

    std::printf("\nW6 -- a whole-valued float\n");
    {
        works_registry r;
        bool threw = false;
        const std::string msg = load_with("works[1].employs = 20000.0; works[2].employs = 2e4", r, threw);
        check(!threw && r.row_at(0)->employs == 20000 && r.row_at(1)->employs == 20000,
              "20000.0 and 2e4 load as 20000" + (threw ? " -- " + msg : std::string()));
    }

    std::printf("\nW7 -- nobody employed\n");
    refused("for _, w in ipairs(works) do w.employs = 0 end", "no work employs anyone",
            "a table in which no work employs anyone is refused");

    std::printf("\n=== %s (%d failure%s) ===\n", failures == 0 ? "ALL PASS" : "FAILURES", failures,
                failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
