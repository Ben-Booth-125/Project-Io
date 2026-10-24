#include "spawn_seat.hpp"

#include "components.hpp"
#include "market_clearing.hpp" // market_for_tile — the score's own catchment partition
#include "corporation_generation.hpp" // BL-1154: the seat opens unarmed

#include <algorithm>
#include <random>
#include <unordered_map>

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

/// BL-1407 (Ben, 2026-10-23; NATIONS.md § What a nation owns): the holdings a
/// seat is read over — the seat IS a nation, so its own buildings.
const std::vector<entity_id>& seat_holdings(const nation_component& nc)
{
    return nc.state_assets;
}

} // namespace

double seat_landscape_score(const world& w, const landscape_score& landscape,
                            entity_id nation, int* holdings_scored)
{
    if (holdings_scored)
        *holdings_scored = 0;
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return 0.0;

    // Market id -> its per-market viability. Built from the score's own
    // `markets` vector, which is sorted by id; a lookup map is order-free.
    std::unordered_map<entity_id, double> viability;
    viability.reserve(landscape.markets.size());
    for (const market_score& m : landscape.markets)
        viability[m.market] = m.actual * m.balance;

    // The mean over SITED holdings, walked in the corp's own asset order. The
    // accumulation is a double sum, and `assets` is a vector whose order is set
    // by generation, not by hash layout — so the sum is the same on every run.
    double sum    = 0.0;
    int    sited  = 0;
    int    scored = 0;
    for (const entity_id bid : seat_holdings(nit->second))
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        // BL-1154 review: a military base is not a holding the seat keeps (the
        // seat opens unarmed, `move_seat_force`), so it is never scored.
        if (bit->second.type == building_type::military_base)
            continue;
        if (w.tiles.find(bit->second.tile) == w.tiles.end())
            continue;
        ++sited;
        const auto vit = viability.find(market_for_tile(w, bit->second.tile));
        if (vit == viability.end())
            continue;               // no scored catchment: zero viability, still counted
        ++scored;
        sum += vit->second;
    }
    if (holdings_scored)
        *holdings_scored = scored;
    return sited > 0 ? sum / static_cast<double>(sited) : 0.0;
}

