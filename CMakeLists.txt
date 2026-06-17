cmake_minimum_required(VERSION 3.25)
project(ProjectIo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# SDL3
# ---------------------------------------------------------------------------
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

# ---------------------------------------------------------------------------
# Lua 5.4  (built as a static lib from source)
# ---------------------------------------------------------------------------
FetchContent_Declare(
    lua_src
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.7
    GIT_SHALLOW    TRUE
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
FetchContent_Declare(
    sol2_src
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.3.0
    GIT_SHALLOW    TRUE
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
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE
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
)

target_include_directories(econ_bankruptcy PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(econ_bankruptcy PRIVATE cxx_std_20)
