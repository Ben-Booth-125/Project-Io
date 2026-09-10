#include "settlement.hpp"

#include "colonisation.hpp" // BL-846/848: the diffusion that dates and colours a founding
#include "tongue.hpp"       // BL-348: quarter words are coined, not borrowed

#include <algorithm>
#include <array>
#include <cstdlib>
#include <unordered_map>
#include <utility>

// ---------------------------------------------------------------------------
// culture_shares (BL-826)
// ---------------------------------------------------------------------------
//
// INTEGER ONLY, AND THE 1000 TOTAL IS CONSERVED TO THE UNIT. Every quantity
// below is per-mille; the floor losses of the proportional take are handed back
// in a fixed order so the result cannot depend on iteration order or on a
// compiler's evaluation order. See settlement.hpp § culture_shares.

void culture_shares::shift_toward(int c, int amount_q)
{
    if (c < 0) return;
    if (amount_q <= 0) return;
    if (amount_q > 1000) amount_q = 1000;

    // Working set: the three slots plus the tail as a fourth component.
    // `tail` is index culture_share_slots and always counts as foreign.
    constexpr int n_comp = culture_share_slots + 1;
    int cid[n_comp];
    int cw[n_comp];
    for (int i = 0; i < culture_share_slots; ++i)
    {
        cid[i] = static_cast<int>(id[i]);
        cw[i]  = static_cast<int>(weight_q[i]);
    }
    cid[culture_share_slots] = -1;
    cw[culture_share_slots]  = static_cast<int>(other_q);

    int foreign_total = 0;
    for (int i = 0; i < n_comp; ++i)
        if (cid[i] != c) foreign_total += cw[i];
    if (foreign_total <= 0) return; // Already wholly this people.

    const int want = (foreign_total * amount_q) / 1000;
    if (want <= 0) return;

    int take[n_comp] = {0, 0, 0, 0};
    int taken = 0;
    for (int i = 0; i < n_comp; ++i)
    {
        if (cid[i] == c) continue;
        take[i] = (cw[i] * amount_q) / 1000;
        if (take[i] > cw[i]) take[i] = cw[i];
        taken += take[i];
    }

    // Hand back the floor losses, largest component first, ties on the lower
    // index, the tail last. At most one unit per component is ever owed, since
    // each floor discards strictly less than one.
    int order[n_comp] = {0, 1, 2, 3};
    for (int a = 0; a < n_comp; ++a)
        for (int b = a + 1; b < n_comp; ++b)
        {
            const int x = order[a], y = order[b];
            const bool x_tail = (x == culture_share_slots);
            const bool y_tail = (y == culture_share_slots);
            bool swap = false;
            if (x_tail != y_tail)      swap = x_tail;          // tail sinks
            else if (cw[x] != cw[y])   swap = cw[x] < cw[y];   // larger first
            else                       swap = x > y;           // lower index first
            if (swap) { order[a] = y; order[b] = x; }
        }
    for (int k = 0; k < n_comp && taken < want; ++k)
    {
        const int i = order[k];
        if (cid[i] == c) continue;
        if (take[i] >= cw[i]) continue;
        ++take[i];
        ++taken;
    }

    for (int i = 0; i < n_comp; ++i) cw[i] -= take[i];

    // The gain lands on c's own slot, or claims the smallest named slot if it
    // is now larger than it, or falls into the tail. It NEVER silently
    // overwrites a larger culture.
    int ci = -1;
    for (int i = 0; i < culture_share_slots; ++i)
        if (cid[i] == c) { ci = i; break; }
    if (ci < 0)
        for (int i = 0; i < culture_share_slots; ++i)
            if (cid[i] < 0) { ci = i; cid[i] = c; cw[i] = 0; break; }
    if (ci < 0)
    {
        // All slots named, none is c. The smallest slot is the only one it may
        // displace, and only on a strict improvement — a tie leaves the
        // incumbent standing, the same tie-break discipline the sim's argmax
        // uses.
        int smallest = 0;
        for (int i = 1; i < culture_share_slots; ++i)
            if (cw[i] < cw[smallest] || (cw[i] == cw[smallest] && cid[i] > cid[smallest]))
                smallest = i;
        if (taken > cw[smallest])
        {
            cw[culture_share_slots] += cw[smallest]; // Demoted into the tail.
            cid[smallest] = c;
            cw[smallest]  = 0;
            ci = smallest;
        }
    }
    if (ci >= 0) cw[ci] += taken;
    else         cw[culture_share_slots] += taken; // Too small to be named yet.

    // Re-sort the named slots descending, ties on the lower culture index.
    for (int a = 0; a < culture_share_slots; ++a)
        for (int b = a + 1; b < culture_share_slots; ++b)
        {
            const bool a_dead = (cid[a] < 0 || cw[a] <= 0);
            const bool b_dead = (cid[b] < 0 || cw[b] <= 0);
            bool swap = false;
            if (a_dead != b_dead)     swap = a_dead;
            else if (a_dead)          swap = false;
            else if (cw[a] != cw[b])  swap = cw[a] < cw[b];
            else                      swap = cid[a] > cid[b];
            if (swap)
            {
                const int ti = cid[a], tw = cw[a];
                cid[a] = cid[b]; cw[a] = cw[b];
                cid[b] = ti;     cw[b] = tw;
            }
        }

    int named = 0;
    for (int i = 0; i < culture_share_slots; ++i)
    {
        if (cid[i] < 0 || cw[i] <= 0)
        {
            // An emptied slot returns whatever rounding left in it to the tail.
            if (cw[i] > 0) cw[culture_share_slots] += cw[i];
            id[i] = -1;
            weight_q[i] = 0;
            continue;
        }
        id[i] = static_cast<int16_t>(cid[i]);
        weight_q[i] = static_cast<int16_t>(cw[i]);
        named += cw[i];
    }
    // The tail is the residual by DEFINITION, so the invariant holds by
    // construction rather than by the arithmetic above happening to balance.
    other_q = static_cast<int16_t>(1000 - named);
}

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — the same splitmix64 shape as planetology.cpp /
// history_ladder.cpp / creeds.cpp, duplicated per the standing convention that
// each generation file owns its stream.
// ---------------------------------------------------------------------------
uint64_t splitmix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

struct rng
{
    uint64_t s;
    rng(uint32_t seed, uint32_t stage_tag)
        : s(splitmix64((static_cast<uint64_t>(seed) << 32) ^ (stage_tag * 0x9E3779B1u))) {}

    /// Integer in [0, n). Integer path only — every draw here participates in
    /// selection, and PLANETOLOGY.md bans a float argmax from a gate path.
    int pick(int n)
    {
        s = splitmix64(s);
        return n <= 0 ? 0 : static_cast<int>((s >> 33) % static_cast<uint64_t>(n));
    }
};

// FRESH stage tags — none collides with the ladder's (0x5A11 / 0xC4A7 /
// 0xF2A6), the creeds' (0xD317 / 0x1B47), the continents' (0xC017) or the
// planetology chain's.
constexpr uint32_t tag_settle   = 0x5E77u; // Region placement + founding dates.
constexpr uint32_t tag_furnace  = 0xF0F6u; // Industrialisation timing.
constexpr uint32_t tag_rupture  = 0x8017u; // The historical-rupture checkpoints.

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int64_t clampi64(int64_t v, int64_t lo, int64_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

int wrapc(int col, int gw) { return ((col % gw) + gw) % gw; }

int raster(int col, int row, int gw) { return wrapc(col, gw) + row * gw; }

/// Chebyshev distance with column wrapping; rows are open edges.
int grid_dist(int c0, int r0, int c1, int r1, int gw)
{
    int dc = std::abs(c0 - c1);
    if (dc > gw / 2) dc = gw - dc;
    return std::max(dc, std::abs(r0 - r1));
}

const tile_component* tile_at(const world& w, const std::vector<entity_id>& ids, int idx)
{
    if (idx < 0 || idx >= static_cast<int>(ids.size())) return nullptr;
    const auto it = w.tiles.find(ids[static_cast<std::size_t>(idx)]);
    return it == w.tiles.end() ? nullptr : &it->second;
}

// ---------------------------------------------------------------------------
// Where people settle
// ---------------------------------------------------------------------------

/// How strongly one tile invites a permanent settlement, 0-100, INTEGER.
///
/// This is the ladder's `agrarian_score` grown up: it keeps the composition /
/// landform terms and adds the two the ladder had to name as missing. RIVER
/// CONNECTIVITY is no longer substituted — BL-170 landed, so `river_edges` is
/// read directly, which is why the wetland weight no longer has to carry the
/// floodplain signal on its own.
int settle_score(const tile_component& t, bool coastal)
{
    // WHAT GROWS HERE sets the score; WHAT THE GROUND IS decides how much of it
    // is real (BL-519). The pre-split table read one slot and so had to choose
    // between naming the cover and naming the ground — which is exactly why a
    // forested crag and a forested floodplain scored identically.
    int score = 0;
    switch (t.cover)
    {
        case terrain_cover::grass:  score = 62; break;
        case terrain_cover::marsh:  score = 58; break; // Floodplain, now that rivers are their own term.
        case terrain_cover::forest: score = 44; break;
        case terrain_cover::scrub:  score =  9; break; // The old `tundra` row.
        default:                    score =  0; break;
    }

    switch (t.substrate)
    {
        // Real soil: the cover's score stands, and every pre-split number is
        // reproduced exactly (grass 62, marsh 58, forest 44, scrub 9).
        case terrain_substrate::sedimentary:
            break;
        // Vegetation on rock is worth SOMETHING and not much — a third. Bare rock
        // keeps its old 6. This is the pair the split exists to express.
        case terrain_substrate::rocky:
            score = score > 0 ? score / 3 : 6;
            break;
        // Ocean, ice, barren, regolith, metallic, volcanic: nothing to settle for,
        // whatever happens to be lying on top.
        default:
            return 0;
    }
    if (score <= 0)
        return 0;

    switch (t.landform)
    {
        case terrain_landform::valley:   score += 18; break;
        case terrain_landform::plains:   score += 12; break;
        case terrain_landform::highland: score -=  8; break;
        case terrain_landform::canyon:   score -= 10; break;
        case terrain_landform::mountain: return 0;
        default: break;
    }

    if (t.river_edges != 0) score += 22; // A river is the cheapest road there is.
    if (coastal)            score += 14; // And a harbour is the cheapest of all.

    score += static_cast<int>(t.habitability * 18.0f);
    score -= static_cast<int>(t.hazard_level * 20.0f);
    return clampi(score, 0, 100);
}

bool touches_ocean(const world& w, const std::vector<entity_id>& ids,
                   int col, int row, int gw, int gh)
{
    const int dc[4] = { 0, 0, -1, 1 };
    const int dr[4] = { -1, 1, 0, 0 };
    for (int i = 0; i < 4; ++i)
    {
        const int nr = row + dr[i];
        if (nr < 0 || nr >= gh) continue;
        const tile_component* t = tile_at(w, ids, raster(col + dc[i], nr, gw));
        if (t && is_water(t->substrate)) return true; // BL-516
    }
    return false;
}

/// The ANCIENT endowment under a region — surveyed once, over the window the
/// region's people would have walked. These deposits predate everyone; what
/// changes across a campaign is who ends up standing on them.
///
/// Held as RAW per-tile-mean richness in thousandths, not as a 0-1000 score:
/// the four classes live on completely different absolute scales (a rich coal
/// window and a rich grain window are nowhere near the same number), so an
/// absolute gain either saturates one class or never fires another. The scores
/// are computed later, against the world's own means — see `score_endowments`.
struct endowment
{
    int farm = 0, ore = 0, energy = 0, water = 0; ///< Per-tile mean × 1000.
};

endowment survey_endowment(const world& w, const std::vector<entity_id>& ids,
                           int col, int row, int gw, int gh)
{
    const int win = std::max(3, gw / 45);
    float farm = 0.0f, ore = 0.0f, energy = 0.0f;
    int cells = 0, water = 0;

    for (int dr = -win; dr <= win; ++dr)
    {
        const int r = row + dr;
        if (r < 0 || r >= gh) continue;
        for (int dc = -win; dc <= win; ++dc)
        {
            const tile_component* t = tile_at(w, ids, raster(col + dc, r, gw));
            if (!t) continue;
            ++cells;
            if (is_water(t->substrate)) { ++water; continue; } // BL-516

            const auto& d = t->resource_deposit;
            farm   += d[static_cast<std::size_t>(resource_type::agricultural_produce)];
            ore    += d[static_cast<std::size_t>(resource_type::iron_ore)]
                    + d[static_cast<std::size_t>(resource_type::copper_ore)]
                    + d[static_cast<std::size_t>(resource_type::rare_earth_ore)];
            energy += d[static_cast<std::size_t>(resource_type::coal)]
                    + d[static_cast<std::size_t>(resource_type::petroleum)];
        }
    }

    const int n = std::max(1, cells);
    endowment e;
    // Integer throughout — these feed a class argmax, and PLANETOLOGY.md bans a
    // float argmax from a gate path.
    e.farm   = static_cast<int>(farm   * 1000.0f) / n;
    e.ore    = static_cast<int>(ore    * 1000.0f) / n;
    e.energy = static_cast<int>(energy * 1000.0f) / n;
    e.water  = (water * 1000) / n;
    return e;
}

/// Score one class against the world's own mean for that class: an average
/// region scores 500, twice the average scores 1000.
///
/// RELATIVE, NOT ABSOLUTE, and that is the load-bearing choice. "Ore country"
/// only means anything next to the rest of the map, and a world-relative score
/// also survives the `deposit_scalar` abundance tier (GENERATION_STRATEGY.md §
/// The resource ceiling) without re-tuning — a lean world still has its own
/// ore regions, they are just poorer in absolute terms.
int score_against(int raw, int mean)
{
    return clampi((raw * 500) / std::max(1, mean), 0, 1000);
}

/// Argmax over the scored endowment, with an explicit fixed order so a tie is
/// resolved by the class list rather than by container order.
region_class classify(int farm_q, int ore_q, int energy_q, int port_q)
{
    int best = farm_q;
    region_class c = region_class::farm;
    if (ore_q    > best) { best = ore_q;    c = region_class::ore; }
    if (energy_q > best) { best = energy_q; c = region_class::energy; }

    // A harbour competes, but at a discount: a coastal region sitting on real
    // ore is an ore region that happens to have a harbour, not the reverse.
    // What lives on trade is the region with a coastline and nothing under it.
    if ((port_q * 4) / 5 > best) { best = (port_q * 4) / 5; c = region_class::port; }

    // Below the world's average on every axis, there is nothing to name.
    return best < 300 ? region_class::none : c;
}

const char* class_word(region_class c)
{
    switch (c)
    {
        case region_class::farm:   return "grain";
        case region_class::ore:    return "ore";
        case region_class::energy: return "coal and oil";
        case region_class::port:   return "harbour";
        default:                     return "little";
    }
}

/// Does this culture's pantheon hold a seat with the given domain? The creed
/// pass raises a FORGE god only where the cradle's window held ore, and a
/// SEALED OATH god only in the charter cradle — so asking this question is
/// asking what the land taught the people, one stage earlier.
bool creed_holds(const creed_state& cs, int culture, const char* domain)
{
    if (culture < 0 || culture >= static_cast<int>(cs.cultures.size())) return false;
    for (const culture_god& g : cs.cultures[static_cast<std::size_t>(culture)].pantheon)
        if (g.domain == domain) return true;
    return false;
}

/// Which of the nine quarter slots this anchor falls in: 0-4 are the north→south
/// bands, 5-8 the dawnward→outer sectors. Split out from the word lookup so the
/// POSITIONAL rule and the LANGUAGE it is spoken in stay independent — BL-348
/// changed the second and deliberately left the first alone, because the point
/// of naming a region for where it sits is that the name carries a fact about
/// the ground.
int quarter_slot(int col, int row, int gw, int gh)
{
    const int band = (row * 5) / std::max(1, gh);
    const int sect = (col * 4) / std::max(1, gw);
    // One axis or the other; the choice is stable for a given anchor.
    return ((band + sect) & 1) ? clampi(band, 0, 4) : 5 + clampi(sect, 0, 3);
}

/// The quarter half of a region name, in the region's OWN tongue (BL-348).
///
/// Before BL-290 the whole name was Earth-flavoured, which was at least
/// consistent. After it, the culture half was coined from the region's own
/// phonology and this half was still English — "MelethWorirUlael Reach" put two
/// naming systems side by side in one string, which reads as a bug rather than
/// a style. This is consumption again, not a new mechanism: `coin_lexicon` is a
/// pure function of the tongue, so a culture's quarter words are the same
/// wherever they are asked for, without any pass sharing a stream.
///
/// The English table survives ONLY as the unusable-tongue fallback — a culture
/// with no phoneme inventory cannot coin anything, and a region with no name
/// at all would be worse than one with an out-of-register name.
std::string quarter_word(const tongue_lexicon& lex, int col, int row, int gw, int gh)
{
    const int slot = quarter_slot(col, row, gw, gh);
    if (static_cast<std::size_t>(slot) < lex.quarter.size())
        return lex.quarter[static_cast<std::size_t>(slot)];

    static const char* fallback[9] = { "northern", "upper", "middle", "lower", "southern",
                                       "dawnward", "inland", "duskward", "outer" };
    return fallback[clampi(slot, 0, 8)];
}

} // namespace

