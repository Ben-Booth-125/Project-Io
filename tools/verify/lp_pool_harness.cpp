// ---------------------------------------------------------------------------
// lp_pool_harness — BL-1309 (logistics one pool), the headless proof
// ---------------------------------------------------------------------------
// docs/economy/LOGISTICS.md § Logistic Points, "One pool, made by buildings,
// consumed by production" (Ben, 2026-10-14). On the SHIPPED start (the
// harness_params.hpp app mirror, which seeds the sources at the fold), per seed:
//
//   M   COVERAGE (report): the fold's seeding — hubs placed, sources per network,
//       consumers fully / partly / not covered, on nobody's ground, need vs pool —
//       then the 12-tick settle's last tick: points granted / needed, buildings
//       short.
// On the FIRST seed only, the mechanics:
//   L1  MADE ONLY BY THE THREE SOURCES: every source row is a hub, station or
//       port; each nation's pool equals the points of the running sources on its
//       ground, exactly (no city, no other building adds any); a road or rail
//       field never enters water.
//   L2  FADE: lp_reach is 1 to the threshold, linear across the fade, 0 beyond;
//       every consumer's coverage is the best reach of its host's sources'
//       fields (recomputed independently).
//   L3  PRODUCTION PROPORTIONAL TO POINTS: one economy step on two copies (every
//       second hub idled on one); every extraction site working in both produces
//       in the ratio of its granted fractions.
//   L4  THE HOST NATION'S POOL: every consumer draws on the nation of its ground;
//       a foreign firm's building counts under `other`; a source owned by a
//       foreigner adds to the ground's nation.
//   L5  SHARES: with a rule set and the pool short, each class gets its share (or
//       its whole claim) and the surplus spills; nothing granted beyond the pool;
//       with no rule every class gets the same fraction.
//   L6  THE SEAM: set_logistics_shares through agent_protocol::service_line as
//       the seat — every malformed or unauthorised command rejected with
//       state_hash unchanged; a valid one applies; clear applies; a rival
//       champion (in-process) is refused.
//   L7  THE SAVE: v51 round-trips a rule with an equal state_hash; a v49 stream
//       (v51 less its share-rule and space-asset sections) reads with no rule; a
//       bad share refuses.
//
// Build: bash tools/verify/build_lua_harness.sh lp_pool_harness
// Run:   build_gen/verify/lp_pool_harness.exe [--seeds a,b,...] [seed ...]   (default 0)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistic_points.hpp"
#include "world/logistics.hpp"
#include "world/spawn_seat.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include "../../src/core/agent_protocol.cpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
int g_pass = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[800];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string serve(world& w, const recipe_registry& reg, const agent_protocol::session& s,
                  const std::string& line)
{
    std::string out;
    agent_protocol::service_line(line, w, reg, w.current_econ_tick, s, out);
    return out;
}

bool result_is(const std::string& out, const char* r)
{
    return out.find(std::string("result=") + r + " ") != std::string::npos
        || out.find(std::string("result=") + r + "\n") != std::string::npos;
}

std::string to_bytes(const world& w)
{
    std::ostringstream os(std::ios::binary);
    write_world_snapshot(w, os);
    return os.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream is(b, std::ios::binary);
    return read_world_snapshot(w, is);
}

bool approx(float a, float b, float tol = 1e-4f)
{
    return std::fabs(a - b) <= tol * std::max(1.0f, std::max(std::fabs(a), std::fabs(b)));
}

