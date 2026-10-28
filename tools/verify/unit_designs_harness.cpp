// ---------------------------------------------------------------------------
// Unit designs (BL-1434; MILITARY.md § The kit). No SDL/Lua.
// ---------------------------------------------------------------------------
// One live design per land class per nation: a kit item per class slot. The base
// kit opens every slot; later kit is locked until researched (its nodes are
// BL-1436's, not yet in the tree, so this harness waives the lock to exercise an
// open slot — `set_kit_research_waived_for_harness`). A design's stats are its
// class's plus its kit's; a hire is raised to the live design and a refit brings an
// existing unit to it.
//
//   U1  THE CATALOGUE is well-formed: every land class has its slots, every slot a
//       base item first and two to four later items; every later item names a
//       research node that is not in the tree, so it reads locked.
//   U2  DEFAULTS ARE BASE KIT: every nation's design of every class is base, its
//       stats the class's own, and nothing is stored.
//   U3  SET AND REFUSALS: a locked item, a rival nation, an unknown class / slot /
//       item, an item of another slot or class and a no-change are each refused
//       with the world byte- and hash-identical; a researched item applies, moves
//       the stats by its delta, and setting it back to base stores nothing again.
//   U4  A HIRE RECORDS ITS DESIGN: it buys the kit's goods, the muster carries the
//       design, the raised unit records it and fights stronger; a hire whose kit
//       the shelves cannot supply is refused whole.
//   U5  A REFIT TO THE LIVE DESIGN: a unit whose design is stale may refit to its own
//       row, buying only the changed slots' kit and no credits; it takes the design
//       when the refit completes; a unit already on the live design may not.
//   U6  SAVE: designs, a unit's kit, a refit's kit and a muster's kit round-trip by
//       value; a v63 stream reads as base kit everywhere; a bad variant is refused.
//   U7  DETERMINISM: twin runs hash and serialise alike; a design moves the hash.
//
// Run: unit_designs_harness
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/digitisation_tree_data.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_designs.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace {

int failures = 0, checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) ++failures;
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

std::string to_bytes(const world& w)
{
    std::ostringstream o(std::ios::binary);
    write_world_snapshot(w, o);
    return o.str();
}

bool from_bytes(const std::string& b, world& w)
{
    std::istringstream in(b, std::ios::binary);
    return read_world_snapshot(w, in);
}

int row_named(const char* name)
{
    const auto& t = unit_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
        if (std::string(t[i].name) == name) return static_cast<int>(i);
    return -1;
}

uint16_t item(const char* id)
{
    return kit_item_by_id(id);
}

constexpr resource_type k_shelf_goods[] = {
    resource_type::steel, resource_type::coal, resource_type::machinery,
    resource_type::alloys, resource_type::electronics, resource_type::ordnance,
};

struct fixture
{
    world     w;
    entity_id body   = null_entity;
    entity_id home   = null_entity;
    entity_id abroad = null_entity;
    entity_id nation = null_entity; ///< the seat
    entity_id other  = null_entity; ///< a rival
    entity_id base   = null_entity;
    entity_id market = null_entity;
};

/// refit_units' fixture: one body, two land tiles, the seat's with a completed
/// military base and a market holding @p shelf of each kit good at 10 cr.
void build(fixture& f, float shelf = 100.0f)
{
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Home"; bc.type = body_type::planet; bc.grid_width = 2; bc.grid_height = 1;
    w.bodies[f.body] = bc;
    for (int x = 0; x < 2; ++x)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body = f.body; tc.grid_x = x; tc.grid_y = 0;
        tc.substrate = terrain_substrate::rocky; tc.landform = terrain_landform::plains;
        w.tiles[t] = tc;
        (x == 0 ? f.home : f.abroad) = t;
    }
    f.nation = w.create_entity();
    nation_component nc{};
    nc.name = "Hosts"; nc.treasury = 100000.0f;
    f.base = w.create_entity();
    building_component b{};
    b.tile = f.home; b.type = building_type::military_base; b.ticks_remaining = 0;
    w.buildings[f.base] = b;
    nc.state_assets.push_back(f.base);
    w.nations[f.nation] = nc;
    f.other = w.create_entity();
    nation_component oc{};
    oc.name = "Others"; oc.treasury = 100000.0f;
    w.nations[f.other] = oc;
    w.player_entity = f.nation;
    w.set_tile_nation(f.home, f.nation);
    w.set_tile_nation(f.abroad, f.other);

    f.market = w.create_entity();
    market_component mc{};
    mc.body = f.body; mc.centre_tile = f.home;
    for (const resource_type r : k_shelf_goods)
    {
        const std::size_t i = static_cast<std::size_t>(r);
        mc.base_price[i] = 10.0f; mc.price[i] = 10.0f; mc.inventory[i] = shelf;
    }
    w.markets[f.market] = mc;
}

