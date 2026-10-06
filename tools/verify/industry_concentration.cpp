// ---------------------------------------------------------------------------
// industry_concentration — WHERE the 1960 world's industry stands, by landmass
// and by nation, and how the leader's lead grew across the Industrialisation
// span. A READING (sprint 48 measurement lane, 2026-10-03): Ben asked whether
// "one highly industrialised continent, dramatically fewer industrial sites
// elsewhere" is a common pattern before anything is changed.
//
// THE WORLD READ is the app's, built as rebless_shape_probe builds it:
// make_hard_coded_world with the app's config and works plus an
// era_minus_one_fixture (records the spans, changes no output), the landscape
// search's winner applied, the validation settle, then the seat.
//
// LANDMASS = `landmass_labels` over the sim's own substrate raster (the
// fixture's), the same labels the span's cross-water reads use; a region is on
// the landmass `landmass_at` gives its anchor, a tile on its own label.
// NATION = `region::nation` on world::gen_settlement at the close (the span's
// polity id); a firm's nation and landmass are its `origin_region`'s.
//
// REPORTS, AND GATES ON ONE THING (BL-1168): the close's landmass reach
// (`stockpile_region_reach`, off the world's tiles) must equal the sim's (off
// the fixture's raster) region for region, since the span's works notes and the
// close's charter price read one reach. A STRADDLE line counts carved centres
// whose own tile stands on a different landmass from their region's anchor, and
// a DATING line splits the charters' founding years by source (note, furnace,
// epoch), heartland vs far landmass.
// A SECOND GATE (BL-1176): a NOTEPRICE line runs the span's works-note price
// step (`works_note_region_prices`) on the close's stock and holds each
// budgeted centre's region to the close's `centre_firm_price`, exactly.
//
// Build: bash tools/verify/build_lua_harness.sh industry_concentration
// Run from the repo root: build_gen/verify/industry_concentration.exe [--seeds a,b]
// Output lines: LM (per landmass), NAT (top five nations by points), SEED (the
// concentration summary: top-landmass shares, Herfindahl over landmasses and
// nations, second/first ratios, the far rival -- another landmass, or a
// capital at least gw/4 columns away), ZONE (the densest quarter-width disc,
// for one-landmass worlds; and seat AFFORDABILITY at the world's price vs a
// price read off the centre's own landmass stock), EARN (the treasury's
// sources per landmass at the close), TRACE (leader, second and far rival:
// points by year off the industrialisation time-lapse, as value/permille of
// the world).
//
// Measured 2026-10-03 on the 16 curated seeds: treasury conversion is 70-99%
// of a leader's points and scale credit per employed head is near-flat across
// landmasses, so concentration beyond population share is the treasury's.
// ---------------------------------------------------------------------------

#include "harness_params.hpp"

#include "scripting/lua_state.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/ocean_currents.hpp"
#include "world/settlement.hpp"
#include "world/spawn_seat.hpp"
#include "world/stockpile_budget.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{

struct agg
{
    long long land_tiles = 0, regions = 0, pop = 0, urban = 0, employed = 0, cities = 0,
              centres = 0, works = 0, ip = 0, ip_treasury = 0, budget = 0, firms = 0,
              specialists = 0, buildings = 0, seats = 0, fuel_wsum = 0, fuel_w = 0, polities = 0;
};

double share(long long a, long long t) { return t > 0 ? double(a) / double(t) : 0.0; }

double hhi(const std::map<int, agg>& m, long long agg::*f)
{
    long long t = 0;
    for (const auto& [k, a] : m) t += a.*f;
    double h = 0;
    for (const auto& [k, a] : m) { const double s = share(a.*f, t); h += s * s; }
    return h;
}

