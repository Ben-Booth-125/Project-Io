// ---------------------------------------------------------------------------
// Headless national-budget harness (BL-537; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// BL-537 gives nation_component::treasury a spend side. Until it, the treasury
// had only ever risen — NR-398's "a scoreboard with no game attached". This
// harness exercises src/world/nation_budget.{hpp,cpp} against the requirement
// group "national-budget":
//
//   R1  CONSERVATION, bit-exact, over any tick span. Every credit that leaves a
//       treasury lands on a NAMED corporation's balance in the same tick and
//       the same float — the world's total credit is unchanged, exactly, and
//       the BL-392 class of silent value destruction is absent at nation grain.
//   R2  INERTNESS at zero weights: a nation whose weights are all zero (and a
//       world where no nation authors a budget at all) leaves every treasury
//       and every balance bit-identical to what the pre-BL-537 build would
//       have left them — the pass is the only new call, so a world it does not
//       move IS the pre-item world.
//   R3  RESERVE respected, and an underspent line ACCUMULATES: only what is
//       actually paid leaves the treasury, so next tick's spendable is larger.
//       A reserve of 1.0 is a nation that banks everything.
//   R4  EMPTY TREASURY spends nothing and errors nowhere — zero, negative, an
//       unpayable claim, and a nation with no claims at all.
//   R5  DETERMINISM: two runs of one fixture are identical, and so is a run
//       whose claim vector arrives in the opposite order (the pass sorts into
//       its own walk order rather than trusting the producer's).
//
// Every number below is a dyadic rational chosen so the float arithmetic is
// EXACT: the conservation assertions are bit equality, deliberately not epsilon.
//
// The process exits non-zero if any assertion FAILs.

#include "world/nation_budget.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool same(float a, float b) { return a == b; } // bit-identical, deliberately not epsilon

constexpr std::size_t k_logistics   = static_cast<std::size_t>(budget_priority::logistics_maintenance);
constexpr std::size_t k_exploration = static_cast<std::size_t>(budget_priority::public_exploration);
constexpr std::size_t k_charters    = static_cast<std::size_t>(budget_priority::charters);

/// Two nations, two corporations. Nation A authors a budget; nation B is the
/// control (present, funded, but authoring nothing).
struct fixture
{
    world     w;
    entity_id nation_a = null_entity;
    entity_id nation_b = null_entity;
    entity_id corp_1   = null_entity;
    entity_id corp_2   = null_entity;

    std::map<entity_id, nation_budget> budgets;
    std::vector<budget_claim>          claims;
};

entity_id add_nation(world& w, const char* name, float treasury)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name     = name;
    nc.treasury = treasury;
    w.nations[n] = nc;
    return n;
}

entity_id add_corp(world& w, const char* name, float balance)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name    = name;
    cc.balance = balance;
    w.corporations[c] = cc;
    return c;
}

/// The reference fixture. Treasury 1000, reserve 1/4 -> spendable 750.
/// Weights 1/2, 1/4, 1/4 -> shares 375, 187.5, 187.5.
///   logistics   : corp_1 200 + corp_2 100 = 300 demand <= 375 share -> paid in full
///   exploration : corp_1 100 + corp_2 150 = 250 demand >  187.5      -> rationed x 0.75
///   charters    : no claim at all                                    -> skipped
/// So corp_1 takes 200 + 75 = 275, corp_2 takes 100 + 112.5 = 212.5,
/// the treasury pays 487.5 and keeps 512.5.
fixture make_fixture()
{
    fixture f;
    f.nation_a = add_nation(f.w, "Authoria",  1000.0f);
    f.nation_b = add_nation(f.w, "Elsewhere", 1000.0f);
    f.corp_1   = add_corp(f.w, "Payee One", 0.0f);
    f.corp_2   = add_corp(f.w, "Payee Two", 0.0f);

    nation_budget nb{};
    nb.reserve_fraction         = 0.25f;
    nb.weights[k_logistics]     = 0.5f;
    nb.weights[k_exploration]   = 0.25f;
    nb.weights[k_charters]      = 0.25f;
    f.budgets[f.nation_a] = nb;

    f.claims = {
        {f.nation_a, f.corp_1, budget_priority::logistics_maintenance, 200.0f},
        {f.nation_a, f.corp_2, budget_priority::logistics_maintenance, 100.0f},
        {f.nation_a, f.corp_1, budget_priority::public_exploration,    100.0f},
        {f.nation_a, f.corp_2, budget_priority::public_exploration,    150.0f},
    };
    return f;
}