entity_id ground(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

const lp_nation_row* nation_row(const lp_tick& t, entity_id n)
{
    for (const lp_nation_row& r : t.nations)
        if (r.nation == n) return &r;
    return nullptr;
}

/// Idle every second running hub (ascending id) — the shortage fixture.
void idle_alternate_hubs(world& w, entity_id only_nation = null_entity, bool all_but_one = false)
{
    std::vector<entity_id> ids;
    for (const auto& [bid, b] : w.buildings)
        if (b.type == building_type::inland_logistics_hub && lp_source_running(b)
            && (only_nation == null_entity || ground(w, b.tile) == only_nation))
            ids.push_back(bid);
    std::sort(ids.begin(), ids.end());
    for (std::size_t i = 0; i < ids.size(); ++i)
        if (all_but_one ? i > 0 : (i % 2 == 1))
            w.buildings.at(ids[i]).decommissioned = true;
    if (only_nation != null_entity && all_but_one)
        for (const auto& [bid, b] : w.buildings)
            if ((b.type == building_type::railway_station || b.type == building_type::port)
                && ground(w, b.tile) == only_nation)
                w.buildings.at(bid).decommissioned = true;
    invalidate_logistics_caches(w);
}

void mechanics(app_start_world& out, std::uint32_t seed)
{
    world& w = out.w;
    const recipe_registry& reg = out.reg;
    const lp_tick t = resolve_logistic_points(w);

    // ---- L1 -----------------------------------------------------------------
    std::printf("--- L1 made only by the three sources ---\n");
    {
        bool types_ok = true;
        for (const lp_source_row& s : t.sources)
        {
            const building_type ty = w.buildings.at(s.building).type;
            lp_network n;
            types_ok = types_ok && lp_source_network(ty, n) && n == s.network;
        }
        std::map<entity_id, float> expect;
        for (const auto& [bid, b] : w.buildings)
        {
            lp_network n;
            if (!lp_source_network(b.type, n) || !lp_source_running(b)) continue;
            const entity_id g = ground(w, b.tile);
            if (g != null_entity) expect[g] += lp_source_points[static_cast<std::size_t>(n)] * building_supply_scalar(b);
        }
        bool pools_ok = true;
        for (const lp_nation_row& r : t.nations)
        {
            const float e = expect.count(r.nation) ? expect[r.nation] : 0.0f;
            if (!approx(r.pool, e)) { pools_ok = false; std::printf("    nation %u pool %.3f expected %.3f\n", r.nation, r.pool, e); }
        }
        for (const auto& [n, e] : expect)
            if (nation_row(t, n) == nullptr) pools_ok = false;
        check(types_ok && !t.sources.empty(), "L1", fmt("%zu sources, every one a hub, station or port on its own network", t.sources.size()));
        check(pools_ok, "L1", fmt("every nation's pool (%zu) is exactly its ground's running sources' points; cities add none", t.nations.size()));
        bool dry = true;
        for (const lp_source_row& s : t.sources)
        {
            if (s.network == lp_network::sea) continue;
            const auto f = lp_distance_field(w, w.tiles.at(s.tile).body, s.network, {s.tile});
            for (const auto& [tile, d] : f)
                if (is_water(w.tiles.at(tile).substrate)) dry = false;
        }
        check(dry, "L1", "no road or rail field enters water");
        // Upkeep: a source supplied at half makes half its points, never zero.
        if (!t.sources.empty() && t.sources.front().nation != null_entity)
        {
            world c = w;
            const lp_source_row& s0 = t.sources.front();
            const float before = nation_row(resolve_logistic_points(c), s0.nation)->pool;
            c.buildings.at(s0.building).supply_factor_permille = 500;
            const float after = nation_row(resolve_logistic_points(c), s0.nation)->pool;
            const float full = lp_source_points[static_cast<std::size_t>(s0.network)] * building_supply_scalar(w.buildings.at(s0.building));
            check(approx(before - after, full - 0.5f * lp_source_points[static_cast<std::size_t>(s0.network)]) && after > 0.0f,
                  "L1", fmt("a source at half its upkeep makes half its points (pool %.1f -> %.1f)", before, after));
        }
    }

    // ---- L2 -----------------------------------------------------------------
    std::printf("--- L2 fade with distance ---\n");
    {
        bool curve = true;
        for (std::size_t n = 0; n < lp_network_count; ++n)
        {
            const lp_network ln = static_cast<lp_network>(n);
            const float F = lp_full_reach[n], L = lp_fade_length[n];
            curve = curve && lp_reach(ln, 0.0f) == 1.0f && lp_reach(ln, F) == 1.0f
                 && approx(lp_reach(ln, F + 0.5f * L), 0.5f) && approx(lp_reach(ln, F + 0.25f * L), 0.75f)
                 && lp_reach(ln, F + L) == 0.0f && lp_reach(ln, F + L + 5.0f) == 0.0f;
        }
        check(curve, "L2", "reach is 1 to the threshold, linear across the fade, 0 beyond (all three networks)");
        // Independent coverage: per host nation, per network, the field of its sources.
        std::map<std::pair<entity_id, int>, std::vector<entity_id>> seeds; // (nation, net) on the home body
        for (const lp_source_row& s : t.sources)
            if (s.nation != null_entity) seeds[{s.nation, static_cast<int>(s.network)}].push_back(s.tile);
        std::map<std::pair<entity_id, int>, std::unordered_map<entity_id, float>> fields;
        int bad = 0, partial = 0, checked = 0;
        for (const lp_building_row& r : t.buildings)
        {
            if (r.nation == null_entity) continue;
            const entity_id tile = w.buildings.at(r.building).tile;
            float cov = 0.0f;
            for (int n = 0; n < 3; ++n)
            {
                const auto k = std::make_pair(r.nation, n);
                if (!seeds.count(k)) continue;
                std::vector<entity_id> on_body;
                for (const entity_id s : seeds[k])
                    if (w.tiles.at(s).body == w.tiles.at(tile).body) on_body.push_back(s);
                if (!fields.count(k)) fields[k] = lp_distance_field(w, w.tiles.at(tile).body, static_cast<lp_network>(n), on_body);
                const auto it = fields[k].find(tile);
                if (it != fields[k].end()) cov = std::max(cov, lp_reach(static_cast<lp_network>(n), it->second));
            }
            ++checked;
            if (r.coverage > 0.0f && r.coverage < 1.0f) ++partial;
            if (!approx(cov, r.coverage)) ++bad;
        }
        check(bad == 0 && checked > 0, "L2", fmt("%d consumers' coverage equals the best reach of their host's sources (%d in the fade)", checked, partial));
    }

    // ---- L3 -----------------------------------------------------------------
    std::printf("--- L3 production proportional to points ---\n");
    {
        world a = w, b = w;
        idle_alternate_hubs(b);
        const economy_report ra = run_economy_step(a, reg, /*spectating=*/true);
        const economy_report rb = run_economy_step(b, reg, /*spectating=*/true);
        std::map<entity_id, float> oa, ob;
        for (const building_report& r : ra.buildings) if (r.type == building_type::extraction_site) oa[r.building] = r.output_quantity;
        for (const building_report& r : rb.buildings) if (r.type == building_type::extraction_site) ob[r.building] = r.output_quantity;
        int compared = 0, mism = 0, moved = 0, zeroed = 0;
        for (const auto& [bid, qa] : oa)
        {
            const auto fa_it = ra.logistic_points.fraction.find(bid);
            const auto fb_it = rb.logistic_points.fraction.find(bid);
            if (fa_it == ra.logistic_points.fraction.end() || fb_it == rb.logistic_points.fraction.end()) continue;
            const float fa = fa_it->second, fb = fb_it->second;
            const float qb = ob.count(bid) ? ob[bid] : 0.0f;
            if (fb == 0.0f) { if (qb == 0.0f) ++zeroed; else ++mism; continue; }
            if (!(qa > 0.0f) || !(fa > 0.0f)) continue;
            ++compared;
            if (fb != fa) ++moved;
            const float want = qa * (fb / fa);
            if (!approx(qb, want, 2e-3f))
            {
                ++mism;
                if (mism <= 5) std::printf("    site %u: out %.4f -> %.4f, fraction %.4f -> %.4f (want %.4f)\n", bid, qa, qb, fa, fb, want);
            }
        }
        check(compared > 0 && moved > 0 && mism == 0, "L3",
              fmt("%d sites in both runs (%d with their fraction moved, %d starved to zero produce nothing): output in the ratio of points granted, %d off",
                  compared, moved, zeroed, mism));
    }

    // ---- L4 -----------------------------------------------------------------
    std::printf("--- L4 the host nation's pool ---\n");
    {
        int wrong = 0, foreign = 0, foreign_bad = 0;
        for (const lp_building_row& r : t.buildings)
        {
            const entity_id g = ground(w, w.buildings.at(r.building).tile);
            if (r.nation != g) ++wrong;
            const entity_id home = owner_home_nation(w, r.owner);
            if (g != null_entity && home != null_entity && home != g)
            {
                ++foreign;
                if (r.cls != lp_class::other) ++foreign_bad;
            }
        }
        int fsrc = 0;
        for (const lp_source_row& s : t.sources)
        {
            const building_owner o = building_owner_of(w, s.building);
            const entity_id home = owner_home_nation(w, o.id);
            if (s.nation != null_entity && home != null_entity && home != s.nation) ++fsrc;
        }
        check(wrong == 0, "L4", fmt("every one of %zu consumers draws on the nation of its ground", t.buildings.size()));
        check(foreign_bad == 0, "L4", fmt("%d foreign firms' buildings counted under 'other' on their host's pool", foreign));
        std::printf("    %d sources stand on a nation's ground owned by a foreigner (added to the ground's pool: L1)\n", fsrc);
    }

    // ---- L5 -----------------------------------------------------------------
    std::printf("--- L5 shares ---\n");
    (void)seat_player_corporation(w, seed, out.land.search.winner_score);
    const entity_id P  = w.player_entity;
    const entity_id pn = player_nation(w);
    {
        // The fixture: the major whose ground carries claims in the most classes;
        // its consumers run at full (need x4 at most) and its sources are idled
        // one by one (descending id) until its pool is short of its claims.
        world c = w;
        entity_id ln = null_entity;
        int best = -1;
        for (const lp_nation_row& r : t.nations)
        {
            if (!is_major_nation(c, r.nation)) continue;
            int k = 0;
            for (std::size_t j = 0; j < lp_class_count; ++j) if (r.claim[j] > 0.0f) ++k;
            if (k > best) { best = k; ln = r.nation; }
        }
        // Short the pool by dimming its sources' upkeep supply (points follow the
        // upkeep goods received): every source of the nation at the factor that
        // leaves the pool half its claims.
        {
            const lp_tick t0 = resolve_logistic_points(c);
            const lp_nation_row* r0 = nation_row(t0, ln);
            const float claims = r0 ? r0->claim[0] + r0->claim[1] + r0->claim[2] : 0.0f;
            const float pool   = r0 ? r0->pool : 0.0f;
            const int permille = (pool > 0.0f) ? std::max(1, static_cast<int>(1000.0f * 0.5f * claims / pool)) : 1000;
            for (auto& [bid, bb] : c.buildings)
            {
                lp_network n;
                if (lp_source_network(bb.type, n) && ground(c, bb.tile) == ln)
                    bb.supply_factor_permille = std::min(1000, permille);
            }
        }
        std::printf("    fixture: nation %u, %d classes claiming\n", ln, best);
        const entity_id lp_n = ln;
        const lp_tick mech = resolve_logistic_points(c);
        const lp_nation_row* mr = nation_row(mech, lp_n);
        bool same = mr != nullptr;
        float g0 = -1.0f;
        if (mr)
            for (std::size_t k = 0; k < lp_class_count; ++k)
                if (mr->claim[k] > 0.0f)
                {
                    const float g = mr->granted[k] / mr->claim[k];
                    if (g0 < 0.0f) g0 = g; else same = same && approx(g, g0);
                }
        check(same, "L5", fmt("no rule: every class of nation %u gets the same fraction (%.3f; pool %.1f, claims %.1f/%.1f/%.1f)",
                              lp_n, g0, mr ? mr->pool : 0.f, mr ? mr->claim[0] : 0.f, mr ? mr->claim[1] : 0.f, mr ? mr->claim[2] : 0.f));
        // The rule is set directly here (the verb is L6's subject).
        c.nations.at(lp_n).lp_shares.set = true;
        c.nations.at(lp_n).lp_shares.share = {0.6f, 0.3f, 0.1f};
        const lp_tick ruled = resolve_logistic_points(c);
        const lp_nation_row* r = nation_row(ruled, lp_n);
        bool ok = r != nullptr && r->rule_set && r->pool < r->claim[0] + r->claim[1] + r->claim[2];
        float total = 0.0f;
        if (ok)
        {
            const std::array<float, 3> sh = {0.6f, 0.3f, 0.1f};
            std::array<float, 3> base{}, shortf{};
            float surplus = 0.0f, st = 0.0f;
            for (std::size_t k = 0; k < 3; ++k)
            {
                const float alloc = r->pool * sh[k];
                base[k] = std::min(alloc, r->claim[k]);
                surplus += std::max(0.0f, alloc - r->claim[k]);
                shortf[k] = r->claim[k] - base[k];
                st += shortf[k];
            }
            for (std::size_t k = 0; k < 3; ++k)
            {
                float want = base[k];
                if (surplus > 0.0f && st > 0.0f) want += std::min(shortf[k], surplus * shortf[k] / st);
                ok = ok && approx(r->granted[k], want, 1e-3f);
                ok = ok && r->granted[k] + 1e-3f >= std::min(r->claim[k], r->pool * sh[k]); // its share, or its whole claim
                total += r->granted[k];
            }
            ok = ok && total <= r->pool * (1.0f + 1e-5f) + 1e-4f;
            std::printf("    rule 0.6/0.3/0.1: pool %.1f, claims %.1f/%.1f/%.1f, granted %.1f/%.1f/%.1f\n", r->pool,
                        r->claim[0], r->claim[1], r->claim[2], r->granted[0], r->granted[1], r->granted[2]);
        }
        // Per building: within a class, every claim granted the same fraction.
        bool pro_rata = ok;
        std::array<float, 3> gk = {-1.f, -1.f, -1.f};
        for (const lp_building_row& b : ruled.buildings)
        {
            if (b.nation != lp_n || !(b.need > 0.0f) || !(b.coverage > 0.0f)) continue;
            const float g = b.fraction / b.coverage;
            float& ref = gk[static_cast<std::size_t>(b.cls)];
            if (ref < 0.0f) ref = g; else pro_rata = pro_rata && approx(g, ref, 1e-3f);
        }
        check(ok && pro_rata, "L5", "with a rule set and the pool short: each class its share or its whole claim, the surplus spilled, pro rata within a class, never beyond the pool");
    }

    // ---- L6 -----------------------------------------------------------------
    std::printf("--- L6 the seam ---\n");
    if (pn != null_entity && champion_of(w, pn) == P)
    {
        agent_protocol::session ps;
        ps.actor = P;
        const unsigned V = static_cast<unsigned>(corp_verb::set_logistics_shares);
        const auto P_ = [&](const std::string& rest) { return fmt("COMMAND corp=%u verb=%u ", static_cast<unsigned>(P), V) + rest; };
        entity_id other_n = null_entity;
        {
            std::vector<entity_id> nids;
            for (const auto& [nid, nc] : w.nations) nids.push_back(nid);
            std::sort(nids.begin(), nids.end());
            for (const entity_id n : nids) if (n != pn) { other_n = n; break; }
        }
        struct rej { std::string line; const char* want; const char* what; };
        std::vector<rej> rows = {
            {P_("quantity=0.5 floor_price=0.3"), "rejected_invalid", "no nation (a corporation's book)"},
            {P_(fmt("nation=%u quantity=0.7 floor_price=0.5", pn)), "rejected_invalid", "shares summing past 1"},
            {P_(fmt("nation=%u quantity=-0.1 floor_price=0.3", pn)), "rejected_invalid", "a negative share"},
            {P_(fmt("nation=%u quantity=1.5 floor_price=0", pn)), "rejected_invalid", "a share past 1"},
            {P_(fmt("nation=%u quantity=nan floor_price=0.3", pn)), "rejected_invalid", "a NaN share"},
            {P_(fmt("nation=%u quantity=inf floor_price=0", pn)), "rejected_invalid", "an infinite share"},
            {P_(fmt("nation=%u quantity=1e300 floor_price=0", pn)), "rejected_invalid", "a share past float"},
            {P_(fmt("nation=%u quantity=0.5 floor_price=0.3 order=2", pn)), "rejected_invalid", "an order past clear"},
            {P_(fmt("nation=%u quantity=0.5 order=1", pn)), "rejected_invalid", "a clear carrying a share"},
            {P_(fmt("nation=%u order=1", pn)), "rejected_state", "a clear with no rule set"},
            {P_("nation=abc quantity=0.5"), "rejected_invalid", "a malformed nation id"},
        };
        if (other_n != null_entity)
            rows.push_back({P_(fmt("nation=%u quantity=0.5 floor_price=0.3", other_n)), "rejected_not_owner", "another nation's shares (actor gate)"});
        bool all_rej = true;
        for (const rej& r : rows)
        {
            const std::uint64_t h0 = w.state_hash(0);
            const std::string o = serve(w, reg, ps, r.line);
            const bool ok = result_is(o, r.want) && w.state_hash(0) == h0 && !w.nations.at(pn).lp_shares.set;
            if (!ok) { all_rej = false; std::printf("    REJECT ROW FAILED: %s -> %s", r.what, o.c_str()); }
        }
        check(all_rej, "L6", fmt("%zu malformed / unauthorised / illegal share commands rejected, state_hash unchanged", rows.size()));
        const std::uint64_t h0 = w.state_hash(0);
        const std::string o1 = serve(w, reg, ps, P_(fmt("nation=%u quantity=0.5 floor_price=0.25", pn)));
        const auto& rule = w.nations.at(pn).lp_shares;
        check(result_is(o1, "applied") && rule.set && approx(rule.share[0], 0.5f) && approx(rule.share[1], 0.25f)
                  && approx(rule.share[2], 0.25f) && w.state_hash(0) != h0,
              "L6", "a valid rule applies (0.5 / 0.25 / the rest 0.25) and moves the state hash");
        const std::string o2 = serve(w, reg, ps, P_(fmt("nation=%u order=1", pn)));
        check(result_is(o2, "applied") && !w.nations.at(pn).lp_shares.set && w.state_hash(0) == h0,
              "L6", "clear applies: back to the mechanical default, the hash back where it was");
        // A rival champion, in-process: refused.
        entity_id rn = null_entity;
        {
            std::vector<entity_id> nids;
            for (const auto& [nid, nc] : w.nations) nids.push_back(nid);
            std::sort(nids.begin(), nids.end());
            for (const entity_id n : nids)
                if (n != pn && is_major_nation(w, n)) { rn = n; break; }
        }
        if (rn != null_entity)
        {
            corp_command cmd;
            cmd.corp = champion_of(w, rn); cmd.as_nation = rn; cmd.verb = corp_verb::set_logistics_shares;
            cmd.quantity = 0.5f; cmd.floor_price = 0.5f;
            const std::uint64_t h1 = w.state_hash(0);
            const corp_command_result r = apply_corp_command(w, reg, cmd);
            check(r == corp_command_result::rejected_not_owner && w.state_hash(0) == h1, "L6",
                  "a rival champion may not set its nation's shares (no AI grant)");
        }
    }
    else
        check(false, "L6", "the seat is its major nation's champion");

    // ---- L7 -----------------------------------------------------------------
    std::printf("--- L7 the save ---\n");
    {
        world c = w;
        c.nations.at(pn).lp_shares.set = true;
        c.nations.at(pn).lp_shares.share = {0.2f, 0.5f, 0.3f};
        const std::string b51 = to_bytes(c);
        world r;
        const bool rt = from_bytes(b51, r);
        check(rt && r.state_hash(0) == c.state_hash(0) && r.nations.at(pn).lp_shares.set
                  && r.nations.at(pn).lp_shares.share == c.nations.at(pn).lp_shares.share,
              "L7", "v51 round-trips the share rule with an equal state_hash");
        // v49: the stream less its trailing section (u32 count + 1 x (id + 3 floats)),
        // and less BL-1327's v51 space-asset section after it (one empty count).
        // BL-1335 (v52): and less the tax section after that (count + 9 bytes a
        // nation whose tier or Discontent moved).
        const std::size_t tax_tail = world_save_tax_section_bytes(c);
        std::string b49 = b51.substr(0, b51.size() - (4 + 4 + 12) - 4 - tax_tail);
        const std::uint32_t v49 = k_world_save_version_pre_lp;
        std::memcpy(&b49[4], &v49, 4);
        world r49;
        const bool ok49 = from_bytes(b49, r49);
        check(ok49 && !r49.nations.at(pn).lp_shares.set, "L7", "a v49 stream reads, every nation on the mechanical default");
        // A bad share in the stream refuses it whole.
        std::string bad = b51;
        const float f = 0.9f;
        std::memcpy(&bad[bad.size() - 8 - tax_tail], &f, 4); // other := 0.9: the sum is 1.6 (before v51's 4-byte space count and v52's tax section)
        world rb;
        rb.current_econ_tick = 77;
        const bool okb = from_bytes(bad, rb);
        check(!okb && rb.current_econ_tick == 77, "L7", "a share rule not summing to 1 refuses the stream whole");
    }
}

void run_seed(lua_state& lua, std::uint32_t seed, bool mech)
{
    std::printf("\n=== seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    const lp_seed_report& s = out.land.lp;
    std::printf("COVERAGE seed %u: hubs placed %d; sources road %d rail %d sea %d; consumers %d: full %d, partial %d, "
                "uncovered %d, nobody's ground %d; need %.1f pool %.1f granted %.1f (%.1f%%)\n",
                static_cast<unsigned>(seed), s.hubs_placed, s.sources_by_net[0], s.sources_by_net[1],
                s.sources_by_net[2], s.consumers, s.fully_covered, s.partly_covered, s.uncovered,
                s.nobodys_ground, s.need_total, s.pool_total, s.granted_total,
                s.need_total > 0.0f ? 100.0 * s.granted_total / s.need_total : 0.0);
    check(s.consumers > 0 && s.uncovered == 0, "M", fmt("seed %u: every consumer on a nation's ground is reached after seeding", seed));
    {
        const lp_tick t0 = resolve_logistic_points(out.w);
        for (const lp_building_row& b : t0.buildings)
            if (b.nation != null_entity && b.coverage < 1.0f)
            {
                const building_component& bc = out.w.buildings.at(b.building);
                const tile_component& tc = out.w.tiles.at(bc.tile);
                std::printf("    NOT FULLY REACHED: building %u type %d owner %u nation %u (major %d) tile %u (%d,%d) water %d coverage %.3f\n",
                            b.building, static_cast<int>(bc.type), b.owner, b.nation, is_major_nation(out.w, b.nation) ? 1 : 0,
                            bc.tile, tc.grid_x, tc.grid_y, is_water(tc.substrate) ? 1 : 0, b.coverage);
            }
    }

    if (mech)
        mechanics(out, seed);

    // The settle (twelve spectating ticks, as the fold runs it), report-only.
    world c = out.w;
    float need = 0.0f, granted = 0.0f, lost_reach = 0.0f;
    int consumers = 0, short_n = 0, starved = 0, unreached = 0;
    for (int k = 0; k < 12; ++k)
    {
        const settle_tick_result r = run_settle_tick(c, out.reg, k, 0, /*spectating=*/true);
        if (k == 11)
            for (const lp_building_row& b : r.report.logistic_points.buildings)
            {
                ++consumers;
                need += b.need;
                granted += b.granted;
                if (b.fraction < 0.999f) ++short_n;
                if (b.fraction <= 0.0f) ++starved;
                lost_reach += b.need * (1.0f - b.coverage);
                if (b.coverage < 1.0f) ++unreached;
            }
    }
    std::printf("SETTLE seed %u tick 12: %d consumers, need %.1f, granted %.1f (%.1f%%), %d short of full, %d starved; "
                "%d not fully reached (%.1f points of need out of reach), %.1f short at the pool\n",
                static_cast<unsigned>(seed), consumers, need, granted, need > 0.0f ? 100.0 * granted / need : 0.0,
                short_n, starved, unreached, lost_reach, need - lost_reach - granted);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::string list = argv[++i];
            std::size_t p = 0;
            while (p < list.size())
            {
                const std::size_t q = list.find(',', p);
                seeds.push_back(static_cast<std::uint32_t>(std::strtoul(list.substr(p, q - p).c_str(), nullptr, 10)));
                if (q == std::string::npos) break;
                p = q + 1;
            }
            continue;
        }
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    }
    if (seeds.empty())
        seeds.push_back(0);
    lua_state lua;
    for (std::size_t i = 0; i < seeds.size(); ++i)
        run_seed(lua, seeds[i], /*mech=*/i == 0);
    std::printf("\nlp_pool_harness: %d pass, %d fail -- %s\n", g_pass, g_fail, g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
