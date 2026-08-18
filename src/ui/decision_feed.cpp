#include "decision_feed.hpp"

#include "foldout_column.hpp" // the shell-column host this ledger draws into
#include "format.hpp"         // fmt::date_from_day / month_abbrev / ordinal_day
#include "market_ledger.hpp"  // market_city_name (BL-452: dispatch_convoy names two markets)
#include "presentation.hpp"   // palette, building_type_name, resource_name
#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace ui {

namespace {

// ---------------------------------------------------------------------------
// Vocabulary
// ---------------------------------------------------------------------------
//
// corp_ai.cpp already has `corp_verb_label` and `corp_decision_reason_label`,
// and these two functions are word-for-word the same phrasing. They are
// duplicated rather than shared because those live in corp_ai.cpp's ANONYMOUS
// namespace — nothing exports them, and this item does not own that file.
//
// The duplication is therefore a known cost, not an oversight: if a verb or a
// reason is added, or its wording is changed, BOTH tables must move or the feed
// will narrate a decision differently from the world history log that records
// the same decision. Hoisting the pair into corp_ai.hpp is the fix; it belongs
// to whoever next has corp_ai.cpp open.

const char* verb_label(corp_verb v)
{
    switch (v)
    {
        case corp_verb::build:              return "build";
        case corp_verb::demolish:           return "demolish";
        case corp_verb::set_recipe:         return "recipe change";
        case corp_verb::set_workforce:      return "workforce change";
        case corp_verb::idle:               return "idle";
        case corp_verb::resume:             return "resume";
        case corp_verb::place_road:         return "road placement";
        case corp_verb::survey:             return "survey";
        case corp_verb::hire_unit:          return "hire unit";
        case corp_verb::place_sell_order:   return "sell order";
        case corp_verb::remove_sell_order:  return "sell-order withdrawal";
        case corp_verb::set_workforce_auto: return "workforce auto";
        case corp_verb::request_quote:      return "quote request";
        case corp_verb::accept_quote:       return "quote acceptance";
        case corp_verb::cancel_contract:    return "contract cancellation";
        case corp_verb::dispatch_convoy:    return "convoy dispatch";
        case corp_verb::hold_convoy:        return "convoy hold";
    }
    return "action";
}

const char* reason_label(corp_decision_reason r)
{
    switch (r)
    {
        case corp_decision_reason::best_build:     return "best available build site";
        case corp_decision_reason::dial_workforce: return "workforce margin";
        case corp_decision_reason::dial_recipe:    return "recipe margin";
        case corp_decision_reason::dial_idle:      return "sustained losses";
        case corp_decision_reason::dial_resume:    return "now profitable";
        case corp_decision_reason::survey_expand:  return "discovery within budget";
        case corp_decision_reason::hire_available: return "roster row available";
        case corp_decision_reason::trade_surplus:  return "stock past the hold threshold";
    }
    return "unspecified";
}

/// Number of `corp_decision_reason` values, for the filter combo. The enum has
/// no count sentinel, so this mirrors it by hand and must grow with it — an
/// out-of-date value silently hides the newest reason from the filter.
constexpr int k_reason_count = 8;

// ---------------------------------------------------------------------------
// Naming the target
// ---------------------------------------------------------------------------

const char* corp_name(const world& w, entity_id corp)
{
    const auto it = w.corporations.find(corp);
    return (it != w.corporations.end()) ? it->second.name.c_str() : "Unknown corp";
}

const char* body_name(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    return (it != w.bodies.end()) ? it->second.name.c_str() : "an unknown body";
}

/// "(12, 7) on Kadru" for a tile, or a plain marker when the tile is gone.
/// A decision's tile CAN have been demolished or otherwise outlived by the time
/// the feed reads it — the ring holds 256 decisions, and the world moves on —
/// so every lookup here degrades to a phrase rather than asserting.
std::string tile_label(const world& w, entity_id tile)
{
    const auto it = w.tiles.find(tile);
    if (it == w.tiles.end())
        return "a tile no longer on the map";

    char buf[64];
    std::snprintf(buf, sizeof buf, "(%d, %d) on ", it->second.grid_x, it->second.grid_y);
    return std::string(buf) + body_name(w, it->second.body);
}

/// The building's type and where it stands — "Extraction site on Kadru".
std::string building_label(const world& w, entity_id building)
{
    const auto it = w.buildings.find(building);
    if (it == w.buildings.end())
        return "a building since removed";

    std::string s = building_type_name(it->second.type);
    const auto tit = w.tiles.find(it->second.tile);
    if (tit != w.tiles.end())
    {
        s += " on ";
        s += body_name(w, tit->second.body);
    }
    return s;
}

/// What the command actually acted on, in words. Which argument carries the
/// subject depends on the verb (corp_command.hpp documents the split), so this
/// is a switch over the verb rather than a single field read.
std::string target_label(const world& w, const corp_command& c)
{
    char buf[96];

    switch (c.verb)
    {
        case corp_verb::build:
        {
            std::string s = building_type_name(c.type);
            // An extraction site's identity is the resource it takes, not its
            // type — two iron mines and a copper mine are one decision repeated
            // and one decision that is different.
            if (c.type == building_type::extraction_site)
            {
                s += " (";
                s += resource_name(c.target);
                s += ")";
            }
            s += " at ";
            s += tile_label(w, c.tile);
            return s;
        }

        case corp_verb::demolish:
        case corp_verb::idle:
        case corp_verb::resume:
            return building_label(w, c.subject);

        case corp_verb::set_recipe:
            // The recipe is an index into the recipe registry, and this surface
            // is not handed one (the signature takes `world` alone, deliberately
            // — see decision_feed.hpp). Printing the index is honest; inventing
            // a name is not.
            std::snprintf(buf, sizeof buf, " to recipe #%u", static_cast<unsigned>(c.recipe));
            return building_label(w, c.subject) + buf;

        case corp_verb::set_workforce:
            std::snprintf(buf, sizeof buf, " to %d%%", c.workforce);
            return building_label(w, c.subject) + buf;

        case corp_verb::set_workforce_auto:
            return building_label(w, c.subject) + " (dial handed back to the solver)";

        case corp_verb::place_road:
            std::snprintf(buf, sizeof buf, "tier %u road at ", static_cast<unsigned>(c.road_tier));
            return buf + tile_label(w, c.tile);

        case corp_verb::survey:
            return body_name(w, c.subject);

        case corp_verb::hire_unit:
        {
            const auto& roster = unit_roster_table();
            const char* row = (c.unit_type < roster.size())
                                  ? roster[c.unit_type].name
                                  : "an unlisted roster row";
            return std::string(row) + " at " + tile_label(w, c.tile);
        }

        case corp_verb::place_sell_order:
            // `subject` is the body for this verb, and `target` is the good.
            std::snprintf(buf, sizeof buf, " x%.0f at floor %.2f on ", c.quantity, c.floor_price);
            return std::string(resource_name(c.target)) + buf + body_name(w, c.subject);

        case corp_verb::remove_sell_order:
            std::snprintf(buf, sizeof buf, "order #%u", static_cast<unsigned>(c.order));
            return buf;

        case corp_verb::request_quote:
            std::snprintf(buf, sizeof buf, " x%.0f from ", c.quantity);
            return std::string(resource_name(c.target)) + buf + corp_name(w, c.counterparty);

        case corp_verb::accept_quote:
            std::snprintf(buf, sizeof buf, "quote #%u", static_cast<unsigned>(c.order));
            return buf;

        case corp_verb::cancel_contract:
            std::snprintf(buf, sizeof buf, "contract #%u", static_cast<unsigned>(c.order));
            return buf;

        // BL-452. `subject` is the SOURCE market and `counterparty` the
        // destination — the one verb where counterparty names a market rather
        // than a corporation, so it gets market_city_name, not corp_name.
        case corp_verb::dispatch_convoy:
            std::snprintf(buf, sizeof buf, " x%.0f from ", c.quantity);
            return std::string(resource_name(c.target)) + buf
                 + market_city_name(w, c.subject) + " to "
                 + market_city_name(w, c.counterparty);

        case corp_verb::hold_convoy:
            std::snprintf(buf, sizeof buf, "convoy #%u", static_cast<unsigned>(c.order));
            return buf;
    }
    return "-";
}

/// The campaign calendar's rendering of a sim day tick, matching the time
/// panel's wording ("1960 Jan 12th") so two surfaces never name one day twice.
std::string date_label(int64_t tick)
{
    // Every decision/agency timestamp is a sim day tick and non-negative by
    // construction (world.hpp § world_history_entry), but date_from_day takes an
    // unsigned, so a corrupt negative would wrap into the far future rather than
    // read as wrong. Clamp instead.
    const uint64_t day = (tick > 0) ? static_cast<uint64_t>(tick) : 0u;
    const fmt::calendar_date d = fmt::date_from_day(day);

    char buf[64];
    std::snprintf(buf, sizeof buf, "%d %s %s", d.year, fmt::month_abbrev(d.month),
                  fmt::ordinal_day(d.day).c_str());
    return buf;
}

// ---------------------------------------------------------------------------
// The margin — why this surface exists
// ---------------------------------------------------------------------------

/// The winning-vs-runner-up pair reduced to one 0..1 "conviction" scalar plus a
/// phrase and a colour, so the reader sees how close the call was without doing
/// the subtraction.
struct margin_read
{
    float       conviction  = 0.0f; ///< 0 = a dead heat; 1 = a walkover.
    const char* phrase      = "";
    ImU32       colour      = palette::neutral;
    bool        uncontested = false; ///< No second candidate existed at all.
};

margin_read read_margin(float win, float run)
{
    margin_read m;

    // Since NR-232 `runner_up` is THE BEST OPTION THIS CORP DID NOT TAKE in the
    // evaluation — not, as before, whatever candidate happened to sort next,
    // which was often a command that also ran. So the pair is now a genuine
    // counterfactual and the phrasing below can be taken at face value.
    //
    // 0 means nothing was passed up: every enumerated candidate was acted on.
    // A walkover, not a rival that scored zero, so it gets its own phrase —
    // a full bar against a "0.00" opponent would state that an option existed
    // and lost badly.
    if (run <= 0.0f)
    {
        m.conviction  = 1.0f;
        m.phrase      = "uncontested";
        m.colour      = palette::positive;
        m.uncontested = true;
        return m;
    }

    // The foregone option can still OUT-SCORE what was taken, and this is now
    // the most interesting row in the feed rather than an artefact. It means
    // the corp passed up a higher-scoring option because something other than
    // score decided: a priority bucket (a lower bucket may never starve a
    // higher one), an action budget, or the solvency floor. Reporting a
    // negative margin would read as "the scorer picked the weaker option",
    // which understates it — the scorer picked the more URGENT or the only
    // AFFORDABLE one, deliberately.
    if (run >= win)
    {
        m.conviction = 0.0f;
        m.phrase     = "overridden";
        m.colour     = palette::pinned;
        return m;
    }

    // Share of the pair, not the raw difference. Candidate scores carry focus
    // weights and predictive multipliers, so their absolute magnitude is not a
    // fixed 0..1 scale and a bare delta is not comparable between two rows —
    // 0.02 apart means something very different at 0.05 than at 5.0. The share
    // sits in (0.5, 1]; rescaling makes a dead heat empty and a walkover full,
    // on any score domain.
    const float share = win / (win + run);
    m.conviction = std::clamp((share - 0.5f) * 2.0f, 0.0f, 1.0f);

    // Red at the bottom of the range flags FRAGILITY, not failure: a decision
    // the tuning could have flipped is the one worth looking at, and the eye
    // should be pulled to it rather than to the walkovers.
    if (m.conviction < 0.10f)      { m.phrase = "coin-flip";  m.colour = palette::negative; }
    else if (m.conviction < 0.35f) { m.phrase = "close call"; m.colour = palette::pinned;   }
    else if (m.conviction < 0.70f) { m.phrase = "clear";      m.colour = palette::text_secondary; }
    else                           { m.phrase = "decisive";   m.colour = palette::positive; }

    return m;
}

/// The conviction bar: a full-width track with the conviction filled from the
/// left. Drawn rather than printed because the point of the column is that a
/// coin-flip and a walkover must be distinguishable at a glance, down a list of
/// hundreds — two floats side by side require reading and subtracting each one.
void draw_conviction_bar(float width, const margin_read& m)
{
    const float  h  = 7.0f;
    const ImVec2 p  = ImGui::GetCursorScreenPos();
    ImDrawList*  dl = ImGui::GetWindowDrawList();

    ImGui::Dummy({width, h});

    const ImVec2 lo{p.x, p.y};
    const ImVec2 hi{p.x + width, p.y + h};

    dl->AddRectFilled(lo, hi, IM_COL32(255, 255, 255, 28), h * 0.5f);

    // A floor on the drawn fill: at conviction 0 an exactly-zero-width rect
    // renders as nothing at all, which reads as "no data" rather than "as close
    // as it gets". The stub keeps the row's state visible.
    const float fill = std::max(h, width * m.conviction);
    dl->AddRectFilled(lo, {p.x + fill, hi.y}, m.colour, h * 0.5f);
}

// ---------------------------------------------------------------------------
// Row assembly
// ---------------------------------------------------------------------------

/// One line of the feed. Either a ring entry (scored, filterable by reason) or
/// a history-log entry recovered from beyond the ring's window (prose only).
struct feed_row
{
    /// Global chronological sequence — pre-ring rows sort before ring rows.
    /// Unique per row, which is what makes the display order a TOTAL order and
    /// therefore deterministic.
    std::size_t seq = 0;

