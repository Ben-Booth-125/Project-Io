@echo off
setlocal enabledelayedexpansion
REM Corp AI stage A harness (BL-202). Build output goes to build_gen\verify\ -
REM never the system temp dir, never the repo root; see tools\verify\README.md.
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER (
    echo VCVARS_NOT_FOUND > corp_ai_build.log
    exit /b 1
)
cd /d "%~dp0"
set OUT=build_gen\verify\corp_ai_harness.exe
if not exist build_gen\verify\corp_ai_harness md build_gen\verify\corp_ai_harness
if exist "%OUT%" erase /q "%OUT%"
cl /nologo /std:c++20 /EHsc /I src tools\verify\corp_ai_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp src\world\market_clearing.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\corp_ai.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\survey_system.cpp ^
   /Fo:build_gen\verify\corp_ai_harness\ /Fe:%OUT% > corp_ai_build.log 2>&1
if exist "%OUT%" (
    echo COMPILE_OK >> corp_ai_build.log
    "%OUT%" >> corp_ai_build.log 2>&1
    echo EXIT:!ERRORLEVEL! >> corp_ai_build.log
) else (
    echo COMPILE_FAILED >> corp_ai_build.log
)
