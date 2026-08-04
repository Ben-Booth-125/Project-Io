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
// nothing to show and drew a flat dark fill.
//
// THE LADDER WRAPS (Ben, 2026-08-04). Solar is as far out as the zoom goes, so
// instead of inventing a further rung the minimap loops back to where the
// player is standing: the sky over the homeworld, seen from the top of a hill.
// Zoom all the way out and you are looking up again. That keeps the panel a
// MINIMAP rather than a fourth canvas — there is nothing to navigate here, only
// something to look at.
//
// The galaxy is in the picture rather than being a separate view of it: the
// core rides across the sky as a bright bulge in the band, so the supermassive
// black hole at the centre of everything is a DIRECTION you can point at from
// the ground, which is how it actually reads to anyone who has stood outside.
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
enum class deep_sky_kind : uint8_t { quasar = 0, supernova_remnant, nebula, cluster, galaxy,
                                     galactic_core };

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
/// Where the galactic band crosses the sky: the band is drawn as a great circle
/// tilted by `band_tilt` and centred on the core's x position, so the core bulge
/// and the band agree by construction rather than by hand-placement.
float star_map_band_tilt();

const char* star_class_name(star_class k);
uint32_t    star_class_rgb(star_class k);
const char* deep_sky_kind_name(deep_sky_kind k);
