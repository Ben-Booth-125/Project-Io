# Project Io — Development Practices

This document defines the coding standards, documentation conventions, and testing approach for Project Io. Apply these consistently across all code written or reviewed. Where code deviates from these standards, note it and suggest a correction — but do not refuse to proceed or treat it as a blocker.

---

## Testing

The project uses **Catch2** for unit testing.

Tests are written alongside the layer they cover, not deferred to the end. Each layer in `development/INITIAL_INSTRUCTIONS.md` should have tests for its core logic before the next layer begins.

### What to test

Focus tests on the simulation's pure logic — the parts that transform state deterministically:

- Price resolution given a supply/demand ratio
- Extraction yield given tile properties and workforce scalar
- Convoy progress accumulation and completion evaluation at tick boundary
- Budget arithmetic (revenue, outgoings, running balance)

Do not test rendering, ImGui panels, or SDL3 platform behaviour. These are not unit-testable in a meaningful way and should be validated by observation.

### How Claude handles tests

Claude will **suggest tests** when writing or reviewing logic, but will not write test files unprompted. When suggesting tests, Claude names the cases worth covering and explains why — the developer writes them. If asked to write a test directly, Claude will do so using Catch2 syntax.

### Catch2 conventions

```cpp
#include <catch2/catch_test_macros.hpp>

TEST_CASE("market price resolves from supply/demand ratio", "[market]") {
    SECTION("excess supply drives price below base") { ... }
    SECTION("excess demand drives price above base") { ... }
}
```

- Tag each test with the system it covers in square brackets: `[market]`, `[extraction]`, `[supply]`, `[budget]`
- One `TEST_CASE` per logical behaviour; use `SECTION` for variations
- Test names are plain English descriptions of what the code should do, not how it does it

### Visual verification (rendering / lenses)

Rendering is *not* unit-tested (see above) but it **is** verifiable by observation,
and that observation is automated. The harness runs the real app in a headless,
deterministic capture mode and writes PNGs that a human — or Claude, via the Read
tool — inspects against the requirement.

```
ProjectIo --verify scripts/verify/<name>.lua
```

- **Deterministic:** fixed window size, seeded world (`make_hard_coded_world`), sim
  paused — so a captured frame is reproducible.
- **Driver:** the script drives view and overlay state through the `verify` Lua API
  (`goto_surface`, `set_overlay`, `set_zoom`, `set_pan`, `add_pan`, `capture`,
  `log_buildings`) — direct state manipulation, no synthetic input.
- **Capture:** `capture("name")` renders one frame and writes `screenshots/name.png`
  (in-app `SDL_RenderReadPixels` → `write_png_rgba`; nothing leaves the window).
- **Output:** PNGs for inspection. Golden-image diffing is not yet built (deferred).

This is the standard tool for the `visual` verification class in
[`req/REQUIREMENTS.md`](req/REQUIREMENTS.md). When a requirement's verification is
`visual` and no other tool fits, author (or extend) a `scripts/verify/*.lua` script
and capture the frames rather than deferring to a manual human check. Entry points:
`app::run_verify` (`src/core/app.cpp`), `write_png_rgba` (`src/core/png_writer.cpp`).
Full keyboard navigation (a player-facing manual drive surface) is planned
separately — see TODO § Canvas.

---

## Code style

### Naming

`snake_case` throughout — types, functions, variables, files, and namespaces.

Private member variables use an `m_` prefix: `m_window`, `m_sim_loop`. Applied consistently to all class members.

```cpp
// Correct
struct market_state { ... };
float resolve_price(float supply, float demand, float base_price);
int workforce_assigned;

// Incorrect
struct MarketState { ... };
float resolvePrice(float supply, float demand, float basePrice);
int workforceAssigned;
```

### General conventions

- Prefer explicit types over `auto` where the type is not immediately obvious from context
- Keep functions short and single-purpose; if a function needs a comment to explain what it does (not why), consider splitting it
- No magic numbers — name constants, even simple ones
- Use `const` by default; only drop it when mutation is required

### File structure

- One type or closely related group of types per header
- Implementation in a matching `.cpp` file
- Headers use `#pragma once`

---

## Documentation

All public interfaces are documented with **Doxygen-style comments**. This applies to structs, public member functions, and any free function callable from outside its translation unit.

```cpp
/// Resolves the market price for a good at the economy tick boundary.
///
/// Price is derived from the ratio of local supply to demand, scaled
/// against the global rarity base. A ratio above 1.0 (oversupply) drives
/// price down; below 1.0 (undersupply) drives it up.
///
/// @param supply     Current local supply quantity.
/// @param demand     Current local demand quantity.
/// @param base_price Global rarity-derived price floor.
/// @return           Resolved price for this tick.
float resolve_price(float supply, float demand, float base_price);
```

### Inline comments

Use inline comments to explain **why**, not what. If the code requires a comment to explain what it is doing, rewrite the code first.

```cpp
// Correct — explains a non-obvious decision
// Progress is clamped to 1.0 here rather than at completion evaluation
// to keep the render interpolation from overshooting on a slow tick.
progress = std::min(progress + delta, 1.0f);

// Incorrect — restates the code
// Add delta to progress
progress += delta;
```

ImGui panel code is an exception — brief section comments are encouraged there since panels serve as the functional specification for the production UI and will be read as reference material.

---

## Lua files

Data definition files in Lua follow the same spirit as the C++ standards:

- `snake_case` for all keys and function names
- Each file covers one domain (one building type catalogue, one resource table)
- A header comment states what the file defines and what system consumes it