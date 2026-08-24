// ---------------------------------------------------------------------------
// Headless nation-scorer harness (BL-542; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// `run_national_budget` (BL-537) consumes a `nation_budget` weight vector and
// nothing authored one. `src/world/nation_ai.{hpp,cpp}` authors it, and it is
// the FIRST USE anyone has made of the 2026-08-18 nation-behaviour grant. That
// makes the grant's terms — pure, seeded, deterministic, replayable, a scorer
// and never a planner — the thing this harness is actually checking, not a
// preamble to it.
//
// Requirement group "nation-scorer":
//
//   R1  DETERMINISM AND ORDER-INDEPENDENCE. Two sweeps of one world agree bit
//       for bit; and so do two worlds with identical CONTENT whose
//       `unordered_map` iteration orders differ. `w.nations` and `w.units` are
//       unordered and float addition is not associative, so the sorts in
//       nation_ai.cpp are load-bearing rather than tidy.
//   R2  CADENCE OVER THE SORTED SET. Every eligible nation is re-scored exactly
//       once per `cadence_k` ticks; `nation_is_due` agrees with the sweep; a
//       nation admitted with the highest id shifts NOBODY else's slot; a
//       landless nation is skipped and counted rather than silently dropped.
//   R3  NICHE FIT is complementarity, not output. A nation whose profile
//       complements its neighbours scores strictly higher than the same nation
//       whose profile duplicates them — and MULTIPLYING a nation's whole
//       abundance array changes nothing at all, because the term reads shares.
//       "A nation scores well by producing what others need and cannot make,
//       never by producing the most."
//   R4  CONFLICT AVOIDANCE is a negative term on expected war cost. Force
//       standing across a border raises `threat` and pushes the budget toward
//       contracted force and away from schooling; the SAME force with no
//       declared hostility threatens strictly less (the pair-state is really
//       read); and own force on own ground damps it (deterrence).
//   R5  GRUDGES BIAS, THEY DO NOT TARGET. A grudge lowers a neighbour's niche
//       contribution and lowers the threat that neighbour's force carries — and
//       with terms 1 and 2 both at zero, a grudge changes the weight vector NOT
//       AT ALL. That last row is the one that would catch a grudge leaking into
//       a line of its own.
//   R6  THE TERRESTRIAL HORIZON. A nation that shares no border is outside the
//       horizon: mutating its abundance, its posture, or the force standing on
//       its ground leaves this nation's weights bit-identical. Each row carries
//       a CONTROL — the same mutation applied to a real neighbour, which must
//       move the weights — so the row cannot pass by being applied to something
//       inert.
//   R7  ANTI-VACUITY. The sweep really covered nations; the weights really sum
//       to one and are not all zero; the reference terms sit strictly INSIDE
//       (0, 1) so no clamp is hiding a mutation; two nations really differ; and
//       the float sum R1 guards is really order-sensitive.
//   R8  BL-572 CONTRACT OFFERS. `derive_contract_offers` (nation_step.cpp),
//       tested on its own fixture (real tiles + a hand-built province
//       partition, per condition_set_harness.cpp's own idiom) rather than the
//       fixture above, which has no bodies wired for tile adjacency and no
//       notion of a province: a threatened (funded) nation targets the
//       WEAKEST bordering province of its highest-grudge neighbour, never
//       the strongest or a non-bordering one; an unthreatened (unfunded) one
//       issues nothing at all; escrow accumulates across ticks and clamps
//       EXACTLY at the fee, with the treasury debited by exactly what moved;
//       a second offer opens on a second border province once the first is
//       already claimed, and an older offer is funded before a younger one;
//       ties break by ascending province id; an unanswered offer expires and
//       returns its escrow; and two identical tick sequences from identical
//       worlds replay bit-identically.
//
// MUTATION IS PART OF THE METHOD, NOT A BONUS. Every row here was confirmed by
// breaking the thing it guards and watching it go red — the sorts deleted, the
// share normalisation removed, the `lack` sign flipped, the hostility read
// dropped, the deterrence divisor dropped, the grudge made additive, the
// neighbourhood mean widened to every nation. A row that cannot fail is not a
// row.
//
// RUN THIS HARNESS UNDER AddressSanitizer AS WELL AS PLAIN. The raster walk and
// the six-side neighbour probe index raw vectors; a wrap or bounds slip there is
// an out-of-bounds read that no value assertion can see. Build it with
//   g++ -std=c++20 -O0 -g -fsanitize=address,undefined -Isrc ...
// and treat a clean run as part of the pass.
//
// The process exits non-zero if any assertion FAILs.

#include "world/nation_ai.hpp"
#include "world/components.hpp"
#include "world/nation_generation.hpp" // BL-572: garrison_strength_in
#include "world/nation_step.hpp"       // BL-572: derive_contract_offers, contract_offer_params
#include "world/province.hpp"          // BL-572: province, province_holder_for
#include "world/recipe_registry.hpp"   // BL-572: derive_contract_offers' reg parameter
#include "world/stance.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// Bit equality, deliberately not epsilon. Every "unchanged" row below is a
/// claim that a float did not move AT ALL; an epsilon there would let a genuine
/// low-bit divergence — exactly the class of defect a replay bug is — pass.
bool same(float a, float b) { return a == b; }

bool same_budget(const nation_budget& a, const nation_budget& b)
{
    if (!same(a.reserve_fraction, b.reserve_fraction))
        return false;
    for (std::size_t i = 0; i < priority_count; ++i)
        if (!same(a.weights[i], b.weights[i]))
            return false;
    return true;
}

bool same_map(const std::map<entity_id, nation_budget>& a,
              const std::map<entity_id, nation_budget>& b)
{
    if (a.size() != b.size())
        return false;
    auto ia = a.begin();
    auto ib = b.begin();
    for (; ia != a.end(); ++ia, ++ib)
    {
        if (ia->first != ib->first)
            return false;
        if (!same_budget(ia->second, ib->second))
            return false;
    }
    return true;
}

constexpr std::size_t k_logistics  = static_cast<std::size_t>(budget_priority::logistics_maintenance);
constexpr std::size_t k_schooling  = static_cast<std::size_t>(budget_priority::schooling);
constexpr std::size_t k_milres     = static_cast<std::size_t>(budget_priority::military_research);
constexpr std::size_t k_academic   = static_cast<std::size_t>(budget_priority::academic_research);
constexpr std::size_t k_explore    = static_cast<std::size_t>(budget_priority::public_exploration);
constexpr std::size_t k_contracted = static_cast<std::size_t>(budget_priority::contracted_force);
constexpr std::size_t k_reserve    = static_cast<std::size_t>(budget_priority::strategic_reserve);
constexpr std::size_t k_works      = static_cast<std::size_t>(budget_priority::public_works);
constexpr std::size_t k_charters   = static_cast<std::size_t>(budget_priority::charters);

// ---------------------------------------------------------------------------
// The fixture geometry
// ---------------------------------------------------------------------------
// One body, 24 columns x 5 rows, tiles created only where a nation stands, so
// the gaps between the stripes are genuine absent ground rather than unclaimed
// tiles. Columns WRAP (the body is a cylinder) and column 23 carries no tile, so
// nation D at columns 0-1 is genuinely isolated — it is nobody's neighbour, and
// that is what makes R6 a locality assertion rather than an accident.
//
//   col:  0  1 | 2..5 |  6  7 |  8  9 | 10 11 | 12..13 | 14 15 | 16..23
//         D  D |  --  |  C  C |  A  A |  B  B |   --   |  E  E |   --
//
// A is the subject of nearly every row. Its horizon is exactly {C, B}: from a
// column-8 or column-9 tile the odd-r offsets reach columns 7..10 and no
// further, so the stripes are two tiles wide precisely so that "one step" never
// skips one.

constexpr int k_gw = 24;
constexpr int k_gh = 5;

struct fixture
{
    world     w;
    entity_id body = null_entity;
    entity_id nat_a = null_entity, nat_b = null_entity, nat_c = null_entity, nat_d = null_entity;
    entity_id corp_a1 = null_entity, corp_a2 = null_entity, corp_a3 = null_entity;
    entity_id corp_b1 = null_entity, corp_b2 = null_entity, corp_b3 = null_entity;
    entity_id market = null_entity;
    std::array<std::array<entity_id, k_gh>, k_gw> tile{}; ///< tile[gx][gy], null where absent.
};

/// Abundance profile: `slot` carries `mass`, everything else zero.
std::array<float, resource_count> profile(std::size_t slot, float mass)
{
    std::array<float, resource_count> a{};
    if (slot < resource_count)
        a[slot] = mass;
    return a;
}

