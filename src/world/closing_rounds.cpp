// closing_rounds.cpp — the size floor and the total, inside the history
// (BL-1278). See closing_rounds.hpp for what this answers and why it is pure.

#include "world/closing_rounds.hpp"

#include <algorithm>
#include <climits>

namespace {

/// One nation-to-be: a realm (`polity >= 0`) or a lone cell no realm held
/// (`lone >= 0`). `anchored` are the regions with a carve cell it holds.
struct fold_group
{
    int              polity = -1;
    int              lone   = -1;
    int64_t          tiles  = 0;
    int              lowest = INT_MAX; ///< Lowest anchored region: the tie-break, as the seed index is the carve's.
    bool             alive  = false;
    bool             exempt = false;   ///< A city state: the floor steps past it.
    std::vector<int> anchored;
};

} // namespace

std::vector<int> closing_carve_cells(const closing_rounds_spec& spec, const std::vector<region>& regions)
{
    std::vector<int> anchors, region_of;
    anchors.reserve(regions.size());
    for (std::size_t ri = 0; ri < regions.size(); ++ri)
        if (regions[ri].anchor >= 0)
        {
            anchors.push_back(regions[ri].anchor);
            region_of.push_back(static_cast<int>(ri));
        }
    std::vector<int> cells = preview_nation_carve(spec.ground, anchors, spec.nation_seed);
    for (int& k : cells)
        k = (k >= 0 && k < static_cast<int>(region_of.size())) ? region_of[static_cast<std::size_t>(k)] : -1;
    return cells;
}

std::vector<int64_t> closing_cell_tiles(const std::vector<int>& cell_region, std::size_t region_count)
{
    std::vector<int64_t> tiles(region_count, 0);
    for (const int r : cell_region)
        if (r >= 0 && static_cast<std::size_t>(r) < region_count) ++tiles[static_cast<std::size_t>(r)];
    return tiles;
}

int predict_fold_count(const std::vector<region>& regions, const std::vector<int>& cell_region)
{
    const std::vector<int64_t> tiles = closing_cell_tiles(cell_region, regions.size());
    int max_p = -1;
    for (std::size_t r = 0; r < regions.size(); ++r)
        if (tiles[r] > 0 && regions[r].nation > max_p) max_p = regions[r].nation;
    std::vector<char> seen(static_cast<std::size_t>(max_p + 1), 0);
    int count = 0;
    for (std::size_t r = 0; r < regions.size(); ++r)
    {
        if (tiles[r] <= 0) continue;
        const int p = regions[r].nation;
        if (p < 0) { ++count; continue; }
        if (!seen[static_cast<std::size_t>(p)]) { seen[static_cast<std::size_t>(p)] = 1; ++count; }
    }
    return count;
}

