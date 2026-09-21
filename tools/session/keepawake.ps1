# keepawake.ps1 — hold the PC awake for exactly as long as a long harness runs.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/session/keepawake.ps1 [-Process player_seed_sweep]
#
# WHY (2026-09-17, 2026-09-21). The PC sleeps after 4 h idle on AC; a sleep inside a timing sweep
# pauses a row mid-tick and records an hours-long "tick" that has to be re-run. No host tool exposes
# keep-awake, so this holds SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED) on its own
# thread while any process of the given name runs, then releases it. It changes no power setting:
# the request dies with this process.
#
# RUN IT FROM THIS FILE. Inlining the body in a bash `powershell -Command '...'` string breaks on the
# nested quotes (Add-Type reports a positional-parameter error) and the sweep runs unheld. Start the
# harness first, then this in the background; its first line must read "held". `powercfg /requests`
# needs admin, so it cannot confirm the hold.
param([string]$Process = 'player_seed_sweep', [int]$GraceSeconds = 30)

Add-Type -Name P -Namespace KeepAwake -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint f);'
$prev = [KeepAwake.P]::SetThreadExecutionState([uint32]2147483649)   # 0x80000001
"held $(Get-Date -Format s) prev=$prev watching '$Process'"
Start-Sleep $GraceSeconds                                            # let the harness start
while (Get-Process $Process -ErrorAction SilentlyContinue) { Start-Sleep 60 }
[void][KeepAwake.P]::SetThreadExecutionState([uint32]2147483648)     # 0x80000000
"released $(Get-Date -Format s)"
