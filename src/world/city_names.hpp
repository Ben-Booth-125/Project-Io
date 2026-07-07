#pragma once

#include <random>
#include <string>

/// Generate one procedural city / settlement name, deterministically from @p rng.
///
/// A small self-contained place-name generator for the generation pass: a consonant
/// onset + vowel + optional medial forms a root, to which an English-ish place suffix
/// is attached (-ton, -ford, -haven, -moor, …), with an occasional directional/quality
/// prefix (New, North, High, …). Draws only from @p rng, so callers control determinism
/// (population_generation uses an independent stream so naming does not perturb the
/// world). Used for population-centre / market-city identity. Free function at global
/// scope, matching the other generation entry points (`world` is a struct, not a namespace).
std::string generate_city_name(std::mt19937& rng);