constexpr float k_corp1_per_tick = 275.0f;
constexpr float k_corp2_per_tick = 212.5f;
constexpr float k_paid_per_tick  = 487.5f;

/// Total credit in the world: every corporation balance plus every treasury.
/// Summed in ASCENDING ENTITY ID so the sum itself is not iteration-dependent.
float world_credit(const world& w)
{
    std::map<entity_id, float> ordered;
    for (const auto& [id, cc] : w.corporations)
        ordered[id] = cc.balance;
    for (const auto& [id, nc] : w.nations)
        ordered[id] = nc.treasury;
    float total = 0.0f;
    for (const auto& [id, v] : ordered)
        total += v;
    return total;
}

/// Every mutable float the pass can touch, keyed by entity id.
std::map<entity_id, float> snapshot(const world& w)
{
    std::map<entity_id, float> s;
    for (const auto& [id, cc] : w.corporations)
        s[id] = cc.balance;
    for (const auto& [id, nc] : w.nations)
        s[id] = nc.treasury;
    return s;
}

national_budget_tick run(fixture& f)
{
    national_budget_tick t;
    run_national_budget(f.w, f.budgets, f.claims, &t);
    return t;
}

const nation_budget_result* result_for(const national_budget_tick& t, entity_id nation)
{
    for (const auto& r : t.nations)
        if (r.nation == nation)
            return &r;
    return nullptr;
}

} // namespace

