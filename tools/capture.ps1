<#
.SYNOPSIS
    Build (optional), launch Project Io, trigger an in-app F12 screenshot, and
    convert it to PNG.

.DESCRIPTION
    Wraps the whole screenshot dev loop into one command so it can be authorized
    once in .claude/settings.local.json. Both you and Claude run the same script.

    Steps:
      1. Stop any stale ProjectIo.exe (so a rebuild can overwrite the binary).
      2. Optionally rebuild (-Build).
      3. Launch the app, send F12 (its built-in capture key), then close it.
      4. Convert the newest BMP under build/Debug/screenshots to PNG.
      5. Print the PNG path.

    The window is closed after each capture so repeated runs stay clean.

.PARAMETER Build
    Rebuild the Debug configuration before launching.

.PARAMETER Config
    CMake configuration to build/run. Defaults to Debug.

.PARAMETER WaitMs
    Milliseconds to let the window settle before sending F12. Default 2000.

.PARAMETER KeepBmp
    Keep the source .bmp alongside the .png (default removes it).

.EXAMPLE
    ./tools/capture.ps1 -Build
#>
[CmdletBinding()]
param(
    [switch]$Build,
    [string]$Config = "Debug",
    [int]$WaitMs = 2000,
    [switch]$KeepBmp
)

$ErrorActionPreference = "Stop"

$root    = Split-Path $PSScriptRoot -Parent
$exeDir  = Join-Path $root "build/$Config"
$exe     = Join-Path $exeDir "ProjectIo.exe"
$shotDir = Join-Path $exeDir "screenshots"

# 1. Stop any running instance so the binary is not locked.
Get-Process ProjectIo -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

# 2. Optional rebuild.
if ($Build) {
    Write-Host "Building ($Config)..."
    cmake --build (Join-Path $root "build") --config $Config
    if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)." }
}

if (-not (Test-Path $exe)) {
    throw "Executable not found: $exe. Run with -Build first."
}

# 3. Launch, focus, capture, close.
$since = Get-Date
$proc  = Start-Process $exe -WorkingDirectory $exeDir -PassThru
Start-Sleep -Milliseconds $WaitMs

$wshell = New-Object -ComObject WScript.Shell
$null = $wshell.AppActivate("Project Io")
Start-Sleep -Milliseconds 400
$wshell.SendKeys("{F12}")
Start-Sleep -Milliseconds 800

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# 4. Find the freshest BMP and convert to PNG.
if (-not (Test-Path $shotDir)) { throw "No screenshots directory produced - F12 capture did not run." }

$bmp = Get-ChildItem $shotDir -Filter *.bmp |
       Where-Object { $_.LastWriteTime -ge $since } |
       Sort-Object LastWriteTime -Descending |
       Select-Object -First 1

if (-not $bmp) { throw "No new .bmp captured this run." }

Add-Type -AssemblyName System.Drawing
$png = [System.IO.Path]::ChangeExtension($bmp.FullName, ".png")
$img = [System.Drawing.Image]::FromFile($bmp.FullName)
try { $img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $img.Dispose() }

if (-not $KeepBmp) { Remove-Item $bmp.FullName -Force }

# 5. Report. The final line is just the path, for easy scripting.
Write-Host "Screenshot ready:"
$png
