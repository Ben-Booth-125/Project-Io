#!/usr/bin/env bash
# Re-bless every visual-verify golden under the deterministic SOFTWARE renderer,
# so the committed goldens (scripts/verify/golden/*.png) match what CI's
# `visual-verify` job produces (it runs --verify with SDL_RENDER_DRIVER=software
# under Xvfb). A GPU/accelerated renderer rasterises tiles and text differently,
# so goldens blessed on a GPU diff against CI — always bless via this script.
#
# Usage (from the repo root, after building ProjectIo):
#   scripts/verify/bless_all.sh [path-to-ProjectIo]      # default: ./build/ProjectIo
#
# Runs under xvfb-run when available (no monitor needed); otherwise renders on the
# real display. Review the changes and commit scripts/verify/golden/*.png after.
set -euo pipefail

EXE="${1:-./build/ProjectIo}"
if [ ! -x "$EXE" ]; then
    echo "error: ProjectIo binary not found/executable at '$EXE'" >&2
    echo "       build it first, or pass the path: scripts/verify/bless_all.sh <path>" >&2
    exit 1
fi

export SDL_RENDER_DRIVER=software

# Use a virtual display if one is available so the bless needs no monitor.
RUN=()
if command -v xvfb-run >/dev/null 2>&1; then
    RUN=(xvfb-run -a)
fi

shopt -s nullglob
count=0
for s in scripts/verify/*.lua; do
    [ "$(basename "$s")" = "lib.lua" ] && continue
    echo "=== bless: $s ==="
    "${RUN[@]}" "$EXE" --verify "$s" --bless
    count=$((count + 1))
done

echo
echo "Blessed $count script(s) under the software renderer."
echo "Review and commit the updated scripts/verify/golden/*.png."
