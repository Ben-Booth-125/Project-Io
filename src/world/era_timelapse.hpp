#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// era_timelapse — the Era -1 ownership replay, as a standalone record
// ---------------------------------------------------------------------------
//
// WHY THIS HEADER EXISTS, and it is a dependency argument rather than a design
// one. The time-lapse is produced by `run_history_sim` (history_sim.hpp) and
// consumed by the History ledger's Ages view through `generation_report`
// (hard_coded_world.hpp). Those two headers must not meet: hard_coded_world.hpp
// says so in its own words, refusing to include history_sim.hpp because "this
// header's several hundred includers should [not] have to pay for" it.
//
// So the shared vocabulary lives here instead, in a header with NO dependencies
// beyond <cstdint> and <vector>. `owner_change` was moved out of history_sim.hpp
// rather than copied — a duplicated wire type is exactly the drift BL-462 was
// written about.
//
// WHAT CHANGED WITH IT (NR-733, Ben's ruling 2026-08-30). The Ages view used to
// RE-RUN the era every time it was opened, which made it a seventh caller of an
// invocation `era_minus_one.hpp` exists to keep singular, and it diverged from
// generation on all six of BL-462's axes. Three of those were closable at the
// call site; the other three were not, because the report carries the settlement
// AFTER the sim mutated it. Replaying a recorded timeline closes all six at once
// by deleting the second caller: generation records what happened, and the view
// shows that, so there is no second invocation left to drift.

/// Reserved for a region leaving all ownership. NOTHING EMITS IT: once settled
/// or conquered a region always has an owner, and no path resets one to unowned.
/// It is kept because `owner_slice_at` needs a value for regions that do not
/// exist yet in an early year, and because depopulation-to-abandonment is a
/// plausible later mechanic — a reader should not infer from the sentinel that
/// abandonment exists (BL-312).
inline constexpr uint16_t owner_none = 0xFFFFu;

/// One ownership change: region `region` came under polity `owner` in year
/// `year`.
///
/// THIS IS THE WHOLE TIME-LAPSE FORMAT. A change LIST rather than a per-year
/// grid, because ownership is overwhelmingly static — most years, on most
/// regions, nothing happens, and a dense grid pays for all of it.
struct owner_change
{
    int32_t  year   = 0;
    uint16_t region = 0;
    uint16_t owner  = 0;
};

// ---------------------------------------------------------------------------
// The playback record (BL-817)
// ---------------------------------------------------------------------------
//
// OWNERSHIP WAS ONE THIRD OF A TIME-LAPSE. `owner_change` answers "who held
// this ground", which draws the spreading colour; it cannot draw the
// LEADERBOARD beside it (military might, population, land held) and it cannot
// show a conquest being DIGESTED. Those are the other two thirds, and they are
// what `GENERATION_STRATEGY.md` § Round 4's arc and § What pass 1 hands back
// actually ask for.
//
// THE CADENCE IS THE RECORDED STEP, and it is deliberately three grains coarse:
//   year          -- what demography advances on
//   decision step -- what a polity acts on (`step_for_year`, 1..100 years)
//   RECORDED STEP -- what this record samples on
// A recorded step is taken on a decision round, and only once
// `history_sim_params::record_interval_years` (20 by default) have passed since
// the last one, plus one final step at the stop year. So the record is never
// finer than the sim's own decisions — sampling between two rounds would record
// the same numbers twice — and a 4000-year run yields ~200 steps rather than
// 4000. A per-year record is the trap the ownership list already avoided once.
//
// IT MUST NOT PERTURB THE SIM IT RECORDS, and that is a property of what the
// recorder is allowed to touch rather than a promise in a comment: it READS the
// ownership vector, the region table and the polity table, it WRITES only into
// the three vectors below, no field here is read back by any decision, none
// feeds a draw, and it consumes no randomness. `history_sim_params::
// record_playback` turns it off, and the harness asserts a recorded run and a
// suppressed run agree bit-for-bit in every other output — the same discipline
// `trace_battles` is held to. Unlike `trace_battles` the default is ON, because
// generation is the consumer, not a harness.

