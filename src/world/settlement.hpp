#pragma once

// ---------------------------------------------------------------------------
// Settlement & industrialisation — HISTORY.md Stages 3-4, made mechanical
// (BL-218 nations rewrite + BL-219's region read; docs/lore/HISTORY.md,
// docs/generation/NATION_GENERATION.md).
//
// The ladder (BL-221) counted how many independent peoples the land supported.
// The creeds (BL-235) gave each of them a tongue and a pantheon. This pass is
// the rung between those and the political map: it settles PROVINCES, carries
// each one's founding CULTURE forward, industrialises the ones the ground can
// pay for, and then hands the political pass its seeds.
//
// THREE THINGS THIS PASS IS FOR:
//
//  1. BELIEF MAPPED ONTO GROUND. A region is not a blob with a name; it is
//     a place a named people settled, and it carries their gods. The culture
//     it inherits is the nearest cradle's, so the pantheon distribution is a
//     map of who walked where. A region in an ancient ore window whose
//     culture raised a FORGE god industrialises earlier than one that did not
//     — the creed and the deposit are the same historical fact seen twice.
//
//  2. CHARACTER AS AN OUTPUT, NOT A DRAW. NATION_GENERATION's Pass 4 drew
//     ideology/expansionism/economic_focus from an independent RNG. Here each
//     axis derives from one quantity this pass produced (BL-218, settled
//     2026-08-02): expansionism <- the border-contest integral, economic_focus
//     <- the dominant resource class of the regions settled DURING
//     industrialisation, ideology <- industrialisation timing against
//     neighbours.
//
//  3. THE RECORD IS NOT SAFE. Wars redraw borders, spread the victor's gods
//     over the loser's regions, and ERASE part of the loser's history —
//     replaced by a dated lacuna, not silently dropped (Ben, 2026-08-02:
//     "don't be afraid to have parts of the record erased when two nations go
//     to war"). A history that cannot be damaged is a history nobody fought
//     over.
//
// THE PASS DRIVES, IT DOES NOT NARRATE — the BL-221/BL-235 rule, inherited.
// Regions are placed BEFORE the political map and become the nation seeds;
// the BFS/growth machinery BL-053 tuned is kept and only its INPUTS change
// (BL-218 § 1: "Seeding changes. Expansion does not.").
//
// DETERMINISM. Integer scoring with explicit index tie-breaks, fresh stage
// tags, every container walk in raster or sorted-id order, no transcendentals.
// The rupture checkpoints reuse planetology.hpp's class-agnostic
// `resolve_checkpoint` rather than inventing a second branch mechanism —
// BL-217 named this pass as its intended second client.
// ---------------------------------------------------------------------------

#include "colonisation.hpp" // BL-918: split_trigger, region_reculture
#include "creeds.hpp"
#include "history_ladder.hpp"
#include "planetology.hpp"
#include "world.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/// What a region's ground is mainly good for — the ANCIENT endowment, read
/// once from the deposits under it and never re-read. This is the quantity
/// BL-218 asks economic_focus to derive from and BL-219 asks corporate focus
/// to derive from, so it is deliberately coarse and shared.
enum class region_class : uint8_t
{
    none = 0,  ///< Nothing worth naming; a subsistence region.
    farm,      ///< Agricultural produce dominates.
    ore,       ///< Iron/copper/rare-earth — the forge god's country.
    energy,    ///< Coal and petroleum: the Stage 4 fuel that breaks the ceiling.
    port,      ///< Coastal, deposit-poor: it lives on what passes through.
};

/// Which of the three domains a region's ANCHOR stands in — the province
/// layer's `province_kind` brought down to the grain the Era -1 sim acts on
/// (BL-777, against Ben's water-domain ruling of 2026-09-06).
///
/// EXCLUSIVE BY CONSTRUCTION, exactly as `province_kind` is: a substrate names
/// one of the three and only one, so this is a classification of the ground and
/// never a judgement about it.
///
/// DERIVED, THEN STORED — and the asymmetry with `province_kind` is deliberate.
/// A province is derived-never-stored because it holds its own tile ids and can
/// re-ask them at any time. A region cannot: the Era -1 sim owns no `world&`,
/// no tile ids and no allocator by design (history_sim.hpp), and once the run
/// is over the campaign holds `settlement_state` without the era fixture's
/// terrain arrays beside it. So the domain is written ONCE at founding, from
/// `region_domain_of` on the anchor's substrate, and travels with the record.
/// Nothing ever moves an anchor, so it cannot desynchronise from the tile it
/// describes; `sim_water_census` asserts that identity rather than assuming it.
///
/// Values match `province_kind`'s numbering so the two can be compared directly.
enum class region_domain : uint8_t
{
    land          = 0, ///< Dry ground. What a region is normally founded on.
    coastal_water = 1, ///< The shoreline ring and the lakes — OWNED, via the shore (BL-776).
    open_ocean    = 2, ///< Open sea. Owned by nobody, so nothing may be founded here.
};

/// The one derivation, shared by every writer of the field above. Mirrors
/// `province_kind_of`'s branch order (province.cpp) — open ocean first, then
/// any other water, then land — so the two classifications cannot drift.
constexpr region_domain region_domain_of(terrain_substrate s)
{
    if (is_open_ocean(s)) return region_domain::open_ocean;
    if (is_water(s))      return region_domain::coastal_water;
    return region_domain::land;
}

// ---------------------------------------------------------------------------
// Culture shares (BL-826)
// ---------------------------------------------------------------------------

/// Cultures a region may carry EXPLICITLY. Fixed width on purpose: the region
/// stays POD-ish, the playback record stays bounded, and the flat-binary seam
/// stays a fixed field count rather than a length-prefixed list.
inline constexpr int culture_share_slots = 3;

/// WHOSE PEOPLE LIVE HERE, AS A DISTRIBUTION (BL-826).
///
/// This replaces `region::culture`, which was a single index — a region was
/// wholly one people or wholly another, and there was no way to say a conquest
/// was half digested. Two things ride on the change:
///
///   1. It is the TAKE-BACK. A cultural mix is a fact pass 1 hands forward that
///      a downstream pass could not have re-derived, because it is the residue
///      of who walked where over four thousand years.
///   2. It is an ANTI-HEGEMONY LEVER MADE REAL. Foreign ground shifts toward
///      its holder SLOWLY (`history_sim_params::assimilation_per_year_q`), so a
///      conquest is digested over centuries rather than at the instant the
///      border moves — and `w_cult` becomes a question about the distribution
///      rather than an equality test, so the discount on foreign ground fades
///      as the ground stops being foreign.
///
/// INTEGER PER-MILLE, AND THE SUM IS EXACTLY 1000. No floats: this accumulates
/// over four thousand years of decision rounds, which is precisely how a float
/// invariant is lost. `slots` carry the largest cultures, sorted DESCENDING by
/// weight with ties broken on the lower culture index, and `other_q` is the
/// tail bucket holding everything that fell off the end. `total_q()` is 1000
/// always, including for an unsettled region — which is all tail.
struct culture_shares
{
    /// Culture index into `creed_state::cultures`; -1 for an unused slot.
    /// Sorted descending by `weight_q`, ties on the lower index.
    int16_t id[culture_share_slots] = {-1, -1, -1};
    /// Per-mille weight of the matching slot. 0 exactly where `id` is -1.
    int16_t weight_q[culture_share_slots] = {0, 0, 0};
    /// The remainder — peoples too small to name. Never negative.
    int16_t other_q = 1000;

    /// A region nobody has settled: no named culture at all, all tail.
    bool empty() const { return id[0] < 0; }

    /// The largest named culture, or -1. THE PLURALITY, not the majority — use
    /// `majority` where a bare plurality should not be enough.
    int plurality() const { return id[0] >= 0 ? static_cast<int>(id[0]) : -1; }

    /// Per-mille of this region that is culture @p c. 0 for -1 and for any
    /// culture that has fallen into the tail — the tail is deliberately NOT
    /// attributable, which is what makes it cheap.
    int share_of(int c) const
    {
        if (c < 0) return 0;
        for (int i = 0; i < culture_share_slots; ++i)
            if (id[i] == static_cast<int16_t>(c)) return static_cast<int>(weight_q[i]);
        return 0;
    }

    /// Strictly more than half the region. The test an institution that cannot
    /// survive on a plurality should use.
    bool majority(int c) const { return share_of(c) > 500; }

    /// Always 1000. Exposed so callers and harnesses can assert it rather than
    /// trust this comment.
    int total_q() const
    {
        int t = static_cast<int>(other_q);
        for (int i = 0; i < culture_share_slots; ++i) t += static_cast<int>(weight_q[i]);
        return t;
    }

