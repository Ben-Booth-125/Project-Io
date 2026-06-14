---
name: scoped-commit
description: Commit the current work as a clean, scoped commit when the working tree also holds unrelated or pre-existing changes. Use when asked to commit/save work and `git status` shows files beyond what this task touched, when on the default branch, or whenever a commit should include only the relevant changes and not sweep up someone else's in-progress edits.
---

# scoped-commit

A repeatable method for committing *just the work that belongs to the current
task*, without bundling unrelated or pre-existing changes that happen to share the
working tree. The goal: a reviewer sees one coherent change, and nobody's
in-progress edits get committed (or reverted) by accident.

## When to use it

- The working tree has changes beyond what the current task produced.
- You are on the default branch (`main`/`master`) and should branch first.
- A commit was requested and you want it scoped, not a blanket `git add -A`.

If the user explicitly says "commit everything" / "keep all changes", skip the
scoping and just stage all and commit (still apply steps 1, 2, and 5).

## Procedure

1. **Survey the tree.** `git status --porcelain` (and the start-of-session git
   snapshot, if available) to separate *your* changes from changes that were
   already there. Anything modified before you started is **not yours** — leave it
   unless it is intertwined (step 4).

2. **Branch if on the default branch.** Never commit straight to `main`/`master`:
   `git checkout -b <type>/<short-topic>` (e.g. `feature/...`, `fix/...`).

3. **Stage explicitly, by path — never `git add -A`/`.`** List exactly the files
   your task created or modified:
   `git add path/to/a path/to/b ...`. Add new untracked files individually too
   (e.g. `git add .claude/skills/foo/SKILL.md`) so sibling untracked files — local
   settings, scratch output — stay out.

4. **Handle intertwined files honestly.** If a file you edited also contains a
   pre-existing hunk that is not yours, you cannot stage your change without it
   (reverting the other hunk would destroy that work). Include the whole file and
   **flag it in your summary** — do not silently bundle, and do not revert others'
   work to "clean" the file.

5. **Drop anything that got carried in.** A rename or change already in the index
   (e.g. inherited when branching) will ride along. Unstage it:
   `git restore --staged <paths>`. Re-check `git diff --cached --name-status`
   before committing — the staged set should be *only* your scope.

6. **Commit with a correct multi-line message.** Use the tool that matches the
   shell you are actually invoking:
   - **Bash tool** → a quoted heredoc (single-quoted `EOF` so `$`/backticks stay
     literal):
     ```
     git commit -F - <<'EOF'
     Short imperative subject

     Body explaining the why, wrapped ~72 cols.

     Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
     EOF
     ```
   - **PowerShell tool** → a single-quoted here-string, closing `'@` at column 0:
     ```
     git commit -m @'
     Short imperative subject

     Body ...

     Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
     '@
     ```
   Do **not** mix them: PowerShell here-string syntax (`@'...'@`) passed to the
   Bash tool is parsed as a literal `@` plus a quoted string, which mangles the
   subject line. If a message comes out wrong, fix it with
   `git commit --amend -F -` (prefer amend over a follow-up "fix message" commit).

7. **Verify and report.** `git log -1 --pretty=format:'%h%n%s%n---%n%b'` to confirm
   the message, and `git show --stat HEAD` for the file set. Tell the user the
   commit hash, the branch, what was deliberately left out, and any intertwined
   file from step 4. Push or open a PR only if asked.

## Conventions

- Commit messages end with the project trailer:
  `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`.
- Prefer one commit per coherent change. Amend rather than stacking a "fixup".
- Never use `--no-verify` or skip hooks unless the user explicitly asks.