/// Culture-share slots carried per region. MIRRORS `culture_shares::
/// culture_share_slots` (settlement.hpp), and cannot include it: this header
/// keeps zero dependencies beyond <cstdint>/<vector> for the reason the top of
/// the file argues. `save_roundtrip` and the sim harness both bind the two
/// together, so a divergence is caught rather than assumed away.
inline constexpr int timelapse_culture_slots = 3;

/// A region's culture mix as it stood at a recorded step — the flattened wire
/// form of `culture_shares`.
///
/// DELTA-ENCODED, exactly like `owner_change`, and for the measured version of
/// the same reason. A dense grid costs regions x recorded steps whether or not
/// anything moved, and almost nothing moves: a region is founded pure and stays
/// pure forever unless it is conquered, and only the conquered tail assimilates.
/// A change list pays for the drift and nothing else. The alternative was
/// considered and rejected on that arithmetic, not on taste — see the harness,
/// which prints both figures.
///
/// One entry means: at `year`, region `region` held these shares, and they
/// differ from the last shares recorded for it. Ascending by year, and within a
/// year ascending by region.
struct culture_change
{
    int32_t year   = 0;
    uint16_t region = 0;
    int16_t  id[timelapse_culture_slots]       = {-1, -1, -1}; ///< Culture index, -1 unused.
    int16_t  weight_q[timelapse_culture_slots] = {0, 0, 0};    ///< Per-mille, descending.
    int16_t  other_q = 1000;                                   ///< The unattributed tail.
};

/// One living polity at one recorded step — the scoreboard substrate (BL-830).
///
/// A SERIES, not an endpoint, and that is the whole reason it is stored per
/// step: an ordered top-16 that RE-RANKS as the time-lapse runs needs the value
/// at each step, and a final standing cannot be re-ranked at all.
///
/// `regions` and `population` are the two land/people axes § Round 4's arc names;
/// the two capacities are the profile axes — MILITARY is "might" and MATERIALS
/// is the industrial rung the same sim reads for `polity::industrial_year`, so
/// the scoreboard and the furnace agree on what a polity's development is.
struct polity_sample
{
    int64_t  population   = 0; ///< Civilian headcount over every region held.
    uint16_t polity       = 0; ///< `polity::id`.
    uint16_t regions      = 0; ///< Regions held at this step.
    uint8_t  cap_military = 0; ///< `polity::capacity[military]`, band 1-6.
    uint8_t  cap_materials= 0; ///< `polity::capacity[materials]`, band 1-6.
    /// BL-1080 — the industry points standing on every region this polity
    /// holds at this step (`region::industry_points`, summed). What the
    /// Industrialisation span actually computes about industry, recorded as it
    /// stands: it is credited only inside that span (INDUSTRIALISATION.md
    /// § Beat 1), so it is zero on every step of the three earlier records. A
    /// READ of the sim like every other field here, never read back by it.
    /// save_game_version 21.
    int64_t  industry_points = 0;
};

/// One recorded step: a year, and the half-open span of `samples` taken at it.
///
/// AN INDEX RATHER THAN A NESTED VECTOR, so the whole record is three flat
/// arrays that serialise without a nested length per step and replay by a
/// straight walk. Living polities come and go, so `count` varies by step.
struct timelapse_step
{
    int32_t year         = 0;
    int32_t first_sample = 0; ///< Index into `era_timelapse::samples`.
    int32_t sample_count = 0; ///< Samples belonging to this step.
};

