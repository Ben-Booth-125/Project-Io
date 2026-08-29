#include "acquisitions_ledger.hpp"

#include "detail_level.hpp"   // the BL-214/BL-265 fold idiom — the profitability takeover
#include "foldout_column.hpp" // shell fold-out column host + shell_column_width
#include "format.hpp"
#include "presentation.hpp"

#include "world/corp_command.hpp" // corp_acquisition_price / corp_trailing_net — never a restated formula

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace ui {

namespace {

// ---------------------------------------------------------------------------
// Disclosure — one predicate, used by every gated cell on both surfaces
// ---------------------------------------------------------------------------

/// Whether @p id's figures may be read by the player's corporation.
///
/// FINANCE.md § Disclosure, in two clauses and no third: **a corporation always
/// reads its own books** (turning the rule on its author would be nonsense —
/// a firm that could not read its own balance sheet could not be run), and
/// otherwise the target's OWNERSHIP CLASS decides. It is binary. There is no
/// graded middle and no fog term: the banded standing read is retired
/// (Ben, 2026-08-26), so the reason a number is absent is always that the firm
/// does not file, never that the player has not earned it.
bool discloses(const world& w, entity_id id, const corporation_component& cc)
{
    if (cc.is_player || id == w.player_entity)
        return true;
    return cc.ownership_class == ownership_class::publicly_held;
}

const char* ownership_class_label(ownership_class oc)
{
    switch (oc)
    {
        case ownership_class::publicly_held:  return "public";
        case ownership_class::privately_held: return "private";
        case ownership_class::closed:         return "closed";
    }
    return "?";
}

// THE DISCLOSURE DASH IS GONE, and its absence is the whole of BL-679.
//
// Ben, 2026-08-29: "operational fog should hide details. But that should be
// binary. If we don't know what a company does, then we don't know anything
// about it. If we do know, then we get all information." So an undisclosed firm
// is not a row of dashes on the financial table — it is NOT LISTED. Once the
// rows are gone the dash has no subject, and the reason-on-hover that explained
// it has nothing left to explain, so both are deleted rather than left
// unreachable.
//
// The DIPLOMACY ledger (corporation_panel.cpp) is a different surface and keeps
// its own dash for Capital: a rival you cannot price is still a rival you hold a
// stance toward, so there the row must stay and the figure must not. Do not
// conflate the two — this ruling is about the financial table only.

// ---------------------------------------------------------------------------
// What a firm makes — derived from its holdings, never from a stored summary
// ---------------------------------------------------------------------------

/// A firm's operation, summarised from the buildings it actually holds.
struct operation
{
    int  extraction = 0;
    int  processing = 0;
    int  other      = 0;
    bool has_end    = false;
    bool has_input  = false;
    resource_type end_res   = resource_type::iron_ore;
    resource_type input_res = resource_type::iron_ore;

    /// Every body this firm holds a building on, ascending and deduplicated.
    ///
    /// A SET, not a modal value, and that is the honest shape: a firm with sites
    /// on two bodies is genuinely on both, and collapsing it to whichever it
    /// holds more of would make the Body filter hide a firm the player can
    /// actually reach. The filter therefore tests MEMBERSHIP.
    std::vector<entity_id> bodies;
};

/// Walk @p cc's holdings and report the MODAL end resource and modal input
/// resource, plus the building-type mix.
///
/// Derived at draw time from `assets` -> `building_component`, exactly as the
/// Corporation lens derives tile ownership: there is no stored "what this firm
/// makes" field, and inventing one would be a second source of truth that could
/// disagree with the buildings.
///
/// An extraction site has an output and NO input — extraction consumes no
/// resource — so a pure extractor's input cell is a genuine absence rather than
/// a withheld figure, and the two are worded differently below.
operation summarise_operation(const world& w, const recipe_registry& reg,
                              const corporation_component& cc)
{
    operation op;
    std::array<int, resource_count> out_tally{};
    std::array<int, resource_count> in_tally{};

    // Ascending building id: the tallies are order-independent, but the walk
    // that builds them must not inherit an unordered map's layout, or two
    // frames could break a tie differently and the cell would flicker.
    std::vector<entity_id> ids = cc.assets;
    std::sort(ids.begin(), ids.end());

    for (const entity_id bid : ids)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;

        // Building -> tile -> body. Derived at draw time from the holdings, the
        // same walk everything else on this row comes from, so the Body filter
        // can never disagree with the buildings on canvas.
        if (const auto tit = w.tiles.find(b.tile); tit != w.tiles.end())
            op.bodies.push_back(tit->second.body);

        if (b.type == building_type::extraction_site)
        {
            ++op.extraction;
            ++out_tally[static_cast<std::size_t>(b.target_resource)];
        }
        else if (b.type == building_type::processing_facility)
        {
            ++op.processing;
            if (b.recipe == no_recipe)
                continue;
            const recipe* r = reg.get_recipe(b.recipe);
            if (r == nullptr)
                continue;
            ++out_tally[static_cast<std::size_t>(primary_output_resource(*r))];
            // The recipe's largest input, by the same "biggest term wins" rule
            // primary_output_resource uses on the other side of the arrow.
            std::size_t best = resource_count;
            float best_v = 0.0f;
            for (std::size_t i = 0; i < resource_count; ++i)
                if (r->inputs[i] > best_v) { best_v = r->inputs[i]; best = i; }
            if (best < resource_count)
                ++in_tally[best];
        }
        else
        {
            ++op.other;
        }
    }

    const auto modal = [](const std::array<int, resource_count>& t,
                          bool& found, resource_type& out)
    {
        int best_n = 0;
        std::size_t best_i = 0;
        for (std::size_t i = 0; i < resource_count; ++i)
            if (t[i] > best_n) { best_n = t[i]; best_i = i; }
        found = best_n > 0;
        if (found)
            out = static_cast<resource_type>(best_i);
    };
    modal(out_tally, op.has_end,   op.end_res);
    modal(in_tally,  op.has_input, op.input_res);

    std::sort(op.bodies.begin(), op.bodies.end());
    op.bodies.erase(std::unique(op.bodies.begin(), op.bodies.end()), op.bodies.end());
    return op;
}

/// The type cell's word. Derived from the building mix, NOT from
/// `industrial_focus`: BL-145 hides that field from the UI blanket (it stays a
/// data-model field for world-gen and the economy), and nothing here overturns
/// that. The mix is legitimately public either way — buildings are visible on
/// canvas, which is the same fact the Corporation table's Reach column already
/// rests on.
const char* operation_type_label(const operation& op)
{
    const int total = op.extraction + op.processing + op.other;
    if (total == 0)
        return "No holdings";
    if (op.extraction > 0 && op.processing > 0)
        return "Mixed";
    if (op.extraction > 0)
        return "Extraction";
    if (op.processing > 0)
        return "Processing";
    return "Other";
}

// ---------------------------------------------------------------------------
// The buyable field
// ---------------------------------------------------------------------------

/// One firm the player may legally attempt to buy, priced.
struct offer_row
{
    entity_id   corp  = null_entity;
    std::string name;
    float       price = 0.0f;
    bool        affordable = false;
};

/// The buyable field, ascending price.
///
/// The predicates MIRROR `apply_corp_command`'s own gates (2)-(4), in its
/// order, and the price comes from `corp_acquisition_price` — never a restated
/// formula, because a surface that restated it would show a price the seam then
/// refused. The seam re-checks every gate, so a divergence surfaces as a
/// rejected press rather than as a wrong number.
///
/// `is_background` is DELIBERATELY NOT A GATE. The verb does not test it, so
/// neither does this list: the buyable field is every public filed firm, which
/// in practice is mostly background companies.
///
/// A firm that does not file is absent entirely — not greyed, not shown at
/// zero. It cannot be priced, so there is no row to draw.
std::vector<offer_row> buyable_field(const world& w, const recipe_registry& reg,
                                     float player_balance)
{
    std::vector<offer_row> rows;
    const float k = reg.acquisition().multiple;

    // Ascending corporation id, so the order this list is BUILT in is
    // deterministic before the price sort breaks its ties.
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());

    for (const entity_id id : ids)
    {
        const corporation_component& cc = w.corporations.at(id);
        if (id == w.player_entity || cc.is_player)
            continue;                                     // gate (2)
        if (cc.ownership_class != ownership_class::publicly_held)
            continue;                                     // gate (3)
        if (cc.returns.empty())
            continue;                                     // gate (4)
        const float price = corp_acquisition_price(cc, k);
        if (!std::isfinite(price))
            continue;      // the seam refuses a non-finite price; so do we
        offer_row r;
        r.corp       = id;
        r.name       = cc.name;
        r.price      = price;
        r.affordable = player_balance >= price;           // gate (5), the seam's own test
        rows.push_back(r);
    }

    std::sort(rows.begin(), rows.end(),
              [](const offer_row& a, const offer_row& b)
              { return a.price != b.price ? a.price < b.price : a.corp < b.corp; });
    return rows;
}

