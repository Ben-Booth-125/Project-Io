@echo off
REM ===========================================================================
REM Re-run CMake configure in build\ against the SAME pinned toolchain
REM build_app.bat uses.
REM
REM Why this exists: tools/verify/*.cpp is a GLOB, so a newly added harness is
REM not a target until CMake re-runs, and `nmake <new_harness>` fails with
REM "don't know how to make" - which reads like a broken build rather than a
REM missing configure step. CONFIGURE_DEPENDS re-globs on build for some
REM generators; NMake Makefiles is not reliably one of them.
REM
REM Same trap as build_app.bat guards: pin VS2022 BuildTools MSVC 14.44 and do
REM NOT fall back to any newer Visual Studio's vcvars, or the configure re-runs
REM the compiler check against mismatched headers.
REM ===========================================================================
setlocal

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo ERROR: VS2022 BuildTools vcvars64.bat not found at:
    echo   "%VCVARS%"
    exit /b 1
)
call "%VCVARS%" >nul 2>&1
if not defined VSCMD_VER (
    echo ERROR: vcvars64.bat did not initialise the environment.
    exit /b 1
)

cd /d "%~dp0build"
if not exist CMakeCache.txt (
    echo ERROR: no configured build in %CD%.
    exit /b 1
)

cmake .
set RC=%ERRORLEVEL%
if %RC%==0 (
    echo RECONFIGURE_OK
) else (
    echo RECONFIGURE_FAILED rc=%RC%
)
exit /b %RC%
