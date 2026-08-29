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

/// The dash, with its reason on hover. Drawn through one function everywhere so
/// a dash can never mean two different things on two cells of the same row.
void disclosure_dash(ownership_class oc)
{
    ImGui::TextDisabled("-");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Does not file - %s corporations publish no return.",
                          ownership_class_label(oc));
}

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

/// One row of the profitability table — every corporation, disclosed or not.
struct profit_row
{
    entity_id   corp = null_entity;
    std::string name;
    bool        disclosed = false;
    bool        is_player = false;
    ownership_class oc = ownership_class::closed;
    const char* type = "-";
    bool        has_end = false, has_input = false;
    resource_type end_res = resource_type::iron_ore;
    resource_type input_res = resource_type::iron_ore;
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
            // The three OPERATIONAL columns are disclosure-gated, so an
            // undisclosed cell sorts as ABSENT — the same rule the two money
            // columns take below. A dash is not a value, and letting the dashes
            // sort among the real words would make the ordering lie.
            case col_type:
                if (a.disclosed != b.disclosed) return a.disclosed;
                return std::string(a.type) < std::string(b.type);
            case col_end:
                if (a.disclosed != b.disclosed) return a.disclosed;
                return std::string(res_or_dash(a.has_end, a.end_res))
                     < std::string(res_or_dash(b.has_end, b.end_res));
            case col_input:
                if (a.disclosed != b.disclosed) return a.disclosed;
                return std::string(res_or_dash(a.has_input, a.input_res))
                     < std::string(res_or_dash(b.has_input, b.input_res));
            // An undisclosed figure sorts as ABSENT, not as zero: a dash is not
            // a small number, and letting it sort among the real ones would
            // make the ordering lie. Absent rows go last in either direction.
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
        profit_row r;
        r.corp      = id;
        r.name      = cc.name;
        r.oc        = cc.ownership_class;
        r.is_player = cc.is_player || id == w.player_entity;
        r.disclosed = discloses(w, id, cc);

        const operation op = summarise_operation(w, reg, cc);
        r.type = operation_type_label(op);

        if (r.disclosed)
        {
            // TYPE, END RESOURCE AND INPUT RESOURCE ARE ALL DISCLOSURE-GATED,
            // and that is a call rather than an oversight — flagged for Ben.
            //
            // BL-633's own note on this class of surface is explicit that "no
            // production rate, stockpile quantity, RECIPE or workforce dial is
            // readable here", and a firm's end and input resources ARE its
            // recipe, read out. Printing them for every firm would widen the
            // operational fog through a financial surface, which is not this
            // surface's to widen.
            //
            // `type` was UNGATED in the first pass, on the argument that the
            // building mix is visible on canvas (the same public fact the
            // Corporation table's Reach column rests on). The capture pass
            // overturned it on two counts: it is derived from the identical
            // holdings walk as the two resource cells, so gating one and not
            // the other made a single row report the same private fact twice
            // at two different sensitivities; and it read "Mixed" on all
            // eighty-eight rows, so ungating it bought no information to weigh
            // against that. Consistency won.
            r.has_end   = op.has_end;
            r.end_res   = op.end_res;
            r.has_input = op.has_input;
            r.input_res = op.input_res;

            if (!cc.returns.empty())
            {
                r.profit     = cc.returns.back().net;
                r.has_profit = true;
            }
            // A price exists only where the VERB would price it: public, filed,
            // and not the player's own corp. The player's own row is disclosed
            // (a firm reads its own books) but has no price, because it cannot
            // be bought through this seam.
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
        }
        rows.push_back(r);
    }

    if (s.acquisitions_sort_column >= 0)
        sort_profit_rows(rows, s.acquisitions_sort_column, s.acquisitions_sort_ascending);

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

        ImGui::TableSetColumnIndex(col_type);
        if (r.disclosed) ImGui::TextUnformatted(r.type);
        else             disclosure_dash(r.oc);

        ImGui::TableSetColumnIndex(col_end);
        if (r.disclosed) ImGui::TextUnformatted(res_or_dash(r.has_end, r.end_res));
        else             disclosure_dash(r.oc);

        ImGui::TableSetColumnIndex(col_input);
        if (r.disclosed) ImGui::TextUnformatted(res_or_dash(r.has_input, r.input_res));
        else             disclosure_dash(r.oc);

        ImGui::TableSetColumnIndex(col_profit);
        if (r.has_profit)
        {
            const ImU32 col = (r.profit < 0.0f) ? palette::negative : palette::positive;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(col), "%s",
                               fmt::credits(r.profit).c_str());
        }
        else if (r.disclosed)
        {
            // Discloses, but has not filed a quarter yet. A different absence
            // from a withheld one, and worded differently so the two never blur.
            ImGui::TextDisabled("-");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Has not filed a return yet.");
        }
        else
        {
            disclosure_dash(r.oc);
        }

        ImGui::TableSetColumnIndex(col_price);
        if (r.has_price)
        {
            ImGui::TextUnformatted(fmt::credits(r.price).c_str());
        }
        else if (r.is_player)
        {
            ImGui::TextDisabled("-");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Your own corporation. It is not for sale.");
        }
        else
        {
            disclosure_dash(r.oc);
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

    // The full-canvas profitability takeover. Drawn BEFORE the column so it is
    // reachable even in the frame the column has been clipped away, and because
    // the two coexist by design (BL-265): the row that opened the takeover stays
    // visible in the column beside it.
    if (fold_overlay_begin(s, detail_surface::acquisitions_profit, 0,
                           "Profitability - every corporation's filed return"))
    {
        ImGui::TextDisabled(
            "One row per corporation. Exact figures where the firm files; a dash "
            "where it does not. Sort on any column.");
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
    ImGui::TextDisabled("Every corporation's filed return, side by side.");

    ui::foldout_end();
}

} // namespace ui
