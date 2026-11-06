#!/usr/bin/env bash
# syntax_check_gui.sh - the bash path to syntax_check_gui.bat: a /Zs compile of GUI-side
# translation units, for a worktree with no configured build and an agent with no cmd.
# It proves names and types, not linking; the integrating session's build_app.bat does that.
#
# USAGE, from the repo root:
#     bash tools/verify/syntax_check_gui.sh src/ui/selection_panel.cpp [more...]
#
# The toolchain resolution mirrors build_lua_harness.sh, whose comments say why it is pinned.
set -uo pipefail

die() { echo "syntax_check_gui: $*" >&2; exit 1; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || die "cannot cd to repo root"
[ $# -gt 0 ] || die "no source named. usage: bash tools/verify/syntax_check_gui.sh <file.cpp> [more...]"

# Before the MSYS exports below, for build_lua_harness.sh's reason.
DEPS="${IO_DEPS_CACHE:-}"
if [ -z "$DEPS" ]; then
    if [ -d "$ROOT/_deps_cache" ]; then
        DEPS="$ROOT/_deps_cache"
    else
        _common="$(git -C "$ROOT" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)"
        [ -n "$_common" ] && DEPS="$(dirname "$_common")/_deps_cache"
    fi
fi
for d in sol2_src/include lua_src sdl3/include imgui_src; do
    [ -d "$DEPS/$d" ] || die "dependency headers not found at \"$DEPS/$d\". Set IO_DEPS_CACHE."
done

VC_ROOT="${IO_VC_ROOT:-/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC}"
MSVC_VER="${IO_MSVC_VER:-$(ls -1 "$VC_ROOT/Tools/MSVC" | sort -V | tail -1)}"
MSVC="$VC_ROOT/Tools/MSVC/$MSVC_VER"
[ -x "$MSVC/bin/Hostx64/x64/cl.exe" ] || die "cl.exe not found under \"$MSVC/bin/Hostx64/x64\"."
SDK_ROOT="${IO_SDK_ROOT:-/c/Program Files (x86)/Windows Kits/10}"
SDK_VER="${IO_SDK_VER:-$(ls -1 "$SDK_ROOT/Include" | sort -V | tail -1)}"
[ -d "$SDK_ROOT/Include/$SDK_VER/ucrt" ] || die "SDK $SDK_VER has no ucrt headers."

w() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

export INCLUDE="$(w "$MSVC/include");$(w "$SDK_ROOT/Include/$SDK_VER/ucrt");$(w "$SDK_ROOT/Include/$SDK_VER/shared");$(w "$SDK_ROOT/Include/$SDK_VER/um");$(w "$SDK_ROOT/Include/$SDK_VER/winrt")"
export PATH="$MSVC/bin/Hostx64/x64:$SDK_ROOT/bin/$SDK_VER/x64:$PATH"
DEPSW="$(w "$DEPS")"
CL="$MSVC/bin/Hostx64/x64/cl.exe"

export MSYS2_ARG_CONV_EXCL='*'
export MSYS_NO_PATHCONV=1

echo "syntax_check_gui: /Zs over $*"
"$CL" /nologo /c /Zs /std:c++20 /EHsc /MP /DIMGUI_DEFINE_MATH_OPERATORS /DSOL_ALL_SAFETIES_ON=1 \
    /I src /I "$DEPSW\\lua_src" /I "$DEPSW\\sol2_src\\include" /I "$DEPSW\\sdl3\\include" \
    /I "$DEPSW\\imgui_src" /I "$DEPSW\\imgui_src\\backends" "$@"
status=$?
[ $status -eq 0 ] && { echo "syntax_check_gui: SYNTAX_OK"; exit 0; }
echo "syntax_check_gui: SYNTAX_FAILED"
exit 1
