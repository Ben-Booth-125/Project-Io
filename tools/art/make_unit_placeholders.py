"""Draw the unit designer's PLACEHOLDER class art (docs/ui/ledgers/military.md § The designer).

One labelled silhouette per land class, written to assets/units/<class>.png, and
beside it <class>.anchors: one line per kit slot naming, image-relative (0..1 from
the top-left), where that slot's equipment sits on the drawing. The designer joins
each slot's box to that point (src/ui/unit_art.hpp reads both files).

These are deliberately crude: flat grey shapes, a dashed frame and the word
PLACEHOLDER, so nobody mistakes them for art. Real art replaces the PNG (and the
.anchors file if the parts move); no code changes.

Slot order per class is MILITARY.md § The kit's (unit_designs.cpp k_slot_names).

    python tools/art/make_unit_placeholders.py

Needs Pillow. Uses the bundled assets/fonts/DejaVuSans.ttf, so the output is the
same on every machine.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "assets", "units")
FONT = os.path.join(ROOT, "assets", "fonts", "DejaVuSans.ttf")

W, H = 640, 400
BODY = (150, 158, 170, 255)
DARK = (98, 104, 116, 255)
LINE = (200, 206, 214, 255)


def soldier(d, x, y, s=1.0, rifle=True):
    """A standing figure, feet at (x, y). Returns the rifle muzzle and torso centre."""
    h = 150 * s
    head_r = 13 * s
    d.ellipse([x - head_r, y - h, x + head_r, y - h + 2 * head_r], fill=BODY)
    d.polygon([(x - 13 * s, y - h + 28 * s), (x + 13 * s, y - h + 28 * s),
               (x + 17 * s, y - 62 * s), (x - 17 * s, y - 62 * s)], fill=BODY)  # torso
    d.polygon([(x - 15 * s, y - 62 * s), (x - 3 * s, y - 62 * s), (x - 7 * s, y), (x - 19 * s, y)], fill=BODY)
    d.polygon([(x + 3 * s, y - 62 * s), (x + 15 * s, y - 62 * s), (x + 19 * s, y), (x + 7 * s, y)], fill=BODY)
    torso = (x, y - h + 60 * s)
    muzzle = (x, y)
    if rifle:
        a = (x - 30 * s, y - 70 * s)
        b = (x + 62 * s, y - 118 * s)
        d.line([a, b], fill=DARK, width=max(2, int(7 * s)))
        muzzle = (x + 50 * s, y - 112 * s)
    return muzzle, torso


def wheels(d, xs, y, r):
    for x in xs:
        d.ellipse([x - r, y - r, x + r, y + r], fill=DARK)
        d.ellipse([x - r * 0.4, y - r * 0.4, x + r * 0.4, y + r * 0.4], fill=BODY)


def draw_infantry(d):
    muzzle, torso = soldier(d, 300, 340, 1.5)
    return [muzzle, torso]


def draw_motorised(d):
    # Truck: cab right, bed left.
    d.rectangle([110, 230, 420, 300], fill=BODY)        # bed
    d.polygon([(420, 300), (420, 190), (490, 190), (530, 245), (530, 300)], fill=BODY)  # cab
    d.polygon([(435, 202), (482, 202), (510, 240), (435, 240)], fill=DARK)  # window
    wheels(d, [170, 360, 480], 305, 30)
    muzzle, torso = soldier(d, 250, 232, 0.85)
    return [muzzle, torso, (470, 270)]


def draw_mechanised(d):
    # Box hull with a sloped glacis and a small turret; a soldier beside it.
    d.polygon([(200, 300), (200, 210), (470, 210), (540, 260), (540, 300)], fill=BODY)
    d.rectangle([300, 185, 380, 212], fill=DARK)        # turret
    d.line([(380, 196), (450, 190)], fill=DARK, width=6)
    d.rounded_rectangle([190, 292, 550, 336], radius=22, fill=DARK)  # tracks
    wheels(d, [225, 290, 355, 420, 485], 314, 14)
    muzzle, torso = soldier(d, 120, 336, 0.95)
    return [muzzle, torso, (430, 250)]


def draw_armour(d):
    # Tank: long gun, turret, hull, tracks.
    d.polygon([(130, 300), (150, 238), (520, 238), (550, 300)], fill=BODY)    # hull
    d.polygon([(250, 238), (270, 190), (400, 185), (430, 238)], fill=BODY)    # turret
    d.rectangle([400, 202, 600, 214], fill=DARK)                              # gun
    d.rounded_rectangle([115, 292, 565, 350], radius=28, fill=DARK)           # tracks
    wheels(d, [160, 230, 300, 370, 440, 515], 321, 18)
    return [(575, 208), (300, 225), (340, 335)]


def draw_light_armour(d):
    # Wheeled scout car with a roof gun.
    d.polygon([(130, 300), (150, 235), (250, 215), (430, 215), (520, 250), (530, 300)], fill=BODY)
    d.rectangle([290, 190, 350, 216], fill=DARK)        # mount
    d.rectangle([340, 196, 460, 204], fill=DARK)        # gun
    d.polygon([(380, 225), (470, 225), (505, 252), (380, 252)], fill=DARK)  # vision block
    wheels(d, [190, 330, 460], 305, 32)
    return [(445, 200), (240, 260), (330, 305)]


def draw_artillery(d):
    # Tractor on the left towing a split-trail howitzer; shell crates beside.
    d.polygon([(60, 300), (60, 230), (130, 200), (185, 200), (185, 300)], fill=BODY)  # tractor
    wheels(d, [95, 160], 305, 26)
    d.line([(185, 285), (250, 290)], fill=DARK, width=6)                       # tow bar
    d.polygon([(250, 296), (370, 270), (380, 282), (260, 306)], fill=DARK)     # trail
    d.rectangle([340, 240, 410, 285], fill=BODY)                              # shield / cradle
    d.line([(380, 250), (560, 150)], fill=BODY, width=14)                      # barrel
    wheels(d, [385], 300, 30)
    for i, (x, y) in enumerate([(470, 296), (520, 296), (495, 266)]):
        d.rectangle([x - 22, y - 14, x + 22, y + 14], fill=BODY)
        d.line([(x - 22, y), (x + 22, y)], fill=DARK, width=2)
    return [(540, 162), (495, 280), (125, 250)]


def draw_air_defence(d):
    # Truck carrying a box launcher, with a radar dish on a mast.
    d.rectangle([110, 240, 440, 300], fill=BODY)
    d.polygon([(440, 300), (440, 200), (500, 200), (535, 250), (535, 300)], fill=BODY)
    d.polygon([(452, 212), (494, 212), (518, 245), (452, 245)], fill=DARK)
    wheels(d, [165, 320, 485], 305, 28)
    # Launcher: four tubes, raised.
    d.polygon([(150, 238), (310, 150), (330, 185), (170, 270)], fill=DARK)
    for k in range(4):
        y0 = 160 + k * 18
        d.line([(170 + k * 4, y0 + 66), (312 + k * 4, y0 - 12)], fill=BODY, width=2)
    # Radar mast and dish.
    d.line([(380, 240), (380, 140)], fill=DARK, width=6)
    d.chord([340, 105, 420, 165], 180, 360, fill=BODY)
    return [(300, 165), (380, 130), (300, 275)]


CLASSES = [
    ("infantry", "Infantry", draw_infantry, ["Weapon", "Armour"]),
    ("motorised", "Motorised infantry", draw_motorised, ["Weapon", "Armour", "Vehicle"]),
    ("mechanised", "Mechanised infantry", draw_mechanised, ["Weapon", "Armour", "Vehicle"]),
    ("armour", "Armour", draw_armour, ["Armament", "Armour", "Tracks"]),
    ("light_armour", "Light armour", draw_light_armour, ["Armament", "Armour", "Wheels"]),
    ("artillery", "Artillery", draw_artillery, ["Gun", "Munitions", "Tow"]),
    ("air_defence", "Air defence", draw_air_defence, ["Launcher", "Fire control", "Vehicle"]),
]


def dashed_rect(d, box, dash=10, gap=8, fill=LINE, width=2):
    x0, y0, x1, y1 = box
    for x in range(x0, x1, dash + gap):
        d.line([(x, y0), (min(x + dash, x1), y0)], fill=fill, width=width)
        d.line([(x, y1), (min(x + dash, x1), y1)], fill=fill, width=width)
    for y in range(y0, y1, dash + gap):
        d.line([(x0, y), (x0, min(y + dash, y1))], fill=fill, width=width)
        d.line([(x1, y), (x1, min(y + dash, y1))], fill=fill, width=width)


def main():
    os.makedirs(OUT, exist_ok=True)
    big = ImageFont.truetype(FONT, 26)
    small = ImageFont.truetype(FONT, 15)
    for stem, label, draw, slots in CLASSES:
        img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        dashed_rect(d, (4, 4, W - 5, H - 5))
        anchors = draw(d)
        assert len(anchors) == len(slots), stem
        d.text((20, 16), label, font=big, fill=LINE)
        d.text((20, 50), "PLACEHOLDER - not final art", font=small, fill=DARK)
        img.save(os.path.join(OUT, stem + ".png"), optimize=False)
        with open(os.path.join(OUT, stem + ".anchors"), "w", newline="\n") as f:
            f.write("# %s - one line per kit slot: slot x y, image-relative (0..1 from top-left).\n" % label)
            f.write("# Written by tools/art/make_unit_placeholders.py; edit with the art.\n")
            for i, ((x, y), name) in enumerate(zip(anchors, slots)):
                f.write("%d %.3f %.3f  # %s\n" % (i, x / W, y / H, name))
        print("wrote", stem)
    return 0


if __name__ == "__main__":
    sys.exit(main())
