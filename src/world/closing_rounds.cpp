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
                                 int min_tiles, bool keep_city_states, int total_target)
{
    closing_plan plan;
    const std::size_t R = regions.size();
    const std::vector<int64_t> tiles = closing_cell_tiles(cell_region, R);

    // --- Cell adjacency: two regions whose carve cells touch cardinally -----
    // Column-wrapped, rows clamped, the carve's own neighbourhood
    // (`cardinal_neighbours`). Right and down cover every pair once.
    std::vector<std::vector<int>> adj(R);
    if (gw > 0 && gh > 0 && cell_region.size() >= static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
    {
        for (int idx = 0; idx < gw * gh; ++idx)
        {
            const int a = cell_region[static_cast<std::size_t>(idx)];
            if (a < 0 || static_cast<std::size_t>(a) >= R) continue;
            const int col = idx % gw, row = idx / gw;
            const int nb[2] = { row * gw + (col + 1) % gw, row + 1 < gh ? idx + gw : -1 };
            for (const int n : nb)
            {
                if (n < 0) continue;
                const int b = cell_region[static_cast<std::size_t>(n)];
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

    // WHO ABSORBS `s`: (absorbed, absorber) groups, or (-1, -1). The largest
    // neighbour across a cell border (ties: the lower anchored region, the
    // carve's own seed order). A neighbour no realm held cannot absorb, so a
    // REALM beside it takes that ground instead — the same union, the realm
    // keeping its name. Ownerless ground beside no realm goes to the largest
    // realm it touches, then to the largest realm on the body (the island
    // case, as the post-pass always had it).
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
        // THE TOTAL OUTRANKS THE FLOOR (Ben, 2026-10-11; NATION_GENERATION.md
        // sec The nation count is a target). A FLOOR-SAFE split -- the block
        // and what the realm keeps both clear the floor -- is always preferred
        // where one exists. Where none does, the split with the SMALLEST
        // SHORTFALL below the floor (summed over both sides) is taken, so the
        // total is still hit; only "no split of any kind" (every realm holds
        // one anchored region) stops the rounds short, and the caller records
        // that miss. Tie order: shortfall; then the fewest regions; then the
        // ground furthest from its seat's reach (the sim's own secession
        // reading); then the larger block; then the lower index.
        while (N < total_target)
        {
            if (static_cast<std::size_t>(next_id) >= owner_index_limit) break;
            int              best = -1;
            std::vector<int> best_block;
            int64_t          best_tiles = 0;
            int64_t          best_short = 0;
            int              best_supply = 0;
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
                    const bool better =
                        best < 0
                        || shortfall < best_short
                        || (shortfall == best_short
                            && (block.size() < best_block.size()
                                || (block.size() == best_block.size()
                                    && (supply < best_supply || (supply == best_supply && bt > best_tiles)))));
                    if (better)
                    {
                        best = static_cast<int>(r);
                        best_block = block;
                        best_tiles = bt;
                        best_short = shortfall;
                        best_supply = supply;
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
    }
    plan.nations_after = N;
    return plan;
}
