cmake_minimum_required(VERSION 3.25)
project(ProjectIo LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
# Headless economy bankruptcy calibration harness (no SDL / Lua / ImGui)
# ---------------------------------------------------------------------------
# Links only the SDL/Lua-free world translation units, mirroring the manual
# build documented in tools/verify/README.md and run_harness.bat.
add_executable(econ_bankruptcy
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/econ_bankruptcy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/economy_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/market_clearing.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/budget_system.cpp"
    # BL-079: economy_system's scoped-agency pass calls estimate_building_profit.
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/building_profit.cpp"
)

target_include_directories(econ_bankruptcy PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(econ_bankruptcy PRIVATE cxx_std_20)

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
# nothing) and body_of_market resolution. Links only world + supply_system.
add_executable(trade_routes_harness
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/trade_routes_harness.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/supply_system.cpp"
)

target_include_directories(trade_routes_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(trade_routes_harness PRIVATE cxx_std_20)

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
add_executable(corp_terrain_matrix
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/corp_terrain_matrix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/world.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/hard_coded_world.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/tile_generation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/nation_generation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/population_generation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/city_names.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/corporation_generation.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/orbital_system.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/placement_rules.cpp"
)

target_include_directories(corp_terrain_matrix PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(corp_terrain_matrix PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# CTest wiring (BL-104) — one test per tools/verify/*.cpp, one local command
# ---------------------------------------------------------------------------
# The harnesses above keep their hand-tuned minimal link sets. This block then
# (a) builds every REMAINING tools/verify/*.cpp not already declared — linking
# the SDL/Lua/ImGui-free world superset (every src/world/*.cpp EXCEPT
# recipe_registry.cpp, the one TU that pulls sol2/Lua), mirroring the CI headless
# loop in .github/workflows/build.yml so CMake and CI stay in lockstep and a new
# harness is picked up automatically — and (b) registers ALL harnesses
# (hand-listed + globbed) as CTest tests. Run the whole logic tier with
# `ctest --test-dir build --output-on-failure` (or check.bat, which wraps
# build + ctest into one command).
enable_testing()

file(GLOB IO_WORLD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/world/*.cpp")
list(FILTER IO_WORLD_SOURCES EXCLUDE REGEX "recipe_registry\\.cpp$")

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
    ${IO_WORLD_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/world/recipe_registry.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scripting/lua_state.cpp"
)
target_include_directories(pregame_balance_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${sol2_src_SOURCE_DIR}/include"
)
target_link_libraries(pregame_balance_harness PRIVATE lua54)
target_compile_features(pregame_balance_harness PRIVATE cxx_std_20)
target_compile_options(pregame_balance_harness PRIVATE ${IO_WARNING_FLAGS})

file(GLOB IO_VERIFY_HARNESSES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify/*.cpp")
foreach(harness_src ${IO_VERIFY_HARNESSES})
    get_filename_component(harness_name "${harness_src}" NAME_WE)
    if(NOT TARGET ${harness_name})
        add_executable(${harness_name} "${harness_src}" ${IO_WORLD_SOURCES})
        target_include_directories(${harness_name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_compile_features(${harness_name} PRIVATE cxx_std_20)
        target_compile_options(${harness_name} PRIVATE ${IO_WARNING_FLAGS})
    endif()
    add_test(NAME ${harness_name} COMMAND ${harness_name})
endforeach()
