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
    ancient    = 1, ///< A campaign dated before `industrial_band_from_year` (epoch 0 CE).
    industrial = 2, ///< The 1960 product, including everything space-facing.
};

/// One past the last band — the size of any per-band table. Derived from the
/// enum's tail, the same way `resource_count` and `building_type_count` derive
/// from theirs: appending a band means moving this with it.
inline constexpr std::size_t era_band_count =
    static_cast<std::size_t>(era_band::industrial) + 1;

/// The first campaign year whose roster is the industrial band.
///
/// THE BAND'S OWN NUMBER (BL-1047). This once shared its 1700 with the
/// settlement pass's antiquity branch and the two-span arc predicate, "so the
/// split between the two arcs is one number". Those two were generation
/// mechanisms keyed on the epoch; the flip took the epoch out of generation, so
/// they read their own years now and this is the one 1700 left that the epoch
/// reaches. The band is what the epoch names besides the calendar
/// (INDUSTRIALISATION.md: "the epoch names a calendar and a recipe band"): a
/// campaign-side roster mask the app applies AFTER generation, never an input
/// to it, so it cannot move the generated world.
inline constexpr int64_t industrial_band_from_year = 1700;

/// The band a campaign's epoch year belongs to: 1960 (the default) is
/// industrial, 0 CE is ancient.
inline era_band era_band_for_epoch(int64_t epoch_year)
{
    return (epoch_year < industrial_band_from_year) ? era_band::ancient : era_band::industrial;
}

/// Does an entry authored for band @p entry appear in a campaign running @p campaign?
/// An `any` entry appears in every band; an `any` campaign (the unset default)
/// admits every entry.
inline bool era_permits(era_band campaign, era_band entry)
{
    return campaign == era_band::any || entry == era_band::any || entry == campaign;
}
