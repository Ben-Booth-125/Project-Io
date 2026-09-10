#include "creeds.hpp"

#include "colonisation.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace {

// ---------------------------------------------------------------------------
// Deterministic RNG — same splitmix64 shape as planetology.cpp /
// history_ladder.cpp, duplicated per the convention that each generation file
// owns its stream.
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

    float unit()
    {
        s = splitmix64(s);
        return static_cast<float>((s >> 40) & 0xFFFFFFull) * (1.0f / 16777216.0f);
    }

    /// Integer in [0, n). Integer path for anything that participates in
    /// selection, per the gate-path float ban.
    int pick(int n)
    {
        s = splitmix64(s);
        return n <= 0 ? 0 : static_cast<int>((s >> 33) % static_cast<uint64_t>(n));
    }
};

// FRESH stage tags — none collides with the ladder's (0x5A11 / 0xC4A7 /
// 0xF2A6), the continents' (0xC017) or the planetology chain's.
constexpr uint32_t tag_pantheon = 0xD317u; // Pantheon + tongue generation.

int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---------------------------------------------------------------------------
// Tongue — a tiny per-culture phonology, now owned by world/tongue.hpp
// (BL-290) so the passes downstream can CONSUME it rather than inventing a
// second sound system. Each culture draws a small consonant and vowel
// inventory; every proper noun it ever coins (its own name, its gods, and —
// once it has settled ground — its nations and cities) is built from that
// inventory, so two cultures' names SOUND different because their inventories
// differ, not because a style flag says so.
//
// The roll itself is unchanged: `roll_tongue`/`tongue_word` consume this
// stream in exactly the order the local helpers here used to.
// ---------------------------------------------------------------------------

/// A name unused so far in @p taken. Bounded retry, deterministic: the retry
/// walk consumes the same stream every run.
std::string fresh_name(rng& r, const tongue& p, int syllables,
                       std::vector<std::string>& taken)
{
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        std::string n = tongue_word(r, p, syllables);
        if (std::find(taken.begin(), taken.end(), n) == taken.end())
        {
            taken.push_back(n);
            return n;
        }
    }
    // Eight collisions means a tiny inventory; suffix the count, still unique.
    std::string n = tongue_word(r, p, syllables) + "-" + std::to_string(taken.size());
    taken.push_back(n);
    return n;
}

const tile_component* tile_at(const world& w, const std::vector<entity_id>& ids, std::size_t idx)
{
    if (idx >= ids.size()) return nullptr;
    const auto it = w.tiles.find(ids[idx]);
    return it == w.tiles.end() ? nullptr : &it->second;
}

/// What the land around a cradle offers its creed. Same neighbourhood window
/// the ladder's Pass 2 claimed for the cradle.
struct cradle_land
{
    int wetland = 0, forest = 0, barrier = 0, land = 0;
    bool ore = false;
};

cradle_land survey_land(const world& w, const std::vector<entity_id>& tile_ids,
                        const agrarian_cradle& c, int gw, int gh)
{
    cradle_land out;
    const int separation = std::max(6, gw / 12);
    for (int dr = -separation; dr <= separation; ++dr)
    {
        const int r = c.row + dr;
        if (r < 0 || r >= gh) continue;
        for (int dc = -separation; dc <= separation; ++dc)
        {
            const int col = ((c.col + dc) % gw + gw) % gw;
            const tile_component* t = tile_at(w, tile_ids, static_cast<std::size_t>(col + r * gw));
            if (!t) continue;
            if (!is_water(t->substrate)) ++out.land; // BL-516: land is not-water, every kind
            // Both are COVER questions (BL-519) — what grows here, not what the
            // ground is made of. The old names were compositions only because
            // cover had nowhere else to live.
            if (t->cover == terrain_cover::marsh)  ++out.wetland;
            if (t->cover == terrain_cover::forest) ++out.forest;
            if (t->landform == terrain_landform::mountain ||
                t->landform == terrain_landform::canyon) ++out.barrier;
            if (t->resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)] > 0.0f)
                out.ore = true;
        }
    }
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// The pantheons — one per cradle, each in its own tongue.
// ---------------------------------------------------------------------------

