// ---------------------------------------------------------------------------
// Headless national-budget harness (BL-537; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// BL-537 gives nation_component::treasury a spend side. Until it, the treasury
// had only ever risen — NR-398's "a scoreboard with no game attached". This
// harness exercises src/world/nation_budget.{hpp,cpp} against the requirement
// group "national-budget":
//
//   R1  CONSERVATION. Every credit that leaves a treasury lands on a NAMED
//       corporation's balance in the same tick and the same float. The pass
//       debits EXACTLY what it credits — that half is bit-exact for any inputs,
//       and is asserted as such on a non-dyadic fixture. The world's total
//       credit is then bit-exact too whenever each credit is representable at
//       its destination; where it is not, the residual is the destination's own
//       `balance += amount` rounding — the same rounding `apply_budget` has
//       always carried — and R1 asserts it is ROUNDING and not a leak: sign
//       varies, magnitude stays at float epsilon, and it vanishes entirely when
//       the destinations are small enough to hold the credit exactly.
//       (Narrowed 2026-08-23 after the Sprint N1 adversarial pass showed the
//       original "bit-exact over any tick span" claim was carried by a fixture
//       engineered from dyadic rationals, and failed at ordinary weights.)
//   R2  INERTNESS at zero weights: a nation whose weights are all zero (and a
//       world where no nation authors a budget at all) leaves every treasury
//       and every balance bit-identical to what the pre-BL-537 build would
//       have left them — the pass is the only new call, so a world it does not
//       move IS the pre-item world.
//   R3  RESERVE respected, and an underspent line ACCUMULATES: only what is
//       actually paid leaves the treasury, so next tick's spendable is larger.
//       A reserve of 1.0 is a nation that banks everything.
//   R4  EMPTY TREASURY spends nothing and errors nowhere — zero, negative, an
//       unpayable claim, a claim naming a line outside the enum, and a nation
//       with no claims at all. NEVER OVERDRAWN: no fixture drives a treasury
//       below its reserve floor, and none drives it negative.
//
// RUN THIS HARNESS UNDER AddressSanitizer AS WELL AS PLAIN. R4f (a claim naming a
// line outside the enum) is a memory row: without the gather-time bounds check it
// is an out-of-bounds write, and an out-of-bounds write is not something a value
// assertion can see. The unfixed pass passes all 34 value rows on a plain build
// and aborts at `nation_budget.cpp:62` the moment ASan is on. Build it with
//   g++ -std=c++20 -O1 -g -fsanitize=address,undefined -Isrc ...
// and treat a clean run as part of the pass, not as a bonus.
//   R5  DETERMINISM: two runs of one fixture are identical, and so is a run
//       whose claim vector arrives in the opposite order (the pass sorts into
//       its own walk order rather than trusting the producer's).
//
// Every number in the REFERENCE fixture is a dyadic rational chosen so the float
// arithmetic is exact: those assertions are bit equality, deliberately not
// epsilon. That is a property of the fixture and not of the pass, which is why
// `awkward_fixture` exists alongside it — ordinary weights, an awkward reserve
// and prices that are not powers of two, so a row cannot pass by construction of
// its own inputs.
//
// The process exits non-zero if any assertion FAILs.

#include "world/nation_budget.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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

/// World credit summed in DOUBLE, so a residual smaller than a float ULP of the
/// total is visible rather than swallowed by the measurement itself. Ascending
/// entity id, as above.
double world_credit_exact(const world& w)
{
    std::map<entity_id, double> ordered;
    for (const auto& [id, cc] : w.corporations)
        ordered[id] = static_cast<double>(cc.balance);
    for (const auto& [id, nc] : w.nations)
        ordered[id] = static_cast<double>(nc.treasury);
    double total = 0.0;
    for (const auto& [id, v] : ordered)
        total += v;
    return total;
}

