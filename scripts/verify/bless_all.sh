#!/usr/bin/env bash
# Re-bless every visual-verify golden on THIS machine's GPU renderer. The planetary
# ground is 3D and draws only on SDL's GPU renderer (BL-1671 (flat ground retired);
# TECH_FOUNDATIONS.md § Rendering dimension), so captures are machine-dependent:
# bless on the machine that will compare, and compare with a tolerance.
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

# A GPU device is required. Under xvfb (Linux, no monitor) the GPU renderer needs a
# Vulkan device - lavapipe (Mesa's software Vulkan) serves; without one the canvas
# draws no ground.
RUN=()
if command -v xvfb-run >/dev/null 2>&1 && [ -z "${DISPLAY:-}" ]; then
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
echo "Blessed $count script(s) on this machine's GPU renderer."
echo "Review and commit the updated scripts/verify/golden/*.png."
