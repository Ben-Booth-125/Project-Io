// ---------------------------------------------------------------------------
// culture_preference_census — BL-1107. READ the cultural preference for goods
// (EXPLORATION.md sec A good acquires a cultural preference) on the shipped
// worlds, beside the Exploration and Industrialisation counters and the trade
// lanes it can move, so a change to the preference's inputs is measured rather
// than guessed at.
//
// REPORTS; DOES NOT GATE. A preference table is a reading, and whether it moved
// the right way is a judgement made off the printed numbers.
//
// Per seed, for each of the two handoffs (1660 Exploration close, 1960
// Industrialisation close):
//   - how many cultures prefer each good, the mean weight, and how many
//     cultures carry any preference at all;
//   - the span's counters: battles, conquests, foundings, subjections formed,
//     provinces bought;
//   - trade lanes: flows standing at the close, their volume, how many go by sea,
//     and sea lanes opened over the span.
// Then the same pooled over the seeds.
//
// Build (needs a live Lua state): bash tools/verify/build_lua_harness.sh culture_preference_census
// Run from the repo root (it reads scripts/*.lua):
//        build_gen/verify/culture_preference_census.exe [--seeds a,b,c] [--lack N] [--amenity-div N]
//        --lack / --amenity-div set world_params' two ground-profile magnitudes (a ladder
//        reading; both default to the shipped values).
//        (default: the 16 curated seeds of docs/generation/seed_library.json)
// ---------------------------------------------------------------------------

#include "harness_params.hpp" // load_app_generation_inputs: the app's config and works

#include "scripting/lua_state.hpp"
#include "world/era_minus_one.hpp"
#include "world/works_roster.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/settlement.hpp"
#include "world/world.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

const char* const good_names[4] = { "farm", "ore", "energy", "port" };

struct span_reading
{
    std::array<int64_t, 4> prefer{};     // cultures preferring each good
    std::array<int64_t, 4> weight_sum{}; // summed weight per good
    int64_t cultures_with_pref = 0;
    int64_t culture_count      = 0;
    int64_t entries            = 0;
    int64_t battles = 0, conquests = 0, foundings = 0, subjections = 0, bought = 0;
    int64_t flows = 0, volume = 0, by_sea = 0, lanes_opened = 0;
    uint64_t pref_hash = 14695981039346656037ull;

    void add(const span_reading& o)
    {
        for (int g = 0; g < 4; ++g) { prefer[g] += o.prefer[g]; weight_sum[g] += o.weight_sum[g]; }
        cultures_with_pref += o.cultures_with_pref;
        culture_count += o.culture_count;
        entries += o.entries;
        battles += o.battles; conquests += o.conquests; foundings += o.foundings;
        subjections += o.subjections; bought += o.bought;
        flows += o.flows; volume += o.volume; by_sea += o.by_sea; lanes_opened += o.lanes_opened;
    }
};

void fold(uint64_t& h, int64_t v)
{
    for (int i = 0; i < 8; ++i) { h ^= static_cast<uint64_t>((v >> (8 * i)) & 0xFF); h *= 1099511628211ull; }
}

int good_index(region_class g)
{
    switch (g)
    {
    case region_class::farm:   return 0;
    case region_class::ore:    return 1;
    case region_class::energy: return 2;
    case region_class::port:   return 3;
    default:                   return -1;
    }
}

span_reading read_span(const exploration_output& o, const history_sim_state& s)
{
    span_reading r;
    r.culture_count = o.culture_count;
    int last_culture = -1;
    for (const culture_good_preference& p : o.culture_preference)
    {
        const int gi = good_index(p.good);
        if (gi < 0) continue;
        ++r.entries;
        ++r.prefer[gi];
        r.weight_sum[gi] += p.weight_q;
        if (p.culture != last_culture) { ++r.cultures_with_pref; last_culture = p.culture; }
        fold(r.pref_hash, p.culture);
        fold(r.pref_hash, gi);
        fold(r.pref_hash, p.weight_q);
    }
    r.battles     = s.battles;
    r.conquests   = s.conquests;
    r.foundings   = s.foundings;
    r.subjections = s.subjections_formed;
    r.bought      = s.provinces_bought;
    for (const trade_flow& f : o.trade_flows)
    {
        ++r.flows;
        r.volume += f.volume_q;
        if (f.by_sea) ++r.by_sea;
    }
    r.lanes_opened = s.sea_lanes_opened;
    return r;
}