entity_id add_nation(world& w, const char* name, expansionism posture,
                     const std::array<float, resource_count>& abundance)
{
    const entity_id  n = w.create_entity();
    nation_component nc{};
    nc.name               = name;
    nc.posture            = posture;
    nc.resource_abundance = abundance;
    w.nations[n]          = nc;
    return n;
}

entity_id add_corp(world& w, const char* name, entity_id home)
{
    const entity_id       c = w.create_entity();
    corporation_component cc{};
    cc.name           = name;
    cc.home_nation    = home;
    w.corporations[c] = cc;
    return c;
}

/// Creates the 5 tiles of column `gx` and assigns them to `nation`.
void add_column(fixture& f, int gx, entity_id nation)
{
    for (int gy = 0; gy < k_gh; ++gy)
    {
        const entity_id t = f.w.create_entity();
        tile_component  tc{};
        tc.body                = f.body;
        tc.grid_x              = gx;
        tc.grid_y              = gy;
        f.w.tiles[t]           = tc;
        f.tile[static_cast<std::size_t>(gx)][static_cast<std::size_t>(gy)] = t;
        if (nation != null_entity)
        {
            f.w.tile_to_nation[t] = nation;
            f.w.nations.at(nation).tiles.push_back(t);
        }
    }
}

entity_id add_unit(world& w, entity_id tile, entity_id owner, int count, int permille)
{
    const entity_id u = w.create_entity();
    unit_component  uc{};
    uc.position                = tile;
    uc.owner                   = owner;
    uc.count                   = count;
    uc.supply_factor_permille  = permille;
    w.units[u]                 = uc;
    return u;
}

/// Nation A produces resource 0 and nothing else; B produces 1; C produces 2;
/// D produces 3. So A's profile COMPLEMENTS both its neighbours and duplicates
/// neither — the reference case term 1 is supposed to reward.
///
/// Prices are deliberately NOT uniform: resource 0 is priced at 2 and everything
/// else at 1, so the price weight genuinely participates. A uniform price array
/// would make `price_weights` return all-ones and quietly remove itself from
/// the arithmetic under test.
fixture make_fixture(std::size_t a_slot = 0, float a_mass = 8.0f, std::size_t b_slot = 1,
                     expansionism b_posture = expansionism::passive)
{
    fixture f;

    f.body = f.w.create_entity();
    body_component bc{};
    bc.name        = "Fixture";
    bc.grid_width  = k_gw;
    bc.grid_height = k_gh;
    f.w.bodies[f.body] = bc;

    f.nat_d = add_nation(f.w, "Distant",  expansionism::passive, profile(3, 8.0f));
    f.nat_c = add_nation(f.w, "Westward", expansionism::passive, profile(2, 8.0f));
    f.nat_a = add_nation(f.w, "Subject",  expansionism::passive, profile(a_slot, a_mass));
    f.nat_b = add_nation(f.w, "Eastward", b_posture,             profile(b_slot, 8.0f));

    f.corp_a1 = add_corp(f.w, "Subject Holdings",  f.nat_a);
    f.corp_a2 = add_corp(f.w, "Subject Refining",  f.nat_a);
    f.corp_a3 = add_corp(f.w, "Subject Carriage",  f.nat_a);
    f.corp_b1 = add_corp(f.w, "Eastward Arms",     f.nat_b);
    f.corp_b2 = add_corp(f.w, "Eastward Freight",  f.nat_b);
    f.corp_b3 = add_corp(f.w, "Eastward Mining",   f.nat_b);

    add_column(f, 0, f.nat_d);
    add_column(f, 1, f.nat_d);
    add_column(f, 6, f.nat_c);
    add_column(f, 7, f.nat_c);
    add_column(f, 8, f.nat_a);
    add_column(f, 9, f.nat_a);
    add_column(f, 10, f.nat_b);
    add_column(f, 11, f.nat_b);

    f.market = f.w.create_entity();
    market_component mc{};
    mc.body = f.body;
    for (std::size_t i = 0; i < resource_count; ++i)
        mc.price[i] = 1.0f;
    mc.price[0]         = 2.0f;
    f.w.markets[f.market] = mc;

    return f;
}

/// The border units of the threat fixture, in creation (= ascending id) order.
///
/// THE SIZE OF THIS TABLE IS THE POINT, and it was arrived at by mutation rather
/// than by taste. The first cut used SIX units and the whole suite passed with
/// the unit sort in `build_force` DELETED — six contributions of similar
/// magnitude have only about five distinct sums across all 720 orderings, so the
/// permutation the hash happened to hand over summed to the same float and the
/// row proved nothing. Forty contributions, searched for spread, come out
/// different under 2895 of 3000 random reorderings. That is what makes R1b a row
/// instead of a decoration, and R7d re-asserts the property at run time rather
/// than trusting this comment.
struct border_unit
{
    int count;
    int permille;
    int hostile_targets; ///< How many of A's three corps this unit's owner hates.
};
constexpr int         k_border_unit_count = 40;
constexpr border_unit k_border_units[k_border_unit_count] = {
    {6, 622, 3},  {9, 319, 0},  {20, 741, 0}, {6, 556, 0},  {17, 341, 1},
    {2, 751, 0},  {5, 246, 0},  {7, 612, 1},  {20, 583, 0}, {6, 586, 0},
    {10, 409, 3}, {10, 242, 0}, {2, 459, 3},  {12, 358, 0}, {12, 918, 1},
    {1, 618, 1},  {3, 849, 0},  {16, 290, 3}, {9, 758, 0},  {7, 630, 3},
    {16, 220, 1}, {1, 955, 0},  {7, 957, 3},  {8, 887, 3},  {4, 811, 1},
    {8, 646, 1},  {16, 699, 1}, {12, 999, 0}, {19, 996, 0}, {11, 769, 1},
    {12, 540, 3}, {12, 648, 1}, {17, 982, 3}, {11, 350, 3}, {7, 300, 1},
    {7, 326, 3},  {16, 609, 1}, {18, 455, 1}, {1, 452, 3},  {14, 466, 1},
};

/// A deterministic permutation of [0, k_border_unit_count): i -> (i*stride) mod n.
/// `stride` must be coprime with n (40), so odd and not a multiple of 5.
/// Deliberately NOT `std::shuffle`: its exact output is unspecified, and a check
/// whose fixture varies with the standard library is not a check.
std::vector<int> stride_permutation(int stride)
{
    std::vector<int> out;
    out.reserve(k_border_unit_count);
    for (int i = 0; i < k_border_unit_count; ++i)
        out.push_back((i * stride) % k_border_unit_count);
    return out;
}

/// Adds the border units on nation B's column-10 tiles (every one of which
/// touches nation A's column 9 and nothing else), and declares the hostility
/// each one's tier calls for.
///
/// `insert_order` permutes THE ORDER THEY ARE INSERTED INTO `w.units`, and
/// nothing else: every entity id is allocated ascending first, so unit `i`
/// carries the same id and the same component whatever order is passed. The
/// world's CONTENT is therefore identical across permutations and only the hash
/// layout differs — which is exactly the perturbation R1b needs, and the one
/// `rehash` could not supply (with 40 consecutive ids, every bucket count above
/// the id range yields the same two orders, so rehashing alone produced two
/// distinct walks where this produces ten).
void arm_border(fixture& f, bool declare_hostility, const std::vector<int>* insert_order = nullptr)
{
    const entity_id owner_for[3] = {f.corp_b3, f.corp_b2, f.corp_b1}; // 0, 1, 3 targets
    const entity_id a_corps[3]   = {f.corp_a1, f.corp_a2, f.corp_a3};

    std::vector<entity_id> uids;
    uids.reserve(k_border_unit_count);
    for (int i = 0; i < k_border_unit_count; ++i)
        uids.push_back(f.w.create_entity());

    std::vector<int> order;
    if (insert_order)
        order = *insert_order;
    else
        for (int i = 0; i < k_border_unit_count; ++i)
            order.push_back(i);

    for (const int i : order)
    {
        const border_unit& bu   = k_border_units[i];
        const int          tier = bu.hostile_targets == 0 ? 0 : (bu.hostile_targets == 1 ? 1 : 2);
        unit_component     uc{};
        uc.position               = f.tile[10][static_cast<std::size_t>(i % k_gh)];
        uc.owner                  = owner_for[tier];
        uc.count                  = bu.count;
        uc.supply_factor_permille = bu.permille;
        f.w.units[uids[static_cast<std::size_t>(i)]] = uc;
    }

    if (!declare_hostility)
        return;
    // corp_b2 hates exactly one of A's corps; corp_b1 hates all three. corp_b3
    // declares nothing — the "same force, no declaration" control R4b needs.
    declare_hostile(f.w, f.corp_b2, a_corps[0]);
    for (const entity_id t : a_corps)
        declare_hostile(f.w, f.corp_b1, t);
}

