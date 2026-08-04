# Project Io — Planetology

**Status: implemented (first cut), some calls still owed.** This doc is the authority for [[BL-167]]
(Planetology — generated atmosphere, chemistry, and a simulated evolution history).

The **R&D pass landed 2026-07-21** (Sprint 1): a chemical model of the full chain from an input
solar system to a human civilisation, researched across 13 domains with an adversarial fact-check on
each. The **implementation landed the same day**, and the **homeworld was recalibrated 2026-07-22**
from a hand-picked input box to a measured, strict floor with reject-and-reroll — see
§ Implementation. What remains open is listed in § Open calls; those are still Ben's and are not
settled.

**Contents** *(status per section, 2026-08-02)*:

| Section | Status |
|---|---|
| Why this exists | settled |
| The chain — A → B → C (S0–S7) | settled |
| S8 Legacy / S9 Spend | settled |
| The homeworld rule | settled |
| Archetypes | settled |
| Mapping history onto Io's resource list | settled |
| Presentation (biography, dated lines) | settled |
| Determinism & cost | settled |
| Implementation (incl. Continents, preferences, checkpoints, verification) | settled |
| Open calls | open (3, 7, 8, 10 remain — 4 closed 2026-08-04, ore provinces) |
| Known weaknesses | recorded — one half-closed 2026-07-30 |

---

## Why this exists

A body's terrain (`TILE_GENERATION.md`) is generated against static, per-body solar parameters
authored today (temperature class, atmosphere class, hydrological state, geological activity).
Planetology replaces "authored atmosphere class" with a **generated one** — a body-level history
pass that runs *before* tile generation and answers: what is this world's air made of, and why?

Ben's explicit reference is **Shadow Empire** (VR Designs/Slitherine): its planet generation runs
a **Planetology** phase (astrophysical rolls), then **Geology** (terrain/rainfall derived from those
rolls), then a simulated **Evolution** phase. Atmospheric composition is explicitly a *consequence*
of that history. Framed by Ben as **the first thing a player sees** — it has to carry real
first-impression weight, not read as a generic flavour text dump.

**The creative constraint (Ben, 2026-07-21): always human, always carbon-based.** The generator
never asks "did intelligence arise, and what shape was it?" This is load-bearing, not cosmetic — it
inverts what the model is *for*:

> The homeworld's chain is constrained to **succeed**. Its variation is in the **endowment humans
> inherit** — oxygen level, ocean fraction, arable land, fossil-fuel reserves, ore accessibility —
> not in whether life or people happened. **Every other body runs the identical chain and is free
> to fail at any stage.** One pipeline, gated differently.

Non-carbon or non-human outcomes are explicitly out of scope, and are the natural content of a
later multiverse-era expansion rather than a v1 axis.

---

## The chain — A → B → C

Ten ordered stages. Each is a **threshold test on already-computed scalars**, not a simulation.
Each emits **one dated line** into the body's history. The interesting output is **which gate a
body died at** — failure is the content, and a sterile world's three-line history is its
characterisation.

| # | Stage | The question it answers | Fails into |
|---|---|---|---|
| S0 | **System** | What did this nebula have to work with? | — (cannot fail) |
| S1 | **Accretion** | What is this body made of? | Chondrite / Core-fragment |
| S2 | **Air** | Did it keep an atmosphere? | Dead Rock |
| S3 | **Engine** | Does its interior still run? | Frozen interior, no ore engine |
| S4 | **Water** | Is there liquid water, and does it stay? | Oven / Snowball / Relict |
| S5 | **Spark** | Did life start? | Sterile |
| S6 | **Breath** | Did oxygen accumulate? | Mat World / Boring Billion |
| S7 | **Green** | Did land get colonised, and can it burn? | Waterworld / green-but-unburnable |
| S8 | **Legacy** | What did life leave in the rocks? | (the payoff stage) |
| S9 | **Spend** | How much has already been dug up? | homeworld only |

### S0 — System *(one roll per campaign, not per body)*

Everything heavier than helium is supernova debris, so **one nebula gives one metallicity to every
body it forms**. Stellar mass fixes luminosity and main-sequence lifetime; pick one consistent
exponent pair (L ∝ M^3.5 with t ∝ M^-2.5). The water ice line sits at ~2.71·√(L/L☉) AU and decides
wet-versus-dry accretion. Radiogenic U/Th is **decoupled** from [Fe/H] — U and Th come from rare
r-process events, so mantle Th/U plausibly varies an order of magnitude between systems at the same
metallicity. That decoupling is free variety: an iron-rich system can be a geologically dead one.

Because the star brightens ~30% over its main sequence, the **continuously** habitable corridor is
much narrower than the instantaneous one — 0.976–1.428 AU for a Sun over 4.5 Gyr, **63%** of the
instantaneous width.

*This stage cannot fail. Its purpose is correlation:* without it, bodies roll independent ages and
metal fractions and the system reads as a bag of unrelated rocks rather than one nebula's children.
It also gives the report a campaign header — *"an iron-poor system; you will be importing steel."*

### S1 — Accretion

Equilibrium condensation, not chance. A cooling solar-composition gas drops solids in strict
temperature order: Ca-Al refractories ~1650 K, metallic Fe-Ni 1334 K, forsterite/enstatite
1310–1336 K, alkalis ~950 K, FeS 664 K, water ice ~150–180 K. **Formation radius against the ice
line sets the volatile inventory before any delivery.**

Water is an **imported veneer**: Earth's ocean is only **0.023% of its mass**, and seawater D/H
matches CI chondrites, not comets. Separately, **late accretion of ~0.3–1% of planetary mass after
core closure** restores the highly siderophile elements (Au, Pt, Ir, Os) that metal-silicate
partitioning otherwise strips >99% into the core — which is why platinum-group metals are a
*stochastic late-veneer roll*, decoupled from mass and tectonics. A small dead world can be
PGM-rich.

Short-lived ²⁶Al (t½ 0.717 Myr) melts anything >~20 km accreting within ~2 Myr of CAI formation. A
later hit-and-run impact can strip the silicate mantle — the honest mechanism for
`composition_bias::metallic`. *(Note: 16 Psyche is no longer a safe exemplar; its bulk density of
3.977 g/cm³ undercuts the exposed-core reading. Iron meteorites are the warrant: >95 wt% Fe-Ni-Co,
Ni 5–25 wt%, Ir spanning 0.01–38 ppm.)*

> **Implementation trap.** The rocky mass-radius relation R ≈ M^0.27 is **invalid below ~0.05 M⊕** —
> it returns 0.451 R⊕ for Mercury against a true 0.383, an 18% error that enters the next stage as
> v_esc⁴, i.e. ~40%. All three of Io's non-homeworld bodies sit in that regime. Switch to constant
> density from core mass fraction below the threshold.

### S2 — Air *(the cheapest and most predictive gate in the model)*

Two loss channels integrated over 4.5 Gyr — thermal Jeans escape, and energy-limited hydrodynamic
escape driven by the ~100 Myr XUV-saturated phase of a young star. Integrating gives Zahnle &
Catling's **cosmic shoreline**, I ∝ v_esc⁴.

Computing `shore = (v_esc/11.186)⁴ / S`, normalised so Mars = 1.0, reproduces the observed split
across eleven bodies:

| Earth | Venus | Titan | Ganymede | Mars | Io | Europa | Mercury | Moon | Ceres |
|---|---|---|---|---|---|---|---|---|---|
| 10.5 | 4.1 | 3.0 | 1.04 | 1.0 | 0.79 | 0.31 | 0.033 | 0.022 | 3.5e-4 |

Bands: `<0.05` → none; `0.05–0.7` → thin; **`0.7–1.5` → marginal** (outcome decided by initial
inventory — this band is what resolves the Mars/Ganymede tension honestly rather than pretending one
constant separates them); `≥1.5` → substantial.

Two honesty notes. **The shoreline is an empirical fit, not a derivation** — energy-limited theory
predicts a shallower exponent and nobody has explained the observed 4, and it is calibrated on the
inner system only. And **drop the magnetic dynamo as a mechanic**: Venus has no dynamo and 92 bar;
measured ion escape at Venus (~0.5 kg/s), Earth (~1.4) and Mars (0.7–2.1) agrees within 1–2 orders
of magnitude, and modelled escape is *non-monotone* in magnetisation. Keep the dynamo as narration
only.

### S3 — Engine

Radiogenic production scales with mantle mass, loss scales with surface area, so shutdown time
scales roughly as ρ·R — **the first-order reason Mars died and Earth did not.** Decay the four
isotopes explicitly (²³⁸U 4.47, ²³⁵U 0.704, ²³²Th 14.05, ⁴⁰K 1.25 Gyr): a single lumped half-life is
a bad fit to a sum of exponentials spanning 0.70–14 Gyr. Earth's present surface heat flow is
47 ± 2 TW with ~20 TW radiogenic, and production was ~4.5× higher at 4.5 Ga — which back-fits to an
effective **2.06 Gyr**, not the 2.85 Gyr commonly quoted.

