// ---------------------------------------------------------------------------
// nation_targets_harness — the nation count is a target (BL-1278;
// NATION_GENERATION.md § The nation count is a target, Ben 2026-10-10)
// ---------------------------------------------------------------------------
// Builds each seed's shipped world the way the campaign does —
// `make_hard_coded_world` on the shipped data layer, then
// `finish_campaign_world` (the landscape search charters the specialists, the
// settle runs) — and reads the two counts the player's targets name:
//
//   TOTAL   nations alive on the homeworld when generation ends (a nation
//           holding at least one homeworld tile);
//   MAJORS  nations holding a champion when generation ends — read as a
//           nation that is the `home_nation` of at least one non-background
//           corporation after the finish (GLOSSARY.md § Major nation).
//
// TWO MODES.
//
//   --natural   the targets unset (0, 0): per seed, the total, the majors, the
//               specialists the carve ledger PLANNED (`plan_charters_by_
//               nation`, recorded before the search), the non-background
//               corporations actually chartered, and the living polities at
//               the Industrialisation span's last step. A census; exits 0.
//
//   grid        (default) every pair (total, majors) of {6, 20, 40} x {1, 3,
//               6} with total >= majors. For each seed the natural world is
//               built first (the gap a reconciliation must close), then each
//               pair: set the targets, build + finish, and check
//                 T  total  == the total target;
//                 M  majors == the major target;
//                 E  every correction carries a history event dated inside
//                    the Industrialisation span, none dated outside it.
//                    TOTAL (BL-1278 lane B fix round): every count change the
//                    closing rounds made has its event — the 25/26 events
//                    equal the rounds' own steps, and the steps equal the
//                    rounds' count delta (before -> after the floor -> after),
//                    however much the lean already closed. MAJORS: at least
//                    |natural - target| major-reconciliation events.
//                 H  the build recorded no handoff violation — a total the
//                    closing rounds could not reach is one (never shipped
//                    silently off target), and so is fold drift.
//               EXPECTED TO FAIL until BL-1278's lanes B (the total, inside
//               the history) and C (the majors, after the search) land: no
//               pass reads the targets yet, and no reconciliation event kind
//               exists, so E counts 0 and says so. Exits 1 on any failure.
//
// FLAGS. --seeds a,b,c (default: docs/generation/seed_library.json, in library
// order) and --pairs t:m,t:m (grid mode only) keep a run short. --floor N
// (BL-1278) runs the closing rounds at a size floor of N tiles
// (`era_minus_one_fixture::harness_floor_tiles`, a harness-only seam) on every
// build of the run, natural included: raised, it forces the case where no
// floor-safe secession exists and the total outranks the floor. Each grid
// line prints how many secessions were taken below the floor ("subfloor").
//
// COST. One build + finish is roughly a minute or two (Release); the full grid
// on sixteen seeds is 16 x (1 + 9) of them. Use --seeds / --pairs.
//
//   bash tools/verify/build_lua_harness.sh nation_targets_harness
//   ./build_gen/verify/nation_targets_harness.exe --natural
//   ./build_gen/verify/nation_targets_harness.exe --seeds 0,11 --pairs 6:1,20:3

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

/// THE RECONCILIATION EVENT, by kind. NONE EXISTS YET: lanes B and C of
/// BL-1278 add the kinds their verbs emit (closing-round absorption, a freed
/// subject or secession forced by the target, a state charter, a cross-border
/// buy-out) and extend this predicate in the same change. Until then it
/// answers false, and the E check counts 0 honestly — never a natural
/// `realm_ended` or `broke_away` passed off as a correction.
bool is_total_reconciliation(const lapse_event& e)
{
    // BL-1278 lane B: the closing rounds' two kinds — an absorption (the size
    // floor's, or one surplus to the total; the prose line carries which) and
    // a secession closing a total the history left short. Both are emitted
    // only by the closing rounds, never by the span's own verbs.
    return e.kind == static_cast<uint8_t>(lapse_event_kind::absorbed_at_close)
        || e.kind == static_cast<uint8_t>(lapse_event_kind::seceded_at_close);
}
bool is_major_reconciliation(const lapse_event& e)
{
    // BL-1278 lane C (`reconcile_majors`): a champion chartered in the richest
    // minor, the same where its treasury made up the price, and a cross-border
    // buy-out. Each is one correction of one major.
    const auto k = static_cast<lapse_event_kind>(e.kind);
    return k == lapse_event_kind::champion_chartered || k == lapse_event_kind::state_charter
        || k == lapse_event_kind::champion_bought_out
        || k == lapse_event_kind::major_demoted; // BL-1404: the poorest surplus major demoted
}