const nation_score_terms* find_terms(const nation_scorer_report& r, entity_id n)
{
    for (const auto& t : r.scored)
        if (t.nation == n)
            return &t;
    return nullptr;
}

/// The tick on which nation `n` is due, in [0, k). -1 if it is never due.
int due_tick(const world& w, entity_id n, const nation_ai_params& p)
{
    for (int t = 0; t < std::max(1, p.cadence_k); ++t)
        if (nation_is_due(w, n, t, p))
            return t;
    return -1;
}

/// Sweeps ticks 0..k-1 and merges every returned vector, so a caller can reason
/// about "one full rotation" rather than one slot.
std::map<entity_id, nation_budget> full_rotation(const world& w, const nation_ai_params& p)
{
    std::map<entity_id, nation_budget> merged;
    for (int t = 0; t < std::max(1, p.cadence_k); ++t)
        for (const auto& kv : score_national_budgets(w, p, t, nullptr))
            merged[kv.first] = kv.second;
    return merged;
}

/// Scores nation `n` on whichever tick it is actually due, so a row never has to
/// know the cadence slot it happened to land in.
bool score_one(const world& w, const nation_ai_params& p, entity_id n, nation_budget& out,
               nation_score_terms* terms_out = nullptr)
{
    const int t = due_tick(w, n, p);
    if (t < 0)
        return false;
    nation_scorer_report r;
    const auto           m = score_national_budgets(w, p, t, &r);
    const auto           it = m.find(n);
    if (it == m.end())
        return false;
    out = it->second;
    if (terms_out)
    {
        const nation_score_terms* tt = find_terms(r, n);
        if (!tt)
            return false;
        *terms_out = *tt;
    }
    return true;
}

std::vector<entity_id> unit_iteration_order(const world& w)
{
    std::vector<entity_id> order;
    order.reserve(w.units.size());
    for (const auto& kv : w.units)
        order.push_back(kv.first);
    return order;
}

std::vector<entity_id> nation_iteration_order(const world& w)
{
    std::vector<entity_id> order;
    order.reserve(w.nations.size());
    for (const auto& kv : w.nations)
        order.push_back(kv.first);
    return order;
}

// ---------------------------------------------------------------------------
// R7 — anti-vacuity. Run FIRST: if the fixture cannot constrain anything, every
// later row is decoration, and the suite should say so before it says PASS.
// ---------------------------------------------------------------------------

void run_r7_antivacuity()
{
    std::printf("\n-- R7 anti-vacuity ---------------------------------------\n");

    nation_ai_params p;
    fixture          f = make_fixture();
    arm_border(f, true);

    nation_scorer_report r0;
    int                  scored_total = 0;
    bool                 all_sum_one  = true;
    bool                 any_nonzero  = false;
    std::vector<std::array<float, priority_count>> vectors;

    for (int t = 0; t < p.cadence_k; ++t)
    {
        nation_scorer_report r;
        const auto           m = score_national_budgets(f.w, p, t, &r);
        if (t == 0)
            r0 = r;
        scored_total += static_cast<int>(m.size());
        for (const auto& kv : m)
        {
            float total = 0.0f;
            bool  nz    = false;
            for (std::size_t i = 0; i < priority_count; ++i)
            {
                total += kv.second.weights[i];
                if (kv.second.weights[i] > 0.0f)
                    nz = true;
            }
            if (std::fabs(total - 1.0f) > 1.0e-5f)
                all_sum_one = false;
            any_nonzero = any_nonzero || nz;
            vectors.push_back(kv.second.weights);
        }
    }

    check(r0.eligible == 4, "R7a  the sweep sees 4 eligible nations (it covered a world, not nothing)");
    check(scored_total == 4, "R7a  one full rotation scores every eligible nation exactly once");
    check(all_sum_one, "R7b  every returned weight vector sums to 1");
    check(any_nonzero, "R7b  at least one returned weight vector is not all zero");

    // R7e — two nations must genuinely DIFFER. A scorer that normalises every
    // input away would still pass R7a/R7b while computing nothing at all.
    bool any_different = false;
    for (std::size_t i = 1; i < vectors.size(); ++i)
        for (std::size_t j = 0; j < priority_count; ++j)
            if (!same(vectors[i][j], vectors[0][j]))
                any_different = true;
    check(any_different, "R7e  two nations produce DIFFERENT weight vectors (content constrains output)");

    // R7c — the reference terms must sit strictly INSIDE (0, 1). A term pinned
    // on its clamp is a term no mutation can move.
    nation_budget      nb;
    nation_score_terms terms;
    const bool         got = score_one(f.w, p, f.nat_a, nb, &terms);
    check(got, "R7c  nation A is scored at all");
    check(got && terms.niche_fit > 0.0f && terms.niche_fit < 1.0f,
          "R7c  niche_fit is strictly inside (0, 1) — no clamp hides a mutation");
    check(got && terms.niche_gap > 0.0f && terms.niche_gap < 1.0f,
          "R7c  niche_gap is strictly inside (0, 1)");
    check(got && terms.threat > 0.0f && terms.threat < 1.0f,
          "R7c  threat is strictly inside (0, 1)");
    check(got && terms.mean_grudge > 0.0f,
          "R7c  the fixture carries a live grudge (else R5's rows are vacuous)");
    if (got)
        std::printf("      terms: niche_fit=%.6f niche_gap=%.6f threat=%.6f mean_grudge=%.6f "
                    "border_force=%.4f neighbours=%zu\n",
                    terms.niche_fit, terms.niche_gap, terms.threat, terms.mean_grudge,
                    terms.border_force, terms.neighbours.size());

    // R7d — the accumulation R1 guards must genuinely be order-sensitive. This
    // reproduces the border-force contributions exactly as build_force computes
    // them and sums them in both directions.
    float contrib[k_border_unit_count];
    for (int i = 0; i < k_border_unit_count; ++i)
    {
        const border_unit& bu     = k_border_units[i];
        const float        supply = static_cast<float>(bu.permille) / 1000.0f;
        const float        raw    = p.force_scale * static_cast<float>(bu.count) * supply;
        const float        share  = static_cast<float>(bu.hostile_targets) / 3.0f;
        contrib[i]                = raw * (1.0f + p.hostility_amplifier * share);
    }
    float asc = 0.0f, desc = 0.0f, woven = 0.0f;
    for (int i = 0; i < k_border_unit_count; ++i)
        asc += contrib[i];
    for (int i = k_border_unit_count - 1; i >= 0; --i)
        desc += contrib[i];
    // A third order, so the row does not rest on one lucky pair: interleaved
    // from both ends. A fixture where all three agree is a fixture whose sort
    // could be deleted unnoticed — which is exactly what the six-unit first cut
    // of this table turned out to be.
    for (int i = 0; i < k_border_unit_count / 2; ++i)
    {
        woven += contrib[i];
        woven += contrib[k_border_unit_count - 1 - i];
    }
    check(!same(asc, desc) && !same(asc, woven),
          "R7d  the border-force sum IS order-sensitive under two reorderings (R1b can fail)");
    std::printf("      ascending=%a  descending=%a  woven=%a\n", asc, desc, woven);
}

// ---------------------------------------------------------------------------
// R1 — determinism and order-independence
// ---------------------------------------------------------------------------

