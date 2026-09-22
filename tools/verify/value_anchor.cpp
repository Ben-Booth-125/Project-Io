// ---------------------------------------------------------------------------
// The value anchor's CHECK (BL-543) — no SDL / Lua / ImGui
// ---------------------------------------------------------------------------
// Ben, 2026-08-22 (docs/politics/NATIONS.md § Settled): "a unit's annual
// equipment costs about twice its annual salary." That sentence is the only
// thing in the project that anchors military value to money — every other
// military number is a per-mille modifier over another per-mille modifier —
// and until now nothing checked it, because nothing could: both halves of the
// upkeep vector ship at ZERO (`scripts/economy.lua` § unit_upkeep, BL-454).
//
// So this harness is the ARGUMENT FOR WHAT THE RATES SHOULD BE, not a gate on
// what they are. It authors no rate. It reads the two authored tables, states
// the identity they must satisfy, and proves the identity is falsifiable.
//
// THE IDENTITY. An economy tick is one quarter (budget_system.hpp,
// k_ticks_per_year = 4) and BOTH halves of the upkeep vector are charged per
// head per tick, so the annual factor of 4 cancels and the anchor is a per-tick
// statement over the same two numbers the economy already charges:
//
//     SUM over r of ( goods_per_head[r] x base_price[r] )  ~=  2 x wage_per_head
//
// TWO NARROWINGS, both Ben's, both load-bearing (2026-08-22):
//
//   1. BASE PRICES, NEVER THE RESOLVED MARKET PRICE. "My 1/2 figure can use
//      base prices." `world_gen.kepler_market.base_price` is an AUTHORED,
//      seed-invariant table, so the anchor is a property of the CONTENT and
//      not of a world, a seed, or a tick. This harness resolves no market and
//      generates no world.
//   2. AUTHORING-TIME, NOT RUNTIME. "We don't have to rederive wages every
//      tick." Nothing here is a per-tick computation and nothing in the engine
//      calls it — the check lives at the desk, beside the tables it reads.
//
// And a third, from the shape of a ratio rather than from Ben: it is a BAND
// with a stated tolerance, not an equality. "About twice" is not 2.0000.
//
// ROWS (requirement group "value-anchor"):
//
//   R1  THE BAND HOLDS for every roster row (hence every class) at the
//       authored rates, within the stated tolerance.
//   R2  IT FAILS WHEN PERTURBED. Move a wage, a goods rate, or a BASE PRICE
//       in-harness and the check goes RED; move one a little and it stays
//       GREEN. This row is what earns the harness — a check that cannot fail
//       is not a check — so the failure is asserted, never claimed.
//   R3  INERT AT ZERO. At the shipped all-zero rates the calibration is
//       reported INERT and passes: the anchor has nothing to bind yet, and
//       saying so is the honest reading rather than a vacuous green.
//   R4  REPORT the implied "units funded per year" for the seeded extraction
//       levy, so NR-382's rate question is answered against a number rather
//       than a feeling. The levy rate is read back through law.cpp's own
//       seeding path, not retyped here.
//
// PLUS ONE FINDING THE ROWS DID NOT ASK FOR (R5). The wage half is per-row
// (`credits_per_head_per_power` scales it by the roster's power_mod, 0..420)
// and the goods half is FLAT (one `{resource = qty}` table for the whole
// roster). A flat numerator over a spread denominator cannot sit inside one
// band unless the spread is small, so the anchor BOUNDS the power coefficient.
// R5 derives that bound and demonstrates it in both directions.
//
// WHY THE .lua TEXT IS PARSED. The Lua TUs (recipe_registry.cpp,
// world_gen_config.cpp) are the excluded set for a headless harness, so the
// loader is unreachable from here. The alternative — mirroring the numbers as
// constants — would make the harness silently stale the day Ben edits a rate,
// which is precisely the day it needs to speak. Parsing the authored text
// keeps the check true to the content by construction; the cost is a repo-root
// working directory, which the FATAL guard below refuses to run without.
//
// Run from the repo root: value_anchor
// The process exits non-zero if any assertion FAILs.
// ---------------------------------------------------------------------------

#include "world/combat.hpp"
#include "world/components.hpp"
#include "world/law.hpp"
#include "world/resource_names.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

// ---------------------------------------------------------------------------
// The anchor's own constants
// ---------------------------------------------------------------------------

/// Ben's figure, 2026-08-22: equipment costs about TWICE the salary.
constexpr float k_anchor = 2.0f;

/// The band around it. A CALL TAKEN IN THIS HARNESS, not Ben's number — his
/// word was "about". +/-25% is wide enough that a roster does not have to hit
/// 2.0000 on every row to be honestly anchored, and narrow enough that a 2x
/// anchor can never be confused with a 1x or a 3x one (the adjacent readings
/// sit at ratio 0.5 and 1.5, both far outside).
constexpr float k_tolerance = 0.25f;

/// An economy tick is one quarter — budget_system.hpp says so in its own
/// words. Named here because no constant carries it in world/*.
constexpr int k_ticks_per_year = 4;

// ---------------------------------------------------------------------------
// Reading the authored tables out of the Lua text
// ---------------------------------------------------------------------------

std::string read_text(const char* path)
{
    std::FILE* f = std::fopen(path, "rb");
    if (!f)
    {
        // Refuse to measure a default rather than report a vacuous green — the
        // interbody_pull_harness rule. Every assertion below would pass over an
        // empty table, which is the exact failure this harness exists to catch.
        std::printf("FATAL: cannot open %s - run this harness from the repo root.\n"
                    "       An empty table would make every assertion here vacuous.\n",
                    path);
        std::exit(2);
    }
    std::string out;
    char        buf[4096];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        out.append(buf, n);
    std::fclose(f);
    return out;
}

bool ident_char(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/// Strip `--` line comments. The authored tables comment heavily and those
/// comments contain both braces and numbers, so brace matching and pair
/// scanning are both wrong without this.
std::string strip_comments(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();)
    {
        if (s[i] == '-' && i + 1 < s.size() && s[i + 1] == '-')
        {
            while (i < s.size() && s[i] != '\n') ++i;
        }
        else
        {
            out.push_back(s[i]);
            ++i;
        }
    }
    return out;
}

