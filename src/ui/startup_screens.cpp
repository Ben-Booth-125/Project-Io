/// @file startup_screens.cpp
/// The app's entry screens (docs/ui/STARTUP.md): the main menu, the New World
/// wizard (BL-167), and the wizard's preview plumbing. Extracted verbatim from
/// app.cpp (BL-361); behaviour unchanged — these remain app member functions,
/// they simply live beside the other ui surfaces now.

#include "core/app.hpp"

#include <imgui.h>

#include "ui/detail_level.hpp"
#include "ui/foldout_column.hpp"     // foldout_scroll_child — BL-904's wizard-column scroll verb
#include "ui/generation_charts.hpp"
#include "ui/generation_wait.hpp"      // BL-1072: the one wait surface
#include "ui/generation_preview.hpp"
#include "ui/history_lapse.hpp"      // BL-829/BL-830: round 4's map and its board
#include "ui/presentation.hpp"       // ui::palette — the per-world nation/realm colour tables (BL-1089)
#include "world/colonisation.hpp"   // colonisation_start_year -- the Culture round's own first year (BL-919)
#include "world/era_timelapse.hpp"   // owner_slice_at — the whole replay substrate
#include "world/logistics.hpp"       // body_tile_grid — the load path's water mask (BL-1089)
#include "world/polity_identity.hpp" // the pure identity walk the load path re-derives with (BL-1089)

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

void app::open_new_world_wizard()
{
    // Nothing is generated yet — the wizard runs on a throwaway preview and only
    // commits when the player reaches its last round and presses "Begin".
    m_wiz_round = 0;
    m_wiz_dirty = true;
    m_screen    = app_screen::generating;

    // THE EMPIRES ROUND STARTS AT 400 BCE (BL-871, revising BL-846's 4000;
    // Ben, 2026-09-09). The start is `world_params::empires_start_year`, the
    // round's own fixed year (BL-1047); it used to be derived as
    // `epoch_year - prehistory_years`, which only landed on 400 BCE while the
    // epoch was 0 CE. `prehistory_years` is now the scope knob alone: any
    // positive value runs the era.
    //
    // NOT SIXTEEN HUNDRED YEARS, AND THAT IS A KNOWN GAP, NOT AN OVERSIGHT.
    // The design's full arithmetic is 400 BCE -> 1200 CE, 1,600 years — but
    // reaching 1200 CE needed an epoch past it when this was written, and this
    // wizard's epoch was 0 CE. (BL-906 later decoupled the Empires span from
    // the epoch, and the Exploration and Industrialisation rounds, BL-946 and
    // BL-1068, now play the spans past it.) CIVILISATION.md's own words: "sixteen hundred years is
    // the constraint on the phase going forward, not something to solve in
    // this item." What BL-871 owes is the SPLIT and the STARTING YEAR the
    // Culture round hands off at; the full depth is follow-on work once the
    // epoch moves.
    //
    // THE MIGRATION IS NOT COUNTED HERE. It used to be: the OLD figure (4000)
    // put the sim's own start at roughly 4000 BCE, so a single continuous run
    // covered colonisation AND conquest and both wizard rounds replayed it.
    // BL-871 splits them — the migration now runs to its own derived end year
    // (`colonisation_start_year`, colonisation.hpp, -2400) and the world
    // COASTS from there to 400 BCE holding what it left behind, so the sim
    // itself only ever needs to start where the Empires round does.
    //
    // THIS IS ALSO WHAT "BEGIN" BUILDS (unchanged from before BL-846): the
    // struct default already IS 400, so this line is written for clarity
    // rather than for a numeric change.
    m_pending_world_params.prehistory_years = 400;
}

void app::refresh_wizard_preview()
{
    // Preferences are not parameters, so they are resolved against the seed FIRST.
    // The resolution rejects and rerolls internally until the homeworld clears the
    // strict Earth-like floor, which is why the preview is always a world the
    // campaign could actually start on — and why it also reports what that cost
    // (resolved_world::attempts), which the round surfaces rather than hides.
    m_wiz_resolved = resolve_preferences(m_pending_world_params.preferences,
                                         m_pending_world_params.seed);
    // The system's coined catalogue for this seed (BL-257) — the wizard must
    // name the bodies the campaign will actually name them.
    m_wiz_names = generate_body_names(m_pending_world_params.seed);
    preview_system(m_wiz_resolved.params, m_wiz_resolved.home_orbit_au,
                   m_pending_world_params.seed, m_wiz_preview);

    // The Spend chart needs the endowment BEFORE the industrial drawdown. Drawdown
    // is the chain's last act and consumes no randomness, so a second run with the
    // dial at zero is the same world minus its industrial history — exactly the
    // "before" reference the hollow bars want.
    planetology_params undrawn = m_wiz_resolved.params;
    undrawn.drawdown = 0.0f;
    preview_system(undrawn, m_wiz_resolved.home_orbit_au,
                   m_pending_world_params.seed, m_wiz_undrawn);

    // The globe's real surface. Synchronous under --verify (a capture must never
    // race the worker); asynchronous in play, marked stale if already in flight.
    if (!m_golden_dir.empty())
    {
        world scratch;
        const entity_id probe = scratch.create_entity();
        const auto tiles = generate_home_surface_preview(scratch, probe,
                                                         m_pending_world_params);
        m_wiz_surface.resize(tiles.size());
        m_wiz_terrain.resize(tiles.size());
        for (std::size_t i = 0; i < tiles.size(); ++i)
        {
            const tile_component& tc = scratch.tiles.at(tiles[i]);
            m_wiz_surface[i] = ui::preview_pack(tc.substrate, tc.cover);
            m_wiz_terrain[i] = ui::pack_lapse_terrain(tc.landform, tc.river_edges,
                                                      tc.river_downstream);
        }
    }
    else if (m_wiz_surface_future.valid())
        m_wiz_surface_stale = true;
    else
        launch_wizard_surface_build();
}

void app::launch_wizard_surface_build()
{
    m_wiz_surface_future = std::async(std::launch::async,
        [params = m_pending_world_params]() {
            world scratch;
            const entity_id probe = scratch.create_entity();
            const auto tiles = generate_home_surface_preview(scratch, probe, params);
            ui::wizard_surface out;
            out.comp.resize(tiles.size());
            out.terrain.resize(tiles.size());
            for (std::size_t i = 0; i < tiles.size(); ++i)
            {
                const tile_component& tc = scratch.tiles.at(tiles[i]);
                out.comp[i]    = ui::preview_pack(tc.substrate, tc.cover);
                out.terrain[i] = ui::pack_lapse_terrain(tc.landform, tc.river_edges,
                                                        tc.river_downstream);
            }
            return out;
        });
}

namespace {

/// Lift the recorded era out of a finished generation report.
///
/// EVERYTHING THE ROUND DRAWS COMES FROM HERE, and it is all a reading: the
/// change list, the settled regions' positions and names, and generation's own
/// three era counters. Nothing is re-simulated and nothing is re-derived — see
/// `app::launch_wizard_history_run` for why that matters more than it looks.
///
/// @param lapse_index Which lapse round this is: 0 = Culture (the migration),
///                   1 = Empires, 2 = Exploration (BL-946), 3 =
///                   Industrialisation (BL-1068). Culture's owners are
///                   CULTURES and it therefore carries the lineage palette
///                   (BL-919); the later rounds' owners are polities and carry
///                   none. Exploration and Industrialisation each read their
///                   own recorded span (`exploration_timelapse` /
///                   `industrialisation_timelapse`) rather than the Empires
///                   round's `prehistory_timelapse`.
/// @param adopted    True when the report is the harness's own finished world
///                   rather than a run stopped at this round's end. A finished
///                   report's record is the Empires sim's, so the Culture round
///                   folds the migration's own out of the settlement instead —
///                   the same fold generation makes, on the same regions.
ui::history_lapse lapse_from_report(const generation_report& rep, int lapse_index,
                                    bool adopted)
{
    const bool migration         = lapse_index == 0;
    const bool exploration       = lapse_index == 2;
    const bool industrialisation = lapse_index == 3;

    ui::history_lapse h;
    h.peoples = migration; // BL-1106: the Culture board reads peoples / Homeland.

    // The homeworld by its authored flag, not by name or position: names are
    // generated and display-only (BL-257), and the body list can be reordered.
    const generation_report::body_entry* home = nullptr;
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld) { home = &b; break; }
    if (home == nullptr) return h;

    h.lapse  = industrialisation ? home->industrialisation_timelapse
             : exploration       ? home->exploration_timelapse
                                 : home->prehistory_timelapse;
    h.grid_w = home_grid_width;
    h.grid_h = home_grid_height;

    // BL-1087: THE ROUND'S FLAG, not the palette's emptiness, says which id
    // space `owner` is in — the lineage palette is built for every lapse now.
    h.owners_are_cultures = migration;

    if (migration)
    {
        const settlement_state& ss = home->settlement;
        if (adopted && !ss.regions.empty())
        {
            // The plurality on a finished report has drifted a little toward the
            // conquerors (culture_shares shifts SLOWLY), so this is the migration
            // as the sim left it rather than as it ended — an approximation the
            // adopt path already accepts for the sake of not running the pass
            // twice, and an honest one: every region still carries its people.
            h.lapse = build_migration_timelapse(ss, colonisation_start_year,
                                                ss.migration_end_year);
            // The counters follow the record, not the report: a finished
            // report's are the Empires sim's, and a migration is a diffusion
            // with no battles in it (the worker path reports the same).
            h.battles   = 0;
            h.conquests = 0;
            h.foundings = static_cast<int64_t>(ss.regions.size());
        }

    }

    // THE CULTURE TREE, rebuilt from what the report carries — FOR EVERY LAPSE
    // ROUND (BL-1087): the Culture round paints by it, and the polity rounds
    // read it for each realm's founding-family wedge and for the dull culture
    // base under the fill. The cradle cultures themselves are not in the
    // report — `creed_state` never crosses it — but every cradle is listed in
    // `cradle_coined_year`, and the daughters are `spawned_cultures` whole,
    // each naming its parent. Daughter ids run one past the last cradle
    // culture (BL-856), so the flat index is cradles first, daughters after,
    // in that order.
    {
        const settlement_state& ss = home->settlement;
        int32_t cradles = 0;
        for (const auto& [cid, year] : ss.cradle_coined_year)
            if (cid >= cradles) cradles = cid + 1;
        std::vector<int32_t> parent(static_cast<std::size_t>(cradles), -1);
        // BL-1017: a cradle never folds; a daughter carries its own fold.
        std::vector<int32_t> folded(static_cast<std::size_t>(cradles), -1);
        parent.reserve(parent.size() + ss.spawned_cultures.size());
        folded.reserve(parent.capacity());
        for (const culture& c : ss.spawned_cultures)
        {
            parent.push_back(c.parent);
            folded.push_back(c.folded_into);
        }
        // THE LIVING TREE (BL-1017): the settlement record's daughters arrive
        // folded, so the wheel is spent only on names that outlived the round.
        ui::build_lineage_palette(h, parent, &folded);
    }

    h.region_col.reserve(home->settlement.regions.size());
    h.region_row.reserve(home->settlement.regions.size());
    h.region_name.reserve(home->settlement.regions.size());
    for (const region& r : home->settlement.regions)
    {
        h.region_col.push_back(r.col);
        h.region_row.push_back(r.row);
        h.region_name.push_back(r.name);
    }
    // THE RECORD'S OWN REGIONS, NOT THE REPORT'S (BL-1087, the review's fix
    // round). A round's worker builds its record from a report STOPPED at the
    // round's close, so its region list is the regions at that close; a
    // record adopted from a FINISHED report (the --verify rounds, and every
    // record the Begin/load derivation walks) would otherwise raster over the
    // regions of 1960 — every region a later span founded — and a strip a
    // 1700 founding takes between two 1200 neighbours would drop their
    // adjacency, move a greedy offset, and colour a nation on load differently
    // from the round the player watched. `region_stride` is the region count
    // at the record's close, and region ids ascend in founding order, so the
    // first `region_stride` anchors ARE the round's own list.
    if (h.lapse.region_stride > 0
        && static_cast<std::size_t>(h.lapse.region_stride) < h.region_col.size())
    {
        const std::size_t n = static_cast<std::size_t>(h.lapse.region_stride);
        h.region_col.resize(n);
        h.region_row.resize(n);
        h.region_name.resize(n);
    }

    if (!(migration && adopted))
    {
        // BL-946: Exploration reads its OWN counters -- the Empires round's
        // battles/conquests/foundings describe a different span entirely, and
        // showing them on this round would misreport what it actually ran.
        // BL-1068: Industrialisation likewise, one span later.
        h.battles   = industrialisation ? rep.industrialisation_battles
                    : exploration       ? rep.exploration_battles   : rep.prehistory_battles;
        h.conquests = industrialisation ? rep.industrialisation_conquests
                    : exploration       ? rep.exploration_conquests : rep.prehistory_conquests;
        h.foundings = industrialisation ? rep.industrialisation_foundings
                    : exploration       ? rep.exploration_foundings : rep.prehistory_foundings;
    }
    return h;
}

/// BL-1087 (the cold review's fix round): A LANDED ROUND'S IDENTITY IS DERIVED
/// ON DEMAND AT THE HAND-OVER, whether or not the round was ever drawn. The
/// pins a round hands its successor (`lapse_pins_for_successor`) are read off
/// `polity_slot`, which only `finish_history_lapse` fills — and until this
/// helper that ran solely from the draw path of the round ON SCREEN. A player
/// who pressed Next while round 4 was still running, and never went back,
/// therefore had round 4 land unseen, un-derived, and hand round 5 EMPTY pins
/// at its landing: round 5 coloured itself from scratch, and the identity the
/// item exists for broke on an ordinary press order (the hard flag never
/// broke this way, because `lapse_hard_at_close` walks an unfinished record).
/// This is the same call the draw path makes, against the wizard's own packed
/// surface, so a record derived here and one derived by a frame are one
/// derivation. A no-op on a record already derived, and honest about a
/// surface not built yet: the pins are then empty and it says so, rather than
/// letting the successor colour from scratch in silence.
/// @param display_round The round as the wizard numbers it for the player
///                      (the lapse index plus the planetology rounds), for the log.
void derive_lapse_for_handover(ui::history_lapse& rec, int display_round,
                               const std::vector<uint8_t>& surface,
                               const std::vector<uint16_t>& terrain)
{
    if (rec.empty() || rec.derived()) return;
    ui::finish_history_lapse(rec,
                             surface.empty() ? nullptr : surface.data(), surface.size(),
                             terrain.empty() ? nullptr : terrain.data(), terrain.size());
    if (rec.derived())
        std::printf("[identity] round %d derived at the hand-over (landed, never drawn): "
                    "%zu realms slotted\n",
                    display_round, rec.polity_slot.size());
    else
        std::printf("[identity] round %d could not be derived at the hand-over: the wizard's "
                    "surface is not built (%zu of %d x %d tiles); its successor opens unpinned "
                    "until it is drawn\n",
                    display_round, surface.size(), rec.grid_w, rec.grid_h);
    std::fflush(stdout);
}

} // namespace