void run_r1_determinism()
{
    std::printf("\n-- R1 determinism ----------------------------------------\n");

    nation_ai_params p;
    fixture          f = make_fixture();
    arm_border(f, true);

    // EVERY COMPARISON BELOW IS OVER A FULL ROTATION, not one tick. The first
    // cut compared a single tick, and on that tick nation A — the only nation
    // with any border force at all — was not in the cadence slot, so the float
    // accumulation the row exists to guard never entered the comparison. The
    // unit sort could be deleted and the row stayed green. A comparison that
    // does not cover the subject is not a comparison.
    const int due_a = due_tick(f.w, f.nat_a, p);
    check(due_a >= 0, "R1   nation A has a cadence slot at all");
    nation_scorer_report ra, rb;
    const auto           m1 = score_national_budgets(f.w, p, due_a, &ra);
    const auto           m2 = score_national_budgets(f.w, p, due_a, &rb);
    check(m1.find(f.nat_a) != m1.end(),
          "R1   and the sweep under test really contains it (not a vacuous comparison)");
    check(same_map(m1, m2), "R1a  two sweeps of the same world at the same tick agree bit for bit");
    check(ra.eligible == rb.eligible && ra.scored.size() == rb.scored.size(),
          "R1a  and so do their reports");

    // R1b — same content, different `w.units` iteration order. TEN insertion
    // permutations, and every distinct walk order they produce is tested rather
    // than the first: a single permutation can coincidentally sum to the same
    // float, and in the six-unit first cut of this fixture it did — the sort in
    // `build_force` was deleted and the whole suite stayed green.
    const auto                          ref = full_rotation(f.w, p);
    std::vector<std::vector<entity_id>> orders;
    orders.push_back(unit_iteration_order(f.w));
    bool all_orders_agree = true;
    for (const int stride : {3, 7, 9, 11, 13, 17, 19, 21, 23, 27})
    {
        const std::vector<int> perm = stride_permutation(stride);
        fixture                g    = make_fixture();
        arm_border(g, true, &perm);
        std::vector<entity_id> ord = unit_iteration_order(g.w);
        if (std::find(orders.begin(), orders.end(), ord) != orders.end())
            continue;
        orders.push_back(ord);
        if (!same_map(ref, full_rotation(g.w, p)))
            all_orders_agree = false;
    }
    check(orders.size() >= 6, "R1b  at least 6 DISTINCT `w.units` walk orders were produced");
    std::printf("      %zu distinct unit walk orders exercised\n", orders.size());
    check(all_orders_agree,
          "R1b  identical content, every differing unit walk order -> identical weights");

    // R1c — same content, different `w.nations` iteration order. This one reaches
    // the cadence index, so an unsorted walk changes WHICH nations are returned,
    // not only their low bits.
    fixture                      h = make_fixture();
    arm_border(h, true);
    const std::vector<entity_id> base_nations = nation_iteration_order(f.w);
    bool                         n_reordered  = false;
    for (std::size_t b : {3u, 5u, 7u, 11u, 13u, 17u, 19u, 23u, 29u, 31u, 37u, 41u, 61u, 97u})
    {
        h.w.nations.rehash(b);
        if (nation_iteration_order(h.w) != base_nations)
        {
            n_reordered = true;
            break;
        }
    }
    check(n_reordered, "R1c  a differing `w.nations` iteration order was actually produced");
    bool all_ticks_agree = n_reordered;
    for (int t = 0; t < p.cadence_k && n_reordered; ++t)
        if (!same_map(score_national_budgets(f.w, p, t, nullptr),
                      score_national_budgets(h.w, p, t, nullptr)))
            all_ticks_agree = false;
    check(all_ticks_agree,
          "R1c  identical content, different nation walk order -> identical weights, every tick");

    // R1d — purity. The sweep must not have moved the world it read.
    fixture    q = make_fixture();
    arm_border(q, true);
    const std::size_t tiles_before  = q.w.tiles.size();
    const std::size_t units_before  = q.w.units.size();
    const float       treasury_before = q.w.nations.at(q.nat_a).treasury;
    for (int t = 0; t < p.cadence_k; ++t)
        (void)score_national_budgets(q.w, p, t, nullptr);
    check(q.w.tiles.size() == tiles_before && q.w.units.size() == units_before &&
              same(q.w.nations.at(q.nat_a).treasury, treasury_before),
          "R1d  the sweep is pure: it moved no world state");
}

// ---------------------------------------------------------------------------
// R2 — cadence over the sorted nation set
// ---------------------------------------------------------------------------

void run_r2_cadence()
{
    std::printf("\n-- R2 cadence --------------------------------------------\n");

    nation_ai_params p;
    fixture          f = make_fixture();

    // R2a — exactly once per rotation, nobody missed, nobody twice.
    std::map<entity_id, int> seen;
    bool                     agrees = true;
    for (int t = 0; t < p.cadence_k; ++t)
    {
        const auto m = score_national_budgets(f.w, p, t, nullptr);
        for (const auto& kv : m)
        {
            ++seen[kv.first];
            if (!nation_is_due(f.w, kv.first, t, p))
                agrees = false;
        }
        // R2c — and nothing due was left out.
        for (const auto& kv : f.w.nations)
            if (nation_is_due(f.w, kv.first, t, p) && !kv.second.tiles.empty() &&
                m.find(kv.first) == m.end())
                agrees = false;
    }
    bool once_each = seen.size() == 4;
    for (const auto& kv : seen)
        if (kv.second != 1)
            once_each = false;
    check(once_each, "R2a  every eligible nation is scored exactly once per cadence_k ticks");
    check(agrees, "R2c  `nation_is_due` and the sweep's key set agree on every tick");

    // R2b — a nation admitted with the HIGHEST id shifts nobody's slot. This is
    // the case a world that grows a nation actually produces, and it is the
    // property that would break the instant the stagger keyed on anything other
    // than the index into the SORTED set.
    std::map<entity_id, int> before;
    for (const auto& kv : f.w.nations)
        before[kv.first] = due_tick(f.w, kv.first, p);

    const entity_id nat_e = add_nation(f.w, "Latecomer", expansionism::passive, profile(4, 8.0f));
    add_column(f, 14, nat_e);
    add_column(f, 15, nat_e);
    bool highest = true;
    for (const auto& kv : f.w.nations)
        if (kv.first != nat_e && kv.first > nat_e)
            highest = false;
    check(highest, "R2b  the admitted nation really does sort last");

    bool unshifted = true;
    for (const auto& kv : before)
        if (due_tick(f.w, kv.first, p) != kv.second)
            unshifted = false;
    check(unshifted, "R2b  admitting it shifts NO existing nation's cadence slot");
    check(due_tick(f.w, nat_e, p) >= 0, "R2b  and the newcomer gets a slot of its own");

    // R2d — a landless nation is skipped, counted, and never returned.
    const entity_id ghost = add_nation(f.w, "Landless", expansionism::passive, profile(5, 8.0f));
    bool            counted = false, absent = true;
    for (int t = 0; t < p.cadence_k; ++t)
    {
        nation_scorer_report r;
        const auto           m = score_national_budgets(f.w, p, t, &r);
        if (r.skipped_landless == 1)
            counted = true;
        if (m.find(ghost) != m.end())
            absent = false;
    }
    check(counted, "R2d  a landless nation is COUNTED as skipped, not silently dropped");
    check(absent, "R2d  and never appears in the returned map");
}

// ---------------------------------------------------------------------------
// R3 — niche fit is complementarity, not output
// ---------------------------------------------------------------------------

void run_r3_niche()
{
    std::printf("\n-- R3 niche fit ------------------------------------------\n");

    nation_ai_params p;

    // R3a — complementary vs duplicating. Only nation B's profile changes: in
    // the first world it produces what A does not, in the second it produces
    // exactly what A does.
    fixture            comp = make_fixture(0, 8.0f, 1); // B produces resource 1
    fixture            dup  = make_fixture(0, 8.0f, 0); // B produces resource 0, same as A
    nation_budget      nb_comp, nb_dup;
    nation_score_terms t_comp, t_dup;
    const bool         ok = score_one(comp.w, p, comp.nat_a, nb_comp, &t_comp) &&
                    score_one(dup.w, p, dup.nat_a, nb_dup, &t_dup);
    check(ok, "R3a  both worlds score nation A");
    check(ok && t_comp.niche_fit > t_dup.niche_fit,
          "R3a  a COMPLEMENTARY neighbour profile scores strictly higher niche fit");
    check(ok && nb_comp.weights[k_charters] > nb_dup.weights[k_charters],
          "R3a  and pushes a strictly larger share onto `charters`");
    if (ok)
        std::printf("      niche_fit complementary=%.6f duplicating=%.6f | charters %.6f vs %.6f\n",
                    t_comp.niche_fit, t_dup.niche_fit, nb_comp.weights[k_charters],
                    nb_dup.weights[k_charters]);

    // R3b — MAGNITUDE INDEPENDENCE. The same nation producing more of the same
    // thing has exactly the same niche. "Never by producing the most."
    //
    // FOUR masses spanning 32x, every one asserted BIT-identical — and the two
    // small ones are the reason the row works. With the abundance normalisation
    // removed, a mass of 8 drives the raw term far past its clamp, both worlds
    // pin at 1.0, and the row passes while the property is broken; only the
    // clamp guard (R7c) noticed. At 0.5 and 1.0 the mutant lands mid-band and
    // this row fails on its own claim, which is where the failure belongs.
    // Powers of two throughout so the share arithmetic is exact in binary and
    // the assertion can be bit equality rather than an epsilon that would hide
    // a real drift.
    bool scale_invariant = true;
    for (const float mass : {0.5f, 1.0f, 16.0f, 32.0f})
    {
        fixture            scaled = make_fixture(0, mass, 1);
        nation_budget      nb_s;
        nation_score_terms t_s;
        if (!score_one(scaled.w, p, scaled.nat_a, nb_s, &t_s) ||
            !same(t_s.niche_fit, t_comp.niche_fit) || !same_budget(nb_s, nb_comp))
            scale_invariant = false;
    }
    check(scale_invariant,
          "R3b  scaling a nation's whole abundance array across 32x leaves the weight "
          "vector BIT-IDENTICAL");

    // R3b control — the fixture is not inert: a nation that produces something
    // ELSE does move. Without this, R3b could pass over a scorer that ignores
    // abundance entirely.
    fixture            other = make_fixture(2, 8.0f, 1); // A produces resource 2, as C does
    nation_budget      nb_other;
    nation_score_terms t_other;
    const bool         ok3 = score_one(other.w, p, other.nat_a, nb_other, &t_other);
    check(ok3 && !same(t_other.niche_fit, t_comp.niche_fit),
          "R3b  control: changing WHICH resource A produces DOES move niche_fit");
}