Tidal heating is a separate and savagely distance-sensitive term, ∝ M_primary^2.5·R⁵·e²/(a^7.5·Q).
The a^-7.5 dependence *is* the mechanism, and it separates Io (~2 W/m², >400 volcanoes) from Europa
(~0.05–0.3 W/m², subsurface ocean) from Callisto (dead).

**Mobile-lid tectonics needs heat *and* water.** Convective stress *rises* as a planet cools, so the
gate is a **band** (roughly 0.7 ≤ Θ ≤ 1.8) plus a mass floor (~0.5 M⊕) plus surface water — hydration
weakens faults. Venus then falls out correctly as stagnant-lid *because it lost its water*, despite
0.815 M⊕ and ample heat. Above ~5–7 M⊕ outgassing collapses entirely as melt goes negatively buoyant:
bigger is not more volcanic.

**Consequence that matters downstream:** no subduction → no arc magmas carrying 2–6 wt% H₂O → **no
porphyry copper**, which is most of the world's copper. A stagnant-lid world is copper-poor
*structurally*, not randomly.

### S4 — Water

The carbonate–silicate (Urey) cycle is the long-term thermostat: CO₂ dissolves in rain, weathers
silicate to bicarbonate, precipitates as seafloor carbonate, and subduction returns it. Weathering
rises exponentially with temperature, so it is a negative feedback stabilising climate on ~10⁶ yr —
fast enough to model as **instantaneous equilibrium at generation resolution**.

> **Do not model the greenhouse as a fixed +33 K offset.** The outer habitable edge is *defined* by a
> maximum ~8 bar CO₂ greenhouse, which at S = 0.344 demands 78–93 K of warming. A fixed offset
> generates frozen homeworlds across the outer half of the corridor. Instead **pin** surface T near
> 273–320 K wherever liquid water and a tectonic return path coexist, and let it run away outside
> the limits.

Two irreversible failures and one survivable one:

- **Inner (Oven).** Above S_eff ≈ **1.0512** (Kopparapu 2013, Sun; 1.107 in the 2014 mass-dependent
  revision — state which) outgoing longwave saturates at the Simpson–Nakajima limit and no
  equilibrium exists below total ocean vaporisation. The moist-greenhouse limit at 1.014 arrives
  first and dries the world over 10⁸–10⁹ yr, leaving Venus's **120–150× D/H** fingerprint.
- **Outer (Snowball / freeze-out).** Below S_eff ≈ **0.3438** additional CO₂ raises Rayleigh albedo
  faster than it adds opacity. Ice-albedo runaway is escapable only because weathering stops with
  the ice while volcanic CO₂ accumulates to the ~0.1 bar deglaciation threshold over 4–30 Myr — so
  **a body with dead tectonics that snowballs stays snowballed.**
- Liquid water needs pressure above the 611.657 Pa triple point *and* temperature below the
  **pressure-dependent** boiling point (373 K is the 1 bar value only; ~319 K at 0.1 bar).

### S5 — Spark *(abiogenesis)*

Life needs five things at once: a polar solvent; one-carbon and one-nitrogen feedstocks; a reductant;
a **concentration mechanism**; and a cycling driver.

The reductant is the cheap part. **Serpentinisation** of olivine yields H₂ exothermically — Lost City
vents run 45–90 °C at pH 9–11 with up to **200 mM** dissolved H₂ — delivering an alkaline effluent
into a CO₂-rich ocean. That is a free 3–4 pH-unit proton gradient of the same polarity every living
cell uses, across Fe(Ni)S mineral walls whose unit cells match ferredoxin and hydrogenase active
sites.

**The hard part is concentration, and this is the design-relevant insight.** HCN oligomerises to
purine precursors only above ~**0.1 M**, competes with hydrolysis at 0.01–0.1 M, and loses below
that. Hadean ocean HCN was orders of magnitude below threshold. So "the ocean was a soup" is
chemically false, and the gate is really about **geometry**: small isolated cyclable water bodies —
subaerial relief plus liquid water — or a hydrothermal pore network (~5000× enrichment
demonstrated). *This is why a waterworld with no land is a worse abiogenesis candidate than a world
with continents.*

Timing licenses treating this as **near-deterministic given preconditions**: oldest undisputed life
at 3.48 Ga (Dresser stromatolites), LUCA phylogenomically 4.33–4.09 Ga. But note **LUCA is not
abiogenesis** — it is an already-sophisticated cell — so the LUCA date is an *upper* bound on the
interval.

> **Flag this honestly in the doc and the readout.** The probability of abiogenesis is the least
> constrained number in the whole model: n = 1, with anthropic selection guaranteeing we observe the
> world where it happened. Gating on preconditions with near-certain firing is a **game-design dial
> wearing scientific clothing**. The opposite position (low probability, mostly-sterile systems) is
> equally consistent with the evidence and produces a very different feel.

### S6 — Breath *(oxygenation — the model's hinge)*

Photosynthesis and respiration are exact inverses, so a productive biosphere that respires
everything it fixes produces **zero net O₂**. Oxygen accumulates only when reduced carbon is
physically **buried** out of contact with it — exactly 1.00 mol O₂ stranded per mol C_org buried,
~1.875 per mol pyrite S. **Oxygenation is a sedimentology problem.**

And it is a **ratio test, not a filling problem**: atmospheric O₂ (~3.7e19 mol) over the gross burial
flux (15.8 ± 3.3 Tmol/yr) is a residence time of only **~2.3 Myr** — nothing on a 4.5 Gyr chain.
Oxygen appears when the *sign* of a near-balanced ledger flips, i.e. when burial flux exceeds the
**geological reductant flux**, which decays monotonically as radiogenic heat runs down (Archean heat
flow was ~3× modern at 4.0 Ga, ~2× at 2.5 Ga).

**It happens twice, a billion years apart, and collapsing them into one gate is the error that makes
a ladder unsatisfiable** — Earth itself would fail a 10%-O₂ complex-life gate immediately after the
GOE:

- **GOE** (2.46–2.43 Ga) reaches only **0.1–1% PAL**, and destroys the methane greenhouse, causing
  the Huronian glaciation.
- **The Boring Billion** (1.8–0.8 Ga) locks low because **ferruginous oceans scavenge phosphate onto
  iron oxyhydroxides**, capping productivity, capping burial, keeping the ocean ferruginous. A
  self-stabilising trap. Escape requires a tectonic nutrient shock.
- **NOE** (850–540 Ma) reaches modern levels.

#### Each gate is judged at its own epoch, not at present day *(2026-08-04)*

The reductant flux "decays monotonically as radiogenic heat runs down" — which means the heat
term in these gates is a **function of when the gate fires**, not of the body's age now. Radiogenic
production falls ~4.7× across a body's life, so testing a 2 Gya event against present-day `theta`
asks the wrong question: an 8 Gyr world is radiogenically cold *today* and was not cold *then*.

`planetology.cpp` therefore carries `theta_at(age)` and `mobile_lid_at(age)` — the radiogenic term
re-evaluated at the requested age, the tidal term carried across unscaled. **The NOE's tectonic
nutrient shock is tested at `age - noe_at`.** `st.theta`, `st.mobile_lid` and `profile.geology`
remain present-day and bit-identical, so Continents, Engine's biography and tile terrain are
untouched; only the NOE branch reads history.

**The GOE gate deliberately still reads present-day `theta`,** and the asymmetry is known rather
than accidental. It is an *upper* bound (`theta < 2.4`) — heat only falls, so today's value is a
conservative proxy — and, decisively, the 2.4 constant was calibrated against present-day theta.
Re-siting it to the GOE epoch (where theta runs ~2× higher) without re-deriving that constant was
measured: acceptance fell 78.5% → 60.2% and 69% of rejects became Mat Worlds. Closing the asymmetry
properly needs an epoch-relative threshold, which is a calibration pass, not an edit. See NR-046.

Found by the **C1 rejection census** (`tools/verify/planetology_sweep.cpp`), which measures *which
floor clause* rejects each homeworld. It is the instrument that made this visible: "cold and old"
cost 2.52 draws against a ~1.24 baseline, for heat the world only lost *after* the gate it was being
judged on. After the fix the worst preference is `oxygen_story=low` at 1.94 — a real design axis
rather than a modelling artifact.

### S7 — Green *(land, and fire)*

Ozone screens UV-C and most UV-B, and the screening is strongly **sublinear** in pO₂. Recent 3D
chemistry-climate work revises ozone columns **down by up to 4.68×** for a given O₂: 279 DU at 100%
PAL, ~66 DU at 10%, 18 DU at 0.1%. **So the land gate sits near 5–10% PAL, not the ~1% older 1D work
implied.** Use a 3–4 point lookup, not a power law (a power-law fit overestimates by ~2.3× at 10%
PAL).

