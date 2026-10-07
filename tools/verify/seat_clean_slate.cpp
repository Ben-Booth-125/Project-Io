// ---------------------------------------------------------------------------
// seat_clean_slate — BL-1206 (seat clean slate; Ben, 2026-10-07)
// ---------------------------------------------------------------------------
// CORPORATION_GENERATION.md § The spawn shortlist, and the seat, step 5: at the
// seat every building of the seated firm still under construction is cancelled
// and what it had consumed is refunded to the firm's balance; finished
// buildings stay. No SDL / Lua.
//
// Fixture: one body, one market stocked with steel. Firm S holds a FINISHED
// processor F and two construction SITES A and B (ticks_remaining 3). Every
// operating cost is zero (maintenance, wages; no laws, no units, no sales), so
// the only money S moves is what its two sites draw: the flat build_cost slice
// and the steel billed at clearing. Two real economy ticks run (economy step,
// clear, budget — the app's order), leaving both sites part-built.
//
//   R1 the record: each site's construction_paid equals what the two ticks took
//      from S's balance between them (B0 - B1 == paid_A + paid_B).
//   R2 the draw (repoint_player): A and B are gone — no building, no
//      stockpile, no asset entry — F stays, and S's balance rises by exactly
//      paid_A + paid_B, i.e. back to B0: what was consumed, never more.
//   R3 the pick (corp_verb::take_seat), on a copy of the same world: the same
//      outcome to the float.
//   R4 the books: the next filed return carries refunds == the refund, and its
//      flows reconstruct its net (income - expenditure - maintenance - wages -
//      interest - levies - upkeep + refunds); refund_unbooked is zero after.
//      And bit-exact telescoping across the seat: net == balance - the previous
//      return's balance, to the float.
//   R5 idempotent: seating again refunds nothing.
//
// Exits non-zero on any FAIL.
// Build: node tools/verify/build_harness.js seat_clean_slate
// ---------------------------------------------------------------------------

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr std::size_t r_steel = static_cast<std::size_t>(resource_type::steel);

bool near(double a, double b) { return std::fabs(a - b) <= 1e-3 * std::max(1.0, std::fabs(b)); }

struct fixture
{
    world w;
    entity_id body = null_entity, market = null_entity;
    entity_id s = null_entity, other = null_entity;
    entity_id f = null_entity, a = null_entity, b = null_entity;
};

entity_id add_tile(world& w, entity_id body)
{
    const entity_id t = w.create_entity();
    tile_component tc{};
    tc.body = body;
    tc.substrate = terrain_substrate::sedimentary;
    tc.cover = terrain_cover::grass; tc.cover_density = 150;
    w.tiles[t] = tc;
    return t;
}

entity_id add_proc(fixture& x, int ticks_remaining)
{
    const entity_id id = x.w.create_entity();
    building_component bc{};
    bc.tile = add_tile(x.w, x.body);
    bc.type = building_type::processing_facility;
    bc.workforce_assigned = 0.5f;
    bc.workforce_auto = false;
    bc.ticks_remaining = ticks_remaining;
    x.w.buildings[id] = bc;
    x.w.stockpiles[id] = stockpile_component{};
    x.w.corporations.at(x.s).assets.push_back(id);
    return id;
}

recipe_registry make_registry()
{
    recipe_registry reg;
    building_economics pe;
    pe.base_rate = 1.0f; pe.maintenance = 0.0f; pe.base_wage = 0.0f;
    pe.build_cost = 90.0f; pe.build_duration_ticks = 3.0f;
    pe.resource_build_cost[r_steel] = 30.0f; // 10 steel a full-rate tick
    reg.set_economics(building_type::processing_facility, pe);
    return reg;
}

fixture make_fixture()
{
    fixture x;
    x.body = x.w.create_entity();
    x.w.bodies[x.body] = body_component{};
    x.w.bodies[x.body].name = "Slate";
    x.w.home_body = x.body;
    x.market = x.w.create_entity();
    market_component mc{};
    mc.body = x.body;
    mc.base_price[r_steel] = 7.0f;
    mc.price = mc.base_price;
    mc.inventory[r_steel] = 1000.0f;
    x.w.markets[x.market] = mc;

    // S — the firm to be seated. Held as the player while the sites build, so
    // the scorer never acts on it (the econ_harness isolation idiom); the seat
    // below re-points onto it, which is the "pick the provisional" case.
    x.s = x.w.create_entity();
    corporation_component sc;
    sc.name = "Slate Works"; sc.balance = 5000.0f; sc.starting_capital = 5000.0f;
    sc.is_player = true;
    x.w.corporations[x.s] = sc;
    x.w.player_entity = x.s;
    // A second, penniless specialist so the world is not a one-firm world.
    x.other = x.w.create_entity();
    corporation_component oc;
    oc.name = "Other"; oc.balance = 0.0f;
    x.w.corporations[x.other] = oc;

    x.f = add_proc(x, 0); // finished
    x.a = add_proc(x, 3); // site
    x.b = add_proc(x, 3); // site
    return x;
}

const quarterly_return* tick(world& w, const recipe_registry& reg, entity_id corp, int t)
{
    w.current_econ_tick = t;
    economy_report rep = run_economy_step(w, reg);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention);
    const auto& rs = w.corporations.at(corp).returns;
    return rs.empty() ? nullptr : &rs.back();
}

bool gone(const world& w, entity_id corp, entity_id bid)
{
    const auto& as = w.corporations.at(corp).assets;
    return w.buildings.count(bid) == 0 && w.stockpiles.count(bid) == 0
        && std::find(as.begin(), as.end(), bid) == as.end();
}

} // namespace