void app::launch_wizard_history_run(int lapse_index)
{
    // Out-of-range is a caller bug, not a display state: the array index below is
    // the one thing here that cannot be clamped away silently.
    if (lapse_index < 0 || lapse_index >= wizard_lapse_round_count) return;
    if (m_wiz_history_future[lapse_index].valid()) return; // already running on this round

    m_wiz_history[lapse_index]         = ui::history_lapse{};
    m_wiz_history[lapse_index].peoples = (lapse_index == 0); // BL-1106: the live Culture record too.
    m_wiz_history_playing[lapse_index] = false;
    m_wiz_history_paused[lapse_index]  = false;
    m_wiz_history_carry[lapse_index]   = 0.0f;

    // THE IDENTITY CARRIES BY ID from the round before, from the FIRST live
    // frame: the colour-slot PINS, the shade rungs and the dead ground
    // (BL-1087, `lapse_pins_for_successor`), the hard-border flag (BL-1090)
    // and the civilisation diamonds (BL-1094). The landing sets them all again
    // (`poll_wizard_history`), but the tap plays the record's opening years
    // long before the future lands, and the `--verify` adopted path below
    // never lands at all. Polity rounds only: the migration's owners are
    // cultures, and nothing is inherited across that boundary. The lineage
    // tree comes with them so the live phase can read a wedge before the
    // report lands (the landing rebuilds it from the report).
    const auto inherit_hard = [this, lapse_index]() {
        if (lapse_index <= 0) return;
        // A LANDED predecessor is derived on demand before its pins are read
        // (`derive_lapse_for_handover`): the player may never have drawn it.
        // A predecessor still RUNNING is left to the draw path — its record is
        // partial, and the landing below re-pins this round from the whole.
        if (!m_wiz_history_future[lapse_index - 1].valid())
            derive_lapse_for_handover(m_wiz_history[lapse_index - 1],
                                      lapse_index - 1 + wizard_planetology_round_count + 1,
                                      m_wiz_surface, m_wiz_terrain);
        const ui::history_lapse& prev = m_wiz_history[lapse_index - 1];
        if (prev.empty()) return;
        ui::history_lapse& cur = m_wiz_history[lapse_index];
        // The tree crosses EVERY boundary, the Culture -> Empires one included:
        // without it the Empires round's live phase would colour by the
        // fallback table and recolour every realm the moment the record landed.
        if (cur.culture_wedge.empty() && !prev.culture_wedge.empty())
        {
            cur.culture_family = prev.culture_family;
            cur.culture_hue    = prev.culture_hue;
            cur.culture_depth  = prev.culture_depth;
            cur.culture_colour = prev.culture_colour;
            cur.culture_wedge  = prev.culture_wedge;
            cur.family_count   = prev.family_count;
        }
        if (prev.owners_are_cultures) return; // no pins, no flags across the culture boundary
        cur.pins     = ui::lapse_pins_for_successor(prev);
        cur.has_pins = !cur.pins.slot.empty();
        cur.hard_carry = ui::lapse_hard_at_close(prev);
        cur.civ_carry  = ui::lapse_civ_marks_at_close(prev); // BL-1094
    };
    inherit_hard();
    // Sentinel, not 0: a signed calendar year of 0 is a real year (0 CE), so
    // it cannot double as "never parked yet". `poll_wizard_history_tap`'s
    // first live update and `poll_wizard_history`'s landing both snap this to
    // the record's own start_year on sight of it (BL-914) — the same "parks
    // at its first year" this member's own comment always promised, just no
    // longer forced to happen ONLY at landing.
    m_wiz_history_year[lapse_index]    = INT32_MIN;

    // BL-914: the tap for a fresh run. Reset FIRST, before anything can
    // publish into it, then pointed at from `prog` — a worker started below
    // reads `prog.lapse_tap` once, at its own leisure, and by then it is
    // already this round's tap and already empty.
    era_lapse_tap& tap = m_wiz_history_tap[lapse_index];
    tap.reset();
    m_wiz_history_tap_seen[lapse_index]       = tap.epoch_now();
    m_wiz_history_tap_redraw_at[lapse_index]  = 0.0;

    // The wait's own content. Cleared FIRST so no frame can read the previous
    // run's pass split as this one's — the same ordering begin_new_game keeps.
    generation_progress& prog = m_wiz_history_progress[lapse_index];
    // BL-1072: every field the wait reads, and the elapsed clock's start.
    // `stage_count` is published below, once this round's stop is known
    // (BL-1053): it is the stages the round's run will report, not a table size.
    prog.begin_wait();
    prog.lapse_tap = &tap; // BL-914: null-safe in run_history_sim/make_hard_coded_world.

    // UNDER --verify, ADOPT THE WORLD THE HARNESS ALREADY BUILT. run_verify opens
    // in_game on a generated world, so `m_generation_report` already holds this
    // very record — running the pass a second time would cost a Debug harness
    // minutes to reproduce a record it is already holding, and would produce the
    // same one. Nothing is faked: it is generation's report either way.
    if (!m_golden_dir.empty())
    {
        ui::history_lapse adopted = lapse_from_report(m_generation_report,
                                                      lapse_index,
                                                      /*adopted=*/true);
        if (!adopted.empty())
        {
            m_wiz_history[lapse_index]      = std::move(adopted);
            m_wiz_history_year[lapse_index] = m_wiz_history[lapse_index].lapse.start_year;
            inherit_hard(); // BL-1090: the adopted record carries the flag too
            return;
        }
    }

    // Lua and the works table are read on THIS thread before the worker starts:
    // sol2 is not thread-safe, and `m_works` is generation's input (BL-321).
    m_lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(m_lua);
    ensure_works_loaded();

    // STOP WHERE THIS ROUND'S OWN SPAN ENDS, AND NO FURTHER (BL-871, extended
    // to a third span by BL-946). The three lapse rounds are no longer one
    // fused pass replayed thrice: round 3 (Culture, lapse_index 0) wants the
    // migration's own record and must stop BEFORE the Empires round's history
    // sim ever starts; round 4 (Empires, lapse_index 1) wants that sim's
    // record and stops once IT has run, before the Exploration span or
    // borders/roads/companies are computed and thrown away; round 5
    // (Exploration, lapse_index 2) wants ITS OWN span's record and stops once
    // it has run, before borders, roads and companies — stages 9-12 — are
    // computed and thrown away (measured 10,805 ms of 11,316, about 95% of
    // the wait, and why the round visibly hung on "Laying roads", Ben,
    // 2026-09-09).
    //
    // Note this is set on the COPY the worker takes, never on the campaign's:
    // `begin_new_game` builds a whole world from its own config, and a world
    // stopped at any of these points has no nations, roads or corporations in it.
    //
    // BL-1040: `stop_after_exploration` also keeps the Industrialisation span out
    // of round 5's run (generation gates the span on it), so this round plays
    // Exploration's record alone whatever `industrialisation_span_enabled` says.
    //
    // ROUND 6 (Industrialisation, lapse_index 3, BL-1068) SETS NO STOP AT ALL.
    // It runs the FULL build -- the Industrialisation span, then borders,
    // roads, companies and the rest -- because that is the world "Begin"
    // builds, and a round that has already built it is the one a later item
    // can hand to Begin rather than building it twice. The span runs because
    // nothing here sets `stop_after_exploration` and the pending params carry
    // `industrialisation_span_enabled` unchanged (on by default). And
    // `stop_after_industrialisation` stays unset: stopping at the 1960 close
    // would throw away exactly the world a full run exists to keep -- and it
    // IS kept now: see the world cache below (BL-1073).
    world_gen_config hist_cfg = cfg;
    if (lapse_index == 0)      hist_cfg.stop_after_migration   = true;
    else if (lapse_index == 1) hist_cfg.stop_after_ancient_era = true;
    else if (lapse_index == 2) hist_cfg.stop_after_exploration = true;

    // BL-1053: the stages this run will report (7 for the Culture round, 8
    // for Empires and Exploration, the full count for Industrialisation's
    // unstopped build), published before the worker starts so the
    // total never reads as the label table's size. Generation restates it.
    prog.stage_count.store(generation_stage_count(hist_cfg), std::memory_order_relaxed);

    // BL-1073 -- THE LAST ROUND KEEPS ITS WORLD. Round 6's run is the full build
    // Begin would make (same params, this same config, the same works), so its
    // world and report are kept in a slot the app owns and Begin adopts them
    // (STARTUP.md § The world cache). A fresh run supersedes whatever was held:
    // the old cache is released now, the new one lands with the record. Rounds
    // 3-5 stop early and their half-built worlds are still discarded -- a world
    // stopped at a round has no nations, roads or companies.
    std::shared_ptr<wizard_world_cache> slot;
    if (lapse_index == wizard_lapse_round_count - 1)
    {
        drop_wizard_world("round 6 is running its build again");
        slot = std::make_shared<wizard_world_cache>();
        slot->params = m_pending_world_params;
        // BL-1085: ROUND 6 DOES ALL OF BEGIN'S WORK. After its build the worker
        // runs `finish_campaign_world` -- the search, the winner, the twelve-
        // tick settle -- on the registry it is handed: loaded from Lua HERE, on
        // the main thread (sol2 is not thread-safe), and COPIED into the slot,
        // the `m_works` pattern. The worker bands the copy from the world it
        // built; Begin moves it into m_registry, so play runs on the registry
        // the settle ran on. The finish's two steps join the bar's plan before
        // the build starts (generation adds `weight_after` into its total).
        load_recipe_registry();
        slot->registry = m_registry;
        prog.weight_after.store(finish_campaign_weight_ms(m_pending_world_params),
                                std::memory_order_relaxed);
        m_wiz_world_pending = slot;
    }

    auto run = [this, hist_cfg, lapse_index, slot, params = m_pending_world_params]() {
        generation_report rep;
        world w = make_hard_coded_world(params, &rep, hist_cfg,
                                        &m_wiz_history_progress[lapse_index], &m_works);
        ui::history_lapse lapse = lapse_from_report(rep, lapse_index, /*adopted=*/false);
        if (slot)
        {
            // THE SAME CALL BEGIN'S COLD WORKER MAKES, in the same order
            // (STARTUP.md § Handoff), so an adopted world and a cold build open
            // the campaign on one state hash. The record above was read off
            // the report BEFORE the finish, which reads the report and never
            // writes it.
            slot->finish = finish_campaign_world(w, rep, slot->registry, params, hist_cfg,
                                                 &m_wiz_history_progress[lapse_index]);
            // MOVED, never copied (see wizard_world_cache). `ready` is the
            // worker's last write; the future landing is what publishes it.
            slot->w      = std::move(w);
            slot->report = std::move(rep);
            slot->ready  = true;
        }
        return lapse;
    };

    if (!m_golden_dir.empty())
        m_wiz_history_future[lapse_index] = std::async(std::launch::deferred, run);
    else
        m_wiz_history_future[lapse_index] = std::async(std::launch::async, run);

    // A deferred future never becomes ready on its own, so a capture path
    // resolves it here and now rather than spinning forever in poll.
    if (!m_golden_dir.empty())
        poll_wizard_history();
}

void app::drop_wizard_world(const char* why)
{
    // The pending slot is released too: the worker filling it holds its own
    // reference, so its world is freed when that run's future is consumed, and
    // the landing finds no slot to cache.
    const bool held = (m_wiz_world != nullptr) || (m_wiz_world_pending != nullptr);
    m_wiz_world.reset();
    m_wiz_world_pending.reset();
    if (held)
    {
        std::printf("[wizard world] released: %s\n", why);
        std::fflush(stdout);
    }
}

