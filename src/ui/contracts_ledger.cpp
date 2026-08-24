#include "contracts_ledger.hpp"

#include "foldout_column.hpp" // shell fold-out column host (BL-122) + nav_button (the toggle rule)
#include "presentation.hpp"   // resource_name / building_type_name (condition_text's UI resolvers), palette
#include "selection_panel.hpp" // unit_roster_display_name — the Soldier card's own roster-name lookup

#include "world/condition_set.hpp"    // condition, condition_text — the contract's predicate, worded
#include "world/nation_generation.hpp" // garrison_strength_in — the client's visible garrison
#include "world/unit_roster.hpp"       // unit_strength — the same derivation the Selection band uses

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ui {

namespace {

/// A nation's display name, or an honest fallback for an id that does not
/// resolve — the same "Body #<id>" / "Market #<id>" fallback idiom
/// market_ledger.cpp uses.
std::string nation_label(const world& w, entity_id nation)
{
    const auto it = w.nations.find(nation);
    if (it != w.nations.end())
        return it->second.name;
    return "Nation #" + std::to_string(nation);
}

/// A province has no authored name (province.hpp carries none) — the
/// numeric id is the only honest label until one exists.
std::string province_label(uint32_t province_id)
{
    return "Province #" + std::to_string(province_id);
}

/// CONTRACTS.md Q4: "Offers are private per client, visible only where the
/// activity fog already reaches (body_activity_visibility, BL-089)". Reads
/// on the offer's TARGET body — resolved through the province partition,
/// since a mercenary_offer names a province, not a body directly. `unknown`
/// is the one tier the fog has not reached at all; `known_stale` still
/// counts (a route once reached the body, which is "the fog reaching it",
/// just cold) — see this function's own design-call note in contracts_ledger.cpp.
bool offer_target_visible(const world& w, const mercenary_offer& o)
{
    const province* pr = w.provinces.find(o.target_province);
    if (pr == nullptr)
        return false; // an offer bound to a province the partition no longer has
    return body_activity_visibility(w, pr->body, w.current_day_tick) != activity_vis::unknown;
}

/// True iff @p unit is owned by the player and not committed to any live
/// (active) mercenary contract — the force picker's candidate filter, using
/// the SAME `mercenary_contract_has_unit` predicate accept_offer's own
/// double-commit check reads (world.hpp), so the picker and the seam cannot
/// disagree about what "committed" means.
bool is_uncommitted_owned_unit(const world& w, entity_id id, const unit_component& u)
{
    if (u.owner != w.player_entity || u.count <= 0)
        return false;
    for (const mercenary_contract& c : w.mercenary_contracts)
    {
        if (c.state != mercenary_contract_state::active)
            continue;
        if (mercenary_contract_has_unit(c, id))
            return false;
    }
    return true;
}

/// The force picker (BL-576) — a popup listing the player's own uncommitted
/// units with a checkbox each, writing into `s.contracts_picker_units`.
/// Opened by the Offers view's Accept press against `o`; Confirm assembles
/// and enqueues the `accept_offer` command, Cancel discards the pick. Both
/// close the popup and clear the picker state, so a stale pick from one
/// offer cannot leak into the next.
void draw_force_picker(const world& w, ui_state& s, const mercenary_offer& o)
{
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::selection), "Choose your force");
    ImGui::TextDisabled("Owned, uncommitted units only.");
    ImGui::Separator();

    std::vector<entity_id> candidates;
    for (const auto& [id, u] : w.units)
        if (is_uncommitted_owned_unit(w, id, u))
            candidates.push_back(id);
    std::sort(candidates.begin(), candidates.end()); // deterministic listing order

    if (candidates.empty())
        ImGui::TextDisabled("No uncommitted units available.");

    for (const entity_id uid : candidates)
    {
        const unit_component& u = w.units.at(uid);

        int  slot    = -1;
        bool checked = false;
        for (std::size_t i = 0; i < mercenary_contract_max_units; ++i)
        {
            if (s.contracts_picker_units[i] == uid)
            {
                checked = true;
                slot    = static_cast<int>(i);
                break;
            }
        }

        ImGui::PushID(static_cast<int>(uid));
        bool val = checked;
        if (ImGui::Checkbox("##pick", &val))
        {
            if (val && !checked)
            {
                for (std::size_t i = 0; i < mercenary_contract_max_units; ++i)
                {
                    if (s.contracts_picker_units[i] == null_entity)
                    {
                        s.contracts_picker_units[i] = uid;
                        break;
                    }
                    // A full picker (all 8 slots already taken) silently drops
                    // the request rather than crashing — mercenary_contract_max_units
                    // is far above any real prototype-scale force (corp_command.hpp).
                }
            }
            else if (!val && checked && slot >= 0)
            {
                s.contracts_picker_units[static_cast<std::size_t>(slot)] = null_entity;
            }
        }
        ImGui::SameLine();
        ImGui::Text("%s - strength %lld", unit_roster_display_name(u.type).c_str(),
                    static_cast<long long>(unit_strength(w, u)));
        ImGui::PopID();
    }

    ImGui::Separator();

    int chosen = 0;
    for (const entity_id e : s.contracts_picker_units)
        if (e != null_entity)
            ++chosen;
    const bool can_confirm = chosen > 0;

    if (!can_confirm)
        ImGui::BeginDisabled();
    if (ImGui::Button("Confirm"))
    {
        corp_command cmd;
        cmd.corp         = w.player_entity;
        cmd.verb         = corp_verb::accept_offer;
        cmd.order        = o.id;
        cmd.counterparty = o.client; // accept_offer reads counterparty as the CLIENT NATION
        cmd.units        = s.contracts_picker_units;
        s.pending_order_commands.push_back(cmd);

        s.contracts_picker_offer = 0;
        s.contracts_picker_units.fill(null_entity);
        ImGui::CloseCurrentPopup();
    }
    if (!can_confirm)
        ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
    {
        s.contracts_picker_offer = 0;
        s.contracts_picker_units.fill(null_entity);
        ImGui::CloseCurrentPopup();
    }
}

