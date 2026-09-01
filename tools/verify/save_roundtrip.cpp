// Headless save/load round-trip harness (BL-536). No SDL / Lua / ImGui.
//
// Verifies the world-snapshot serialiser against requirement group
// `world-save-snapshot` rows R1-R4 and R6's world half:
//
//   P1  R1  A round trip preserves every serialised field.
//   P2  R1  state_hash agrees across the round trip, at the same tick.
//   P3  R2  A bad magic / bad version / truncated stream is REJECTED, and the
//           destination world is left untouched.
//   P4  R3  Derived caches are cleared on load and rebuild identically.
//   P5  R3  corp_modifiers survives with its ORDER intact -- the case a re-fold
//           from earned_techs would get wrong (NR-510).
//   P6  R4  A battle round-trips mid-fight and continues on the same RNG stream.
//   P7  R1  Container coverage: every container the canonical world populates is
//           non-empty on the far side, so an omitted section cannot pass vacuously.
//   P8  R1  The containers a fresh world leaves empty, populated by hand.
//   P9      Sprint N3 T2 -- `world::nation_budgets` (format v3): the map
//           round-trips exactly (weights + reserve for two nations, a
//           non-dyadic weight included), an EMPTY map round-trips as empty, and
//           a v2-versioned stream is refused with the destination untouched.
//
// ON THE TWO KINDS OF CHECK IN P1. Byte-equality of a RE-serialisation
// (write -> read -> write, compare the two streams) proves the read and write
// sides agree field for field, and it cannot miss a field the way a hand-written
// comparison walker can. What it CANNOT catch is a container nobody serialises at
// all: omitted from the write, empty after the read, omitted from the re-write,
// bytes equal, bug shipped. P7 is the coverage half that closes that hole, and
// the two are only sound together.
//
// Exits 0 on PASS, non-zero on any failure (naming it). Kept outside src/ so the
// CMake game glob does not pull it into the build.

#include "world/campaign_battle.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_log.hpp"
#include "world/logistics.hpp"
#include "world/modifier_set.hpp"
#include "world/nation_budget.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "harness_params.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

void note(const char* fmt, unsigned long long a)
{
    std::printf("     %s%llu\n", fmt, a);
}

/// Serialise @p w to a string.
std::string to_bytes(const world& w)
{
    std::ostringstream out(std::ios::binary);
    write_world_snapshot(w, out);
    return out.str();
}

/// Read a snapshot from @p bytes into @p w.
bool from_bytes(const std::string& bytes, world& w)
{
    std::istringstream in(bytes, std::ios::binary);
    return read_world_snapshot(w, in);
}

} // namespace