void app::poll_wizard_history()
{
    // Every lapse round, not just the one on screen: a player who presses Next
    // while round 4 is still running must not strand its worker's result — the
    // future is adopted wherever the wizard has got to by the time it lands.
    for (int i = 0; i < wizard_lapse_round_count; ++i)
    {
        if (!m_wiz_history_future[i].valid())
            continue;
        if (m_golden_dir.empty()
            && m_wiz_history_future[i].wait_for(std::chrono::seconds(0))
                   != std::future_status::ready)
            continue;
        ui::history_lapse landed = m_wiz_history_future[i].get();
        // BL-1073: the future just landed, so round 6's slot is whole (or this
        // is a stale run whose world is dropped with its record, below).
        std::shared_ptr<wizard_world_cache> world_slot;
        if (i == wizard_lapse_round_count - 1)
            world_slot = std::move(m_wiz_world_pending);
        if (m_wiz_history_stale[i])
        {
            // The ground moved while this ran: it is a true history of a world the
            // player has already rerolled away from. Dropped rather than drawn.
            m_wiz_history_stale[i]   = false;
            m_wiz_history[i]         = ui::history_lapse{};
            m_wiz_history_playing[i] = false;
            if (world_slot)
            {
                std::printf("[wizard world] dropped on landing: its round went stale mid-run\n");
                std::fflush(stdout);
            }
            // RELAUNCHED ON LANDING (BL-1083, the review's fix round): the stale
            // run was the one a lean change or a reroll asked to replace, and it
            // could not be recalled while in flight. Its slot is free now, so the
            // fresh run the player already asked for starts here if they are still
            // on this round -- otherwise the round sat empty until they left and
            // came back, which read as the lean doing nothing.
            if (m_screen == app_screen::generating
                && i == m_wiz_round - wizard_planetology_round_count
                && !m_wiz_history_future[i].valid())
            {
                std::printf("[wizard] relaunching round %d after its stale run landed\n",
                            i + wizard_planetology_round_count + 1);
                std::fflush(stdout);
                launch_wizard_history_run(i);
            }
            continue;
        }
        m_wiz_history[i] = std::move(landed);
        if (world_slot && world_slot->ready)
        {
            m_wiz_world = std::move(world_slot);
            std::printf("[wizard world] cached for Begin (seed %u, era seed %u, "
                        "span seeds %u/%u/%u/%u): searched and settled, %zu corporations\n",
                        m_wiz_world->params.seed, m_wiz_world->params.era_seed,
                        m_wiz_world->params.span_seed[0], m_wiz_world->params.span_seed[1],
                        m_wiz_world->params.span_seed[2], m_wiz_world->params.span_seed[3],
                        m_wiz_world->w.corporations.size());
            std::fflush(stdout);
        }

        // CONTINUITY (Ben, 2026-09-16): a round opens on the ground the round
        // before it left. The predecessor's LAST frame is folded to one colour
        // per region and handed over, and the map paints it under
        // ground nobody holds yet, fading over the opening tenth of this span.
        // Taken at LANDING rather than at draw time because the predecessor's
        // own record is complete by then and never changes again — the carried
        // frame is a fact about a finished round, not a second surface to keep
        // in step. Round 3 has nothing behind it and carries nothing.
        // NOT gated on the predecessor having SAMPLE steps: the migration's
        // record carries ownership deltas and no polity samples at all (its
        // board has no People column for exactly that reason), and gating on
        // steps left the Culture round unable to hand anything over — the one
        // hand-over this was built for. owner_slice_at reconstructs from the
        // deltas, so a record with a span is enough.
        if (i > 0 && m_wiz_history[i - 1].lapse.years > 0)
        {
            // The predecessor's identity, derived on demand if no frame ever
            // drew it (`derive_lapse_for_handover`): the carried colours below
            // and the pins read `polity_slot`, which is empty on an undrawn
            // record. Only a LANDED predecessor — a running one is partial.
            if (!m_wiz_history_future[i - 1].valid())
                derive_lapse_for_handover(m_wiz_history[i - 1],
                                          i - 1 + wizard_planetology_round_count + 1,
                                          m_wiz_surface, m_wiz_terrain);
            const ui::history_lapse& prev = m_wiz_history[i - 1];
            const int prev_end = prev.lapse.start_year + prev.lapse.years;
            const std::vector<uint16_t> last = owner_slice_at(prev.lapse, prev_end);
            std::vector<uint32_t> cols(last.size(), 0u);
            int held = 0;
            for (std::size_t r = 0; r < last.size(); ++r)
                if (last[r] != owner_none)
                {
                    // BL-1087: the colour as it stood at the predecessor's
                    // close, the shade ratchet included.
                    cols[r] = static_cast<uint32_t>(ui::lapse_owner_colour(prev, last[r], prev_end));
                    ++held;
                }
            if (held > 0) m_wiz_history[i].carry_colour = std::move(cols);

            // AND THE REALMS KEEP THEIR IDENTITY ACROSS THE HAND-OVER (Ben,
            // 2026-09-16: the Exploration round "looks like it actually carried
            // over from culture"; ruled whole 2026-09-24, STARTUP.md § Identity
            // across the rounds). The first cut copied the predecessor's slots
            // over the landed record's — DEAD CODE, since `lapse_from_report`
            // never colours and `polity_slot` was empty here (BL-1087's finding).
            // Now the predecessor hands over PINS (`lapse_pins_for_successor`:
            // every slot it assigned, the rung each realm reached, the ground
            // its dead last held), the hard-border flag (BL-1090) and the
            // civilisation diamonds (BL-1094), all by id; the landed record is
            // re-derived so its own colouring keeps every pin, colours only the
            // rest, and its hysteresis walk starts where the last round left it.
            //
            // Polity rounds only: the migration's owners are CULTURES in their
            // own id space and its lineage palette is a different thing
            // entirely, so nothing is inherited across that boundary.
            if (!prev.owners_are_cultures)
            {
                ui::history_lapse& cur = m_wiz_history[i];
                cur.pins       = ui::lapse_pins_for_successor(prev);
                cur.has_pins   = !cur.pins.slot.empty();
                cur.hard_carry = ui::lapse_hard_at_close(prev);
                cur.civ_carry  = ui::lapse_civ_marks_at_close(prev); // BL-1094: the diamonds stay
                cur.tile_region.clear();
            }
        }

        // AND THE ROUND AFTER THIS ONE IS RE-PINNED, IF IT IS ALREADY OPEN
        // (BL-1087, the review's fix round). A player who pressed Next while
        // this round was still running has its successor live — or even
        // landed — on pins taken from a partial record, or from no record at
        // all. The landing is the first moment the whole record exists, so
        // the hand-over is made again here, from the landed record, exactly
        // as it is made for a successor that launches later; the successor
        // re-derives on its next frame. A running successor keeps its own
        // record (only its pins move); a landed one is re-coloured whole.
        // The chain stops at one: the successor's own successor is re-pinned
        // when the successor lands, from a record that is whole by then.
        // The successor is re-pinned whether or not its tap has published yet
        // (the second cold review): a rapid double-Next has the successor
        // launched but empty when this lands, and it must not wait for its own
        // landing to learn its pins.
        if (i + 1 < wizard_lapse_round_count
            && !m_wiz_history[i].owners_are_cultures)
        {
            derive_lapse_for_handover(m_wiz_history[i], i + wizard_planetology_round_count + 1,
                                      m_wiz_surface, m_wiz_terrain);
            const ui::history_lapse& prev = m_wiz_history[i];
            ui::history_lapse&       nxt  = m_wiz_history[i + 1];
            nxt.pins       = ui::lapse_pins_for_successor(prev);
            nxt.has_pins   = !nxt.pins.slot.empty();
            nxt.hard_carry = ui::lapse_hard_at_close(prev);
            nxt.civ_carry  = ui::lapse_civ_marks_at_close(prev);
            nxt.tile_region.clear();
            std::printf("[identity] round %d re-pinned from round %d's landing (it was already open)\n",
                        i + 1 + wizard_planetology_round_count + 1, i + wizard_planetology_round_count + 1);
            std::fflush(stdout);
        }

        // BL-914: LANDING NO LONGER RE-PARKS THE PLAYHEAD AT THE START. Under
        // the old design the future carried the whole record and this was the
        // first moment any of it was visible, so parking at the start was the
        // only sensible year. Now the live phase has usually already been
        // playing this very round for most of its 30 seconds — snapping back
        // to year one the instant the future resolves would look like the
        // transport lurching backwards at exactly the moment it should read as
        // seamless. So: clamp what is already there into the landed record's
        // range, and only fall back to its start_year for a round that was
        // never live-drawn at all (the `--verify`/adopted paths, whose year is
        // still the launch-time sentinel).
        // PARKED AT THE FIRST YEAR (Ben, 2026-09-16). BL-914 kept whatever
        // year the live phase had reached, because snapping back would have
        // looked like the transport lurching. There is no live phase any more —
        // the wait is a wait — so the record plays from its beginning, which is
        // also the only way its hand-over cross-fade is ever seen.
        const int lstart = m_wiz_history[i].lapse.start_year;
        const int lend   = lstart + m_wiz_history[i].lapse.years;
        m_wiz_history_year[i]  = lstart;
        m_wiz_history_carry[i] = 0.0f;
        if (m_wiz_history_year[i] > lend) m_wiz_history_year[i] = lend;

        // It plays the moment it lands (or keeps playing, if the live phase
        // already had it going): the run was the wait, and the playback is
        // what arriving on the round asked for. Frozen under --verify, where the
        // year is set by the script instead (verify.history_year).
        m_wiz_history_playing[i] = m_golden_dir.empty();
        m_wiz_history_paused[i]  = false;
    }
}

void app::poll_wizard_history_tap(int lapse_index)
{
    // Only a round whose worker is still running publishes anything new; once
    // landed, `poll_wizard_history` above owns the record wholesale, and a
    // round nobody has started yet has no tap worth reading either.
    if (lapse_index < 0 || lapse_index >= wizard_lapse_round_count) return;
    if (!m_wiz_history_future[lapse_index].valid()) return;
    // FROZEN UNDER --verify, same reason every other wizard animation is: a
    // capture must never race a live redraw, and a `--verify` run resolves its
    // (deferred) future synchronously before the first frame draws anyway, so
    // this branch would have nothing to do even without the guard.
    if (!m_golden_dir.empty()) return;

    era_lapse_tap& tap   = m_wiz_history_tap[lapse_index];
    const uint32_t epoch = tap.epoch_now();
    if (epoch == m_wiz_history_tap_seen[lapse_index]) return; // nothing new published

    // THROTTLED INDEPENDENTLY OF THE PUBLISH RATE. A founding or a recorded
    // step can publish a few thousand times across a run; re-deriving the
    // drawable map (`finish_history_lapse`'s BFS + terrain bake + polity
    // colouring) that often would cost far more than the animation it is for.
    // Redraws every ~0.2 s regardless of how many publishes landed in between
    // — always the LATEST snapshot, never a queued backlog of frames.
    const double now = ImGui::GetTime();
    if (now < m_wiz_history_tap_redraw_at[lapse_index]) return;
    m_wiz_history_tap_redraw_at[lapse_index] = now + 0.2;

    ui::history_lapse& rec = m_wiz_history[lapse_index];
    int32_t start_year = 0, year_reached = 0;
    m_wiz_history_tap_seen[lapse_index] = tap.snapshot(
        rec.lapse.changes, rec.lapse.culture_changes, rec.lapse.events,
        rec.region_col, rec.region_row, rec.region_name,
        rec.lapse.polity_name, // BL-1088: the name table as it grows
        start_year, year_reached);
    // BL-1106: the name table, so a live ticker names a civilisation or a
    // creed in the year it is coined rather than only once the future lands.
    tap.snapshot_names(rec.lapse.civilisation_name, rec.lapse.creed_name,
                       rec.lapse.polity_creed);

    if (rec.region_col.empty())
        return; // Geometry has not been published yet — nothing drawable this poll.

    const bool first_populate = (rec.grid_w == 0);

    rec.lapse.start_year    = start_year;
    rec.lapse.years         = std::max<int32_t>(0, year_reached - start_year);
    rec.lapse.region_stride = static_cast<int32_t>(rec.region_col.size());
    rec.grid_w = home_grid_width;
    rec.grid_h = home_grid_height;

    // Force `finish_history_lapse` to re-run: new regions and/or new ownership
    // widen the tile assignment and can move the polity adjacency graph, so
    // the whole one-shot derivation (tile_region, the terrain bake, the
    // palette) is invalidated rather than patched. See history_lapse.hpp;
    // `derived()` reads `tile_region` alone, so clearing it is sufficient.
    rec.tile_region.clear();

    // The playhead parks at the record's own first year the moment there is
    // anything to show at all — "arriving on the round IS the instruction to
    // run it" (STARTUP.md), now true of the FIRST live frame rather than only
    // of the moment the future eventually lands.
    if (first_populate)
        m_wiz_history_year[lapse_index] = start_year;
}

void app::poll_wizard_surface()
{
    if (!m_wiz_surface_future.valid())
        return;
    if (m_wiz_surface_future.wait_for(std::chrono::seconds(0))
            != std::future_status::ready)
        return;
    ui::wizard_surface built = m_wiz_surface_future.get();
    m_wiz_surface = std::move(built.comp);
    m_wiz_terrain = std::move(built.terrain);
    if (m_wiz_surface_stale)
    {
        // Preferences moved while that build ran: it is already the wrong
        // world, so go straight around again with the current params.
        m_wiz_surface_stale = false;
        launch_wizard_surface_build();
    }
}

void app::draw_main_menu()
{
    const ImVec2 disp = ImGui::GetIO().DisplaySize;

    // A dark, centred title card. Borderless, non-interactive-move window sized to
    // its contents; buttons carry the only input. Kept deliberately spare — this is
    // the launch entry point, not a settings hub (no Load/Save in the prototype).
    ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##main_menu", nullptr, flags))
    {
        // Title, centred over the button column.
        const char* title = "PROJECT IO";
        const char* tag   = "Near-future corporate 4X";
        auto centre_text = [&](const char* s, ImU32 col) {
            const float w = ImGui::CalcTextSize(s).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (280.0f - w) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, col);
            ImGui::TextUnformatted(s);
            ImGui::PopStyleColor();
        };
        centre_text(title, IM_COL32(225, 230, 240, 255));
        centre_text(tag,   IM_COL32(120, 128, 145, 255));
        ImGui::Dummy({0.0f, 18.0f});

        // --- New World setup (BL-114). Every widget edits m_pending_world_params,
        //     which the wizard continues to edit and start_new_game() finally
        //     consumes; each carries a unique ##id so it never collides with the
        //     centred buttons below. ---
        world_params& wp = m_pending_world_params;
        ImGui::SeparatorText("New World");

        // THE MENU OPENS ON A ROLLED SEED (BL-890; Ben, 2026-09-10). The
        // default of 0 made every new game the same reference world unless the
        // player thought to press Roll, which turns rerolling into a thing you
        // must know to do rather than the ordinary way in. STARTUP.md carries
        // the ruling.
        //
        // ROLLED HERE, AND ONCE, FOR TWO REASONS THE LIVE CHECK FOUND. Rolling
        // per frame would make the field impossible to type into. Rolling when
        // the WIZARD opens -- the first place this was tried -- silently threw
        // away a seed the player had just typed or pasted into this very field,
        // because New Game runs that path on the way out of this screen. The
        // latch keeps the draw idempotent: the number is fresh on arrival and
        // is then the player's.
        //
        // THE ENTROPY STOPS HERE, exactly as the Roll button's below already
        // does. world/* stays a pure function of the seed, so save, replay and
        // the multiplayer argument are untouched; seed 0 still names the
        // reference world, it is simply no longer what you get by accident.
        //
        // NOT UNDER --verify. Every scripted capture reaches this menu, and a
        // rolled seed would make each run a different world and turn the visual
        // suite non-deterministic. `m_golden_dir` non-empty is this file's own
        // test for "a harness is driving".
        if (!m_seed_rolled)
        {
            m_seed_rolled = true;
            if (m_golden_dir.empty())
            {
                std::random_device rd_seed;
                wp.seed = static_cast<uint32_t>(rd_seed());
            }
        }

        // Seed — hex entry + a one-shot randomise. The random_device draw feeds ONLY
        // the seed value; no entropy ever enters world generation, which stays a pure
        // function of this seed (same seed + knobs -> identical world).
        ImGui::TextUnformatted("Seed");
        ImGui::SetNextItemWidth(210.0f);
        ImGui::InputScalar("##seed", ImGuiDataType_U32, &wp.seed, nullptr, nullptr,
                           "%08X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::Button("Roll##seedroll", {64.0f, 0.0f}))
        {
            std::random_device rd;
            wp.seed = static_cast<uint32_t>(rd());
        }
        {
            // Copyable readout of the reproducible key.
            char seedhex[16];
            std::snprintf(seedhex, sizeof seedhex, "%08X", wp.seed);
            if (ImGui::Button("Copy seed##seedcopy", {280.0f, 0.0f}))
                ImGui::SetClipboardText(seedhex);
        }

        // Resource abundance — Earth-like 'Standard' is the ceiling; the leaner tiers
        // step down (GENERATION_STRATEGY.md § The resource ceiling).
        ImGui::TextUnformatted("Resources");
        int ab = static_cast<int>(wp.abundance);
        ImGui::RadioButton("Sparse##ab",   &ab, static_cast<int>(abundance_level::sparse));
        ImGui::SameLine();
        ImGui::RadioButton("Lean##ab",     &ab, static_cast<int>(abundance_level::lean));
        ImGui::SameLine();
        ImGui::RadioButton("Standard##ab", &ab, static_cast<int>(abundance_level::standard));
        wp.abundance = static_cast<abundance_level>(ab);

        // No nation knob: the number of nations on the home body is a consequence of
        // its landmass and the minimum-viable-territory floor, not a pre-set target
        // (docs/generation/NATION_GENERATION.md § Pass 1 / Pass 2c).

        // Bodies — the count knob is phased to a later update; shown disabled so the
        // intent reads without implying it works yet.
        ImGui::BeginDisabled();
        int bodies_stub = 5;
        ImGui::SetNextItemWidth(280.0f);
        ImGui::SliderInt("##bodies", &bodies_stub, 5, 5, "Bodies: %d (fixed)");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("A variable body count is coming in a later update.");

        // The Planetology knobs (BL-167) used to sit here as six sliders. They now
        // live in the New World wizard, one decision per chain stage, where each is
        // taken against a chart of the system as it stands — a slider whose effect
        // you cannot see is a slider you cannot judge.
        {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 280.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 145, 255));
            ImGui::TextUnformatted("The world's character is chosen during generation.");
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
        }

        ImGui::Dummy({0.0f, 12.0f});

        const ImVec2 btn = {280.0f, 40.0f};
        if (ImGui::Button("New Game", btn))
            open_new_world_wizard();
        ImGui::Dummy({0.0f, 6.0f});
        if (ImGui::Button("Quit", btn))
            m_quit_requested = true;
    }
    ImGui::End();
}

