// density_census — BL-1204 (density per good served): what the charter walk's
// density ceiling did, per body, on the SHIPPED start.
//
// Per seed: build_app_start_world (the app's begin_new_game +
// start_new_game_prelude, the shipped stockpile budget and spend), with the
// winner's charter_spend_report captured. Prints, per budgeted body: |G|, the
// density ceiling in force, the yards' places, the even share, the firms the
// walk chartered, and firms per good (every good of G, by name). Then the
// unspent points by reason over the whole spend, and the total background firms.
//
// A PURE READER of the report; nothing here writes the world.
//
// Build:  bash tools/verify/build_lua_harness.sh density_census
// Run:    build_gen/verify/density_census.exe [--seeds 0,43,10]

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/charter_budget.hpp"
#include "world/resource_names.hpp"
#include "world/world.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds{0, 43, 10};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ','))
                if (!t.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(t.c_str(), nullptr, 10)));
        }
        else
        {
            std::fprintf(stderr, "usage: density_census [--seeds a,b]\n");
            return 2;
        }
    }
    int failures = 0;
    for (const std::uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        charter_spend_report rep;
        harness_charter_input in;
        in.report = &rep;
        build_app_base_world(lua, p, *start);
        apply_app_start_landscape(*start, in);
        std::printf("=== seed %u ===  refused %d fell_back %d  specialists %zu firms %zu  "
                    "spent %lld / %lld\n",
                    seed, rep.refused ? 1 : 0, rep.fell_back ? 1 : 0, rep.specialists.size(),
                    rep.firms.size(), static_cast<long long>(rep.points_spent),
                    static_cast<long long>(rep.points_budgeted));
        if (rep.bodies.empty())
            ++failures;
        for (const charter_body_record& b : rep.bodies)
        {
            std::printf("  body %u: |G| %d ceiling %d yards %d share %d+%d cap %d charters %lld firms %d\n",
                        static_cast<unsigned>(b.body), b.goods_in_g, b.density_ceiling, b.yard_places,
                        b.even_share, b.even_share_extra, b.per_good_cap,
                        static_cast<long long>(b.firm_charters), b.firms);
            std::printf("    per good:");
            for (const std::uint16_t g : b.goods)
            {
                const int n = (g < b.firms_by_good.size()) ? b.firms_by_good[g] : 0;
                std::printf(" %s=%d", resource_names::name_of(static_cast<resource_type>(g)).c_str(), n);
            }
            std::printf("\n");
        }
        std::array<long long, charter_unspent_reason_count> u{};
        for (const charter_unspent& x : rep.unspent)
            u[static_cast<std::size_t>(x.reason)] += x.points;
        std::printf("  unspent:");
        for (int r = 0; r < charter_unspent_reason_count; ++r)
            if (u[static_cast<std::size_t>(r)] != 0)
                std::printf(" %s %lld", charter_unspent_reason_name(static_cast<charter_unspent_reason>(r)),
                            u[static_cast<std::size_t>(r)]);
        std::printf("\n  state_hash %016llx\n", static_cast<unsigned long long>(start->w.state_hash(0)));
    }
    return failures == 0 ? 0 : 1;
}