// ---------------------------------------------------------------------------
// The event layer (BL-916)
// ---------------------------------------------------------------------------
//
// THE MOMENTS OF THE ARC ARE PART OF THE TIME-LAPSE, not outcomes read off at
// the end (Ben, 2026-09-11; CIVILISATION.md § The arc is watched, not read off
// at the end). The three records above can show WHERE the colour moved and WHO
// led; they cannot say WHY a colour flipped. A secession and a conquest are the
// same ownership change on the map, and a realm ending is a polity simply
// missing from the next step's samples — which is how "0 destroyed" was read
// off a world in which a dozen powers had fallen.
//
// So the sim also records the NAMED MOMENTS, typed, at the sites where it
// already pushes a prose line. Delta / append only, ascending by year, and a
// pure record in the sense every other array here is: nothing in world/* reads
// an event back, no field feeds a decision or a draw, and the list is suppressed
// together with the playback record by `record_playback` so the harness can
// hold a recorded and a suppressed run bit-identical on every other output.
//
// INTEGER ONLY and five fields wide, so the whole list is a flat array that
// serialises with no strings in it. Names are resolved on the READ side from the
// region index — the region table already carries a generated, in-world name
// for every region, and a polity is named by its seat there too.

/// What happened. The `other` field's meaning depends on the kind; see each.
/// Values are the wire form — append only, never renumber.
enum class lapse_event_kind : uint8_t
{
    founded             = 0, ///< A polity came into being; `region` = its seat.
    seat_captured       = 1, ///< A seat changed hands by conquest; `other` = the loser.
    realm_ended         = 2, ///< A polity lost its last ground; `other` = the killer.
    broke_away          = 3, ///< A successor seceded; `other` = the parent polity.
    capital_moved       = 4, ///< A polity re-seated; `other` = the OLD capital region.
    road_promoted       = 5, ///< A corridor crossed a tier; `region`/`other` = its ends, `polity` = the tier.
    civilisation_formed = 6, ///< Two peoples settled a shared way of life; `other` = the civilisation index.
    creed_preached      = 7, ///< A universal creed arose; `other` = the creed index.
    culture_split       = 8, ///< The migration coined a daughter people; `polity` = the daughter culture, `other` = its parent culture.
    supply_site_upgraded = 9, ///< BL-929: a region bought its own reach relief outright; `polity` = the buyer.
    trade_link_opened   = 10, ///< BL-925: a cross-border corridor turned amicable; `region`/`other` = its ends.
    trade_link_closed   = 11, ///< BL-925: a grudge shut a cross-border corridor; `region`/`other` = its ends.
    treaty_formed       = 12, ///< BL-933: a bound pair took a term; `polity`/`other` = the two parties.
    treaty_broken       = 13, ///< BL-933: a treaty was broken; `polity` = the defector, `other` = the wronged party.
    subject_bound       = 14, ///< BL-934: `polity` = the native subject, `other` = its new overlord.
    subject_freed       = 15, ///< BL-934: a subject refused renewal; `polity` = the subject, `other` = the former overlord.
    schism              = 16, ///< BL-944: a reasserted people broke away over creed, not reach; `other` = the parent polity.
    furnace_lit         = 17, ///< BL-1080: a region crossed the furnace (`region::industrial_year`); `polity` = its holder. A resumed span notes the crossings it inherits, dated at their own (earlier) year, ahead of its first event (save_game_version 21).
    province_bought     = 18, ///< BL-1096: a native was BOUGHT rather than taken (EXPLORATION.md sec Two ways to claim ground across water); `region` = the native seat, `polity` = the native, `other` = the buyer. Noted INSTEAD of `subject_bound` for that binding (save_game_version 22).
    sea_lane_opened     = 19, ///< BL-1097: a sea leg's uses crossed `sea_lane_tier1_uses`; `region`/`other` = its ends (lo, hi), `polity` = the tier (1). The water analogue of `road_promoted` (save_game_version 22).
    // 20 is `inherited`, lane I1's (sprint 47): the value is left to that lane so
    // no two lanes append the same byte; the gap stands until it lands.
    works_chartered     = 21, ///< BL-1099: a region's `industry_points` crossed the next multiple of `works_event_fraction_q` x the RUNNING charter price (the world's stock so far over `k_stockpile_price_divisor`); `region` = the works' region, `polity` = its holder, `other` = the `industrial_focus` a firm chartered there takes (`focus_from_region`). RECORD-ONLY: no point is debited, at most `works_event_region_cap` per region per span (save_game_version 22).
    count
};

