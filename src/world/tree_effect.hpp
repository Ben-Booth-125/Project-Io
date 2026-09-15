#pragma once
// tree_effect.hpp — the ONE effect vocabulary every generated tree table
// (src/world/<tree>_tree_data.hpp) speaks, and the sim reads (BL-973).
//
// docs/generation/trees/TREES.md sec Effects and the sim's terms: an effect is
// `(kind, target, per_mille)`; the eleven kinds are the closed union
// tree_lint.js validates, and a `modifier`'s target is one of the twelve
// terms in the same doc's table. This header is hand-written and SHARED so
// that one `apply_tree_effects` can fold every tree's table without a
// per-tree twin: the enums here are the store's vocabulary transcribed, and
// tree_lint.js holds the store to the same set (EFFECT_KINDS, MOD_TERMS,
// EFFECT_KEYS). Change the vocabulary in the lint and here in the same
// commit, or the generator's `static_assert`s below fail.
//
// A non-modifier effect the sim reads BY IDENTITY (an access the subjection
// block gates on, an upgrade the road ladder gates on) carries a machine
// `key` in the store beside its prose target, so that no node index and no
// id string is ever compared inside the sim (the three hand-wired nodes this
// item retired). An `open` effect's target is parsed by the generator into
// `open_ring` / `open_tree`; everything else is prose the sim does not read.

#include <cstdint>

namespace io {

/// The eleven effect kinds (TECH_EFFECTS.md's closed union, as TREES.md
/// transcribes it). Order matches tree_lint.js EFFECT_KINDS.
enum class tree_effect_kind : uint8_t
{
    unlock = 0, upgrade, retire, modifier, access, reach, intel,
    institution, doctrine, resource, open,
};
inline constexpr int tree_effect_kind_count = 11;
inline constexpr const char* tree_effect_kind_names[tree_effect_kind_count] = {
    "unlock", "upgrade", "retire", "modifier", "access", "reach", "intel",
    "institution", "doctrine", "resource", "open",
};

/// A `modifier` effect's target: the twelve sim terms of TREES.md's table.
/// Order matches tree_lint.js MOD_TERMS and indexes `polity::tree_mod_q`.
enum class tree_modifier_term : int8_t
{
    none = -1,
    reach = 0, carrying_capacity, manpower, defence, industrial, stores,
    cohesion, assimilation, plague, research, forage, muster_cost,
};
inline constexpr int tree_modifier_term_count = 12;
inline constexpr const char* tree_modifier_term_names[tree_modifier_term_count] = {
    "reach", "carrying_capacity", "manpower", "defence", "industrial", "stores",
    "cohesion", "assimilation", "plague", "research", "forage", "muster_cost",
};

/// The machine key a non-modifier effect carries when the sim reads it by
/// identity (store field `key`; tree_lint.js EFFECT_KEYS). `none` is every
/// effect whose target is prose only. A key is a BIT INDEX into
/// `polity::tree_keys`, so at most 32 keys — far more than any sim reader
/// set will want.
enum class tree_effect_key : uint8_t
{
    none = 0,
    sea_legs,   ///< access: a crossing to unmet ground needs no adjacent shore (subjection)
    post_roads, ///< upgrade: the road ladder's third rung may be bought with capital
};
inline constexpr int tree_effect_key_count = 3;
inline constexpr const char* tree_effect_key_names[tree_effect_key_count] = {
    "none", "sea_legs", "post_roads",
};

/// One effect row in a generated tree table. Nodes index a contiguous run
/// of these (`node::effects_begin`, `node::effects_n`).
struct tree_effect
{
    tree_effect_kind   kind;
    tree_modifier_term term;      ///< modifier only; `none` otherwise
    int16_t            per_mille; ///< modifier only; 0 otherwise
    tree_effect_key    key;       ///< identity read, or `none`
    uint8_t            open_ring; ///< open only: the ring this opens, or 0
    bool               open_tree; ///< open only: opens the NEXT tree (the rim)
    const char*        target;    ///< the store's prose, for reports and the harness
};

} // namespace io
