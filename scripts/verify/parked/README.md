# Parked visual checks

A script here is **excluded from `--verify-all` and routine passes by location** —
the batch enumerates only `scripts/verify/*.lua`, so parking is visible in the
tree rather than encoded in a flag someone has to remember.

A parked check is a **debt, not a category** (the same rule CMakeLists.txt states
for parked ctest harnesses): each one is a hole in the golden coverage, carries a
backlog item naming what must be fixed to bring it back, and is un-parked as part
of landing that item.

| Script | Why parked | Debt item |
|---|---|---|
| `history_ages.lua` | Its lazy Era −1 time-lapse run costs >8 min on the Debug build (both the 3× map and the 70%-area resize); it silently hung two full visual passes on 2026-08-14. | BL-425 (ages lazy-sim cost) |
