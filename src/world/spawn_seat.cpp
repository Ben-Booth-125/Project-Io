#include "spawn_seat.hpp"

#include "components.hpp"

#include <algorithm>
#include <random>

namespace {

/// True if SOME population centre on @p tc's body sits within @p radius grid
/// steps of it.
///
/// Wrapped squared grid distance — columns wrap (the body is a horizontal
/// cylinder), rows do not — the same metric every other proximity read in the
/// codebase uses (`placement_rules.cpp` § centre_within_radius, nearest_market,
/// corp-holding contiguity, cradle spacing). An EXISTENCE test over an
/// unordered container is order-independent, so this is deterministic by
/// construction rather than by a sort.
///
/// Duplicated rather than shared because the placement version lives in that
/// file's anonymous namespace; the metric is stated in both places so neither
/// can drift silently.
bool centre_near_tile(const world& w, const tile_component& tc, int radius)
{
    int gw = 0;
    if (const auto bit = w.bodies.find(tc.body); bit != w.bodies.end())
        gw = bit->second.grid_width;

    const long long r2 = static_cast<long long>(radius) * radius;
    for (const auto& [pid, pcc] : w.population_centres)
    {
        // A razed centre is not populated ground (BL-624): no labour, no demand,
        // no market. Weighting toward one would be weighting toward a ruin.
        if (pcc.razed || pcc.population <= 0)
            continue;
        const auto tit = w.population_centre_tile.find(pid);
        if (tit == w.population_centre_tile.end())
            continue;
        const auto ctc_it = w.tiles.find(tit->second);
        if (ctc_it == w.tiles.end() || ctc_it->second.body != tc.body)
            continue;
        long long dx = ctc_it->second.grid_x - tc.grid_x;
        if (dx < 0) dx = -dx;
        if (gw > 0 && gw - dx < dx)
            dx = gw - dx; // shorter way round the cylinder
        long long dy = ctc_it->second.grid_y - tc.grid_y;
        if (dy < 0) dy = -dy;
        if (dx * dx + dy * dy <= r2)
            return true;
    }
    return false;
}

} // namespace