/// Nothing dyadic anywhere: an awkward treasury, an awkward reserve, weights that
/// do not sum to 1.0 and are not representable, and claims priced off the grid.
/// The reference fixture proves the ARITHMETIC; this one proves the arithmetic is
/// not what was making the reference fixture pass.
fixture make_awkward_fixture()
{
    fixture f;
    f.nation_a = add_nation(f.w, "Awkwardia", 1234.56f);
    f.nation_b = add_nation(f.w, "Elsewhere",  777.77f);
    f.corp_1   = add_corp(f.w, "Payee One",   0.0f);
    f.corp_2   = add_corp(f.w, "Payee Two",   0.0f);

    nation_budget nb{};
    nb.reserve_fraction       = 0.13f;
    nb.weights[k_logistics]   = 0.37f;
    nb.weights[k_exploration] = 0.21f;
    nb.weights[k_charters]    = 0.42f;
    f.budgets[f.nation_a] = nb;

    f.claims = {
        {f.nation_a, f.corp_1, budget_priority::logistics_maintenance, 123.457f},
        {f.nation_a, f.corp_2, budget_priority::logistics_maintenance,  77.13f},
        {f.nation_a, f.corp_1, budget_priority::public_exploration,    311.9f},
        {f.nation_a, f.corp_2, budget_priority::public_exploration,     88.888f},
        {f.nation_a, f.corp_2, budget_priority::charters,              404.04f},
        {f.nation_a, f.corp_1, budget_priority::charters,               19.191f},
    };
    return f;
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

    // --- R1 at ordinary weights --------------------------------------------
    // The rows above run on dyadic rationals, where every intermediate float is
    // exact. That makes them a check on the ARITHMETIC and not on the pass: a
    // conservation row cannot fail on a fixture whose every operation is exact.
    // Everything below runs on `make_awkward_fixture` instead.
    std::printf("\n-- R1  ...and at ordinary weights, where nothing is exact --\n");
    {
        // The half that IS bit-exact for any inputs: the treasury is debited
        // EXACTLY the total the report says was transferred, and exactly the sum
        // of the credits issued. `total_transferred` and the treasury delta are
        // the same accumulation, so this holds however awkward the weights.
        fixture     f      = make_awkward_fixture();
        const float before = f.w.nations.at(f.nation_a).treasury;
        const auto  t      = run(f);
        const auto* a      = result_for(t, f.nation_a);

        check(a != nullptr &&
              same(f.w.nations.at(f.nation_a).treasury, before - a->paid) &&
              same(t.total_transferred, a->paid),
              "R1l at non-dyadic weights the treasury is debited EXACTLY the "
              "reported total - the debit half is bit-exact for any inputs");

        // ...and the credits are that same total. Corp balances start at zero, so
        // each credit lands exactly and the world total is bit-exact too.
        check(same(f.w.corporations.at(f.corp_1).balance +
                   f.w.corporations.at(f.corp_2).balance, a ? a->paid : -1.0f),
              "R1m ...and lands, to the bit, on the named corporations");

        check(same(f.w.nations.at(f.nation_b).treasury, 777.77f),
              "R1n ...while the nation authoring nothing is untouched");
    }
    {
        // Where the world total is NOT bit-exact, the residual is the
        // destination's own `balance += amount` rounding and nothing else. Two
        // 64-tick spans on the same awkward fixture prove it: one lets balances
        // accumulate until a credit is below half a ULP of them, the other sweeps
        // the balances each tick so every credit lands exactly.
        //
        // The swept span must be EXACTLY zero. If it is not, the pass itself is
        // losing credit and this row is the one that says so.
        auto span = [](bool sweep, int ticks, double& cum, double& moved,
                       int& credits, float& peak_balance)
        {
            fixture f = make_awkward_fixture();
            cum = moved = 0.0; credits = 0; peak_balance = 0.0f;
            for (int i = 0; i < ticks; ++i)
            {
                if (sweep)
                    for (auto& [id, cc] : f.w.corporations)
                        cc.balance = 0.0f;
                f.w.nations[f.nation_a].treasury = 1234.56f; // this tick's income
                const double c0 = world_credit_exact(f.w);
                const auto   t  = run(f);
                cum   += world_credit_exact(f.w) - c0;
                moved += static_cast<double>(t.total_transferred);
                credits += static_cast<int>(f.claims.size());
                for (const auto& [id, cc] : f.w.corporations)
                    peak_balance = std::max(peak_balance, cc.balance);
            }
        };

        constexpr int k_ticks = 64;
        double cum_swept = 1.0, moved_swept = 0.0, cum_accum = 0.0, moved_accum = 0.0;
        int    cred_s = 0, cred_a = 0;
        float  peak_s = 0.0f, peak_a = 0.0f;
        span(true,  k_ticks, cum_swept, moved_swept, cred_s, peak_s);
        span(false, k_ticks, cum_accum, moved_accum, cred_a, peak_a);

        check(cum_swept == 0.0,
              "R1o 64 ticks at ordinary weights, destinations able to hold each "
              "credit exactly: world credit moves by EXACTLY zero - the pass "
              "itself neither mints nor destroys");

        // The accumulating span WILL drift, and that is not a defect: adding ~540
        // to a balance of ~34000 rounds, exactly as `apply_budget`'s
        // `cc.balance += delta` has always rounded. The bound is therefore the
        // DESTINATIONS' OWN rounding budget - half a ULP of the largest balance,
        // once per credit applied - and not a magic epsilon. A leak proportional
        // to throughput would be four orders above it.
        const float  ulp    = std::nextafter(peak_a, 2.0f * peak_a) - peak_a;
        const double budget = 0.5 * static_cast<double>(cred_a) * static_cast<double>(ulp);
        check(budget > 0.0 && std::fabs(cum_accum) <= budget,
              "R1p ...and where they cannot, the residual stays inside the "
              "destinations' own rounding budget (half a ULP per credit), which "
              "is orders below anything throughput-proportional");

        // And the residual is the DESTINATION's, not the pass's: both spans
        // transferred the identical total, bit for bit - the pass did the same
        // work in each - and only the one whose balances had outgrown their
        // credits lost anything at all.
        check(moved_swept == moved_accum && cum_swept == 0.0 && cum_accum != 0.0,
              "R1q ...and it is the destination's rounding and not the pass's: "
              "two spans move the identical total, one loses exactly zero");
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

        // NEVER OVERDRAWN, under pressure. Fifty corps each asking for the whole
        // treasury on one line, across sixteen ticks with no income: the reserve
        // floor must hold every tick and the treasury must never go negative.
        // The bound this exercises is the running-total headroom; the decremented
        // remainder it replaced stalls once a pay falls below half a ULP of it
        // (Sprint N1 defect 2), and a stalled bound is an unbounded overdraw.
        fixture n;
        n.nation_a = add_nation(n.w, "Besieged", 1000.0f);
        nation_budget sb{};
        sb.reserve_fraction     = 0.4f;   // floor at 400
        sb.weights[k_logistics] = 1.0f;
        n.budgets[n.nation_a]   = sb;
        for (int i = 0; i < 50; ++i)
        {
            const entity_id c = add_corp(n.w, "Claimant", 0.0f);
            n.claims.push_back({n.nation_a, c, budget_priority::logistics_maintenance, 1000.0f});
        }
        //
        // The floor is stated in the pass's own terms - `treasury - spendable`,
        // which is exactly what rule 2 withholds - rather than recomputed as
        // `treasury x reserve_fraction`. The two differ by a ULP because 0.4f is
        // not representable, and a requirement row that fails on the
        // irrepresentability of its own constant is testing arithmetic, not the
        // subject.
        bool floor_held = true, never_negative = true, within_spendable = true;
        for (int i = 0; i < 16; ++i)
        {
            const float before = n.w.nations.at(n.nation_a).treasury;
            const auto  t  = run(n);
            const auto* r  = result_for(t, n.nation_a);
            const float now = n.w.nations.at(n.nation_a).treasury;
            floor_held      = floor_held && r != nullptr && now >= before - r->spendable;
            never_negative  = never_negative && now >= 0.0f;
            within_spendable = within_spendable && r != nullptr && r->paid <= r->spendable;
        }
        check(floor_held && never_negative && within_spendable,
              "R3e fifty claimants against one line for sixteen ticks: the "
              "reserve floor holds every tick, the treasury never goes negative, "
              "and no tick pays out more than it allotted");

        // The other half of the same bound: MANY SMALL claims must each be paid
        // and the treasury must fall by exactly their sum. A decremented
        // remainder would still pay them - and would not notice it had.
        fixture q;
        q.nation_a = add_nation(q.w, "Granular", 1000.0f);
        nation_budget qb{};
        qb.reserve_fraction     = 0.25f;
        qb.weights[k_logistics] = 1.0f;
        q.budgets[q.nation_a]   = qb;
        for (int i = 0; i < 256; ++i)
        {
            const entity_id c = add_corp(q.w, "Small", 0.0f);
            q.claims.push_back({q.nation_a, c, budget_priority::logistics_maintenance, 0.001f});
        }
        const auto  qt = run(q);
        const auto* qr = result_for(qt, q.nation_a);
        float credited = 0.0f;
        for (const auto& [id, cc] : std::map<entity_id, corporation_component>(
                 q.w.corporations.begin(), q.w.corporations.end()))
            credited += cc.balance;
        check(qr != nullptr && qr->paid > 0.0f &&
              same(q.w.nations.at(q.nation_a).treasury, 1000.0f - qr->paid) &&
              std::fabs(credited - qr->paid) <= 1e-6f,
              "R3f 256 claims of 0.001 are each paid and the treasury falls by "
              "exactly their sum - small transfers are summed among themselves "
              "before they meet a treasury large enough to absorb them");
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

        // A LINE OUTSIDE THE ENUM. `budget_priority` has a `uint8_t` underlying
        // type, so 0..255 are all valid VALUES while only 0..8 index the weight
        // vector: the gather loop must reject the claim, not index on it.
        // Without the check this is a heap-buffer-overflow, confirmed by
        // AddressSanitizer (Sprint N1 defect 1) - and the standing rule that an
        // AI-facing seam is an untrusted input boundary makes it wire-reachable
        // the moment a claim arrives over `--serve`.
        fixture p = make_fixture();
        budget_claim rogue{};
        rogue.nation = p.nation_a;
        rogue.corp   = p.corp_1;
        rogue.amount = 500.0f;
        rogue.line   = static_cast<budget_priority>(200);
        p.claims.push_back(rogue);
        const auto pt = run(p);
        const auto* pr = result_for(pt, p.nation_a);
        check(pr != nullptr && same(pr->paid, k_paid_per_tick) &&
              same(p.w.corporations.at(p.corp_1).balance, k_corp1_per_tick) &&
              same(p.w.nations.at(p.nation_a).treasury, 1000.0f - k_paid_per_tick),
              "R4f a claim naming a line outside the enum is dropped at gather "
              "time - the tick is bit-identical to the one without it");

        // NEVER OVERDRAWN, across 512 generated shapes rather than one authored
        // one. The reference fixture cannot reach this: the bound only fails when
        // enough claims land on enough lines that the low bits it drops add up.
        //
        // The generator is a seeded xorshift walked in a fixed order - pure,
        // replayable, and part of the CHECK rather than of the simulation, so the
        // determinism rule is untouched. What it varies is exactly what the
        // authored fixtures hold still: nation and corp counts, treasuries,
        // reserves, weight sets, claim counts and claim amounts.
        //
        // The defect it exists for: the decremented-remainder bound this pass
        // shipped with under-counts what has been paid (each `remaining -= pay`
        // drops the low bits of `pay`), so a nation keeps paying past its
        // allotment. Measured against the quarantined original over exactly
        // these 512 shapes: 156 nations overdrew and 26 solvent treasuries were
        // driven NEGATIVE, by up to 0.0024 credits. That is ULP-scale in size and
        // not cosmetic in effect, because the next tick's `treasury > 0` gate
        // then locks that nation out of spending entirely. (Sprint N1 defect 2,
        // reproduced and quantified 2026-08-23.)
        {
            auto next = [](std::uint32_t& st) {
                st ^= st << 13; st ^= st >> 17; st ^= st << 5; return st;
            };
            auto real = [&next](std::uint32_t& st, float lo, float hi) {
                return lo + (hi - lo) *
                       (static_cast<float>(next(st) & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu));
            };

            bool never_overpaid = true, never_negative = true;
            int  scenarios = 0;
            for (std::uint32_t seed = 1; seed <= 512; ++seed)
            {
                std::uint32_t st = seed;
                fixture s;
                std::vector<entity_id> nats, corps;
                const int n_nations = 1 + static_cast<int>(next(st) % 4);
                const int n_corps   = 1 + static_cast<int>(next(st) % 8);
                for (int i = 0; i < n_nations; ++i)
                    nats.push_back(add_nation(s.w, "Fuzzland", real(st, -50.0f, 5000.0f)));
                for (int i = 0; i < n_corps; ++i)
                    corps.push_back(add_corp(s.w, "Claimant", real(st, 0.0f, 20000.0f)));
                for (entity_id n : nats)
                {
                    if (next(st) % 5 == 0)
                        continue; // some nations author nothing
                    nation_budget nb{};
                    nb.reserve_fraction = real(st, -0.2f, 1.2f); // out of range on purpose
                    for (std::size_t i = 0; i < priority_count; ++i)
                        if (next(st) % 3)
                            nb.weights[i] = real(st, 0.0f, 2.0f);
                    s.budgets[n] = nb;
                }
                const int n_claims = static_cast<int>(next(st) % 400);
                for (int i = 0; i < n_claims; ++i)
                {
                    budget_claim c{};
                    c.nation = nats[next(st) % nats.size()];
                    c.corp   = corps[next(st) % corps.size()];
                    c.line   = static_cast<budget_priority>(next(st) % priority_count);
                    c.amount = real(st, -10.0f, 3000.0f); // some non-positive, dropped
                    s.claims.push_back(c);
                }

                // Some generated nations START in debt; the row is about the pass
                // driving a solvent one under, so the before-state is the datum.
                std::map<entity_id, float> before;
                for (const auto& [id, nc] : s.w.nations)
                    before[id] = nc.treasury;

                const auto t = run(s);
                ++scenarios;
                for (const auto& r : t.nations)
                    never_overpaid = never_overpaid && r.paid <= r.spendable;
                for (const auto& [id, nc] : s.w.nations)
                    if (before[id] >= 0.0f)
                        never_negative = never_negative && nc.treasury >= 0.0f;
            }
            check(scenarios == 512 && never_overpaid,
                  "R4g across 512 generated shapes no nation ever pays out more "
                  "than it allotted - the bound is a running total, not a "
                  "remainder that drops the low bits of what it subtracts");
            check(never_negative,
                  "R4h ...and none is driven to a negative treasury, which would "
                  "lock it out of spending on every following tick");
        }

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