creed_state run_creeds(const planetology_state& pl,
                       const history_ladder_state& hl,
                       const world& w,
                       const std::vector<entity_id>& tile_ids,
                       int gw, int gh, uint32_t seed)
{
    creed_state out;
    if (hl.cradles.empty() || gw <= 0 || gh <= 0)
        return out;

    const int arable_q = clampi(static_cast<int>(pl.arable_share * 1000.0f), 0, 1000);
    rng r(seed, tag_pantheon);

    for (std::size_t ci = 0; ci < hl.cradles.size(); ++ci)
    {
        const agrarian_cradle& c = hl.cradles[ci];
        const cradle_land land = survey_land(w, tile_ids, c, gw, gh);

        culture cu;
        cu.cradle      = static_cast<int>(ci);
        // The cradle's year is the migration span's start (BL-873) — a real,
        // meaningful date, never the never-coined sentinel.
        cu.coined_year = colonisation_start_year;

        const tongue p = roll_tongue(r);
        cu.speech = p; // Retained: the nations and cities on this culture's
                       // ground are named from the same inventory (BL-290).
        std::vector<std::string> taken;
        cu.name = fresh_name(r, p, 2 + r.pick(2), taken);

        // Harsh ground breeds a harsher creed: the barrier share of the
        // cradle's own window raises every god's zeal floor. Integer percent.
        const int harsh = clampi((land.barrier * 100) / std::max(1, land.land), 0, 100);

        auto add_god = [&](const char* domain, const char* epithet,
                           int zeal_lo, int zeal_hi, int dom_lo, int dom_hi) {
            culture_god g;
            g.name     = fresh_name(r, p, 2 + r.pick(2), taken);
            g.domain   = domain;
            g.epithet  = epithet;
            g.zeal     = clampi(zeal_lo + r.pick(zeal_hi - zeal_lo + 1) + harsh / 34, 0, 10);
            g.dominion = clampi(dom_lo + r.pick(dom_hi - dom_lo + 1), 0, 10);
            cu.pantheon.push_back(std::move(g));
        };

        // The chief god is the land's own portrait — the archetype table's
        // terrain column. Coast beats floodplain beats forest beats open sky,
        // because that is the order in which each dominates a people's days.
        if (c.coastal)
            add_god("the sea", "who holds the harbours", 2, 6, 5, 9);
        else if (land.wetland * 2 >= std::max(1, land.land) / 4)
            add_god("the river", "who fattens the floodplain", 1, 4, 4, 8);
        else if (land.forest * 2 >= std::max(1, land.land) / 3)
            add_god("the green dark", "who keeps the old paths", 2, 5, 3, 7);
        else
            add_god("the storm", "who splits the sky", 4, 8, 5, 9);

        // Every creed raises a war god and a door of the dead — no people
        // skipped either question. The war god carries the culture's teeth.
        add_god("war", "who counts the spears", 6, 10, 3, 9);
        add_god("the door of the dead", "who takes no bribe", 0, 3, 8, 10);

        // Conditional seats: an ore region raises a forge; the charter
        // cradle raises an oath god — the seed Stage 1's Charter Act grows
        // from, planted here so the enforceable promise has a creed behind it.
        if (land.ore)
            add_god("the forge", "who buys sweat with iron", 3, 6, 4, 8);
        if (hl.charter_cradle == static_cast<int>(ci))
            add_god("the sealed oath", "who witnesses every bargain", 0, 2, 6, 10);

        const culture_god& chief = cu.pantheon[0];
        const culture_god& war_g = cu.pantheon[1];
        cu.aggression_q = clampi(war_g.zeal * 70 + chief.zeal * 20 + war_g.dominion * 10, 0, 1000);

        // The shrine line. Dated AFTER the granary line by construction: the
        // granary year is -(3000 + 6*arable + jitter), this is
        // -(2500 + 5*arable + jitter<=400), and 500 + arable always clears it.
        const int jitter = static_cast<int>(r.unit() * 400.0f);
        const int64_t year = -(2500 + static_cast<int64_t>(arable_q) * 5 + jitter);

        out.history.push_back(history_event{
            years_from_calendar_year(year), chain_stage::legacy,
            "The " + cu.name + " raise the first shrine of " +
                chief.name + ", " + chief.domain + " - " + chief.epithet + ".",
            "-> one pantheon, one tongue; " + std::to_string(cu.pantheon.size()) +
                " gods, and the war-bands' zeal is " + std::to_string(war_g.zeal) + "/10" });

        out.cultures.push_back(std::move(cu));
    }

    return out;
}