Land plants then invented **lignin** — randomly cross-linked, not hydrolysable, and with a bulk
terrestrial C:P far above marine, so burying plant carbon strands O₂ *without* starving the
phosphorus cycle that produced it. Self-reinforcing. Root-driven weathering simultaneously pulls CO₂
down and cools the planet into an ice age whose 100 kyr glacio-eustatic cycles are what stack peat
into economic coal seams. **The endowment terraforms its own climate.**

Separately and at a much higher bar sits **combustion**: sustained fire in natural fuels needs
roughly **15–18.5% O₂** by mole fraction (contested; Belcher & McElwain argue the higher end, and it
is fuel-moisture and pressure dependent — read effective pO₂, not a bare mole fraction). This is the
single hardest physical gate between a biosphere and a civilisation, and it is **separable from the
land gate** — which yields a genuinely novel outcome: **a green world that cannot burn.** No
charcoal, no kiln, no smelting, no ceramics, at any ore grade.

Above ~21% the trade reverses: fire *suppresses* forest cover — ~26% reduction at 20.95% O₂ rising
to ~60% at 35%. A high-oxygen world is grassland-biased and timber-poor.

---

## S8 — Legacy: the B → C joint

**This is the stage the brief says is usually hand-waved, and it is the answer to Ben's question.**
The link from life to civilisation is not metaphorical and not a tech tree. **A biosphere physically
manufactures the industrial resource base**, and each product has its own chemical gate:

**Banded iron → the iron endowment.** Fe(II) is soluble; Fe(III) is not, by orders of magnitude. An
anoxic ocean transports hydrothermal Fe(II) basin-wide; where it meets an oxidant it precipitates and
rains out. **BIF holds >60% of world iron reserves** — the Hamersley alone is ~150,000 km² and
~300 Gt. The window **opens** with an Fe-oxidising biosphere and **closes** at ~1.8 Ga when the deep
ocean finally went oxic and stopped carrying dissolved iron. So **iron endowment is the *duration* of
the partially-oxygenated window, not the endpoint O₂.** Then a *second, independent* event: raw BIF is
only ~30% Fe (taconite); the 60–65% direct-shipping ore is later oxidative silica leaching (Hamersley
upgraded 1.4–1.1 Ga). Iron is **window duration × later oxic weathering**.

**Fossil carbon, and it forks.** *Petroleum* is a marine **algal and planktonic** product — Type I/II
kerogen needing bottom water below 0.2 mL/L O₂ to survive remineralisation, then burial into the
**60–120 °C oil window** at 2–4 km (gas above 150 °C, nothing above 200 °C). It therefore needs only
a **microbial-grade** biosphere. *Coal* needs vascular land plants **and** everwet tropical basins
**and** subsidence; ~90% of coal beds come from a single ~107 Myr Carboniferous–Permian window, and
the cause is climate × tectonics — the lignin-versus-fungi lag story is refuted on mass balance.
Rank is thermal (<35 °C peat, 35–50 lignite, 50–80 subbituminous, 80–180 bituminous, >180 anthracite)
and **only mid-bituminous cokes**, so a world can be coal-rich and metallurgically poor. *(Counterweight:
charcoal reduced iron at industrial scale for two millennia — coke gates **cheap** steel, not steel.)*

**Laterite and bauxite.** Hydrolysis under >1500 mm/yr and >20 °C strips silica and bases; **free O₂**
converts mobile Fe(II) to immobile Fe(III) so iron and aluminium stay put. Needs oxygen, tropics, and
**tectonic quiescence** — directly opposed to coal's subsidence requirement. A free, physically
motivated trade-off axis.

**Supergene copper.** Crustal Cu is ~28 ppm against a 0.15–1% cutoff: a **150–350× concentration
factor**, against iron's mere 5–12×. *That asymmetry is why iron is near-universal and copper is the
chokepoint.* Oxygenated meteoric water leaches Cu²⁺ from a pyrite cap and reprecipitates chalcocite
below the water table — Chuquicamata runs 0.7% hypogene to 2–3% supergene. Needs oxygen **and** a
semiarid seasonal climate **and** 0.5–9 Myr of stability. Without oxygen, copper exists only as
porphyry protore an early industry cannot use.

**Soil.** Regolith is not soil. Soil needs clay-humus complexes, aggregate structure, and above all
**biological nitrogen fixation** — N₂'s 941 kJ/mol triple bond yields only to diazotrophs. A mature
profile forms in 1,500–7,000 years, geologically instantaneous, so arable land is a pure function of
climate × land × biosphere with no window roll. Earth's arable cropland is only **~10–11% of land
area** — the knife-edge is real.

**Limestone.** Skeletal carbonate from shelly life: the flux for iron smelting and the binder for all
masonry. *(Io has no resource slot for it — see Open calls.)*

### And then the civilisation gate itself

Not a timer. Five concrete requirements, each traceable to something above:

1. **Fire** — pO₂ ≥ ~0.16. Without it, no metallurgy at any ore grade.
2. **Ore at usable grade** — BIF-upgraded iron; supergene or oxide-cap copper.
3. **A dense fuel.** Photosynthesis captures ~0.05% of the solar constant, yielding under **1 W/m²**
   against a coal seam's **10³–10⁴ W/m²**. Britain's entire theoretical organic iron ceiling was
   ~1.25 Mt/yr *with woodland over half the island*; coke-fired Britain made 2.5 Mt in 1850.
4. **Arable land plus climate stable enough to amortise cultivation** — the Holocene from 11,650 cal
   BP, after which agriculture arose independently on four continents within ~5,000 years.
5. **Cheap bulk transport** — Diocletian's Price Edict implies sea : river : road cost ratios near
   **1 : 4.9 : 34–42**. *(This is the same argument [[BL-170]]'s river logistics discount already
   encodes.)*

**And the escape ceiling.** Δv is roughly linear in escape velocity while mass ratio is *exponential*
in Δv: 1000 kg to escape costs ~44 t of vehicle at 1 M⊕ and ~2,936 t at 6 M⊕, becoming impractical
above ~10–11.5 M⊕. A fully industrial world can be **planetbound**.

---

## S9 — Spend: why the game's premise exists

Not chemistry — history, compressed to one scalar. **The inversion is the point: a richer endowment
industrialises *earlier* and is therefore *more* drawn down at campaign start.** Depletion multiplies
the **accessible** fraction, never the formed fraction — the ore is still there, the *cheap* ore is
not. Terrestrial peak conventional oil and the exhaustion of near-surface high-grade copper are the
real analogue.

This is the cheapest possible generated, in-fiction answer to *"why would a corporation go to
space"* — and it makes Era 0 → Era 1 a **pressure** rather than a menu choice, which is exactly what
`GENERATION_STRATEGY.md`'s economic premise asserts but does not currently generate.

---

## The homeworld rule — constrain the inputs, never the gates

Two tempting answers both fail:

- **Clamping the output** (`endowment[r] = max(endowment[r], floor[r])`) flattens exactly the variance
  it exists to protect, cannot express "rich in one thing, poor in another," and reads as rigged the
  moment a player compares two campaigns.
- **Clamping only the life gates** leaves nothing to stop the homeworld rolling S = 1.6, tripping the
  runaway-greenhouse branch, and arriving as a 737 K airless rock with a civilisation stapled on.
  That is not one pipeline gated differently; it is two pipelines wearing one coat.

**The rule: sample the homeworld's *inputs* from the sub-region of parameter space where the
identical unmodified chain succeeds.** That region is computed from the campaign's own S0 rolls, so
it differs every campaign. Concretely: orbit inside the *continuously* habitable corridor derived
from the rolled star mass and age; M_p ∈ [0.7, 1.5] M⊕ (above S3's mobile-lid floor, below the
outgassing collapse); radiogenic index high enough that Θ lands in the mobile-lid band; and the water
endowment forced by **accretion geometry** (formation radius against the ice line, plus the
late-veneer roll) rather than a post-hoc clamp on a derived quantity.

Then run the chain. Every gate is evaluated. Every gate emits its line. None is bypassed.

**Why it does not feel rigged — three devices, all free:**

1. **Surface the margins, not just the outcomes.** *"Held its air — 11.2 km/s against 1.02 suns;
   comfortable"* versus *"9.4 km/s against 1.04 suns; marginal, and it thinned for a billion years."*
   Both pass. One reads as lucky. The margin is already computed; printing it costs a format string
   and converts a guarantee into a near-miss.
2. **Let it fail on any single axis.** A homeworld with no supergene copper, no coal (it industrialised
   on charcoal and arrived late), 17% oxygen sitting one point above the fire minimum, or 72% ocean
   and scarce arable land, is a legitimate and interesting campaign. Only the *conjunction* of total
   failure is excluded. This does more anti-rigging work than everything else combined — the player's
   first thought on a bad copper roll is "this world is poor in copper," not "the generator is on
   rails."
