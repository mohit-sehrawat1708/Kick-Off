"""Sponsor wordmark atlas for the LED boards (fictional brands): T_PK_Sponsors.png, 2048 x 2048, eight rows of 256 px,
white marks on transparent (the board material colours them). Row i = ad i in M_PK_Board.
Run: python Tools/art/make_sponsors.py   (then Tools/editor_scripts/import_ui.py imports it)
"""
import os
from PIL import Image, ImageDraw, ImageFont

OUT = os.path.dirname(os.path.abspath(__file__))
FONTS = "C:/Windows/Fonts/"
W, ROW = 2048, 256

# (word, font, symbol) - symbols drawn before the word
BRANDS = [
    ("VOLTA ENERGY", "impact.ttf", "bolt"),
    ("NORDIC AIR", "seguibl.ttf", "wing"),
    ("KAIZEN", "bahnschrift.ttf", "ring"),
    ("ORBIT BANK", "segoeuib.ttf", "orbit"),
    ("SOLA", "seguibli.ttf", "sun"),
    ("PEAK SPORTS", "impact.ttf", "peak"),
    ("PITCH KINGS FC", "impact.ttf", "ring"),   # rows 6-7: supporters' banners
    ("ULTRAS 1904", "impact.ttf", "bolt"),
]


def symbol(d, kind, x, y, s):
    if kind == "bolt":
        d.polygon([(x + s * 0.55, y), (x + s * 0.15, y + s * 0.55), (x + s * 0.45, y + s * 0.55), (x + s * 0.3, y + s), (x + s * 0.85, y + s * 0.4), (x + s * 0.55, y + s * 0.4), (x + s * 0.75, y)], fill=255)
    elif kind == "wing":
        d.polygon([(x, y + s * 0.7), (x + s, y + s * 0.15), (x + s * 0.75, y + s * 0.55), (x + s * 0.3, y + s * 0.8)], fill=255)
    elif kind == "ring":
        d.ellipse([x, y, x + s, y + s], outline=255, width=int(s * 0.16))
        d.ellipse([x + s * 0.38, y + s * 0.38, x + s * 0.62, y + s * 0.62], fill=255)
    elif kind == "orbit":
        d.ellipse([x + s * 0.25, y + s * 0.25, x + s * 0.75, y + s * 0.75], fill=255)
        d.ellipse([x, y + s * 0.32, x + s, y + s * 0.68], outline=255, width=int(s * 0.07))
    elif kind == "sun":
        d.ellipse([x + s * 0.2, y + s * 0.2, x + s * 0.8, y + s * 0.8], fill=255)
        for i in range(8):
            import math
            a = i * math.pi / 4
            cx, cy = x + s / 2, y + s / 2
            d.line([(cx + math.cos(a) * s * 0.38, cy + math.sin(a) * s * 0.38), (cx + math.cos(a) * s * 0.5, cy + math.sin(a) * s * 0.5)], fill=255, width=int(s * 0.07))
    elif kind == "peak":
        d.polygon([(x, y + s), (x + s * 0.4, y + s * 0.15), (x + s * 0.6, y + s * 0.5), (x + s * 0.75, y + s * 0.3), (x + s, y + s)], fill=255)


def main():
    atlas = Image.new("L", (W, ROW * len(BRANDS)), 0)
    for i, (word, font, sym) in enumerate(BRANDS):
        S = 4  # supersample each row
        row = Image.new("L", (W * S // 2, ROW * S // 2), 0)
        d = ImageDraw.Draw(row)
        size = int(ROW * S // 2 * 0.62)
        f = ImageFont.truetype(FONTS + font, size)
        bbox = d.textbbox((0, 0), word, font=f)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        s = int(ROW * S // 2 * 0.62)
        total = s + int(s * 0.25) + tw
        x0 = (row.width - total) // 2
        y0 = (row.height - s) // 2
        symbol(d, sym, x0, y0, s)
        d.text((x0 + s + int(s * 0.25) - bbox[0], (row.height - th) // 2 - bbox[1]), word, font=f, fill=255)
        row = row.resize((W, ROW), Image.LANCZOS)
        atlas.paste(row, (0, i * ROW))
    rgba = Image.merge("RGBA", (atlas, atlas, atlas, atlas))
    path = os.path.join(OUT, "T_PK_Sponsors.png")
    rgba.save(path)
    print("wrote", path)


if __name__ == "__main__":
    main()
