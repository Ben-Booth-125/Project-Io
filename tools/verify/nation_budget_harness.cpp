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
//   R7  THE SPACE PROGRAMME (BL-644). The tenth line's consumer, end to end:
//       derive -> spend -> settle. A nation with treasury, weight and a stocked
//       supplier buys a WHOLE lump at the supplier market's price and the goods
//       are CONSUMED (a terminal sink); a supplier short of a lump is not a
//       supplier; a share below the lump banks and later fires whole; claimed
//       stock is reserved against a second nation; the player's corp is never
//       a supplier; a funded lump the pool can no longer cover is clawed back.
//   R9  NETWORK UPKEEP (BL-643). The logistics_maintenance line's consumer and
//       the Infrastructure demand channel: the bill is GEOGRAPHY (road tiles by
//       level plus active hubs, derived from the world each tick), the claim is
//       UNEARMARKED so rule 3's pro-rata fill applies (half the repair budget
//       buys half the materials — the deliberate contrast with R7's lumps), the
//       goods are drawn from a named supplier's pool and CONSUMED, the player
//       is never a supplier, and both claw-backs (drained pool; rogue transfer)
//       are defended. R9h drives run_nation_step end to end, the R8 precedent.
//   R6  THE TRANSFER RECORD AND THE EARMARK (Sprint N3 T3, 2026-08-23). Every
//       credit moved is recorded on `national_budget_tick::transfers` — who
//       paid whom, on which line, for what — sorted (corp, nation, line), and
//       re-walked in the pass's order the records reproduce `paid` and
//       `total_transferred` BIT-EXACTLY (one accumulation, recorded). A
//       `public_exploration` claim must name the body it would survey, or it
//       is refused whole at gather; and an earmarked claim is paid WHOLE OR
//       NOTHING (rule 3a, Ben's NR-568 ruling) while an unearmarked one keeps
//       rule 3's pro-rata fill.
//
// Every number in the REFERENCE fixture is a dyadic rational chosen so the float
// arithmetic is exact: those assertions are bit equality, deliberately not
// epsilon. That is a property of the fixture and not of the pass, which is why
// `awkward_fixture` exists alongside it — ordinary weights, an awkward reserve
// and prices that are not powers of two, so a row cannot pass by construction of
// its own inputs.
//
// The process exits non-zero if any assertion FAILs.

#include "world/economy_system.hpp"   // economy_report — R8 drives the real step
#include "world/nation_budget.hpp"
#include "world/nation_step.hpp"      // run_nation_step — R8, the wiring itself
#include "world/recipe_registry.hpp"  // set_space_programme — R8's params route
#include "world/network_upkeep.hpp"
#include "world/space_programme.hpp"
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
constexpr std::size_t k_schooling   = static_cast<std::size_t>(budget_priority::schooling);
constexpr std::size_t k_exploration = static_cast<std::size_t>(budget_priority::public_exploration);
constexpr std::size_t k_charters    = static_cast<std::size_t>(budget_priority::charters);

/// Two nations, two corporations. Nation A authors a budget; nation B is the
/// control (present, funded, but authoring nothing).
///
/// The pro-rata rows (R1) run on `schooling`, a line that takes no subject.
/// They ran on `public_exploration` until Sprint N3 made that line EARMARKED
/// (rule 3a: a subject is required and a claim is whole or nothing), which is a
/// different mechanic with its own rows (R6). The arithmetic is unchanged by
/// the move — a share is spendable x weight / total whatever the index — so
/// every reference constant below still holds to the bit.
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

/// A body for an earmarked claim to name. Survey state is irrelevant to the
/// budget pass (the dispatch is T6's); only existence in `w.bodies` is checked.
entity_id add_body(world& w, const char* name)
{
    const entity_id b = w.create_entity();
    body_component bc{};
    bc.name        = name;
    bc.type        = body_type::planet;
    bc.grid_width  = 4;
    bc.grid_height = 4;
    w.bodies[b] = bc;
    return b;
}

