"""Generate the close-zoom detail textures (BL-1658 (close-zoom detail layer);
docs/ui/RENDERING.md § Level of detail, "The two closest rungs add detail at draw time").

Also writes assets/detail/rolling.png, the draw-time rolling ground's height (256 px).
Writes two scales per family, both 8-bit greyscale, seamless in both axes, mean grey
128, each normalised so its 99th-percentile deviation from the mean is the full swing
(0 or 255):
  - assets/detail/<family>.png       256 px, the COARSE layer;
  - assets/detail/fine/<family>.png   64 px, the FINE layer (its own seed, its lattice
    periods halved so a cell keeps a few texels).
The ground shader reads each as a luminance modulation of +-(its strength) at the
extremes. Deterministic: a fixed seed per family and scale, no dependencies beyond the
standard library. Repainting one of these files by hand replaces its output.

    python tools/art/gen_detail_textures.py [out_dir]
"""
import math
import os
import random
import struct
import sys
import zlib

N = 256  # texture side, px (set per scale by main)
DIV = 1  # lattice periods are divided by this (the fine scale halves them)


def P(p):
    return max(2, p // DIV)


def value_noise(period, rng, warp=None, warp_amp=0.0, period_y=None):
    """Periodic gradient (Perlin) noise in 0..1: `period` lattice cells across the
    texture (`period_y` down it, if given: an unequal pair stretches the noise into
    streaks), quintic-faded, so no lattice axis shows. `warp` (a 0..1 field) offsets
    the lookup by up to `warp_amp` cells, diagonally, to bend straight features."""
    py_ = period_y or period
    grad = []
    for _ in range(period * py_):
        a = rng.random() * 2 * math.pi
        grad.append((math.cos(a), math.sin(a)))

    def g(ix, iy):
        return grad[(iy % py_) * period + (ix % period)]

    def fade(t):
        return t * t * t * (t * (t * 6 - 15) + 10)

    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            fx = x * period / N
            fy = y * py_ / N
            if warp is not None:
                w = (warp[y][x] - 0.5) * warp_amp
                fx += w
                fy -= w
            x0, y0 = math.floor(fx), math.floor(fy)
            tx, ty = fx - x0, fy - y0
            def dot(ix, iy, dx, dy):
                gx, gy = g(ix, iy)
                return gx * dx + gy * dy
            n00 = dot(x0, y0, tx, ty)
            n10 = dot(x0 + 1, y0, tx - 1, ty)
            n01 = dot(x0, y0 + 1, tx, ty - 1)
            n11 = dot(x0 + 1, y0 + 1, tx - 1, ty - 1)
            u, v = fade(tx), fade(ty)
            a = n00 + (n10 - n00) * u
            b = n01 + (n11 - n01) * u
            out[y][x] = 0.5 + 0.7071 * (a + (b - a) * v)
    return out


def fbm(periods, weights, rng, ridged=False, warp=None, warp_amp=0.0):
    acc = [[0.0] * N for _ in range(N)]
    for p, w in zip(periods, weights):
        n = value_noise(p, rng, warp, warp_amp * p / periods[0])
        for y in range(N):
            row, src = acc[y], n[y]
            for x in range(N):
                v = src[x]
                if ridged:
                    v = 1.0 - abs(2.0 * v - 1.0)
                row[x] += w * v
    return acc


def warp_field(rng, amp):
    """Two periodic low fields (x and y offsets), in cell units scaled by `amp`."""
    wx = fbm([P(4), P(8), P(16)], [0.55, 0.3, 0.15], rng)
    wy = fbm([P(4), P(8), P(16)], [0.55, 0.3, 0.15], rng)
    return wx, wy, amp


def worley(cells, rng, jitter=0.9, warp=None):
    """Periodic cellular noise: `cells` jittered points per side. Returns per pixel
    (f1, f2, id of the nearest point, its offset dx, dy), distances in cell units.
    `warp` (from warp_field) bends the lookup so cell edges wander, not run straight."""
    pts = []
    for cy in range(cells):
        for cx in range(cells):
            pts.append((cx + 0.5 + (rng.random() - 0.5) * jitter,
                        cy + 0.5 + (rng.random() - 0.5) * jitter, rng.random()))
    out = [[None] * N for _ in range(N)]
    for y in range(N):
        fy = (y + 0.5) * cells / N
        for x in range(N):
            fx = (x + 0.5) * cells / N
            fyw = fy
            if warp is not None:
                wx, wy, amp = warp
                fx += (wx[y][x] - 0.5) * amp
                fyw = fy + (wy[y][x] - 0.5) * amp
            cx0 = math.floor(fx)
            cy0 = math.floor(fyw)
            f1 = f2 = 9.0
            best = None
            for oy in (-1, 0, 1):
                for ox in (-1, 0, 1):
                    cx, cy = cx0 + ox, cy0 + oy
                    px, py, r = pts[(cy % cells) * cells + (cx % cells)]
                    px += cx - (cx % cells)
                    py += cy - (cy % cells)
                    dx, dy = fx - px, fyw - py
                    d = math.sqrt(dx * dx + dy * dy)
                    if d < f1:
                        f2, f1, best = f1, d, (r, dx, dy)
                    elif d < f2:
                        f2 = d
            out[y][x] = (f1, f2, best[0], best[1], best[2])
    return out


def smooth(e0, e1, v):
    t = min(1.0, max(0.0, (v - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


LIGHT = (-0.6, -0.6)  # light from the upper left, as the bake's relief


def vegetation(rng):
    # Grass (Ben, 2026-11-16: the cellular tufts "just look rocky"): soft clumps with no
    # edges (rounded noise, sharpened only gently), a short blade grain - noise stretched
    # into streaks down the texture, bent by a low warp so no stripe runs straight - and
    # softly thinner, paler patches. No cells and no seams, so nothing reads as stone.
    clumps = fbm([P(8), P(16), P(32)], [0.5, 0.3, 0.2], rng)
    bend = fbm([P(4), P(8)], [0.7, 0.3], rng)
    blades = value_noise(P(96), rng, bend, 1.5, period_y=P(24))
    blades2 = value_noise(P(160), rng, bend, 2.0, period_y=P(40))
    patches = fbm([P(4), P(8)], [0.7, 0.3], rng)
    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            c = clumps[y][x]
            b = 0.6 * blades[y][x] + 0.4 * blades2[y][x]
            v = 0.45 * c + 0.45 * (b - 0.5) * (0.6 + 0.4 * c) + 0.25
            thin = smooth(0.55, 0.75, patches[y][x])
            out[y][x] = v * (1 - 0.25 * thin) + 0.08 * thin
    return out


def forest(rng):
    # Canopy: round crowns of varied size as lit domes, in shadowed gaps where they do not
    # meet, each crown its own tone, a leaf grain on top. Ben, 2026-11-16: the first crowns
    # (filled cells, 20 a texture) read "too jagged" - so these are smaller (30 a texture)
    # and ROUND, a dome inside each cell rather than the whole cell, so the gaps between
    # them are irregular rather than a polygon net.
    w = worley(P(30), rng, 1.0, warp_field(rng, 0.6))
    grain = worley(P(128), rng, 1.0)
    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            f1, f2, r, dx, dy = w[y][x]
            rad = 0.62 + 0.22 * r                         # this crown's radius, cell units
            u = min(f1 / rad, 1.0)
            crown = 1.0 - smooth(0.8, 1.0, u)             # 0 in the gap, 1 inside a crown
            dome = math.sqrt(max(0.0, 1.0 - u * u))       # the crown's rounded top
            lit = -(dx * LIGHT[0] + dy * LIGHT[1]) / rad * 0.5
            g = grain[y][x]
            leaf = smooth(0.0, 0.15, g[1] - g[0])
            v = (0.45 + 0.15 * r + 0.25 * dome + 0.3 * lit) * (0.85 + 0.15 * leaf)
            out[y][x] = 0.3 + crown * (v - 0.2)
    return out


def arid(rng):
    # Wind ripples with a gentle stoss face and a steep, dark lee face (an asymmetric
    # wave, not a sine), bent by a periodic warp; pebbles scattered as hard dots.
    warp = fbm([P(4), P(8)], [0.7, 0.3], rng)
    peb = worley(P(40), rng, 1.0)
    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            f = ((P(12) * x + P(5) * y) / N + 1.4 * warp[y][x]) % 1.0
            wave = f / 0.82 if f < 0.82 else 1.0 - (f - 0.82) / 0.18 * 1.6
            f1, f2, r, dx, dy = peb[y][x]
            pebble = (1.0 - smooth(0.06, 0.11, f1)) if r < 0.35 else 0.0
            out[y][x] = 0.25 + 0.6 * wave - 0.7 * pebble
    return out


def rock(rng):
    # Broken stone: plates split by thin dark cracks that wander (warped cellular
    # edges), each plate its own tone and lit tilt, warped strata across them, and a
    # ridged grain for the stone's surface.
    w = worley(P(9), rng, 1.0, warp_field(rng, P(9) * 0.45))
    warp = fbm([P(4), P(8)], [0.7, 0.3], rng)
    grain = fbm([P(32), P(64)], [0.6, 0.4], rng, ridged=True)
    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            f1, f2, r, dx, dy = w[y][x]
            crack = smooth(0.0, 0.07, f2 - f1)
            tilt = -(dx * LIGHT[0] + dy * LIGHT[1]) * 0.5
            strata = 0.5 + 0.5 * math.sin(2 * math.pi * (P(7) * y / N + 1.6 * warp[y][x]))
            strata = smooth(0.3, 0.7, strata)
            v = 0.42 + 0.22 * r + 0.18 * tilt + 0.14 * strata + 0.2 * (grain[y][x] - 0.5)
            out[y][x] = 0.12 + crack * v
    return out


def normalise(img):
    flat = [v for row in img for v in row]
    mean = sum(flat) / len(flat)
    devs = sorted(abs(v - mean) for v in flat)
    p99 = devs[int(0.99 * (len(devs) - 1))] or 1.0
    k = 127.5 / p99
    return bytes(
        max(0, min(255, int(round(127.5 + (v - mean) * k)))) for row in img for v in row
    )


def write_png_grey(path, data):
    raw = b"".join(b"\x00" + data[y * N:(y + 1) * N] for y in range(N))

    def chunk(tag, body):
        return (struct.pack(">I", len(body)) + tag + body
                + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF))

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 0, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


FAMILIES = [  # order = the channel the app packs it into (r, g, b, a)
    ("vegetation", vegetation, 1658001),
    ("forest", forest, 1658002),
    ("arid", arid, 1658003),
    ("rock", rock, 1658004),
]


SCALES = [  # (sub-directory, side px, period divisor, seed offset)
    ("", 256, 1, 0),
    ("fine", 256, 1, 100),  # 256 px over 2 units (Ben, 2026-11-16): the old fine lattice per unit, four times the pixels
]


def rolling(rng):
    # The rolling ground (Ben, 2026-11-16: flat tiles showed "no elevation or shading"):
    # broad, smooth swells, read by the ground shader as a height over a 12-unit repeat,
    # so a swell is a few hex radii across. Few octaves: it is landform, not texture.
    return fbm([2, 4, 8], [0.6, 0.3, 0.1], rng)


def main():
    global N, DIV
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join("assets", "detail")
    for sub, side, div, seed_off in SCALES:
        N, DIV = side, div
        out_dir = os.path.join(root, sub) if sub else root
        os.makedirs(out_dir, exist_ok=True)
        for name, fn, seed in FAMILIES:
            data = normalise(fn(random.Random(seed + seed_off)))
            path = os.path.join(out_dir, name + ".png")
            write_png_grey(path, data)
            print("wrote", path, len(data), "px")
    N, DIV = 256, 1
    path = os.path.join(root, "rolling.png")
    write_png_grey(path, normalise(rolling(random.Random(1658010))))
    print("wrote", path)


if __name__ == "__main__":
    main()