// ---------------------------------------------------------------------------
// Stage 1 — the enforceable promise, as a diffusion frame (BL-638)
//
// Three facts the ladder and the creeds have ALREADY produced, read a third
// time rather than re-derived:
//
//   1. WHERE. `history_ladder_state::charter_cradle` is the best-placed
//      trading cradle — "coastal access dominates", because Stage 1 is about
//      promises between STRANGERS and strangers arrive by sea.
//   2. WHO. `run_creeds` plants the sealed-oath god ("who witnesses every
//      bargain") on exactly that cradle's culture, and calls it "the seed
//      Stage 1's Charter Act grows from". That culture is the one people who
//      do not merely know the promise but keep it.
//   3. HOW FAR. Barrier terrain prices contact the same way it prices
//      conquest, so the ladder's `conquest_cost_q` is the resistance the
//      charter's copies have to cross.
//
// Nothing here reads a furnace date, which is the point: Stage 4 never runs on
// an antiquity world, Stage 1 runs in every era.
// ---------------------------------------------------------------------------

charter_reach derive_charter_reach(const history_ladder_state& hl,
                                   const creed_state& cs, int gw, int gh)
{
    charter_reach ch;
    ch.grid_w = gw;
    if (gw <= 0 || gh <= 0) return ch;

    // No cradle -> no charter was ever written -> nothing reached the promise.
    // An honest all-closed world, not a degenerate one: it is the state the
    // public floor's "unmeetable by construction" waiver exists for.
    if (hl.charter_cradle < 0
     || static_cast<std::size_t>(hl.charter_cradle) >= hl.cradles.size())
        return ch;

    const agrarian_cradle& seat =
        hl.cradles[static_cast<std::size_t>(hl.charter_cradle)];
    ch.written = true;
    ch.col = seat.col;
    ch.row = seat.row;

    // The oath-keepers. Walked in cradle order, so the first match is the only
    // match `run_creeds` could have produced (one culture per cradle).
    for (std::size_t ci = 0; ci < cs.cultures.size(); ++ci)
        if (cs.cultures[ci].cradle == hl.charter_cradle)
        {
            ch.culture = static_cast<int>(ci);
            break;
        }

    // The two radii. `span` is the largest value `grid_dist` can return on this
    // grid, so both bands are stated as a FRACTION OF THE WORLD rather than as
    // a tile count that would silently change meaning if the grid did.
    //
    // The bounds are a shape, not a target: the charter never covers the whole
    // world (there is always ground it never reached, so `closed` stays a real
    // state) and never fails to cover its own hinterland (so `public` stays a
    // real state). Resistance slides between them.
    const int span = std::max(gw / 2, gh - 1);
    const int floor_d = std::max(1, span / 10);
    const int ceil_d  = std::max(floor_d, span / 2);
    const int resist  = clampi(hl.conquest_cost_q, 0, 1000);
    ch.far_dist   = floor_d + ((ceil_d - floor_d) * (1000 - resist)) / 1000;
    ch.near_dist  = ch.far_dist / 2;
    ch.port_reach = span / 5;
    return ch;
}

namespace {

/// Contact distance from the seat, discounted by how much of a port the region
/// is — the sea road the charter actually travels.
int charter_contact(const charter_reach& ch, const region& p)
{
    const int d = grid_dist(p.col, p.row, ch.col, ch.row, ch.grid_w);
    return d - (ch.port_reach * clampi(p.port_q, 0, 1000)) / 1000;
}

} // namespace

bool charter_lived(const charter_reach& ch, const region& p)
{
    if (!ch.written) return false;
    // The oath-keepers carry it wherever they are — and only where they still
    // are. Read from the CURRENT culture, not the founding one, so a conquest
    // that replaced the founders' gods took their institutions with them, and
    // a charter people who conquered outward carried theirs along.
    //
    // BL-826 — MAJORITY, not plurality. A sealed oath is an institution, and an
    // institution does not survive on a third of the ground: a region where the
    // oath-keepers are merely the largest of three peoples has stopped keeping
    // it. Half-digested ground therefore falls back to the CONTACT test below,
    // which is the honest answer for a place the charter reaches but no longer
    // holds. On unconquered ground (pure shares) this is exactly the old test.
    if (ch.culture >= 0 && p.culture.majority(ch.culture)) return true;
    return charter_contact(ch, p) <= ch.near_dist;
}

bool charter_copied(const charter_reach& ch, const region& p)
{
    if (!ch.written) return false;
    // BL-826 — majority, for the same reason `charter_lived` uses it.
    if (ch.culture >= 0 && p.culture.majority(ch.culture)) return true;
    return charter_contact(ch, p) <= ch.far_dist;
}

namespace
{

/// Coin a daughter culture from its parent (BL-856).
///
/// DERIVED, NOT ROLLED FRESH, and that is the whole of what makes the output a
/// FAMILY of peoples rather than a bag of unrelated ones. The daughter keeps its
/// parent's pantheon and its parent's cradle — it is the same people, later and
/// further away — and diverges in the two places a separated people actually
/// diverges: its NAME, and the SOUNDS it coins names from.
///
/// THE TONGUE DRIFTS RATHER THAN RE-ROLLING. A fresh `roll_tongue` would give a
/// daughter a phonology unrelated to its parent's, so the two would not read as
/// kin — and reading as kin is the entire deliverable, since it is what lets a
/// player see at a glance that two nations on opposite coasts came from the same
/// migration. So the inventory is inherited and perturbed: one sound dropped and
/// one admitted, deterministically from the seed.
///
/// AGGRESSION DRIFTS A LITTLE, because doctrine is downstream of settlement and
/// a people who spent six centuries walking are not quite who they were. Bounded
/// hard, so a chain of splits cannot walk a culture to either extreme.
///
/// EVERY NAME STAYS SCI-FI/FANTASY, out of the seeded phoneme tables — never an
/// Earth proper noun (.claude/rules/io-standing-rules.md § Terms & docs).
culture derive_daughter_culture(const culture& parent, int parent_id, int8_t origin_class,
                                uint32_t seed, int spawn_index, int64_t coined_year)
{
    culture d = parent;                 // Pantheon, cradle and speech inherited whole.
    // DESCENT (BL-865). The tree the migration builds is retained rather than
    // discarded, because kinship is what the empire phase reads for how alike
    // two peoples are (CIVILISATION.md § Culture relations).
    d.parent            = parent_id;
    d.origin_farm_class = origin_class;
    d.coined_year       = coined_year;
    rng r(seed, static_cast<uint32_t>(0xDA05u + spawn_index));

    // Drift the inventory: drop one onset, admit one from a fixed pool. Both
    // picks are seeded, and the pool is the same table `roll_tongue` draws from,
    // so a daughter's sounds stay inside the language family.
    if (d.speech.onsets.size() > 4)
        d.speech.onsets.erase(d.speech.onsets.begin()
                              + r.pick(static_cast<int>(d.speech.onsets.size())));
    static const char* const drift_onsets[] = { "k", "t", "m", "n", "s", "r", "l", "v",
                                                "th", "sh", "g", "d", "b", "h", "z", "kh" };
    d.speech.onsets.push_back(drift_onsets[r.pick(16)]);
    if (!d.speech.vowels.empty() && d.speech.vowels.size() < 6)
    {
        static const char* const drift_vowels[] = { "a", "e", "i", "o", "u", "ai", "ua", "ei" };
        d.speech.vowels.push_back(drift_vowels[r.pick(8)]);
    }

    // A NEW NAME IN THE DRIFTED TONGUE. Two syllables, as the cradle names are.
    std::string coined = tongue_word(r, d.speech, 2);
    d.name = coined.empty() ? parent.name : coined;

    d.aggression_q = clampi(parent.aggression_q - 60 + r.pick(121), 0, 1000);
    return d;
}

} // namespace

