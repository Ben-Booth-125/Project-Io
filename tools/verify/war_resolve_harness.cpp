// ---------------------------------------------------------------------------
// war_resolve_harness - can a war be fought back and ended, on the path the game runs?
// (Ben's 2026-11-09 playtest.) No SDL/Lua.
// ---------------------------------------------------------------------------
//   R1  a rival declares on a nation: the defender's direction stays non-hostile and its
//       front may not push until the free answer; the answer opens it.
//   R2  the winner holds every goal: the score, the imposition, a cession, the rival's reply.
//   R3  the goal is the loser's last province: no cession, and a white peace still ends it.
//   R4  how lopsided the battles must be before the score reaches 100.
//
// Declarations go through declare_war, so no row writes the defender's hostility by hand.
// Run: node tools/verify/build_harness.js war_resolve_harness --run
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corp_command.hpp"
#include "world/fronts.hpp"
#include "world/province.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/war.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

constexpr int W = 12, H = 6;

struct fixture
{
    world w;
    entity_id body = null_entity, A = null_entity, B = null_entity, C = null_entity;
    std::vector<entity_id> cell;
    entity_id at(int x, int y) const { return cell[static_cast<std::size_t>(y * W + x)]; }
    uint32_t  block(int k) const { return at(3 * k, 3); }
    uint32_t  mine() const { return at(0, 0); }
};

/// A owns rows 0-2 tile by tile. Rows 3-5 are four 3x3 blocks: the first @p b_blocks are B's,
/// the rest C's. `A` is the seated nation.
void build(fixture& f, int b_blocks)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Cylinder"; bc.type = body_type::planet; bc.grid_width = W; bc.grid_height = H;
    w.bodies[f.body] = bc;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = f.body; tc.grid_x = x; tc.grid_y = y;
            tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
            w.tiles[t] = tc;
            f.cell.push_back(t);
        }
    const auto nation = [&](const char* name) {
        const entity_id n = w.create_entity();
        nation_component nc{};
        nc.name = name; nc.treasury = 1.0e6f;
        w.nations[n] = nc;
        return n;
    };
    f.A = nation("Aster");
    f.B = nation("Brael");
    f.C = nation("Corvan");
    w.player_entity = f.A;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            const entity_id owner = y < 3 ? f.A : (x / 3 < b_blocks ? f.B : f.C);
            w.set_tile_nation(f.at(x, y), owner);
            w.nations.at(owner).tiles.push_back(f.at(x, y));
        }
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < W; ++x)
        {
            province p; p.id = f.at(x, y); p.body = f.body; p.tiles = { f.at(x, y) };
            w.provinces.provinces.push_back(p);
        }
    for (int k = 0; k < 4; ++k)
    {
        province p; p.id = f.block(k); p.body = f.body;
        for (int y = 3; y < H; ++y)
            for (int x = 3 * k; x < 3 * k + 3; ++x)
                p.tiles.push_back(f.at(x, y));
        std::sort(p.tiles.begin(), p.tiles.end());
        w.provinces.provinces.push_back(p);
    }
    std::sort(w.provinces.provinces.begin(), w.provinces.provinces.end(),
              [](const province& a, const province& b) { return a.id < b.id; });
    for (const province& p : w.provinces.provinces)
        for (const entity_id t : p.tiles)
            w.provinces.tile_province[t] = p.id;
    w.province_holder.assign(w.provinces.provinces.size(), null_entity);
    for (std::size_t i = 0; i < w.provinces.provinces.size(); ++i)
    {
        const auto own = w.tile_to_nation.find(w.provinces.provinces[i].tiles.front());
        if (own != w.tile_to_nation.end())
            w.province_holder[i] = own->second;
    }
}

void hold(fixture& f, uint32_t province_id, entity_id holder)
{
    auto& v = f.w.provinces.provinces;
    for (std::size_t i = 0; i < v.size(); ++i)
        if (v[i].id == province_id)
            f.w.province_holder[i] = holder;
}

front front_of(entity_id nation, entity_id neighbour)
{
    front f;
    f.nation    = nation;
    f.neighbour = neighbour;
    return f;
}

peace_terms white()
{
    peace_terms t;
    t.kind = peace_kind::white;
    return t;
}

peace_terms cession(entity_id gainer, uint32_t province_id)
{
    peace_terms t;
    t.kind      = peace_kind::cession;
    t.gainer    = gainer;
    t.provinces = { province_id };
    return t;
}

