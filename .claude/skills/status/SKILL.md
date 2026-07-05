---
name: status
description: Show the Project Io status dashboard — open backlog items (what needs doing, grouped by priority, blocked ones flagged) plus recently delivered items and commits. Runs tools/status.ps1 (PowerShell; reads docs/development/backlog.json + git log live, no dependencies). Use when asked "what's left", "what's the backlog", "what should I work on next", "what did we ship recently", or for any project-state overview.
---

# status

Runs the Project Io status dashboard and reports it — a live, one-command view over
`docs/development/backlog.json` + `git log`, the antidote to the multi-file backlog
fragmentation (backlog.json / REFINED.md / requirements.json / DEVLOG). Windows-native
(PowerShell); the Python original is `tools/gyre.py` (unusable on the no-Python dev box).

## When to use

- "what's left to do", "what's the backlog", "what should I work on next"
- "what did we ship / finish recently"
- any request for an overall project-state snapshot.

## How to run

Invoke via the Bash tool from the repo root (it calls `powershell.exe`):

- Full dashboard: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/status.ps1`
- To-do only:     `powershell -NoProfile -ExecutionPolicy Bypass -File tools/status.ps1 -Todo`
- Recently done:  `powershell -NoProfile -ExecutionPolicy Bypass -File tools/status.ps1 -Done` (add `-Recent <N>` to widen)

Report the output. If the user asks "what next", read the top TO DO priority group and
surface the promote-ready (`+`) items over the design-owed (`~`) ones, and call out anything
flagged `waiting on:` (an unmet dependency or hard block).

## Notes

- Reads live data, so it is always current — no stale intermediate files (item-commits.json /
  session-*.json are not consulted).
- Markers: `+` = designed (promote-ready), `~` = design-owed (design first), `*` = done.
- Sections: a progress summary (open/done/%, by-status, by-priority), TO DO (by priority,
  parked items counted separately), and DONE lately (recently resolved items + recent commits).
- Pure-ASCII output; reads `backlog.json` as UTF-8 so it is correct on Windows PowerShell 5.1
  and PowerShell 7 alike.
