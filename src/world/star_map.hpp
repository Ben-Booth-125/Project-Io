#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// The star map — the sky beyond the campaign system (2026-08-04)
// ---------------------------------------------------------------------------
// A FIXED, AUTHORED table. Every campaign generates a different world around a
// different star, but the galaxy those systems sit in is the same galaxy: the
// constellations do not reroll. That is the whole point — the sky is the one
// thing a player carries between campaigns, so it is authored data rather than
// generated, and nothing here reads a seed.
//
// It fills the Solar minimap slot, which MINIMAP.md § The top rung records as a
// branding placeholder: Solar has no zoom-out neighbour, so the inset had
// nothing to show and drew a flat dark fill. The rung above a solar system is
// the galaxy it belongs to.
//
// NAMES ARE INVENTED, not transferred. The standing rule
// (.claude/rules/io-standing-rules.md § Terms & docs) is that real history is a
// mechanism reference and never a name source; that applies to the sky as much
// as to nations. No Orion, no Cassiopeia, no Betelgeuse.
//
// Coordinates are a plate-carrée projection of the celestial sphere onto the
// unit square: x in [0,1] runs once around the sky, y in [0,1] from one pole to
// the other. The galactic plane is drawn as a band across the middle rather
// than being derived, because this is a picture, not a simulation.

/// Spectral class, in the usual hot-to-cool order. Drives the rendered colour.
enum class star_class : uint8_t { blue = 0, white, yellow, orange, red };

/// One fixed star.
struct star_map_star
{
    const char* name;      ///< Invented. May be empty for anonymous field stars.
    float       x, y;      ///< Plate-carrée position on the unit square.
    float       magnitude; ///< Apparent brightness, 0 (brightest) to 6 (naked-eye limit).
    star_class  klass;
};

/// A named grouping of stars, by index into the star table.
struct star_map_constellation
{
    const char*    name;
    const int*     stars;      ///< Indices into star_map_stars().
    int            count;
    const int*     edges;       ///< Pairs of indices into `stars` (not the table) to join.
    int            edge_count;  ///< Number of PAIRS, so `edges` holds 2 * edge_count ints.
};

/// Things that are not stars: the quasar, the remnants, the nebulae. These are
/// the objects the biography's flavour lines point at (BL-289) — a supernova
/// remnant in the sky is the same object the history says was seen to flare.
enum class deep_sky_kind : uint8_t { quasar = 0, supernova_remnant, nebula, cluster, galaxy };

struct star_map_object
{
    const char*   name;
    float         x, y;
    float         size;      ///< Rendered extent on the unit square.
    deep_sky_kind kind;
    /// How long ago the event that made this object was SEEN from the campaign
    /// system, in Gya. Zero for objects with no event (nebulae, clusters,
    /// galaxies, the quasar — which has been shining throughout).
    float         seen_gya;
};

const star_map_star* star_map_stars(int& count);
const star_map_constellation* star_map_constellations(int& count);
const star_map_object* star_map_objects(int& count);

/// Display name for a spectral class, and its identity colour as 0xRRGGBB.
const char* star_class_name(star_class k);
uint32_t    star_class_rgb(star_class k);
const char* deep_sky_kind_name(deep_sky_kind k);
