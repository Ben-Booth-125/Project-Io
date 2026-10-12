#!/usr/bin/env bash
# par_seeds.sh — run ONE seed-sweeping harness across its seeds IN PARALLEL,
# then merge the shards' verdicts into one.
#
#   bash tools/verify/par_seeds.sh <harness> [--tier quick|full] [--seeds a,b,c]
#        [--range N] [-j N] [--build] [--out DIR] [-- <harness args...>]
#
# WHY (2026-10-12, Ben: "consider what the wait period is caused by"). Every
# seed harness runs its seeds one after another in one process, on one core of
# a 16-thread machine. The sprint-53 gates were waits, not work: the 16x9
# nation-targets grid took 16 h, industrialisation_sim_harness 69 min,
# market_viability 20 min. Seeds are independent worlds, so the sweep is
# embarrassingly parallel: one process per shard, each handed `--seeds <shard>`,
# which every library-sweeping harness already parses. Nothing inside a
# harness changes, so a shard's output is byte-for-byte what the serial run
# prints for those seeds.
#
# TIERS (the two gate tiers, same date):
#   quick — 4 representative library seeds (0 reference, 28 small/quiet,
#           46 rich, 12 inward). For ITERATION: an agent calibrating or
#           mid-fix runs this, never the full set.
#   full  — the 16 curated seeds of docs/generation/seed_library.json. For
#           MERGE: the gate a branch must pass before it lands.
# `--seeds` names worlds outright; `--range N` sweeps 0..N-1 (player_seed_sweep
# --guard 12's own definition is `--range 12`). Default tier: full.
#
# Harness args go after `--` and come FIRST on the harness's command line, so a
# mode flag that must be argv[1] (player_seed_sweep --guard) stays argv[1]:
#   bash tools/verify/par_seeds.sh player_seed_sweep --range 12 -- --guard
#   bash tools/verify/par_seeds.sh market_viability --tier quick
#   bash tools/verify/par_seeds.sh nation_targets_harness -- --pairs 6:1,40:6
#
# OUTPUT. Each shard writes DIR/<harness>.shard<k>.txt (default DIR:
# build_gen/verify/out/par/<harness>). The merge (par_seeds_merge.js) prints
# every shard's verdict lines, then one MERGED block: pooled market_viability
# gates recomputed from the shards' absolute numerators, summed check/failure
# counts, summed history_patterns hits. Exit 0 only if every shard exited 0.
#
# Not for the fixed-fixture harnesses (world_determinism, history_sim_harness,
# exploration_sim_harness, realm_names_check): they take no seed list — run
# them directly.
set -uo pipefail

die() { echo "par_seeds: $*" >&2; exit 2; }

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT" || die "cannot cd to repo root"

H="${1:-}"; [ -n "$H" ] || die "usage: par_seeds.sh <harness> [--tier quick|full] [--seeds a,b] [--range N] [-j N] [--build] [--out DIR] [-- args]"
shift

TIER=full; SEEDS=""; RANGE=""; JOBS=""; BUILD=0; OUT=""; HARGS=()
while [ $# -gt 0 ]; do
    case "$1" in
        --tier)  TIER="$2"; shift 2 ;;
        --seeds) SEEDS="$2"; shift 2 ;;
        --range) RANGE="$2"; shift 2 ;;
        -j)      JOBS="$2"; shift 2 ;;
        --build) BUILD=1; shift ;;
        --out)   OUT="$2"; shift 2 ;;
        --)      shift; HARGS=("$@"); break ;;
        *)       die "unknown option $1 (harness args go after --)" ;;
    esac
done

if [ -z "$SEEDS" ]; then
    if [ -n "$RANGE" ]; then
        SEEDS="$(seq -s, 0 $((RANGE - 1)))"
    elif [ "$TIER" = quick ]; then
        SEEDS="0,28,46,12"
    elif [ "$TIER" = full ]; then
        SEEDS="$(node -e 'const j=require("./docs/generation/seed_library.json");console.log(j.seeds.map(s=>s.seed).join(","))')" \
            || die "cannot read docs/generation/seed_library.json"
    else
        die "--tier must be quick or full"
    fi
fi
IFS=',' read -r -a SEED_ARR <<< "$SEEDS"
N=${#SEED_ARR[@]}; [ "$N" -gt 0 ] || die "no seeds"

NPROC="$(nproc 2>/dev/null || echo 8)"
[ -n "$JOBS" ] || JOBS=$(( N < NPROC ? N : NPROC ))
[ "$JOBS" -gt "$N" ] && JOBS=$N

if [ "$BUILD" = 1 ]; then
    bash tools/verify/build_lua_harness.sh "$H" | tail -1
fi
EXE="build_gen/verify/$H.exe"
[ -x "$EXE" ] || die "$EXE not built (pass --build, or run build_lua_harness.sh $H)"

[ -n "$OUT" ] || OUT="build_gen/verify/out/par/$H"
mkdir -p "$OUT"
rm -f "$OUT"/"$H".shard*.txt "$OUT"/"$H".shard*.exit

# Round-robin the seeds over the shards, in the order given, so a seed list
# that names its slow worlds first spreads them.
declare -a SHARD
for ((i = 0; i < N; ++i)); do
    k=$(( i % JOBS ))
    SHARD[$k]="${SHARD[$k]:+${SHARD[$k]},}${SEED_ARR[$i]}"
done

# ONE HEAVY RUN PER MACHINE (BL-1299). A shard set already uses every core, so
# two at once only slow each other. The lock lives in the MAIN checkout (found
# through git's common dir), so every worktree's runner queues on the same one.
# mkdir is atomic; a lock older than 6 h is a crashed run and is broken.
COMMON="$(git rev-parse --git-common-dir 2>/dev/null)"
LOCK="$(cd "$(dirname "${COMMON:-.git}")" && pwd)/build_gen/verify/out/par_seeds.lock"
mkdir -p "$(dirname "$LOCK")"
WAITED=0
until mkdir "$LOCK" 2>/dev/null; do
    if [ -n "$(find "$LOCK" -maxdepth 0 -mmin +360 2>/dev/null)" ]; then rm -rf "$LOCK"; continue; fi
    [ "$WAITED" = 0 ] && echo "par_seeds: another heavy run holds the machine ($(cat "$LOCK/owner" 2>/dev/null)); queued"
    WAITED=1; sleep 15
done
echo "$H pid $$ $(date +%T) $ROOT" > "$LOCK/owner"
trap 'rm -rf "$LOCK"' EXIT

echo "par_seeds: $H over $N seed(s) in $JOBS shard(s) [${TIER}${RANGE:+, range $RANGE}]  args: ${HARGS[*]:-(none)}"
T0=$(date +%s)
for ((k = 0; k < JOBS; ++k)); do
    (
        "./$EXE" "${HARGS[@]}" --seeds "${SHARD[$k]}" > "$OUT/$H.shard$k.txt" 2>&1
        echo $? > "$OUT/$H.shard$k.exit"
    ) &
done
wait
T1=$(date +%s)

node tools/verify/par_seeds_merge.js "$H" "$OUT" "$JOBS" "$((T1 - T0))" "${SHARD[@]}"