    /// A region wholly one people. `pure(-1)` is the unsettled default.
    static culture_shares pure(int c)
    {
        culture_shares s;
        if (c < 0) return s;
        s.id[0] = static_cast<int16_t>(c);
        s.weight_q[0] = 1000;
        s.other_q = 0;
        return s;
    }

    /// ASSIMILATION. Move @p amount_q per-mille OF THE FOREIGN REMAINDER toward
    /// culture @p c, conserving the 1000 total exactly.
    ///
    /// Proportional rather than flat, for the reason `w_cult` had to become
    /// proportional (history_sim.hpp § The corollary): a flat transfer converts
    /// the last sliver of a minority at the same speed as the first half of a
    /// majority, so it reads as a deadline rather than as a force. Proportional
    /// means the same thing at any starting mix, and it never reaches 1000, so
    /// a conquered people is never arithmetically erased.
    ///
    /// Integer throughout. The floor losses of the proportional take are
    /// redistributed deterministically (largest component first, ties on the
    /// lower slot, tail last), so the total is conserved to the unit and the
    /// result cannot depend on iteration order.
    void shift_toward(int c, int amount_q);

    bool operator==(const culture_shares& o) const
    {
        if (other_q != o.other_q) return false;
        for (int i = 0; i < culture_share_slots; ++i)
            if (id[i] != o.id[i] || weight_q[i] != o.weight_q[i]) return false;
        return true;
    }
    bool operator!=(const culture_shares& o) const { return !(*this == o); }
};

/// One settled region — the unit of settlement history, and the unit BL-219
/// reads a corporation's focus from.
struct region
{
    int anchor = -1;        ///< Raster index (row * gw + col) of the core tile.
    int col = 0;
    int row = 0;

    /// BL-777 — WHAT THIS GROUND IS, not merely where it is.
    ///
    /// Set at founding from the anchor's substrate and never recomputed. The
    /// sim is the first pass that decides anything about water and until this
    /// field existed it could not see any: `sim_terrain_view` carried the
    /// substrate all along, and nothing read it. Defaults to `land` because a
    /// region built by a caller with no terrain (the synthetic harness cases,
    /// whose `sim_terrain_view` is empty by design) is standing on the neutral
    /// default ground `sub_at` hands out, which is dry.
    region_domain domain = region_domain::land;

    /// WHOSE GODS THIS REGION KEEPS, as a DISTRIBUTION (BL-826).
    ///
    /// Was a single `int` index; a region was wholly one people. It starts as
    /// the nearest cradle's, pure, and conquest then shifts it SLOWLY toward
    /// the holder rather than flipping it — see `culture_shares` above and
    /// `history_sim_params::assimilation_per_year_q`.
    ///
    /// EVERY former `p.culture == q.culture` test had to be answered
    /// deliberately as plurality, majority or share-weighted; the type change
    /// is what forced each one to be answered rather than defaulted.
    culture_shares culture;
    /// The culture that FOUNDED it. Never overwritten, so a conquered region
    /// still records who built it — the erasure is of the record, not of this.
    int founding_culture = -1;
    /// True once a conqueror's pantheon replaced the founders' here.
    bool creed_conquered = false;

    std::string name;       ///< "<people>'s <quarter>", in the founders' tongue.

    int settle_score_q = 0; ///< 0-1000 — how strongly the ground invited settlement.

    /// Ancient endowment windows, 0-1000, surveyed once over the region's
    /// neighbourhood. These are the deposits that were already in the ground
    /// before anybody arrived; industrialisation spends them.
    int farm_q = 0;
    int ore_q = 0;
    int energy_q = 0;
    int port_q = 0;
    region_class dominant = region_class::none;

    int64_t founded_year = 0;     ///< Calendar year settled (negative = before epoch year 0).
    int64_t industrial_year = 0;  ///< Calendar year the furnaces lit; 0 when never.
    bool    industrialised = false;

    /// BL-748 — THE ENDOWMENT HALF OF STAGE 4, SEPARATED FROM THE DATE.
    ///
    /// Years this ground takes to raise a furnace once its owner can pay for
    /// one at all, or **negative when it never can**. `run_settlement` still
    /// owns the gate and the gradient — the fuel test and the endowment/creed
    /// terms are unchanged, coefficient for coefficient — but it no longer
    /// owns the DATE. Under an industrial epoch the second span is where
    /// industrialisation happens, so the year a region lights is the year its
    /// polity's materials capacity crossed the Industrial rung inside
    /// `run_history_sim`, plus this lag. Endowment, not virtue, in both
    /// directions (HISTORY.md § Stage 4) — and reached by playing rather than
    /// pre-resolved before the loop starts.
    ///
    /// -1 rather than 0 as "never", because 0 is a legitimate lag: the
    /// best-endowed ground in a world lights the year its owner crosses.
    int industrial_lag_years = -1;

    /// Index into the nation-id list, once the political pass has run.
    ///
    /// BEFORE that pass it holds the POLITY id that owned this region at the
    /// epoch — `run_history_sim` writes it as it goes, and BL-769 is the item
    /// that stopped throwing it away: `settlement_seed_polities` reads exactly
    /// this field to hand the history's political map to `generate_nations`,
    /// which folds the regions of one polity into one nation rather than
    /// re-inventing borders from the anchors.
    int nation = -1;
    int contest_q = 0; ///< 0-1000 — how hard this region's frontier was pressed.

    /// BL-750 — the tariff posture of whoever held this region at the epoch,
    /// 0-1000, broadcast off `polity::protection_q` at the end of the sim. Held
    /// per region for the same reason `contest_q` is: the political pass reads
    /// regions, not polities, and this is the handoff object. Zero on every
    /// path where no sim ran.
    int protection_q = 0;

    // --- Settlement seats and hinterland (BL-866) --------------------------
    // CIVILISATION.md § The unit is the city state, and settlements are
    // sparse. SETTLED (Ben, 2026-09-09): "a settlement is a SEAT FLAG ON A
    // REGION, and every region points at the seat it feeds." No new table,
    // no new id space — a region already carries everything a seat needs to
    // be worth taking (population, manpower_stock, army_stock, the four
    // endowment windows, the urban record), so this is two fields, not a
    // record.

    /// TRUE FOR A SPARSE MINORITY OF REGIONS. A settlement worth taking: it
    /// holds the stores (owed, § Materials are spent — BL-867) and is where
    /// the muster forms. `history_sim.cpp` seeds one per polity, at its
    /// `capital` — the polity's best-settled region — and never clears it:
    /// a seat is a fact about the GROUND, so conquest changes who governs
    /// from it, never whether it is one (mirrors `founding_culture`, which
    /// conquest also never overwrites).
    bool is_seat = false;

    /// WHICH SEAT'S HINTERLAND THIS REGION IS PART OF — an index into the
    /// same `settlement_state::regions` vector, itself when `is_seat` is
    /// true, or -1 when no seat reaches it at all (a legitimate outcome,
    /// CIVILISATION.md: "falls outside anyone's reach").
    ///
    /// THE POINTER IS WHAT MAKES GROUND CHANGE HANDS WITH ITS SEAT. Taking a
    /// seat in `history_sim.cpp` carries every region pointing at it into the
    /// same ownership change, in the same event — ground is not conquered
    /// region by region. A region captured on its OWN, decoupled from its
    /// seat, is re-pointed at its new owner's seat instead of left dangling.
    int seat_region = -1;

    // --- BL-910: capitals and markets stand at the close --------------------
    // CIVILISATION.md sec Capitals exist at the close. SETTLED (Ben,
    // 2026-09-11): near the Empire pass's 1200 CE close, every surviving
    // polity that still holds a governable seat gets a market centred on
    // it. THE CAPITAL IS ALREADY THE SEAT (`is_seat`/`polity::capital`
    // above) -- this is not a second placement pass, and this flag is a
    // PROMOTION of that existing fact, never a new id space.

    /// TRUE ONLY ON A REGION THAT IS BOTH A SEAT AND STILL GOVERNED AT THE
    /// CLOSE. Set once, in `run_history_sim`'s closing block, for every
    /// living polity's `capital` region; never set anywhere else and never
    /// cleared once set (a market that stood is not un-stood by a later
    /// conquest -- the same ground keeps carrying it, mirroring `is_seat`).
    /// NO QUOTA: a polity with no governable seat at the close leaves every
    /// one of its former regions at `false`, on purpose.
    ///
    /// A PLACE AND A VISIBLE CONDITION, NOT AN ORDER BOOK. No firms, no
    /// clearing tick, no price band, no inventory ride on this flag -- the
    /// economy pass materialises those, from this fact, downstream.
    bool has_market = false;

