// ---------------------------------------------------------------------------
// span_seed_isolation — does a per-span reroll counter re-seed ONLY its own span?
// (BL-1083, one seed per span; Lua-free: node tools/verify/build_harness.js
// span_seed_isolation --run)
// ---------------------------------------------------------------------------
// THE CLAIM UNDER TEST (STARTUP.md § Each pass round is rerollable; the fold
// rule on `world_params::span_seed`): a round-N reroll re-seeds span N and
// nothing else. `span_seed[k]` is folded into exactly one span's seed — [0]
// the migration (`run_settlement`), [1] Empires, [2] Exploration, [3]
// Industrialisation — so a non-zero slot k must change span k's record and,
// because every later span resumes from the world span k left, every later
// span's record too, while every EARLIER span's record stays byte-identical
// to the unrerolled build. That last half is the property the wizard's
// "rounds above keep the record they show" rests on, and it is a property of
// the arithmetic, not of the UI: this harness checks it with no UI in the
// loop.
//
// WHAT IT DOES. For each variant of `world_params{}` (seed 0 unless --seed):
//   zero        all four slots 0                — the unrerolled world;
//   zero-again  the same, built a second time   — the control: proves the
//                                                 digest below is stable across
//                                                 builds, so "identical" means
//                                                 something;
//   slot k = 1  for k in 0..3                   — one reroll on round k;
//   era_seed 1  the legacy term                 — documents its reach: every
//                                                 history span, never the
//                                                 migration.
// it builds the world TWICE the way the wizard does — once stopped after the
// migration (`world_gen_config::stop_after_migration`), which is the Culture
// round's own record as generation folds it, and once stopped after the
// Industrialisation span (`stop_after_industrialisation`), which leaves the
// Empires, Exploration and Industrialisation records on the report and runs
// nothing of world setup — and digests the four `era_timelapse` records field
// by field (no struct padding in the hash). The rows then compare each
// variant's four digests to `zero`'s.
//
// ROWS.
//   C1  zero-again == zero on all four records (the control).
//   S0  slot 0 = 1: the migration differs                       (R3 of the group)
//       and so do Empires, Exploration, Industrialisation.
//   S1  slot 1 = 1: the migration is identical; Empires, Exploration and
//       Industrialisation differ.
//   S2  slot 2 = 1: the migration AND Empires are identical; Exploration
//       differs, Industrialisation differs                      (R2 of the group)
//   S3  slot 3 = 1: migration, Empires, Exploration identical; Industrialisation
//       differs.
//   E1  era_seed = 1: the migration is identical; the three history spans differ.
//
// "Differs" is a digest inequality, and a rerolled span that happened to
// reproduce its record digit for digit would fail the row — over thousands of
// dated changes that is not a coincidence a seed produces, and a FAIL there
// would mean the slot is not read at all, which is exactly the defect worth
// failing on.
//
// The process exits non-zero if any row FAILs. Links only the SDL/Lua-free
// world translation units.

#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"
#include "world/world_gen_config.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

int g_failures = 0;

void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}

// FNV-1a over the record's FIELDS — never over the struct bytes, because
// `polity_sample` and `lapse_event` carry padding and a padding byte is not a
// property of the history.
struct fnv
{
    uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, std::size_t n)
    {
        const unsigned char* c = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i)
        {
            h ^= c[i];
            h *= 1099511628211ull;
        }
    }
    template <class T> void v(const T& x) { bytes(&x, sizeof x); }
};

uint64_t digest(const era_timelapse& t)
{
    fnv f;
    f.v(t.region_stride);
    f.v(t.start_year);
    f.v(t.years);
    f.v(static_cast<uint64_t>(t.changes.size()));
    for (const owner_change& c : t.changes)
    {
        f.v(c.year); f.v(c.region); f.v(c.owner);
    }
    f.v(static_cast<uint64_t>(t.steps.size()));
    for (const timelapse_step& s : t.steps)
    {
        f.v(s.year); f.v(s.first_sample); f.v(s.sample_count);
    }
    f.v(static_cast<uint64_t>(t.samples.size()));
    for (const polity_sample& s : t.samples)
    {
        f.v(s.population); f.v(s.polity); f.v(s.regions);
        f.v(s.cap_military); f.v(s.cap_materials); f.v(s.industry_points);
    }
    f.v(static_cast<uint64_t>(t.culture_changes.size()));
    for (const culture_change& c : t.culture_changes)
    {
        f.v(c.year); f.v(c.region);
        for (int k = 0; k < timelapse_culture_slots; ++k) { f.v(c.id[k]); f.v(c.weight_q[k]); }
        f.v(c.other_q);
    }
    f.v(static_cast<uint64_t>(t.events.size()));
    for (const lapse_event& e : t.events)
    {
        f.v(e.year); f.v(e.kind); f.v(e.region); f.v(e.polity); f.v(e.other);
    }
    return f.h;
}

/// The four records, one per lapse round, indexed exactly as `span_seed` is.
struct span_digests
{
    uint64_t d[4] = {0, 0, 0, 0};
    std::size_t changes[4] = {0, 0, 0, 0}; ///< For the report line: an empty record is a build that did not run the span.
};

const char* span_name(int k)
{
    switch (k)
    {
        case 0:  return "migration";
        case 1:  return "Empires";
        case 2:  return "Exploration";
        default: return "Industrialisation";
    }
}

const generation_report::body_entry* homeworld(const generation_report& rep)
{
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld) return &b;
    return nullptr;
}