int main()
{
    std::printf("seat_clean_slate — BL-1206\n");
    const recipe_registry reg = make_registry();
    fixture x = make_fixture();
    const double b0 = x.w.corporations.at(x.s).balance;
    tick(x.w, reg, x.s, 0);
    tick(x.w, reg, x.s, 1);
    const double b1 = x.w.corporations.at(x.s).balance;
    const double pa = x.w.buildings.at(x.a).construction_paid;
    const double pb = x.w.buildings.at(x.b).construction_paid;
    std::printf("  balance %.4f -> %.4f after two ticks; paid A %.4f B %.4f; sites left A %d B %d ticks\n",
                b0, b1, pa, pb, x.w.buildings.at(x.a).ticks_remaining, x.w.buildings.at(x.b).ticks_remaining);
    check(pa > 0.0 && pb > 0.0 && x.w.buildings.at(x.a).ticks_remaining > 0
              && x.w.buildings.at(x.b).ticks_remaining > 0,
          "fixture: both sites have been charged and are still under construction");
    check(near(b0 - b1, pa + pb),
          "R1 the record: construction_paid A + B == what the two ticks took from S");

    world copy = x.w; // R3 runs the pick on the same world

    // R2 — the draw's re-point.
    const bool ok = repoint_player(x.w, x.s);
    const double b2 = x.w.corporations.at(x.s).balance;
    std::printf("  draw: balance %.4f -> %.4f (refund %.4f)\n", b1, b2, b2 - b1);
    check(ok && gone(x.w, x.s, x.a) && gone(x.w, x.s, x.b),
          "R2 draw: both sites cancelled — no building, stockpile or asset entry left");
    check(x.w.buildings.count(x.f) == 1, "R2 draw: the finished building stays");
    check(near(b2 - b1, pa + pb) && near(b2, b0),
          "R2 draw: balance += exactly what the sites consumed (S is back to its opening balance)");

    // R3 — the pick.
    corp_command cmd;
    cmd.corp = x.s;
    cmd.verb = corp_verb::take_seat;
    const corp_command_result res = apply_corp_command(copy, reg, cmd);
    const double b3 = copy.corporations.at(x.s).balance;
    check(res == corp_command_result::applied && gone(copy, x.s, x.a) && gone(copy, x.s, x.b)
              && copy.buildings.count(x.f) == 1 && b3 == b2,
          "R3 pick (take_seat): the same cancellation and the same balance, to the float");

    // R4 — the books.
    const float prev_close = x.w.corporations.at(x.s).returns.back().balance; // tick 1's return
    const quarterly_return* q = tick(x.w, reg, x.s, 2);
    const double rebuilt = q ? static_cast<double>(q->income) - q->expenditure - q->maintenance
                                   - q->wages - q->interest - q->levies - q->upkeep + q->refunds
                             : 0.0;
    std::printf("  next return: refunds %.4f net %.4f rebuilt %.4f\n", q ? q->refunds : 0.0f,
                q ? q->net : 0.0f, rebuilt);
    check(q != nullptr && near(q->refunds, pa + pb),
          "R4 the next return books the refund as `refunds`");
    check(q != nullptr && near(rebuilt, q->net), "R4 its flows reconstruct its net");
    // Bit-exact across the seat: the booking return opens from the previous
    // return's close itself (the stored pre-refund balance), not from
    // fl(fl(C0 + R) - R), so net is exactly the difference of the two closes.
    check(q != nullptr && q->net == q->balance - prev_close,
          "R4 bit-exact telescoping across the seat: net == balance - previous return's balance");
    check(x.w.corporations.at(x.s).refund_unbooked == 0.0f, "R4 nothing is left unbooked");

    // R4b — the rounding case the stored opening exists for: a balance and a
    // refund whose float sum does not subtract back exactly.
    {
        // Find (C0, R) where the subtraction form gives a DIFFERENT net from the
        // stored opening: fl(c - C0) != fl(c - fl(c - R)), c = fl(C0 + R). A
        // fixed deterministic walk; the row is vacuous if none is found.
        volatile float c0 = 0.0f, r = 3.0f;
        bool rounds = false;
        for (int k = 1; k < 100000 && !rounds; ++k)
        {
            c0 = 0.1f * static_cast<float>(k);
            const float c = c0 + r;
            const float back = c - r;
            rounds = back != c0 && (c - c0) != (c - back);
        }
        fixture y = make_fixture();
        corporation_component& yc = y.w.corporations.at(y.s);
        yc.balance = c0;
        y.w.buildings.at(y.a).construction_paid = r;
        y.w.buildings.at(y.b).construction_paid = 0.0f;
        repoint_player(y.w, y.s);
        const quarterly_return* yq = tick(y.w, reg, y.s, 0);
        std::printf("  rounding case: fl(fl(C0+R)-R) %s C0; net %.9g, balance - C0 %.9g\n",
                    rounds ? "!=" : "==", yq ? yq->net : 0.0f, yq ? yq->balance - c0 : 0.0f);
        check(rounds && yq != nullptr && yq->net == yq->balance - c0,
              "R4b where fl(fl(C0+R)-R) != C0, net is still exactly balance - C0 (the stored opening)");
    }

    // R5 — idempotent.
    const double b4 = x.w.corporations.at(x.s).balance;
    check(seat_clean_slate(x.w, x.s) == 0.0f && x.w.corporations.at(x.s).balance == b4,
          "R5 seating again refunds nothing");

    std::printf("\n%s (%d failure%s)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail,
                g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
