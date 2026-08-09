#pragma once

#include "creeds.hpp"
#include "tongue.hpp"
#include "world.hpp"

#include <random>
#include <string>

struct settlement_state;

/// Generate one procedural city / settlement name in @p t, deterministically
/// from @p rng.
///
/// A root of one or two syllables drawn from the tongue's own phoneme
/// inventory, suffixed with a settlement morpheme that tongue coined for
/// itself (`coin_lexicon`), with an occasional coined epithet in front. No
/// English place morpheme is involved — the "-ton"/"-ford"/"-haven" bank was
/// removed under BL-290, because a culture that names its gods in one tongue
/// does not name its towns in another. Draws only from @p rng, so callers
/// control determinism. Returns empty if @p t cannot coin a word.
std::string generate_city_name(std::mt19937& rng, const tongue& t);

/// Re-name every population centre on @p body_id in the tongue of the culture
/// that settled the ground under it (BL-290).
///
/// Population centres are placed before the creeds/settlement passes run, so
/// they are first named from a tongue rolled for the body; once the political
/// history exists, this pass gives each centre the speech of its NEAREST
/// PROVINCE's culture — the same "whose gods" rule the settlement pass used.
/// Pure and deterministic in @p seed: sorted-id order, an independent stream,
/// and no other pass's RNG touched. A body with no settlement record is left
/// alone.
void name_population_centres(world& w, entity_id body_id, int gw,
                             const settlement_state& ss, const creed_state& cs,
                             uint32_t seed);
