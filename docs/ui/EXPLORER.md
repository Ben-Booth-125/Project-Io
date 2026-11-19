# Project Io — Explorer

> **Settles:** whether an Explorer surface exists, and what became of the shell
> band it once reserved · whether pinning is a player affordance · what a fresh
> pinning or quick-navigation surface would have to do rather than reviving this one.
> **Not here:** the shell's region map (LAYOUT) · the rail's slots (MENU).
> **Confused with:** LAYOUT.md, MENU.md.

There is **no Explorer surface** in Io. The Explorer was a specified placeholder — a pinning /
quick-navigation panel reserving the right middle shell band — and its band is not reserved:
the right middle of the shell is canvas. The shell has no comms surface either: the comms dock
is retired (Ben, 2026-11-19; `LAYOUT.md` § The comms dock is retired).

**Pinning is not a player affordance.** `highlight.hpp` carries a `pinned` tier and the palette
an amber slot for it, and nothing sets either. If a pinning affordance is ever wanted it is
designed fresh — Selection-driven or its own surface — rather than reviving a reserved band. This
file exists so the name resolves; the original spec is in git history.