struct reading
{
    uint32_t seed            = 0;
    int      target_total    = 0;
    int      target_majors   = 0;
    int      total           = 0; ///< nations holding a homeworld tile
    int      empty_nations   = 0; ///< nations on the world with no tile at all
    int      majors          = 0; ///< nations home to a non-background corp
    int64_t  planned_specs   = 0; ///< the carve ledger's planned specialists
    int      chartered       = 0; ///< non-background corps after the finish
    int      polities        = 0; ///< living polities at Industrialisation's last step
    int      ind_start       = 0; ///< the Industrialisation span, for E
    int      ind_end         = 0;
    int      recon_total_in  = 0; ///< total-reconciliation events inside the span
    int      recon_major_in  = 0; ///< major-reconciliation events inside the span
    int      majors_pre      = 0; ///< majors just before reconcile_majors in THIS build (the gap E-major reads)
    int      recon_outside   = 0; ///< either kind dated outside the span (a defect)
    int      closing_steps   = 0; ///< the closing rounds' own steps (absorptions + secessions)
    int      subfloor        = 0; ///< secessions taken below the floor (the target outranks it)
    int      closing_delta   = 0; ///< |before - after floor| + |after floor - after|
    bool     closing_ran     = false;
    bool     handoff_invalid = false;
    std::string handoff_violation;
    double   build_s         = 0;
    double   finish_s        = 0;
    bool     ok              = false;
};

struct data_layer
{
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    recipe_registry  reg;
};

int g_floor = 0; ///< --floor: the harness-only closing-rounds floor (0 = as derived)

reading build_and_read(const data_layer& dl, uint32_t seed, int total_target, int major_target)
{
    reading r;
    r.seed          = seed;
    r.target_total  = total_target;
    r.target_majors = major_target;

    world_params p = arc_params(world_arc::shipped);
    p.seed                = seed;
    p.nation_total_target = total_target;
    p.major_target        = major_target;
    if (!nation_targets_valid(p))
    {
        std::printf("seed %u: targets %d/%d REJECTED by nation_targets_valid\n", seed, total_target,
                    major_target);
        return r;
    }

    generation_report     rep{};
    era_minus_one_fixture fx{};
    fx.harness_floor_tiles = g_floor;
    const auto t0 = std::chrono::steady_clock::now();
    world w = make_hard_coded_world(p, &rep, dl.cfg, nullptr, &dl.works, &fx);
    const auto t1 = std::chrono::steady_clock::now();
    recipe_registry seed_reg = dl.reg; // the worker's copy, banded inside the finish
    const finish_campaign_result fin = finish_campaign_world(w, rep, seed_reg, p, dl.cfg, nullptr);
    r.majors_pre = fin.majors.natural; // the majors the reconciliation found, after the history's total moved them
    const auto t2 = std::chrono::steady_clock::now();
    r.build_s  = std::chrono::duration<double>(t1 - t0).count();
    r.finish_s = std::chrono::duration<double>(t2 - t1).count();

    const generation_report::body_entry* home = nullptr;
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld) { home = &b; break; }
    if (home == nullptr) { std::printf("seed %u: no homeworld entry\n", seed); return r; }

    // TOTAL: a nation on the homeworld holds at least one of its tiles. Keyed
    // on the first tile's body; a nation's tiles are one body's by construction.
    for (const auto& kv : w.nations)
    {
        const nation_component& n = kv.second;
        if (n.tiles.empty()) { ++r.empty_nations; continue; }
        const auto t = w.tiles.find(n.tiles.front());
        if (t != w.tiles.end() && t->second.body == home->id) ++r.total;
    }

    // MAJORS: BL-1404 — the saved fact `is_major_nation` (the player's nation
    // included — the player is a major nation). Before BL-1404 this read the
    // nations home to a non-background corporation; a DEMOTED major keeps its
    // specialist until the fold empties it, so that reading no longer names
    // the majors. With no major target nothing is demoted and the two agree.
    for (const auto& kv : w.corporations)
        if (!kv.second.is_background && kv.second.home_nation != null_entity)
            ++r.chartered;
    for (const auto& kv : w.nations)
        if (is_major_nation(w, kv.first))
            ++r.majors;

    for (const auto& c : rep.carve_competitors) r.planned_specs += c.planned_specialists;

    const era_timelapse& ind = home->industrialisation_timelapse;
    if (!ind.steps.empty()) r.polities = static_cast<int>(ind.steps.back().sample_count);
    {
        const history_sim_state& hs = fx.industrialisation_state;
        r.closing_ran   = hs.closing_predicted >= 0;
        r.subfloor      = hs.closing_subfloor_secessions;
        r.closing_steps = hs.closing_floor_absorptions + hs.closing_target_absorptions
                        + hs.closing_target_secessions;
        if (r.closing_ran)
        {
            const int a = hs.closing_nations_before - hs.closing_nations_after_floor;
            const int b = hs.closing_nations_after_floor - hs.closing_predicted;
            r.closing_delta = (a < 0 ? -a : a) + (b < 0 ? -b : b);
        }
    }
    r.handoff_invalid   = rep.handoff_invalid;
    r.handoff_violation = rep.handoff_violation;
    r.ind_start = ind.start_year;
    r.ind_end   = ind.start_year + ind.years;
    for (const lapse_event& e : ind.events)
    {
        const bool tot = is_total_reconciliation(e), maj = is_major_reconciliation(e);
        if (!tot && !maj) continue;
        const bool inside = e.year >= r.ind_start && e.year <= r.ind_end;
        if (!inside) { ++r.recon_outside; continue; }
        r.recon_total_in += tot;
        r.recon_major_in += maj;
    }
    r.ok = true;
    return r;
}

