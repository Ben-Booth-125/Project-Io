#!/usr/bin/env python3
"""gyre — diagnostic tools for an agenticprocess-backlog repo (Project-Io / Project-Gyre).

Answers the simple questions without bothering Claude:
  how many items are left, how many are delivered, how many commits,
  and by which session.

Usage:
  tools/gyre.py                 # full dashboard (default)
  tools/gyre.py items           # backlog: open / delivered breakdown
  tools/gyre.py commits         # commit counts, grouped by session
  tools/gyre.py sessions        # per-session commit activity
  tools/gyre.py watch [SECS]    # live dashboard, refreshes every SECS (default 15)

Reads docs/development/backlog.json and `git log`. No dependencies.
"""
import json, os, subprocess, sys, time
from collections import Counter, defaultdict, OrderedDict

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BACKLOG = os.path.join(REPO, "docs", "development", "backlog.json")
PROJECT = os.path.basename(REPO)  # repo-aware: "Project-Io" or "Project-Gyre"

# ---- ANSI helpers (auto-disabled when not a tty) ----
_C = sys.stdout.isatty()
def c(s, code): return f"\033[{code}m{s}\033[0m" if _C else s
def bold(s):   return c(s, "1")
def dim(s):    return c(s, "2")
def green(s):  return c(s, "32")
def yellow(s): return c(s, "33")
def cyan(s):   return c(s, "36")

PRIORITY_ORDER = ["SSS", "S", "A", "B", "C", "F"]  # highest first

def load_backlog():
    with open(BACKLOG) as f:
        return json.load(f)

def git(*args):
    out = subprocess.run(["git", "-C", REPO, *args],
                         capture_output=True, text=True)
    return out.stdout

# ---- data gathering ----
def commit_records():
    """List of dicts: {hash, author, date, subject, session}."""
    sep = "\x1e"; fs = "\x1f"
    fmt = fs.join(["%h", "%an", "%ad", "%s", "%b"]) + sep
    raw = git("log", "--date=short", f"--pretty=format:{fmt}")
    recs = []
    for chunk in raw.split(sep):
        chunk = chunk.strip("\n")
        if not chunk:
            continue
        h, author, date, subject, body = (chunk.split(fs) + [""] * 5)[:5]
        session = "(none)"
        for line in body.splitlines():
            if line.startswith("Claude-Session:"):
                url = line.split(":", 1)[1].strip()
                session = url.rstrip("/").split("/")[-1]  # session_XXXX
        recs.append(dict(hash=h, author=author, date=date,
                         subject=subject, session=session))
    return recs

# ---- views ----
def view_items():
    d = load_backlog()
    items, delivered = d.get("items", []), d.get("delivered", [])
    total = len(items) + len(delivered)
    print(bold("── Backlog ──"))
    print(f"  {yellow(str(len(items)))} open   "
          f"{green(str(len(delivered)))} delivered   "
          f"{dim(str(total)+' total')}")
    if total:
        pct = 100 * len(delivered) / total
        print(f"  progress: {green(f'{pct:.0f}%')} delivered")

    by_status = Counter(i.get("status", "?") for i in items)
    if by_status:
        print("  open by status: " +
              "  ".join(f"{k}={v}" for k, v in sorted(by_status.items())))

    by_pri = Counter(i.get("priority", "?") for i in items)
    if by_pri:
        ordered = [(p, by_pri[p]) for p in PRIORITY_ORDER if p in by_pri]
        ordered += [(p, n) for p, n in by_pri.items() if p not in PRIORITY_ORDER]
        print("  open by priority: " +
              "  ".join(f"{p}={n}" for p, n in ordered))

    blocked = [i for i in items if i.get("blocked_on") or i.get("waits_on")]
    if blocked:
        print(f"  {yellow(str(len(blocked)))} open item(s) waiting/blocked:")
        for i in blocked:
            deps = (i.get("blocked_on") or []) + (i.get("waits_on") or [])
            print(dim(f"      {i['id']} {i.get('short_name','')} ← {', '.join(deps)}"))
    print()

def view_sessions(recs=None):
    recs = recs if recs is not None else commit_records()
    by_session = defaultdict(list)
    for r in recs:
        by_session[r["session"]].append(r)
    print(bold("── Commits by session ──"))
    print(f"  {cyan(str(len(recs)))} commit(s) across "
          f"{cyan(str(len(by_session)))} session(s)")
    # order sessions by most recent commit (recs are newest-first)
    order = list(OrderedDict((r["session"], None) for r in recs))
    for s in order:
        rs = by_session[s]
        span = f"{rs[-1]['date']}→{rs[0]['date']}" if rs[-1]['date'] != rs[0]['date'] else rs[0]['date']
        label = s if s != "(none)" else dim("(no session tag)")
        print(f"  {label}  {cyan(str(len(rs)))} commit(s)  {dim(span)}")
        print(dim(f"      latest: {rs[0]['hash']} {rs[0]['subject'][:60]}"))
    print()

def view_commits(recs=None):
    recs = recs if recs is not None else commit_records()
    print(bold("── Commits ──"))
    print(f"  {cyan(str(len(recs)))} total")
    by_author = Counter(r["author"] for r in recs)
    print("  by author: " +
          "  ".join(f"{a}={n}" for a, n in by_author.most_common()))
    print()

def dashboard():
    recs = commit_records()
    view_items()
    view_commits(recs)
    view_sessions(recs)

def watch(secs):
    try:
        while True:
            os.system("clear" if os.name == "posix" else "cls")
            print(bold(cyan(f"{PROJECT} diagnostics")) +
                  dim(f"   (refresh {secs}s — Ctrl-C to stop)\n"))
            dashboard()
            time.sleep(secs)
    except KeyboardInterrupt:
        print("\nstopped.")

def main():
    args = sys.argv[1:]
    cmd = args[0] if args else "dashboard"
    if cmd in ("items", "backlog"):     view_items()
    elif cmd == "commits":              view_commits()
    elif cmd == "sessions":             view_sessions()
    elif cmd == "watch":
        secs = int(args[1]) if len(args) > 1 else 15
        watch(secs)
    elif cmd in ("dashboard", "all"):   dashboard()
    elif cmd in ("-h", "--help", "help"):
        print(__doc__)
    else:
        print(f"unknown command: {cmd}\n"); print(__doc__); sys.exit(1)

if __name__ == "__main__":
    main()