void print_span(const char* label, const span_reading& r)
{
    std::printf("  %-6s cultures %lld  with a preference %lld  entries %lld  pref-hash %016llx\n",
                label, static_cast<long long>(r.culture_count),
                static_cast<long long>(r.cultures_with_pref), static_cast<long long>(r.entries),
                static_cast<unsigned long long>(r.pref_hash));
    std::printf("         prefer:");
    for (int g = 0; g < 4; ++g)
        std::printf("  %s %lld (mean %lld)", good_names[g], static_cast<long long>(r.prefer[g]),
                    static_cast<long long>(r.prefer[g] ? r.weight_sum[g] / r.prefer[g] : 0));
    std::printf("\n         battles %lld  conquests %lld  foundings %lld  subjections %lld  bought %lld\n",
                static_cast<long long>(r.battles), static_cast<long long>(r.conquests),
                static_cast<long long>(r.foundings), static_cast<long long>(r.subjections),
                static_cast<long long>(r.bought));
    std::printf("         trade flows %lld  volume %lld  by sea %lld  sea lanes opened %lld\n",
                static_cast<long long>(r.flows), static_cast<long long>(r.volume),
                static_cast<long long>(r.by_sea), static_cast<long long>(r.lanes_opened));
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };
    int lack_q = world_params{}.culture_profile_lack_max_q;      // --lack N (BL-1107 ladder)
    int amenity_div = world_params{}.culture_profile_amenity_div; // --amenity-div N
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--lack") == 0 && i + 1 < argc)
        { lack_q = std::atoi(argv[++i]); continue; }
        if (std::strcmp(argv[i], "--amenity-div") == 0 && i + 1 < argc)
        { amenity_div = std::atoi(argv[++i]); continue; }
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t at = 0;
            while (at <= s.size())
            {
                const std::size_t c = s.find(',', at);
                const std::string tok = s.substr(at, c == std::string::npos ? std::string::npos : c - at);
                if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (c == std::string::npos) break;
                at = c + 1;
            }
        }
    }

    // THE APP'S INPUTS, not the C++ fallbacks: the parsed world_gen.lua config
    // and the works table, so the worlds read are the ones the game builds.
    lua_state        lua;
    world_gen_config cfg;
    works_registry   works;
    load_app_generation_inputs(lua, cfg, works);
    std::printf("=== culture_preference_census (BL-1107) — %d seeds, config %s, works %zu ===\n",
                static_cast<int>(seeds.size()), cfg.is_fallback ? "FALLBACK" : "world_gen.lua",
                static_cast<std::size_t>(works.size()));
    std::printf("profile magnitudes: lack ceiling %d, amenity divisor %d\n", lack_q, amenity_div);
    span_reading pool_e, pool_i;
    int profile_failures = 0;
    for (uint32_t seed : seeds)
    {
        world_params wp;
        wp.seed = seed;
        wp.culture_profile_lack_max_q  = lack_q;      // BL-1107 ladder
        wp.culture_profile_amenity_div = amenity_div;
        generation_report     rep;
        era_minus_one_fixture fx;
        const world w = make_hard_coded_world(wp, &rep, cfg, nullptr, &works, &fx);
        (void)w;
        std::printf("seed %u\n", seed);
        // THE PROFILE ITSELF (BL-1107): every culture coined, every daughter
        // carrying the profile of the culture it was coined from.
        {
            const std::vector<culture>& cs = fx.creeds.cultures;
            int coined = 0, daughters = 0, inherited = 0;
            std::array<int, 4> amen{};
            for (const culture& c : cs)
            {
                if (c.profile.coined()) ++coined;
                if (c.coined_from < 0)
                {
                    if (c.profile.amenity >= 0 && c.profile.amenity < 4) ++amen[static_cast<std::size_t>(c.profile.amenity)];
                    continue;
                }
                ++daughters;
                if (static_cast<std::size_t>(c.coined_from) < cs.size()
                    && cs[static_cast<std::size_t>(c.coined_from)].profile == c.profile)
                    ++inherited;
            }
            const bool ok = coined == static_cast<int>(cs.size()) && inherited == daughters;
            if (!ok) ++profile_failures;
            std::printf("  profile %s  cultures %d  coined %d  daughters %d  inherit %d  "
                        "cradle amenity open/forest/coastal/marsh %d/%d/%d/%d\n",
                        ok ? "PASS" : "FAIL", static_cast<int>(cs.size()), coined, daughters,
                        inherited, amen[0], amen[1], amen[2], amen[3]);
        }
        if (!fx.exploration_ran) { std::printf("  exploration did not run\n"); continue; }
        const span_reading e = read_span(fx.exploration_handoff, fx.exploration_state);
        print_span("1660", e);
        pool_e.add(e);
        if (fx.industrialisation_ran)
        {
            const span_reading in = read_span(fx.industrialisation_handoff, fx.industrialisation_state);
            print_span("1960", in);
            pool_i.add(in);
        }
        std::fflush(stdout);
    }
    std::printf("\n=== pooled ===\n");
    print_span("1660", pool_e);
    print_span("1960", pool_i);
    std::printf("%s  every culture carries a coined ground profile and every daughter its parent's "
                "(%d seed(s) failing)\n", profile_failures == 0 ? "PASS" : "FAIL", profile_failures);
    return profile_failures == 0 ? 0 : 1;
}
