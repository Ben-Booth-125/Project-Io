@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0"
del corp_ai_harness.exe 2>nul
cl /nologo /std:c++20 /EHsc /I src tools\verify\corp_ai_harness.cpp ^
   src\world\world.cpp src\world\economy_system.cpp src\world\market_clearing.cpp ^
   src\world\budget_system.cpp src\world\building_profit.cpp src\world\corp_ai.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\survey_system.cpp /Fe:corp_ai_harness.exe > corp_ai_build.log 2>&1
if exist corp_ai_harness.exe (
    echo COMPILE_OK >> corp_ai_build.log
    .\corp_ai_harness.exe >> corp_ai_build.log 2>&1
    echo EXIT:%ERRORLEVEL% >> corp_ai_build.log
) else (
    echo COMPILE_FAILED >> corp_ai_build.log
)
