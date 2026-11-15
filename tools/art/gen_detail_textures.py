"""Generate the close-zoom detail textures (BL-1658 (close-zoom detail layer);
docs/ui/RENDERING.md § Level of detail, "The two closest rungs add detail at draw time").

Writes assets/detail/<family>.png: 256 px square, 8-bit greyscale, seamless in both
axes, mean grey 128, each normalised so its 99th-percentile deviation from the mean is
the full swing (0 or 255). The ground shader reads them as a luminance modulation of
+-(strength) at the extremes. Deterministic: a fixed seed per family, no dependencies
beyond the standard library. Repainting one of these files by hand replaces its output.

    python tools/art/gen_detail_textures.py [out_dir]
"""
import math
import os
import random
import struct
import sys
import zlib

N = 256  # texture side, px


def value_noise(period, rng, warp=None, warp_amp=0.0):
    """Periodic gradient (Perlin) noise in 0..1: `period` lattice cells across the
    texture, quintic-faded, so no lattice axis shows. `warp` (a 0..1 field) offsets
    the lookup by up to `warp_amp` cells, diagonally, to bend straight features."""
    grad = []
    for _ in range(period * period):
        a = rng.random() * 2 * math.pi
        grad.append((math.cos(a), math.sin(a)))

    def g(ix, iy):
        return grad[(iy % period) * period + (ix % period)]

    def fade(t):
        return t * t * t * (t * (t * 6 - 15) + 10)

    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            fx = x * period / N
            fy = y * period / N
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


def vegetation(rng):
    # Soft mottle: grass and low cover read as small irregular patches.
    return fbm([8, 16, 32, 64], [0.35, 0.3, 0.2, 0.15], rng)


def forest(rng):
    # Canopy clumps: mid-frequency blobs pushed toward light/dark, plus leaf grain.
    base = fbm([16, 32], [0.65, 0.35], rng)
    grain = value_noise(64, rng)
    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            c = base[y][x]
            c = 0.5 + 0.5 * math.tanh(6.0 * (c - 0.5))
            out[y][x] = 0.75 * c + 0.25 * grain[y][x]
    return out


def arid(rng):
    # Wind ripples: bands along one axis, their phase warped by low noise. Integer
    # wave counts and a periodic warp keep it seamless.
    warp = fbm([4, 8], [0.7, 0.3], rng)
    grain = value_noise(64, rng)
    out = [[0.0] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            # Whole numbers of waves across x and y (diagonal bands, still seamless);
            # the periodic warp bends them.
            phase = 2 * math.pi * ((10 * x + 4 * y) / N + 1.6 * warp[y][x])
            out[y][x] = 0.7 * (0.5 + 0.5 * math.sin(phase)) + 0.3 * grain[y][x]
    return out


def rock(rng):
    # Ridged noise over a warped domain: thin bright ridges and dark cracks that
    # wander, the grain of broken stone.
    warp = fbm([4, 8], [0.7, 0.3], rng)
    return fbm([8, 16, 32, 64], [0.4, 0.3, 0.2, 0.1], rng, ridged=True, warp=warp, warp_amp=1.5)


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


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join("assets", "detail")
    os.makedirs(out_dir, exist_ok=True)
    for name, fn, seed in FAMILIES:
        data = normalise(fn(random.Random(seed)))
        path = os.path.join(out_dir, name + ".png")
        write_png_grey(path, data)
        print("wrote", path, len(data), "px")


if __name__ == "__main__":
    main()