// ---------------------------------------------------------------------------
// The settlement pass
// ---------------------------------------------------------------------------

settlement_state run_settlement(const planetology_state& pl,
                                const history_ladder_state& hl,
                                const creed_state& cs,
                                const world& w,
                                const std::vector<entity_id>& tile_ids,
                                int gw, int gh,
                                int target_regions,
                                uint32_t seed,
                                int64_t stop_year,
                                int64_t sim_start_year)
{
    settlement_state out;
    if (gw <= 0 || gh <= 0 || target_regions <= 0 || hl.cradles.empty())
        return out;

    // Stage 1's frame (BL-638). Derived here because this pass already holds
    // both inputs and every downstream consumer already holds this record.
    // RNG-free, so it perturbs no stream.
    out.charter = derive_charter_reach(hl, cs, gw, gh);

    const int total = gw * gh;
    rng r(seed, tag_settle);

    // BL-348: each culture's quarter words, coined once and reused. `coin_lexicon`
    // is a pure function of the tongue, so calling it per region would give the
    // same answer — this only avoids re-hashing an inventory per region. An
    // out-of-range or unusable culture yields an empty lexicon, which is exactly
    // what `quarter_word`'s fallback branch is written for.
    // RESOLVE A CULTURE ID ACROSS BOTH LISTS (BL-856). Ids below
    // `cs.cultures.size()` are the cradle cultures the creeds pass coined; ids
    // at or above it are the ones the MIGRATION coined, which live in
    // `out.spawned_cultures` until the caller appends them to the roster.
    //
    // THIS EXISTS BECAUSE ITS ABSENCE SEGFAULTED. The naming line below read
    // `cs.cultures[best_c]` unguarded, which was safe for exactly as long as
    // every culture came from the creeds pass -- and stopped being safe the
    // moment a region could carry a culture the walk had coined. Nothing in the
    // type system was going to catch that: the id is a plain int, and the
    // out-of-bounds read was into a live vector, so it crashed rather than
    // returning nonsense only because the index ran far enough past the end.
    const auto culture_at = [&](int id) -> const ::culture* {
        if (id < 0) return nullptr;
        if (id < static_cast<int>(cs.cultures.size()))
            return &cs.cultures[static_cast<std::size_t>(id)];
        const int local = id - static_cast<int>(cs.cultures.size());
        if (local < static_cast<int>(out.spawned_cultures.size()))
            return &out.spawned_cultures[static_cast<std::size_t>(local)];
        return nullptr;
    };

    std::unordered_map<int, tongue_lexicon> lex_cache;
    const auto lexicon_for = [&](int culture) -> const tongue_lexicon& {
        const auto it = lex_cache.find(culture);
        if (it != lex_cache.end())
            return it->second;
        tongue_lexicon lex;
        // `::culture` because this lambda's own parameter is named `culture`
        // and shadows the type.
        if (const ::culture* cu = culture_at(culture))
            lex = coin_lexicon(cu->speech);
        return lex_cache.emplace(culture, std::move(lex)).first->second;
    };

    // --- The colonisation diffusion, run once before anything is placed -------
    //
    // BL-846/BL-848. Every founding below takes its DATE and its CULTURE from
    // this field rather than from a settle-score formula and a nearest-cradle
    // distance. `docs/generation/COLONISATION.md` is the authority; what this
    // block does is hand the placement loop two answers it used to invent.
    //
    // WHY IT RUNS HERE AND NOT INSIDE THE SIM. The sim has no world& by
    // deliberate design (history_sim.hpp), and the walk needs the tile raster.
    // This pass already holds both, and it is the pass that decides which ground
    // is founded — so the flood belongs on this side of the seam and the
    // SCHEDULE it produces is what crosses.
    //
    // ONE FLOOD FOR THE WHOLE MAP, seeded from the cradles alone. Not from every
    // candidate site: a candidate is not a people, and seeding the walk from the
    // ground it is supposed to be deciding would make the answer circular.
    std::vector<terrain_substrate> col_sub(static_cast<std::size_t>(total),
                                           terrain_substrate::ocean);
    std::vector<terrain_cover>     col_cov(static_cast<std::size_t>(total),
                                           terrain_cover::none);
    std::vector<terrain_landform>  col_lf(static_cast<std::size_t>(total),
                                          terrain_landform::plains);
    std::vector<uint8_t>           col_river(static_cast<std::size_t>(total), 0u);
    for (int idx = 0; idx < total; ++idx)
    {
        const tile_component* t = tile_at(w, tile_ids, idx);
        if (t == nullptr) continue;
        col_sub[static_cast<std::size_t>(idx)] = t->substrate;
        col_cov[static_cast<std::size_t>(idx)] = t->cover;
        col_lf [static_cast<std::size_t>(idx)] = t->landform;
        // RIVERS, WHICH THE WALK COULD NOT SEE UNTIL NOW (BL-857).
        // COLONISATION.md names river courses as the cheapest ground of all and
        // the walk was pricing the coast as its cheapest route, because a river
        // in this codebase is an EDGE on the tile rather than a tile property
        // and `sim_terrain_view` carries no river array.
        //
        // It needs none: this pass reads `tile_component` directly, so the
        // raster is built here from `river_edges` and BL-853 (the SIM's view
        // carrying rivers) is not a prerequisite after all -- it stays open for
        // the sim's own consumers, which is a different need.
        col_river[static_cast<std::size_t>(idx)] = t->river_edges != 0 ? 1u : 0u;
    }

    // A CULTURE'S CRADLE IS ITS SOURCE, so the stream that arrives somewhere
    // carries a culture index the rest of this pass already understands. Walked
    // in culture order, which is the sorted order `creed_state` holds them in —
    // the flood's tie-break reads `source_region`, so the walk order here is
    // what makes two streams arriving in the same year resolve identically on
    // every machine.
    std::vector<colonisation_source> col_sources;
    col_sources.reserve(cs.cultures.size());
    for (std::size_t ci = 0; ci < cs.cultures.size(); ++ci)
    {
        const int cr = cs.cultures[ci].cradle;
        if (cr < 0 || cr >= static_cast<int>(hl.cradles.size())) continue;
        const agrarian_cradle& ac = hl.cradles[static_cast<std::size_t>(cr)];
        const int anchor = ac.row * gw + ac.col;
        if (anchor < 0 || anchor >= total) continue;
        col_sources.push_back(colonisation_source{
            anchor,
            static_cast<int32_t>(col_sources.size()),
            static_cast<int32_t>(ci),
            // EVERY CRADLE STARTS AT THE SAME MOMENT, and that is the honest
            // reading of Stage 0: the cradles are where agriculture began, not
            // a staggered set of later arrivals. What separates them afterwards
            // is the ground and the package, never a head start.
            colonisation_start_year,
            coin_package(col_sub, col_cov, col_lf, gw, gh, ac.col, ac.row,
                         colonisation_cradle_window)});
    }

    // A CRADLE CULTURE IS A PEOPLE OF ITS OWN COUNTRY TOO (BL-865). The daughters
    // get their origin class from the split that made them; the twelve that were
    // never split have to take theirs from the ground they started on, or half
    // the tree carries the field and half does not — and an opposition axis with
    // holes in it is worse than none.
    //
    // Written onto the SOURCE list rather than `cs.cultures`, which is const
    // here; `hard_coded_world` copies it back onto the roster beside the spawned
    // ones.
    for (colonisation_source& src0 : col_sources)
        if (src0.culture >= 0 && src0.tile >= 0 && src0.tile < total)
            out.cradle_origin_class.emplace_back(
                src0.culture,
                static_cast<int8_t>(classify_farm_class(
                    col_sub[static_cast<std::size_t>(src0.tile)],
                    col_cov[static_cast<std::size_t>(src0.tile)],
                    col_lf [static_cast<std::size_t>(src0.tile)],
                    /*shoreline=*/false)));

    colonisation_input col_in;
    col_in.substrate     = &col_sub;
    col_in.cover         = &col_cov;
    col_in.landform      = &col_lf;
    col_in.river         = &col_river;
    col_in.gw            = gw;
    col_in.gh            = gh;
    // Daughter ids run one past the last cradle culture (BL-856).
    col_in.first_spawn_culture = static_cast<int32_t>(cs.cultures.size());
    // THE WALK IS NOT CAPPED BY THE CAMPAIGN EPOCH (BL-858). `stop_year` is
    // where the GAME starts, not where the migration finishes, and capping the
    // flood at it silently truncated any stream still walking — ground that
    // would have been peopled read as ground nobody could reach.
    //
    // The flood needs no year cap at all: it is bounded by the tile count and
    // terminates when its frontier is exhausted. `stop_year` is still passed as
    // a far backstop so an arrival year cannot run to an absurd value on a
    // pathological map, and `colonisation_field::last_arrival_year` reports when
    // the migration actually ended.
    col_in.boundary_year = stop_year;
    const colonisation_field col_field = run_colonisation(col_in, col_sources);

    // WHEN THE MIGRATION ENDED, for the round that displays it (BL-858). Ben's
    // rule is "all land has some culture"; read literally that never terminates,
    // because ground no package can farm never gets one — and BL-859's census
    // shows that is 44-57% of every world, dominated by polar ice. What the rule
    // MEANS is "wait until nothing more is going to happen", and the flood
    // answers that exactly: the last landing is the end of the migration.
    out.migration_end_year = col_field.last_arrival_year;

    // MATERIALISE THE CULTURES THE MIGRATION COINED (BL-856). The walk allocates
    // ids and records parentage; it has no vocabulary for a pantheon or a tongue
    // and should not grow one. This turns each spawn into a real `culture`
    // derived from its parent.
    //
    // IN ALLOCATION ORDER, which is arrival order, so id N is always the Nth
    // people to come into being and the numbering is stable across machines.
    // A spawn's parent is always ALREADY materialised when it is read: ids are
    // handed out in ascending arrival order and a daughter's parent arrived
    // strictly earlier, so the vector only ever grows behind the read.
    out.spawned_cultures.reserve(col_field.spawns.size());
    for (std::size_t si = 0; si < col_field.spawns.size(); ++si)
    {
        const culture_spawn& sp = col_field.spawns[si];
        const int pid = sp.parent;
        const culture* par = nullptr;
        if (pid >= 0 && pid < static_cast<int>(cs.cultures.size()))
            par = &cs.cultures[static_cast<std::size_t>(pid)];
        else
        {
            const int local = pid - static_cast<int>(cs.cultures.size());
            if (local >= 0 && local < static_cast<int>(out.spawned_cultures.size()))
                par = &out.spawned_cultures[static_cast<std::size_t>(local)];
        }
        if (par == nullptr) { out.spawned_cultures.push_back(culture{}); continue; }
        out.spawned_cultures.push_back(
            derive_daughter_culture(*par, pid, static_cast<int8_t>(sp.origin_class),
                                    seed ^ 0xC0DAu, static_cast<int>(si),
                                    sp.coined_year));
    }

    // --- Score every tile once, in raster order --------------------------------
    std::vector<int> score(static_cast<std::size_t>(total), 0);
    for (int idx = 0; idx < total; ++idx)
    {
        const tile_component* t = tile_at(w, tile_ids, idx);
        if (!t || is_water(t->substrate)) continue; // BL-516
        const int col = idx % gw, row = idx / gw;
        score[static_cast<std::size_t>(idx)] =
            settle_score(*t, touches_ocean(w, tile_ids, col, row, gw, gh));
    }

    // --- Rank candidates: score descending, then raster index ascending --------
    // The index tie-break is the whole determinism story here; two tiles with
    // equal ground must not depend on sort stability alone.
    std::vector<int> order;
    order.reserve(static_cast<std::size_t>(total) / 4);
    for (int idx = 0; idx < total; ++idx)
        if (score[static_cast<std::size_t>(idx)] > 0) order.push_back(idx);
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        const int sa = score[static_cast<std::size_t>(a)];
        const int sb = score[static_cast<std::size_t>(b)];
        return sa != sb ? sa > sb : a < b;
    });

    // --- Place regions greedily under a separation rule ----------------------
    std::vector<endowment> raw; // Parallel to out.regions; scored below.
    const int sep = std::max(3, gw / 40);
    for (int idx : order)
    {
        if (static_cast<int>(out.regions.size()) >= target_regions) break;
        const int col = idx % gw, row = idx / gw;

        bool too_close = false;
        for (const region& p : out.regions)
            if (grid_dist(col, row, p.col, p.row, gw) < sep) { too_close = true; break; }
        if (too_close) continue;

        region p;
        p.anchor = idx;
        p.col = col;
        p.row = row;
        // BL-777: derived, not assumed. This pass already refuses water above
        // (`score` is 0 on any water tile, BL-516), so the answer is always
        // `land` here — but deriving it from the tile rather than writing the
        // default keeps ONE writer's rule for the field, so a future change to
        // the scoring filter cannot silently mislabel the ground.
        {
            const tile_component* at = tile_at(w, tile_ids, idx);
            p.domain = at ? region_domain_of(at->substrate) : region_domain::land;
        }
        p.settle_score_q = clampi(score[static_cast<std::size_t>(idx)] * 10, 0, 1000);

        // WHOSE GODS — THE CULTURE OF THE STREAM THAT REACHED IT (BL-848).
        //
        // This supersedes the nearest-cradle rule, which was a straight-line
        // geometric assignment standing in for a history nothing was
        // simulating. Under the diffusion the same field is a PATH fact:
        // whoever's stream arrived, arrived. A cradle no longer owns the ground
        // on the far side of its own mountains — the culture that came the long
        // way round by the coast does, because it got there first.
        //
        // That is what makes CREEDS.md's "a record of who walked where" true
        // rather than merely claimed: it could not be delivered before, because
        // nobody walked.
        const std::size_t ci_tile = static_cast<std::size_t>(idx);
        const int best_c = col_field.culture.empty() ? -1 : col_field.culture[ci_tile];

        // GROUND NO STREAM REACHED IS NOT SETTLED, and ground no arriving
        // package could farm is not settled either. Emptiness is a real outcome
        // here, not a failure to fill — so this is a `continue`, not a fallback
        // to some nearest cradle. A candidate site that scored well and was
        // never reached simply has nobody living on it.
        if (best_c < 0 || col_field.arrival_year[ci_tile] == colonisation_never_reached
            || !col_field.farmable[ci_tile])
            continue;

        // BL-826: a region arrives WHOLLY its cradle's. Mixing is something
        // history does to it, never something settlement hands it.
        p.culture = culture_shares::pure(best_c);
        p.founding_culture = best_c;

        raw.push_back(survey_endowment(w, tile_ids, col, row, gw, gh));

        // WHEN THE STREAM GOT THERE, in years, over ground alone.
        //
        // The old rule was `-2000 + (1000 - settle_score_q) * 3 + rng(120)` —
        // better ground settled earlier, which reads as sensible and encodes
        // nothing about distance, route or barrier. Under it a valley forty
        // tiles beyond a mountain range was founded on the same terms as one
        // beside the first granary, which is exactly the missing frontier
        // BL-847 was written about.
        //
        // THE RNG DRAW IS GONE, and deliberately: an arrival year is a
        // consequence of the ground, not a roll on top of one
        // (GENERATION_STRATEGY.md § Asymmetry is the deliverable). Nothing here
        // consumes randomness any more.
        p.founded_year = col_field.arrival_year[ci_tile];

        const ::culture* named = culture_at(best_c);
        const std::string people = named ? named->name : std::string("nameless");
        p.name = people + " " + quarter_word(lexicon_for(best_c), col, row, gw, gh);

        out.regions.push_back(std::move(p));
    }

    if (out.regions.empty())
        return out;

    // --- Score the endowments against the world's own means --------------------
    {
        int64_t sf = 0, so = 0, se = 0, sw = 0;
        for (const endowment& e : raw)
        {
            sf += e.farm; so += e.ore; se += e.energy; sw += e.water;
        }
        const int n = static_cast<int>(raw.size());
        const int mf = static_cast<int>(sf / n), mo = static_cast<int>(so / n);
        const int me = static_cast<int>(se / n), mw = static_cast<int>(sw / n);

        for (std::size_t i = 0; i < out.regions.size(); ++i)
        {
            region& p = out.regions[i];
            const endowment& e = raw[i];
            p.farm_q   = score_against(e.farm,   mf);
            p.ore_q    = score_against(e.ore,    mo);
            p.energy_q = score_against(e.energy, me);
            p.port_q   = score_against(e.water,  mw);
            p.dominant = classify(p.farm_q, p.ore_q, p.energy_q, p.port_q);
        }
    }

    // --- Antiquity stop (BL-271) ----------------------------------------------
    // Below the industrial era the pass generates the world AT `stop_year`:
    // regions founded later do not exist yet (the year-tick sim founds them),
    // and the population that HAS arrived is seeded at founding and grown
    // logistically to the start year. RNG-safe: the founding loop above already
    // consumed its draws for every candidate, so the streams match the 1960 arc.
    const bool antiquity = stop_year < 1700;
    if (antiquity)
    {
        out.regions.erase(
            std::remove_if(out.regions.begin(), out.regions.end(),
                           [stop_year](const region& p) { return p.founded_year > stop_year; }),
            out.regions.end());

        for (region& p : out.regions)
        {
            // Founding band ~2k-26k settlers, richer ground drawing more; the
            // logistic model then does the two millennia (or fewer) of growth.
            p.population = 2000 + static_cast<int64_t>(p.farm_q) * 24;
            p.last_demography_year = p.founded_year;
            advance_region_demography(
                p, static_cast<int>(stop_year - p.founded_year), /*war_pressure_q=*/0);
        }

        // --- THE FOUNDING SCHEDULE (BL-846) -------------------------------
        //
        // Regions the colonisation walk dated AFTER the sim starts are not
        // placed on the map here. They are handed to the sim, which founds each
        // one as its year comes round — so the world FILLING is inside the
        // recorded span instead of finished before it opens.
        //
        // WHY THIS IS THE WHOLE FIX for "phase 4 reads as combined colonisation
        // and conquest" (Ben, 2026-09-09). Every region existed at tick zero, so
        // the only thing a time-lapse could ever show was borders moving. The
        // spreading had already happened, off-screen, in a pass with no clock.
        //
        // The partition is STABLE — a region either sits before the sim's start
        // or after it, and nothing here re-scores or re-orders. `regions` keeps
        // its placement order (best ground first), which the nation seeds read.
        if (sim_start_year != INT64_MAX)
        {
            std::vector<region> present;
            present.reserve(out.regions.size());
            for (region& p : out.regions)
            {
                if (p.founded_year > sim_start_year)
                    out.pending_foundings.push_back(std::move(p));
                else
                    present.push_back(std::move(p));
            }
            out.regions = std::move(present);

            // ASCENDING BY YEAR, ties on the anchor — a TOTAL order, so two
            // regions dated to the same year are founded in an order that
            // cannot depend on a sort's stability or on placement order having
            // survived the partition.
            std::sort(out.pending_foundings.begin(), out.pending_foundings.end(),
                      [](const region& a, const region& b) {
                          if (a.founded_year != b.founded_year)
                              return a.founded_year < b.founded_year;
                          return a.anchor < b.anchor;
                      });

            // A SCHEDULED REGION IS SEEDED AT ITS FOUNDING, NOT GROWN TO THE
            // EPOCH. The loop above grew every region from its founding year to
            // `stop_year`; for one that has not been founded yet that is
            // centuries of growth before anybody lives there. Reset to the
            // founding band and let the sim's own demography carry it forward
            // from the year it actually arrives.
            for (region& p : out.pending_foundings)
            {
                p.population           = 2000 + static_cast<int64_t>(p.farm_q) * 24;
                p.last_demography_year = p.founded_year;
                p.army_stock           = 0; // No garrison before there are people.
                replenish_manpower(p);
            }
        }
    }

    // --- Stage 4: WHO CAN light the furnaces, and how long their ground takes --
    // "Coal-near-cities made Britain — endowment, not virtue" (HISTORY.md Stage
    // 4). The gate is the ground; the creed only moves the date, and only where
    // the creed itself came from the same ground (a forge god is raised over
    // ore, one stage earlier). Never reached by antiquity worlds: no furnace
    // has lit by year 0, and that history belongs to the sim, not the pass.
    //
    // THE DATE MOVED INSIDE THE RUN (BL-748). This block used to resolve
    // `industrial_year` and `industrialised` here, before `run_history_sim`
    // started — which under an industrial epoch is backwards, because the
    // second span is exactly where industrialisation happens. What it produces
    // now is the ENDOWMENT HALF only: the gate (above-average fuel) and the
    // LAG the ground imposes once its owner reaches the Industrial rung. The
    // arithmetic below is unchanged coefficient for coefficient; only its
    // anchor moved, from the calendar to the polity's own crossing.
    rng rf(seed, tag_furnace);
    const int arable_q = clampi(static_cast<int>(pl.arable_share * 1000.0f), 0, 1000);

    for (region& p : out.regions)
    {
        if (antiquity) break;
        // The gate is ABOVE-AVERAGE fuel, not any fuel: the scores are relative
        // to the world's own means (500 = average), so an average region sits
        // at 750 here and does not industrialise. Only the endowed do.
        const int fuel = p.energy_q + p.ore_q / 2;
        if (fuel < 900) continue; // No fuel, no ceiling to break.

        int64_t year = 1790
                     - p.energy_q / 12
                     - p.ore_q / 22
                     - arable_q / 200          // A fed population industrialises sooner.
                     + rf.pick(45);

        if (creed_holds(cs, p.culture.plurality(), "the forge"))       year -= 25;
        if (creed_holds(cs, p.culture.plurality(), "the sealed oath")) year -= 15; // Stage 3: contract law reaches capital.
        if (p.founded_year > 0) year += p.founded_year / 8;            // Late settlement, late furnaces.

        // THE SAME NUMBER, RE-ANCHORED. The clamp's own floor (1700) becomes
        // the ZERO of the lag axis, so the best-endowed ground in a world takes
        // 0 years past its polity's crossing and the worst takes 235 — exactly
        // the spread the absolute dates carried, expressed against a year the
        // sim knows rather than a calendar it does not.
        p.industrial_lag_years = clampi(static_cast<int>(year), 1700, 1935) - 1700;
        // `industrial_year` and `industrialised` are LEFT ALONE here. The run
        // sets them (history_sim.cpp § the furnace), and a region whose polity
        // never crosses the rung ends the epoch never having industrialised —
        // which is a legitimate outcome, not a gap to fill in.
    }

    // `median_industrial_year` is NOT computed here any more (BL-748). Nothing
    // has industrialised at this point in the pass by construction, so a median
    // taken here would be a guaranteed zero dressed as a measurement. The sim
    // recomputes it into this same field at the end of its run, which is the
    // first moment the answer exists; a world whose era never ran (
    // `prehistory_years == 0`) therefore reports 0 — nobody — and the
    // never-industrialised rung in corporation_generation.cpp fires, which is
    // the branch that exists for exactly that case.

    // --- The founding lines ----------------------------------------------------
    // Bounded deliberately: a biography with seventy founding lines is a table,
    // not a history. The six best-placed regions speak for the settlement
    // stage; the rest exist in the data and drive the map regardless.
    const int named = std::min<int>(6, static_cast<int>(out.regions.size()));
    for (int i = 0; i < named; ++i)
    {
        const region& p = out.regions[static_cast<std::size_t>(i)];
        out.history.push_back(history_event{
            years_from_calendar_year(p.founded_year), chain_stage::legacy,
            "The " + p.name + " is settled under the same gods as its cradle.",
            std::string("-> ") + class_word(p.dominant) + " country; the ground was there first" });
    }

    return out;
}

