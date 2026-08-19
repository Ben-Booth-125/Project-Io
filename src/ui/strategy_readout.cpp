#include "strategy_readout.hpp"

#include "charts.hpp"         // draw_stacked_band / draw_value_bar
#include "foldout_column.hpp" // the shell-column host this ledger draws into
#include "presentation.hpp"   // palette, corp_identity_colour
#include "world/corp_ai.hpp"  // corp_verb_label / corp_decision_reason_label / bucket_for_reason
#include "world/world.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace ui {

namespace {

// ---------------------------------------------------------------------------
// The three priority buckets, as colour and word
// ---------------------------------------------------------------------------
//
// The bucket is the single most legible health signal the decision stream
// carries (BL-411's design): a corp spending its run in must-have is defending
// its solvency; one living in nice-to-have is expanding unopposed. The colours
// say exactly that — negative / pinned / positive — so the band reads as a
// health strip without a legend lookup once learned.

constexpr int bucket_count = 3;

ImU32 bucket_colour(int b)
{
    switch (b)
    {
        case 0: return palette::negative; // must-have: solvency defence
        case 1: return palette::pinned;   // should-have: feed the running assets
        default: return palette::positive; // nice-to-have: expansion
    }
}

const char* bucket_word(int b)
{
    switch (b)
    {
        case 0: return "Must-have";
        case 1: return "Should-have";
        default: return "Nice-to-have";
    }
}

const char* bucket_gloss(int b)
{
    switch (b)
    {
        case 0: return "solvency defence";
        case 1: return "feeding running assets";
        default: return "expansion";
    }
}

const char* corp_name(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    return (it != w.corporations.end()) ? it->second.name.c_str() : "Unknown corp";
}

/// Push one sample onto a band series, capped at the window length — the same
/// drop-oldest idiom as plot_history.hpp's push_capped, against this surface's
/// own cap so the two retention windows cannot drift apart silently.
void push_sample(std::vector<float>& series, float v)
{
    series.push_back(v);
    if (series.size() > strategy_window_samples)
        series.erase(series.begin());
}

/// Apply one compact decision to a corp's window counters, in either
/// direction: +1 as it enters the window, -1 as it is evicted. One function
/// for both signs so entry and eviction cannot disagree about what a decision
/// counts toward.
void apply_to_window(strategy_readout_state::corp_profile& p,
                     const strategy_readout_state::compact_decision& c, int sign)
{
    if (c.verb < corp_verb_count)
        p.verbs[c.verb] += static_cast<std::uint32_t>(sign);
    if (c.reason < corp_decision_reason_count)
    {
        p.reasons[c.reason] += static_cast<std::uint32_t>(sign);
        const int b = static_cast<int>(
            bucket_for_reason(static_cast<corp_decision_reason>(c.reason)));
        if (b >= 0 && b < bucket_count)
            p.buckets[static_cast<std::size_t>(b)] += static_cast<std::uint32_t>(sign);
    }
    p.window_total += static_cast<std::uint32_t>(sign);
}

// ---------------------------------------------------------------------------
// Draw helpers
// ---------------------------------------------------------------------------

/// One full-width horizontal share bar split into the three bucket segments —
/// the all-corporations comparison row. Track first, so an empty window still
/// shows a rail rather than nothing.
void draw_bucket_share_bar(float width, const std::array<std::uint32_t, 3>& buckets)
{
    const float  h  = 9.0f;
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    ImDrawList*  dl = ImGui::GetWindowDrawList();

    ImGui::Dummy({width, h});
    dl->AddRectFilled({p.x, p.y}, {p.x + width, p.y + h}, IM_COL32(255, 255, 255, 28), h * 0.5f);

    float total = 0.0f;
    for (int b = 0; b < bucket_count; ++b)
        total += static_cast<float>(buckets[static_cast<std::size_t>(b)]);
    if (total <= 0.0f)
        return;

    float x = p.x;
    for (int b = 0; b < bucket_count; ++b)
    {
        const float share = static_cast<float>(buckets[static_cast<std::size_t>(b)]) / total;
        if (share <= 0.0f)
            continue;
        const float seg = width * share;
        dl->AddRectFilled({x, p.y}, {x + seg, p.y + h}, bucket_colour(b), h * 0.5f);
        x += seg;
    }
}

/// The bucket legend: three swatches with their words (and optional counts).
/// Drawn once per view rather than per row — the colours are the vocabulary.
void draw_bucket_legend(const std::array<std::uint32_t, 3>* counts)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int b = 0; b < bucket_count; ++b)
    {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled({p.x, p.y + 3.0f}, {p.x + 9.0f, p.y + 12.0f}, bucket_colour(b), 2.0f);
        ImGui::Dummy({11.0f, ImGui::GetTextLineHeight()});
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        if (counts)
            ImGui::Text("%s (%s) - %u", bucket_word(b), bucket_gloss(b),
                        (*counts)[static_cast<std::size_t>(b)]);
        else
            ImGui::Text("%s (%s)", bucket_word(b), bucket_gloss(b));
        ImGui::PopStyleColor();
    }
}

