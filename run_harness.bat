@echo off
setlocal enabledelayedexpansion
REM Layer 3 economy harness. Build output goes to build_gen\verify\ - never the
REM system temp dir, never the repo root; see tools\verify\README.md for why.
REM TU list includes the BL-202 ripple (anything linking economy_system.cpp also
REM needs building_profit / corp_ai / corp_command / construction /
REM placement_rules / survey_system).
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER (
    echo VCVARS_NOT_FOUND > harness.log
    exit /b 1
)
cd /d "%~dp0"
set OUT=build_gen\verify\econ_harness.exe
if not exist build_gen\verify\econ_harness md build_gen\verify\econ_harness
if exist "%OUT%" erase /q "%OUT%"
cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   src\world\building_profit.cpp src\world\corp_ai.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp ^
   src\world\placement_rules.cpp src\world\survey_system.cpp ^
   /Fo:build_gen\verify\econ_harness\ /Fe:%OUT% > harness.log 2>&1
if exist "%OUT%" (
    echo COMPILE_OK >> harness.log
    "%OUT%" >> harness.log 2>&1
    echo EXIT:!ERRORLEVEL! >> harness.log
) else (
    echo COMPILE_FAILED >> harness.log
)