// ---------------------------------------------------------------------------
// Fragmentation from contact (BL-852, retiring the tribal marches NR-808
// resolved) — the creeds reach the political map without a war.
// ---------------------------------------------------------------------------

void record_cultural_contact(history_ladder_state& hl,
                             const std::vector<int>& region_mix_q)
{
    if (region_mix_q.empty())
        return;

    // THE NON-HEGEMONY FLOOR (BL-224), re-derived over the STRUCTURAL reading
    // Stage 3 computed from terrain and cradle count alone, before any
    // culture existed to meet another — the same base value and the same
    // half-floor the retired welding enforced, so creeds alone still cannot
    // manufacture a hegemon by themselves.
    const int floor_q = hl.fragmentation_q / 2;

    // Average interpenetration across every settled region: how far a second
    // people has mixed into ground a plurality culture still holds. No roll,
    // no pairwise comparison, no war — a pure read of the settled shares.
    int64_t total = 0;
    for (const int m : region_mix_q) total += clampi(m, 0, 1000);
    const int avg_mix_q = static_cast<int>(total / static_cast<int64_t>(region_mix_q.size()));

    hl.fragmentation_q = std::max(floor_q, hl.fragmentation_q - avg_mix_q);
}

// ---------------------------------------------------------------------------
// Globalisation — the common tongue closes generation.
// ---------------------------------------------------------------------------

void record_globalisation(creed_state& cs, const world& w, entity_id body_id)
{
    if (cs.cultures.empty())
        return;

    // The same sorted-key realm count record_institutional_history performs —
    // sorted, so the count cannot depend on unordered_map iteration order.
    std::vector<entity_id> nation_ids;
    nation_ids.reserve(w.nations.size());
    for (const auto& kv : w.nations) nation_ids.push_back(kv.first);
    std::sort(nation_ids.begin(), nation_ids.end());

    int realms = 0;
    for (const entity_id nid : nation_ids)
    {
        const auto nit = w.nations.find(nid);
        if (nit == w.nations.end() || nit->second.tiles.empty()) continue;
        const auto tit = w.tiles.find(nit->second.tiles.front());
        if (tit != w.tiles.end() && tit->second.body == body_id) ++realms;
    }

    // Fixed late date, deliberately: globalisation is the END of the
    // generated story, the hinge to the campaign epoch, not a rolled outcome.
    cs.history.push_back(history_event{
        years_from_calendar_year(1951), chain_stage::legacy,
        "The common tongue spreads through every port and press.",
        "-> " + std::to_string(std::max(realms, 1)) +
            " realms, one trade language; the old tongues survive in the names of gods" });
}

// ---------------------------------------------------------------------------
// Culture relations (BL-870; CIVILISATION.md § Culture relations)
// ---------------------------------------------------------------------------