/// No party in this slot — a founding has no killer, a cradle culture no parent.
inline constexpr uint16_t lapse_event_none = 0xFFFFu;

/// One named moment. Ascending by year in `era_timelapse::events`.
struct lapse_event
{
    int32_t  year   = 0;
    uint8_t  kind   = 0;                ///< `lapse_event_kind`, as its wire byte.
    uint16_t region = lapse_event_none; ///< Where — the region the moment is pinned to.
    uint16_t polity = lapse_event_none; ///< Whose — the acting or affected polity (a tier / culture for two kinds).
    uint16_t other  = lapse_event_none; ///< The counterparty, per kind.
};

/// The recorded Era -1 ownership history of one body: everything the Ages view
/// needs to replay it, and nothing else.
///
/// EMPTY IS MEANINGFUL and is the common case. Generation runs the era for at
/// most ONE body (the cradle), and only when `era_minus_one_enabled` — so every
/// other body carries an empty record, which the view reads as "never settled"
/// rather than as missing data.
struct era_timelapse
{
    std::vector<owner_change> changes;   ///< Ascending by year; the replay substrate.
    int32_t region_stride = 0;           ///< Final region count — the slice width.
    int32_t start_year    = 0;           ///< First simulated year (negative = BCE).
    int32_t years         = 0;           ///< Years simulated, so last = start + years.

    /// THE PLAYBACK RECORD (BL-817) — the other two thirds of a time-lapse.
    /// `steps` is ascending by year and indexes into `samples`; `culture_changes`
    /// is the delta-encoded mix history. All three are empty on a run with
    /// `record_playback` off, and on every body the era never ran for.
    std::vector<timelapse_step>  steps;
    std::vector<polity_sample>   samples;
    std::vector<culture_change>  culture_changes;

    /// THE EVENT LAYER (BL-916) — the named moments, ascending by year. Empty
    /// with `record_playback` off and on every body the era never ran for; the
    /// migration record carries `culture_split` alone.
    std::vector<lapse_event>     events;

    /// THE NAME TABLE (BL-1106) — the names prose needs that no region can
    /// give. A `civilisation_formed` event's `other` indexes
    /// `civilisation_name`, a `creed_preached` event's `other` indexes
    /// `creed_name`, and `polity_creed` is polity id -> the creed that realm
    /// adopted (-1 = none), so a `schism` line can name the creed the seceding
    /// people walked out on — the parent's, which no event carries. Filled by
    /// `as_timelapse` from the sim's own lists, IN INDEX ORDER, so a resumed
    /// span's numbering continues (history_sim.hpp's civilisation/creed copy);
    /// read by the ticker and by nothing in world/*. Every name is coined by
    /// the sim from a generated tongue — never an Earth name. Empty on the
    /// migration record and on every body the era never ran for
    /// (save_game_version 22).
    std::vector<std::string> civilisation_name;
    std::vector<std::string> creed_name;
    std::vector<int32_t>     polity_creed;

    /// Ownership alone. The playback record can be present with no ownership
    /// change ever recorded, and a caller replaying colour wants to know about
    /// exactly that, so this stays the ownership question it always was.
    bool empty() const { return changes.empty(); }

    /// Bytes the three playback arrays occupy — the quantity BL-817 bounds.
    /// Excludes `changes`, which `owner_ring_bytes` already reports, so the two
    /// figures add rather than overlap.
    int64_t playback_bytes() const
    {
        return static_cast<int64_t>(steps.size())           * static_cast<int64_t>(sizeof(timelapse_step))
             + static_cast<int64_t>(samples.size())         * static_cast<int64_t>(sizeof(polity_sample))
             + static_cast<int64_t>(culture_changes.size()) * static_cast<int64_t>(sizeof(culture_change));
    }