std::vector<uint32_t> parse_seeds(const char* s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty()) out.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

bool parse_pairs(const char* s, std::vector<std::pair<int, int>>& out)
{
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        if (tok.empty()) continue;
        const std::size_t c = tok.find(':');
        if (c == std::string::npos) return false;
        out.emplace_back(std::atoi(tok.substr(0, c).c_str()), std::atoi(tok.substr(c + 1).c_str()));
    }
    return !out.empty();
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t>            seeds;
    std::vector<std::pair<int, int>> pairs;
    bool natural = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--natural") == 0) natural = true;
        else if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc) seeds = parse_seeds(argv[++i]);
        else if (std::strcmp(argv[i], "--floor") == 0 && i + 1 < argc) g_floor = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--pairs") == 0 && i + 1 < argc)
        {
            if (!parse_pairs(argv[++i], pairs))
            {
                std::printf("FATAL  --pairs wants t:m,t:m\n");
                return 2;
            }
        }
        else
        {
            std::printf("usage: nation_targets_harness [--natural] [--seeds a,b,c] [--pairs t:m,t:m] [--floor N]\n");
            return 2;
        }
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL  no seeds (docs/generation/seed_library.json)\n"); return 2; }
    if (pairs.empty())
        for (const int t : {6, 20, 40})
            for (const int m : {1, 3, 6})
                if (t >= m) pairs.emplace_back(t, m);

    // The shipped data layer, in app order: world_gen, works, then the recipe
    // registry the finish's settle runs on (gen_step_costs --finish's order).
    data_layer dl;
    dl.lua.load("scripts/world_gen.lua");
    dl.cfg.load_from_lua(dl.lua);
    dl.lua.load("scripts/works.lua");
    dl.works.load_from_lua(dl.lua);
    dl.lua.load("scripts/recipes.lua");
    dl.lua.load("scripts/economy.lua");
    dl.reg.load_from_lua(dl.lua);

    std::printf("nation_targets_harness: %s, %zu seed(s)%s\n", natural ? "--natural" : "grid", seeds.size(),
                g_floor > 0 ? (", closing-rounds floor raised to " + std::to_string(g_floor) + " tiles").c_str() : "");

    if (natural)
    {
        std::printf("%-6s %6s %7s %8s %10s %9s %8s %8s\n", "seed", "total", "majors", "planned",
                    "chartered", "polities", "build_s", "finish_s");
        int n = 0, tmin = 1 << 30, tmax = 0, mmin = 1 << 30, mmax = 0;
        long tsum = 0, msum = 0;
        for (const uint32_t seed : seeds)
        {
            const reading r = build_and_read(dl, seed, 0, 0);
            if (!r.ok) continue;
            std::printf("%-6u %6d %7d %8lld %10d %9d %8.1f %8.1f%s%s%s\n", r.seed, r.total, r.majors,
                        static_cast<long long>(r.planned_specs), r.chartered, r.polities, r.build_s,
                        r.finish_s, r.empty_nations > 0 ? "  (tileless nations present)" : "",
                        r.handoff_invalid ? "  HANDOFF VIOLATION: " : "",
                        r.handoff_invalid ? r.handoff_violation.c_str() : "");
            std::fflush(stdout);
            ++n;
            tsum += r.total; msum += r.majors;
            if (r.total < tmin) tmin = r.total;
            if (r.total > tmax) tmax = r.total;
            if (r.majors < mmin) mmin = r.majors;
            if (r.majors > mmax) mmax = r.majors;
        }
        if (n > 0)
            std::printf("\nSUMMARY natural over %d seeds: total %d..%d (mean %.1f), majors %d..%d (mean %.1f)\n",
                        n, tmin, tmax, static_cast<double>(tsum) / n, mmin, mmax,
                        static_cast<double>(msum) / n);
        return 0;
    }

    int checks = 0, fails = 0;
    for (const uint32_t seed : seeds)
    {
        const reading nat = build_and_read(dl, seed, 0, 0);
        if (!nat.ok) { ++fails; continue; }
        std::printf("seed %u natural: total %d, majors %d (industrialisation %d..%d)\n", seed, nat.total,
                    nat.majors, nat.ind_start, nat.ind_end);
        std::fflush(stdout);
        for (const auto& [tt, mt] : pairs)
        {
            const reading r = build_and_read(dl, seed, tt, mt);
            if (!r.ok) { ++fails; ++checks; continue; }
            const int  gap_t = nat.total > tt ? nat.total - tt : tt - nat.total;
            const int  gap_m = nat.majors > mt ? nat.majors - mt : mt - nat.majors;
            (void)gap_t; // the total's E no longer reads the natural gap (the lean closes part of it)
            const bool t_ok  = r.total == tt;
            const bool m_ok  = r.majors == mt;
            const bool et_ok = r.closing_ran && r.recon_total_in == r.closing_steps
                            && r.closing_steps == r.closing_delta && r.recon_outside == 0;
            // E-major reads the gap the reconciliation itself faced: the
            // history's total corrections absorb or split majors first, so the
            // natural build's count is the wrong reference (measured 2026-10-12:
            // 47 false misses on the 16 x 9 grid).
            const int  gap_pre = r.majors_pre > mt ? r.majors_pre - mt : mt - r.majors_pre;
            (void)gap_m;
            const bool em_ok = mt == 0 || (r.recon_major_in == gap_pre && r.recon_outside == 0)
                            || (r.handoff_invalid && r.recon_major_in < gap_pre);
            const bool e_ok  = et_ok && em_ok;
            const bool h_ok  = !r.handoff_invalid;
            checks += 4;
            fails += !t_ok + !m_ok + !e_ok + !h_ok;
            std::printf("  %s seed %-3u target %2d/%d  T total %2d (natural %2d) %s  M majors %d (natural %d) %s  "
                        "E events total %d/steps %d/delta %d %s major %d/%d %s outside %d  H %s%s  subfloor %d  [%.0f+%.0f s]\n",
                        (t_ok && m_ok && e_ok && h_ok) ? "PASS" : "FAIL", seed, tt, mt, r.total, nat.total,
                        t_ok ? "ok" : "MISS", r.majors, nat.majors, m_ok ? "ok" : "MISS", r.recon_total_in,
                        r.closing_steps, r.closing_delta, et_ok ? "ok" : "MISS", r.recon_major_in, gap_pre,
                        em_ok ? "ok" : "MISS", r.recon_outside, h_ok ? "ok" : "VIOLATION ",
                        h_ok ? "" : r.handoff_violation.c_str(), r.subfloor, r.build_s, r.finish_s);
            std::fflush(stdout);
        }
    }
    std::printf("\nSUMMARY grid: %d seed(s) x %zu pair(s), %d check(s), %d failure(s) -- %s\n", static_cast<int>(seeds.size()),
                pairs.size(), checks, fails, fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