void draw_offers_view(const world& w, ui_state& s)
{
    std::vector<const mercenary_offer*> visible;
    for (const mercenary_offer& o : w.mercenary_offers)
        if (offer_target_visible(w, o))
            visible.push_back(&o);
    std::sort(visible.begin(), visible.end(),
             [](const mercenary_offer* a, const mercenary_offer* b) { return a->id < b->id; });

    if (visible.empty())
    {
        ImGui::TextDisabled("No offers visible right now.");
        ImGui::TextDisabled("An offer only shows once your commercial reach touches its target body.");
        return;
    }

    if (ImGui::BeginChild("##offers_scroll", {0.0f, 0.0f}, false))
    {
        for (const mercenary_offer* op : visible)
        {
            const mercenary_offer& o = *op;
            ImGui::PushID(static_cast<int>(o.id));
            ImGui::Separator();

            ImGui::Text("%s", nation_label(w, o.client).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", province_label(o.target_province).c_str());

            ImGui::Text("Fee %.0f cr  (escrow %.0f / %.0f)",
                        static_cast<double>(o.fee), static_cast<double>(o.offer_escrow),
                        static_cast<double>(o.fee));
            const int ticks_left = o.deadline - w.current_econ_tick;
            ImGui::Text("Contract deadline in %d ticks", ticks_left > 0 ? ticks_left : 0);
            ImGui::Text("Garrison at target: %lld",
                        static_cast<long long>(garrison_strength_in(w, o.target_province)));

            const bool fully_escrowed = o.offer_escrow >= o.fee;
            const bool expired        = w.current_econ_tick >= o.deadline;
            const bool can_accept     = fully_escrowed && !expired;

            if (!can_accept)
                ImGui::BeginDisabled();
            if (ImGui::Button("Accept"))
            {
                s.contracts_picker_offer = o.id;
                s.contracts_picker_units.fill(null_entity);
                ImGui::OpenPopup("contracts_force_picker");
            }
            if (!can_accept)
                ImGui::EndDisabled();

            // The force-picker popup lives in THIS row's ID scope (PushID(o.id)
            // above), so ImGui's own popup-open state cannot leak between rows
            // even though every row reuses the same string id.
            if (ImGui::BeginPopup("contracts_force_picker"))
            {
                draw_force_picker(w, s, o);
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            if (!can_accept)
                ImGui::TextDisabled(expired ? "(expired)" : "(still filling)");
            else
                ImGui::TextDisabled("Decline stays open - refusable, not dismissable.");

            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void draw_active_view(const world& w, const recipe_registry& reg,
                      const contract_template_registry& templates, ui_state& s)
{
    std::vector<const mercenary_contract*> mine;
    for (const mercenary_contract& c : w.mercenary_contracts)
        if (c.contractor == w.player_entity && c.state == mercenary_contract_state::active)
            mine.push_back(&c);
    std::sort(mine.begin(), mine.end(),
             [](const mercenary_contract* a, const mercenary_contract* b) { return a->id < b->id; });

    if (mine.empty())
    {
        ImGui::TextDisabled("No active contracts.");
        return;
    }

    // The Abandon press's reputation cost — CONTRACTS.md § Q2: "an honest
    // early exit costs less than a rout, but it still costs". Read once: the
    // same `contract_cancelled` Trust weight every abandoned contract pays,
    // regardless of which row's popup is open.
    const float rep_cost = reg.sentiment()
                               .factors[static_cast<std::size_t>(sentiment_factor_kind::contract_cancelled)]
                               .trust;

    if (ImGui::BeginChild("##active_scroll", {0.0f, 0.0f}, false))
    {
        for (const mercenary_contract* cp : mine)
        {
            const mercenary_contract& c = *cp;
            ImGui::PushID(static_cast<int>(c.id));
            ImGui::Separator();

            ImGui::Text("%s", nation_label(w, c.client).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", province_label(c.province).c_str());

            if (c.template_index >= 0 &&
                static_cast<std::size_t>(c.template_index) < templates.size())
            {
                const contract_template& tmpl = templates.at(static_cast<std::size_t>(c.template_index));
                condition pred = tmpl.predicate; // province is bound per accepted offer, never authored
                pred.province  = c.province;
                ImGui::TextWrapped("%s", condition_text(pred, resource_name, building_type_name).c_str());
            }
            else
            {
                ImGui::TextDisabled("(contract kind not in the loaded template table)");
            }

            const int ticks_left = c.deadline - w.current_econ_tick;
            ImGui::Text("Deadline in %d ticks", ticks_left > 0 ? ticks_left : 0);

            ImGui::TextDisabled("Force:");
            for (const entity_id uid : c.units)
            {
                if (uid == null_entity)
                    continue;
                const auto uit = w.units.find(uid);
                if (uit == w.units.end())
                    continue;
                ImGui::BulletText("%s - strength %lld", unit_roster_display_name(uit->second.type).c_str(),
                                  static_cast<long long>(unit_strength(w, uit->second)));
            }

            if (ImGui::Button("Abandon"))
                ImGui::OpenPopup("contracts_confirm_abandon");
            if (ImGui::BeginPopup("contracts_confirm_abandon"))
            {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                                   "Abandon this contract?");
                ImGui::TextDisabled("Deposit forfeit; nothing further paid.");
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(palette::negative),
                                   "Reputation cost: %.1f Trust with %s.",
                                   static_cast<double>(rep_cost), nation_label(w, c.client).c_str());
                ImGui::Separator();
                if (ImGui::Button("Abandon"))
                {
                    corp_command cmd;
                    cmd.corp  = w.player_entity;
                    cmd.verb  = corp_verb::abandon_contract;
                    cmd.order = c.id;
                    s.pending_order_commands.push_back(cmd);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Keep"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

void draw_history_view(const world& w)
{
    std::vector<const mercenary_contract*> past;
    for (const mercenary_contract& c : w.mercenary_contracts)
        if (c.contractor == w.player_entity && c.state != mercenary_contract_state::active)
            past.push_back(&c);
    std::sort(past.begin(), past.end(),
             [](const mercenary_contract* a, const mercenary_contract* b) { return a->id < b->id; });

    if (past.empty())
    {
        ImGui::TextDisabled("No settled contracts yet.");
        return;
    }

    if (ImGui::BeginChild("##history_scroll", {0.0f, 0.0f}, false))
    {
        for (const mercenary_contract* cp : past)
        {
            const mercenary_contract& c = *cp;
            ImGui::PushID(static_cast<int>(c.id));
            ImGui::Separator();

            ImGui::Text("%s", nation_label(w, c.client).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", province_label(c.province).c_str());

            const char* state_word =
                (c.state == mercenary_contract_state::completed) ? "Completed" :
                (c.state == mercenary_contract_state::failed)    ? "Failed"    :
                                                                    "Abandoned";
            const ImU32 state_colour =
                (c.state == mercenary_contract_state::completed) ? palette::positive : palette::negative;
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(state_colour), "%s", state_word);
            ImGui::SameLine();

            // Completion pays the FULL fee (deposit + the remainder disbursed
            // at settlement, run_mercenary_contract_tick); failure/abandonment
            // pay nothing beyond the deposit already taken at accept_offer
            // (CONTRACTS.md § Q2: "you are not paid for trying") — so the
            // record's own two fields already say the whole story with no
            // third field needed for "amount paid".
            const float paid = (c.state == mercenary_contract_state::completed) ? c.fee : c.deposit_paid;
            ImGui::Text("- %.0f cr paid", static_cast<double>(paid));

            ImGui::PopID();
        }
    }
    ImGui::EndChild();
}

} // namespace

void draw_contracts_ledger(const world& w, const recipe_registry& reg,
                           const contract_template_registry& templates,
                           ui_state& s, bool& open)
{
    if (!open)
        return;

    if (!ui::foldout_begin("Contracts"))
    {
        ui::foldout_end();
        return;
    }

    // View tabs — the standing button-strip idiom every split ledger uses
    // (LAYOUT.md § One-question-per-view splits). Re-clicking the active tab
    // closes the whole ledger (the toggle rule); switching tabs is an
    // ordinary view change.
    ui::nav_button("Offers",  0, s.contracts_ledger_view, &open);
    ImGui::SameLine();
    ui::nav_button("Active",  1, s.contracts_ledger_view, &open);
    ImGui::SameLine();
    ui::nav_button("History", 2, s.contracts_ledger_view, &open);
    ImGui::Separator();
    ImGui::Spacing();

    switch (s.contracts_ledger_view)
    {
        case 1: draw_active_view(w, reg, templates, s); break;
        case 2: draw_history_view(w);                   break;
        default: draw_offers_view(w, s);                break;
    }

    ui::foldout_end();
}

} // namespace ui