/// Index just past the `=` of a top-level-or-nested `key =` assignment, or
/// npos. Requires a non-identifier char before the key and an `=` after it, so
/// `credits_per_head` does not match `credits_per_head_per_power`.
std::size_t find_assignment(const std::string& s, const std::string& key, std::size_t from = 0)
{
    for (std::size_t at = s.find(key, from); at != std::string::npos;
         at = s.find(key, at + 1))
    {
        if (at > 0 && ident_char(s[at - 1])) continue;
        std::size_t j = at + key.size();
        if (j < s.size() && ident_char(s[j])) continue;
        while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) ++j;
        if (j >= s.size() || s[j] != '=') continue;
        return j + 1;
    }
    return std::string::npos;
}

/// The brace-matched `{ ... }` block starting at or after @p from, exclusive of
/// the outer braces.
std::string brace_block(const std::string& s, std::size_t from, const char* what)
{
    const std::size_t open = s.find('{', from);
    if (open == std::string::npos)
    {
        std::printf("FATAL: no table body for %s in the authored Lua.\n", what);
        std::exit(2);
    }
    int depth = 0;
    for (std::size_t i = open; i < s.size(); ++i)
    {
        if (s[i] == '{') ++depth;
        else if (s[i] == '}' && --depth == 0)
            return s.substr(open + 1, i - open - 1);
    }
    std::printf("FATAL: unterminated table body for %s in the authored Lua.\n", what);
    std::exit(2);
    return {};
}

/// Every `identifier = <number>` pair in @p block, in authored order. Pairs
/// whose value is not a number (a nested table) are skipped rather than
/// guessed at.
std::vector<std::pair<std::string, float>> parse_pairs(const std::string& block)
{
    std::vector<std::pair<std::string, float>> out;
    for (std::size_t i = 0; i < block.size();)
    {
        if (!ident_char(block[i])) { ++i; continue; }
        const std::size_t start = i;
        while (i < block.size() && ident_char(block[i])) ++i;
        const std::string name = block.substr(start, i - start);

        std::size_t j = i;
        while (j < block.size() && std::isspace(static_cast<unsigned char>(block[j]))) ++j;
        if (j >= block.size() || block[j] != '=') continue;
        ++j;
        while (j < block.size() && std::isspace(static_cast<unsigned char>(block[j]))) ++j;

        char*       end = nullptr;
        const float v   = std::strtof(block.c_str() + j, &end);
        if (end == block.c_str() + j) { i = j; continue; } // a nested table, not a number
        out.emplace_back(name, v);
        i = static_cast<std::size_t>(end - block.c_str());
    }
    return out;
}

/// A resource-indexed array filled from `{resource_name = qty}` pairs. An
/// unrecognised name is FATAL, not skipped — resource_names.hpp's own rule:
/// an unknown resource name in authored content is a content bug that must be
/// loud, not a case to guess through.
std::array<float, resource_count> resource_table(const std::vector<std::pair<std::string, float>>& pairs,
                                                 const char* what)
{
    std::array<float, resource_count> out{};
    for (const auto& [name, value] : pairs)
    {
        bool                ok  = false;
        const resource_type res = resource_names::resource_from_name(name, ok);
        if (!ok)
        {
            std::printf("FATAL: unknown resource '%s' in %s.\n", name.c_str(), what);
            std::exit(2);
        }
        out[static_cast<std::size_t>(res)] = value;
    }
    return out;
}

float scalar_at(const std::string& s, const std::string& key, const char* what)
{
    const std::size_t at = find_assignment(s, key);
    if (at == std::string::npos)
    {
        std::printf("FATAL: %s is not authored in the Lua - the table has moved.\n", what);
        std::exit(2);
    }
    char* end = nullptr;
    return std::strtof(s.c_str() + at, &end);
}

/// The `inputs = { ... }` or `outputs = { ... }` basket of the recipe whose
/// `name` is @p recipe_name, read out of the comment-stripped recipes.lua text.
/// FATAL if the recipe has moved or been renamed — a silently-empty basket would
/// make the markup rows below vacuous, which is the exact failure mode this
/// harness keeps catching.
std::array<float, resource_count> recipe_basket(const std::string& recipes,
                                                const char*        recipe_name,
                                                const char*        which)
{
    const std::string quoted = std::string("\"") + recipe_name + "\"";
    std::size_t       at     = std::string::npos;
    for (std::size_t i = recipes.find(quoted); i != std::string::npos;
         i = recipes.find(quoted, i + 1))
    {
        // Must be the value of a `name =` assignment, not a mention elsewhere.
        std::size_t j = i;
        while (j > 0 && std::isspace(static_cast<unsigned char>(recipes[j - 1]))) --j;
        if (j == 0 || recipes[j - 1] != '=') continue;
        --j;
        while (j > 0 && std::isspace(static_cast<unsigned char>(recipes[j - 1]))) --j;
        if (j < 4 || recipes.compare(j - 4, 4, "name") != 0) continue;
        at = i;
        break;
    }
    if (at == std::string::npos)
    {
        std::printf("FATAL: no recipe named '%s' in scripts/recipes.lua - it has moved or "
                    "been renamed.\n", recipe_name);
        std::exit(2);
    }
    const std::size_t basket = find_assignment(recipes, which, at);
    if (basket == std::string::npos)
    {
        std::printf("FATAL: recipe '%s' has no %s table.\n", recipe_name, which);
        std::exit(2);
    }
    const auto pairs = parse_pairs(brace_block(recipes, basket, recipe_name));
    if (pairs.empty())
    {
        std::printf("FATAL: recipe '%s' parsed to an EMPTY %s basket.\n", recipe_name, which);
        std::exit(2);
    }
    return resource_table(pairs, recipe_name);
}

/// The two authored tables, read once.
struct authored
{
    unit_upkeep_params                upkeep;
    std::array<float, resource_count> base_price{};
    int                               priced_resources = 0;
    int                               named_goods      = 0;

    /// The comment-stripped recipes.lua text, for the markup row. Kept as text
    /// rather than parsed up front: only a handful of baskets are read.
    std::string                       recipes;
};