closing_plan plan_closing_rounds(const std::vector<region>& regions,
                                 const std::vector<polity>& polities,
                                 const std::vector<int>&    cell_region,
                                 int gw, int gh,
                                 int min_tiles, bool keep_city_states, int total_target,
                                 const std::vector<terrain_substrate>* sub)
{
    closing_plan plan;
    const std::size_t R = regions.size();
    const int total = (gw > 0 && gh > 0) ? gw * gh : 0;
    const bool grid_ok = total > 0 && cell_region.size() >= static_cast<std::size_t>(total);
    const bool strait_rule = sub != nullptr && grid_ok && sub->size() >= static_cast<std::size_t>(total);

    // THE CARVE AS THE ROUNDS LEAVE IT. A copy of the oracle the cessions
    // relabel (§ Pass 2e); everything after them reads this one.
    std::vector<int> cells = cell_region;
    std::vector<char> water(static_cast<std::size_t>(total), 0);
    if (strait_rule)
        for (int t = 0; t < total; ++t) water[static_cast<std::size_t>(t)] = is_water((*sub)[static_cast<std::size_t>(t)]) ? 1 : 0;

    // --- The plan's own copy of who holds what ------------------------------
    std::vector<int>  cur_polity(R), cur_seat(R);
    std::vector<char> seat_flag(R);
    int max_p = static_cast<int>(polities.size()) - 1;
    for (std::size_t r = 0; r < R; ++r)
    {
        cur_polity[r] = regions[r].nation;
        cur_seat[r]   = regions[r].seat_region;
        seat_flag[r]  = regions[r].is_seat ? 1 : 0;
        if (regions[r].nation > max_p) max_p = regions[r].nation;
    }
    std::vector<int> cap(static_cast<std::size_t>(max_p + 1), -1);
    for (std::size_t p = 0; p < polities.size(); ++p) cap[p] = polities[p].capital;
    int next_id = static_cast<int>(polities.size()); // the id the sim's next push takes
    // A realm's CURRENT seat: its capital where it holds it, else the lowest
    // region it holds -- never a stale capital another realm now owns.
    const auto seat_of = [&](int p) -> int {
        if (p < 0) return -1;
        const int c = static_cast<std::size_t>(p) < cap.size() ? cap[static_cast<std::size_t>(p)] : -1;
        if (c >= 0 && static_cast<std::size_t>(c) < R && cur_polity[static_cast<std::size_t>(c)] == p) return c;
        for (std::size_t h = 0; h < R; ++h)
            if (cur_polity[h] == p) return static_cast<int>(h);
        return -1;
    };
    const auto anchor_of = [&](int r) { return regions[static_cast<std::size_t>(r)].anchor; };
    // A region whose cell still holds its own anchor: a seed of the fold.
    const auto live_cell = [&](int r) {
        const int a = anchor_of(r);
        return a >= 0 && a < total && grid_ok && cells[static_cast<std::size_t>(a)] == r;
    };

    // --- THE CESSIONS (BL-1310; NATION_GENERATION.md § Pass 2e) -------------
    // Owner keys at the fold's grain: realm p -> p; a region no realm holds is
    // a nation of its own -> KP + r. The rank is the region the fold's
    // representative will be (the lowest live region), so ties fall as the
    // tail's nation indices do. A realm's core is its capital's group.
    // KP grows with the successors a secession founds (the late repair).
    int KP = max_p + 1;
    int K  = KP + static_cast<int>(R);
    const auto key_of = [&](int r) { return cur_polity[static_cast<std::size_t>(r)] >= 0 ? cur_polity[static_cast<std::size_t>(r)] : KP + r; };
    std::vector<int> own(static_cast<std::size_t>(total), -1), core_tile, order;
    std::vector<int> frozen_polity_core, frozen_lone_core(R, -1); // tiles, -1 = not yet read
    const auto build_keys = [&]() {
        KP = static_cast<int>(cap.size());
        for (std::size_t r = 0; r < R; ++r) KP = std::max(KP, cur_polity[r] + 1);
        K = KP + static_cast<int>(R);
        if (frozen_polity_core.size() < static_cast<std::size_t>(KP))
            frozen_polity_core.resize(static_cast<std::size_t>(KP), -1);
        for (int t = 0; t < total; ++t)
        {
            const int r = cells[static_cast<std::size_t>(t)];
            own[static_cast<std::size_t>(t)] =
                (!water[static_cast<std::size_t>(t)] && r >= 0 && static_cast<std::size_t>(r) < R) ? key_of(r) : -1;
        }
        core_tile.assign(static_cast<std::size_t>(K), -1);
        order.assign(static_cast<std::size_t>(K), INT_MAX);
        for (std::size_t r = 0; r < R; ++r)
        {
            if (!live_cell(static_cast<int>(r))) continue;
            const int k = key_of(static_cast<int>(r));
            if (order[static_cast<std::size_t>(k)] == INT_MAX) order[static_cast<std::size_t>(k)] = static_cast<int>(r);
        }
        for (int k = 0; k < K; ++k)
        {
            if (order[static_cast<std::size_t>(k)] == INT_MAX) continue;
            // A CORE IS FROZEN the first time it is read and carried through
            // every receive (BL-1310 review): re-deriving it per cession let a
            // realm whose capital has no live cell (a region founded on a
            // coastal anchor never gets a seed) take its core from
            // `order[k]` -- and a low-numbered exclave it RECEIVED then became
            // the core, and its real mainland was ceded away province by
            // province. Re-read only where the frozen tile is no longer the
            // key's own (a realm that died, or a key re-used by a successor).
            int& frozen = k < KP ? frozen_polity_core[static_cast<std::size_t>(k)]
                                 : frozen_lone_core[static_cast<std::size_t>(k - KP)];
            if (frozen >= 0 && frozen < total && own[static_cast<std::size_t>(frozen)] == k)
            {
                core_tile[static_cast<std::size_t>(k)] = frozen;
                continue;
            }
            int cr = order[static_cast<std::size_t>(k)];
            if (k < KP)
            {
                const int s = seat_of(k);
                if (s >= 0 && live_cell(s)) cr = s;
            }
            core_tile[static_cast<std::size_t>(k)] = anchor_of(cr);
            frozen = core_tile[static_cast<std::size_t>(k)];
        }
    };

    const auto run_cessions = [&](bool late) {
        if (!strait_rule) return;
        // Every cession moves one group and lowers (non-territory tiles, land
        // groups) lexicographically, so the walk ends; the cap is a seatbelt.
        for (int guard = 0; guard < total; ++guard)
        {
            build_keys();
            exclave_cession ex;
            if (!next_exclave_cession(own, water, gw, gh, core_tile, order, ex)) break;
            const int a = ex.from, b = ex.to;
            const bool b_realm = b < KP;
            const int q = cells[static_cast<std::size_t>(ex.contact)]; // the receiver's region at the contact
            const int ca = cells[static_cast<std::size_t>(core_tile[static_cast<std::size_t>(a)])]; // the cedent's core region
            const auto in_x = [&](int t) { return std::binary_search(ex.tiles.begin(), ex.tiles.end(), t); };

            // The provinces in the exclave: regions whose live cell's anchor lies in it.
            std::vector<int>  moved;
            std::vector<char> moving(R, 0);
            int lowest_ground = INT_MAX;
            for (const int t : ex.tiles) lowest_ground = std::min(lowest_ground, cells[static_cast<std::size_t>(t)]);
            for (std::size_t r = 0; r < R; ++r)
                if (live_cell(static_cast<int>(r)) && in_x(anchor_of(static_cast<int>(r))))
                {
                    moved.push_back(static_cast<int>(r));
                    moving[r] = 1;
                }

            // GROUND ONLY: the exclave moves, nothing else. A moved province's
            // cell ground OUTSIDE the exclave (an islet Pass 2b hung on it)
            // stays with the cedent, under its core region.
            for (int t = 0; t < total; ++t)
            {
                if (water[static_cast<std::size_t>(t)]) continue;
                const int r = cells[static_cast<std::size_t>(t)];
                if (r < 0 || !moving[static_cast<std::size_t>(r)] || in_x(t)) continue;
                cells[static_cast<std::size_t>(t)] = ca;
            }
            const int into_seat = b_realm ? seat_of(b) : -1;
            const int from_seat = a < KP ? seat_of(a) : -1;
            if (b_realm)
            {
                // A REALM RECEIVES: its provinces change hands as regions; any
                // other ground of the exclave joins the receiver's cell at the contact.
                for (const int t : ex.tiles)
                    if (!moving[static_cast<std::size_t>(cells[static_cast<std::size_t>(t)])]) cells[static_cast<std::size_t>(t)] = q;
                for (const int r : moved)
                {
                    cur_polity[static_cast<std::size_t>(r)] = b;
                    if (!seat_flag[static_cast<std::size_t>(r)]) cur_seat[static_cast<std::size_t>(r)] = into_seat;
                }
            }
            else
            {
                // GROUND NO REALM HELD RECEIVES. It cannot take a province as
                // a realm does (a region nobody holds is a nation of its own,
                // so a moved region would add one): the exclave joins the
                // unheld cell at the contact, and its provinces fall out of
                // every realm with no ground of their own left.
                ++plan.cessions_to_unheld;
                for (const int t : ex.tiles) cells[static_cast<std::size_t>(t)] = q;
                for (const int r : moved)
                {
                    cur_polity[static_cast<std::size_t>(r)] = -1;
                    if (!seat_flag[static_cast<std::size_t>(r)]) cur_seat[static_cast<std::size_t>(r)] = -1;
                }
            }
            // The cedent's ground that pointed at a departing seat re-points at its own.
            if (a < KP)
                for (std::size_t h = 0; h < R; ++h)
                    if (cur_polity[h] == a && cur_seat[h] >= 0 && moving[static_cast<std::size_t>(cur_seat[h])])
                        cur_seat[h] = from_seat;
            rederive_carve_water(cells, *sub, gw, gh);

            closing_step st;
            st.kind        = closing_step_kind::exclave_cede;
            st.from_polity = a < KP ? a : -1;
            st.into_polity = b_realm ? b : -1;
            st.region      = moved.empty() ? lowest_ground : moved.front();
            st.moved       = moved;
            st.late        = late;
            plan.ceded_regions.insert(plan.ceded_regions.end(), moved.begin(), moved.end());
            plan.steps.push_back(std::move(st));
            ++plan.cessions;
            if (moved.empty()) ++plan.cessions_islet;
            if (late) ++plan.cessions_late;
        }
    };
    // THE CORE STAYS PUT (the review's check): every key's frozen core group
    // as the cessions open, flooded on its own land; after them, no tile of it
    // may have changed hands. A cession only ever moves ground outside a
    // territory, so a lost core tile means a core moved under a realm.
    std::vector<std::pair<int, int>> core_snapshot; // (tile, key)
    if (strait_rule)
    {
        build_keys();
        std::vector<char> seen(static_cast<std::size_t>(total), 0);
        for (int k = 0; k < K; ++k)
        {
            const int c0 = core_tile[static_cast<std::size_t>(k)];
            if (c0 < 0 || c0 >= total || own[static_cast<std::size_t>(c0)] != k || seen[static_cast<std::size_t>(c0)]) continue;
            std::vector<int> st{ c0 };
            seen[static_cast<std::size_t>(c0)] = 1;
            while (!st.empty())
            {
                const int t = st.back();
                st.pop_back();
                core_snapshot.emplace_back(t, k);
                const int col = t % gw, row = t / gw;
                const int nb[4] = { row * gw + (col + 1) % gw, row * gw + (col - 1 + gw) % gw,
                                    row > 0 ? t - gw : -1, row + 1 < gh ? t + gw : -1 };
                for (const int u : nb)
                    if (u >= 0 && !seen[static_cast<std::size_t>(u)] && own[static_cast<std::size_t>(u)] == k)
                    {
                        seen[static_cast<std::size_t>(u)] = 1;
                        st.push_back(u);
                    }
            }
        }
    }
    run_cessions(false);
    if (strait_rule)
    {
        build_keys();
        for (const auto& [t, k] : core_snapshot)
            if (own[static_cast<std::size_t>(t)] != k) ++plan.core_tiles_lost;
    }

    const std::vector<int64_t> tiles = closing_cell_tiles(cells, R);

    // --- Cell adjacency: two regions whose carve cells touch cardinally -----
    // Column-wrapped, rows clamped, the carve's own neighbourhood
    // (`cardinal_neighbours`). Right and down cover every pair once.
    std::vector<std::vector<int>> adj(R);
    if (grid_ok)
    {
        for (int idx = 0; idx < gw * gh; ++idx)
        {
            const int a = cells[static_cast<std::size_t>(idx)];
            if (a < 0 || static_cast<std::size_t>(a) >= R) continue;
            const int col = idx % gw, row = idx / gw;
            const int nb[2] = { row * gw + (col + 1) % gw, row + 1 < gh ? idx + gw : -1 };
            for (const int n : nb)
            {
                if (n < 0) continue;
                const int b = cells[static_cast<std::size_t>(n)];
                if (b < 0 || b == a || static_cast<std::size_t>(b) >= R) continue;
                adj[static_cast<std::size_t>(a)].push_back(b);
                adj[static_cast<std::size_t>(b)].push_back(a);
            }
        }
        for (std::vector<int>& v : adj)
        {
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
        }
    }

    // --- The groups the fold would make --------------------------------------
    std::vector<fold_group> groups;
    std::vector<int> gid(R, -1);
    std::vector<int> polity_group(static_cast<std::size_t>(max_p + 1), -1);
    for (std::size_t r = 0; r < R; ++r)
    {
        if (tiles[r] <= 0) continue;
        const int p = cur_polity[r];
        int g = -1;
        if (p >= 0)
        {
            g = polity_group[static_cast<std::size_t>(p)];
            if (g < 0)
            {
                g = static_cast<int>(groups.size());
                fold_group ng;
                ng.polity = p;
                ng.alive  = true;
                groups.push_back(ng);
                polity_group[static_cast<std::size_t>(p)] = g;
            }
        }
        else
        {
            g = static_cast<int>(groups.size());
            fold_group ng;
            ng.lone  = static_cast<int>(r);
            ng.alive = true;
            groups.push_back(ng);
        }
        fold_group& gr = groups[static_cast<std::size_t>(g)];
        gr.tiles += tiles[r];
        gr.lowest = std::min(gr.lowest, static_cast<int>(r));
        gr.anchored.push_back(static_cast<int>(r));
        gid[r] = g;
    }
    int N = static_cast<int>(groups.size());
    plan.nations_before = N;

    // A CITY STATE, at the history's grain: a realm whose fold holds one
    // anchored region, with a settled centre standing on its ground
    // (NATION_GENERATION.md § Pass 2d — "a polity that reached the close
    // holding one region with a population centre on it"). The centres are
    // the region's own (`region::centres`), the history's record of the town
    // the campaign-era centre is later stood from.
    if (keep_city_states)
    {
        std::vector<char> has_centre(static_cast<std::size_t>(max_p + 1), 0);
        for (std::size_t r = 0; r < R; ++r)
            if (cur_polity[r] >= 0 && regions[r].population > 0 && regions[r].centres > 0)
                has_centre[static_cast<std::size_t>(cur_polity[r])] = 1;
        for (fold_group& g : groups)
            if (g.polity >= 0 && g.anchored.size() == 1 && has_centre[static_cast<std::size_t>(g.polity)])
                g.exempt = true;
    }

    const auto larger = [&](int a, int b) { // is group a preferred over b as the larger?
        if (b < 0) return true;
        const fold_group& x = groups[static_cast<std::size_t>(a)];
        const fold_group& y = groups[static_cast<std::size_t>(b)];
        return x.tiles != y.tiles ? x.tiles > y.tiles : x.lowest < y.lowest;
    };
    const auto smallest = [&](bool exempt_ones) {
        int s = -1;
        for (int g = 0; g < static_cast<int>(groups.size()); ++g)
        {
            const fold_group& x = groups[static_cast<std::size_t>(g)];
            if (!x.alive || x.exempt != exempt_ones) continue;
            if (s < 0) { s = g; continue; }
            const fold_group& y = groups[static_cast<std::size_t>(s)];
            if (x.tiles != y.tiles ? x.tiles < y.tiles : x.lowest < y.lowest) s = g;
        }
        return s;
    };
    // Land tiles by fold group, for the water reads below.
    const auto group_owner = [&]() {
        std::vector<int> o(static_cast<std::size_t>(total), -1);
        for (int t = 0; t < total; ++t)
        {
            const int r = cells[static_cast<std::size_t>(t)];
            if (water[static_cast<std::size_t>(t)] || r < 0 || static_cast<std::size_t>(r) >= R) continue;
            o[static_cast<std::size_t>(t)] = gid[static_cast<std::size_t>(r)];
        }
        return o;
    };

    // WHO ABSORBS `s`: (absorbed, absorber) groups, or (-1, -1). The largest
    // neighbour across a cell border (ties: the lower anchored region, the
    // carve's own seed order). A neighbour no realm held cannot absorb, so a
    // REALM beside it takes that ground instead — the same union, the realm
    // keeping its name. Ownerless ground beside no realm goes to the largest
    // realm it touches, then -- the island case -- to the NEAREST realm across
    // the water (BL-1310, § Pass 2e: "never the globally largest"), the
    // largest realm on the body only where the water reaches none.
    const auto choose = [&](int s, bool allow_exempt) -> std::pair<int, int> {
        std::vector<int> nbr;
        for (const int r : groups[static_cast<std::size_t>(s)].anchored)
            for (const int n : adj[static_cast<std::size_t>(r)])
            {
                const int g = gid[static_cast<std::size_t>(n)];
                if (g >= 0 && g != s && groups[static_cast<std::size_t>(g)].alive) nbr.push_back(g);
            }
        std::sort(nbr.begin(), nbr.end());
        nbr.erase(std::unique(nbr.begin(), nbr.end()), nbr.end());
        const auto usable = [&](int g) { return allow_exempt || !groups[static_cast<std::size_t>(g)].exempt; };
        int best = -1, best_realm = -1;
        for (const int g : nbr)
        {
            if (!usable(g)) continue;
            if (larger(g, best)) best = g;
            if (groups[static_cast<std::size_t>(g)].polity >= 0 && larger(g, best_realm)) best_realm = g;
        }
        const bool s_realm = groups[static_cast<std::size_t>(s)].polity >= 0;
        if (best >= 0)
        {
            if (groups[static_cast<std::size_t>(best)].polity >= 0) return {s, best};
            if (s_realm) return {best, s};                 // the realm takes the unheld ground beside it
            if (best_realm >= 0) return {s, best_realm};
        }
        if (strait_rule)
        {
            std::vector<char> eligible(groups.size(), 0);
            std::vector<int>  rank(groups.size(), INT_MAX);
            for (int g = 0; g < static_cast<int>(groups.size()); ++g)
            {
                const fold_group& G = groups[static_cast<std::size_t>(g)];
                rank[static_cast<std::size_t>(g)] = G.lowest;
                eligible[static_cast<std::size_t>(g)] = (g != s && G.alive && usable(g) && G.polity >= 0) ? 1 : 0;
            }
            const int near = nearest_key_across_water(group_owner(), water, gw, gh, s, eligible, rank);
            if (near >= 0) return {s, near};
        }
        int global = -1;
        for (int g = 0; g < static_cast<int>(groups.size()); ++g)
        {
            if (g == s || !groups[static_cast<std::size_t>(g)].alive || !usable(g)) continue;
            if (groups[static_cast<std::size_t>(g)].polity < 0) continue;
            if (larger(g, global)) global = g;
        }
        if (global >= 0) return {s, global};
        return {-1, -1};
    };

    const auto absorb = [&](int x, int a, closing_step_kind kind, bool annex) {
        fold_group& X = groups[static_cast<std::size_t>(x)];
        fold_group& A = groups[static_cast<std::size_t>(a)];
        closing_step st;
        st.kind        = kind;
        st.annex       = annex;
        st.from_polity = X.polity;
        st.into_polity = A.polity;
        st.region      = X.polity >= 0 ? seat_of(X.polity) : X.lone;
        if (X.polity >= 0)
        {
            for (std::size_t h = 0; h < R; ++h)
                if (cur_polity[h] == X.polity) st.moved.push_back(static_cast<int>(h));
        }
        else
            st.moved.push_back(X.lone);
        for (const int h : st.moved)
        {
            cur_polity[static_cast<std::size_t>(h)] = A.polity;
            if (X.polity < 0 && !seat_flag[static_cast<std::size_t>(h)])
                cur_seat[static_cast<std::size_t>(h)] = seat_of(A.polity);
        }
        if (st.region < 0 && !st.moved.empty()) st.region = st.moved.front();
        A.tiles += X.tiles;
        A.lowest = std::min(A.lowest, X.lowest);
        for (const int r : X.anchored)
        {
            A.anchored.push_back(r);
            gid[static_cast<std::size_t>(r)] = a;
        }
        A.exempt = false; // an absorber is no longer one region's city
        if (X.polity >= 0) polity_group[static_cast<std::size_t>(X.polity)] = -1;
        X.alive = false;
        X.anchored.clear();
        X.tiles = 0;
        --N;
        if (kind == closing_step_kind::floor_absorb) ++plan.floor_absorptions;
        else                                          ++plan.target_absorptions;
        plan.steps.push_back(std::move(st));
    };

    // --- THE FLOOR (Pass 2c's rule, at the history's grain) ------------------
    // The smallest non-exempt nation below the floor is absorbed, until the
    // smallest clears it or only one is left — the post-pass's loop, step for
    // step, over the fold's groups instead of the carve's owner map.
    if (min_tiles > 0)
    {
        while (N > 1)
        {
            const int s = smallest(false);
            if (s < 0) break;
            if (groups[static_cast<std::size_t>(s)].tiles >= min_tiles) break;
            const std::pair<int, int> xa = choose(s, false);
            if (xa.first < 0) break;
            absorb(xa.first, xa.second, closing_step_kind::floor_absorb, xa.first != s);
        }
    }
    plan.nations_after_floor = N;

    if (total_target > 0)
    {
        // --- TOO MANY: absorption, the smallest first ------------------------
        // City states last: they are spared while any other nation can go.
        while (N > total_target)
        {
            int s = smallest(false);
            if (s < 0) s = smallest(true);
            if (s < 0) break;
            std::pair<int, int> xa = choose(s, false);
            if (xa.first < 0) xa = choose(s, true);
            if (xa.first < 0) break;
            absorb(xa.first, xa.second, closing_step_kind::target_absorb, false);
        }

        // --- TOO FEW: a province secedes --------------------------------------
        // The province is a region with a cell, held by a realm that keeps
        // another, never the realm's seat and never empty ground -- and, where
        // its own cell is under the floor, the neighbouring cells of the same
        // realm it carries with it (adjacent across a cell border, lowest
        // `network_supply_q` first, then the lower index), never so many that
        // the realm keeps no cell. Every prefix of that growth is a candidate.
        //
        // ONE TERRITORY FIRST (BL-1310, § Pass 2e: "a secession is chosen so
        // both sides remain one territory"): a split leaving the successor and
        // the realm each one territory is always preferred where one exists.
        //
        // THE TOTAL OUTRANKS THE FLOOR (Ben, 2026-10-11; NATION_GENERATION.md
        // sec The nation count is a target). A FLOOR-SAFE split -- the block
        // and what the realm keeps both clear the floor -- is always preferred
        // where one exists. Where none does, the split with the SMALLEST
        // SHORTFALL below the floor (summed over both sides) is taken, so the
        // total is still hit; only "no split of any kind" (every realm holds
        // one anchored region) stops the rounds short, and the caller records
        // that miss. Tie order: one territory; shortfall; then the fewest
        // regions; then the ground furthest from its seat's reach (the sim's
        // own secession reading); then the larger block; then the lower index.
        // A split that could not keep both sides one territory is repaired by
        // a late cession after the rounds.
        std::vector<std::vector<int>> region_land(R);
        if (strait_rule)
            for (int t = 0; t < total; ++t)
            {
                const int r = cells[static_cast<std::size_t>(t)];
                if (!water[static_cast<std::size_t>(t)] && r >= 0 && static_cast<std::size_t>(r) < R)
                    region_land[static_cast<std::size_t>(r)].push_back(t);
            }
        bool any_split = false;
        while (N < total_target)
        {
            if (static_cast<std::size_t>(next_id) >= owner_index_limit) break;
            std::vector<int> gown;
            if (strait_rule) gown = group_owner();
            const int scratch_key = static_cast<int>(groups.size());
            int              best = -1;
            std::vector<int> best_block;
            int64_t          best_tiles = 0;
            int64_t          best_short = 0;
            int              best_supply = 0;
            bool             best_whole = false;
            for (std::size_t r = 0; r < R; ++r)
            {
                const int g = gid[r];
                if (g < 0) continue;
                const fold_group& G = groups[static_cast<std::size_t>(g)];
                const int p = G.polity;
                if (p < 0 || G.anchored.size() < 2) continue;
                const int pseat = seat_of(p);
                if (static_cast<int>(r) == pseat) continue;
                if (regions[r].population <= 0) continue;
                const int supply = regions[r].network_supply_q;
                std::vector<int> block{static_cast<int>(r)};
                int64_t bt = tiles[r];
                for (;;)
                {
                    // This prefix as a candidate.
                    const int64_t keep = G.tiles - bt;
                    const int64_t shortfall =
                        min_tiles > 0 ? std::max<int64_t>(0, min_tiles - bt) + std::max<int64_t>(0, min_tiles - keep)
                                      : 0;
                    // The order without the one-territory key: does this
                    // prefix beat the incumbent on everything after it?
                    const bool rest_better =
                        best < 0
                        || shortfall < best_short
                        || (shortfall == best_short
                            && (block.size() < best_block.size()
                                || (block.size() == best_block.size()
                                    && (supply < best_supply || (supply == best_supply && bt > best_tiles)))));
                    // Both sides one territory? Read on the group map with the
                    // block relabelled to a scratch key, then restored. Read
                    // ONLY when it can decide the order (BL-1310 review, the
                    // cost): against a one-territory incumbent a prefix that
                    // loses on the rest cannot win, whatever it reads.
                    const bool can_win = best < 0 || !best_whole || rest_better;
                    bool whole = true;
                    if (strait_rule && can_win)
                    {
                        for (const int m : block)
                            for (const int t : region_land[static_cast<std::size_t>(m)]) gown[static_cast<std::size_t>(t)] = scratch_key;
                        int parent_core = -1;
                        if (pseat >= 0 && live_cell(pseat)) parent_core = anchor_of(pseat);
                        whole = key_is_one_territory(gown, water, gw, gh, scratch_key, anchor_of(static_cast<int>(r)))
                             && key_is_one_territory(gown, water, gw, gh, g, parent_core);
                        for (const int m : block)
                            for (const int t : region_land[static_cast<std::size_t>(m)]) gown[static_cast<std::size_t>(t)] = g;
                    }
                    const bool better =
                        can_win
                        && (best < 0
                            || (whole && !best_whole)
                            || (whole == best_whole && rest_better));
                    if (better)
                    {
                        best = static_cast<int>(r);
                        best_block = block;
                        best_tiles = bt;
                        best_short = shortfall;
                        best_supply = supply;
                        best_whole = whole;
                    }
                    // Grow only while the block is under the floor, and never
                    // to the realm's last cell.
                    if (min_tiles <= 0 || bt >= min_tiles) break;
                    if (block.size() + 1 >= G.anchored.size()) break;
                    int pick = -1;
                    for (const int m : block)
                        for (const int n : adj[static_cast<std::size_t>(m)])
                        {
                            if (gid[static_cast<std::size_t>(n)] != g || n == pseat) continue;
                            if (std::find(block.begin(), block.end(), n) != block.end()) continue;
                            if (pick < 0
                                || regions[static_cast<std::size_t>(n)].network_supply_q
                                       < regions[static_cast<std::size_t>(pick)].network_supply_q
                                || (regions[static_cast<std::size_t>(n)].network_supply_q
                                        == regions[static_cast<std::size_t>(pick)].network_supply_q
                                    && n < pick))
                                pick = n;
                        }
                    if (pick < 0) break;
                    block.push_back(pick);
                    bt += tiles[static_cast<std::size_t>(pick)];
                }
            }
            if (best >= 0 && best_short > 0) ++plan.subfloor_secessions;
            if (best < 0) break;
            if (!best_whole)
            {
                ++plan.secessions_split;
                any_split = true;
            }

            const std::size_t br = static_cast<std::size_t>(best);
            const int g = gid[br];
            const int p = groups[static_cast<std::size_t>(g)].polity;
            const int nid = next_id++;
            const int pseat = seat_of(p);
            std::vector<int> in_block = best_block;
            std::sort(in_block.begin(), in_block.end());
            const auto blocked = [&](int h) { return std::binary_search(in_block.begin(), in_block.end(), h); };
            closing_step st;
            st.kind        = closing_step_kind::target_secede;
            st.from_polity = p;
            st.into_polity = nid;
            st.region      = best;
            st.moved       = in_block;
            // The unanchored hinterland of the block's cells leaves with them.
            for (std::size_t h = 0; h < R; ++h)
                if (!blocked(static_cast<int>(h)) && cur_polity[h] == p && tiles[h] <= 0
                    && static_cast<int>(h) != pseat && cur_seat[h] >= 0 && blocked(cur_seat[h]))
                    st.moved.push_back(static_cast<int>(h));
            std::sort(st.moved.begin(), st.moved.end());
            for (const int h : st.moved)
            {
                cur_polity[static_cast<std::size_t>(h)] = nid;
                cur_seat[static_cast<std::size_t>(h)]   = best;
                seat_flag[static_cast<std::size_t>(h)]  = (h == best) ? 1 : 0;
            }
            // Ground the realm keeps that pointed at a departing seat re-points
            // at its own seat (the conquest rule for a region decoupled from
            // its seat).
            std::vector<int> moved_sorted = st.moved;
            for (std::size_t h = 0; h < R; ++h)
                if (cur_polity[h] == p && cur_seat[h] >= 0
                    && std::binary_search(moved_sorted.begin(), moved_sorted.end(), cur_seat[h]))
                    cur_seat[h] = pseat;
            if (cap.size() <= static_cast<std::size_t>(nid)) cap.resize(static_cast<std::size_t>(nid) + 1, -1);
            if (polity_group.size() <= static_cast<std::size_t>(nid))
                polity_group.resize(static_cast<std::size_t>(nid) + 1, -1);
            cap[static_cast<std::size_t>(nid)] = best;

            fold_group& G = groups[static_cast<std::size_t>(g)];
            G.tiles -= best_tiles;
            G.anchored.erase(std::remove_if(G.anchored.begin(), G.anchored.end(), blocked), G.anchored.end());
            G.lowest = INT_MAX;
            for (const int r : G.anchored) G.lowest = std::min(G.lowest, r);

            fold_group ng;
            ng.polity   = nid;
            ng.alive    = true;
            ng.tiles    = best_tiles;
            ng.lowest   = in_block.front();
            ng.anchored = in_block;
            const int ngi = static_cast<int>(groups.size());
            for (const int r : in_block) gid[static_cast<std::size_t>(r)] = ngi;
            polity_group[static_cast<std::size_t>(nid)] = ngi;
            groups.push_back(std::move(ng));
            ++N;
            ++plan.target_secessions;
            plan.steps.push_back(std::move(st));
        }
        plan.short_of_target = N != total_target;

        // THE REPAIR: a secession that could not keep both sides one territory
        // leaves ground cut off; it is ceded as at the open. Ground only, so
        // the count the total reached stands.
        (void)any_split;
    }
    // THE REPAIR, always: whatever the floor and the total left outside the
    // rule -- a split with no one-territory candidate, or an absorption whose
    // neighbourhood (cells touching across owned coast or a lake) reached
    // farther than a strait -- is ceded as at the open. Ground only, so every
    // count above stands.
    if (!plan.steps.empty() && (plan.floor_absorptions + plan.target_absorptions + plan.target_secessions) > 0)
        run_cessions(/*late=*/true);
    plan.nations_after = N;

    // The overrides the tail applies, and the diagnostic read of the
    // representative against the capital.
    if (strait_rule)
    {
        std::sort(plan.ceded_regions.begin(), plan.ceded_regions.end());
        plan.ceded_regions.erase(std::unique(plan.ceded_regions.begin(), plan.ceded_regions.end()),
                                 plan.ceded_regions.end());
        build_keys();
        plan.polity_core_tiles.assign(static_cast<std::size_t>(KP), -1);
        for (int k = 0; k < KP; ++k) plan.polity_core_tiles[static_cast<std::size_t>(k)] = core_tile[static_cast<std::size_t>(k)];
        for (int k = 0; k < KP; ++k)
        {
            if (order[static_cast<std::size_t>(k)] == INT_MAX) continue;
            const int rep_tile = anchor_of(order[static_cast<std::size_t>(k)]);
            if (rep_tile == core_tile[static_cast<std::size_t>(k)]) continue;
            if (key_is_one_territory(own, water, gw, gh, k, core_tile[static_cast<std::size_t>(k)])
                && !key_is_one_territory(own, water, gw, gh, k, rep_tile))
                ++plan.reps_off_core;
        }
        for (int t = 0; t < total; ++t)
            if (!water[static_cast<std::size_t>(t)] && cells[static_cast<std::size_t>(t)] != cell_region[static_cast<std::size_t>(t)]
                && cells[static_cast<std::size_t>(t)] >= 0)
                plan.cell_overrides.emplace_back(t, cells[static_cast<std::size_t>(t)]);
    }
    return plan;
}