std::vector<int> settlement_seed_tiles(const settlement_state& ss)
{
    std::vector<int> seeds;
    seeds.reserve(ss.regions.size());
    for (const region& p : ss.regions)
        if (p.anchor >= 0) seeds.push_back(p.anchor);
    return seeds;
}

std::vector<int> settlement_seed_polities(const settlement_state& ss)
{
    // SAME FILTER, SAME ORDER as `settlement_seed_tiles` — the two are read as
    // parallel arrays by `generate_nations`, so a divergence here would silently
    // give a nation somebody else's history.
    std::vector<int> owners;
    owners.reserve(ss.regions.size());
    for (const region& p : ss.regions)
        if (p.anchor >= 0) owners.push_back(p.nation);
    return owners;
}

std::vector<int> derive_national_protection(const settlement_state& ss, int nation_count)
{
    std::vector<int> out(static_cast<std::size_t>(nation_count > 0 ? nation_count : 0), 0);
    for (const region& p : ss.regions)
    {
        if (p.nation < 0 || p.nation >= nation_count) continue;
        int& v = out[static_cast<std::size_t>(p.nation)];
        if (p.protection_q > v) v = p.protection_q;
    }
    return out;
}

int nearest_region(const settlement_state& ss, int col, int row, int gw)
{
    int best = -1, best_d = 1 << 30;
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        const int d = grid_dist(col, row, ss.regions[i].col, ss.regions[i].row, gw);
        if (d < best_d) { best_d = d; best = static_cast<int>(i); }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Political character — three axes, three derivations, no draws
// ---------------------------------------------------------------------------

namespace {

/// Reverse index from nation entity id to its position in `nation_ids`.
std::unordered_map<entity_id, int> nation_index(const std::vector<entity_id>& nation_ids)
{
    std::unordered_map<entity_id, int> m;
    m.reserve(nation_ids.size() * 2);
    for (std::size_t i = 0; i < nation_ids.size(); ++i)
        m[nation_ids[i]] = static_cast<int>(i);
    return m;
}

/// Per-tile owning nation index, in raster order (-1 = unowned/ocean).
std::vector<int> owner_map_of(const world& w,
                              const std::vector<entity_id>& tile_ids,
                              const std::unordered_map<entity_id, int>& nidx,
                              int gw, int gh)
{
    const int total = gw * gh;
    std::vector<int> owner(static_cast<std::size_t>(total), -1);
    for (int idx = 0; idx < total; ++idx)
    {
        if (idx >= static_cast<int>(tile_ids.size())) break;
        const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
        if (tid == null_entity) continue;
        const auto it = w.tile_to_nation.find(tid);
        if (it == w.tile_to_nation.end()) continue;
        const auto ni = nidx.find(it->second);
        if (ni != nidx.end()) owner[static_cast<std::size_t>(idx)] = ni->second;
    }
    return owner;
}

economic_focus focus_of_class(region_class c)
{
    switch (c)
    {
        case region_class::energy: return economic_focus::processing;
        case region_class::port:   return economic_focus::trade;
        default:                     return economic_focus::extraction; // farm, ore, none
    }
}

} // namespace

void derive_national_character(settlement_state& ss,
                               const creed_state& cs,
                               world& w,
                               const std::vector<entity_id>& nation_ids,
                               const std::vector<entity_id>& tile_ids,
                               int gw, int gh)
{
    if (ss.regions.empty() || nation_ids.empty() || gw <= 0 || gh <= 0)
        return;

    const auto nidx = nation_index(nation_ids);
    const std::vector<int> owner = owner_map_of(w, tile_ids, nidx, gw, gh);
    const int nations = static_cast<int>(nation_ids.size());
    const int total = gw * gh;

    // --- Attribute every region to whoever ended up holding it ---------------
    for (region& p : ss.regions)
        p.nation = (p.anchor >= 0 && p.anchor < total)
                 ? owner[static_cast<std::size_t>(p.anchor)] : -1;

    // --- BL-866 — THE SEAT WINS ITS OWN HINTERLAND, EVEN HERE -----------------
    //
    // ROOT CAUSE of the small, persistent seat/hinterland nation mismatches
    // `colonisation_harness`'s case_settlement_seats (D1/D3) measures after a
    // full era -1 run: `generate_nations` grows the political map from tile
    // ANCHORS via its own Voronoi/BFS over the grid, which knows nothing about
    // `region::seat_region` — so a hinterland region's anchor tile can fall a
    // hair on the far side of a border its own seat's anchor did not, even
    // though `history_sim.cpp` kept the two in lockstep, region-index for
    // region-index, for the whole 2000+ year run. The blanket per-anchor
    // assignment above is exactly that geometric read, with no seat awareness
    // at all — unlike `resolve_historical_ruptures::carry_seat_on_transfer`
    // below, which already carries a seat's hinterland on every nation
    // transfer IT makes. This is the one write in the pass 1 -> pass 2 handoff
    // that had no equivalent.
    //
    // THE FIX: same rule, same grain (CIVILISATION.md § The unit is the city
    // state — "taking a seat takes what points at it"). Every region whose
    // `seat_region` names a seat inherits THAT SEAT's freshly-assigned nation,
    // overriding whatever the tile-level growth gave it on its own. A no-op
    // wherever the geometry already agreed (the overwhelming majority of
    // regions), and the one place a hinterland's own anchor tile disagreed
    // with its seat's.
    for (std::size_t si = 0; si < ss.regions.size(); ++si)
    {
        const region& seat = ss.regions[si];
        if (!seat.is_seat) continue;
        const int seat_nation = seat.nation;
        for (std::size_t hi = 0; hi < ss.regions.size(); ++hi)
        {
            if (hi == si) continue;
            if (ss.regions[hi].seat_region == static_cast<int>(si))
                ss.regions[hi].nation = seat_nation;
        }
    }

    // --- The border-contest integral -------------------------------------------
    // Two terms, because a frontier is two things: how much of your edge faces
    // somebody (the static share) and how much settled weight is pressed
    // against it (the historical pressure). A nation that expanded into empty
    // land scores near zero on both, which is BL-218's whole point.
    std::vector<int> border(static_cast<std::size_t>(nations), 0);
    std::vector<int> owned(static_cast<std::size_t>(nations), 0);
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner[static_cast<std::size_t>(idx)];
        if (ni < 0) continue;
        ++owned[static_cast<std::size_t>(ni)];

        const int col = idx % gw, row = idx / gw;
        const int dc[4] = { 0, 0, -1, 1 };
        const int dr[4] = { -1, 1, 0, 0 };
        for (int i = 0; i < 4; ++i)
        {
            const int nr = row + dr[i];
            if (nr < 0 || nr >= gh) continue;
            const int no = owner[static_cast<std::size_t>(raster(col + dc[i], nr, gw))];
            if (no >= 0 && no != ni) { ++border[static_cast<std::size_t>(ni)]; break; }
        }
    }

    std::vector<int> rival_pressure(static_cast<std::size_t>(nations), 0);
    std::vector<int> region_count(static_cast<std::size_t>(nations), 0);
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
    {
        const region& a = ss.regions[i];
        if (a.nation < 0) continue;
        ++region_count[static_cast<std::size_t>(a.nation)];
        for (std::size_t j = 0; j < ss.regions.size(); ++j)
        {
            if (i == j) continue;
            const region& b = ss.regions[j];
            if (b.nation < 0 || b.nation == a.nation) continue;
            if (grid_dist(a.col, a.row, b.col, b.row, gw) <= 12)
                ++rival_pressure[static_cast<std::size_t>(a.nation)];
        }
    }

    std::vector<int> contest(static_cast<std::size_t>(nations), 0);
    for (int ni = 0; ni < nations; ++ni)
    {
        const int share = owned[static_cast<std::size_t>(ni)] > 0
            ? (border[static_cast<std::size_t>(ni)] * 1000) / owned[static_cast<std::size_t>(ni)]
            : 0;
        const int press = region_count[static_cast<std::size_t>(ni)] > 0
            ? (rival_pressure[static_cast<std::size_t>(ni)] * 150)
              / region_count[static_cast<std::size_t>(ni)]
            : 0;
        contest[static_cast<std::size_t>(ni)] = clampi(share * 2 + press, 0, 1000);
    }
    for (region& p : ss.regions)
        if (p.nation >= 0) p.contest_q = contest[static_cast<std::size_t>(p.nation)];

    // --- Industrialisation timing, per nation ----------------------------------
    constexpr int64_t never = 1 << 30;
    std::vector<int64_t> first_furnace(static_cast<std::size_t>(nations), never);
    for (const region& p : ss.regions)
    {
        if (p.nation < 0 || !p.industrialised) continue;
        int64_t& f = first_furnace[static_cast<std::size_t>(p.nation)];
        f = std::min(f, p.industrial_year);
    }

    // Terciles over the nations that industrialised at all — "relative to
    // neighbours" is a RANK, so it has to be computed against the field.
    std::vector<int64_t> ranked;
    for (int ni = 0; ni < nations; ++ni)
        if (first_furnace[static_cast<std::size_t>(ni)] != never)
            ranked.push_back(first_furnace[static_cast<std::size_t>(ni)]);
    std::sort(ranked.begin(), ranked.end());
    const int64_t early_cut = ranked.empty() ? 0 : ranked[ranked.size() / 3];
    const int64_t late_cut  = ranked.empty() ? 0 : ranked[(ranked.size() * 2) / 3];

    // --- Write the four axes ----------------------------------------------------
    for (int ni = 0; ni < nations; ++ni)
    {
        const auto it = w.nations.find(nation_ids[static_cast<std::size_t>(ni)]);
        if (it == w.nations.end()) continue;
        nation_component& nc = it->second;

        // expansionism <- the border-contest integral.
        const int cq = contest[static_cast<std::size_t>(ni)];
        nc.posture = cq >= 600 ? expansionism::aggressive
                   : cq >= 280 ? expansionism::moderate
                               : expansionism::passive;

        // economic_focus <- the dominant resource class of the regions
        // settled DURING industrialisation, not of everything it holds.
        // `ind_regions` rides the same walk for the qualification axis below.
        std::array<int, 3> tally = { 0, 0, 0 };
        int ind_regions = 0;
        for (const region& p : ss.regions)
        {
            if (p.nation != ni) continue;
            if (!p.industrialised) continue;
            ++ind_regions;
            ++tally[static_cast<std::size_t>(focus_of_class(p.dominant))];
        }
        if (tally[0] + tally[1] + tally[2] == 0)
            for (const region& p : ss.regions)
                if (p.nation == ni) ++tally[static_cast<std::size_t>(focus_of_class(p.dominant))];

        int best = 0;
        for (int f = 1; f < 3; ++f)
            if (tally[static_cast<std::size_t>(f)] > tally[static_cast<std::size_t>(best)]) best = f;
        nc.focus = static_cast<economic_focus>(best);

        const int64_t ff = first_furnace[static_cast<std::size_t>(ni)];

        // The movement up the value chain (BL-219 § 3, applied first at the
        // national level): a first mover had time to build refining on top of
        // its extraction base.
        if (ff != never && !ranked.empty() && ff <= early_cut
         && nc.focus == economic_focus::extraction)
            nc.focus = economic_focus::processing;

        // ideology <- industrialisation timing against neighbours.
        if (ff == never)                nc.politics = ideology::isolationist;
        else if (ff <= early_cut)       nc.politics = ideology::mercantile;
        else if (ff <= late_cut)        nc.politics = ideology::technocratic;
        else                            nc.politics = ideology::authoritarian;

        // qualification <- the same timing record, as a FRACTION rather than a
        // rank (BL-613, qualification fraction; POPULATION.md § Qualification:
        // "early industrialisers open qualified, late ones raw"). Two terms,
        // both already computed above: WHEN the first furnace lit (the tercile
        // base — a first mover has had generations of schooling on top of its
        // industry) and HOW MUCH of the nation industrialised (the breadth
        // bonus — one furnace region does not school a whole realm). A nation
        // that never industrialised opens at a floor, not zero: some literate
        // administration exists anywhere a nation does. First-cut constants,
        // tune-not-restructure (the NR-600 idiom).
        {
            const float ind_share =
                region_count[static_cast<std::size_t>(ni)] > 0
                    ? static_cast<float>(ind_regions)
                      / static_cast<float>(region_count[static_cast<std::size_t>(ni)])
                    : 0.0f;
            float base = 0.05f;                    // never industrialised
            if (ff != never)
                base = ff <= early_cut ? 0.35f
                     : ff <= late_cut  ? 0.22f
                                       : 0.12f;
            const float q = base + 0.25f * ind_share;
            nc.qualification = q < 0.0f ? 0.0f : (q > 1.0f ? 1.0f : q);
        }
    }

    // --- Stage 4's lines, now that they can name a nation ----------------------
    // The earliest three furnaces only: this is the seed's own Britain, and
    // naming it is the whole payload HISTORY.md Stage 4 asks for.
    std::vector<int> by_year;
    for (std::size_t i = 0; i < ss.regions.size(); ++i)
        if (ss.regions[i].industrialised && ss.regions[i].nation >= 0)
            by_year.push_back(static_cast<int>(i));
    std::sort(by_year.begin(), by_year.end(), [&](int a, int b) {
        const region& pa = ss.regions[static_cast<std::size_t>(a)];
        const region& pb = ss.regions[static_cast<std::size_t>(b)];
        return pa.industrial_year != pb.industrial_year
             ? pa.industrial_year < pb.industrial_year : a < b;
    });

    const int lines = std::min<int>(3, static_cast<int>(by_year.size()));
    for (int i = 0; i < lines; ++i)
    {
        const region& p = ss.regions[static_cast<std::size_t>(by_year[static_cast<std::size_t>(i)])];
        const auto it = w.nations.find(nation_ids[static_cast<std::size_t>(p.nation)]);
        const std::string nation = it != w.nations.end() ? it->second.name : std::string("a realm");
        const bool forge = creed_holds(cs, p.culture.plurality(), "the forge");
        ss.history.push_back(history_event{
            years_from_calendar_year(p.industrial_year), chain_stage::spend,
            nation + " lights the first coke furnaces of the " + p.name + ".",
            forge ? "-> the forge god's country cashes its ore"
                  : "-> endowment, not virtue; the organic ceiling breaks here" });
    }
}

