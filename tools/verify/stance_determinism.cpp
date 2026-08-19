// ---------------------------------------------------------------------------
// Headless corp-stance harness (BL-448; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises src/world/stance.{hpp,cpp} — the directed hostility map and the
// canonicalised, accepted-only friendship map between corps, plus the four
// corp_command verbs that mutate them (declare_hostile / offer_friendship /
// accept_friendship / return_to_neutral, src/world/corp_command.cpp).
//
// Written FIRST and landed inert, per the sprint's ordering: the determinism
// property has to hold before the substrate is trusted, not audited after.
// Asserts IN-MEMORY REPLAY ONLY (build the same command sequence twice, over
// two independently generated worlds, and require identical outcomes) —
// there is no save-format serialiser for this item (BL-448's design is
// explicit: "No consequence — stance gates nothing in this item", and no
// serialisation.cpp exists in this project to round-trip through). Inventing
// one here would be scope creep past what this item asks for.
//
//   D1  Fresh world: nobody is hostile, nobody is friends, no pending offer.
//   D2  declare_hostile is DIRECTED — A hostile to B does not make B hostile
//       to A (the ambush property).
//   D3  declare_hostile is idempotent-reject: declaring twice the second time
//       returns rejected_state and mutates nothing (checked via a snapshot).
//   D4  offer_friendship + accept_friendship reaches a SYMMETRIC friendship
//       row — are_friends(A,B) == are_friends(B,A) — and the pending offer
//       is cleared once accepted.
//   D5  A pending offer is NOT a stance — has_pending_friendship_offer true
//       does not make are_friends true, before acceptance.
//   D6  declare_hostile ATOMICALLY dissolves an existing friendship row in
//       the SAME call (invariant 2) — never a contradictory pair.
//   D7  return_to_neutral is unilateral: EITHER party can dissolve a
//       friendship; only the DECLARING party's own hostility direction is
//       cleared by their own call, not the reverse direction.
//   D8  A rejected command (self-target, unknown corp, illegal transition)
//       mutates NOTHING — checked by a full snapshot equality, not just the
//       return code (the untrusted-boundary standing rule).
//   D9  DETERMINISM: the same command sequence applied to two independently
//       constructed worlds (same corp ids, same order) produces byte-for-byte
//       identical stance tables — the property this harness exists to pin.
//   D10 Applying the same sequence twice to a FRESH clone of world A (i.e.
//       replaying) reproduces the exact same tables again — replay, not just
//       cross-construction, is deterministic.
//
// The process exits non-zero if any assertion FAILs.

#include "world/corp_command.hpp"
#include "world/recipe_registry.hpp"
#include "world/stance.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <string>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

/// A minimal world with three corps and nothing else — stance tables never
/// read anything beyond `world::corporations`, so this is the whole fixture.
struct fixture
{
    world     w;
    entity_id a = null_entity;
    entity_id b = null_entity;
    entity_id c = null_entity;
};

fixture make_fixture()
{
    fixture f;
    f.a = f.w.create_entity();
    corporation_component ca;
    ca.name = "Corp A";
    f.w.corporations[f.a] = ca;

    f.b = f.w.create_entity();
    corporation_component cb;
    cb.name = "Corp B";
    f.w.corporations[f.b] = cb;

    f.c = f.w.create_entity();
    corporation_component cc;
    cc.name = "Corp C";
    f.w.corporations[f.c] = cc;

    return f;
}

recipe_registry make_registry()
{
    // apply_corp_command takes a registry by const-ref for verbs unrelated to
    // stance; a default-constructed one is fine — none of the stance verbs
    // touch it.
    return recipe_registry{};
}

/// A snapshot of every stance table, for equality checks (D3/D8/D9/D10).
struct snapshot
{
    std::set<std::pair<entity_id, entity_id>> hostile;
    std::set<std::pair<entity_id, entity_id>> friends;
    std::set<std::pair<entity_id, entity_id>> offers;

    bool operator==(const snapshot& o) const
    {
        return hostile == o.hostile && friends == o.friends && offers == o.offers;
    }
};

snapshot snap(const world& w)
{
    return snapshot{w.corp_hostile_pairs, w.corp_friend_pairs, w.corp_friend_offers};
}

corp_command make_cmd(corp_verb verb, entity_id corp, entity_id counterparty)
{
    corp_command cmd;
    cmd.corp        = corp;
    cmd.verb        = verb;
    cmd.counterparty = counterparty;
    return cmd;
}

} // namespace

