#!/bin/bash
# Build the PLAY tree (build_rel: Release, Ninja) from Git Bash, with no cmd.exe.
#
# WHY THIS EXISTS (BL-1373). `build_app.bat` is the normal build and it builds
# `build/` (Debug, NMake). A WORKTREE-ISOLATED AGENT cannot invoke `cmd.exe` at
# all -- the guard refuses it -- so neither `build_app.bat` nor `vcvars64.bat`
# can be run there, and three lanes in a row each spent an hour rebuilding this
# environment by hand before they could compile anything. This is that hour,
# saved. It works in the main checkout and in any worktree: the root is derived,
# never hard-coded.
#
# THE TOOLCHAIN IS PINNED, and the pin is the whole point (CLAUDE.md § Windows
# build toolchain trap): `build/CMakeCache.txt` is configured against VS2022
# BuildTools MSVC 14.44.35207. A newer Visual Studio is also installed, and
# putting ITS headers in front of the 14.44 compiler fails inside <variant> with
# a wall of bogus `std::std::` errors that read as a corrupt STL, or on MSVC
# 14.51 as `STL1001: Unexpected compiler version`. Neither names the real cause.
#
#   bash tools/build_rel.sh              # build the ProjectIo target
#   bash tools/build_rel.sh <target>     # build a named target
#
# Prints BUILD_REL_EXIT <code> last: read that, not the tail of the log, because
# a pipeline's exit code is the last command's, not the build's (CLAUDE.md
# § Background build exit code is the echo's).
set -u

TARGET="${1:-ProjectIo}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

VS='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools'
MSVC="$VS\\VC\\Tools\\MSVC\\14.44.35207"
KIT='C:\Program Files (x86)\Windows Kits\10'
SDKV='10.0.26100.0'
CL="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe"

if [ ! -x "$CL" ]; then
    echo "build_rel.sh: MSVC 14.44.35207 not found at:"
    echo "  $CL"
    echo "That exact toolset is the pin. Do not substitute a newer one -- see the header."
    echo "BUILD_REL_EXIT 9"
    exit 9
fi

# The environment vcvars64.bat would set, by hand.
export INCLUDE="$MSVC\\include;$KIT\\Include\\$SDKV\\ucrt;$KIT\\Include\\$SDKV\\shared;$KIT\\Include\\$SDKV\\um;$KIT\\Include\\$SDKV\\winrt;$KIT\\Include\\$SDKV\\cppwinrt"
export LIB="$MSVC\\lib\\x64;$KIT\\Lib\\$SDKV\\ucrt\\x64;$KIT\\Lib\\$SDKV\\um\\x64"
export LIBPATH="$LIB"
export PATH="/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64:/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja:/c/Program Files (x86)/Windows Kits/10/bin/$SDKV/x64:$PATH"
export VSCMD_ARG_TGT_ARCH=x64

cd "$ROOT" || { echo "BUILD_REL_EXIT 9"; exit 9; }

if [ ! -f build_rel/CMakeCache.txt ]; then
    echo "build_rel.sh: cold-configuring build_rel (Ninja, Release) in $ROOT"
    cmake -S . -B build_rel -G Ninja -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_COMPILER="$CL" -DCMAKE_C_COMPILER="$CL" \
        || { echo "BUILD_REL_EXIT 1"; exit 1; }
fi

cmake --build build_rel --target "$TARGET" -j
code=$?
echo "BUILD_REL_EXIT $code"
[ -f build_rel/ProjectIo.exe ] && ls -l build_rel/ProjectIo.exe
exit $code
