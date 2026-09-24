#pragma once
// The era band — which product's roster an authored entry belongs to — in a
// header of its own (BL-744, 2026-09-02). Split out so a header lighter than
// recipe_registry.hpp can read the band; the per-band price table that first
// needed it was overturned the same day (NR-778 — one table, a depth-one
// ancient steel route instead), and the split stays because it costs nothing
// and the lesson it carries does: a function called from io_world_obj must not
// live in a Lua-linked .cpp, or every Lua-free harness fails to link.
// recipe_registry.hpp includes this and is unchanged for every reader.

#include <cstddef>
#include <cstdint>

/// BL-433: which product's roster an authored entry belongs to.
///
/// Two bands plus a wildcard, and deliberately NOT ERAS.md's Era 0 / Era 1
/// numbering — that axis is about space access *within* the industrial arc and
/// is gated on launchpad presence, a different question from "which product is
/// this". One field, one meaning.
///
/// `any` is the default and it is load-bearing: a registry whose band is never
/// set permits everything, so every headless harness — none of which knows about
/// eras — loads exactly the roster it loaded before this existed.
enum class era_band : uint8_t
{
    any        = 0, ///< Shared by both arcs. The default for an untagged entry.
    ancient    = 1, ///< A world whose history never reached the Industrial rung (BL-1101).
    industrial = 2, ///< The 1960 product, including everything space-facing.
};

/// One past the last band — the size of any per-band table. Derived from the
/// enum's tail, the same way `resource_count` and `building_type_count` derive
/// from theirs: appending a band means moving this with it.
inline constexpr std::size_t era_band_count =
    static_cast<std::size_t>(era_band::industrial) + 1;

// THE EPOCH NAMES NO BAND (BL-1101, Ben 2026-09-24). The 1700 threshold and the
// year-to-band function lived here until then: the last place the campaign
// epoch reached besides the calendar. The band is now a fact about the
// HISTORY the world was generated with — `industrial` iff any living polity's
// materials capacity sits at the Industrial rung at the 1960 fold, `ancient`
// otherwise (`derive_campaign_band`, history_sim.hpp) — written once onto
// `world::campaign_band` and read from there by the app and every harness. A
// registry is banded from a world, never from a year.

/// The band's name, for banners and manifests. Total over the enum.
inline const char* era_band_name(era_band b)
{
    switch (b)
    {
    case era_band::ancient:    return "ancient";
    case era_band::industrial: return "industrial";
    default:                   return "any";
    }
}

/// Does an entry authored for band @p entry appear in a campaign running @p campaign?
/// An `any` entry appears in every band; an `any` campaign (the unset default)
/// admits every entry.
inline bool era_permits(era_band campaign, era_band entry)
{
    return campaign == era_band::any || entry == era_band::any || entry == campaign;
}
