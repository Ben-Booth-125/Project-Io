#include "province.hpp"

#include "hex_neighbors.hpp"
#include "logistics.hpp" // body_tile_grid — the raster index every body-grid pass walks

#include <algorithm>
#include <istream>
#include <map>
#include <ostream>

namespace {

// ---------------------------------------------------------------------------
// Stateless folds — no RNG stream is consumed anywhere in this file
// ---------------------------------------------------------------------------

/// SplitMix64 finaliser. A pure avalanche of @p x; the whole file's randomness.
uint64_t mix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

uint64_t fold(uint64_t a, uint64_t b) { return mix64(mix64(a) ^ (b * 0x9E3779B97F4A7C15ull)); }

/// Three-input fold — the per-tile-pair jitter draw, (seed, tile a, tile b).
uint64_t fold(uint64_t a, uint64_t b, uint64_t c) { return mix64(fold(a, b) ^ mix64(c)); }

// ---------------------------------------------------------------------------
// Id layout (see province::id)
// ---------------------------------------------------------------------------

constexpr int      k_component_bits = 3;
constexpr int      k_block_bits     = 17;
constexpr uint32_t k_max_blocks     = 1u << k_block_bits;
constexpr uint32_t k_max_components = 1u << k_component_bits;

uint32_t make_province_id(uint32_t body_rank, uint32_t block_id, uint32_t component)
{
    return (body_rank << (k_block_bits + k_component_bits)) | (block_id << k_component_bits)
           | component;
}

/// The odd-r hex neighbour of (@p gx, @p gy) on side @p side, with the column
/// wrapped into [0, gw) and the row rejected when it leaves the grid. Columns
/// wrap and rows do not — the same cylinder logistics.cpp walks.
/// @return True when the neighbour exists; then @p out_x / @p out_y hold it.
bool wrapped_neighbour(int gx, int gy, int side, int gw, int gh, int& out_x, int& out_y)
{
    const hex_neighbors::coord c = hex_neighbors::neighbour(gx, gy, side);
    if (c.gy < 0 || c.gy >= gh)
        return false;
    int nx = c.gx % gw;
    if (nx < 0)
        nx += gw;
    out_x = nx;
    out_y = c.gy;
    return true;
}

/// Per-body working state for the three passes. Everything is keyed by tile
/// entity id or by an index into `land` (ascending tile id), never by container
/// iteration order.
struct body_work
{
    int gw = 0;
    int gh = 0;

