@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER echo build_check.bat: vcvars64 failed to load - check the Build Tools path && exit /b 1
cmake --build "C:\Claude\Project-Io\build" --config Debug
