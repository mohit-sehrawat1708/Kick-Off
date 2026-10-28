"""City skyline backdrop: T_PK_Skyline.png, the horizon band of a Poly Haven panorama (CC0) with the sky cut away.

Source: "Homecoming Center Rooftop" (Cape Town, clear midday), tonemapped 8K equirectangular JPG from
https://polyhaven.com/a/homecoming_center_rooftop (CC0: no attribution required, credited here anyway).
Only the band from -1.5 to +16 degrees elevation is kept (city blocks, the CBD towers and Table Mountain);
the sky above the buildings becomes transparent so the game's own sky shows through, and the two flagpoles on the
photographer's roof are painted out. M_PK_Skyline (make_skyline_material.py) maps the band onto a far cylinder.

Run: python Tools/art/make_skyline.py <path to homecoming_center_rooftop.jpg>
"""
import json
import os
import sys

import numpy as np
from PIL import Image
from scipy.ndimage import maximum_filter1d, minimum_filter1d, gaussian_filter

OUT = os.path.dirname(os.path.abspath(__file__))
E_TOP, E_BOTTOM = 16.0, -1.5        # band kept, degrees of elevation
OUT_W, OUT_H = 8192, 512
CITY = (0.522, 0.986)              # the downtown stretch (fractions of the panorama width)

Image.MAX_IMAGE_PIXELS = None
src = Image.open(sys.argv[1]).convert("RGB")
W, H = src.size
y0 = int(round((90.0 - E_TOP) / 180.0 * H))
y1 = int(round((90.0 - E_BOTTOM) / 180.0 * H))
band = np.asarray(src.crop((0, y0, W, y1))).astype(np.float32)
h, w, _ = band.shape
r, g, b = band[..., 0], band[..., 1], band[..., 2]
lum = 0.3 * r + 0.59 * g + 0.11 * b
mx, mn = band.max(axis=2), band.min(axis=2)
sat = (mx - mn) / np.maximum(mx, 1.0)

# Sky: clearly blue, or (near the top of the band) bright low-saturation cloud.
row = np.arange(h)[:, None] / h
sky = ((b > r + 22) & (b > g + 4) & (sat > 0.25)) | ((lum > 140) & (sat < 0.3) & (row < 0.45)) | (row < 0.06)

# First solid pixel per column, from the top; everything below it is city.
solid = ~sky
# ignore isolated specks (antennas, birds): need 3 solid pixels in a row vertically
run = solid[:-2] & solid[1:-1] & solid[2:]
first = np.where(run.any(axis=0), run.argmax(axis=0), h - 1).astype(np.float32)

# Flagpoles: narrow upward spikes in the skyline outline are cut back to sky (the cut line is reshaped only).
win = max(31, w // 260)
opened = minimum_filter1d(maximum_filter1d(first, win), win)
spike = first < opened - 6
clean = band.copy()
x = 0
while x < w:
    if not spike[x]:
        x += 1
        continue
    a = x
    while x < w and spike[x]:
        x += 1
first = np.where(spike, opened, first)

# The two flagpoles also cross the mountain and buildings (no sky behind them there): paint those columns out at
# their positions in this panorama (fractions of the width), interpolating from the columns either side.
for f in (0.2138, 0.3788):
    c0, c1 = int((f - 0.0042) * w), int((f + 0.0042) * w)
    # clone the strip just left of the pole over it (interpolating smeared it into a streak), blending the seams
    n = c1 - c0
    patch = clean[:, c0 - n - 2:c0 - 2].copy()
    ramp = np.clip(np.minimum(np.arange(n), n - 1 - np.arange(n)) / 4.0, 0, 1)[None, :, None]
    clean[:, c0:c1] = patch * ramp + clean[:, c0:c1] * (1 - ramp) * 0 + patch * (1 - ramp) * 0 + (
        clean[:, c0 - 1:c0] * (1 - ramp) * (np.arange(n)[None, :, None] < n / 2) +
        clean[:, c1:c1 + 1] * (1 - ramp) * (np.arange(n)[None, :, None] >= n / 2))
# Buildings right next to the photographer's roof would read as giant towers beside the stadium: cut them back to
# sky so only the distant city, the CBD and Table Mountain remain.
NEAR = [(0.0, 0.066), (0.116, 0.199), (0.413, 0.519), (0.989, 1.0)]
cut = np.zeros(w, bool)
for a0, a1 in NEAR:
    cut[int(a0 * w):int(a1 * w)] = True
first = np.where(cut, h, first)

alpha = (np.arange(h)[:, None] >= first[None, :]).astype(np.float32)
alpha = gaussian_filter(alpha, sigma=(0.8, 0.6))           # soft one-pixel edge
rgba = np.dstack([clean, alpha * 255.0]).clip(0, 255).astype(np.uint8)
# Only the city: the downtown stretch with the towers (the mountain read as a cliff from the pitch). It is repeated,
# the second copy mirrored, to go all the way round the stadium.
city = rgba[:, int(CITY[0] * w):int(CITY[1] * w)]
rgba = np.concatenate([city, city[:, ::-1]], axis=1)
img = Image.fromarray(rgba, "RGBA").resize((OUT_W, OUT_H), Image.LANCZOS)
img.save(os.path.join(OUT, "T_PK_Skyline.png"), optimize=True)
json.dump({"elev_top": E_TOP, "elev_bottom": E_BOTTOM, "source": "polyhaven.com/a/homecoming_center_rooftop (CC0)"},
          open(os.path.join(OUT, "skyline.json"), "w"), indent=1)
print("band %dx%d -> %dx%d, %d pole columns painted out, %.0f KB" % (w, h, OUT_W, OUT_H, int(spike.sum()),
      os.path.getsize(os.path.join(OUT, "T_PK_Skyline.png")) / 1024))