    // --- Materials and labour (BL-867) --------------------------------------
    // CIVILISATION.md § Materials are spent when something happens. SETTLED
    // (Ben, 2026-09-09): "the stores sit AT THE SEAT, and fall with it." No
    // polity-level treasury and no per-region heap — a hinterland region's
    // industry flows OUT through `seat_region` and only ever accumulates on
    // the region that IS a seat (`is_seat`). That is what makes conquest of
    // the seat conquest of the hoard FOR FREE: nothing here is copied when
    // ownership changes above, only `nation`/`owner[]` move, and this field
    // travels with the region exactly as `population` and `army_stock` do.

    /// Material units standing at this region, if it is a seat. Zero and
    /// permanently unused on every ordinary hinterland region — industry
    /// produced there is credited to `seat_region`'s stock, never banked
    /// locally.
    int64_t material_stock = 0;

    // --- BL-932: the treasury -----------------------------------------------
    // EXPLORATION.md sec Capital arrives, and it sits in the capital. SETTLED
    // (Ben, 2026-09-11, superseding a same-day morning ruling that floated it
    // free of the map): "the treasury is moved to the capital for this phase."
    // ONE PER POLITY, AND IT LIVES ON THE REGION, EXACTLY LIKE `material_stock`
    // ABOVE, FOR THE SAME REASON. A conqueror who takes the seat must take
    // something real; putting the treasury on `polity` instead would have
    // meant inventing a SECOND capture path (an explicit transfer at the
    // conquest site) where `material_stock` needs none at all — it is a fact
    // about the ground, so ownership changing above (`nation`/`owner[]`) is
    // the whole of the transfer, for free, by construction.

    /// Capital standing at this region, valid only where this region is a
    /// living polity's `capital` (which is always its seat — BL-866). Zero
    /// and unused everywhere else. Fed by `run_exploration_upkeep` (BL-932):
    /// endowment on held ground, the inherited corridor network, and the
    /// trade flowing through its market (BL-954) — nothing tops it up, so a polity that inherited
    /// little stays poor. GENERATION SCRATCH, NOT SAVED, same footing as
    /// `material_stock`/`network_supply_q` above.
    int64_t treasury = 0;

    // --- BL-939: the scarcity signal -----------------------------------------
    // EXPLORATION.md sec There is no price here, only a scarcity signal.
    // ONE INTEGER PER GOOD PER MARKET, 0-1000, valid only where `has_market`
    // is true. NO PRICE, NO CLEARING, NO ORDER BOOK, NO FIRM -- "how badly
    // this market wants this good", nothing more. Indexed in `region_class`
    // order, offset by one to skip `region_class::none` (farm=0, ore=1,
    // energy=2, port=3) — see `history_sim.cpp`'s `scarcity_good_index`.

    /// THE UNMET SIGNAL (BL-954; EXPLORATION.md sec There is no price here:
    /// "the signal reads UNMET want"). `scarcity_raw_q` below less the volume
    /// every inbound `trade_flow` brought this round, floored at 0 -- what
    /// every reader (the tech chooser's `wants_unmet_q`, `market_scarcity_q`)
    /// sees. Equal to the raw signal wherever nothing flowed in. Zero and
    /// unused on every non-market region. GENERATION SCRATCH, NOT SAVED,
    /// same footing as `treasury`/`material_stock` above.
    int32_t scarcity_q[4] = {0, 0, 0, 0};

    /// THE RAW WANT (BL-954): what the market's ground lacks and its people
    /// need, before trade. Refreshed every decision round by
    /// `refresh_market_scarcity` (BL-939) while
    /// `history_sim_params::exploration_upkeep_enabled` is set; the ONE input
    /// a trade flow's volume reads for its want bound, so a flow cannot
    /// relieve the very signal that sized it. GENERATION SCRATCH, NOT SAVED.
    int32_t scarcity_raw_q[4] = {0, 0, 0, 0};

    // --- BL-935: a built port, navy and standing army ------------------------
    // EXPLORATION.md sec Force persists now, and persistence has a bill.
    // `port_q` ABOVE is ENDOWMENT -- the ground's own window, surveyed once and
    // never spent. This is the ASSET a polity's treasury actually builds on
    // that window, and the two must never be conflated (the doc's own words:
    // "otherwise every coastal polity begins with the thing the phase is
    // about acquiring").

    /// 0-1000, built by `run_exploration_upkeep` out of `treasury` while this
    /// region carries a `port_q` endowment window; silts back toward 0 when
    /// the treasury cannot maintain it. Zero and permanently unused on ground
    /// with no port window at all. GENERATION SCRATCH, NOT SAVED, same footing
    /// as `treasury`/`scarcity_q` above.
    int32_t port_stock_q = 0;

    /// BL-955 — THE PAID STANDING ARMY: how many of `army_stock`'s heads are
    /// men the treasury bought (`run_exploration_upkeep`'s army step) rather
    /// than men the muster raised. They ARE garrison men — counted inside
    /// `army_stock`, so they fight, march and die exactly as any other — and
    /// the one difference is that `muster_garrison` never disbands them: its
    /// target and its excess are read on the ORDINARY men
    /// (`army_stock - standing_army_heads`). Lost in proportion to the pool on
    /// every loss; unfunded, they decay back to ordinary men in the upkeep.
    /// Valid only while `nation == standing_army_owner` (read through
    /// `standing_army_heads`), so ground that changes hands loses its paid
    /// status without every ownership site having to clear it. Zero
    /// throughout the Empire span, where nothing buys it. GENERATION SCRATCH,
    /// NOT SAVED: the men themselves persist in the saved `army_stock`.
    int64_t standing_army       = 0;
    int     standing_army_owner = -1; ///< the polity that paid for `standing_army`.

    // --- Civilisations (BL-869) ---------------------------------------------
    // CIVILISATION.md § A civilisation is what mixing makes, and it is not a
    // creed. "A region carrying two peoples in quantity, for a long time" is
    // the trigger; these two fields are the ground's own memory of how long,
    // and what it grew. No existing timer covered this — `assimilation_per_
    // year_q` moves `culture` itself but nothing recorded how long a mix had
    // already sat there, so this is new rather than a repurposed field.

    /// Consecutive decision rounds' worth of years (advanced by the sim's own
    /// `step_years`, exactly like the assimilation shift) this region's
    /// SECOND-largest culture share has sat at or above
    /// `civilisation_mix_threshold_q`, UNBROKEN. Reset to 0 the instant the
    /// mix falls below the threshold or the region has no second culture —
    /// "for a long time" means unbroken, not cumulative, so a mix that
    /// digests away and later returns starts the clock over.
    int64_t mix_years = 0;

    /// Index into the sim's civilisation list (`history_sim_state::
    /// civilisations`), or -1. Set once by `run_civilisation_formation`
    /// (history_sim.cpp) and NEVER cleared afterward — a fact about the
    /// ground, mirroring `founding_culture`, which is what lets a
    /// civilisation OUTLIVE the polity that formed it.
    int civilisation = -1;

    // --- Demography (BL-273) ----------------------------------------------
    // The region is the unit of population as well as of settlement — see
    // demography.md's section header below for the model. Left at zero here;
    // seeding an initial headcount is the graduation pass's job (BL-271), not
    // this item's (design: "you don't need to wire that graduation path").

    /// Integer headcount. Zero until a caller seeds it — a demography step
    /// from zero stays zero (nothing to grow from), which is the logistic
    /// model's own edge case, not a special case coded around it.
    int64_t population = 0;
    /// Calendar year `advance_region_demography` last advanced this
    /// region to — bookkeeping so repeated calls compose (each step only
    /// covers the years since the last one), never re-read for anything else.
    int64_t last_demography_year = 0;
    /// Recruitable manpower currently banked — the army budget BL-273 closes
    /// the loop with. A bounded fraction of `population` (`manpower_ceiling`),
    /// refilled gradually by `replenish_manpower`, spent by `raise_manpower`.
    ///
    /// BL-835 — THIS IS A POOL OF ELIGIBLE CIVILIANS, NOT AN ARMY. Drawing it
    /// is what raising an army COSTS; the soldiers themselves stand in
    /// `army_stock` below. Nothing in the sim fights out of this field.
    int64_t manpower_stock = 0;