spawn_seat_result rank_spawn_candidates(const world& w, const landscape_score& landscape,
                                        spawn_seat_params params)
{
    spawn_seat_result out;

    // --- the major pool -------------------------------------------------------
    //
    // BL-1407 (CONCEPT.md § Player identity; STARTUP.md § The seat): the player
    // IS a MAJOR NATION, so the seat is chosen over the majors (the saved fact
    // `is_major_nation`). A minor is never a seat. Sorted by entity id because
    // `w.nations` is an unordered map and an unsorted walk would hand the same
    // seed a different seat run to run — the BL-406 lesson.
    std::vector<entity_id> majors;
    for (const auto& [id, nc] : w.nations)
        if (nc.is_major)
            majors.push_back(id);
    std::sort(majors.begin(), majors.end());

    out.major_count = static_cast<int>(majors.size());
    if (majors.empty())
        return out; // Nothing to seat. Caller keeps whatever the generator picked.

    // --- read the floor, and the two weights' inputs -------------------------
    out.candidates.reserve(majors.size());
    for (const entity_id id : majors)
    {
        const nation_component& nc = w.nations.at(id);

        spawn_seat_candidate c;
        c.nation  = id;

        // THE FLOOR (BL-1020, Ben 2026-09-16, NR-881). The static landscape
        // score phase 6 computed, read at this seat's holdings: does this corp
        // stand on ground where the landscape actually works? A property of the
        // ground, its reach and its market — knowable before the first convoy
        // runs, which a trading record over a twelve-tick settle is not. About
        // whether the seat is VIABLE, never about whether there is a good GAME
        // in it, which is the weighting's job below.
        c.landscape   = seat_landscape_score(w, landscape, id, &c.holdings_scored);
        c.shortlisted = (c.landscape > 0.0);

        // INFORMATION, NOT THE GATE. The settle's closing treasury, kept for
        // the seat card (BL-1407: the seat's money is its nation's). The
        // trailing net over the nation's filed returns is read once the nation
        // files them; until then the card reads none.
        c.balance = nc.treasury;
        c.solvent = (c.balance > 0.0f);
        c.quarters_read = 0;

        for (const entity_id bid : seat_holdings(nc))
        {
            const auto bit = w.buildings.find(bid);
            if (bit == w.buildings.end())
                continue;
            // BL-1154 review: the menu ranks what the seat will HOLD, and the
            // seat opens unarmed — a rival's muster base is not counted.
            if (bit->second.type == building_type::military_base)
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

    // --- THE ORDER -----------------------------------------------------------
    //
    // Ranked on the static score, highest first, entity id breaking a tie — a
    // TOTAL order, so the ranking is the same on every machine whatever order
    // the specialists were gathered in. It is the order the shortlist is offered
    // in and the order the draw walks; the draw's probabilities are the weights'
    // alone, so the rank moves which corp a given cursor lands on and nothing
    // about how often each is chosen. Stable sort over an id-sorted vector, and
    // the id tie-break is explicit anyway.
    std::stable_sort(out.candidates.begin(), out.candidates.end(),
                     [](const spawn_seat_candidate& a, const spawn_seat_candidate& b)
                     {
                         if (a.landscape != b.landscape)
                             return a.landscape > b.landscape;
                         return a.nation < b.nation;
                     });

    // AN UNMET FLOOR STANDS (recorded here so a ranking read by the selection
    // canvas says it too, not only a draw).
    out.floor_unmet = (out.shortlist_size == 0);
    return out;
}

entity_id draw_spawn_seat(const spawn_seat_result& ranked, std::uint32_t seed)
{
    entity_id chosen = null_entity;

    if (ranked.shortlist_size > 0)
    {
        // Cumulative-weight sampling over the sorted candidate walk — the same
        // idiom `pick_home_nation` uses (corporation_generation.cpp), so the
        // draw's determinism contract is the one the rest of generation already
        // holds. Seeded from the world seed alone.
        float total_w = 0.0f;
        for (const spawn_seat_candidate& c : ranked.candidates)
            total_w += c.weight;

        std::mt19937 rng(seed ^ 0x5EA7C0DEu);
        std::uniform_real_distribution<float> draw(0.0f, total_w);
        float cursor = draw(rng);
        for (const spawn_seat_candidate& c : ranked.candidates)
        {
            if (c.weight <= 0.0f)
                continue;
            cursor -= c.weight;
            if (cursor <= 0.0f)
            {
                chosen = c.nation;
                break;
            }
        }
        // Float accumulation can leave the cursor a hair above zero after the
        // last subtraction. Fall through to the last shortlisted corp rather
        // than to null — the same guard pick_home_nation's fallback serves.
        if (chosen == null_entity)
            for (const spawn_seat_candidate& c : ranked.candidates)
                if (c.shortlisted)
                    chosen = c.nation;
    }
    else
    {
        // AN UNMET FLOOR STANDS. Nothing here conjures a corp, patches one, or
        // relaxes the floor to make the shortlist non-empty: the first-ranked
        // specialist is seated as-is and the fact is RECORDED, so the sweep reads
        // it as the viability signal it is. Every score is zero here, so rank
        // order is entity-id order. The trailing net is NOT consulted — it
        // stopped deciding anything with BL-1020, and a fallback that read it
        // would bring the retired gate back through the side door.
        if (!ranked.candidates.empty())
            chosen = ranked.candidates.front().nation;
    }
    return chosen;
}

bool repoint_player(world& w, entity_id nation)
{
    // BL-1407: the seat is a NATION (the player IS its nation).
    if (nation == null_entity || w.nations.find(nation) == w.nations.end())
        return false;
    const entity_id previous = w.player_entity;
    w.player_entity          = nation;
    // BL-1206 (Ben, 2026-10-07): the seat opens with a clean slate of
    // construction — its unfinished settle builds cancelled and refunded. Before
    // the force moves, so an unfinished muster base is refunded, not disarmed.
    seat_clean_slate(w, nation);
    // BL-1154 (Ben, 2026-10-01, NR-963 A): THE SEAT OPENS UNARMED, WHICHEVER
    // NATION IT IS. Rivals were armed at chartering and the generation-time
    // pick was not; when the seat moves, the new seat's opening force goes and
    // the nation it leaves takes the one every rival has. Deterministic and
    // draw-free: the seeding is a nearest-valid-tile search.
    move_seat_force(w, previous, nation);   // the rule the pick path shares
    return true;
}

spawn_seat_result seat_player_corporation(world& w, std::uint32_t seed,
                                          const landscape_score& landscape,
                                          spawn_seat_params params)
{
    // Rank, draw, re-point — the three halves in sequence, byte-for-byte the
    // decision this function made before the canvas split them (BL-1076).
    spawn_seat_result out = rank_spawn_candidates(w, landscape, params);
    const entity_id chosen = draw_spawn_seat(out, seed);
    if (repoint_player(w, chosen))
    {
        out.seated = chosen;
        // NR-986 (Ben, 2026-10-09): the handoff releases the seat's dial-idled
        // plants to auto. Here, not in repoint_player, which generation's
        // provisional pick also calls.
        seat_release_dial_idled(w, chosen);
    }
    return out;
}