    std::vector<entity_id> land;            ///< Land tiles, ascending entity id.
    std::vector<entity_id> grid;            ///< gy*gw + gx -> tile, null_entity if absent.
    std::unordered_map<entity_id, uint32_t> owner; ///< tile -> province id.
    std::map<uint32_t, std::vector<entity_id>> members; ///< province id -> tiles (ascending id).
};

/// Seam evidence on the edge from @p a out of its side @p side to @p b: how
/// strongly the terrain argues that a province border belongs ON this edge.
/// A river crossing is the strongest signal (2); a composition or landform
/// change adds one each. 0 means the two tiles are the same ground.
int seam_evidence(const tile_component& a, const tile_component& b, int side)
{
    int e = 0;
    if ((a.river_edges >> side) & 1u)
        e += 2;
    if (a.composition != b.composition)
        e += 1;
    if (a.landform != b.landform)
        e += 1;
    return e;
}

/// Whether the tiles of @p tiles are hex-connected on @p bw's grid.
bool connected(const body_work& bw, const world& w, const std::vector<entity_id>& tiles)
{
    if (tiles.size() <= 1)
        return true;

    std::vector<entity_id> open{ tiles.front() };
    std::vector<entity_id> seen{ tiles.front() };
    const auto in_set = [&tiles](entity_id t) {
        return std::find(tiles.begin(), tiles.end(), t) != tiles.end();
    };

    while (!open.empty())
    {
        const entity_id cur = open.back();
        open.pop_back();
        const auto tit = w.tiles.find(cur);
        if (tit == w.tiles.end())
            continue;
        for (int s = 0; s < 6; ++s)
        {
            int nx = 0, ny = 0;
            if (!wrapped_neighbour(tit->second.grid_x, tit->second.grid_y, s, bw.gw, bw.gh, nx, ny))
                continue;
            const entity_id n = bw.grid[static_cast<std::size_t>(ny) * bw.gw + nx];
            if (n == null_entity || !in_set(n))
                continue;
            if (std::find(seen.begin(), seen.end(), n) != seen.end())
                continue;
            seen.push_back(n);
            open.push_back(n);
        }
    }
    return seen.size() == tiles.size();
}

/// Split @p tiles into hex-connected components, each ascending by tile id, and
/// the components themselves ordered smallest-member-tile-id first.
std::vector<std::vector<entity_id>> components(const body_work& bw, const world& w,
                                               std::vector<entity_id> tiles)
{
    std::vector<std::vector<entity_id>> out;
    std::sort(tiles.begin(), tiles.end());

    std::vector<bool> used(tiles.size(), false);
    for (std::size_t i = 0; i < tiles.size(); ++i)
    {
        if (used[i])
            continue;
        std::vector<entity_id> comp{ tiles[i] };
        used[i] = true;
        for (std::size_t head = 0; head < comp.size(); ++head)
        {
            const auto tit = w.tiles.find(comp[head]);
            if (tit == w.tiles.end())
                continue;
            for (int s = 0; s < 6; ++s)
            {
                int nx = 0, ny = 0;
                if (!wrapped_neighbour(tit->second.grid_x, tit->second.grid_y, s, bw.gw, bw.gh, nx,
                                       ny))
                    continue;
                const entity_id n = bw.grid[static_cast<std::size_t>(ny) * bw.gw + nx];
                if (n == null_entity)
                    continue;
                const auto at = std::find(tiles.begin(), tiles.end(), n);
                if (at == tiles.end())
                    continue;
                const std::size_t idx = static_cast<std::size_t>(at - tiles.begin());
                if (used[idx])
                    continue;
                used[idx] = true;
                comp.push_back(n);
            }
        }
        std::sort(comp.begin(), comp.end());
        out.push_back(std::move(comp));
    }

    // Components arrive in ascending smallest-member order already, because the
    // outer loop walks the sorted tile list and claims each component whole.
    return out;
}

} // namespace

const province* province_partition::find(uint32_t id) const
{
    const auto it = std::lower_bound(provinces.begin(), provinces.end(), id,
                                     [](const province& p, uint32_t v) { return p.id < v; });
    if (it == provinces.end() || it->id != id)
        return nullptr;
    return &*it;
}

