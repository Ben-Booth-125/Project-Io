#pragma once

// ---------------------------------------------------------------------------
// Body names — the system's own catalogue, coined rather than authored
// (BL-257).
//
// The five bodies used to carry string literals (Helios / Cinder / Kepler /
// Selene / Pallas), which made the solar system read as authored rather than
// generated. They are now coined, deterministically, from ONE tongue —
// world/tongue.hpp, the same phoneme inventory and word builder every culture,
// nation and city name comes from (BL-290). Bodies are therefore not a fourth
// naming bank; they are the same sound system applied to the sky.
//
// WHY ONE SEED-ROLLED TONGUE AND NOT A CULTURE'S. The cradle cultures (and
// their tongues) do not exist until the creeds pass, which needs the homeworld's
// tiles, which needs the planetology chain — and the chain WRITES BODY NAMES
// INTO THE BIOGRAPHY PROSE it emits ("<name> settles into an orbit that nearly
// matches the star's disc"). A name decided after the chain would leave that
// prose stale. So the sky-watchers' tongue is rolled from the world seed before
// anything else runs. It shares the machinery, not one culture's inventory.
//
// THE REGISTER RULE (BL-257 § Register/tone; Ben, 2026-08-01 — "a deliberate
// mix is great", governed by a rule so the mixing is legible rather than
// noisy). Which register a body gets is DECIDED BY WHAT THE BODY IS, and each
// register wears a different SHAPE, so a player can half-perceive the rule
// without ever articulating it:
//
//   * Star and planets — MYTHOLOGICAL. One bare coined word, two or three
//     syllables, no affix. Named first, from a distance, by people who could
//     only see lights.
//   * Moons — DESCRIPTIVE, and tied to their parent. A coined qualifier from
//     the same tongue, then the parent planet's name — two words, so a moon
//     always reads as belonging to its planet. Named later, close up, by
//     whoever surveyed them.
//   * The home body — the odd one out, deliberately. A coined root plus the
//     tongue's own "place" morpheme (`tongue_lexicon::settlement`): the one
//     world named by people standing ON it, describing the ground rather than
//     a light in the sky.
//   * Asteroids — CATALOGUE. A coined word plus its catalogue index. Neither
//     observed as a light nor walked on: an entry in a list.
//
// A later session extending this must extend it the same way — an unstated
// rule decays into the noise it was meant to prevent. Authority:
// docs/generation/PLANETOLOGY.md § Body naming.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <string>
#include <vector>

/// Every name in one system, coined together so no two collide.
struct body_naming
{
    std::string              star;   ///< The central star.
    std::vector<std::string> bodies; ///< One per `prototype_body` index, in that order.
};

/// Coin the whole system's catalogue. A PURE function of @p seed: same seed,
/// same names, run to run — the standing determinism invariant, and what the
/// goldens and the determinism harnesses depend on. No duplicates within a
/// system, moons included.
body_naming generate_body_names(uint32_t seed);
