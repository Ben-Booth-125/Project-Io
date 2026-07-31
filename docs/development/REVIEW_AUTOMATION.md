# Review & Merge Automation — general practice

How to automate the PR review/accept loop **without losing the human as the design
authority**. Written to be **repo-agnostic** — copy this file (and the two artifacts it
names) into any project and adjust the anchors. The Project-Io specifics are quarantined
in the last section.

## The principle: author ≠ reviewer

The bias in "the thing that wrote the code reviews the code" does **not** come from it
being the same model. It comes from two things:

1. **Shared context** — the author carries every rationalisation for why the change is
   correct, and shares its own blind spots.
2. **A confirmatory mandate** — "check my work" pulls toward "confirm it's done".

So the fix is not a label ("review mode"); it is:

- **Fresh context** — the reviewer never saw the authoring conversation, only the
  artifacts (diff + spec + standing rules).
- **Adversarial mandate** — the reviewer is told to *refute* ("assume it's broken, find
  the failing input"), not to bless.
- **Spec as the shared truth, not the author's narrative** — give the reviewer the
  requirements, *not* the author's "here's why it's fine". Feed it the justification and
  you re-import the bias.

**Honest limit.** Two same-model agents in separate contexts decorrelate *anchoring and
intent* bias, but still share **model-level (weight) blind spots** — they can miss the
same class of bug. The only true defense against that is a **different model**. So reserve
a **cross-model** review (e.g. GitHub Copilot, or another LLM) for the load-bearing work.

## The non-negotiable

**The author never approves or merges its own change.** The session (or person) that wrote
the code may open the PR and may run cheap self-checks, but the *approving* verdict comes
from an independent actor. Author and approver must not be the same.

## Tiering (size the rigour to the change)

| Change class | Review |
|---|---|
| **Light** — one-liner, doc tweak, obvious cleanup | CI green is enough; optional quick self-check. |
| **Full** — real logic, > ~2 files, or touches a load-bearing seam | **Independent cold + adversarial** review (the `code-reviewer` agent / the CI action), **plus cross-model** (Copilot) review. |

The seams that always earn the full treatment are the project's own hard-rule surfaces
(for Project-Io: the economy, the save/serialisation format, determinism — see
`.claude/rules/`). When unsure, treat it as Full.

## The pipeline

```
author opens PR  ──►  CI gate (hard)  ──►  independent review posts findings
   (never                │ build + tests        │ cold + adversarial agent;
    self-approves)       │ must be green         │ cross-model (Copilot) on Full
                         ▼                       ▼
                 branch protection         human reads the digest, decides
                 blocks red merges                │
                                                  ▼
                                  approve ──► auto-merge fires on green ──► branch deleted
                                                  │
                                          (Claude babysits: autofix CI,
                                           answer review threads)
```

- **CI as the keystone.** Branch-protect the default branch to require the build/test
  checks. This is what makes automating *accept* safe — red can't merge, so approval stops
  being where you catch breakage.
- **Independent review** runs on PR-open (the CI action below), or is spawned as a separate
  Claude invocation using the `code-reviewer` agent. It **posts findings; it does not
  approve.**
- **Cross-model** review (Copilot) is requested on Full-mode PRs for weight-level
  decorrelation.
- **Auto-merge** arms the PR to merge itself once checks pass and you've approved — so your
  only manual act is a 30-second approve, from anywhere.
- **Babysit** — subscribe the authoring session to PR activity so CI failures and review
  comments get handled without you.

## The artifacts (portable)

1. **`.claude/agents/code-reviewer.md`** — the cold, adversarial reviewer persona. Read-only
   tools; reports findings; never approves/merges/pushes. Used by the CI action and by any
   manual `Agent`/Task review pass. Repo-agnostic.
2. **`.github/workflows/claude-review.yml`** — runs the reviewer automatically on PR-open.
   Requires one repo secret (the Anthropic API key or a Claude Code OAuth token — see the
   workflow header). This is the only piece that costs API tokens per PR; keep it on Full
   PRs (e.g. gate by a label or path filter) if cost matters.
3. **One-time GitHub config** (set via the UI or `gh api`, below).

## One-time GitHub config

These are GitHub-side settings — set once per repo, independent of any tooling. Replace
`OWNER/REPO` and `main` as needed. (Run locally where `gh` is authenticated; the in-session
GitHub tools generally cannot set branch protection.)

**Branch protection** — require the build checks green before merge:

```sh
gh api -X PUT repos/OWNER/REPO/branches/main/protection \
  -H "Accept: application/vnd.github+json" \
  -f 'required_status_checks[strict]=true' \
  -f 'required_status_checks[contexts][]=Linux GCC 13 (build + headless harnesses)' \
  -f 'required_status_checks[contexts][]=Windows (build)' \
  -F 'enforce_admins=false' \
  -F 'required_pull_request_reviews[required_approving_review_count]=1' \
  -F 'restrictions=' -F 'required_linear_history=true'
```

(Use the **exact check names** from your CI job `name:` fields. Leave the advisory
visual-verify job **out** of the required set — goldens are brittle. For a true solo repo
you may set `required_approving_review_count=0` and rely on CI + the independent review
comment; keeping `1` preserves the explicit human gate.)

**Allow auto-merge + auto-delete branches:**

```sh
gh api -X PATCH repos/OWNER/REPO \
  -F allow_auto_merge=true -F delete_branch_on_merge=true -F allow_squash_merge=true
```

**Arm auto-merge on a PR** (merges itself once checks pass + approved):

```sh
gh pr merge <number> --squash --auto --delete-branch
```

**Your side, reduced to one command** — an approve-and-ship alias:

```sh
gh alias set shipit '!gh pr review --approve "$1" && gh pr merge --squash --auto --delete-branch "$1"'
# usage: gh shipit <number>
```

## Porting to another repo — checklist

1. Copy `.claude/agents/code-reviewer.md` and this file.
2. Add the CI review workflow (`.github/workflows/claude-review.yml`) and set the API-key /
   OAuth secret.
3. Set branch protection on the default branch with **that repo's** check names.
4. Enable auto-merge + delete-branch-on-merge.
5. Point the reviewer at that repo's standing-rules file(s) (the agent reads
   `CLAUDE.md` / `.claude/rules/` generically; no edit needed if those exist).
