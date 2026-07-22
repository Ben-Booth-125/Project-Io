@echo off
REM Build the project with the MSVC environment loaded.
REM
REM The build directory is derived from THIS SCRIPT's location (%~dp0) rather than
REM hard-coded, so moving or cloning the repo elsewhere cannot silently break it —
REM which is exactly what happened when the tree moved off C:\Claude\Project-Io.
REM
REM Any extra arguments are passed straight through to cmake, so a single target
REM can be built directly:  build_check.bat --target planetology_harness
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VSCMD_VER echo build_check.bat: vcvars64 failed to load - check the Build Tools path && exit /b 1
if not exist "%~dp0build\CMakeCache.txt" cmake -S "%~dp0." -B "%~dp0build" || exit /b 1
cmake --build "%~dp0build" --config Debug %*
