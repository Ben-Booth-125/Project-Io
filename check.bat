@echo off
REM One-command local build + test loop (BL-104): build the app + every
REM verification harness, then run the whole CTest logic tier. Mirrors the CI
REM headless job locally. See docs/development/DEVELOPMENT_PRACTICES.md.
cmake --build build --config Release
if errorlevel 1 exit /b 1
ctest --test-dir build -C Release --output-on-failure