authored read_authored()
{
    authored a;

    const std::string econ = strip_comments(read_text("scripts/economy.lua"));
    const std::size_t up   = find_assignment(econ, "unit_upkeep");
    if (up == std::string::npos)
    {
        std::printf("FATAL: economy.military.unit_upkeep is not authored - the table has moved.\n");
        std::exit(2);
    }
    const std::string upkeep_block = brace_block(econ, up, "unit_upkeep");

    a.upkeep.credits_per_head =
        scalar_at(upkeep_block, "credits_per_head", "unit_upkeep.credits_per_head");
    a.upkeep.credits_per_head_per_power =
        scalar_at(upkeep_block, "credits_per_head_per_power",
                  "unit_upkeep.credits_per_head_per_power");

    const std::size_t goods = find_assignment(upkeep_block, "goods_per_head");
    if (goods == std::string::npos)
    {
        std::printf("FATAL: unit_upkeep.goods_per_head is not authored - the table has moved.\n");
        std::exit(2);
    }
    const auto goods_pairs = parse_pairs(brace_block(upkeep_block, goods, "goods_per_head"));
    a.upkeep.goods_per_head = resource_table(goods_pairs, "unit_upkeep.goods_per_head");
    a.named_goods           = static_cast<int>(goods_pairs.size());

    const std::string gen   = strip_comments(read_text("scripts/world_gen.lua"));
    const std::size_t price = find_assignment(gen, "base_price");
    if (price == std::string::npos)
    {
        std::printf("FATAL: world_gen.kepler_market.base_price is not authored - the table has moved.\n");
        std::exit(2);
    }
    const auto price_pairs = parse_pairs(brace_block(gen, price, "base_price"));
    a.base_price           = resource_table(price_pairs, "world_gen.kepler_market.base_price");
    a.priced_resources     = static_cast<int>(price_pairs.size());

    a.recipes = strip_comments(read_text("scripts/recipes.lua"));

    return a;
}

// ---------------------------------------------------------------------------
// The anchor itself
// ---------------------------------------------------------------------------

/// The goods half of one head's per-tick draw, valued at base prices.
///
/// ASCENDING RESOURCE INDEX, ALWAYS. Float addition is not associative, so the
/// accumulation order is part of the contract, not an implementation detail —
/// the same rule `unit_strength` states for its own sum.
float basket_value(const std::array<float, resource_count>& goods,
                   const std::array<float, resource_count>& price)
{
    float v = 0.0f;
    for (std::size_t r = 0; r < resource_count; ++r)
        v += goods[r] * price[r];
    return v;
}

const char* class_name(unit_class c)
{
    switch (c)
    {
    case unit_class::infantry: return "infantry";
    case unit_class::cavalry:  return "cavalry";
    case unit_class::ranged:   return "ranged";
    case unit_class::siege:    return "siege";
    case unit_class::naval:    return "naval";
    }
    return "?";
}

/// One roster row's reading of the anchor.
struct reading
{
    const roster_row* row       = nullptr;
    float             wage      = 0.0f; ///< Credits per head per tick.
    float             equipment = 0.0f; ///< Goods per head per tick, at base prices.
    float             ratio     = 0.0f; ///< equipment / (2 x wage); 0 where undefined.
    bool              inert     = false;
    bool              in_band   = false;
};

/// Resolve one row THROUGH THE PRODUCTION RESOLVER (`resolve_unit_upkeep`)
/// rather than re-deriving what the harness thinks the formula is — the same
/// reason `unit_roster_table()` is exposed at all.
reading read_row(std::uint16_t idx,
                 const unit_upkeep_params& p,
                 const std::array<float, resource_count>& price)
{
    unit_component u{};
    u.position = null_entity;
    u.owner    = null_entity;
    u.count    = 1; // per HEAD: n = 1 makes every product below exact
    u.type     = idx;

    const unit_upkeep_draw d = resolve_unit_upkeep(u, p);

    reading r;
    r.row       = &unit_roster_table()[idx];
    r.wage      = d.credits;
    r.equipment = basket_value(d.goods, price);

    // Inert is its own verdict, not a pass and not a failure: 0 == 2 x 0 is
    // true and says nothing. R3 is the row that reads it.
    r.inert = (r.wage == 0.0f && r.equipment == 0.0f);
    if (r.inert)
        return r;

    const float target = k_anchor * r.wage;
    if (!(target > 0.0f))
        return r; // paid entirely in goods: no salary to be twice OF

    r.ratio   = r.equipment / target;
    r.in_band = (r.ratio >= 1.0f - k_tolerance && r.ratio <= 1.0f + k_tolerance);
    return r;
}

struct sweep
{
    std::vector<reading> rows;
    int                  inert = 0, in_band = 0, out_of_band = 0;
};

/// Every roster row, in table index order — the roster's own walk order, which
/// is what makes this deterministic without sorting anything.
sweep sweep_roster(const unit_upkeep_params& p,
                   const std::array<float, resource_count>& price)
{
    sweep s;
    const std::vector<roster_row>& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        const reading r = read_row(static_cast<std::uint16_t>(i), p, price);
        s.rows.push_back(r);
        if (r.inert)        ++s.inert;
        else if (r.in_band) ++s.in_band;
        else                ++s.out_of_band;
    }
    return s;
}

void print_sweep(const sweep& s, const char* title)
{
    std::printf("     %s\n", title);
    std::printf("     %-22s %-9s %5s %10s %10s %8s  %s\n",
                "row", "class", "power", "wage/hd", "equip/hd", "ratio", "verdict");
    for (const reading& r : s.rows)
    {
        char ratio[16];
        if (r.inert)          std::snprintf(ratio, sizeof ratio, "%8s", "-");
        else if (r.ratio > 0) std::snprintf(ratio, sizeof ratio, "%8.3f", r.ratio);
        else                  std::snprintf(ratio, sizeof ratio, "%8s", "inf");

        std::printf("     %-22s %-9s %5d %10.4f %10.4f %s  %s\n",
                    r.row->name, class_name(r.row->cls), r.row->power_mod,
                    r.wage, r.equipment, ratio,
                    r.inert ? "INERT" : (r.in_band ? "in band" : "OUT OF BAND"));
    }
    std::printf("     -> %d in band, %d out of band, %d inert (tolerance +/-%.0f%%)\n\n",
                s.in_band, s.out_of_band, s.inert, k_tolerance * 100.0f);
}