/// Run the rival's war evaluation once a tick from @p from for @p ticks; the tick the war
/// closed, or -1.
int ticks_to_close(fixture& f, const recipe_registry& reg, entity_id rival, int from, int ticks)
{
    for (int t = from; t < from + ticks; ++t)
    {
        f.w.current_econ_tick = t;
        score_nation_war(f.w, reg, rival, t, false);
        if (!at_war(f.w, f.A, rival))
            return t;
    }
    return -1;
}

} // namespace

int main()
{
    const recipe_registry reg;
    const sentiment_params& sp = reg.sentiment();

    std::printf("R1 a rival declares on the seated nation\n");
    {
        fixture f; build(f, 4);
        f.w.current_econ_tick = 20;
        check(declare_war(f.w, sp, f.B, f.A, { f.mine() }) == war_result::applied, "R1a declare_war applies");
        check(at_war(f.w, f.A, f.B), "R1b the pair is at war");
        check(is_hostile(f.w, f.B, f.A), "R1c the declarer's direction is hostile");
        check(!is_hostile(f.w, f.A, f.B), "R1d the defender's direction is NOT hostile until it answers");
        check(!front_may_push(f.w, front_of(f.A, f.B)), "R1e the defender's front may not push unanswered");
        check(front_may_push(f.w, front_of(f.B, f.A)), "R1f the declarer's front may push");
        check(answer_war(f.w, f.A, f.B, { f.block(0) }) == war_result::applied, "R1g the free answer applies");
        check(is_hostile(f.w, f.A, f.B), "R1h the answer writes the defender's hostility");
        check(front_may_push(f.w, front_of(f.A, f.B)), "R1i the answered front may push");
    }

    std::printf("R2 the winner holds every goal; the loser keeps other ground\n");
    {
        fixture f; build(f, 4);
        f.w.current_econ_tick = 20;
        (void)declare_war(f.w, sp, f.A, f.B, { f.block(0), f.block(1) });
        hold(f, f.block(0), f.A);
        hold(f, f.block(1), f.A);
        const war_record& war = *war_between(f.w, f.A, f.B);
        const int sc = war_score(f.w, war);
        std::printf("  MEASURED score with every goal held and no battle: %+d\n", sc);
        check(sc == 50, "R2a holding every goal without a battle scores 50");
        check(impose_peace(f.w, sp, f.A, f.B, white()) == war_result::rejected_state,
              "R2b imposition is refused below 100");
        check(check_peace_terms(f.w, war, cession(f.A, f.block(0))) == war_result::applied,
              "R2c the cession of a held goal is legal");
        check(offer_peace(f.w, f.A, f.B, cession(f.A, f.block(0))) == war_result::applied, "R2d the cession is offered");
        const int closed = ticks_to_close(f, reg, f.B, 20, 40);
        std::printf("  MEASURED the rival's reply to a cession: war %s (tick %d)\n",
                    closed < 0 ? "still open" : "closed", closed);
    }

    std::printf("R3 the goal is the loser's last province\n");
    {
        fixture f; build(f, 1);
        f.w.current_econ_tick = 20;
        (void)declare_war(f.w, sp, f.A, f.B, { f.block(0) });
        hold(f, f.block(0), f.A);
        const war_record& war = *war_between(f.w, f.A, f.B);
        check(check_peace_terms(f.w, war, cession(f.A, f.block(0))) == war_result::rejected_state,
              "R3a the last province may not be ceded");
        check(offer_peace(f.w, f.A, f.B, white()) == war_result::applied, "R3b a white peace may be offered");
        const int closed = ticks_to_close(f, reg, f.B, 24, 40);
        std::printf("  MEASURED the rival's reply to a white peace: closed tick %d\n", closed);
        check(closed >= 0, "R3c the white peace ends the war inside ten years");
        check(!at_war(f.w, f.A, f.B), "R3d no war remains");
    }

    std::printf("R4 what the battles must read for the score to reach 100\n");
    {
        fixture f; build(f, 4);
        (void)declare_war(f.w, sp, f.A, f.B, { f.block(0) });
        hold(f, f.block(0), f.A);
        war_record& war = f.w.wars.at({ f.A, f.B });
        const std::pair<int64_t, int64_t> losses[] = { { 0, 0 }, { 100, 1000 }, { 10, 1000 }, { 5, 1000 }, { 1, 1000 } };
        for (const auto& [ours, theirs] : losses)
        {
            war.declarer_men_lost = ours;
            war.defender_men_lost = theirs;
            std::printf("  MEASURED goals held, men lost %lld : %lld -> score %+d\n",
                        static_cast<long long>(ours), static_cast<long long>(theirs), war_score(f.w, war));
        }
    }

    std::printf("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
