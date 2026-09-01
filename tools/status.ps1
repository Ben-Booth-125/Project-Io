<#
.SYNOPSIS
  Project Io status dashboard - what needs doing, and what was done recently.

.DESCRIPTION
  A Windows-runnable (PowerShell) companion to tools/gyre.py, for the dev box that
  has no Python. Reads docs/development/backlog.json + `git log`; no dependencies.
  Answers the two questions a fragmented multi-file backlog makes hard:
    - TO DO       : open items, grouped by priority, blocked ones flagged.
    - DONE lately : the most recently resolved items (title + date) + recent commits.

  Pure ASCII so it runs on both Windows PowerShell 5.1 and PowerShell 7 (pwsh).

.EXAMPLE
  powershell -File tools/status.ps1              # full dashboard
  powershell -File tools/status.ps1 -Todo        # just the to-do list
  powershell -File tools/status.ps1 -Recent 12   # widen the done/commit window
#>
[CmdletBinding()]
param(
    [switch]$Todo,          # show only the TO DO section
    [switch]$Done,          # show only the RECENTLY DONE section
    [int]$Recent = 8        # how many recent done-items / commits to list
)

$ErrorActionPreference = 'Stop'
# Render em-dashes and other non-ASCII correctly where the host allows; harmless if not.
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch { }
$repo        = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$backlogPath = Join-Path $repo 'docs/development/backlog.json'
$items       = (Get-Content -Raw -Encoding UTF8 $backlogPath | ConvertFrom-Json).items

# Landed items no longer live in backlog.json at all (archive_landed.js, 2026-09-01):
# the hot file is the live worklist. "DONE lately" is a question about exactly the rows
# that left, so fold the cold store back in. Whole rows only — a prose-only record from
# the earlier archive_designs pass carries no `status` and is not an item.
$archiveDir = Join-Path $repo 'docs/development/archive'
if (Test-Path $archiveDir) {
    $landed = foreach ($f in Get-ChildItem -Path $archiveDir -Filter 'backlog-design-*.json') {
        $records = (Get-Content -Raw -Encoding UTF8 $f.FullName | ConvertFrom-Json).records
        if ($null -eq $records) { continue }
        foreach ($p in $records.PSObject.Properties) {
            if ($null -eq $p.Value.status) { continue }
            $row = $p.Value
            if ($null -eq $row.id) { $row | Add-Member -NotePropertyName id -NotePropertyValue $p.Name -Force }
            $row
        }
    }
    # Hot wins on any id held both places, matching archive_store.allItems().
    $hotIds = [System.Collections.Generic.HashSet[string]]::new()
    $items | ForEach-Object { [void]$hotIds.Add($_.id) }
    $items = @($items) + @($landed | Where-Object { -not $hotIds.Contains($_.id) })
}

# DELIVERED vs CLOSED. A cancelled item is off the worklist but was never built, so
# counting it as done would inflate the one figure that says how much of the design
# exists (archive_store.js § CLOSED). `$terminal` stays DELIVERED-only and drives the
# done count and "DONE lately"; `$closed` is what removes something from TO DO.
$terminal = @('complete', 'shipped', 'delivered')
$cancelled = @('cancelled', 'purged', 'superseded')
$closed = $terminal + $cancelled
$priOrder = @('SSS', 'S', 'A', 'B', 'C', 'F')
$priRank  = @{}; for ($i = 0; $i -lt $priOrder.Count; $i++) { $priRank[$priOrder[$i]] = $i }

$open        = @($items | Where-Object { $_.status -notin $closed })
$doneItems   = @($items | Where-Object { $_.status -in $terminal })
$cancelledItems = @($items | Where-Object { $_.status -in $cancelled })
$terminalIds = [System.Collections.Generic.HashSet[string]]::new()
$doneItems | ForEach-Object { [void]$terminalIds.Add($_.id) }

# Transliterate the punctuation the design docs favour (em/en dashes, curly quotes)
# to ASCII so the dashboard is legible on every console + when redirected, regardless
# of codepage. Uses \u escapes (pure-ASCII source). Display-only; data keeps its chars.
function Ascii($s) {
    if ($null -eq $s) { return '' }
    $t = [string]$s
    foreach ($d in 0x2012, 0x2013, 0x2014, 0x2015) { $t = $t.Replace([char]$d, '-') }
    foreach ($q in 0x2018, 0x2019)                  { $t = $t.Replace([char]$q, "'") }
    foreach ($q in 0x201C, 0x201D)                  { $t = $t.Replace([char]$q, '"') }
    $t
}
function Head($t) { Write-Host ""; Write-Host "-- $t --" -ForegroundColor Cyan }
function Marker($s) { switch ($s) { 'designed' { '+' } 'design-owed' { '~' } default { '*' } } }

