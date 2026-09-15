// ---------------------------------------------------------------------------
// Shared harness helper — the cultures that never hold ground (BL-968 step 1)
// ---------------------------------------------------------------------------
// About 95% of coined cultures never hold a region (DEVLOG 2026-09-11). The
// split census counts them as diversity, the Empires round and the lineage
// palette inherit their names, and `culture_kinship_years` walks a tree most
// of whose leaves are empty. Before any rule is chosen (fold an empty culture
// back into its parent, or gate coining on a footprint — Ben's call, NOT
// built here) the number has to be on the table, and both `history_sweep`
// and `colonisation_harness` have to read it the SAME way. This header is
// that one reading.
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
    std::vector<int> empty_leaf_depth;

    /// Per-mille of coined cultures holding ground at the boundary; 0 if none coined.
    int holding_boundary_permille() const
    {
        return coined > 0 ? static_cast<int>((static_cast<int64_t>(holding_boundary) * 1000) / coined) : 0;
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
    for (std::size_t i = 0; i < n; ++i)
    {
        const int par = cultures[i].parent;
        if (par >= 0 && static_cast<std::size_t>(par) < n) has_child[static_cast<std::size_t>(par)] = 1u;
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
               && cultures[static_cast<std::size_t>(at)].parent >= 0
               && cultures[static_cast<std::size_t>(at)].parent < at)
        { at = cultures[static_cast<std::size_t>(at)].parent; ++depth; }
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