    /// BL-835 — THE ARMY STANDING ON THIS REGION, in heads. Separate from
    /// `population` and from `manpower_stock`, and that separation is the
    /// whole point of the field.
    ///
    /// BEN'S RULING, 2026-09-08: "population as a civilian thing — where
    /// armies are distinct from population, and we don't simulate total
    /// warfare in stage 4." Population moves on demography, habitability,
    /// famine and plague. Battles destroy ARMIES; they do not thin the people
    /// living on the ground.
    ///
    /// WHY IT HAD TO EXIST. Before it, a region's defence was read straight
    /// off `manpower_stock`, which is capped by `manpower_ceiling(population)`
    /// — so a region war had emptied of people had no manpower, therefore no
    /// defence, FOREVER. It outscored every real objective on every round of
    /// the rest of the run. In the seed-0 fixture all 258 battles were that
    /// one region: `battles == conquests == 258`, exactly 1:1.
    ///
    /// Under an army pool an undefended region is a NORMAL and TEMPORARY
    /// state — an army marched away, a levy not yet raised — rather than a
    /// permanent property of dead ground. Walking in is cheap exactly once,
    /// because the army that walked in is then standing there.
    ///
    /// RAISED by `muster_garrison` out of `manpower_stock` (the cost, in
    /// bodies that leave the fields and come back only slowly), SPENT by
    /// `spend_army` when a battle goes against it, and MOVED between regions
    /// by the sim's campaign verb. `population` is untouched by all three.
    int64_t army_stock = 0;

    // --- The urban record (BL-766, the population map is drawn early) ------
    // WHY IT LIVES HERE AND NOT AS ENTITIES. The Era -1 sim has no ECS access
    // by design (history_sim.hpp: no `world&`, no tile ids, no allocator), so
    // a city the sim can grow and sack cannot BE a population_centre entity
    // while the sim runs. It is represented at SIM GRAIN instead — three
    // integers on the region — and `generate_population_centres` materialises
    // the campaign-era entities from this record once the sim has finished.
    //
    // The map is DRAWN BEFORE THE SIM (`draw_urban_map`), weighted toward
    // ground that farms easily, and then grown by `advance_region_urban` and
    // destroyed by `sack_region_urban` as the history runs. That ordering is
    // the item's whole point: centres are still history's consequence, but
    // because history grew and sacked them rather than because they were
    // placed afterwards — and the sim stops running over a world with no
    // cities in it.

    /// Population centres standing in this region. Promoted as
    /// `urban_population` crosses `region_centre_heads`, cut by a sack.
    int centres = 0;

    /// Centres history DESTROYED here — cumulative, never decremented. A
    /// region that was sacked and rebuilt still records that it was sacked,
    /// which is what makes the ruin legible rather than merely absent.
    int centres_razed = 0;

    /// Heads living in this region's centres, a subset of `population`. The
    /// quantity the campaign-era centre count and scale carve reads.
    int64_t urban_population = 0;

    /// BL-872 (CIVILISATION.md "Centres are derived by supply and
    /// governance") — 0-1000, how well THIS region's own seat can still
    /// reach it: `history_sim.cpp`'s terrain-and-road Dijkstra reach from
    /// the polity's capital, the SAME quantity BL-837 already prices a
    /// campaign at and attrites an unsustained garrison with. Governance
    /// ("can the seat rule this ground") and supply ("can materials reach
    /// it") are read as ONE quantity here — a settled call, not an
    /// oversight: this sim has one network, one Dijkstra, one seat per
    /// region, so a second number would only ever restate the first.
    ///
    /// Written by `run_history_sim` once per decision round for every
    /// region a living polity holds (a region nobody holds keeps its last
    /// value); read by `advance_region_urban`'s caller to decide whether
    /// `centres` may still grow. Defaults to 1000 (full supply) so a region
    /// not yet touched by a decision round — the opening seed, a region
    /// founded this very year — is never spuriously frozen before the
    /// network model has had a chance to price it.
    ///
    /// GENERATION SCRATCH, NOT SAVED. Like `is_seat`/`seat_region`/
    /// `material_stock` before it (BL-866/BL-867), this is a fact the Era -1
    /// sim maintains about ground it is actively simulating, not a fact the
    /// campaign era reads afterwards — `w_region`/`r_region`
    /// (`src/core/save_game.cpp`) do not carry any of those four fields, and
    /// this one follows the same precedent rather than adding one.
    int network_supply_q = 1000;

    // --- BL-897: a creed that spans cultures, at the PEOPLE grain -----------
    //
    // `../../docs/lore/CREEDS.md` sec A creed that spans cultures settles a
    // creed that subsumes the pantheons it meets and spreads along CONTACT
    // rather than ancestry. Two grains carry it and they are EXPECTED TO
    // DISAGREE: the polity adopts it as an institution (`polity::
    // universal_creed`), and the peoples under that polity convert at their own
    // pace or refuse. The gap between the two is the fault line.
    //
    // THE REGION STANDS FOR THE PEOPLE ON IT. This struct already carries a
    // culture mix and a plurality, and the sim has no finer grain than ground;
    // a per-culture creed roll would be a second population model. So "a people
    // holds the creed" is recorded here, which also makes the unevenness
    // SPATIAL -- an empire's near ground converts and its far ground does not,
    // which is the shape a schism needs.
    //
    // GENERATION SCRATCH, NOT SAVED, exactly like `network_supply_q` and
    // `material_stock` above: these are facts the Era -1 sim maintains about
    // ground it is simulating, and `w_region`/`r_region` do not carry them.

    /// Where this ground stands with the universal creed its owner adopted.
    /// 0 none, 1 DUAL-HOLD (it carries its pantheon AND the creed), 2 CONVERTED
    /// (the pantheon has faded to residue), 3 REASSERTED (the pantheon won and
    /// the people fell back out of the creed).
    ///
    /// THE DUAL-HOLD STATE IS THE DESIGN, not an implementation convenience.
    /// CREEDS.md: "a single-state model -- converted or not -- would have left
    /// the fracture nothing to fracture along."
    int8_t creed_hold = 0;

    /// Index into `history_sim_state::universal_creeds`, or -1. Set when this
    /// ground enters dual-hold; cleared again if the pantheon REASSERTS.
    int universal_creed = -1;

    /// THE PANTHEON UNDERNEATH -- the culture index whose creed this ground held
    /// when the universal creed reached it, kept whichever way the pair
    /// resolved. Never cleared: a subsumed pantheon is RESIDUE, not an erasure,
    /// and residue is what a later schism is made of.
    int creed_residue_culture = -1;

    /// Years this ground has carried both at once. Advanced by the sim's own
    /// `step_years`, exactly like `mix_years` above.
    int64_t creed_hold_years = 0;

    // --- Era -1 works (BL-321) --------------------------------------------
    // What this region has BUILT, and what those works are worth. The works
    // TABLE lives in works_roster.hpp/works.lua; only the per-region record
    // lives here, and deliberately as plain integers rather than a
    // `work_effect` member: works_roster.hpp already forward-declares
    // `region`, so including it back here would close a cycle to store five
    // ints. The accumulators below are the same five fields by another name.
    //
    // A BITMASK, NOT A VECTOR. The design offered either; the mask wins because
    // this struct is copied on every settle (the Settle verb push_backs a whole
    // region) and lives in a vector that grows past 400 entries inside a pass
    // already costing ~23 s of a ~25 s world. A mask keeps `region` free of
    // heap ownership and makes "have I already built this?" one AND.
    //
    // The consequence is a hard 32-row ceiling on the works table, which
    // `works_registry::load_from_lua` checks so a 33rd row fails at startup
    // rather than silently becoming unbuildable.

    /// Bit `i` set = row `i` of `works_registry::rows()` stands here. Works are
    /// PHYSICAL: conquest transfers them with the region rather than razing
    /// them, which is what makes a developed region worth taking.
    uint32_t works_built = 0;

    /// Accumulated per-mille effects of `works_built`, maintained incrementally
    /// by `apply_work_to_region` so every read is O(1). Re-derivable from the
    /// mask at any time via `works_registry::total_effect_mask`.
    int work_capacity_mod   = 0; ///< Raises `region_carrying_capacity`.
    int work_manpower_mod   = 0; ///< Raises `manpower_ceiling`.
    int work_reach_mod      = 0; ///< Discounts terrain-weighted supply cost.
    int work_defence_mod    = 0; ///< Readiness this region adds when DEFENDING.
    int work_industrial_mod = 0; ///< Pull-forward on the industrial clock.
};

// ---------------------------------------------------------------------------
// Stage 1 — the enforceable promise, as a diffusion frame (BL-638)
// ---------------------------------------------------------------------------