// ---------------------------------------------------------------------------
// R4 — conflict avoidance
// ---------------------------------------------------------------------------

void run_r4_conflict()
{
    std::printf("\n-- R4 conflict avoidance ---------------------------------\n");

    nation_ai_params p;

    fixture            calm = make_fixture();
    nation_budget      nb_calm;
    nation_score_terms t_calm;
    bool               ok = score_one(calm.w, p, calm.nat_a, nb_calm, &t_calm);
    check(ok && same(t_calm.threat, 0.0f) && same(t_calm.calm, 1.0f),
          "R4a  an empty border is zero threat and full calm");

    fixture            armed = make_fixture();
    arm_border(armed, true);
    nation_budget      nb_armed;
    nation_score_terms t_armed;
    ok = ok && score_one(armed.w, p, armed.nat_a, nb_armed, &t_armed);
    check(ok && t_armed.threat > 0.0f, "R4a  hostile force across the border raises threat");
    check(ok && nb_armed.weights[k_contracted] > nb_calm.weights[k_contracted],
          "R4a  and pushes the budget toward `contracted_force`");
    check(ok && nb_armed.weights[k_reserve] > nb_calm.weights[k_reserve],
          "R4a  and toward `strategic_reserve`");
    check(ok && nb_armed.weights[k_milres] > nb_calm.weights[k_milres],
          "R4a  and toward `military_research`");
    check(ok && nb_armed.weights[k_schooling] < nb_calm.weights[k_schooling],
          "R4a  and AWAY from `schooling` — the deferred civil horizon IS the cost");

    // R4b — the pair-state is really read. Identical force, no declaration.
    fixture            quiet = make_fixture();
    arm_border(quiet, false);
    nation_budget      nb_quiet;
    nation_score_terms t_quiet;
    ok = ok && score_one(quiet.w, p, quiet.nat_a, nb_quiet, &t_quiet);
    check(ok && t_quiet.threat > 0.0f,
          "R4b  undeclared force still threatens something (force is not a proxy for stance)");
    check(ok && t_quiet.threat < t_armed.threat,
          "R4b  but strictly less than the SAME force with hostility declared");
    if (ok)
        std::printf("      threat: declared=%.6f undeclared=%.6f\n", t_armed.threat, t_quiet.threat);

    // R4c — deterrence. Own force on own ground damps the same threat.
    fixture held = make_fixture();
    arm_border(held, true);
    for (int gy = 0; gy < k_gh; ++gy)
        add_unit(held.w, held.tile[8][static_cast<std::size_t>(gy)], held.corp_a1, 6, 900);
    nation_budget      nb_held;
    nation_score_terms t_held;
    ok = ok && score_one(held.w, p, held.nat_a, nb_held, &t_held);
    check(ok && t_held.own_force > 0.0f, "R4c  own force on own ground is counted");
    check(ok && t_held.threat < t_armed.threat,
          "R4c  and DAMPS the identical border threat (deterrence)");
    check(ok && t_held.threat > 0.0f, "R4c  without cancelling it");

    // R4c control — a hostile corp with NO army near the border threatens
    // nothing. The declaration is a multiplier on force, never a substitute.
    fixture words = make_fixture();
    declare_hostile(words.w, words.corp_b1, words.corp_a1);
    declare_hostile(words.w, words.corp_b1, words.corp_a2);
    nation_budget      nb_words;
    nation_score_terms t_words;
    ok = ok && score_one(words.w, p, words.nat_a, nb_words, &t_words);
    check(ok && same(t_words.threat, 0.0f),
          "R4c  control: hostility with no force near the border threatens NOTHING");
}

// ---------------------------------------------------------------------------
// R5 — grudges bias terms 1 and 2, and target nothing
// ---------------------------------------------------------------------------

void run_r5_grudges()
{
    std::printf("\n-- R5 grudges --------------------------------------------\n");

    nation_ai_params p;

    // A grudge is raised by making the NEIGHBOUR aggressive; nation A itself is
    // untouched, so nothing but the pair changes.
    fixture mild  = make_fixture(0, 8.0f, 1, expansionism::passive);
    fixture bitter = make_fixture(0, 8.0f, 1, expansionism::aggressive);

    const float g_mild   = nation_grudge(mild.w, mild.nat_a, mild.nat_b, p);
    const float g_bitter = nation_grudge(bitter.w, bitter.nat_a, bitter.nat_b, p);
    check(g_bitter > g_mild, "R5   an aggressive neighbour is a bigger grudge");
    check(same(nation_grudge(mild.w, mild.nat_a, mild.nat_d, p), 0.0f),
          "R5   a nation sharing no border carries no grudge at all");
    std::printf("      grudge(A,B): mild=%.6f bitter=%.6f\n", g_mild, g_bitter);

    nation_budget      nb_mild, nb_bitter;
    nation_score_terms t_mild, t_bitter;
    bool ok = score_one(mild.w, p, mild.nat_a, nb_mild, &t_mild) &&
              score_one(bitter.w, p, bitter.nat_a, nb_bitter, &t_bitter);
    check(ok && t_bitter.niche_fit < t_mild.niche_fit,
          "R5a  a grudge makes that neighbour's niche LESS attractive to complement");

    fixture mild_armed = make_fixture(0, 8.0f, 1, expansionism::passive);
    arm_border(mild_armed, true);
    fixture bitter_armed = make_fixture(0, 8.0f, 1, expansionism::aggressive);
    arm_border(bitter_armed, true);
    nation_budget      nb_ma, nb_ba;
    nation_score_terms t_ma, t_ba;
    ok = ok && score_one(mild_armed.w, p, mild_armed.nat_a, nb_ma, &t_ma) &&
         score_one(bitter_armed.w, p, bitter_armed.nat_a, nb_ba, &t_ba);
    check(ok && same(t_ma.border_force, t_ba.border_force),
          "R5b  the raw border force is identical in both worlds");
    check(ok && t_ba.threat < t_ma.threat,
          "R5b  a grudge makes a conflict with that neighbour LESS costly to accept");
    if (ok)
        std::printf("      threat: mild=%.6f bitter=%.6f | niche_fit %.6f vs %.6f\n",
                    t_ma.threat, t_ba.threat, t_mild.niche_fit, t_bitter.niche_fit);

    // R5c — THE ROW THAT MATTERS. With terms 1 and 2 both at zero, a grudge
    // changes NOTHING. Every nation carries the same profile, so no neighbour
    // lacks anything; no unit stands anywhere, so no border carries force. If a
    // grudge had leaked into a line of its own, this is where it would show.
    fixture flat_mild   = make_fixture(2, 8.0f, 2, expansionism::passive);
    fixture flat_bitter = make_fixture(2, 8.0f, 2, expansionism::aggressive);
    // Nation C already produces resource 2; make D match too, so the whole
    // neighbourhood is uniform and every `lack` is exactly zero.
    flat_mild.w.nations.at(flat_mild.nat_d).resource_abundance   = profile(2, 8.0f);
    flat_bitter.w.nations.at(flat_bitter.nat_d).resource_abundance = profile(2, 8.0f);

    nation_budget      nb_fm, nb_fb;
    nation_score_terms t_fm, t_fb;
    const bool         ok2 = score_one(flat_mild.w, p, flat_mild.nat_a, nb_fm, &t_fm) &&
                     score_one(flat_bitter.w, p, flat_bitter.nat_a, nb_fb, &t_fb);
    check(ok2 && same(t_fm.niche_fit, 0.0f) && same(t_fm.niche_gap, 0.0f) &&
              same(t_fm.threat, 0.0f),
          "R5c  the flat fixture really does zero terms 1 and 2");
    check(ok2 && t_fb.mean_grudge > t_fm.mean_grudge,
          "R5c  and the grudge really does differ between the two worlds");
    check(ok2 && same_budget(nb_fm, nb_fb),
          "R5c  with terms 1 and 2 at zero, a grudge changes the weight vector NOT AT ALL");
}

// ---------------------------------------------------------------------------
// R6 — the terrestrial horizon
// ---------------------------------------------------------------------------