// ---------------------------------------------------------------------------
// The historical ruptures — transforms, and a record that can be destroyed
// ---------------------------------------------------------------------------

namespace {

/// Move one tile between nations, keeping `tile_to_nation` and both nations'
/// tile lists in step. Doing this in one place is the only reason the transforms
/// below can be read as history rather than as bookkeeping.
void transfer_tile(world& w, entity_id tid, entity_id from_nid, entity_id to_nid)
{
    if (tid == null_entity || from_nid == to_nid) return;
    w.tile_to_nation[tid] = to_nid;

    const auto f = w.nations.find(from_nid);
    if (f != w.nations.end())
    {
        auto& v = f->second.tiles;
        v.erase(std::remove(v.begin(), v.end(), tid), v.end());
    }
    const auto t = w.nations.find(to_nid);
    if (t != w.nations.end())
        t->second.tiles.push_back(tid);
}

void scale_abundance(world& w, entity_id nid, float factor)
{
    const auto it = w.nations.find(nid);
    if (it == w.nations.end()) return;
    for (std::size_t r = 0; r < resource_count; ++r)
        it->second.resource_abundance[r] *= factor;
}

ideology flip(ideology i)
{
    switch (i)
    {
        case ideology::mercantile:    return ideology::authoritarian;
        case ideology::authoritarian: return ideology::mercantile;
        case ideology::technocratic:  return ideology::isolationist;
        default:                      return ideology::technocratic;
    }
}

/// ERASE part of the record. Every line naming @p subject is destroyed and one
/// dated lacuna is left in its place — a hole the player can see, which is the
/// difference between a history that was fought over and a history that was
/// merely written.
void redact(settlement_state& ss, const std::string& subject,
            int64_t year, const std::string& by)
{
    const std::size_t before = ss.history.size();
    ss.history.erase(std::remove_if(ss.history.begin(), ss.history.end(),
        [&](const history_event& e) {
            return e.event.find(subject) != std::string::npos
                || e.consequence.find(subject) != std::string::npos;
        }), ss.history.end());

    const int lost = static_cast<int>(before - ss.history.size());
    if (lost <= 0) return;

    ss.lacunae += lost;
    ss.history.push_back(history_event{
        years_from_calendar_year(year), chain_stage::legacy,
        "What the " + subject + " wrote of itself does not survive " + by + ".",
        "-> " + std::to_string(lost) + " lines lost; the victors keep the record" });
}

} // namespace

