// ---------------------------------------------------------------------------
// Shared harness helper — the cultures that never hold ground (BL-968 step 1)
// ---------------------------------------------------------------------------
// About 95% of coined cultures never hold a region (DEVLOG 2026-09-11). The
// split census counts them as diversity, the Empires round and the lineage
// palette inherit their names, and `culture_kinship_years` walks a tree most
// of whose leaves are empty. Ben ruled on the number (NR-879, 2026-09-16):
// FOLD an empty culture back into its parent at the boundary rather than
// gate coining, and `run_settlement` does so (BL-1017). Both `history_sweep`
// and `colonisation_harness` read the empty mass AND the folded tree the SAME
// way, and this header is that one reading. The empty mass is read on the
// LINEAGE (`coined_from`), so it is the same number before and after the fold.
//
// REPORT-ONLY. Nothing here mutates a world or a culture list; it is a pure
// fold over what the fixture already carries. Determinism is untouched: the
// walk is index-ordered and a parent is always lower-indexed than its child
// (creeds.hpp, BL-865), so the depth walk is bounded and cannot loop.
//
// "Holding ground" is PLURALITY on at least one region (`culture_shares::
// plurality`), which is exactly what `run_history_sim` seeds a polity from
// (history_sim.cpp, "a people is present where it is the largest share") —
// so a culture this helper calls empty is one the Empires round never gave
// an actor. At the Culture/Empires boundary shares are pure, so plurality
// and founding agree there; at 1200 CE assimilation has moved them and only
// plurality is the honest test.
#pragma once

#include "world/creeds.hpp"
#include "world/settlement.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

/// One seed's culture footprint. Every count is over the SAME culture list
/// (`creed_state::cultures`, cradles and spawned daughters together, as
/// generation merged them before the Empires round).
struct culture_footprint
{
    int coined            = 0; ///< Every culture the world coined, cradles included.
    int holding_boundary  = 0; ///< Plurality on >= 1 region at the boundary (400 BCE).
    int holding_close     = 0; ///< Plurality on >= 1 region at the close (1200 CE); -1 if no close state.
    int empty_boundary    = 0; ///< `coined - holding_boundary`.
    int empty_leaves      = 0; ///< Empty at the boundary AND no child names it as parent.
    int empty_interior    = 0; ///< Empty at the boundary AND at least one child does.
    /// Empty leaves by depth (generations below a cradle); index 0 = a
    /// cradle that never held ground. Sized to the deepest empty leaf + 1.
    /// Depth is read on the LINEAGE (`coined_from`), so it describes where the
    /// migration coined the empty name, before or after the fold.
    std::vector<int> empty_leaf_depth;

    // --- BL-1017: the boundary fold, read off the same list -----------------
    int living          = 0; ///< The tree's size: cultures that kept their name (`folded_into` -1).
    int folded          = 0; ///< `coined - living`.
    int reparented      = 0; ///< Living cultures whose `parent` is not the one they were coined from.
    int deepest_living  = 0; ///< Deepest descent on the living tree, in generations below a cradle.
    /// Living cultures NOT holding plurality at the boundary: a cradle that
    /// never held ground, or a people whose only ground is a scheduled
    /// founding. The whole gap between `living` and `holding_boundary`.
    int living_without_plurality = 0;

    /// Per-mille of coined cultures holding ground at the boundary; 0 if none coined.
    int holding_boundary_permille() const
    {
        return coined > 0 ? static_cast<int>((static_cast<int64_t>(holding_boundary) * 1000) / coined) : 0;
    }
    /// BL-1017: per-mille of the FOLDED tree holding ground at the boundary.
    int holding_living_permille() const
    {
        return living > 0 ? static_cast<int>((static_cast<int64_t>(holding_boundary) * 1000) / living) : 0;
    }
};

/// Fold the footprint. @p close may be null (no 1200 CE state in hand), in
/// which case `holding_close` reads -1.
inline culture_footprint measure_culture_footprint(const std::vector<culture>& cultures,
                                                   const std::vector<region>&  boundary,
                                                   const std::vector<region>*  close)
{
    culture_footprint f;
    f.coined = static_cast<int>(cultures.size());
    const std::size_t n = cultures.size();

    std::vector<uint8_t> held_b(n, 0u), held_c(n, 0u), has_child(n, 0u);
    for (const region& p : boundary)
    {
        const int c = p.culture.plurality();
        if (c >= 0 && static_cast<std::size_t>(c) < n) held_b[static_cast<std::size_t>(c)] = 1u;
    }
    if (close != nullptr)
        for (const region& p : *close)
        {
            const int c = p.culture.plurality();
            if (c >= 0 && static_cast<std::size_t>(c) < n) held_c[static_cast<std::size_t>(c)] = 1u;
        }
    // THE LINEAGE, not the living tree (BL-1017): "empty leaf vs interior" is
    // a question about the tree the migration COINED, so it must read the same
    // before the fold as after it. `coined_from` where recorded, else `parent`.
    const auto coined_parent = [&](std::size_t i) {
        return cultures[i].coined_from >= 0 ? cultures[i].coined_from : cultures[i].parent;
    };
    for (std::size_t i = 0; i < n; ++i)
    {
        const int par = coined_parent(i);
        if (par >= 0 && static_cast<std::size_t>(par) < n) has_child[static_cast<std::size_t>(par)] = 1u;
    }

    // The fold, as the list carries it.
    std::vector<int> living_depth(n, 0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const culture& c = cultures[i];
        if (c.folded_into >= 0) { ++f.folded; continue; }
        ++f.living;
        if (!held_b[i]) ++f.living_without_plurality;
        if (c.coined_from >= 0 && c.parent != c.coined_from) ++f.reparented;
        if (c.parent >= 0 && static_cast<std::size_t>(c.parent) < i)
            living_depth[i] = living_depth[static_cast<std::size_t>(c.parent)] + 1;
        if (living_depth[i] > f.deepest_living) f.deepest_living = living_depth[i];
    }

    f.holding_close = close != nullptr ? 0 : -1;
    for (std::size_t i = 0; i < n; ++i)
    {
        f.holding_boundary += held_b[i];
        if (close != nullptr) f.holding_close += held_c[i];
        if (held_b[i]) continue;
        ++f.empty_boundary;
        if (has_child[i]) { ++f.empty_interior; continue; }
        ++f.empty_leaves;
        // Depth: the bounded walk creeds.hpp's kinship helper relies on.
        int depth = 0;
        int at = static_cast<int>(i);
        while (at >= 0 && static_cast<std::size_t>(at) < n
               && coined_parent(static_cast<std::size_t>(at)) >= 0
               && coined_parent(static_cast<std::size_t>(at)) < at)
        { at = coined_parent(static_cast<std::size_t>(at)); ++depth; }
        if (static_cast<std::size_t>(depth) >= f.empty_leaf_depth.size())
            f.empty_leaf_depth.resize(static_cast<std::size_t>(depth) + 1, 0);
        ++f.empty_leaf_depth[static_cast<std::size_t>(depth)];
    }
    return f;
}

/// The depth histogram as "d0:n d1:n ...", for a one-line print.
inline void print_empty_leaf_depths(const culture_footprint& f)
{
    if (f.empty_leaf_depth.empty()) { std::printf("(none)"); return; }
    for (std::size_t d = 0; d < f.empty_leaf_depth.size(); ++d)
        if (f.empty_leaf_depth[d] > 0)
            std::printf("%sd%zu:%d", d ? " " : "", d, f.empty_leaf_depth[d]);
}