int64_t culture_kinship_years(const std::vector<culture>& cultures, int a, int b)
{
    const int n = static_cast<int>(cultures.size());
    if (a < 0 || b < 0 || a >= n || b >= n) return -1;
    if (a == b) return 0;

    // Collect a's chain to the root, itself included. Bounded by the culture
    // count so a corrupt tree fails this read rather than hanging it — the
    // same guard `case_family_tree` checks the walk actually needs.
    std::vector<int> chain_a;
    chain_a.reserve(16);
    for (int at = a, guard = 0; at >= 0 && guard <= n; ++guard)
    {
        chain_a.push_back(at);
        at = cultures[static_cast<std::size_t>(at)].parent;
    }

    // Walk b's chain until it lands on one of a's ancestors — the first hit
    // IS the most recent common ancestor, because both walks strictly
    // decrease toward the root and neither can loop (BL-865).
    for (int at = b, guard = 0; at >= 0 && guard <= n; ++guard)
    {
        if (std::find(chain_a.begin(), chain_a.end(), at) != chain_a.end())
        {
            const culture& anc = cultures[static_cast<std::size_t>(at)];
            const culture& ca  = cultures[static_cast<std::size_t>(a)];
            const culture& cb  = cultures[static_cast<std::size_t>(b)];
            if (anc.coined_year < 0 || ca.coined_year < 0 || cb.coined_year < 0)
                return -1; // Ancestry known, dates are not — unmeasurable.
            return std::max(ca.coined_year, cb.coined_year) - anc.coined_year;
        }
        at = cultures[static_cast<std::size_t>(at)].parent;
    }
    return -1; // No shared ancestor found within a rooted tree — treat as unrelated.
}

namespace {
/// Years apart at which kinship stops discounting opposition at all. A
/// PLACEHOLDER awaiting `history_sweep`, like every magnitude in this layer:
/// the migration's own deepest descent runs 9-10 generations over roughly the
/// whole 4000-year span (BL-865's census), so this puts "fully foreign" a
/// little short of the oldest splits rather than at the horizon itself.
constexpr int64_t kinship_full_weight_years = 3000;
}

int culture_opposition_q(const std::vector<culture>& cultures, int a, int b)
{
    const int n = static_cast<int>(cultures.size());
    if (a < 0 || b < 0 || a >= n || b >= n || a == b) return 0;

    const culture& ca = cultures[static_cast<std::size_t>(a)];
    const culture& cb = cultures[static_cast<std::size_t>(b)];

    // AXIS ONE — TEMPERAMENT. The war god specifically (`pantheon[1]`, the god
    // every creed raises — creeds.cpp's `add_god("war", ...)`), not the whole
    // pantheon: a daughter inherits its parent's pantheon UNCHANGED
    // (`derive_daughter_culture` copies it whole), so comparing the full list
    // would mostly re-measure kinship a second time. The war god's zeal and
    // dominion each run 0-10, so the raw difference runs 0-20.
    int temper_q = 0;
    if (ca.pantheon.size() > 1 && cb.pantheon.size() > 1)
    {
        const culture_god& wa = ca.pantheon[1];
        const culture_god& wb = cb.pantheon[1];
        const int diff = std::abs(wa.zeal - wb.zeal) + std::abs(wa.dominion - wb.dominion);
        temper_q = clampi((diff * 1000) / 20, 0, 1000);
    }

    // AXIS TWO — COUNTRY. A binary disagreement about how to live
    // (CIVILISATION.md: "a people of the floodplain and a people of the
    // highlands... a material disagreement rather than a stated one") rather
    // than a graded one — there is no natural ordering over farm classes to
    // grade a difference by.
    const int farm_q = (ca.origin_farm_class >= 0 && cb.origin_farm_class >= 0
                      && ca.origin_farm_class != cb.origin_farm_class) ? 1000 : 0;

    const int raw_q = (temper_q + farm_q) / 2;

    // KINSHIP DISCOUNTS THE RESULT. A people that split off a few centuries
    // ago still largely shares its parent's pantheon (temperament differences
    // are then near zero already) but may already have settled different
    // ground (BL-864) — the discount is what stops a fresh, amicable split
    // from reading as a settled cross-cultural rivalry on farm class alone.
    // UNKNOWN ancestry (-1) gets NO discount, the safe default: opposition
    // should never read as lower for a pair the tree cannot actually relate.
    const int64_t years = culture_kinship_years(cultures, a, b);
    const int kin_q = years < 0
        ? 1000
        : clampi(static_cast<int>((years * 1000) / kinship_full_weight_years), 0, 1000);

    return (raw_q * kin_q) / 1000;
}
