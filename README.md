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

## Latest release — v0.0.7

Interactive & legible: the economy becomes something the player *drives*. Adds **player
construction** (validated placement, resource build cost, recipe & workforce control),
**population centres** with habitability→workforce feedback, **selectable entity markers** with
lens-contextual hover cards and the pinned Selection panel, the placement-suitability surface, a
full **hotkey system** (F1 cheat-sheet), balance/market **trend plots**, and a **cross-platform
build** (Linux primary, Windows CI). v0.0.8 (Discovery & intelligence) is in progress. Full
history in [`CHANGELOG.md`](CHANGELOG.md).

## Versioning & releases

Version history is tracked with annotated git tags (`vX.Y.Z`); each tag is a recoverable
release point and `CHANGELOG.md` carries its summary. Releases are produced by the **Cut**
process — see [`docs/development/DEVELOPMENT_PRACTICES.md`](docs/development/DEVELOPMENT_PRACTICES.md)
§ Cutting a release.