/// WHERE the enforceable promise was written, and how far it travelled from
/// there. HISTORY.md § Stage 1: contract law extends beyond kinship until an
/// entity can outlive its members and own, sue and be sued. The ladder already
/// picks the seat (`history_ladder_state::charter_cradle` — the best-placed
/// TRADING cradle, because Stage 1 is about promises between strangers) and
/// the creeds already plant the sealed-oath god on it. This frame is the third
/// read of that same fact: which ground the charter reached.
///
/// ERA-AGNOSTIC BY CONSTRUCTION, which is the whole reason it exists. The
/// ladder picks a charter cradle in EVERY era, so a class derived from this
/// frame says something on an antiquity world as well as an industrial one.
/// Stage 4's furnace dates cannot: an antiquity world skips Stage 4 entirely,
/// so `median_industrial_year` is 0 there and anything reading it collapses.
///
/// Pure: `derive_charter_reach` takes no rng and the two predicates take no
/// state beyond the frame and the region.
struct charter_reach
{
    /// True once a charter cradle exists at all. A world with no agrarian
    /// cradle wrote no charter, and NOTHING reached the promise there — an
    /// honest all-closed world, and the one the public floor's unmeetable
    /// waiver exists for.
    bool written = false;

    /// Index into `creed_state::cultures` of the people who keep the sealed
    /// oath — the charter cradle's own culture. -1 when no creed pass ran.
    int culture = -1;

    int col = 0;      ///< The seat's core tile (the charter cradle's).
    int row = 0;
    int grid_w = 0;   ///< Column wrap for the contact metric.

    /// Contact distance within which the charter is LIVED rather than merely
    /// known — the seat's own trading hinterland.
    int near_dist = 0;
    /// ... and within which it has at least been COPIED. Beyond this, nothing.
    int far_dist = 0;
    /// How much distance a fully-port region discounts: the charter is a
    /// merchant's instrument and arrives by ship, which is why the ladder
    /// weights coastal access so heavily when it picks the seat at all.
    int port_reach = 0;
};

/// Derive the frame from the ladder and the creeds.
///
/// The two radii are set by the ground, not by a target: the same barrier
/// terrain that prices conquest prices contact, so the charter's copies travel
/// far across open country and stall against broken country. At zero
/// resistance the copied band is half the world; at total resistance, a tenth
/// of it. The lived band is half the copied one.
charter_reach derive_charter_reach(const history_ladder_state& hl,
                                   const creed_state& cs, int gw, int gh);

/// Does this region LIVE the enforceable promise — its own creed witnesses the
/// sealed oath, or it sits inside the seat's trading hinterland?
bool charter_lived(const charter_reach& ch, const region& p);

/// Has the promise at least REACHED this region — lived, or copied from
/// neighbours in contact with the seat? HISTORY.md § Stage 2's lesson is that
/// an institution re-emerges next door, so the reach deliberately ignores
/// political borders: it is a map of contact, not of sovereignty.
bool charter_copied(const charter_reach& ch, const region& p);

/// What the settlement pass computed for one body.
struct settlement_state
{
    std::vector<region> regions;      ///< In placement order (best ground first).
    std::vector<history_event> history;   ///< Dated settlement / furnace / war lines.
    std::vector<checkpoint_record> checkpoints; ///< Rupture branch decisions (BL-217 shape).

    /// How many history lines the wars destroyed. Every one is replaced by a
    /// dated lacuna line, so this is a count of holes the player can SEE.
    int lacunae = 0;

    /// The world-median industrialisation year over industrialised regions,
    /// or 0 when none industrialised. BL-219's "early vs late" pivot reads it.
    int64_t median_industrial_year = 0;

    /// True once `draw_urban_map` has run over these regions (BL-766). It is
    /// the difference between "history razed every city" and "no urban map was
    /// ever drawn", which a zero urban headcount alone cannot tell apart — and
    /// the two want opposite behaviour from the campaign-era carve.
    bool urban_map_drawn = false;

    /// Stage 1's diffusion frame (BL-638). Carried on the settlement record
    /// rather than passed separately because every consumer already holds one:
    /// the corporation pass takes `const settlement_state*`, and the generation
    /// report copies this struct whole.
    charter_reach charter;

    /// THE FOUNDING SCHEDULE — regions the colonisation walk dated INSIDE the
    /// sim's own span, waiting for their year to come round (BL-846).
    ///
    /// WHY IT EXISTS. Settlement used to hand the sim a finished map: every
    /// region it would ever have, placed and dated before the first tick. The
    /// sim then only ever redrew BORDERS, so a time-lapse of it opened with
    /// every continent already claimed and there was no origin to watch —
    /// round 4 was showing the last tenth of the story it advertised
    /// (Ben, 2026-09-09, at the live app: "I'm still seeing phase 4 as our
    /// combined colonization and conquest parts").
    ///
    /// Now a region whose stream arrives after the sim starts waits HERE, and
    /// `run_history_sim` founds it when its year arrives — emitting an
    /// ownership change like any other founding, so the filling of the world is
    /// something the record CONTAINS rather than something that happened before
    /// it began. Colonisation and conquest become two visibly different halves
    /// of one span.
    ///
    /// The farm class each CRADLE culture was coined on, as (culture id, class)
    /// pairs (BL-865). The daughters carry theirs on the culture record itself;
    /// the cradles cannot, because `run_settlement` takes `creed_state` by const
    /// reference, so the caller copies these back onto the roster.
    std::vector<std::pair<int, int8_t>> cradle_origin_class;

    /// The year each CRADLE culture was coined, as (culture id, year) pairs
    /// (BL-870) — the other half of `cradle_origin_class`'s round-trip, and
    /// for the same reason: `run_settlement` cannot write `creed_state`
    /// itself, so the caller copies these back onto the roster. Every cradle
    /// is `colonisation_start_year` (BL-856: "EVERY CRADLE STARTS AT THE SAME
    /// MOMENT"); daughters carry their own arrival year on the culture record
    /// directly, set where they are derived.
    std::vector<std::pair<int, int64_t>> cradle_coined_year;

    /// THE CULTURES THE MIGRATION COINED (BL-856), in allocation order, with
    /// ids running one past the last cradle culture. Derived from their
    /// parents rather than rolled fresh, so a homeworld ends with a FAMILY of
    /// related peoples grouped by the routes their ancestors took — which is
    /// what makes the round a map of a migration rather than of where
    /// agriculture started.
    ///
    /// APPENDED TO `creed_state::cultures` by the caller immediately after this
    /// pass, so every downstream consumer (the sim's per-culture aggression, the
    /// naming passes) sees one flat list and needs no second lookup.
    std::vector<culture> spawned_cultures;

    /// THE YEAR THE MIGRATION ENDED (BL-858) — when the last stream that was
    /// ever going to land, landed. The migration round's terminating condition,
    /// carried on the settlement record because every consumer already holds
    /// one.
    ///
    /// Ben's rule is "all land has some culture". Read literally it never
    /// terminates: ground no package can farm never gets a culture by
    /// construction, and BL-859's census shows that is 44-57% of every world,
    /// dominated by polar ice. What the rule MEANS is *wait until nothing more
    /// is going to happen*, and the flood answers that by finishing — so this
    /// needs no safety stop, no cap and no watchdog. It is a reading of a walk
    /// that terminates, not a loop that might not.
    int64_t migration_end_year = 0;

    /// ASCENDING BY `founded_year`, then by placement order — a total order, so
    /// two regions dated to the same year are founded in an order that cannot
    /// depend on a sort's stability.
    ///
    /// EMPTY IS THE ORDINARY CASE for every caller that does not ask for a
    /// schedule: `run_settlement`'s `sim_start_year` defaults to the stop year,
    /// which puts every region in `regions` exactly as before.
    ///
    /// IT IS DRAINED BY THE SIM, not carried past it. After `run_history_sim`
    /// this is empty and every region it held is in `regions` — which is what
    /// keeps it off the save seam: `generation_report` copies the settlement
    /// AFTER the sim, so there is no half-founded state to serialise.
    std::vector<region> pending_foundings;