void resolve_historical_ruptures(settlement_state& ss,
                                 const creed_state& cs,
                                 world& w,
                                 const std::vector<entity_id>& nation_ids,
                                 const std::vector<entity_id>& tile_ids,
                                 int gw, int gh,
                                 uint32_t seed)
{
    if (ss.regions.empty() || nation_ids.size() < 2 || gw <= 0 || gh <= 0)
        return;

    const auto nidx = nation_index(nation_ids);
    const std::vector<int> owner = owner_map_of(w, tile_ids, nidx, gw, gh);
    const int nations = static_cast<int>(nation_ids.size());
    const int total = gw * gh;

    // BL-866 — THE SEAT/HINTERLAND CARRY, at the NATION grain this pass works
    // at. The same principle `history_sim.cpp`'s per-round conquest applies:
    // taking a seat takes what points at it, in the same event. This pass
    // moves whole REGIONS between nations (collapse, war) rather than
    // fighting round by round, but the invariant is the same one
    // (CIVILISATION.md § The unit is the city state) and it is what
    // `colonisation_harness`'s D1/D3 case checks at the epoch.
    const auto carry_seat_on_transfer = [&](int region_index, int new_nation) {
        region& p = ss.regions[static_cast<std::size_t>(region_index)];
        if (p.is_seat)
        {
            for (std::size_t hi = 0; hi < ss.regions.size(); ++hi)
            {
                if (static_cast<int>(hi) == region_index) continue;
                region& h = ss.regions[hi];
                if (h.seat_region != region_index) continue;
                h.nation = new_nation;
            }
        }
        else
        {
            // Decoupled from its old seat: re-point at the new nation's own
            // seat (its lowest-indexed one), or fall outside anyone's reach
            // if it holds none yet.
            int seat = -1;
            for (std::size_t i = 0; i < ss.regions.size(); ++i)
                if (ss.regions[i].nation == new_nation && ss.regions[i].is_seat)
                    { seat = static_cast<int>(i); break; }
            p.seat_region = seat;
        }
    };

    // Shared-border counts, so "war" can pick a real neighbour rather than a
    // nation on the other side of the world.
    std::vector<std::vector<int>> shared(
        static_cast<std::size_t>(nations), std::vector<int>(static_cast<std::size_t>(nations), 0));
    for (int idx = 0; idx < total; ++idx)
    {
        const int ni = owner[static_cast<std::size_t>(idx)];
        if (ni < 0) continue;
        const int col = idx % gw, row = idx / gw;
        const int dc[4] = { 0, 0, -1, 1 };
        const int dr[4] = { -1, 1, 0, 0 };
        for (int i = 0; i < 4; ++i)
        {
            const int nr = row + dr[i];
            if (nr < 0 || nr >= gh) continue;
            const int no = owner[static_cast<std::size_t>(raster(col + dc[i], nr, gw))];
            if (no >= 0 && no != ni) ++shared[static_cast<std::size_t>(ni)][static_cast<std::size_t>(no)];
        }
    }

    // The nations a rupture is even about: the most-contested handful. Bounded
    // so the count of ruptures is a property of the design, not of the map size.
    std::vector<int> candidates;
    for (int ni = 0; ni < nations; ++ni) candidates.push_back(ni);
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
        int ca = 0, cb = 0;
        for (const region& p : ss.regions)
        {
            if (p.nation == a) ca = std::max(ca, p.contest_q);
            if (p.nation == b) cb = std::max(cb, p.contest_q);
        }
        return ca != cb ? ca > cb : a < b;
    });
    if (candidates.size() > 6) candidates.resize(6);

    rng pick_rng(seed, tag_rupture);

    for (std::size_t k = 0; k < candidates.size(); ++k)
    {
        const int ni = candidates[k];
        const entity_id nid = nation_ids[static_cast<std::size_t>(ni)];
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end() || nit->second.tiles.empty()) continue;

        // Region inventory for this nation, in placement order.
        std::vector<int> mine;
        for (std::size_t i = 0; i < ss.regions.size(); ++i)
            if (ss.regions[i].nation == ni) mine.push_back(static_cast<int>(i));
        if (mine.empty()) continue;

        // Strongest real neighbour, by shared border (tie: lowest index).
        int rival = -1, rival_border = 0;
        for (int nj = 0; nj < nations; ++nj)
        {
            const int b = shared[static_cast<std::size_t>(ni)][static_cast<std::size_t>(nj)];
            if (b > rival_border) { rival_border = b; rival = nj; }
        }

        const int contest = ss.regions[static_cast<std::size_t>(mine.front())].contest_q;
        const int64_t year = 1901 + static_cast<int64_t>(k) * 6 + pick_rng.pick(9);

        bool applied = false;
        resolve_checkpoint(
            chain_stage::spend, seed, tag_rupture ^ (static_cast<uint32_t>(ni) * 0x9E3779B1u),
            /*max_attempts=*/4,
            // propose — a lean is an ELIGIBILITY FILTER, never a weight.
            [&](checkpoint_rng&, uint32_t) {
                std::vector<checkpoint_branch> b;
                b.push_back({ "collapse", mine.size() >= 3 });
                b.push_back({ "war", rival >= 0 && contest >= 280 });
                b.push_back({ "revolution", true });
                return b;
            },
            // apply — each branch is a transform on state; the line it appends
            // is a RECORD of the transform, never a substitute for it.
            [&](checkpoint_rng&, const checkpoint_branch& chosen) {
                if (applied) return; // A reroll must not stack transforms.
                const std::string label = chosen.label;
                const std::string self = nit->second.name;

                if (label == "revolution")
                {
                    nit->second.politics = flip(nit->second.politics);
                    scale_abundance(w, nid, 0.92f);
                    ss.history.push_back(history_event{
                        years_from_calendar_year(year), chain_stage::spend,
                        "The " + ss.regions[static_cast<std::size_t>(mine.front())].name +
                            " rises; " + self + " is remade from the inside.",
                        "-> ideology flips; territory untouched" });
                    applied = true;
                    return;
                }

                if (label == "collapse")
                {
                    // Contract toward the core: the two most peripheral
                    // regions are lost, and their industrial clock resets.
                    std::vector<int> ordered = mine;
                    const region& core = ss.regions[static_cast<std::size_t>(mine.front())];
                    std::sort(ordered.begin(), ordered.end(), [&](int a, int b) {
                        const region& pa = ss.regions[static_cast<std::size_t>(a)];
                        const region& pb = ss.regions[static_cast<std::size_t>(b)];
                        const int da = grid_dist(pa.col, pa.row, core.col, core.row, gw);
                        const int db = grid_dist(pb.col, pb.row, core.col, core.row, gw);
                        return da != db ? da > db : a < b;
                    });

                    int lost = 0;
                    for (int pi : ordered)
                    {
                        if (lost >= 2) break;
                        region& p = ss.regions[static_cast<std::size_t>(pi)];
                        if (pi == mine.front()) continue;

                        // Hand the region's anchor window to the nearest
                        // OTHER nation that already borders it.
                        int taker = -1;
                        for (int nj = 0; nj < nations && taker < 0; ++nj)
                            if (nj != ni && shared[static_cast<std::size_t>(ni)][static_cast<std::size_t>(nj)] > 0)
                                taker = nj;
                        if (taker < 0) break;

                        const entity_id tnid = nation_ids[static_cast<std::size_t>(taker)];
                        int moved = 0;
                        for (int dr2 = -3; dr2 <= 3 && moved < 24; ++dr2)
                            for (int dc2 = -3; dc2 <= 3 && moved < 24; ++dc2)
                            {
                                const int rr = p.row + dr2;
                                if (rr < 0 || rr >= gh) continue;
                                const int idx = raster(p.col + dc2, rr, gw);
                                if (owner[static_cast<std::size_t>(idx)] != ni) continue;
                                const entity_id tid = tile_ids[static_cast<std::size_t>(idx)];
                                transfer_tile(w, tid, nid, tnid);
                                ++moved;
                            }
                        carry_seat_on_transfer(pi, taker); // BL-866
                        p.nation = taker;
                        p.industrial_year = p.industrialised
                            ? std::min<int64_t>(1935, p.industrial_year + 40) : 0;
                        ++lost;
                    }

                    scale_abundance(w, nid, 0.80f);
                    ss.history.push_back(history_event{
                        years_from_calendar_year(year), chain_stage::spend,
                        self + " cannot hold its periphery; " + std::to_string(lost) +
                            " regions answer to someone else by winter.",
                        "-> territory contracts; the industrial clock resets where it went" });
                    applied = true;
                    return;
                }

                // --- war -------------------------------------------------------
                const entity_id rnid = nation_ids[static_cast<std::size_t>(rival)];
                const auto rit = w.nations.find(rnid);
                if (rit == w.nations.end()) { applied = true; return; }

                // The stronger is the one with more to fight with: territory
                // plus how much of it is industrial.
                int my_industry = 0, their_industry = 0;
                for (const region& p : ss.regions)
                {
                    if (!p.industrialised) continue;
                    if (p.nation == ni) ++my_industry;
                    if (p.nation == rival) ++their_industry;
                }
                const int mine_w  = static_cast<int>(nit->second.tiles.size()) + my_industry * 40;
                const int their_w = static_cast<int>(rit->second.tiles.size()) + their_industry * 40;

                const int win_ni = mine_w >= their_w ? ni : rival;
                const int los_ni = mine_w >= their_w ? rival : ni;
                const entity_id win_nid = nation_ids[static_cast<std::size_t>(win_ni)];
                const entity_id los_nid = nation_ids[static_cast<std::size_t>(los_ni)];
                auto& winner = w.nations[win_nid];
                auto& loser  = w.nations[los_nid];

                const std::string win_name = winner.name;
                const std::string los_name = loser.name;

                // Redraw the contested border toward the stronger — bounded to
                // a quarter of the loser's territory, because BL-224's
                // non-hegemony invariant is respected here, not spent.
                const int budget = std::max(1, static_cast<int>(loser.tiles.size()) / 4);
                int moved = 0;
                for (int idx = 0; idx < total && moved < budget; ++idx)
                {
                    if (owner[static_cast<std::size_t>(idx)] != los_ni) continue;
                    const int col = idx % gw, row = idx / gw;
                    bool front = false;
                    const int dc[4] = { 0, 0, -1, 1 };
                    const int dr[4] = { -1, 1, 0, 0 };
                    for (int i = 0; i < 4 && !front; ++i)
                    {
                        const int nr = row + dr[i];
                        if (nr < 0 || nr >= gh) continue;
                        if (owner[static_cast<std::size_t>(raster(col + dc[i], nr, gw))] == win_ni)
                            front = true;
                    }
                    if (!front) continue;
                    transfer_tile(w, tile_ids[static_cast<std::size_t>(idx)], los_nid, win_nid);
                    ++moved;
                }

                // Grievance is the point of the axis: the loser comes out of it
                // more expansionist, not less.
                loser.posture = expansionism::aggressive;
                if (winner.posture == expansionism::passive)
                    winner.posture = expansionism::moderate;
                scale_abundance(w, los_nid, 0.75f);
                scale_abundance(w, win_nid, 0.90f);

                // THE VICTOR'S GODS TRAVEL WITH THE BORDER. A region taken
                // keeps its founders in `founding_culture` and its conquerors
                // in `culture` — which is exactly the pair a later religion or
                // population layer needs to describe a grievance.
                int converted = 0;
                int victor_culture = -1;
                for (const region& p : ss.regions)
                    if (p.nation == win_ni) { victor_culture = p.culture.plurality(); break; }

                for (std::size_t pi2 = 0; pi2 < ss.regions.size(); ++pi2)
                {
                    region& p = ss.regions[pi2];
                    if (p.nation != los_ni) continue;
                    if (p.anchor < 0) continue;
                    bool near_front = false;
                    for (const region& q : ss.regions)
                        if (q.nation == win_ni
                         && grid_dist(p.col, p.row, q.col, q.row, gw) <= 10) { near_front = true; break; }
                    if (!near_front) continue;

                    carry_seat_on_transfer(static_cast<int>(pi2), win_ni); // BL-866
                    p.nation = win_ni;
                    // BL-826 — TOTAL here, and deliberately so. This is the
                    // settlement pass's own prehistoric conflict, resolved
                    // millennia before the year-tick sim starts; there is no
                    // clock in this pass for a gradual shift to run on, and the
                    // gradual model belongs to the sim that has one. Written as
                    // `pure` rather than left implicit so the choice is visible.
                    if (victor_culture >= 0 && victor_culture != p.culture.plurality())
                    {
                        p.culture = culture_shares::pure(victor_culture);
                        p.creed_conquered = true;
                        ++converted;
                    }
                }

                ss.history.push_back(history_event{
                    years_from_calendar_year(year), chain_stage::spend,
                    win_name + " and " + los_name + " fight over the same frontier; " +
                        win_name + " keeps it.",
                    "-> " + std::to_string(moved) + " tiles change hands; " +
                        std::to_string(converted) + " regions change gods" });

                // And now the erasure. The loser's own account of itself is
                // what a sack destroys first.
                if (!mine.empty())
                {
                    for (const region& p : ss.regions)
                    {
                        if (p.nation != win_ni || !p.creed_conquered) continue;
                        redact(ss, p.name, year, "the " + win_name + " occupation");
                        break;
                    }
                }

                // The victor's pantheon is now the frontier's pantheon — name
                // it, since the gods are the only part of the old tongues that
                // survived globalisation (CREEDS.md).
                if (converted > 0 && victor_culture >= 0
                 && victor_culture < static_cast<int>(cs.cultures.size())
                 && !cs.cultures[static_cast<std::size_t>(victor_culture)].pantheon.empty())
                {
                    ss.history.push_back(history_event{
                        years_from_calendar_year(year + 1), chain_stage::spend,
                        "The shrines of the taken regions are rededicated to " +
                            cs.cultures[static_cast<std::size_t>(victor_culture)].pantheon[0].name + ".",
                        "-> the founders' names survive only in the land registry" });
                }
                applied = true;
            },
            // floor_ok — the viability predicate, evaluated after apply. A
            // rupture that leaves a nation with nothing is not a rupture, it is
            // a deletion, and the transforms above are bounded so this holds.
            [&]() {
                const auto it2 = w.nations.find(nid);
                return it2 != w.nations.end() && !it2->second.tiles.empty();
            },
            ss.checkpoints);
    }

    // Chronological, so the biography reads forward regardless of which
    // transform appended what.
    std::stable_sort(ss.history.begin(), ss.history.end(),
        [](const history_event& a, const history_event& b)
        { return a.years_before_epoch > b.years_before_epoch; });
}