// ---------------------------------------------------------------------------
// The column's two groups
// ---------------------------------------------------------------------------

/// Draw one group's rows. @p with_press is the whole difference between the two
/// groups, and it is the reason the second group exists: a Possible row shows
/// the price and no press, so the player can see what the next rung costs.
///
/// COLUMN BUDGET (NR-709). The name column stretches and the numeric/press
/// columns are FIXED AND MEASURED — `CalcTextSize` on the widest string this
/// group will actually draw, not a guessed pixel constant. A name column left
/// on `WidthStretch` behind fixed siblings that over-claim collapses to nothing
/// and the firm's identity — the one thing the row is for — disappears. So the
/// fixed widths are computed first and the stretch column takes what is left,
/// which is `shell_column_width`-proportional by construction.
void draw_group(const world& w, ui_state& s, const std::vector<offer_row>& rows,
                bool with_press, const char* table_id)
{
    if (rows.empty())
    {
        ImGui::TextDisabled("  None.");
        return;
    }

    // Measure the widest price this group will draw, so the column is exactly
    // as wide as it needs to be and never wider.
    float price_w = ImGui::CalcTextSize("Price").x;
    for (const offer_row& r : rows)
        price_w = std::max(price_w,
                           ImGui::CalcTextSize(fmt::credits(r.price).c_str()).x);
    price_w += ImGui::GetStyle().CellPadding.x * 2.0f + 4.0f;

    const float press_w = with_press
        ? ImGui::CalcTextSize("Buy").x + ImGui::GetStyle().FramePadding.x * 2.0f
              + ImGui::GetStyle().CellPadding.x * 2.0f + 4.0f
        : 0.0f;

    const int cols = with_press ? 3 : 2;
    if (!ImGui::BeginTable(table_id, cols,
                           ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        return;

    ImGui::TableSetupColumn("Firm",  ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, price_w);
    if (with_press)
        ImGui::TableSetupColumn("##buy", ImGuiTableColumnFlags_WidthFixed, press_w);

    for (const offer_row& r : rows)
    {
        ImGui::TableNextRow();
        ImGui::PushID(static_cast<int>(r.corp));

        // The Company-lens click's landing mark. A highlight, never a filter:
        // the field is two rows long and filtering it to one would answer a
        // question nobody asked.
        if (s.acquisitions_focus_corp == r.corp)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 40));

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(r.name.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", r.name.c_str());   // the full name, if the cell clipped it

        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(fmt::credits(r.price).c_str());
        if (r.price <= 0.0f && ImGui::IsItemHovered())
        {
            // A FREE FIRM IS NOT A CHEAP FIRM (FINANCE.md § Whole-firm
            // acquisition, measured 2026-08-26). The `balance` term is signed,
            // so a deeply indebted firm's negative cash cancels its book value
            // and the max(0, ...) floor hands it over for nothing — and the
            // dissolution rule then transfers the debt at face value. Said on
            // the row rather than left for the player to discover after the
            // press.
            ImGui::SetTooltip("Priced at nothing: this firm's debts cancel its "
                              "book value.\nBuying it takes on those debts in full.");
        }

        if (with_press)
        {
            ImGui::TableSetColumnIndex(2);
            const bool pressed = ImGui::SmallButton("Buy");

            // Publish the FIRST Buy button's centre so a verify script can click
            // the real control rather than a computed guess (see ui_state's own
            // note). Written after the button so the rect is this frame's.
            if (&r == &rows.front())
            {
                const ImVec2 mn = ImGui::GetItemRectMin();
                const ImVec2 mx = ImGui::GetItemRectMax();
                s.acquisitions_buy_x    = (mn.x + mx.x) * 0.5f;
                s.acquisitions_buy_y    = (mn.y + mx.y) * 0.5f;
                s.acquisitions_buy_corp = r.corp;
            }

            if (pressed)
            {
                // Through the seam, never a direct write. `buy_corporation`
                // re-checks every gate this list mirrored and prices the firm
                // itself, so the press cannot commit at a stale price.
                corp_command cmd;
                cmd.corp         = w.player_entity;
                cmd.verb         = corp_verb::buy_corporation;
                cmd.counterparty = r.corp;
                s.pending_order_commands.push_back(cmd);
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Buy %s outright for %s.\nHoldings, stock, cash "
                                  "and filed returns transfer; the firm is dissolved.",
                                  r.name.c_str(), fmt::credits(r.price).c_str());
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
}

// ---------------------------------------------------------------------------
// The profitability fold-out (BL-627)
// ---------------------------------------------------------------------------

/// One row of the profitability table — one DISCLOSED corporation.
///
/// There is no `disclosed` flag, and its absence is deliberate rather than an
/// oversight: after BL-679 an undisclosed firm never becomes a row at all, so a
/// flag that would read `true` on every row of every frame would be dead weight
/// that invited a future cell to gate on it again.
struct profit_row
{
    entity_id   corp = null_entity;
    std::string name;
    bool        is_player = false;
    ownership_class oc = ownership_class::closed;
    const char* type = "-";
    bool        has_end = false, has_input = false;
    resource_type end_res = resource_type::iron_ore;
    resource_type input_res = resource_type::iron_ore;
    std::vector<entity_id> bodies;
    float       profit = 0.0f;   ///< the filed net of the most recent quarter
    bool        has_profit = false;
    float       price = 0.0f;
    bool        has_price = false;
};

enum profit_col : int
{
    col_firm = 0, col_type, col_end, col_input, col_profit, col_price, col_class,
    profit_col_count
};

/// The resource cell's word, or the GENUINE-ABSENCE dash.
///
/// The only dash left on this table, and it never means fog. An extraction site
/// consumes no resource, so a pure extractor's input cell is a real absence
/// rather than a withheld figure — the firm discloses fully and there is simply
/// nothing to disclose. Kept distinct from the retired disclosure dash by
/// construction: that one had a reason-on-hover explaining what was being
/// withheld, and nothing is being withheld here.
const char* res_or_dash(bool has, resource_type r)
{
    return has ? resource_name(r) : "-";
}

void sort_profit_rows(std::vector<profit_row>& rows, int column, bool ascending)
{
    // A stable sort over a deterministic base order (ascending corp id, below),
    // so equal keys keep that order rather than an implementation's whim.
    const auto cmp = [column](const profit_row& a, const profit_row& b) -> bool
    {
        switch (column)
        {
            // THE ABSENT-SORTS-LAST TIE-BREAKS ARE GONE from the three
            // operational columns (BL-679). They existed only so a dashed cell
            // sorted sanely against a real word, and every listed row now
            // carries a real word: a plain comparison is the whole rule.
            case col_type:
                return std::string(a.type) < std::string(b.type);
            case col_end:
                return std::string(res_or_dash(a.has_end, a.end_res))
                     < std::string(res_or_dash(b.has_end, b.end_res));
            case col_input:
                return std::string(res_or_dash(a.has_input, a.input_res))
                     < std::string(res_or_dash(b.has_input, b.input_res));
            // The two money columns keep theirs, and the reason is different in
            // kind: a listed firm may genuinely not have filed a quarter yet,
            // and the player's own row has no price because it is not for sale.
            // Neither absence is fog, and neither is a small number.
            case col_profit:
                if (a.has_profit != b.has_profit) return a.has_profit;
                return a.profit < b.profit;
            case col_price:
                if (a.has_price != b.has_price) return a.has_price;
                return a.price < b.price;
            case col_class:
                return static_cast<int>(a.oc) < static_cast<int>(b.oc);
            default:
                return a.name < b.name;
        }
    };
    std::stable_sort(rows.begin(), rows.end(),
                     [&](const profit_row& a, const profit_row& b)
                     { return ascending ? cmp(a, b) : cmp(b, a); });
}

const char* body_name(const world& w, entity_id id)
{
    const auto it = w.bodies.find(id);
    return (it == w.bodies.end()) ? "?" : it->second.name.c_str();
}

/// One filter combo, drawn and published.
///
/// @param slot   0 = end resource, 1 = input resource, 2 = body — the index the
///               published position arrays use.
/// @param value  the live filter field, edited in place. @p none is "every".
///
/// Returns nothing; the press writes straight through. All three are CROSS-
/// CUTTING SELECTORS and therefore exempt from the toggle rule (they switch a
/// target rather than express an active state), exactly as the Market ledger's
/// Body and Market combos are — so re-picking the current value is a no-op and
/// deliberately not an undo.
template <typename T, typename LabelFn>
void filter_combo(ui_state& s, int slot, const char* label, const char* every_label,
                  const std::vector<T>& opts, T& value, T none, LabelFn label_of)
{
    const char* preview = (value == none) ? every_label : label_of(value);

    ImGui::SetNextItemWidth(ImGui::CalcTextSize(every_label).x + 90.0f);
    const bool open = ImGui::BeginCombo(label, preview);

    // Published AFTER the combo so the rect is this frame's, exactly as the Buy
    // button's centre is.
    const ImVec2 cmn = ImGui::GetItemRectMin();
    const ImVec2 cmx = ImGui::GetItemRectMax();
    s.acquisitions_filter_x[slot] = (cmn.x + cmx.x) * 0.5f;
    s.acquisitions_filter_y[slot] = (cmn.y + cmx.y) * 0.5f;

    if (open)
    {
        if (ImGui::Selectable(every_label, value == none))
            value = none;

        bool first = true;
        for (const T& o : opts)
        {
            const bool sel = (o == value);
            if (ImGui::Selectable(label_of(o), sel))
                value = o;
            // The first REAL option's centre — the one a verify script clicks to
            // prove the selector actually narrows the table. "Every" is not it:
            // selecting "every" changes nothing and would check nothing.
            if (first)
            {
                const ImVec2 mn = ImGui::GetItemRectMin();
                const ImVec2 mx = ImGui::GetItemRectMax();
                s.acquisitions_filter_opt_x[slot] = (mn.x + mx.x) * 0.5f;
                s.acquisitions_filter_opt_y[slot] = (mn.y + mx.y) * 0.5f;
                first = false;
            }
            if (sel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

/// The three filters, on one line above the table.
///
/// COLUMN BUDGET: this row lives in the FULL-CANVAS fold-out, never in the
/// ~358 px shell column — the profitability table has no in-column rung at all
/// (`in_place = false` on its disclosure control, because 88 rows over seven
/// columns do not fit a column at any font size). So the vertical cost of a
/// filter row is charged against the canvas, which has it, and not against the
/// column, which does not.
void draw_filter_row(const world& w, ui_state& s,
                     const std::vector<resource_type>& end_opts,
                     const std::vector<resource_type>& input_opts,
                     const std::vector<entity_id>&     body_opts)
{
    const auto res_label  = [](resource_type r) { return resource_name(r); };
    const auto body_label = [&w](entity_id b)   { return body_name(w, b); };

    auto end_v   = static_cast<resource_type>(std::max(s.acquisitions_filter_end, 0));
    auto input_v = static_cast<resource_type>(std::max(s.acquisitions_filter_input, 0));

    // A sentinel outside the enum's range, so "every" can never collide with a
    // real resource — resource index 0 is a good like any other.
    constexpr auto res_none = static_cast<resource_type>(resource_count);
    if (s.acquisitions_filter_end   < 0) end_v   = res_none;
    if (s.acquisitions_filter_input < 0) input_v = res_none;

    filter_combo(s, 0, "End resource##acq_f_end", "Every end resource",
                 end_opts, end_v, res_none, res_label);
    ImGui::SameLine();
    filter_combo(s, 1, "Input resource##acq_f_in", "Every input resource",
                 input_opts, input_v, res_none, res_label);
    ImGui::SameLine();
    entity_id body_v = s.acquisitions_filter_body;
    filter_combo(s, 2, "Body##acq_f_body", "Every body",
                 body_opts, body_v, null_entity, body_label);

    s.acquisitions_filter_end =
        (end_v == res_none) ? -1 : static_cast<int>(end_v);
    s.acquisitions_filter_input =
        (input_v == res_none) ? -1 : static_cast<int>(input_v);
    s.acquisitions_filter_body = body_v;
}

void draw_profit_table(const world& w, const recipe_registry& reg, ui_state& s)
{
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());   // the deterministic resting order

    const float k = reg.acquisition().multiple;

    std::vector<profit_row> rows;
    rows.reserve(ids.size());
    for (const entity_id id : ids)
    {
        const corporation_component& cc = w.corporations.at(id);

        // BL-679, AND IT IS THE WHOLE OF IT. A firm the player knows nothing
        // about is not listed at all — the row is never built, rather than
        // built and then dashed out cell by cell.
        //
        // This is a FILTER, not a new predicate. `discloses()` was already
        // binary and already read ownership class alone; the per-column gate
        // that used to sit on type / end resource / input resource was an
        // implementation choice layered on top of it, never a rule any doc
        // stated. Ben, 2026-08-29: "If we don't know what a company does, then
        // we don't know anything about it. If we do know, then we get all
        // information." So the gate moves from the CELL to the ROW, and inside
        // a listed row every field prints unconditionally.
        if (!discloses(w, id, cc))
            continue;

        profit_row r;
        r.corp      = id;
        r.name      = cc.name;
        r.oc        = cc.ownership_class;
        r.is_player = cc.is_player || id == w.player_entity;

        const operation op = summarise_operation(w, reg, cc);
        r.type      = operation_type_label(op);
        r.has_end   = op.has_end;
        r.end_res   = op.end_res;
        r.has_input = op.has_input;
        r.input_res = op.input_res;
        r.bodies    = op.bodies;

        if (!cc.returns.empty())
        {
            r.profit     = cc.returns.back().net;
            r.has_profit = true;
        }
        // A price exists only where the VERB would price it: public, filed, and
        // not the player's own corp. The player's own row is listed (a firm
        // reads its own books) but has no price, because it cannot be bought
        // through this seam.
        if (!r.is_player && cc.ownership_class == ownership_class::publicly_held
            && !cc.returns.empty())
        {
            const float p = corp_acquisition_price(cc, k);
            if (std::isfinite(p))
            {
                r.price     = p;
                r.has_price = true;
            }
        }
        rows.push_back(r);
    }

    // -----------------------------------------------------------------------
    // BL-680 — the three cross-cutting selectors
    //
    // POPULATED FROM THE ROWS ACTUALLY PRESENT, never from the resource
    // registry. A filter offering forty goods where the listed field holds four
    // is a worse surface than no filter at all: every empty option is a
    // question the table cannot answer, and the player has to press each one to
    // find that out.
    //
    // The option sets are built from the LISTED set, before the filters narrow
    // it — not from the post-filter set. Cascading them would let one choice
    // empty another combo out from under its own current value, stranding the
    // player in a state they could not see their way out of.
    // -----------------------------------------------------------------------
    std::vector<resource_type> end_opts, input_opts;
    std::vector<entity_id>     body_opts;
    for (const profit_row& r : rows)
    {
        if (r.has_end)   end_opts.push_back(r.end_res);
        if (r.has_input) input_opts.push_back(r.input_res);
        body_opts.insert(body_opts.end(), r.bodies.begin(), r.bodies.end());
    }
    const auto dedupe = [](auto& v)
    {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    };
    dedupe(end_opts);
    dedupe(input_opts);
    dedupe(body_opts);

    // A filter holding a value the world no longer offers (the firm was bought,
    // the last site on a body closed) silently narrows to nothing and looks
    // like a broken table. Drop it back to "every" instead.
    const auto still_offered = [](const auto& opts, auto v)
    { return std::find(opts.begin(), opts.end(), v) != opts.end(); };
    if (s.acquisitions_filter_end >= 0 &&
        !still_offered(end_opts, static_cast<resource_type>(s.acquisitions_filter_end)))
        s.acquisitions_filter_end = -1;
    if (s.acquisitions_filter_input >= 0 &&
        !still_offered(input_opts, static_cast<resource_type>(s.acquisitions_filter_input)))
        s.acquisitions_filter_input = -1;
    if (s.acquisitions_filter_body != null_entity &&
        !still_offered(body_opts, s.acquisitions_filter_body))
        s.acquisitions_filter_body = null_entity;

    draw_filter_row(w, s, end_opts, input_opts, body_opts);

    const std::size_t listed_total = rows.size();

    // AND across the three, which is the reading the three questions share: a
    // player asking "who makes X, from Y, within reach of Z" is asking one
    // question, not three.
    const auto excluded = [&s](const profit_row& r)
    {
        if (s.acquisitions_filter_end >= 0 &&
            !(r.has_end && static_cast<int>(r.end_res) == s.acquisitions_filter_end))
            return true;
        if (s.acquisitions_filter_input >= 0 &&
            !(r.has_input && static_cast<int>(r.input_res) == s.acquisitions_filter_input))
            return true;
        if (s.acquisitions_filter_body != null_entity &&
            std::find(r.bodies.begin(), r.bodies.end(), s.acquisitions_filter_body)
                == r.bodies.end())
            return true;
        return false;
    };
    rows.erase(std::remove_if(rows.begin(), rows.end(), excluded), rows.end());

    // SAID, NOT DRAWN EMPTY. A table with headers and no rows reads as a defect;
    // the combination that produced it is the actual answer, so it is stated.
    if (rows.empty())
    {
        ImGui::Spacing();
        if (listed_total == 0)
        {
            ImGui::TextDisabled("No corporation discloses its return.");
        }
        else
        {
            ImGui::TextDisabled("No firm matches all three filters.");
            ImGui::TextDisabled("  %zu firms disclose; this combination excludes every one.",
                                listed_total);
        }
        return;
    }

    if (s.acquisitions_sort_column >= 0)
        sort_profit_rows(rows, s.acquisitions_sort_column, s.acquisitions_sort_ascending);

    // What this table is about to draw, in draw order. Published so a check can
    // assert against the surface's own output rather than re-deriving the
    // listing and filter rules and grading its own work.
    s.acquisitions_profit_shown.clear();
    s.acquisitions_profit_shown.reserve(rows.size());
    for (const profit_row& r : rows)
        s.acquisitions_profit_shown.push_back(r.corp);

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Sortable |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##acq_profit", profit_col_count, flags,
                           {0.0f, ImGui::GetContentRegionAvail().y}))
        return;

    // The firm's name stretches; every other column is fixed and sized to the
    // widest thing it draws. Same NR-709 rule as the column groups — here the
    // host is the canvas rather than the 380 px column, so there is room, but a
    // stretch name behind six fixed siblings is the failure either way.
    ImGui::TableSetupScrollFreeze(0, 1);
    // MEASURED, not guessed. The first capture pass sized the two resource
    // columns at a round 132 px and "Agricultural Produce" spilled straight
    // across the Profit/qtr column — a clipping defect `expect_no_clipping`
    // reported ZERO records against (NR-663: table cells are not instrumented),
    // so the only thing that caught it was looking at the PNG. Sizing every
    // fixed column off `CalcTextSize` of the widest string it can hold makes
    // the failure impossible rather than merely fixed: a longer resource name
    // authored tomorrow widens the column instead of overflowing it.
    float res_w = ImGui::CalcTextSize("Input resource").x;
    for (std::size_t i = 0; i < resource_count; ++i)
        res_w = std::max(res_w,
            ImGui::CalcTextSize(resource_name(static_cast<resource_type>(i))).x);
    const float pad = ImGui::GetStyle().CellPadding.x * 2.0f + 8.0f;
    res_w += pad;

    float money_w = ImGui::CalcTextSize("Profit/qtr").x;
    for (const profit_row& r : rows)
    {
        if (r.has_profit)
            money_w = std::max(money_w, ImGui::CalcTextSize(fmt::credits(r.profit).c_str()).x);
        if (r.has_price)
            money_w = std::max(money_w, ImGui::CalcTextSize(fmt::credits(r.price).c_str()).x);
    }
    money_w += pad;

    float type_w = ImGui::CalcTextSize("Type").x;
    for (const char* t : { "Extraction", "Processing", "Mixed", "Other", "No holdings" })
        type_w = std::max(type_w, ImGui::CalcTextSize(t).x);
    type_w += pad;

    const float class_w = ImGui::CalcTextSize("Ownership").x + pad;

    ImGui::TableSetupColumn("Firm",          ImGuiTableColumnFlags_WidthStretch |
                                             ImGuiTableColumnFlags_DefaultSort);
    ImGui::TableSetupColumn("Type",          ImGuiTableColumnFlags_WidthFixed, type_w);
    ImGui::TableSetupColumn("End resource",  ImGuiTableColumnFlags_WidthFixed, res_w);
    ImGui::TableSetupColumn("Input resource",ImGuiTableColumnFlags_WidthFixed, res_w);
    ImGui::TableSetupColumn("Profit/qtr",    ImGuiTableColumnFlags_WidthFixed, money_w);
    ImGui::TableSetupColumn("Price",         ImGuiTableColumnFlags_WidthFixed, money_w);
    ImGui::TableSetupColumn("Ownership",     ImGuiTableColumnFlags_WidthFixed, class_w);
    ImGui::TableHeadersRow();

    // Mirror ImGui's sort specs into ui_state. The shell's windows carry
    // NoSavedSettings, so ImGui forgets the sort the moment the takeover closes;
    // holding it here is what makes the sort survive a close and reopen.
    if (ImGuiTableSortSpecs* spec = ImGui::TableGetSortSpecs();
        spec != nullptr && spec->SpecsDirty && spec->SpecsCount > 0)
    {
        s.acquisitions_sort_column    = spec->Specs[0].ColumnIndex;
        s.acquisitions_sort_ascending = spec->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
        spec->SpecsDirty = false;
    }

    for (const profit_row& r : rows)
    {
        ImGui::TableNextRow();
        if (r.is_player)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 40));
        else if (s.acquisitions_focus_corp == r.corp)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 255, 255, 28));

        ImGui::TableSetColumnIndex(col_firm);
        ImGui::TextUnformatted(r.name.c_str());
        if (ImGui::IsItemHovered())
        {
            // WHERE the firm operates, on hover rather than in an eighth
            // column. The Body filter needs to be legible — a player who
            // narrows to a body must be able to see why a row survived — but a
            // body column would cost width on a table already carrying two
            // resource names, and this row is the only place the answer is
            // wanted. The full name comes with it, for a cell that clipped.
            std::string tip = r.name;
            if (r.bodies.empty())
            {
                tip += "\nNo holdings on any body.";
            }
            else
            {
                tip += "\nOperates on: ";
                for (std::size_t i = 0; i < r.bodies.size(); ++i)
                {
                    if (i > 0) tip += ", ";
                    tip += body_name(w, r.bodies[i]);
                }
            }
            ImGui::SetTooltip("%s", tip.c_str());
        }

        // THE THREE OPERATIONAL CELLS PRINT UNCONDITIONALLY (BL-679). Every row
        // that reached here discloses, and a disclosing firm discloses
        // everything — there is no per-column gate left to consult.
        ImGui::TableSetColumnIndex(col_type);
        ImGui::TextUnformatted(r.type);

        ImGui::TableSetColumnIndex(col_end);
        ImGui::TextUnformatted(res_or_dash(r.has_end, r.end_res));

        ImGui::TableSetColumnIndex(col_input);
        ImGui::TextUnformatted(res_or_dash(r.has_input, r.input_res));
        if (!r.has_input && ImGui::IsItemHovered())
            ImGui::SetTooltip("Consumes no resource - this firm only extracts.");

        ImGui::TableSetColumnIndex(col_profit);
        if (r.has_profit)
        {
            const ImU32 col = (r.profit < 0.0f) ? palette::negative : palette::positive;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s",
                               fmt::credits(r.profit).c_str());
        }
        else
        {
            // Discloses, but has not filed a quarter yet. A GENUINE absence,
            // and the only reason a Profit cell can now be empty.
            ImGui::TextDisabled("-");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Has not filed a return yet.");
        }

        ImGui::TableSetColumnIndex(col_price);
        if (r.has_price)
        {
            ImGui::TextUnformatted(fmt::credits(r.price).c_str());
        }
        else
        {
            ImGui::TextDisabled("-");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip(r.is_player
                    ? "Your own corporation. It is not for sale."
                    : "Not priceable: this firm has not filed a return.");
        }

        ImGui::TableSetColumnIndex(col_class);
        ImGui::TextUnformatted(ownership_class_label(r.oc));
    }

    ImGui::EndTable();
}

} // namespace