void build_province_partition(world& w, uint32_t seed)
{
    province_partition out;
    out.seed = seed;

    // `world::bodies` is an UNORDERED map: its iteration order is a container-layout
    // accident, and is not even stable across a copy of the same world. Body rank is
    // baked into every province id, so the walk is sorted here explicitly. This is
    // the one place the partition could leak non-determinism, and it is exactly what
    // the harness's P6 replay check caught.
    std::vector<entity_id> body_ids;
    body_ids.reserve(w.bodies.size());
    for (const auto& entry : w.bodies)
        body_ids.push_back(entry.first);
    std::sort(body_ids.begin(), body_ids.end());

    uint32_t body_rank = 0;
    for (const entity_id body_id : body_ids)
    {
        const body_component& bc   = w.bodies.at(body_id);
        const uint32_t        rank = body_rank++;
        const int             gw   = bc.grid_width;
        const int             gh   = bc.grid_height;
        if (gw <= 0 || gh <= 0)
            continue;

        body_work bw;
        bw.gw   = gw;
        bw.gh   = gh;
        bw.grid = body_tile_grid(w, body_id); // raster copy; the cache stays intact

        // Land mask, ascending tile id. Ocean is excluded outright.
        for (const entity_id t : bw.grid)
        {
            if (t == null_entity)
                continue;
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end())
                continue;
            if (tit->second.composition == terrain_composition::ocean)
                continue;
            bw.land.push_back(t);
        }
        if (bw.land.empty())
            continue;
        std::sort(bw.land.begin(), bw.land.end());

        // --- Pass 1: base 2x2 blocks, then a component split so no province
        // spans a strait even before jitter runs.
        const uint64_t h  = fold(seed, static_cast<uint64_t>(body_id));
        const int      dx = static_cast<int>(h & 1u);
        const int      dy = static_cast<int>((h >> 1) & 1u);

        const int block_cols = (gw + dx + 1) / 2;
        const int block_rows = (gh + dy + 1) / 2;
        if (static_cast<uint64_t>(block_cols) * block_rows >= k_max_blocks)
            continue; // grid past the id layout's 17-bit block field; no real body is

        std::map<uint32_t, std::vector<entity_id>> blocks;
        for (const entity_id t : bw.land)
        {
            const tile_component& tc = w.tiles.at(t);
            const uint32_t        bx = static_cast<uint32_t>((tc.grid_x + dx) / 2);
            const uint32_t        by = static_cast<uint32_t>((tc.grid_y + dy) / 2);
            blocks[by * static_cast<uint32_t>(block_cols) + bx].push_back(t);
        }

        for (auto& [block_id, tiles] : blocks)
        {
            auto comps = components(bw, w, std::move(tiles));
            for (std::size_t c = 0; c < comps.size() && c < k_max_components; ++c)
            {
                const uint32_t pid = make_province_id(rank, block_id, static_cast<uint32_t>(c));
                for (const entity_id t : comps[c])
                    bw.owner[t] = pid;
                bw.members[pid] = std::move(comps[c]);
            }
        }

        // --- Pass 2: terrain-seam jitter. One sweep, ascending tile id, each
        // tile moving at most once and each province resized at most once.
        std::vector<uint32_t> resized;
        const auto            is_resized = [&resized](uint32_t p) {
            return std::find(resized.begin(), resized.end(), p) != resized.end();
        };

        for (const entity_id a : bw.land)
        {
            const uint32_t src = bw.owner.at(a);
            if (is_resized(src))
                continue;

            auto& src_tiles = bw.members.at(src);
            if (src_tiles.size() < 3 || src_tiles.size() > 5)
                continue; // only the in-band interior jitters; the coast is pass 3's job

            const tile_component& ta = w.tiles.at(a);

            // Does `a` sit across a seam from its OWN province? That is the
            // evidence the border is in the wrong place.
            int own_seam = 0;
            for (int s = 0; s < 6; ++s)
            {
                int nx = 0, ny = 0;
                if (!wrapped_neighbour(ta.grid_x, ta.grid_y, s, gw, gh, nx, ny))
                    continue;
                const entity_id n = bw.grid[static_cast<std::size_t>(ny) * gw + nx];
                if (n == null_entity)
                    continue;
                const auto oit = bw.owner.find(n);
                if (oit == bw.owner.end() || oit->second != src)
                    continue;
                own_seam = std::max(own_seam, seam_evidence(ta, w.tiles.at(n), s));
            }
            if (own_seam < 2)
                continue;

            for (int s = 0; s < 6; ++s)
            {
                int nx = 0, ny = 0;
                if (!wrapped_neighbour(ta.grid_x, ta.grid_y, s, gw, gh, nx, ny))
                    continue;
                const entity_id b = bw.grid[static_cast<std::size_t>(ny) * gw + nx];
                if (b == null_entity)
                    continue;
                const auto oit = bw.owner.find(b);
                if (oit == bw.owner.end() || oit->second == src)
                    continue;

                const uint32_t dst = oit->second;
                if (is_resized(dst))
                    continue;
                if (seam_evidence(ta, w.tiles.at(b), s) != 0)
                    continue; // this edge IS a seam — the border belongs here, not past it

                auto& dst_tiles = bw.members.at(dst);
                if (dst_tiles.size() < 3 || dst_tiles.size() >= 5)
                    continue;
                if (src_tiles.size() <= 3)
                    continue; // the clamp never creates a fragment

                if ((fold(seed, static_cast<uint64_t>(a), static_cast<uint64_t>(b)) & 0xFFu) >= 128u)
                    continue;

                // The mover may not be a cut vertex of its source.
                std::vector<entity_id> without;
                without.reserve(src_tiles.size() - 1);
                for (const entity_id t : src_tiles)
                    if (t != a)
                        without.push_back(t);
                if (!connected(bw, w, without))
                    continue;

                src_tiles = std::move(without);
                dst_tiles.push_back(a);
                std::sort(dst_tiles.begin(), dst_tiles.end());
                bw.owner[a] = dst;
                resized.push_back(src);
                resized.push_back(dst);
                break;
            }
        }

        // --- Pass 3: coastal repair. A one-tile province merges into its
        // lowest-id hex-adjacent province; a true islet stands alone.
        for (auto it = bw.members.begin(); it != bw.members.end();)
        {
            if (it->second.size() != 1)
            {
                ++it;
                continue;
            }
            const entity_id       a  = it->second.front();
            const tile_component& ta = w.tiles.at(a);

            uint32_t best  = 0;
            bool     found = false;
            for (int s = 0; s < 6; ++s)
            {
                int nx = 0, ny = 0;
                if (!wrapped_neighbour(ta.grid_x, ta.grid_y, s, gw, gh, nx, ny))
                    continue;
                const entity_id n = bw.grid[static_cast<std::size_t>(ny) * gw + nx];
                if (n == null_entity)
                    continue;
                const auto oit = bw.owner.find(n);
                if (oit == bw.owner.end() || oit->second == it->first)
                    continue;
                if (!found || oit->second < best)
                {
                    best  = oit->second;
                    found = true;
                }
            }
            if (!found)
            {
                ++it; // islet — no neighbour to merge into
                continue;
            }

            auto& into = bw.members.at(best);
            into.push_back(a);
            std::sort(into.begin(), into.end());
            bw.owner[a] = best;
            it          = bw.members.erase(it);
        }

        for (auto& [pid, tiles] : bw.members)
        {
            province p;
            p.id    = pid;
            p.body  = body_id;
            p.tiles = std::move(tiles);
            out.provinces.push_back(std::move(p));
        }
    }

    // Ascending id — the iteration contract. The per-body runs are already
    // ascending and body-major by construction; sorting makes it unconditional.
    std::sort(out.provinces.begin(), out.provinces.end(),
              [](const province& a, const province& b) { return a.id < b.id; });

    for (const province& p : out.provinces)
        for (const entity_id t : p.tiles)
            out.tile_province[t] = p.id;

    w.provinces = std::move(out);
}

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------

