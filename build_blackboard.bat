@echo off
setlocal enabledelayedexpansion
REM Blackboard harness. Build output goes to build_gen\verify\ - never the system
REM temp dir, never the repo root; see tools\verify\README.md for why.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER (
    echo VCVARS_NOT_FOUND > blackboard_build.log
    exit /b 1
)
cd /d "%~dp0"
set OUT=build_gen\verify\blackboard_harness.exe
if not exist build_gen\verify\blackboard_harness md build_gen\verify\blackboard_harness
if exist "%OUT%" erase /q "%OUT%"
cl /nologo /std:c++20 /EHsc /I src tools\verify\blackboard_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp src\world\market_clearing.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\corp_ai.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\survey_system.cpp ^
   /Fo:build_gen\verify\blackboard_harness\ /Fe:%OUT% > blackboard_build.log 2>&1
if exist "%OUT%" (
    echo COMPILE_OK >> blackboard_build.log
    "%OUT%" >> blackboard_build.log 2>&1
    echo EXIT:!ERRORLEVEL! >> blackboard_build.log
) else (
    echo COMPILE_FAILED >> blackboard_build.log
)