void run_r6_horizon()
{
    std::printf("\n-- R6 terrestrial horizon --------------------------------\n");

    nation_ai_params p;

    fixture            base = make_fixture();
    arm_border(base, true);
    nation_budget      nb_base;
    nation_score_terms t_base;
    const bool         ok = score_one(base.w, p, base.nat_a, nb_base, &t_base);
    check(ok, "R6   the reference world scores nation A");

    bool d_outside = true;
    for (const entity_id n : t_base.neighbours)
        if (n == base.nat_d)
            d_outside = false;
    check(ok && d_outside && t_base.neighbours.size() == 2,
          "R6   A's horizon is exactly its two bordering nations; the distant one is outside it");

    // R6a — a non-neighbour's abundance.
    fixture far_abundance = make_fixture();
    arm_border(far_abundance, true);
    far_abundance.w.nations.at(far_abundance.nat_d).resource_abundance = profile(0, 4096.0f);
    nation_budget nb_fa;
    check(ok && score_one(far_abundance.w, p, far_abundance.nat_a, nb_fa) &&
              same_budget(nb_fa, nb_base),
          "R6a  a NON-NEIGHBOUR's abundance leaves A's weights bit-identical");

    // R6a control — the same mutation on a real neighbour must move A. Without
    // it, R6a would pass over a scorer that reads no abundance at all.
    fixture near_abundance = make_fixture();
    arm_border(near_abundance, true);
    near_abundance.w.nations.at(near_abundance.nat_b).resource_abundance = profile(0, 4096.0f);
    nation_budget nb_na;
    check(ok && score_one(near_abundance.w, p, near_abundance.nat_a, nb_na) &&
              !same_budget(nb_na, nb_base),
          "R6a  control: the SAME mutation on a bordering nation DOES move A's weights");

    // R6b — a non-neighbour's posture (the grudge input).
    fixture far_posture = make_fixture();
    arm_border(far_posture, true);
    far_posture.w.nations.at(far_posture.nat_d).posture = expansionism::aggressive;
    nation_budget nb_fp;
    check(ok && score_one(far_posture.w, p, far_posture.nat_a, nb_fp) &&
              same_budget(nb_fp, nb_base),
          "R6b  a NON-NEIGHBOUR's posture leaves A's weights bit-identical");

    fixture near_posture = make_fixture();
    arm_border(near_posture, true);
    near_posture.w.nations.at(near_posture.nat_b).posture = expansionism::aggressive;
    nation_budget nb_np;
    check(ok && score_one(near_posture.w, p, near_posture.nat_a, nb_np) &&
              !same_budget(nb_np, nb_base),
          "R6b  control: a BORDERING nation's posture DOES move A's weights");

    // R6c — force standing deep inside a non-neighbour's ground.
    fixture far_force = make_fixture();
    arm_border(far_force, true);
    for (int gy = 0; gy < k_gh; ++gy)
        add_unit(far_force.w, far_force.tile[0][static_cast<std::size_t>(gy)], far_force.corp_b1,
                 400, 1000);
    nation_budget      nb_ff;
    nation_score_terms t_ff;
    check(ok && score_one(far_force.w, p, far_force.nat_a, nb_ff, &t_ff) &&
              same_budget(nb_ff, nb_base) && same(t_ff.threat, t_base.threat),
          "R6c  a hostile army on a NON-NEIGHBOUR's ground leaves A's threat bit-identical");

    fixture near_force = make_fixture();
    arm_border(near_force, true);
    for (int gy = 0; gy < k_gh; ++gy)
        add_unit(near_force.w, near_force.tile[10][static_cast<std::size_t>(gy)],
                 near_force.corp_b1, 400, 1000);
    nation_budget nb_nf;
    check(ok && score_one(near_force.w, p, near_force.nat_a, nb_nf) &&
              !same_budget(nb_nf, nb_base),
          "R6c  control: the SAME army on the bordering nation's ground DOES move A");
}

// ---------------------------------------------------------------------------
// R8 — BL-572 contract offers (derive_contract_offers, nation_step.cpp)
// ---------------------------------------------------------------------------
// A SEPARATE, self-contained fixture -- the R1-R7 fixture above has no body
// wired for `body_tile_grid`/hex adjacency and no notion of a province at
// all, and `derive_contract_offers` needs both. Real tiles, plus a HAND-BUILT
// province partition positionally aligned with `province_holder` -- the same
// idiom condition_set_harness.cpp's own C8 (province_held) uses: "ids need
// not be contiguous or match position, only ascending".
//
// GEOMETRY. One row (gh=1), so only the E/W hex offsets stay in range and
// every tile borders exactly its two column neighbours -- a controlled,
// minimal border rather than a real partition's shape.
//
//   SINGLE (one bordering candidate): A at columns 0-1; B at 2-5 (2=weak
//   border province, 3-5=non-bordering "mid"); columns 6-7 left EMPTY (no
//   tile at all) so B's column 5 does NOT wrap around to touch A -- there is
//   exactly one province of B's that borders A.
//
//   DUAL (two bordering candidates): A at columns 0-1; B at 2-7, EVERY
//   column claimed, so column 7 wraps around to touch column 0 -- B holds
//   TWO border provinces (2=weak, 7=strong) plus one non-bordering "mid"
//   (3-6).
//
// Nation budgets are HAND-AUTHORED here (one weight, `contracted_force`,
// `reserve_fraction` 0) rather than read off the scorer -- R1-R7 above is
// what proves the scorer's OWN arithmetic; this section is about the
// CONSUMER, exactly the same split nation_wiring.cpp draws between "the
// wiring closes" and "the scorer's weights" (its own file comment).

constexpr int k_offer_gh = 1;

struct offer_fixture
{
    world     w;
    entity_id body        = null_entity;
    entity_id nat_a        = null_entity, nat_b = null_entity;
    uint32_t  prov_a       = 0;
    uint32_t  prov_weak    = 0; ///< Always borders A; the LOWER-strength candidate.
    uint32_t  prov_mid     = 0; ///< Never borders A -- must never be picked.
    uint32_t  prov_strong  = 0; ///< DUAL only: borders A via the column wrap.
};

entity_id offer_add_nation(world& w, const char* name)
{
    const entity_id  n = w.create_entity();
    nation_component nc{};
    nc.name      = name;
    w.nations[n] = nc;
    return n;
}

entity_id offer_add_tile(world& w, entity_id body, int gx, entity_id nation)
{
    const entity_id t = w.create_entity();
    tile_component  tc{};
    tc.body       = body;
    tc.grid_x     = gx;
    tc.grid_y     = 0;
    w.tiles[t]    = tc;
    if (nation != null_entity)
    {
        w.tile_to_nation[t] = nation;
        w.nations.at(nation).tiles.push_back(t);
    }
    return t;
}

void offer_add_garrison(world& w, entity_id tile, entity_id owner, int count)
{
    if (count <= 0)
        return;
    const entity_id u = w.create_entity();
    unit_component  uc{};
    uc.owner                  = owner;
    uc.position                = tile;
    uc.count                   = count;
    uc.type                    = 0;
    uc.supply_factor_permille  = 1000;
    w.units[u]                 = uc;
}

/// SINGLE: exactly one border candidate (prov_weak). `prov_strong` is left 0
/// (there is none).
offer_fixture make_offer_fixture_single(int weak_garrison)
{
    offer_fixture f;
    f.body = f.w.create_entity();
    body_component bc{};
    bc.grid_width  = 8; // columns 6-7 stay empty -- no tile, no wrap-around touch
    bc.grid_height = k_offer_gh;
    f.w.bodies[f.body] = bc;

    f.nat_a = offer_add_nation(f.w, "Subject");
    f.nat_b = offer_add_nation(f.w, "Neighbour");

    const entity_id t0 = offer_add_tile(f.w, f.body, 0, f.nat_a);
    const entity_id t1 = offer_add_tile(f.w, f.body, 1, f.nat_a);
    const entity_id t2 = offer_add_tile(f.w, f.body, 2, f.nat_b); // weak, borders A
    const entity_id t3 = offer_add_tile(f.w, f.body, 3, f.nat_b);
    const entity_id t4 = offer_add_tile(f.w, f.body, 4, f.nat_b);
    const entity_id t5 = offer_add_tile(f.w, f.body, 5, f.nat_b);
    // Columns 6, 7: no tile at all -- body_tile_grid leaves those raster
    // cells null_entity, so column 5's east neighbour resolves to nothing
    // and the wrap back to column 0 (A) never happens.

    offer_add_garrison(f.w, t2, f.nat_b, weak_garrison);

    province p_a;    p_a.id    = 1; p_a.body    = f.body; p_a.tiles = { t0, t1 };
    province p_weak; p_weak.id = 2; p_weak.body = f.body; p_weak.tiles = { t2 };
    province p_mid;  p_mid.id  = 3; p_mid.body  = f.body; p_mid.tiles = { t3, t4, t5 };

    f.w.provinces.provinces = { p_a, p_weak, p_mid };       // ascending id
    f.w.province_holder     = { f.nat_a, f.nat_b, f.nat_b }; // positionally aligned

    f.w.provinces.tile_province[t0] = 1;
    f.w.provinces.tile_province[t1] = 1;
    f.w.provinces.tile_province[t2] = 2;
    f.w.provinces.tile_province[t3] = 3;
    f.w.provinces.tile_province[t4] = 3;
    f.w.provinces.tile_province[t5] = 3;

    f.prov_a = 1; f.prov_weak = 2; f.prov_mid = 3;
    return f;
}