    /// THE COLONISATION SPAN'S SETTLED CELLS (BL-849) — raster-order (row * gw +
    /// col, the same order `colonisation_field` and the caller's `tile_ids` use),
    /// one byte per tile: 1 where the migration's flood both reached the ground
    /// AND could farm it (`colonisation_field::farmable`), 0 otherwise. Water and
    /// never-reached ground both read 0 — this is not a "was this tile visited"
    /// record, it is "did anybody's stream actually live here".
    ///
    /// WHY IT IS CARRIED HERE RATHER THAN LEFT INSIDE `run_settlement`: the
    /// province partition (`docs/generation/PROVINCES.md` § The settled cells
    /// are a binding input) needs it as a HARD INPUT, the way it already takes
    /// the national assignment, and the partition runs long after this call has
    /// returned and `col_field` has gone out of scope. The caller
    /// (`hard_coded_world.cpp`) turns this into `world::tile_settled` before
    /// `build_province_partition` runs.
    ///
    /// EMPTY for a caller that never asked for a schedule (`sim_start_year`
    /// still at its `INT64_MAX` default runs the walk exactly as before and
    /// still fills this — it is a straight copy of `colonisation_field::
    /// farmable`, so it costs one vector move and reproduces byte-identically
    /// whether or not anything reads it).
    std::vector<uint8_t> settled_cells;

    /// THE SHAPE OF THE MIGRATION'S CULTURE TREE (BL-918) — the numbers
    /// "over-tune the splits" is only honest against. Read at the end of the
    /// Culture round over every region the walk placed, BEFORE the antiquity
    /// stop trims the list to the epoch, so it is a fact about the migration
    /// and not about where the campaign happens to start.
    ///
    /// THE SWEEP'S HOOK: `history_sweep` carries `era_minus_one_fixture::
    /// settlement`, so this rides on it with no further plumbing; the sibling
    /// item that prints it on the sweep's JSON row reads it from here.
    struct culture_census
    {
        int32_t cultures = 0;      ///< Every people at the round's end, cradles included.
        int32_t cradles  = 0;      ///< Of which coined by a cradle rather than the walk.
        int32_t holding_ground = 0; ///< Peoples that are the plurality on at least one region.
        int32_t tree_depth = 0;    ///< Deepest descent, in generations from a cradle.
        /// Distinct pairs of DIFFERENT peoples whose regions are cheaply adjacent
        /// (`isolation_adjacent`), and the mean of `culture_kinship_years` over
        /// them; -1 when there are no such pairs. How kin the neighbours are.
        int64_t adjacent_pairs = 0;
        int64_t mean_adjacent_kinship_years = -1;
        /// Splits by trigger, indexed by `split_trigger`.
        int32_t splits[split_trigger_count] = {};
        /// Regions the isolation pass moved to a daughter, and the record steps
        /// it read.
        int32_t recultured_regions = 0;
        int32_t isolation_steps    = 0;
    };
    culture_census census;

    /// EVERY REGION THE ISOLATION PASS MOVED TO A DAUGHTER (BL-918), with the
    /// year: the migration time-lapse's hook for showing a range coming apart.
    /// `region` indexes `regions` as it stood when the pass ran — before the
    /// antiquity stop and the founding schedule partitioned the list — so a
    /// reader that wants a live index must map through `anchor`.
    std::vector<region_reculture> culture_recultured;
};

/// Settle the body: place regions, inherit each one's cradle culture, survey
/// its ancient endowment, and industrialise the ones the ground can pay for.
///
/// Pure deterministic function of its inputs. Must run AFTER `run_creeds` /
/// `record_tribal_conflict` (it reads the surviving cultures) and BEFORE
/// `generate_nations` (it supplies the seeds).
///
/// @param pl        Body planetology state (arable share, endemics).
/// @param hl        The ladder — cradle positions, fragmentation, charter cradle.
/// @param cs        The creeds — one culture per cradle, with its pantheon.
/// @param w         World holding the tiles. Read-only.
/// @param tile_ids  Raster-order tile IDs (index = row * gw + col).
/// @param gw, gh    Grid dimensions (columns wrap; rows do not).
/// @param target_regions  How many regions to aim for; the political pass's
///                  own seed budget, so the two agree by construction. Clamped
///                  to what the land can actually hold at the separation rule.
/// @param seed      Per-body seed, already folded with the campaign seed.
/// @param stop_year Calendar year the pass generates AT (BL-271). 1960 runs the
///                  full arc. Below 1700: regions founded after it are dropped
///                  (not yet founded), Stage 4 never runs (no furnace has lit by
///                  antiquity), and demography is seeded at founding then grown
///                  to `stop_year` (the graduation path the region struct
///                  names as BL-271's job).
/// @param sim_start_year The year `run_history_sim` will begin at. Regions the
///                  colonisation walk dates AFTER it are not placed in
///                  `regions`; they go to `pending_foundings` for the sim to
///                  found as their year arrives, so the world FILLING is inside
///                  the recorded span rather than before it (BL-846).
///                  Defaults to `INT64_MAX`, which schedules nothing and
///                  reproduces the pre-BL-846 behaviour exactly — every caller
///                  that does not ask for a schedule is unaffected.
settlement_state run_settlement(const planetology_state& pl,
                                const history_ladder_state& hl,
                                const creed_state& cs,
                                const world& w,
                                const std::vector<entity_id>& tile_ids,
                                int gw, int gh,
                                int target_regions,
                                uint32_t seed,
                                int64_t stop_year = 1960,
                                int64_t sim_start_year = INT64_MAX);

/// The region anchors, as raster indices, in placement order — the seed list
/// `nation_params::seed_tiles` takes. This is the whole of "seeding changes,
/// expansion does not": the Voronoi/BFS growth machinery is untouched, it just
/// starts from places people actually settled.
std::vector<int> settlement_seed_tiles(const settlement_state& ss);

/// BL-769 — THE HISTORY'S POLITICAL MAP, in the shape `generate_nations` reads.
///
/// Parallel to `settlement_seed_tiles` entry for entry (same filter, same
/// order): the id of the polity that held each anchored region at the epoch, or
/// -1 for ground no polity ended up holding. Phase 5 folds the regions of one
/// polity into one nation instead of growing an independent realm out of every
/// anchor — which is the whole of BL-769's "finalise what the history produced,
/// rather than invent it".
///
/// MUST BE CALLED BEFORE `derive_national_character`, which overwrites
/// `region::nation` with the nation index. Pure; no RNG.
std::vector<int> settlement_seed_polities(const settlement_state& ss);

/// BL-750 — each nation's tariff posture, 0-1000, indexed by nation index.
///
/// Reads `region::protection_q` (the polity's, broadcast at the end of the sim)
/// over the regions each nation ended up holding, and takes the MAXIMUM. Under
/// BL-769's polity fold a nation's regions all carry the same value and the max
/// is that value; the max is what keeps the answer a deterministic total where
/// the size-floor merge has folded two polities together — the more protective
/// history is the one the merged realm inherits.
///
/// MUST BE CALLED AFTER `derive_national_character`, which is what puts the
/// nation index in `region::nation`. Pure; no RNG.
std::vector<int> derive_national_protection(const settlement_state& ss, int nation_count);

/// Attribute every region to the nation that ended up holding it, compute the
/// border-contest integral, and DERIVE the three political axes from the
/// settlement record (BL-218 § 2). Overwrites whatever Pass 4 drew.
///
/// Appends the Stage 4 industrialisation lines, which can only be attributed to
/// a nation once one exists — the same two-entry-point shape the ladder uses.
///
/// @param nation_ids  Nation entity ids in generation order (generate_nations'
///                    return value); regions store indices into this list.
void derive_national_character(settlement_state& ss,
                               const creed_state& cs,
                               world& w,
                               const std::vector<entity_id>& nation_ids,
                               const std::vector<entity_id>& tile_ids,
                               int gw, int gh);

/// The historical-rupture checkpoint class (BL-218 § 2a) — collapse, war and
/// revolution as TRANSFORMS on nation state, never as narration alone.
///
///  * **Collapse** — the nation's peripheral regions are lost to their
///    nearest neighbour and their industrial clock resets; abundance falls.
///  * **War** — the contested border redraws toward the stronger; the loser's
///    expansionism rises (grievance); the victor's gods are planted on the
///    regions taken; and PART OF THE LOSER'S RECORD IS DESTROYED, replaced
///    by a dated lacuna.
///  * **Revolution** — territory untouched; the ideology axis flips; abundance
///    takes a one-off hit.
///
/// Branch eligibility is a FILTER, never a weight (BL-217's rule): a nation
/// with no land neighbour cannot go to war, a single-region nation cannot
/// collapse. Every attempt appends a `checkpoint_record`, failures included.
///
/// Mutates @p ss (records, redaction, region culture) and @p w (tile
/// ownership, nation character, resource abundance).
void resolve_historical_ruptures(settlement_state& ss,
                                 const creed_state& cs,
                                 world& w,
                                 const std::vector<entity_id>& nation_ids,
                                 const std::vector<entity_id>& tile_ids,
                                 int gw, int gh,
                                 uint32_t seed);