// ---------------------------------------------------------------------------
// The New World wizard (BL-167)
//
// THREE rounds, not ten stages. The chain still has ten links and the player is
// still shown every one of them — each round stacks its stages' charts and
// explanations into one scroll — but the decisions are batched into three
// thematic groups. Ben's call (2026-07-22): "we don't need so many rounds, it's
// just too slow - try to batch them together thematically."
//
// What the player sets is a PREFERENCE, not a parameter: a named lean per axis
// ("Dimmer", "Metal-rich"), resolved against the seed by resolve_preferences. No
// raw generated value is editable, and none is printed in the decision area — the
// charts above it show what the roll actually produced, which is the only honest
// feedback there is. "If you have preferences you can find them, but really you
// don't get full customization. Just a step more detail than a seeded world."
//
// NOTHING IS GENERATED HERE. resolve_preferences and preview_system run the whole
// chain over the prototype body set whenever a control moves; both are pure,
// throwaway, and never touch m_world. The world is built once, from the finished
// preferences, when the player presses "Begin" at the last round (start_new_game).
//
// Rounds are causal: rerolling round A re-draws B and C downstream. That is
// correct — the chain is causal too — and it is why Back is a plain revision with
// no per-round snapshot to keep.
//
// Charts are drawn with ui::charts — the primitives extracted from the tile
// selection graphs — so the wizard and the in-game surfaces share one visual
// language by construction rather than by imitation.
// ---------------------------------------------------------------------------

