@echo off
setlocal enabledelayedexpansion
REM Generic world-superset harness build: %1 = harness name (tools\verify\%1.cpp).
REM Mirrors CMakeLists' io_world_obj source set (all src\world\*.cpp minus the
REM four sol2/Lua TUs), so the TU list cannot drift stale.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER (
    echo VCVARS_NOT_FOUND
    exit /b 1
)
cd /d "%~dp0"
set NAME=%1
set OUT=build_gen\verify\%NAME%.exe
if not exist build_gen\verify\%NAME% md build_gen\verify\%NAME%
if exist "%OUT%" erase /q "%OUT%"
set SRCS=
for %%f in (src\world\*.cpp) do (
    set N=%%~nf
    if /I not "!N!"=="recipe_registry" if /I not "!N!"=="works_registry" if /I not "!N!"=="tech_tree" if /I not "!N!"=="world_gen_config" set SRCS=!SRCS! %%f
)
cl /nologo /std:c++20 /EHsc /MP /O2 /I src /I tools\verify tools\verify\%NAME%.cpp !SRCS! ^
   /Fo:build_gen\verify\%NAME%\ /Fe:%OUT% > build_gen\verify\%NAME%.log 2>&1
if exist "%OUT%" (
    echo COMPILE_OK
) else (
    echo COMPILE_FAILED
    exit /b 1
)