// ---------------------------------------------------------------------------
// The calibrated fixture (R2, R4, R5)
// ---------------------------------------------------------------------------
//
// A HARNESS FIXTURE, NOT A PROPOSAL. The anchor pins a RATIO; it says nothing
// about the LEVEL, and picking the level is affordability work that
// `unit_upkeep_rates.cpp` already owns (wage bill against income, goods draw
// against production). So the basket below is a shape, chosen from the two
// goods `economy.lua` already names, and the wage is then SOLVED from the
// identity rather than picked. Perturbing a solved fixture is what lets R2
// assert a red; nothing here is offered as a rate to author.

struct calibration
{
    unit_upkeep_params params;
    float              equipment = 0.0f;
    float              wage      = 0.0f;
    bool               inert     = true;
};

calibration calibrate(const authored& a)
{
    calibration c;

    // The basket SHAPE: one part ordnance to ten parts rations, per head per
    // tick. Both goods are the ones already named at zero in economy.lua —
    // naming a third would be authoring content, not calibrating.
    c.params.goods_per_head[static_cast<std::size_t>(resource_type::ordnance)]     = 0.10f;
    c.params.goods_per_head[static_cast<std::size_t>(resource_type::food_rations)] = 1.00f;

    c.equipment = basket_value(c.params.goods_per_head, a.base_price);

    // ...and the wage falls out of the anchor. Flat: R5 explains why the
    // power-scaled term has to be zero for a flat basket to hold across the
    // roster, and derives the bound.
    c.wage                             = c.equipment / k_anchor;
    c.params.credits_per_head          = c.wage;
    c.params.credits_per_head_per_power = 0.0f;
    c.inert                            = false;
    return c;
}

// ---------------------------------------------------------------------------
// R4 — the seeded levy, read back through law.cpp rather than retyped
// ---------------------------------------------------------------------------

struct levy_probe
{
    bool  seeded = false;
    float rate   = 0.0f; ///< Credits per unit of raw output, in the author's jurisdiction.
};

/// The smallest world `seed_prototype_laws` will seed into: one nation, one
/// tile it owns, one player corp domiciled there. No generation, no market, no
/// tick — the rate is generation-time content, so reading it costs a world of
/// four entities.
levy_probe probe_seeded_levy()
{
    world w;

    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};

    const entity_id nation = w.create_entity();
    nation_component nc{};
    nc.name = "Anchoria";
    w.nations[nation] = nc;

    const entity_id tile = w.create_entity();
    tile_component tc{};
    tc.body      = body;
    w.tiles[tile] = tc;
    w.tile_to_nation[tile] = nation;
    w.nations[nation].tiles.push_back(tile);

    const entity_id corp = w.create_entity();
    corporation_component cc{};
    cc.name        = "Anchor Holdings";
    cc.balance     = 0.0f;
    cc.is_player   = true;
    cc.home_nation = nation;
    w.corporations[corp] = cc;
    w.player_entity      = corp;

    // DEFAULT ARGUMENT DELIBERATELY. The seeded rate is law.hpp's business; a
    // literal 1.0 here would be a second copy of it, stale the day it moves.
    seed_prototype_laws(w);

    const law_effects fx = evaluate_laws(w, corp);

    levy_probe p;
    p.seeded = fx.any && !fx.levies.empty();
    if (p.seeded)
        p.rate = fx.levies.front()
                     .extraction_levy[static_cast<std::size_t>(resource_type::iron_ore)];
    return p;
}

} // namespace