int main()
{
    // One generation, reused by every phase below -- make_hard_coded_world is
    // the expensive thing in this file by two orders of magnitude, and none of
    // the phases need a second, differently-seeded world.
    generation_report report;
    world w = make_hard_coded_world(no_prehistory(), &report);
    seed_genesis_history(w, report);

    std::printf("Generated world: %llu tiles, %llu bodies, %llu corps, %llu history entries\n\n",
                (unsigned long long)w.tiles.size(), (unsigned long long)w.bodies.size(),
                (unsigned long long)w.corporations.size(),
                (unsigned long long)w.history_log.size());

    // BL-613: make the nation record's newest field non-trivial before the round
    // trip, so P1's byte-equality cannot pass over it on an all-zero default (a
    // `no_prehistory()` world may seed every nation's qualification at the
    // floor). Lowest nation id, so the choice cannot ride the unordered layout.
    entity_id qual_nation = null_entity;
    for (const auto& [nid, nc] : w.nations)
        if (qual_nation == null_entity || nid < qual_nation)
            qual_nation = nid;
    if (qual_nation != null_entity)
        w.nations.at(qual_nation).qualification = 0.375f;

    // BL-614: same treatment for the building record's newest field — the
    // default is 0 everywhere (nothing sets a wage bid yet), so give one
    // building a distinctive bid before the round trip. Lowest building id.
    entity_id bid_building = null_entity;
    for (const auto& [bid, bc] : w.buildings)
        if (bid_building == null_entity || bid < bid_building)
            bid_building = bid;
    if (bid_building != null_entity)
        w.buildings.at(bid_building).wage_bid = 0.4375f;

    // BL-641: `supply_factor_permille` is the OTHER new persistent field in the
    // building record, and it defaults to 1000 on every building in a world whose
    // upkeep rates ship at zero — a uniform column P1's byte-equality would pass
    // over without noticing a misread. Pin the same building to a distinctive
    // value, for the same reason and by the same discipline as the wage bid above.
    if (bid_building != null_entity)
        w.buildings.at(bid_building).supply_factor_permille = 637;

    // BL-631: `ownership_class` is a NEW persistent field in the corp record.
    // A `no_prehistory()` world reaches it through the national-character
    // fallback, which can class a whole world alike — so pin one corp to a
    // distinctive value before the trip, or P1's byte-equality could pass over a
    // uniform column. Lowest corp id, same discipline as the fixtures around it.
    entity_id oc_corp = null_entity;
    for (const auto& [cid, cc] : w.corporations)
        if (oc_corp == null_entity || cid < oc_corp)
            oc_corp = cid;
    if (oc_corp != null_entity)
        w.corporations.at(oc_corp).ownership_class = ownership_class::publicly_held;

    // BL-616: decline rides `growth_accumulator` as a NEGATIVE consecutive-
    // failure streak (no new persistent field, no version bump) — so make one
    // centre's accumulator negative before the round trip: a reader that
    // rejected or zero-clamped a negative streak would break byte-equality
    // here. Lowest centre id, same discipline as the two fixtures above.
    entity_id neg_centre = null_entity;
    for (const auto& [cid, pcc] : w.population_centres)
        if (neg_centre == null_entity || cid < neg_centre)
            neg_centre = cid;
    if (neg_centre != null_entity)
        w.population_centres.at(neg_centre).growth_accumulator = -37;

    // BL-624: the razed tier is a NEW persistent field (format v15), default
    // false everywhere at generation — so demote one centre to the razed
    // state before the round trip, in the shape raze_centre leaves it
    // (population 0, scale 1, razed set). Highest centre id, so it never
    // collides with the negative-streak fixture above.
    entity_id razed_centre = null_entity;
    for (const auto& [cid, pcc] : w.population_centres)
        if (razed_centre == null_entity || cid > razed_centre)
            razed_centre = cid;
    if (razed_centre != null_entity && razed_centre != neg_centre)
    {
        auto& pcc      = w.population_centres.at(razed_centre);
        pcc.razed      = true;
        pcc.population = 0;
        pcc.scale      = 1;
    }
    else
        razed_centre = null_entity; // a one-centre world: skip the fixture

    // BL-626: `corporation_component::returns` is a NEW persistent container
    // (format v16), empty at generation because it is apply_budget that fills
    // it -- and this harness never runs the economy. So hand-file a short
    // history on the lowest-id corp, with every field distinct, before the
    // round trip: an empty vector would let P1's byte-equality pass over the
    // whole record. The full behavioural coverage is
    // tools/verify/quarterly_return.cpp; what is owed HERE is the seam.
    entity_id filing_corp = null_entity;
    for (const auto& [cid, cc] : w.corporations)
        if (filing_corp == null_entity || cid < filing_corp)
            filing_corp = cid;
    if (filing_corp != null_entity)
    {
        std::vector<quarterly_return>& hist = w.corporations.at(filing_corp).returns;
        for (int q = 0; q < 3; ++q)
        {
            const float k = static_cast<float>(q + 1);
            quarterly_return r;
            r.income      = 100.0f * k;
            r.expenditure =  11.0f * k;
            r.maintenance =  12.0f * k;
            r.wages       =  13.0f * k;
            r.interest    =  14.0f * k;
            r.levies      =  15.0f * k;
            r.upkeep      =  16.0f * k;
            r.net         = -19.0f * k;   // signed, so a sign-losing read fails
            r.balance     = 5000.0f - 19.0f * k;
            r.holdings    = static_cast<uint32_t>(q + 2);
            r.book_value  = 250.0f * k;
            hist.push_back(r);
        }
    }

    // -----------------------------------------------------------------------
    // P1 (R1) -- a round trip preserves every serialised field
    // -----------------------------------------------------------------------
    const std::string bytes_once = to_bytes(w);
    note("snapshot bytes: ", (unsigned long long)bytes_once.size());

    world loaded;
    const bool read_ok = from_bytes(bytes_once, loaded);
    check(read_ok, "P1 a freshly written snapshot reads back without rejection");

    const std::string bytes_twice = read_ok ? to_bytes(loaded) : std::string();
    check(read_ok && bytes_once == bytes_twice,
          "P1 re-serialising the loaded world reproduces the snapshot byte for byte");

    // BL-613: the qualification fraction survives by VALUE, not just by byte
    // agreement of the two writes.
    if (qual_nation != null_entity)
    {
        const auto nit = loaded.nations.find(qual_nation);
        check(read_ok && nit != loaded.nations.end()
                  && nit->second.qualification == 0.375f,
              "P1 nation qualification (BL-613) round-trips at its written value");
    }

    // BL-614: likewise for the wage bid.
    if (bid_building != null_entity)
    {
        const auto bit = loaded.buildings.find(bid_building);
        check(read_ok && bit != loaded.buildings.end()
                  && bit->second.wage_bid == 0.4375f,
              "P1 building wage_bid (BL-614) round-trips at its written value");
    }

    // BL-641: likewise for the building supply factor.
    if (bid_building != null_entity)
    {
        const auto bit = loaded.buildings.find(bid_building);
        check(read_ok && bit != loaded.buildings.end()
                  && bit->second.supply_factor_permille == 637,
              "P1 building supply_factor_permille (BL-641) round-trips at its written value");
    }

    // BL-626: the filed quarterly returns survive by VALUE, in order, with the
    // signed `net` intact — the field a sign-losing or reordering read would
    // get wrong while still producing an equal-length record.
    if (filing_corp != null_entity)
    {
        const auto cit = loaded.corporations.find(filing_corp);
        bool ok = read_ok && cit != loaded.corporations.end()
               && cit->second.returns.size() == w.corporations.at(filing_corp).returns.size();
        if (ok)
        {
            const auto& want = w.corporations.at(filing_corp).returns;
            const auto& got  = cit->second.returns;
            for (std::size_t i = 0; i < want.size() && ok; ++i)
                ok = got[i].income == want[i].income
                  && got[i].expenditure == want[i].expenditure
                  && got[i].maintenance == want[i].maintenance
                  && got[i].wages == want[i].wages && got[i].interest == want[i].interest
                  && got[i].levies == want[i].levies && got[i].upkeep == want[i].upkeep
                  && got[i].net == want[i].net && got[i].balance == want[i].balance
                  && got[i].holdings == want[i].holdings
                  && got[i].book_value == want[i].book_value;
        }
        check(ok, "P1 filed quarterly returns (BL-626) round-trip by value, in order");
    }

    // BL-624: the razed centre reloads as the ruin it is — flag, zeroed
    // population and floored scale all by VALUE, and the entity/name/tile
    // records all still present (demotion, not erasure).
    if (razed_centre != null_entity)
    {
        const auto cit = loaded.population_centres.find(razed_centre);
        check(read_ok && cit != loaded.population_centres.end()
                  && cit->second.razed && cit->second.population == 0
                  && cit->second.scale == 1
                  && loaded.population_centre_tile.count(razed_centre) == 1
                  && loaded.population_centre_name.count(razed_centre) == 1,
              "P1 a razed centre (BL-624) round-trips as razed, with entity/name/tile kept");
    }

    // -----------------------------------------------------------------------
    // P2 (R1) -- state_hash agrees, at the same tick
    // -----------------------------------------------------------------------
    // Corroboration, not the assertion: state_hash folds only the fields a TICK
    // may mutate, so on its own it would pass over a dropped body name or a lost
    // province partition. P1 is the assertion; this catches the narrower class of
    // bug where a value round-trips structurally but changes numerically.
    {
        const int tick = 4242;
        const uint64_t before = w.state_hash(tick);
        const uint64_t after  = loaded.state_hash(tick);
        check(before == after, "P2 state_hash agrees across the round trip at the same tick");
        if (before != after)
            std::printf("     before=%016llX after=%016llX\n",
                        (unsigned long long)before, (unsigned long long)after);
    }

    // -----------------------------------------------------------------------
    // P3 (R2) -- a bad stream is rejected AND mutates nothing
    // -----------------------------------------------------------------------
    // The second half is the one worth having. A loader that rejects cleanly but
    // has already half-replaced the caller's world has not rejected at all.
    {
        // A sentinel world the rejected loads are aimed at. If any of them writes
        // through, these fields move.
        world victim;
        victim.player_entity = 12345;
        victim.home_body     = 999;
        victim.next_order_id = 77;
        const entity_id keep_player = victim.player_entity;
        const entity_id keep_home   = victim.home_body;
        const uint32_t  keep_order  = victim.next_order_id;

        {
            std::string bad = bytes_once;
            bad[0] = 'X'; // corrupt the magic
            check(!from_bytes(bad, victim), "P3 a wrong magic is rejected");
        }
        {
            std::string bad = bytes_once;
            // The version is the second uint32; bump it past what we accept.
            const uint32_t wrong = world_save_version + 1;
            std::memcpy(&bad[4], &wrong, sizeof wrong);
            check(!from_bytes(bad, victim), "P3 a mismatched version is rejected");
        }
        {
            // Sprint 16, BL-570: A v4 stream's `condition` records are one
            // field short (no `province`), so a reader that accepted it would
            // misread whatever bytes follow the first law's/embargo's condition
            // as that condition's province id, and every field after it in the
            // same record would shift with it. The whole stream is refused
            // instead -- a v4 save is not migrated, it is rejected, and the
            // destination is not touched.
            // P9..P19 name v4..v14 as refused predecessors; P20 names the
            // immediately-prior format symbolically. DELIBERATELY NOT PINNED TO A
            // LITERAL: two slices each claimed a version in one wave from separate
            // worktrees, so a literal here breaks whichever lands second and would
            // be re-blessed rather than read. What must hold is the property the
            // rows actually rest on -- every literal below is a PAST format, never
            // the current one.
            static_assert(world_save_version > 14,
                          "P9..P19 name v4..v14 as refused predecessors; "
                          "re-read these rows on a bump");
            std::string bad = bytes_once;
            const uint32_t v4 = 4;
            std::memcpy(&bad[4], &v4, sizeof v4);
            check(!from_bytes(bad, victim), "P9 a v4-versioned stream is refused");
        }
        {
            // Sprint 16, BL-571: the IMMEDIATE previous format (BL-570's v5,
            // the version this batch actually released before BL-571 bumped
            // again). A v5 stream's nation record is one w_id short (no
            // capital_tile) -- not a trailing gap but a MID-RECORD one, so a
            // reader that accepted it would misread every field of every
            // nation after the first and everything serialised after
            // `nations` besides. Refused whole, same contract as every prior
            // bump.
            std::string bad = bytes_once;
            const uint32_t v5 = 5;
            std::memcpy(&bad[4], &v5, sizeof v5);
            check(!from_bytes(bad, victim), "P10 a v5-versioned stream is refused");
        }
        {
            // Sprint 16, BL-572: the IMMEDIATE previous format (BL-571's v6,
            // the version this batch released before BL-572 bumped again). A
            // v6 stream simply ENDS where the offers/next_offer_id trailing
            // section now continues -- not a mid-record misalignment like
            // P10's, but the read still must not accept a stream that is
            // shorter than the reader now expects. Refused whole, same
            // contract as every prior bump.
            std::string bad = bytes_once;
            const uint32_t v6 = 6;
            std::memcpy(&bad[4], &v6, sizeof v6);
            check(!from_bytes(bad, victim), "P11 a v6-versioned stream is refused");
        }
        {
            // Sprint 16, BL-573: the IMMEDIATE previous format (BL-572's v7,
            // the version this batch released before BL-573 bumped again). A
            // v7 stream simply ENDS where the mercenary_contracts/
            // next_contract_id trailing section now continues -- same shape
            // as P11 one bump up. Refused whole, same contract as every prior
            // bump.
            std::string bad = bytes_once;
            const uint32_t v7 = 7;
            std::memcpy(&bad[4], &v7, sizeof v7);
            check(!from_bytes(bad, victim), "P12 a v7-versioned stream is refused");
        }
        {
            // BL-585: the IMMEDIATE previous format (Sprint 16's v8, the
            // version this batch released before BL-585 bumped again). A v8
            // stream's per-resource arrays are `resource_count`=38 long; a v9
            // reader expects 42 -- not a trailing gap or a mid-record shift
            // like the rows above, but every per-resource array in the WHOLE
            // stream reading four fields short of what follows. Refused
            // whole, same contract as every prior bump.
            std::string bad = bytes_once;
            const uint32_t v8 = 8;
            std::memcpy(&bad[4], &v8, sizeof v8);
            check(!from_bytes(bad, victim), "P13 a v8-versioned stream is refused");
        }
        {
            // BL-586 slice 2: the IMMEDIATE previous format (BL-585's v9, the
            // version this batch released before this slice bumped again). A
            // v9 stream's per-resource arrays are `resource_count`=42 long; a
            // v10 reader expects 47 -- the same structural class of move as
            // P13's v8/v9 gap, not a trailing gap or a mid-record shift.
            // Refused whole, same contract as every prior bump.
            std::string bad = bytes_once;
            const uint32_t v9 = 9;
            std::memcpy(&bad[4], &v9, sizeof v9);
            check(!from_bytes(bad, victim), "P14 a v9-versioned stream is refused");
        }
        {
            // v10 (BL-586 slice 2) is refused twice over after this wave: it has
            // no `land_use` section (BL-612 -- the bytes where v11+ expects it
            // read as the nation store's count) AND its nation record is one
            // w_f32 short of `qualification` (BL-613). Either alone misaligns
            // everything downstream. Refused whole, same contract as every
            // prior bump.
            std::string bad = bytes_once;
            const uint32_t v10 = 10;
            std::memcpy(&bad[4], &v10, sizeof v10);
            check(!from_bytes(bad, victim), "P15 a v10-versioned stream is refused");
        }
        {
            // v11 (BL-612's intermediate shape, this wave) still lacks the
            // province_anchor centre field (BL-611), the nation qualification
            // (BL-613) and the building wage_bid (BL-614) -- mid-record gaps
            // all three. Refused whole, same contract as every prior bump.
            // Refused whole, same contract as every prior bump.
            std::string bad = bytes_once;
            const uint32_t v11 = 11;
            std::memcpy(&bad[4], &v11, sizeof v11);
            check(!from_bytes(bad, victim), "P16 a v11-versioned stream is refused");
        }
        {
            // v12 (BL-611's intermediate shape, this wave) predates BL-613's
            // nation qualification and BL-614's building wage_bid -- both
            // mid-record gaps. Refused whole. (BL-613/BL-614 were numbered
            // 11/12 on their own branch; renumbered to 13/14 at integration.)
            std::string bad = bytes_once;
            const uint32_t v12 = 12;
            std::memcpy(&bad[4], &v12, sizeof v12);
            check(!from_bytes(bad, victim), "P17 a v12-versioned stream is refused");
        }
        {
            // v13 (BL-613's renumbered intermediate) still lacks wage_bid in
            // the building record -- one w_f32 short, mid-record. Refused
            // whole, same contract as every prior bump.
            std::string bad = bytes_once;
            const uint32_t v13 = 13;
            std::memcpy(&bad[4], &v13, sizeof v13);
            check(!from_bytes(bad, victim), "P18 a v13-versioned stream is refused");
        }
        {
            // v14 (the pre-BL-624 release format) still lacks the razed flag
            // in the population-centre record -- one w_int short, mid-record.
            // Refused whole, same contract as every prior bump.
            std::string bad = bytes_once;
            const uint32_t v14 = 14;
            std::memcpy(&bad[4], &v14, sizeof v14);
            check(!from_bytes(bad, victim), "P19 a v14-versioned stream is refused");
        }
        {
            // P19b -- v15, the last ACTUALLY RELEASED format, and the coverage
            // hole Sprint 20 wave 1's version stack opened: P19 pins 14 and P20
            // names `world_save_version - 1` (16 after the stack), so without
            // this row nothing asserts the one format real saves on disk carry.
            // A v15 corp record lacks BOTH of wave 1's additions -- no
            // `ownership_class` byte after `focus`, no `returns` run after
            // `produced_ever`. Refused whole.
            std::string bad = bytes_once;
            const uint32_t v15 = 15;
            std::memcpy(&bad[4], &v15, sizeof v15);
            check(!from_bytes(bad, victim), "P19b a v15-versioned stream is refused");
        }
        {
            // P20 (R5) -- the IMMEDIATELY PRIOR format, named through the constant
            // rather than by number. Sprint 20 wave 1 put TWO changes in the corp
            // record and this one row covers both:
            //   BL-631: `ownership_class`, one enum byte after `focus` -- a
            //     MID-RECORD gap, so a reader that accepted an older stream would
            //     misread starting_capital / balance / is_player / is_background in
            //     every corp, and everything serialised after `corporations` besides.
            //   BL-626: `returns`, a count-prefixed run after `produced_ever` -- an
            //     older corp record simply ENDS there, so every container written
            //     after the corporations reads misaligned from the first one onward.
            // Refused whole, same strict-equality contract as every prior bump.
            //
            // SYMBOLIC ON PURPOSE. Two slices claimed a version from separate
            // worktrees in one wave; a literal here would have had to be re-pinned
            // at the integration merge, which makes the check the thing that gets
            // re-blessed. `world_save_version - 1` is true of whatever number the
            // stack settles on -- and it is why this merge cost no assertion.
            std::string bad = bytes_once;
            const uint32_t prev = world_save_version - 1u;
            std::memcpy(&bad[4], &prev, sizeof prev);
            check(!from_bytes(bad, victim),
                  "P20 a stream at world_save_version - 1 is refused");
        }
        {
            // Truncated mid-way through the tile store -- far enough in that a
            // naive reader would already have replaced several containers.
            const std::string bad = bytes_once.substr(0, bytes_once.size() / 2);
            check(!from_bytes(bad, victim), "P3 a truncated stream is rejected");
        }
        {
            check(!from_bytes(std::string(), victim), "P3 an empty stream is rejected");
        }

        check(victim.player_entity == keep_player && victim.home_body == keep_home
                  && victim.next_order_id == keep_order && victim.tiles.empty()
                  && victim.bodies.empty(),
              "P3 every rejected load left the destination world untouched");
        check(victim.nations.empty() && victim.nation_budgets.empty()
                  && victim.province_holder.empty(),
              "P9 the v4 refusal left nations, nation_budgets and province_holder untouched");
        check(victim.mercenary_offers.empty() && victim.next_offer_id == 1,
              "P11 every rejected load left mercenary_offers and its id counter untouched");
    }

    // -----------------------------------------------------------------------
    // P4 (R3) -- derived caches are cleared on load and rebuild identically
    // -----------------------------------------------------------------------
    {
        check(loaded.body_tile_index.empty() && loaded.astar_cost_cache.empty()
                  && loaded.body_reach_cost.empty() && loaded.body_market_index.empty()
                  && loaded.body_market_index_count == 0
                  && loaded.body_market_index_max_id == null_entity,
              "P4 the loaded world starts with every derived cache cleared");

        check(loaded.ai_decisions.entries.empty() && loaded.ai_decisions.total == 0,
              "P4 the AI decision ring is not resurrected across a load");

        // Rebuild the two expensive ones on both sides and compare. `w`'s caches
        // may already be warm from generation, so clear it the same way a load
        // does -- otherwise this would compare a warm cache against a cold one
        // and prove nothing about the rebuild.
        world fresh_side = w;
        clear_derived_state(fresh_side);

        const entity_id home = loaded.home_body;
        const std::vector<entity_id>& grid_a = body_tile_grid(fresh_side, home);
        const std::vector<entity_id>& grid_b = body_tile_grid(loaded, home);
        check(grid_a == grid_b, "P4 body_tile_index rebuilds to identical contents");

        const std::vector<float>& reach_a = body_reach_field(fresh_side, home);
        const std::vector<float>& reach_b = body_reach_field(loaded, home);
        check(reach_a == reach_b, "P4 body_reach_cost rebuilds to identical contents");
        note("home reach field cells: ", (unsigned long long)reach_b.size());
    }

    // -----------------------------------------------------------------------
    // P5 (R3) -- corp_modifiers keeps its ORDER, which a re-fold cannot
    // -----------------------------------------------------------------------
    // The canonical world earns no modify_scalar tech, so this is built rather
    // than found: the property under test is about ORDER, and it is only visible
    // when a non-commuting pair is stored in a deliberate sequence.
    {
        world m = w;
        const entity_id corp = m.player_entity;

        // add-then-multiply. Reversed, the same two modifiers give a different
        // answer, which is exactly why the stored order is state and not a cache.
        m.corp_modifiers[corp] = {
            scalar_modifier{ modifier_subject::extraction_rate, modifier_op::add, 2.0f },
            scalar_modifier{ modifier_subject::extraction_rate, modifier_op::multiply, 3.0f },
        };
        // A second corp holding the reverse order, so the check cannot pass by
        // accident on a symmetric fold.
        entity_id other = null_entity;
        for (const auto& kv : m.corporations)
            if (kv.first != corp)
            {
                other = kv.first;
                break;
            }
        if (other != null_entity)
            m.corp_modifiers[other] = {
                scalar_modifier{ modifier_subject::extraction_rate, modifier_op::multiply, 3.0f },
                scalar_modifier{ modifier_subject::extraction_rate, modifier_op::add, 2.0f },
            };

        const float base   = 10.0f;
        const float want_a = m.modified_scalar(corp, modifier_subject::extraction_rate, base);
        const float want_b = (other == null_entity)
                                 ? 0.0f
                                 : m.modified_scalar(other, modifier_subject::extraction_rate, base);

        check(other == null_entity || want_a != want_b,
              "P5 the two orders really do give different answers (the test is not vacuous)");

        world back;
        const bool ok = from_bytes(to_bytes(m), back);
        check(ok, "P5 a world carrying scalar modifiers round-trips");

        const float got_a = ok ? back.modified_scalar(corp, modifier_subject::extraction_rate, base)
                               : -1.0f;
        const float got_b = (ok && other != null_entity)
                                ? back.modified_scalar(other, modifier_subject::extraction_rate, base)
                                : 0.0f;
        check(ok && got_a == want_a && got_b == want_b,
              "P5 modified_scalar returns the same value after a load, order intact");
        std::printf("     add-then-multiply=%.1f  multiply-then-add=%.1f\n", got_a, got_b);
    }

    // -----------------------------------------------------------------------
    // P6 (R4) -- a battle round-trips mid-fight and continues on the same stream
    // -----------------------------------------------------------------------
    // world.hpp said of `battles`: "NOT SERIALISED, deliberately... a save taken
    // mid-battle therefore drops the fight - acceptable while nothing can save
    // mid-tick, and the thing to revisit first if that changes." It changed
    // (Ben, 2026-08-22). This is the row that proves the revisit landed.
    {
        world b = w;

        campaign_battle_identity id;
        id.attacker   = 11;
        id.defender   = 22;
        id.province   = 7;
        id.tick       = 900;
        id.world_seed = 4242;

        const std::vector<army_stack_entry> att = {
            { 1, unit_class::infantry, 40, 0 },
            { 2, unit_class::cavalry, 12, 0 },
        };
        const std::vector<army_stack_entry> def = {
            { 1, unit_class::infantry, 35, 0 },
            { 3, unit_class::ranged, 10, 0 },
        };

        active_battle ab;
        ab.province       = id.province;
        ab.attacker       = id.attacker;
        ab.defender       = id.defender;
        ab.attacker_units = { 101, 102 };
        ab.defender_units = { 201 };
        ab.state = begin_campaign_battle(id, att, doctrine_row{}, def, doctrine_row{},
                                         terrain_substrate::sedimentary, terrain_cover::grass,
                                         150u, terrain_landform::plains, season::summer,
                                         1000, 1000);

        // Fight a round or two so the save is taken MID-fight, with the stream
        // already advanced -- a battle saved before its first round would not
        // test the thing this row is about.
        step_campaign_battle(ab.state);
        check(ab.state.end == campaign_battle_end::in_progress,
              "P6 the test battle is still in progress when the snapshot is taken");
        note("rng_state at save: ", (unsigned long long)ab.state.rng_state);

        b.battles.push_back(ab);

        world after;
        const bool ok = from_bytes(to_bytes(b), after);
        check(ok, "P6 a world carrying an in-progress battle round-trips");
        check(ok && after.battles.size() == 1, "P6 the battle survives the load");

        if (ok && after.battles.size() == 1)
        {
            const active_battle& r = after.battles[0];
            check(r.province == ab.province && r.attacker == ab.attacker
                      && r.defender == ab.defender && r.attacker_units == ab.attacker_units
                      && r.defender_units == ab.defender_units,
                  "P6 the battle's identity and unit membership survive");
            check(r.state.rng_state == ab.state.rng_state
                      && r.state.stream_seed == ab.state.stream_seed
                      && r.state.rounds_fought == ab.state.rounds_fought,
                  "P6 the RNG stream position and round count survive");

            // Continue both to conclusion. If the stream really was restored,
            // the two fights end identically -- same rounds, same losses.
            campaign_battle_state live  = ab.state;
            campaign_battle_state saved = after.battles[0].state;
            while (step_campaign_battle(live)) {}
            while (step_campaign_battle(saved)) {}

            const campaign_battle_outcome ol = campaign_battle_result(live);
            const campaign_battle_outcome os = campaign_battle_result(saved);
            check(ol.end == os.end && ol.result == os.result
                      && ol.rounds_fought == os.rounds_fought
                      && ol.attacker_losses_permille == os.attacker_losses_permille
                      && ol.defender_losses_permille == os.defender_losses_permille,
                  "P6 continuing the LOADED battle produces the same outcome as continuing the live one");
            std::printf("     both fights: %d rounds, attacker -%d permille, defender -%d permille\n",
                        os.rounds_fought, os.attacker_losses_permille, os.defender_losses_permille);
        }
    }

    // -----------------------------------------------------------------------
    // P7 (R1) -- container coverage, so P1 cannot pass vacuously
    // -----------------------------------------------------------------------
    // Every container the CANONICAL world actually populates must be non-empty on
    // the far side. A container this world leaves empty is reported rather than
    // asserted -- an honest "not covered here" beats a check that always passes.
    {
        struct row { const char* name; std::size_t before; std::size_t after; };
        const std::vector<row> rows = {
            { "bodies", w.bodies.size(), loaded.bodies.size() },
            { "tiles", w.tiles.size(), loaded.tiles.size() },
            { "buildings", w.buildings.size(), loaded.buildings.size() },
            { "stockpiles", w.stockpiles.size(), loaded.stockpiles.size() },
            { "markets", w.markets.size(), loaded.markets.size() },
            { "units", w.units.size(), loaded.units.size() },
            { "population_centres", w.population_centres.size(), loaded.population_centres.size() },
            { "population_centre_tile", w.population_centre_tile.size(), loaded.population_centre_tile.size() },
            { "population_centre_name", w.population_centre_name.size(), loaded.population_centre_name.size() },
            { "land_use", w.land_use.size(), loaded.land_use.size() },
            { "nations", w.nations.size(), loaded.nations.size() },
            { "nation_budgets", w.nation_budgets.size(), loaded.nation_budgets.size() },
            { "tile_to_nation", w.tile_to_nation.size(), loaded.tile_to_nation.size() },
            { "corporations", w.corporations.size(), loaded.corporations.size() },
            { "corp_body_pools", w.corp_body_pools.size(), loaded.corp_body_pools.size() },
            { "workforce_supply_overrides", w.workforce_supply_overrides.size(), loaded.workforce_supply_overrides.size() },
            { "sentiment", w.sentiment.pairs.size(), loaded.sentiment.pairs.size() },
            { "convoys", w.convoys.size(), loaded.convoys.size() },
            { "trade_routes", w.trade_routes.size(), loaded.trade_routes.size() },
            { "body_last_glimpse_tick", w.body_last_glimpse_tick.size(), loaded.body_last_glimpse_tick.size() },
            { "sell_orders", w.sell_orders.size(), loaded.sell_orders.size() },
            { "buy_orders", w.buy_orders.size(), loaded.buy_orders.size() },
            { "procurement_quotes", w.procurement_quotes.size(), loaded.procurement_quotes.size() },
            { "procurement_contracts", w.procurement_contracts.size(), loaded.procurement_contracts.size() },
            { "corp_embargo_conditions", w.corp_embargo_conditions.size(), loaded.corp_embargo_conditions.size() },
            { "corp_hostile_pairs", w.corp_hostile_pairs.size(), loaded.corp_hostile_pairs.size() },
            { "corp_friend_pairs", w.corp_friend_pairs.size(), loaded.corp_friend_pairs.size() },
            { "corp_friend_offers", w.corp_friend_offers.size(), loaded.corp_friend_offers.size() },
            { "earned_techs", w.earned_techs.size(), loaded.earned_techs.size() },
            { "laws", w.laws.size(), loaded.laws.size() },
            { "corp_modifiers", w.corp_modifiers.size(), loaded.corp_modifiers.size() },
            { "battles", w.battles.size(), loaded.battles.size() },
            { "history_log", w.history_log.size(), loaded.history_log.size() },
            { "provinces", w.provinces.provinces.size(), loaded.provinces.provinces.size() },
            { "provinces.tile_province", w.provinces.tile_province.size(), loaded.provinces.tile_province.size() },
            { "mercenary_offers", w.mercenary_offers.size(), loaded.mercenary_offers.size() },
            { "mercenary_contracts", w.mercenary_contracts.size(), loaded.mercenary_contracts.size() },
            { "exchanges", w.exchanges.size(), loaded.exchanges.size() },
        };

        bool all_match = true;
        int  covered = 0, vacuous = 0;
        std::printf("\n  container                      generated     loaded\n");
        for (const row& r : rows)
        {
            const bool same = r.before == r.after;
            all_match = all_match && same;
            if (r.before == 0)
                ++vacuous;
            else
                ++covered;
            std::printf("  %-28s %10llu %10llu%s%s\n", r.name,
                        (unsigned long long)r.before, (unsigned long long)r.after,
                        same ? "" : "   <-- MISMATCH", r.before == 0 ? "   (empty here)" : "");
        }
        std::printf("\n");
        check(all_match, "P7 every container has the same element count after the round trip");
        std::printf("     %d container(s) genuinely covered, %d empty in this world\n",
                    covered, vacuous);

        // The well-known entities and the id counters are single fields rather
        // than containers, and a snapshot that lost one would still pass P7's
        // table -- so they are named here explicitly.
        check(loaded.player_entity == w.player_entity && loaded.star_body == w.star_body
                  && loaded.home_body == w.home_body,
              "P7 the well-known entities survive");
        check(loaded.next_entity_id() == w.next_entity_id()
                  && loaded.next_convoy_id == w.next_convoy_id
                  && loaded.next_order_id == w.next_order_id
                  && loaded.next_procurement_id == w.next_procurement_id
                  && loaded.next_offer_id == w.next_offer_id
                  && loaded.next_contract_id == w.next_contract_id,
              "P7 every id counter survives, allocator cursor included");
        check(loaded.belt.inner_radius_au == w.belt.inner_radius_au
                  && loaded.belt.outer_radius_au == w.belt.outer_radius_au,
              "P7 the asteroid belt survives");
        note("next entity id: ", (unsigned long long)loaded.next_entity_id());

        // Sprint N3 T2: the EMPTINESS of this map is the inertness proof for
        // the national budget pass (world.hpp). A generated world has never
        // scored a nation, so the map must be empty on both sides -- and P7's
        // table would report it "(empty here)" without asserting it, which is
        // why it is named here.
        check(w.nation_budgets.empty() && loaded.nation_budgets.empty(),
              "P9 an EMPTY nation_budgets map round-trips as empty (the inertness state)");

        // BL-572: the same inertness proof one line over -- a generated world
        // has never funded a contracted_force share (NR-580: every nation's
        // treasury starts at 0.0), so derive_contract_offers has never run
        // against a live one, and this vector must be empty on both sides.
        check(w.mercenary_offers.empty() && loaded.mercenary_offers.empty(),
              "P11 an EMPTY mercenary_offers vector round-trips as empty (the inertness state)");

        // BL-573: same inertness proof one line over -- a generated world has
        // never had accept_offer issued against it, so this vector must be
        // empty on both sides too.
        check(w.mercenary_contracts.empty() && loaded.mercenary_contracts.empty(),
              "P12 an EMPTY mercenary_contracts vector round-trips as empty (the inertness state)");

        // BL-685: same inertness proof one line over -- generation never clears
        // a market, so a freshly generated world has never recorded an exchange
        // and this ring must be empty on both sides. The populated case is
        // P13 below and, through the outer IOSG file, save_envelope_roundtrip.
        check(w.exchanges.entries.empty() && loaded.exchanges.entries.empty()
                  && loaded.exchanges.next == 0 && loaded.exchanges.total == 0,
              "P12 an EMPTY exchange record round-trips as empty (the inertness state)");
    }

    // -----------------------------------------------------------------------
    // P13 (BL-685) -- the exchange record, POPULATED and WRAPPED
    // -----------------------------------------------------------------------
    // The ring's stored order stops being its chronological order once it wraps,
    // so `next` is state and not a derivable index. A reader that dropped it
    // would still pass every check above (an unwrapped ring has next == 0) and
    // would hand every consumer the retained history rotated at the wrap point.
    {
        world f = w;
        const std::size_t cap  = exchange_record_ring::capacity;
        const std::size_t over = cap + 9;
        for (std::size_t i = 0; i < over; ++i)
        {
            // Every field distinct from its neighbours and from the same field
            // one row over: a value of 0 round-trips even read into the wrong
            // member, which is exactly what this fixture exists to catch.
            exchange_record e;
            e.tick       = static_cast<int>(2000 + i);
            e.market     = static_cast<entity_id>(300 + i);
            e.resource   = static_cast<resource_type>(i % resource_count);
            e.quantity   = 1.25f + static_cast<float>(i);
            e.unit_price = 0.75f + static_cast<float>(i) * 3.0f;
            e.seller     = static_cast<entity_id>(700 + i);
            // null_entity is a LEGAL counterparty here (it means the market
            // itself), so the fixture carries both cases.
            e.buyer      = (i % 4 == 0) ? null_entity : static_cast<entity_id>(800 + i);
            f.exchanges.push(e);
        }

        std::stringstream s;
        write_world_snapshot(f, s);
        world back;
        check(read_world_snapshot(back, s), "P13 a world with a wrapped exchange record reads back");

        check(back.exchanges.entries.size() == cap && back.exchanges.next == over - cap
                  && back.exchanges.total == over,
              "P13 the ring's rows, wrap cursor and lifetime counter all survive");

        bool rows_match = true;
        for (std::size_t i = 0; i < f.exchanges.entries.size(); ++i)
        {
            const exchange_record& a = f.exchanges.entries[i];
            const exchange_record& b = back.exchanges.entries[i];
            rows_match = rows_match && a.tick == b.tick && a.market == b.market
                && a.resource == b.resource && a.quantity == b.quantity
                && a.unit_price == b.unit_price && a.seller == b.seller && a.buyer == b.buyer;
        }
        check(rows_match, "P13 every field of every row survives in STORED order");

        bool chronological = true;
        for (std::size_t i = 1; i < back.exchanges.size(); ++i)
            chronological = chronological
                && back.exchanges.oldest_first(i).tick
                       == back.exchanges.oldest_first(i - 1).tick + 1;
        check(chronological, "P13 the loaded ring reads chronologically across the wrap");
    }

    // -----------------------------------------------------------------------
    // P8 (R1) -- the containers a FRESH world leaves empty
    // -----------------------------------------------------------------------
    // P7 reports that roughly half the containers on `world` are empty in the
    // canonical world, which means P1 proved nothing about them. Those are also
    // the ones most likely to carry a bug, because they hold the intricate
    // records -- a nested set of strings, a condition_set inside a map, three
    // pair-keyed sets. So they are POPULATED here with deliberately distinctive
    // values and round-tripped on their own.
    //
    // Distinctive matters: a field written in the wrong order or read into the
    // wrong member still round-trips if every value is 0. Nothing below is 0.
    {
        world f = w;
        const entity_id c1 = 11, c2 = 22, c3 = 33, b1 = 44, b2 = 55;

        f.workforce_supply_overrides[{ c1, b1 }] = 7.25f;
        f.workforce_supply_overrides[{ c2, b2 }] = 1.5f;
        // BL-546: reputation is the TRUST dimension of the substrate now, and
        // the record carries a second float. Access and Trust are given
        // DIFFERENT values on purpose — equal ones round-trip even if the two
        // are read into each other, which is the defect this fixture exists to
        // catch.
        f.sentiment.pairs[{ c1, c2 }] = sentiment_value{ 2.25f, -0.75f };
        f.sentiment.pairs[{ c2, c1 }] = sentiment_value{ -6.5f, 3.5f };

        // Sprint N3 T2: two nations' weight vectors. Every weight is DISTINCT
        // and one (0.3f) is non-dyadic on purpose -- a reader that widened or
        // re-rounded a float would move it, and a reader that swapped two
        // lines would be caught by the distinct values. The two reserves differ
        // from every weight so a reserve read into a weight slot shows.
        const entity_id n1 = 66, n2 = 67;
        nation_budget   nb1;
        for (std::size_t i = 0; i < priority_count; ++i)
            nb1.weights[i] = 0.05f * static_cast<float>(i + 1);
        nb1.weights[static_cast<std::size_t>(budget_priority::public_exploration)] = 0.3f;
        nb1.reserve_fraction = 0.125f;
        nation_budget nb2;
        nb2.weights[static_cast<std::size_t>(budget_priority::contracted_force)] = 0.7f;
        nb2.weights[static_cast<std::size_t>(budget_priority::charters)]         = 0.3f;
        nb2.reserve_fraction = 0.875f;
        f.nation_budgets[n1] = nb1;
        f.nation_budgets[n2] = nb2;

        // BL-572: two offers from DIFFERENT clients, DISTINCT in every field --
        // a reader that swapped two same-typed members (target_province and
        // deadline are both plausible mixups; both are ints in the stream)
        // would show here. One offer's escrow already clears its fee (the
        // "ready to accept, nobody has yet" state); the other is still
        // filling, so both live shapes round-trip, not just one.
        f.mercenary_offers.push_back({ /*id*/ 12, /*client*/ n1, /*target_province*/ 4001,
                                       /*template_index*/ 0, /*fee*/ 400.0f, /*deadline*/ 999,
                                       /*issued_tick*/ 10, /*offer_escrow*/ 400.0f });
        f.mercenary_offers.push_back({ /*id*/ 13, /*client*/ n2, /*target_province*/ 4002,
                                       /*template_index*/ 1, /*fee*/ 500.0f, /*deadline*/ 1080,
                                       /*issued_tick*/ 20, /*offer_escrow*/ 125.5f });
        f.next_offer_id = 14;

        // BL-573: one open (still `active`) mercenary contract, with a
        // PARTIAL committed force (2 of the 8 slots) so both the populated
        // and the null_entity-padded ends of the fixed array round-trip.
        // Distinct from either offer above in every field, same discipline.
        mercenary_contract mc;
        mc.id = 21; mc.client = n1; mc.contractor = c2; mc.template_index = 0;
        mc.province = 4001; mc.fee = 400.0f; mc.deposit_paid = 100.0f;
        mc.deadline = 999; mc.accepted_tick = 15;
        mc.units[0] = c1; mc.units[1] = c3; // slots 2..7 stay null_entity
        mc.state = mercenary_contract_state::active;
        f.mercenary_contracts.push_back(mc);
        f.next_contract_id = 22;

        convoy_component cv;
        cv.source_market = 101; cv.dest_market = 202; cv.mode = convoy_mode::sea;
        cv.cargo_resource = resource_type::ordnance; cv.cargo_qty = 12.5f;
        cv.progress = 0.375f; cv.speed = 2.25f; cv.corp = c1; cv.arrived = false;
        cv.id = 9; cv.held = true; cv.cost_paid = 44.5f;
        f.convoys.push_back(cv);
        cv.mode = convoy_mode::space; cv.id = 10; cv.held = false; cv.arrived = true;
        f.convoys.push_back(cv);

        f.trade_routes.push_back({ b1, b2, c1, 1234, 7 });
        f.body_last_glimpse_tick[b1] = 555;
        f.body_last_glimpse_tick[b2] = -3; // negative on purpose: signedness bugs hide in ticks

        f.sell_orders.push_back({ 5, c1, b1, resource_type::steel, 30.0f, 2.5f });
        f.sell_orders.push_back({ 6, c2, b2, resource_type::propellant, 4.0f, 0.0f });
        f.buy_orders.push_back({ 7, c2, b1, resource_type::machinery, 8.0f, 99.5f, c1 });

        f.procurement_quotes.push_back({ 3, c1, c2, b1, b2, resource_type::alloys,
                                         15.0f, 3.25f, 6, 12.5f });
        f.procurement_contracts.push_back({ 4, c2, c1, b2, resource_type::electronics, b1,
                                            20.0f, 7.5f, 8, 3, 25.0f, 6.25f });

        condition_set embargo;
        embargo.all.push_back({ condition_subject::science, condition_comparator::greater_than,
                                42.5f, resource_type::coffee, building_type::launchpad,
                                "embargo-key" }); // province defaults to no_province — untouched
        // BL-570: the 7th positional field (province) round-trips too — a
        // real, non-default id, so byte-equality below is not just proving the
        // pre-existing 6 fields survive while a defaulted 7th rides along free.
        embargo.all.push_back({ condition_subject::province_held, condition_comparator::at_least,
                                1.0f, resource_type::furs, building_type::military_base, "",
                                4242u });
        f.corp_embargo_conditions[c3] = embargo;

        f.corp_hostile_pairs.insert({ c1, c2 });
        f.corp_hostile_pairs.insert({ c3, c1 });
        f.corp_friend_pairs.insert({ c1, c3 });
        f.corp_friend_offers.insert({ c2, c3 });

        f.earned_techs[c1] = { "rocketry", "metallurgy" };
        f.earned_techs[c2] = { "optics" };

        f.corp_modifiers[c1] = {
            { modifier_subject::logistics_cost, modifier_op::subtract, 0.125f },
            { modifier_subject::wage_floor, modifier_op::multiply, 1.75f },
        };

        law l;
        l.id = "levy-test"; l.name = "Test Levy"; l.conditions = embargo;
        l.effect = law_effect_kind::import_tariff; l.enacted = true;
        l.enacting_nation = 66; l.rate = 0.135f;
        l.scope_resource = static_cast<int>(resource_type::timber);
        f.laws.push_back(l);

        world back;
        const bool ok = from_bytes(to_bytes(f), back);
        check(ok, "P8 a world with every container populated round-trips");

        // Byte-equality of the re-serialisation is the fidelity proof, exactly as
        // in P1 -- but now over records P1 never reached.
        check(ok && to_bytes(f) == to_bytes(back),
              "P8 re-serialising reproduces the populated snapshot byte for byte");

        if (ok)
        {
            // A handful of values read back by hand, because byte-equality would
            // also hold if a pair of same-typed fields were consistently swapped
            // on both sides.
            check(back.convoys.size() == 2 && back.convoys[0].mode == convoy_mode::sea
                      && back.convoys[0].held && back.convoys[0].cargo_qty == 12.5f
                      && back.convoys[1].arrived && back.convoys[1].mode == convoy_mode::space,
                  "P8 convoy fields land in the right members");
            check(back.body_last_glimpse_tick.at(b2) == -3,
                  "P8 a negative glimpse tick survives (no unsigned round trip)");
            check(back.buy_orders.size() == 1 && back.buy_orders[0].preferred_seller == c1
                      && back.buy_orders[0].max_price == 99.5f,
                  "P8 the buy side keeps its preferred seller");
            check(back.procurement_contracts.size() == 1
                      && back.procurement_contracts[0].delivery_body == b1
                      && back.procurement_contracts[0].ticks_elapsed == 3,
                  "P8 a procurement contract keeps delivery body and elapsed ticks");
            const auto& e = back.corp_embargo_conditions.at(c3);
            check(e.all.size() == 2 && e.all[0].key == "embargo-key"
                      && e.all[0].subject == condition_subject::science
                      && e.all[1].structure == building_type::military_base,
                  "P8 a condition_set nested in a map survives, strings included");
            check(e.all[0].province == no_province && e.all[1].province == 4242u,
                  "P8 BL-570: condition::province round-trips (default AND a real id)");
            check(back.earned_techs.at(c1) == std::set<std::string>{ "metallurgy", "rocketry" }
                      && back.earned_techs.at(c2).count("optics") == 1,
                  "P8 the per-corp earned-tech string sets survive");
            check(back.corp_hostile_pairs.size() == 2 && back.corp_friend_pairs.size() == 1
                      && back.corp_friend_offers.count({ c2, c3 }) == 1
                      && back.corp_hostile_pairs.count({ c1, c2 }) == 1,
                  "P8 the three stance tables stay distinct and keep their directions");
            const law& lb = back.laws.back();
            check(lb.id == "levy-test" && lb.enacted && lb.rate == 0.135f
                      && lb.effect == law_effect_kind::import_tariff
                      && lb.conditions.all.size() == 2,
                  "P8 a law survives with its conditions and effect");
            check(back.workforce_supply_overrides.at({ c1, b1 }) == 7.25f
                      && back.sentiment.pairs.at({ c2, c1 }).trust == 3.5f
                      && back.sentiment.pairs.at({ c2, c1 }).access == -6.5f
                      && back.sentiment.pairs.at({ c1, c2 }).access == 2.25f,
                  "P8 the pair-keyed float tables keep key orientation, and "
                  "sentiment keeps Access and Trust distinct (BL-546)");

            // Sprint N3 T2: the weight map, slot by slot. Exact float equality
            // is the point -- the scorer's output must survive a save unchanged
            // or a loaded campaign spends by a vector nobody computed.
            bool budgets_exact = back.nation_budgets.size() == 2
                              && back.nation_budgets.count(n1) == 1
                              && back.nation_budgets.count(n2) == 1;
            if (budgets_exact)
            {
                const nation_budget& r1 = back.nation_budgets.at(n1);
                const nation_budget& r2 = back.nation_budgets.at(n2);
                for (std::size_t i = 0; i < priority_count; ++i)
                    budgets_exact = budgets_exact && r1.weights[i] == nb1.weights[i]
                                                  && r2.weights[i] == nb2.weights[i];
                budgets_exact = budgets_exact && r1.reserve_fraction == 0.125f
                                              && r2.reserve_fraction == 0.875f
                                              && r1.weights[4] == 0.3f
                                              && r2.weights[8] == 0.3f;
            }
            check(budgets_exact,
                  "P9 nation_budgets round-trips exactly: nine weights + reserve for two "
                  "nations, the non-dyadic 0.3f included");

            // BL-572: both offers, field by field -- including the fully-
            // funded one (offer_escrow == fee) so a reader that clamped or
            // re-derived escrow instead of storing it would show here.
            check(back.mercenary_offers.size() == 2 && back.next_offer_id == 14,
                  "P11 both mercenary_offers survive, allocator cursor included");
            if (back.mercenary_offers.size() == 2)
            {
                const mercenary_offer& r1 = back.mercenary_offers[0];
                const mercenary_offer& r2 = back.mercenary_offers[1];
                check(r1.id == 12 && r1.client == n1 && r1.target_province == 4001
                          && r1.template_index == 0 && r1.fee == 400.0f && r1.deadline == 999
                          && r1.issued_tick == 10 && r1.offer_escrow == 400.0f,
                      "P11 a fully-funded offer round-trips exactly");
                check(r2.id == 13 && r2.client == n2 && r2.target_province == 4002
                          && r2.template_index == 1 && r2.fee == 500.0f && r2.deadline == 1080
                          && r2.issued_tick == 20 && r2.offer_escrow == 125.5f,
                      "P11 a still-filling offer round-trips exactly, escrow < fee included");
            }

            // BL-573: the accepted contract, field by field -- the committed
            // force in particular, since that fixed array is this item's own
            // save-format addition and the one a reader most plausibly gets
            // the wrong length or the wrong padding value for.
            check(back.mercenary_contracts.size() == 1 && back.next_contract_id == 22,
                  "P12 the mercenary_contract survives, allocator cursor included");
            if (back.mercenary_contracts.size() == 1)
            {
                const mercenary_contract& rc = back.mercenary_contracts[0];
                check(rc.id == 21 && rc.client == n1 && rc.contractor == c2
                          && rc.template_index == 0 && rc.province == 4001
                          && rc.fee == 400.0f && rc.deposit_paid == 100.0f
                          && rc.deadline == 999 && rc.accepted_tick == 15
                          && rc.state == mercenary_contract_state::active,
                      "P12 the contract's scalar fields round-trip exactly");
                check(rc.units[0] == c1 && rc.units[1] == c3,
                      "P12 the committed force's populated slots survive");
                bool rest_null = true;
                for (std::size_t i = 2; i < mercenary_contract_max_units; ++i)
                    rest_null = rest_null && rc.units[i] == null_entity;
                check(rest_null,
                      "P12 the committed force's unused slots stay null_entity, not padding "
                      "leaked from a neighbouring record");
            }
        }
    }

    std::printf("\n");
    if (g_failures == 0)
        std::printf("SAVE ROUND TRIP OK - a snapshot restores the world it was taken from.\n");
    else
        std::printf("SAVE ROUND TRIP FAIL - %d failure(s).\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