3. **Print the near-misses.** *"Survived two snowballs." "One mass extinction at 252 Mya; recovery took
   ten million years."*

**What actually varies, and it is a wide space.** The strongest axis is real chemistry:

> **The iron/coal antagonism.** One number — *when the deep ocean stopped being ferruginous* — sets
> both. Oxygenate **fast**: a short BIF window and poor iron, but ozone arrives early, land plants get
> a long run, and coal is rich. Oxygenate **slowly**: an unbounded BIF window and colossal iron, but
> late land colonisation and a short coal age. Two homeworlds can differ several-fold in iron and coal
> *in opposite directions*, with every gate passed.

Plus: the **subsidence/stability trade** (coal and source-rock burial want subsiding basins; supergene
enrichment, laterite and the BIF grade upgrade want quiescence — one dial, opposite signs), the
**water band** (ocean fraction buys shelf area and coastline but costs land, coal, arable and exposed
ore, with arable peaking as an inverted-U), the **oxygen level** (fire margin against forest
suppression), and **depletion**.

---

## Archetypes — the one-word verdict

Derived *from* the timeline by a switch statement, not rolled ahead of it. Named archetypes are what
make a generated world memorable; parameter columns are not.

| Archetype | Died at | Has | Lacks |
|---|---|---|---|
| **Cradle** | nothing — then S9 spent it | everything | the *easy* version of everything |
| **Mat World** | S6 GOE | colossal BIF iron, algal petroleum | coal, soil, timber, arable, fire |
| **Boring Billion** | S6 NOE | iron, marine petroleum | land life of any kind; O₂ present and useless |
| **Graveyard** | terminal event after S7 | coal, oil, iron, limestone — geology outlives biology | timber, produce, living soil |
| **Waterworld** | S7 (no land) — and S5 is marginal | limestone, phosphorite, marine oil | coal, soil, arable, **access to any ore** |
| **Oven** | S4 inner edge | carbon, sulfur, magmatic ore, evaporites | water, hydrothermal ore, any survivable surface |
| **Snowball Lock** | S4 ice-albedo, no escape | *everything it had, under kilometres of ice* | access — the limiting factor is depth, not presence |
| **Relict Wet World** | S2 slowly, over Gyr | clays, sulfates, iron oxides, polar ice | fossil fuels, oxygen, ore at grade |
| **Ocean Vault** | S2, but S5 passes subsurface | water at absurd scale, organics, chemical energy | surface access, oxidised mineralogy, any path to fire |
| **Cryo-Organic** | S4 — cold beats small on the shoreline | industrial-scale *abiotic* hydrocarbons, nitrogen | liquid water, **any oxidiser** — import O₂ to burn its own fuel |
| **Bake-Out** | S3 from the other side — tidal runaway | sulfur, refractory silicates, free heat | water, sediment, a surface that persists |
| **Dead Rock** | S2 immediately | unweathered surface ore, regolith, polar ice | coal, oil, timber, agriculture, **clay and sand** |
| **Core Fragment** | never entered the chain | Fe-Ni and PGM at grades no crust reaches | literally everything else |

**Three inversions carry the whole feature**, and they are what stop the archetype list collapsing
into a quality ladder:

- A world that **failed** at oxygenation is the **iron-richest** body in the system.
- A world that **died** is the richest prize — no competition, no rival gravity well.
- The **homeworld** is rich and *therefore* already spent.