namespace {

// The stage explainers, the round table, and every stage chart moved out of this
// file into ui::generation_charts (src/ui/generation_charts.hpp), so the History
// ledger can redraw the same plots from the persisted generation_report. The
// wizard is no longer the only place the chain is ever visible (BL-211).

/// How many preference rows a round owns, and how many dim caption lines sit under
/// them. Both feed the height reserved for the decision block, which is pinned to
/// the bottom so the charts get everything left over.
/// File-local mirrors of app's round counts: those are private to `app`, and these
/// helpers are free functions. draw_generation_screen static_asserts the pair against
/// the real constants, so a drift here is a compile error, not a wrong layout.
constexpr int planetology_rounds = 2;  // System, Life (BL-863)
constexpr int pass_rounds        = 4;  // Culture, Empires, Exploration, Industrialisation (BL-946)
constexpr int lapse_rounds       = 4;  // every pass round replays a real record (BL-1068)

/// Empires' historical-turbulence caption, shared between the layout-height
/// estimate below and the actual draw call in the round switch, so the two
/// can never drift apart the way the fixed-line-count guess did (BL-904).
constexpr const char* kTurbulenceCaption =
    "Calm: peoples differ less, neighbours let a riser rise, and "
    "distance is cheap to hold. Turbulent: the warlike are more so, "
    "a riser draws a coalition, and an over-reached empire cannot "
    "feed what it took. It leans the forces - it sets no number of "
    "realms, and either setting can surprise you.";

/// Culture (round 2) and Empires (round 3) are PASS rounds, not planetology
/// rounds, but each still carries exactly one lean row of its own -- Drawdown
/// (moved here by BL-863) and the historical-turbulence lean (BL-839). The old
/// `r >= planetology_rounds` guard zeroed both out, under-reserving the layout
/// by one row and pushing each round's preference block and Next/Back footer
/// below the visible fold at 1080p (BL-904, found via history_lapse_press.lua
/// and round4_arc_reach.lua going red).
int round_pref_count(int r)
{
    if (r == 0) return 4;
    if (r == 1) return 3;
    if (r == 2 || r == 3) return 1;
    return 0;
}
int round_note_lines(int r)
{
    if (r == 1) return 3; ///< B carries the iron/coal caption.
    if (r == 0) return 1;
    // Culture's Drawdown carries no caption. Empires' turbulence caption is
    // far longer than a fixed line count can safely predict, so its height is
    // measured directly against kTurbulenceCaption where decide_h is built.
    return 0;
}

/// A round's header text. The planetology rounds take theirs from the shared chain
/// table (so the wizard and the History ledger name them identically); the three
/// pass rounds are not chain rounds and carry their own, from STARTUP.md
/// § Rounds 4, 5 and 6.
struct wizard_round_head
{
    const char* name;
    const char* question;
};

wizard_round_head wizard_round_head_at(int r)
{
    if (r < planetology_rounds)
    {
        const ui::chain_round& cr = ui::chain_round_at(r);
        return { cr.name, cr.question };
    }
    static const wizard_round_head passes[pass_rounds] = {
        // THE ROUNDS ARE NOT CONTINUOUS (Ben, 2026-09-09). Round 4 used to fuse
        // the peopling of the world with the empires that followed, and that cut
        // failed twice over: it drew conquest with the migration already finished
        // off-screen, then — once migration moved inside it — migration with no
        // conquest at all. Two subjects, two rounds.
        { "Culture",
          "Who reached this ground first, and by which routes?" },
        { "Empires",
          // THIS ROUND'S OWN SPAN, SEPARATE FROM ROUND 3's (BL-871). Before the
          // split both rounds replayed the same fused record — colonisation and
          // conquest run together — so round 4 opened with the whole map
          // already claimed and had nothing left to show but the tail of one
          // pass. Now round 3 stops at the migration's own end and round 4
          // starts the Empires sim at 400 BCE
          // (`docs/generation/CIVILISATION.md` § The span is 400 BCE to
          // 1200 CE), so this round watches conquest from a world that is
          // freshly peopled rather than one already settled off-screen.
          //
          // Titled to what it plays rather than left aspirational, on the rule
          // that a surface must not assert something the code has not delivered.
          "Who claimed this ground, and who lost it, in the age before the epoch?" },
        { "Exploration",
          // BL-946: the fourth pre-game round, on the same shared engine as
          // Empires (EXPLORATION.md sec The engine is shared) -- 1200 to 1660
          // CE, where conflict moves off the home coast and a treasury, a
          // fleet and a treaty become real.
          "Who reaches beyond this ground, and what do they bring back?" },
        { "Industrialisation",
          // BL-1068: the last pre-game span, 1660 to 1960 CE, on the same
          // shared engine -- and the round whose run builds the whole world
          // (docs/generation/INDUSTRIALISATION.md).
          "What does that ground produce, and who trades it?" },
    };
    int i = r - planetology_rounds;
    if (i < 0)             i = 0;
    if (i >= pass_rounds)  i = pass_rounds - 1;
    return passes[i];
}

/// One preference row: a name, then four segmented options with `Any` first.
///
/// Named leans only, and deliberately no number anywhere. A lean narrows the range
/// the seed is sampled from; it never pins a value. The moment a player can read
/// `1.0342` off this screen it is a settings form again rather than a preference,
/// which is the whole distinction the wizard is built on.
///
/// @return true when the player moved it, so the caller can mark the preview dirty.
bool lean_row(const char* id, const char* label, lean& value,
              const char* low, const char* mid, const char* high)
{
    static constexpr lean order[4] = { lean::any, lean::low, lean::mid, lean::high };
    const char* names[4] = { "Any", low, mid, high };

    // PushID scopes the four buttons to this row, so two rows sharing an option
    // name ("Balanced" appears under both Ocean and Oxygen) never collide.
    // Label on its own line, options in a 2x2 grid beneath — the wizard's
    // control column is a third of the panel now, and four radios in a row
    // ("Old and cold ... Young and vigorous") no longer fit one line.
    ImGui::PushID(id);
    ImGui::TextUnformatted(label);

    const float col2 = ImGui::GetContentRegionAvail().x * 0.5f;
    bool changed = false;
    for (int i = 0; i < 4; ++i)
    {
        if (i % 2 == 1) ImGui::SameLine(col2);
        int v = static_cast<int>(value);
        if (ImGui::RadioButton(names[i], &v, static_cast<int>(order[i])))
        {
            value   = order[i];
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

/// The turbulence lean's own row (BL-839), and NOT `lean_row` above.
///
/// THREE OPTIONS, NOT FOUR, and that is the whole reason this is a separate
/// function. `lean_row` leads with "Any", which means "sample the whole viable
/// range" -- honest for a planetology preference, which names a VALUE drawn
/// from a distribution. This axis names a FORCE (`world/history_sim.hpp` sec
/// THE HISTORICAL TURBULENCE LEAN), and there is no distribution of forces to
/// sample, so an "Any" the resolver would silently read as "Ordinary" would be
/// a control that lies about what it does.
///
/// NAMED SETTINGS RATHER THAN A SLIDER, for the reason STARTUP.md sec Rounds 4,
/// 5 and 6 gives the wizard as a whole: the player has to be able to tell what
/// they rolled. "Turbulent" is a thing you can look for in the arc readout
/// above; a per-mille dial on a force nobody can see is not.
bool turbulence_row(lean& value)
{
    static constexpr lean order[3] = { lean::low, lean::mid, lean::high };
    const char* names[3] = { "Calm", "Ordinary", "Turbulent" };

    ImGui::PushID("turbulence");
    ImGui::TextUnformatted("History");

    bool changed = false;
    for (int i = 0; i < 3; ++i)
    {
        if (i > 0) ImGui::SameLine();
        int v = static_cast<int>(value);
        // `any` can arrive here from an old save or a bare `world_params`; the
        // resolver reads it as ordinary, so the row shows it that way rather
        // than lighting nothing and implying a fourth state.
        if (value == lean::any) v = static_cast<int>(lean::mid);
        if (ImGui::RadioButton(names[i], &v, static_cast<int>(order[i])))
        {
            value   = order[i];
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

} // namespace

void app::draw_generation_screen()
{
    const ImVec2      disp  = ImGui::GetIO().DisplaySize;
    const ImGuiStyle& style = ImGui::GetStyle();

    // One reroll counter per PLANETOLOGY round: `roll` is a planetology input and
    // reaches resolve_preferences, so it is keyed to the chain's rounds, not to the
    // wizard's total. The wizard's two pass rounds carry their own counters
    // (m_wiz_pass_roll) because they are not planetology inputs at all.
    // AT LEAST ONE COUNTER PER ROUND (BL-863). `roll` keeps THREE rather than
    // shrinking with the round count: it is on the save format
    // (save_envelope_roundtrip asserts 8 leans + roll[3] survive), and shrinking
    // it would make a UI reorder a save-format change. The spare counter is not
    // dead -- it is the drawdown lean's, for when round 5 takes it.
    static_assert(sizeof(world_preferences::roll)
                      >= sizeof(uint32_t)
                             * static_cast<std::size_t>(wizard_planetology_round_count),
                  "world_preferences::roll must carry one counter per planetology round");

    // Clamp first — Back/Continue and verify.generation_stage all write this.
    if (m_wiz_round < 0)                   m_wiz_round = 0;
    if (m_wiz_round >= wizard_round_count) m_wiz_round = wizard_round_count - 1;

    // Recompute only when a control moved. Resolution plus the chain is cheap but
    // not free, and a cached preview keeps the charts still while the player reads
    // them — a chart that twitches every frame cannot be read at all.
    if (m_wiz_dirty || m_wiz_preview.empty())
    {
        refresh_wizard_preview();
        m_wiz_dirty = false;
        // The planetology chain just moved, so both pass rounds are stale: the
        // history runs ON this world. Causality flows one way, downstream only.
        invalidate_wizard_rounds_below(wizard_planetology_round_count - 1);
    }
    if (m_wiz_preview.empty())
        return; // defensive: the preview is the wizard's only data source

    // Adopt a finished real-surface build (and chain a relaunch if the params
    // moved mid-build). Cheap zero-wait probe; runs every wizard frame.
    poll_wizard_surface();
    // The same probe for round 4's history run (BL-829). Also every frame, and
    // also cheap: the wizard must keep repainting while the pass works, because
    // on this round the wait IS the content.
    poll_wizard_history();

    // What this GUARDED, before the wizard grew past the chain (BL-816): that every
    // wizard round the code hands to ui::chain_round_at has a chart-round entry
    // behind it. It was written as an equality only because the two numbers happened
    // to be the same when the wizard was planetology and nothing else. They are not
    // the same number any more — the wizard walks six rounds, the chart chain still
    // covers three — so the equality is re-expressed as the two facts it stood for:
    // the chart chain covers exactly the planetology rounds, and the planetology
    // rounds are a strict prefix of the wizard. Every chain_round_at call below is
    // gated on `planetology_round` accordingly.
    // COVERS AT LEAST, NOT EXACTLY (BL-863). The chart chain keeps all THREE
    // groups because tile_inspector.cpp's History ledger reads the same table --
    // dropping the third would delete the legacy/spend charts from the in-game
    // ledger, which is not what retiring a WIZARD round asked for. The wizard
    // walks the first two; the invariant that matters is that every round it
    // hands to chain_round_at has an entry.
    static_assert(ui::chain_round_count >= wizard_planetology_round_count,
                  "the chart chain must cover at least the wizard's planetology rounds");
    static_assert(wizard_planetology_round_count < wizard_round_count,
                  "the planetology rounds are a strict prefix of the wizard's rounds");
    static_assert(planetology_rounds == wizard_planetology_round_count
                      && pass_rounds == wizard_pass_round_count
                      && lapse_rounds == wizard_lapse_round_count,
                  "the file-local round-count mirrors must track app's constants");
    static_assert(wizard_lapse_round_count <= wizard_pass_round_count,
                  "the lapse rounds are a prefix of the pass rounds");

    // Which kind of round is on screen. The planetology rounds preview a pure chain
    // per keystroke; the pass rounds cannot (STARTUP.md § The wait, then the lapse).
    const bool planetology_round = (m_wiz_round < wizard_planetology_round_count);
    const int  pass_index        = m_wiz_round - wizard_planetology_round_count;

    // ── A LAPSE ROUND'S playback, advanced once per frame and read TWICE — the
    //    board on the left and the map on the right must show the same instant, so
    //    the slice is materialised here rather than in each of them. Rounds 3-6
    //    (Culture, Empires, Exploration, Industrialisation) are all lapse rounds
    //    and each owns its own record slot (BL-946, BL-1068). ──
    const bool lapse_round =
        (!planetology_round && pass_index < wizard_lapse_round_count);
    const int  lapse_index = lapse_round ? pass_index : 0;
    // BL-914: pull whatever the round's own worker has published so far,
    // BEFORE the empty() check below — this is what turns "empty until the
    // future lands" into "has a growing record from the first publish on".
    if (lapse_round) poll_wizard_history_tap(lapse_index);
    std::vector<uint16_t> hist_slice, hist_lagged;
    int hist_lagged_year = INT32_MIN; // the year `hist_lagged` was taken at (BL-1000)
    if (lapse_round && !m_wiz_history[lapse_index].empty())
    {
        // The land mask comes from the wizard's OWN packed surface — the same
        // raster the globe samples and the same one "Begin" builds — so the
        // coastline a frontier stalls at is the coastline the campaign has. It is
        // a no-op until that async build lands; the round simply retries.
        ui::history_lapse& rec = m_wiz_history[lapse_index];
        ui::finish_history_lapse(rec,
                                 m_wiz_surface.empty() ? nullptr : m_wiz_surface.data(),
                                 m_wiz_surface.size(),
                                 m_wiz_terrain.empty() ? nullptr : m_wiz_terrain.data(),
                                 m_wiz_terrain.size());

        const int first = rec.lapse.start_year;
        const int last  = first + rec.lapse.years; // BL-914: the YEAR REACHED SO FAR
                                                    // while live — see poll_wizard_history_tap
                                                    // — and the true final year once landed.
        const bool live = m_wiz_history_future[lapse_index].valid();

        // FROZEN UNDER --verify, for the reason the globe's rotation is: a capture
        // must never race an animation. The year is then whatever the script set.
        //
        // BL-914: a LIVE round is always advancing — there is nothing to pause
        // yet, only a frontier the playhead can be waiting at (`year == last`,
        // handled by the clamp below rather than by stopping here). Once
        // landed, `m_wiz_history_playing` is what the new Pause control drives.
        if ((live || m_wiz_history_playing[lapse_index]) && m_golden_dir.empty())
        {
            // THE RATE IS AGAINST THE ROUND'S FULL SPAN, NOT AGAINST HOW FAR IT
            // HAS GOT (Ben, 2026-09-11: "played at a constant (slower) rate
            // than calculation"). `last - first` is the wrong divisor while
            // live — it grows every publish, and dividing by a growing number
            // would make the transport visibly slow down as history runs.
            // Round 4's full span is known before a single owner_change
            // exists (`sub_total`, set in `hard_coded_world.cpp` right before
            // `run_history_sim` starts); round 3's migration record has no
            // such upfront figure, but it is not built incrementally either
            // (settlement.cpp is untouched by this item) — its very first
            // publish already carries the WHOLE finished record, so
            // `last - first` is already the true total the first time this
            // branch ever runs for it, and never grows again afterward.
            const int sub_total =
                m_wiz_history_progress[lapse_index].sub_total.load(std::memory_order_relaxed);
            const float span = sub_total > 0 ? static_cast<float>(sub_total)
                                             : static_cast<float>(last - first);
            // BL-948: the viewer's own choice of wall clock for the whole span,
            // not a fixed 30 s. The divisor is the only thing that changed; the
            // "against the full span, never against how far it has got" rule
            // above is what keeps a live round from slowing down as it runs.
            const float run_secs = m_wiz_history_secs[lapse_index] > 0.0f
                                       ? m_wiz_history_secs[lapse_index]
                                       : wizard_lapse_secs_default;
            const float rate = span > 0.0f ? span / run_secs : 1.0f;
            m_wiz_history_carry[lapse_index] += ImGui::GetIO().DeltaTime * rate;
            const int whole = static_cast<int>(m_wiz_history_carry[lapse_index]);
            if (whole > 0)
            {
                m_wiz_history_carry[lapse_index] -= static_cast<float>(whole);
                m_wiz_history_year[lapse_index]  += whole;
            }
            // Stop-at-the-end applies only once landed: hitting the current
            // frontier of a still-running pass is a WAIT (the clamp below
            // holds the playhead there), never the end of the transport.
            if (!live && m_wiz_history_year[lapse_index] >= last)
            {
                m_wiz_history_year[lapse_index]    = last;
                m_wiz_history_playing[lapse_index] = false;
            }
        }
        int& year = m_wiz_history_year[lapse_index];
        if (year < first) year = first;
        // NEVER READS A YEAR THE PASS HAS NOT REACHED (the determinism-
        // adjacent half of this item's DONE WHEN): `last` is `year_reached`
        // while live, so this is the fixed-rate/published-year clamp the
        // design asks for, expressed as the one clamp the code already had.
        if (year > last)  year = last;

        hist_slice = owner_slice_at(rec.lapse, year);
        // The lagged board, for the entry/exit marks: a twelfth of the span
        // back, CLAMPED to the record's first year (BL-1106) — the one rule,
        // in history_lapse.cpp, that the verify read shares. Unclamped, a
        // resumed span's first twelfth marked every inherited realm '*'.
        hist_lagged_year = ui::lapse_lagged_year(rec, year);
        hist_lagged = owner_slice_at(rec.lapse, hist_lagged_year);
    }

    const wizard_round_head wr       = wizard_round_head_at(m_wiz_round);
    const int               n_bodies = std::min(static_cast<int>(m_wiz_preview.size()),
                                               prototype_body_count());

    // The homeworld is the subject of every single-body chart. Located by its
    // authored flag rather than by position, so the body list can be reordered.
    std::size_t home = 0;
    for (int i = 0; i < n_bodies; ++i)
        if (prototype_body(i).is_homeworld) { home = static_cast<std::size_t>(i); break; }

    // The charts themselves live in ui::generation_charts, shared with the History
    // ledger so the plots a player decided against are the same plots they can
    // reopen mid-campaign. The wizard hands in the live preview plus its
    // zero-drawdown twin — the "formed" reference the Spend chart's hollow columns
    // are measured against.
    std::vector<ui::generation_chart_body> chart_bodies;
    chart_bodies.reserve(static_cast<std::size_t>(n_bodies));
    for (int i = 0; i < n_bodies; ++i)
    {
        const std::size_t k = static_cast<std::size_t>(i);
        chart_bodies.push_back(ui::generation_chart_body{
            m_wiz_names.bodies[k].c_str(), // generated, not the table placeholder (BL-257)
            &m_wiz_preview[k],
            (k < m_wiz_undrawn.size()) ? &m_wiz_undrawn[k] : nullptr });
    }
    const ui::generation_chart_source chart_src{
        chart_bodies.data(), chart_bodies.size(), home };

    constexpr ImU32 col_bright = IM_COL32(225, 230, 240, 255);
    constexpr ImU32 col_dim    = IM_COL32(120, 128, 145, 255);

    // One dim, wrapping text helper — every subtitle and caption reads in the same
    // colour as the menu's tagline.
    auto dim_text = [&](const char* t) {
        ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
        ImGui::TextWrapped("%s", t);
        ImGui::PopStyleColor();
    };


    // A wide centred surface — this is the first thing a player sees, so it takes
    // the screen rather than the menu's 280px column. Same borderless,
    // background-less idiom as draw_main_menu; the render clear colour is the backdrop.
    // Split 1/3 : 2/3 — controls (stage folds, leans, reroll) left, the round's
    // painted preview right, so every reroll is SEEN, not just re-plotted.
    const float panel_w = std::min(disp.x - 96.0f, 1440.0f);
    const float panel_h = std::max(420.0f, disp.y - 96.0f);
    ImGui::SetNextWindowPos({disp.x * 0.5f, disp.y * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({panel_w, panel_h}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground;
    if (ImGui::Begin("##generation", nullptr, flags))
    {
        char buf[256];

        // ── Left third: everything the player DOES — header, stage folds, leans,
        //    navigation. The stage folds still expand to their full chart views
        //    (the fold idiom is unchanged); they simply live in a column now. ──
        const float col_w = (panel_w - style.WindowPadding.x * 2.0f
                             - style.ItemSpacing.x) / 3.0f;
        ImGui::BeginChild("##wiz_left", {col_w, 0.0f}, false,
                          ImGuiWindowFlags_NoBackground);
        // BL-904: gives verify.scroll_panel("wizard", ...) a real scroller to
        // aim at. The column is a plain BeginChild, not a foldout ledger, but
        // `foldout_scroll_child` only matches on the id string it is handed,
        // so it works here unchanged.
        ui::foldout_scroll_child("##wiz_left");

        // ── (a) Header: the round name large, what it settles beneath, progress right ──
        {
            ImDrawList*  dl    = ImGui::GetWindowDrawList();
            const float  big   = ImGui::GetFontSize() * 1.7f;
            const ImVec2 p     = ImGui::GetCursorScreenPos();
            const float  avail = ImGui::GetContentRegionAvail().x;

            // The atlas carries a single size, so the title is scaled through the
            // draw list rather than by swapping fonts (there is no second font).
            dl->AddText(ImGui::GetFont(), big, p, col_bright, wr.name); // fit-exempt: chrome strip authored to fit at the 1280x720 floor

            std::snprintf(buf, sizeof buf, "Round %d of %d", m_wiz_round + 1, wizard_round_count);
            const ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText({p.x + avail - ts.x, p.y + (big - ts.y) * 0.5f}, col_dim, buf); // fit-exempt: chrome strip authored to fit at the 1280x720 floor

            ImGui::Dummy({avail, big + 2.0f});
        }
        dim_text(wr.question);

        // One pip per wizard round, the current one lit: past rounds filled dim,
        // future ones hollow. Drives off wizard_round_count, so it grew with it.
        {
            ImDrawList*  dl = ImGui::GetWindowDrawList();
            const ImVec2 p  = ImGui::GetCursorScreenPos();
            constexpr float pip = 10.0f, gap = 6.0f;
            for (int i = 0; i < wizard_round_count; ++i)
            {
                const ImVec2 a{p.x + static_cast<float>(i) * (pip + gap), p.y + 4.0f};
                const ImVec2 b{a.x + pip, a.y + pip};
                if (i == m_wiz_round)     dl->AddRectFilled(a, b, col_bright);
                else if (i < m_wiz_round) dl->AddRectFilled(a, b, col_dim);
                else                      dl->AddRect(a, b, col_dim);
            }
            ImGui::Dummy({static_cast<float>(wizard_round_count) * (pip + gap), pip + 8.0f});
        }
        ImGui::Separator();

        // ── (b) The round's stages, charted. Sized to leave the preference block and
        //    the footer pinned below, so the controls never scroll away from the
        //    charts they act on. ──
        const float frame_h  = ImGui::GetFrameHeight();
        const float line_h   = ImGui::GetTextLineHeightWithSpacing();
        // Each lean row is now a label line plus a 2x2 radio grid (three lines).
        float decide_h = static_cast<float>(round_pref_count(m_wiz_round))
                             * (line_h + 2.0f * (frame_h + style.ItemSpacing.y))
                       + line_h * static_cast<float>(round_note_lines(m_wiz_round)
                                                     + (m_wiz_resolved.gave_up ? 2 : 0))
                       + style.ItemSpacing.y * 3.0f;
        // Empires' turbulence lean is `turbulence_row`, not `lean_row` — ONE row
        // of three radios (Calm/Ordinary/Turbulent), not the 2x2 grid
        // round_pref_count's generic multiplier assumes for every other lean.
        // Correct that one row back out, then add the caption's real height,
        // measured rather than guessed (BL-904): it runs to five sentences,
        // well past what a fixed line count could safely predict at every
        // column width the wizard can be shown at.
        if (m_wiz_round == 3)
            decide_h += ImGui::CalcTextSize(kTurbulenceCaption, nullptr, false,
                                             ImGui::GetContentRegionAvail().x).y
                      + style.ItemSpacing.y
                      - (frame_h + style.ItemSpacing.y);
        // Two button rows now: Reroll full-width above, Back / Continue below.
        const float footer_h = 34.0f * 2.0f + style.ItemSpacing.y * 3.0f;
        ImGui::BeginChild("##wiz_charts", {0.0f, -(decide_h + footer_h)}, false,
                          ImGuiWindowFlags_NoBackground);
        // BL-1000: the lapse rounds' board, ticker and arc readout live in THIS
        // child, and at 1080p the arc readout sits below its fold — so
        // verify.scroll_panel("wizard_charts", ...) needs a scroller of its own
        // to reach it, exactly as "wizard" reaches the outer column.
        ui::foldout_scroll_child("##wiz_charts");

        // The player still watches the chain work link by link — they have just
        // stopped clicking between the links. Each stage measures its own column
        // metric from the region it is handed, so the same call fits here and in the
        // History ledger's much narrower fold-out.
        // Each stage rests as one verdict line and expands to its full view (Ben,
        // 2026-08-01). The wizard is where the fold idiom is TAUGHT — it is the first
        // surface a player meets, so the gesture is learned before the first ledger
        // opens. It also turns a long scroll into a readable chain: the round's
        // stages fit on one screen as verdicts, and the player opens the ones the
        // roll made interesting.
        if (planetology_round)
        {
            const ui::chain_round& cr = ui::chain_round_at(m_wiz_round);
            for (int s = static_cast<int>(cr.first); s <= static_cast<int>(cr.last); ++s)
                ui::draw_stage_fold(chart_src, static_cast<chain_stage>(s), m_ui,
                                    detail_surface::generation_stage);
        }
        else if (lapse_round)
        {
            // ── Rounds 3-6: the wait, then the board (BL-829 / BL-830 /
            //    BL-860 / BL-946 / BL-1068) ──
            //
            // THE TRANSPORT IS DELIBERATELY PLAIN. The wizard's standing premise —
            // *you set conditions here, you do not steer* — and the globe's own
            // no-input ruling both argue for the plainer answer, and BL-829 files
            // scrub/pause as a question to be settled by WATCHING rather than in
            // advance. So: it runs, and it can be run again from the start. No
            // pause and no scrub until Ben has watched one.
            ui::history_lapse& rec = m_wiz_history[lapse_index];
            if (m_wiz_history_future[lapse_index].valid())
            {
                // THE WAIT IS A WAIT, AND SAYS SO (Ben, 2026-09-16): "separate each
                // part with an otherwise completely blank Loading X Round. This way,
                // the player can see that they have to wait, and what they are
                // watching is a time-lapse of that very fast calculation." One
                // centred line, and no map beside it: the pass stages and the year
                // counter went with it, because a reader who has been told to wait
                // does not also need to be told which of twelve links the wait is on.
                //
                // THIS REVERSES BL-914 for the pass rounds, deliberately. "The wait
                // is the round" put the sim frontier on screen as it computed; the
                // calculation is fast and jerky where the lapse is paced and whole,
                // so showing the first as if it were the second made the second
                // unreadable — and a round that opened already chasing the frontier
                // was past its hand-over cross-fade before anyone could see it.
                {
                    static const char* const k_loading[wizard_lapse_round_count] = {
                        "Loading the Culture round",
                        "Loading the Empires round",
                        "Loading the Exploration round",
                        "Loading the Industrialisation round",
                    };
                    const char* const label = k_loading[lapse_index];
                    const float w  = ImGui::GetContentRegionAvail().x;
                    const float h2 = ImGui::GetContentRegionAvail().y;
                    const ImVec2 sz = ImGui::CalcTextSize(label);
                    ImGui::Dummy({w, std::max(0.0f, h2 * 0.40f)});
                    if (w > sz.x)
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - sz.x) * 0.5f);
                    ImGui::TextUnformatted(label);

                    // THE WAIT HAS A BAR (Ben, 2026-09-18: "wire in a progress bar
                    // for 'Loading x round'"), AND SAYS WHAT IT IS DOING (Ben,
                    // 2026-09-24, amending the 2026-09-16 "no captions"): the outer
                    // bar weighted by what each step costs, an inner bar inside
                    // every long step, a caption naming the step and an elapsed
                    // count. It is the one wait surface the building screen draws
                    // too (ui::draw_generation_wait, BL-1072): one wait, not two.
                    ui::draw_generation_wait(m_wiz_history_progress[lapse_index], w,
                                             nullptr, m_golden_dir.empty());
                }
            }
            else if (rec.empty())
            {
                // NO RUN BUTTON (Ben, 2026-09-09: "we can retire the 'run'
                // button. Wire that to auto start when next is clicked in phase
                // 3"). Arriving on the round IS the instruction to run it —
                // there was never a second thing the player might have wanted
                // here, so the button asked a question with one answer.
                //
                // The launch itself is on the PREVIOUS round's Next press rather
                // than here, so the pass is already under way by the time this
                // frame draws; see the navigation block below. This branch is
                // only reached if a run has not been started or has been cleared.
                dim_text(lapse_index == 0
                    ? "The peopling of an empty world, run here rather than previewed: "
                      "the pass behind it is the most expensive in the project, and it "
                      "cannot be re-rolled on every keystroke the way the planetology "
                      "rounds are."
                    : lapse_index == 1
                    ? "Claim and counter-claim from 400 BCE, run here rather than "
                      "previewed: the history is the most expensive pass in the "
                      "project, and it cannot be re-rolled on every keystroke the way "
                      "the planetology rounds are."
                    : lapse_index == 2
                    ? "Treasuries, treaties and fleets from 1200 CE, run here rather "
                      "than previewed: conflict moves off the home coast in this span, "
                      "and it cannot be re-rolled on every keystroke the way the "
                      "planetology rounds are."
                    : "Industry from 1660 to 1960, run here rather than previewed: "
                      "this round builds the whole world the campaign opens on, and "
                      "it cannot be re-rolled on every keystroke the way the "
                      "planetology rounds are.");
            }
            else
            {
                // NO RESTART BUTTON (Ben, 2026-09-11): with a scrubber below it
                // is redundant — anywhere the run ever reached is a drag away —
                // and its row goes to the ranking board this round's whole
                // point is to show. BL-914's transport is Pause/Play plus the
                // scrubber, both live only now that the record is complete
                // (NR-813's deferral premise — no real arc to sit through — is
                // gone since BL-906 lengthened round 4's own span).
                //
                // A TOGGLE (io-standing-rules.md § Toggle rule): the label IS
                // the visible active state, so the same press that started
                // playing undoes it.
                const int  first_y = rec.lapse.start_year;
                const int  last_y  = first_y + rec.lapse.years;
                const bool playing = m_wiz_history_playing[lapse_index];
                if (ImGui::Button(playing ? "Pause##wizhisttransport"
                                          : "Play##wizhisttransport",
                                  {ImGui::GetContentRegionAvail().x, 30.0f}))
                {
                    m_wiz_history_playing[lapse_index] = !playing;
                    m_wiz_history_paused[lapse_index]  =  playing; // now paused iff it just stopped
                    // Resuming from the very end restarts rather than sitting on
                    // a Play button that visibly does nothing — the one case
                    // Restart used to cover that a plain toggle would not.
                    if (!playing && m_wiz_history_year[lapse_index] >= last_y)
                    {
                        m_wiz_history_year[lapse_index]  = first_y;
                        m_wiz_history_carry[lapse_index] = 0.0f;
                    }
                }
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                int scrub_year = m_wiz_history_year[lapse_index];
                if (ImGui::SliderInt("##wizhistscrub", &scrub_year, first_y, last_y,
                                     ui::lapse_year_label(scrub_year).c_str()))
                {
                    // Dragging is itself an implicit pause — a scrubber that
                    // fought the playhead for the same year would be
                    // unreadable, and BL-914 only ever asks for Pause AND a
                    // scrubber together, never scrubbing while still playing.
                    m_wiz_history_year[lapse_index]    = scrub_year;
                    m_wiz_history_carry[lapse_index]   = 0.0f;
                    m_wiz_history_playing[lapse_index] = false;
                    m_wiz_history_paused[lapse_index]  = true;
                }
                // BL-948 — THE SPEED CONTROL, on every lapse round. Three
                // rungs of WALL CLOCK for the whole span (Ben: the lapses run
                // too fast to watch), in the wizard's own three-way idiom —
                // the same radio row Sparse/Lean/Standard uses on the menu, so
                // it needs no explaining. It changes the rate the playhead
                // advances at and nothing else: the scrubber above still goes
                // anywhere, and a live round still draws as fast as the pass
                // computes.
                {
                    ImGui::TextUnformatted("Pace");
                    ImGui::SameLine();
                    for (int i = 0; i < 3; ++i)
                    {
                        // Minutes read as minutes: "1m 30s", never "90s".
                        char id[32];
                        const int secs = static_cast<int>(wizard_lapse_secs[i]);
                        if (secs < 60)
                            std::snprintf(id, sizeof id, "%ds##wizhistpace%d", secs, i);
                        else if (secs % 60 == 0)
                            std::snprintf(id, sizeof id, "%dm##wizhistpace%d", secs / 60, i);
                        else
                            std::snprintf(id, sizeof id, "%dm %ds##wizhistpace%d",
                                          secs / 60, secs % 60, i);
                        if (i > 0) ImGui::SameLine();
                        if (ImGui::RadioButton(id, m_wiz_history_secs[lapse_index]
                                                       == wizard_lapse_secs[i]))
                        {
                            // The carry is fractional years at the OLD rate;
                            // keeping it would hand the new rate a debt it
                            // never ran up. The playhead itself does not move.
                            m_wiz_history_secs[lapse_index]  = wizard_lapse_secs[i];
                            m_wiz_history_carry[lapse_index] = 0.0f;
                        }
                    }
                }
                ImGui::Spacing();

                if (rec.peoples)
                {
                    // BL-1106: NO BATTLE CELLS ON THE CULTURE ROUND — a
                    // migration is a diffusion with nothing fighting in it
                    // (STARTUP.md § Round 3). The line counts what the record
                    // holds instead: every people the record ever shows
                    // holding ground, over the regions it settled.
                    int peoples = 0;
                    for (const int32_t seat : rec.polity_seat) if (seat >= 0) ++peoples;
                    std::snprintf(buf, sizeof buf, "%s  -  %d peoples across %d regions "
                                                   "in the full run",
                                  ui::lapse_year_label(m_wiz_history_year[lapse_index]).c_str(),
                                  peoples, static_cast<int>(rec.lapse.region_stride));
                }
                else
                    std::snprintf(buf, sizeof buf, "%s  -  %lld battles, %lld conquests, "
                                                   "%lld foundings in the full run",
                                  ui::lapse_year_label(m_wiz_history_year[lapse_index]).c_str(),
                                  static_cast<long long>(rec.battles),
                                  static_cast<long long>(rec.conquests),
                                  static_cast<long long>(rec.foundings));
                dim_text(buf);
                ImGui::Separator();

                ui::draw_lapse_scoreboard(rec, hist_slice, hist_lagged,
                                          m_wiz_history_year[lapse_index],
                                          hist_lagged_year);
                // BL-916: the ticker — the named moments at or before the
                //         playhead, the newest of which is the marker on the map.
                ui::draw_lapse_ticker(rec, m_wiz_history_year[lapse_index]);
                // BL-891: the arc, so a rolled world can be judged without
                //         watching the whole replay.
                ui::draw_lapse_arc(rec);

                ImGui::Spacing();
                // WHAT THIS ROUND IS AND IS NOT (BL-871, then BL-906, then
                // BL-946). The three lapse rounds stop at different points —
                // the migration at its own derived end year, the Empires sim
                // at 1200 CE, the Exploration span at 1660 CE — so they do
                // not replay the same recorded age. BL-906 (2026-09-11) closed
                // the gap this note used to name: the Empires round now runs
                // its full 400 BCE -> 1200 CE span, decoupled from the epoch
                // (`CIVILISATION.md` § The span is 400 BCE to 1200 CE).
                // IN-WORLD FOOTERS (BL-1106). A footer names the age the
                // round plays, in the history's own words; it cites no
                // repository path, because the player is not reading the
                // repository. Each names its own span, and the spans meet
                // end to end (STARTUP.md § Rounds).
                if (lapse_index == 1)
                    dim_text("400 BCE to 1200 CE: the realms that rose on the ground the "
                             "migration left, and what became of them.");
                else if (lapse_index == 2)
                    dim_text("1200 to 1660 CE: the age of exploration, when conflict moved "
                             "off the home coast and across the water.");
                else if (lapse_index == 3)
                    dim_text("1660 to 1960 CE: the age of industry, the last before the "
                             "campaign opens.");
            }
        }

        ImGui::EndChild();

        // ── (c) The round's preferences. Named leans and nothing else: no value from
        //    the resolved params is printed or editable here, because the charts above
        //    already show what the roll produced and that is the honest feedback. ──
        world_preferences& pf = m_pending_world_params.preferences;
        ImGui::Separator();

        switch (m_wiz_round)
        {
            case 0:
                if (lean_row("star", "Star", pf.star,
                             "Dimmer", "Sun-like", "Brighter"))                      m_wiz_dirty = true;
                if (lean_row("size", "World", pf.world_size,
                             "Small", "Earth-like", "Large"))                        m_wiz_dirty = true;
                if (lean_row("interior", "Interior", pf.interior,
                             "Old and cold", "Moderate", "Young and vigorous"))      m_wiz_dirty = true;
                if (lean_row("metal", "Metal", pf.metal,
                             "Metal-poor", "Normal", "Metal-rich"))                  m_wiz_dirty = true;
                break;

            case 1:
                if (lean_row("ocean", "Ocean", pf.ocean,
                             "Continental", "Balanced", "Oceanic"))                  m_wiz_dirty = true;
                if (lean_row("oxygen", "Oxygen", pf.oxygen_story,
                             "Oxygenated early", "Balanced", "Oxygenated late"))     m_wiz_dirty = true;
                // The one trade worth spelling out: a single choice moves two resources
                // in opposite directions, with every gate still passed either way.
                dim_text("Oxygenated early -> coal-rich and iron-lean; oxygenated late -> "
                         "iron-rich and coal-lean.");
                if (lean_row("coal", "Coal basins", pf.coal_basins,
                             "Seasonal", "Mixed", "Everwet"))                        m_wiz_dirty = true;
                break;

            case 2:
                if (lean_row("drawdown", "Drawdown", pf.drawdown,
                             "Barely touched", "Worked", "Stripped"))                m_wiz_dirty = true;
                break;

            // THE EMPIRES ROUND TAKES THE TURBULENCE LEAN (BL-839; Ben,
            // 2026-09-08). It is sited on the round whose own pass it leans, so
            // the control and the thing it moves are on the same screen: set it,
            // roll, and the arc readout beside it is the answer.
            //
            // IT SETS CONDITIONS, IT DOES NOT STEER. The three settings move the
            // spread of culture aggression, how sharply neighbours coalesce
            // against a riser, and how fast reach decays -- and not one of them
            // names, targets or corrects a number of realms. A calm world that
            // fragments anyway is a correct calm world. See
            // `world/history_sim.hpp` sec THE HISTORICAL TURBULENCE LEAN.
            case 3:
                if (turbulence_row(pf.history_turbulence))
                {
                    // THE LEAN'S OWN DIRTY PATH (BL-1083; STARTUP.md § Each pass
                    // round is rerollable: "a lean change on a round takes the
                    // same path: it invalidates that round onward, relaunches
                    // it at once, and touches nothing above"). NOT
                    // `m_wiz_dirty`: that flag re-previews the planetology
                    // chain and invalidates below round 1, which wiped all
                    // four records — Culture's included — for a migration the
                    // lean provably does not touch (`era_minus_one_sim_params`
                    // is the only reader of `history_turbulence`, and the
                    // migration runs before it). `pf` aliases
                    // `m_pending_world_params.preferences`, so the worker
                    // launched below already carries the new lean.
                    //
                    // THIS ROUND'S OWN RECORD GOES TOO, which is why the
                    // argument is `m_wiz_round - 1` and not `m_wiz_round`. The
                    // setting is an INPUT to the pass this round runs, so a
                    // record made under the previous setting is an account of a
                    // history the player has just stopped asking for -- exactly
                    // the silent failure `invalidate_wizard_rounds_below` was
                    // wired ahead of the passes to prevent, one round earlier
                    // than the reroll button needs it.
                    invalidate_wizard_rounds_below(m_wiz_round - 1);
                    // RELAUNCHED AT ONCE, on the same guard the reroll uses: a
                    // run already in flight cannot be recalled, so it was marked
                    // stale above and is dropped when it lands; a fresh one is
                    // started only when the slot is free. The lean is not a
                    // reroll — `span_seed[1]` is untouched — so the same lean
                    // set twice rebuilds the same history.
                    if (lapse_round && !m_wiz_history_future[lapse_index].valid())
                        launch_wizard_history_run(lapse_index);
                }
                dim_text(kTurbulenceCaption);
                break;

            default:
                break;
        }

        // The reroll cost, told rather than hidden. Resolution rejects and re-draws
        // until the homeworld clears the strict Earth-like floor; how many draws that
        // took is a true thing about the preferences just set, and a narrow set of
        // leans is meant to feel like one.
        if (m_wiz_resolved.gave_up)
        {
            dim_text("These preferences have almost no viable region - no draw cleared the "
                     "Earth-like floor, so this is the closest world found. Loosen one of "
                     "them, or reroll.");
        }
        else if (m_wiz_resolved.attempts > 1)
        {
            std::snprintf(buf, sizeof buf, "Found on attempt %u.",
                          static_cast<unsigned>(m_wiz_resolved.attempts));
            dim_text(buf);
        }

        // ── (d) Navigation. Reroll re-draws THIS round from a fresh number (and
        //    everything downstream of it, because the chain is causal); the preview
        //    pane repaints from the same roll, so what changed is SEEN. The seed
        //    still names a determinate family: (seed, leans, roll counters) is the
        //    whole input, and the same triple always returns the same world. ──
        const bool  last  = (m_wiz_round == wizard_round_count - 1);
        const float bar_w = ImGui::GetContentRegionAvail().x;

        // Reroll full-width and first — it is the wizard's main verb now.
        if (ImGui::Button("Reroll##wizroll", {bar_w, 34.0f}))
        {
            if (planetology_round)
            {
                ++pf.roll[m_wiz_round];
                m_wiz_dirty = true;
            }
            else
            {
                // Each pass round keeps its OWN reroll (Ben, 2026-09-08): the 4000
                // years can be rerolled, and the focused 400-year economy pass is its
                // own page with its own run and reroll.
                ++m_wiz_pass_roll[pass_index];
                m_wiz_pass_current[pass_index] = false; // re-run, not yet accepted
                invalidate_wizard_rounds_below(m_wiz_round);
                // A lapse round rerolls by RE-RUNNING its pass, not by re-drawing
                // a cached one (STARTUP.md § Each pass round is rerollable) —
                // which is the whole reason the wait had to be worth watching
                // rather than merely tolerable.
                if (lapse_round && !m_wiz_history_future[lapse_index].valid())
                {
                    // A DIFFERENT AGE OVER THE SAME GROUND (Ben, 2026-09-09:
                    // "reroll should produce differences regardless"), AND ONLY
                    // THIS ROUND'S AGE (Ben, 2026-09-24; STARTUP.md § Each pass
                    // round is rerollable). Each span carries a seed of its
                    // own, indexed exactly as the lapse rounds are — 0 the
                    // migration, 1 Empires, 2 Exploration, 3 Industrialisation
                    // — and a span folds only its own slot, so bumping this one
                    // re-seeds span N alone: the rounds above keep the record
                    // they show, the rounds below were invalidated just above
                    // and rerun on the changed ground. The one shared term,
                    // `era_seed`, is the legacy seed no control moves any more:
                    // bumping it here re-seeded Empires, Exploration AND
                    // Industrialisation together, so a round-5 reroll silently
                    // replaced the history round 4 still showed, and a Culture
                    // reroll changed nothing visible while forking every later
                    // age. See world_params::span_seed for the fold rule.
                    ++m_pending_world_params.span_seed[lapse_index];
                    m_wiz_history[lapse_index] = ui::history_lapse{};
                    launch_wizard_history_run(lapse_index);
                }
            }
        }

        // Back always steps out one level, and the level outside round 0 is the main
        // menu — a wizard the player cannot leave is a trap (nothing is generated
        // until "Begin", so leaving costs nothing). Preferences survive the trip, so
        // re-entering resumes the same leans from round 0.
        const float half = (bar_w - style.ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Back##wizback", {half, 34.0f}))
        {
            if (m_wiz_round == 0)
            {
                m_screen = app_screen::menu;
                // BL-1073: re-entering the wizard re-runs the chain and every
                // round below it, so a held world can never be adopted from
                // here -- release its memory now rather than on re-entry.
                drop_wizard_world("left the wizard for the menu");
            }
            else
                --m_wiz_round;
        }
        ImGui::SameLine();
        // "Begin" is the wizard's ONLY generating press and it belongs on the LAST
        // round alone (Ben, 2026-09-08: "in place of begin we should see next").
        // Rounds 1-5 advance; only round 6 commits.
        if (ImGui::Button(last ? "Begin##wizgo" : "Next##wizgo", {half, 34.0f}))
        {
            if (last)
                begin_new_game(); // async since 2026-08-12 — see app::begin_new_game
            else
            {
                ++m_wiz_round;
                // A PASS STARTS WHEN THE PLAYER ARRIVES, NOT WHEN THEY ASK
                // (Ben, 2026-09-09: "wire that to auto start when next is
                // clicked in phase 3"). Retiring the Run button means the press
                // that MOVES ONTO a round is the press that begins it, so the
                // pass is already under way while the round's first frame draws
                // — which is the difference between a wait that started when
                // you arrived and one that started when you found the button.
                //
                // EACH LAPSE ROUND STARTS ITS OWN (BL-860), rather than round 4
                // alone: the arrival is generic, so round 5 gets the same
                // treatment without a second special case.
                //
                // Guarded on there being nothing already in flight or landed on
                // THAT round, so stepping Back and forward again does not throw
                // away a finished record and re-run it.
                const int arrived = m_wiz_round - wizard_planetology_round_count;
                if (arrived >= 0 && arrived < wizard_lapse_round_count
                    && m_wiz_history[arrived].empty()
                    && !m_wiz_history_future[arrived].valid())
                    launch_wizard_history_run(arrived);
            }
        }
        ImGui::EndChild(); // ##wiz_left

        // ── Right two-thirds: what this roll LOOKS like. A stylised painting read
        //    straight from the preview states — the system in round 0, the homeworld
        //    surface in round 1, its industrial history in round 2. ──
        ImGui::SameLine();
        ImGui::BeginChild("##wiz_preview", {0.0f, 0.0f}, false,
                          ImGuiWindowFlags_NoBackground);
        if (lapse_round)
        {
            // ── The lapse rounds replace the globe with a 2D MAP (Ben, 2026-09-08,
            //    at the live app). A globe shows a world; a map shows a FRONTIER,
            //    and the frontier is these rounds' whole subject — a border that
            //    stalls at a strait reads as a stall on a map and as foreshortening
            //    on a sphere. It inherits the globe's no-input rule: nothing here is
            //    a widget. ──
            // NOTHING BESIDE A WAIT (Ben, 2026-09-16). While the pass is still
            // computing, this pane stays empty: the left column says "Loading the
            // ... round" and that is the whole of the surface. Drawing the sim's
            // own frontier here is what BL-914 did and what this reverses — the
            // map appears when there is a finished record to play, and plays it
            // from its first year.
            if (m_wiz_history_future[lapse_index].valid()) { /* the wait draws nothing */ }
            else if (m_wiz_history[lapse_index].empty())
            {
                ImGui::Dummy({0.0f, ImGui::GetContentRegionAvail().y * 0.45f});
                ImGui::PushStyleColor(ImGuiCol_Text, col_dim);
                // BL-914: the map now draws WHILE the pass runs, so this text
                // is only ever seen for the first moment or two of a run —
                // before its worker has published a first region and a first
                // owner for it — rather than for the whole wait as before.
                const bool running = m_wiz_history_future[lapse_index].valid();
                if (lapse_index == 0)
                    ImGui::TextWrapped(running
                        ? "  The migration is starting."
                        : "  The migration has not been run for this world yet.");
                else if (lapse_index == 1)
                    ImGui::TextWrapped(running
                        ? "  The history is starting."
                        : "  The history has not been run for this world yet.");
                else if (lapse_index == 2)
                    ImGui::TextWrapped(running
                        ? "  The exploration is starting."
                        : "  The exploration has not been run for this world yet.");
                else
                    ImGui::TextWrapped(running
                        ? "  The industrialisation is starting."
                        : "  The industrialisation has not been run for this world yet.");
                ImGui::PopStyleColor();
            }
            else
            {
                ui::draw_lapse_map(m_wiz_history[lapse_index], hist_slice,
                                   m_wiz_history_year[lapse_index]);
            }
        }
        else
        {
            std::vector<ui::preview_body> pv;
            pv.reserve(static_cast<std::size_t>(n_bodies));
            for (int i = 0; i < n_bodies; ++i)
            {
                const body_inputs& bi = prototype_body(i);
                pv.push_back(ui::preview_body{
                    m_wiz_names.bodies[static_cast<std::size_t>(i)].c_str(), // BL-257
                    bi.orbit_au, bi.mass_earths, bi.parent_orbit_au,
                    bi.is_homeworld, &m_wiz_preview[static_cast<std::size_t>(i)] });
            }
            // Kepler turns slowly — one revolution per minute, wall-clock — so
            // the far hemisphere can be read too. Frozen under --verify
            // (m_golden_dir set): a golden capture must never race an animation.
            const float rot = m_golden_dir.empty()
                ? static_cast<float>(std::fmod(ImGui::GetTime() / 60.0, 1.0)) * 6.2831853f
                : 0.0f;
            const ui::preview_surface_view surf{
                home_grid_width, home_grid_height,
                m_wiz_surface.size() == static_cast<std::size_t>(home_grid_width)
                                            * static_cast<std::size_t>(home_grid_height)
                    ? m_wiz_surface.data() : nullptr };
            ui::draw_generation_preview(pv.data(), pv.size(),
                                        m_wiz_resolved.params, m_wiz_round, rot, surf);

            // The honest wait note: while a build is in flight the globe still
            // shows the PREVIOUS roll's surface (or the stylised stand-in on
            // the very first frames).
            if (m_wiz_surface_future.valid() && m_wiz_round > 0)
            {
                ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 28.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(120, 128, 145, 255));
                ImGui::TextUnformatted("  resolving the surface...");
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();

    // --autostart-windowed: walk the wizard as a player would — dwell ~20 frames
    // a round (so refresh_wizard_preview and the async surface build both run for
    // every round), then press Begin from HERE, inside the wizard's own frame,
    // which is the call site the real button uses.
    if (m_autostart_wizard >= 0)
    {
        ++m_autostart_wizard;
        // A player rerolls; a walk that never does stays inside the default
        // seed's resolved family and misses every reroll-dependent world.
        if (m_autostart_wizard % 20 == 10)
        {
            // GATED ON THE PLANETOLOGY ROUNDS, exactly as the Reroll button is.
            // `roll` is uint32_t[3] and is the last member of world_preferences,
            // itself the last member of world_params — so indexing it with a
            // round of 3 or 4 wrote past the end of m_pending_world_params into
            // whatever followed it. The Reroll path was gated when the wizard
            // grew to five rounds (BL-816); this driver was missed, and it is
            // the path nobody eyeballs, which is why it survived.
            if (m_wiz_round < wizard_planetology_round_count)
            {
                ++m_pending_world_params.preferences.roll[m_wiz_round];
                m_wiz_dirty = true;
            }
        }
        if (m_autostart_wizard % 20 == 0)
        {
            // A PLAYER-SHAPED WALK (BL-1085, the review's fix round): the walk
            // dwells on a lapse round until its run has landed before pressing
            // Next, as a player watching the round would. Advancing every 20
            // frames regardless launched rounds 3-6 within ~60 frames -- four
            // world builds plus the surface build at once, the memory-pressure
            // shape BL-1078 was filed on -- and this walk is the scripted proof
            // the adopt path rests on, so its peak should be a player's peak.
            const int cur = m_wiz_round - wizard_planetology_round_count;
            const bool round_building = cur >= 0 && cur < wizard_lapse_round_count
                                        && m_wiz_history_future[cur].valid();
            if (round_building)
            {
                // Still building: try again on the next multiple of 20.
            }
            else if (m_wiz_round < wizard_round_count - 1)
            {
                ++m_wiz_round;
                // Dirty on the PLANETOLOGY rounds only, as the walk always
                // meant (the chain preview and the surface build re-run per
                // round); on a lapse round the Next press never dirties, and a
                // dirty flag there re-runs the chain, which invalidates every
                // lapse round below it and drops round 6's slot mid-build.
                if (m_wiz_round < wizard_planetology_round_count)
                    m_wiz_dirty = true;
                // ARRIVING ON A LAPSE ROUND STARTS ITS RUN, here as on the Next
                // press above (BL-1085): the walk used to skip the handler, so
                // no round ever ran and Begin always built cold -- the one
                // path the item exists to retire. Same guard as the press.
                const int arrived = m_wiz_round - wizard_planetology_round_count;
                if (arrived >= 0 && arrived < wizard_lapse_round_count
                    && m_wiz_history[arrived].empty()
                    && !m_wiz_history_future[arrived].valid())
                    launch_wizard_history_run(arrived);
            }
            else
            {
                std::printf("[autostart-windowed] wizard walked; pressing Begin\n");
                std::fflush(stdout);
                m_autostart_wizard = -1;
                begin_new_game();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// BL-1089 — the per-world nation -> colour table, from the saved report
// ---------------------------------------------------------------------------
//
// A NATION'S COLOUR IS ITS REALM'S (Ben, 2026-09-24, R6; NATION_GENERATION.md
// § Pass 5; LENSES.md § The Country lens has retired). The wizard's rounds
// pinned every realm's slot by id; at Begin the last landed polity round holds
// exactly the colours the player watched, and a LOAD re-derives them from the
// saved report over the same pure functions (world/polity_identity.hpp) and
// the same raster walk, so a loaded game colours every nation as the wizard
// did. `ui::palette::nation_colour` reads the table first and hashes only an
// id the table does not hold — a nation of ownerless ground, or a world with
// no history.

namespace {

/// The realm colours (by polity id) re-derived from the report alone: the
/// three polity records in sequence, each assigned with the pins the one
/// before handed over, coloured at the last record's close. The water mask
/// is the world's own tiles, which is the surface the wizard's raster was
/// packed from. Empty when the body carries no record.
/// @param round_count The wizard's lapse round count (`app::wizard_lapse_round_count`),
///                    passed in because this is a free function and the count is the app's.
std::vector<uint32_t> derive_realm_colours(const generation_report& rep,
                                           const generation_report::body_entry& home,
                                           world& w, int round_count)
{
    std::vector<uint32_t> realm;
    // The homeworld grid is the CONSTANT the lapse rounds raster over
    // (`lapse_from_report` sets `grid_w`/`grid_h` from it) and the grid the
    // world's own tiles sit on. NOT the report's `tiles.gw/gh`: that is the
    // generator's call record and reads 180 x 84 on a --verify world while the
    // body carries 261 x 121 tiles — measured 2026-09-25, when reading it here
    // left every nation on the hash.
    const int gw = home_grid_width, gh = home_grid_height;
    if (gw <= 0 || gh <= 0) return realm;
    const std::vector<entity_id>& grid = body_tile_grid(w, home.id);
    if (grid.size() != static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
    {
        // Said aloud rather than silently hashed: a grid that does not match
        // the report's raster is a wiring fault, not a world with no history.
        std::printf("[identity] realm colours NOT derived: body %llu grid %zu tiles vs %d x %d\n",
                    static_cast<unsigned long long>(home.id), grid.size(), gw, gh);
        return realm;
    }

    std::vector<uint8_t> water(grid.size(), 1);
    for (std::size_t k = 0; k < grid.size(); ++k)
    {
        const auto tit = w.tiles.find(grid[k]);
        if (tit != w.tiles.end() && !is_water(tit->second.substrate)) water[k] = 0;
    }

    polity_pins pins;
    bool has_pins = false;
    int family_count = 0, records = 0;
    std::size_t regions_last = 0;
    std::vector<int32_t> last_slot, last_rung;
    for (int i = 1; i < round_count; ++i)
    {
        ui::history_lapse h = lapse_from_report(rep, i, /*adopted=*/true);
        if (h.empty()) continue;
        ++records;
        family_count = h.family_count;
        // PER RECORD, OVER THE RECORD'S OWN REGIONS (the review's fix round):
        // `lapse_from_report` cuts the anchor list to the record's
        // `region_stride`, so this raster and adjacency are the ones the
        // wizard's round drew by — the regions at that round's close, not the
        // 1960 set — and a strip founded in a later span cannot move an
        // earlier round's adjacency here and nowhere else.
        const std::vector<int32_t> tile_region =
            nearest_region_raster(water, gw, gh, h.region_col, h.region_row);
        const std::vector<std::vector<int32_t>> nbrs =
            region_adjacency(tile_region, gw, gh, h.region_col.size());
        regions_last = h.region_col.size();
        const std::vector<int32_t> first = polity_first_region(h.lapse);
        const std::vector<int32_t> seat  = polity_seat_region(h.lapse, first);
        std::vector<int32_t> wedge;
        if (h.family_count > 0)
        {
            const std::vector<int32_t> culture = polity_founding_culture(h.lapse, seat, first);
            wedge.assign(culture.size(), -1);
            for (std::size_t p = 0; p < culture.size(); ++p)
                if (culture[p] >= 0 && static_cast<std::size_t>(culture[p]) < h.culture_wedge.size())
                    wedge[p] = h.culture_wedge[static_cast<std::size_t>(culture[p])];
        }
        polity_identity_input in;
        in.rec          = &h.lapse;
        in.region_nbrs  = &nbrs;
        in.family       = h.family_count > 0 ? &wedge : nullptr;
        in.family_count = h.family_count;
        in.pins         = has_pins ? &pins : nullptr;
        const polity_identity id = assign_polity_identity(in);
        const polity_pins next   = pins_from(id, h.lapse, has_pins ? &pins : nullptr);
        pins      = next;
        has_pins  = true;
        last_slot = pins.slot;
        last_rung = pins.rung;
    }
    realm.assign(last_slot.size(), 0u);
    std::size_t coloured = 0;
    for (std::size_t p = 0; p < last_slot.size(); ++p)
        if (last_slot[p] >= 0)
        {
            realm[p] = static_cast<uint32_t>(ui::palette::polity_slot_colour(
                last_slot[p], family_count, p < last_rung.size() ? last_rung[p] : 0));
            ++coloured;
        }
    std::printf("[identity] realm colours derived from the report: %zu of %zu realms over %d record(s), "
                "%d families, %d x %d raster, %zu regions at the last close\n",
                coloured, last_slot.size(), records, family_count, gw, gh, regions_last);
    return realm;
}

/// Is the wizard round @p h holds THIS report's own record? A round's record
/// is a copy of the homeworld's time-lapse for that round, so the descriptor
/// (start year, span, region count), the list sizes and the realm name table
/// all agree when it is, and a record of another world fails on the first of
/// them that differs. Cheap: sizes and one string-vector compare.
bool lapse_is_reports_own(const ui::history_lapse& h, int lapse_index, const generation_report& rep)
{
    const generation_report::body_entry* home = nullptr;
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld) { home = &b; break; }
    if (home == nullptr) return false;
    const era_timelapse& t = lapse_index == 3 ? home->industrialisation_timelapse
                           : lapse_index == 2 ? home->exploration_timelapse
                                              : home->prehistory_timelapse;
    const era_timelapse& r = h.lapse;
    return r.start_year == t.start_year && r.years == t.years
        && r.region_stride == t.region_stride
        && r.changes.size() == t.changes.size() && r.events.size() == t.events.size()
        && r.samples.size() == t.samples.size() && r.steps.size() == t.steps.size()
        && r.polity_name == t.polity_name;
}

} // namespace

bool app::pin_realm_colours_from_wizard(const generation_report* rep)
{
    // The last landed polity round holds the colours the player watched:
    // that table, verbatim, at its close. Nothing when the wizard did not run
    // (a cold Begin, `--autostart`), and the report path below fills in.
    for (int i = wizard_lapse_round_count - 1; i >= 1; --i)
    {
        // A ROUND STILL RUNNING holds a partial record (the tap's), and a
        // partial realm table is not the one the player watched: on the
        // vouched Begin path the carve would otherwise wear round 6's
        // half-built table instead of round 5's whole one (the second cold
        // review). Only a LANDED round is read here.
        if (m_wiz_history_future[i].valid()) continue;
        // A landed round the player never drew is derived here as at the
        // hand-over: its slots are the derivation's.
        derive_lapse_for_handover(m_wiz_history[i], i + wizard_planetology_round_count + 1,
                                  m_wiz_surface, m_wiz_terrain);
        const ui::history_lapse& h = m_wiz_history[i];
        if (h.empty() || !h.derived() || h.owners_are_cultures || h.polity_slot.empty()) continue;
        // THE RECORD MUST BE THIS WORLD'S (the cold review's finding on
        // BL-1089). The wizard's records survive a Begin, a load and a trip
        // through the menu, and nothing else clears them; a save of another
        // world loaded after a wizard run would otherwise index that world's
        // nations into this one's slots, and the console would still say
        // "pinned from the realms". The round's record is a copy of the
        // report's own time-lapse, so equality on its descriptor and its
        // tables is the test; a mismatch on the last landed round means the
        // rounds below it are the same session's, so nothing is trusted.
        if (rep != nullptr && !lapse_is_reports_own(h, i, *rep))
        {
            std::printf("[identity] the wizard's round %d record is not this world's "
                        "(start %d, %d years, %d regions, %zu realms named): its realm table "
                        "is not used; the report's own derivation stands\n",
                        i + wizard_planetology_round_count + 1, h.lapse.start_year, h.lapse.years,
                        h.lapse.region_stride, h.lapse.polity_name.size());
            std::fflush(stdout);
            return false;
        }
        const int end = h.lapse.start_year + h.lapse.years;
        std::vector<uint32_t> realm(h.polity_slot.size(), 0u);
        for (std::size_t p = 0; p < h.polity_slot.size(); ++p)
            if (h.polity_slot[p] >= 0)
                realm[p] = ui::lapse_owner_colour(h, static_cast<uint16_t>(p), end);
        ui::palette::set_realm_colour_table(realm);
        return true;
    }
    return false;
}

void app::pin_nation_colours_from_report()
{
    ui::palette::clear_nation_colour_table();
    const generation_report::body_entry* home = nullptr;
    for (const generation_report::body_entry& b : m_generation_report.bodies)
        if (b.is_homeworld) { home = &b; break; }
    if (home == nullptr || home->nation_ids.empty()) return;

    // The wizard's own colours where it ran; the report's derivation otherwise
    // (a load, or a cold Begin). One rule, two sources of the same numbers —
    // and where both exist they are COMPARED (the review's fix round): the
    // equality R2 promises between the round the player watched and the
    // table a load re-derives is a count printed here, not an assertion.
    // The derivation is three rasters and three assignments, cheap at Begin.
    const bool wizard_table = pin_realm_colours_from_wizard(&m_generation_report);
    const std::vector<uint32_t> derived =
        derive_realm_colours(m_generation_report, *home, m_world, wizard_lapse_round_count);
    if (!wizard_table)
        ui::palette::set_realm_colour_table(derived);
    else
    {
        std::size_t compared = 0, differ = 0, one_sided = 0;
        for (std::size_t p = 0; p < derived.size(); ++p)
        {
            bool found = false;
            const ImU32 c = ui::palette::realm_colour(static_cast<int>(p), &found);
            const bool has_derived = derived[p] != 0u;
            if (!found && !has_derived) continue;
            if (found != has_derived) { ++one_sided; continue; }
            ++compared;
            if (static_cast<uint32_t>(c) != derived[p]) ++differ;
        }
        std::printf("[identity] the wizard's realm table against the report's derivation: "
                    "%zu of %zu realms differ, %zu held by one side only\n",
                    differ, compared, one_sided);
        std::fflush(stdout);
    }

    std::vector<std::pair<entity_id, ImU32>> table;
    table.reserve(home->nation_ids.size());
    int pinned = 0, ownerless = 0;
    std::vector<std::size_t> ownerless_idx;
    for (std::size_t n = 0; n < home->nation_ids.size() && n < home->nation_polity.size(); ++n)
    {
        const int32_t pol = home->nation_polity[n];
        if (pol < 0) { ++ownerless; ownerless_idx.push_back(n); continue; }
        bool found = false;
        const ImU32 c = ui::palette::realm_colour(pol, &found);
        if (!found) continue;
        table.emplace_back(home->nation_ids[n], c);
        ++pinned;
    }

    // OWNERLESS GROUND TAKES A SLOT THE SAME RULE HANDS IT AGAINST ITS
    // NEIGHBOURS (NATION_GENERATION.md § Pass 5): a nation no realm founded
    // has no wedge to draw from, so it takes the lowest fallback-table slot
    // whose colour no nation adjacent to it on the carve already wears —
    // the greedy walk the realms had, over the world's own tile ownership.
    // In ascending nation-index order, so the walk is deterministic.
    if (!ownerless_idx.empty())
    {
        const int gw = home_grid_width, gh = home_grid_height; // the world's grid, as above
        const std::vector<entity_id>& grid = body_tile_grid(m_world, home->id);
        std::vector<int32_t> tile_nation; // raster -> nation index, -1 none
        std::vector<std::vector<int32_t>> nbrs(home->nation_ids.size());
        if (gw > 0 && gh > 0 && grid.size() == static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh))
        {
            std::vector<std::pair<entity_id, int32_t>> by_id;
            by_id.reserve(home->nation_ids.size());
            for (std::size_t n = 0; n < home->nation_ids.size(); ++n)
                by_id.emplace_back(home->nation_ids[n], static_cast<int32_t>(n));
            std::sort(by_id.begin(), by_id.end());
            const auto index_of = [&](entity_id id) -> int32_t {
                const auto it = std::lower_bound(by_id.begin(), by_id.end(), std::make_pair(id, int32_t{0}),
                                                 [](const auto& a, const auto& b) { return a.first < b.first; });
                return (it != by_id.end() && it->first == id) ? it->second : -1;
            };
            tile_nation.assign(grid.size(), -1);
            for (std::size_t k = 0; k < grid.size(); ++k)
            {
                const auto nit = m_world.tile_to_nation.find(grid[k]);
                if (nit != m_world.tile_to_nation.end()) tile_nation[k] = index_of(nit->second);
            }
            nbrs = region_adjacency(tile_nation, gw, gh, home->nation_ids.size());
        }
        // Colour by nation index, for the adjacency test (realms already pinned).
        std::vector<ImU32> colour_of(home->nation_ids.size(), 0u);
        for (const auto& [id, c] : table)
        {
            for (std::size_t n = 0; n < home->nation_ids.size(); ++n)
                if (home->nation_ids[n] == id) { colour_of[n] = c; break; }
        }
        for (const std::size_t n : ownerless_idx)
        {
            ImU32 chosen = 0u;
            for (int slot = 0; slot < polity_fallback_slot_count && chosen == 0u; ++slot)
            {
                const ImU32 c = ui::palette::lapse_polity_colour(slot);
                bool used = false;
                if (n < nbrs.size())
                    for (const int32_t nb : nbrs[n])
                        if (nb >= 0 && static_cast<std::size_t>(nb) < colour_of.size() && colour_of[static_cast<std::size_t>(nb)] == c)
                        { used = true; break; }
                if (!used) chosen = c;
            }
            if (chosen == 0u) chosen = ui::palette::lapse_polity_colour(static_cast<int>(n)); // every slot taken: the index's
            colour_of[n] = chosen;
            table.emplace_back(home->nation_ids[n], chosen);
        }
    }
    ui::palette::set_nation_colour_table(table);
    std::printf("[identity] nation colours pinned from the realms: %d of %zu nations, %d of ownerless ground\n",
                pinned, home->nation_ids.size(), ownerless);
    std::fflush(stdout);
}
