#!/usr/bin/env bash
# build_env_harness.sh — build ONE tools/verify/<name>.cpp that includes
# core/save_game.hpp (the save ENVELOPE), from bash, with no cmd.
#
#   bash tools/verify/build_env_harness.sh <harness_name> [--run]
#
# WHY THIS EXISTS (SAVE_BUILDING_REPORT, 2026-11-10). save_game.hpp includes
# ui/ui_state.hpp and so <imgui.h>, and save_game.cpp lives in src/core, so
# neither headless builder compiles it: build_harness.js refuses such a harness
# and points at CMake, and cmd (build_app.bat) is blocked for worktree-isolated
# sub-agents. This is build_lua_harness.sh EXACTLY, plus two things:
#   - src/core/save_game.cpp joins the world superset;
#   - _deps_cache/imgui_src joins the include path (headers only: the envelope
#     uses ImGui's types, never its functions, so no imgui TU is linked).
# Everything else -- the pinned MSVC, the Lua object cache, the output path
# build_gen/verify/<name>.exe -- is that script's, for that script's reasons.
# Harnesses built here: save_envelope_roundtrip, save_continuity.
set -uo pipefail

die() { echo "build_env_harness: $*" >&2; exit 1; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || die "cannot cd to repo root"

NAME="${1:-}"
[ -n "$NAME" ] || die "no harness named.
  usage: bash tools/verify/build_env_harness.sh <harness_name> [--run]"
RUN=0
[ "${2:-}" = "--run" ] && RUN=1

SRC="tools/verify/${NAME}.cpp"
[ -f "$SRC" ] || die "ERROR: $SRC does not exist."

# --- the dependency cache ---------------------------------------------------
# RESOLVED BEFORE THE MSYS EXPORTS BELOW, AND THAT ORDER IS LOAD-BEARING.
# MSYS_NO_PATHCONV=1 / MSYS2_ARG_CONV_EXCL='*' stop Git Bash rewriting POSIX
# paths into Windows ones. They are set for the `cl` invocations, which need
# their /switches left alone - but with them in force `git -C /c/Users/...`
# hands Windows git a path it cannot resolve, rev-parse dies with "cannot change
# to", the fallback below silently yields nothing, and DEPS lands on the
# worktree's own non-existent cache. The symptom is a missing sol/sol.hpp, which
# reads like the wrong builder. Measured 2026-09-06 after a second agent
# reported it; the first fix put this block after the exports and was inert.
#
# IO_DEPS_CACHE mirrors CMakeLists' own env override, exactly as the .bat does.
# A git WORKTREE has no _deps_cache of its own, so fall back to the MAIN
# checkout via git's common dir before giving up. Without this every worktree
# agent has to discover IO_DEPS_CACHE for itself (reported 2026-09-06).
DEPS="${IO_DEPS_CACHE:-}"
if [ -z "$DEPS" ]; then
    if [ -d "$ROOT/_deps_cache" ]; then
        DEPS="$ROOT/_deps_cache"
    else
        _common="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)"
        [ -n "$_common" ] && DEPS="$(dirname "$_common")/_deps_cache"
        [ -n "$DEPS" ] || DEPS="$ROOT/_deps_cache"
    fi
fi
[ -f "$DEPS/sol2_src/include/sol/sol.hpp" ] || die "ERROR: sol2 headers not found under \"$DEPS/sol2_src/include\".
  Set IO_DEPS_CACHE to a checkout that carries lua_src / sol2_src."
[ -f "$DEPS/lua_src/lua.h" ] || die "ERROR: Lua sources not found under \"$DEPS/lua_src\"."

# --- the MSVC environment, built by hand ------------------------------------
# PINNED, and the pin is load-bearing (build_app.bat § 2 records why): the tree
# is built by BuildTools MSVC 14.44.35207. If a NEWER Visual Studio is also
# installed, putting its headers on INCLUDE while the 14.44 compiler is invoked
# detonates inside <type_traits> with errors that read like broken code. So this
# resolves the BuildTools toolchain specifically and takes the highest version
# under it, rather than whatever happens to be first on PATH.
VC_ROOT="${IO_VC_ROOT:-/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC}"
[ -d "$VC_ROOT/Tools/MSVC" ] || die "ERROR: no MSVC toolchain under \"$VC_ROOT/Tools/MSVC\".
  Set IO_VC_ROOT to a Visual Studio VC directory that carries Tools/MSVC."
MSVC_VER="${IO_MSVC_VER:-$(ls -1 "$VC_ROOT/Tools/MSVC" | sort -V | tail -1)}"
MSVC="$VC_ROOT/Tools/MSVC/$MSVC_VER"
[ -x "$MSVC/bin/Hostx64/x64/cl.exe" ] || die "ERROR: cl.exe not found under \"$MSVC/bin/Hostx64/x64\"."

SDK_ROOT="${IO_SDK_ROOT:-/c/Program Files (x86)/Windows Kits/10}"
[ -d "$SDK_ROOT/Include" ] || die "ERROR: no Windows SDK under \"$SDK_ROOT/Include\"."
SDK_VER="${IO_SDK_VER:-$(ls -1 "$SDK_ROOT/Include" | sort -V | tail -1)}"
[ -d "$SDK_ROOT/Include/$SDK_VER/ucrt" ] || die "ERROR: SDK $SDK_VER has no ucrt headers."

w() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

export INCLUDE="$(w "$MSVC/include");$(w "$SDK_ROOT/Include/$SDK_VER/ucrt");$(w "$SDK_ROOT/Include/$SDK_VER/shared");$(w "$SDK_ROOT/Include/$SDK_VER/um");$(w "$SDK_ROOT/Include/$SDK_VER/winrt")"
export LIB="$(w "$MSVC/lib/x64");$(w "$SDK_ROOT/Lib/$SDK_VER/ucrt/x64");$(w "$SDK_ROOT/Lib/$SDK_VER/um/x64")"
export PATH="$MSVC/bin/Hostx64/x64:$SDK_ROOT/bin/$SDK_VER/x64:$PATH"
CL="$MSVC/bin/Hostx64/x64/cl.exe"

# Git Bash rewrites any argument that looks like a POSIX path, which mangles
# every /nologo-style switch. This turns that off for the cl invocations below.
export MSYS2_ARG_CONV_EXCL='*'
export MSYS_NO_PATHCONV=1

OUTDIR="build_gen/verify"
OBJDIR="$OUTDIR/${NAME}.obj"
LUAOBJ="$OUTDIR/_lua_obj"
mkdir -p "$OBJDIR" "$LUAOBJ"
rm -f "$OUTDIR/${NAME}.exe"

# --- Lua, compiled once and cached ------------------------------------------
# lua.c (standalone interpreter), luac.c (compiler) and onelua.c (amalgamation,
# which includes lua.c) are excluded, exactly as CMakeLists excludes them.
if [ ! -f "$LUAOBJ/lapi.obj" ]; then
    echo "build_env_harness: compiling Lua 5.4 (cached after this)"
    LUAC=()
    for f in "$DEPS"/lua_src/*.c; do
        b="$(basename "$f")"
        case "$b" in lua.c|luac.c|onelua.c) continue ;; esac
        LUAC+=("$(w "$f")")
    done
    "$CL" /nologo /c /O2 /MD /MP /I "$(w "$DEPS/lua_src")" "${LUAC[@]}" /Fo:"$(w "$LUAOBJ")\\" \
        || die "LUA_COMPILE_FAILED"
fi

# The world superset — ALL of src/world/*.cpp (the sol2 TUs included), plus the
# scripting bridge. A glob, never a hand-picked list.
WORLD=()
for f in src/world/*.cpp; do WORLD+=("$(w "$f")"); done

# THE WHOLE src/scripting BRIDGE, not just lua_state.cpp. The .bat compiles only
# lua_state.cpp, which means persona_counsel_harness — the harness whose
# persona_pack dependency build_harness.js explicitly ROUTES HERE — fails with
# five LNK2019s on persona::pack. A builder that refuses a harness by name and
# then cannot build it is worse than one that never offered, because the link
# error reads exactly like broken code, which is the failure BL-774 exists to
# kill. Globbed for the same reason the world set is: a hand list rots.
for f in src/scripting/*.cpp; do WORLD+=("$(w "$f")"); done
WORLD+=("$(w src/core/save_game.cpp)") # the envelope (this builder's reason)
LUAOBJS=()
for f in "$LUAOBJ"/*.obj; do LUAOBJS+=("$(w "$f")"); done

echo "build_env_harness: $NAME <- world superset + sol2/Lua TUs (cl $MSVC_VER, /O2 /MD)"
"$CL" /nologo /std:c++20 /EHsc /MP /MD /O2 /DNDEBUG /DSOL_ALL_SAFETIES_ON=1 \
    /I "$(w "$ROOT/src")" /I "$(w "$ROOT/tools/verify")" /I "$(w "$DEPS/imgui_src")" \
    /I "$(w "$DEPS/lua_src")" /I "$(w "$DEPS/sol2_src/include")" \
    "$(w "$SRC")" "${WORLD[@]}" \
    /Fo:"$(w "$OBJDIR")\\" /Fe:"$(w "$OUTDIR/$NAME.exe")" \
    /link "${LUAOBJS[@]}" \
    || die "COMPILE_FAILED"

[ -f "$OUTDIR/${NAME}.exe" ] || die "COMPILE_FAILED (no executable)"
echo "build_env_harness: COMPILE_OK  $OUTDIR/${NAME}.exe"

if [ "$RUN" = "1" ]; then
    echo "build_env_harness: running $NAME"
    "./$OUTDIR/${NAME}.exe"
fi
