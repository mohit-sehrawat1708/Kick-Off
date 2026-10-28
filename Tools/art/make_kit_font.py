"""Kit lettering atlas: T_PK_KitFont.png, a signed-distance-field font (Bahnschrift Bold Condensed, a DIN-style kit face)
for the shirt numbers and the names on the backs. 8 x 5 cells of 128 px; 0.5 = glyph edge, values rise inside.
Cell i holds CHARS[i]; the advance widths (fraction of the glyph height) go to kit_font.json, which
Tools/editor_scripts/make_player_mh.py bakes into the cloth shader.
Run: python Tools/art/make_kit_font.py   (then Tools/editor_scripts/import_kit_font.py imports it)
"""
import json
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy.ndimage import distance_transform_edt

OUT = os.path.dirname(os.path.abspath(__file__))
CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-' "
COLS, ROWS, CELL, HI = 8, 5, 128, 4          # final cell size, supersampling for the distance field
SPREAD = 10.0                                # distance (final px) from the edge to 0 / 1
GLYPH = 0.72                                 # cap height as a fraction of the cell

font = ImageFont.truetype("C:/Windows/Fonts/bahnschrift.ttf", int(CELL * HI * GLYPH / 0.72))
font.set_variation_by_name("Bold Condensed")
cap = font.getbbox("H")
cap_h = cap[3] - cap[1]
scale = CELL * HI * GLYPH / cap_h
font = ImageFont.truetype("C:/Windows/Fonts/bahnschrift.ttf", int(CELL * HI * GLYPH / 0.72 * scale))
font.set_variation_by_name("Bold Condensed")
cap = font.getbbox("H")
cap_h = cap[3] - cap[1]

atlas = np.zeros((ROWS * CELL, COLS * CELL), np.float32)
advance = []
for i, ch in enumerate(CHARS):
    big = Image.new("L", (CELL * HI, CELL * HI), 0)
    d = ImageDraw.Draw(big)
    w = font.getlength(ch)
    advance.append(round(w / cap_h, 4))
    if ch.strip():
        # centred horizontally, cap line at the top margin, baseline at (1 - margin)
        margin = (1.0 - GLYPH) / 2.0
        d.text(((CELL * HI - w) / 2.0, CELL * HI * margin - cap[1]), ch, font=font, fill=255)
    m = np.array(big) > 127
    inside = distance_transform_edt(m)
    outside = distance_transform_edt(~m)
    sdf = (inside - outside) / HI                        # final-pixel units, + inside
    sdf = np.clip(0.5 + sdf / (2.0 * SPREAD), 0.0, 1.0)
    small = Image.fromarray((sdf * 255).astype(np.uint8)).resize((CELL, CELL), Image.BILINEAR)
    r, c = divmod(i, COLS)
    atlas[r * CELL:(r + 1) * CELL, c * CELL:(c + 1) * CELL] = np.array(small) / 255.0

Image.fromarray((atlas * 255).astype(np.uint8)).save(os.path.join(OUT, "T_PK_KitFont.png"))
json.dump({"chars": CHARS, "cols": COLS, "rows": ROWS, "glyph": GLYPH, "spread": SPREAD / CELL, "advance": advance},
          open(os.path.join(OUT, "kit_font.json"), "w"), indent=1)
print("atlas %dx%d, advances %s" % (COLS * CELL, ROWS * CELL, advance[:12]))