/// Index of the region whose anchor is nearest (col,row), or -1 when there
/// are none. Column-wrapped; ties break on the lowest region index.
int nearest_region(const settlement_state& ss, int col, int row, int gw);

/// BL-219's derivation: a corporation anchored in @p p operates at the tier its
/// region's history earned. The ancient endowment sets the base class; the
/// **movement up the value chain** — early industrialisers refined, late ones
/// stayed raw — shifts it one tier when the region industrialised before the
/// world median.
///
/// @param p        The corporation's home region.
/// @param median   `settlement_state::median_industrial_year` (0 = nobody did).
industrial_focus focus_from_region(const region& p, int64_t median);

// ---------------------------------------------------------------------------
// Demography (BL-273) — population growth, war/plague drawdown, manpower.
//
// Feeds the eventual Era -1 history sim (BL-271, not built by this item) and,
// on graduation to the 1960 era, the population-centre scale distribution
// (POPULATION.md) — neither consumer is wired up here; this is the
// region-level model only, correct and self-contained per the item's own
// scope note.
//
// INTEGER FIXED-POINT THROUGHOUT. Every rate below is a "_q" quantity in
// thousandths (1000 = 1.0), following PLANETOLOGY.md's "no floats in a gate
// path" convention — population feeds manpower, which is itself a budget
// other deterministic systems will draw against, so it is a gate path too.
// ---------------------------------------------------------------------------

/// The population a region's farm endowment can support, in raw headcount
/// — "the ground feeds who it can feed" (BL-273 design). A linear function of
/// `farm_q` (0-1000, world-relative per settlement.cpp's `score_against`) off
/// a subsistence floor, so even a farm_q=0 region holds a small population.
///
/// NOT industrialisation-aware in this first cut (BL-273 design: carrying
/// capacity "does NOT need to move with industrialisation" yet) — a future
/// item can add an industrial-era term without changing this signature.
int64_t region_carrying_capacity(int farm_q);

/// The same ceiling raised by a region's WORKS (BL-321). `capacity_mod_q` is
/// per-mille — a Granary at +180 feeds 18% more people off the same ground.
///
/// This is the promised industrialisation-aware term the one-argument overload
/// says a future item can add "without changing this signature", and it is
/// added exactly that way: the old signature still exists and still means what
/// it meant, so every caller that has no works to account for is untouched.
/// Clamped at the bottom so a (currently impossible) negative modifier cannot
/// drive the ceiling under the subsistence floor.
int64_t region_carrying_capacity(int farm_q, int capacity_mod_q);

/// Advance one region's population by `years` simulated years: logistic
/// growth toward `region_carrying_capacity(p.farm_q)`, war drawdown scaled
/// by `war_pressure_q`, then a manpower-stock replenishment pass. Integer
/// fixed-point throughout; no RNG — growth/drawdown are not a checkpoint
/// draw (only `resolve_plague_event` is, per the design's explicit call-out).
///
/// Pure function of `p`'s own fields and the two arguments: two calls with
/// identical inputs produce identical outputs, so a caller replaying the
/// same (years, war_pressure_q) sequence over the same starting region
/// reproduces the same trajectory — the determinism the sim harness checks.
///
/// @param p               Region to advance, mutated in place.
/// @param years            Simulated years this step covers; <= 0 is a no-op.
/// @param war_pressure_q   0-1000, this step's battle intensity on `p` (0 =
///                         no war). The caller derives it from combat/
///                         checkpoint records — this function only spends it.
void advance_region_demography(region& p, int years, int war_pressure_q);

// ---------------------------------------------------------------------------
// The urban record (BL-766) — cities at sim grain
// ---------------------------------------------------------------------------

/// The headcount one sim-grain population centre stands on. Deliberately the
/// SAME rung the campaign-era carve counts centres by
/// (`k_demography_heads_per_centre`, population_generation.hpp) so a region
/// that stood up three centres during the era materialises three at the epoch;
/// population_generation.cpp static_asserts the two against each other, since
/// two copies of a rung is how they drift apart.
inline constexpr int64_t region_centre_heads = 10000;

/// Hard ceiling on one region's centre count. Structural, not tuning: the
/// campaign-era carve caps the body total at 65,536 and a runaway region
/// should hit a named bound rather than eat that budget silently.
inline constexpr int region_centre_limit = 32;

/// The headcount `run_history_sim` seeds an unpopulated region with, and the
/// figure `draw_urban_map` sizes its seed cities against. ONE derivation, read
/// by both — the sim's seeding line and the urban draw have to agree or the
/// map is drawn against a population that never arrives.
int64_t region_seed_population(int farm_q);

/// The share of a region's people who live in its centres, per mille. Rises
/// with `farm_q`: a surplus is what feeds a town, so easy-farming ground
/// towns a larger fraction of itself than ground that barely feeds its own
/// farmers. This is Ben's "extra attention to areas where farming would be
/// easy" at region grain (the tile-grain half is the placement weight in
/// population_generation.cpp).
int region_urban_share_q(int farm_q);

/// Draw ONE region's opening urban record from its farming ground: the map's
/// rule for a single region. Applied at the opening draw and again at every
/// founding the Era -1 sim makes, so a frontier region settled in year 300
/// gets its settlement on the same terms as one settled before the sim began.
void draw_region_urban(region& p);

/// DRAW THE POPULATION MAP (BL-766). Runs over a settled body BEFORE the Era
/// -1 sim: every region whose ground clears the farming floor is given an
/// opening urban headcount and the centres those heads stand up, so the sim
/// runs over a world that already has cities in it.
///
/// PURE — no RNG, no seed. A deterministic consequence of `farm_q` and the
/// region's own population, per the generation layer's standing shape
/// (consequences of upstream scalars, not dice). Idempotent: running it twice
/// produces the same map.
void draw_urban_map(settlement_state& s);

/// Advance one region's urban headcount by one simulated year: converge a
/// fraction of the gap toward `population * region_urban_share_q(farm_q)`,
/// then promote `centres` to whatever the surviving heads stand up —
/// PROVIDED the network still reaches this ground.
///
/// @param network_ok  BL-872 (CIVILISATION.md "Centres are derived by supply
///                    and governance") — whether `region::network_supply_q`
///                    is still above the caller's sustainable-settlement
///                    floor. True is the old behaviour unchanged. False
///                    FREEZES `centres`: it neither grows nor shrinks here,
///                    because a network cut is not the deliberate act of
///                    history `sack_region_urban` exists for.
///
/// GROWTH ONLY PROMOTES, and only while the network holds. A shrinking city
/// keeps its centre and a cut-off one keeps its centres too — POPULATION.md's
/// asymmetry, now covering both kinds of passive failure. Destruction is
/// `sack_region_urban` alone, a deliberate act of history.
void advance_region_urban(region& p, bool network_ok);

/// SACK a region's cities. `population_loss_q` is the per-mille the
/// countryside lost; the city loses a multiple of it, because a sack falls on
/// the walls and not the fields. Centres fall to what the surviving heads can
/// stand, and every one lost is recorded in `centres_razed`.
void sack_region_urban(region& p, int population_loss_q);

/// The manpower ceiling a region's CURRENT population can support — a
/// bounded fraction (`manpower_ceiling`'s own constant), not additive, so a
/// region cannot bank more than its living population could ever field.
int64_t manpower_ceiling(int64_t population);

/// The same ceiling raised by a region's WORKS (BL-321) — an Arsenal at +320
/// lets the same population field 32% more. Per-mille, clamped non-negative.
int64_t manpower_ceiling(int64_t population, int manpower_mod_q);

/// Move `manpower_stock` a fraction of the way toward its ceiling. Called
/// once per `advance_region_demography` step (after growth/drawdown update
/// `population`) — recovery is gradual, not an instant snap-to-ceiling, so a
/// region that just spent its levy stays weak for a few years, not one.
void replenish_manpower(region& p);

/// Raise up to `want` manpower from `p.manpower_stock`. Returns the amount
/// actually raised: `0 <= raised <= min(want, manpower_stock)`, so raising
/// past what is available is bounded rather than going negative or
/// unbounded — the self-limiting close BL-273 asks for (ancient hegemonies
/// stall on manpower exhaustion rather than being capped by fiat).
int64_t raise_manpower(region& p, int64_t want);

