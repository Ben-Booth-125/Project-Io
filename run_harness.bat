@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
del econ_harness.exe 2>nul
cl /nologo /std:c++20 /EHsc /I src tools\verify\econ_harness.cpp src\world\world.cpp src\world\economy_system.cpp src\world\market_clearing.cpp src\world\budget_system.cpp /Fe:econ_harness.exe > harness.log 2>&1
if exist econ_harness.exe (
    echo COMPILE_OK >> harness.log
    .\econ_harness.exe >> harness.log 2>&1
    echo EXIT:%ERRORLEVEL% >> harness.log
) else (
    echo COMPILE_FAILED >> harness.log
)
