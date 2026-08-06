-- Mock tech / quest tree (BL-087). DATA ONLY — the tech system is post-prototype
-- (BL-087 resolution 6); nothing in the simulation reads this. It exists so the
-- F9 read-only viewer can show the tree while the design is fresh.
--
-- Schema (mirrors docs/research/ERA1_TECH_LANDSCAPE.md / docs/research/TECH_EFFECTS.md):
--   quest: id, name, era, type ("gate"|"standing"), thread, thesis, capstone,
--          opens (quest ids the capstone unlocks — capstones open QUEST TREES,
--          not Eras, per the 2026-07-08 Era reframe), status
--   tech:  id, name, quest, kind ("invention"|"tier"|"capstone"), tier,
--          prereqs (tech ids; MAY cross quests, sparingly), cost (S/M/L/XL,
--          default M), payoff (gate|resource|building|recipe|efficiency|enabling),
--          condition (research|structure|stockpile|market|surplus|era|deed —
--          "deed" added 2026-08-05: a tangible in-game action, not a state
--          predicate; see ERA1_TECH_LANDSCAPE.md § The deed primitive),
--          unlocks, status
--
-- Era 0 section: shape rules from the 2026-07-08 resolutions (mostly a binary
-- tree with dead-end leaves; cross-quest prereqs sparing and deliberate;
-- economic conditions reserved for capstones + marquee nodes). Unchanged.
--
-- Era 1 section: REWRITTEN 2026-08-06 to transcribe "The Era 1 tree — first
-- draft" (ERA1_TECH_LANDSCAPE.md, 2026-08-05) — five sectors (Launch,
-- Volatiles, Mobility, Yards, Extraction) x three rings (R1 Reach, R2
-- Foothold, R3 Industry), four keystone forks each opened by a DEED rather
-- than a research/structure/market predicate, with permanent branch exclusion.
-- STATUS: "sketch" everywhere in this section — Ben has explicitly not yet
-- reviewed the node list ("the shape is the argument; the node list is a
-- starting point, not a proposal" — ERA1_TECH_LANDSCAPE.md). This viewer
-- rendering IS that review's first concrete look, not a ratification.

tech_tree = {
    quests = {
        -- ---------------------------------------------------------------- Era 0
        { id = "E0-HEAVY", name = "Heavy Industry", era = 0, type = "gate",
          thread = "Industry",
          thesis = "A steel / chemicals / refining base broad enough to build rockets from.",
          capstone = "E0-HI-04", opens = { "E0-ROCKET" }, status = "stub" },

        { id = "E0-ELEC", name = "Electronics & Computing", era = 0, type = "gate",
          thread = "Compute",
          thesis = "Info-Age footing; compute is the input the R&D Lab consumes.",
          capstone = "E0-EC-04", opens = {}, status = "stub" },

        { id = "E0-ROCKET", name = "Rocketry", era = 0, type = "gate",
          thread = "Launch",
          thesis = "An operable launch complex — the condition set ERAS.md records, re-read as a quest-tree gate.",
          capstone = "E0-RK-05",
          opens = { "E1-LAUNCH", "E1-VOLATILES", "E1-MOBILITY", "E1-YARDS", "E1-EXTRACTION" },
          status = "stub" },

        -- ---------------------------------------------------------------- Era 1
        -- Five sectors (ERA1_TECH_LANDSCAPE.md § Shape). Each sector's tree
        -- carries R1/R2/R3 rings; four of the five carry a keystone fork
        -- (Mobility does not). "capstone" here names the sector's KEYSTONE
        -- tech (the fork), not a terminal gate — no Era-2 hook exists yet
        -- (§ What this draft deliberately does not do).
        { id = "E1-LAUNCH", name = "Launch", era = 1, type = "gate",
          thread = "Launch",
          thesis = "Cadence, reuse, lift cost per kg — can you get there at all?",
          capstone = "E1-LA-KEYSTONE", opens = {}, status = "sketch" },

        { id = "E1-VOLATILES", name = "Volatiles", era = 1, type = "gate",
          thread = "ISRU",
          thesis = "Water, electrolysis, the propellant loop — can you make something locally?",
          capstone = "E1-VO-KEYSTONE", opens = {}, status = "sketch" },

        { id = "E1-MOBILITY", name = "Mobility", era = 1, type = "gate",
          thread = "Propulsion",
          thesis = "Transfer cost, the propellant tax, engine classes. No keystone fork — a plain depth line.",
          capstone = "", opens = {}, status = "sketch" },

        { id = "E1-YARDS", name = "Yards", era = 1, type = "gate",
          thread = "Orbital Construction",
          thesis = "Orbital construction, assembly, the port as trade participant.",
          capstone = "E1-YA-KEYSTONE", opens = {}, status = "sketch" },

        { id = "E1-EXTRACTION", name = "Extraction", era = 1, type = "gate",
          thread = "Off-world deposits",
          thesis = "Regolith, iron-nickel, PGM — does it pay for itself, can it supply someone else?",
          capstone = "E1-EX-KEYSTONE", opens = {}, status = "sketch" },

        -- --------------------------------------------------------- Standing lines
        { id = "L-LOG", name = "Logistics", era = 0, type = "standing",
          thread = "Propulsion",
          thesis = "The convoy propellant tax, deepened forever: chemical, ion, nuclear-thermal (parked).",
          capstone = "", opens = {}, status = "stub" },

        { id = "L-AUTO", name = "Economy & Automation", era = 0, type = "standing",
          thread = "Power & Automation",
          thesis = "The workforce-scalar and power substrate behind every quest. Crosses Extraction at R3 to carry the Autonomy Doctrine's second branch.",
          capstone = "", opens = {}, status = "stub" },

        { id = "L-MIL", name = "Military", era = 0, type = "standing",
          thread = "",
          thesis = "Reserved — not enumerated until the combat system is mapped.",
          capstone = "", opens = {}, status = "reserved" },
    },

    techs = {
        -- ------------------------------------------------- E0-HEAVY (stub, 4)
        { id = "E0-HI-01", name = "Blast-Furnace Steel", quest = "E0-HEAVY",
          kind = "invention", prereqs = {}, cost = "M", payoff = "building",
          condition = "research", unlocks = "Steel Mill", status = "stub" },
        { id = "E0-HI-02", name = "Industrial Chemistry", quest = "E0-HEAVY",
          kind = "invention", prereqs = { "E0-HI-01" }, cost = "M", payoff = "recipe",
          condition = "research", unlocks = "Petroleum -> refined fuel chains", status = "stub" },
        { id = "E0-HI-03", name = "Blast-Furnace Steel II", quest = "E0-HEAVY",
          kind = "tier", tier = 2, prereqs = { "E0-HI-01" }, cost = "S", payoff = "efficiency",
          condition = "research", unlocks = "Steel yield up", status = "stub" },
        { id = "E0-HI-04", name = "Integrated Supply Chains", quest = "E0-HEAVY",
          kind = "capstone", prereqs = { "E0-HI-02" }, cost = "XL", payoff = "gate",
          condition = "surplus", unlocks = "Quest goal: sustained steel + fuel surplus feeds Rocketry", status = "stub" },

        -- -------------------------------------------------- E0-ELEC (stub, 4)
        { id = "E0-EC-01", name = "Semiconductor Fabrication", quest = "E0-ELEC",
          kind = "invention", prereqs = {}, cost = "M", payoff = "building",
          condition = "research", unlocks = "Semiconductor Fab", status = "stub" },
        { id = "E0-EC-02", name = "Compute Clusters", quest = "E0-ELEC",
          kind = "invention", prereqs = { "E0-EC-01" }, cost = "M", payoff = "recipe",
          condition = "research", unlocks = "Compute — the R&D Lab input good", status = "stub" },
        { id = "E0-EC-03", name = "Semiconductor Fabrication II", quest = "E0-ELEC",
          kind = "tier", tier = 2, prereqs = { "E0-EC-01" }, cost = "S", payoff = "efficiency",
          condition = "research", unlocks = "Fab yield up", status = "stub" },
        { id = "E0-EC-04", name = "Information Economy", quest = "E0-ELEC",
          kind = "capstone", prereqs = { "E0-EC-02" }, cost = "XL", payoff = "gate",
          condition = "market", unlocks = "Quest goal: compute cheap enough to industrialise research", status = "stub" },

        -- ------------------------------------------------ E0-ROCKET (stub, 5)
        { id = "E0-RK-01", name = "Guided Rocketry", quest = "E0-ROCKET",
          kind = "invention", prereqs = {}, cost = "M", payoff = "enabling",
          condition = "research", unlocks = "Rocket airframes and guidance", status = "stub" },
        { id = "E0-RK-02", name = "Orbital-Class Boosters", quest = "E0-ROCKET",
          kind = "invention", prereqs = { "E0-RK-01" }, cost = "L", payoff = "building",
          condition = "research", unlocks = "Launchpad", status = "stub" },
        { id = "E0-RK-03", name = "Cryogenic Ground Handling", quest = "E0-ROCKET",
          kind = "invention", prereqs = { "E0-RK-01" }, cost = "M", payoff = "recipe",
          condition = "research", unlocks = "Propellant production and storage on the pad", status = "stub" },
        { id = "E0-RK-04", name = "Orbital-Class Boosters II", quest = "E0-ROCKET",
          kind = "tier", tier = 2, prereqs = { "E0-RK-02" }, cost = "S", payoff = "efficiency",
          condition = "research", unlocks = "Payload per launch up", status = "stub" },
        { id = "E0-RK-05", name = "Operable Launch Complex", quest = "E0-ROCKET",
          kind = "capstone", prereqs = { "E0-RK-02", "E0-RK-03" }, cost = "XL", payoff = "gate",
          condition = "structure", unlocks = "Quest goal: staffed Launchpad + propellant stockpile; opens the five Era 1 sectors", status = "stub" },

        -- =====================================================================
        -- ERA 1 — five sectors, three rings, four deed-gated keystones.
        -- All "sketch" — first draft, unreviewed node list.
        -- =====================================================================

        -- ---------------------------------------------------------- Launch (6)
        { id = "E1-LA-01", name = "Expendable Lift Cadence", quest = "E1-LAUNCH",
          kind = "invention", tier = 1, prereqs = { "E0-RK-05" }, cost = "M", payoff = "efficiency",
          condition = "research", unlocks = "Launch cost per kg down; repeat dispatch without refit", status = "sketch" },
        { id = "E1-LA-02", name = "Recovery & Refurbishment", quest = "E1-LAUNCH",
          kind = "invention", tier = 1, prereqs = { "E0-RK-05" }, cost = "M", payoff = "efficiency",
          condition = "research", unlocks = "Launch cost falls with cadence", status = "sketch" },
        { id = "E1-LA-KEYSTONE", name = "Lift Doctrine — Ten Flights", quest = "E1-LAUNCH",
          kind = "capstone", tier = 2, prereqs = { "E1-LA-01", "E1-LA-02" }, cost = "L", payoff = "gate",
          condition = "deed", unlocks = "DEED: cumulative launches >= N from one body. Opens a permanent fork — Reusable Chemical (A) vs Fixed Infrastructure (B).", status = "sketch" },
        { id = "E1-LA-03A", name = "Reusable Booster (branch A)", quest = "E1-LAUNCH",
          kind = "invention", tier = 2, prereqs = { "E1-LA-KEYSTONE" }, cost = "L", payoff = "efficiency",
          condition = "research", unlocks = "[Lift Doctrine A] Launchpad throughput up; low capital, scales anywhere. Excludes branch B.", status = "sketch" },
        { id = "E1-LA-03B", name = "Mass Driver (branch B)", quest = "E1-LAUNCH",
          kind = "invention", tier = 3, prereqs = { "E1-LA-KEYSTONE" }, cost = "XL", payoff = "building",
          condition = "research", unlocks = "[Lift Doctrine B] Non-rocket lift; enormous capital + a site requirement, then near-zero marginal lift. Excludes branch A.", status = "sketch" },
        { id = "E1-LA-04", name = "Heavy Lift Stack", quest = "E1-LAUNCH",
          kind = "invention", tier = 2, prereqs = { "E1-LA-KEYSTONE" }, cost = "L", payoff = "building",
          condition = "research", unlocks = "Oversize payload class (assembly modules)", status = "sketch" },
        { id = "E1-LA-05", name = "Launch Site Network", quest = "E1-LAUNCH",
          kind = "tier", tier = 3, prereqs = { "E1-LA-03A" }, cost = "L", payoff = "enabling",
          condition = "research", unlocks = "Dispatch from a second body; retires the single-site dispatch limit", status = "sketch" },

        -- -------------------------------------------------------- Volatiles (8)
        { id = "E1-VO-01", name = "Volatile Prospecting", quest = "E1-VOLATILES",
          kind = "invention", tier = 1, prereqs = { "E0-RK-05" }, cost = "M", payoff = "resource",
          condition = "research", unlocks = "Ice/water richness bands appear in survey", status = "sketch" },
        { id = "E1-VO-02", name = "Ice Extraction", quest = "E1-VOLATILES",
          kind = "invention", tier = 1, prereqs = { "E1-VO-01" }, cost = "M", payoff = "building",
          condition = "research", unlocks = "Ice Extractor", status = "sketch" },
        { id = "E1-VO-03", name = "Water Purification", quest = "E1-VOLATILES",
          kind = "invention", tier = 2, prereqs = { "E1-VO-02" }, cost = "M", payoff = "recipe",
          condition = "research", unlocks = "Water recipe chain", status = "sketch" },
        { id = "E1-VO-KEYSTONE", name = "Propellant Doctrine — The First Tank", quest = "E1-VOLATILES",
          kind = "capstone", tier = 2, prereqs = { "E1-VO-03" }, cost = "L", payoff = "gate",
          condition = "deed", unlocks = "DEED: produce any propellant off-world, once. Opens a permanent fork — Methalox (A) vs Hydrolox (B), read from the body's substrate.", status = "sketch" },
        { id = "E1-VO-04A", name = "Sabatier Reactor (branch A, Methalox)", quest = "E1-VOLATILES",
          kind = "invention", tier = 2, prereqs = { "E1-VO-KEYSTONE" }, cost = "M", payoff = "recipe",
          condition = "research", unlocks = "[Propellant Doctrine A] CO2 + H2 -> methane; wants a carbon source (the methane tell). Excludes branch B.", status = "sketch" },
        { id = "E1-VO-04B", name = "Electrolysis (branch B, Hydrolox)", quest = "E1-VOLATILES",
          kind = "invention", tier = 2, prereqs = { "E1-VO-KEYSTONE" }, cost = "M", payoff = "recipe",
          condition = "research", unlocks = "[Propellant Doctrine B] Water -> LH2 + LOX; wants water ice, higher performance, harder storage. Excludes branch A.", status = "sketch" },
        { id = "E1-VO-05", name = "Cryogenic Storage", quest = "E1-VOLATILES",
          kind = "invention", tier = 2, prereqs = { "E1-VO-04A", "E1-VO-04B" }, cost = "M", payoff = "building",
          condition = "research", unlocks = "Depot tank; sets the boil-off baseline", status = "sketch" },
        { id = "E1-VO-06", name = "In-Situ Tank Fabrication", quest = "E1-VOLATILES",
          kind = "tier", tier = 3, prereqs = { "E1-VO-05" }, cost = "M", payoff = "access",
          condition = "research", unlocks = "Depot buildable without lifted parts", status = "sketch" },
        { id = "E1-VO-07", name = "Propellant Export", quest = "E1-VOLATILES",
          kind = "tier", tier = 3, prereqs = { "E1-VO-05" }, cost = "M", payoff = "resource",
          condition = "market", unlocks = "Propellant enters the space market as a traded good", status = "sketch" },

        -- --------------------------------------------------------- Mobility (5, no keystone)
        { id = "E1-MO-01", name = "Transfer Planning", quest = "E1-MOBILITY",
          kind = "invention", tier = 1, prereqs = { "E0-RK-05" }, cost = "S", payoff = "efficiency",
          condition = "research", unlocks = "Propellant per leg down", status = "sketch" },
        { id = "E1-MO-02", name = "High-Impulse Engines", quest = "E1-MOBILITY",
          kind = "invention", tier = 2, prereqs = { "E1-MO-01" }, cost = "L", payoff = "efficiency",
          condition = "research", unlocks = "Propellant tax down; longer legs reachable", status = "sketch" },
        { id = "E1-MO-03", name = "Electric Propulsion", quest = "E1-MOBILITY",
          kind = "invention", tier = 2, prereqs = { "E1-MO-01" }, cost = "L", payoff = "efficiency",
          condition = "research", unlocks = "Propellant tax down for slow cargo; unlocks a bulk-freight lane class", status = "sketch" },
        { id = "E1-MO-04", name = "Nuclear Thermal", quest = "E1-MOBILITY",
          kind = "tier", tier = 3, prereqs = { "E1-MO-02" }, cost = "XL", payoff = "efficiency",
          condition = "research", unlocks = "Reaches the outer-body lanes; transit time down", status = "sketch" },
        { id = "E1-MO-05", name = "Depot Routing", quest = "E1-MOBILITY",
          kind = "tier", tier = 3, prereqs = { "E1-MO-03" }, cost = "M", payoff = "enabling",
          condition = "research", unlocks = "Refuel mid-route; lane freshness intel", status = "sketch" },

        -- ------------------------------------------------------------ Yards (6)
        { id = "E1-YA-01", name = "Orbital Rendezvous", quest = "E1-YARDS",
          kind = "invention", tier = 1, prereqs = { "E0-RK-05" }, cost = "M", payoff = "enabling",
          condition = "research", unlocks = "Two-vehicle operations", status = "sketch" },
        { id = "E1-YA-02", name = "Modular Assembly", quest = "E1-YARDS",
          kind = "invention", tier = 2, prereqs = { "E1-YA-01" }, cost = "L", payoff = "building",
          condition = "research", unlocks = "Assembly Plant", status = "sketch" },
        { id = "E1-YA-KEYSTONE", name = "Yard Doctrine — The First Truss", quest = "E1-YARDS",
          kind = "capstone", tier = 2, prereqs = { "E1-YA-02" }, cost = "L", payoff = "gate",
          condition = "deed", unlocks = "DEED: complete one structure assembled off Earth. Opens a permanent fork — Orbital Assembly (A) vs Surface Assembly (B).", status = "sketch" },
        { id = "E1-YA-03A", name = "Orbital Port (branch A, Orbital Assembly)", quest = "E1-YARDS",
          kind = "invention", tier = 2, prereqs = { "E1-YA-KEYSTONE" }, cost = "L", payoff = "building",
          condition = "research", unlocks = "[Yard Doctrine A] Orbital Port; joins the trade graph; no gravity well but total dependence on lift. Excludes branch B.", status = "sketch" },
        { id = "E1-YA-03B", name = "Surface Assembly Yard (branch B)", quest = "E1-YARDS",
          kind = "invention", tier = 2, prereqs = { "E1-YA-KEYSTONE" }, cost = "L", payoff = "building",
          condition = "research", unlocks = "[Yard Doctrine B] Build on the body; local materials, but pays the well every launch. Excludes branch A.", status = "sketch" },
        { id = "E1-YA-04", name = "Station Fabrication", quest = "E1-YARDS",
          kind = "tier", tier = 3, prereqs = { "E1-YA-03A" }, cost = "L", payoff = "access",
          condition = "research", unlocks = "Habitat/station class; orbital build sites", status = "sketch" },
        { id = "E1-YA-05", name = "On-Orbit Servicing", quest = "E1-YARDS",
          kind = "tier", tier = 3, prereqs = { "E1-YA-03A", "E1-YA-03B" }, cost = "M", payoff = "efficiency",
          condition = "research", unlocks = "Asset maintenance down; retires single-use orbital assets", status = "sketch" },

        -- ------------------------------------------------------- Extraction (5)
        { id = "E1-EX-01", name = "Regolith Excavation", quest = "E1-EXTRACTION",
          kind = "invention", tier = 1, prereqs = { "E0-RK-05" }, cost = "M", payoff = "building",
          condition = "research", unlocks = "Surface Extractor", status = "sketch" },
        { id = "E1-EX-02", name = "Metallic Body Working", quest = "E1-EXTRACTION",
          kind = "invention", tier = 2, prereqs = { "E1-EX-01" }, cost = "M", payoff = "recipe",
          condition = "research", unlocks = "Iron-nickel ore chain; access to metallic bodies", status = "sketch" },
        { id = "E1-EX-03", name = "Platinum-Group Separation", quest = "E1-EXTRACTION",
          kind = "invention", tier = 2, prereqs = { "E1-EX-02" }, cost = "L", payoff = "recipe",
          condition = "research", unlocks = "PGM recipe; PGM enters the traded-goods list", status = "sketch" },
        { id = "E1-EX-04", name = "Deep Regolith Processing", quest = "E1-EXTRACTION",
          kind = "tier", tier = 3, prereqs = { "E1-EX-01" }, cost = "M", payoff = "efficiency",
          condition = "research", unlocks = "Extractor throughput up", status = "sketch" },
        { id = "E1-EX-KEYSTONE", name = "Autonomy Doctrine — The Empty Shift", quest = "E1-EXTRACTION",
          kind = "capstone", tier = 3, prereqs = { "E1-EX-04", "L-AU-01" }, cost = "L", payoff = "gate",
          condition = "deed", unlocks = "DEED: run an off-world building at zero workforce for N days. Crosses Extraction x L-AUTO. Opens a permanent fork — Crewed Operations (A) vs Autonomous Fleets (B).", status = "sketch" },
        { id = "E1-EX-05A", name = "Crewed Operations (branch A)", quest = "E1-EXTRACTION",
          kind = "invention", tier = 3, prereqs = { "E1-EX-KEYSTONE" }, cost = "L", payoff = "efficiency",
          condition = "research", unlocks = "[Autonomy Doctrine A] Higher output ceiling; habitability and life-support burden. Excludes branch B.", status = "sketch" },
        { id = "E1-EX-05B", name = "Autonomous Mining Rig (branch B)", quest = "E1-EXTRACTION",
          kind = "invention", tier = 3, prereqs = { "E1-EX-KEYSTONE" }, cost = "L", payoff = "efficiency",
          condition = "research", unlocks = "[Autonomy Doctrine B] Workforce scalar collapses; electronics dependency becomes strategic. Excludes branch A.", status = "sketch" },

        -- --------------------------------------------------------- L-LOG (stub, 4)
        { id = "L-LG-01", name = "Freight Optimisation", quest = "L-LOG",
          kind = "invention", prereqs = {}, cost = "M", payoff = "efficiency",
          condition = "research", unlocks = "Convoy cost per leg down", status = "stub" },
        { id = "L-LG-02", name = "Ion Propulsion", quest = "L-LOG",
          kind = "invention", prereqs = { "L-LG-01" }, cost = "L", payoff = "efficiency",
          condition = "research", unlocks = "Propellant per space leg down; slower transit", status = "stub" },
        { id = "L-LG-03", name = "Ion Propulsion II", quest = "L-LOG",
          kind = "tier", tier = 2, prereqs = { "L-LG-02" }, cost = "M", payoff = "efficiency",
          condition = "research", unlocks = "Thrust up (mid-tier must carry a real trade-off — the TI lesson)", status = "stub" },
        { id = "L-LG-04", name = "Nuclear-Thermal Propulsion", quest = "L-LOG",
          kind = "invention", prereqs = { "L-LG-02" }, cost = "XL", payoff = "efficiency",
          condition = "research", unlocks = "High-thrust deep-space transit", status = "parked" },

        -- -------------------------------------------------- L-AUTO (stub, 3)
        { id = "L-AU-01", name = "Industrial Automation", quest = "L-AUTO",
          kind = "invention", prereqs = {}, cost = "M", payoff = "efficiency",
          condition = "research", unlocks = "Workforce scalar down across extraction", status = "stub" },
        { id = "L-AU-02", name = "Industrial Automation II", quest = "L-AUTO",
          kind = "tier", tier = 2, prereqs = { "L-AU-01" }, cost = "S", payoff = "efficiency",
          condition = "research", unlocks = "Workforce scalar down further", status = "stub" },
        { id = "L-AU-03", name = "Autonomous Operations", quest = "L-AUTO",
          kind = "invention", prereqs = { "L-AU-01" }, cost = "L", payoff = "efficiency",
          condition = "research", unlocks = "Remote/unmanned building operation off-world", status = "stub" },
    },
}
