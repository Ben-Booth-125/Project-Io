@echo off
REM ---------------------------------------------------------------------------
REM  build_lua_harness.bat -- build ONE tools/verify/<name>.cpp that needs a LIVE
REM  Lua state, without a CMake configure and without the network.
REM
REM  WHY THIS EXISTS (Sprint 20, BL-635). build_harness.bat/.js build the
REM  SDL/Lua-free world superset and refuse by name the handful of harnesses that
REM  genuinely load scripts/*.lua -- pregame_balance_harness, condition_set_harness,
REM  mercenary_contract_harness, and now spawn_solvency. NR-558 records that gap.
REM  The only documented route for those was "build it through CMake", which in a
REM  fresh sub-agent worktree means a configure that tries to download ~120 MB of
REM  SDL3/Lua/sol2/ImGui -- refused outright by some session network policies.
REM
REM  This closes the gap the same way build_harness.bat closed its own: compile
REM  the world superset PLUS the five sol2/Lua translation units the superset
REM  excludes, plus src/scripting/lua_state.cpp, plus Lua itself, against the
REM  dependency sources already sitting in the shared _deps_cache. No CMake, no
REM  network, and no dependency on the main tree's build/ directory.
REM
REM  RELEASE, DELIBERATELY. The main tree configures Debug, so its lua54.lib is
REM  /MDd and linking it forces the whole harness to /MDd /Od. A generation +
REM  80-tick warm start over a seed sweep is ~60x slower there -- measured at
REM  ~10 minutes PER SEED, which makes a sweep unrunnable. Compiling Lua from the
REM  cached sources costs about 8 seconds and buys /O2 /MD throughout. The Lua
REM  objects are cached in build_gen\verify\_lua_obj and reused.
REM
REM  USAGE (from Git Bash, PowerShell or cmd, always from the repo root):
REM      cmd //c tools\verify\build_lua_harness.bat <harness_name>
REM
REM  Output lands in build_gen\verify\<name>.exe, the same path-scoped target
REM  build_harness.js uses (%TEMP% is banned as a harness target).
REM ---------------------------------------------------------------------------

setlocal enabledelayedexpansion

if "%~1"=="" (
    echo ERROR: no harness named.
    echo   usage: tools\verify\build_lua_harness.bat ^<harness_name^>
    exit /b 2
)
set "NAME=%~1"
set "SRC=tools\verify\%NAME%.cpp"
if not exist "%SRC%" (
    echo ERROR: %SRC% does not exist.
    exit /b 2
)

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
where cl >nul 2>&1
if errorlevel 1 (
    if not exist "%VCVARS%" (
        echo ERROR: cl is not on PATH and vcvars64.bat is not at "%VCVARS%".
        exit /b 1
    )
    call "%VCVARS%" >nul 2>&1
)
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl is still not on PATH after vcvars64.bat.
    exit /b 1
)

REM --- the dependency cache --------------------------------------------------
REM IO_DEPS_CACHE mirrors CMakeLists' own env override; the main checkout's
REM _deps_cache is the default, exactly as CMakeLists resolves it.
if "%IO_DEPS_CACHE%"=="" set "IO_DEPS_CACHE=C:\Users\benbo\Project-Io\_deps_cache"

if not exist "%IO_DEPS_CACHE%\sol2_src\include\sol\sol.hpp" (
    echo ERROR: sol2 headers not found under "%IO_DEPS_CACHE%\sol2_src\include".
    echo   Set IO_DEPS_CACHE to a checkout that carries lua_src / sol2_src.
    exit /b 1
)
if not exist "%IO_DEPS_CACHE%\lua_src\lua.h" (
    echo ERROR: Lua sources not found under "%IO_DEPS_CACHE%\lua_src".
    exit /b 1
)

set "OUTDIR=build_gen\verify"
set "OBJDIR=%OUTDIR%\%NAME%.obj"
set "LUAOBJ=%OUTDIR%\_lua_obj"
if not exist "%OBJDIR%" mkdir "%OBJDIR%" 2>nul
if not exist "%LUAOBJ%" mkdir "%LUAOBJ%" 2>nul
if exist "%OUTDIR%\%NAME%.exe" del /q "%OUTDIR%\%NAME%.exe"

REM --- Lua, compiled once and cached -----------------------------------------
REM lua.c (standalone interpreter), luac.c (compiler) and onelua.c (amalgamation,
REM which includes lua.c) are excluded, exactly as CMakeLists excludes them.
if not exist "%LUAOBJ%\lapi.obj" (
    echo build_lua_harness: compiling Lua 5.4 ^(cached after this^)
    set "LUAC="
    for %%F in ("%IO_DEPS_CACHE%\lua_src\*.c") do (
        if /I not "%%~nxF"=="lua.c" if /I not "%%~nxF"=="luac.c" if /I not "%%~nxF"=="onelua.c" (
            set "LUAC=!LUAC! "%%F""
        )
    )
    cl /nologo /c /O2 /MD /MP /I "%IO_DEPS_CACHE%\lua_src" !LUAC! /Fo:"%LUAOBJ%\\"
    if errorlevel 1 (
        echo build_lua_harness: LUA_COMPILE_FAILED
        exit /b 1
    )
)

REM The world superset -- ALL of src/world/*.cpp this time (the five sol2 TUs
REM included), plus the scripting bridge. A glob, never a hand-picked list: the
REM hand-picked list in CMakeLists rotted twice.
set "WORLD="
for %%F in (src\world\*.cpp) do set "WORLD=!WORLD! %%F"

set "LUAOBJS="
for %%F in ("%LUAOBJ%\*.obj") do set "LUAOBJS=!LUAOBJS! "%%F""

echo build_lua_harness: %NAME% ^<- world superset + sol2/Lua TUs (cl, /O2 /MD)
cl /nologo /std:c++20 /EHsc /MP /MD /O2 /DNDEBUG /DSOL_ALL_SAFETIES_ON=1 ^
   /I src /I tools\verify /I "%IO_DEPS_CACHE%\lua_src" /I "%IO_DEPS_CACHE%\sol2_src\include" ^
   "%SRC%" !WORLD! src\scripting\lua_state.cpp ^
   /Fo:"%OBJDIR%\\" /Fe:"%OUTDIR%\%NAME%.exe" ^
   /link !LUAOBJS!

if errorlevel 1 (
    echo build_lua_harness: COMPILE_FAILED
    exit /b 1
)
if not exist "%OUTDIR%\%NAME%.exe" (
    echo build_lua_harness: COMPILE_FAILED ^(no executable^)
    exit /b 1
)
echo build_lua_harness: COMPILE_OK  %OUTDIR%\%NAME%.exe
exit /b 0