spawn_seat_result seat_player_corporation(world& w, std::uint32_t seed,
                                          spawn_seat_params params)
{
    spawn_seat_result out;

    // --- the specialist pool ------------------------------------------------
    //
    // Specialist == NOT a background firm. `generate_background_firms` (BL-365)
    // adds the mundane industry that saturates the market; those are the world's
    // furniture, never the player's seat. Sorted by entity id because
    // `w.corporations` is an unordered_map and an unsorted walk would hand the
    // same seed a different seat run to run — the BL-406 lesson.
    std::vector<entity_id> specialists;
    for (const auto& [id, cc] : w.corporations)
        if (!cc.is_background)
            specialists.push_back(id);
    std::sort(specialists.begin(), specialists.end());

    out.specialist_count = static_cast<int>(specialists.size());
    if (specialists.empty())
        return out; // Nothing to seat. Caller keeps whatever the generator picked.

    // --- read the floor, and the two weights' inputs -------------------------
    out.candidates.reserve(specialists.size());
    for (const entity_id id : specialists)
    {
        const auto cit = w.corporations.find(id);
        if (cit == w.corporations.end())
            continue;
        const corporation_component& cc = cit->second;

        spawn_seat_candidate c;
        c.corp    = id;
        c.balance = cc.balance;
        c.solvent = (cc.balance > 0.0f);

        // Trailing net over the last k_spawn_trailing_quarters FILED returns.
        // `returns` is oldest-first and rolling-capped, so the window is its
        // tail; a corp that has filed fewer is read over everything it has,
        // which is the same accommodation the acquisition price makes.
        const std::size_t filed = cc.returns.size();
        const std::size_t take  = std::min(filed, k_spawn_trailing_quarters);
        for (std::size_t i = filed - take; i < filed; ++i)
            c.trailing_net += cc.returns[i].net;
        c.quarters_read = static_cast<int>(take);

        // THE FLOOR. Solvent at the end of the warm start, trailing net
        // non-negative. Two conditions, both about whether this corp can carry
        // its own weight — neither about whether there is a good GAME in it,
        // which is the weighting's job below.
        c.shortlisted = c.solvent && (c.trailing_net >= 0.0f);

        for (const entity_id bid : cc.assets)
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            if (bit->second.type == building_type::processing_facility)
                c.has_processor = true;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit == w.tiles.end())
                continue;
            ++c.holdings;
            if (centre_near_tile(w, tit->second, params.population_radius))
                ++c.holdings_near_pop;
        }
        c.population_share =
            c.holdings > 0 ? static_cast<float>(c.holdings_near_pop)
                             / static_cast<float>(c.holdings)
                           : 0.0f;

        // THE WEIGHT — a BIAS AND NEVER A SECOND GATE. The base of 1.0 is the
        // whole guarantee: a shortlisted corp holding nothing but extraction on
        // thin ground stays drawable, just five times less often than a
        // processor-bearing one sitting entirely on populated ground. Additive
        // rather than multiplicative so the spread stays bounded and legible —
        // 1.0 to 5.0 on the shipped first cut — and so neither term can collapse
        // the other to nothing.
        if (c.shortlisted)
            c.weight = 1.0f
                     + (c.has_processor ? params.processor_bonus : 0.0f)
                     + params.population_bonus * c.population_share;

        out.candidates.push_back(c);
    }

    for (const spawn_seat_candidate& c : out.candidates)
        if (c.shortlisted)
            ++out.shortlist_size;

    // --- the draw ------------------------------------------------------------
    entity_id chosen = null_entity;

    if (out.shortlist_size > 0)
    {
        // Cumulative-weight sampling over the sorted candidate walk — the same
        // idiom `pick_home_nation` uses (corporation_generation.cpp), so the
        // draw's determinism contract is the one the rest of generation already
        // holds. Seeded from the world seed alone.
        float total_w = 0.0f;
        for (const spawn_seat_candidate& c : out.candidates)
            total_w += c.weight;

        std::mt19937 rng(seed ^ 0x5EA7C0DEu);
        std::uniform_real_distribution<float> draw(0.0f, total_w);
        float cursor = draw(rng);
        for (const spawn_seat_candidate& c : out.candidates)
        {
            if (c.weight <= 0.0f)
                continue;
            cursor -= c.weight;
            if (cursor <= 0.0f)
            {
                chosen = c.corp;
                break;
            }
        }
        // Float accumulation can leave the cursor a hair above zero after the
        // last subtraction. Fall through to the last shortlisted corp rather
        // than to null — the same guard pick_home_nation's fallback serves.
        if (chosen == null_entity)
            for (const spawn_seat_candidate& c : out.candidates)
                if (c.shortlisted)
                    chosen = c.corp;
    }
    else
    {
        // AN UNMET FLOOR STANDS. Nothing here conjures a corp, patches one, or
        // relaxes the floor to make the shortlist non-empty: the highest
        // trailing-net specialist is seated as-is and the fact is RECORDED, so
        // the sweep reads it as the viability signal it is. Ties break on the
        // lowest entity id, which the sorted walk gives for free.
        out.floor_unmet = true;
        float best = 0.0f;
        for (const spawn_seat_candidate& c : out.candidates)
            if (chosen == null_entity || c.trailing_net > best)
            {
                chosen = c.corp;
                best   = c.trailing_net;
            }
    }

    // --- re-point ------------------------------------------------------------
    if (chosen != null_entity && w.corporations.find(chosen) != w.corporations.end())
    {
        for (auto& [id, cc] : w.corporations)
            cc.is_player = false;
        w.corporations[chosen].is_player = true;
        w.player_entity                  = chosen;
        out.seated                       = chosen;
    }

    return out;
}