6. Decide the accept tier (default: auto-merge Light/docs, human-gate Full).

## Project-Io anchors

> **Current reality (2026-07-31).** The pipeline above is the method; most of it is not live here.
> Branch protection is impossible on this plan — BL-105 (main merge gate) recorded the HTTP 403 on
> 2026-07-05 (private repo, free plan) — so merges are **local and direct to `main`**, never
> through a PR. The last PR was #25 (2026-07-19); the PR-triggered `claude-review.yml` has not
> fired since (its last run was on PR #24's branch, 2026-07-18). The **live** review gate is the
> `verifier-review` skill (DELIVERY step 4a) plus the requirements ledger
> (`req/requirements.json`); the generic method below stays as the target state, not the practice.

- **Standing rules** the reviewer must enforce: `.claude/rules/io-standing-rules.md`
  (determinism, no tile data to Lua, no unprotected sol2, flat-binary serialisation, AI a
  data-model stub) and the lifecycle in `docs/development/DELIVERY.md`.
- **Spec source** for "is every requirement met": `docs/development/req/requirements.json`
  (the row's `verification` must have actually been *run*).
- **CI checks** live in `.github/workflows/build.yml` — job names: *Linux GCC 13/14 (build
  + headless harnesses)*, *Windows (build)*, *Linux visual-verify (advisory)*.
- **Accept policy (current):** auto-merge Light / doc-only; **human-gate Full-mode** work
  touching the economy / save-format / determinism seams. This mirrors Rule 0.
- The author session must **not** post an approving self-review; it runs `verifier-review`
  as a cheap *pre-compile* filter (DELIVERY step 4a), but the **deciding** review is the
  independent `code-reviewer` pass + your approval.