// ---------------------------------------------------------------------------
// BL-219's read — a corporation operates at the tier its region earned
// ---------------------------------------------------------------------------

industrial_focus focus_from_region(const region& p, int64_t median)
{
    // The ancient endowment sets the base tier.
    industrial_focus f = industrial_focus::extraction;
    switch (p.dominant)
    {
        case region_class::energy: f = industrial_focus::processing; break;
        case region_class::port:   f = industrial_focus::trade;      break;
        default:                     f = industrial_focus::extraction; break;
    }

    // MOVEMENT UP THE CHAIN (BL-219 § 3): an early industrialiser had time to
    // build refining on top of its extraction base and trade on top of that. A
    // late one — or one that never lit a furnace — stayed raw.
    if (p.industrialised && median > 0 && p.industrial_year <= median)
    {
        if (f == industrial_focus::extraction) f = industrial_focus::processing;
        else if (f == industrial_focus::processing) f = industrial_focus::trade;
    }
    return f;
}

// ---------------------------------------------------------------------------
// Demography (BL-273) — population growth, war/plague drawdown, manpower.
// ---------------------------------------------------------------------------

namespace {

// Fixed-point rate table, all in thousandths (1000 = 1.0). Named constants
// rather than inline magic numbers so the tuning surface is one place.
constexpr int64_t demog_capacity_floor    = 5000;  // Headcount at farm_q = 0.
constexpr int64_t demog_capacity_per_q    = 500;   // Extra headcount per farm_q point.
constexpr int64_t demog_growth_rate_q     = 12;     // ~1.2%/yr logistic rate, low-density.
constexpr int64_t demog_war_drawdown_q    = 40;     // Up to ~4%/yr loss at war_pressure_q = 1000.
constexpr int64_t demog_manpower_frac_q   = 50;     // Ceiling: 5% of population under arms at once.
constexpr int64_t demog_manpower_recover_q = 250;   // Stock closes 25% of the ceiling gap per step.
constexpr int64_t demog_plague_core_q     = 250;    // Epicentre loses up to 25% of its population.
constexpr int64_t demog_plague_falloff_q  = 60;     // Per grid step of distance, the loss tapers.
constexpr int      demog_plague_radius    = 6;      // Neighbours beyond this feel nothing.

// --- The urban record (BL-766) ---------------------------------------------
// Same fixed-point discipline: thousandths, no floats, no RNG anywhere on this
// path. Every one of these is a first cut sized off the demography constants
// above rather than a measured figure, and they are grouped here so the tuning
// surface stays one place.

/// The population the sim seeds an empty region with, as a divisor of its
/// carrying capacity. Mirrors what `run_history_sim` has always used; named
/// here because the urban draw has to size itself against the same figure.
constexpr int64_t urban_seed_pop_divisor = 8;

/// Below this `farm_q` the ground feeds its own farmers and nobody else, so no
/// town is drawn on it at all. THE ITEM'S WHOLE WEIGHTING, in one gate: the
/// opening map is a map of where farming is easy.
constexpr int urban_seed_farm_floor_q = 300;

/// Urban share of a region's people: a floor plus a slope on `farm_q`, so a
/// hard-farming region towns 4% of itself and the best ground towns 20%.
constexpr int urban_share_floor_q = 40;
constexpr int urban_share_farm_q  = 160;

/// Per-mille of the gap to the urban target closed each simulated year. Cities
/// grow and shrink; they never jump.
constexpr int64_t urban_converge_q = 40;

/// A sack costs the city this multiple of what it costs the countryside
/// (per mille, so 2000 = twice). Walls are what an army sacks; fields survive
/// it. This is the term that lets history DESTROY a centre rather than only
/// thin it.
constexpr int64_t urban_sack_multiple_q = 2000;

} // namespace

int64_t region_carrying_capacity(int farm_q)
{
    const int q = clampi(farm_q, 0, 1000);
    return demog_capacity_floor + static_cast<int64_t>(q) * demog_capacity_per_q;
}

int64_t region_carrying_capacity(int farm_q, int capacity_mod_q)
{
    const int64_t base = region_carrying_capacity(farm_q);
    // Clamped at -999 so the multiplier stays positive: a modifier of -1000
    // would zero the ceiling and make the logistic term divide by zero.
    const int m = clampi(capacity_mod_q, -999, 100000);
    return base + (base * m) / 1000;
}

int64_t manpower_ceiling(int64_t population)
{
    if (population <= 0) return 0;
    return (population * demog_manpower_frac_q) / 1000;
}