namespace {

void write_u32(std::ostream& out, uint32_t v)
{
    out.write(reinterpret_cast<const char*>(&v), sizeof v);
}

bool read_u32(std::istream& in, uint32_t& v)
{
    in.read(reinterpret_cast<char*>(&v), sizeof v);
    return static_cast<bool>(in);
}

} // namespace

void write_province_section(const province_partition& p, std::ostream& out)
{
    write_u32(out, province_section_magic);
    write_u32(out, province_section_version);
    write_u32(out, p.seed);
    write_u32(out, static_cast<uint32_t>(p.provinces.size()));

    for (const province& pr : p.provinces)
    {
        write_u32(out, pr.id);
        write_u32(out, pr.body);
        write_u32(out, static_cast<uint32_t>(pr.tiles.size()));
        for (const entity_id t : pr.tiles)
            write_u32(out, t);
    }
}

bool read_province_section(province_partition& out, std::istream& in)
{
    out = province_partition{};

    uint32_t magic = 0;
    if (!read_u32(in, magic))
        return true; // clean end of stream — a pre-BL-466 save, which must still load
    if (magic != province_section_magic)
        return false;

    uint32_t version = 0;
    if (!read_u32(in, version) || version != province_section_version)
        return false;

    if (!read_u32(in, out.seed))
        return false;

    uint32_t count = 0;
    if (!read_u32(in, count))
        return false;
    if (count > province_section_max_provinces)
        return false;

    out.provinces.reserve(count);
    uint32_t prev_id  = 0;
    bool     have_prev = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        province pr;
        if (!read_u32(in, pr.id))
            return false;
        if (have_prev && pr.id <= prev_id)
            return false; // ids must be strictly ascending — the iteration contract
        prev_id   = pr.id;
        have_prev = true;

        if (!read_u32(in, pr.body))
            return false;

        uint32_t tiles = 0;
        if (!read_u32(in, tiles))
            return false;
        if (tiles == 0 || tiles > province_section_max_tiles)
            return false;

        pr.tiles.resize(tiles);
        for (uint32_t t = 0; t < tiles; ++t)
            if (!read_u32(in, pr.tiles[t]))
                return false;

        out.provinces.push_back(std::move(pr));
    }

    for (const province& pr : out.provinces)
        for (const entity_id t : pr.tiles)
            out.tile_province[t] = pr.id;

    return true;
}

