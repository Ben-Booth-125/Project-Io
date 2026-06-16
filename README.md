# Project Io

Project Io is a near-future, space-based 4X grand strategy game. You control a **corporation**
— not a nation — competing across an Earth-like solar system through resource extraction, trade,
and military conflict. Unlike a nation-state, a corporation begins with no territory and no
standing force: every asset must justify itself through economic or strategic return, and the
corporation persists only as long as it holds at least one asset.

The game progresses through **Eras** — beginning on the home planet (Era 0, Terrestrial) and
opening up the solar system once the gate to Era 1 (Early Space) is met. Play is structured
around three nested **canvases** (Solar → Circumplanetary → Planetary) and a tick-based economy
where supply, demand, and prices resolve per body.

The project is in **prototype phase**, solo-developed in C++ with Lua scripting. Design and
technical documentation lives under [`docs/`](docs/) — start with
[`docs/CONCEPT.md`](docs/CONCEPT.md) and [`docs/SYSTEMS.md`](docs/SYSTEMS.md).

## Latest release — v0.0.5

Layer 4 foundations: the reusable placement-rules seam, a multi-tick economy-stability harness,
the workforce pool (step 1), and the Layer 4 UI scaffold. Adds the Resource / Market / Population /
Scarcity **map lenses**, tile-centred markets, player sell-orders, golden-image visual
verification, and world-generation fixes (full land claim, lean corporation holdings). Full
history in [`CHANGELOG.md`](CHANGELOG.md).

## Versioning & releases

Version history is tracked with annotated git tags (`vX.Y.Z`); each tag is a recoverable
release point and `CHANGELOG.md` carries its summary. Releases are produced by the **Cut**
process — see [`docs/development/DEVELOPMENT_PRACTICES.md`](docs/development/DEVELOPMENT_PRACTICES.md)
§ Cutting a release.
