---
name: commit
description: Create a git commit for the current work, using a custom message when one is supplied or a generated conventional message otherwise. Use for a general/quick commit when asked to commit or save work. For a tree that also holds unrelated or pre-existing changes, or when on the default branch, prefer scoped-commit.
---

# commit

A general-purpose commit skill. Stages the current work and commits it with a
**custom message when the user supplies one**, or a concise generated message
derived from the diff otherwise. The faster path than `scoped-commit` when the
change is self-contained and you just want it committed cleanly.

Use `scoped-commit` instead when `git status` shows changes beyond what this commit
is about (unrelated or pre-existing edits to avoid sweeping up), or when on the
default branch — that skill exists precisely to isolate the relevant files.

## Argument

The **commit message** (optional), passed verbatim as the argument:

- **Argument given →** use it as the commit message exactly as written. If it is a
  single line, that is the whole subject; multi-line text is used as subject + body.
- **No argument →** read the staged/working diff and generate a concise, imperative
  subject line (plus a short body if the change spans several concerns).

## Procedure

1. **Inspect.** `git status --short` and `git diff --stat`. If the tree holds
   unrelated or pre-existing changes, stop and recommend `scoped-commit` (or scope
   explicitly) rather than `git add -A`.
2. **Branch check.** If on the default branch (`main`), branch first unless the user
   has said to commit there — committing straight to `main` is avoided per the repo
   conventions.
3. **Stage.** Stage the files that belong to this change (explicit paths preferred
   over `git add -A` so nothing unexpected rides along).
4. **Message.** Use the supplied argument verbatim, or generate one from the diff.
   Append the repo's required trailer on its own line unless the supplied message
   already includes it:
   ```
   Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
   ```
   (Use the multiline-message technique for the shell in use — e.g. a single-quoted
   here-string in PowerShell — so `$` and backticks are not expanded.)
5. **Commit.** Run `git commit`. Never `--no-verify` / skip hooks or signing unless
   the user explicitly asked; if a pre-commit hook fails, fix the cause rather than
   bypass it.
6. **Report** the new commit's short hash and subject.

## Notes

- This skill commits; it does not push. Push only when the user asks.
- For a multi-Brief publish, the per-Brief commit format lives in CLAUDE.md
  § Publication pipeline — use that format (and one commit per Brief) rather than a
  single squashed message.
