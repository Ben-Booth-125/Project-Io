@echo off
REM ---------------------------------------------------------------------------
REM  build_harness.bat -- Bash-safe entry point for tools/verify/build_harness.js
REM
REM  WHY THIS EXISTS (Sprint 20, 2026-08-26; NR-658). build_harness.js spawns a
REM  nested `cmd /c call vcvars64.bat && cl ...`. That nesting is mangled when the
REM  outer process is Git Bash, and the compile dies with
REM      'cl' is not recognized as an internal or external command
REM  which reads as a broken harness rather than a broken invocation. Three
REM  separate agent sessions hit it and each burned time rediscovering the same
REM  workaround, which is the signal that it wanted a committed fix.
REM
REM  The fix is to establish the MSVC environment ONCE, in this shell, before node
REM  runs -- so the inner spawn inherits a PATH that already has cl on it instead
REM  of trying to build one across a mangled quoting boundary.
REM
REM  USAGE (from Git Bash, PowerShell or cmd, always from the repo root):
REM      cmd //c tools\verify\build_harness.bat <harness_name> [more names...]
REM      tools\verify\build_harness.bat <harness_name>
REM
REM  Output location, arguments and exit code are build_harness.js's own -- this
REM  wrapper adds nothing but the environment. Harnesses still land in
REM  build_gen\verify\<name>.exe (%TEMP% is banned as a harness target).
REM
REM  The 14.44 BuildTools pin is deliberate and is build_app.bat's; read its
REM  header before pointing this at a newer vcvars.
REM ---------------------------------------------------------------------------

setlocal

if "%~1"=="" (
    echo ERROR: no harness named.
    echo   usage: tools\verify\build_harness.bat ^<harness_name^> [more names...]
    exit /b 2
)

set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" (
    echo ERROR: VS2022 BuildTools vcvars64.bat not found at:
    echo   "%VCVARS%"
    echo Install the VS2022 Build Tools, or edit this path to match the compiler
    echo you do have - see build_app.bat's header before pointing it at a newer one.
    exit /b 1
)

REM Only if cl is not already on PATH -- calling vcvars twice is harmless but slow,
REM and a caller who has already run it may have pinned a toolset on purpose.
where cl >nul 2>&1
if errorlevel 1 call "%VCVARS%" >nul 2>&1

where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl is still not on PATH after calling vcvars64.bat.
    exit /b 1
)

node "%~dp0build_harness.js" %*
exit /b %ERRORLEVEL%