    /// Bytes the event layer occupies — the quantity BL-916 bounds, disjoint
    /// from the two figures above so the three add.
    int64_t event_bytes() const
    {
        return static_cast<int64_t>(events.size()) * static_cast<int64_t>(sizeof(lapse_event));
    }
};

/// Materialise the ownership map as it stood in `year`.
///
/// Folds every change up to and including `year`; `changes` is in year order, so
/// the walk stops at the first one past it. A region that has not appeared yet
/// reads `owner_none`.
std::vector<uint16_t> owner_slice_at(const era_timelapse& t, int64_t year);

// ---------------------------------------------------------------------------
// The live tap (BL-914) — render while it computes, not after
// ---------------------------------------------------------------------------
//
// UNTIL NOW A PASS ROUND WAS COMPUTED SILENTLY and only handed to the renderer
// once `std::async` landed the whole `era_timelapse` in one piece — "it plays
// the moment it lands: the run was the wait" (STARTUP.md's old wording). Ben,
// 2026-09-11: the wizard should draw the map moving from the first frame, at a
// fixed pace, always behind the year the pass has actually reached.
//
// THE SAME CONTRACT THE MARKET CARVE USES (hard_coded_world.hpp's
// `generation_progress`): a pure, WRITE-ONLY tap. The worker thread appends to
// it exactly as it appends to its own real record and nothing here is ever
// read back by the sim — no field feeds a decision, a branch or a random draw
// — so a watched run and an unwatched run take identical paths and are
// byte-identical. Every producer takes `era_lapse_tap*` defaulted to null and
// treats null as "publish nothing"; the harness and every headless caller pass
// nothing and pay not even the cost of a lock.
//
// A MUTEX RATHER THAN THE CARVE'S BARE ATOMICS, and that is a frequency
// argument, not a style one: the carve republishes a single grid cell tens of
// thousands of times a run, so a lock per publish would be the bottleneck. A
// time-lapse publishes on recorded steps and discrete events — at most a few
// thousand times across a 4000-year run — so a short critical section around a
// `vector::push_back` costs nothing measurable and is far easier to reason
// about correctly than a lock-free double buffer of growing vectors would be.
// `<mutex>`/`<atomic>` are standard-library, not a project header, so this
// keeps era_timelapse.hpp's real promise — no history_sim.hpp, no
// hard_coded_world.hpp — while paying for the lock only where one is used.
//
// APPEND-ONLY ON BOTH SIDES, exactly like the vectors it mirrors: the worker
// never rewrites or truncates what it already published, so the reader can
// track "how much of each list have I already copied" by size alone and copy
// only the new tail on every snapshot.
//
// REGION GEOMETRY RIDES ALONG TOO, and this is a considered widening past the
// three lists BL-914's own design names, not an oversight. A political map
// cannot be drawn from ownership alone: `finish_history_lapse` (history_lapse.
// hpp) needs every region's tile position and name before it can assign a
// single tile, and those positions do not exist anywhere the renderer can
// already see them — `settlement_state` is worker-local and never crosses the
// thread boundary until the whole future lands. Without republishing it here,
// "the map moving in the first second" is not implementable at all, only
// "the year counter moving in the first second" is. Regions are append-only
// for the life of a run (Settle only ever adds one — see history_sim.cpp), so
// this is the same tail-copy contract as the three lists above, at columns/
// rows/names instead of changes.
struct era_lapse_tap
{
    mutable std::mutex mutex;

    std::vector<owner_change>   changes;         ///< Mirrors `history_sim_state::owner_changes`.
    std::vector<culture_change> culture_changes;  ///< Mirrors `history_sim_state::culture_changes`.
    std::vector<lapse_event>    events;           ///< Mirrors `history_sim_state::events`.

    // Region geometry — see the widening note above. Parallel arrays, indexed
    // exactly as `settlement_state::regions` is; `region_col`/`region_row` are
    // `region::col`/`::row`, `region_name` is `region::name`.
    std::vector<int32_t>     region_col;
    std::vector<int32_t>     region_row;
    std::vector<std::string> region_name;