/// DUAL: two border candidates, `prov_weak` and `prov_strong`, plus a
/// non-bordering `prov_mid` between them.
offer_fixture make_offer_fixture_dual(int weak_garrison, int strong_garrison)
{
    offer_fixture f;
    f.body = f.w.create_entity();
    body_component bc{};
    bc.grid_width  = 8; // every column claimed -- column 7 wraps to column 0 (A)
    bc.grid_height = k_offer_gh;
    f.w.bodies[f.body] = bc;

    f.nat_a = offer_add_nation(f.w, "Subject");
    f.nat_b = offer_add_nation(f.w, "Neighbour");

    const entity_id t0 = offer_add_tile(f.w, f.body, 0, f.nat_a);
    const entity_id t1 = offer_add_tile(f.w, f.body, 1, f.nat_a);
    const entity_id t2 = offer_add_tile(f.w, f.body, 2, f.nat_b); // weak, borders A
    const entity_id t3 = offer_add_tile(f.w, f.body, 3, f.nat_b);
    const entity_id t4 = offer_add_tile(f.w, f.body, 4, f.nat_b);
    const entity_id t5 = offer_add_tile(f.w, f.body, 5, f.nat_b);
    const entity_id t6 = offer_add_tile(f.w, f.body, 6, f.nat_b);
    const entity_id t7 = offer_add_tile(f.w, f.body, 7, f.nat_b); // strong, borders A (wrap)

    offer_add_garrison(f.w, t2, f.nat_b, weak_garrison);
    offer_add_garrison(f.w, t7, f.nat_b, strong_garrison);

    province p_a;      p_a.id      = 1; p_a.body      = f.body; p_a.tiles = { t0, t1 };
    province p_weak;   p_weak.id   = 2; p_weak.body   = f.body; p_weak.tiles = { t2 };
    province p_mid;    p_mid.id    = 3; p_mid.body    = f.body; p_mid.tiles = { t3, t4, t5, t6 };
    province p_strong; p_strong.id = 4; p_strong.body = f.body; p_strong.tiles = { t7 };

    f.w.provinces.provinces = { p_a, p_weak, p_mid, p_strong };       // ascending id
    f.w.province_holder     = { f.nat_a, f.nat_b, f.nat_b, f.nat_b }; // positionally aligned

    f.w.provinces.tile_province[t0] = 1;
    f.w.provinces.tile_province[t1] = 1;
    f.w.provinces.tile_province[t2] = 2;
    f.w.provinces.tile_province[t3] = 3;
    f.w.provinces.tile_province[t4] = 3;
    f.w.provinces.tile_province[t5] = 3;
    f.w.provinces.tile_province[t6] = 3;
    f.w.provinces.tile_province[t7] = 4;

    f.prov_a = 1; f.prov_weak = 2; f.prov_mid = 3; f.prov_strong = 4;
    return f;
}

/// Hand-authors a `contracted_force`-only budget for `n`, and sets its
/// treasury -- the "fund a nation's treasury by hand" pattern nation_wiring.cpp
/// and money_conservation.cpp use for the budget pass, applied one line
/// (BL-572's own) further down the same chain. `weight` defaults to 1.0 so
/// `share == spendable == treasury` exactly (reserve 0, one weight, ratio 1)
/// and every escrow arithmetic check below can be worked out by hand.
void offer_fund(world& w, entity_id n, float treasury, float weight = 1.0f)
{
    w.nations.at(n).treasury = treasury;
    nation_budget nb;
    nb.reserve_fraction = 0.0f;
    nb.weights[static_cast<std::size_t>(budget_priority::contracted_force)] = weight;
    w.nation_budgets[n] = nb;
}

const mercenary_offer* offer_targeting(const world& w, uint32_t province_id)
{
    for (const mercenary_offer& off : w.mercenary_offers)
        if (off.target_province == province_id)
            return &off;
    return nullptr;
}