int64_t manpower_ceiling(int64_t population, int manpower_mod_q)
{
    const int64_t base = manpower_ceiling(population);
    const int m = clampi(manpower_mod_q, -1000, 100000);
    return base + (base * m) / 1000;
}

void replenish_manpower(region& p)
{
    // Reads the region's own works (BL-321) rather than taking the modifier
    // as an argument: an Arsenal is a property of the place, and every existing
    // caller should get its effect without being told the works exist.
    const int64_t ceiling = manpower_ceiling(p.population, p.work_manpower_mod);
    if (p.manpower_stock > ceiling) { p.manpower_stock = ceiling; return; } // Losses shrink the ceiling too.
    const int64_t gap = ceiling - p.manpower_stock;
    p.manpower_stock += (gap * demog_manpower_recover_q) / 1000;
    p.manpower_stock = clampi64(p.manpower_stock, 0, ceiling);
}

int64_t raise_manpower(region& p, int64_t want)
{
    if (want <= 0 || p.manpower_stock <= 0) return 0;
    const int64_t raised = std::min(want, p.manpower_stock);
    p.manpower_stock -= raised; // Bounded by construction: never negative, never over-drawn.
    return raised;
}

// ---------------------------------------------------------------------------
// The army pool (BL-835) — armies are distinct from population
// ---------------------------------------------------------------------------

int64_t garrison_target(const region& p, int garrison_fraction_q)
{
    const int64_t ceiling = manpower_ceiling(p.population, p.work_manpower_mod);
    if (ceiling <= 0) return 0;
    // Clamped at 1000: a garrison larger than the recruitable ceiling would be
    // an army with no pool to have come out of, and the muster below would
    // then chase a target it can never reach for the rest of the run.
    const int q = clampi(garrison_fraction_q, 0, 1000);
    return (ceiling * q) / 1000;
}

void muster_garrison(region& p, int garrison_fraction_q,
                     int muster_rate_q, int disband_rate_q)
{
    const int64_t target = garrison_target(p, garrison_fraction_q);

    if (p.army_stock < target)
    {
        const int64_t gap  = target - p.army_stock;
        const int64_t want = (gap * clampi(muster_rate_q, 0, 1000)) / 1000;
        // THE COST, and the only place it is charged: bodies come out of the
        // recruitable pool. `raise_manpower` is self-limiting, so a region
        // whose people are already under arms simply musters nothing this year
        // rather than conjuring soldiers off a population it cannot spare.
        p.army_stock += raise_manpower(p, want);
        return;
    }

    if (p.army_stock > target)
    {
        const int64_t excess = p.army_stock - target;
        const int64_t home   = (excess * clampi(disband_rate_q, 0, 1000)) / 1000;
        p.army_stock -= home;
        // Discharged, back to the pool they were raised from — NOT to
        // `population`, which never lost them. Capped at the ceiling by the
        // same rule `replenish_manpower` uses, so a demobilisation cannot bank
        // more manpower than the living population could ever field.
        const int64_t ceiling = manpower_ceiling(p.population, p.work_manpower_mod);
        p.manpower_stock = clampi64(p.manpower_stock + home, 0, ceiling);
    }
}

int64_t spend_army(region& p, int64_t lost)
{
    if (lost <= 0 || p.army_stock <= 0) return 0;
    const int64_t spent = std::min(lost, p.army_stock);
    p.army_stock -= spent;
    return spent;
}

// ---------------------------------------------------------------------------
// Materials and labour (BL-867)
// ---------------------------------------------------------------------------

int64_t region_industry_capacity(const region& p)
{
    const int64_t ceiling = manpower_ceiling(p.population, p.work_manpower_mod);
    return clampi64(ceiling - p.army_stock, 0, ceiling);
}

int region_industry_yield_q(const region& p)
{
    return clampi(p.ore_q, 0, 1000);
}

int64_t region_industry_output(const region& p)
{
    const int64_t heads = region_industry_capacity(p);
    return (heads * region_industry_yield_q(p)) / 1000;
}

// ---------------------------------------------------------------------------
// The urban record (BL-766) — the population map, drawn early and then lived in
// ---------------------------------------------------------------------------

int64_t region_seed_population(int farm_q)
{
    const int64_t k = region_carrying_capacity(farm_q) / urban_seed_pop_divisor;
    return clampi64(k, 1, 1 << 30);
}

int region_urban_share_q(int farm_q)
{
    const int q = clampi(farm_q, 0, 1000);
    return urban_share_floor_q + (q * urban_share_farm_q) / 1000;
}

namespace {

/// Promote `centres` to whatever `urban_population` now stands up, never
/// demote. POPULATION.md's asymmetry: passive failure shrinks a centre and
/// never destroys one, so only `sack_region_urban` takes a centre off the map.
void promote_centres(region& p)
{
    const int stood = static_cast<int>(
        clampi64(p.urban_population / region_centre_heads, 0, region_centre_limit));
    if (stood > p.centres)
        p.centres = stood;
}

} // namespace

void draw_region_urban(region& p)
{
    // The ground that feeds a town. Below the floor the map stays empty here —
    // and it should: a region with no city is what makes a region WITH one mean
    // something.
    if (clampi(p.farm_q, 0, 1000) < urban_seed_farm_floor_q)
    {
        p.urban_population = 0;
        p.centres          = 0;
        return;
    }

    // Size against the population the region HAS, or — the case at the opening
    // draw, before the sim has seeded anything — against the one it is about to
    // be given.
    const int64_t heads = (p.population > 0) ? p.population
                                             : region_seed_population(p.farm_q);
    p.urban_population = clampi64(
        (heads * region_urban_share_q(p.farm_q)) / 1000, 0, 1LL << 40);

    // At least one settlement stands where the ground invited one, even where
    // the opening headcount is below a full rung. Two reasons, and the second
    // is the one that took a measurement to find:
    //
    //  1. The map has to be DRAWN before the sim, not merely made drawable, or
    //     the sim runs over an empty world again and nothing has changed.
    //  2. Applied at EVERY founding, it is what makes a frontier region
    //     razeable. Without it a region founded by the Settle verb held no
    //     settlement until its townsfolk crossed a full rung, and since most
    //     ground a war changes hands over is exactly that young frontier, a
    //     sack had nothing to destroy: 207 conquests over a 400-year era razed
    //     zero centres. The rule is now ONE rule — ground that farms gets a
    //     settlement, whenever it is settled — rather than one rule for the
    //     opening map and another for everything history founded after it.
    p.centres = 1;
    promote_centres(p);
}

void draw_urban_map(settlement_state& s)
{
    for (region& p : s.regions)
        draw_region_urban(p);
    s.urban_map_drawn = true;
}

void advance_region_urban(region& p)
{
    if (p.population <= 0)
    {
        // A region history emptied keeps no city. `centres_razed` is not
        // touched here: nobody sacked these walls, the people simply went.
        p.urban_population = 0;
        p.centres          = 0;
        return;
    }

    const int64_t target =
        clampi64((p.population * region_urban_share_q(p.farm_q)) / 1000, 0, 1LL << 40);
    const int64_t gap = target - p.urban_population;
    // Integer division truncates toward zero in both directions, so the step is
    // symmetric and a gap smaller than 1000/urban_converge_q simply stalls —
    // which is the correct behaviour for a town already at its ground's size.
    p.urban_population = clampi64(p.urban_population + (gap * urban_converge_q) / 1000,
                                  0, 1LL << 40);
    promote_centres(p);
}

void sack_region_urban(region& p, int population_loss_q)
{
    if (p.centres <= 0 && p.urban_population <= 0)
        return;

    const int64_t loss_q =
        clampi64((static_cast<int64_t>(clampi(population_loss_q, 0, 1000))
                  * urban_sack_multiple_q) / 1000, 0, 1000);
    p.urban_population = clampi64(
        p.urban_population - (p.urban_population * loss_q) / 1000, 0, 1LL << 40);

    // What the survivors can still stand up. The difference is destruction, and
    // it is RECORDED — a razed city that is later rebuilt still says it was
    // razed, which is the only way the epoch map can read as historied.
    const int stands = static_cast<int>(
        clampi64(p.urban_population / region_centre_heads, 0, region_centre_limit));
    if (stands < p.centres)
    {
        p.centres_razed += p.centres - stands;
        p.centres = stands;
    }
}

void advance_region_demography(region& p, int years, int war_pressure_q)
{
    if (years <= 0) return;
    const int wq = clampi(war_pressure_q, 0, 1000);
    // The ceiling the region's WORKS raised (BL-321). A Granary is a change
    // to how many people the ground feeds, so it belongs in K rather than in a
    // one-off population grant: it lifts the asymptote the logistic term is
    // growing toward, which is a permanent change in trajectory rather than a
    // step that growth would simply re-flatten.
    const int64_t K = region_carrying_capacity(p.farm_q, p.work_capacity_mod);

    for (int y = 0; y < years; ++y)
    {
        if (p.population <= 0) { p.population = 0; continue; } // Nothing to grow from.

        const int64_t P = p.population;
        // Logistic growth term: dP = r * P * (K - P) / K, all integer.
        const int64_t delta = (demog_growth_rate_q * P * (K - P)) / (K * 1000);
        int64_t next = P + delta;

        if (wq > 0)
        {
            const int64_t loss = (next * static_cast<int64_t>(wq) * demog_war_drawdown_q) / (1000 * 1000);
            next -= loss;
        }

        p.population = clampi64(next, 0, K);
    }

    p.last_demography_year += years;
    replenish_manpower(p);
}

bool resolve_plague_event(std::vector<region>& regions,
                          std::vector<checkpoint_record>& checkpoints,
                          uint32_t seed, uint32_t stage_tag, int gw)
{
    if (regions.empty()) return false;

    bool applied = false;
    return resolve_checkpoint(
        chain_stage::spend, seed, stage_tag,
        /*max_attempts=*/4,
        // propose — one branch per region; eligibility is a FILTER
        // (BL-217's rule), never a weight: a region with nobody left
        // cannot be an epicentre.
        [&](checkpoint_rng&, uint32_t) {
            // Each label is the address of that region's OWN name buffer
            // (a std::string's storage is per-object even when empty via
            // SSO), so identity comparison in apply() below is exact and
            // survives duplicate name text.
            std::vector<checkpoint_branch> b;
            b.reserve(regions.size());
            for (const region& p : regions)
                b.push_back({ p.name.c_str(), p.population > 0 });
            return b;
        },
        // apply — the chosen region is the epicentre; nearby regions
        // (grid-proximity connectivity proxy, see the header comment on why
        // not the full trade graph) lose population too, tapering with
        // distance.
        [&](checkpoint_rng&, const checkpoint_branch& chosen) {
            if (applied) return; // A reroll must not stack losses.

            // Identity, not content, comparison: `chosen.label` is the address
            // of a specific region's own `name.c_str()` (set in propose,
            // same call), so a pointer match is exact even when two
            // regions share a name string (quarter-word collisions do
            // happen — see settle-name generation in run_settlement).
            int epicentre = -1;
            for (std::size_t i = 0; i < regions.size(); ++i)
                if (regions[i].name.c_str() == chosen.label) { epicentre = static_cast<int>(i); break; }
            if (epicentre < 0) return;

            const region& origin = regions[static_cast<std::size_t>(epicentre)];
            for (region& p : regions)
            {
                if (p.population <= 0) continue;
                const int dist = grid_dist(p.col, p.row, origin.col, origin.row, gw);
                if (dist > demog_plague_radius) continue;

                const int64_t falloff = static_cast<int64_t>(dist) * demog_plague_falloff_q;
                const int64_t loss_q = clampi64(demog_plague_core_q - falloff, 0, 1000);
                if (loss_q <= 0) continue;

                const int64_t loss = (p.population * loss_q) / 1000;
                p.population = clampi64(p.population - loss, 0, p.population);
                replenish_manpower(p); // The ceiling just fell with the population.
            }
            applied = true;
        },
        // floor_ok — a plague never has to leave anyone alive to be viable;
        // it is not a nation-level checkpoint, so there is no floor beyond
        // the bounded, non-negative arithmetic above already guaranteeing.
        [&]() { return true; },
        checkpoints);
}