span_digests build_and_digest(const world_params& p, const char* what)
{
    span_digests out;
    const auto t0 = std::chrono::steady_clock::now();

    // The Culture round's build: stopped after the migration, whose record is
    // the report's `prehistory_timelapse` on that path (hard_coded_world.cpp,
    // the `stop_after_migration` return).
    {
        world_gen_config cfg{};
        cfg.stop_after_migration = true;
        generation_report rep;
        (void)make_hard_coded_world(p, &rep, cfg);
        if (const generation_report::body_entry* home = homeworld(rep))
        {
            out.d[0]       = digest(home->prehistory_timelapse);
            out.changes[0] = home->prehistory_timelapse.changes.size();
        }
    }
    // The three history spans in one build, stopped before world setup.
    {
        world_gen_config cfg{};
        cfg.stop_after_industrialisation = true;
        generation_report rep;
        (void)make_hard_coded_world(p, &rep, cfg);
        if (const generation_report::body_entry* home = homeworld(rep))
        {
            out.d[1]       = digest(home->prehistory_timelapse);
            out.changes[1] = home->prehistory_timelapse.changes.size();
            out.d[2]       = digest(home->exploration_timelapse);
            out.changes[2] = home->exploration_timelapse.changes.size();
            out.d[3]       = digest(home->industrialisation_timelapse);
            out.changes[3] = home->industrialisation_timelapse.changes.size();
        }
    }

    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  [%-11s] %6.2f s  era_seed %u  span_seed {%u,%u,%u,%u}\n", what, secs,
                p.era_seed, p.span_seed[0], p.span_seed[1], p.span_seed[2], p.span_seed[3]);
    for (int k = 0; k < 4; ++k)
        std::printf("      %-17s %016llX  (%zu changes)\n", span_name(k),
                    static_cast<unsigned long long>(out.d[k]), out.changes[k]);
    return out;
}

/// One row per span: identical to `zero` below `from`, different at and after it.
void expect_reseeded_from(const span_digests& zero, const span_digests& v, int from,
                          const char* row)
{
    char buf[160];
    for (int k = 0; k < 4; ++k)
    {
        const bool same = zero.d[k] == v.d[k];
        const bool want_same = k < from;
        std::snprintf(buf, sizeof buf, "%s %s record %s the zero build's", row, span_name(k),
                      want_same ? "is identical to" : "differs from");
        check(same == want_same, buf);
    }
}

} // namespace

int main(int argc, char** argv)
{
    world_params base{};
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            base.seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));

    std::printf("span_seed_isolation (BL-1083) -- seed %u, epoch %lld, spans: Empires %lld..%lld,"
                " Exploration ..%lld, Industrialisation %s ..%lld\n",
                base.seed, static_cast<long long>(base.epoch_year),
                static_cast<long long>(base.empires_start_year),
                static_cast<long long>(base.empires_stop_year),
                static_cast<long long>(base.exploration_stop_year),
                base.industrialisation_span_enabled ? "on" : "OFF",
                static_cast<long long>(base.industrialisation_stop_year));

    std::printf("\nBuilds (each variant: one build stopped after the migration, one after the"
                " Industrialisation span):\n");
    const span_digests zero  = build_and_digest(base, "zero");
    const span_digests again = build_and_digest(base, "zero-again");

    span_digests slot[4];
    for (int k = 0; k < 4; ++k)
    {
        world_params p = base;
        p.span_seed[k] = 1u;
        char what[16];
        std::snprintf(what, sizeof what, "slot%d=1", k);
        slot[k] = build_and_digest(p, what);
    }
    world_params legacy = base;
    legacy.era_seed     = 1u;
    const span_digests era = build_and_digest(legacy, "era_seed=1");

    std::printf("\nRows:\n");
    // C1 — the control. Without it every "identical" below could be two
    // builds that agree by accident of an unread field.
    check(zero.d[0] == again.d[0] && zero.d[1] == again.d[1] && zero.d[2] == again.d[2]
              && zero.d[3] == again.d[3],
          "C1 the zero build's four records digest identically across two builds (the control)");
    check(zero.changes[0] > 0 && zero.changes[1] > 0 && zero.changes[2] > 0
              && zero.changes[3] > 0,
          "C2 every span actually ran and left a non-empty record on the zero build");

    expect_reseeded_from(zero, slot[0], 0, "S0 span_seed[0]=1:");
    expect_reseeded_from(zero, slot[1], 1, "S1 span_seed[1]=1:");
    expect_reseeded_from(zero, slot[2], 2, "S2 span_seed[2]=1:");
    expect_reseeded_from(zero, slot[3], 3, "S3 span_seed[3]=1:");
    // E1 — the legacy term reaches every history span and never the migration.
    expect_reseeded_from(zero, era, 1, "E1 era_seed=1 (legacy):");

    // THE READING BEHIND AN S0 MIGRATION FAIL, so the line is not a mystery.
    // `run_settlement`'s seed is the only place slot 0 is folded, and inside
    // that pass the seed reaches the daughters' tongue drift, coined names and
    // aggression (`derive_daughter_culture`) and the furnace lag
    // (`tag_furnace`) — never the walk. Founding years, the plurality culture
    // and the split years, which are ALL the migration record holds, come
    // from the colonisation diffusion, which COLONISATION.md § No actor pins
    // as a deterministic function of upstream scalars and never a roll. So a
    // slot-0 reroll forks every later span (aggression is what the Empires
    // sim reads) while the Culture round's own record stays byte-identical.
    // Whether the migration should carry dice is a design call, not a fold.
    if (zero.d[0] == slot[0].d[0])
        std::printf("\n  NOTE: S0's migration record is identical under span_seed[0]=1 because the\n"
                    "        colonisation walk consumes no seed (COLONISATION.md § No actor); the\n"
                    "        migration's seed reaches daughter-culture names, tongue drift and\n"
                    "        aggression, and the furnace lag -- which is why every later span moved.\n");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