# Unmet dependency ids for an open item (a required item that has not landed, or a hard block).
function Blockers($it) {
    $b = @()
    foreach ($r in @($it.requires))  { if ($r -and -not $terminalIds.Contains($r)) { $b += $r } }
    foreach ($x in @($it.blocked_on)) { if ($x) { $b += $x } }
    $b
}

$showTodo = $Todo -or (-not $Todo -and -not $Done)
$showDone = $Done -or (-not $Todo -and -not $Done)

# ---- Summary ----
if (-not $Done) {
    $total = $items.Count
    $pct   = if ($total) { [math]::Round(100.0 * $doneItems.Count / $total) } else { 0 }
    Head 'Project Io - backlog'
    Write-Host ("  {0} open   {1} done   {2} total   ({3}% delivered)" -f `
        $open.Count, $doneItems.Count, $total, $pct)
    # Cancelled work is reported, never merged into "done". A backlog that shrinks
    # because items were closed unbuilt looks identical to one that shrank because
    # they shipped, unless this line exists to tell them apart.
    if ($cancelledItems.Count) {
        Write-Host ("  {0} cancelled (closed unbuilt; restore with archive_landed.js --restore)" -f `
            $cancelledItems.Count) -ForegroundColor DarkGray
    }
    $byStatus = $open | Group-Object status | Sort-Object Name
    Write-Host ("  open by status:   " + (($byStatus | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join '  ')) -ForegroundColor DarkGray
    $byPri = $open | Group-Object priority | Sort-Object { $priRank[$_.Name] }
    Write-Host ("  open by priority: " + (($byPri | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join '  ')) -ForegroundColor DarkGray
}

# ---- TO DO ----
if ($showTodo) {
    Head 'TO DO  (open items, highest priority first)'
    $active = $open | Where-Object { -not $_.parked } |
        Sort-Object @{ e = { $priRank[$_.priority] } }, @{ e = { [int]($_.id -replace '\D', '') } }
    $lastPri = $null
    foreach ($it in $active) {
        if ($it.priority -ne $lastPri) { Write-Host ""; Write-Host "  [$($it.priority)]" -ForegroundColor Yellow; $lastPri = $it.priority }
        $blk  = Blockers $it
        $name = if ($it.short_name) { "$($it.id) $($it.short_name)" } else { $it.id }
        $line = "    {0} {1}  {2}  (d{3})" -f (Marker $it.status), $name, (Ascii $it.title), $it.difficulty
        if ($blk.Count) {
            Write-Host $line -ForegroundColor DarkGray
            Write-Host ("         waiting on: " + ($blk -join ', ')) -ForegroundColor DarkYellow
        }
        else { Write-Host $line -ForegroundColor Gray }
    }
    $parked = @($open | Where-Object { $_.parked })
    if ($parked.Count) { Write-Host ""; Write-Host ("  (+ {0} parked/deferred, priority F)" -f $parked.Count) -ForegroundColor DarkGray }
    Write-Host ""
    Write-Host "  Legend: + designed (promote-ready)   ~ design-owed (design first)" -ForegroundColor DarkGray
}

# ---- RECENTLY DONE ----
if ($showDone) {
    Head "DONE lately  (last $Recent resolved items)"
    $resolved = @($doneItems | Where-Object { $_.resolved } | Sort-Object { $_.resolved } -Descending)
    $take = [math]::Min($Recent, $resolved.Count)
    if ($take -gt 0) {
        foreach ($it in $resolved[0..($take - 1)]) {
            $name = if ($it.short_name) { "$($it.id) $($it.short_name)" } else { $it.id }
            Write-Host ("    {0}  {1}  {2}" -f $it.resolved, $name, (Ascii $it.title)) -ForegroundColor Green
        }
    }
    else { Write-Host "    (no items carry a 'resolved' date yet)" -ForegroundColor DarkGray }

    Head "Recent commits (last $Recent)"
    $log = & git -C $repo log --date=short --pretty='    %h %ad  %s' -n $Recent 2>$null
    if ($log) { $log | ForEach-Object { Write-Host (Ascii $_) } }
    else { Write-Host "    (git log unavailable)" -ForegroundColor DarkGray }
    Write-Host ""
}
