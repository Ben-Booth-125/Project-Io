@echo off
REM BL-392 + the D4 import tariff: money/goods conservation across both flows.
REM See tools\verify\README.md for why the output goes to build_gen\verify\.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0"
if not exist build_gen\verify\money_conservation md build_gen\verify\money_conservation
cl /nologo /std:c++20 /EHsc /I src tools\verify\money_conservation.cpp ^
   src\world\world.cpp src\world\economy_system.cpp ^
   src\world\market_clearing.cpp src\world\budget_system.cpp ^
   src\world\corp_command.cpp src\world\construction.cpp src\world\placement_rules.cpp ^
   src\world\condition_set.cpp src\world\survey_system.cpp src\world\logistics.cpp ^
   src\world\unit_roster.cpp src\world\law.cpp src\world\building_profit.cpp ^
   src\world\corp_ai.cpp src\world\stance.cpp src\world\supply_system.cpp ^
   src\world\tech_gate.cpp src\world\river_generation.cpp src\world\orbital_system.cpp ^
   /Fo:build_gen\verify\money_conservation\ /Fe:build_gen\verify\money_conservation.exe
if exist build_gen\verify\money_conservation.exe build_gen\verify\money_conservation.exe