/// One labelled count row: the label on its own line, the magnitude bar under
/// it with the count printed inside (draw_value_bar never elides it). Stacked
/// rather than side-by-side because the canonical labels are PHRASES ("stock
/// piled up past the hold threshold") and this surface must not keep a second,
/// shorter word table — that is exactly the drift BL-420 removed. The
/// narrow-list idiom otherwise: draw_bars' gutter + legend cannot fit the
/// fold-out column (charts.hpp).
void draw_count_row(const char* label, std::uint32_t count, std::uint32_t ceiling,
                    ImU32 colour)
{
    ImGui::TextWrapped("%s", label);

    const ImVec2 p  = ImGui::GetCursorScreenPos();
    const float  bw = std::max(40.0f, ImGui::GetContentRegionAvail().x - 4.0f);
    const float  bh = ImGui::GetTextLineHeight(); // the printed count sits inside the bar
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    ImGui::Dummy({bw, bh + 2.0f});
    charts::draw_value_bar(dl, {p.x, p.y}, {p.x + bw, p.y + bh},
                           static_cast<float>(count),
                           static_cast<float>(std::max<std::uint32_t>(ceiling, 1u)),
                           colour, "%.0f");
}

/// Nonzero counters of an enum-indexed array, sorted count-descending with the
/// enum order breaking ties — a total order, so the display is deterministic.
std::vector<std::pair<std::uint32_t, std::size_t>>
ranked_nonzero(const std::uint32_t* counts, std::size_t n)
{
    std::vector<std::pair<std::uint32_t, std::size_t>> out;
    for (std::size_t i = 0; i < n; ++i)
        if (counts[i] > 0)
            out.emplace_back(counts[i], i);
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second < b.second;
    });
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// The recorder — once per econ tick, O(new decisions)
// ---------------------------------------------------------------------------

void record_strategy_readout(const world& w, strategy_readout_state& s)
{
    const corp_decision_ring& ring = w.ai_decisions;

    // The ring is derived observability, not save-format state: a new game or
    // a load restarts it at zero. A lifetime count BELOW what we already
    // consumed can only mean that restart happened, so the aggregation starts
    // over rather than underflowing into a 2^64 "fresh" count.
    if (ring.total < s.consumed)
        s = {};

    const std::uint64_t sample = s.sample_index++;

    // --- Consume what was pushed since the last record ----------------------
    std::size_t fresh = ring.total - s.consumed;
    const std::size_t n = ring.entries.size();
    if (fresh > n)
    {
        // More decisions than the ring holds arrived between two records: the
        // overwritten ones are gone unobserved. Practically unreachable at one
        // record per econ tick (a tick pushes far fewer than 256), but if it
        // ever happens the count is DISCLOSED on the surface, not absorbed.
        s.missed += fresh - n;
        fresh = n;
    }

    if (fresh > 0)
    {
        // Chronological position of the oldest unconsumed entry (same layout
        // reasoning as the decision feed's ring_in_order): until the ring
        // wraps, index order is chronological from 0; once full, it runs from
        // `next` (the slot about to be overwritten = the oldest entry).
        const std::size_t start = (n >= corp_decision_ring::capacity) ? (ring.next % n) : 0;
        const std::size_t begin = (start + n - fresh) % n;

        for (std::size_t k = 0; k < fresh; ++k)
        {
            const corp_decision& d = ring.entries[(begin + k) % n];

            strategy_readout_state::compact_decision c;
            c.sample = sample;
            c.corp   = d.corp;
            c.verb   = static_cast<std::uint8_t>(d.command.verb);
            c.reason = static_cast<std::uint8_t>(d.reason);
            // NO SCORE FIELDS are carried across — winning_score/runner_up
            // stay on the ring entry (NR-226 fence; see the header).

            s.window.push_back(c);
            auto& p = s.corps[c.corp];
            apply_to_window(p, c, +1);
            ++p.lifetime_total;
            ++s.observed;
        }
    }
    s.consumed = ring.total;

    // --- Evict what fell out of the rolling window --------------------------
    while (!s.window.empty()
           && s.window.front().sample + strategy_window_samples <= sample)
    {
        const auto& c = s.window.front();
        const auto it = s.corps.find(c.corp);
        if (it != s.corps.end())
            apply_to_window(it->second, c, -1);
        s.window.pop_front();
    }

    // --- Advance every corp's band series one sample -------------------------
    // A corp that decided nothing this quarter pushes zeros: the band shows an
    // honest gap for that sample rather than stretching its last activity.
    for (auto& [id, p] : s.corps)
    {
        std::array<float, 3> this_sample{};
        for (auto rit = s.window.rbegin(); rit != s.window.rend(); ++rit)
        {
            if (rit->sample != sample)
                break; // the deque is sample-ordered; the tail is this sample
            if (rit->corp != id || rit->reason >= corp_decision_reason_count)
                continue;
            const int b = static_cast<int>(
                bucket_for_reason(static_cast<corp_decision_reason>(rit->reason)));
            if (b >= 0 && b < bucket_count)
                this_sample[static_cast<std::size_t>(b)] += 1.0f;
        }
        for (int b = 0; b < bucket_count; ++b)
            push_sample(p.bucket_series[static_cast<std::size_t>(b)],
                        this_sample[static_cast<std::size_t>(b)]);
    }
}

