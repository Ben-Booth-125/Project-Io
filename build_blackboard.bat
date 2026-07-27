@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0"
del blackboard_harness.exe 2>nul
cl /nologo /std:c++20 /EHsc /I src tools\verify\blackboard_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp src\world\market_clearing.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\corp_ai.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\survey_system.cpp /Fe:blackboard_harness.exe > blackboard_build.log 2>&1
if exist blackboard_harness.exe (
    echo COMPILE_OK >> blackboard_build.log
    .\blackboard_harness.exe >> blackboard_build.log 2>&1
    echo EXIT:%ERRORLEVEL% >> blackboard_build.log
) else (
    echo COMPILE_FAILED >> blackboard_build.log
)
