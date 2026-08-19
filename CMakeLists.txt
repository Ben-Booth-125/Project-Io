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

# A WORKTREE HAS NO CACHE OF ITS OWN (NR-283, 2026-08-17). The default above
# resolves inside the source tree, and a sub-agent worktree at
# .claude/worktrees/agent-<id> carries no `_deps_cache` — so the seeding fell
# straight through to COLD and every parallel agent re-downloaded ~120 MB. The
# env-var comment above documented the manual export as the answer, which meant
# the fix only worked for whoever remembered to type it; the case the cache
# exists FOR is precisely the one that missed it.
#
# Ask git where the real checkout is rather than guessing a `../../..` depth,
# which would silently break if the worktree location ever moves.
# `--git-common-dir` resolves to the MAIN repo's .git for a linked worktree and
# to our own for a normal checkout, so this is a no-op in the normal case.
if(NOT EXISTS "${IO_DEPS_CACHE}")
    find_package(Git QUIET)
    if(GIT_EXECUTABLE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --path-format=absolute --git-common-dir
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE IO_GIT_COMMON_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        if(IO_GIT_COMMON_DIR)
            get_filename_component(IO_MAIN_CHECKOUT "${IO_GIT_COMMON_DIR}" DIRECTORY)
            if(EXISTS "${IO_MAIN_CHECKOUT}/_deps_cache")
                set(IO_DEPS_CACHE "${IO_MAIN_CHECKOUT}/_deps_cache")
                message(STATUS "Deps: worktree — using the main checkout's cache at ${IO_DEPS_CACHE}")
            endif()
        endif()
    endif()
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
    # NO `-src` SUFFIX HERE (NR-276, fixed 2026-08-17). The suffix is a
    # FetchContent BUILD-TREE convention — `_deps/<name>-src` — and the
    # IS_DIRECTORY test below is right to use it. The shared CACHE is a plain
    # checkout that carries `sdl3`, `lua_src`, `sol2_src`, `imgui_src`, so
    # appending `-src` here matched nothing, every dep classified COLD, and the
    # BL-302 seeding this loop exists to perform never fired once.
    #
    # It went unseen because the main repo's build/ tree has been warm since
    # before BL-302, so the IS_DIRECTORY branch above answered first every time
    # and no one configured cold. It surfaced only when sub-agent worktrees
    # started each trying to download ~120 MB.
    set(dep_cached_src "${IO_DEPS_CACHE}/${dep_name}")

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

# BL-363: unchecked sol2 conversions must throw a catchable sol::error, not UB.
# Directory scope so every sol2-including TU (ProjectIo + the Lua-linked
# harnesses declared below) gets it; no-op for TUs that never include sol.
add_compile_definitions(SOL_ALL_SAFETIES_ON=1)

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

# interbody_pull_harness (BL-404) is the third live-Lua harness, and for the
# same reason as pregame_balance_harness: the question it asks is what the
# AUTHORED economy numbers do, so it must load scripts/economy.lua + recipes.lua
# rather than hand-build a registry. A hand-built fixture is precisely how the
# defect it measures went unnoticed under a green market_emergence_harness.
#
# It also needs WORKING_DIRECTORY at the add_test below: it resolves the script
# paths relative to the repo root, and ctest otherwise runs it from the build
# dir, where the load fails and every assertion passes VACUOUSLY.
add_executable(interbody_pull_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/interbody_pull_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(interbody_pull_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(interbody_pull_harness PRIVATE io_world_obj lua54)
target_compile_features(interbody_pull_harness PRIVATE cxx_std_20)
target_compile_options(interbody_pull_harness PRIVATE ${IO_WARNING_FLAGS})

# era_roster (BL-433) is a live-Lua harness for the same reason as the two above:
# the question it asks is what the AUTHORED era tags do. A hand-built registry
# would assert only that the mask works on data the harness itself wrote, which
# is precisely how a mis-tagged entry would slip through.
add_executable(era_roster
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/era_roster.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(era_roster PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(era_roster PRIVATE io_world_obj lua54)
target_compile_features(era_roster PRIVATE cxx_std_20)
target_compile_options(era_roster PRIVATE ${IO_WARNING_FLAGS})

# recipe_switch_harness (BL-430) is a live-Lua harness for the same reason as
# era_roster above: its no-dominance guard (R1) asks what the AUTHORED recipe
# roster's cost/depth shape actually is, which a hand-built fixture cannot
# answer without becoming the thing under test.
add_executable(recipe_switch_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/recipe_switch_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(recipe_switch_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(recipe_switch_harness PRIVATE io_world_obj lua54)
target_compile_features(recipe_switch_harness PRIVATE cxx_std_20)
target_compile_options(recipe_switch_harness PRIVATE ${IO_WARNING_FLAGS})

# chain_depth (BL-428) — the growth-spine metric. Live-Lua for its D6 rows, which
# ask what the SHIPPED recipe graph's depth actually is; D1-D5 are hand-built
# fixtures, because those ask whether the metric computes what it claims and must
# not drift with the economy.
add_executable(chain_depth
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/chain_depth.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(chain_depth PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(chain_depth PRIVATE io_world_obj lua54)
target_compile_features(chain_depth PRIVATE cxx_std_20)
target_compile_options(chain_depth PRIVATE ${IO_WARNING_FLAGS})

# player_seed_sweep (2026-08-16) — live-Lua for the same reason pregame_balance_harness
# is: it runs the REAL economy over the REAL authored recipes, per seed. A hand-built
# registry would make every seed look identically poor and the sweep would report
# nothing about the game that actually ships.
add_executable(player_seed_sweep
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/player_seed_sweep.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(player_seed_sweep PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(player_seed_sweep PRIVATE io_world_obj lua54)
target_compile_features(player_seed_sweep PRIVATE cxx_std_20)
target_compile_options(player_seed_sweep PRIVATE ${IO_WARNING_FLAGS})

# tier_margin (BL-436, 2026-08-16) — live-Lua for the same reason: it asks
# whether REFINING PAYS in the shipped economy, so it must read the authored
# prices, rates and recipes rather than a hand-built mirror of them. It also
# reads estimate_building_profit, the same per-building breakdown the
# Profitability page shows a player, so what it reports is what the game itself
# believes rather than a parallel model that could be wrong on its own.
add_executable(tier_margin
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/tier_margin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
    # world_gen_config.cpp: the harness loads scripts/world_gen.lua so markets
    # carry the AUTHORED base_price table. Without it they fall back to the
    # built-in defaults, which price only a subset of resources (not coal) and
    # make processors look starved for a reason the real game does not have.
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world_gen_config.cpp"
)
target_include_directories(tier_margin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(tier_margin PRIVATE io_world_obj lua54)
target_compile_features(tier_margin PRIVATE cxx_std_20)
target_compile_options(tier_margin PRIVATE ${IO_WARNING_FLAGS})

# haulage_measure (BL-442 step 2, 2026-08-17) — live-Lua for the same reason as
# tier_margin: it measures the per-unit haulage the supply layer ACTUALLY charges
# between adjacent markets, which is the authored logistics cost table times the
# terrain-weighted A* cost in the real generated world. It exists so the widened
# price ceiling is a derived number rather than a guess, and so it can be
# re-derived later rather than trusted.
add_executable(haulage_measure
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/haulage_measure.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world_gen_config.cpp"
)
target_include_directories(haulage_measure PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(haulage_measure PRIVATE io_world_obj lua54)
target_compile_features(haulage_measure PRIVATE cxx_std_20)
target_compile_options(haulage_measure PRIVATE ${IO_WARNING_FLAGS})

# substrate_census (NEW-10, 2026-08-18) — live-Lua, and NOT as a convenience.
# It censuses the world at the point the player first sees it, which is AFTER
# generate_background_firms (BL-365). That pass's measured stop condition reads
# economy.lua's population_demand/background_demand baskets, which are all-zero
# on a default-constructed registry — so with a hand-built registry it places
# ZERO firms and the census reports a world the game never builds. world_gen.lua
# (base prices/market seeding) and works.lua (the Era -1 roster, which generation
# forwards to the pre-history sim) are loaded for the same reason: this harness's
# whole subject is the SHIPPED world, so every authored input must be the real one.
add_executable(substrate_census
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/substrate_census.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/works_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world_gen_config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(substrate_census PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(substrate_census PRIVATE io_world_obj lua54)
target_compile_features(substrate_census PRIVATE cxx_std_20)
target_compile_options(substrate_census PRIVATE ${IO_WARNING_FLAGS})

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

# price_band_harness (BL-442) is live-Lua for one row only, P2: it asserts that
# the band the SHIPPED scripts/economy.lua authors is the band that reaches
# recipe_registry, which a hand-built fixture cannot answer without becoming the
# thing under test. Its remaining rows are hand-built fixtures.
add_executable(price_band_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/price_band_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(price_band_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(price_band_harness PRIVATE io_world_obj lua54)
target_compile_features(price_band_harness PRIVATE cxx_std_20)
target_compile_options(price_band_harness PRIVATE ${IO_WARNING_FLAGS})

# agent_seam_harness (BL-412) — the live agent control seam. Hand-declared
# because it links two src/core TUs the world superset deliberately excludes:
# agent_protocol.cpp (the --serve line protocol, shared with run_serve) and
# agent_seam.cpp (the loopback listener + tick-boundary drain). Both are
# SDL/Lua/ImGui-free — the socket layer is Winsock/BSD behind one shim — so
# the harness stays in the fast headless tier; on Windows that shim needs
# ws2_32 (the pragma in agent_seam.cpp covers MSVC, the explicit link covers
# everything else).
add_executable(agent_seam_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/agent_seam_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/agent_protocol.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/agent_seam.cpp"
)
target_include_directories(agent_seam_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(agent_seam_harness PRIVATE io_world_obj)
if(WIN32)
    target_link_libraries(agent_seam_harness PRIVATE ws2_32)
endif()
target_compile_features(agent_seam_harness PRIVATE cxx_std_20)
target_compile_options(agent_seam_harness PRIVATE ${IO_WARNING_FLAGS})

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

# RE-TIERED AGAIN 2026-08-13. Wiring the Era -1 year-tick sim into generation
# added ~23 s to every generated world. Harnesses that do not test the era now
# say so (tools/verify/harness_params.hpp, `no_prehistory()`) and pay nothing.
#
# CORRECTION 2026-08-18: that ~23 s is a **Debug** number, and it is quoted as
# an unqualified cost here, in hard_coded_world.hpp and in harness_params.hpp.
# Measured at /O2 /DNDEBUG the same pass is **1.2-1.8 s per world** — roughly
# 15x cheaper. `no_prehistory()`'s argument still holds for the Debug gate,
# which is what `build/` is configured as, but any Release-cost reasoning built
# on ~23 s is wrong by an order of magnitude. See NR-336.
# era_world_harness cannot: testing the 0 CE world IS its subject, and it builds
# two of them for its determinism check. It is structurally long-running now,
# not slow by accident, so it moves tier rather than having the check thinned.
set(IO_TEST_LONG_HARNESSES
    corp_terrain_matrix
    era_world_harness
    econ_stability
    earthlike_lean_trace
    earthlike_tile_census
    mediterranean_sweep
    notable_worlds
    # player_seed_sweep added 2026-08-16: it generates one full world per seed
    # (24 by default) and takes ~69 s on this box, so the 60 s default tier
    # timed it out EVERY run since it was added the day before — it had never
    # once passed under ctest, while passing standalone. Found by the NR-257
    # suite run, not by the harness reporting anything: a Timeout is a silent
    # failure mode, since the tool prints its findings and exits 0 either way.
    player_seed_sweep
    # world_determinism moved 2026-08-18, PRE-EMPTIVELY, when BL-462's R3 group
    # gave it the first prehistory-ON cases it has ever had. Every earlier case
    # passed `prehistory_years = 0`, so the project's determinism guarantee
    # excluded the era sim entirely; R3 builds five worlds at the real shipping
    # default (epoch 0, 400 years) and takes **128 s in Debug** against 10.3 s
    # at /O2. `build/` is configured Debug, so the 60 s tier would have timed it
    # out on its very first gate run — the player_seed_sweep failure above,
    # repeated one day later on the harness that guards determinism itself.
    # Moved before it ever ran red rather than after, which is the whole point
    # of writing that story down.
    # HEADROOM IS THIN: 128 s against this tier's 240 s is ~1.9x, well under the
    # "3x-6000x" this block claims above. If R3 grows a sixth world, re-tier or
    # label it `sweep` rather than letting the margin quietly close.
    world_determinism
)

# EXCLUDED FROM THE ROUTINE GATE - labelled `sweep`. TWO REASONS live here, and
# conflating them would hide the second.
#
# (1) RESEARCH SWEEPS. Not regression checks that happen to be slow; open-ended
# exploration tools that answer a design question over many generated worlds, and
# both ran past 400s without finishing. Their cost is the point, so the answer is
# to stop pretending they are gate tests rather than to keep raising a bound they
# will always outgrow. Run them deliberately, by name or by label.
# (Their runtime is BL-320's subject; this only stops them failing the gate.)
#
# (2) PARKED REGRESSION CHECKS. A check that SHOULD bind in the gate but has been
# made unaffordable by a performance regression elsewhere. These are a debt, not a
# category — each one is a hole in the gate, and each carries a note saying what
# must be fixed to bring it back. Do not let one sit here quietly.
set(IO_TEST_SWEEP_HARNESSES
    history_sim_harness
    history_sweep

    # CATEGORY (1), added 2026-08-13. seed_sweep_probe generates a full world PER
    # SEED across a spread, to catch a seed-dependent generation failure — the
    # instrument written to chase the startup "crash" (NR-209). Its cost is the
    # point, exactly as for the two above, and it does not test the era so it
    # cannot opt out of the pre-epoch sim the way an ordinary harness can. Run it
    # by name when a seed is suspected.
    seed_sweep_probe

    # CATEGORY (1), added 2026-08-18. substrate_census (NEW-10) generates a full
    # world PER SEED *with the pre-epoch era at its shipped 400 years* — it cannot
    # use no_prehistory(), because the province set that era leaves behind is what
    # the settlements, roads and markets it counts are carved onto. Measured
    # 2026-08-18: ~1.0 s/world Release, ~62 s/world Debug. Its three-seed default
    # is therefore ~186 s in a Debug tree — past the 60 s default tier and inside
    # 25% of the 240 s long tier, which is precisely the "2s under the bound —
    # flaky by luck" shape the BL-288 re-tier above exists to stop. It REPORTS and
    # gates nothing but structure, so it has nothing to contribute to a routine
    # run in any case; registering it here is the honest reading of NR-259 (a
    # Timeout is a silent failure, so a harness that cannot reliably fit the gate
    # must not be registered as though it could).
    substrate_census

    # PARKED FOR A DIFFERENT REASON (2026-08-12), and the distinction matters.
    #
    # The two above are open-ended exploration tools whose cost IS the point.
    # data_creep_harness is not: it is a genuine regression check that belongs in
    # the gate, and it is here only because it has become unaffordable to run.
    #
    # The 3x map (15,120 -> 45,240 tiles) put the real-world rollout at ~165 ms/tick
    # and rising as buildings accumulate. Its plateau window must reach saturation
    # to mean anything, and on the larger world that needs ~4500 ticks — which
    # exceeded a 1200 s bound when tried. Trimming ticks to fit was rejected: a
    # plateau measured BEFORE saturation keeps the test green while emptying it,
    # and raising the bound again is the exact treadmill this section exists to
    # refuse.
    #
    # WHAT IS LOST, STATED PLAINLY: the gate currently has NO data-creep coverage.
    # That is a real hole, accepted only because the alternative was a test that
    # passes without checking anything.
    #
    # TO RESTORE IT: fix the tick cost, not this list. NR-207 records that
    # run_economy_step is 89.7% of the tick (measured by tools/verify/tick_profile.cpp),
    # that these are Debug-tree figures whose Release equivalent is unknown, and
    # that econ_stability sweeps bodies x corps and can never see a tile-count term
    # at all — which is how a ~50x regression sat behind a green performance gate.
    data_creep_harness
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

# Harnesses that load the real scripts/*.lua by relative path, and so must run
# with the repo root as their working directory (see the foreach below).
set(IO_TEST_SCRIPT_ROOTED_HARNESSES
    chain_depth
    era_roster
    haulage_measure
    player_seed_sweep
    interbody_pull_harness
    pregame_balance_harness
    persona_counsel_harness
    recipe_switch_harness
    # ADDED 2026-08-17 (BL-442 step 2). Both load scripts/*.lua by relative path
    # and both say so in their own headers — price_band_harness's README entry
    # reads "Run it from the repo root", tier_margin's CMake block says it loads
    # scripts/world_gen.lua — but neither was ever listed here, so under ctest
    # they ran from build/ and read build/scripts/, which the POST_BUILD copy only
    # refreshes when the ProjectIo TARGET relinks. A Lua-only edit does not relink
    # anything, so that copy goes stale silently and these two measured the OLD
    # economy while the repo held the new one. That is how BL-442 step 2's ceiling
    # change reached tier_margin standalone (net/tick -17.68) and NOT under ctest
    # (-10.81, the pre-change number) in the same session. Exactly the vacuity trap
    # the comment at the foreach below already warns about, found twice more.
    price_band_harness
    tier_margin
    # ADDED 2026-08-18 (NEW-10). Loads recipes/economy/world_gen/works by relative
    # path and FATALs on an empty recipe set rather than measuring a world the
    # game never builds — but only if it is looking at the real scripts/.
    substrate_census
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
    if(harness_name IN_LIST IO_TEST_SWEEP_HARNESSES)
        # No TIMEOUT is set deliberately: a sweep runs until it has its answer.
        # The COMMAND is a gate script, not the exe (BL-415): without
        # IO_RUN_SWEEPS in the environment it prints the skip line and CTest
        # marks the test Skipped via SKIP_REGULAR_EXPRESSION — exclusion from
        # the routine gate is machinery, not a `-LE sweep` flag the caller has
        # to remember. (CONFIGURATIONS was tried first and does not gate on a
        # single-config generator — measured, not assumed.) Deliberate run:
        # `$env:IO_RUN_SWEEPS=1; ctest --test-dir build -L sweep`, or run the
        # exe directly. The label remains for selection.
        add_test(NAME ${harness_name} COMMAND ${CMAKE_COMMAND}
            -DIO_SWEEP_EXE=$<TARGET_FILE:${harness_name}>
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/run_sweep.cmake")
        set_tests_properties(${harness_name} PROPERTIES
            LABELS "sweep"
            SKIP_REGULAR_EXPRESSION "sweep harness skipped")
    elseif(harness_name IN_LIST IO_TEST_LONG_HARNESSES)
        add_test(NAME ${harness_name} COMMAND ${harness_name})
        set_tests_properties(${harness_name} PROPERTIES TIMEOUT ${IO_TEST_TIMEOUT_LONG})
    else()
        add_test(NAME ${harness_name} COMMAND ${harness_name})
        set_tests_properties(${harness_name} PROPERTIES TIMEOUT ${IO_TEST_TIMEOUT_DEFAULT})
    endif()
    if(harness_name IN_LIST IO_TEST_BENCH_HARNESSES)
        set_property(TEST ${harness_name} APPEND PROPERTY LABELS "bench")
    endif()
    # Harnesses that load the real scripts/*.lua resolve those paths relative to
    # the repo root. Without this they run from the build dir, load nothing, and
    # pass vacuously against a default registry (BL-404's own lesson).
    if(harness_name IN_LIST IO_TEST_SCRIPT_ROOTED_HARNESSES)
        set_tests_properties(${harness_name} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
endforeach()

# BL-435 task E — the same exe, its ASSERTING mode, as its own test. The default
# player_seed_sweep run reports a design question and exits 0 whatever it finds;
# `--guard` asserts the four properties the starting-corp selection screen needs
# to hold in EVERY world (a choice exists, the specialist/background split holds,
# no specialist that cannot produce, processor coverage above the pre-BL-435
# floor) and exits non-zero when one breaks. Registering it separately is what
# makes it a gate rather than a thing someone remembers to run.
#
# 12 seeds, not the sweep's 24: each seed is a full world generation, and the
# properties are per-seed invariants rather than a distribution needing a large
# sample. Script-rooted for the same reason the sweep is — it loads the real
# recipes.lua/economy.lua and would otherwise measure an empty registry.
add_test(NAME player_seed_sweep_guard COMMAND player_seed_sweep --guard 12)
set_tests_properties(player_seed_sweep_guard PROPERTIES
    TIMEOUT ${IO_TEST_TIMEOUT_LONG}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
