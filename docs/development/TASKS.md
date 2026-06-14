# Project Io — TASKS

The **active, prioritised, actionable worklist**. Unlike [`TODO.md`](TODO.md)
(described intent), every entry here is a concrete, file-scoped,
individually-buildable step ready to execute. Tasks are **promoted** from a
TODO.md item (see TODO.md § TODO vs. TASKS) and cleared as they complete — this
file is transient and is expected to be empty between work blocks.

## Task format

List tasks in **execution order**, grouped by the TODO item they were promoted
from. Each task carries:

- **A group-scoped ID letter** (A, B, C, …), so dependencies and parallel pairs
  can be named.
- **A difficulty** in brackets (the TODO.md 1–6 scale).
- **A one-line action** — imperative; what to change.
- **File scope** — the files the task is expected to touch. This is what makes
  collisions between tasks visible.
- **Dependencies** — which sibling tasks must land first (or "foundation" /
  "independent root").
- **Parallelisation** — whether it can run concurrently with a sibling, and
  whether as a sub-agent. Only true when the file scopes are **disjoint**.

End each group with a **parallelisation note**: the dependency shape and which
roots are safe to fan out. Run concurrent tasks only when their file scopes do
not overlap; keep same-file tasks sequential. Spawn a sub-agent for a parallel
branch only when it is genuinely disjoint and self-contained, and have the
integrating session run the build — sub-agents should not build or commit.

### Template

```
## <Group name> (promoted from TODO § <item>)

- **[<difficulty>] A — <action>.** Files: `<paths>`. Deps: foundation.
- **[<difficulty>] B — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with C.
- **[<difficulty>] C — <action>.** Files: `<paths>`. Deps: A. Parallel-safe with B.

Parallelisation note: A → {B, C}; B ∥ C (disjoint files). Promote D once B and C
land; D depends on both.
```

---

*No active tasks.*