    int64_t   tick = 0;
    entity_id corp = null_entity;

    bool  scored    = false; ///< False for a history-log row: no margin exists.
    float winning   = 0.0f;
    float runner_up = 0.0f;
    int   reason    = -1;    ///< corp_decision_reason value, or -1 when unscored.

    std::string headline; ///< "build - Extraction site (Iron ore) at (12, 7) on Kadru"
    std::string detail;   ///< The reason phrase, or the log's own prose.
};

/// Chronological order of the ring, oldest first.
///
/// The ring is NOT stored newest-first and its layout depends on whether it has
/// wrapped yet: until it fills, `push` appends and leaves `next` at 0, so index
/// order IS chronological order; once full, `next` is the slot about to be
/// overwritten, which is exactly the OLDEST entry, and the sequence runs
/// `next, next+1, ..., capacity-1, 0, ..., next-1`. Reading the vector in index
/// order after a wrap interleaves the newest entries in front of the oldest.
std::vector<const corp_decision*> ring_in_order(const corp_decision_ring& ring)
{
    std::vector<const corp_decision*> out;
    const std::size_t n = ring.entries.size();
    if (n == 0)
        return out;

    // Keyed on "has it wrapped", not on `next != 0`: `next` is only advanced in
    // the full branch, so the two agree today, but the wrap test is the property
    // the layout actually depends on.
    const std::size_t start = (n >= corp_decision_ring::capacity) ? (ring.next % n) : 0;

    out.reserve(n);
    for (std::size_t k = 0; k < n; ++k)
        out.push_back(&ring.entries[(start + k) % n]);
    return out;
}

/// How many pre-ring rows to recover from the history log.
///
/// The log never evicts and grows for the life of a campaign, so "show
/// everything older than the ring" is an unbounded per-frame walk and an
/// unbounded row count. This bound is a display budget, not a claim that older
/// decisions do not exist — the summary line says so explicitly.
constexpr std::size_t k_history_tail = 256;

/// Recover the decisions the ring has already evicted, newest-evicted first.
///
/// The correspondence is positional, not by timestamp: corp_ai.cpp pushes to
/// `ai_decisions` and appends the `decision`-topic log entry in the same
/// unconditional block, so the Nth decision-topic log entry IS the Nth ring
/// push. The ring therefore holds the LAST `entries.size()` of them, and
/// everything before that is what the ring lost. Matching on tick instead would
/// mis-split a tick that straddles the boundary, since several corps decide on
/// the same tick.
///
/// Walks backwards so the cost is O(ring size + tail), not O(campaign length).
///
/// The `agency` topic is deliberately NOT merged in. Its strategic-tier entries
/// are a second narration of the very same decisions — pushed one-for-one in
/// that same block — so merging would duplicate every row; its reflex-tier
/// entries (BL-079, economy_system.cpp) are not scorer decisions at all and
/// carry neither a reason code nor a margin, which is the whole subject here.
std::vector<const world_history_entry*> evicted_decisions(const world& w)
{
    std::vector<const world_history_entry*> out;

    std::size_t skip = w.ai_decisions.entries.size(); // the ones the ring still holds
    for (auto it = w.history_log.rbegin(); it != w.history_log.rend(); ++it)
    {
        if (it->topic != history_topic::decision)
            continue;
        if (skip > 0)
        {
            --skip;
            continue;
        }
        out.push_back(&*it);
        if (out.size() >= k_history_tail)
            break;
    }
    return out;
}

/// Every row the feed can show, in chronological order (oldest first). Filtering
/// and the newest-first sort happen on top of this, so the sequence numbers
/// stamped here stay stable however the view is filtered.
std::vector<feed_row> assemble_rows(const world& w)
{
    const std::vector<const world_history_entry*> older = evicted_decisions(w);
    const std::vector<const corp_decision*>       live  = ring_in_order(w.ai_decisions);

    std::vector<feed_row> rows;
    rows.reserve(older.size() + live.size());

    // `older` came off a reverse iterator, so it is newest-first; walk it
    // backwards to restore chronological order ahead of the ring's rows.
    for (auto it = older.rbegin(); it != older.rend(); ++it)
    {
        const world_history_entry& e = **it;
        feed_row r;
        r.seq      = rows.size();
        r.tick     = e.timestamp;
        r.corp     = e.corp;
        r.scored   = false;
        r.headline = e.event;
        r.detail   = e.consequence;
        rows.push_back(std::move(r));
    }

    for (const corp_decision* d : live)
    {
        feed_row r;
        r.seq       = rows.size();
        r.tick      = d->tick;
        r.corp      = d->corp;
        r.scored    = true;
        r.winning   = d->winning_score;
        r.runner_up = d->runner_up;
        r.reason    = static_cast<int>(d->reason);
        r.headline  = std::string(verb_label(d->command.verb)) + " - "
                      + target_label(w, d->command);
        r.detail    = reason_label(d->reason);
        rows.push_back(std::move(r));
    }

    return rows;
}

} // namespace