/// An earmarked claim: `public_exploration`, subject = `body`.
budget_claim exploration_claim(entity_id nation, entity_id corp, float amount, entity_id body)
{
    budget_claim c{};
    c.nation  = nation;
    c.corp    = corp;
    c.line    = budget_priority::public_exploration;
    c.amount  = amount;
    c.subject = body;
    return c;
}

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
///   schooling   : corp_1 100 + corp_2 150 = 250 demand >  187.5      -> rationed x 0.75
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
    nb.weights[k_schooling]     = 0.25f;
    nb.weights[k_charters]      = 0.25f;
    f.budgets[f.nation_a] = nb;

    f.claims = {
        {f.nation_a, f.corp_1, budget_priority::logistics_maintenance, 200.0f},
        {f.nation_a, f.corp_2, budget_priority::logistics_maintenance, 100.0f},
        {f.nation_a, f.corp_1, budget_priority::schooling,             100.0f},
        {f.nation_a, f.corp_2, budget_priority::schooling,             150.0f},
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
    nb.weights[k_schooling]   = 0.21f;
    nb.weights[k_charters]    = 0.42f;
    f.budgets[f.nation_a] = nb;

    f.claims = {
        {f.nation_a, f.corp_1, budget_priority::logistics_maintenance, 123.457f},
        {f.nation_a, f.corp_2, budget_priority::logistics_maintenance,  77.13f},
        {f.nation_a, f.corp_1, budget_priority::schooling,             311.9f},
        {f.nation_a, f.corp_2, budget_priority::schooling,              88.888f},
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

/// Re-sum `t.transfers` in the PASS'S OWN order — per nation ascending, each
/// nation's credits in (line, corp, arrival) order, the per-nation totals then
/// summed — so the result is the same float accumulation `total_transferred`
/// and each `nation_budget_result::paid` came from. The report is sorted
/// (corp, nation, line) for the payee's benefit, and float addition is not
/// associative, so a flat sum over it would agree only to rounding; this is the
/// walk that makes the identity bit-exact rather than approximate.
struct rewalk
{
    float total            = 0.0f;
    bool  per_nation_exact = true; ///< Every nation's re-sum == its `paid`, bit-exact.
};

rewalk rewalk_transfers(const national_budget_tick& t)
{
    rewalk r;
    for (const nation_budget_result& n : t.nations)
    {
        // Filtering the (corp, nation, line)-sorted list by nation keeps
        // (corp, line, arrival); a stable sort by line then gives
        // (line, corp, arrival) — exactly the pay loop's order.
        std::vector<budget_transfer> mine;
        for (const budget_transfer& x : t.transfers)
            if (x.nation == n.nation)
                mine.push_back(x);
        std::stable_sort(mine.begin(), mine.end(),
                         [](const budget_transfer& a, const budget_transfer& b) {
                             return static_cast<int>(a.line) < static_cast<int>(b.line);
                         });
        float paid = 0.0f;
        for (const budget_transfer& x : mine)
            paid += x.credits;
        r.per_nation_exact = r.per_nation_exact && same(paid, n.paid);
        if (paid != 0.0f)
            r.total += paid;
    }
    return r;
}

/// The report's stated order: (corp, nation, line), non-decreasing.
bool transfers_sorted(const national_budget_tick& t)
{
    for (std::size_t i = 1; i < t.transfers.size(); ++i)
    {
        const budget_transfer& a = t.transfers[i - 1];
        const budget_transfer& b = t.transfers[i];
        if (a.corp != b.corp)     { if (a.corp > b.corp) return false; continue; }
        if (a.nation != b.nation) { if (a.nation > b.nation) return false; continue; }
        if (static_cast<int>(a.line) > static_cast<int>(b.line)) return false;
    }
    return true;
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
              same(a->lines[k_schooling].share, 187.5f) &&
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
              a->lines[k_schooling].rationed &&
              same(a->lines[k_schooling].demand, 250.0f) &&
              same(a->lines[k_schooling].paid,   187.5f) &&
              same(a->lines[k_schooling].fill_fraction, 0.75f),
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
        // type, so 0..255 are all valid VALUES while only 0..9 index the weight
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
            bool record_exact = true, earmarks_whole = true, record_sorted = true;
            int  scenarios = 0, earmarked_paid = 0, earmarked_skipped = 0;
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
                // Two bodies for the earmarked line to name. Added WITHOUT a
                // draw from `st`, so the 512 shapes above (treasuries, weights,
                // claim amounts) are the same ones the Sprint N1 defect was
                // measured on; what changed is that a `public_exploration`
                // claim now needs a subject (rule 3a), assigned below from the
                // claim index rather than the stream, and one in three carries
                // none and is dropped at gather.
                const entity_id body_a = add_body(s.w, "Fuzzworld A");
                const entity_id body_b = add_body(s.w, "Fuzzworld B");
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
                    if (c.line == budget_priority::public_exploration)
                        c.subject = (i % 3 == 0) ? body_a : (i % 3 == 1) ? body_b : null_entity;
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
                {
                    never_overpaid = never_overpaid && r.paid <= r.spendable;
                    earmarked_skipped += r.lines[k_exploration].skipped;
                }
                for (const auto& [id, nc] : s.w.nations)
                    if (before[id] >= 0.0f)
                        never_negative = never_negative && nc.treasury >= 0.0f;

                // The transfer record, under every shape: re-walked in the
                // pass's order it IS the total, bit for bit; every earmarked
                // entry is whole; the list is in its stated order.
                const rewalk rw = rewalk_transfers(t);
                record_exact  = record_exact && rw.per_nation_exact &&
                                same(rw.total, t.total_transferred);
                record_sorted = record_sorted && transfers_sorted(t);
                for (const budget_transfer& x : t.transfers)
                {
                    if (x.subject == null_entity)
                        continue;
                    ++earmarked_paid;
                    earmarks_whole = earmarks_whole && same(x.fill_fraction, 1.0f) &&
                                     x.line == budget_priority::public_exploration;
                }
            }
            check(scenarios == 512 && never_overpaid,
                  "R4g across 512 generated shapes no nation ever pays out more "
                  "than it allotted - the bound is a running total, not a "
                  "remainder that drops the low bits of what it subtracts");
            check(never_negative,
                  "R4h ...and none is driven to a negative treasury, which would "
                  "lock it out of spending on every following tick");
            check(record_exact,
                  "R6h across the same 512 shapes the transfer record, re-walked "
                  "in the pass's order, reproduces every nation's `paid` and the "
                  "tick's total_transferred BIT-EXACTLY - one accumulation, "
                  "recorded, not a second one that agrees approximately");
            check(earmarked_paid > 0 && earmarked_skipped > 0 && earmarks_whole,
                  "R6i ...and every earmarked transfer in them is whole (fill "
                  "1.0) while some earmarked claims were skipped - rule 3a "
                  "exercised both ways, not vacuously");
            check(record_sorted,
                  "R6j ...and every record is in its stated (corp, nation, line) "
                  "order");
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

    // --- R6: the transfer record, and the earmark (Sprint N3 T3) ------------
    std::printf("\n-- R6  who paid whom, for what - and whole or nothing --\n");
    {
        // The record on the reference fixture: four claims paid, four records,
        // in (corp, nation, line) order, each carrying ITS OWN fill and the
        // line's rationed flag. Sums re-walked in the pass's order are the
        // pass's own floats.
        fixture     f = make_fixture();
        const auto  t = run(f);
        const auto& x = t.transfers;
        check(x.size() == 4 && transfers_sorted(t) &&
              x[0].corp == f.corp_1 && x[0].line == budget_priority::logistics_maintenance &&
              x[1].corp == f.corp_1 && x[1].line == budget_priority::schooling &&
              x[2].corp == f.corp_2 && x[2].line == budget_priority::logistics_maintenance &&
              x[3].corp == f.corp_2 && x[3].line == budget_priority::schooling,
              "R6a one record per claim paid, sorted (corp, nation, line) - the "
              "payee's view of who is paying it");
        check(x.size() == 4 &&
              same(x[0].credits, 200.0f) && same(x[0].fill_fraction, 1.0f)  && !x[0].rationed &&
              same(x[1].credits,  75.0f) && same(x[1].fill_fraction, 0.75f) &&  x[1].rationed &&
              same(x[2].credits, 100.0f) && same(x[2].fill_fraction, 1.0f)  && !x[2].rationed &&
              same(x[3].credits, 112.5f) && same(x[3].fill_fraction, 0.75f) &&  x[3].rationed &&
              x[0].nation == f.nation_a && x[0].subject == null_entity,
              "R6b each record carries the credits that moved, THIS claim's "
              "fill (1.0 / 0.75), the line's rationed flag, and no earmark on "
              "a line that takes none");

        const rewalk rw = rewalk_transfers(t);
        check(rw.per_nation_exact && same(rw.total, t.total_transferred) &&
              same(rw.total, k_paid_per_tick),
              "R6c re-walked in the pass's order the records sum to "
              "total_transferred bit-exactly (487.5)");

        // ...and at ordinary weights, where the identity is the point.
        fixture    g  = make_awkward_fixture();
        const auto gt = run(g);
        const rewalk gw = rewalk_transfers(gt);
        check(gt.transfers.size() == 6 && gw.per_nation_exact &&
              same(gw.total, gt.total_transferred),
              "R6d ...and at non-dyadic weights too: six records, re-walked, "
              "ARE the total to the bit - the same accumulation, recorded");

        // THE EARMARK. A `public_exploration` claim names the body it would
        // survey; without one there is nothing for the credit to dispatch, so
        // the claim is refused whole at gather and the tick is bit-identical to
        // the one without it. A body the world does not hold is the same.
        {
            fixture    h  = make_fixture();
            fixture    k  = make_fixture();
            h.budgets[h.nation_a].weights[k_exploration] = 0.25f;
            k.budgets[k.nation_a].weights[k_exploration] = 0.25f;
            budget_claim bare{};
            bare.nation = k.nation_a;
            bare.corp   = k.corp_1;
            bare.line   = budget_priority::public_exploration;
            bare.amount = 50.0f; // subject left null
            k.claims.push_back(bare);
            k.claims.push_back(exploration_claim(k.nation_a, k.corp_2, 60.0f, 9999u)); // no such body
            const auto ht = run(h);
            const auto kt = run(k);
            const auto* kr = result_for(kt, k.nation_a);
            check(snapshot(h.w) == snapshot(k.w) &&
                  same(ht.total_transferred, kt.total_transferred) &&
                  ht.transfers.size() == kt.transfers.size() &&
                  kr != nullptr && same(kr->lines[k_exploration].demand, 0.0f) &&
                  same(kr->lines[k_exploration].paid, 0.0f),
                  "R6e a public_exploration claim with no subject, or a subject "
                  "the world does not hold, is rejected whole at gather - no "
                  "demand, no debit, no record; the tick is bit-identical");
        }

        // RULE 3a, both ways. Treasury 1000, reserve 1/4 -> spendable 750;
        // exploration and schooling weighted 1/4 each (shares 187.5), logistics
        // 1/2 with no claim. The SAME two amounts, 100 and 150, on each line:
        //   exploration (earmarked)  : corp_1 100 <= 187.5 -> paid WHOLE;
        //                              corp_2 150 >  87.5  -> SKIPPED, not 87.5
        //   schooling   (unearmarked): rationed x 0.75 -> 75 and 112.5, as R1j
        // So the treasury pays 100 + 187.5 = 287.5 and the 87.5 the skipped
        // claim would have taken is still in it.
        {
            fixture m;
            m.nation_a = add_nation(m.w, "Earmarker", 1000.0f);
            m.corp_1   = add_corp(m.w, "Payee One", 0.0f);
            m.corp_2   = add_corp(m.w, "Payee Two", 0.0f);
            const entity_id body_a = add_body(m.w, "Near");
            const entity_id body_b = add_body(m.w, "Far");
            nation_budget nb{};
            nb.reserve_fraction       = 0.25f;
            nb.weights[k_logistics]   = 0.5f;
            nb.weights[k_schooling]   = 0.25f;
            nb.weights[k_exploration] = 0.25f;
            m.budgets[m.nation_a] = nb;
            m.claims = {
                exploration_claim(m.nation_a, m.corp_1, 100.0f, body_a),
                exploration_claim(m.nation_a, m.corp_2, 150.0f, body_b),
                {m.nation_a, m.corp_1, budget_priority::schooling, 100.0f},
                {m.nation_a, m.corp_2, budget_priority::schooling, 150.0f},
            };
            const auto  mt = run(m);
            const auto* mr = result_for(mt, m.nation_a);
            const budget_line_result& ex = mr->lines[k_exploration];
            const budget_line_result& sc = mr->lines[k_schooling];

            check(mr != nullptr &&
                  same(ex.share, 187.5f) && same(ex.demand, 250.0f) &&
                  same(ex.paid, 100.0f) && ex.skipped == 1 && ex.rationed &&
                  same(ex.fill_fraction, 0.4f),
                  "R6f (3a) an earmarked claim above the line's remaining share "
                  "is SKIPPED WHOLE - 100 paid in full, 150 paid nothing rather "
                  "than 87.5; the line says so (skipped 1, rationed, fill 0.4)");
            check(same(m.w.corporations.at(m.corp_2).balance, 112.5f) &&
                  same(m.w.corporations.at(m.corp_1).balance, 175.0f) &&
                  same(m.w.nations.at(m.nation_a).treasury, 1000.0f - 287.5f) &&
                  same(mr->paid, 287.5f),
                  "R6g ...the treasury is untouched by the skipped claim (pays "
                  "287.5, keeps the 87.5), and the skipped corp receives nothing "
                  "on that line - no fraction of a survey");
            check(sc.rationed && sc.skipped == 0 &&
                  same(sc.paid, 187.5f) && same(sc.fill_fraction, 0.75f),
                  "R6k ...while the same two amounts on an UNEARMARKED line are "
                  "still partially filled pro rata (75 + 112.5) - rule 3 is "
                  "unchanged where no earmark is in play");

            // The one earmarked record: whole, on the body it named, flagged
            // with the line's rationing; the skipped claim has no record.
            int ear = 0; bool ear_ok = true;
            for (const budget_transfer& r : mt.transfers)
                if (r.line == budget_priority::public_exploration)
                {
                    ++ear;
                    ear_ok = ear_ok && r.corp == m.corp_1 && r.subject == body_a &&
                             same(r.credits, 100.0f) && same(r.fill_fraction, 1.0f) &&
                             r.rationed;
                }
            check(ear == 1 && ear_ok && mt.transfers.size() == 3,
                  "R6l the earmarked record names the body, is whole (fill 1.0), "
                  "carries the line's rationed flag, and the skipped claim left "
                  "no record at all");

            // Determinism of the record: the reversed claim vector yields the
            // identical list, field for field. A fresh world built in the same
            // order, so the ids agree.
            fixture p;
            p.nation_a = add_nation(p.w, "Earmarker", 1000.0f);
            p.corp_1   = add_corp(p.w, "Payee One", 0.0f);
            p.corp_2   = add_corp(p.w, "Payee Two", 0.0f);
            const entity_id pa = add_body(p.w, "Near");
            const entity_id pb = add_body(p.w, "Far");
            p.budgets = m.budgets;
            p.claims = {
                {p.nation_a, p.corp_2, budget_priority::schooling, 150.0f},
                {p.nation_a, p.corp_1, budget_priority::schooling, 100.0f},
                exploration_claim(p.nation_a, p.corp_2, 150.0f, pb),
                exploration_claim(p.nation_a, p.corp_1, 100.0f, pa),
            };
            const auto pt = run(p);
            bool identical = pt.transfers.size() == mt.transfers.size();
            for (std::size_t i = 0; identical && i < pt.transfers.size(); ++i)
            {
                const budget_transfer& a = mt.transfers[i];
                const budget_transfer& b = pt.transfers[i];
                identical = a.corp == b.corp && a.nation == b.nation && a.line == b.line &&
                            a.subject == b.subject && same(a.credits, b.credits) &&
                            same(a.fill_fraction, b.fill_fraction) && a.rationed == b.rationed;
            }
            check(identical,
                  "R6m the record is the same list, field for field, when the "
                  "claims arrive in the opposite order");
        }
    }

    // --- R7: the space programme (BL-644) -----------------------------------
    // The tenth line's consumer: derive -> spend -> settle. Every number below
    // is dyadic (prices 2.0 / 0.5, lumps 8 / 16, treasury 1024, reserve 1/4,
    // weights quarters and halves), so the assertions are bit equality.
    std::printf("\n-- R7  the space programme buys, consumes, and skips whole --\n");
    {
        constexpr std::size_t k_comp = static_cast<std::size_t>(resource_type::spacecraft_components);
        constexpr std::size_t k_prop = static_cast<std::size_t>(resource_type::propellant);
        constexpr std::size_t k_space = static_cast<std::size_t>(budget_priority::space_programme);

        // A market with authored prices, so the derivation has its procurement
        // price basis (resolved price; base_price is the fallback, exercised
        // by leaving one good's `price` at zero in R7g's twin below).
        auto add_market = [](world& w, entity_id body) {
            const entity_id m = w.create_entity();
            market_component mc{};
            mc.body = body;
            w.markets[m] = mc;
            return m;
        };

        // The reference space fixture. Treasury 1024, reserve 1/4 -> spendable
        // 768; weights space 1/4, logistics 1/2, schooling 1/4 -> space share
        // 192. Supplier pool on body_a: 100 components, 100 propellant; prices
        // 2.0 and 0.5; lumps 8 and 16 -> claim amounts 16.0 and 8.0.
        struct space_fixture
        {
            fixture   f;      // reuses the base fixture's world/corp helpers
            entity_id body_a  = null_entity;
            entity_id market  = null_entity;
            space_programme_params params;
        };
        auto make_space_fixture = [&](float components_stock, float propellant_stock) {
            space_fixture s;
            s.f.nation_a = add_nation(s.f.w, "Spacefaria", 1024.0f);
            s.f.corp_1   = add_corp(s.f.w, "Supplier",  0.0f);
            s.f.corp_2   = add_corp(s.f.w, "Bystander", 0.0f);
            s.body_a     = add_body(s.f.w, "Padworld");
            s.market     = add_market(s.f.w, s.body_a);
            s.f.w.markets.at(s.market).price[k_comp] = 2.0f;
            s.f.w.markets.at(s.market).price[k_prop] = 0.5f;

            nation_budget nb{};
            nb.reserve_fraction     = 0.25f;
            nb.weights[k_space]     = 0.25f;
            nb.weights[k_logistics] = 0.5f;
            nb.weights[k_schooling] = 0.25f;
            s.f.budgets[s.f.nation_a] = nb;

            auto& pool = s.f.w.corp_body_pools[std::make_pair(s.f.corp_1, s.body_a)];
            pool.quantities[k_comp] = components_stock;
            pool.quantities[k_prop] = propellant_stock;

            s.params.components_lump = 8.0f;
            s.params.propellant_lump = 16.0f;
            return s;
        };
        auto run_space = [](space_fixture& s) {
            std::vector<space_purchase> intents = derive_space_programme_claims(
                s.f.w, s.f.budgets, s.params, s.f.claims);
            national_budget_tick t;
            run_national_budget(s.f.w, s.f.budgets, s.f.claims, &t);
            settle_space_purchases(s.f.w, intents, t);
            return std::make_pair(std::move(intents), std::move(t));
        };

        // R7a: the whole loop. Both lumps derived, funded, consumed.
        {
            space_fixture s = make_space_fixture(100.0f, 100.0f);
            const double credit_before = world_credit_exact(s.f.w);
            auto [intents, t] = run_space(s);
            const auto& pool = s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a));

            check(intents.size() == 2 &&
                  intents[0].resource == resource_type::spacecraft_components &&
                  same(intents[0].quantity, 8.0f) && same(intents[0].credits, 16.0f) &&
                  intents[0].supplier == s.f.corp_1 && intents[0].body == s.body_a &&
                  intents[1].resource == resource_type::propellant &&
                  same(intents[1].quantity, 16.0f) && same(intents[1].credits, 8.0f) &&
                  intents[0].funded && intents[0].completed &&
                  intents[1].funded && intents[1].completed,
                  "R7a a nation with treasury, weight and a stocked supplier "
                  "derives both lumps at the supplier market's price and both "
                  "are funded and completed");
            check(same(pool.quantities[k_comp], 92.0f) &&
                  same(pool.quantities[k_prop], 84.0f),
                  "R7b ...and the goods are CONSUMED: exactly one lump of each "
                  "leaves the pool and lands nowhere - the satellite launched");
            check(same(s.f.w.corporations.at(s.f.corp_1).balance, 24.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 1000.0f) &&
                  same(t.total_transferred, 24.0f) &&
                  world_credit_exact(s.f.w) == credit_before,
                  "R7c ...conservation-exact: the supplier is credited the 24.0 "
                  "the treasury paid (1024 -> 1000), and total world credit is "
                  "unchanged to the double-summed bit");
        }

        // R7d: no whole lump in any pool -> no claim at all. The state does
        // not split a launch across suppliers or buy a partial lot.
        {
            space_fixture s = make_space_fixture(7.5f, 0.0f); // both short
            const auto before = snapshot(s.f.w);
            auto [intents, t] = run_space(s);
            check(intents.empty() && s.f.claims.empty() && t.transfers.empty() &&
                  snapshot(s.f.w) == before &&
                  same(s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a))
                           .quantities[k_comp], 7.5f),
                  "R7d a supplier short of a whole lump is not a supplier: no "
                  "claim, no transfer, no draw - the tick is bit-identical");
        }

        // R7e: the LUMP property. A share below the lump's cost buys nothing
        // and the treasury banks it; grown past the cost, the lump fires
        // WHOLE. Components only (propellant lump zeroed): share = treasury x
        // 3/4 x 1/4 = treasury x 3/16; cost 16.0 -> fires at treasury > 85.33.
        {
            space_fixture s = make_space_fixture(100.0f, 100.0f);
            s.params.propellant_lump = 0.0f;
            s.f.w.nations.at(s.f.nation_a).treasury = 64.0f; // share 12 < 16
            {
                auto [intents, t] = run_space(s);
                check(intents.empty() && t.transfers.empty() &&
                      same(s.f.w.nations.at(s.f.nation_a).treasury, 64.0f),
                      "R7e a share below the lump buys NOTHING - no partial "
                      "purchase, the treasury banks the share (state demand "
                      "arrives in lumps)");
            }
            s.f.claims.clear();
            s.f.w.nations.at(s.f.nation_a).treasury = 128.0f; // share 24 >= 16
            {
                auto [intents, t] = run_space(s);
                check(intents.size() == 1 && intents[0].funded && intents[0].completed &&
                      same(intents[0].credits, 16.0f) &&
                      same(s.f.w.nations.at(s.f.nation_a).treasury, 112.0f) &&
                      same(s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a))
                               .quantities[k_comp], 92.0f),
                      "R7f ...and once the accumulated share covers it the lump "
                      "fires whole - 16.0 paid, 8 units consumed, nothing "
                      "in between");
            }
        }

        // R7g: two nations, one pool - the reservation. Stock covers ONE
        // components lump; the lower-id nation claims it and the second
        // derives nothing, rather than both being paid for the same units.
        {
            space_fixture s = make_space_fixture(10.0f, 0.0f);
            s.params.propellant_lump = 0.0f;
            const entity_id nation_b = add_nation(s.f.w, "Latecomia", 1024.0f);
            s.f.budgets[nation_b] = s.f.budgets.at(s.f.nation_a);
            auto [intents, t] = run_space(s);
            (void)t;
            check(intents.size() == 1 && intents[0].nation == s.f.nation_a &&
                  intents[0].funded && intents[0].completed &&
                  same(s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a))
                           .quantities[k_comp], 2.0f) &&
                  same(s.f.w.nations.at(nation_b).treasury, 1024.0f),
                  "R7g stock a claim names is RESERVED: with one lump on hand "
                  "two nations do not both buy it - the ascending-id walk "
                  "takes it and the second treasury is untouched");
        }

        // R7h: the player's corp is never a supplier - a state purchase drains
        // the pool unasked, and on the player's corp that is a forced sale the
        // standing rules do not sanction. The fatter player pool loses to the
        // thinner rival pool; with ONLY the player stocked, nothing is bought.
        {
            space_fixture s = make_space_fixture(8.0f, 0.0f); // corp_1: exactly one lump
            s.params.propellant_lump = 0.0f;
            s.f.w.player_entity = s.f.corp_2;
            s.f.w.corp_body_pools[std::make_pair(s.f.corp_2, s.body_a)]
                .quantities[k_comp] = 1000.0f; // fatter, and ineligible
            auto [intents, t] = run_space(s);
            (void)t;
            const bool rival_chosen = intents.size() == 1 && intents[0].supplier == s.f.corp_1;

            space_fixture q = make_space_fixture(0.0f, 0.0f);
            q.params.propellant_lump = 0.0f;
            q.f.w.player_entity = q.f.corp_1;
            q.f.w.corp_body_pools.at(std::make_pair(q.f.corp_1, q.body_a))
                .quantities[k_comp] = 1000.0f; // only the player holds stock
            auto [q_intents, qt] = run_space(q);
            check(rival_chosen && q_intents.empty() && qt.transfers.empty() &&
                  same(q.f.w.corp_body_pools.at(std::make_pair(q.f.corp_1, q.body_a))
                           .quantities[k_comp], 1000.0f),
                  "R7h the player's corp is never a supplier: a fatter player "
                  "pool loses to a rival's, and a world where only the player "
                  "holds stock sees no state purchase at all");
        }

        // R7i: the claw-back defence. Drain the pool BETWEEN derive and spend
        // (an out-of-band draw the reservation cannot see) - the transfer is
        // reversed in the same two floats, so the nation did not pay for a
        // launch that never happened and world credit still balances.
        {
            space_fixture s = make_space_fixture(100.0f, 0.0f);
            s.params.propellant_lump = 0.0f;
            const double credit_before = world_credit_exact(s.f.w);
            std::vector<space_purchase> intents = derive_space_programme_claims(
                s.f.w, s.f.budgets, s.params, s.f.claims);
            s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a))
                .quantities[k_comp] = 0.0f; // the out-of-band draw
            national_budget_tick t;
            run_national_budget(s.f.w, s.f.budgets, s.f.claims, &t);
            settle_space_purchases(s.f.w, intents, t);
            check(intents.size() == 1 && intents[0].funded && !intents[0].completed &&
                  same(s.f.w.corporations.at(s.f.corp_1).balance, 0.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 1024.0f) &&
                  world_credit_exact(s.f.w) == credit_before,
                  "R7i a funded lump the pool can no longer cover is CLAWED "
                  "BACK whole - balance and treasury both restored, funded but "
                  "not completed, credit conserved");
        }

        // R7j: determinism - two identical space fixtures, identical floats
        // and identical intent lists, over the full derive/spend/settle loop.
        {
            space_fixture a = make_space_fixture(100.0f, 100.0f);
            space_fixture b = make_space_fixture(100.0f, 100.0f);
            auto [ia, ta] = run_space(a);
            auto [ib, tb] = run_space(b);
            (void)ta; (void)tb;
            bool identical = snapshot(a.f.w) == snapshot(b.f.w) && ia.size() == ib.size();
            for (std::size_t i = 0; identical && i < ia.size(); ++i)
                identical = ia[i].nation == ib[i].nation && ia[i].supplier == ib[i].supplier &&
                            ia[i].body == ib[i].body && ia[i].resource == ib[i].resource &&
                            same(ia[i].quantity, ib[i].quantity) &&
                            same(ia[i].credits, ib[i].credits) &&
                            ia[i].funded == ib[i].funded && ia[i].completed == ib[i].completed;
            check(identical,
                  "R7j two runs of one space fixture are bit-identical, intents "
                  "included");
        }

        // R7k: the pricing twin the add_market comment promised (cold-review
        // finding 2). Half one: an unresolved price (0) falls back to
        // base_price — request_quote's own reading. Half two: a body with NO
        // market refuses the purchase outright — a zero-credit draw would be
        // confiscation wearing a purchase's name.
        {
            space_fixture s = make_space_fixture(100.0f, 0.0f);
            s.params.propellant_lump = 0.0f;
            s.f.w.markets.at(s.market).price[k_comp]      = 0.0f;
            s.f.w.markets.at(s.market).base_price[k_comp] = 2.0f;
            auto [intents, t] = run_space(s);
            (void)t;
            const bool fallback_ok =
                intents.size() == 1 && intents[0].funded && intents[0].completed &&
                same(intents[0].credits, 16.0f);

            space_fixture q = make_space_fixture(100.0f, 100.0f);
            q.f.w.markets.erase(q.market); // no market on the body at all
            const auto before = snapshot(q.f.w);
            auto [q_intents, qt] = run_space(q);
            check(fallback_ok && q_intents.empty() && qt.transfers.empty() &&
                  snapshot(q.f.w) == before,
                  "R7k the price basis: an unresolved price falls back to "
                  "base_price (8 x 2.0 = 16.0 paid), and a body with no market "
                  "sees no purchase at all");
        }

        // R7l: a PAID space transfer with no intent behind it. In-process the
        // derivation is the line's only claimant, but the claim vector is an
        // AI-facing seam (wire-reachable over --serve), so settle claws back
        // any space transfer it cannot match — otherwise a rogue claim leaves
        // credits on a corp with no goods drawn and no ledger row (cold-review
        // finding 4).
        {
            space_fixture s = make_space_fixture(100.0f, 100.0f);
            const double credit_before = world_credit_exact(s.f.w);
            std::vector<space_purchase> intents = derive_space_programme_claims(
                s.f.w, s.f.budgets, s.params, s.f.claims);
            budget_claim rogue;
            rogue.nation  = s.f.nation_a;
            rogue.corp    = s.f.corp_2; // holds no stock, made no intent
            rogue.line    = budget_priority::space_programme;
            rogue.amount  = 4.0f;       // within the line's remaining share
            rogue.subject = s.body_a;
            s.f.claims.push_back(rogue);
            national_budget_tick t;
            run_national_budget(s.f.w, s.f.budgets, s.f.claims, &t);
            settle_space_purchases(s.f.w, intents, t);
            check(same(s.f.w.corporations.at(s.f.corp_2).balance, 0.0f) &&
                  same(s.f.w.corporations.at(s.f.corp_1).balance, 24.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 1000.0f) &&
                  intents.size() == 2 && intents[0].completed && intents[1].completed &&
                  world_credit_exact(s.f.w) == credit_before,
                  "R7l a paid space transfer with NO intent behind it is clawed "
                  "back whole: the rogue claimant keeps nothing, the legitimate "
                  "purchases stand, world credit conserved");
        }

        // R8: the REAL wiring (cold-review finding 1). R7a-R7l call the three
        // functions directly; this row drives run_nation_step itself, so
        // deleting or reordering the 2b/4b calls in nation_step.cpp goes red
        // here and nowhere else. econ_tick 1: the single nation (index 0) is
        // not due for re-scoring on any cadence >= 2, so the authored budget
        // survives step 1 and the derivation reads it.
        {
            space_fixture s = make_space_fixture(100.0f, 100.0f);
            s.f.w.nation_budgets[s.f.nation_a] = s.f.budgets.at(s.f.nation_a);
            recipe_registry reg;
            reg.set_space_programme(s.params);
            const double credit_before = world_credit_exact(s.f.w);
            economy_report rep;
            run_nation_step(s.f.w, reg, rep, /*econ_tick=*/1);
            const auto& pool =
                s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a));
            check(rep.space_purchases.size() == 2 &&
                  rep.space_purchases[0].completed && rep.space_purchases[1].completed &&
                  same(pool.quantities[k_comp], 92.0f) &&
                  same(pool.quantities[k_prop], 84.0f) &&
                  same(s.f.w.corporations.at(s.f.corp_1).balance, 24.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 1000.0f) &&
                  same(rep.budgets[s.f.corp_1].subsidies, 24.0f) &&
                  world_credit_exact(s.f.w) == credit_before,
                  "R8 run_nation_step end to end: derive -> spend -> settle -> "
                  "fold - pools drained, supplier credited, subsidies explain "
                  "the delta, report.space_purchases carries both completed rows");
        }
    }

    // --- R9: network upkeep (BL-643) ----------------------------------------
    // The logistics_maintenance line's consumer: derive -> spend -> settle.
    // Every number is dyadic (rates 1/2/4 and 0.5/1/2, hub 8/4, prices 2.0 and
    // 4.0, treasury 1024 or 128, reserve 1/4 or 1/2), so assertions are bit
    // equality. Network: 4 Track + 1 Highway + 1 active port -> stone bill
    // 4x1 + 1x4 + 8 = 16, timber bill 4x0.5 + 1x2 + 4 = 8; at prices 2.0/4.0
    // both claims are 32.0 credits.
    std::printf("\n-- R9  network upkeep bills geography, fills pro rata, consumes --\n");
    {
        constexpr std::size_t k_stone  = static_cast<std::size_t>(resource_type::stone);
        constexpr std::size_t k_timber = static_cast<std::size_t>(resource_type::timber);

        auto add_market = [](world& w, entity_id body) {
            const entity_id m = w.create_entity();
            market_component mc{};
            mc.body = body;
            w.markets[m] = mc;
            return m;
        };
        auto add_tile = [](world& w, entity_id body, int x, int y, std::uint8_t road,
                           entity_id nation /* null_entity = unowned */) {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body       = body;
            tc.grid_x     = x;
            tc.grid_y     = y;
            tc.road_level = road;
            w.tiles[t] = tc;
            if (nation != null_entity)
                w.tile_to_nation[t] = nation;
            return t;
        };
        auto add_hub = [](world& w, entity_id tile, building_type type,
                          int ticks_remaining, bool decommissioned) {
            const entity_id b = w.create_entity();
            building_component bc{};
            bc.tile            = tile;
            bc.type            = type;
            bc.ticks_remaining = ticks_remaining;
            bc.decommissioned  = decommissioned;
            w.buildings[b] = bc;
            return b;
        };

        struct net_fixture
        {
            fixture   f;
            entity_id body_a = null_entity;
            entity_id market = null_entity;
            network_upkeep_params params;
        };
        // The reference network fixture. Weights logistics 1/2, schooling 1/4,
        // exploration 1/4; treasury 1024, reserve 1/4 -> spendable 768,
        // logistics share 384 >= the 64.0 bill -> paid in full.
        auto make_net_fixture = [&](float stone_stock, float timber_stock) {
            net_fixture s;
            s.f.nation_a = add_nation(s.f.w, "Roadsteadia", 1024.0f);
            s.f.corp_1   = add_corp(s.f.w, "Quarryco",  0.0f);
            s.f.corp_2   = add_corp(s.f.w, "Bystander", 0.0f);
            s.body_a     = add_body(s.f.w, "Gridworld");
            s.market     = add_market(s.f.w, s.body_a);
            s.f.w.markets.at(s.market).price[k_stone]  = 2.0f;
            s.f.w.markets.at(s.market).price[k_timber] = 4.0f;

            // The network: 4 Track + 1 Highway owned by the nation, one ACTIVE
            // port on owned ground. Three non-members prove the gates: a roaded
            // tile on UNCLAIMED ground, an under-construction hub, and a
            // decommissioned hub — none of them may move the bill.
            for (int x = 0; x < 4; ++x)
                add_tile(s.f.w, s.body_a, x, 0, /*road=*/1, s.f.nation_a);
            const entity_id hw = add_tile(s.f.w, s.body_a, 0, 1, /*road=*/3, s.f.nation_a);
            (void)hw;
            const entity_id port_tile = add_tile(s.f.w, s.body_a, 1, 1, /*road=*/0, s.f.nation_a);
            add_hub(s.f.w, port_tile, building_type::port, 0, false);
            add_tile(s.f.w, s.body_a, 2, 1, /*road=*/2, null_entity); // unowned road
            const entity_id t_build = add_tile(s.f.w, s.body_a, 3, 1, 0, s.f.nation_a);
            add_hub(s.f.w, t_build, building_type::inland_logistics_hub, 5, false); // building
            const entity_id t_dec = add_tile(s.f.w, s.body_a, 0, 2, 0, s.f.nation_a);
            add_hub(s.f.w, t_dec, building_type::port, 0, true); // decommissioned

            nation_budget nb{};
            nb.reserve_fraction       = 0.25f;
            nb.weights[k_logistics]   = 0.5f;
            nb.weights[k_schooling]   = 0.25f;
            nb.weights[k_exploration] = 0.25f;
            s.f.budgets[s.f.nation_a] = nb;

            auto& pool = s.f.w.corp_body_pools[std::make_pair(s.f.corp_1, s.body_a)];
            pool.quantities[k_stone]  = stone_stock;
            pool.quantities[k_timber] = timber_stock;

            s.params.stone_per_level  = {1.0f, 2.0f, 4.0f};
            s.params.timber_per_level = {0.5f, 1.0f, 2.0f};
            s.params.stone_per_hub    = 8.0f;
            s.params.timber_per_hub   = 4.0f;
            return s;
        };
        auto run_net = [](net_fixture& s) {
            std::vector<network_purchase> intents = derive_network_upkeep_claims(
                s.f.w, s.f.budgets, s.params, s.f.claims);
            national_budget_tick t;
            run_national_budget(s.f.w, s.f.budgets, s.f.claims, &t);
            settle_network_purchases(s.f.w, intents, t);
            return std::make_pair(std::move(intents), std::move(t));
        };

        // R9a: the whole loop, fully funded. The gates on what counts as
        // network are asserted through the BILL ITSELF: 16 stone / 8 timber is
        // only right if the unowned road, the building hub and the
        // decommissioned hub all counted for nothing.
        {
            net_fixture s = make_net_fixture(100.0f, 100.0f);
            const double credit_before = world_credit_exact(s.f.w);
            auto [intents, t] = run_net(s);
            const auto& pool = s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a));

            check(intents.size() == 2 &&
                  intents[0].resource == resource_type::stone &&
                  same(intents[0].quantity, 16.0f) && same(intents[0].credits, 32.0f) &&
                  intents[0].supplier == s.f.corp_1 && intents[0].body == s.body_a &&
                  intents[1].resource == resource_type::timber &&
                  same(intents[1].quantity, 8.0f) && same(intents[1].credits, 32.0f) &&
                  intents[0].funded && intents[0].completed &&
                  intents[1].funded && intents[1].completed,
                  "R9a the network bills its geography - road tiles by level "
                  "plus the one ACTIVE hub (unowned road, building hub and "
                  "decommissioned hub all count nothing) - and both material "
                  "claims fund whole");
            check(same(pool.quantities[k_stone], 84.0f) &&
                  same(pool.quantities[k_timber], 92.0f) &&
                  same(intents[0].drawn, 16.0f) && same(intents[1].drawn, 8.0f),
                  "R9b ...and the goods are CONSUMED: the full bill leaves the "
                  "pool and lands nowhere - the repairs went into the roadbed");
            check(same(s.f.w.corporations.at(s.f.corp_1).balance, 64.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 960.0f) &&
                  same(t.total_transferred, 64.0f) &&
                  same(intents[0].paid, 32.0f) && same(intents[1].paid, 32.0f) &&
                  world_credit_exact(s.f.w) == credit_before,
                  "R9c ...conservation-exact: the supplier is credited the 64.0 "
                  "the treasury paid (1024 -> 960), world credit unchanged to "
                  "the double-summed bit");
        }

        // R9d: the PRO-RATA property - the deliberate contrast with R7e's
        // lump. Share 32 against a 64.0 bill fills every claim at exactly 1/2:
        // half the repair budget buys half the materials, nothing banks.
        {
            net_fixture s = make_net_fixture(100.0f, 100.0f);
            s.f.w.nations.at(s.f.nation_a).treasury = 128.0f;
            auto& nb = s.f.budgets.at(s.f.nation_a);
            nb.reserve_fraction       = 0.5f;
            nb.weights[k_logistics]   = 0.5f;
            nb.weights[k_schooling]   = 0.5f;
            nb.weights[k_exploration] = 0.0f;
            auto [intents, t] = run_net(s);
            const auto& pool = s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a));

            bool fills_ok = true;
            int  n_log = 0;
            for (const budget_transfer& tr : t.transfers)
                if (tr.line == budget_priority::logistics_maintenance)
                {
                    ++n_log;
                    fills_ok = fills_ok && same(tr.fill_fraction, 0.5f) && tr.rationed;
                }
            check(intents.size() == 2 && n_log == 2 && fills_ok &&
                  intents[0].funded && intents[0].completed &&
                  intents[1].funded && intents[1].completed &&
                  same(intents[0].paid, 16.0f) && same(intents[0].drawn, 8.0f) &&
                  same(intents[1].paid, 16.0f) && same(intents[1].drawn, 4.0f) &&
                  same(pool.quantities[k_stone], 92.0f) &&
                  same(pool.quantities[k_timber], 96.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 96.0f) &&
                  same(s.f.w.corporations.at(s.f.corp_1).balance, 32.0f),
                  "R9d a share of 32 against a 64.0 bill fills PRO RATA at 1/2 "
                  "- half the credits move, half the materials draw (8 stone, "
                  "4 timber), no lump banks and no claim is skipped");
        }

        // R9e: a pool short of the bill CAPS the claim rather than refusing it
        // - the other half of the continuous shape (R7d's twin, inverted:
        // where the state splits no launch, it happily buys a partial repair).
        {
            net_fixture s = make_net_fixture(6.0f, 0.0f);
            auto [intents, t] = run_net(s);
            (void)t;
            check(intents.size() == 1 &&
                  intents[0].resource == resource_type::stone &&
                  same(intents[0].quantity, 6.0f) && same(intents[0].credits, 12.0f) &&
                  intents[0].funded && intents[0].completed &&
                  same(intents[0].drawn, 6.0f) &&
                  same(s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a))
                           .quantities[k_stone], 0.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 1012.0f),
                  "R9e a supplier short of the bill still supplies: the 16-unit "
                  "stone bill caps to the 6 on hand (12.0 paid), and an empty "
                  "timber pool derives no claim at all");
        }

        // R9f: the player's corp is never a supplier - R7h's own two halves.
        {
            net_fixture s = make_net_fixture(4.0f, 0.0f); // corp_1: thin but eligible
            s.f.w.player_entity = s.f.corp_2;
            s.f.w.corp_body_pools[std::make_pair(s.f.corp_2, s.body_a)]
                .quantities[k_stone] = 1000.0f; // fatter, and ineligible
            auto [intents, t] = run_net(s);
            (void)t;
            const bool rival_chosen = intents.size() == 1 && intents[0].supplier == s.f.corp_1;

            net_fixture q = make_net_fixture(0.0f, 0.0f);
            q.f.w.player_entity = q.f.corp_1;
            q.f.w.corp_body_pools.at(std::make_pair(q.f.corp_1, q.body_a))
                .quantities[k_stone] = 1000.0f; // only the player holds stock
            auto [q_intents, qt] = run_net(q);
            check(rival_chosen && q_intents.empty() && qt.transfers.empty() &&
                  same(q.f.w.corp_body_pools.at(std::make_pair(q.f.corp_1, q.body_a))
                           .quantities[k_stone], 1000.0f),
                  "R9f the player's corp is never a supplier: a fatter player "
                  "pool loses to a rival's, and a world where only the player "
                  "holds stock sees no upkeep purchase at all");
        }

        // R9g: no price basis -> no purchase; and zero rates -> inert tick.
        {
            net_fixture s = make_net_fixture(100.0f, 100.0f);
            s.f.w.markets.erase(s.market); // no market on the body at all
            const auto before = snapshot(s.f.w);
            auto [intents, t] = run_net(s);
            const bool no_market_ok = intents.empty() && t.transfers.empty() &&
                                      snapshot(s.f.w) == before;

            net_fixture z = make_net_fixture(100.0f, 100.0f);
            z.params = network_upkeep_params{}; // unauthored: every rate zero
            const auto z_before = snapshot(z.f.w);
            auto [z_intents, zt] = run_net(z);
            check(no_market_ok && z_intents.empty() && zt.transfers.empty() &&
                  z.f.claims.empty() && snapshot(z.f.w) == z_before,
                  "R9g a body with no market sees no purchase, and unauthored "
                  "(all-zero) rates derive nothing at all - the tick is "
                  "bit-identical");
        }

        // R9h: both claw-backs. Half one: the pool is drained BETWEEN derive
        // and spend - the funded transfer is reversed in the same two floats.
        // Half two: a rogue logistics claim (no intent behind it) is paid by
        // the pass and clawed back whole at settle.
        {
            net_fixture s = make_net_fixture(100.0f, 0.0f);
            const double credit_before = world_credit_exact(s.f.w);
            std::vector<network_purchase> intents = derive_network_upkeep_claims(
                s.f.w, s.f.budgets, s.params, s.f.claims);
            s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a))
                .quantities[k_stone] = 0.0f; // the out-of-band draw
            national_budget_tick t;
            run_national_budget(s.f.w, s.f.budgets, s.f.claims, &t);
            settle_network_purchases(s.f.w, intents, t);
            const bool drained_ok =
                intents.size() == 1 && intents[0].funded && !intents[0].completed &&
                same(intents[0].drawn, 0.0f) &&
                same(s.f.w.corporations.at(s.f.corp_1).balance, 0.0f) &&
                same(s.f.w.nations.at(s.f.nation_a).treasury, 1024.0f) &&
                world_credit_exact(s.f.w) == credit_before;

            net_fixture r = make_net_fixture(100.0f, 100.0f);
            const double r_credit_before = world_credit_exact(r.f.w);
            std::vector<network_purchase> r_intents = derive_network_upkeep_claims(
                r.f.w, r.f.budgets, r.params, r.f.claims);
            budget_claim rogue;
            rogue.nation = r.f.nation_a;
            rogue.corp   = r.f.corp_2; // holds no stock, made no intent
            rogue.line   = budget_priority::logistics_maintenance;
            rogue.amount = 4.0f;
            r.f.claims.push_back(rogue);
            national_budget_tick rt;
            run_national_budget(r.f.w, r.f.budgets, r.f.claims, &rt);
            settle_network_purchases(r.f.w, r_intents, rt);
            check(drained_ok &&
                  same(r.f.w.corporations.at(r.f.corp_2).balance, 0.0f) &&
                  same(r.f.w.corporations.at(r.f.corp_1).balance, 64.0f) &&
                  same(r.f.w.nations.at(r.f.nation_a).treasury, 960.0f) &&
                  r_intents.size() == 2 && r_intents[0].completed && r_intents[1].completed &&
                  world_credit_exact(r.f.w) == r_credit_before,
                  "R9h both claw-backs: a funded draw the pool can no longer "
                  "cover is reversed whole, and a rogue logistics transfer "
                  "with no intent behind it leaves its claimant nothing - "
                  "world credit conserved in both");
        }

        // R9i: determinism - two identical fixtures, identical floats and
        // identical intent lists over the full derive/spend/settle loop.
        {
            net_fixture a = make_net_fixture(100.0f, 100.0f);
            net_fixture b = make_net_fixture(100.0f, 100.0f);
            auto [ia, ta] = run_net(a);
            auto [ib, tb] = run_net(b);
            (void)ta; (void)tb;
            bool identical = snapshot(a.f.w) == snapshot(b.f.w) && ia.size() == ib.size();
            for (std::size_t i = 0; identical && i < ia.size(); ++i)
                identical = ia[i].nation == ib[i].nation && ia[i].supplier == ib[i].supplier &&
                            ia[i].body == ib[i].body && ia[i].resource == ib[i].resource &&
                            same(ia[i].quantity, ib[i].quantity) &&
                            same(ia[i].credits, ib[i].credits) &&
                            same(ia[i].paid, ib[i].paid) && same(ia[i].drawn, ib[i].drawn) &&
                            ia[i].funded == ib[i].funded && ia[i].completed == ib[i].completed;
            check(identical,
                  "R9i two runs of one network fixture are bit-identical, "
                  "intents included");
        }

        // R9j: the REAL wiring - run_nation_step itself (the R8 precedent), so
        // deleting or reordering the 2c/4c calls in nation_step.cpp goes red
        // here and nowhere else. econ_tick 1: the single nation is not due for
        // re-scoring, so the authored budget survives step 1.
        {
            net_fixture s = make_net_fixture(100.0f, 100.0f);
            s.f.w.nation_budgets[s.f.nation_a] = s.f.budgets.at(s.f.nation_a);
            recipe_registry reg;
            reg.set_network_upkeep(s.params);
            const double credit_before = world_credit_exact(s.f.w);
            economy_report rep;
            run_nation_step(s.f.w, reg, rep, /*econ_tick=*/1);
            const auto& pool =
                s.f.w.corp_body_pools.at(std::make_pair(s.f.corp_1, s.body_a));
            check(rep.network_purchases.size() == 2 &&
                  rep.network_purchases[0].completed && rep.network_purchases[1].completed &&
                  same(pool.quantities[k_stone], 84.0f) &&
                  same(pool.quantities[k_timber], 92.0f) &&
                  same(s.f.w.corporations.at(s.f.corp_1).balance, 64.0f) &&
                  same(s.f.w.nations.at(s.f.nation_a).treasury, 960.0f) &&
                  same(rep.budgets[s.f.corp_1].subsidies, 64.0f) &&
                  world_credit_exact(s.f.w) == credit_before,
                  "R9j run_nation_step end to end: derive -> spend -> settle -> "
                  "fold - pools drained, supplier credited, subsidies explain "
                  "the delta, report.network_purchases carries both completed "
                  "rows");
        }
    }

    std::printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
