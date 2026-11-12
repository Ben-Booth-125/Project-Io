#pragma once

#include "world/world.hpp"

namespace ui {

/// The kind of entity the current selection refers to. Drives the polymorphic
/// Selection info element (see docs/ui/SELECTION.md): the panel dispatches on
/// this to choose which per-entity content builder to render and where the
/// 'go to' button navigates. Resolved from the entity id by probing the world's
/// component stores (selection_kind_of).
enum class selection_kind
{
    none,     ///< No selection, or a stale id that matches no live entity.
    body,     ///< A celestial body (planet/moon/asteroid/station/star).
    tile,     ///< A surface tile.
    building, ///< A surface installation.
    market,   ///< A body's local market.
    unit,     ///< A unit / unit stack.
    nation,   ///< A generated nation (political territory).
    corporation, ///< A generated corporation (faction entity).
    /// A battle in progress (BL-469). NOT resolved by `selection_kind_of`, and
    /// that is the point rather than an omission: a battle has no entity id to
    /// probe for. It is keyed by (province, attacker, defender) exactly as
    /// `active_battle` is, and it lives in its own `ui_state` fields the way
    /// `selected_province` does — the precedent BL-511 set for a selection whose
    /// subject is not an entity.
    battle,
    /// An accepted mercenary contract (BL-577), the battle card's sibling.
    /// Like `battle`, NOT resolved by `selection_kind_of` — a `mercenary_contract`
    /// lives in `world::mercenary_contracts` (a vector, keyed by its own stable
    /// `id`), not in a per-type entity map, so there is no entity id to probe
    /// for. It lives in its own `ui_state` field (`selected_contract_id`), the
    /// same precedent `selected_province` and the battle triple established.
    contract,
    /// BL-1450: an air WING (`world::wings`, air_combat.hpp). Its id comes from
    /// `create_entity`, so it travels in `selected_entity` and `selection_kind_of`
    /// resolves it; its card is its own element (wing_card.hpp).
    wing,
    /// BL-1534: a SHIP (`world::ships`, sea_combat.hpp), on the wing's rules; its card is
    /// its own element (ship_card.hpp).
    ship,
    /// BL-1590: a SATELLITE or a STATION (`world::space_assets`, `world::stations`),
    /// pressed on the Circumplanetary rung. A station is selected by its id, which is
    /// its core asset's; a station's modules are not selectable on their own. Its
    /// card is the shared space-asset summary (circum_assets.hpp).
    space_asset,
    /// BL-1609: a SPACE UNIT (`world::space_units`), pressed as its fleet's lead on
    /// the Circumplanetary or Solar rung. Its id comes from `create_entity`. Its card
    /// is the shared space-unit summary with the player's orders (circum_units.hpp).
    space_unit,
    // Future kinds (logistics vessel, …) slot in here once their component
    // stores exist; see SELECTION.md (§ Polymorphism).
};

/// Resolve the kind of @p e by probing the world's per-type component stores, in
/// the same discrimination order as ui::focus_on_entity. Entity ids are unique
/// across the stores, so an id matches at most one. A null or stale id resolves
/// to selection_kind::none.
///
/// @param w Read-only world state.
/// @param e Entity id to classify.
/// @return  The matching selection_kind, or none if unknown / stale.
selection_kind selection_kind_of(const world& w, entity_id e);

/// Human-readable label for a selection kind (panel title prefix, debugging).
///
/// @param k Selection kind.
/// @return  Null-terminated display name (e.g. "Body", "Tile").
const char* selection_kind_name(selection_kind k);

} // namespace ui