    int32_t start_year   = 0;  ///< Year of the first publish this run.
    int32_t year_reached = 0;  ///< The highest year folded into the three lists above.
    bool    started      = false;

    /// Bumped after every publish, under the lock. The renderer polls this
    /// WITHOUT the lock (`epoch_now`) and only pays for the copy in `snapshot`
    /// when the count has moved since its last read — a still frame (no
    /// publish since last poll) costs one relaxed atomic load.
    std::atomic<uint32_t> epoch{0};

    /// Worker side (called from `world/*` only). `all_*` are the sim's own
    /// growing vectors; only the tail past what this tap already holds is
    /// copied in, so a publish costs O(new entries) rather than O(everything
    /// so far). `year` is the highest year folded into this publish — it must
    /// never regress across calls, mirroring the sim's own forward-only clock.
    void publish(const std::vector<owner_change>&   all_changes,
                 const std::vector<culture_change>&  all_culture_changes,
                 const std::vector<lapse_event>&     all_events,
                 int32_t                             year)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!started) { start_year = year; started = true; }
        for (std::size_t i = changes.size(); i < all_changes.size(); ++i)
            changes.push_back(all_changes[i]);
        for (std::size_t i = culture_changes.size(); i < all_culture_changes.size(); ++i)
            culture_changes.push_back(all_culture_changes[i]);
        for (std::size_t i = events.size(); i < all_events.size(); ++i)
            events.push_back(all_events[i]);
        if (year > year_reached || !started) year_reached = year;
        epoch.fetch_add(1, std::memory_order_release);
    }

    /// Worker side, region geometry. Called wherever a region is founded
    /// (settlement's own founding, and the per-year founding schedule inside
    /// `run_history_sim` — see history_sim.cpp). `all_col`/`all_row`/`all_name`
    /// are the caller's own flattened, append-only mirror of its region list;
    /// only the new tail is copied in, same contract as `publish`. Takes its
    /// own lock rather than folding into `publish`: geometry and ownership
    /// grow on different schedules (a founding happens far less often than a
    /// year advances), so a caller publishing one has no reason to pay for
    /// re-copying the other.
    void publish_regions(const std::vector<int32_t>&     all_col,
                          const std::vector<int32_t>&     all_row,
                          const std::vector<std::string>& all_name)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (std::size_t i = region_col.size(); i < all_col.size(); ++i)
            region_col.push_back(all_col[i]);
        for (std::size_t i = region_row.size(); i < all_row.size(); ++i)
            region_row.push_back(all_row[i]);
        for (std::size_t i = region_name.size(); i < all_name.size(); ++i)
            region_name.push_back(all_name[i]);
        epoch.fetch_add(1, std::memory_order_release);
    }

    /// Renderer side. Lock-free peek: has anything published since `last_seen`
    /// (a value this same function returned before)? Callers skip `snapshot`
    /// entirely when this comes back equal.
    uint32_t epoch_now() const { return epoch.load(std::memory_order_acquire); }

    /// Renderer side. Copies the whole record out under the lock, so a reader
    /// never observes a half-appended publish, and returns the epoch the copy
    /// was taken at (compare against a later `epoch_now()` to know it moved
    /// again). Deliberately a full copy, not a delta: the renderer's own
    /// playhead needs the whole prefix to redraw from, and a lapse tops out at
    /// a few thousand entries, so this is cheap next to one frame's render.
    uint32_t snapshot(std::vector<owner_change>&   out_changes,
                       std::vector<culture_change>& out_culture_changes,
                       std::vector<lapse_event>&    out_events,
                       std::vector<int32_t>&        out_region_col,
                       std::vector<int32_t>&        out_region_row,
                       std::vector<std::string>&    out_region_name,
                       int32_t& out_start_year, int32_t& out_year_reached) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        out_changes         = changes;
        out_culture_changes = culture_changes;
        out_events          = events;
        out_region_col      = region_col;
        out_region_row      = region_row;
        out_region_name     = region_name;
        out_start_year      = start_year;
        out_year_reached    = year_reached;
        return epoch.load(std::memory_order_relaxed);
    }

    // The name table (BL-1106) — `era_timelapse::civilisation_name` /
    // `creed_name` / `polity_creed`, so a live round names a civilisation or
    // a creed in the year it is coined rather than only once the future lands.
    // Its own pair of calls rather than a widening of `publish`/`snapshot`,
    // for the same reason `publish_regions` has its own: names are coined a
    // few dozen times a run, and a caller publishing a year has no reason to
    // re-copy them. The two name lists are append-only, tail-copied under the
    // same contract as every list above; `polity_creed` is not (a realm's -1
    // becomes an index the year it adopts), so it is copied whole — a few
    // hundred ints at most.
    std::vector<std::string> civilisation_name;
    std::vector<std::string> creed_name;
    std::vector<int32_t>     polity_creed;

    /// Worker side. `all_*` are the sim's own lists; only the tail past what
    /// this tap already holds is copied in. `all_polity_creed` is polity id ->
    /// adopted creed, copied whole.
    void publish_names(const std::vector<std::string>& all_civilisation_name,
                       const std::vector<std::string>& all_creed_name,
                       const std::vector<int32_t>&     all_polity_creed)
    {
        std::lock_guard<std::mutex> lock(mutex);
        for (std::size_t i = civilisation_name.size(); i < all_civilisation_name.size(); ++i)
            civilisation_name.push_back(all_civilisation_name[i]);
        for (std::size_t i = creed_name.size(); i < all_creed_name.size(); ++i)
            creed_name.push_back(all_creed_name[i]);
        polity_creed = all_polity_creed;
        epoch.fetch_add(1, std::memory_order_release);
    }

    /// Renderer side. Copies the name table out under the lock.
    void snapshot_names(std::vector<std::string>& out_civilisation_name,
                        std::vector<std::string>& out_creed_name,
                        std::vector<int32_t>&     out_polity_creed) const
    {
        std::lock_guard<std::mutex> lock(mutex);
        out_civilisation_name = civilisation_name;
        out_creed_name        = creed_name;
        out_polity_creed      = polity_creed;
    }

    /// Owner side, between runs (a fresh round, or a reroll). Never called from
    /// `world/*` — only the app resets a tap it owns, before handing a fresh
    /// pointer to the next worker.
    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        changes.clear();
        culture_changes.clear();
        events.clear();
        region_col.clear();
        region_row.clear();
        region_name.clear();
        civilisation_name.clear();
        creed_name.clear();
        polity_creed.clear();
        start_year   = 0;
        year_reached = 0;
        started      = false;
        epoch.store(0, std::memory_order_relaxed);
    }
};