float shelf(const fixture& f, resource_type r)
{
    return f.w.markets.at(f.market).inventory[static_cast<std::size_t>(r)];
}

entity_id add_unit(fixture& f, int row, entity_id owner, entity_id tile)
{
    const entity_id u = f.w.create_entity();
    unit_component uc{};
    uc.position = tile; uc.owner = owner; uc.count = 50; uc.type = static_cast<uint16_t>(row);
    uc.muster_base = f.base; uc.establishment = 50;
    f.w.units[u] = uc;
    return u;
}

corp_command_result design(fixture& f, const recipe_registry& reg, entity_id actor, uint32_t cls,
                           uint16_t slot, uint16_t it)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = actor; cmd.verb = corp_verb::set_unit_design;
    cmd.order = cls; cmd.recipe = slot; cmd.unit_type = it;
    return apply_corp_command(f.w, reg, cmd);
}

corp_command_result hire(fixture& f, const recipe_registry& reg, int row)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = f.nation; cmd.verb = corp_verb::hire_unit;
    cmd.tile = f.home; cmd.unit_type = static_cast<uint16_t>(row);
    return apply_corp_command(f.w, reg, cmd);
}

corp_command_result refit(fixture& f, const recipe_registry& reg, entity_id unit, int row)
{
    corp_command cmd;
    cmd.tick = 1; cmd.corp = f.nation; cmd.verb = corp_verb::refit_unit;
    cmd.subject = unit; cmd.unit_type = static_cast<uint16_t>(row);
    return apply_corp_command(f.w, reg, cmd);
}

/// Refused with @p want, and the world byte- and hash-identical to before.
void design_refused(fixture& f, const recipe_registry& reg, entity_id actor, uint32_t cls,
                    uint16_t slot, uint16_t it, corp_command_result want, const std::string& what)
{
    const std::string before = to_bytes(f.w);
    const uint64_t    h      = f.w.state_hash(1);
    const auto r = design(f, reg, actor, cls, slot, it);
    check(r == want && to_bytes(f.w) == before && f.w.state_hash(1) == h, what);
}

const uint32_t INF = static_cast<uint32_t>(land_class::infantry);

} // namespace