int main()
{
    std::printf("=== value anchor (BL-543) ===\n");
    std::printf("Ben, 2026-08-22: \"a unit's annual equipment costs about twice its annual salary\"\n");
    std::printf("Identity (per head per tick, the annual x4 cancels on both sides):\n");
    std::printf("    SUM_r goods_per_head[r] x base_price[r]  ~=  %.1f x wage_per_head   (+/-%.0f%%)\n\n",
                k_anchor, k_tolerance * 100.0f);

    const authored a = read_authored();
    std::printf("Authored tables read from the Lua text:\n");
    std::printf("    scripts/economy.lua   unit_upkeep: credits_per_head=%.4f  "
                "credits_per_head_per_power=%.6f  goods named=%d\n",
                a.upkeep.credits_per_head, a.upkeep.credits_per_head_per_power, a.named_goods);
    std::printf("    scripts/world_gen.lua base_price: %d resources priced "
                "(ordnance=%.1f food_rations=%.1f)\n\n",
                a.priced_resources,
                a.base_price[static_cast<std::size_t>(resource_type::ordnance)],
                a.base_price[static_cast<std::size_t>(resource_type::food_rations)]);

    const std::vector<roster_row>& table = unit_roster_table();

    // -----------------------------------------------------------------------
    // R1 — the band holds at the authored rates
    // -----------------------------------------------------------------------
    std::printf("-- R1  the band holds for every roster row, at the AUTHORED rates --\n");
    const sweep authored_sweep = sweep_roster(a.upkeep, a.base_price);
    print_sweep(authored_sweep, "at scripts/economy.lua's shipped rates:");

    check(authored_sweep.out_of_band == 0,
          "R1a no roster row sits outside the anchor band at the authored rates");
    check(static_cast<int>(authored_sweep.rows.size()) == static_cast<int>(table.size()) &&
              !table.empty(),
          "R1b the sweep covered the whole roster (not a vacuous zero-row pass)");
    {
        // Every class is represented, so "every roster class" is a claim about
        // coverage and not only about the rows that happen to be cheap.
        bool seen[unit_class_count] = {};
        for (const reading& r : authored_sweep.rows)
            seen[static_cast<std::size_t>(r.row->cls)] = true;
        bool all = true;
        for (bool s : seen) all = all && s;
        check(all, "R1c all five unit classes appear in the sweep");
    }

    // -----------------------------------------------------------------------
    // R3 — WHICH STATE THE CONTENT IS IN. Read before R2, because R2 needs a
    //      live fixture either way.
    // -----------------------------------------------------------------------
    // R3 IS A BRANCH, NOT AN ASSERTION, and that is the whole point of it.
    // BL-454 landed the upkeep vector INERT: both halves ship at bit-exact zero.
    // The obvious row to write is "assert they are zero" — and it is the wrong
    // row, because it goes RED on the one day this harness exists for, the day
    // Ben authors a rate and the anchor finally has something to bind. A check
    // that fails when its subject is satisfied is worse than no check.
    //
    // So R3 asks which state the content is in and asserts the invariant that
    // belongs to that state. Inert: nothing is drawn, at any scale, and the
    // sweep says INERT rather than green. Authored: something IS drawn, the
    // sweep is live rather than inert, and R1's band — already asserted above —
    // is a claim about the shipped content rather than about a fixture.
    // (Sprint N1 adversarial pass, 2026-08-22, finding F1: the reviewer authored
    // this harness's own solved fixture into economy.lua and got exit 1 with the
    // anchor satisfied and all 19 rows in band.)
    std::printf("-- R3  which state is the content in? --\n");
    const bool rates_authored = !(a.upkeep.credits_per_head == 0.0f &&
                                  a.upkeep.credits_per_head_per_power == 0.0f &&
                                  [&] {
                                      for (std::size_t r = 0; r < resource_count; ++r)
                                          if (a.upkeep.goods_per_head[r] != 0.0f)
                                              return false;
                                      return true;
                                  }());
    {
        // What the resolver actually does at the authored rates, proven THROUGH
        // it rather than asserted in a comment.
        bool draws_nothing = true, draws_something = false;
        for (std::size_t i = 0; i < table.size(); ++i)
        {
            unit_component u{};
            u.position = null_entity;
            u.owner    = null_entity;
            u.count    = 10000; // a large force: a rate of zero is zero at any scale
            u.type     = static_cast<std::uint16_t>(i);
            const unit_upkeep_draw d = resolve_unit_upkeep(u, a.upkeep);
            bool row_zero = d.credits == 0.0f && !d.any_goods;
            for (std::size_t r = 0; r < resource_count; ++r)
                row_zero = row_zero && d.goods[r] == 0.0f;
            draws_nothing   = draws_nothing && row_zero;
            draws_something = draws_something || !row_zero;
        }

        if (!rates_authored)
        {
            std::printf("     state: INERT - scripts/economy.lua ships both halves of the "
                        "upkeep vector at zero.\n");
            check(draws_nothing,
                  "R3a INERT: resolve_unit_upkeep returns a bit-exactly zero draw for every "
                  "roster row at 10000 heads - no credits, no goods, at any scale");
            check(authored_sweep.inert == static_cast<int>(table.size()),
                  "R3b INERT: the calibration reports INERT for the whole roster rather than "
                  "a vacuous green");
            std::printf("     -> the anchor has nothing to bind yet. R2/R4/R5 run against a "
                        "calibrated fixture.\n\n");
        }
        else
        {
            std::printf("     state: AUTHORED - the anchor now binds the SHIPPED content, and "
                        "R1 above is the row that matters.\n");
            check(draws_something,
                  "R3a AUTHORED: resolve_unit_upkeep draws a real cost for at least one roster "
                  "row - the authored rates reach the resolver");
            check(authored_sweep.inert == 0,
                  "R3b AUTHORED: no roster row reports INERT, so R1a's band is a statement "
                  "about the shipped rates and not about a fixture");
            std::printf("     -> R1a's green is now load-bearing. R2/R5 still run against the "
                        "solved fixture, which is a control.\n\n");
        }
    }

    // -----------------------------------------------------------------------
    // R2 — it fails when perturbed
    // -----------------------------------------------------------------------
    std::printf("-- R2  the check goes RED when a rate or a base price moves --\n");
    const calibration cal = calibrate(a);
    std::printf("     fixture (NOT a proposal): ordnance %.2f + food_rations %.2f per head per tick\n"
                "       = %.4f cr of equipment, so the anchor solves the wage at %.4f cr/head/tick\n"
                "       (%.2f cr/head/year equipment, %.2f cr/head/year wage)\n\n",
                cal.params.goods_per_head[static_cast<std::size_t>(resource_type::ordnance)],
                cal.params.goods_per_head[static_cast<std::size_t>(resource_type::food_rations)],
                cal.equipment, cal.wage,
                cal.equipment * k_ticks_per_year, cal.wage * k_ticks_per_year);
    {
        const sweep green = sweep_roster(cal.params, a.base_price);
        check(green.out_of_band == 0 && green.inert == 0,
              "R2a POSITIVE CONTROL: the solved fixture is GREEN across the whole roster");

        // (a) the wage moves.
        unit_upkeep_params wage_up      = cal.params;
        wage_up.credits_per_head        = cal.wage * 1.5f;
        const sweep wage_up_sweep       = sweep_roster(wage_up, a.base_price);
        check(wage_up_sweep.out_of_band == static_cast<int>(table.size()),
              "R2b a wage 50% above the anchor turns EVERY row RED");

        // PERTURBATION SIZES ARE DERIVED FROM THE BAND, not picked. The ratio is
        // equipment / wage, so scaling the ordnance line by f moves the ratio by
        //     1 + (f - 1) x ordnance_share
        // where ordnance_share is ordnance's fraction of the equipment basket.
        // In band needs the ratio inside [1 - tol, 1 + tol] of the control, so
        // the smallest f that must break it is 1 + (tol / share) and the largest
        // that must not is 1 + (tol / share) x 0.5. Both follow the band: widen
        // k_tolerance and both rows move with it, instead of R2c quietly
        // becoming a check that cannot discriminate.
        const float ordnance_value =
            cal.params.goods_per_head[static_cast<std::size_t>(resource_type::ordnance)] *
            a.base_price[static_cast<std::size_t>(resource_type::ordnance)];
        const float ordnance_share = (cal.equipment > 0.0f) ? ordnance_value / cal.equipment : 0.0f;
        const float break_factor   = 1.0f + (k_tolerance / ordnance_share) * 1.25f; // clear of the edge
        const float hold_factor    = 1.0f + (k_tolerance / ordnance_share) * 0.50f; // clear inside it
        const float price_break    = break_factor;
        const float price_hold     = hold_factor;
        std::printf("     ordnance is %.1f%% of the equipment basket, so the band breaks at a "
                    "%.2fx ordnance move\n       and holds at %.2fx - both derived from "
                    "k_tolerance (%.0f%%), neither picked.\n",
                    ordnance_share * 100.0f, break_factor, hold_factor, k_tolerance * 100.0f);

        // (b) a goods rate moves.
        unit_upkeep_params goods_up = cal.params;
        goods_up.goods_per_head[static_cast<std::size_t>(resource_type::ordnance)] *= break_factor;
        const sweep goods_up_sweep = sweep_roster(goods_up, a.base_price);
        check(goods_up_sweep.out_of_band == static_cast<int>(table.size()),
              "R2c an ordnance draw scaled past the band turns EVERY row RED");

        unit_upkeep_params goods_nudge = cal.params;
        goods_nudge.goods_per_head[static_cast<std::size_t>(resource_type::ordnance)] *= hold_factor;
        const sweep goods_nudge_sweep = sweep_roster(goods_nudge, a.base_price);
        check(goods_nudge_sweep.out_of_band == 0,
              "R2g ...and one scaled to half that stays GREEN - the size is the band's, "
              "not a magic 4x");

        // (c) a BASE PRICE moves — the half of the identity that lives in
        // world_gen.lua, and the one a check built only on economy.lua would be
        // blind to. It says the anchor spans BOTH authored files. It does NOT
        // say the anchor is content-wide; R6 below says what it is instead.
        std::array<float, resource_count> dearer = a.base_price;
        dearer[static_cast<std::size_t>(resource_type::ordnance)] *= price_break;
        const sweep price_up_sweep = sweep_roster(cal.params, dearer);
        check(price_up_sweep.out_of_band == static_cast<int>(table.size()),
              "R2d ordnance repriced past the band in base_price turns EVERY row RED, "
              "with no rate touched - the anchor spans both authored files");

        // (d) ...and a small move does NOT, or the band is an equality wearing
        // a tolerance's clothes.
        unit_upkeep_params nudged = cal.params;
        nudged.credits_per_head   = cal.wage * 1.10f;
        const sweep nudged_sweep  = sweep_roster(nudged, a.base_price);
        check(nudged_sweep.out_of_band == 0,
              "R2e a 10% wage move stays GREEN - it is a band, not an equality");

        // (e) ...and a price move JUST INSIDE the band does not either. Paired
        // with R2d this is what makes the perturbation sizes meaningful rather
        // than magic: both are derived from k_tolerance below, so widening the
        // band moves both rows together instead of silently making R2d pass on a
        // check that no longer discriminates.
        std::array<float, resource_count> dearer_a_little = a.base_price;
        dearer_a_little[static_cast<std::size_t>(resource_type::ordnance)] *= price_hold;
        const sweep price_hold_sweep = sweep_roster(cal.params, dearer_a_little);
        check(price_hold_sweep.out_of_band == 0,
              "R2f ...while a reprice just INSIDE the band leaves every row GREEN - the two "
              "sizes bracket the band edge rather than straddling it by a magic factor");

        std::printf("     ratios: control %.3f | wage+50%% %.3f | ordnance x%.2f %.3f | "
                    "price x%.2f %.3f | wage+10%% %.3f\n\n",
                    green.rows.front().ratio, wage_up_sweep.rows.front().ratio,
                    break_factor, goods_up_sweep.rows.front().ratio,
                    price_break, price_up_sweep.rows.front().ratio,
                    nudged_sweep.rows.front().ratio);
    }

    // -----------------------------------------------------------------------
    // R6 — WHAT THE ANCHOR DOES AND DOES NOT CONSTRAIN
    // -----------------------------------------------------------------------
    // R2d turns red when ordnance is repriced, and it is tempting to read that
    // as "the anchor is content-wide". It is not, and the Sprint N1 adversarial
    // pass proved it the direct way: multiply ALL 33 authored base prices by ten
    // and every row stays green (finding F2). That is not a bug — it is what a
    // RATIO is. `calibrate()` solves the wage from the same prices it values the
    // basket with, so a uniform reprice moves numerator and denominator together
    // and the anchor cannot see it. Nor should it: "equipment costs twice the
    // salary" is a statement about relative value, and relative value survives a
    // change of unit.
    //
    // So R6 does two things the original rows did not. It ASSERTS the invariance
    // rather than being quietly carried by it, and it adds the row that DOES
    // bind an absolute price relation — one that a non-uniform edit breaks,
    // which is the edit that actually happens.
    std::printf("-- R6  the anchor is scale-invariant; the markup band is what binds a price --\n");
    {
        // (a) A uniform reprice cannot move the anchor. Stated, not assumed.
        std::array<float, resource_count> tenfold = a.base_price;
        for (float& p : tenfold)
            p *= 10.0f;
        const calibration cal10 = [&] {
            authored scaled = a;
            scaled.base_price = tenfold;
            return calibrate(scaled);
        }();
        const sweep control = sweep_roster(cal.params, a.base_price);
        const sweep scaled  = sweep_roster(cal10.params, tenfold);
        bool ratios_identical = control.rows.size() == scaled.rows.size();
        for (std::size_t i = 0; ratios_identical && i < control.rows.size(); ++i)
            ratios_identical = control.rows[i].ratio == scaled.rows[i].ratio;
        check(ratios_identical && !control.rows.empty(),
              "R6a every base price x10 leaves every roster row's ratio BIT-IDENTICAL - the "
              "anchor is a ratio, so it is scale-invariant and constrains no price on its own");

        // (b) What does bind a price: the markup a processed good carries over
        // its own input basket. recipes.lua id 27 states the rule in its own
        // words - "PRICE IS DERIVED, NOT PICKED" - and names the peers it was
        // derived against. This row holds the content to that sentence.
        //
        // BL-744 (2026-09-02) RE-AIMED THIS ROW. The rule that binds a price is
        // no longer "one common markup across the tier" - it is the recipe
        // margin anchor (PRODUCTION.md § The recipe margin anchor): on a good's
        // cheapest in-band route, margin >= k x marginal cost at base with
        // k = economy.recipe_margin_anchor.profit_over_marginal = 1.0, i.e. the
        // output is worth at least (1 + k) x (inputs + wage per batch). Dropping
        // the wage term makes that a LOWER bound on the markup over the input
        // basket alone - markup >= 1 + k - which is what this row now holds the
        // peers to. Their markups no longer sit within 5% of each other and are
        // not meant to: a cheap good's wage term is a larger share of its price,
        // so refined_fuel (2.23) sits above machinery (2.06). The peers are named
        // by their ANCHOR route where a good has several (electronics is priced
        // off the contact-grade route, spacecraft_components off the heavy one);
        // tools/verify/recipe_margin is the full statement, this is the echo the
        // military anchor needs so that ordnance's price is a derived one.
        constexpr float k_recipe_anchor = 1.0f; // mirrors economy.recipe_margin_anchor.profit_over_marginal
        struct peer { const char* recipe; const char* good; };
        static const peer peers[] = {
            { "machinery", "machinery" },
            { "alloys", "alloys" },
            { "electronics_contact_grade", "electronics" },
            { "spacecraft_components_heavy", "spacecraft_components" },
            { "refined_fuel", "refined_fuel" },
        };
        auto markup = [&](const char* recipe, resource_type out) {
            const auto  in  = recipe_basket(a.recipes, recipe, "inputs");
            const auto  o   = recipe_basket(a.recipes, recipe, "outputs");
            const float qty = o[static_cast<std::size_t>(out)];
            const float bas = basket_value(in, a.base_price);
            return (bas > 0.0f && qty > 0.0f)
                       ? (a.base_price[static_cast<std::size_t>(out)] * qty) / bas
                       : 0.0f;
        };

        float peer_min = 0.0f, peer_max = 0.0f;
        std::printf("     markup over input basket, at world_gen.lua base prices:\n");
        for (std::size_t i = 0; i < sizeof peers / sizeof peers[0]; ++i)
        {
            bool                ok = false;
            const resource_type r  = resource_names::resource_from_name(peers[i].good, ok);
            if (!ok)
            {
                std::printf("FATAL: peer good '%s' is not a resource.\n", peers[i].good);
                std::exit(2);
            }
            const float m = markup(peers[i].recipe, r);
            std::printf("       %-24s %.3f   (route %s)\n", peers[i].good, m, peers[i].recipe);
            peer_min = (i == 0 || m < peer_min) ? m : peer_min;
            peer_max = (i == 0 || m > peer_max) ? m : peer_max;
        }
        const float ordnance_markup = markup("ordnance", resource_type::ordnance);
        std::printf("       %-24s %.3f   <- the good the anchor draws\n",
                    "ordnance", ordnance_markup);

        const float anchor_floor = 1.0f + k_recipe_anchor;
        check(peer_min >= anchor_floor,
              "R6b every Advanced-Fabrication peer clears the recipe margin anchor on its anchor "
              "route - markup over its input basket >= 1 + k (2.0), the floor the wage term only raises");
        check(ordnance_markup >= anchor_floor,
              "R6c ...and ordnance clears it too, so its base_price is derived from the anchor and "
              "not picked - reprice steel or machinery alone and this goes red");

        // The vacuity guard: a non-uniform edit must actually break it, or the
        // row is measuring nothing. Repricing steel alone moves the basket and
        // not the output.
        std::array<float, resource_count> steel_dear = a.base_price;
        steel_dear[static_cast<std::size_t>(resource_type::steel)] *= 3.0f;
        const auto  in_ord   = recipe_basket(a.recipes, "ordnance", "inputs");
        const float broken   = a.base_price[static_cast<std::size_t>(resource_type::ordnance)] /
                               basket_value(in_ord, steel_dear);
        check(broken < anchor_floor,
              "R6d ...and tripling STEEL alone - one price, in one file - drops ordnance below "
              "the anchor, which a uniform reprice never would");
    }
    std::printf("\n");

    // -----------------------------------------------------------------------
    // R5 — the finding: the anchor BOUNDS credits_per_head_per_power
    // -----------------------------------------------------------------------
    std::printf("-- R5  the anchor bounds the power-scaled wage term (a finding, not a row) --\n");
    {
        int power_min = table.front().power_mod, power_max = table.front().power_mod;
        for (const roster_row& r : table)
        {
            power_min = (r.power_mod < power_min) ? r.power_mod : power_min;
            power_max = (r.power_mod > power_max) ? r.power_mod : power_max;
        }
        const float span = static_cast<float>(power_max - power_min);

        // The goods half is FLAT across the roster and the wage half is not, so
        // the ratio is widest-over-narrowest exactly as the wage is. In band for
        // every row requires
        //     wage(row) in [ equip / (2(1+tol)), equip / (2(1-tol)) ]
        // and wage is monotone in power_mod, so the two ends bind. Placing the
        // lightest row at the low end leaves the whole width for the span:
        const float wage_low  = cal.equipment / (k_anchor * (1.0f + k_tolerance));
        const float wage_high = cal.equipment / (k_anchor * (1.0f - k_tolerance));
        const float per_power_max = (span > 0.0f) ? (wage_high - wage_low) / span : 0.0f;

        std::printf("     roster power_mod span: %d..%d (%.0f points)\n", power_min, power_max, span);
        std::printf("     at this fixture's equipment value the whole roster fits the band only if\n"
                    "       credits_per_head in [%.4f, %.4f] and credits_per_head_per_power <= %.6f\n",
                    wage_low, wage_high, per_power_max);
        std::printf("     i.e. the heaviest row may cost at most %.3fx the lightest to WAGE,\n"
                    "       because it costs exactly 1.000x to EQUIP.\n",
                    (1.0f + k_tolerance) / (1.0f - k_tolerance));

        // At the bound, everything fits — placed a clear margin INSIDE it rather
        // than on the edge. `wage_low` is the exact ratio boundary, and a row
        // that sits on a boundary is decided by which way the last float rounds:
        // it would pass or fail on a compiler's licence rather than on the
        // finding it is stating. Step 2% of the band's width in from each end.
        const float inset                    = (wage_high - wage_low) * 0.02f;
        unit_upkeep_params at_bound          = cal.params;
        at_bound.credits_per_head            = wage_low + inset;
        at_bound.credits_per_head_per_power  = (per_power_max - 2.0f * inset / span) * 0.98f;
        const sweep at_bound_sweep           = sweep_roster(at_bound, a.base_price);
        check(at_bound_sweep.out_of_band == 0,
              "R5a a clear margin INSIDE the derived per-power bound the whole roster is in "
              "band - measured off the edge, not on it");

        // Past it, the heaviest row leaves the band while the lightest stays —
        // the asymmetry is the finding, and a check that only asserted the
        // failure would not show it.
        unit_upkeep_params past_bound         = at_bound;
        past_bound.credits_per_head_per_power = per_power_max * 2.0f;
        const sweep past_bound_sweep          = sweep_roster(past_bound, a.base_price);

        const reading* lightest = nullptr;
        const reading* heaviest = nullptr;
        for (const reading& r : past_bound_sweep.rows)
        {
            if (!lightest || r.row->power_mod < lightest->row->power_mod) lightest = &r;
            if (!heaviest || r.row->power_mod > heaviest->row->power_mod) heaviest = &r;
        }
        check(lightest && heaviest && lightest->in_band && !heaviest->in_band,
              "R5b at TWICE the bound the heaviest row falls out while the lightest holds "
              "- a flat goods table cannot follow a power-scaled wage");
        if (lightest && heaviest)
            std::printf("     -> %s (power %d) ratio %.3f in band; %s (power %d) ratio %.3f OUT\n\n",
                        lightest->row->name, lightest->row->power_mod, lightest->ratio,
                        heaviest->row->name, heaviest->row->power_mod, heaviest->ratio);
    }

    // -----------------------------------------------------------------------
    // R4 — units funded per year by the seeded extraction levy
    // -----------------------------------------------------------------------
    std::printf("-- R4  what the seeded extraction levy funds (NR-382's rate question) --\n");
    {
        const levy_probe levy = probe_seeded_levy();
        check(levy.seeded && levy.rate > 0.0f && std::isfinite(levy.rate),
              "R4a seed_prototype_laws seeds an enacted levy and evaluate_laws reads a "
              "positive finite rate back (the report below is not measuring a default)");
        std::printf("     seeded levy: %.4f cr per unit of raw output, in the author's jurisdiction\n",
                    levy.rate);

        const float authored_year = (a.upkeep.credits_per_head +
                                     basket_value(a.upkeep.goods_per_head, a.base_price)) *
                                    k_ticks_per_year;
        if (!rates_authored)
        {
            std::printf("     at the AUTHORED rates one head costs %.4f cr/year -> the levy funds an\n"
                        "       UNBOUNDED force. The question is degenerate until a rate is authored;\n"
                        "       the table below answers it for the calibrated fixture instead.\n\n",
                        authored_year);
            check(authored_year == 0.0f,
                  "R4b INERT: the degeneracy is exactly the inertness R3 reports, not a "
                  "measurement");
        }
        else
        {
            // The branch this row exists for: once a rate is authored, NR-382's
            // question stops being degenerate and gets a real answer.
            std::printf("     at the AUTHORED rates one head costs %.4f cr/year -> the levy funds\n"
                        "       %.1f heads per 1000 units/tick of taxed raw output.\n\n",
                        authored_year,
                        (1000.0f * static_cast<float>(k_ticks_per_year) * levy.rate) / authored_year);
            check(authored_year > 0.0f && std::isfinite(authored_year),
                  "R4b AUTHORED: one head-year costs a positive finite number of credits, so "
                  "NR-382's question has a real answer rather than a degenerate one");
        }

        // The rate-independent half of the answer: how much taxed raw output one
        // head-year costs. This is the number NR-382 can be answered against,
        // because it does not need a world — only the levy rate and the anchor.
        const float head_year = (cal.wage + cal.equipment) * k_ticks_per_year;
        const float units_per_head_year = head_year / levy.rate;
        std::printf("     calibrated fixture: %.4f cr/head/year (%.4f wage + %.4f equipment)\n",
                    head_year, cal.wage * k_ticks_per_year, cal.equipment * k_ticks_per_year);
        std::printf("     => ONE HEAD-YEAR COSTS %.2f UNITS of levied raw output at %.2f cr/unit.\n\n",
                    units_per_head_year, levy.rate);

        std::printf("     heads funded per year, by the jurisdiction's raw extraction throughput:\n");
        std::printf("     %14s %16s %16s\n", "output/tick", "levy cr/year", "heads funded");
        for (const float per_tick : {10.0f, 100.0f, 1000.0f, 10000.0f, 100000.0f})
        {
            const float revenue = per_tick * static_cast<float>(k_ticks_per_year) * levy.rate;
            std::printf("     %14.0f %16.1f %16.1f\n", per_tick, revenue, revenue / head_year);
        }
        std::printf("\n     THROUGHPUT IS PARAMETRIC ON PURPOSE. Measuring the real figure needs a\n"
                    "     generated world running the real recipe registry, and the registry is\n"
                    "     Lua - the excluded TU set for a headless harness. Read the row that\n"
                    "     matches a measured extraction volume rather than treating any one as the\n"
                    "     answer. (haulage_measure is the live-Lua harness that could supply it.)\n\n");

        // Vacuity guard on the arithmetic itself: the identity must close.
        const float revenue = 1000.0f * static_cast<float>(k_ticks_per_year) * levy.rate;
        const float heads   = revenue / head_year;
        check(std::fabs(heads * head_year - revenue) <= revenue * 1e-5f,
              "R4c the funding identity closes: heads x cost/head/year == levy revenue/year");
    }

    // -----------------------------------------------------------------------
    // Determinism — the standing invariant, asserted rather than assumed
    // -----------------------------------------------------------------------
    std::printf("-- D  determinism --\n");
    {
        const sweep again = sweep_roster(cal.params, a.base_price);
        const sweep once  = sweep_roster(cal.params, a.base_price);
        bool identical = again.rows.size() == once.rows.size();
        for (std::size_t i = 0; identical && i < once.rows.size(); ++i)
            identical = once.rows[i].wage == again.rows[i].wage &&
                        once.rows[i].equipment == again.rows[i].equipment &&
                        once.rows[i].ratio == again.rows[i].ratio;
        check(identical,
              "D1 two sweeps of one rate set are bit-identical (fixed roster order, fixed "
              "ascending resource-index accumulation, no RNG, no clock)");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