void draw_acquisitions_ledger(const world& w, const recipe_registry& reg,
                              ui_state& s, bool& open)
{
    if (!open)
        return;

    // Cleared BEFORE the fold-out draws, so a filter combo's position is
    // published as absent in the frames the takeover is closed rather than
    // lingering as a stale point a script would click into empty canvas.
    for (int i = 0; i < 3; ++i)
    {
        s.acquisitions_filter_x[i]     = -1.0f;
        s.acquisitions_filter_y[i]     = -1.0f;
        s.acquisitions_filter_opt_x[i] = -1.0f;
        s.acquisitions_filter_opt_y[i] = -1.0f;
    }
    s.acquisitions_profit_shown.clear();

    // The full-canvas profitability takeover. Drawn BEFORE the column so it is
    // reachable even in the frame the column has been clipped away, and because
    // the two coexist by design (BL-265): the row that opened the takeover stays
    // visible in the column beside it.
    if (fold_overlay_begin(s, detail_surface::acquisitions_profit, 0,
                           "Profitability - every firm that files"))
    {
        ImGui::TextDisabled(
            "One row per firm that files. A firm that does not file is not listed - "
            "if it discloses, every figure is here. Sort on any column.");
        ImGui::Separator();
        draw_profit_table(w, reg, s);
        fold_overlay_end(s);
    }

    // Cleared every frame BEFORE the groups draw, so "there is no Buy button"
    // is published as such rather than as a stale position from a frame when
    // there was one.
    s.acquisitions_buy_x    = -1.0f;
    s.acquisitions_buy_y    = -1.0f;
    s.acquisitions_buy_corp = null_entity;

    if (!ui::foldout_begin("Acquisitions"))
    {
        ui::foldout_end();
        return;
    }

    float balance = 0.0f;
    const auto pit = w.corporations.find(w.player_entity);
    if (pit != w.corporations.end())
        balance = pit->second.balance;

    const std::vector<offer_row> field = buyable_field(w, reg, balance);

    std::vector<offer_row> purchasable, possible;
    for (const offer_row& r : field)
        (r.affordable ? purchasable : possible).push_back(r);

    // The field's size, stated rather than implied. It is genuinely small — a
    // measured mean of 1.6 buyable firms out of eighty-eight, over twelve seeds
    // — so a player who sees one row should be able to tell that is the world
    // and not a bug.
    ImGui::Text("Your balance: %s", fmt::credits(balance).c_str());
    ImGui::TextDisabled("%zu of %zu firms file and can be priced.",
                        field.size(), w.corporations.size());
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Only a public firm that has filed a return can be "
                          "priced, and only a priced firm can be bought.\n"
                          "Private and closed firms publish nothing, so there is "
                          "no price to show and no offer to make.");
    ImGui::Separator();
    ImGui::Spacing();

    // Two collapsing groups. Both open at rest — with a field this small, a
    // collapsed group hides the whole answer. `CollapsingHeader` is a toggle by
    // construction, which is the standing rule for any control whose active
    // state is visible.
    char hdr[64];
    std::snprintf(hdr, sizeof hdr, "Purchasable (%zu)###acq_purch", purchasable.size());
    ImGui::SetNextItemOpen(s.acquisitions_purchasable_open, ImGuiCond_Always);
    s.acquisitions_purchasable_open = ImGui::CollapsingHeader(hdr);
    if (s.acquisitions_purchasable_open)
    {
        ImGui::TextDisabled("Within your balance today.");
        draw_group(w, s, purchasable, /*with_press=*/true, "##acq_purch_tbl");
    }

    ImGui::Spacing();
    std::snprintf(hdr, sizeof hdr, "Possible (%zu)###acq_poss", possible.size());
    ImGui::SetNextItemOpen(s.acquisitions_possible_open, ImGuiCond_Always);
    s.acquisitions_possible_open = ImGui::CollapsingHeader(hdr);
    if (s.acquisitions_possible_open)
    {
        ImGui::TextDisabled("Priced, but beyond your balance today.");
        // NO PRESS on these rows, and that is the whole reason this group
        // exists: the price is what the player is saving toward, and a
        // disabled button would say "refused" where the truth is "not yet".
        draw_group(w, s, possible, /*with_press=*/false, "##acq_poss_tbl");
    }

    ImGui::Spacing();
    ImGui::Separator();

    // The fold-out's door. `in_place = false`: ~88 rows over seven columns do
    // not fit a 380 px column at any font size, so only the `›` full-canvas
    // control is drawn and it keeps the same rightmost gutter as every other
    // disclosure control in the app.
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection),
                       "Profitability");
    disclosure_controls(s, detail_surface::acquisitions_profit, 0, /*in_place=*/false);
    ImGui::TextDisabled("Every disclosing firm's return, side by side.");

    ui::foldout_end();
}

} // namespace ui
