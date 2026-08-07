-- Project Io — works.lua
-- The Era -1 works table (BL-321): what a polity can BUILD in pre-history.
-- Loaded at startup into the C++ works registry (see src/world/works_roster.hpp
-- and src/world/works_registry.cpp).
--
-- Sibling of the unit roster: unit_roster says what a polity can FIELD, this
-- says what it can BUILD. Availability is DERIVED, never unlocked — a row is
-- offered when the province's ground and population clear its gate, and the
-- bands are cumulative because nothing un-invents a granary.
--
-- Each row is { name, band, gate, effect, weight }:
--
--   band    "classical" | "medieval" | "gunpowder" | "industrial"
--
--   gate    Thresholds the province must MEET. The four endowment windows are
--           0-1000 (ore_q / farm_q / port_q / energy_q); omit an axis for no
--           gate. `population` is an absolute headcount floor.
--
--   effect  Per-mille deltas on quantities the Era -1 sim already computes.
--           Omit a field for zero.
--             capacity    demographic carrying capacity (BL-273's ceiling)
--             manpower    the manpower ceiling — what the province can raise
--             reach       discount on terrain-weighted supply cost (BL-316)
--             defence     modifier this province contributes when DEFENDING
--             industrial  pull-forward on the Stage 4 furnace date
--
--   weight  Relative pull when a polity scores which work to raise next. Not a
--           count and not a cost.
--
-- NAMES ARE GENERIC MECHANISM NOUNS — io-standing-rules § Terms & docs. "Way
-- Station" describes what the work is; it is not a people, a place, or an Earth
-- institution, and none of these may read as recognisably Latin.
--
-- The loader VALIDATES this table and throws on a malformed or incoherent row
-- (unknown band, no effect at all, duplicate name, an unreachable reach work).
-- That check runs on every startup, so a bad edit here fails loudly and early
-- rather than producing a quietly degraded world.
--
-- Magnitudes are authored by judgement and are meant to be calibrated against
-- BL-275's sweep once BL-316's terrain-weighted reach exists to discount.

works = {
    -- ======================================================================
    -- Classical — earthworks, stores, a wall. Nothing here needs metal the
    -- province cannot dig locally, which is why the band gates lightly.
    -- ======================================================================
    {
        name   = "Granary",
        band   = "classical",
        gate   = { farm_q = 300, population = 4000 },
        effect = { capacity = 180, manpower = 40 },
        weight = 220,
    },
    {
        name   = "Channel Works",
        band   = "classical",
        gate   = { farm_q = 450, population = 6000 },
        effect = { capacity = 240, reach = 40, industrial = 30 },
        weight = 190,
    },
    {
        name   = "Ore Pits",
        band   = "classical",
        gate   = { ore_q = 400, population = 5000 },
        effect = { manpower = 60, industrial = 120 },
        weight = 170,
    },
    {
        name   = "Wall Circuit",
        band   = "classical",
        gate   = { population = 8000 },
        effect = { capacity = 40, defence = 250 },
        weight = 200,
    },
    -- The first reach work, and the cheapest. A polity that cannot yet afford
    -- stone still wants the road stage between its provinces.
    {
        name   = "Way Station",
        band   = "classical",
        gate   = { population = 3000 },
        effect = { reach = 120 },
        weight = 240,
    },
    {
        name   = "Harbour Mole",
        band   = "classical",
        gate   = { port_q = 400, population = 5000 },
        effect = { capacity = 120, reach = 200, industrial = 40 },
        weight = 150,
    },

    -- ======================================================================
    -- Medieval — the mill, the fortress, the guild.
    -- ======================================================================
    {
        name   = "Water Mill",
        band   = "medieval",
        gate   = { farm_q = 400, population = 12000 },
        effect = { capacity = 220, industrial = 140 },
        weight = 180,
    },
    {
        name   = "Stone Fortress",
        band   = "medieval",
        gate   = { ore_q = 350, population = 15000 },
        effect = { manpower = 80, defence = 480 },
        weight = 210,
    },
    {
        name   = "Guild Quarter",
        band   = "medieval",
        gate   = { population = 20000 },
        effect = { capacity = 160, reach = 60, industrial = 220 },
        weight = 170,
    },
    {
        name   = "Deepwater Wharf",
        band   = "medieval",
        gate   = { port_q = 550, population = 14000 },
        effect = { capacity = 140, reach = 300, industrial = 120 },
        weight = 140,
    },
    {
        name   = "Span Bridge",
        band   = "medieval",
        gate   = { ore_q = 300, population = 10000 },
        effect = { reach = 260, defence = 60 },
        weight = 190,
    },

    -- ======================================================================
    -- Gunpowder — energy stands in for saltpetre/sulphur here for the same
    -- reason it does in the unit roster: there is no signal of its own. The
    -- substitution is NAMED, not faked.
    -- ======================================================================
    {
        name   = "Bastion Fort",
        band   = "gunpowder",
        gate   = { ore_q = 500, energy_q = 350, population = 25000 },
        effect = { manpower = 100, defence = 640 },
        weight = 200,
    },
    {
        name   = "Powder Mill",
        band   = "gunpowder",
        gate   = { energy_q = 450, population = 22000 },
        effect = { manpower = 180, defence = 80, industrial = 160 },
        weight = 160,
    },
    {
        name   = "Counting House",
        band   = "gunpowder",
        gate   = { population = 30000 },
        effect = { capacity = 200, reach = 100, industrial = 300 },
        weight = 180,
    },
    {
        name   = "Cut Canal",
        band   = "gunpowder",
        gate   = { farm_q = 300, population = 28000 },
        effect = { capacity = 120, reach = 420, industrial = 180 },
        weight = 170,
    },
    {
        name   = "Naval Yard",
        band   = "gunpowder",
        gate   = { port_q = 600, energy_q = 300, population = 26000 },
        effect = { capacity = 100, manpower = 60, reach = 340, industrial = 200 },
        weight = 130,
    },

    -- ======================================================================
    -- Industrial — the ANCESTORS of components.hpp's building_type. A province
    -- that lit Blast Works is where the 1960 smelter sits and why its
    -- substrate_density is high.
    -- ======================================================================
    {
        name   = "Blast Works",
        band   = "industrial",
        gate   = { ore_q = 600, energy_q = 500, population = 40000 },
        effect = { capacity = 180, manpower = 140, industrial = 520 },
        weight = 240,
    },
    {
        name   = "Rail Head",
        band   = "industrial",
        gate   = { ore_q = 550, energy_q = 550, population = 45000 },
        effect = { capacity = 220, manpower = 80, reach = 700, industrial = 380 },
        weight = 260,
    },
    {
        name   = "Signal Line",
        band   = "industrial",
        gate   = { ore_q = 450, energy_q = 400, population = 35000 },
        effect = { reach = 380, defence = 80, industrial = 220 },
        weight = 190,
    },
    {
        name   = "Arsenal",
        band   = "industrial",
        gate   = { ore_q = 650, energy_q = 600, population = 50000 },
        effect = { manpower = 320, defence = 260, industrial = 180 },
        weight = 180,
    },
    {
        name   = "Coaling Station",
        band   = "industrial",
        gate   = { port_q = 650, energy_q = 550, population = 38000 },
        effect = { capacity = 80, reach = 480, industrial = 200 },
        weight = 140,
    },
}