// ---------------------------------------------------------------------------
// The ancient road record (BL-768)
// ---------------------------------------------------------------------------
//
// THE SECOND ERA -1 RECORD THAT CROSSES INTO GENERATION, and it sits here for
// exactly the dependency argument the top of this file makes about the first:
// it is produced by `run_history_sim` (history_sim.hpp) and consumed by
// `stamp_history_roads` (road_generation.hpp), and those two headers must not
// meet either — the road pass has no business carrying the sim's polity /
// combat / creed vocabulary, and a duplicated wire type is the drift BL-462 was
// written about. So this header is now the Era -1 records that cross into
// generation, plural, rather than the ownership replay alone.
//
// WHY A RECORD AT ALL, RATHER THAN ROADS LAID IN THE SIM. Three structural
// reasons, none of them incidental (GENERATION_STRATEGY.md § The eight phases):
// the sim has no write channel to the world — no `world&`, deliberately; its
// pathfinder is region-to-region over the neighbour graph and returns a COST,
// never a tile list; and `generate_roads`' node source does not exist until
// after the sim has run. There is also an idiom mismatch — the sim's decision
// path is integer fixed-point with no floats in it, while the modern road pass's
// tier gate and redundancy rationing are float. So the sim RECORDS the corridors
// it moved along and a pass immediately after it STAMPS them. What is recorded
// is a fact about the history — an army was supplied along this line, a founding
// party walked it — never a road the sim pretended to build.