int main()
{
    std::printf("=== unit designs (BL-1434) ===\n");
    const recipe_registry reg{};
    const std::vector<kit_item>& cat = kit_catalogue();
    const int rifle = row_named("Rifle Infantry");
    const int levy  = row_named("Levy Spear");
    check(rifle >= 0 && levy >= 0, "U0 the fixture's rows exist");

    std::printf("\nU1 - the catalogue\n");
    {
        const char* err = kit_catalogue_first_error();
        check(err == nullptr, std::string("U1a the catalogue validates") + (err ? std::string(": ") + err : ""));
        const int want_slots[land_class_count] = {2, 3, 3, 3, 3, 3, 3, 0};
        bool slots_ok = true, shape_ok = true, hard_ok = true;
        int later = 0, locked = 0, in_tree = 0;
        for (int c = 0; c < land_class_count; ++c)
        {
            const land_class lc = static_cast<land_class>(c);
            slots_ok = slots_ok && kit_slot_count(lc) == want_slots[c];
            for (int s = 0; s < kit_slot_count(lc); ++s)
            {
                const std::vector<uint16_t>& v = kit_items_for(lc, s);
                shape_ok = shape_ok && v.size() >= 3 && v.size() <= 5
                        && cat[v[0]].research_node == nullptr && kit_slot_name(lc, s)[0] != '\0';
                static const char* const names[land_class_count] = {
                    "infantry", "motorised", "mechanised", "armour", "light armour",
                    "artillery", "air defence", "none"};
                std::printf("     %-13s %-12s %zu items:", names[c], kit_slot_name(lc, s), v.size());
                for (const uint16_t id : v) std::printf(" %s", cat[id].id);
                std::printf("\n");
            }
            hard_ok = hard_ok && kit_stats(lc, unit_design{}).hardness == land_class_stats(lc).hardness;
        }
        fixture f;
        build(f);
        for (std::size_t i = 0; i < cat.size(); ++i)
        {
            if (cat[i].research_node == nullptr)
                continue;
            ++later;
            if (!kit_item_researched(f.w, f.nation, static_cast<uint16_t>(i))) ++locked;
            if (io::dg::find_node(cat[i].research_node) >= 0) ++in_tree;
        }
        check(slots_ok, "U1b slots per class: infantry 2, the six others 3, none 0");
        check(shape_ok, "U1c every slot: a named slot, base item first, 2..4 later items");
        check(hard_ok, "U1d base kit leaves every class's stats as authored");
        check(later > 0 && locked == later && in_tree == 0,
              "U1e every one of the " + std::to_string(later) + " later items is locked behind a node not yet in the tree");
        check(cat.size() < k_no_kit_item && kit_item_by_id("inf.weapon.assault_rifle") != k_no_kit_item
                  && kit_item_by_id("no.such.item") == k_no_kit_item,
              "U1f items resolve by id; an unknown id does not (" + std::to_string(cat.size()) + " items)");
    }

    std::printf("\nU2 - defaults are base kit\n");
    {
        fixture f;
        build(f);
        bool base = true;
        for (const entity_id n : { f.nation, f.other })
            for (int c = 0; c < land_class_count; ++c)
            {
                const land_class lc = static_cast<land_class>(c);
                const land_combat_stats a = design_stats(f.w, n, lc), b = land_class_stats(lc);
                base = base && design_of(f.w, n, lc).is_base() && a.soft == b.soft && a.hard == b.hard
                    && a.hardness == b.hardness && a.defence == b.defence && a.breakthrough == b.breakthrough;
                for (int s = 0; s < kit_slot_count(lc); ++s)
                    base = base && design_fitted(f.w, n, lc, s) == kit_items_for(lc, s)[0];
            }
        check(base, "U2a every nation's design of every class is base kit, with the class's own stats");
        check(f.w.nations.at(f.nation).unit_design_kit.empty() && f.w.nations.at(f.other).unit_design_kit.empty(),
              "U2b nothing is stored");
    }

    std::printf("\nU3 - set and refusals\n");
    {
        fixture f;
        build(f);
        const uint64_t h0 = f.w.state_hash(1);
        const uint16_t ar = item("inf.weapon.assault_rifle");
        set_kit_research_waived_for_harness(false);
        design_refused(f, reg, f.nation, INF, 0, ar, corp_command_result::rejected_tech_locked,
                       "U3a a locked item (the assault rifle) is refused rejected_tech_locked");
        set_kit_research_waived_for_harness(true);
        design_refused(f, reg, f.other, INF, 0, ar, corp_command_result::rejected_not_owner,
                       "U3b a rival nation is refused rejected_not_owner (rivals keep base kit)");
        design_refused(f, reg, f.nation, static_cast<uint32_t>(land_class::none), 0, ar,
                       corp_command_result::rejected_invalid, "U3c class none (no slots) is refused");
        design_refused(f, reg, f.nation, 99, 0, ar, corp_command_result::rejected_invalid,
                       "U3d an unknown class is refused");
        design_refused(f, reg, f.nation, INF, 2, ar, corp_command_result::rejected_invalid,
                       "U3e a slot infantry lacks (2) is refused");
        design_refused(f, reg, f.nation, INF, no_recipe, ar, corp_command_result::rejected_invalid,
                       "U3f an omitted slot (no_recipe) is refused");
        design_refused(f, reg, f.nation, INF, 0, static_cast<uint16_t>(cat.size()),
                       corp_command_result::rejected_invalid, "U3g an unknown item is refused");
        design_refused(f, reg, f.nation, INF, 1, ar, corp_command_result::rejected_invalid,
                       "U3h a weapon in the armour slot does not fit and is refused");
        design_refused(f, reg, f.nation, INF, 0, item("mot.weapon.assault_rifle"),
                       corp_command_result::rejected_invalid, "U3i another class's item is refused");
        design_refused(f, reg, f.nation, INF, 0, item("inf.weapon.battle_rifle"),
                       corp_command_result::rejected_state, "U3j the base item on a base slot is no change, refused");

        const land_combat_stats before = design_stats(f.w, f.nation, land_class::infantry);
        check(design(f, reg, f.nation, INF, 0, ar) == corp_command_result::applied,
              "U3k a researched item applies");
        const land_combat_stats after = design_stats(f.w, f.nation, land_class::infantry);
        check(design_fitted(f.w, f.nation, land_class::infantry, 0) == ar
                  && after.soft == before.soft + cat[ar].delta.soft && after.hard == before.hard
                  && f.w.nations.at(f.nation).unit_design_kit.size() == 1
                  && design_of(f.w, f.other, land_class::infantry).is_base(),
              "U3l the slot holds it, soft attack " + std::to_string(before.soft) + " -> "
                  + std::to_string(after.soft) + "; the rival is untouched");
        check(f.w.state_hash(1) != h0, "U3m a design moves the hash");
        design_refused(f, reg, f.nation, INF, 0, ar, corp_command_result::rejected_state,
                       "U3n setting it again is no change, refused");
        const uint16_t atgm = item("inf.weapon.fire_and_forget_atgm");
        check(design(f, reg, f.nation, INF, 0, atgm) == corp_command_result::applied
                  && f.w.nations.at(f.nation).unit_design_kit.size() == 1
                  && design_fitted(f.w, f.nation, land_class::infantry, 0) == atgm,
              "U3o a second item in the slot replaces the first (one live design per class)");
        check(design(f, reg, f.nation, INF, 0, item("inf.weapon.battle_rifle")) == corp_command_result::applied
                  && f.w.nations.at(f.nation).unit_design_kit.empty() && f.w.state_hash(1) == h0,
              "U3p back to base stores nothing, and the hash is the default world's again");
    }

    std::printf("\nU4 - a hire records its design\n");
    {
        set_kit_research_waived_for_harness(true);
        fixture f;
        build(f);
        const uint16_t ar = item("inf.weapon.assault_rifle"), vest = item("inf.armour.flak_vest");
        design(f, reg, f.nation, INF, 0, ar);
        design(f, reg, f.nation, INF, 1, vest);
        const unit_design d = design_of(f.w, f.nation, land_class::infantry);
        const float tre = f.w.nations.at(f.nation).treasury;
        const auto r = hire(f, reg, rifle);
        check(r == corp_command_result::applied && f.w.musters.size() == 1 && f.w.musters[0].kit == d,
              "U4a the hire applies and its muster carries the live design");
        // assault rifle: 2 steel + 1 machinery; flak vest: 1 steel + 1 alloys.
        check(near(shelf(f, resource_type::steel), 97.0f) && near(shelf(f, resource_type::machinery), 99.0f)
                  && near(shelf(f, resource_type::alloys), 99.0f) && near(shelf(f, resource_type::electronics), 100.0f),
              "U4b it buys the kit's goods: 3 steel, 1 machinery, 1 alloys");
        const float hire_cost = reg.military().hire_base_cost
            + reg.military().hire_cost_per_power * static_cast<float>(unit_roster_table()[rifle].power_mod);
        check(near(tre - f.w.nations.at(f.nation).treasury, hire_cost + 5.0f * 10.0f),
              "U4c and pays the hire price plus the goods (" + std::to_string(tre - f.w.nations.at(f.nation).treasury) + " cr)");
        f.w.musters[0].days_remaining = 0;
        advance_musters(f.w);
        entity_id raised = null_entity;
        for (const auto& [uid, u] : f.w.units) raised = uid;
        check(raised != null_entity && f.w.units.at(raised).kit == d && !unit_design_stale(f.w, f.w.units.at(raised)),
              "U4d the raised unit records the design");
        const entity_id plain = add_unit(f, rifle, f.nation, f.home); // base kit, same row
        const land_combat_stats a = unit_land_stats(f.w.units.at(raised)), b = unit_land_stats(f.w.units.at(plain));
        check(a.soft > b.soft && a.defence > b.defence && a.hard == b.hard,
              "U4e it fights stronger than a base-kit Rifle Infantry: soft " + std::to_string(b.soft) + " -> "
                  + std::to_string(a.soft) + ", defence " + std::to_string(b.defence) + " -> " + std::to_string(a.defence));

        fixture g;
        build(g, /*shelf=*/1.0f); // 3 steel needed
        design(g, reg, g.nation, INF, 0, ar);
        design(g, reg, g.nation, INF, 1, vest);
        const std::string pre = to_bytes(g.w);
        check(hire(g, reg, rifle) == corp_command_result::rejected_funds && to_bytes(g.w) == pre,
              "U4f a hire whose kit the shelves cannot supply is refused rejected_funds, nothing changed");
        fixture k;
        build(k, /*shelf=*/1.0f);
        check(hire(k, reg, rifle) == corp_command_result::applied && k.w.musters[0].kit.is_base(),
              "U4g the same shelves raise a base-kit hire (base kit buys no goods)");
    }

    std::printf("\nU5 - a refit to the live design\n");
    {
        set_kit_research_waived_for_harness(true);
        fixture f;
        build(f);
        const entity_id u = add_unit(f, rifle, f.nation, f.home);
        {
            const std::string pre = to_bytes(f.w);
            check(refit(f, reg, u, rifle) == corp_command_result::rejected_invalid && to_bytes(f.w) == pre,
                  "U5a a unit already on the live design may not refit to its own row");
        }
        design(f, reg, f.nation, INF, 1, item("inf.armour.ceramic_plates"));
        const std::vector<uint16_t> targets =
            refit_targets(f.w, f.nation, f.w.units.at(u), campaign_roster_band_for(reg.era()));
        check(unit_design_stale(f.w, f.w.units.at(u)) && !targets.empty() && targets[0] == rifle,
              "U5b once the design changes the unit is stale, and its own row heads its refit targets");
        check(refit_check(f.w, reg, f.nation, u, static_cast<uint16_t>(rifle)) == corp_command_result::applied,
              "U5c refit_check admits the design refit");
        const float tre = f.w.nations.at(f.nation).treasury;
        const auto r = refit(f, reg, u, rifle);
        const unit_component& uc = f.w.units.at(u);
        check(r == corp_command_result::applied && uc.refit_type == rifle && uc.type == rifle
                  && uc.refit_kit == design_of(f.w, f.nation, land_class::infantry) && uc.kit.is_base()
                  && uc.refit_days == muster_days_for(static_cast<uint16_t>(rifle)),
              "U5d it applies: the unit keeps its row and kit, the live design recorded as its refit");
        check(near(shelf(f, resource_type::alloys), 98.0f) && near(shelf(f, resource_type::steel), 100.0f)
                  && near(tre - f.w.nations.at(f.nation).treasury, 2.0f * 10.0f),
              "U5e it buys only the changed slot's kit (2 alloys, 20 cr) and no credits");
        const uint64_t mid = f.w.state_hash(2);
        {
            world back;
            const std::string b = to_bytes(f.w);
            check(from_bytes(b, back) && back.units.at(u).refit_kit == uc.refit_kit && back.state_hash(2) == mid
                      && to_bytes(back) == b,
                  "U5f a design refit in flight round-trips (its row unchanged, its design recorded)");
        }
        advance_refits(f.w);
        const unit_component& done = f.w.units.at(u);
        check(!unit_refitting(done) && done.kit == design_of(f.w, f.nation, land_class::infantry)
                  && done.refit_kit.is_base() && !unit_design_stale(f.w, done),
              "U5g when it completes the unit holds the live design");

        // A refit to a newer row also brings the unit to the live design.
        const entity_id old = add_unit(f, levy, f.nation, f.home);
        check(refit(f, reg, old, rifle) == corp_command_result::applied
                  && f.w.units.at(old).refit_kit == design_of(f.w, f.nation, land_class::infantry),
              "U5h a refit to a newer row records the live design too");
    }

    std::printf("\nU6 - save\n");
    {
        set_kit_research_waived_for_harness(true);
        fixture f;
        build(f);
        design(f, reg, f.nation, INF, 0, item("inf.weapon.light_mortar"));
        design(f, reg, f.nation, static_cast<uint32_t>(land_class::artillery), 2, item("art.tow.self_propelled"));
        hire(f, reg, rifle); // a muster carrying the design
        const entity_id a = add_unit(f, rifle, f.nation, f.home);
        f.w.units.at(a).kit.fit = {2, 1, 0};
        const entity_id b = add_unit(f, rifle, f.nation, f.home); // stale, mid-refit
        refit(f, reg, b, rifle);
        const std::string bytes = to_bytes(f.w);
        world back;
        const bool ok = from_bytes(bytes, back);
        check(ok && back.nations.at(f.nation).unit_design_kit == f.w.nations.at(f.nation).unit_design_kit
                  && back.units.at(a).kit == f.w.units.at(a).kit && back.units.at(b).refit_kit == f.w.units.at(b).refit_kit
                  && back.musters.size() == 1 && back.musters[0].kit == f.w.musters[0].kit && !back.musters[0].kit.is_base()
                  && back.state_hash(3) == f.w.state_hash(3) && to_bytes(back) == bytes,
              "U6a designs, a unit's kit, a refit's kit and a muster's kit round-trip by value (hash and bytes equal)");

        std::string v63;
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_designs(f.w, o);
            v63 = o.str();
        }
        uint32_t stamped = 0;
        std::memcpy(&stamped, &v63[4], sizeof stamped);
        world old;
        // A v63 stream cannot carry a design refit that keeps its row: drop b's.
        world plain = f.w;
        plain.units.at(b).refit_type = k_no_refit; plain.units.at(b).refit_days = 0;
        plain.units.at(b).refit_kit = unit_design{};
        {
            std::ostringstream o(std::ios::binary);
            write_world_snapshot_pre_designs(plain, o);
            v63 = o.str();
        }
        bool ok63 = stamped == k_world_save_version_pre_designs && from_bytes(v63, old);
        if (ok63)
        {
            ok63 = old.nations.at(f.nation).unit_design_kit.empty() && old.musters.size() == 1
                && old.musters[0].kit.is_base();
            for (const auto& [uid, u] : old.units) ok63 = ok63 && u.kit.is_base() && u.refit_kit.is_base();
        }
        check(ok63, "U6b a v63 stream reads as base kit everywhere");
        // The tail: 3 counts + nation (4+4+2n) + units (4+6 each) + musters (4+3 each).
        const std::size_t n = f.w.nations.at(f.nation).unit_design_kit.size();
        world plain_now = plain;
        const std::string pb = to_bytes(plain_now);
        std::size_t units_off = 0;
        for (const auto& [uid, u] : plain.units) units_off += (!u.kit.is_base() || !u.refit_kit.is_base()) ? 1 : 0;
        check(pb.size() == v63.size() + 12 + (8 + 2 * n) + 10 * units_off + 7 + 4, // + BL-1445 (v65): the empty march-grant count
              "U6c v63 is today's stream less the designs section");

        std::string bad = bytes;
        bad[bad.size() - 1] = static_cast<char>(9); // the muster's last variant: no slot has 10 items
        bad[bad.size() - 3] = static_cast<char>(9);
        world sentinel;
        check(!from_bytes(bad, sentinel) && sentinel.units.empty(),
              "U6d a design variant past its slot's list is REFUSED, destination untouched");
    }

    std::printf("\nU7 - determinism\n");
    {
        set_kit_research_waived_for_harness(true);
        auto run = [&](fixture& f) {
            build(f);
            design(f, reg, f.nation, INF, 0, item("inf.weapon.assault_rifle"));
            hire(f, reg, rifle);
            const entity_id u = add_unit(f, rifle, f.nation, f.home);
            design(f, reg, f.nation, INF, 1, item("inf.armour.flak_vest"));
            refit(f, reg, u, rifle);
            for (muster_order& m : f.w.musters) m.days_remaining = 0;
            advance_musters(f.w);
            advance_refits(f.w);
        };
        fixture a, b;
        run(a);
        run(b);
        check(a.w.state_hash(5) == b.w.state_hash(5) && to_bytes(a.w) == to_bytes(b.w),
              "U7a twin runs hash and serialise alike");
        fixture c;
        build(c);
        add_unit(c, rifle, c.nation, c.home);
        const uint64_t idle = c.w.state_hash(0);
        advance_refits(c.w);
        advance_musters(c.w);
        check(c.w.state_hash(0) == idle, "U7b a base-kit world is untouched by the passes");
    }
    set_kit_research_waived_for_harness(false);

    std::printf("\n%d/%d checks passed\n", checks - failures, checks);
    if (failures == 0) std::printf("PASS\n");
    return failures == 0 ? 0 : 1;
}