*(Note: Bake-Out is Io itself — ~2 W/m² against Earth's ~0.09. The model should be able to produce
the game's namesake.)*

---

## Mapping history onto Io's resource list

Four endowment scalars plus two flags fan out through **one fixed table** into the 16 raw resources
that currently generate as tile deposits. Four rather than nineteen because each scalar must carry
exactly one sentence a player reads.

| Channel | Resources |
|---|---|
| `biotic_carbon` | coal, petroleum, peat, timber, agricultural produce |
| `oxidised_ore` | iron ore, copper ore *(supergene tier)* |
| `hydrothermal_ore` | copper ore *(porphyry tier)*, rare earth ore, platinum group metals |
| `sedimentary` | clay, sand, stone, silica |
| `core_exposed` (bool) | iron-nickel ore, platinum group metals |
| `cold_traps` (float) | water, on polar tiles of airless bodies only |

**The gates that matter:**

- **Petroleum** gates on `life_stage ≥ oxygenated` — **not** on complex life. This is the sharpest
  divergence in the mapping and the reason a microbial world can be oil-rich.
- **Coal and peat** hard-zero below `land`. **Timber and agricultural produce** require life
  *currently* ≥ land. Splitting **`life_stage`** (living) from **`life_stage_peak`** (fossil) costs
  one byte and *is* the entire Graveyard mechanic: a dead world keeps its coal and loses its forests.
- **Iron** never zero — base ~0.35 from magmatic and impact sources on any differentiated body, plus
  the BIF term, times the oxic-weathering grade upgrade.
- **Copper** ~zero on any stagnant-lid world. Structural poverty, not a bad roll.
- **Rare earths** never zero — lithophile, with a floor from one-shot magma-ocean fractionation (the
  lunar KREEP route, no tectonics required), rising with crustal reworking.
- **Clay requires liquid water at some epoch** — clays are phyllosilicates from aqueous alteration.
  *This is a live bug today:* `generate_deposits` puts clay on any wetland or valley tile, including
  on airless Selene. Mars has abundant clay; the Moon has essentially none.
- **Sand requires an atmosphere or liquid water** to sort and transport it. Airless bodies get impact
  regolith — unsorted, glass-rich, agglutinate-bearing. A different material.
- **Water on airless bodies** comes from polar cold traps — LCROSS measured **5.6 ± 2.9 wt%** in
  Cabeus regolith in permanently shadowed regions below ~110 K. This single fix turns *"Selene has no
  water"* into *"Selene has water at the poles and nowhere else"*, which is the propellant argument
  for the entire off-world programme.

**The highest-leverage line in the whole feature** is not a multiplier: `life_stage < land` **strikes
grassland, forest, wetland and tundra out of tile Pass 4**, falling back to a re-roll against the
*abiotic* entry of the same (band × moisture) cell — not a substitution table, since tundra's abiotic
partner is rocky, not icy. This is what makes a dead world **look** dead on the canvas rather than
merely reading poorer in a ledger.

---

## Presentation — a biography, not a stat panel

Eight lines maximum, dated, two columns: **what happened** on the left, **what it left behind** on
the right. The right column is the entire feature. *"640 Myr of ferruginous ocean → iron ×2.8"* is a
sentence a player argues with; `oxidised_ore: 2.8` is a number they scroll past.

```
KEPLER — Cradle.  4.55 Gyr, 1.00 g, 21% O2, mobile lid.
  4.5 Gya   Accreted beyond the ice line; 1.4 oceans arrive late.
  3.8 Gya   Serpentinising vents. Life.
  2.4 Gya   Oxygen begins. Ferruginous ocean for 0.6 Gyr.   -> iron x1.4  (a short window — Kepler is iron-lean)
  1.8 Gya   Deep ocean oxidises. Banded iron ceases.
  0.5 Gya   Ozone column closes; land colonised, 0.3 Gyr early.
  0.36 Gya  Everwet basins over a subsiding craton.         -> coal x2.4, coking rank
  now       1,400 years of industry.                        -> 62% of accessible coal and iron already drawn

SELENE — Dead Rock.  Chain ended at retention, 4.5 Gya.
  4.5 Gya   Accretion. Escape velocity 2.38 km/s against Kepler's 11.19.
  4.5 Gya   No atmosphere retained. Sterile, and it stayed that way.
            -> no fossil carbon, no soil, no clay. Water ice in permanent shadow only.
```

Five things make this work, each cheap:

1. **Asymmetry is the characterisation.** A Cradle gets eight lines; Selene gets three; Pallas gets
   two. Do not pad the failures — the brevity is the point, and the contrast is what makes a system
   read as a set of biographies rather than a table. Close a short report with an explicit
   **has/lacks pair**; a dead world's endowment is genuinely interesting and the report should sell
   it rather than apologise.
2. **Every line carries its consequence.** The because-clause is the difference between generation
   that reads as a *rule* and generation that reads as a *die roll*.
3. **Print the denial list, not just the grant.** *"No subduction, therefore no porphyry copper"* is
   more useful than any positive number — it tells the player where to go instead. Shadow Empire's
   causal rules are real and under-surfaced; its players learned life→oil by folklore.
4. **Quote everything homeworld-relative.** *"2.38 km/s against Kepler's 11.19."* Free legibility, one
   format string. Never print bare SI for a body the player has no intuition for.
5. **Do not linearise deep time.** Nothing happens for two billion years and then everything happens
   in the last five hundred million. Use event density as pacing — sparse cosmic openers, one line
   for an eon in the middle (*"and then nothing much for a billion years"* is a better beat than a
   gap), crowded and consequential at the end.

### Dating a line — one field, both regimes *(BL-220, landed 2026-07-30)*

A biography line is timestamped by **`history_event::years_before_epoch`, a signed integer count of
years back from the campaign epoch** (1 January 1960 — `campaign_epoch_year`, mirrored by
`ui::fmt::campaign_epoch_year` and cross-checked by a `static_assert` in `tile_inspector.cpp`).

It replaced a `float gya` for two reasons. **Display:** 1450 CE is 5.1e-7 Gya, so the ledger's
`%.2f Gya` rendered every historical date as `0.00 Gya` — the whole of recorded history collapsed
onto one string. **Arithmetic:** any date derived near the epoch from a deep-time baseline dies to
cancellation, since `4.5f - 273yr` is exactly `4.5f`, and the historical ladder computes dates that
way constantly. Integers also sort exactly and tie-break deterministically, which matters because a
timestamp that participates in sorting is effectively a gate path (§ Determinism & cost).

> Correcting a claim in BL-220 as filed: it also argued that float would make *"two events centuries
> apart compare equal"*. That is overstated — float32 carries ~7 significant digits at any exponent,
> so 1687 and 1688 stored directly as Gya do compare unequal and round-trip intact. The conclusion
> stands on display and cancellation; the stated mechanism did not.

**One field, not two**, because deep time and recorded history sort into a *single* ordered
biography — that list is the whole presentation model, and a second field would mean the ledger
could not sort its own lines. `format_history_date` picks the unit from the magnitude, which is
rule 5 ("do not linearise deep time") made mechanical:

| Magnitude | Reads as |
|---|---|
| ≥ 1 Gyr | `4.50 Gya` |
| ≥ 1 Myr | `252 Mya` |
| ≥ 10 kyr | `11,650 years ago` |
| within 10 kyr | `1687`, `-3200` |
| 0 | `now` |

The 10 kyr boundary is the conventional start of the Neolithic: older is a **climate** event and
reads as an interval, younger is a **human** one and reads as a calendar year. It is a judgement
call rather than a physical constant, and the one number here worth revisiting — set it too late and
a Younger-Dryas line claims a calendar year it has no business having; too early and a granary-cities
line reads as a bare interval.

Build a timestamp with `years_from_gya` (deep-time stages still date in Gya, because that is how
their chemistry is argued) or `years_from_calendar_year` (the historical ladder's entry point);
render it with `format_history_date`, never by hand.

`years_from_gya` is **deterministic but not exact**, and the difference is worth stating: float32
carries ~7 significant digits at any exponent, so `2.4f` lands about 95 years off a round 2.4 Gya.
That is invisible at a display resolution of 0.01 Gyr and it reproduces bit-for-bit, which is all the
model requires — but it is not exactness, so no assertion or doc line should claim it. Dates that
must be exact (every historical line) come in through `years_from_calendar_year`, which is integer
end to end. `planetology_harness` **R14** pins the
conversions, every format band, total oldest-first ordering, and that a historical line interleaves
correctly between deep time and the epoch.

**One `chain_stage` enum too**, for the same reason (BL-220 residual call 2, settled here). The
stages of the historical ladder (`docs/lore/HISTORY.md`, landing as BL-221/BL-222) join `chain_stage`
rather than running a parallel enum — a second one would leave the ledger unable to sort its own
lines. Their causal position is **after `legacy`** (S8 leaves the endowment) and **before `spend`**
(S9 draws it down, which is what industrialisation *does*), with a seam reserved ahead of settlement
for a pre-settlement narrative stage. Note the consequence: `chain_stage` is **ordered by chain
position and inserted into, not appended to** — the opposite of `body_archetype`'s append-only rule —
because gates and R6 both compare stages with `>=`. That is safe only while no serialiser exists; if
one lands, this enum needs a stable wire mapping before anything else is inserted.

**The diagnostic test, applied ruthlessly: for every generated value, name the decision it changes.
Anything that fails is deleted, not tuned.** That test is what removes the magnetic dynamo, D/H
enrichment, salinity, per-species Jeans parameters and fifteen of nineteen deposit multipliers. It is
the only defence against a feature that is technically deep and experientially invisible.

---

## Determinism & cost

**Purity.** A pure function of `(campaign_seed, body_id)`. **Per-stage hashed sub-streams**
(`splitmix64(mix(seed, body_id, STAGE_TAG))`), not one sequential stream — this is the single most
valuable determinism decision available and it is free: retuning the retention gate in S2 cannot
silently re-roll every deposit in S8. Allocate fresh XOR constants; note `0x4A71012u` is already
folded twice in `hard_coded_world.cpp` (the `generate_nations` and `generate_corporations`
seed folds).

**Two portability hazards the standing rule does not currently name:**

- `std::normal_distribution` and `std::uniform_int_distribution` are **implementation-defined** and
  produce different sequences on libstdc++ versus MSVC from identical seeds. Hand-roll Box–Muller or
  use a fixed table. Never the standard distributions.
- `exp`/`log`/`pow`/`cbrt` are **not bit-identical** across compilers, libm versions, or fast-math
  settings. Where a transcendental sits in a *gate* path (the radiogenic decay curve, the shoreline
  exponent, the boiling-point fit), precompute it as a fixed lookup — the decay on 0.05 Gyr bins, the
  shoreline as an integer power.

**Iteration order.** `world.bodies` is `std::unordered_map`; the pass must walk an explicit sorted
list, as `hard_coded_world.cpp`'s market seeding already does for `centre_ids`.

**Compute is a non-issue** — ~90 flops and ~20 hashes per body, once, at generation. It should not be
quoted as a virtue.

**Code cost is the real number, and it is larger than "one multiplier."** A new sibling pass
`src/world/planetology.{hpp,cpp}` invoked before `generate_body_tiles` (the settled BL-051
architecture) — with `body_profile` becoming the pass's **return value** rather than four literals,
which is the narrow waist. But then: a per-resource deposit weight vector (a **signature change** —
today's `deposit_scalar` is a single float), a new condition and fallback rule in Pass 4's composition
table, the clay/sand/cold-trap gates in `generate_deposits`, and a report surface.
**"The pipeline itself is otherwise unchanged" will not survive implementation.**

**Save format.** No serialiser exists in the repo today, so the cost is zero now and a design
constraint later. Bodies are few, so inline on `body_component` is right — `survey_state` is the
precedent. ~120 bytes plus a ≤64-byte event array per body.

**Tuning surface.** ~40–60 named constants. The only thing that pins them is a committed
`tools/verify/planetology.cpp` harness (per the tool-creation rule, authored *as part of* the item)
running the chain over the ten real solar-system bodies and asserting each lands in its stated
archetype — plus asserting the four prototype bodies' derived `body_profile` matches today's authored
one. **Be honest in the harness comments: the model has more free parameters than calibration bodies,
so a green check means the constants have not *moved*, not that they are right.**

---

## Implementation

**The pass** is `src/world/planetology.{hpp,cpp}` — a body-level sibling pass invoked from
`make_hard_coded_world` *before* `generate_body_tiles`, per BL-051's settled convention. It is a
pure function of `(campaign_seed, body_inputs, planetology_params)`.

**It now derives the `body_profile`** that used to be four hand-authored literals. Checked against
the previous authored values, **23 of 24 fields reproduce exactly** — Kepler is identical on all six,
so the homeworld surface is unchanged. The single divergence is **Cinder's `geological_activity`,
authored `high`, derived `low`**: Mercury is genuinely geologically dead, and the authored value was
flavour. It costs Cinder some mountain and rift cluster seeds.

**The Continents/Drift sibling pass now runs between S3 and the tiles** (landed 2026-07-28,
BL-210 first slice — `run_continents`, `src/world/continents.cpp`, authority `CONTINENTS.md`).
It reads Engine's already-computed `mobile_lid`/`theta` — plate count, drift speed, everything a
consequence, nothing an independent roll — feeds a per-tile height bias into tile Pass 1, and
appends dated `chain_stage::engine` lines to the body's biography (collision → *"porphyry copper
where it persists"*; a stagnant lid drifts as one immobile plate).

**Two hooks into the tile pipeline**, both taking an optional `const planetology_state*` so a null
state reproduces the pre-BL-167 surface bit-for-bit:

- **Pass 4** — `life_stage < land` masks the biotic compositions out, falling back to
  `composition_abiotic`, which mirrors `composition_atmospheric`'s RNG consumption draw-for-draw so
  the two branches stay stream-aligned.
- **Pass 6** — the per-resource `endowment` multiplies the generated deposits, alongside
  `deposit_scalar` and in the same pure post-multiply shape (no RNG draw).

**The ten player decisions.** `planetology_params` carries one knob per decision-bearing stage;
the New World flow walks them in chain order. **Air** and **Legacy** carry *no* decision — the first
is a consequence of earlier choices, the second is the payoff — and the UI says so rather than
inventing a knob.

The bands below are the **measured always-viable spans**, not authored preferences —
`earthlike_corridor` swept each axis and these are the spans where every seed clears the homeworld
gates, widened at the wet end so Earth's own 0.71 ocean fraction stays reachable. Verified against
`planetology.cpp:321-386` on 2026-08-04. *(This table previously read 0.80–1.20 / 3–9 / 0.40–2.00 /
0.70–1.50 / 0.40–1.80 / 0.30–0.80 and 0–1 for oxygenation; every row had moved.)*

| Stage | Knob | `any` band (measured) |
|---|---|---|
| System | `star_mass` | 0.60–1.50 M☉ |
| System | `system_age_gyr` | 2.12–9.02 Gyr, **then capped by the star** |
| System | `metallicity` | 0.30–2.20 |
| Accretion | `home_mass` | 0.66–1.48 M⊕ |
| Air | — *(consequence)* | |
| Engine | `radiogenic` | 0.57–1.73 |
| Water | `home_ocean` | 0.40–0.75 |
| Spark | `abiogenesis_ease` | 0–1 |
| Breath | `oxygenation` | 0.30–0.91 |
| Green | `coal_climate` | 0–1 |
| Legacy | — *(payoff)* | |
| Spend | `drawdown` | 0–0.95 |

**The age band is not free — the star caps it.** Main-sequence lifetime goes as `10 / (M² · √M)`, so
a 1.5 M☉ star lives ~3.6 Gyr, and a system cannot be older than the star lighting it. The cap is
applied after the lean's band is picked (`age_band.hi = min(hi, t_ms)`). If the ask is contradictory
— a bright star *and* an ancient system — the star wins, being the harder physical limit, and the
biography says so rather than the generator silently ignoring the request.

That cap is also what finally gave `star` a job. T5 measured it **inert on all ten tile metrics**,
which this doc elsewhere called "exactly right"; it was not — a knob the player can move that
changes nothing is a dead control. Now a brighter star forces a younger system, a younger system has
a shorter biosphere window, and through the age × radiogenic ridge the pair atlas mapped, that buys
a leaner fossil endowment. Eclipse geometry is its second job.

**`radiogenic` is deliberately independent of `metallicity`** — U and Th come from rare r-process
events, so a metal-rich system is not automatically a geologically active one. Keeping them separate
is both truer and a second free axis.

**The prototype body set is authored once**, in `planetology.cpp`, and reached through
`prototype_body()` / `prototype_body_seed()`. Both `make_hard_coded_world` and the New World
wizard's live `preview_system()` walk that same list with the same seeds, so **the charts a player
decides against cannot drift from the world actually built.**

**Charts** are drawn by `src/ui/charts.{hpp,cpp}`, extracted from the tile-selection deposit graphs
(BL-123) so every chart in the app shares one implementation. At two bars `draw_bars` lays out
pixel-identically to the original, which is what let the tile graphs move onto it.

### The homeworld: preferences, a strict floor, and reject-and-reroll

**Settled 2026-07-22 (Ben).** The first cut guaranteed the homeworld by clamping its inputs to a
hand-picked box. That box was drawn around an answer already known, which is why generated Earths
read as *forced*. Two calls replaced it:

1. **The floor stays strict.** Every homeworld should be recognisably Earth.
2. **A miss is rejected and rerolled, never clamped.** No value is ever silently overridden, so
   every world a player sees genuinely rolled and genuinely passed.

**The measurement came first**, via `tools/verify/planetology_sweep.cpp`, and it found a modelling
error rather than a tuning problem:

> **The homeworld's orbit was pinned at 1 AU while the star's mass varied.** Luminosity goes as
> M^3.5, so rolling a star across 0.6–1.5 M☉ swings instellation across **0.17–4.1** against a viable
> window near **0.34–1.05**. Two thirds of all draws died at the Water gate — 51% Ovens, 16% Snowball
> Locks.

A homeworld sits in its star's habitable zone **by construction, not by luck**. Deriving the orbit
from the star (placed in the *continuously* habitable band, which is ~63% the width of the
instantaneous one) eliminated that failure mode outright:

| | Acceptance | Draws to a viable homeworld |
|---|---|---|
| Orbit pinned at 1 AU | 0.9% | ~110 |
| **Orbit derived from the star** | **77.4%** | **1.29** |

**The strict floor** (`homeworld_viability`): a Cradle that reached civilisation, O₂ in 16–30%
(breathable, above the combustion floor, below fire-suppression), liquid surface water at 40–75%,
275–305 K, arable ≥ 8% of *surface* (`arable_share` already folds in the land fraction), escape
velocity 9.5–13 km/s, at least one primary fuel, and workable iron. Each clause names its reason,
so it can be argued with rather than merely tuned.

> **Drier than Earth by design (Ben, 2026-08-04, NR-049).** The arable clause is mechanically a
> hard ocean cap. `arable_share = land × (0.28 − (O₂ − 0.21) × 0.45) × lid-factor`, so even in
> the best case (O₂ at exactly 21%, mobile lid) the ≥ 8% floor caps ocean fraction at **0.714** —
> 0.4% above Earth's own 0.71. Earth passes only in that corner: the same world with a stagnant
> lid, or at 23% O₂, is rejected. The measured consequence is that homeworlds run ~46–48% land
> against Earth's 29%, and an ocean-heavy homeworld is unreachable by construction. **This stands
> as a playability constraint, not a miscalibration**: the player must have land to build on, and
> this clause is where that requirement lives. Ocean worlds remain generatable (see the
> Waterworld class above) — they are simply never the homeworld.

**Variety survives the floor** — the thing a strict floor risks destroying:

| coal | petroleum | copper | iron | arable | ocean | surface temp |
|---|---|---|---|---|---|---|
| **×6.99** | ×3.22 | ×2.72 | ×1.82 | ×1.85 | ×1.58 | ×1.04 |

*(p05 → p95 across accepted worlds.)* An order of magnitude of coal variation under a strict floor.
Surface temperature pinned to 4% is **correct** — that is the carbonate thermostat doing its job.

### Preferences, not parameters

**Settled 2026-07-22 (Ben):** *"if you have preferences you can find them, but really you don't get
full customization. Just a step more detail than a seeded world."*

The wizard's input is `world_preferences` — eight axes, each a **`lean`** (`any` / `low` / `mid` /
`high`) that narrows a sampling range. The value is rolled from the seed and is **never editable or
displayed**; the moment a player can type `star_mass = 1.0342` the screen is a form again.

`resolve_preferences(prefs, seed)` turns those into concrete parameters, rejecting and rerolling with
the attempt index folded into the draw — so the whole search stays a pure function of the seed.

**Three rounds, on the chain's own A → B → C shape**, replacing ten:

| Round | Chain stages | Preferences |
|---|---|---|
| **A — The System** | System, Accretion, Air, Engine | star, world size, interior, metal |
| **B — Life** | Water, Spark, Breath, Green | ocean, oxygen story, coal basins |
| **C — Inheritance** | Legacy, Spend | drawdown |

Air and Legacy carried no decision, so as standalone screens they were pure interstitials; grouped,
they become their round's consequence and payoff panels. Each round has its own **reroll** —
set leans, roll, read the charts, roll again, commit.

**`interior` deliberately folds age and radiogenic endowment together.** "Old and cold" against
"young and vigorous" is one idea to a player; splitting it into two sliders would be two settings
rather than one preference. **`abiogenesis_ease` is not a preference at all** — rolling "life is
rare" and then rejecting every lifeless homeworld is pure waste, so life on the homeworld is a given.

**No lean is a dead end.** A preference that reads as a choice but almost never yields a viable world
is a lie in the UI. Measured per-lean cost, in mean draws:

```
star 1.24 / 1.24 / 1.24      ocean         1.17 / 1.17 / 1.43
world_size 1.36 / 1.23 / 1.17  oxygen_story  1.86 / 1.17 / 1.15
interior 2.57 / 1.12 / 1.19    coal_basins   1.24 / 1.24 / 1.24
metal 1.24 / 1.24 / 1.24       drawdown      1.24 / 1.24 / 1.24
```

> **⚠ These figures are stale — superseded 2026-08-04, not re-measured here.** Three commits moved
> them: `1a69621` (wizard bands become the measured spans), `fed808`/`fed8db` (arable floor) and
> `5360ce5` (the star's age cap). Re-run `earthlike_lean_trace` before quoting any number in this
> block. Two conclusions did change direction and are worth stating:
>
> - **`interior = high` is now the worst lean, at ~2.97 draws** — not `interior = low` at 2.57.
> - **"`star` costs nothing … which is exactly right" was the wrong reading.** A knob that costs
>   nothing *and* changes nothing is inert, not free; T5 measured `star` at 0.00 on all ten tile
>   metrics and it was treated as a defect. It now caps system age (see § the knob table).

`metal`, `coal_basins` and `drawdown` cost nothing at any lean — they are pure character axes with
no viability consequence. Where a lean *does* cost draws, the cost is **physically honest**: an old,
radiogenically poor world loses its mobile lid, so the second oxygenation never fires and it stalls
at a Boring Billion. The reroll absorbs it rather than the model hiding it.

### Checkpoints — branch decisions as a first-class record (BL-217)

**Settled 2026-08-02.** Preferences narrow a *sampling range* within one round; they never touch
*which branch a checkpoint proposes*. BL-217 (GENERATION_CHECKPOINT_BRANCH_MODEL) generalises the
mechanism so a lean can also narrow the **candidate set** at a genuine branch point — the S5–S8
biological die-offs today, a second checkpoint class once BL-218's historical rupture lands — while
keeping the same reject-and-reroll discipline this doc has used since the homeworld rule.

**What counts as a checkpoint, and what does not.** A checkpoint is a point where the chain's
*outcome distribution genuinely branches* — two runs from the same prior state can diverge into
materially different worlds. It is **not** every point where something narratively interesting
happens. An interesting event that does not change what the world *can become* is a history-log
entry (BL-208, not yet built), not a checkpoint. Applying that rule: the S5–S8 mass-extinction
die-offs qualify (Sterile vs. Microbial vs. Mat World are different worlds); a settlement-stage
collapse/war/revolution will qualify (BL-218's job); a body's day-length or axial tilt rolling one
way or another does not, because nothing downstream reads it as a fork. Exactly **two** checkpoint
classes exist today — **biological mass-extinction** (built, this item) and **historical rupture**
(named, built by BL-218) — and no third is needed before those two land.

**The record.** `checkpoint_record { stage_id, branch_taken, seed_used, viability_result }`
(`src/world/planetology.hpp`), held in an **append-only, ordered** `planetology_state::checkpoints`
list. No random-access map keyed by stage, and no mutable last-branch-wins field — either would
make a later move into BL-208's eventual world-history-log a conversion project instead of a move.
`seed_used` is mandatory: a branch that cannot be replayed in isolation from its own seed is not
debuggable.

**A lean is an eligibility filter, never a weight.** It only removes candidates from the proposal
set (`checkpoint_branch::eligible`); the choice among the survivors stays the existing **uniform**
reject-and-reroll — the same discipline § Preferences, not parameters already documents for
sampling ranges. So a lean can make an outcome *impossible* and can never make one *more likely by a
tunable amount*: there is no slider to expose, no weight to tune. **Corollary, built in rather than
discovered:** if a lean's filter excludes every candidate, that is itself a viability failure and the
whole checkpoint rerolls — it must never silently fall back to an ineligible branch. Without this,
a lean would stop mattering in exactly the cases where it was most specific, which is the worst
failure available: the player's preference *appears* honoured and is not.

**One mechanism, per-checkpoint-class floor.** `resolve_checkpoint` (`src/world/planetology.hpp`,
a template so it stays class-agnostic) takes a `propose` callback (builds this attempt's candidate
list), an `apply` callback (mutates caller-owned state), and a `floor_ok` predicate — the
checkpoint class's own viability floor, evaluated *after* `apply`, reroll on false. This generalises
`homeworld_viability` exactly: one global floor cannot work, because a biological extinction's
question ("can this biosphere still reach sapience?") and a historical rupture's ("is there still a
civilisation left to industrialise?") are not the same question, so each checkpoint class supplies
its own — but the *mechanism* invoking them is one function, uniform across classes. The homeworld
rule still governs throughout: a floor rejects and rerolls; it never edits a result into viability.

**The S5–S8 retrofit is legibility only.** The biology itself is unchanged — `run_planetology` still
computes exactly the same abiogenesis/GOE/NOE/land/fire decisions it always did. What BL-217 adds is
a `checkpoint_record` push alongside each of those decisions (Spark's abiogenesis-or-not, Breath's
GOE and NOE, Green's land colonisation and fire threshold), so the branch the biology already takes
is now legible as data rather than only as a `died_at`/`archetype` read. No real lean filters a
biology candidate today — the eligibility-filter half of the mechanism is proven by a synthetic
checkpoint class in the harness, ready for BL-218 to be the first real second user.

**Nothing here hardcodes "four biology stages."** `resolve_checkpoint` takes a `chain_stage` value
to tag its record, but never enumerates which stages exist or how many checkpoints a body has;
BL-218 registers its own checkpoint (candidates, apply, floor) at the settlement stage without
touching this item's code, which is the whole point of building the mechanism ahead of its second
user.

### Verification

`tools/verify/planetology_harness.cpp`, auto-registered as a CTest. The groups this doc owns (the
harness also carries R9–R11, which belong to the endemic-goods and market items, and reserves R12
for BL-209's molecular trace):

| | Asserts |
|---|---|
| R1 | Determinism — same seed identical, different seed different |
| R2 | The four bodies land in their stated archetypes and derive a usable profile |
| R3 | **The B→C joint** — no life, no coal; no aqueous history, no clay; airless bodies keep polar ice |
| R4 | **The iron/coal antagonism**, with both ends of the dial still passing every gate |
| R5 | The homeworld corridor holds across the whole knob space (100/100 combinations) |
| R6 | **Wizard stability** — a decision at stage N never rewrites the history of a stage before N |
| R7 | Every new knob demonstrably moves its own outcome |
| R8 | **Preferences and reroll** — resolution is pure in (preferences, seed); a reroll redraws its own round and leaves earlier ones alone; **every** resolved homeworld clears the strict floor across 400 preference combinations; a dimmer star pulls the orbit inward |
| R13 | **The checkpoint model** (BL-217) — same seed → identical `checkpoints`, different seed → different; the S5–S8 checkpoints mirror what `died_at`/`archetype` already encode; a synthetic checkpoint class proves an all-ineligible candidate set forces a reroll rather than a silent fallback |
| R14 | **The dated timestamp** (BL-220) — the conversions from Gya and from a calendar year, every display band *and both edges of every band*, total oldest-first ordering, and a historical line interleaving between deep time and the epoch |

R6 is what makes the wizard's Back/Continue safe, and R8 is what makes the rejection sampling a
generator rather than a slot machine. Both are asserted, not assumed.

A second harness, **`tools/verify/planetology_sweep.cpp`**, measures rather than checks: acceptance
rate, per-lean cost (so a dead preference shows up as a number rather than a mysteriously slow
reroll), and the endowment spread across accepted worlds.

**The harness earned its keep immediately** — it caught five real defects before the feature was ever
seen: Selene's tidal heating overflowed to ~3×10¹¹ (raw AU fed into an a^-7.5 term), Cinder was
mis-banded a whole temperature class by a wet-planet albedo on bare rock, Selene grew clay it cannot
have, Pallas reported 0 K because it exited before instellation was computed, and the iron endowment
saturated its clamp so both ends of the oxygenation dial returned the same number.

**Known dormancy.** The biotic terrain mask is built and unit-tested but has **no body to fire on**
in the shipped set: it only bites on a world that *has* an atmosphere yet never reached land, and
Kepler is the only atmospheric body and always lives. It is exercised by a synthetic small-wet-world
case in the harness. This is expected — one living world is the intended shape (Ben, 2026-07-21).

---

## Open calls — Ben's, not settled

1. ~~**Scope of the first cut.**~~ **Settled 2026-07-21 — the full chain shipped**, including the
   terrain mask and the biogenic deposit gates, not atmosphere-only. Original reasoning kept: This doc's earlier recommendation was atmosphere-only. But
   atmosphere-only produces **no visible difference on the four shipping bodies** — three fail at
   gate one and the fourth is authored — so it would be invisible against a feature billed as a first
   impression. *The minimum that produces a visible difference is the biotic terrain mask plus the
   biogenic deposit gates,* which is already past atmosphere-only.
2. ~~**The dead-code problem.**~~ **Settled 2026-07-21 (Ben): 'One planet with life is much better for
   what I'm imagining.'** The unreachable rungs are accepted — the system is meant to read as one
   living world among dead ones. Original concern kept for the record: At current scope the feature produces **one
   interesting body**. Cinder, Selene and Pallas all terminate at S1–S3; Kepler is the constrained
   homeworld. Six of thirteen archetypes and the entire S5–S8 ladder — including the iron/coal
   antagonism that justifies the model — are unreachable until `world_params::body_count` is
   activated. *Either the body count grows first, or BL-167 ships scoped down.*
3. **Resource-list expansion.** Five of the model's chemical chains terminate in resources Io has no
   slot for: limestone, bauxite, salt/potash, phosphate, uranium. **Limestone has the strongest case**
   — it is the flux for iron smelting and the binder for all masonry, and without it the carbonate
   half of the thermostat has nowhere to go. Each addition shifts `resource_count` and every
   `std::array` width in the model.
4. ~~**Provinces or global scalars.**~~ **Closed — ore provinces landed 2026-08-04 (`613b78a`).**
   The concern was right: a body-level "1.4× copper" smears evenly and reads as noise, where real ore
   is province-scale. Tile Pass 6 now seeds `ore_province` records (`provinces_for`, `province_field`
   in `tile_generation.cpp`) and redistributes a large share of each world's total into 2–3 of them:
   copper 65%, petroleum 60%, iron 55%, coal 45%.

   One implementation note worth keeping, because the measurement caught it: **conservation is over
   the resource's *bearing set*, not over all land.** Conserving across land drained petroleum by 47%,
   since most land bears none of it — the province took its share from tiles that never had any.
5. ~~**`deposit_scalar` ownership.**~~ **Settled in implementation (marked resolved 2026-07-31):**
   the levers are **pure post-multiplies** in tile Pass 6, and none draws RNG, so each is bit-exact
   at its identity value and sparse/lean/standard stays interpretable as a flat tier over whatever
   the endowment shaped. **There are now three, not two** (updated 2026-08-04): BL-114's
   `deposit_scalar`, then this pass's per-resource `endowment`, then the **ore-province field**
   (open call 4). The province multiply is skipped entirely when `pl == nullptr`, which is what
   keeps the identity contract intact for bodies with no planetology. See `generate_body_tiles`
   (`tile_generation.cpp`) and `TILE_GENERATION.md` § Post-multiplies. Original concern kept:
   BL-114's per-body abundance multiplier already exists with the same semantics this pass
   claims; two independent levers multiplying risked making the tiers uninterpretable.
6. ~~**Do the four authored bodies become derived?**~~ **Settled — option (a) shipped:** authored
   inputs, derived outputs, with the old values as regression assertions. 23 of 24 fields reproduce;
   Cinder's geology is the one divergence. Original options kept: (a) authored inputs / derived outputs, with today's
   values as regression assertions — cheapest, turns the four bodies into free tests; (b) accept
   divergence and re-baseline every capture; (c) generate history and endowment only, leave profiles
   hand-written. *Note the real regression risk:* Selene's `polar_frozen` + `cold` is load-bearing in a
   documented comment in `band_for_row` (`tile_generation.cpp`), and a naive derivation would
   classify it temperate.
7. **Survey-gated or always visible?** `DISCOVERY.md`'s geographic fog means a body starts with only
   type, orbit and grid size known. Gating the timeline behind the survey gives the discovery system a
   real payload and makes the first impression a *sequence* — but the homeworld must show its full
   history at campaign start, because that is literally the first thing the player sees.
8. **Report surface.** A Tile/Generation Ledger section (free, already has a body selector, and
   `GENERATION_LEDGER.md` already designs a "profile echo") versus a dedicated Planetology ledger (a
   nav-rail slot and the toggle rule, but better framing for a pillar).
9. ~~**Abiogenesis: preconditions or probability?**~~ **Settled — it became a player dial**
   (`abiogenesis_ease`), so both positions are reachable per campaign rather than baked in.
   Original framing kept: Gating on preconditions with near-certain firing
   makes microbial worlds common; a low probability makes living worlds rare and precious. **This is a
   pure game-design dial and the doc should label it as one.**
10. **Fire threshold: hard gate or soft penalty?** A hard gate produces the memorable "green world that
    cannot burn"; a soft penalty is more forgiving and less legible. It can only ever fire on
    non-homeworld bodies, since humans need pO₂ ≥ ~0.16 to breathe.
11. ~~**Whether the homeworld's joint iron/fuel floor is stated or hidden.**~~ **Settled 2026-07-22 —
    STATED.** The floor is an explicit predicate (homeworld_viability), enforced by reject-and-reroll
    rather than by a hidden clamp, and the wizard reports the attempt count when it exceeds one.
    Original reasoning kept: A world with neither fuel
    nor iron kills the premise at turn one, so *some* joint constraint is needed. **Players forgive a
    stated constraint and never forgive a discovered one.**

---

## Known weaknesses of this model

Recorded so nobody later mistakes design choices for derived physics.

- **The endowment framing is physically contrived, and the docs should say so plainly.** Oxygen level,
  ocean fraction, arable land and coal reserves are *not* independently tunable dials — they
  co-evolved through exactly the couplings modelled above. Holding "humans arise" fixed while varying
  them is a game contrivance.
- **Several load-bearing thresholds sit on contested science.** The ozone/land gate (1D vs 3D models
  differ by up to 4.68×); the **BIF oxidant** — genuinely unresolved between cyanobacterial O₂,
  anoxygenic photoferrotrophy (which needs no free oxygen at all) and abiotic UV photo-oxidation,
  *and it decides whether BIF gates on life or on oxygen*, which is what the iron/coal antagonism
  hangs on; whether liquid water is strictly required for mobile-lid tectonics (a modelling
  convenience that reproduces Venus, not an established result); the fire minimum; and the abiogenesis
  probability. **The report screen must not imply more certainty than exists.**
- ~~**The homeworld corridor's width is unmeasured.**~~ **Measured 2026-08-04.** The point stood —
  for a seeded generator the measurement *is* part of the deliverable — so it was built:
  `earthlike_corridor` sweeps each axis (65 steps × 128 seeds), with `earthlike_pairs`,
  `earthlike_tile_census` and `earthlike_lean_trace` alongside. The corridor is now the *source* of
  the wizard's `any` bands rather than an open question about them.

  The finding that changed the model: **the sampling bands, not the viability floor, are the
  specification of Earth.** Ten of the fourteen floor clauses never fire. The floor was doing much
  less work than it appeared to, and the bands were doing all of it.
- **The depletion inversion is a design assertion dressed as a finding.** Historically suggestive, but
  the constants are free parameters chosen for tension.
- **The pass hands nothing to nation or corporation generation — now half-closed (2026-07-30,
  BL-221 pre-national ladder).** The nation half is closed: `run_history_ladder` consumes the
  planetology state (`life_stage` peak, `arable_share`, `endemics`) and
  `nation_params_from_ladder` drives the seed budget from it, so the political map now knows the
  world has a biography (`NATION_GENERATION.md` § Pass 0). The **corporation half stays open**:
  corporate focus and asset mix still read nothing from the chain — that connection belongs to
  BL-210 (oral-history pivot, design-owed).
- **Shadow Empire's own cautionary bug is only half-fixed.** Its life→oil rule with no life→ore
  counterweight produces a documented, still-current imbalance ("oil is always abundant to an extreme,
  metal is always extremely rare"). BIF supplies the counterweight — but the BIF window *closes* on
  oxygenation success, so a Cradle homeworld is iron-poorer than a Mat World. Scientifically correct;
  possibly economically annoying; not yet balance-tested.

---

## Relationship to other docs

- **`TILE_GENERATION.md`** — Planetology's output *is* the `body_profile` input to the six-pass
  pipeline (derived, no longer authored). It also **adds** a composition mask in Pass 4 and the
  endowment post-multiply in Pass 6.
- **`CONTINENTS.md`** — the plate-drift sibling pass between S3 Engine and the tiles; consumes
  `mobile_lid`/`theta`, appends `chain_stage::engine` biography lines (see § Implementation).
- **`GENERATION_STRATEGY.md`** — records Planetology as the first stage in generation ordering and the
  sibling-pass architecture convention (BL-051).
- **`RESOURCES.md` / `PRODUCTION.md`** — the endowment mapping above is the seam; § Open calls 3 is a
  potential expansion of the resource list.
- **[[BL-114]]** — `deposit_scalar` ownership must be reconciled (§ Open calls 5).
- **[[BL-170]]** — rivers already encode the bulk-transport argument S8 arrives at independently.
- **[[BL-166]]** / **[[BL-168]]** — habitability and arable share are downstream of S7/S8.
- **`DISCOVERY.md`** — the survey fog is the natural gate on the planet report (§ Open calls 7).
- **[[BL-217]]** (§ Checkpoints — branch decisions as a first-class record) — the checkpoint/branch/
  eligibility-filter foundation this session builds; the S5–S8 mass-extinction retrofit is its first
  checkpoint class.
- **[[BL-218]]** — the settlement-stage historical-rupture checkpoint, meant to register as this
  mechanism's second class without changing BL-217's code.
- **[[BL-208]]** (world-history-log, design-owed, v0.3.0) — `checkpoint_record`'s append-only,
  ordered shape is deliberately identical to this future log, so migrating checkpoints into it is a
  move, not a rewrite, once it lands.
