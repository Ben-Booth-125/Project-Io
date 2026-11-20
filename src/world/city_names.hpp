#pragma once

#include "creeds.hpp"
#include "tongue.hpp"
#include "world.hpp"

#include <random>
#include <string>
#include <unordered_set>

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

/// A city name not already in @p used, which it is then added to. NO TWO CITIES ON
/// ONE BODY SHARE A NAME (Ben, 2026-10-13: a market is named for its city, and two
/// markets read the same when two cities did). Re-draws from @p rng — the naming
/// pass's own stream, so only later names move — and, if a tongue's stock is too
/// small to find a fresh name, lengthens the name by another root of the same tongue
/// until it is fresh. Empty only when the tongue cannot coin at all.
std::string generate_unique_city_name(std::mt19937& rng, const tongue& t,
                                      std::unordered_set<std::string>& used);

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

/// BL-1708 (Ben, 2026-11-19; PROVINCES.md § A province's name): name EVERY province
/// of the canonical partition into `world::province_name`, replacing what was there.
///
/// A province holding a population centre takes the centre's name (the lowest centre
/// id where it holds two). Every other one - a sea province, an uncentred pocket, a
/// province on a body nobody settled - coins a name in the tongue of the people whose
/// ground it is: on @p home_body, the plurality culture of the region nearest the
/// province's id tile (the "whose ground" rule `name_population_centres` names a city
/// by); on any other body, or where no culture answers, a tongue rolled from the
/// body's own id. The coining is a PURE FUNCTION of that tongue and the province id -
/// a stream seeded from the id alone, never a generation stream - so it moves nothing
/// else the seed produces. Names are unique on their body: centre names are taken
/// first, then provinces claim theirs in ascending id order, a collision lengthening
/// the name by another root of the same tongue.
///
/// Run once the canonical partition and every centre exist (run_tail, after
/// `seed_province_holders`).
void name_provinces(world& w, entity_id home_body, int home_gw,
                    const settlement_state& ss, const creed_state& cs);
