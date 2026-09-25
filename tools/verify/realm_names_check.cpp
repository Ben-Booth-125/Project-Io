// ---------------------------------------------------------------------------
// realm_names_check — a realm's name crosses the seams and the fold
// (BL-1088 REALM_KEEPS_ITS_NAME R1/R4; BL-1089 REALM_BECOMES_NATION R1)
// ---------------------------------------------------------------------------
// Builds the SHIPPED default world once (seed 0 unless --seed N; epoch 1960;
// the three spans) with a generation report, and checks what the record and
// the fold record say against the world that was made from them:
//
//   N1  Every polity on every record carries a coined, non-empty name — the
//       six sites that push a polity onto the table all coin (history_sim.cpp,
//       `coin_name_for`), so an empty entry is a site that was missed or a
//       tongue that could not coin, and either is reported by count.
//   N2  A shared polity id carries ONE name across prehistory -> exploration
//       -> industrialisation: the resume carries the table by id and coins
//       nothing (CIVILISATION.md § A realm's name). Zero renames.
//   N3  A resumed record opens with `inherited`, never a `founded` for the
//       same realm in its opening year: nothing rises twice (BL-1088 R4).
//   N4  The fold record is well-formed: as many ids as nations the world holds,
//       the four per-nation arrays agree, every absorbed range lies inside the
//       flat list, no polity is both a nation's own realm and an absorbed one,
//       and no absorbed id repeats.
//   N5  THE ITEM-SPANNING GATE (BL-1089 R1): every nation whose fold
//       representative carried a polity has THAT polity's coined name,
//       verbatim, on `nation_component::name` — merge rule A read off the
//       world itself, not off the pass that wrote it.
//   N6  Every nation of ownerless ground still carries a non-empty name (the
//       one kind Pass 5 coins).
//
// Reported beside the checks: names per record, renames, absorbed realms,
// ownerless nations. Exits non-zero on any FAIL.
//
//   bash tools/verify/build_lua_harness.sh realm_names_check --run
//   build_gen/verify/realm_names_check.exe [--seed N]

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/world.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const char* what)
{
    std::printf("  %s  %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

/// Names on @p rec, or an empty table when the record never ran.
const std::vector<std::string>& names_of(const era_timelapse& rec) { return rec.polity_name; }

} // namespace

int main(int argc, char** argv)
{
    uint32_t seed = 0;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));

    world_params p = arc_params(world_arc::shipped);
    p.seed         = seed;

    std::printf("realm_names_check: seed %u, the shipped arc (epoch %lld, prehistory %d y)\n",
                seed, static_cast<long long>(p.epoch_year), p.prehistory_years);

    generation_report     rep{};
    era_minus_one_fixture fx{};
    const auto t0 = std::chrono::steady_clock::now();
    world       w  = make_hard_coded_world(p, &rep, {}, nullptr, nullptr, &fx);
    std::printf("  built in %.1f s\n",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());

    const generation_report::body_entry* home = nullptr;
    for (const generation_report::body_entry& b : rep.bodies)
        if (b.is_homeworld) { home = &b; break; }
    check(home != nullptr, "the report carries a homeworld entry");
    if (home == nullptr) return 1;

    const era_timelapse* records[3] = { &home->prehistory_timelapse, &home->exploration_timelapse,
                                        &home->industrialisation_timelapse };
    const char* record_names[3] = { "prehistory", "exploration", "industrialisation" };

    // --- N1: every polity named, per record --------------------------------
    for (int r = 0; r < 3; ++r)
    {
        const era_timelapse& rec = *records[r];
        if (rec.empty() && names_of(rec).empty()) { std::printf("  (%s record: none)\n", record_names[r]); continue; }
        std::size_t named = 0;
        for (const std::string& n : names_of(rec)) if (!n.empty()) ++named;
        std::printf("  %-18s %zu polities on the table, %zu named\n", record_names[r],
                    names_of(rec).size(), named);
        char buf[160];
        std::snprintf(buf, sizeof buf, "N1 %s: every polity on the record carries a coined name (%zu of %zu)",
                      record_names[r], named, names_of(rec).size());
        check(!names_of(rec).empty() && named == names_of(rec).size(), buf);
    }

    // --- N2: one name per id across the seams -------------------------------
    {
        int renamed = 0, shared = 0;
        for (int r = 1; r < 3; ++r)
        {
            const std::vector<std::string>& a = names_of(*records[r - 1]);
            const std::vector<std::string>& b = names_of(*records[r]);
            const std::size_t n = a.size() < b.size() ? a.size() : b.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                ++shared;
                if (a[i] != b[i]) ++renamed;
            }
            // A resumed table only ever GROWS: the successor holds every id the
            // predecessor did.
            char buf[160];
            std::snprintf(buf, sizeof buf, "N2 the %s table holds every id the %s table did (%zu >= %zu)",
                          record_names[r], record_names[r - 1], b.size(), a.size());
            check(b.size() >= a.size(), buf);
        }
        char buf[160];
        std::snprintf(buf, sizeof buf, "N2 a shared id carries one name across the seams (%d shared, %d renamed)",
                      shared, renamed);
        check(shared > 0 && renamed == 0, buf);
    }

    // --- N3: a resumed record inherits, it does not re-found ----------------
    for (int r = 1; r < 3; ++r)
    {
        const era_timelapse& rec = *records[r];
        if (rec.empty()) continue;
        std::set<uint16_t> inherited, founded_at_open;
        for (const lapse_event& e : rec.events)
        {
            if (e.year != rec.start_year) continue;
            if (e.kind == static_cast<uint8_t>(lapse_event_kind::inherited)) inherited.insert(e.polity);
            if (e.kind == static_cast<uint8_t>(lapse_event_kind::founded))   founded_at_open.insert(e.polity);
        }
        int twice = 0;
        for (const uint16_t q : founded_at_open) if (inherited.count(q)) ++twice;
        char buf[200];
        std::snprintf(buf, sizeof buf,
                      "N3 the %s open states its living realms as inherited, none rising twice "
                      "(%zu inherited, %zu founded at the open, %d both)",
                      record_names[r], inherited.size(), founded_at_open.size(), twice);
        check(!inherited.empty() && twice == 0, buf);
    }

    // --- N4: the fold record is well-formed --------------------------------
    const std::size_t nn = home->nation_ids.size();
    {
        std::size_t world_nations = 0;
        for (const auto& kv : w.nations) { (void)kv; ++world_nations; }
        char buf[200];
        std::snprintf(buf, sizeof buf, "N4 the fold record names every nation the world holds (%zu ids, %zu nations)",
                      nn, world_nations);
        check(nn > 0 && nn == world_nations, buf);
        check(home->nation_polity.size() == nn && home->nation_absorbed_first.size() == nn
                  && home->nation_absorbed_count.size() == nn,
              "N4 the four per-nation arrays agree in length");
        bool ranges_ok = true;
        std::set<int32_t> own, absorbed_seen;
        int absorbed_total = 0, dup = 0, both = 0;
        for (std::size_t n = 0; n < nn && n < home->nation_polity.size(); ++n)
            if (home->nation_polity[n] >= 0) own.insert(home->nation_polity[n]);
        for (std::size_t n = 0; n < nn && n < home->nation_absorbed_first.size(); ++n)
        {
            const int64_t first = home->nation_absorbed_first[n], count = home->nation_absorbed_count[n];
            if (first < 0 || count < 0 || first + count > static_cast<int64_t>(home->nation_absorbed.size()))
            { ranges_ok = false; continue; }
            for (int64_t k = 0; k < count; ++k)
            {
                const int32_t q = home->nation_absorbed[static_cast<std::size_t>(first + k)];
                ++absorbed_total;
                if (own.count(q)) ++both;
                if (!absorbed_seen.insert(q).second) ++dup;
            }
        }
        check(ranges_ok, "N4 every absorbed range lies inside the flat list");
        std::snprintf(buf, sizeof buf, "N4 no polity is both a nation's own realm and an absorbed one, none absorbed twice "
                      "(%d absorbed, %d both, %d repeated)", absorbed_total, both, dup);
        check(both == 0 && dup == 0, buf);
        check(static_cast<int>(home->nation_absorbed.size()) == absorbed_total,
              "N4 the flat list holds exactly the absorbed realms the ranges cover");
    }

    // --- N5: the gate — the nation carries its realm's name verbatim ---------
    {
        // The last close's table: the same read the seat card makes.
        const std::vector<std::string>* names = nullptr;
        for (int r = 2; r >= 0 && names == nullptr; --r)
            if (!names_of(*records[r]).empty()) names = &names_of(*records[r]);
        int with_realm = 0, matched = 0, ownerless = 0, ownerless_named = 0, unnamed_realm = 0;
        for (std::size_t n = 0; n < nn && n < home->nation_polity.size(); ++n)
        {
            const auto it = w.nations.find(home->nation_ids[n]);
            if (it == w.nations.end()) continue;
            const int32_t pol = home->nation_polity[n];
            if (pol < 0)
            {
                ++ownerless;
                if (!it->second.name.empty()) ++ownerless_named;
                continue;
            }
            ++with_realm;
            const std::string realm =
                (names != nullptr && static_cast<std::size_t>(pol) < names->size()) ? (*names)[static_cast<std::size_t>(pol)]
                                                                                    : std::string{};
            if (realm.empty()) { ++unnamed_realm; continue; }
            if (it->second.name == realm) ++matched;
            else if (matched + 1 == with_realm - unnamed_realm) // first mismatch: say which
                std::printf("     mismatch: nation %llu '%s' vs realm %d '%s'\n",
                            static_cast<unsigned long long>(home->nation_ids[n]), it->second.name.c_str(),
                            pol, realm.c_str());
        }
        std::printf("  %d nations of a realm, %d of ownerless ground; %d absorbed realms listed\n",
                    with_realm, ownerless, static_cast<int>(home->nation_absorbed.size()));
        char buf[220];
        std::snprintf(buf, sizeof buf,
                      "N5 every nation with a founding realm carries that realm's coined name verbatim "
                      "(%d of %d; %d realms had no coined name)",
                      matched, with_realm, unnamed_realm);
        check(with_realm > 0 && matched == with_realm - unnamed_realm && unnamed_realm == 0, buf);
        std::snprintf(buf, sizeof buf, "N6 every nation of ownerless ground carries a coined name (%d of %d)",
                      ownerless_named, ownerless);
        check(ownerless_named == ownerless, buf);
    }

    std::printf("realm_names_check: %s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILED",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