// ---------------------------------------------------------------------------
// The ledger
// ---------------------------------------------------------------------------

void draw_decision_feed(const world& w, ui_state& st, bool* open)
{
    if (!open || !*open)
        return;

    if (!foldout_begin("AI decisions"))
    {
        foldout_end();
        return;
    }

    std::vector<feed_row> rows = assemble_rows(w);

    if (rows.empty())
    {
        ImGui::TextWrapped("No strategic decisions yet. The scorer logs one the first "
                           "time a corporation acts.");
        foldout_end();
        return;
    }

    // --- Filters ------------------------------------------------------------
    // Both live in ui_state, not in statics here, so a filter survives a
    // selection change elsewhere in the game (R2): a spectator reading one
    // corp's run should not lose their filter by clicking a tile.

    // The corp list is taken from the ROWS, not from w.corporations: a corp that
    // has never decided anything is not an answerable filter, and w.corporations
    // is an unordered_map whose iteration order is not a stable thing to build a
    // menu from.
    std::vector<entity_id> corps;
    for (const feed_row& r : rows)
        if (r.corp != null_entity && std::find(corps.begin(), corps.end(), r.corp) == corps.end())
            corps.push_back(r.corp);
    std::sort(corps.begin(), corps.end());

    // A filter can outlive the rows that justified it — the ring wraps past the
    // last decision a corp ever took. Clear it rather than leaving the reader on
    // a silently empty list with a live filter they cannot see the effect of.
    if (st.decision_feed_corp != null_entity
        && std::find(corps.begin(), corps.end(), st.decision_feed_corp) == corps.end())
        st.decision_feed_corp = null_entity;

    const float avail = ImGui::GetContentRegionAvail().x;

    ImGui::SetNextItemWidth(avail * 0.62f);
    if (ImGui::BeginCombo("Corp", st.decision_feed_corp == null_entity
                                      ? "All corporations"
                                      : corp_name(w, st.decision_feed_corp)))
    {
        if (ImGui::Selectable("All corporations", st.decision_feed_corp == null_entity))
            st.decision_feed_corp = null_entity;
        for (const entity_id c : corps)
        {
            const bool sel = (c == st.decision_feed_corp);
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  palette::corp_identity_colour(c, w.player_entity));
            if (ImGui::Selectable(corp_name(w, c), sel))
                st.decision_feed_corp = c;
            ImGui::PopStyleColor();
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::SetNextItemWidth(avail * 0.62f);
    if (ImGui::BeginCombo("Reason", st.decision_feed_reason < 0
                                        ? "Any reason"
                                        : reason_label(static_cast<corp_decision_reason>(
                                              st.decision_feed_reason))))
    {
        if (ImGui::Selectable("Any reason", st.decision_feed_reason < 0))
            st.decision_feed_reason = -1;
        for (int i = 0; i < k_reason_count; ++i)
        {
            const bool sel = (i == st.decision_feed_reason);
            if (ImGui::Selectable(reason_label(static_cast<corp_decision_reason>(i)), sel))
                st.decision_feed_reason = i;
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    const bool filtered = (st.decision_feed_corp != null_entity) || (st.decision_feed_reason >= 0);
    if (filtered)
    {
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear"))
        {
            st.decision_feed_corp   = null_entity;
            st.decision_feed_reason = -1;
        }
    }

    // --- Apply the filters --------------------------------------------------
    std::vector<const feed_row*> shown;
    shown.reserve(rows.size());
    std::size_t hidden_unscored = 0;
    for (const feed_row& r : rows)
    {
        if (st.decision_feed_corp != null_entity && r.corp != st.decision_feed_corp)
            continue;
        if (st.decision_feed_reason >= 0)
        {
            // A recovered log row carries prose, not a reason code. It is
            // excluded rather than kept, because keeping it would answer "show
            // me every sustained-loss idle" with rows nobody has checked against
            // that question. The count below says how many went, so the omission
            // is stated rather than silent.
            if (!r.scored) { ++hidden_unscored; continue; }
            if (r.reason != st.decision_feed_reason) continue;
        }
        shown.push_back(&r);
    }

    // --- Newest first (R1) --------------------------------------------------
    // Tick descending, then lowest corp id, then latest-applied first. `seq` is
    // unique per row, so the comparator is a strict weak ordering with no ties
    // left over — the displayed order is fully determined by the data.
    std::sort(shown.begin(), shown.end(), [](const feed_row* a, const feed_row* b) {
        if (a->tick != b->tick) return a->tick > b->tick;
        if (a->corp != b->corp) return a->corp < b->corp;
        return a->seq > b->seq;
    });

    // --- Provenance line ----------------------------------------------------
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
    ImGui::TextWrapped("%zu shown - the ring holds %zu of %zu decisions taken; %zu older "
                       "recovered from the history log.",
                       shown.size(), w.ai_decisions.entries.size(), w.ai_decisions.total,
                       rows.size() - w.ai_decisions.entries.size());
    if (hidden_unscored > 0)
        ImGui::TextWrapped("%zu history-log rows hidden: they carry no reason code to "
                           "filter on.", hidden_unscored);
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (shown.empty())
    {
        ImGui::TextWrapped("No decisions match these filters.");
        foldout_end();
        return;
    }

    // --- Rows ---------------------------------------------------------------
    for (const feed_row* r : shown)
    {
        ImGui::PushID(static_cast<int>(r->seq));

        const float row_w = ImGui::GetContentRegionAvail().x;

        // Date, then the corp in its identity colour — the same colour its
        // buildings and markers carry on the canvas, so a row is attributable
        // without reading the name (presentation.hpp, BL-090).
        ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
        ImGui::TextUnformatted(date_label(r->tick).c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               palette::corp_identity_colour(r->corp, w.player_entity)),
                           "%s", corp_name(w, r->corp));

        ImGui::TextWrapped("%s", r->headline.c_str());

        if (!r->detail.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, palette::text_secondary);
            ImGui::TextWrapped("%s", r->detail.c_str());
            ImGui::PopStyleColor();
        }

        if (r->scored)
        {
            const margin_read m = read_margin(r->winning, r->runner_up);

            // Bar first, scores after it on the same line: the bar answers "how
            // close was this" at a glance and the numbers are the audit trail
            // behind that answer, so the glance-value goes on the left.
            const float bar_w = std::max(48.0f, row_w - 118.0f);
            draw_conviction_bar(bar_w, m);
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, m.colour);
            if (m.uncontested)
                ImGui::Text("%.2f, %s", r->winning, m.phrase);
            else
                ImGui::Text("%.2f v %.2f", r->winning, r->runner_up);
            ImGui::PopStyleColor();

            if (!m.uncontested)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, m.colour);
                ImGui::TextUnformatted(m.phrase);
                ImGui::PopStyleColor();
            }
        }
        else
        {
            // R3: the ring's 256-entry cap is deliberate ("derived
            // observability, not save-format state - bounded so the log can run
            // forever", corp_command.hpp). What the log kept is the prose; the
            // scores were never written to it. Say that, rather than draw an
            // empty bar and a 0.00 that would read as a real dead heat.
            ImGui::PushStyleColor(ImGuiCol_Text, palette::neutral);
            ImGui::TextWrapped("no margin recorded - older than the decision ring");
            ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    foldout_end();
}

} // namespace ui
