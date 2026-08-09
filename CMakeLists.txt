cmake_minimum_required(VERSION 3.25)
project(ProjectIo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# Default build type (BL-255)
# ---------------------------------------------------------------------------
# Multi-config generators (Visual Studio, Ninja Multi-Config) are unaffected
# by this: check.bat/build_app.bat pass --config Release explicitly and the
# generator carries every config's flags simultaneously.
#
# A single-config generator (Ninja, Unix Makefiles — every Linux/WSL/cloud
# configure that doesn't pass -DCMAKE_BUILD_TYPE) is different: with no
# CMAKE_BUILD_TYPE set, CMake adds NO optimisation flags at all (effectively
# -O0), and a later --config is silently ignored because there is only ever
# one config. The econ_stability timing assertions (BL-250) were measured at
# -O2 with real but thin margins, so an accidental -O0 configure fails them
# for build-configuration reasons, not economy regressions.
#
# Set a default only when the generator is single-config AND the user has
# not already chosen a build type — never override an explicit choice
# (including an explicit Debug, which stays honoured for its own tradeoffs).
get_property(IO_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT IO_IS_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# ---------------------------------------------------------------------------
# Dependency acquisition posture (BL-302)
# ---------------------------------------------------------------------------
# Every dependency below (SDL3, Lua, sol2, ImGui) is fetched at CONFIGURE time
# — ~120 MB of tarballs from codeload.github.com. BL-302 was filed when a fresh
# configure on Windows could not download SDL3 at all (a schannel TLS
# revocation failure), which stops the build before a single file compiles.
#
# Posture chosen: seed a shared per-checkout SOURCE cache. Not vendoring the
# tarballs (120 MB into git to solve a fault seen on one machine), and not a
# machine-specific TLS fix (not reproducible, not portable).
#
# Measured on Linux 2026-08-09, and this is why the cache shares *sources only*
# rather than a whole shared FETCHCONTENT_BASE_DIR:
#
#   * From-cold configure, empty cache, network live:  SUCCEEDS in ~74 s.
#     The schannel fault is Windows-specific and does NOT reproduce here.
#   * Shared FETCHCONTENT_BASE_DIR across two build trees: HARD FAILS.
#     The <dep>-subbuild dir carries a generator-locked CMakeCache, so a
#     second tree with a different generator dies with "generator : Ninja
#     does not match the generator used previously: Unix Makefiles".
#     That rules the whole-base-dir variant out — this checkout has
#     build_gen / build_vs / build_linux / build_rel side by side.
#   * Shared FETCHCONTENT_SOURCE_DIR_<dep>: WORKS. Different generator,
#     FETCHCONTENT_FULLY_DISCONNECTED=ON, zero downloads, exit 0. Each build
#     tree still owns its own <dep>-build, so nothing is shared that a
#     generator can lock.
#
# Behaviour:
#   * No cache dir present  -> nothing happens; each build dir downloads as
#                              before. This is the default on a fresh clone.
#   * Cache dir present     -> a COLD build tree takes its sources from there
#                              and never touches the network.
#   * Build tree already populated -> left completely alone (build_linux and
#                              friends keep the sources they already have; no
#                              re-download, no forced SDL rebuild).
#   * -DFETCHCONTENT_SOURCE_DIR_<dep>=... on the command line always wins.
#
# Seed the cache once (see DEVELOPMENT_PRACTICES.md § Dependency acquisition):
#   cmake -S . -B <throwaway> -DFETCHCONTENT_BASE_DIR="$PWD/_deps_cache"
#
# IO_DEPS_CACHE (env var) overrides the location. A sub-agent worktree points
# it at the main checkout's cache so a fresh worktree configures offline:
#   export IO_DEPS_CACHE=/path/to/Project-Io/_deps_cache
set(IO_DEPS_CACHE "$ENV{IO_DEPS_CACHE}")
if(NOT IO_DEPS_CACHE)
    set(IO_DEPS_CACHE "${CMAKE_CURRENT_SOURCE_DIR}/_deps_cache")
endif()

# Where THIS build tree keeps its own populated deps. FETCHCONTENT_BASE_DIR is
# not defaulted until include(FetchContent) below, so mirror its default here.
if(FETCHCONTENT_BASE_DIR)
    set(IO_LOCAL_DEPS "${FETCHCONTENT_BASE_DIR}")
else()
    set(IO_LOCAL_DEPS "${CMAKE_CURRENT_BINARY_DIR}/_deps")
endif()

# "<FetchContent name>|<a file that proves the source tree is really there>".
# A bare directory test would accept a half-extracted cache.
set(IO_DEPS_SEEDED "")
set(IO_DEPS_COLD "")
foreach(dep_spec IN ITEMS
        "sdl3|CMakeLists.txt"
        "lua_src|lua.h"
        "sol2_src|include/sol/sol.hpp"
        "imgui_src|imgui.cpp")
    string(REPLACE "|" ";" dep_parts "${dep_spec}")
    list(GET dep_parts 0 dep_name)
    list(GET dep_parts 1 dep_marker)

    string(TOUPPER "${dep_name}" dep_upper)
    set(dep_var "FETCHCONTENT_SOURCE_DIR_${dep_upper}")
    set(dep_cached_src "${IO_DEPS_CACHE}/${dep_name}-src")

    if(NOT "${${dep_var}}" STREQUAL "")
        # Explicitly pointed somewhere by the caller — never second-guess it.
    elseif(IS_DIRECTORY "${IO_LOCAL_DEPS}/${dep_name}-src")
        # This build tree already has it; leave warm trees untouched.
    elseif(EXISTS "${dep_cached_src}/${dep_marker}")
        set(${dep_var} "${dep_cached_src}" CACHE PATH
            "Seeded from the shared Io dependency cache (BL-302)")
        list(APPEND IO_DEPS_SEEDED "${dep_name}")
    else()
        list(APPEND IO_DEPS_COLD "${dep_name}")
    endif()
endforeach()

# Say cold-or-warm out loud. A green local build dir hides this failure class
# completely (BL-302) — the configure line is where it becomes visible.
if(IO_DEPS_SEEDED)
    message(STATUS "Deps: seeded from ${IO_DEPS_CACHE} -> ${IO_DEPS_SEEDED}")
endif()
if(IO_DEPS_COLD)
    message(STATUS "Deps: COLD -> ${IO_DEPS_COLD}")
    message(STATUS "      these download at configure time (~120 MB total);"
                   " no cache at ${IO_DEPS_CACHE}")
endif()

include(FetchContent)

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
FetchContent_Declare(
    SDL3
    URL https://codeload.github.com/libsdl-org/SDL/tar.gz/refs/tags/release-3.2.0
)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# ---------------------------------------------------------------------------
# Lua 5.4  (built as a static lib from source)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    lua_src
    URL https://codeload.github.com/lua/lua/tar.gz/refs/tags/v5.4.7
)
FetchContent_GetProperties(lua_src)
if(NOT lua_src_POPULATED)
    FetchContent_Populate(lua_src)
    file(GLOB LUA_SOURCES "${lua_src_SOURCE_DIR}/*.c")
    list(REMOVE_ITEM LUA_SOURCES "${lua_src_SOURCE_DIR}/lua.c")    # standalone interpreter
    list(REMOVE_ITEM LUA_SOURCES "${lua_src_SOURCE_DIR}/luac.c")  # standalone compiler
    list(REMOVE_ITEM LUA_SOURCES "${lua_src_SOURCE_DIR}/onelua.c") # single-file amalgamation (includes lua.c)
    add_library(lua54 STATIC ${LUA_SOURCES})
    target_include_directories(lua54 PUBLIC "${lua_src_SOURCE_DIR}")
    if(UNIX)
        target_compile_definitions(lua54 PRIVATE LUA_USE_POSIX)
    endif()
endif()

# ---------------------------------------------------------------------------
# sol2  (header-only — skip its CMake so it does not search for system Lua)
# ---------------------------------------------------------------------------
# v3.5.0 (not the older v3.3.0): GCC 14+ and Clang 19+ parse template bodies
# strictly and reject sol2 v3.3.0's optional<T&>::emplace, which calls a
# `construct` member that specialisation lacks ("has no member named construct"
# [-Wtemplate-body]). Fixed upstream in sol2 PR #1606 (merged Jul 2024) and
# released in v3.5.0, so a fresh build on a modern toolchain compiles without
# pinning an older compiler. Our usage is the stable core (sol::state/table/
# optional, safe_script_file) — unchanged across v3.3.0 → v3.5.0.
FetchContent_Declare(
    sol2_src
    URL https://codeload.github.com/ThePhD/sol2/tar.gz/refs/tags/v3.5.0
)
FetchContent_GetProperties(sol2_src)
if(NOT sol2_src_POPULATED)
    FetchContent_Populate(sol2_src)
endif()

# ---------------------------------------------------------------------------
# Dear ImGui  (SDL3 + SDLRenderer3 backends)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    imgui_src
    URL https://codeload.github.com/ocornut/imgui/tar.gz/refs/tags/v1.91.6
)
FetchContent_GetProperties(imgui_src)
if(NOT imgui_src_POPULATED)
    FetchContent_Populate(imgui_src)
    add_library(imgui STATIC
        "${imgui_src_SOURCE_DIR}/imgui.cpp"
        "${imgui_src_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_src_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_src_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
        "${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
    )
    target_include_directories(imgui PUBLIC
        "${imgui_src_SOURCE_DIR}"
        "${imgui_src_SOURCE_DIR}/backends"
    )
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
endif()

