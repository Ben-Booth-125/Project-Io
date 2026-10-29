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
//   P5  R3  The nation tech state (BL-1273, v47) -- held set, pool, queue --
//           survives exactly; a queue entry outside the tree is refused.
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
#include "world/market_clearing.hpp"   // BL-1125: market_for_tile across the round trip
#include "world/nation_tech.hpp"     // BL-1273: P5, the nation tech state
#include "world/nation_budget.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"
#include "harness_params.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

/// BL-1437 (v60): the obsolete-flips section's bytes — a count, then (id, n, n rows).
std::size_t obsolete_section_bytes(const world& w)
{
    std::size_t n = 4;
    for (const auto& [nid, nc] : w.nations)
        if (!nc.roster_obsolete_flips.empty())
            n += 4 + 4 + 2 * nc.roster_obsolete_flips.size();
    return n;
}

/// v61 (refit to a newer type, 2026-10-27): the refit section's bytes — a count, then
/// (id, u16 target, i32 days) for every refitting unit.
std::size_t refit_section_bytes(const world& w)
{
    std::size_t n = 4;
    for (const auto& [uid, u] : w.units)
        if (unit_refitting(u))
            n += 4 + 2 + 4;
    return n;
}

/// v62 (BL-1427, fronts): the fronts section's bytes — a count and (id, key) for every
/// assigned unit, then a count and (nation, key, u8 order) for every stored order. The
/// stream's last section.
std::size_t fronts_section_bytes(const world& w)
{
    std::size_t n = 4 + 4 + 9 * w.front_orders.size();
    for (const auto& [uid, u] : w.units)
        if (u.front != null_entity)
            n += 4 + 4;
    return n;
}

/// BL-1443 (v63): the attacking-stack section after the fronts — a battle count, then
/// each battle's stack as (count, ids).
std::size_t stack_section_bytes(const world& w)
{
    std::size_t n = 4;
    for (const active_battle& b : w.battles)
        n += 4 + 4 * b.attacking_units.size();
    return n;
}

/// BL-1434 (v64): the designs section, the stream's last — three counts, then
/// (id, n, n ids) per designed nation, (id, 3 + 3 variants) per unit off base kit and
/// (id, 3 variants) per muster off base kit.
std::size_t designs_section_bytes(const world& w)
{
    std::size_t n = 12;
    for (const auto& [nid, nc] : w.nations)
        if (!nc.unit_design_kit.empty())
            n += 4 + 4 + 2 * nc.unit_design_kit.size();
    for (const auto& [uid, u] : w.units)
        if (!u.kit.is_base() || !u.refit_kit.is_base())
            n += 4 + 6;
    for (const muster_order& m : w.musters)
        if (!m.kit.is_base())
            n += 4 + 3;
    return n;
}

/// BL-1449 (v65): the air section, the stream's last since — six counts, then a record
/// per wing, superiority entry, reveal, hit-bank entry, air-designed nation and
/// damaged building.
std::size_t air_section_bytes(const world& w)
{
    std::size_t n = 24;
    for (const auto& [wid, wg] : w.wings)
        n += 4 + 4 + 4 + 2 + 16 + 3 + 1 + 4 + 4 * wg.provinces.size();
    n += 12 * w.air_superiority.size() + 8 * w.air_revealed.size() + 12 * w.air_hit_bank.size();
    for (const auto& [nid, nc] : w.nations)
        if (!nc.air_design_kit.empty())
            n += 4 + 4 + 2 * nc.air_design_kit.size();
    for (const auto& [bid, b] : w.buildings)
        if (b.condition_permille != 1000)
            n += 8;
    return n;
}

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

/// BL-1340: @p w as a PRE-v53 stream — the tile record before a road was an edge. The
/// base every old-stream row below truncates, since v47-v52 share that record and v53
/// does not.
std::string to_bytes_pre_road_edges(const world& w)
{
    std::ostringstream out(std::ios::binary);
    write_world_snapshot_pre_road_edges(w, out);
    return out.str();
}

