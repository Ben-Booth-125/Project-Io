@echo off
REM ---------------------------------------------------------------------------
REM  syntax_check_gui.bat -- syntax-only (/Zs) compile of GUI-side translation
REM  units, for a worktree with no configured CMake build.
REM
REM  WHY THIS EXISTS (BL-692, 2026-08-29). build_harness.bat and
REM  build_lua_harness.bat both compile the world/* superset only. A change that
REM  touches src/ui/*.cpp or src/core/*.cpp is therefore NOT compiled by any
REM  harness, and in a fresh sub-agent worktree build_app.bat refuses outright
REM  ("no configured build") because the worktree has no build/ directory. That
REM  left GUI-side edits with no compile check at all short of a full CMake
REM  configure, which is minutes of work and, on a session with no network, may
REM  not be possible at all.
REM
REM  /Zs parses and type-checks without emitting objects or linking, so it
REM  catches exactly the class of error a gate-removal introduces: a name that
REM  no longer exists, an unused variable under /WX, a switch over an enum whose
REM  enumerator was deleted. It does NOT prove the program links -- run the real
REM  build_app.bat in the integrating session for that.
REM
REM  USAGE (from Git Bash, PowerShell or cmd, always from the repo root):
REM      cmd //c "tools\verify\syntax_check_gui.bat src\ui\selection_panel.cpp [more...]"
REM
REM  Include paths and the 14.44 toolchain pin mirror build_lua_harness.bat's.
REM ---------------------------------------------------------------------------

setlocal

if "%~1"=="" (
    echo ERROR: no source named.
    echo   usage: tools\verify\syntax_check_gui.bat ^<src\path\file.cpp^> [more...]
    exit /b 2
)

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
where cl >nul 2>&1
if errorlevel 1 (
    if not exist "%VCVARS%" (
        echo ERROR: cl is not on PATH and vcvars64.bat is not at "%VCVARS%".
        exit /b 1
    )
    call "%VCVARS%" -vcvars_ver=14.44 >nul 2>&1
)
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl is still not on PATH after vcvars64.bat.
    exit /b 1
)

if "%IO_DEPS_CACHE%"=="" set "IO_DEPS_CACHE=C:\Users\benbo\Project-Io\_deps_cache"

for %%D in ("%IO_DEPS_CACHE%\sol2_src\include" "%IO_DEPS_CACHE%\lua_src" ^
            "%IO_DEPS_CACHE%\sdl3\include" "%IO_DEPS_CACHE%\imgui_src") do (
    if not exist %%D (
        echo ERROR: dependency headers not found at %%D
        echo   Set IO_DEPS_CACHE to a checkout that carries them.
        exit /b 1
    )
)

echo syntax_check_gui: /Zs over %*
cl /nologo /c /Zs /std:c++20 /EHsc /MP ^
   /I src ^
   /I "%IO_DEPS_CACHE%\lua_src" ^
   /I "%IO_DEPS_CACHE%\sol2_src\include" ^
   /I "%IO_DEPS_CACHE%\sdl3\include" ^
   /I "%IO_DEPS_CACHE%\imgui_src" ^
   /I "%IO_DEPS_CACHE%\imgui_src\backends" ^
   %*
if errorlevel 1 (
    echo syntax_check_gui: SYNTAX_FAILED
    exit /b 1
)
echo syntax_check_gui: SYNTAX_OK
exit /b 0