void run_r8_contract_offers()
{
    std::printf("\n-- R8 contract offers (BL-572) ---------------------------\n");

    const recipe_registry reg; // default nation_ai_params -- derive_contract_offers
                                // only reads reg.nation_ai() for the grudge pick

    // ---- R8a: targets the WEAKEST border province, never the strongest or
    // the non-bordering middle one -------------------------------------------
    {
        offer_fixture f = make_offer_fixture_dual(/*weak*/ 5, /*strong*/ 50);
        contract_offer_params params;
        offer_fund(f.w, f.nat_a, 1.0f); // just enough to clear the funding gate

        derive_contract_offers(f.w, reg, /*econ_tick*/ 0, params);

        check(f.w.mercenary_offers.size() == 1,
              "R8a exactly one offer opens on the first tick");
        check(offer_targeting(f.w, f.prov_weak) != nullptr,
              "R8a the target is the WEAKEST border province");
        check(offer_targeting(f.w, f.prov_strong) == nullptr,
              "R8a not the STRONGEST border province");
        check(offer_targeting(f.w, f.prov_mid) == nullptr,
              "R8a not the non-bordering MIDDLE province, however weak it might be");

        const mercenary_offer* off = offer_targeting(f.w, f.prov_weak);
        check(off != nullptr && off->client == f.nat_a,
              "R8a the offer's client is the nation whose budget funded it");
        check(off != nullptr && off->fee == params.base_fee,
              "R8a the fee is fixed at issuance to the template minimum");
        check(off != nullptr && off->issued_tick == 0 && off->deadline == params.deadline_ticks,
              "R8a issued_tick/deadline are set at issuance");
    }

    // ---- R8c: escrow accumulates across ticks and CLAMPS exactly at the
    // fee; the treasury moves by exactly what the escrow gained, and nothing
    // more once there is nowhere else for the leftover share to go ----------
    {
        offer_fixture f = make_offer_fixture_single(/*weak*/ 5);
        contract_offer_params params;
        params.base_fee = 90.0f;

        offer_fund(f.w, f.nat_a, 40.0f); // less than the fee: forces multi-tick accrual
        derive_contract_offers(f.w, reg, 0, params);

        const mercenary_offer* off = offer_targeting(f.w, f.prov_weak);
        check(off != nullptr && off->offer_escrow == 40.0f,
              "R8c the first tick's whole share lands in escrow (40 < fee 90)");
        check(same(f.w.nations.at(f.nat_a).treasury, 0.0f),
              "R8c the treasury is debited by exactly what the escrow gained");

        // Simulates a second tick's income (apply_budget's levy, which this
        // harness does not run) -- the weight is already authored, so only
        // the treasury needs topping up.
        f.w.nations.at(f.nat_a).treasury = 30.0f;
        derive_contract_offers(f.w, reg, 1, params);
        off = offer_targeting(f.w, f.prov_weak);
        check(off != nullptr && off->offer_escrow == 70.0f,
              "R8c escrow accumulates across ticks: 40 + 30 = 70, still under the 90 fee");
        check(f.w.mercenary_offers.size() == 1,
              "R8c still exactly one offer -- there is no second border province to open one on");

        // A third tick funds PAST the fee -- escrow must clamp EXACTLY at fee
        // (NR-568's whole-or-nothing), and because this fixture has no second
        // candidate for the leftover to spill onto, the UNSPENT portion of
        // the share must not leave the treasury at all (rule 2: an
        // underspent line accumulates rather than evaporating).
        f.w.nations.at(f.nat_a).treasury = 100.0f;
        derive_contract_offers(f.w, reg, 2, params);
        off = offer_targeting(f.w, f.prov_weak);
        check(off != nullptr && off->offer_escrow == 90.0f,
              "R8c escrow clamps EXACTLY at the fee (70 + 100 would be 170; capped to 90)");
        check(same(f.w.nations.at(f.nat_a).treasury, 80.0f),
              "R8c only the 20 credits actually needed left the treasury, not the whole 100 share");
    }

    // ---- R8b: an UNTHREATENED nation -- no treasury -- issues no offer ----
    {
        offer_fixture f = make_offer_fixture_single(5);
        contract_offer_params params;

        derive_contract_offers(f.w, reg, 0, params);
        check(f.w.mercenary_offers.empty(),
              "R8b a nation with no treasury and no authored budget issues no offer");

        // Same border, an authored budget this time, but ZERO treasury
        // specifically -- the funding axis alone gates issuance (NR-580: a
        // freshly generated world's nations all start here).
        offer_fund(f.w, f.nat_a, 0.0f);
        derive_contract_offers(f.w, reg, 0, params);
        check(f.w.mercenary_offers.empty(),
              "R8b a nation with an authored budget but ZERO treasury still issues no offer");
    }

    // ---- R8d: a SECOND border province gets its own offer once the first
    // is already claimed -- several offers open and filling concurrently --
    // and the OLDER offer is funded before the younger one, oldest-issued-
    // first, falling through to the younger offer once the older is full --
    {
        offer_fixture f = make_offer_fixture_dual(/*weak*/ 5, /*strong*/ 50);
        contract_offer_params params;
        params.base_fee = 20.0f; // small: the weak province's offer clears in one tick

        offer_fund(f.w, f.nat_a, 20.0f);
        derive_contract_offers(f.w, reg, 0, params);
        const mercenary_offer* weak_off = offer_targeting(f.w, f.prov_weak);
        check(weak_off != nullptr && weak_off->offer_escrow == 20.0f,
              "R8d the weak province's offer clears fully in one tick at this fee");
        check(offer_targeting(f.w, f.prov_strong) == nullptr,
              "R8d the strong province has no offer yet -- the weak one is still the only target");

        // Next tick: prov_weak is already claimed, so THIS tick's share opens
        // (and starts funding) prov_strong -- "several offers open and
        // filling concurrently".
        f.w.nations.at(f.nat_a).treasury = 8.0f;
        derive_contract_offers(f.w, reg, 1, params);
        check(f.w.mercenary_offers.size() == 2,
              "R8d a second offer opens on the strong province once the weak one is claimed");
        weak_off = offer_targeting(f.w, f.prov_weak);
        const mercenary_offer* strong_off = offer_targeting(f.w, f.prov_strong);
        check(weak_off != nullptr && strong_off != nullptr,
              "R8d one offer per bordering province, weak and strong both represented");
        check(weak_off != nullptr && weak_off->offer_escrow == 20.0f,
              "R8d the already-full offer took nothing further -- it stops asking");
        check(strong_off != nullptr && strong_off->offer_escrow == 8.0f,
              "R8d the whole tick's share landed on the newer offer once the older one was full");

        // Tick 2: the older (weak) offer is still full, so funding must FALL
        // THROUGH it to reach the younger (strong) one rather than stalling.
        f.w.nations.at(f.nat_a).treasury = 5.0f;
        derive_contract_offers(f.w, reg, 2, params);
        strong_off = offer_targeting(f.w, f.prov_strong);
        check(strong_off != nullptr && strong_off->offer_escrow == 13.0f,
              "R8d funding falls through a full older offer to reach the younger one (8 + 5 = 13)");
    }

    // ---- R8e: TTL expiry returns the escrow and removes the offer ---------
    {
        offer_fixture f = make_offer_fixture_single(5);
        contract_offer_params params;
        params.base_fee       = 90.0f;
        params.offer_ttl_ticks = 5;

        offer_fund(f.w, f.nat_a, 40.0f);
        derive_contract_offers(f.w, reg, 0, params);
        check(f.w.mercenary_offers.size() == 1, "R8e one offer opens at tick 0");

        // Advance past the TTL with the budget WITHDRAWN, not just the
        // treasury drained -- move 1's refund lands BEFORE move 2/3 read the
        // treasury this same tick, so a still-authored weight would let the
        // refunded credits fund a BRAND NEW offer on the same province in
        // the same call, masking the expiry this row exists to isolate. That
        // immediate-reissue behaviour is real and not itself wrong -- a
        // nation that still wants the work and still has the money reopens
        // it -- but it is a DIFFERENT claim from "the escrow is refunded",
        // so this row removes the budget entirely to isolate the latter.
        f.w.nations.at(f.nat_a).treasury = 0.0f;
        f.w.nation_budgets.erase(f.nat_a);
        derive_contract_offers(f.w, reg, /*econ_tick*/ 5, params); // 5 - 0 >= ttl(5)
        check(f.w.mercenary_offers.empty(),
              "R8e the offer expires once issued_tick is offer_ttl_ticks behind the current tick");
        check(same(f.w.nations.at(f.nat_a).treasury, 40.0f),
              "R8e its escrow (40) is refunded to the treasury on expiry");
    }

    // ---- R8e2: the DISCOVERED interaction, asserted rather than left
    // implicit -- expiry (move 1) refunds the escrow BEFORE move 2 reads the
    // treasury, so a nation whose budget is STILL authored and whose target
    // is STILL eligible re-opens the SAME province in the SAME tick the old
    // offer expired in. Deterministic, and arguably correct (an unmet threat
    // that is still funded keeps asking) -- but worth a named row, since R8e
    // above had to withdraw the budget specifically to avoid this masking it.
    {
        offer_fixture f = make_offer_fixture_single(5);
        contract_offer_params params;
        params.base_fee        = 90.0f;
        params.offer_ttl_ticks = 5;

        offer_fund(f.w, f.nat_a, 40.0f);
        derive_contract_offers(f.w, reg, 0, params);
        const mercenary_offer* first = offer_targeting(f.w, f.prov_weak);
        check(first != nullptr && first->issued_tick == 0 && first->offer_escrow == 40.0f,
              "R8e2 the first offer opens at tick 0 with escrow 40");

        // Treasury drained, budget LEFT AUTHORED this time.
        f.w.nations.at(f.nat_a).treasury = 0.0f;
        derive_contract_offers(f.w, reg, 5, params);
        const mercenary_offer* second = offer_targeting(f.w, f.prov_weak);
        check(f.w.mercenary_offers.size() == 1,
              "R8e2 exactly one offer exists after the expire-and-reopen tick");
        check(second != nullptr && second->issued_tick == 5,
              "R8e2 it is a NEW offer (issued_tick moved to the reopening tick)");
        check(second != nullptr && second->offer_escrow == 40.0f,
              "R8e2 funded by the SAME 40 credits the expiry just refunded");
        check(same(f.w.nations.at(f.nat_a).treasury, 0.0f),
              "R8e2 the treasury ends the tick exactly where the reissue left it");
    }

    // ---- R8f: ties break by ascending province id --------------------------
    {
        offer_fixture f = make_offer_fixture_dual(/*weak*/ 10, /*strong*/ 10); // EQUAL garrisons
        contract_offer_params params;
        offer_fund(f.w, f.nat_a, 5.0f);

        derive_contract_offers(f.w, reg, 0, params);
        check(f.w.mercenary_offers.size() == 1 && offer_targeting(f.w, f.prov_weak) != nullptr,
              "R8f equal garrison strength ties break by ASCENDING province id "
              "(prov_weak's id is lower than prov_strong's)");
    }

    // ---- R8g: the sequence replays -----------------------------------------
    {
        auto run_sequence = [&]() {
            offer_fixture f = make_offer_fixture_dual(5, 50);
            contract_offer_params params;
            params.base_fee = 90.0f;
            offer_fund(f.w, f.nat_a, 0.0f);
            for (int t = 0; t < 6; ++t)
            {
                f.w.nations.at(f.nat_a).treasury += 15.0f;
                derive_contract_offers(f.w, reg, t, params);
            }
            return f;
        };

        const offer_fixture r1 = run_sequence();
        const offer_fixture r2 = run_sequence();

        check(r1.nat_a == r2.nat_a && r1.prov_weak == r2.prov_weak,
              "R8g the two runs really are built identically (same entity/province ids)");

        bool same_offers = r1.w.mercenary_offers.size() == r2.w.mercenary_offers.size()
                         && !r1.w.mercenary_offers.empty();
        for (std::size_t i = 0; same_offers && i < r1.w.mercenary_offers.size(); ++i)
        {
            const mercenary_offer& a = r1.w.mercenary_offers[i];
            const mercenary_offer& b = r2.w.mercenary_offers[i];
            same_offers = a.id == b.id && a.client == b.client
                       && a.target_province == b.target_province
                       && a.template_index == b.template_index && same(a.fee, b.fee)
                       && a.deadline == b.deadline && a.issued_tick == b.issued_tick
                       && same(a.offer_escrow, b.offer_escrow);
        }
        check(same_offers, "R8g two identical tick sequences produce bit-identical offers");
        check(same(r1.w.nations.at(r1.nat_a).treasury, r2.w.nations.at(r2.nat_a).treasury),
              "R8g and bit-identical treasuries");
    }
}

} // namespace

int main()
{
    std::printf("=== nation_scorer_harness (BL-542) ===\n");
    run_r7_antivacuity();
    run_r1_determinism();
    run_r2_cadence();
    run_r3_niche();
    run_r4_conflict();
    run_r5_grudges();
    run_r6_horizon();
    run_r8_contract_offers();

    std::printf("\n---------------------------------------------------------\n");
    std::printf("PASS %d   FAIL %d\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