// ---------------------------------------------------------------------------
// Province building ceiling (BL-513) — the four-input sustain heuristic
// ---------------------------------------------------------------------------
// The shape, the role each of Ben's four inputs plays, and the pinning
// discipline behind the single free coefficient all live in province.hpp beside
// the constants. This file is only the arithmetic.
//
// DETERMINISM. Two ordering hazards exist here and both are closed:
//   * the tile sum walks `province::tiles`, ascending entity id by the partition
//     contract, so the float addition order is fixed;
//   * population centres are stored centre-keyed in an unordered_map, so the
//     reverse tile -> centre lookup is gathered into an ORDERED std::map first
//     and read back in tile order. No unordered iteration order reaches the sum.
// ---------------------------------------------------------------------------

namespace {

/// Tile -> summed population scale standing on it. The map is ordered so the
/// caller reads it in ascending tile order; the CONTENT is independent of the
/// source unordered_map's iteration order because every entry is an
/// accumulation, never a first-wins pick.
std::map<entity_id, int> population_scale_by_tile(const world& w)
{
    std::map<entity_id, int> by_tile;
    for (const auto& [centre_id, tile_id] : w.population_centre_tile)
    {
        const auto pit = w.population_centres.find(centre_id);
        if (pit == w.population_centres.end())
            continue;
        by_tile[tile_id] += pit->second.scale;
    }
    return by_tile;
}

} // namespace

province_sustain measure_province_sustain(const world& w, const province& pr)
{
    const std::map<entity_id, int> pop_by_tile = population_scale_by_tile(w);

    province_sustain s;
    int pop_scale_total = 0;

    for (const entity_id tile_id : pr.tiles) // ascending, by the partition contract
    {
        const auto tit = w.tiles.find(tile_id);
        if (tit == w.tiles.end())
            continue;
        const tile_component& tc = tit->second;

        ++s.land_tiles;                         // AREA
        s.habitability_area += tc.habitability;  // HABITABILITY
        s.infrastructure_gain +=                 // INFRASTRUCTURE
            tc.habitability * (static_cast<float>(tc.road_level) / k_road_ladder_max);

        const auto pit = pop_by_tile.find(tile_id);
        if (pit != pop_by_tile.end())
            pop_scale_total += pit->second;      // POPULATION
    }

    s.population_factor =
        1.0f + static_cast<float>(pop_scale_total) / k_population_scale_max;
    s.units = (s.habitability_area + s.infrastructure_gain) * s.population_factor;

    // A province that exists at all sustains at least one building. That floor is
    // the partition's own claim that this is habitable land, not a tuning clamp:
    // the partition never emits an ocean province, so "some land, room for
    // nothing" would be a contradiction rather than a constraint.
    const int scaled =
        static_cast<int>(s.units * k_province_buildings_per_sustain_unit + 0.5f);
    s.ceiling = scaled < 1 ? 1 : scaled;
    return s;
}

int province_building_ceiling(const world& w, uint32_t province_id)
{
    const province* pr = w.provinces.find(province_id);
    if (pr == nullptr)
        return -1; // UNKNOWN, never "no room" — see the header's contract.
    return measure_province_sustain(w, *pr).ceiling;
}

int province_buildings_standing(const world& w, uint32_t province_id)
{
    if (province_id == 0)
        return 0;
    // Order-independent: a count, not a fold, so the unordered walk is safe.
    int n = 0;
    for (const auto& [bid, bc] : w.buildings)
        if (w.provinces.province_of(bc.tile) == province_id)
            ++n;
    return n;
}