/// ONE REGION-TO-REGION CORRIDOR THE HISTORY ACTUALLY USED, and how often.
///
/// PURE OBSERVATION, in the sense `battle_trace` is: nothing in the sim reads a
/// corridor back, no field feeds a decision or a draw, and a run with the record
/// suppressed would be byte-identical in every other output. Unlike the battle
/// traces it is NOT gated on `trace_battles`, because generation — not a
/// harness — is its consumer.
///
/// `a < b` always, and the vector is sorted ascending by (a, b): the stamping
/// pass walks it in that order, so which corridor wins an overlapping tile
/// cannot depend on the order the events happened to be appended in.
struct history_corridor
{
    uint16_t a    = 0; ///< Lower region index.
    uint16_t b    = 0; ///< Higher region index.
    int32_t  uses = 0; ///< Times a polity moved supply or settlers along it.
    /// BL-949 -- THE RUNG THE SIM ACTUALLY USED on this corridor when the record
    /// was folded at the run's close: 0 none, 1 Track, 2 Road, 3 Post Road
    /// (`history_sim_params::road_tier{1,2,3}_uses` read against the sim's
    /// LIVE count, not against `uses`). Carried rather than re-derived because
    /// the two differ by construction: `uses` is TRAFFIC — the walks — while
    /// a purchase (`try_build_post_road`, `try_upgrade_corridor`) sets the live
    /// count straight to its threshold without walking it, and a refused
    /// promotion holds the live count one short of a walk the record still
    /// counts. So the rung is read HERE, and `uses` stays throughput.
    uint8_t  tier = 0;
};

/// ONE SEA LEG THE HISTORY ACTUALLY CROSSED, and how often (BL-1097;
/// EXPLORATION.md sec The colonial tie is a sea lane). The water sibling of
/// `history_corridor`: `a < b` always, the table sorted ascending by (a, b)
/// with `uses` summed, so a stamping pass is order-independent by construction.
/// Three things write a use -- a wet campaign's crossing at its launch, a
/// purchase party's crossing, and the standing traffic between a metropole
/// and each subject it holds, one per decision round while the link stands.
/// No `tier`: a lane is EARNED BY TRAFFIC ONLY (`sea_lane_tier1_uses`), never
/// bought, so its rung is a pure function of `uses` and is not carried twice.
/// PURE OBSERVATION in the sense `history_corridor` once was: nothing in the
/// sim reads a leg back, and a run with the table suppressed would be
/// byte-identical in every other output.
struct sea_leg
{
    uint16_t a    = 0; ///< Lower region index.
    uint16_t b    = 0; ///< Higher region index.
    int32_t  uses = 0; ///< Crossings recorded between the two.
};

/// Where one region stood, and what its own history invested in MOVING things.
///
/// The flattened form of the two `region` fields `stamp_history_roads` needs,
/// passed as an array rather than as a `settlement_state` for the same reason
/// `sim_terrain_view` is flattened: the road pass has no use for the settlement
/// vocabulary, and a harness can build a synthetic corridor case out of two
/// plain structs instead of out of a whole settled world.
struct history_road_node
{
    int32_t col       = 0;
    int32_t row       = 0;
    /// `region::work_reach_mod` — the accumulated per-mille reach effect of the
    /// works this region raised. Every reach-bearing row of the Era -1 works
    /// roster (Way Station, Span Bridge, Cut Canal and their siblings) adds to
    /// it and nothing else does, so a non-zero value means "this ground built
    /// something for the road" without the road pass naming a single work row.
    int32_t reach_mod = 0;
};