/// BL-1407: @p w as a PRE-v56 stream at @p version carrying @p corps as its
/// corporation store — a real v54 / v55 stream for the loader's fold (R2).
std::string to_bytes_legacy(const world& w, const std::map<entity_id, legacy_corp_record>& corps,
                            uint32_t version)
{
    std::ostringstream out(std::ios::binary);
    write_world_snapshot_legacy(w, corps, out, version);
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
    // BL-1407: generation creates no corporation and the v56 stream carries none.
    // The fixture world's CHARTERS (never saved) are what a pre-v56 corporation
    // store is built from for the legacy-stream rows below.
    std::map<entity_id, legacy_corp_record> legacy_corps;
    for (const auto& [cid, ch] : w.gen_charters)
    {
        legacy_corp_record lc;
        lc.name             = ch.name;
        lc.home_nation      = ch.home_nation;
        lc.focus            = ch.focus;
        lc.starting_capital = ch.starting_capital;
        lc.is_background    = ch.is_background;
        legacy_corps[cid]   = lc;
    }

    std::printf("Generated world: %llu tiles, %llu bodies, %llu corps, %llu history entries\n\n",
                (unsigned long long)w.tiles.size(), (unsigned long long)w.bodies.size(),
                (unsigned long long)legacy_corps.size(),
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

    // BL-1101: the world's own recipe band (format v26). A `no_prehistory()`
    // world never runs the Industrialisation fold, so it leaves the field at
    // `any` (byte 0) and P1's byte-equality would pass over a reader that
    // dropped it. Pin it to the value the fold writes on a crossed history.
    w.campaign_band = era_band::industrial;

    // BL-1196 (households consume): the market record's two new arrays, the
    // household bid and fill the growth gate reads before the next clear. A
    // freshly generated world has never cleared, so both are zero everywhere
    // and P1's byte-equality would pass over a reader that dropped them. Pin
    // the lowest market's food slot to distinctive values (bid 12.5, fill 7.25).
    entity_id hh_market = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (hh_market == null_entity || mid < hh_market)
            hh_market = mid;
    const std::size_t hh_food = static_cast<std::size_t>(resource_type::food_rations);
    if (hh_market != null_entity)
    {
        w.markets.at(hh_market).household_bid[hh_food]  = 12.5f;
        w.markets.at(hh_market).household_fill[hh_food] = 7.25f;
        // BL-1203: the hauler's room fields (world_save_version 36).
        w.markets.at(hh_market).household_weight[hh_food] = 3.375f;
        w.markets.at(hh_market).hauler_want[hh_food]      = 9.625f;
        w.markets.at(hh_market).unposted_bid[hh_food]      = 4.875f; // BL-1227
        w.markets.at(hh_market).unposted_bid_tick[hh_food] = 1234;   // BL-1227

        w.markets.at(hh_market).background_fill[hh_food]     = 5.875f; // BL-1217 G1b R2
        w.markets.at(hh_market).trade_lp_spare               = 6.125f; // v46: rival Marketplace estimate
    }

    // BL-1266 (world_save_version 46): a MANUAL TRADE and a corporation's trade
    // reserve and points, pinned at distinctive values so the round trip is
    // checked by VALUE. The trade names a live corporation and two standing
    // markets (the reader rejects anything else).
    // BL-1407: the trade's owner is a NATION (the lowest id).
    entity_id trade_corp = null_entity, trade_m1 = null_entity, trade_m2 = null_entity;
    for (const auto& [cid, cc] : w.nations)
        if (trade_corp == null_entity || cid < trade_corp)
            trade_corp = cid;
    {
        std::vector<entity_id> mids;
        for (const auto& [mid, mc] : w.markets)
            mids.push_back(mid);
        std::sort(mids.begin(), mids.end());
        if (mids.size() >= 2)
        {
            trade_m1 = mids[0];
            trade_m2 = mids[1];
        }
    }
    if (trade_corp != null_entity && trade_m2 != null_entity)
    {
        w.trades.push_back({ w.allocate_trade_id(), trade_corp, resource_type::steel,
                             trade_m1, trade_m2, 2.625f });
        w.nations.at(trade_corp).trade_reserve = 1.75f; // BL-1407: the v56 nation books
        w.nations.at(trade_corp).trade_points  = 3.125f;
    }

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
    // BL-1407: the ownership class retired with the corporation record.

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
    // BL-1407: the returns are a NATION's (v56 nation books); the lowest id files.
    entity_id filing_corp = null_entity;
    for (const auto& [cid, cc] : w.nations)
        if (filing_corp == null_entity || cid < filing_corp)
            filing_corp = cid;
    if (filing_corp != null_entity)
    {
        std::vector<quarterly_return>& hist = w.nations.at(filing_corp).returns;
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
            r.taxes       = 2.5f * k;              // BL-1407: carried in the v56 books
            r.net        += (q == 1) ? 1.0e-9 : 0.0; // an exact net its f32 slot cannot hold
            hist.push_back(r);
        }
        w.nations.at(filing_corp).refund_unbooked = 12.25f;
        w.nations.at(filing_corp).refund_opening  = 48.5f;
        // BL-1099 (format v27): the firm's origin pair, two int32 at the
        // record's tail after `returns`. Generation stamps them only through
        // the charter walk, which this harness's fixture world never runs, so
        // hand-stamp distinct non-default values on the same corp: the
        // defaults (0 / -1) would let a dropped pair pass P1 unnoticed.
    }

    // world_save_version 41 (propellant routes follow the body's air, Ben
    // 2026-10-09): `body_component::atmosphere`, copied from each body's
    // generated profile. Census first: the generated world must carry BOTH an
    // airless body and one with air, or the copy never ran (every body would
    // sit at the `moderate` default) and P1's byte-equality would pass over a
    // dropped byte on a uniform column.
    int atmos_by_class[4] = {0, 0, 0, 0};
    int airless_bodies = 0, aired_bodies = 0;
    for (const auto& [bid, bc] : w.bodies)
    {
        if (bc.type == body_type::star)
            continue; // no surface, never generated
        ++atmos_by_class[static_cast<int>(bc.atmosphere) & 3];
        if (atmosphere_is_airless(bc.atmosphere)) ++airless_bodies; else ++aired_bodies;
    }
    std::printf("     bodies by atmosphere (star excluded): none %d  thin %d  moderate %d  thick %d"
                "  -> airless %d, with air %d\n",
                atmos_by_class[0], atmos_by_class[1], atmos_by_class[2], atmos_by_class[3],
                airless_bodies, aired_bodies);
    check(airless_bodies > 0 && aired_bodies > 0,
          "P1 v41: the generated world copies each body's atmosphere (an airless body AND one with air)");

    // BL-1404 (v55): MAJORS ARE A SAVED FACT. The legacy roster marks every
    // nation home to a specialist a major; DEMOTE the lowest such nation (its
    // specialist stays, as `reconcile_majors`' TOO MANY leaves it), so the saved
    // fact and the champion derivation DISAGREE on one nation and P1 cannot pass
    // over a reader that re-derived instead of reading.
    entity_id demoted_nation = null_entity;
    int majors_saved = 0;
    for (const auto& [nid, nc] : w.nations)
        if (nc.is_major && (demoted_nation == null_entity || nid < demoted_nation))
            demoted_nation = nid;
    if (demoted_nation != null_entity)
        w.nations.at(demoted_nation).is_major = false;
    for (const auto& [nid, nc] : w.nations)
        majors_saved += nc.is_major ? 1 : 0;
    check(demoted_nation != null_entity && majors_saved > 0,
          "P21 the fixture holds a demoted nation with a specialist and at least one major (not vacuous)");

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

    // v41: every body's atmosphere survives by VALUE.
    {
        bool same = read_ok && loaded.bodies.size() == w.bodies.size();
        for (const auto& [bid, bc] : w.bodies)
        {
            const auto lit = loaded.bodies.find(bid);
            same = same && lit != loaded.bodies.end() && lit->second.atmosphere == bc.atmosphere;
        }
        check(same, "P1 v41: body_component::atmosphere round-trips by value on every body");
    }

    // BL-613: the qualification fraction survives by VALUE, not just by byte
    // agreement of the two writes.
    if (qual_nation != null_entity)
    {
        const auto nit = loaded.nations.find(qual_nation);
        check(read_ok && nit != loaded.nations.end()
                  && nit->second.qualification == 0.375f,
              "P1 nation qualification (BL-613) round-trips at its written value");
    }

    // BL-1404 (v55): the majors survive by VALUE — the demoted nation stays a
    // minor though it still holds its specialist, and every other major stays one.
    {
        bool same = read_ok && loaded.nations.size() == w.nations.size();
        for (const auto& [nid, nc] : w.nations)
        {
            const auto lit = loaded.nations.find(nid);
            same = same && lit != loaded.nations.end() && lit->second.is_major == nc.is_major
                && is_major_nation(loaded, nid) == nc.is_major;
        }
        check(same && demoted_nation != null_entity && !is_major_nation(loaded, demoted_nation),
              "P21 is_major (BL-1404, v55) round-trips by value; a demoted nation stays a minor");
        check(read_ok && loaded.state_hash(0) == w.state_hash(0),
              "P21 the reloaded world's state_hash (which folds the majors) equals the saved one");
        world flipped = w;
        if (demoted_nation != null_entity)
            flipped.nations.at(demoted_nation).is_major = true;
        check(flipped.state_hash(0) != w.state_hash(0),
              "P21 state_hash sees a major change (the sparse majors fold)");
    }
    // BL-1404: a v54 stream (this one less its majors' section, stamped 54)
    // reads, with the majors DERIVED from the champions — the demoted nation
    // holds a specialist, so it reads back a major, as every v54 world did.
    {
        // BL-1407: a REAL v54 stream (the legacy writer), its corporation store
        // built from the fixture's charters; the loader derives its majors from
        // those specialists before it folds them away.
        const std::string v54 = to_bytes_legacy(w, legacy_corps, k_world_save_version_pre_major);
        world old;
        bool ok = from_bytes(v54, old);
        for (const auto& [nid, nc] : old.nations)
        {
            bool home = false;
            for (const auto& [cid, cc] : legacy_corps)
                home = home || (!cc.is_background && cc.home_nation == nid);
            ok = ok && nc.is_major == home;
        }
        check(ok && demoted_nation != null_entity && is_major_nation(old, demoted_nation),
              "P21 a v54 stream (the v55 prefix) reads, its majors derived from its champions");
        // A v55 stream naming a nation that is not in it is refused whole.
        // A v55 stream (the legacy writer: the majors are its LAST section).
        std::string bad = to_bytes_legacy(w, legacy_corps, k_world_save_version_pre_books);
        if (world_save_major_section_bytes(w) > 4)
        {
            const entity_id ghost = 0x7FFFFFF0u;
            std::memcpy(&bad[bad.size() - sizeof ghost], &ghost, sizeof ghost);
        }
        world sentinel;
        check(world_save_major_section_bytes(w) > 4 && !from_bytes(bad, sentinel) && sentinel.nations.empty(),
              "P21 a majors' section naming no nation in the stream is REFUSED, destination untouched");
    }

    // -----------------------------------------------------------------------
    // P23 (Ben, 2026-10-24; PRODUCTION.md § Construction is measured in days) --
    // the v57 construction-days section round-trips by value, and a v56 stream
    // reads with each site's days DERIVED from its ticks (x 90).
    {
        world wd = w;
        std::vector<entity_id> bids;
        for (const auto& [bid, b] : wd.buildings) bids.push_back(bid);
        std::sort(bids.begin(), bids.end());
        bool have = bids.size() >= 2;
        if (have)
        {
            building_component& site = wd.buildings.at(bids[0]);
            site.ticks_remaining = 2; site.days_remaining = 150; site.build_rate_permille = 600;
            site.build_paid_days = 40; site.build_day_milli = 321; site.construction_progress = 0.0f;
            site.build_credit_paid = 1234.5f; site.build_arrears_milli = 12345; // Ben, 2026-10-25
            building_component& done = wd.buildings.at(bids[1]);
            done.ticks_remaining = 0; done.days_remaining = 0; done.completion_day = 12;
            done.output_share_permille = 500;
            done.build_arrears_milli = 700; // completed owing materials: still owed
            wd.construction_quarter_day = 37;
        }
        const std::string b57 = to_bytes(wd);
        world back;
        bool ok = have && from_bytes(b57, back) && back.construction_quarter_day == 37;
        if (ok)
        {
            const building_component& s2 = back.buildings.at(bids[0]);
            const building_component& d2 = back.buildings.at(bids[1]);
            ok = s2.ticks_remaining == 2 && s2.days_remaining == 150 && s2.build_rate_permille == 600
              && s2.build_paid_days == 40 && s2.build_day_milli == 321
              && d2.completion_day == 12 && d2.output_share_permille == 500
              && s2.build_credit_paid == 1234.5f && s2.build_arrears_milli == 12345 && d2.build_arrears_milli == 700
              && back.state_hash(7) == wd.state_hash(7) && to_bytes(back) == b57;
        }
        check(ok, "P23 v57: a site's days, rate, paid days, sub-day progress, a completion day, "
                  "an output share, credits paid at placement, materials owed and the day cursor round-trip by value (hash and bytes equal)");

        // A v56 stream: the same world less the section. Days derived from ticks.
        world w56 = wd;
        if (have)
        {
            building_component& site = w56.buildings.at(bids[0]);
            site.days_remaining = 0; site.build_rate_permille = 0; site.build_paid_days = 0;
            site.build_day_milli = 0; site.construction_progress = 0.25f; // a quarter of a quarter done
            site.build_credit_paid = 0.0f; site.build_arrears_milli = 0;
            building_component& done = w56.buildings.at(bids[1]);
            done.completion_day = 0; done.output_share_permille = 1000; done.build_arrears_milli = 0;
            w56.construction_quarter_day = 0;
        }
        std::string b56;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_days(w56, o);
            b56 = o.str();
        }
        uint32_t stamped = 0;
        std::memcpy(&stamped, &b56[4], sizeof stamped);
        world old;
        bool ok56 = have && stamped == k_world_save_version_pre_days && from_bytes(b56, old);
        if (ok56)
        {
            const building_component& s3 = old.buildings.at(bids[0]);
            ok56 = s3.ticks_remaining == 2 && s3.days_remaining == 2 * 90 - 22
                && old.construction_quarter_day == 0;
            for (const auto& [bid, b] : old.buildings)
                ok56 = ok56 && (b.ticks_remaining > 0
                                    ? (b.days_remaining + 89) / 90 == b.ticks_remaining
                                    : b.days_remaining == 0);
        }
        check(ok56, "P23 a v56 stream reads, each site's days derived from its ticks (2 qtrs less "
                    "a quarter of one = 158 days), the cursor 0");
        std::size_t rows = 0; // the buildings the v57 section carries
        for (const auto& [bid, b] : wd.buildings)
            rows += canonical_construction_days(b).any() ? 1 : 0;
        // BL-1421 (v58): today's stream carries the unit combat-state section AFTER
        // the days, 4 bytes + 16 a unit, so the days section ends that far from the tail.
        // BL-1435 (v59): the muster section follows it — next id and count, 8 bytes, 22 a row.
        const std::size_t unit_section = 4 + 16 * wd.units.size() + 8 + 22 * wd.musters.size()
                                       + obsolete_section_bytes(wd)  // BL-1437 (v60)
                                       + refit_section_bytes(wd)     // v61
                                       + fronts_section_bytes(wd)    // BL-1427 (v62)
                                       + stack_section_bytes(wd)     // BL-1443 (v63)
                                       + designs_section_bytes(wd)   // BL-1434 (v64)
                                       + air_section_bytes(wd)       // BL-1449 (v65)
                                       + world_save_battle_carry_section_bytes(wd); // BL-1453 (v67), last
        check(have && b57.size() == b56.size() + 8 + 36 * rows + unit_section && rows >= 2,
              "P23 v56 is today's stream less the days section (8 bytes + 36 a row) and the unit section");

        // Untrusted: a cursor past the quarter is refused whole.
        std::string bad = b57;
        if (have && b57.size() >= 8 + 36 * rows + unit_section)
        {
            const int32_t past = 91;
            std::memcpy(&bad[b57.size() - unit_section - 8 - 36 * rows], &past, sizeof past);
        }
        world sentinel;
        check(have && !from_bytes(bad, sentinel) && sentinel.buildings.empty(),
              "P23 a cursor past day 90 is REFUSED, destination untouched");
    }

    // -----------------------------------------------------------------------
    // P24 (BL-1421, requirement R6) -- the v58 unit combat state round-trips by
    // value, a v57 stream reads with full readiness and morale, and a value out
    // of range is refused whole.
    // -----------------------------------------------------------------------
    {
        world wu = w;
        std::vector<entity_id> uids;
        for (const auto& [uid, u] : wu.units) uids.push_back(uid);
        std::sort(uids.begin(), uids.end());
        const bool have = !uids.empty();
        if (have)
        {
            unit_component& u = wu.units.at(uids[0]);
            u.establishment      = u.count + 17;
            u.readiness_permille = 333;
            u.morale_permille    = 777;
        }
        const std::string b58 = to_bytes(wu);
        world back;
        bool ok = have && from_bytes(b58, back);
        if (ok)
        {
            const unit_component& r = back.units.at(uids[0]);
            ok = r.establishment == wu.units.at(uids[0]).establishment
              && r.readiness_permille == 333 && r.morale_permille == 777
              && back.state_hash(3) == wu.state_hash(3) && to_bytes(back) == b58;
        }
        check(ok, "P24 a unit's establishment, readiness and morale round-trip by value (hash and bytes equal)");

        std::string b57;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_combat(wu, o);
            b57 = o.str();
        }
        uint32_t stamped = 0;
        std::memcpy(&stamped, &b57[4], sizeof stamped);
        world old;
        bool ok57 = have && stamped == k_world_save_version_pre_combat && from_bytes(b57, old);
        if (ok57)
            for (const auto& [uid, u] : old.units)
                ok57 = ok57 && u.establishment == 0 && u.readiness_permille == 1000
                    && u.morale_permille == 1000;
        check(ok57, "P24 a v57 stream reads, its units with no establishment and full readiness and morale");
        const std::size_t muster_section = 8 + 22 * wu.musters.size() // BL-1435 (v59), after the units
                                         + obsolete_section_bytes(wu)  // BL-1437 (v60)
                                         + refit_section_bytes(wu)     // v61
                                         + fronts_section_bytes(wu)    // BL-1427 (v62)
                                         + stack_section_bytes(wu)     // BL-1443 (v63)
                                         + designs_section_bytes(wu)   // BL-1434 (v64)
                                         + air_section_bytes(wu)       // BL-1449 (v65)
                                         + world_save_battle_carry_section_bytes(wu); // BL-1453 (v67), last
        check(have && b58.size() == b57.size() + 4 + 16 * wu.units.size() + muster_section,
              "P24 v57 is v58 less its section (4 bytes + 16 a unit)");

        // Untrusted: readiness past 1000 is refused whole. The first row's
        // readiness sits 8 bytes into it (after the id and the establishment).
        std::string bad = b58;
        const std::size_t row0 = b58.size() - muster_section - 16 * wu.units.size();
        if (have)
        {
            const int32_t over = 1001;
            std::memcpy(&bad[row0 + 8], &over, sizeof over);
        }
        world sentinel;
        check(have && !from_bytes(bad, sentinel) && sentinel.units.empty(),
              "P24 a readiness past 1000 is REFUSED, destination untouched");
    }

    // -----------------------------------------------------------------------
    // P22 (BL-1407, requirement R2) -- A REAL v54 STREAM LOADS INTO THE
    // NATION-OWNED WORLD. The legacy writer produces a v54 stream whose
    // corporation store holds firms that OWN buildings, money, a reserve, a unit,
    // a trade and the seat; the loader folds them on read and discards them.
    // -----------------------------------------------------------------------
    {
        // Two nations: the firm's HOME, and the nation whose ground its building
        // stands on (the same nation when the fixture has only one with ground).
        std::vector<entity_id> nids;
        for (const auto& [nid, nc] : w.nations)
            if (!nc.state_assets.empty())
                nids.push_back(nid);
        std::sort(nids.begin(), nids.end());
        bool built = nids.size() >= 1;
        world legacy_w = w;
        std::map<entity_id, legacy_corp_record> firms;
        entity_id firm_id = null_entity, moved = null_entity, home = null_entity, ground = null_entity;
        entity_id unit_id = null_entity;
        float home_before = 0.0f, home_reserve_before = 0.0f;
        if (built)
        {
            ground = nids.front();
            home   = nids.back();
            // Take the ground nation's first building out of its list: in a v54
            // world a firm, not the nation, held it.
            std::vector<entity_id>& sa = legacy_w.nations.at(ground).state_assets;
            moved = sa.front();
            sa.erase(sa.begin());
            firm_id = legacy_w.create_entity();
            legacy_corp_record lc;
            lc.name          = "Legacy Holdings";
            lc.home_nation   = home;
            lc.balance       = 321.5f;
            lc.trade_reserve = 4.25f;
            lc.is_player     = true;
            lc.assets.push_back(moved);
            firms[firm_id] = lc;
            legacy_w.player_entity = firm_id; // a v54 seat named its corporation
            // A unit the firm raised, with no muster base: it goes home.
            unit_id = legacy_w.create_entity();
            unit_component u{};
            u.owner = firm_id;
            u.count = 7;
            u.position = legacy_w.buildings.at(moved).tile;
            legacy_w.units[unit_id] = u;
            // The firm's trade.
            if (trade_m2 != null_entity)
                legacy_w.trades.push_back({ legacy_w.allocate_trade_id(), firm_id, resource_type::steel,
                                            trade_m1, trade_m2, 1.5f });
            // The returns' books do not exist at v54: clear the nation books the
            // fixture set, so the v54 world is one a v54 build could have written.
            for (auto& [nid, nc] : legacy_w.nations)
            {
                nc.trade_reserve = nc.trade_points = 0.0f;
                nc.refund_unbooked = nc.refund_opening = 0.0f;
                nc.returns.clear();
            }
            home_before         = legacy_w.nations.at(home).treasury;
            home_reserve_before = legacy_w.nations.at(home).trade_reserve;
        }
        const std::string v54 = built ? to_bytes_legacy(legacy_w, firms, k_world_save_version_pre_major)
                                      : std::string();
        world got;
        const bool ok = built && from_bytes(v54, got);
        check(ok, "P22 a real v54 stream with a building-holding corporation reads (BL-1407 R2)");
        if (ok)
        {
            const entity_id ground_now = owner_nation_of(got, moved);
            check(ground_now == ground,
                  "P22 the firm's building joined the nation whose ground it stands on");
            check(got.nations.at(home).treasury == home_before + 321.5f
                      && got.nations.at(home).trade_reserve == home_reserve_before + 4.25f,
                  "P22 the firm's balance and trade reserve went to its home treasury, exactly");
            check(got.units.count(unit_id) == 1 && got.units.at(unit_id).owner == home,
                  "P22 the firm's unit is its home nation's");
            bool trade_ok = true;
            for (const standing_trade& t : got.trades)
                trade_ok = trade_ok && got.nations.count(t.owner) != 0;
            check(trade_ok, "P22 every standing trade is a nation's after the load");
            check(got.player_entity == home,
                  "P22 the v54 seat (a corporation) loads as its home NATION");
            check(is_major_nation(got, home),
                  "P22 the v54 majors are derived from the stream's specialists");
            // A v56 re-save carries none of it: the stream re-reads equal.
            world again;
            check(from_bytes(to_bytes(got), again) && to_bytes(again) == to_bytes(got),
                  "P22 the folded world writes a v56 stream that re-reads byte for byte");
        }
    }

    // BL-1101: the band survives by VALUE — a save opens on the band its world
    // earned, which is the whole reason the field is persisted.
    check(read_ok && loaded.campaign_band == era_band::industrial,
          "P1 world campaign_band (BL-1101) round-trips at its written value");

    // BL-1196: the household bid and fill survive by VALUE (world_save_version 35; first claimed as 32).
    if (hh_market != null_entity)
    {
        const auto mit = loaded.markets.find(hh_market);
        check(read_ok && mit != loaded.markets.end()
                  && mit->second.household_bid[hh_food] == 12.5f
                  && mit->second.household_fill[hh_food] == 7.25f,
              "P1 market household_bid / household_fill (BL-1196) round-trip at their written values");
    }
    if (hh_market != null_entity)
    {
        const auto mit = loaded.markets.find(hh_market);
        check(read_ok && mit != loaded.markets.end()
                  && mit->second.household_weight[hh_food] == 3.375f
                  && mit->second.hauler_want[hh_food] == 9.625f,
              "P1 market household_weight / hauler_want (BL-1203) round-trip at their written values");
    }
    if (hh_market != null_entity)
    {
        const auto mit = loaded.markets.find(hh_market);
        check(read_ok && mit != loaded.markets.end()
                  && mit->second.unposted_bid[hh_food] == 4.875f
                  && mit->second.unposted_bid_tick[hh_food] == 1234,
              "P1 market unposted_bid / unposted_bid_tick (BL-1227, world_save_version 38) round-trip at their written values");
    }
    check(trade_m2 != null_entity && !w.trades.empty(),
          "P1 the fixture holds a manual trade, so the trade rows below are not vacuous (BL-1266)");
    if (trade_m2 != null_entity && !w.trades.empty())
    {
        check(read_ok && loaded.trades.size() == w.trades.size()
                  && loaded.trades.back().id == w.trades.back().id
                  && loaded.trades.back().owner == trade_corp
                  && loaded.trades.back().resource == resource_type::steel
                  && loaded.trades.back().from_market == trade_m1
                  && loaded.trades.back().to_market == trade_m2
                  && loaded.trades.back().points == 2.625f
                  && loaded.next_trade_id == w.next_trade_id,
              "P1 a manual trade (BL-1266, world_save_version 46) round-trips at its written values");
        check(read_ok && loaded.nations.at(trade_corp).trade_reserve == 1.75f
                  && loaded.nations.at(trade_corp).trade_points == 3.125f,
              "P1 a nation's trade_reserve / trade_points (BL-1407, v56 books) round-trip at their written values");

        // A trade naming a market that does not stand cannot have been written
        // (the seam refuses it): the reader refuses the stream whole.
        world orphan = w;
        orphan.trades.back().to_market = entity_id{0x7FFFFFF0u};
        world sink;
        check(!from_bytes(to_bytes(orphan), sink),
              "P1 a manual trade naming no standing market is rejected (BL-1266)");
        world orphan2 = w;
        orphan2.trades.back().owner = entity_id{0x7FFFFFF1u};
        check(!from_bytes(to_bytes(orphan2), sink),
              "P1 a manual trade whose owner is not a nation is rejected (BL-1266 / BL-1407)");
    }
    {
        const auto mit = loaded.markets.find(hh_market);
        check(read_ok && hh_market != null_entity && mit != loaded.markets.end()
                  && mit->second.background_fill[hh_food] == 5.875f,
              "P1 market background_fill (BL-1217 G1b R2, world_save_version 43) round-trips at its written value");
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
        const auto cit = loaded.nations.find(filing_corp);
        bool ok = read_ok && cit != loaded.nations.end()
               && cit->second.returns.size() == w.nations.at(filing_corp).returns.size();
        if (ok)
        {
            const auto& want = w.nations.at(filing_corp).returns;
            const auto& got  = cit->second.returns;
            for (std::size_t i = 0; i < want.size() && ok; ++i)
                ok = got[i].income == want[i].income
                  && got[i].expenditure == want[i].expenditure
                  && got[i].maintenance == want[i].maintenance
                  && got[i].wages == want[i].wages && got[i].interest == want[i].interest
                  && got[i].levies == want[i].levies && got[i].upkeep == want[i].upkeep
                  && got[i].net == want[i].net && got[i].balance == want[i].balance
                  && got[i].holdings == want[i].holdings
                  && got[i].book_value == want[i].book_value
                  && got[i].taxes == want[i].taxes;
        }
        check(ok, "P1 a nation's filed quarterly returns (BL-626 / BL-1407, v56 books) round-trip "
                  "by value, in order, the exact double net and the taxes included");
        check(read_ok && cit != loaded.nations.end() && cit->second.refund_unbooked == 12.25f
                  && cit->second.refund_opening == 48.5f,
              "P1 a nation's unbooked refund and its opening (BL-1206 / BL-1407) round-trip");
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
        victim.next_trade_id = 77;
        const entity_id keep_player = victim.player_entity;
        const entity_id keep_home   = victim.home_body;
        const uint32_t  keep_order  = victim.next_trade_id;

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
            // BL-1101 (format v26): the band byte sits directly after the
            // allocator cursors — magic (4) + version (4) + three ids (12) +
            // the belt's two floats (8) + four cursors (16) = offset 44. A
            // value past `industrial` cannot have been written, so the reader
            // refuses the whole stream rather than banding the world on a
            // roster nobody authored.
            std::string bad = bytes_once;
            bad[44] = static_cast<char>(static_cast<uint8_t>(era_band::industrial) + 1);
            check(!from_bytes(bad, victim), "P3 an out-of-range campaign_band byte (BL-1101) is rejected");
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
            // v6 stream simply ENDED where BL-572's trailing section began --
            // not a mid-record misalignment like P10's, but the read still
            // must not accept a stream shorter than the reader expects.
            // Refused whole, same contract as every prior bump. (BL-572's
            // payload was later torn out by cc88997c; this row asserts the
            // VERSION contract, which is unaffected.)
            std::string bad = bytes_once;
            const uint32_t v6 = 6;
            std::memcpy(&bad[4], &v6, sizeof v6);
            check(!from_bytes(bad, victim), "P11 a v6-versioned stream is refused");
        }
        {
            // Sprint 16, BL-573: the IMMEDIATE previous format (BL-572's v7,
            // the version this batch released before BL-573 bumped again). A
            // v7 stream simply ENDED where BL-573's trailing section began --
            // same shape as P11 one bump up. Refused whole, same contract as
            // every prior bump. Torn out by cc88997c, same note as P11.
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
            //
            // BL-1296 (v48): v47 is a STRICT PREFIX of v48 — state industry is
            // v48's trailing section and nothing else moved — so v47 is the one
            // predecessor still read (`k_world_save_version_pre_state`; the
            // envelope's prefix-compatible precedent). The newest REFUSED prior
            // format is therefore the one before it; P20b reads the v47 prefix.
            std::string bad = bytes_once;
            const uint32_t prev = k_world_save_version_pre_state - 1u;
            std::memcpy(&bad[4], &prev, sizeof prev);
            check(!from_bytes(bad, victim),
                  "P20 a stream at the newest unread prior version is refused");
        }
        // BL-1340 (v53) -- THE BASE OF EVERY OLD-STREAM ROW BELOW. Until v53 every
        // bump was a trailing section, so an older stream was the current one truncated
        // and relabelled. v53 widened the TILE RECORD (one road tier a tile became a
        // six-side mask), so a relabelled v53 stream is not a v47 stream at all: its tile
        // records are two bytes wider than a v47 reader expects. The pre-edge writer
        // (`write_world_snapshot_pre_road_edges`, verification-only) produces the v52
        // shape, which IS the shape v47-v52 share, and the trailing-section truncation
        // then works exactly as it always did off that base.
        const std::string pre_edge_once = to_bytes_pre_road_edges(w);
        {
            // P20b (BL-1296) -- the v47 stream: this v48 stream less its
            // state-industry section (a world with none writes two empty
            // counts, 8 bytes), stamped 47. It reads, as a world with no state
            // industry. Its own destination, so P3's untouched victim stays so.
            // BL-1255 (v49): the rail section follows it (a world with no rail
            // writes three empty counts, 12 more bytes), so the v47 prefix is 20 short.
            // BL-1309 (v50) and BL-1327 (v51) each add one more empty count after
            // it (share rules, space assets): 28 bytes in all. The reader ignores
            // trailing bytes, so a stale count here still "passed" -- it is exact now.
            // BL-1335 (v52): plus the tax section (count + 9 bytes a moved nation).
            // INTEGRATION (sprints 55 + 56): sliced off the PRE-EDGE stream, and with no
            // exact-net tail subtracted - that writer writes at v52 and the net section is
            // written only at `world_save_version`, so this stream ends before it.
            // BL-1407: every generated building is its nation's, so the v47 row
            // writes a copy whose nations hold none (a v47 world had no state
            // industry), keeping the 8-byte empty section the slice assumes.
            world stateless = w;
            for (auto& [nid, nc] : stateless.nations)
                nc.state_assets.clear();
            const std::string stateless_pre_edge = to_bytes_pre_road_edges(stateless);
            std::string v47 = stateless_pre_edge.substr(0, stateless_pre_edge.size() - 7 * sizeof(uint32_t)
                                                          - (world_save_tax_section_bytes(stateless)));
            const uint32_t pre = k_world_save_version_pre_state;
            std::memcpy(&v47[4], &pre, sizeof pre);
            world old;
            bool none = true;
            for (const auto& [nid, nc] : stateless.nations) // the copy holds none (8-byte section)
                none = none && nc.state_assets.empty();
            none = none && from_bytes(v47, old);
            for (const auto& [nid, nc] : old.nations)
                none = none && nc.state_assets.empty();
            check(none, "P20b a v47 stream (the v48 prefix) reads, with no state industry");
        }
        {
            // P20c (BL-1255) -- the v48 stream: this v49 stream less its rail
            // section (a world with no rail writes three empty counts, 12 bytes),
            // stamped 48. It reads, as a world with no rail. (+2 empty counts:
            // the v50 share rules and the v51 space assets.)
            // INTEGRATION (sprints 55 + 56): sliced off the PRE-EDGE stream, and with no
            // exact-net tail subtracted - that writer writes at v52 and the net section is
            // written only at `world_save_version`, so this stream ends before it.
            std::string v48 = pre_edge_once.substr(0, pre_edge_once.size() - 5 * sizeof(uint32_t)
                                                          - (world_save_tax_section_bytes(w))); // + BL-1335's v52 tax section
            const uint32_t pre = k_world_save_version_pre_rail;
            std::memcpy(&v48[4], &pre, sizeof pre);
            world old;
            const bool ok48 = w.rail_track.empty() && from_bytes(v48, old) && old.rail_track.empty();
            check(ok48, "P20c a v48 stream (the v49 prefix) reads, with no rail");
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
                  && victim.next_trade_id == keep_order && victim.tiles.empty()
                  && victim.bodies.empty(),
              "P3 every rejected load left the destination world untouched");
        check(victim.nations.empty() && victim.nation_budgets.empty()
                  && victim.province_holder.empty(),
              "P9 the v4 refusal left nations, nation_budgets and province_holder untouched");
    }

    // -----------------------------------------------------------------------
    // P4 (R3) -- derived caches are cleared on load and rebuild identically
    // -----------------------------------------------------------------------
    {
        check(loaded.body_tile_index.empty() && loaded.astar_cost_cache.empty()
                  && loaded.body_reach_cost.empty() && loaded.body_market_index.empty()
                  && loaded.body_market_index_count == 0
                  && loaded.body_market_index_cursor == 0,
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
    // P5 (R3) -- the nation tech state round-trips (BL-1273, format v47)
    // -----------------------------------------------------------------------
    // Replaces the BL-536 corp_modifiers ORDER row: corp_modifiers and the
    // per-corp earned set retired with the four tech gates (NR-1022). Built
    // rather than found: a freshly generated world has researched nothing.
    // Two nations, each with a held set, a pool (the head's in-progress
    // remainder) and a queue filled through `enqueue_node` itself; a third
    // nation left absent must stay absent.
    {
        world m = w;
        std::vector<entity_id> nids;
        for (const auto& kv : m.nations)
            nids.push_back(kv.first);
        std::sort(nids.begin(), nids.end());
        check(nids.size() >= 2, "P5 the canonical world has two nations to carry tech state");
        if (nids.size() >= 2)
        {
            const entity_id a = nids[0];
            const entity_id b = nids[1];
            nation_tech_state& sa = m.nation_tech[a];
            sa.held.set(static_cast<std::size_t>(io::dg::find_node("DG-PR-EX-T1")));
            sa.held.set(static_cast<std::size_t>(io::dg::find_node("DG-PR-EX-T2")));
            sa.pool_milli = 137250;
            enqueue_node(m, a, io::dg::find_node("DG-PR-EX-A"));
            enqueue_node(m, b, io::dg::find_node("DG-ST-AC-C"));
            m.nation_tech[b].pool_milli = 333; // a third of an RP, held exactly

            world back;
            const bool ok = from_bytes(to_bytes(m), back);
            check(ok, "P5 a world carrying nation tech state round-trips");
            bool same = ok && back.nation_tech.size() == m.nation_tech.size();
            if (same)
                for (const auto& [nid, st] : m.nation_tech)
                {
                    const auto it = back.nation_tech.find(nid);
                    same = same && it != back.nation_tech.end() && it->second.held == st.held
                        && it->second.pool_milli == st.pool_milli && it->second.queue == st.queue;
                }
            check(same, "P5 held set, pool and queue survive the load exactly, per nation");
            check(ok && !back.nation_tech.at(b).queue.empty()
                      && back.nation_tech.at(a).queue.size() == 3,
                  "P5 the queues are not vacuous (A1, A2, then the major; B's path)");
            std::printf("     nation %u: held %zu, pool %lld milli-RP, queue %zu | nation %u: queue %zu\n",
                        (unsigned)a, m.nation_tech.at(a).held.count(), static_cast<long long>(m.nation_tech.at(a).pool_milli),
                        m.nation_tech.at(a).queue.size(), (unsigned)b,
                        m.nation_tech.at(b).queue.size());

            // A corrupt queue entry (an index past the tree) refuses the stream
            // whole and leaves the destination untouched.
            world bad = m;
            bad.nation_tech[a].queue.push_back(static_cast<uint16_t>(io::dg::node_count));
            world dest = w;
            const uint64_t before = dest.state_hash(0);
            check(!from_bytes(to_bytes(bad), dest) && dest.state_hash(0) == before,
                  "P5 a queue entry outside the tree refuses the stream, destination untouched");
        }
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
        id.tile       = 777; // the defended tile (BL-1443)
        id.tick       = 900;
        id.world_seed = 4242;

        // BL-1421: the round fights combatants built from units each tick; the
        // saved battle carries the stream and the strength reading, and these
        // vectors stand in for the units on both sides of the load.
        auto combatant = [](entity_id u, land_class c, int men) {
            land_combatant x; x.unit = u; x.men = men; x.stats = land_class_stats(c); return x;
        };
        std::vector<land_combatant> att = { combatant(101, land_class::infantry, 400),
                                            combatant(102, land_class::light_armour, 120) };
        std::vector<land_combatant> def = { combatant(201, land_class::infantry, 350) };

        active_battle ab;
        ab.tile           = id.tile; // the identity: the defended tile (world_save_version 63)
        ab.province       = 7;       // derived at open; carried in the record's province slot
        ab.attacker       = id.attacker;
        ab.defender       = id.defender;
        ab.attacker_units = { 101, 102 };
        ab.defender_units = { 201 };
        ab.ground_tile    = 777; // BL-1145 review (world_save_version 30): the ground, stored at open
        ab.attacking_units = { 101 }; // BL-1443 (v63): the stack whose march made the attack
        ab.state = begin_campaign_battle(id, att, def, terrain_substrate::sedimentary,
                                         terrain_cover::grass, 150u, terrain_landform::plains);

        // Fight a round or two so the save is taken MID-fight, with the stream
        // already advanced -- a battle saved before its first round would not
        // test the thing this row is about.
        step_campaign_battle(ab.state, att, def);
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
            check(r.tile == ab.tile && r.province == ab.province && r.attacker == ab.attacker
                      && r.defender == ab.defender && r.attacker_units == ab.attacker_units
                      && r.defender_units == ab.defender_units,
                  "P6 the battle's identity and unit membership survive");
            check(r.ground_tile == ab.ground_tile,
                  "P6 the battle's ground tile survives (BL-1145 review, world_save_version 30)");
            check(r.attacking_units == ab.attacking_units,
                  "P6 the battle's attacking stack survives (BL-1443, world_save_version 63)");
            check(r.state.rng_state == ab.state.rng_state
                      && r.state.stream_seed == ab.state.stream_seed
                      && r.state.rounds_fought == ab.state.rounds_fought,
                  "P6 the RNG stream position and round count survive");

            // Continue both to conclusion. If the stream really was restored,
            // the two fights end identically -- same rounds, same losses.
            campaign_battle_state live  = ab.state;
            campaign_battle_state saved = after.battles[0].state;
            std::vector<land_combatant> la = att, ld = def, sa = att, sd = def;
            while (step_campaign_battle(live, la, ld)) {}
            while (step_campaign_battle(saved, sa, sd)) {}

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
            // BL-1407: `corporations` is deleted with the type; nothing to cover.

            { "workforce_supply_overrides", w.workforce_supply_overrides.size(), loaded.workforce_supply_overrides.size() },
            { "sentiment", w.sentiment.pairs.size(), loaded.sentiment.pairs.size() },
            { "convoys", w.convoys.size(), loaded.convoys.size() },
            { "trade_routes", w.trade_routes.size(), loaded.trade_routes.size() },
            { "body_last_glimpse_tick", w.body_last_glimpse_tick.size(), loaded.body_last_glimpse_tick.size() },
            { "trades", w.trades.size(), loaded.trades.size() },
            { "procurement_quotes", w.procurement_quotes.size(), loaded.procurement_quotes.size() },
            { "procurement_contracts", w.procurement_contracts.size(), loaded.procurement_contracts.size() },
            { "corp_embargo_conditions", w.corp_embargo_conditions.size(), loaded.corp_embargo_conditions.size() },
            { "corp_hostile_pairs", w.corp_hostile_pairs.size(), loaded.corp_hostile_pairs.size() },
            { "corp_friend_pairs", w.corp_friend_pairs.size(), loaded.corp_friend_pairs.size() },
            { "corp_friend_offers", w.corp_friend_offers.size(), loaded.corp_friend_offers.size() },
            { "nation_tech", w.nation_tech.size(), loaded.nation_tech.size() }, // BL-1273
            { "laws", w.laws.size(), loaded.laws.size() },
            { "battles", w.battles.size(), loaded.battles.size() },
            { "history_log", w.history_log.size(), loaded.history_log.size() },
            { "provinces", w.provinces.provinces.size(), loaded.provinces.provinces.size() },
            { "provinces.tile_province", w.provinces.tile_province.size(), loaded.provinces.tile_province.size() },
            { "exchanges", w.exchanges.size(), loaded.exchanges.size() },
            { "folded_markets", w.folded_markets.size(), loaded.folded_markets.size() }, // BL-1125
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

        // BL-1125: the fold map is routing state, so the round trip must route
        // every tile to the same market it routed to before the save.
        {
            std::size_t tiles = 0, differ = 0;
            for (const auto& [tid, tc] : w.tiles)
            {
                ++tiles;
                if (market_for_tile(w, tid) != market_for_tile(loaded, tid)) ++differ;
            }
            std::printf("     fold map: %zu folded market(s); %zu of %zu tiles route differently after the load\n",
                        w.folded_markets.size(), differ, tiles);
            // The row is vacuous on a world that folded nothing, so it says so.
            check(!w.folded_markets.empty(),
                  "P7 this world folds markets, so the fold-map row below is not vacuous");
            check(differ == 0 && loaded.folded_markets.size() == w.folded_markets.size()
                      && loaded.folded_markets == w.folded_markets,
                  "P7 every tile routes to the same market after the round trip (the fold map survives)");
        }
        check(loaded.next_entity_id() == w.next_entity_id()
                  && loaded.next_convoy_id == w.next_convoy_id
                  && loaded.next_trade_id == w.next_trade_id
                  && loaded.next_procurement_id == w.next_procurement_id,
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

        convoy_component cv;
        cv.source_market = 101; cv.dest_market = 202; cv.mode = convoy_mode::sea;
        cv.cargo_resource = resource_type::ordnance; cv.cargo_qty = 12.5f;
        cv.progress = 0.375f; cv.speed = 2.25f; cv.corp = c1; cv.arrived = false;
        cv.id = 9; cv.held = true; cv.cost_paid = 44.5f;
        cv.origin_tile = 301; cv.port_a = 302; cv.port_b = 303; // BL-1195 (v42)
        f.convoys.push_back(cv);
        cv.mode = convoy_mode::space; cv.id = 10; cv.held = false; cv.arrived = true;
        // Distinct from convoy 0's, and port_a left null while port_b is set, so a
        // reader that swapped or dropped a field breaks the by-hand check below.
        cv.origin_tile = 401; cv.port_a = null_entity; cv.port_b = 403;
        f.convoys.push_back(cv);

        f.trade_routes.push_back({ b1, b2, c1, 1234, 7 });
        f.body_last_glimpse_tick[b1] = 555;
        f.body_last_glimpse_tick[b2] = -3; // negative on purpose: signedness bugs hide in ticks

        // BL-1266 (v46): a second manual trade, every field distinct from P1's.
        if (trade_m2 != null_entity)
            f.trades.push_back({ f.allocate_trade_id(), trade_corp, resource_type::propellant,
                                 trade_m2, trade_m1, 0.375f });

        f.procurement_quotes.push_back({ 3, c1, c2, b1, b2, resource_type::alloys,
                                         15.0f, 3.25f, 6, 12.5f });
        f.procurement_contracts.push_back({ 4, c2, c1, b2, resource_type::electronics, b1,
                                            20.0f, 7.5f, 8, 3, 25.0f, 6.25f });

        condition_set embargo;
        embargo.all.push_back({ condition_subject::military_strength, condition_comparator::greater_than,
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
                      && back.convoys[1].arrived && back.convoys[1].mode == convoy_mode::space
                      && back.convoys[0].origin_tile == 301 && back.convoys[0].port_a == 302
                      && back.convoys[0].port_b == 303 && back.convoys[1].port_a == null_entity
                      && back.convoys[1].origin_tile == 401
                      && back.convoys[1].port_b == 403,
                  "P8 convoy fields land in the right members (BL-1195: the route's waypoints)");
            check(back.body_last_glimpse_tick.at(b2) == -3,
                  "P8 a negative glimpse tick survives (no unsigned round trip)");
            check(trade_m2 == null_entity
                      || (back.trades.size() == f.trades.size()
                          && back.trades.back().resource == resource_type::propellant
                          && back.trades.back().from_market == trade_m2
                          && back.trades.back().points == 0.375f),
                  "P8 a second manual trade keeps its fields in the right members (BL-1266)");
            check(back.procurement_contracts.size() == 1
                      && back.procurement_contracts[0].delivery_body == b1
                      && back.procurement_contracts[0].ticks_elapsed == 3,
                  "P8 a procurement contract keeps delivery body and elapsed ticks");
            const auto& e = back.corp_embargo_conditions.at(c3);
            check(e.all.size() == 2 && e.all[0].key == "embargo-key"
                      && e.all[0].subject == condition_subject::military_strength
                      && e.all[1].structure == building_type::military_base,
                  "P8 a condition_set nested in a map survives, strings included");
            check(e.all[0].province == no_province && e.all[1].province == 4242u,
                  "P8 BL-570: condition::province round-trips (default AND a real id)");
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
                  "P9 nation_budgets round-trips exactly: all priority_count weights "
                  "(ten since BL-644) + reserve for two nations, the non-dyadic 0.3f "
                  "included");
        }
    }

    std::printf("\n");
    if (g_failures == 0)
        std::printf("SAVE ROUND TRIP OK - a snapshot restores the world it was taken from.\n");
    else
        std::printf("SAVE ROUND TRIP FAIL - %d failure(s).\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
