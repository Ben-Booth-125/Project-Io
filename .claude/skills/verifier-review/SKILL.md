---
name: verifier-review
description: Run a Project Io static code review over an integrated change before a fresh full compile. Reads the merged diff (no compile) and reports a verdict on cross-item symbol consistency, compile-by-inspection, standing invariants, the serialisation seam, and requirement coverage. Use as the cheap pre-compile gate in a Batch Delivery, after slice merges and before the integrating build. Authorising a new lens = adding it under Review lenses here.
---

# verifier-review

Reads an **integrated diff** and reasons about it **without compiling** — the cheapest
tier on the verification ladder (static review < headless harness < full build <
visual harness). Its job is to catch the *cross-item integration* class of failure —
one item building against a symbol, field, or Lua key another item was supposed to
provide — **before** the expensive full compile is spent. It is a filter, not a
guarantee: a clean verdict means "worth compiling", not "compiles". Counterpart to
`verifier-headless` (runs logic) and `verifier-visual` (runs the rendered frame).
Design context: `docs/development/DELIVERY.md` § Batch Delivery; `docs/development/
DEVELOPMENT_PRACTICES.md` § Testing.

## When to run

In a **Batch Delivery**, after the slice sub-agents have merged into the integrating
tree and **before** the integrating build (DELIVERY.md step 4 → 5 barrier). Also valid
on demand for any change that touches more than one item's files, or before any fresh
full compile late in a session when compile cost is the bottleneck. Skip it for a
single-item Light change with no cross-item surface.

## Inputs (assemble these into the brief)

Run as a **focused sub-agent** with a tight brief — give it these, not the design corpus:

1. **The integrated diff** — `git diff <batch-base>..HEAD` over the integrating tree (or
   the merged worktree). This is the object under review.
2. **The provides/consumes map** — the batch's symbol-level dependency contract from
   `REFINED.md` (each task's `provides:` / `consumes:` lines; see DELIVERY.md § collision
   map). This is the review's primary checklist.
3. **The requirement groups** — the `req/requirements.json` entries for the items in the
   batch, so coverage can be checked.
4. **One or two authority docs** the changed systems touch, plus
   `.claude/rules/io-standing-rules.md`. Not the whole `docs/` tree.

If the provides/consumes map is missing, reconstruct it from the diff (list each new/
changed public symbol per item and each external symbol each item references) and flag
that it was absent — its absence is the failure mode the symbol-level contract exists to
prevent.

## Review lenses

Tuned to this project — **not** a generic web-security review (no SQL/XSS/auth surface
in a single-player C++/Lua sim). In priority order:

1. **Cross-item symbol consistency (headline).** For every `consumes` symbol — struct
   field, enum value, function signature, recipe key, Lua table key — confirm a matching
   `provides` actually landed in the integrated diff with the expected shape. A consumer
   referencing a symbol no producer added, or added with a different signature, is a
   **Critical**.
2. **Compiles-by-inspection.** Declarations match definitions; includes/forward-decls
   present for newly-referenced types; no obvious missing-member, arity mismatch, or
   ODR/duplicate-definition across merged slices.
3. **Standing invariants** (`io-standing-rules.md`): no non-determinism introduced into
   `world/*`; no per-tile data exposed to Lua; no unprotected sol2 calls where errors can
   occur; no SQLite; nothing outside prototype scope.
4. **Serialisation seam.** A field added to a serialised struct must have matching
   read **and** write in the flat-binary path — an asymmetry corrupts the snapshot.
5. **Requirement coverage.** Does the diff actually satisfy each item's requirement
   group? Name any requirement with no corresponding change.
6. **Conventions.** `snake_case`, `m_` members, Doxygen on new public interfaces,
   canonical glossary terms (no synonym substitution).

## Output

```markdown
## Review: <batch / change name>  — verdict: GO COMPILE | FIX FIRST | NEEDS DISCUSSION

### Critical (block the compile)
| # | item | file:line | issue | lens |
|---|------|-----------|-------|------|

### Suggestions (non-blocking)
| # | item | file:line | suggestion | lens |
|---|------|-----------|------------|------|

### Provides/consumes ledger
- <symbol> — provided by <item>, consumed by <item> — OK / MISSING / SHAPE MISMATCH

### What looks sound
- <brief positives>
```

The **verdict gates the compile**: `FIX FIRST` means resolve the Criticals before
spending the full build; `GO COMPILE` means the cross-item surface is consistent and the
expensive tier is justified.

## Notes

- **No-compile filter, not a substitute.** It reduces *how often* the full compile fails
  late, which is the cost problem; it cannot prove the build. Always still compile.
- **One reviewer per batch, over the whole integrated tree** — the failure is *cross*-
  slice, so a per-slice reviewer structurally cannot see it. Give it the integrated diff,
  not one slice.
- **To add a lens:** describe it under **Review lenses** above with the failure it
  catches, so the check stays a permanent, reusable asset.