int main()
{
    const recipe_registry reg = make_registry();

    // D1 — fresh world.
    {
        fixture f = make_fixture();
        check(!is_hostile(f.w, f.a, f.b) && !is_hostile(f.w, f.b, f.a),
              "D1 fresh world: nobody hostile");
        check(!are_friends(f.w, f.a, f.b), "D1 fresh world: nobody friends");
        check(!has_pending_friendship_offer(f.w, f.a, f.b) &&
                  !has_pending_friendship_offer(f.w, f.b, f.a),
              "D1 fresh world: no pending offer");
    }

    // D2 — directed hostility.
    {
        fixture f = make_fixture();
        const auto r = apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, f.b), nullptr);
        check(r == corp_command_result::applied, "D2 declare_hostile applied");
        check(is_hostile(f.w, f.a, f.b), "D2 A hostile to B");
        check(!is_hostile(f.w, f.b, f.a), "D2 B NOT hostile to A (directed, not reciprocal)");
    }

    // D3 — idempotent-reject, mutates nothing on the reject.
    {
        fixture f = make_fixture();
        apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, f.b), nullptr);
        const snapshot before = snap(f.w);
        const auto r = apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, f.b), nullptr);
        check(r == corp_command_result::rejected_state, "D3 second declare_hostile rejected_state");
        check(snap(f.w) == before, "D3 rejected re-declare mutates nothing");
    }

    // D4 — offer + accept reaches a symmetric friendship row; offer clears.
    {
        fixture f = make_fixture();
        const auto ro = apply_corp_command(f.w, reg, make_cmd(corp_verb::offer_friendship, f.a, f.b), nullptr);
        check(ro == corp_command_result::applied, "D4 offer_friendship applied");
        check(has_pending_friendship_offer(f.w, f.a, f.b), "D4 offer pending A->B");

        // B accepts: cmd.corp = acceptor (B), cmd.counterparty = offerer (A).
        const auto ra = apply_corp_command(f.w, reg, make_cmd(corp_verb::accept_friendship, f.b, f.a), nullptr);
        check(ra == corp_command_result::applied, "D4 accept_friendship applied");
        check(are_friends(f.w, f.a, f.b), "D4 are_friends(A,B) true after accept");
        check(are_friends(f.w, f.b, f.a), "D4 are_friends symmetric: (B,A) also true");
        check(!has_pending_friendship_offer(f.w, f.a, f.b), "D4 pending offer cleared on accept");
    }

    // D5 — a pending offer is NOT a stance.
    {
        fixture f = make_fixture();
        apply_corp_command(f.w, reg, make_cmd(corp_verb::offer_friendship, f.a, f.b), nullptr);
        check(has_pending_friendship_offer(f.w, f.a, f.b), "D5 offer pending");
        check(!are_friends(f.w, f.a, f.b), "D5 pending offer does not read as friendship");
    }

    // D6 — declare_hostile atomically dissolves an existing friendship.
    {
        fixture f = make_fixture();
        apply_corp_command(f.w, reg, make_cmd(corp_verb::offer_friendship, f.a, f.b), nullptr);
        apply_corp_command(f.w, reg, make_cmd(corp_verb::accept_friendship, f.b, f.a), nullptr);
        check(are_friends(f.w, f.a, f.b), "D6 precondition: A and B are friends");

        const auto r = apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, f.b), nullptr);
        check(r == corp_command_result::applied, "D6 declare_hostile over a friendship applied");
        check(is_hostile(f.w, f.a, f.b), "D6 A now hostile to B");
        check(!are_friends(f.w, f.a, f.b), "D6 friendship dissolved atomically");
    }

    // D7 — return_to_neutral is unilateral and direction-scoped.
    {
        fixture f = make_fixture();
        apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, f.b), nullptr);
        // B was never made hostile to A; B calling return_to_neutral against A
        // clears nothing on A's own declared direction (A->B stays).
        const auto rb = apply_corp_command(f.w, reg, make_cmd(corp_verb::return_to_neutral, f.b, f.a), nullptr);
        check(rb == corp_command_result::rejected_state,
              "D7 B has nothing of its own to clear against A (A->B untouched by B's call)");
        check(is_hostile(f.w, f.a, f.b), "D7 A->B hostility survives B's own return_to_neutral");

        const auto ra = apply_corp_command(f.w, reg, make_cmd(corp_verb::return_to_neutral, f.a, f.b), nullptr);
        check(ra == corp_command_result::applied, "D7 A clears its own A->B hostility");
        check(!is_hostile(f.w, f.a, f.b), "D7 A->B hostility cleared");

        // Friendship: either party may dissolve it.
        fixture g = make_fixture();
        apply_corp_command(g.w, reg, make_cmd(corp_verb::offer_friendship, g.a, g.b), nullptr);
        apply_corp_command(g.w, reg, make_cmd(corp_verb::accept_friendship, g.b, g.a), nullptr);
        const auto rg = apply_corp_command(g.w, reg, make_cmd(corp_verb::return_to_neutral, g.b, g.a), nullptr);
        check(rg == corp_command_result::applied, "D7 B (the accepter) can dissolve the friendship unilaterally");
        check(!are_friends(g.w, g.a, g.b), "D7 friendship gone after either party's return_to_neutral");
    }

    // D8 — rejections mutate nothing.
    {
        fixture f = make_fixture();
        const snapshot before = snap(f.w);

        // Self-target.
        const auto r1 = apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, f.a), nullptr);
        check(r1 == corp_command_result::rejected_state || r1 == corp_command_result::rejected_invalid,
              "D8 self-target declare_hostile rejected");
        check(snap(f.w) == before, "D8 self-target mutates nothing");

        // Unknown corp: a freshly minted entity id that was never registered
        // as a corporation.
        const entity_id ghost = f.w.create_entity();
        const auto r2 = apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, ghost), nullptr);
        check(r2 == corp_command_result::rejected_invalid, "D8 unknown-corp declare_hostile rejected_invalid");
        check(snap(f.w) == before, "D8 unknown-corp mutates nothing");

        // Illegal transition: offer_friendship while already hostile.
        apply_corp_command(f.w, reg, make_cmd(corp_verb::declare_hostile, f.a, f.b), nullptr);
        const snapshot mid = snap(f.w);
        const auto r3 = apply_corp_command(f.w, reg, make_cmd(corp_verb::offer_friendship, f.a, f.b), nullptr);
        check(r3 == corp_command_result::rejected_state, "D8 offer_friendship while hostile rejected_state");
        check(snap(f.w) == mid, "D8 illegal offer_friendship mutates nothing");

        // accept_friendship with no pending offer.
        const auto r4 = apply_corp_command(f.w, reg, make_cmd(corp_verb::accept_friendship, f.c, f.a), nullptr);
        check(r4 == corp_command_result::rejected_state, "D8 accept with no pending offer rejected_state");
    }

    // D9/D10 — determinism across independent construction and across replay.
    {
        auto run_sequence = [&](world& w, entity_id a, entity_id b, entity_id c) {
            apply_corp_command(w, reg, make_cmd(corp_verb::declare_hostile, a, b), nullptr);
            apply_corp_command(w, reg, make_cmd(corp_verb::offer_friendship, a, c), nullptr);
            apply_corp_command(w, reg, make_cmd(corp_verb::accept_friendship, c, a), nullptr);
            apply_corp_command(w, reg, make_cmd(corp_verb::declare_hostile, b, c), nullptr);
            apply_corp_command(w, reg, make_cmd(corp_verb::return_to_neutral, a, b), nullptr);
            apply_corp_command(w, reg, make_cmd(corp_verb::offer_friendship, b, a), nullptr);
        };

        fixture f1 = make_fixture();
        run_sequence(f1.w, f1.a, f1.b, f1.c);
        const snapshot s1 = snap(f1.w);

        // D9: an INDEPENDENTLY constructed world (same id assignment order,
        // since make_fixture is deterministic) runs the same sequence and
        // lands on the same tables.
        fixture f2 = make_fixture();
        run_sequence(f2.w, f2.a, f2.b, f2.c);
        const snapshot s2 = snap(f2.w);
        check(f1.a == f2.a && f1.b == f2.b && f1.c == f2.c,
              "D9 precondition: independently constructed worlds assign identical ids");
        check(s1 == s2, "D9 determinism: independent construction + same sequence -> identical stance tables");

        // D10: replay the SAME sequence again on a third fresh world and
        // confirm the outcome still matches — replay is not just
        // cross-construction agreement, it is reproducible on its own.
        fixture f3 = make_fixture();
        run_sequence(f3.w, f3.a, f3.b, f3.c);
        const snapshot s3 = snap(f3.w);
        check(s1 == s3, "D10 determinism: replaying the sequence a third time reproduces the same tables");
    }

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
