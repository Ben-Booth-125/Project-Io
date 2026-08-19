#pragma once

#include "ui_state.hpp"
#include "world/corp_command.hpp" // corp_verb_count / corp_decision_reason_count

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

struct world;

namespace ui {

/// The emergent-strategy readout (BL-411) — a per-corp aggregation over the
/// same corp_decision stream the AI decision feed (BL-407) lists row by row.
/// The feed shows the moves; this shows the SHAPE across a run: verb mix,
/// spend split across the must-have / should-have / nice-to-have priority
/// buckets (`bucket_for_reason`, corp_ai.hpp), and the reason-code tally.
///
/// **Score fields are excluded — deliberately and permanently** (NR-226 fence,
/// 2026-08-19): `runner_up` can legitimately exceed `winning_score`, because
/// candidates sort by priority bucket before score (AI_OPPONENT.md § 10i), so
/// any aggregate over raw margins would launder a bucket artefact into a
/// "conviction" statistic. The readout aggregates verb mix, spend buckets and
/// reason codes only. If margins are ever wanted here, normalise within
/// priority bucket first.
///
/// **No strategy is ever named.** A label like "aggressive expander" rendered
/// from three thresholds would be trusted more than it earns, and would
/// pre-empt STRATEGIES.md's settled position that the deck is discovered, not
/// authored. The readout shows the distributions and lets the shape speak.

/// How many econ-tick samples (one per quarter) the rolling window spans.
/// Matches plot_history_cap so the readout's horizon and the other ledgers'
/// graphs retain the same slice of the run: 64 quarters = 16 in-game years.
inline constexpr std::size_t strategy_window_samples = 64;

/// The rolling aggregation state — app-owned, presentation-only, UNSERIALISED
/// (the same lifetime as the balance history: outside the save seam, survives
/// the session only). Updated ONCE PER ECON TICK by record_strategy_readout —
/// never per frame — by consuming only the ring entries pushed since the last
/// call, so the per-tick cost is O(new decisions), not O(ring).
struct strategy_readout_state
{
    /// One decision reduced to what the readout aggregates: who, which verb,
    /// which reason, at which sample. NO SCORES — see the header note.
    struct compact_decision
    {
        std::uint64_t sample = 0;   ///< The window-eviction clock (econ-tick index).
        entity_id     corp   = null_entity;
        std::uint8_t  verb   = 0;   ///< corp_verb value.
        std::uint8_t  reason = 0;   ///< corp_decision_reason value.
    };

    /// Per-corp aggregates. Counters cover the rolling window; the series
    /// cover the last strategy_window_samples quarters (capped in lockstep).
    struct corp_profile
    {
        std::array<std::uint32_t, corp_verb_count>            verbs{};   ///< Window verb mix.
        std::array<std::uint32_t, corp_decision_reason_count> reasons{}; ///< Window reason tally.
        std::array<std::uint32_t, 3> buckets{};      ///< Window count per corp_priority_bucket.
        std::uint32_t                window_total = 0;
        std::uint64_t                lifetime_total = 0; ///< Every decision observed, ever.

        /// Decisions per priority bucket per sample — the stacked band's three
        /// layers, oldest → newest, capped at strategy_window_samples.
        std::array<std::vector<float>, 3> bucket_series;
    };

    /// std::map, not unordered: the draw iterates it directly, and the display
    /// order must be a deterministic function of the data.
    std::map<entity_id, corp_profile> corps;

    std::deque<compact_decision> window;      ///< Decisions inside the rolling window.
    std::uint64_t                sample_index = 0; ///< Econ ticks recorded (the window clock).
    std::size_t                  consumed = 0;     ///< ring.total consumed so far.
    std::uint64_t                observed = 0;     ///< Decisions this state has aggregated.
    std::uint64_t                missed   = 0;     ///< Pushed but never observed (ring wrapped between records; disclosed, not hidden).
};

/// Consume the decisions pushed since the last call and advance the window one
/// sample. Call once per econ tick, from the step_economy recorder block —
/// alongside record_histories, NOT from the draw path. Self-healing: a world
/// whose ring restarted (new game, load — the ring is not save-format state)
/// resets the aggregation rather than underflowing.
void record_strategy_readout(const world& w, strategy_readout_state& s);

/// Draw the Strategy readout ledger. Read-only over `w` and `s`; mutates
/// nothing but its own ui_state filter (`strategy_readout_corp`).
void draw_strategy_readout(const world& w, const strategy_readout_state& s,
                           ui_state& st, bool* open);

} // namespace ui