int main()
{
    std::printf("=== national budget harness (BL-537) ===\n\n");

    // --- R1: every credit out is a transfer, conserved bit-exactly ----------
    std::printf("-- R1  a budget line is a direct transfer, never a leak --\n");
    {
        fixture f = make_fixture();
        const float credit_before = world_credit(f.w);
        const auto  t = run(f);
        const auto* a = result_for(t, f.nation_a);

        check(a != nullptr &&
              same(a->spendable, 750.0f) &&
              same(a->lines[k_logistics].share,   375.0f) &&
              same(a->lines[k_exploration].share, 187.5f) &&
              same(a->lines[k_charters].share,    187.5f),
              "R1a shares are spendable x normalised weight (750 -> 375 / 187.5 "
              "/ 187.5)");

        check(same(f.w.corporations.at(f.corp_1).balance, k_corp1_per_tick) &&
              same(f.w.corporations.at(f.corp_2).balance, k_corp2_per_tick),
              "R1b each NAMED corporation is credited exactly its claims "
              "(corp_1 275, corp_2 212.5) - direct, not via the market");

        check(a != nullptr &&
              same(a->paid, k_paid_per_tick) &&
              same(f.w.nations.at(f.nation_a).treasury, 1000.0f - k_paid_per_tick),
              "R1c the treasury is debited exactly what the corps were credited "
              "(487.5 out, 512.5 kept)");

        check(same(world_credit(f.w), credit_before),
              "R1d world credit is unchanged, bit-exactly - nothing minted, "
              "nothing destroyed");

        check(same(f.w.nations.at(f.nation_b).treasury, 1000.0f),
              "R1e a nation that authors no budget is untouched");
    }

    // --- R1 over a tick span ------------------------------------------------
    // The treasury is topped back up between ticks, exactly as the levy and
    // tariff credit it in the live loop, so each tick runs the same exact
    // arithmetic and the span total is checkable to the bit.
    {
        fixture   f    = make_fixture();
        constexpr int k_span = 8;
        bool per_tick_ok = true, conserved = true;
        for (int i = 0; i < k_span; ++i)
        {
            f.w.nations[f.nation_a].treasury = 1000.0f; // this tick's income
            const float credit_before = world_credit(f.w);
            const auto  t = run(f);
            const auto* a = result_for(t, f.nation_a);
            per_tick_ok = per_tick_ok && a != nullptr &&
                          same(a->paid, k_paid_per_tick) &&
                          same(t.total_transferred, k_paid_per_tick);
            conserved = conserved && same(world_credit(f.w), credit_before);
        }
        check(per_tick_ok,
              "R1f every tick of an 8-tick span pays the same exact total");
        check(conserved,
              "R1g ...and world credit is conserved bit-exactly on every one");
        check(same(f.w.corporations.at(f.corp_1).balance, k_corp1_per_tick * 8.0f) &&
              same(f.w.corporations.at(f.corp_2).balance, k_corp2_per_tick * 8.0f),
              "R1h ...and the span totals are the arithmetic totals "
              "(2200 and 1700)");
    }

    // --- R1 partial fill ----------------------------------------------------
    {
        fixture     f = make_fixture();
        const auto  t = run(f);
        const auto* a = result_for(t, f.nation_a);
        check(a != nullptr &&
              !a->lines[k_logistics].rationed &&
              same(a->lines[k_logistics].fill_fraction, 1.0f) &&
              same(a->lines[k_logistics].paid, 300.0f),
              "R1i a line whose demand fits is filled in full and says so");
        check(a != nullptr &&
              a->lines[k_exploration].rationed &&
              same(a->lines[k_exploration].demand, 250.0f) &&
              same(a->lines[k_exploration].paid,   187.5f) &&
              same(a->lines[k_exploration].fill_fraction, 0.75f),
              "R1j an over-subscribed line is PARTIALLY FILLED pro rata and "
              "says so (250 asked, 187.5 paid, fill 0.75) - never overdrawn, "
              "never silently dropped");
        check(a != nullptr && same(a->lines[k_charters].paid, 0.0f) &&
              same(a->lines[k_charters].share, 187.5f),
              "R1k a line with no consumer transfers to nobody and is skipped");
    }

    // --- R2: inertness ------------------------------------------------------
    std::printf("\n-- R2  authored-zero is bit-identical to the pre-item build --\n");
    {
        // Zero weights, claims present: the pass must move nothing.
        fixture f = make_fixture();
        for (float& weight : f.budgets[f.nation_a].weights)
            weight = 0.0f;
        const auto before = snapshot(f.w);
        const auto t = run(f);
        check(snapshot(f.w) == before && same(t.total_transferred, 0.0f),
              "R2a every weight zero: not one treasury or balance moves");

        // No nation authors a budget at all.
        fixture g = make_fixture();
        g.budgets.clear();
        const auto g_before = snapshot(g.w);
        const auto gt = run(g);
        check(snapshot(g.w) == g_before && same(gt.total_transferred, 0.0f),
              "R2b no budget authored anywhere: the pass returns having read "
              "nothing - the world IS the pre-BL-537 world");

        // No claims: the same, from the other side.
        fixture h = make_fixture();
        h.claims.clear();
        const auto h_before = snapshot(h.w);
        run(h);
        check(snapshot(h.w) == h_before,
              "R2c budget authored but no line has a consumer: nothing moves");
    }

    // --- R3: the reserve, and saving up -------------------------------------
    std::printf("\n-- R3  nations save --\n");
    {
        // Reserve 1.0 - banks everything.
        fixture f = make_fixture();
        f.budgets[f.nation_a].reserve_fraction = 1.0f;
        const auto before = snapshot(f.w);
        run(f);
        check(snapshot(f.w) == before,
              "R3a reserve_fraction 1.0 spends nothing at all");

        // An underspent line accumulates: one line, weight 1.0, demand well
        // under the share. Only what is PAID leaves, so the treasury keeps the
        // remainder and next tick's spendable is larger.
        fixture g;
        g.nation_a = add_nation(g.w, "Thrifty", 1000.0f);
        g.corp_1   = add_corp(g.w, "Payee One", 0.0f);
        nation_budget nb{};
        nb.reserve_fraction     = 0.25f;
        nb.weights[k_logistics] = 1.0f;
        g.budgets[g.nation_a]   = nb;
        g.claims = {{g.nation_a, g.corp_1, budget_priority::logistics_maintenance, 100.0f}};

        const auto t1 = run(g);
        const auto* r1 = result_for(t1, g.nation_a);
        check(r1 != nullptr && same(r1->spendable, 750.0f) &&
              same(r1->lines[k_logistics].share, 750.0f) &&
              same(r1->lines[k_logistics].paid,  100.0f) &&
              same(g.w.nations.at(g.nation_a).treasury, 900.0f),
              "R3b the reserve is withheld (750 of 1000 spendable), and only "
              "the 100 actually paid leaves - the unspent 650 stays put");

        const auto t2 = run(g);
        const auto* r2 = result_for(t2, g.nation_a);
        check(r2 != nullptr && same(r2->spendable, 675.0f) &&
              same(r2->lines[k_logistics].paid, 100.0f) &&
              same(g.w.nations.at(g.nation_a).treasury, 800.0f),
              "R3c ...and next tick spends from the accumulated balance, not "
              "from a line bucket that evaporated");

        // A weight set that does not sum to 1.0 is normalised on read rather
        // than allowed to overdraw.
        fixture h = make_fixture();
        for (float& weight : h.budgets[h.nation_a].weights)
            weight *= 4.0f; // sums to 4.0
        const auto ht = run(h);
        const auto* hr = result_for(ht, h.nation_a);
        check(hr != nullptr && same(hr->spendable, 750.0f) &&
              same(hr->lines[k_logistics].share, 375.0f) &&
              same(hr->paid, k_paid_per_tick),
              "R3d un-normalised weights are normalised on read - a nation can "
              "never allocate more than it holds");
    }

    // --- R4: empty treasury, and unpayable claims ---------------------------
    std::printf("\n-- R4  nothing to spend, nothing spent --\n");
    {
        fixture f = make_fixture();
        f.w.nations[f.nation_a].treasury = 0.0f;
        const auto before = snapshot(f.w);
        const auto t = run(f);
        check(snapshot(f.w) == before && same(t.total_transferred, 0.0f),
              "R4a an empty treasury spends nothing and errors nowhere");

        fixture g = make_fixture();
        g.w.nations[g.nation_a].treasury = -250.0f; // a debt, however it got there
        const auto g_before = snapshot(g.w);
        run(g);
        check(snapshot(g.w) == g_before,
              "R4b a negative treasury spends nothing and is not driven further "
              "down");

        // A claim naming a corp that does not exist must be dropped BEFORE it
        // enters the demand total: admitting it would debit a treasury with
        // nothing to credit.
        fixture h = make_fixture();
        h.claims.push_back({h.nation_a, 9999u, budget_priority::charters, 500.0f});
        const auto ht = run(h);
        const auto* hr = result_for(ht, h.nation_a);
        check(hr != nullptr && same(hr->lines[k_charters].demand, 0.0f) &&
              same(hr->paid, k_paid_per_tick) &&
              same(h.w.nations.at(h.nation_a).treasury, 1000.0f - k_paid_per_tick),
              "R4c a claim naming no real corporation is dropped at gather time "
              "- no phantom demand, no phantom debit");

        // Nonsense amounts are dropped the same way.
        fixture k = make_fixture();
        k.claims.push_back({k.nation_a, k.corp_1, budget_priority::charters, -50.0f});
        k.claims.push_back({k.nation_a, k.corp_2, budget_priority::charters,  0.0f});
        const auto kt = run(k);
        const auto* kr = result_for(kt, k.nation_a);
        check(kr != nullptr && same(kr->lines[k_charters].paid, 0.0f) &&
              same(k.w.corporations.at(k.corp_1).balance, k_corp1_per_tick),
              "R4d a non-positive claim never becomes a credit");

        // A nation named by a claim but authoring no budget pays nothing.
        fixture m = make_fixture();
        m.claims.push_back({m.nation_b, m.corp_1, budget_priority::charters, 400.0f});
        run(m);
        check(same(m.w.nations.at(m.nation_b).treasury, 1000.0f) &&
              same(m.w.corporations.at(m.corp_1).balance, k_corp1_per_tick),
              "R4e a claim on a nation with no authored budget pays nothing");
    }

    // --- R5: determinism ----------------------------------------------------
    std::printf("\n-- R5  same inputs, same floats --\n");
    {
        fixture a = make_fixture();
        fixture b = make_fixture();
        bool identical = true;
        for (int i = 0; i < 6; ++i)
        {
            run(a);
            run(b);
            identical = identical && snapshot(a.w) == snapshot(b.w);
        }
        check(identical,
              "R5a two runs of one fixture are bit-identical over six ticks");

        // The producer's claim order must not reach the arithmetic: the pass
        // sorts into (nation, line, corp, arrival) before it accumulates.
        fixture c = make_fixture();
        fixture d = make_fixture();
        std::reverse(d.claims.begin(), d.claims.end());
        bool order_free = true;
        for (int i = 0; i < 6; ++i)
        {
            run(c);
            run(d);
            order_free = order_free && snapshot(c.w) == snapshot(d.w);
        }
        check(order_free,
              "R5b a reversed claim vector produces bit-identical balances - "
              "the pass does not trust the producer's order");
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