// ---------------------------------------------------------------------------
// The army pool (BL-835) — armies are distinct from population
//
// Three calls, and between them they are the whole model: what size of army
// this ground keeps under arms, one year of raising or disbanding toward it,
// and spending the army when a battle goes against it. NONE of the three
// reads or writes `population`. That is the invariant the item exists for and
// `demography_harness` asserts it directly.
//
// The tuning lives in the CALLER (`history_sim_params`) rather than as
// constants here, because the Era -1 sim is the only consumer with a view on
// how militarised an ancient polity should be, and a second consumer would
// want a different answer.
// ---------------------------------------------------------------------------

/// The standing army this region's people can keep under arms — a fraction of
/// the recruitable manpower ceiling, NOT of the population directly. It is a
/// second bound below `manpower_ceiling`, so a region always keeps a reserve
/// of eligible civilians it has not called up.
///
/// @param p                    The region; reads `population` and `work_manpower_mod`.
/// @param garrison_fraction_q  Per-mille of the manpower ceiling to hold under arms.
int64_t garrison_target(const region& p, int garrison_fraction_q);

/// BL-955 — the paid standing heads inside `p.army_stock`: `standing_army`
/// while the ground is still held by the polity that paid for it, clamped to
/// the pool; 0 otherwise. Always 0 in the Empire span.
inline int64_t standing_army_heads(const region& p)
{
    if (p.standing_army <= 0 || p.nation != p.standing_army_owner || p.army_stock <= 0) return 0;
    return p.standing_army < p.army_stock ? p.standing_army : p.army_stock;
}

/// BL-955 — after `p.army_stock` fell from @p stock_before, lose the paid
/// standing heads IN PROPORTION: a loss falls on paid and mustered men alike.
/// The paid count is read against the pool as it stood BEFORE the loss
/// (never the shrunken pool, which would destroy paid men twice), and the
/// result is always <= `army_stock`. A no-op wherever there is no standing
/// army (the whole Empire span).
inline void scale_standing_army(region& p, int64_t stock_before)
{
    const int64_t raw = (p.standing_army > 0 && p.standing_army_owner == p.nation) ? p.standing_army : 0;
    const int64_t s   = raw < stock_before ? raw : stock_before;
    if (s <= 0 || stock_before <= 0 || p.army_stock <= 0) { p.standing_army = 0; return; }
    if (p.army_stock >= stock_before) { p.standing_army = s < p.army_stock ? s : p.army_stock; return; }
    p.standing_army = (s * p.army_stock) / stock_before; // s <= stock_before, so <= army_stock
}

/// BL-955 — draw @p take heads out of `p.army_stock` (clamped to the pool)
/// for a march, losing the paid heads in proportion. Returns the PAID heads
/// drawn, which never exceed the heads drawn.
inline int64_t draw_army_with_standing(region& p, int64_t take)
{
    const int64_t stock_before = p.army_stock;
    if (take <= 0 || stock_before <= 0) return 0;
    if (take > stock_before) take = stock_before;
    const int64_t paid_before = standing_army_heads(p);
    p.army_stock -= take;
    scale_standing_army(p, stock_before);
    const int64_t drawn = paid_before - p.standing_army;
    return drawn < 0 ? 0 : (drawn > take ? take : drawn);
}

/// BL-955 — call after writing `p.nation`: ground that changed hands keeps
/// none of its previous holder's paid heads, so a region that is lost and
/// retaken within one round cannot revive a stale paid count.
inline void void_stale_standing_army(region& p)
{
    if (p.standing_army_owner != p.nation) p.standing_army = 0;
}

/// BL-955 — the raw invariant: paid heads never exceed the pool, and a paid
/// count only stands on ground its payer holds.
inline bool standing_army_invariant_holds(const region& p)
{
    return p.standing_army <= p.army_stock
        && (p.standing_army <= 0 || p.standing_army_owner == p.nation);
}

/// ONE YEAR of the muster. Below target, close `muster_rate_q` per-mille of
/// the shortfall by drawing from `manpower_stock` — which is what raising an
/// army COSTS, and the reason recovery is slow: the pool itself only refills
/// at `replenish_manpower`'s rate off a population that war never touched.
/// Above target (the ground can no longer feed the host it is carrying, after
/// a plague or a lost hinterland), `disband_rate_q` per-mille of the excess
/// goes home — and it goes home to `manpower_stock`, not to `population`,
/// because a discharged soldier was never subtracted from the civilian count
/// in the first place.
///
/// Pure in `p` and its three arguments; no RNG. Deterministic and idempotent
/// per call, so a caller replaying the same year sequence replays the same
/// muster (`world_determinism`'s requirement, and the sim runs this over every
/// region of every year).
void muster_garrison(region& p, int garrison_fraction_q,
                     int muster_rate_q, int disband_rate_q);

/// Spend `lost` heads off `army_stock`, bounded at zero. Returns the number
/// actually spent, which is less than `lost` when the army was already smaller
/// than its casualties — the caller's loss figure is a per-mille of a
/// COMMITTED stack and this pool is the region's whole army, so the two can
/// disagree and the bound is the honest answer rather than a negative pool.
int64_t spend_army(region& p, int64_t lost);

// ---------------------------------------------------------------------------
// Materials and labour (BL-867) — CIVILISATION.md § Materials are spent when
// something happens
//
// THE THREE-WAY SPLIT, WITHOUT A THIRD FIELD. Subsistence is not modelled as
// a share to spend, because it already has a home: `manpower_ceiling` IS the
// ground's non-subsistence surplus — the population minus whatever it takes
// to feed itself — so a region's people are, by construction, either
// growing food (the untouched remainder of `population`), standing under
// arms (`army_stock`, drawn from the ceiling), or making things (the
// ceiling's remainder below). The equilibrium Ben asked for — "a high
// population province must reserve population for work in industry to feed
// the settlements" — is exactly `manpower_ceiling` doing double duty as the
// budget both muster and industry draw against: a garrison mustered to its
// full ceiling leaves nothing over, which is "starves or stops producing"
// arriving as arithmetic on fields that already existed, never as a term
// invented for this item alone.
// ---------------------------------------------------------------------------

/// Heads this region has left for INDUSTRY once the standing muster has
/// taken its share of the recruitable surplus (`manpower_ceiling`). Floored
/// at zero: a garrison holding the whole ceiling under arms leaves nothing,
/// it does not go negative and it does not touch `population`.
int64_t region_industry_capacity(const region& p);

/// Per-mille yield the ground's own ORE endowment gives that labour —
/// `ore_q`, because ore is materials' domain (history_sim.hpp's own mapping:
/// "ore to materials, energy to energy"). Zero ore is zero yield: the labour
/// is there, there is simply nothing under it to work.
int region_industry_yield_q(const region& p);

/// One simulated year of this region's industrial output, in material
/// units — the quantity a caller credits to `seat_region`'s `material_stock`.
/// Pure function of `p`'s own fields; this function has no view of the rest
/// of `settlement_state::regions` so it does not do the crediting itself.
int64_t region_industry_output(const region& p);

/// Resolve one plague-event checkpoint over a body's settled regions,
/// reusing `resolve_checkpoint`'s class-agnostic mechanism from
/// planetology.hpp exactly as BL-217 named it to be reused a second time —
/// eligibility is a FILTER, never a weight. Each candidate branch is a
/// possible epicentre region; a region is ELIGIBLE only if it still has
/// population to strike (`population > 0`).
///
/// SIMPLIFICATION, noted per the design's own fallback clause: severity uses
/// a GRID-PROXIMITY connectivity proxy (nearby regions by `col`/`row`, via
/// the same Chebyshev distance settlement.cpp already computes) rather than
/// reading the full logistics/trade graph — that graph is built at a later
/// generation stage than settlement and is not reachable here without
/// pulling in the logistics module, which the design's fallback explicitly
/// allows trading for cheapness. A true trade-linked read is a future
/// refinement, not this item's job.
///
/// Mutates `regions` in place (population loss at the epicentre and its
/// grid neighbours) and appends exactly one `checkpoint_record` per attempt
/// to `checkpoints` (failed/ineligible attempts included, per BL-217's rule).
///
/// @param regions    The body's regions; indices double as branch ids.
/// @param checkpoints  Receives one record per attempt (see BL-217 shape).
/// @param seed         Per-body seed (recorded as the checkpoint's seed_used).
/// @param stage_tag    This call's RNG sub-stream tag — vary per plague-check
///                      opportunity so repeat calls draw independently.
/// @param gw            Grid width, for the column-wrapped distance metric.
/// @return true once a branch was applied and cleared its floor; false if
///         every region was ineligible (nobody left to strike) within the
///         attempt budget.
bool resolve_plague_event(std::vector<region>& regions,
                          std::vector<checkpoint_record>& checkpoints,
                          uint32_t seed, uint32_t stage_tag, int gw);