# ---------------------------------------------------------------------------
# Game executable
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(ProjectIo ${GAME_SOURCES})

target_include_directories(ProjectIo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)

target_link_libraries(ProjectIo PRIVATE
    SDL3::SDL3
    lua54
    imgui
)

# IMGUI_DEFINE_MATH_OPERATORS must be visible before imgui.h in every TU.
# Since the PCH includes imgui.h, per-file #defines are too late — pass it
# on the compiler command line so it precedes any header parsing.
target_compile_definitions(ProjectIo PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

# ---------------------------------------------------------------------------
# Compiler warnings (BL-101) — advisory diagnostics on OWNED targets only
# ---------------------------------------------------------------------------
# -Wall/-Wextra (GNU/Clang) or /W4 (MSVC), applied to ProjectIo and the
# verification harnesses (below) — never the fetched SDL3/Lua/sol2/ImGui libs,
# whose headers would flood the output. NOT -Werror yet: promote to a hard gate
# in CI once the surfaced warnings are cleaned. IO_WARNING_FLAGS is reused by the
# harness targets. See docs/development/DEVELOPMENT_PRACTICES.md.
set(IO_WARNING_FLAGS
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
target_compile_options(ProjectIo PRIVATE ${IO_WARNING_FLAGS})

# ---------------------------------------------------------------------------
# Precompiled header
# Covers headers present in 5+ TUs. Excluded: sol2 (3 TUs, very slow to
# compile even once) and project-internal headers (invalidate PCH on edit).
# ---------------------------------------------------------------------------
target_precompile_headers(ProjectIo PRIVATE
    # Standard library
    <algorithm>
    <array>
    <cmath>
    <cstddef>
    <cstdint>
    <cstdio>
    <limits>
    <random>
    <string>
    <unordered_map>
    <utility>
    <vector>
    # Dear ImGui (included in 14 of 45 TUs — largest single parsing cost)
    <imgui.h>
)

# Copy scripts directory next to the executable after each build
add_custom_command(TARGET ProjectIo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts"
        "$<TARGET_FILE_DIR:ProjectIo>/scripts"
    COMMENT "Copying scripts to output directory"
)

# Copy assets directory (bundled fonts, etc.) next to the executable after each
# build, mirroring the scripts copy so cwd-relative asset loads — e.g. the
# bundled assets/fonts/DejaVuSans.ttf in ui::load_ui_font — resolve on every OS.
add_custom_command(TARGET ProjectIo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:ProjectIo>/assets"
    COMMENT "Copying assets to output directory"
)

# Copy SDL3.dll on Windows (shared build)
if(WIN32)
    add_custom_command(TARGET ProjectIo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:ProjectIo>
            $<TARGET_FILE_DIR:ProjectIo>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs"
    )
endif()

# ---------------------------------------------------------------------------
# econ_bankruptcy is NOT declared here — it is built by the generic harness
# batch at the foot of this file, against the whole ${IO_WORLD_SOURCES} superset.
#
# It used to carry a hand-picked source list, and that list rotted: every time
# economy_system.cpp gained a cross-TU call, this target had to be patched to
# match. It was patched once for BL-079 (estimate_building_profit) and then NOT
# patched for BL-202, which added run_corp_strategic_step — so the target failed
# to link with an undefined reference, and because the `if(NOT TARGET ...)` guard
# in the batch below skips any target already declared, the self-maintaining path
# could not rescue it.
#
# Found 2026-07-30 by the first full `cmake --build` on Linux; Windows never
# caught it because the day-to-day loop builds the ProjectIo target alone. The
# other hand-declared harnesses above the batch carry the same latent hazard.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Headless survey-system harness (BL-067; no SDL / Lua / ImGui)
# ---------------------------------------------------------------------------
add_executable(survey_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/survey_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/survey_system.cpp"
)

target_include_directories(survey_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(survey_harness PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# Headless visibility-model harness (BL-068; no SDL / Lua / ImGui)
# ---------------------------------------------------------------------------
# Exercises the read-side ownership accessors (owner_corp_of / is_player_owned).
add_executable(visibility_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/visibility_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world.cpp"
)

target_include_directories(visibility_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(visibility_harness PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# Headless workforce-efficiency curve harness (BL-069; no SDL / Lua / ImGui)
# ---------------------------------------------------------------------------
# Header-only regression guard: workforce_efficiency == the prior inline curve.
add_executable(workforce_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/workforce_harness.cpp"
)

target_include_directories(workforce_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(workforce_harness PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# Headless trade-routes harness (BL-088; no SDL / Lua / ImGui)
# ---------------------------------------------------------------------------
# Exercises credit_arrived_convoys' persistent-route upsert (correct body-pair,
# last_tick stamp, no duplicate on a repeat/reverse lane, intra-body records
# nothing) and body_of_market resolution.
#
# trade_routes_harness is NOT declared here — the same rot as econ_bankruptcy and
# corp_terrain_matrix, and caught 2026-08-02. Its hand-picked list linked
# logistics.cpp but not river_generation.cpp, so BL-170 (rivers) broke it the
# moment intra_body_path started reading river_edge_discount / hex_side_for_offset,
# exactly as the econ_bankruptcy commit predicted the remaining hand-declared
# targets would. The generic batch at the foot of this file builds it against the
# whole ${IO_WORLD_SOURCES} superset, which cannot drift this way.

# ---------------------------------------------------------------------------
# Headless commercial-fog harness (BL-089; no SDL / Lua / ImGui)
# ---------------------------------------------------------------------------
# Exercises body_activity_visibility's tiers (unknown/known_stale/known/visible)
# from routes + live convoys + ownership + tick, and independence from survey phase.
add_executable(commercial_fog_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/commercial_fog_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world.cpp"
)

target_include_directories(commercial_fog_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(commercial_fog_harness PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# Headless corp-terrain viability harness (BL-075; no SDL / Lua / ImGui)
# ---------------------------------------------------------------------------
# For every generated corporation in the canonical world, runs a two-tier
# viability matrix: tier-1 asset legality + buildable focus (hard FAIL), tier-2
# reachable market + profitable chain (WARN). Links the SDL/Lua-free world-gen
# translation units (world, hard_coded_world + its generation passes, and the
# placement-rules seam); recipe_registry.cpp (the Lua loader) is intentionally
# NOT linked — the harness hand-builds a coarse reference registry via the
# header's inline setters.
# corp_terrain_matrix is NOT declared here either - the same rot as
# econ_bankruptcy. Its hand-picked list linked hard_coded_world.cpp but not
# history_ladder.cpp, so BL-221 broke it the moment the ladder was wired in -
# exactly as the econ_bankruptcy commit predicted the remaining hand-declared
# targets would break. The generic batch at the foot of this file builds it
# against the whole ${IO_WORLD_SOURCES} superset, which cannot drift this way.

# ---------------------------------------------------------------------------
# CTest wiring (BL-104) — one test per tools/verify/*.cpp, one local command
# ---------------------------------------------------------------------------
# The harnesses above keep their hand-tuned minimal link sets. This block then
# (a) builds every REMAINING tools/verify/*.cpp not already declared — linking
# the SDL/Lua/ImGui-free world superset (every src/world/*.cpp EXCEPT
# recipe_registry.cpp, tech_tree.cpp, and world_gen_config.cpp, the TUs that pull
# sol2/Lua), mirroring the CI headless
# loop in .github/workflows/build.yml so CMake and CI stay in lockstep and a new
# harness is picked up automatically — and (b) registers ALL harnesses
# (hand-listed + globbed) as CTest tests. Run the whole logic tier with
# `ctest --test-dir build --output-on-failure` (or check.bat, which wraps
# build + ctest into one command).
enable_testing()

file(GLOB IO_WORLD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/world/*.cpp")
list(FILTER IO_WORLD_SOURCES EXCLUDE REGEX "(recipe_registry|works_registry|tech_tree|world_gen_config)\\.cpp$")

# ---------------------------------------------------------------------------
# The world layer, compiled ONCE (BL-287)
# ---------------------------------------------------------------------------
# Every harness below used to take ${IO_WORLD_SOURCES} directly as sources, so
# each one compiled the whole world layer from scratch. With 44 harnesses and 30
# world TUs that is 1,320 compilations of the same 30 files per clean build,
# producing byte-identical objects 44 times over — measured at 45-90 minutes on
# the single-threaded default configuration, and paid in full by any edit to a
# widely-included header (components.hpp, world.hpp).
#
# An OBJECT library compiles them once and hands the same objects to every
# consumer: 1,320 -> 30. Nothing about the harnesses changes; this is only how
# the objects are produced.
#
# PUBLIC include dir + cxx_std_20 so consumers inherit them by linking. The
# warning flags stay PRIVATE — they apply to compiling the world layer itself,
# and must not leak onto a consumer's own TU.
add_library(io_world_obj OBJECT ${IO_WORLD_SOURCES})
target_include_directories(io_world_obj PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(io_world_obj PUBLIC cxx_std_20)
target_compile_options(io_world_obj PRIVATE ${IO_WARNING_FLAGS})

# pregame_balance_harness is the one harness that genuinely needs a live Lua
# state: it constructs a `lua_state`, loads scripts/economy.lua + recipes.lua,
# and calls recipe_registry::load_from_lua (defined in recipe_registry.cpp, the
# sol2/Lua TU the world superset above deliberately excludes). So the generic
# batch below cannot build it — declare it explicitly here, mirroring the
# ProjectIo target's sol2 setup: add back recipe_registry.cpp + scripting/
# lua_state.cpp, the sol2 include dir, and link lua54 (which propagates the Lua
# headers sol2 needs). Being a TARGET already, the `if(NOT TARGET ...)` guard in
# the foreach skips redeclaring it, while the unconditional add_test still
# registers it as a CTest test.
add_executable(pregame_balance_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/pregame_balance_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(pregame_balance_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(pregame_balance_harness PRIVATE io_world_obj lua54)
target_compile_features(pregame_balance_harness PRIVATE cxx_std_20)
target_compile_options(pregame_balance_harness PRIVATE ${IO_WARNING_FLAGS})

# persona_counsel_harness (BL-207) needs a live sol2/Lua state too — it loads
# the real scripts/personas/*.lua packs through src/scripting/persona_pack.cpp.
# Same shape as pregame_balance_harness above: add back the sol2/Lua TU +
# include dir + lua54 link; the `if(NOT TARGET ...)` guard in the foreach below
# then skips it while the unconditional add_test still registers it.
add_executable(persona_counsel_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/persona_counsel_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/persona_pack.cpp"
)
target_include_directories(persona_counsel_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(persona_counsel_harness PRIVATE io_world_obj lua54)
target_compile_features(persona_counsel_harness PRIVATE cxx_std_20)
target_compile_options(persona_counsel_harness PRIVATE ${IO_WARNING_FLAGS})

# font_glyph_harness (BL-234) is the one harness that links ImGui rather than
# ${IO_WORLD_SOURCES}: the defect it guards is the ImFontConfig handed to
# AddFontFromFileTTF, which lives in src/ui/fonts.cpp, not in world/*. It still
# links no SDL and no Lua — ImGui builds its font atlas on the CPU with no
# renderer or backend — so it stays in the fast headless tier. Declared before
# the foreach so the `if(NOT TARGET ...)` guard skips it while the unconditional
# add_test still registers it.
#
# Note it links the imgui target (which carries SDL3::SDL3 as a PUBLIC dep for
# its backends) but never opens a window: main() only builds an atlas.
add_executable(font_glyph_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/font_glyph_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/fonts.cpp"
)
target_include_directories(font_glyph_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui"
)
target_link_libraries(font_glyph_harness PRIVATE imgui)
target_compile_features(font_glyph_harness PRIVATE cxx_std_20)
target_compile_options(font_glyph_harness PRIVATE ${IO_WARNING_FLAGS})

file(GLOB IO_VERIFY_HARNESSES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/*.cpp")

# ---------------------------------------------------------------------------
# Test timeouts (BL-255)
# ---------------------------------------------------------------------------
# No harness previously carried a ctest TIMEOUT, so a hang (e.g. a future
# infinite loop reached by an economy-loop change) blocked the whole suite on
# ctest's 1500s default instead of failing fast. Sized against a Debug/-O0
# build, not the fast Release numbers - a plain configure with no
# CMAKE_BUILD_TYPE used to mean -O0 (see the build-type default above), and
# even now nothing stops a contributor from configuring Debug on purpose.
#
# Measured 2026-08-01, WSL2/Ubuntu, Ninja, 16 threads, full suite:
#             Release   Debug(-O0)
#   corp_terrain_matrix   1.80s    20.16s   <- slowest harness
#   history_ladder_harness 1.45s    8.18s
#   planetology_harness    1.30s    7.45s
#   ai_skill_harness        1.07s   11.00s  (fails on goldens - BL-252, not a
#                                            timing issue; timing is incidental)
#   road_generation_harness 1.01s    5.80s
#   world_determinism      0.73s    4.34s
#   every other harness    <1s      <2s
#
# IO_TEST_TIMEOUT_DEFAULT covers every harness above at 3x-6000x headroom
# over its Debug time. IO_TEST_TIMEOUT_LONG is for the harnesses that are
# structurally long-running (a multi-second generation/sweep/simulation
# loop, not just a slow build) and named explicitly rather than inferred
# from a one-off measurement - corp_terrain_matrix (measured above) plus
# econ_stability and data_creep_harness, whose long-running sweep/1500-tick
# passes are not yet present in this checkout (see BL-255 report) but land
# on the same fast-tier build these values must already tolerate.
#
# RE-TIERED 2026-08-09 (BL-288, at the v0.1.8 cut). The tiers above were set
# from Debug measurements before the sweeps existed, and the result was that
# `ctest` reported ten failures of which only ONE was a failing assertion.
# Measured on Linux/GCC Release, each harness run alone on an idle machine:
#
#   earthlike_lean_trace    121s   PASSES   (was reported Timeout at 60s)
#   notable_worlds          105s   PASSES   (was reported Timeout at 60s)
#   mediterranean_sweep      87s   PASSES   (was reported Timeout at 60s)
#   earthlike_tile_census    58s   PASSES   (2s under the bound - flaky by luck)
#   history_sim_harness    >400s   did not finish
#   history_sweep          >400s   did not finish
#
# A timeout is not a failing assertion, and reporting it as one trains sessions
# to ignore a red suite - which is exactly how the real ai_skill_harness golden
# drift sat unnoticed among nine false positives.
set(IO_TEST_TIMEOUT_DEFAULT 60)
set(IO_TEST_TIMEOUT_LONG 240)
set(IO_TEST_LONG_HARNESSES
    corp_terrain_matrix
    econ_stability
    data_creep_harness
    earthlike_lean_trace
    earthlike_tile_census
    mediterranean_sweep
    notable_worlds
)

# RESEARCH SWEEPS - labelled `sweep`, and NOT part of the routine gate.
#
# These are not regression checks that happen to be slow; they are open-ended
# exploration tools that answer a design question over many generated worlds,
# and both ran past 400s without finishing. Their cost is the point, so the
# answer is to stop pretending they are gate tests rather than to keep raising
# a bound they will always outgrow. Run them deliberately, by name or by label.
# (Their runtime is BL-320's subject; this only stops them failing the gate.)
set(IO_TEST_SWEEP_HARNESSES
    history_sim_harness
    history_sweep
)

# WALL-CLOCK BENCHES - labelled `bench`, and only meaningful on an idle machine.
#
# These assert ABSOLUTE times (econ_stability's per-tick bound, the 1s ceiling
# on home_surface_bench's preview), so they measure the machine as much as the
# code. Both PASS standalone and both failed in the 2026-08-09 suite run purely
# because a concurrent build was loading the box. Kept in the gate, because the
# budgets are real and worth defending - but labelled, so a failure here is read
# as "re-run it idle" rather than as a regression.
set(IO_TEST_BENCH_HARNESSES
    econ_stability
    home_surface_bench
)

foreach(harness_src ${IO_VERIFY_HARNESSES})
    get_filename_component(harness_name "${harness_src}" NAME_WE)
    if(NOT TARGET ${harness_name})
        # Links io_world_obj rather than recompiling ${IO_WORLD_SOURCES} (BL-287).
        add_executable(${harness_name} "${harness_src}")
        target_link_libraries(${harness_name} PRIVATE io_world_obj)
        target_include_directories(${harness_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_features(${harness_name} PRIVATE cxx_std_20)
        target_compile_options(${harness_name} PRIVATE ${IO_WARNING_FLAGS})
    endif()
    add_test(NAME ${harness_name} COMMAND ${harness_name})
    if(harness_name IN_LIST IO_TEST_SWEEP_HARNESSES)
        # No TIMEOUT is set deliberately: a sweep runs until it has its answer,
        # and the `sweep` label is what keeps it out of the routine gate.
        set_tests_properties(${harness_name} PROPERTIES LABELS "sweep")
    elseif(harness_name IN_LIST IO_TEST_LONG_HARNESSES)
        set_tests_properties(${harness_name} PROPERTIES TIMEOUT ${IO_TEST_TIMEOUT_LONG})
    else()
        set_tests_properties(${harness_name} PROPERTIES TIMEOUT ${IO_TEST_TIMEOUT_DEFAULT})
    endif()
    if(harness_name IN_LIST IO_TEST_BENCH_HARNESSES)
        set_property(TEST ${harness_name} APPEND PROPERTY LABELS "bench")
    endif()
endforeach()
