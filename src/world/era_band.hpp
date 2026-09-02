#pragma once
// The era band — which product's roster an authored entry belongs to — in a
// header of its own (BL-744, 2026-09-02) so that world_gen_config.hpp can
// select a base-price table by band WITHOUT pulling recipe_registry.hpp's
// include set into every generation TU, and without putting the accessor in a
// Lua-linked .cpp that the Lua-free harnesses never link (the first cut did
// exactly that, and every harness built on io_world_obj alone failed to link).
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
    ancient    = 1, ///< The 0 CE product (world_params::epoch_year < 1700).
    industrial = 2, ///< The 1960 arc, including everything space-facing.
};

/// One past the last band — the size of any per-band table. Derived from the
/// enum's tail, the same way `resource_count` and `building_type_count` derive
/// from theirs: appending a band means moving this with it.
inline constexpr std::size_t era_band_count =
    static_cast<std::size_t>(era_band::industrial) + 1;

/// The band a campaign's epoch year belongs to. Uses the SAME 1700 threshold the
/// antiquity branch already documents on world_params::epoch_year, so the split
/// between the two arcs is one number in the codebase rather than two.
inline era_band era_band_for_epoch(int64_t epoch_year)
{
    return (epoch_year < 1700) ? era_band::ancient : era_band::industrial;
}

/// Does an entry authored for band @p entry appear in a campaign running @p campaign?
/// An `any` entry appears in every band; an `any` campaign (the unset default)
/// admits every entry.
inline bool era_permits(era_band campaign, era_band entry)
{
    return campaign == era_band::any || entry == era_band::any || entry == campaign;
}