std::vector<std::pair<long long, int>> ranked(const std::map<int, agg>& m, long long agg::*f)
{
    std::vector<std::pair<long long, int>> v;
    for (const auto& [k, a] : m) v.push_back({a.*f, k});
    std::sort(v.begin(), v.end(), [](auto x, auto y) { return x.first != y.first ? x.first > y.first : x.second < y.second; });
    return v;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            std::string s = argv[++i];
            std::size_t at = 0;
            while (at <= s.size())
            {
                const std::size_t c = s.find(',', at);
                const std::string tok = s.substr(at, c == std::string::npos ? std::string::npos : c - at);
                if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (c == std::string::npos) break;
                at = c + 1;
            }
        }

    lua_state lua;
    int failures = 0;
    long long note_price_compared = 0; // BL-1176: the NOTE PRICE row must compare something
    for (uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        auto out = std::make_unique<app_start_world>();
        era_minus_one_fixture fx;
        out->params = p;
        lua.load("scripts/world_gen.lua");
        out->cfg = world_gen_config{};
        out->cfg.load_from_lua(lua);
        if (out->works.size() == 0)
        {
            lua.load("scripts/works.lua");
            out->works.load_from_lua(lua);
        }
        out->report = generation_report{};
        out->w = make_hard_coded_world(p, &out->report, out->cfg, nullptr, &out->works, &fx);
        seed_genesis_history(out->w, out->report);
        init_survey_states(out->w);
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        out->reg.load_from_lua(lua);
        band_registry_from_world(out->reg, out->w);
        assign_default_recipes(out->w, out->reg);
        // The winner's spend report, kept so the charters can be dated as
        // finish_campaign_world dates them (an output only: the search and
        // the apply read nothing from it).
        charter_spend_report charter_rep;
        {
            harness_charter_input in;
            in.report = &charter_rep;
            apply_app_start_landscape(*out, in);
        }
        // BL-1099/BL-1168: date the charters against the span's record, as
        // finish_campaign_world does after the apply and before the settle.
        const std::vector<lapse_event>* span_events = nullptr;
        for (const generation_report::body_entry& be : out->report.bodies)
            if (!be.industrialisation_timelapse.events.empty())
            {
                span_events = &be.industrialisation_timelapse.events;
                break;
            }
        {
            static const std::vector<lapse_event> none;
            date_chartered_firms(out->w, charter_rep, span_events ? *span_events : none,
                                 static_cast<int32_t>(p.epoch_year));
        }
        run_app_validation_settle(out->w, out->reg);
        const spawn_seat_result seat =
            seat_player_corporation(out->w, seed, out->land.search.winner_score);

        const world& w = out->w;
        const entity_id home = w.home_body;
        const int gw = fx.gw, gh = fx.gh;
        if (!w.gen_settlement || !fx.industrialisation_ran || gw <= 0)
        {
            std::printf("SEED seed=%u skipped (no settlement / span)\n", seed);
            continue;
        }
        const std::vector<region>& R = w.gen_settlement->regions;
        // NATION = the span's POLITY id at the close (the handoff's regions,
        // same index as gen_settlement's); gen_settlement's `nation` is the
        // folded nation index, which the span's polity record cannot be read by.
        const std::vector<region>& H = fx.industrialisation_handoff.regions;
        if (H.size() != R.size())
            std::printf("WARN seed=%u handoff regions %zu != settlement regions %zu\n", seed, H.size(), R.size());
        std::vector<int> pol(R.size(), -1);
        long long ip_mismatch = 0;
        for (std::size_t i = 0; i < R.size() && i < H.size(); ++i)
        {
            pol[i] = H[i].nation;
            if (H[i].industry_points != R[i].industry_points) ++ip_mismatch;
        }
        const std::vector<int32_t> labels = landmass_labels(fx.terrain.substrate, gw, gh);

        std::vector<int> reg_lm(R.size(), -1);
        for (std::size_t i = 0; i < R.size(); ++i)
            reg_lm[i] = landmass_at(labels, gw, gh, R[i].col, R[i].row);
        {
            // BL-1168: the charter price reads the landmass off the world's own
            // tiles (`stockpile_region_reach`); it must agree with the sim's.
            const std::vector<std::int64_t> reach = stockpile_region_reach(w, charter_price_reach::landmass);
            int differ = 0;
            for (std::size_t i = 0; i < R.size() && i < reach.size(); ++i)
                if (reach[i] != reg_lm[i]) ++differ;
            std::printf("REACH seed=%u regions=%zu landmass_differs_from_sim=%d\n", seed, R.size(),
                        reach.size() == R.size() ? differ : -1);
            // A GATE, not a reading: the span's works notes and the close's
            // price must read one reach (NR-907's one-divisor rule, carried to
            // the reach by BL-1168), so a region the two place on different
            // landmasses is a defect.
            if (reach.size() != R.size() || differ != 0)
            {
                std::printf("FAIL  REACH seed=%u: the close's landmass reach and the sim's disagree "
                            "(%d regions differ, %zu reach keys for %zu regions)\n",
                            seed, differ, reach.size(), R.size());
                ++failures;
            }
        }
        {
            // NOTE PRICE (BL-1176, NOTE_PRICE_MATCHES_CLOSE_ROW): the span's
            // works-note price step -- `works_note_region_prices`, the one
            // function the sim's note loop calls every year -- run on the
            // close's stock over the sim's own landmass raster, against the
            // price the close charges each budgeted centre
            // (`stockpile_budget::centre_firm_price`, the shipped reach). NR-907
            // and BL-1168 hold the span and the close to one divisor and one
            // reach; the REACH gate above only checks the two label rasters
            // agree, so a change to how the sim's step labels or sums a region
            // fails HERE. A GATE: every budgeted centre is compared, by its
            // carve slot's region, and one differing price is a defect.
            const stockpile_budget sb = build_stockpile_budget(w);
            std::vector<std::int64_t> note_price;
            const bool priced = works_note_region_prices(R, labels, gw, gh, note_price);
            int compared = 0, differ = 0, unslotted = 0, shown = 0;
            for (const auto& [centre, close_price] : sb.centre_firm_price)
            {
                const auto slot = w.gen_carve_centres.find(centre);
                if (slot == w.gen_carve_centres.end() || slot->second.region < 0
                    || slot->second.region >= static_cast<int>(R.size()))
                {
                    ++unslotted;
                    continue;
                }
                if (!priced) continue;
                ++compared;
                const std::int64_t span_price = note_price[static_cast<std::size_t>(slot->second.region)];
                if (span_price != static_cast<std::int64_t>(close_price))
                {
                    if (shown++ < 5)
                        std::printf("NOTEPRICE seed=%u centre=%llu region=%d span=%lld close=%d\n", seed,
                                    static_cast<unsigned long long>(centre), slot->second.region,
                                    static_cast<long long>(span_price), close_price);
                    ++differ;
                }
            }
            note_price_compared += compared;
            std::printf("NOTEPRICE seed=%u reach=%s centres=%zu compared=%d differ=%d unslotted=%d world_price=%d%s\n",
                        seed, sb.reach == charter_price_reach::landmass ? "landmass" : "other",
                        sb.centre_firm_price.size(), compared, differ, unslotted, sb.firm_price_points,
                        priced ? "" : " (span step refused the stock)");
            if (!priced || differ != 0 || unslotted != 0 || sb.reach != k_stockpile_charter_reach)
            {
                std::printf("FAIL  NOTEPRICE seed=%u: the span's note price and the close's charter price "
                            "disagree (%d of %d centres differ, %d unslotted, span step %s)\n",
                            seed, differ, compared, unslotted, priced ? "priced" : "REFUSED");
                ++failures;
            }
        }
        auto tile_lm = [&](entity_id tid) -> int {
            const auto t = w.tiles.find(tid);
            if (t == w.tiles.end() || t->second.body != home) return -2;
            const int x = t->second.grid_x, y = t->second.grid_y;
            if (x < 0 || y < 0 || x >= gw || y >= gh) return -2;
            const int l = labels[static_cast<std::size_t>(y * gw + x)];
            return l >= 0 ? l : landmass_at(labels, gw, gh, x, y);
        };

        // STRADDLE (BL-1168 review, item 4): a carved centre's own tile against
        // its region's anchor. The close prices a centre by its REGION's reach
        // (the anchor's landmass); a centre standing on another landmass would
        // be priced by ground it cannot walk to.
        {
            int carved = 0, straddle = 0, no_tile = 0;
            for (const auto& [cid, cs] : w.gen_carve_centres)
            {
                if (cs.region < 0 || cs.region >= static_cast<int>(R.size())) continue;
                const auto t = w.population_centre_tile.find(cid);
                if (t == w.population_centre_tile.end()) { ++no_tile; continue; }
                ++carved;
                const int l = tile_lm(t->second);
                if (l != reg_lm[static_cast<std::size_t>(cs.region)])
                {
                    if (straddle < 5)
                        std::printf("STRADDLE seed=%u centre=%llu region=%d anchor_lm=%d centre_lm=%d\n", seed,
                                    static_cast<unsigned long long>(cid), cs.region,
                                    reg_lm[static_cast<std::size_t>(cs.region)], l);
                    ++straddle;
                }
            }
            std::printf("STRADDLE seed=%u carved_centres=%d on_another_landmass=%d no_tile=%d\n", seed, carved,
                        straddle, no_tile);
        }

        // DATING (BL-1168 review, item 1): where each charter's founding year
        // came from, recounted from the record independently of
        // `date_chartered_firms` (a region's k-th charter, in report order,
        // takes its k-th note, else its furnace year, else the epoch), split by
        // whether its origin region stands on the HEARTLAND (the landmass
        // holding most points) or a far landmass. The founded year the world
        // carries must agree with the recount.
        {
            std::map<int, std::vector<int32_t>> notes_of;
            std::map<int, int32_t>              lit;
            if (span_events != nullptr)
                for (const lapse_event& e : *span_events)
                {
                    if (e.region == lapse_event_none) continue;
                    const int rg = static_cast<int>(e.region);
                    if (e.kind == static_cast<uint8_t>(lapse_event_kind::works_chartered))
                        notes_of[rg].push_back(e.year);
                    else if (e.kind == static_cast<uint8_t>(lapse_event_kind::furnace_lit))
                        lit.try_emplace(rg, e.year);
                }
            std::map<int, long long> lm_ip;
            for (std::size_t i = 0; i < R.size(); ++i)
                if (reg_lm[i] >= 0) lm_ip[reg_lm[i]] += R[i].industry_points;
            int heart = -1;
            long long heart_ip = -1;
            for (const auto& [l, v] : lm_ip)
                if (v > heart_ip) { heart = l; heart_ip = v; }
            int src[2][4] = {};   // [far][note, furnace, epoch, no origin]
            int disagree = 0;
            std::map<int, std::size_t> taken;
            for (const charter_record& c : charter_rep.charters)
            {
                const auto cit = w.corporations.find(c.corp);
                if (cit == w.corporations.end()) continue;
                const int org = cit->second.origin_region;
                const int far = (org >= 0 && static_cast<std::size_t>(org) < R.size()
                                 && reg_lm[static_cast<std::size_t>(org)] != heart) ? 1 : 0;
                int32_t want = static_cast<int32_t>(p.epoch_year);
                if (org < 0) ++src[far][3];
                else
                {
                    const std::size_t k = taken[org]++;
                    const auto nit = notes_of.find(org);
                    const auto fit = lit.find(org);
                    if (nit != notes_of.end() && k < nit->second.size()) { ++src[far][0]; want = nit->second[k]; }
                    else if (fit != lit.end()) { ++src[far][1]; want = fit->second; }
                    else ++src[far][2];
                }
                if (cit->second.founded_year != want) ++disagree;
            }
            int span_notes = 0;
            for (const auto& [rg, v] : notes_of) span_notes += static_cast<int>(v.size());
            // BL-1178 (WORKS_FRACTION_UNDER_REACH): WHEN the notes fire, by
            // century of the span (it opens at `industry_open_year`, 1660),
            // heartland vs far landmass -- the NR-940 early clustering read
            // under the reach price. A reading, not a gate.
            int cent[2][4] = {};   // [far][1660s-1750s, 1760s-1850s, 1860s on, no landmass]
            for (const auto& [rg, v] : notes_of)
            {
                const bool known = rg >= 0 && static_cast<std::size_t>(rg) < R.size()
                                && reg_lm[static_cast<std::size_t>(rg)] >= 0;
                const int far = (known && reg_lm[static_cast<std::size_t>(rg)] != heart) ? 1 : 0;
                for (const int32_t y : v)
                    ++cent[far][!known ? 3 : (y < 1760 ? 0 : (y < 1860 ? 1 : 2))];
            }
            std::printf("NOTECENT seed=%u heartland %d/%d/%d far %d/%d/%d no_landmass %d"
                        " (notes by century: <1760 / 1760-1859 / >=1860)\n",
                        seed, cent[0][0], cent[0][1], cent[0][2], cent[1][0], cent[1][1], cent[1][2],
                        cent[0][3] + cent[1][3]);
            std::printf("DATING seed=%u notes=%d heartland_lm=%d heartland note=%d furnace=%d epoch=%d | "
                        "far note=%d furnace=%d epoch=%d | no_origin=%d disagree=%d\n",
                        seed, span_notes, heart, src[0][0], src[0][1], src[0][2], src[1][0], src[1][1], src[1][2],
                        src[0][3] + src[1][3], disagree);
            if (disagree != 0)
            {
                std::printf("FAIL  DATING seed=%u: %d charters carry a year the record does not pair them with\n",
                            seed, disagree);
                ++failures;
            }
        }

        std::map<int, agg> LM, NAT;
        for (int32_t l : labels)
            if (l >= 0) ++LM[l].land_tiles;
        for (std::size_t i = 0; i < R.size(); ++i)
        {
            const region& r = R[i];
            const long long emp = std::min<long long>(r.urban_population,
                                                      out->works.employed_heads_mask(r.works_built));
            for (agg* a : {&LM[reg_lm[i]], &NAT[pol[i]]})
            {
                ++a->regions;
                a->pop += r.population;
                a->urban += r.urban_population;
                a->employed += r.centres > 0 ? emp : 0;
                a->centres += r.centres;
                a->works += std::popcount(r.works_built);
                a->ip += r.industry_points;
                a->ip_treasury += r.industry_points_from_treasury;
                if (r.centres > 0)
                {
                    a->fuel_wsum += static_cast<long long>(industry_fuel_reading_q(r)) * std::max<long long>(emp, 1);
                    a->fuel_w += std::max<long long>(emp, 1);
                }
            }
        }
        // Cities: centres of scale >= 3 on the home body, by tile.
        for (const auto& [id, c] : w.population_centres)
        {
            if (c.scale < 3) continue;
            const auto t = w.population_centre_tile.find(id);
            if (t == w.population_centre_tile.end()) continue;
            const int l = tile_lm(t->second);
            if (l == -2) continue;
            ++LM[l].cities;
            const auto cs = w.gen_carve_centres.find(id);
            if (cs != w.gen_carve_centres.end() && cs->second.region >= 0 &&
                cs->second.region < static_cast<int>(R.size()))
                ++NAT[pol[cs->second.region]].cities;
        }
        // Charter budget by centre -> region.
        for (const auto& [cid, pts] : out->land.stockpile.budget.points())
        {
            const auto cs = w.gen_carve_centres.find(cid);
            if (cs == w.gen_carve_centres.end() || cs->second.region < 0 ||
                cs->second.region >= static_cast<int>(R.size())) continue;
            LM[reg_lm[cs->second.region]].budget += pts;
            NAT[pol[cs->second.region]].budget += pts;
        }
        // Firms and specialists by origin region.
        auto corp_region = [&](entity_id cid) -> int {
            const auto c = w.corporations.find(cid);
            if (c == w.corporations.end()) return -1;
            const int o = c->second.origin_region;
            return (o >= 0 && o < static_cast<int>(R.size())) ? o : -1;
        };
        long long firms_unplaced = 0;
        for (entity_id f : out->land.firms)
        {
            const int o = corp_region(f);
            if (o < 0) { ++firms_unplaced; continue; }
            ++LM[reg_lm[o]].firms;
            ++NAT[pol[o]].firms;
        }
        for (entity_id f : out->land.specialists)
        {
            const int o = corp_region(f);
            if (o < 0) { ++firms_unplaced; continue; }
            ++LM[reg_lm[o]].specialists;
            ++NAT[pol[o]].specialists;
        }
        for (const spawn_seat_candidate& c : seat.candidates)
        {
            const int o = corp_region(c.corp);
            if (o < 0) continue;
            ++LM[reg_lm[o]].seats;
            ++NAT[pol[o]].seats;
        }
        // Buildings at the start, by tile (landmass) and owner's origin (nation).
        for (const auto& [bid, b] : w.buildings)
        {
            const int l = tile_lm(b.tile);
            if (l == -2) continue;
            ++LM[l].buildings;
            const int o = corp_region(owner_corp_of(w, bid));
            if (o >= 0) ++NAT[pol[o]].buildings;
        }
        for (const polity& q : fx.industrialisation_handoff.polities)
            if (q.alive && q.capital >= 0 && q.capital < static_cast<int>(R.size())) ++LM[reg_lm[q.capital]].polities;
        LM.erase(-1);

        long long tot_ip = 0, tot_firms = 0, tot_bld = 0, tot_land = 0;
        for (const auto& [k, a] : LM)
        {
            tot_ip += a.ip;
            tot_firms += a.firms + a.specialists;
            tot_bld += a.buildings;
            tot_land += a.land_tiles;
        }

        for (const auto& [k, a] : LM)
        {
            if (a.pop == 0 && a.ip == 0 && a.buildings == 0) continue;
            std::printf("LM seed=%u lm=%d land=%lld land_share=%.3f regions=%lld pop=%lld urban=%lld employed=%lld "
                        "cities=%lld works=%lld ip=%lld ip_share=%.3f ip_treas=%lld budget=%lld firms=%lld "
                        "specialists=%lld buildings=%lld seats=%lld polities=%lld\n",
                        seed, k, a.land_tiles, share(a.land_tiles, tot_land), a.regions, a.pop, a.urban,
                        a.employed, a.cities, a.works, a.ip, share(a.ip, tot_ip), a.ip_treasury, a.budget,
                        a.firms, a.specialists, a.buildings, a.seats, a.polities);
        }

        // Nations: the capital region of each (is_seat, nation == id).
        std::map<int, int> cap;
        const auto& polities = fx.industrialisation_handoff.polities;
        for (const polity& q : polities)
            if (q.alive && q.capital >= 0 && q.capital < static_cast<int>(R.size())) cap[q.id] = q.capital;
        auto nat_lm = [&](int n) -> int {
            const auto c = cap.find(n);
            if (c != cap.end()) return reg_lm[c->second];
            // Fallback: the landmass holding most of the nation's points.
            std::map<int, long long> by;
            for (std::size_t i = 0; i < R.size(); ++i)
                if (pol[i] == n) by[reg_lm[i]] += R[i].industry_points + 1;
            int best = -1; long long bv = -1;
            for (auto [l, v] : by) if (v > bv) { bv = v; best = l; }
            return best;
        };
        auto nat_pos = [&](int n, int& col, int& row) -> bool {
            const auto c = cap.find(n);
            if (c == cap.end()) return false;
            col = R[c->second].col; row = R[c->second].row; return true;
        };
        auto tree_q = [&](int n) -> int {
            if (n < 0 || n >= static_cast<int>(polities.size())) return 0;
            return industry_tree_industrial_q(polities[n].industry_mask);
        };
        NAT.erase(-1);
        const auto nat_rank = ranked(NAT, &agg::ip);
        long long nat_tot = 0;
        for (auto [v, k] : nat_rank) nat_tot += v;
        for (std::size_t i = 0; i < nat_rank.size() && i < 5; ++i)
        {
            const int n = nat_rank[i].second;
            const agg& a = NAT[n];
            std::printf("NAT seed=%u rank=%zu nation=%d lm=%d regions=%lld pop=%lld urban=%lld employed=%lld "
                        "cities=%lld works=%lld ip=%lld ip_share=%.3f ip_treas=%lld budget=%lld firms=%lld "
                        "specialists=%lld buildings=%lld seats=%lld tree_q=%d fuel=%lld ind_year=%lld\n",
                        seed, i + 1, n, nat_lm(n), a.regions, a.pop, a.urban, a.employed, a.cities, a.works,
                        a.ip, share(a.ip, nat_tot), a.ip_treasury, a.budget, a.firms, a.specialists,
                        a.buildings, a.seats, tree_q(n), a.fuel_w ? a.fuel_wsum / a.fuel_w : -1,
                        (n >= 0 && n < static_cast<int>(polities.size())) ? (long long)polities[n].industrial_year : -1);
        }

        // Concentration.
        const auto lm_rank = ranked(LM, &agg::ip);
        const int top_lm = lm_rank.empty() ? -1 : lm_rank[0].second;
        const agg& T = LM[top_lm];
        const double lm_ratio = lm_rank.size() > 1 && lm_rank[0].first > 0 ? double(lm_rank[1].first) / lm_rank[0].first : 0;
        const double nat_ratio = nat_rank.size() > 1 && nat_rank[0].first > 0 ? double(nat_rank[1].first) / nat_rank[0].first : 0;

        // The far rival.
        const int lead = nat_rank.empty() ? -1 : nat_rank[0].second;
        const int lead_lm = nat_lm(lead);
        int lc = 0, lr = 0;
        const bool lpos = nat_pos(lead, lc, lr);
        const int far_n = gw / 4;
        int rival_lm = -1, rival_far = -1;
        for (std::size_t i = 1; i < nat_rank.size(); ++i)
        {
            const int n = nat_rank[i].second;
            if (rival_lm < 0 && nat_lm(n) != lead_lm) rival_lm = n;
            int c = 0, r = 0;
            if (rival_far < 0 && lpos && nat_pos(n, c, r))
            {
                int dx = std::abs(c - lc); dx = std::min(dx, gw - dx);
                const int d = std::max(dx, std::abs(r - lr));
                if (nat_lm(n) != lead_lm || d >= far_n) rival_far = n;
            }
        }
        const long long lead_ip = lead >= 0 ? NAT[lead].ip : 0;
        const long long rlm_ip = rival_lm >= 0 ? NAT[rival_lm].ip : 0;
        const long long rfar_ip = rival_far >= 0 ? NAT[rival_far].ip : 0;
        // Leader's landmass vs the top landmass that is not the leader's.
        long long other_lm_best = 0;
        for (auto [v, k] : lm_rank) if (k != lead_lm) { other_lm_best = v; break; }

        // Seats on the leading landmass.
        const long long seats_top = T.seats;
        long long seats_all = 0;
        for (const auto& [k, a] : LM) seats_all += a.seats;

        std::printf("SEED seed=%u landmasses=%zu top_lm=%d top_land_share=%.3f top_ip_share=%.3f top_firm_share=%.3f "
                    "top_bld_share=%.3f hhi_lm=%.3f hhi_nat=%.3f lm2_over_lm1=%.3f nat2_over_nat1=%.3f "
                    "lead=%d lead_lm=%d lead_nat_share=%.3f rival_other_lm=%d rival_other_lm_ratio=%.3f "
                    "rival_far=%d rival_far_ratio=%.3f other_lm_best_over_lead_lm=%.3f seats=%lld seats_top_lm=%lld "
                    "firms_unplaced=%lld firms_total=%lld bld_total=%lld\n",
                    seed, lm_rank.size(), top_lm, share(T.land_tiles, tot_land), share(T.ip, tot_ip),
                    share(T.firms + T.specialists, tot_firms), share(T.buildings, tot_bld), hhi(LM, &agg::ip),
                    hhi(NAT, &agg::ip), lm_ratio, nat_ratio, lead, lead_lm, share(lead_ip, nat_tot), rival_lm,
                    lead_ip ? double(rlm_ip) / lead_ip : 0, rival_far, lead_ip ? double(rfar_ip) / lead_ip : 0,
                    LM[lead_lm].ip ? double(other_lm_best) / LM[lead_lm].ip : 0, seats_all, seats_top,
                    firms_unplaced, tot_firms, tot_bld, ip_mismatch);

        // ZONE, for one-landmass worlds: the densest disc (Chebyshev radius
        // gw/8, columns wrapping -- a quarter of the world's width across)
        // around any centred region, its share of the world's points and land.
        {
            const int rad = std::max(1, gw / 8);
            long long best = -1, best_land = 0;
            for (std::size_t i = 0; i < R.size(); ++i)
            {
                if (R[i].centres <= 0) continue;
                long long s = 0;
                for (std::size_t j = 0; j < R.size(); ++j)
                {
                    int dx = std::abs(R[i].col - R[j].col); dx = std::min(dx, gw - dx);
                    if (std::max(dx, std::abs(R[i].row - R[j].row)) <= rad) s += R[j].industry_points;
                }
                if (s > best)
                {
                    best = s;
                    best_land = 0;
                    for (int y = std::max(0, R[i].row - rad); y <= std::min(gh - 1, R[i].row + rad); ++y)
                        for (int dx = -rad; dx <= rad; ++dx)
                        {
                            const int x = ((R[i].col + dx) % gw + gw) % gw;
                            if (labels[static_cast<std::size_t>(y * gw + x)] >= 0) ++best_land;
                        }
                }
            }
            // CANDIDATE C, computed off the budget (no world change): a seat's
            // price read against the stock of the LANDMASS the centre stands on
            // (its trade reach) rather than the world's. Counts centres whose
            // budget covers k_stockpile_specialist_firm_charters x price -- the
            // affordability test only, so it is compared against the same count
            // at the world's price, never against the placed menu.
            const long long div = out->land.stockpile.price_divisor > 0 ? out->land.stockpile.price_divisor
                                                                         : k_stockpile_price_divisor;
            long long afford_world = 0, afford_lm = 0, afford_world_top = 0, afford_lm_top = 0;
            for (const auto& [cid, pts] : out->land.stockpile.budget.points())
            {
                const auto cs = w.gen_carve_centres.find(cid);
                if (cs == w.gen_carve_centres.end() || cs->second.region < 0 ||
                    cs->second.region >= static_cast<int>(R.size())) continue;
                const int l = reg_lm[cs->second.region];
                const long long pw = std::max<long long>(1, tot_ip / div);
                const long long pl = std::max<long long>(1, (l >= 0 ? LM[l].ip : tot_ip) / div);
                if (pts >= k_stockpile_specialist_firm_charters * pw) { ++afford_world; if (l == top_lm) ++afford_world_top; }
                if (pts >= k_stockpile_specialist_firm_charters * pl) { ++afford_lm; if (l == top_lm) ++afford_lm_top; }
            }
            long long big_lms = 0, emp_top = T.employed, emp_tot = 0;
            for (const auto& [k, a] : LM) { if (a.land_tiles * 20 >= tot_land) ++big_lms; emp_tot += a.employed; }
            std::printf("ZONE seed=%u big_landmasses=%lld disc_ip_share=%.3f disc_land_share=%.3f "
                        "top_lm_employed_share=%.3f afford_world=%lld/%lld afford_lm_price=%lld/%lld\n",
                        seed, big_lms, share(best, tot_ip), share(best_land, tot_land), share(emp_top, emp_tot),
                        afford_world_top, afford_world, afford_lm_top, afford_lm);
        }

        // THE TREASURY'S SOURCES by landmass at the close (the earn reads trade
        // volume through the capital, corridors touched, a flat endowment; the
        // bills read the standing army): the last round's flows credited to
        // each capital, corridors with an end on the landmass, army heads.
        {
            std::map<int, long long> vol, vol_sea, corr, army;
            auto cap_lm = [&](int pid) -> int {
                const auto c = cap.find(pid);
                return c == cap.end() ? -1 : reg_lm[c->second];
            };
            for (const trade_flow& f : fx.industrialisation_state.trade_flows)
                for (int pid : {static_cast<int>(f.seller), static_cast<int>(f.buyer)})
                {
                    vol[cap_lm(pid)] += f.volume_q;
                    if (f.by_sea) vol_sea[cap_lm(pid)] += f.volume_q;
                }
            for (const history_corridor& c : fx.industrialisation_state.supply_corridors)
                if (c.a >= 0 && static_cast<std::size_t>(c.a) < R.size()) ++corr[reg_lm[c.a]];
            for (std::size_t i = 0; i < R.size(); ++i) army[reg_lm[i]] += R[i].army_stock;
            for (const auto& [k, a] : LM)
            {
                if (a.pop < 1000000) continue;
                std::printf("EARN seed=%u lm=%d polities=%lld trade_vol=%lld sea_vol=%lld corridors=%lld "
                            "army=%lld ip_treas=%lld ip_scale=%lld\n",
                            seed, k, a.polities, vol[k], vol_sea[k], corr[k], army[k], a.ip_treasury,
                            a.ip - a.ip_treasury);
            }
        }

        // The trace: polity points at each step near 1660/1700/1800/1900/1960,
        // for the leader, the second, and the far rival.
        const era_timelapse* tl = nullptr;
        for (const auto& b : out->report.bodies)
            if (b.id == home) tl = &b.industrialisation_timelapse;
        // 1660 state off the span-open regions, by polity.
        std::map<int, agg> OPEN;
        for (const region& r : fx.industrialisation_open_regions)
        {
            agg& a = OPEN[r.nation];
            a.urban += r.urban_population;
            a.employed += r.centres > 0 ? std::min<long long>(r.urban_population, out->works.employed_heads_mask(r.works_built)) : 0;
            a.works += std::popcount(r.works_built);
            a.pop += r.population;
        }
        const int second = nat_rank.size() > 1 ? nat_rank[1].second : -1;
        for (int who : {lead, second, rival_far})
        {
            if (who < 0) continue;
            std::string row = "TRACE seed=" + std::to_string(seed) + " nation=" + std::to_string(who) +
                              " role=" + (who == lead ? "lead" : who == second ? "second" : "far");
            const agg& o = OPEN[who];
            row += " open_urban=" + std::to_string(o.urban) + " open_employed=" + std::to_string(o.employed) +
                   " open_works=" + std::to_string(o.works);
            const agg& a = NAT[who];
            row += " end_urban=" + std::to_string(a.urban) + " end_employed=" + std::to_string(a.employed) +
                   " end_works=" + std::to_string(a.works) + " end_treas_share=" +
                   std::to_string(a.ip ? (a.ip_treasury * 1000 / a.ip) : 0) + " tree_q=" + std::to_string(tree_q(who));
            if (tl)
                for (int y : {1700, 1750, 1800, 1850, 1900, 1930, 1960})
                {
                    const timelapse_step* best = nullptr;
                    for (const timelapse_step& s : tl->steps)
                        if (s.year <= y) best = &s;
                    long long v = 0, wtot = 0;
                    if (best)
                        for (int k = best->first_sample; k < best->first_sample + best->sample_count; ++k)
                        {
                            wtot += tl->samples[k].industry_points;
                            if (tl->samples[k].polity == who) v = tl->samples[k].industry_points;
                        }
                    row += " y" + std::to_string(y) + "=" + std::to_string(v) + "/" +
                           std::to_string(wtot ? v * 1000 / wtot : 0);
                }
            std::printf("%s\n", row.c_str());
        }
        std::fflush(stdout);
    }
    // BL-1176: a row that compared no centre on any seed proved nothing.
    if (note_price_compared == 0)
    {
        std::printf("FAIL  NOTEPRICE: no budgeted centre was compared on any seed\n");
        ++failures;
    }
    std::printf("%s (%d failure%s)\n", failures ? "FAIL" : "PASS", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