// ---------------------------------------------------------------------------
// The ledger
// ---------------------------------------------------------------------------

void draw_strategy_readout(const world& w, const strategy_readout_state& s,
                           ui_state& st, bool* open)
{
    if (!open || !*open)
        return;

    if (!foldout_begin("Strategy readout"))
    {
        foldout_end();
        return;
    }

    if (s.observed == 0)
    {
        ImGui::TextWrapped("No strategic decisions observed yet. The readout "
                           "aggregates the same stream the AI decision feed "
                           "lists, one sample per quarter.");
        foldout_end();
        return;
    }

    // --- Corp selector -------------------------------------------------------
    // Lives in ui_state, not a static, so the filter survives selection changes
    // elsewhere (the decision feed's precedent). The list is the corps that
    // have ever decided anything — s.corps is a std::map, so the menu order is
    // deterministic by construction.
    if (st.strategy_readout_corp != null_entity
        && s.corps.find(st.strategy_readout_corp) == s.corps.end())
        st.strategy_readout_corp = null_entity;

    const float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetNextItemWidth(avail * 0.62f);
    if (ImGui::BeginCombo("Corp", st.strategy_readout_corp == null_entity
                                      ? "All corporations"
                                      : corp_name(w, st.strategy_readout_corp)))
    {
        if (ImGui::Selectable("All corporations", st.strategy_readout_corp == null_entity))
            st.strategy_readout_corp = null_entity;
        for (const auto& [c, p] : s.corps)
        {
            const bool sel = (c == st.strategy_readout_corp);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  palette::corp_identity_colour(c, w.player_entity));
            if (ImGui::Selectable(corp_name(w, c), sel))
                st.strategy_readout_corp = c;
            ImGui::PopStyleColor();
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // --- Provenance ----------------------------------------------------------
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    ImGui::TextWrapped("%llu decisions observed; the window holds the last %zu "
                       "quarters (%zu decisions).",
                       static_cast<unsigned long long>(s.observed),
                       strategy_window_samples, s.window.size());
    if (s.missed > 0)
        ImGui::TextWrapped("%llu decisions passed unobserved (the ring wrapped "
                           "between samples) and are missing from every figure here.",
                           static_cast<unsigned long long>(s.missed));
    ImGui::PopStyleColor();
    ImGui::Separator();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    if (st.strategy_readout_corp == null_entity)
    {
        // --- All corporations: the bucket-split comparison ------------------
        // One share bar per corp. This is the at-a-glance health read the
        // buckets were chosen for: mostly-red is defending solvency,
        // mostly-green is expanding unopposed. Counts are printed under each
        // bar because a share hides the volume (10% of 300 beats 100% of 2).
        draw_bucket_legend(nullptr);
        ImGui::Separator();

        std::size_t active = 0;
        for (const auto& [c, p] : s.corps)
        {
            if (p.window_total == 0)
                continue;
            ++active;
            ImGui::PushID(static_cast<int>(c));

            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                                   palette::corp_identity_colour(c, w.player_entity)),
                               "%s", corp_name(w, c));
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
            ImGui::Text("- %u decisions", p.window_total);
            ImGui::PopStyleColor();

            draw_bucket_share_bar(ImGui::GetContentRegionAvail().x - 4.0f, p.buckets);

            ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
            ImGui::Text("%u / %u / %u", p.buckets[0], p.buckets[1], p.buckets[2]);
            ImGui::PopStyleColor();

            ImGui::Separator();
            ImGui::PopID();
        }
        if (active == 0)
            ImGui::TextWrapped("No decisions inside the current window. Older "
                               "activity exists - select a corporation to see "
                               "its lifetime count.");
    }
    else
    {
        // --- One corporation: the full profile -------------------------------
        const auto& p = s.corps.at(st.strategy_readout_corp);

        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               palette::corp_identity_colour(st.strategy_readout_corp,
                                                             w.player_entity)),
                           "%s", corp_name(w, st.strategy_readout_corp));
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        ImGui::Text("%u decisions in window, %llu observed lifetime",
                    p.window_total, static_cast<unsigned long long>(p.lifetime_total));
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Bucket split, quarter by quarter — the stacked band. The transition
        // is the artifact worth seeing: the quarter a corp falls out of
        // nice-to-have and never recovers is the quarter it lost.
        ImGui::TextUnformatted("Bucket split by quarter");
        {
            const ImVec2 bp = ImGui::GetCursorScreenPos();
            const float  bw = std::max(60.0f, ImGui::GetContentRegionAvail().x - 4.0f);
            const float  bh = 56.0f;
            ImGui::Dummy({bw, bh});

            charts::band_layer layers[bucket_count];
            for (int b = 0; b < bucket_count; ++b)
            {
                const auto& series = p.bucket_series[static_cast<std::size_t>(b)];
                layers[b].values = series.empty() ? nullptr : series.data();
                layers[b].count  = series.size();
                layers[b].colour = bucket_colour(b);
            }
            dl->AddRectFilled({bp.x, bp.y}, {bp.x + bw, bp.y + bh},
                              IM_COL32(255, 255, 255, 14));
            charts::draw_stacked_band(dl, {bp.x, bp.y}, {bp.x + bw, bp.y + bh},
                                      layers, bucket_count);
        }
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        ImGui::TextWrapped("oldest quarter on the left; a gap is a quarter "
                           "with no decisions");
        ImGui::PopStyleColor();
        draw_bucket_legend(&p.buckets);
        ImGui::Separator();

        // Verb mix — what fraction of the window went to each verb. An
        // expander and a consolidator have visibly different mixes; the label
        // vocabulary is corp_verb_label, the same words the feed and the
        // history log narrate with.
        ImGui::TextUnformatted("Verb mix");
        {
            const auto ranked = ranked_nonzero(p.verbs.data(), p.verbs.size());
            const std::uint32_t ceiling = ranked.empty() ? 1u : ranked.front().first;
            for (const auto& [count, idx] : ranked)
                draw_count_row(corp_verb_label(static_cast<corp_verb>(idx)),
                               count, ceiling, palette::hover);
            if (ranked.empty())
                ImGui::TextDisabled("(none in window)");
        }
        ImGui::Separator();

        // Reason tally — which reasons actually fired, each bar tinted by the
        // priority bucket its reason maps to (bucket_for_reason), so the tally
        // teaches the spend-bucket vocabulary instead of sitting beside it.
        ImGui::TextUnformatted("Reasons");
        {
            const auto ranked = ranked_nonzero(p.reasons.data(), p.reasons.size());
            const std::uint32_t ceiling = ranked.empty() ? 1u : ranked.front().first;
            for (const auto& [count, idx] : ranked)
            {
                const auto reason = static_cast<corp_decision_reason>(idx);
                const int  b      = static_cast<int>(bucket_for_reason(reason));
                draw_count_row(corp_decision_reason_label(reason), count, ceiling,
                               bucket_colour(b));
            }
            if (ranked.empty())
                ImGui::TextDisabled("(none in window)");
        }
    }

    foldout_end();
}

} // namespace ui
