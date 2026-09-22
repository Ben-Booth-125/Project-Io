@echo off
REM ===========================================================================
REM Incremental build of the ProjectIo app on Windows.
REM
REM   build_app.bat            - build the ProjectIo target
REM   build_app.bat <target>   - build a named target (e.g. econ_harness)
REM
REM Two traps this script exists to close:
REM
REM 1. TOOLCHAIN PINNING. build\CMakeCache.txt is configured against VS2022
REM    BuildTools MSVC 14.44.35207. If a newer Visual Studio is also installed,
REM    calling its vcvars first puts the NEWER headers on INCLUDE while CMake
REM    still invokes the 14.44 compiler. The mismatch detonates inside
REM    <variant> with a wall of bogus "std::std::" / C2098 / C2059 errors that
REM    look like a corrupt STL rather than a config problem. So pin BuildTools
REM    explicitly and do NOT fall back to any other install.
REM
REM 2. THE BUILD DIRECTORY. This script builds build\ (Debug; NMake) - the
REM    debugging tree. The PLAY build is build_rel\ (Release; Ninja), rebuilt
REM    with: cmake --build build_rel --target ProjectIo -j  (after vcvars).
REM    These two are the only build trees (2026-08-25 trim; build_live's
REM    running-copy convention retired with it).
REM
REM Output: build\ProjectIo.exe (single-config Debug; NMake Makefiles).
REM Verify runs read scripts from build\scripts\, which the build refreshes.
REM ===========================================================================
setlocal

REM Quote the whole assignment: the "(x86)" in the path is parsed as a block
REM delimiter otherwise, and cmd dies with "\Microsoft was unexpected at this time."
set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo ERROR: VS2022 BuildTools vcvars64.bat not found at:
    REM Quote the expansion: inside a parenthesised block the "(x86)" in the
    REM expanded path would close the block early ("\Microsoft was unexpected").
    echo   "%VCVARS%"
    echo This is the toolchain build\CMakeCache.txt was configured with. Either
    echo install VS2022 BuildTools, or reconfigure the cache from scratch against
    echo the compiler you do have - do not just point this at a newer vcvars.
    exit /b 1
)
call "%VCVARS%" >nul 2>&1
if not defined VSCMD_VER (
    echo ERROR: vcvars64.bat did not initialise the environment.
    exit /b 1
)

REM COLD-CONFIGURE IF build\ IS ABSENT (2026-08-31, NR-755).
REM
REM build\ is gitignored, so a FRESH GIT WORKTREE has none and this script used
REM to hard-fail here. That is not a cosmetic inconvenience: every worktree
REM sub-agent then improvises its own configure, and a different generator can
REM produce different codegen - which is exactly what forced an independent
REM re-verification of a state_hash result in sprint 26 wave 1. One agent
REM reached for Ninja; the answer happened to match, but nobody could know that
REM without redoing it.
REM
REM So configure here, with the SAME generator and build type the script has
REM always used, and every worktree gets a toolchain identical to main's.
cd /d "%~dp0"
if not exist "build\CMakeCache.txt" (
    echo build_app: no configured build - cold-configuring NMake Makefiles / Debug
    cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
    if errorlevel 1 (
        echo BUILD_FAILED configure
        exit /b 1
    )
)

cd /d "%~dp0build"
if not exist CMakeCache.txt (
    echo ERROR: no configured build in %CD% and the cold-configure above did not produce one.
    exit /b 1
)

set TARGET=%1
if "%TARGET%"=="" set TARGET=ProjectIo

nmake %TARGET%
set RC=%ERRORLEVEL%
if %RC%==0 (
    echo BUILD_OK %TARGET%
) else (
    echo BUILD_FAILED %TARGET% rc=%RC%
)
exit /b %RC%
