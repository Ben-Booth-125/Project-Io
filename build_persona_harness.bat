@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d "%~dp0\build"
cmake .. > ..\persona_configure.log 2>&1
echo CONFIGURE_EXIT:%ERRORLEVEL% >> ..\persona_configure.log
cmake --build . --target persona_counsel_harness > ..\persona_build.log 2>&1
echo BUILD_EXIT:%ERRORLEVEL% >> ..\persona_build.log
if exist persona_counsel_harness.exe (
    .\persona_counsel_harness.exe >> ..\persona_build.log 2>&1
    echo RUN_EXIT:%ERRORLEVEL% >> ..\persona_build.log
)
